#!/usr/bin/env python3
"""T2 — Score free-text benchmark answers 0-2 with an LLM-as-judge.

Reads results/<model_key>.jsonl produced by eval_scivis_benchmark.py, selects
the open-ended categories (L1_perception, L2_description, L3_comparison,
L4_reasoning, L5_domain, L6_spatial) and grades each answer against the
reference answer extracted from the paper.

    python judge_free_text.py --model qwen3.5-vl            # concurrent
    python judge_free_text.py --model qwen3.5-vl --batch    # Batches API, 50% cost

Output goes to judged/<model_key>.jsonl, an append-only resumable checkpoint.

Credentials resolve the same way the SDK does: ANTHROPIC_API_KEY, then
ANTHROPIC_AUTH_TOKEN, then an `ant auth login` profile. If no key is set, run
`ant auth status` — a live profile needs no environment variable.
"""

import argparse
import json
import os
import re
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import yaml


SYSTEM_PROMPT = """You are grading answers to questions about figures from scientific simulation and visualization papers. For each item you are given the question, a reference answer, and a candidate answer produced by a vision-language model that was shown the figure. Assign an integer score.

Scoring rubric:

2 — Correct. The candidate conveys the substance of the reference answer. Every specific claim it makes that overlaps with the reference is consistent with it.
1 — Partially correct. The candidate gets the general subject or one significant element right, but omits a central element of the reference, or mixes a correct element with an incorrect one.
0 — Incorrect, empty, or irrelevant. The candidate contradicts the reference, describes something else, restates the question without answering it, or is blank.

How to apply the rubric:

- Grade meaning, not wording. A correct answer phrased completely differently from the reference scores 2. Word overlap alone is not evidence of correctness.
- Grade brevity fairly. A short answer that captures the key claim scores 2. Do not penalise a candidate for being less detailed than the reference, only for omitting something the reference treats as central.
- Extra content is harmless unless wrong. Additional detail beyond the reference does not lower the score. Additional detail that contradicts the reference does.
- The reference answers were extracted automatically from figure captions and paper body text. Some are truncated mid-sentence, contain fragments of surrounding prose, or answer a slightly different question than the one asked. When the reference is unusable in this way, grade the candidate against the question and whatever the reference does establish, and say so in your reason.
- Specific quantities matter. If the reference gives a number, axis label, panel count, colour mapping, or named technique and the candidate gives a different one, that is a contradiction, not an omission.
- Being cut off mid-sentence is not itself an error. Grade what was said.

Respond with a single JSON object and nothing else:

{"score": <0, 1, or 2>, "reason": "<at most 20 words>"}"""


USER_TEMPLATE = """Question: {question}

Reference answer: {gold}

Candidate answer: {prediction}"""


def build_user_message(record):
    prediction = (record.get("prediction") or "").strip()
    return USER_TEMPLATE.format(
        question=record["question"].strip(),
        gold=(record.get("gold") or "").strip(),
        prediction=prediction if prediction else "(empty)",
    )


def parse_score(text):
    """Extract (score, reason) from the judge's reply, defensively.

    Sonnet 4.6 does not support the structured-outputs API, so the reply is
    parsed rather than schema-enforced. Returns (None, raw) if no score is
    recoverable, which aggregate_results.py reports as a judge parse failure.
    """
    text = (text or "").strip()

    # Whole reply as JSON, optionally fenced.
    fenced = re.sub(r"^```(?:json)?\s*|\s*```$", "", text).strip()
    for candidate in (fenced, text):
        try:
            obj = json.loads(candidate)
            score = int(obj["score"])
            if score in (0, 1, 2):
                return score, str(obj.get("reason", ""))[:200]
        except (json.JSONDecodeError, KeyError, TypeError, ValueError):
            pass

    # A "score": N pair embedded in surrounding prose.
    match = re.search(r'"?score"?\s*[:=]\s*"?([0-2])"?', text)
    if match:
        reason = re.search(r'"?reason"?\s*[:=]\s*"([^"]*)"', text)
        return int(match.group(1)), (reason.group(1) if reason else "")[:200]

    # Bare digit as the entire reply.
    if re.fullmatch(r"[0-2]", text):
        return int(text), ""

    return None, text[:200]


def load_targets(results_path, categories):
    """Free-text records from a T1 checkpoint, in file order."""
    targets = []
    with open(results_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if record.get("category") in categories:
                targets.append(record)
    return targets


def completed_ids(path):
    done = set()
    if not os.path.exists(path):
        return done
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                done.add(json.loads(line)["id"])
            except (json.JSONDecodeError, KeyError):
                continue
    return done


def result_row(record, score, reason, raw, error=None):
    return {
        "id": record["id"],
        "model": record.get("model"),
        "figure_id": record.get("figure_id"),
        "domain": record.get("domain"),
        "category": record.get("category"),
        "difficulty": record.get("difficulty"),
        "question": record.get("question"),
        "gold": record.get("gold"),
        "prediction": record.get("prediction"),
        "judge_score": score,
        "judge_reason": reason,
        "judge_raw": raw if score is None else None,
        "error": error,
    }


# ── Interactive mode ─────────────────────────────────────────────────────────

def request_params(cfg_judge, record, cache_system):
    system_block = {"type": "text", "text": SYSTEM_PROMPT}
    if cache_system:
        # Caches the rubric across every request. Sonnet 4.6's minimum
        # cacheable prefix is 1024 tokens — if the rubric is shorter, this is a
        # silent no-op rather than an error. Check
        # usage.cache_read_input_tokens on a real run to confirm it landed.
        system_block["cache_control"] = {"type": "ephemeral"}

    return {
        "model": cfg_judge["model"],
        "max_tokens": cfg_judge.get("max_tokens", 200),
        # Thinking is left unset: on Sonnet 4.6 that means no thinking, which
        # is what a scoped rubric-scoring task wants.
        "output_config": {"effort": cfg_judge.get("effort", "low")},
        "system": [system_block],
        "messages": [{"role": "user", "content": build_user_message(record)}],
    }


def run_interactive(client, cfg_judge, targets, out_path):
    import anthropic

    lock = threading.Lock()
    counts = {"done": 0, "unparsed": 0, "error": 0}
    started = time.time()

    def judge_one(record):
        try:
            response = client.messages.create(**request_params(cfg_judge, record, True))
        except anthropic.APIError as exc:
            return result_row(record, None, "", "", f"{type(exc).__name__}: {exc}")

        if response.stop_reason == "refusal":
            return result_row(record, None, "", "", "refusal")

        text = next((b.text for b in response.content if b.type == "text"), "")
        score, reason = parse_score(text)
        return result_row(record, score, reason, text)

    with open(out_path, "a", encoding="utf-8") as sink:
        with ThreadPoolExecutor(max_workers=cfg_judge.get("max_workers", 8)) as pool:
            futures = {pool.submit(judge_one, r): r for r in targets}
            for future in as_completed(futures):
                row = future.result()
                with lock:
                    sink.write(json.dumps(row, ensure_ascii=False) + "\n")
                    sink.flush()
                    os.fsync(sink.fileno())
                    counts["done"] += 1
                    if row["error"]:
                        counts["error"] += 1
                    elif row["judge_score"] is None:
                        counts["unparsed"] += 1
                    if counts["done"] % 50 == 0:
                        rate = counts["done"] / (time.time() - started)
                        left = (len(targets) - counts["done"]) / rate if rate else 0
                        print(
                            f"[{counts['done']}/{len(targets)}] "
                            f"{rate:.1f} items/s, ~{left / 60:.0f} min left",
                            flush=True,
                        )
    return counts


# ── Batch mode ───────────────────────────────────────────────────────────────

def run_batch(client, cfg_judge, targets, out_path, poll_seconds):
    """Score via the Message Batches API — 50% cheaper, up to 24h latency."""
    from anthropic.types.message_create_params import MessageCreateParamsNonStreaming
    from anthropic.types.messages.batch_create_params import Request

    # custom_id is capped at 64 chars and restricted to [A-Za-z0-9_-], while
    # question ids are slash-separated paths that can exceed both. Use a
    # positional key and map back through this dict.
    by_custom_id = {f"q{i:06d}": record for i, record in enumerate(targets)}
    requests = [
        Request(
            custom_id=custom_id,
            params=MessageCreateParamsNonStreaming(
                **request_params(cfg_judge, record, True)
            ),
        )
        for custom_id, record in by_custom_id.items()
    ]

    # The API caps a batch at 100,000 requests / 256 MB; chunk defensively so
    # this also works if the benchmark grows.
    chunk_size = 50_000
    counts = {"done": 0, "unparsed": 0, "error": 0}

    with open(out_path, "a", encoding="utf-8") as sink:
        for start in range(0, len(requests), chunk_size):
            chunk = requests[start : start + chunk_size]
            batch = client.messages.batches.create(requests=chunk)
            print(f"[batch] created {batch.id} with {len(chunk)} requests", flush=True)

            while True:
                batch = client.messages.batches.retrieve(batch.id)
                if batch.processing_status == "ended":
                    break
                print(
                    f"[batch] {batch.processing_status}: "
                    f"{batch.request_counts.processing} processing, "
                    f"{batch.request_counts.succeeded} succeeded",
                    flush=True,
                )
                time.sleep(poll_seconds)

            for result in client.messages.batches.results(batch.id):
                record = by_custom_id[result.custom_id]
                if result.result.type != "succeeded":
                    row = result_row(record, None, "", "", f"batch_{result.result.type}")
                else:
                    message = result.result.message
                    if message.stop_reason == "refusal":
                        row = result_row(record, None, "", "", "refusal")
                    else:
                        text = next(
                            (b.text for b in message.content if b.type == "text"), ""
                        )
                        score, reason = parse_score(text)
                        row = result_row(record, score, reason, text)

                sink.write(json.dumps(row, ensure_ascii=False) + "\n")
                sink.flush()
                counts["done"] += 1
                if row["error"]:
                    counts["error"] += 1
                elif row["judge_score"] is None:
                    counts["unparsed"] += 1
            os.fsync(sink.fileno())

    return counts


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default=str(Path(__file__).parent / "config.yaml"))
    parser.add_argument("--model", required=True, help="model key whose results to judge")
    parser.add_argument("--limit", type=int, default=None)
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument(
        "--batch",
        action="store_true",
        help="use the Message Batches API (50%% cheaper, results within 24h)",
    )
    parser.add_argument("--poll-seconds", type=int, default=60)
    parser.add_argument(
        "--dry-run", action="store_true", help="print prompts, make no API calls"
    )
    args = parser.parse_args()

    cfg = yaml.safe_load(open(args.config, encoding="utf-8"))
    cfg_judge = cfg["judge"]
    categories = set(cfg_judge["free_text_categories"])

    results_path = Path(cfg["paths"]["results_dir"]) / f"{args.model}.jsonl"
    if not results_path.exists():
        parser.error(
            f"no T1 results at {results_path} — run eval_scivis_benchmark.py "
            f"--model {args.model} first"
        )

    judged_dir = Path(cfg["paths"]["judged_dir"])
    judged_dir.mkdir(parents=True, exist_ok=True)
    out_path = judged_dir / f"{args.model}.jsonl"

    targets = load_targets(results_path, categories)
    print(f"[data] {len(targets)} free-text answers in {results_path.name}", flush=True)

    if args.overwrite and out_path.exists():
        out_path.unlink()
    done = completed_ids(out_path)
    targets = [r for r in targets if r["id"] not in done]
    if done:
        print(f"[resume] {len(done)} already judged, {len(targets)} remaining", flush=True)
    if args.limit:
        targets = targets[: args.limit]

    if args.dry_run:
        for record in targets[:3]:
            print("-" * 70)
            print(record["id"], "|", record["category"])
            print(build_user_message(record))
        print(f"\n[dry-run] would judge {len(targets)} answers with {cfg_judge['model']}")
        return

    if not targets:
        print("[done] nothing to judge")
        return

    import anthropic

    client = anthropic.Anthropic()
    if args.batch:
        counts = run_batch(client, cfg_judge, targets, out_path, args.poll_seconds)
    else:
        counts = run_interactive(client, cfg_judge, targets, out_path)

    print(f"\n[done] wrote {out_path}")
    print(
        f"[done] judged {counts['done']}, "
        f"unparseable {counts['unparsed']}, errored {counts['error']}"
    )
    if counts["error"]:
        print("[note] re-run the same command to retry errored items (they resume).",
              file=sys.stderr)


if __name__ == "__main__":
    main()

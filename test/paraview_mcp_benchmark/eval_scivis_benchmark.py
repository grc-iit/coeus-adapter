#!/usr/bin/env python3
"""T1 — Run a vision-language model over the SciVis QA benchmark.

Emits one JSONL record per question to results/<model_key>.jsonl. Binary
(yes/no) and multiple-choice answers are auto-scored inline; free-text answers
are written unscored and graded afterwards by judge_free_text.py (T2).

The output file is an append-only checkpoint: re-running the same command
skips every question already present, so a job killed by the SLURM wall clock
resumes where it stopped.

    python eval_scivis_benchmark.py --model qwen3.5-vl
    python eval_scivis_benchmark.py --model molmo2-8b --limit 50
"""

import argparse
import json
import os
import re
import string
import sys
import time
from pathlib import Path

import yaml


# ── Prompt construction ──────────────────────────────────────────────────────

YES_NO_SUFFIX = "Answer with exactly one word: Yes or No."

MC_SUFFIX = "Answer with exactly one letter: the letter of the correct option."

FREE_TEXT_SUFFIX = "Answer in one or two sentences."


def build_prompt(qa):
    """Return the user-turn text for a QA record."""
    fmt = qa.get("format", "free_text")
    question = qa["question"].strip()

    if fmt == "yes_no":
        return f"{question}\n\n{YES_NO_SUFFIX}"

    if fmt == "multiple_choice":
        options = qa["options"]
        rendered = "\n".join(f"{k}. {v}" for k, v in sorted(options.items()))
        return f"{question}\n\n{rendered}\n\n{MC_SUFFIX}"

    return f"{question}\n\n{FREE_TEXT_SUFFIX}"


# ── Auto-scoring ─────────────────────────────────────────────────────────────

_PUNCT = str.maketrans("", "", string.punctuation)


def _words(text):
    return text.lower().translate(_PUNCT).split()


def score_yes_no(prediction, gold):
    """Return 1/0, or None if the model produced neither yes nor no."""
    for word in _words(prediction):
        if word in ("yes", "yeah", "true"):
            return int(gold.strip().lower() == "yes"), "yes"
        if word in ("no", "nope", "false"):
            return int(gold.strip().lower() == "no"), "no"
    return None, None


def score_multiple_choice(prediction, gold_letter, options):
    """Return 1/0 and the parsed letter, or (None, None) if unparseable.

    Tries, in order: a standalone A-D letter, a "(A)"/"A."-style prefix, then a
    verbatim match against the option text (some models answer with the option
    rather than its letter).
    """
    valid = set(options)

    match = re.search(r"\b([A-D])\b", prediction.upper())
    if match and match.group(1) in valid:
        letter = match.group(1)
        return int(letter == gold_letter), letter

    match = re.match(r"\s*[\(\[]?([A-D])[\)\].:]", prediction.upper())
    if match and match.group(1) in valid:
        letter = match.group(1)
        return int(letter == gold_letter), letter

    normalized = " ".join(_words(prediction))
    for letter, text in options.items():
        if " ".join(_words(str(text))) and " ".join(_words(str(text))) in normalized:
            return int(letter == gold_letter), letter

    return None, None


# ── Dataset ──────────────────────────────────────────────────────────────────

def load_questions(qa_path, collection_root):
    """Flatten qa_dataset.jsonl into one record per question."""
    records = []
    with open(qa_path, encoding="utf-8") as f:
        for line in f:
            entry = json.loads(line)
            image = Path(collection_root) / entry["image_path"]
            for qa in entry["qa_pairs"]:
                records.append(
                    {
                        "id": qa["id"],
                        "figure_id": entry["figure_id"],
                        "image_path": str(image),
                        "domain": entry["domain"],
                        "category": qa["category"],
                        "format": qa.get("format", "free_text"),
                        "difficulty": qa["difficulty"],
                        "question": qa["question"],
                        "options": qa.get("options"),
                        "answer": qa["answer"],
                        "answer_text": qa.get("answer_text"),
                    }
                )
    # Group questions about the same figure together so each image is decoded
    # once per contiguous run rather than repeatedly across the file.
    records.sort(key=lambda r: (r["image_path"], r["id"]))
    return records


def completed_ids(path):
    """IDs already written to a checkpoint, tolerating a truncated last line."""
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
                continue  # partial write from a killed job
    return done


# ── Model ────────────────────────────────────────────────────────────────────

def load_model(spec):
    import torch
    from transformers import AutoModelForImageTextToText, AutoProcessor

    hf_id = spec["hf_id"]
    trust = bool(spec.get("trust_remote_code", False))

    # Some models expect `dtype`, others only honour `torch_dtype` and silently
    # load fp32 when given the wrong one. The name is declared per model in the
    # config; verify the loaded footprint below matches bf16 expectations.
    dtype_kwarg = spec.get("dtype_kwarg", "dtype")
    load_kwargs = {
        dtype_kwarg: torch.bfloat16,
        "device_map": "auto",
        "trust_remote_code": trust,
    }

    print(f"[load] {hf_id} ({dtype_kwarg}=bfloat16, device_map=auto)", flush=True)
    processor = AutoProcessor.from_pretrained(hf_id, trust_remote_code=trust)
    model = AutoModelForImageTextToText.from_pretrained(hf_id, **load_kwargs)
    model.eval()

    params = sum(p.numel() for p in model.parameters())
    bytes_per_param = next(model.parameters()).element_size()
    print(
        f"[load] {params / 1e9:.1f}B params @ {bytes_per_param} bytes/param "
        f"= {params * bytes_per_param / 1e9:.1f} GB",
        flush=True,
    )
    if bytes_per_param != 2:
        print(
            f"[warn] parameters are {bytes_per_param} bytes wide, not 2 — the "
            f"'{dtype_kwarg}' kwarg was probably ignored and the model loaded "
            f"in fp32. Check dtype_kwarg for this model in config.yaml.",
            file=sys.stderr,
            flush=True,
        )
    return processor, model


def load_image(path, max_long_edge):
    from PIL import Image

    image = Image.open(path).convert("RGB")
    if max_long_edge:
        longest = max(image.size)
        if longest > max_long_edge:
            scale = max_long_edge / longest
            new_size = (round(image.width * scale), round(image.height * scale))
            image = image.resize(new_size, Image.LANCZOS)
    return image


def generate(processor, model, image, prompt, max_new_tokens, chat_template_kwargs):
    import torch

    messages = [
        {
            "role": "user",
            "content": [
                {"type": "image", "image": image},
                {"type": "text", "text": prompt},
            ],
        }
    ]

    try:
        inputs = processor.apply_chat_template(
            messages,
            add_generation_prompt=True,
            tokenize=True,
            return_dict=True,
            return_tensors="pt",
            **chat_template_kwargs,
        )
    except (TypeError, ValueError):
        # Processors that only render text from the template; feed the image
        # through the processor call instead.
        text = processor.apply_chat_template(
            messages,
            add_generation_prompt=True,
            tokenize=False,
            **chat_template_kwargs,
        )
        inputs = processor(text=text, images=[image], return_tensors="pt")

    inputs = {k: (v.to(model.device) if hasattr(v, "to") else v) for k, v in inputs.items()}
    prompt_len = inputs["input_ids"].shape[1]

    with torch.inference_mode():
        output = model.generate(
            **inputs,
            max_new_tokens=max_new_tokens,
            do_sample=False,
            num_beams=1,
        )

    return processor.decode(output[0][prompt_len:], skip_special_tokens=True).strip()


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default=str(Path(__file__).parent / "config.yaml"))
    parser.add_argument("--model", required=True, help="key under `models:` in config.yaml")
    parser.add_argument("--limit", type=int, default=None, help="stop after N questions")
    parser.add_argument("--overwrite", action="store_true", help="ignore existing checkpoint")
    parser.add_argument("--dry-run", action="store_true", help="build prompts, load no model")
    args = parser.parse_args()

    cfg = yaml.safe_load(open(args.config, encoding="utf-8"))
    if args.model not in cfg["models"]:
        parser.error(f"unknown model '{args.model}'. Known: {', '.join(cfg['models'])}")
    spec = cfg["models"][args.model]

    paths = cfg["paths"]
    results_dir = Path(paths["results_dir"])
    results_dir.mkdir(parents=True, exist_ok=True)
    out_path = results_dir / f"{args.model}.jsonl"

    records = load_questions(paths["qa_dataset"], paths["collection_root"])
    print(f"[data] {len(records)} questions", flush=True)

    missing = [r for r in records if not os.path.exists(r["image_path"])]
    if missing:
        print(
            f"[warn] {len(missing)} questions reference a missing image "
            f"(first: {missing[0]['image_path']}); they will be skipped.",
            file=sys.stderr,
            flush=True,
        )
        missing_ids = {r["id"] for r in missing}
        records = [r for r in records if r["id"] not in missing_ids]

    if args.overwrite and out_path.exists():
        out_path.unlink()
    done = completed_ids(out_path)
    todo = [r for r in records if r["id"] not in done]
    if done:
        print(f"[resume] {len(done)} already scored, {len(todo)} remaining", flush=True)
    if args.limit:
        todo = todo[: args.limit]

    if args.dry_run:
        for record in todo[:5]:
            print("-" * 70)
            print(record["id"], "|", record["format"])
            print(build_prompt(record))
        print(f"\n[dry-run] would evaluate {len(todo)} questions")
        return

    processor, model = load_model(spec)
    max_new = cfg["generation"]["max_new_tokens"]
    max_long_edge = cfg["image"].get("max_long_edge")
    chat_kwargs = spec.get("chat_template_kwargs") or {}

    cached_path, cached_image = None, None
    started = time.time()
    n_scored = n_unparsed = 0

    # Line-buffered plus an explicit flush per record: Python fully buffers file
    # writes in a non-tty SLURM job, so without this the checkpoint reads as
    # 0 bytes for hours and a killed job loses everything.
    with open(out_path, "a", encoding="utf-8") as sink:
        for i, record in enumerate(todo, 1):
            if record["image_path"] != cached_path:
                cached_image = load_image(record["image_path"], max_long_edge)
                cached_path = record["image_path"]

            prompt = build_prompt(record)
            budget = max_new.get(record["format"], 15)

            try:
                prediction = generate(
                    processor, model, cached_image, prompt, budget, chat_kwargs
                )
                error = None
            except Exception as exc:  # keep the sweep alive; record the failure
                prediction, error = "", f"{type(exc).__name__}: {exc}"
                print(f"[error] {record['id']}: {error}", file=sys.stderr, flush=True)

            correct, parsed = None, None
            if error is None:
                if record["format"] == "yes_no":
                    correct, parsed = score_yes_no(prediction, record["answer"])
                elif record["format"] == "multiple_choice":
                    correct, parsed = score_multiple_choice(
                        prediction, record["answer"], record["options"]
                    )

            if record["format"] in ("yes_no", "multiple_choice"):
                if correct is None:
                    n_unparsed += 1
                else:
                    n_scored += 1

            sink.write(
                json.dumps(
                    {
                        "id": record["id"],
                        "model": args.model,
                        "figure_id": record["figure_id"],
                        "image_path": record["image_path"],
                        "domain": record["domain"],
                        "category": record["category"],
                        "format": record["format"],
                        "difficulty": record["difficulty"],
                        "question": record["question"],
                        "options": record["options"],
                        "gold": record["answer"],
                        "gold_text": record["answer_text"],
                        "prediction": prediction,
                        "parsed": parsed,
                        "correct": correct,
                        "error": error,
                    },
                    ensure_ascii=False,
                )
                + "\n"
            )
            sink.flush()
            os.fsync(sink.fileno())

            if i % 25 == 0 or i == len(todo):
                rate = i / (time.time() - started)
                remaining = (len(todo) - i) / rate if rate else 0
                print(
                    f"[{i}/{len(todo)}] {rate:.2f} q/s, ~{remaining / 60:.0f} min left",
                    flush=True,
                )

    print(f"\n[done] wrote {out_path}")
    print(f"[done] auto-scored {n_scored}, unparseable {n_unparsed}")
    if n_unparsed:
        print(
            "[note] unparseable answers count as neither correct nor incorrect in "
            "the raw file. aggregate_results.py scores them as incorrect and "
            "reports the parse rate separately."
        )


if __name__ == "__main__":
    main()

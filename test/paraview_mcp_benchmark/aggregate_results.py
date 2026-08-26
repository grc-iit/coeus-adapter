#!/usr/bin/env python3
"""T3 — Aggregate T1 and T2 outputs into the reported result tables.

Merges results/<model>.jsonl (auto-scored binary + multiple choice) with
judged/<model>.jsonl (LLM-as-judge free-text scores) and writes:

    tables/overall.{csv,md}          per model x question type
    tables/by_domain.{csv,md}        per model x domain x question type  (Table 2)
    tables/by_category.{csv,md}      per model x L1-L6 category
    tables/by_difficulty.{csv,md}    per model x easy/medium/hard
    tables/coverage.{csv,md}         completeness + parse rates per model

    python aggregate_results.py
    python aggregate_results.py --models qwen3.5-vl molmo2-8b

Scoring conventions, applied uniformly:
  * yes_no / multiple_choice — accuracy in [0,1]. An answer the parser could
    not resolve to a label counts as INCORRECT, not as missing; the parse rate
    is reported separately in coverage so a low score from an unparseable
    output format is distinguishable from a low score from wrong answers.
  * free_text — the judge's 0-2 score rescaled to [0,1] (score/2). Items the
    judge could not score are excluded from the mean and counted in coverage.
"""

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

import yaml


QUESTION_TYPES = ["yes_no", "multiple_choice", "free_text"]
CATEGORIES = [
    "L1_yesno",
    "L1_multichoice",
    "L1_perception",
    "L2_description",
    "L2_multichoice",
    "L3_comparison",
    "L4_reasoning",
    "L5_domain",
    "L6_spatial",
]
DIFFICULTIES = ["easy", "medium", "hard"]


class Cell:
    """A running mean plus the counters needed to explain it."""

    __slots__ = ("total", "n", "n_unparsed", "n_error")

    def __init__(self):
        self.total = 0.0
        self.n = 0
        self.n_unparsed = 0
        self.n_error = 0

    def add(self, value):
        self.total += value
        self.n += 1

    @property
    def mean(self):
        return self.total / self.n if self.n else None


def fmt(cell):
    if cell is None or cell.n == 0:
        return "--"
    return f"{cell.mean:.3f}"


def fmt_n(cell):
    return "0" if cell is None else str(cell.n)


def read_jsonl(path):
    if not path.exists():
        return
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


def load_model_scores(results_path, judged_path, unparsed_policy="exclude"):
    """Return (scored_records, coverage) for one model.

    Each scored record carries a `score` in [0,1] plus its grouping keys.

    `unparsed_policy` decides what happens to a binary/MC answer the parser
    could not resolve: "exclude" drops it from the denominator (reproduces the
    published tables), "incorrect" scores it 0 against the full denominator.
    """
    judged = {}
    for row in read_jsonl(judged_path):
        judged[row["id"]] = row

    scored = []
    coverage = {
        "n_results": 0,
        "n_inference_errors": 0,
        "n_auto": 0,
        "n_auto_unparsed": 0,
        "n_free_text": 0,
        "n_free_text_judged": 0,
        "n_judge_unparsed": 0,
        "n_judge_errors": 0,
    }

    for row in read_jsonl(results_path):
        coverage["n_results"] += 1
        if row.get("error"):
            coverage["n_inference_errors"] += 1

        fmt_key = row.get("format", "free_text")
        base = {
            "domain": row.get("domain"),
            "category": row.get("category"),
            "difficulty": row.get("difficulty"),
            "format": fmt_key,
        }

        if fmt_key in ("yes_no", "multiple_choice"):
            coverage["n_auto"] += 1
            correct = row.get("correct")
            if correct is None:
                coverage["n_auto_unparsed"] += 1
                if unparsed_policy == "incorrect":
                    scored.append({**base, "score": 0.0, "resolved": False})
                # "exclude": omitted from the denominator entirely.
            else:
                scored.append({**base, "score": float(correct), "resolved": True})
            continue

        coverage["n_free_text"] += 1
        verdict = judged.get(row["id"])
        if verdict is None:
            continue  # not judged yet — excluded from the mean
        if verdict.get("error"):
            coverage["n_judge_errors"] += 1
            continue
        if verdict.get("judge_score") is None:
            coverage["n_judge_unparsed"] += 1
            continue
        coverage["n_free_text_judged"] += 1
        scored.append({**base, "score": verdict["judge_score"] / 2.0, "resolved": True})

    return scored, coverage


def tabulate(scored, key_fn):
    """Group scored records into {key: {question_type: Cell}} plus an ALL column."""
    table = defaultdict(lambda: defaultdict(Cell))
    for record in scored:
        key = key_fn(record)
        if key is None:
            continue
        table[key][record["format"]].add(record["score"])
        table[key]["ALL"].add(record["score"])
    return table


def write_table(path_stem, header, rows):
    """Write one table as both CSV and GitHub-flavoured markdown."""
    with open(f"{path_stem}.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

    with open(f"{path_stem}.md", "w", encoding="utf-8") as f:
        f.write("| " + " | ".join(header) + " |\n")
        f.write("|" + "|".join(["---"] * len(header)) + "|\n")
        for row in rows:
            f.write("| " + " | ".join(str(c) for c in row) + " |\n")


def print_table(title, header, rows):
    widths = [
        max(len(str(header[i])), *(len(str(r[i])) for r in rows)) if rows else len(header[i])
        for i in range(len(header))
    ]
    line = "  ".join(str(header[i]).ljust(widths[i]) for i in range(len(header)))
    print(f"\n{title}")
    print("=" * len(line))
    print(line)
    print("-" * len(line))
    for row in rows:
        print("  ".join(str(row[i]).ljust(widths[i]) for i in range(len(header))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default=str(Path(__file__).parent / "config.yaml"))
    parser.add_argument("--models", nargs="*", default=None, help="default: all with results")
    parser.add_argument(
        "--unparsed-policy",
        choices=["exclude", "incorrect"],
        default=None,
        help="override scoring.unparsed_policy from the config",
    )
    args = parser.parse_args()

    cfg = yaml.safe_load(open(args.config, encoding="utf-8"))
    results_dir = Path(cfg["paths"]["results_dir"])
    judged_dir = Path(cfg["paths"]["judged_dir"])
    tables_dir = Path(cfg["paths"]["tables_dir"])
    tables_dir.mkdir(parents=True, exist_ok=True)

    if args.models:
        model_keys = args.models
    else:
        model_keys = sorted(p.stem for p in results_dir.glob("*.jsonl"))
    if not model_keys:
        parser.error(f"no result files in {results_dir} — run eval_scivis_benchmark.py first")

    policy = args.unparsed_policy or cfg.get("scoring", {}).get(
        "unparsed_policy", "exclude"
    )
    print(f"[config] unparsed_policy = {policy}", flush=True)

    per_model = {}
    for key in model_keys:
        scored, coverage = load_model_scores(
            results_dir / f"{key}.jsonl", judged_dir / f"{key}.jsonl", policy
        )
        per_model[key] = (scored, coverage)
        print(f"[load] {key}: {len(scored)} scored records", flush=True)

    # The published tables use `exclude`; report the strict number alongside so
    # the effect of the choice is visible rather than buried in the config.
    other = "incorrect" if policy == "exclude" else "exclude"
    contrast = {}
    for key in model_keys:
        alt, _ = load_model_scores(
            results_dir / f"{key}.jsonl", judged_dir / f"{key}.jsonl", other
        )
        cell = Cell()
        for record in alt:
            cell.add(record["score"])
        contrast[key] = cell

    # ── Coverage ─────────────────────────────────────────────────────────────
    header = [
        "model",
        "answers",
        "inference_errors",
        "auto_scored",
        "auto_parse_rate",
        "free_text",
        "judged",
        "judge_parse_rate",
    ]
    rows = []
    for key in model_keys:
        c = per_model[key][1]
        auto_rate = (c["n_auto"] - c["n_auto_unparsed"]) / c["n_auto"] if c["n_auto"] else 0
        attempted = c["n_free_text_judged"] + c["n_judge_unparsed"]
        judge_rate = c["n_free_text_judged"] / attempted if attempted else 0
        rows.append(
            [
                key,
                c["n_results"],
                c["n_inference_errors"],
                c["n_auto"],
                f"{auto_rate:.3f}",
                c["n_free_text"],
                c["n_free_text_judged"],
                f"{judge_rate:.3f}",
            ]
        )
    write_table(tables_dir / "coverage", header, rows)
    print_table("Coverage", header, rows)

    # ── Overall: model x question type ───────────────────────────────────────
    header = ["model"] + QUESTION_TYPES + ["ALL", "n", f"ALL[{other}]", f"n[{other}]"]
    rows = []
    for key in model_keys:
        table = tabulate(per_model[key][0], lambda r: "all")["all"]
        rows.append(
            [key] + [fmt(table.get(t)) for t in QUESTION_TYPES]
            + [fmt(table.get("ALL")), fmt_n(table.get("ALL"))]
            + [fmt(contrast[key]), fmt_n(contrast[key])]
        )
    write_table(tables_dir / "overall", header, rows)
    print_table(
        f"Overall accuracy (free_text = judge score / 2; unparsed = {policy})",
        header,
        rows,
    )

    # ── Table 2: model x domain x question type ──────────────────────────────
    header = ["model", "domain"] + QUESTION_TYPES + ["ALL", "n"]
    rows = []
    for key in model_keys:
        table = tabulate(per_model[key][0], lambda r: r["domain"])
        for domain in sorted(table):
            cells = table[domain]
            rows.append(
                [key, domain]
                + [fmt(cells.get(t)) for t in QUESTION_TYPES]
                + [fmt(cells.get("ALL")), fmt_n(cells.get("ALL"))]
            )
    write_table(tables_dir / "by_domain", header, rows)
    print_table("Per-domain x per-question-type accuracy (Table 2)", header, rows)

    # ── Category and difficulty breakdowns ───────────────────────────────────
    header = ["model"] + CATEGORIES
    rows = []
    for key in model_keys:
        table = tabulate(per_model[key][0], lambda r: r["category"])
        rows.append([key] + [fmt(table.get(c, {}).get("ALL")) for c in CATEGORIES])
    write_table(tables_dir / "by_category", header, rows)
    print_table("Per-category accuracy", header, rows)

    header = ["model"] + DIFFICULTIES
    rows = []
    for key in model_keys:
        table = tabulate(per_model[key][0], lambda r: r["difficulty"])
        rows.append([key] + [fmt(table.get(d, {}).get("ALL")) for d in DIFFICULTIES])
    write_table(tables_dir / "by_difficulty", header, rows)
    print_table("Per-difficulty accuracy", header, rows)

    print(f"\n[done] wrote CSV + markdown tables to {tables_dir}")


if __name__ == "__main__":
    main()

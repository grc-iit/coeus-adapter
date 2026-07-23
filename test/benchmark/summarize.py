#!/usr/bin/env python3
"""
Summarize benchmark transcripts:
  - Per-case stats (turns, tool calls, tokens, $ cost, ground-truth match)
  - Totals across the run
  - Per-tool call counts (which tools the agent reaches for)

Usage:
    python summarize.py
    python summarize.py --transcripts benchmark/transcripts --cases benchmark/cases
"""
import argparse
import json
import re
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# US$ per million tokens (Anthropic list prices). Update as needed.
PRICING = {
    "claude-sonnet-4-6":            {"in": 3.00,  "out": 15.00},
    "claude-sonnet-4-5-20250929":   {"in": 3.00,  "out": 15.00},
    "claude-opus-4-6":              {"in": 15.00, "out": 75.00},
    "claude-opus-4-5-20251101":     {"in": 15.00, "out": 75.00},
    "claude-haiku-4-5-20251001":    {"in": 1.00,  "out": 5.00},
}


def cost_for(model, usage):
    p = PRICING.get(model)
    if not p:
        return None
    inp = (usage.get("input_tokens", 0)
           + usage.get("cache_creation_input_tokens", 0))
    cached = usage.get("cache_read_input_tokens", 0)
    out = usage.get("output_tokens", 0)
    # Cached input is typically 0.1× the regular input rate; if the proxy
    # doesn't pass this through it'll just be 0 and have no effect.
    return (inp * p["in"] + cached * p["in"] * 0.1 + out * p["out"]) / 1_000_000


# ---------------------------------------------------------------------------
# Answer parsing — pull predicted coordinates / counts out of free-form text
# ---------------------------------------------------------------------------

INT_RE = r"-?\d+"
FLOAT_RE = r"-?\d+(?:\.\d+)?"
NUM_RE = FLOAT_RE


def parse_xyz(text):
    """
    Find an (x, y, z) integer triple in the model's free-form answer.

    Strategy: collect ALL plausible triples, return the LAST one (the model
    typically restates its final answer at the end). We use a stricter
    pattern for x=N y=N z=N requiring only punctuation/whitespace between
    the three components, so we don't conflate analysis traces like
    "x=24 to x=30, ... y=29, ... z=26" with a real coordinate.
    """
    if not text:
        return None

    candidates = []

    # Plain "(9, 47, 39)" or "[9, 47, 39]" — purely numeric triples
    for m in re.finditer(
        rf"[\(\[]\s*({NUM_RE})\s*,\s*({NUM_RE})\s*,\s*({NUM_RE})\s*[\)\]]",
        text,
    ):
        candidates.append([round(float(m.group(i))) for i in (1, 2, 3)])

    # "x=27, y=29, z=26" or "(x=27, y=29, z=26)" — only allow punctuation /
    # whitespace between the three components.
    for m in re.finditer(
        rf"x\s*=\s*({NUM_RE})\s*[,;]\s*y\s*=\s*({NUM_RE})\s*[,;]\s*z\s*=\s*({NUM_RE})",
        text, re.IGNORECASE,
    ):
        candidates.append([round(float(m.group(i))) for i in (1, 2, 3)])

    return candidates[-1] if candidates else None


def parse_count(text):
    """Find an integer count of objects in the answer (for blobs)."""
    if not text:
        return None
    patterns = [
        # "3 distinct blob objects", "3 blob objects", "3 distinct blobs"
        r"(\d+)\s+(?:distinct\s+)?(?:\w+\s+)?(?:blobs?|objects|components|features|peaks)\b",
        # "Total distinct blobs: 3", "Total: 3 blobs", "Total blobs = 3"
        r"total[\s\w]*?[:=]\s*(\d+)",
        # "There are 3 distinct ...", "Identified 3 ..."
        r"(?:there\s+are|identified|counted|found)\s+(\d+)\s+(?:distinct\s+)?(?:\w+\s+)?(?:blobs?|objects|components|peaks)",
    ]
    for p in patterns:
        m = re.search(p, text, re.IGNORECASE)
        if m:
            return int(m.group(1))
    return None


def parse_radius(text):
    """
    Find a radius value in the answer (for cavity).

    Handles many phrasings the model produces:
      - "radius = 3", "radius: 3", "radius ≈ 3", "radius is 3", "radius of 3"
      - "r = 3", "r ≈ 3 voxels"
      - "half-width = 3"
      - "≈ 4 voxels" (a leading "**≈" pattern when radius is the section topic)
    """
    if not text:
        return None
    patterns = [
        rf"radius\s*(?:is|of)?\s*[:=≈~]?\s*({FLOAT_RE})",
        rf"\br\s*[:=≈~]\s*({FLOAT_RE})\s*voxels?",         # "r = 7 voxels"
        rf"\*\*r\s*[:=≈~]\s*({FLOAT_RE})",                  # "**r ≈ 7"
        rf"half[-_\s]?width\s*[:=≈~]?\s*({FLOAT_RE})",
        # Bare "≈ 4 voxels" appearing right after a "Cavity Radius" header
        rf"cavity\s+radius[\s\S]{{0,80}}?[≈~=:]\s*({FLOAT_RE})\s*voxels?",
        rf"\*\*[≈~]?\s*({FLOAT_RE})\s*voxels?\*\*",         # bold "**≈ 4 voxels**"
    ]
    for p in patterns:
        m = re.search(p, text, re.IGNORECASE)
        if m:
            return float(m.group(1))
    return None


# ---------------------------------------------------------------------------
# Scoring per needle type
# ---------------------------------------------------------------------------

def score_case(transcript, truth):
    needle = truth.get("needle")
    answer = transcript.get("final_answer", "") or ""
    pred_xyz = parse_xyz(answer)

    score = {"needle": needle, "pred_xyz": pred_xyz}

    if needle == "hotspot":
        center = truth["center_voxel"]
        if pred_xyz is None:
            score["status"] = "no_answer"
            score["distance"] = None
        else:
            d = sum((pred_xyz[i] - center[i]) ** 2 for i in range(3)) ** 0.5
            score["truth_xyz"] = center
            score["distance"] = round(d, 2)
            score["status"] = "hit" if d <= 2 else ("near" if d <= 5 else "miss")

    elif needle == "cavity":
        center = truth["inner_center_voxel"]
        true_r = truth["inner_radius_voxels"]
        pred_r = parse_radius(answer)
        score["truth_xyz"] = center
        score["truth_radius"] = true_r
        score["pred_radius"] = pred_r
        if pred_xyz is None:
            score["status"] = "no_answer"
            score["distance"] = None
        else:
            d = sum((pred_xyz[i] - center[i]) ** 2 for i in range(3)) ** 0.5
            score["distance"] = round(d, 2)
            r_ok = pred_r is not None and abs(pred_r - true_r) <= 2
            d_ok = d <= 3
            score["status"] = "hit" if (d_ok and r_ok) else (
                "partial" if d_ok or r_ok else "miss"
            )

    elif needle == "disconnected_blobs":
        true_n = truth["n_blobs"]
        pred_n = parse_count(answer)
        score["truth_n"] = true_n
        score["pred_n"] = pred_n
        if pred_n is None:
            score["status"] = "no_answer"
        elif pred_n == true_n:
            score["status"] = "hit"
        elif abs(pred_n - true_n) == 1:
            score["status"] = "near"
        else:
            score["status"] = "miss"

    return score


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--transcripts", type=Path, default=REPO / "benchmark/transcripts")
    parser.add_argument("--cases", type=Path, default=REPO / "benchmark/cases")
    args = parser.parse_args()

    transcripts = sorted(args.transcripts.glob("transcript_case_*.json"))
    if not transcripts:
        print(f"No transcripts found in {args.transcripts}")
        return

    rows = []
    tool_counter = Counter()
    totals = {"in": 0, "out": 0, "cached": 0, "cache_create": 0, "cost": 0.0,
              "turns": 0, "tool_calls": 0, "n": 0, "hit": 0}

    for tpath in transcripts:
        t = json.load(open(tpath))
        case_id = t["case_id"]
        truth_path = args.cases / f"case_{case_id:03d}_{t['needle']}.truth.json"
        truth = json.load(open(truth_path))
        score = score_case(t, truth)

        u = t.get("usage", {})
        c = cost_for(t["model"], u) or 0.0

        for tc in t.get("tool_calls", []):
            tool_counter[tc["name"]] += 1

        rows.append({
            "case": case_id,
            "needle": t["needle"],
            "stop": t.get("stop"),
            "turns": t.get("n_turns"),
            "calls": t.get("n_tool_calls"),
            "in": u.get("input_tokens", 0),
            "out": u.get("output_tokens", 0),
            "cost": c,
            "score": score,
        })

        totals["in"] += u.get("input_tokens", 0)
        totals["out"] += u.get("output_tokens", 0)
        totals["cached"] += u.get("cache_read_input_tokens", 0)
        totals["cache_create"] += u.get("cache_creation_input_tokens", 0)
        totals["cost"] += c
        totals["turns"] += t.get("n_turns", 0) or 0
        totals["tool_calls"] += t.get("n_tool_calls", 0) or 0
        totals["n"] += 1
        if score["status"] == "hit":
            totals["hit"] += 1

    # Per-case table
    print(f"{'case':>4}  {'needle':<20} {'stop':<22} {'turns':>5} {'calls':>5} "
          f"{'in_tok':>8} {'out_tok':>7} {'cost$':>7}  {'score':<10}  detail")
    print("-" * 130)
    for r in rows:
        s = r["score"]
        status = s["status"]
        detail = ""
        if s["needle"] == "hotspot":
            detail = (f"pred={s.get('pred_xyz')} truth={s.get('truth_xyz')} "
                      f"d={s.get('distance')}")
        elif s["needle"] == "cavity":
            detail = (f"pred={s.get('pred_xyz')}/r={s.get('pred_radius')} "
                      f"truth={s.get('truth_xyz')}/r={s.get('truth_radius')} "
                      f"d={s.get('distance')}")
        elif s["needle"] == "disconnected_blobs":
            detail = f"pred_n={s.get('pred_n')} truth_n={s.get('truth_n')}"

        print(f"{r['case']:>4}  {r['needle']:<20} {str(r['stop']):<22} "
              f"{r['turns']:>5} {r['calls']:>5} {r['in']:>8} {r['out']:>7} "
              f"{r['cost']:>7.4f}  {status:<10}  {detail}")

    # Totals
    print("-" * 130)
    print(f"TOTAL: {totals['n']} cases, {totals['hit']} hits "
          f"({100 * totals['hit'] / totals['n']:.0f}%), "
          f"{totals['turns']} turns, {totals['tool_calls']} tool calls")
    print(f"       input={totals['in']}  output={totals['out']}  "
          f"cache_read={totals['cached']}  cache_create={totals['cache_create']}")
    print(f"       cost ≈ ${totals['cost']:.4f}")

    # Tool histogram
    print()
    print("Tool call counts (across all cases):")
    for name, n in tool_counter.most_common():
        print(f"  {n:>4}  {name}")


if __name__ == "__main__":
    main()

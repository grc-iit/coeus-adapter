#!/usr/bin/env python3
"""Trace the exact global variance(V) that the Vigil trigger keys on.

The engine pools the per-block `derive/VarV` values (N_b-weighted, with the
between-block term from `derive/AddV`) into the exact global variance, so
computing the global variance directly from a BP file reproduces what the
trigger sees, without needing the engine in the loop. Use it to calibrate
`trigger_baseline_ratio` for a regime before running the gated pipeline.

Usage:
    python3 varv_trace.py <run.bp> [<run2.bp> ...] [--full]

Prints, per run: the t=0 baseline, the ratio trajectory summary, and the first
rising-edge crossing of each candidate threshold. With --full, prints every
output step.

Requires the adios2 Python bindings on PYTHONPATH (spack load adios2).
"""
import sys
import numpy as np
import adios2

THRESHOLDS = (1.05, 1.08, 1.20, 2.0, 10.0)


def trace(bp):
    var = []
    with adios2.FileReader(bp) as f:
        for step in range(f.num_steps()):
            v = np.asarray(f.read("V", step_selection=[step, 1]),
                           dtype=np.float64)
            var.append(v.var())
    return np.array(var)


def report(bp, full=False):
    var = trace(bp)
    if not len(var):
        print(f"{bp}: no steps")
        return
    base = var[0]
    r = var / base
    print(f"\n=== {bp} ===")
    print(f"  baseline (output 0, the t=0 seed) = {base:.6e}")
    print(f"  ratio range {r.min():.3f} (output {r.argmin()}) "
          f".. {r.max():.3f} (output {r.argmax()}), final {r[-1]:.3f}")
    # The baseline is captured on the first evaluated step, which for a fast
    # transient can differ sharply from the next one; report that sensitivity.
    if len(r) > 1:
        print(f"  baseline sensitivity: output 0 -> 1 changes the statistic by "
              f"{100 * (r[1] - 1):+.0f}%")
    for thr in THRESHOLDS:
        hits = np.where(r >= thr)[0]
        where = f"output {hits[0]}" if len(hits) else "NEVER"
        print(f"  rising edge >= {thr:5.2f}x : {where:>12}   "
              f"({len(hits)}/{len(r)} outputs above)")
    if full:
        print(f"  {'out':>4} {'simstep*':>9} {'variance':>13} {'ratio':>7}")
        for i, (v, ratio) in enumerate(zip(var, r)):
            print(f"  {i:4d} {'':>9} {v:13.6e} {ratio:7.3f}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        sys.exit(1)
    for bp in args:
        report(bp, full="--full" in sys.argv)

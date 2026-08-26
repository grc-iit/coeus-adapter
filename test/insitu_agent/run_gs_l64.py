#!/usr/bin/env pvpython
"""
Gray-Scott AI-agent streaming bridge -- L=64 version (the ORIGINAL config).

This is the published, verified-working configuration: Volume rendering of V,
which at L=64 genuinely shows the collapse (labyrinth creases visible in the
first frame of the warn window are nearly gone by the fourth). Warn lands at
output ~24.

It is a thin entry point: it forces --profile l64 and forwards every other
argument to insitu_streaming.py unchanged. All the logic (and every bug fix)
lives in the one shared implementation, so the two scale versions cannot drift
apart. See gs_profiles.py for what actually differs and why.

MUST run under pvpython (not the spack python) -- see the consumer scripts.

Usage (identical to insitu_streaming.py, minus --profile):
  pvpython run_gs_l64.py -j gs-fides.json -b $HOME/iowarp/gs.bp --staging \
      --server localhost --port 11112 --paused \
      --status-file streaming_status.json --frames-dir agent_results/frames
"""
import os
import sys

sys.argv = [sys.argv[0]] + [a for a in sys.argv[1:] if not a.startswith("--profile")]
os.environ.setdefault("INSITU_PROFILE", "l64")

_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)

import insitu_streaming  # noqa: E402  (path setup must precede the import)

if __name__ == "__main__":
    insitu_streaming.main()

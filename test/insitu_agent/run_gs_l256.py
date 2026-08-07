#!/usr/bin/env pvpython
"""
Gray-Scott AI-agent streaming bridge -- L=256 version (the SCALE config).

Differs from the L=64 version in exactly one respect that matters: it renders a
CONTOUR of V instead of a Volume. Volume rendering integrates opacity along the
ray, so a thin feature's contrast scales with its fraction of the path -- at
L=256 the warn window renders as four indistinguishable featureless blobs and
the agent correctly reports that it can see no structure. A contour extracts
geometry at a fixed V value and is resolution-independent, turning the collapse
into a crisp signal (measured: the V=0.3 isosurface falls 10825 -> 3168 points,
-71%, monotonically across the 4-step window).

Watch the per-step log line:
    Step N RENDER V=[...] iso@0.3=<N>pts
It must fall monotonically. If <N> is 0 or ~10^7 the isovalue is wrong -- read
the 'isovalue probe' sweep printed once at setup and override with
INSITU_CONTOUR_ISOVALUE. Keep probes below 0.6: the bulk sits just under the
analytic steady state V* ~ 0.592, so a 0.60 surface slices the whole domain
(12.2M points).

Warn lands at output ~125, i.e. ~15 min into a 256-rank run.

It is a thin entry point: it forces --profile l256 and forwards every other
argument to insitu_streaming.py unchanged, so the two scale versions share one
implementation and cannot drift apart. See gs_profiles.py for the measurements.

MUST run under pvpython (not the spack python) -- see the consumer scripts.

Usage (identical to insitu_streaming.py, minus --profile):
  pvpython run_gs_l256.py -j gs-fides.json -b $HOME/iowarp/gs.bp --staging \
      --server localhost --port 11112 --paused \
      --status-file streaming_status.json --frames-dir agent_results/frames
"""
import os
import sys

sys.argv = [sys.argv[0]] + [a for a in sys.argv[1:] if not a.startswith("--profile")]
os.environ.setdefault("INSITU_PROFILE", "l256")

_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)

import insitu_streaming  # noqa: E402  (path setup must precede the import)

if __name__ == "__main__":
    insitu_streaming.main()

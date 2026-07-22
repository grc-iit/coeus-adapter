#!/bin/bash
# =============================================================================
# iowarp@dev variant of run_gs_multinode.sh (L=512, 256 ranks).
#
# WHY A DEV RUN: iowarp@dev (/pj67x7i, clio-core 2192c467) contains upstream
# commit e6edd69e "fix(shm): size MPSC ring for realistic async fan-out
# (#768 deadlock)", which is NOT in iowarp@main (/pboox7q, b3ee500e).
# Verified in the installed prefixes:
#
#     main  kShmMpscDefaultSegmentSize = 128 * 1024   ->  4 ring slots
#     dev   kShmMpscDefaultSegmentSize = 1024 * 1024  -> 32 ring slots
#
# That ring is the client<->daemon lightbeam SHM transport (ipc_mode: "shm",
# which this pipeline uses). Its failure mode as described upstream -- "the
# client blocks in SendIn while the worker blocks in SendOut and neither drains
# the other" -- matches the L=512 producer-side stall signature we see on Delta
# (daemon ingests ~1 output then flat; ranks parked in clio Wait backoff; 2 of
# N daemon workers pegged, rest idle).
#
# NOTE this is a DIFFERENT ring from the worker TaskQueue lanes sized by the
# jarvis `queue_depth` knob (multi_ring_buffer, still FIXED_SIZE|WAIT_FOR_SPACE
# with default 1024 on BOTH main and dev). Two independent chokepoints; this
# run tests only the first.
#
# EXPERIMENT DESIGN: the paired pipeline gray-scott-warn-mn-dev.yaml keeps the
# runtime knobs at the values used by the 2026-07-19 retest #2 that STALLED on
# main (num_threads: 4, client_data_segment_size: 2G, queue_depth unset =>
# 1024). So the ONLY variable changed vs that known-stalling run is
# main -> dev. Do not add queue_depth here -- that is run #2 if this stalls.
#
# Usage:
#   bash CI/Delta/set_nodes.sh cnAAA,cnBBB cnCCC     # stamp in the allocation
#   export ANTHROPIC_API_KEY=sk-ant-...
#   bash CI/Delta/run_gs_multinode_dev.sh
# =============================================================================
set -uo pipefail

REPO="${REPO:-/u/hxu13/software/coeus-adapter}"

# The single reason this wrapper exists: point the orchestrator at the dev env
# (iowarp@dev + build-dev/bin) instead of env.sh (iowarp@main + build/bin).
export ENV_SH="${ENV_SH:-$REPO/CI/Delta/env-dev.sh}"

[[ -f "$ENV_SH" ]] || { echo "ERROR: no dev env at $ENV_SH" >&2; exit 1; }
[[ -x "$REPO/build-dev/bin/adios2-gray-scott" ]] || {
    echo "ERROR: build-dev/bin/adios2-gray-scott missing -- build it first:" >&2
    echo "  source CI/Delta/env-dev.sh && cd build-dev && make -j16" >&2; exit 1; }

# Node placement comes from set_nodes.sh (which also wrote the jarvis hostfile
# and the yaml's srun_nodelist from the same input, so the three cannot drift).
NODES_ENV="${NODES_ENV:-$HOME/iowarp/nodes.env}"
# shellcheck source=/dev/null
[[ -f "$NODES_ENV" ]] && source "$NODES_ENV"

# Guard against running with another allocation's node names still stamped in.
: "${PRODUCER_NODES:?run CI/Delta/set_nodes.sh <prod1,prod2> <consumer> first}"
: "${CONSUMER_NODE:?run CI/Delta/set_nodes.sh <prod1,prod2> <consumer> first}"
export PRODUCER_NODES CONSUMER_NODE

echo "[dev-run] ENV_SH=$ENV_SH"
echo "[dev-run] binary=$REPO/build-dev/bin/adios2-gray-scott"
echo "[dev-run] producers=$PRODUCER_NODES consumer=$CONSUMER_NODE"

exec bash "$REPO/CI/Delta/run_gs_multinode.sh"

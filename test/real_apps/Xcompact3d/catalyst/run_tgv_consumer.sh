#!/bin/bash
# Start the TGV in-situ consumer side (pvserver + streaming bridge) on the
# consumer node (e.g. ares-comp-26). Run AFTER `jarvis ppl run` has created
# the SST contact file (tgv.bp.sst in OUTPUT_DIR) — or before; the bridge
# polls until the writer appears.
#
#   ssh ares-comp-26 bash <this-dir>/run_tgv_consumer.sh
#
# Then run the agent (any node that reaches PVSERVER_HOST:PVSERVER_PORT):
#   see TGV_INSITU_AGENT.md section 4.
#
# Environment overrides: OUTPUT_DIR, PVSERVER_PORT, BRIDGE_ARGS (extra args
# for tgv_insitu_streaming.py, e.g. BRIDGE_ARGS="--max-steps 3" for a gated
# window of 3).

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-/mnt/common/hxu40/incompact3d/output}"
PVSERVER_PORT="${PVSERVER_PORT:-11112}"
BRIDGE_ARGS="${BRIDGE_ARGS:-}"

PARAVIEW=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus

echo "[run_tgv_consumer] pvserver on port ${PVSERVER_PORT} (log: ${SCRIPT_DIR}/pvserver.log)"
"${PARAVIEW}/bin/pvserver" --multi-clients --server-port="${PVSERVER_PORT}" \
    > "${SCRIPT_DIR}/pvserver.log" 2>&1 &
PVSERVER_PID=$!
sleep 3

echo "[run_tgv_consumer] streaming bridge (log: ${SCRIPT_DIR}/tgv_bridge.log)"
PYTHONUNBUFFERED=1 "${PARAVIEW}/bin/pvpython" "${SCRIPT_DIR}/tgv_insitu_streaming.py" \
    -j "${SCRIPT_DIR}/tgv-fides.json" \
    -b "${OUTPUT_DIR}/tgv.bp" \
    --staging --server localhost --port "${PVSERVER_PORT}" --paused \
    --status-file "${SCRIPT_DIR}/tgv_streaming_status.json" \
    --screenshot-file "${SCRIPT_DIR}/tgv_bridge_view.png" \
    --timing-file "${SCRIPT_DIR}/tgv_bridge_timing.jsonl" \
    ${BRIDGE_ARGS} \
    > "${SCRIPT_DIR}/tgv_bridge.log" 2>&1 &
BRIDGE_PID=$!

echo "[run_tgv_consumer] pvserver pid=${PVSERVER_PID} bridge pid=${BRIDGE_PID}"
echo "[run_tgv_consumer] waiting on bridge (Ctrl-C or kill to stop both)"
trap 'kill ${BRIDGE_PID} ${PVSERVER_PID} 2>/dev/null' EXIT INT TERM
wait ${BRIDGE_PID}

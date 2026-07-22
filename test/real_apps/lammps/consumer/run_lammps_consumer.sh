#!/usr/bin/env bash
# Vigil LAMMPS render + reason consumer.
#
# Renders the streamed atom scatter (lammps_sst_reader.py) and drives the AI
# agent (lammps_agent.py -> lammps_insitu_mcp_server.py) to a stop verdict.
#
# Offline test on a BP5 file (no SST / hermes / ParaView needed):
#   ENGINE=BP5 STREAM=/path/to/lammps.bp bash run_lammps_consumer.sh
#
# Live gated SST from the hermes engine (reader connects to the SST stream):
#   ENGINE=SST STREAM=lammps.bp bash run_lammps_consumer.sh
#
# Env knobs: STREAM, ENGINE, MODEL, MAX_STEPS, RUN_DIR, RENDER_ONLY=1 (skip agent)
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STREAM="${STREAM:-lammps.bp}"
ENGINE="${ENGINE:-BP5}"
MODEL="${MODEL:-claude-opus-4-8}"
MAX_STEPS="${MAX_STEPS:-0}"
RUN_DIR="${RUN_DIR:-$(pwd)}"

FRAMES="${RUN_DIR}/frames"
STATUS="${RUN_DIR}/lammps_status.json"
STOP_FLAG="${RUN_DIR}/lammps.bp.stop"
mkdir -p "${FRAMES}"
rm -f "${STATUS}" "${STOP_FLAG}"

echo "[run] reader: stream=${STREAM} engine=${ENGINE} -> ${FRAMES}, ${STATUS}"
python3 "${HERE}/lammps_sst_reader.py" \
    --stream "${STREAM}" --engine "${ENGINE}" \
    --png-dir "${FRAMES}" --status-file "${STATUS}" \
    --max-steps "${MAX_STEPS}" &
READER_PID=$!

# Wait for the first rendered frame to appear in the status file.
for i in $(seq 1 120); do
    [ -f "${STATUS}" ] && grep -q '"image"' "${STATUS}" 2>/dev/null && break
    kill -0 "${READER_PID}" 2>/dev/null || break
    sleep 1
done

if [ "${RENDER_ONLY:-0}" = "1" ]; then
    echo "[run] RENDER_ONLY: reader running (pid ${READER_PID}); frames in ${FRAMES}"
    wait "${READER_PID}"
    exit 0
fi

if [ -z "${ANTHROPIC_API_KEY:-}${ANTHROPIC_AUTH_TOKEN:-}" ]; then
    echo "[run] ANTHROPIC_API_KEY not set -- skipping agent (frames + status still produced)."
else
    echo "[run] agent: model=${MODEL}"
    python3 "${HERE}/lammps_agent.py" \
        --status-file "${STATUS}" --stop-flag "${STOP_FLAG}" --model "${MODEL}"
fi

# Let the reader drain / exit; don't leave it running.
wait "${READER_PID}" 2>/dev/null
[ -f "${STOP_FLAG}" ] && echo "[run] stop verdict: $(cat "${STOP_FLAG}")"
echo "[run] done."

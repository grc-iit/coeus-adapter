#!/bin/bash
# =============================================================================
# Consumer CLIENT stack (bridge + ParaView MCP server + agent) for the
# MULTI-NODE Gray-Scott trigger-render-reason run. Runs ON the consumer node as
# a single srun task; connects to a pvserver ALREADY running on the same node
# (the orchestrator launches the pvserver -- possibly parallel/32-rank -- as a
# separate srun step). Reads the producer's gated SST stream via the NFS
# contact file ($STREAM.sst) and runs the REASON stage.
#
#   pvserver (headless osmesa, N ranks)  <-- launched by the orchestrator
#     ^ bridge (pvpython insitu_streaming)   [this script]
#     ^ MCP server (pvpython insitu_mcp_server)
#     ^ agent (spack python insitu_agent)
#
# Required env (exported by the orchestrator, propagated via srun --export=ALL):
#   PV_HASH PORT STREAM STATUS_FILE SHOT_FILE STOP_FLAG MODEL INSITU MAX_WALL
#   NUM_STEPS ANTHROPIC_API_KEY
# =============================================================================
set -uo pipefail
unset PYTHONPATH LD_LIBRARY_PATH           # drop any inherited iowarp pollution
source "${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh" >/dev/null 2>&1
log(){ echo "[consumer $(hostname -s) $(date +%H:%M:%S)] $*"; }

pids=()
cleanup(){ for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done; }
trap cleanup EXIT INT TERM

log "waiting for pvserver on localhost:$PORT"
for _ in $(seq 1 40); do
    (exec 3<>/dev/tcp/localhost/"$PORT") 2>/dev/null && { exec 3>&- 3<&-; break; }
    sleep 2
done

log "bridge (pvpython insitu_streaming) --paused, reading $STREAM"
( spack load "$PV_HASH"
  exec pvpython "$INSITU/insitu_streaming.py" \
       -j "$INSITU/gs-fides.json" -b "$STREAM" --staging \
       --server localhost --port "$PORT" --paused \
       --status-file "$STATUS_FILE" --screenshot-file "$SHOT_FILE" \
       --max-steps 8 ) > "$(dirname "$STREAM")/bridge_mn.log" 2>&1 &
pids+=($!); sleep 8

# Wait for the TRIGGER WARN before starting the agent: at scale (L=512) the
# writer can take many minutes to reach the warn step, and an agent started
# early burns its --max-wall-seconds blocked inside advance_step. The bridge
# (above) must connect early — the writer blocks at rendezvous on it — but the
# agent only has work once the bridge has actually received the first flagged
# step (streaming_status.json step >= 1).
WARN_WAIT="${WARN_WAIT:-3600}"
log "waiting for the first flagged step (trigger warn) before starting the agent (timeout ${WARN_WAIT}s)"
# Signal: the bridge's paused loop rewrites STATUS_FILE every 0.5s, but ONLY
# after its (gated) Fides setup unblocks — i.e. only once the engine warned and
# shipped the first flagged step. Before that the file is written exactly once
# (pre-Fides) and its mtime freezes — so a single "recent mtime" check false-
# positives right after bridge start. Require the mtime to ADVANCE between two
# samples 4s apart: only the live rewrite loop does that.
elapsed=0
while (( elapsed < WARN_WAIT )); do
    if [[ -f "$STATUS_FILE" ]]; then
        m1=$(stat -c %Y "$STATUS_FILE" 2>/dev/null || echo 0)
        sleep 4
        m2=$(stat -c %Y "$STATUS_FILE" 2>/dev/null || echo 0)
        (( m2 > m1 )) && { log "flagged step landed (status mtime advancing) after ${elapsed}s"; break; }
        elapsed=$((elapsed+4))
    fi
    sleep 5; elapsed=$((elapsed+5))
done
if (( elapsed >= WARN_WAIT )); then
    log "ERROR: no flagged step within ${WARN_WAIT}s — not starting the agent"
    exit 1
fi

log "agent ($MODEL); MCP server spawned via pvpython"
spack load "$PV_HASH"
PV="$(spack location -i "$PV_HASH")"
SPY="$(spack location -i python)/bin/python3"
cd "$INSITU"
env PYTHONPATH="$PV/lib/python3.12/site-packages:$HOME/software/paraview_mcp" \
    LD_LIBRARY_PATH="$PV/lib" \
    ANTHROPIC_API_KEY="$ANTHROPIC_API_KEY" \
  "$SPY" insitu_agent.py \
      --provider anthropic --model "$MODEL" \
      --pvpython "$PV/bin/pvpython" \
      --server-host localhost --server-port "$PORT" \
      --status-file "$STATUS_FILE" --screenshot-file "$SHOT_FILE" \
      --stop-flag "$STOP_FLAG" --results-dir "$INSITU/agent_results" \
      --max-iterations 15 --max-wall-seconds "$MAX_WALL" \
      --prompt "You are inspecting a gated collapse window of exactly ${NUM_STEPS:-4} \
streamed steps from a live large-scale Gray-Scott run. For each: call advance_step, \
then get_screenshot, and note the V field. As soon as the isosurface has vanished and the \
field looks spatially uniform (the pattern has expanded to blank) — or after you have seen all \
${NUM_STEPS:-4} frames — call fire_stop_simulation with a one-line reason. IMPORTANT: call \
advance_step at most ${NUM_STEPS:-4} times; the stream ships nothing after this window, so a \
further advance_step just blocks. Fire promptly once the collapse is clear."
log "agent finished"

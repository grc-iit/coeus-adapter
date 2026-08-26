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
# Optional: RES (timing/results dir; default $INSITU/agent_results)
# =============================================================================
set -uo pipefail
unset PYTHONPATH LD_LIBRARY_PATH           # drop any inherited iowarp pollution
source "${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh" >/dev/null 2>&1
log(){ echo "[consumer $(hostname -s) $(date +%H:%M:%S)] $*"; }

# Timing sinks (shared FS, so the login node can run analyze_timeline.py after).
# Both jsonl files are opened append-only by the bridge/MCP, so clear them per
# run or the analysis silently mixes this run with the previous one.
RES="${RES:-$INSITU/agent_results}"
BRIDGE_TIMING="$RES/streaming_timing.jsonl"   # sst_wait / pipeline_update / render
MCP_TIMING="$RES/mcp_tool_timing.jsonl"       # per MCP tool call
FRAMES_DIR="$RES/frames"
mkdir -p "$RES" "$FRAMES_DIR"
: > "$BRIDGE_TIMING"
: > "$MCP_TIMING"

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
  # -u: unbuffered. Without it pvpython block-buffers and bridge_mn.log stays
  # 0 bytes until exit -- so a killed run leaves NO evidence of whether the
  # bridge ever connected, and "did the SST reader attach?" has to be inferred
  # indirectly from daemon RSS. Keep this; it costs nothing.
  # Scale profile: 'l256' renders a Contour (REQUIRED at L=256 -- Volume gives
  # 4 identical featureless blobs there); 'l64' keeps the original Volume
  # rendering. See test/insitu_agent/gs_profiles.py.
  exec pvpython -u "$INSITU/insitu_streaming.py" \
       --profile "${GS_PROFILE:-l256}" \
       -j "$INSITU/gs-fides.json" -b "$STREAM" --staging \
       --server localhost --port "$PORT" --paused \
       --status-file "$STATUS_FILE" --screenshot-file "$SHOT_FILE" \
       --timing-file "$BRIDGE_TIMING" --frames-dir "$FRAMES_DIR" \
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
#
# This gate is CORRECT, but it silently depends on test/insitu_agent/gs-fides.json
# EXISTING. That file is what makes setup_fides_reader() block until the warn.
# It was missing 2026-08-05..06 (deleted from the tree; still in commits 686f7d6 /
# a37d86a): FidesJSONReader accepts a nonexistent FileName, UpdatePipelineInformation
# returns WITHOUT setting Fides up, so the bridge fell straight through to its
# paused loop, the mtime advanced immediately, and the gate false-positived. The
# real damage was downstream — the reader never opened SST, so the 256-rank writer
# sat in SSTIO->Open() forever (daemon RSS byte-flat at ~110 MB) and every
# advance_step failed with "PrepareNextStep() has been called, but Fides has not
# been set up yet". Restoring gs-fides.json fixed both (job 20904018).
#
# Do NOT "fix" this by gating on pipeline_ready instead: that DEADLOCKS. The bridge
# runs --paused and only sets pipeline_ready after it reads a step, which requires
# an advance_one command from the agent — so the agent would wait for the bridge
# while the bridge waits for the agent.
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
# The agent needs `mcp`+`anthropic`, pip-installed --user under python 3.12
# (~/.local/lib/python3.12/site-packages), matching the $PV/lib/python3.12
# PYTHONPATH below. Pin to the 3.12 spack python explicitly: a bare
# `spack location -i python` became AMBIGUOUS once the iowarp@dev tree pulled a
# second python (3.14) into the spack tree, so it errored and SPY silently
# collapsed to /bin/python3 (system 3.9, no mcp) -> agent died on `import mcp`.
SPY="$(spack location -i python@3.12 2>/dev/null)/bin/python3"
[[ -x "$SPY" ]] || { echo "ERROR: no spack python@3.12 (where mcp is installed); got '$SPY'" >&2; exit 1; }
"$SPY" -c "import mcp, anthropic" 2>/dev/null || { echo "ERROR: $SPY cannot import mcp/anthropic -- pip install --user 'mcp[cli]' anthropic into python3.12" >&2; exit 1; }
cd "$INSITU"
env PYTHONPATH="$PV/lib/python3.12/site-packages:$HOME/software/paraview_mcp" \
    LD_LIBRARY_PATH="$PV/lib" \
    ANTHROPIC_API_KEY="$ANTHROPIC_API_KEY" \
  "$SPY" insitu_agent.py \
      --provider anthropic --model "$MODEL" \
      --pvpython "$PV/bin/pvpython" \
      --server-host localhost --server-port "$PORT" \
      --status-file "$STATUS_FILE" --screenshot-file "$SHOT_FILE" \
      --stop-flag "$STOP_FLAG" --results-dir "$RES" \
      --timing-file "$MCP_TIMING" --frames-dir "$FRAMES_DIR" \
      --max-iterations 15 --max-wall-seconds "$MAX_WALL" \
      --prompt "You are inspecting a gated collapse window of exactly ${NUM_STEPS:-4} \
streamed steps from a live large-scale Gray-Scott run. For each: call advance_step, then \
get_screenshot, and describe what you actually see in the V field — structure present, \
partially dissolved, or gone. \
\
YOUR VERDICT MUST BE GROUNDED IN THE IMAGES. Call fire_stop_simulation ONLY if the V \
isosurface has genuinely vanished AND the field looks spatially uniform (a featureless block \
— the pattern has expanded to blank). Do NOT fire merely because you have run out of frames, \
and do NOT assume the collapse has happened because you were told to look for one. If \
structure is still visible in the final frame, that is a legitimate and expected outcome: say \
so explicitly, state that you are NOT firing and why, and stop — do not call \
fire_stop_simulation at all. A false halt is worse than no halt. \
\
IMPORTANT: call advance_step at most ${NUM_STEPS:-4} times; the stream ships nothing after \
this window, so a further advance_step just blocks."
log "agent finished"

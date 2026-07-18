#!/bin/bash
# =============================================================================
# Gray-Scott Trigger-Render-Reason (Vigil) on a SINGLE Delta node (e.g. cn024).
#
# Composes docs/BUILD_AND_RUN_GRAY_SCOTT.md Section 7.4, adapted for Delta:
#
#   TRIGGER  jarvis writer (coeus-gray-scott pipeline): the hermes engine pools
#            variance(derive/VarV), arms at 20x baseline and WARNS on the
#            collapse back to 13x, shipping the flagged window over gated SST.
#   RENDER   ParaView reads the SST stream and renders headlessly (OSMesa):
#              MODE=render  -> pvbatch catalyst/gs-pipeline.py (no LLM, no API key)
#              MODE=agent   -> insitu_streaming.py bridge into a --multi-clients
#                              pvserver (the ParaView MCP server + agent attach)
#   REASON   MODE=agent only: insitu_agent.py drives insitu_mcp_server.py (the
#            ParaView MCP server) over stdio MCP; the agent inspects the streamed
#            collapse steps and calls fire_stop_simulation -> writes <out>.stop
#            -> the writer halts early.
#
# Delta specifics baked in:
#  * ParaView = the spack build paraview@5.13.3 +osmesa (hash below): cn024/cn045
#    are CPU nodes (no GPU/EGL) with no X server, so rendering needs OSMesa
#    software GL. This build is ~x (native offscreen), embeds adios2@2.11 (matches
#    the writer, reads the SST stream), keeps the PV5.13 Fides API the reader
#    scripts target, and its pvpython imports mcp+anthropic. There are TWO
#    paraview@5.13.3 installs (the other is the +x one that cannot headless-render),
#    so ParaView is loaded BY HASH, never `spack load paraview`.
#  * The writer needs the iowarp env (CI/Delta/env.sh); ParaView must NOT inherit
#    it (iowarp's numpy pollutes) -> writer and ParaView run in separate subshells.
#
# Usage:
#   CI/Delta/run_gray_scott_agent.sh                 # MODE=render (default; no key)
#   MODE=agent ANTHROPIC_API_KEY=sk-ant-... \
#       CI/Delta/run_gray_scott_agent.sh             # full agent loop
# =============================================================================
set -uo pipefail

# ----- config -----
REPO="${REPO:-/u/hxu13/software/coeus-adapter}"
ENV_SH="$REPO/CI/Delta/env.sh"
SPACK_SETUP="${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh"
INSITU="$REPO/test/insitu_agent"
CATALYST="$REPO/test/real_apps/gray-scott/catalyst"

# The headless (+osmesa, ~x) paraview@5.13.3 spack hash. Disambiguates from the
# +x install. Rebuild: spack install paraview@5.13.3 +adios2+fides+mpi+python~qt
#                                     ^mesa~glx+osmesa
PV_HASH="${PV_HASH:-/7fqo3o7}"

MODE="${MODE:-render}"                 # render | agent
PORT="${PORT:-11112}"                  # pvserver port
MODEL="${MODEL:-claude-haiku-4-5}"     # agent model
RUNDIR="${RUNDIR:-$HOME/iowarp}"       # writer cwd -> gs.bp.sst lands here
STREAM="gs.bp"                          # CatalystStream in the generated adios2.xml
OUT_FILE="${OUT_FILE:-$HOME/iowarp/gs_warn.bp}"   # must match the pipeline out_file
STOP_FLAG="$OUT_FILE.stop"
NUM_STEPS="${NUM_STEPS:-4}"            # = trigger_inspect_steps (render mode)
MAX_WALL="${MAX_WALL:-300}"           # agent wall-time cap (s)
SSTFILE="$RUNDIR/$STREAM.sst"
STATUS_FILE="$INSITU/streaming_status.json"
SHOT_FILE="/tmp/gs_bridge_view_${USER}.png"

mkdir -p "$RUNDIR"
log() { echo "[$(date +%H:%M:%S)] $*"; }

# Load the headless spack paraview into a clean (no-iowarp) subshell.
pv_env() { source "$SPACK_SETUP" >/dev/null 2>&1; spack load "$PV_HASH"; }

pids=()
cleanup() {
    log "cleanup: tearing down"
    pkill -u "$USER" -f '[a]dios2-gray-scott' 2>/dev/null
    ( source "$ENV_SH" >/dev/null 2>&1; jarvis ppl kill >/dev/null 2>&1 ) &
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null; done
    pkill -u "$USER" -f '[p]vserver --server-port='"$PORT" 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT INT TERM

# ----- pre-flight -----
if [[ "$MODE" == "agent" && -z "${ANTHROPIC_API_KEY:-}" ]]; then
    echo "ERROR: MODE=agent needs ANTHROPIC_API_KEY exported." >&2; exit 1
fi
if [[ ! -f "$ENV_SH" || ! -f "$SPACK_SETUP" ]]; then
    echo "ERROR: missing $ENV_SH or $SPACK_SETUP" >&2; exit 1
fi
log "MODE=$MODE  PORT=$PORT  RUNDIR=$RUNDIR  node=$(hostname -s)  paraview=$PV_HASH"

# ----- clean stale state -----
log "cleaning stale contact files + prior deployment"
rm -f "$SSTFILE" "$HOME/$STREAM.sst" "$STOP_FLAG" "$STATUS_FILE" "$SHOT_FILE" "$RUNDIR"/output-*.png
pkill -u "$USER" -f '[p]vserver --server-port='"$PORT" 2>/dev/null
( source "$ENV_SH" >/dev/null 2>&1; jarvis ppl kill >/dev/null 2>&1 )
sleep 2

# ----- 1. pvserver (agent mode only; headless OSMesa) -----
if [[ "$MODE" == "agent" ]]; then
    log "starting fresh pvserver on :$PORT (headless osmesa)"
    ( pv_env
      exec pvserver --multi-clients --server-port="$PORT" \
           --force-offscreen-rendering ) &
    pids+=($!)
    sleep 6
fi

# ----- 2. writer (jarvis pipeline) in the iowarp env, cwd=RUNDIR -----
log "launching writer (jarvis ppl run) -- will block at SST rendezvous"
( source "$ENV_SH" >/dev/null 2>&1; cd "$RUNDIR" && jarvis ppl run ) \
    > "$RUNDIR/writer.log" 2>&1 &
pids+=($!)

# ----- 3. wait for the SST contact file -----
log "waiting for $SSTFILE (writer reaching rendezvous)"
for _ in $(seq 1 120); do
    [[ -f "$SSTFILE" ]] && break
    grep -q "not launch\|prterun was unable" "$RUNDIR/writer.log" 2>/dev/null && {
        echo "ERROR: writer mpirun failed -- see $RUNDIR/writer.log" >&2
        tail -20 "$RUNDIR/writer.log" >&2; exit 1; }
    sleep 2
done
[[ -f "$SSTFILE" ]] || { echo "ERROR: $SSTFILE never appeared" >&2; tail -30 "$RUNDIR/writer.log" >&2; exit 1; }
log "rendezvous ready: $SSTFILE"

# ----- 4a. RENDER mode: pvbatch reader, headless, no LLM, no API key -----
if [[ "$MODE" == "render" ]]; then
    log "RENDER: pvbatch reader (num-steps=$NUM_STEPS) -- waits for the trigger, "
    log "        renders the flagged window via OSMesa, then exits"
    ( pv_env
      cd "$RUNDIR"
      exec pvbatch "$CATALYST/gs-pipeline.py" \
           -j "$CATALYST/gs-fides.json" \
           -b "$RUNDIR/$STREAM" --staging --num-steps "$NUM_STEPS" )
    log "RENDER done. PNGs (output-*.png) in $RUNDIR:"
    ls -la "$RUNDIR"/output-*.png 2>/dev/null | sed 's/^/  /'
    exit 0
fi

# ----- 4b. AGENT mode: bridge (pvpython) + MCP server (pvpython) + agent (spack py) -----
log "AGENT: starting streaming bridge (pvpython) --paused"
( pv_env
  exec pvpython "$INSITU/insitu_streaming.py" \
       -j "$INSITU/gs-fides.json" \
       -b "$RUNDIR/$STREAM" --staging \
       --server localhost --port "$PORT" --paused \
       --status-file "$STATUS_FILE" \
       --screenshot-file "$SHOT_FILE" --max-steps 8 ) \
    > "$RUNDIR/bridge.log" 2>&1 &
pids+=($!)
sleep 6

# Agent = spack python (mcp+anthropic); it spawns the MCP server via --pvpython
# (this paraview's pvpython, which imports paraview_manager AND mcp natively).
log "AGENT: launching insitu_agent.py ($MODEL); MCP server spawned via pvpython"
( source "$SPACK_SETUP" >/dev/null 2>&1
  PV="$(spack location -i "$PV_HASH")"
  SPY="$(spack location -i python)/bin/python3"
  cd "$INSITU"
  exec env \
    PYTHONPATH="$PV/lib/python3.12/site-packages:$HOME/software/paraview_mcp" \
    LD_LIBRARY_PATH="$PV/lib" \
    ANTHROPIC_API_KEY="$ANTHROPIC_API_KEY" \
    "$SPY" insitu_agent.py \
        --provider anthropic --model "$MODEL" \
        --pvpython "$PV/bin/pvpython" \
        --server-host localhost --server-port "$PORT" \
        --status-file "$STATUS_FILE" \
        --screenshot-file "$SHOT_FILE" \
        --stop-flag "$STOP_FLAG" \
        --results-dir "$INSITU/agent_results" \
        --max-iterations 15 --max-wall-seconds "$MAX_WALL" \
        --prompt "You are inspecting a gated collapse window of exactly $NUM_STEPS \
streamed steps from a live Gray-Scott run. For each step: call advance_step, then \
get_screenshot, and note the V field. As soon as the isosurface has vanished and \
the field looks spatially uniform (the pattern has expanded to blank) — or after \
you have seen all $NUM_STEPS frames — call fire_stop_simulation with a one-line \
reason. IMPORTANT: call advance_step at most $NUM_STEPS times; the stream ships \
nothing after this window, so any further advance_step just blocks. Do not stall — \
issue the fire verdict promptly once the collapse is clear." )

log "AGENT done. Verdict + screenshots in $INSITU/agent_results;"
log "stop flag (if fired): $STOP_FLAG ; writer log: $RUNDIR/writer.log"

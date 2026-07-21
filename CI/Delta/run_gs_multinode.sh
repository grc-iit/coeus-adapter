#!/bin/bash
# =============================================================================
# MULTI-NODE Gray-Scott Trigger-Render-Reason (Vigil) on Delta, one SLURM job:
#   PRODUCER  256 gray-scott ranks across 2 nodes (128/node) via jarvis+srun+PMIx
#             (pipeline gray-scott-warn-mn.yaml; clio daemons on both nodes via
#             the ssh shim's srun-remote path).
#   CONSUMER  pvserver + bridge + ParaView MCP server + agent on a 3rd node,
#             reading the gated SST stream via the NFS contact file (CI/Delta/
#             gs_consumer.sh, launched with srun --nodelist).
#   REASON    the agent inspects the streamed collapse window and calls
#             fire_stop_simulation -> the 256-rank writer halts early.
#
# Prereqs: inside a SLURM allocation containing PRODUCER_NODES + CONSUMER_NODE;
#          jarvis hostfile = PRODUCER_NODES; pipeline loaded from
#          gray-scott-warn-mn.yaml with srun_nodelist=PRODUCER_NODES; env built.
#
# Usage:  MODE handled by the pipeline (agent). Provide the key:
#   ANTHROPIC_API_KEY=sk-ant-... bash CI/Delta/run_gs_multinode.sh
# =============================================================================
set -uo pipefail

REPO="${REPO:-/u/hxu13/software/coeus-adapter}"
# Overridable so the iowarp@dev variant (CI/Delta/env-dev.sh -> build-dev/) can
# reuse this orchestrator verbatim. Was hardcoded, which silently put the
# iowarp@main build/bin on PATH even when the caller had sourced env-dev.sh.
ENV_SH="${ENV_SH:-$REPO/CI/Delta/env.sh}"
INSITU="$REPO/test/insitu_agent"
PV_HASH="${PV_HASH:-/7fqo3o7}"
PRODUCER_NODES="${PRODUCER_NODES:-cn024,cn046}"
CONSUMER_NODE="${CONSUMER_NODE:-cn071}"
PORT="${PORT:-11112}"
CONS_NPROCS="${CONS_NPROCS:-32}"          # parallel pvserver ranks on the consumer node
MODEL="${MODEL:-claude-haiku-4-5}"
RUNDIR="${RUNDIR:-$HOME/iowarp}"
STREAM="$RUNDIR/gs.bp"; SSTFILE="$STREAM.sst"
OUT_FILE="${OUT_FILE:-$HOME/iowarp/gs_warn.bp}"; STOP_FLAG="$OUT_FILE.stop"
STATUS_FILE="$INSITU/streaming_status.json"
SHOT_FILE="$RUNDIR/bridge_view_mn.png"
NUM_STEPS="${NUM_STEPS:-4}"; MAX_WALL="${MAX_WALL:-600}"
# Producer and consumer may live in DIFFERENT SLURM jobs (SST crosses jobs via
# the NFS contact file + TCP). PRODUCER_JID drives jarvis/srun for the writer
# ranks + clio daemons; CONSUMER_JID drives the pvserver + client-stack sruns.
PRODUCER_JID="${PRODUCER_JID:-${SLURM_JOB_ID:?set PRODUCER_JID or SLURM_JOB_ID}}"
CONSUMER_JID="${CONSUMER_JID:-$PRODUCER_JID}"

[[ -z "${ANTHROPIC_API_KEY:-}" ]] && { echo "ERROR: export ANTHROPIC_API_KEY" >&2; exit 1; }
mkdir -p "$RUNDIR"
log(){ echo "[orch $(date +%H:%M:%S)] $*"; }

kill_all(){
    ( source "$ENV_SH" >/dev/null 2>&1; jarvis ppl kill >/dev/null 2>&1 )
    pkill -9 -u "$USER" -f '[a]dios2-gray-scott' 2>/dev/null
    pkill -9 -u "$USER" -f '[c]lio_run runtime' 2>/dev/null
    for n in ${PRODUCER_NODES//,/ }; do
        srun --jobid="$PRODUCER_JID" -w "$n" --overlap --ntasks=1 bash -c \
          'pkill -9 -f "[c]lio_run runtime"; pkill -9 -f "[a]dios2-gray-scott"; pkill -9 -f "[p]vserver"; pkill -9 -f "insitu_"' 2>/dev/null
    done
    srun --jobid="$CONSUMER_JID" -w "$CONSUMER_NODE" --overlap --ntasks=1 bash -c \
      'pkill -9 -f "[c]lio_run runtime"; pkill -9 -f "[a]dios2-gray-scott"; pkill -9 -f "[p]vserver"; pkill -9 -f "insitu_"' 2>/dev/null
}
cleanup(){ log "cleanup"; kill_all; }
trap cleanup EXIT INT TERM

log "PRODUCER=$PRODUCER_NODES (256 ranks)  CONSUMER=$CONSUMER_NODE  paraview=$PV_HASH"
log "cleaning stale state"
kill_all; sleep 2
rm -f "$SSTFILE" "$STOP_FLAG" "$STATUS_FILE" "$SHOT_FILE" "$RUNDIR"/output-*.png
rm -rf "$INSITU/agent_results/screenshots"/* 2>/dev/null

# ----- producer (jarvis; blocks at SST rendezvous) -----
log "launching producer (jarvis ppl run) -> writer_mn.log"
( source "$ENV_SH" >/dev/null 2>&1
  export SLURM_JOB_ID="$PRODUCER_JID"   # pkg.py + ssh shim srun into THIS job
  cd "$RUNDIR" && jarvis ppl run ) \
    > "$RUNDIR/writer_mn.log" 2>&1 &

log "waiting for rendezvous ($SSTFILE)"
for _ in $(seq 1 150); do
    [[ -f "$SSTFILE" ]] && break
    sleep 2
done
[[ -f "$SSTFILE" ]] || { echo "ERROR: no rendezvous; see $RUNDIR/writer_mn.log" >&2
    grep -iE "error|CRITICAL|not launch" "$RUNDIR/writer_mn.log" | tail -10 >&2; exit 1; }
log "rendezvous ready"

# ----- consumer: parallel pvserver (its own srun step) + client stack -----
export PV_HASH PORT STREAM STATUS_FILE SHOT_FILE STOP_FLAG MODEL INSITU MAX_WALL \
       NUM_STEPS ANTHROPIC_API_KEY CONS_NPROCS

# Parallel pvserver MUST be launched with mpirun (local fork on the consumer
# node), NOT `srun --mpi=pmix`: srun+PMIx-launched reader cohorts hang the SST
# N-to-M rendezvous (isolated 2026-07-17; matches the Ares-proven run-sst.sh,
# which used mpirun). SLURM_* env is scrubbed inside the step so mpirun's slurm
# RAS doesn't miscount the 1-task step and refuse -np N. Single node only —
# cross-node readers hang in IceT compositing (run-sst.sh note).
log "launching ${CONS_NPROCS}-rank pvserver on $CONSUMER_NODE (mpirun local fork, headless osmesa)"
srun --jobid="$CONSUMER_JID" --nodelist="$CONSUMER_NODE" -N1 -n1 -c 64 --overlap \
     --export=ALL bash -c '
        for v in $(env | awk -F= "/^SLURM/{print \$1}"); do unset $v; done
        unset PYTHONPATH LD_LIBRARY_PATH
        source "${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh" >/dev/null 2>&1
        spack load "'"$PV_HASH"'"
        export OMPI_MCA_pml=ob1 OMPI_MCA_btl=tcp,self OMPI_MCA_osc=^ucx
        export OMPI_MCA_btl_tcp_if_include=hsn0 OMPI_MCA_oob_tcp_if_include=hsn0
        exec mpirun -np '"$CONS_NPROCS"' pvserver --multi-clients \
             --server-port='"$PORT"' --force-offscreen-rendering \
             --disable-xdisplay-test
     ' > "$RUNDIR/pvserver_mn.log" 2>&1 &
PVSERVER_SRUN_PID=$!

log "launching consumer client (bridge + MCP + agent) on $CONSUMER_NODE"
srun --jobid="$CONSUMER_JID" --nodelist="$CONSUMER_NODE" -N1 --ntasks=1 --overlap \
     --export=ALL bash "$REPO/CI/Delta/gs_consumer.sh"

kill "$PVSERVER_SRUN_PID" 2>/dev/null
log "consumer finished. stop flag: $STOP_FLAG"
ls -la "$STOP_FLAG" 2>/dev/null && cat "$STOP_FLAG" 2>/dev/null

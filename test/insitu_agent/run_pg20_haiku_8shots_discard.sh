#!/bin/bash
# Discard-policy variant of run_pg20_haiku_8shots.sh.
#
# Same config (L=512, plotgap=20, sim 8n/128p, pvserver 4n/64p, Haiku 8-shot
# prompt, bridge max-steps=20) but the simulation uses
# adios2-sst-discard.xml instead of adios2-sst-blocking.xml:
#   - RendezvousReaderCount=1 (writer still waits for the bridge to connect)
#   - QueueLimit=5
#   - QueueFullPolicy=Discard  ← NEW: writer drops data instead of blocking
#
# Hypothesis: this should let the sim run at its native ~12 s/output cadence
# (total ~240 s sim wall) without being rate-limited by the slow agent. The
# bridge may see fewer than 20 outputs (some get discarded) but the overall
# wall time should drop from the 522 s we measured under Block policy.
#
# Writes to results/l512_pg20_8shots/AG_Haiku_8shots_Discard/.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="${SCRIPT_DIR}/results/l512_pg20_8shots/AG_Haiku_8shots_Discard"
mkdir -p "${RESULTS_DIR}"

GS_EXE="/home/hxu40/software/gray-scott/build/adios2-gray-scott"
SSH_WRAPPER="/home/hxu40/software/gray-scott/ssh-spack-wrapper.sh"
PVPYTHON="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython"

SIM_HOSTS="ares-comp-10:16,ares-comp-11:16,ares-comp-12:16,ares-comp-13:16,ares-comp-14:16,ares-comp-15:16,ares-comp-16:16,ares-comp-17:16"
SIM_NP=128
PV_HOSTS="ares-comp-21:16,ares-comp-22:16,ares-comp-18:16,ares-comp-19:16"
PV_NP=64
PV_SERVER_HOST="ares-comp-21"

SETTINGS_JSON="settings-staging-512-pg20-discard.json"

eval $(spack load --sh adios2@2.11.0/g)
eval $(spack load --sh paraview)
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1

export ANTHROPIC_BASE_URL="https://yxai.anthropic.edu.pl"
export ANTHROPIC_API_KEY="sk-4TbsHfCmmzbsvw4Ynzk2tiZDWMIv1jwtL1x94ELsgnxYUDHR"

echo "============================================="
echo "  L=512 plotgap=20 Haiku 8-shot — DISCARD policy"
echo "  Sim: ${SIM_NP}p / 8 nodes"
echo "  pvserver: ${PV_NP}p / 4 nodes"
echo "  Results: ${RESULTS_DIR}"
echo "============================================="

cleanup() {
    set +e
    [ -n "${BRIDGE_PID:-}" ] && kill ${BRIDGE_PID} 2>/dev/null
    [ -n "${PVSERVER_PID:-}" ] && kill ${PVSERVER_PID} 2>/dev/null
    [ -n "${SIM_PID:-}" ] && kill ${SIM_PID} 2>/dev/null
    sleep 2
    pkill -9 -f "pvserver --multi-clients" 2>/dev/null
    pkill -9 -f "${SETTINGS_JSON}" 2>/dev/null
    pkill -9 -f "insitu_streaming.py" 2>/dev/null
    set -e
}
trap cleanup EXIT

# 0. Cleanup leftover files
rm -f "${SCRIPT_DIR}/gs.bp.sst" "${SCRIPT_DIR}/streaming_status.json" "${SCRIPT_DIR}/streaming_command.json"

# 1. pvserver
echo "[$(date)] Starting pvserver (np=${PV_NP})..."
nohup mpirun \
    --mca plm_rsh_agent "${SSH_WRAPPER}" \
    --mca plm_ssh_no_tree_spawn 1 \
    --bind-to none \
    --host ${PV_HOSTS} \
    -np ${PV_NP} \
    bash -c "
        export OMPI_MCA_pml=ob1
        export OMPI_MCA_btl=tcp,self
        export OMPI_MCA_osc='^ucx'
        export OMPI_MCA_btl_tcp_if_include=eno1
        export OMPI_MCA_oob_tcp_if_include=eno1
        eval \$(spack load --sh paraview)
        pvserver --multi-clients --server-port=11112 --force-offscreen-rendering
    " > "${RESULTS_DIR}/pvserver.log" 2>&1 &
PVSERVER_PID=$!
sleep 25

# 2. Simulation (Discard policy)
echo "[$(date)] Starting Gray-Scott (L=512, 400 steps, plotgap=20, Discard)..."
WALL_START=$(date +%s.%N)
nohup mpirun \
    --mca plm_rsh_agent "${SSH_WRAPPER}" \
    --mca plm_ssh_no_tree_spawn 1 \
    --host ${SIM_HOSTS} \
    -np ${SIM_NP} \
    bash -c "
        export OMPI_MCA_pml=ob1
        export OMPI_MCA_btl=tcp,self
        export OMPI_MCA_osc='^ucx'
        export OMPI_MCA_btl_tcp_if_include=eno1
        export OMPI_MCA_oob_tcp_if_include=eno1
        cd ${SCRIPT_DIR}
        ${GS_EXE} ${SETTINGS_JSON} 0
    " > "${RESULTS_DIR}/gray-scott.log" 2>&1 &
SIM_PID=$!

# 3. Wait for SST contact file
echo "[$(date)] Waiting for SST contact file..."
for i in $(seq 1 90); do
    [ -f "${SCRIPT_DIR}/gs.bp.sst" ] && { echo "[$(date)] SST contact after ${i}s"; break; }
    sleep 2
done
[ ! -f "${SCRIPT_DIR}/gs.bp.sst" ] && { echo "TIMEOUT"; exit 1; }

# 4. Bridge (NO --max-steps here; with Discard the bridge may see fewer than 20
#    outputs because some will be dropped. We let END_OF_STREAM do the work.
#    The agent's --max-wall-seconds 600 and shell timeout 720 still protect us.)
BRIDGE_SCREENSHOT="${SCRIPT_DIR}/bridge_latest.png"
rm -f "${BRIDGE_SCREENSHOT}"
echo "[$(date)] Starting streaming bridge..."
nohup ${PVPYTHON} -u "${SCRIPT_DIR}/insitu_streaming.py" \
    -j "${SCRIPT_DIR}/gs-fides.json" \
    -b "${SCRIPT_DIR}/gs.bp" \
    --staging --server ${PV_SERVER_HOST} --port 11112 \
    --paused \
    --max-steps 25 \
    --timing-file "${RESULTS_DIR}/streaming_timing.jsonl" \
    --screenshot-file "${BRIDGE_SCREENSHOT}" \
    > "${RESULTS_DIR}/streaming_bridge.log" 2>&1 &
BRIDGE_PID=$!

for i in $(seq 1 30); do
    [ -f "${SCRIPT_DIR}/streaming_status.json" ] && { echo "[$(date)] Bridge ready after ${i}s"; break; }
    sleep 2
done

# 5. Haiku agent with 8-shot prompt (same prompt as Block variant)
echo "[$(date)] Running Haiku agent with 8-shot prompt..."
timeout --signal=TERM --kill-after=30 720 \
    /usr/bin/python3 "${SCRIPT_DIR}/insitu_agent.py" \
        --provider anthropic \
        --model claude-haiku-4-5-20251001 \
        --pvpython "${PVPYTHON}" \
        --server-host ${PV_SERVER_HOST} \
        --server-port 11112 \
        --timing-file "${RESULTS_DIR}/mcp_tool_timing.jsonl" \
        --screenshot-file "${BRIDGE_SCREENSHOT}" \
        --results-dir "${RESULTS_DIR}" \
        --prompt-file "${SCRIPT_DIR}/prompts/agent_middle_8shots.txt" \
        --max-iterations 150 \
        --max-wall-seconds 600 \
    > "${RESULTS_DIR}/consumer.log" 2>&1
CONSUMER_EXIT=$?
echo "[$(date)] Agent exit: ${CONSUMER_EXIT}"

# 6. Drain + wait for sim
echo '{"action": "resume"}' > "${SCRIPT_DIR}/streaming_command.json"
sleep 3

TIMEOUT=300
elapsed=0
while kill -0 ${SIM_PID} 2>/dev/null; do
    sleep 2
    elapsed=$((elapsed + 2))
    if [ $elapsed -ge $TIMEOUT ]; then
        echo "[$(date)] Sim timeout after ${TIMEOUT}s — killing"
        kill ${SIM_PID} 2>/dev/null
        break
    fi
done

WALL_END=$(date +%s.%N)
WALL_TOTAL=$(echo "${WALL_END} - ${WALL_START}" | bc)

cat > "${RESULTS_DIR}/config.json" << EOF
{
    "variant": "AG_Haiku_8shots_Discard",
    "L": 512,
    "steps": 400,
    "plotgap": 20,
    "output_steps": 20,
    "target_screenshots": 8,
    "sst_policy": "Discard",
    "queue_limit": 5,
    "sim_hosts": "${SIM_HOSTS}",
    "sim_nprocs": ${SIM_NP},
    "pvserver_hosts": "${PV_HOSTS}",
    "pvserver_nprocs": ${PV_NP},
    "wall_time_s": ${WALL_TOTAL},
    "consumer_exit": ${CONSUMER_EXIT}
}
EOF

echo "============================================="
echo "  Done. Wall time: ${WALL_TOTAL}s"
echo "  Results: ${RESULTS_DIR}"
echo "============================================="

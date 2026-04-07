#!/bin/bash
# Orchestration script for the L=512 BL vs AG comparison.
#
# Sim:      8 nodes / 128 procs (ares-comp-10..17), L=512, 400 steps, plotgap=10
# pvserver: 4 nodes / 64 procs (ares-comp-21,22,18,19)  [proven sweet spot]
# Total SST output steps per run: 40
#
# Usage:
#   bash run_bl_vs_ag_512.sh <VARIANT>
#
# Variants:
#   BL_K1, BL_K2, BL_K5, BL_K10
#   AG_Haiku_Open, AG_Haiku_Goal, AG_Sonnet_Open, AG_Sonnet_Goal

set -e

VARIANT="${1:?Usage: $0 <VARIANT>}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_ROOT="${SCRIPT_DIR}/results/bl_vs_ag_512"
RESULTS_DIR="${RESULTS_ROOT}/${VARIANT}"
mkdir -p "${RESULTS_DIR}"

GS_EXE="/home/hxu40/software/gray-scott/build/adios2-gray-scott"
SSH_WRAPPER="/home/hxu40/software/gray-scott/ssh-spack-wrapper.sh"
PVPYTHON="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython"

# Sim and pvserver topology
SIM_HOSTS="ares-comp-10:16,ares-comp-11:16,ares-comp-12:16,ares-comp-13:16,ares-comp-14:16,ares-comp-15:16,ares-comp-16:16,ares-comp-17:16"
SIM_NP=128
PV_HOSTS="ares-comp-21:16,ares-comp-22:16,ares-comp-18:16,ares-comp-19:16"
PV_NP=64
PV_SERVER_HOST="ares-comp-21"   # bridge + agent connect here

SETTINGS_JSON="settings-staging-512-blocking.json"

# Environment
eval $(spack load --sh adios2@2.11.0/g)
eval $(spack load --sh paraview)
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1

# Anthropic SDK (used only by AG_* variants)
export ANTHROPIC_BASE_URL="https://yxai.anthropic.edu.pl"
export ANTHROPIC_API_KEY="sk-4TbsHfCmmzbsvw4Ynzk2tiZDWMIv1jwtL1x94ELsgnxYUDHR"

echo "============================================="
echo "  L=512 BL vs AG run: ${VARIANT}"
echo "  Sim: ${SIM_NP} procs on 8 nodes"
echo "  pvserver: ${PV_NP} procs on 4 nodes"
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

# 1. Start pvserver (4 nodes / 64 procs)
echo "[$(date)] Starting pvserver (np=${PV_NP}, hosts=${PV_HOSTS})..."
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
echo "[$(date)] pvserver PID: ${PVSERVER_PID}"

# 2. Start simulation (8 nodes / 128 procs)
echo "[$(date)] Starting Gray-Scott (L=512, 400 steps, plotgap=10)..."
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
    if [ -f "${SCRIPT_DIR}/gs.bp.sst" ]; then
        echo "[$(date)] SST contact file found after ${i}s"
        break
    fi
    sleep 2
done
if [ ! -f "${SCRIPT_DIR}/gs.bp.sst" ]; then
    echo "[$(date)] TIMEOUT waiting for SST contact file"
    exit 1
fi

# 4. Start streaming bridge (paused — consumer drives it)
echo "[$(date)] Starting streaming bridge..."
nohup ${PVPYTHON} -u "${SCRIPT_DIR}/insitu_streaming.py" \
    -j "${SCRIPT_DIR}/gs-fides.json" \
    -b "${SCRIPT_DIR}/gs.bp" \
    --staging --server ${PV_SERVER_HOST} --port 11112 \
    --paused \
    --timing-file "${RESULTS_DIR}/streaming_timing.jsonl" \
    > "${RESULTS_DIR}/streaming_bridge.log" 2>&1 &
BRIDGE_PID=$!

echo "[$(date)] Waiting for bridge to initialize..."
for i in $(seq 1 30); do
    if [ -f "${SCRIPT_DIR}/streaming_status.json" ]; then
        echo "[$(date)] Bridge ready after ${i}s"
        break
    fi
    sleep 2
done

# 5. Run the consumer (BL or AG)
echo "[$(date)] Running consumer for variant ${VARIANT}..."
case "${VARIANT}" in
    BL_K1)  K=1 ;;
    BL_K2)  K=2 ;;
    BL_K5)  K=5 ;;
    BL_K10) K=10 ;;
    AG_*)   K="" ;;
    *) echo "Unknown variant ${VARIANT}"; exit 1 ;;
esac

# Consumer wall-time cap. At L=512 the sim needs ~4-5 min, so 12 min is a
# generous safety net. If the agent (or bridge) ends up wedged on stale data,
# this prevents one bad variant from burning the whole matrix.
CONSUMER_TIMEOUT=720

if [ -n "${K}" ]; then
    # Baseline runner
    timeout --signal=TERM --kill-after=30 ${CONSUMER_TIMEOUT} \
        /usr/bin/python3 "${SCRIPT_DIR}/run_baseline.py" \
            --K ${K} \
            --results-dir "${RESULTS_DIR}" \
            --pvpython "${PVPYTHON}" \
            --server-host ${PV_SERVER_HOST} \
            --server-port 11112 \
        > "${RESULTS_DIR}/consumer.log" 2>&1
    CONSUMER_EXIT=$?
else
    # Agent runner
    case "${VARIANT}" in
        AG_Haiku_Open)  MODEL=claude-haiku-4-5-20251001;  PROMPT_FILE=prompts/agent_open.txt ;;
        AG_Haiku_Goal)  MODEL=claude-haiku-4-5-20251001;  PROMPT_FILE=prompts/agent_goal.txt ;;
        AG_Sonnet_Open) MODEL=claude-sonnet-4-5-20250929; PROMPT_FILE=prompts/agent_open.txt ;;
        AG_Sonnet_Goal) MODEL=claude-sonnet-4-5-20250929; PROMPT_FILE=prompts/agent_goal.txt ;;
    esac
    timeout --signal=TERM --kill-after=30 ${CONSUMER_TIMEOUT} \
        /usr/bin/python3 "${SCRIPT_DIR}/insitu_agent.py" \
            --provider anthropic \
            --model ${MODEL} \
            --pvpython "${PVPYTHON}" \
            --server-host ${PV_SERVER_HOST} \
            --server-port 11112 \
            --timing-file "${RESULTS_DIR}/mcp_tool_timing.jsonl" \
            --results-dir "${RESULTS_DIR}" \
            --prompt-file "${SCRIPT_DIR}/${PROMPT_FILE}" \
            --max-iterations 150 \
            --max-wall-seconds 600 \
        > "${RESULTS_DIR}/consumer.log" 2>&1
    CONSUMER_EXIT=$?
fi
echo "[$(date)] Consumer finished (exit: ${CONSUMER_EXIT})"

# 6. Drain the stream
echo "[$(date)] Draining remaining steps..."
echo '{"action": "resume"}' > "${SCRIPT_DIR}/streaming_command.json"
sleep 3

# Wait for sim to finish (with timeout)
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
echo "[$(date)] Total wall time: ${WALL_TOTAL}s"

# 7. Save config metadata
cat > "${RESULTS_DIR}/config.json" << EOF
{
    "variant": "${VARIANT}",
    "L": 512,
    "steps": 400,
    "plotgap": 10,
    "output_steps": 40,
    "sim_hosts": "${SIM_HOSTS}",
    "sim_nprocs": ${SIM_NP},
    "pvserver_hosts": "${PV_HOSTS}",
    "pvserver_nprocs": ${PV_NP},
    "wall_time_s": ${WALL_TOTAL},
    "consumer_exit": ${CONSUMER_EXIT}
}
EOF

echo "============================================="
echo "  ${VARIANT} complete"
echo "  Wall time: ${WALL_TOTAL}s"
echo "  Results in: ${RESULTS_DIR}"
echo "============================================="

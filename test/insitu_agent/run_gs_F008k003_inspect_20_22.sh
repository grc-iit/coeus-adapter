#!/bin/bash
# Gray-Scott L=256, F=0.08, k=0.03, dt=1.0, plotgap=50, steps=5000, noise=0.01
# → 100 SST outputs total, Haiku agent inspects outputs 20/21/22 then drains.
#
# Topology:
#   sim:      128 procs on 8 nodes (ares-comp-10..17, 16 ppn)
#   pvserver: 32 procs on 2 nodes  (ares-comp-21, 22, 16 ppn)
# SST policy: Block (so sim pauses for reader during the 3 inspections)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="${SCRIPT_DIR}/results/gs_F008k003_inspect_20_22/AG_Haiku"
mkdir -p "${RESULTS_DIR}"

GS_EXE="/home/hxu40/software/gray-scott/build/adios2-gray-scott"
SSH_WRAPPER="/home/hxu40/software/gray-scott/ssh-spack-wrapper.sh"
PVPYTHON="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython"

SIM_HOSTS="ares-comp-10:16,ares-comp-11:16,ares-comp-12:16,ares-comp-13:16,ares-comp-14:16,ares-comp-15:16,ares-comp-16:16,ares-comp-17:16"
SIM_NP=128
PV_HOSTS="ares-comp-21:16,ares-comp-22:16"
PV_NP=32
PV_SERVER_HOST="ares-comp-21"

SETTINGS_JSON="settings-gs-F008k003-blocking.json"

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
echo "  Gray-Scott L=256 F=0.08 k=0.03 dt=1.0"
echo "  plotgap=50  steps=5000  noise=0.01"
echo "  → 100 SST outputs total"
echo "  Inspect outputs 20, 21, 22 (3 screenshots)"
echo "  Sim: ${SIM_NP}p / 8 nodes"
echo "  pvserver: ${PV_NP}p / 2 nodes"
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

rm -f "${SCRIPT_DIR}/gs.bp.sst" "${SCRIPT_DIR}/streaming_status.json" "${SCRIPT_DIR}/streaming_command.json"

# 1. pvserver (2 nodes / 32 procs)
echo "[$(date)] Starting pvserver..."
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

# 2. Simulation
echo "[$(date)] Starting Gray-Scott..."
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

echo "[$(date)] Waiting for SST contact file..."
for i in $(seq 1 90); do
    [ -f "${SCRIPT_DIR}/gs.bp.sst" ] && { echo "[$(date)] SST contact after ${i}s"; break; }
    sleep 2
done
[ ! -f "${SCRIPT_DIR}/gs.bp.sst" ] && { echo "TIMEOUT"; exit 1; }

# 4. Bridge
#    --max-steps 110: covers all 100 outputs with safety margin
#    --render-steps 20,21,22: only these 3 steps do pipeline+render+save.
#                             Phase 1 (1..19) and Phase 3 (23..100) just
#                             consume SST without rendering.
BRIDGE_SCREENSHOT="${SCRIPT_DIR}/bridge_latest.png"
rm -f "${BRIDGE_SCREENSHOT}"
echo "[$(date)] Starting streaming bridge..."
nohup ${PVPYTHON} -u "${SCRIPT_DIR}/insitu_streaming.py" \
    -j "${SCRIPT_DIR}/gs-fides.json" \
    -b "${SCRIPT_DIR}/gs.bp" \
    --staging --server ${PV_SERVER_HOST} --port 11112 \
    --paused \
    --max-steps 110 \
    --render-steps 20,21,22 \
    --timing-file "${RESULTS_DIR}/streaming_timing.jsonl" \
    --screenshot-file "${BRIDGE_SCREENSHOT}" \
    > "${RESULTS_DIR}/streaming_bridge.log" 2>&1 &
BRIDGE_PID=$!

for i in $(seq 1 30); do
    [ -f "${SCRIPT_DIR}/streaming_status.json" ] && { echo "[$(date)] Bridge ready after ${i}s"; break; }
    sleep 2
done

# 5. Haiku agent with the 3-phase inspect-20-22 prompt.
#    Longer wall cap (900s) because plotgap=50 at L=256 gives ~3.3 s/output
#    and 100 outputs × 3.3 s ≈ 330 s of pure sim plus agent overhead.
echo "[$(date)] Running Haiku agent..."
timeout --signal=TERM --kill-after=30 1200 \
    /usr/bin/python3 "${SCRIPT_DIR}/insitu_agent.py" \
        --provider anthropic \
        --model claude-haiku-4-5-20251001 \
        --pvpython "${PVPYTHON}" \
        --server-host ${PV_SERVER_HOST} \
        --server-port 11112 \
        --timing-file "${RESULTS_DIR}/mcp_tool_timing.jsonl" \
        --screenshot-file "${BRIDGE_SCREENSHOT}" \
        --results-dir "${RESULTS_DIR}" \
        --prompt-file "${SCRIPT_DIR}/prompts/agent_inspect_20_22.txt" \
        --max-iterations 200 \
        --max-wall-seconds 900 \
    > "${RESULTS_DIR}/consumer.log" 2>&1
CONSUMER_EXIT=$?
echo "[$(date)] Agent exit: ${CONSUMER_EXIT}"

# 6. Agent should have already called resume_streaming. Just in case:
echo '{"action": "resume"}' > "${SCRIPT_DIR}/streaming_command.json"
sleep 3

# Sim measured rate at this config is ~13 s/output × 100 outputs ≈ 22 min.
# Bump the drain-wait timeout so the sim can complete all 100 outputs.
TIMEOUT=1800
elapsed=0
while kill -0 ${SIM_PID} 2>/dev/null; do
    sleep 2
    elapsed=$((elapsed + 2))
    if [ $elapsed -ge $TIMEOUT ]; then
        kill ${SIM_PID} 2>/dev/null
        break
    fi
done

WALL_END=$(date +%s.%N)
WALL_TOTAL=$(echo "${WALL_END} - ${WALL_START}" | bc)

cat > "${RESULTS_DIR}/config.json" << EOF
{
    "variant": "AG_Haiku_inspect_20_22",
    "L": 256,
    "F": 0.08,
    "k": 0.03,
    "dt": 1.0,
    "plotgap": 50,
    "steps": 5000,
    "noise": 0.01,
    "output_steps": 100,
    "target_screenshots": 3,
    "inspect_outputs": [20, 21, 22],
    "sst_policy": "Block",
    "sim_hosts": "${SIM_HOSTS}",
    "sim_nprocs": ${SIM_NP},
    "pvserver_hosts": "${PV_HOSTS}",
    "pvserver_nprocs": ${PV_NP},
    "wall_time_s": ${WALL_TOTAL},
    "consumer_exit": ${CONSUMER_EXIT}
}
EOF

echo "============================================="
echo "  Done. Wall: ${WALL_TOTAL}s  Results: ${RESULTS_DIR}"
echo "============================================="

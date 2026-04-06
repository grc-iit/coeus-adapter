#!/bin/bash
# Scalability evaluation: Dimension A (sim scaling, 1 pvserver proc)
# Usage: bash run_scaling_test.sh <config_id> <sim_hosts> <sim_np>
# Example: bash run_scaling_test.sh A2 "ares-comp-25:16,ares-comp-26:16,ares-comp-27:16,ares-comp-28:16" 64

set -e

CONFIG_ID="${1:?Usage: $0 <config_id> <sim_hosts> <sim_np>}"
SIM_HOSTS="${2:?Missing sim_hosts}"
SIM_NP="${3:?Missing sim_np}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="${SCRIPT_DIR}/results/run_${CONFIG_ID}"
GS_EXE="/home/hxu40/software/gray-scott/build/adios2-gray-scott"
SSH_WRAPPER="/home/hxu40/software/gray-scott/ssh-spack-wrapper.sh"
PVPYTHON="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython"

# Environment
eval $(spack load --sh adios2@2.11.0/g)
eval $(spack load --sh paraview)
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1

# Anthropic SDK
export ANTHROPIC_BASE_URL="https://yxai.anthropic.edu.pl"
export ANTHROPIC_API_KEY="sk-4TbsHfCmmzbsvw4Ynzk2tiZDWMIv1jwtL1x94ELsgnxYUDHR"

mkdir -p "${RESULTS_DIR}"

echo "============================================="
echo "  Scaling Test: ${CONFIG_ID}"
echo "  Sim hosts: ${SIM_HOSTS}"
echo "  Sim nprocs: ${SIM_NP}"
echo "  Results: ${RESULTS_DIR}"
echo "============================================="

# 0. Cleanup
rm -f "${SCRIPT_DIR}/gs.bp.sst" "${SCRIPT_DIR}/streaming_status.json" "${SCRIPT_DIR}/streaming_command.json"

# 1. Start pvserver locally (single process, B1)
echo "[$(date)] Starting pvserver on localhost..."
nohup pvserver --multi-clients --server-port=11112 --force-offscreen-rendering > "${RESULTS_DIR}/pvserver.log" 2>&1 &
PVSERVER_PID=$!
sleep 3
echo "[$(date)] pvserver PID: ${PVSERVER_PID}"

# 2. Start simulation
echo "[$(date)] Starting Gray-Scott simulation..."
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
        ${GS_EXE} settings-staging-256-blocking.json 0
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
    kill ${SIM_PID} ${PVSERVER_PID} 2>/dev/null
    exit 1
fi

# 4. Start streaming bridge with timing
echo "[$(date)] Starting streaming bridge..."
nohup ${PVPYTHON} -u "${SCRIPT_DIR}/insitu_streaming.py" \
    -j "${SCRIPT_DIR}/gs-fides.json" \
    -b "${SCRIPT_DIR}/gs.bp" \
    --staging --server localhost --port 11112 \
    --paused \
    --timing-file "${RESULTS_DIR}/streaming_timing.jsonl" \
    > "${RESULTS_DIR}/streaming_bridge.log" 2>&1 &
BRIDGE_PID=$!

# Wait for bridge to initialize
echo "[$(date)] Waiting for bridge to initialize..."
for i in $(seq 1 30); do
    if [ -f "${SCRIPT_DIR}/streaming_status.json" ]; then
        echo "[$(date)] Bridge ready after ${i}s"
        break
    fi
    sleep 2
done

# 5. Run the agent (scripted_basic test)
echo "[$(date)] Running agent (scripted_basic)..."
/usr/bin/python3 "${SCRIPT_DIR}/insitu_agent.py" \
    --provider anthropic \
    --model claude-haiku-4-5-20251001 \
    --pvpython "${PVPYTHON}" \
    --server-host localhost \
    --server-port 11112 \
    --timing-file "${RESULTS_DIR}/mcp_tool_timing.jsonl" \
    --prompt "Execute this sequence precisely:
1. get_streaming_status
2. advance_step (wait 3 seconds after)
3. get_streaming_status
4. get_available_arrays
5. create_isosurface of V at 0.3
6. get_screenshot
7. Then advance 5 more steps, taking a screenshot after each. Report each timestep number.
8. When done, report all timestep numbers visited." \
    > "${RESULTS_DIR}/agent_output.log" 2>&1

AGENT_EXIT=$?
echo "[$(date)] Agent finished (exit: ${AGENT_EXIT})"

# 6. Resume streaming to drain remaining steps
echo "[$(date)] Draining remaining steps..."
echo '{"action": "resume"}' > "${SCRIPT_DIR}/streaming_command.json"
sleep 5

# Wait for sim to finish (with timeout)
TIMEOUT=120
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

# 7. Kill remaining processes
sleep 3
kill ${BRIDGE_PID} 2>/dev/null
kill ${PVSERVER_PID} 2>/dev/null

# 8. Save config metadata
cat > "${RESULTS_DIR}/config.json" << EOF
{
    "config_id": "${CONFIG_ID}",
    "sim_hosts": "${SIM_HOSTS}",
    "sim_nprocs": ${SIM_NP},
    "L": 256,
    "steps": 200,
    "plotgap": 10,
    "output_steps": 20,
    "pvserver_procs": 1,
    "wall_time_s": ${WALL_TOTAL},
    "agent_exit": ${AGENT_EXIT}
}
EOF

echo "============================================="
echo "  ${CONFIG_ID} complete"
echo "  Wall time: ${WALL_TOTAL}s"
echo "  Results in: ${RESULTS_DIR}"
echo "============================================="

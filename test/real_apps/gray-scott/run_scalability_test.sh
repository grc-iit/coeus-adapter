#!/bin/bash
#
# Gray-Scott simulation + SST reader pipeline scalability test.
# Uses 8 nodes (already configured in server.list), L=256.
# Varies nprocs (16,32,64,128) with proportionally increasing steps.
# Simulation uses Hermes ADIOS2 engine; reader uses ParaView SST.
#

set -euo pipefail

# ---- Configuration ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="${SCRIPT_DIR}/results"
SST_CONTACT_HOME="/home/hxu40/gs.bp.sst"

READER_NODE="ares-comp-18"
NUM_NODES=8

# Test configurations: "nprocs steps"
# Steps scale proportionally: 16p=200, 32p=400, 64p=800, 128p=1600
CONFIGS=("16 200" "32 400" "64 800" "128 1600")

# Timing
SST_WAIT_TIMEOUT=180   # Max seconds to wait for SST contact file

# ---- Helper functions ----

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"
}

wait_for_sst_contact() {
    local elapsed=0
    log "Waiting for SST contact file (timeout: ${SST_WAIT_TIMEOUT}s)..."
    while [ ! -f "${SST_CONTACT_HOME}" ] && [ $elapsed -lt $SST_WAIT_TIMEOUT ]; do
        sleep 5
        elapsed=$((elapsed + 5))
        if (( elapsed % 30 == 0 )); then
            log "  Still waiting for SST contact file... (${elapsed}s)"
        fi
    done
    if [ -f "${SST_CONTACT_HOME}" ]; then
        log "SST contact file found after ${elapsed}s"
        return 0
    else
        log "ERROR: SST contact file not found after ${SST_WAIT_TIMEOUT}s"
        return 1
    fi
}

cleanup_sst_artifacts() {
    log "Cleaning up SST artifacts..."
    rm -f "${SST_CONTACT_HOME}"
}

kill_stale_processes() {
    log "Killing any stale simulation/reader processes..."
    jarvis ppl kill 2>/dev/null || true
    sleep 5
}

# ---- Setup results directory ----
mkdir -p "${RESULTS_DIR}"
CSV_FILE="${RESULTS_DIR}/scalability_results.csv"

# Write CSV header
echo "nprocs,steps,num_nodes,sim_wall_time_s,reader_wall_time_s" > "${CSV_FILE}"

log "============================================="
log "  Gray-Scott Scalability Test"
log "  L=256, engine=hermes, nodes=${NUM_NODES}"
log "  Reader node: ${READER_NODE}"
log "  Configurations: ${#CONFIGS[@]}"
log "============================================="

# ---- Main test loop ----
for config in "${CONFIGS[@]}"; do
    nprocs=$(echo "$config" | cut -d' ' -f1)
    steps=$(echo "$config" | cut -d' ' -f2)

    RUN_DIR="${RESULTS_DIR}/run_${nprocs}"
    mkdir -p "${RUN_DIR}"

    log ""
    log "============================================="
    log "  Test: nprocs=${nprocs}, steps=${steps}"
    log "============================================="

    # 1. Configure pipeline
    log "Configuring pipeline with nprocs=${nprocs}, L=256, steps=${steps}..."
    jarvis ppl conf coeus-gray-scott.jarvis_coeus.adios2_gray_scott \
        nprocs="${nprocs}" L=256 steps="${steps}" \
        > "${RUN_DIR}/conf.log" 2>&1
    log "Pipeline configured."

    # 2. Kill prior run, clean SST artifacts
    kill_stale_processes
    cleanup_sst_artifacts

    # 3. Start pipeline in background and time it
    log "Starting simulation pipeline..."
    SIM_START=$(date +%s)
    jarvis ppl run > "${RUN_DIR}/sim_stdout.log" 2> "${RUN_DIR}/sim_stderr.log" &
    SIM_PID=$!
    log "Simulation launched (PID: ${SIM_PID})"

    # 4. Wait for SST contact file
    if ! wait_for_sst_contact; then
        log "ERROR: Skipping test nprocs=${nprocs} — SST contact file never appeared"
        kill ${SIM_PID} 2>/dev/null || true
        wait ${SIM_PID} 2>/dev/null || true
        kill_stale_processes
        echo "${nprocs},${steps},${NUM_NODES},FAILED,FAILED" >> "${CSV_FILE}"
        continue
    fi

    # 5. Launch reader in background
    log "Launching SST reader on ${READER_NODE}..."
    READER_START=$(date +%s)
    READER_NODES="${READER_NODE}:16" \
        "${SCRIPT_DIR}/run-sst.sh" \
        > "${RUN_DIR}/reader_stdout.log" 2> "${RUN_DIR}/reader_stderr.log" &
    READER_PID=$!
    log "Reader launched (PID: ${READER_PID})"

    # 6. Wait for simulation to finish
    log "Waiting for simulation to complete..."
    SIM_EXIT=0
    wait ${SIM_PID} || SIM_EXIT=$?
    SIM_END=$(date +%s)
    SIM_WALL=$((SIM_END - SIM_START))
    log "Simulation finished (exit: ${SIM_EXIT}, wall time: ${SIM_WALL}s)"

    # 7. Wait for reader to finish
    log "Waiting for reader to complete..."
    READER_EXIT=0
    wait ${READER_PID} || READER_EXIT=$?
    READER_END=$(date +%s)
    READER_WALL=$((READER_END - READER_START))
    log "Reader finished (exit: ${READER_EXIT}, wall time: ${READER_WALL}s)"

    # 8. Record metrics
    echo "${nprocs},${steps},${NUM_NODES},${SIM_WALL},${READER_WALL}" >> "${CSV_FILE}"
    log "Results appended to ${CSV_FILE}"

    # 9. Cleanup
    kill_stale_processes
    cleanup_sst_artifacts

    # Collect any output PNGs
    if ls /home/hxu40/*.png 1>/dev/null 2>&1; then
        mv /home/hxu40/*.png "${RUN_DIR}/" 2>/dev/null || true
        log "Collected output PNGs to ${RUN_DIR}/"
    fi

    log "Test nprocs=${nprocs} complete."
done

log ""
log "============================================="
log "  All tests complete"
log "  Results: ${CSV_FILE}"
log "============================================="
log ""
log "Results summary:"
column -t -s',' "${CSV_FILE}"

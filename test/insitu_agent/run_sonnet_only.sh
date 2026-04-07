#!/bin/bash
# Rerun the two Sonnet variants only (after the original Sonnet_Open hung
# on a dead sim and got killed). The orchestration script now wraps each
# consumer in `timeout 720` and the agent enforces a 600s wall cap, so a
# stuck run can no longer poison the matrix.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${SCRIPT_DIR}/results/bl_vs_ag_512"

VARIANTS=(AG_Sonnet_Open AG_Sonnet_Goal)

OVERALL_START=$(date +%s)
for V in "${VARIANTS[@]}"; do
    echo
    echo "######################################################"
    echo "###  ${V}  $(date)"
    echo "######################################################"
    bash "${SCRIPT_DIR}/run_bl_vs_ag_512.sh" "${V}" \
        > "${LOG_DIR}/run_${V}.log" 2>&1
    EXIT=$?
    echo "[$(date)] ${V} exit=${EXIT}"
    sleep 30
done
OVERALL_END=$(date +%s)
echo
echo "######################################################"
echo "###  SONNET RUNS DONE — $((OVERALL_END - OVERALL_START))s"
echo "######################################################"

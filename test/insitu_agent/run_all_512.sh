#!/bin/bash
# Wrapper that runs the remaining 7 variants of the L=512 BL vs AG matrix.
# (BL_K2 was already executed as a smoke test.)
#
# Sequence: BL_K1, BL_K5, BL_K10, AG_Haiku_Open, AG_Haiku_Goal,
#           AG_Sonnet_Open, AG_Sonnet_Goal
#
# Between AG runs we sleep 30 s to give the proxy rate-limiter (45 req/min)
# headroom before the next conversation kicks off.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${SCRIPT_DIR}/results/bl_vs_ag_512"
mkdir -p "${LOG_DIR}"

VARIANTS=(BL_K1 BL_K5 BL_K10 AG_Haiku_Open AG_Haiku_Goal AG_Sonnet_Open AG_Sonnet_Goal)

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

    # Pause briefly between agent runs to let the proxy rate-limit recover
    case "${V}" in
        AG_*) sleep 30 ;;
        *)    sleep 5  ;;
    esac
done
OVERALL_END=$(date +%s)
echo
echo "######################################################"
echo "###  ALL DONE — total elapsed $((OVERALL_END - OVERALL_START))s"
echo "######################################################"

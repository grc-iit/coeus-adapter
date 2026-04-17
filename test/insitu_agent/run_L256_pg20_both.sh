#!/bin/bash
# Sequential runner: L=256 pg=20 Haiku 8-shot, Block then Discard.

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${SCRIPT_DIR}/results/l256_pg20_8shots"
mkdir -p "${LOG_DIR}"

echo "######################################################"
echo "###  BLOCK  $(date)"
echo "######################################################"
bash "${SCRIPT_DIR}/run_L256_pg20_haiku_8shots.sh" > "${LOG_DIR}/runner_block.log" 2>&1
echo "[$(date)] Block exit=$?"

sleep 20

echo "######################################################"
echo "###  DISCARD  $(date)"
echo "######################################################"
bash "${SCRIPT_DIR}/run_L256_pg20_haiku_8shots_discard.sh" > "${LOG_DIR}/runner_discard.log" 2>&1
echo "[$(date)] Discard exit=$?"

echo
echo "######################################################"
echo "###  BOTH DONE  $(date)"
echo "######################################################"

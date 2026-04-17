#!/bin/bash
# Sequential rerun of the L=512 pg=20 Haiku 8-shot test with the slice
# fix in insitu_streaming.py. Runs Block first, then Discard, with a
# small cooldown between them.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="${SCRIPT_DIR}/results/l512_pg20_8shots"

echo "######################################################"
echo "###  BLOCK  $(date)"
echo "######################################################"
bash "${SCRIPT_DIR}/run_pg20_haiku_8shots.sh" > "${LOG_DIR}/runner.log" 2>&1
echo "[$(date)] Block exit=$?"

sleep 20

echo "######################################################"
echo "###  DISCARD  $(date)"
echo "######################################################"
bash "${SCRIPT_DIR}/run_pg20_haiku_8shots_discard.sh" > "${LOG_DIR}/runner_discard.log" 2>&1
echo "[$(date)] Discard exit=$?"

echo
echo "######################################################"
echo "###  BOTH DONE  $(date)"
echo "######################################################"

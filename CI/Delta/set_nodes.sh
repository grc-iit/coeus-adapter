#!/bin/bash
# =============================================================================
# Stamp one allocation's node names into every place that must agree.
#
# Node placement on Delta lives in FOUR spots, and a stale one fails in a
# confusing way (ranks land on the consumer node, or the clio daemon never
# starts on producer #2). This writes all of them from one command:
#
#   1. the jarvis hostfile          -> PRODUCERS ONLY
#   2. srun_nodelist in the pipeline yaml
#   3. $HOME/iowarp/nodes.env       -> PRODUCER_NODES / CONSUMER_NODE for the
#                                      run_gs_multinode*.sh orchestrators
#   4. (reported) the SLURM job id the orchestrator will srun into
#
# Usage:
#   bash CI/Delta/set_nodes.sh <producer1,producer2> <consumer> [pipeline.yaml]
#
# Example:
#   bash CI/Delta/set_nodes.sh cn063,cn064 cn065
# =============================================================================
set -euo pipefail

REPO="${REPO:-/u/hxu13/software/coeus-adapter}"
PRODUCERS="${1:?usage: set_nodes.sh <prod1,prod2> <consumer> [pipeline.yaml]}"
CONSUMER="${2:?usage: set_nodes.sh <prod1,prod2> <consumer> [pipeline.yaml]}"
YAML="${3:-$REPO/test/jarvis/jarvis_coeus/pipelines/delta/gray-scott-warn-mn-dev.yaml}"

[[ -f "$YAML" ]] || { echo "ERROR: no such pipeline yaml: $YAML" >&2; exit 1; }

RUNDIR="$HOME/iowarp"; mkdir -p "$RUNDIR"
HOSTFILE="$HOME/producer_hostfile"

# --- sanity: do these nodes actually belong to a live allocation? ------------
ALLOC="$(squeue -u "$USER" -h -o '%N' 2>/dev/null | head -1)"
if [[ -n "$ALLOC" ]]; then
    MEMBERS="$(scontrol show hostnames "$ALLOC" 2>/dev/null | tr '\n' ' ')"
    for n in ${PRODUCERS//,/ } "$CONSUMER"; do
        [[ " $MEMBERS " == *" $n "* ]] \
            || echo "WARNING: $n is not in your allocation ($ALLOC)" >&2
    done
    echo "[set_nodes] allocation: $ALLOC"
else
    echo "WARNING: no live allocation found for $USER -- not validating names" >&2
fi

# --- 1. jarvis hostfile: PRODUCERS ONLY (consumer must not get writer ranks) --
printf "%s\n" ${PRODUCERS//,/ } > "$HOSTFILE"
echo "[set_nodes] hostfile $HOSTFILE:"; sed 's/^/    /' "$HOSTFILE"

# --- 2. pipeline yaml srun_nodelist -----------------------------------------
# Matches both the committed PLACEHOLDER and a previously-stamped node list.
sed -i -E "s|^([[:space:]]*srun_nodelist:[[:space:]]*)\"[^\"]*\"|\1\"$PRODUCERS\"|" "$YAML"
echo "[set_nodes] $(basename "$YAML"): $(grep -E '^\s*srun_nodelist:' "$YAML" | sed 's/^ *//')"

# --- 3. nodes.env for the orchestrators -------------------------------------
cat > "$RUNDIR/nodes.env" <<EOF
# Written by CI/Delta/set_nodes.sh -- $(date)
export PRODUCER_NODES="$PRODUCERS"
export CONSUMER_NODE="$CONSUMER"
EOF
echo "[set_nodes] wrote $RUNDIR/nodes.env"

# --- 4. report the job id the orchestrator will use --------------------------
JID="$(squeue -u "$USER" -h -o '%i' 2>/dev/null | head -1)"
echo "[set_nodes] SLURM_JOB_ID for srun steps: ${JID:-<none - start an salloc>}"

cat <<EOF

Next:
  jarvis hostfile set $HOSTFILE
  source $REPO/CI/Delta/env-dev.sh
  jarvis ppl load yaml $YAML
  jarvis ppl env build
  export ANTHROPIC_API_KEY=sk-ant-...
  bash $REPO/CI/Delta/run_gs_multinode_dev.sh
EOF

#!/bin/bash
#
# Launch ParaView Fides SST reader (consumer) for Gray-Scott simulation.
# Connects to an already-running SST writer at the simulation directory.
#

# ---- Load MPI/ADIOS2 via spack ----
eval $(spack load --sh adios2@2.11.0/g)

# ---- MPI environment for OpenMPI over TCP ----
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1

# ---- Paths ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GS_PIPELINE="${SCRIPT_DIR}/catalyst/gs-pipeline.py"
GS_FIDES_JSON="${SCRIPT_DIR}/catalyst/gs-fides.json"
SSH_WRAPPER="${SCRIPT_DIR}/ssh-spack-wrapper.sh"
# The simulation's CWD is the home directory, so the SST contact file
# (gs.bp.sst) is created there (CatalystStream=gs.bp is a relative path).
GS_BP="/home/hxu40/gs.bp"

# ---- Node assignments ----
# NOTE: Cross-node SaveScreenshot hangs due to IceT compositing issues
# with the current MPI/network config.  Use a single reader node.
READER_NODES="ares-comp-18:16"

# ---- Working directory (run from project root) ----
cd "${SCRIPT_DIR}"

echo "============================================="
echo "  Gray-Scott SST Staging Run"
echo "============================================="
echo "Reader nodes:     ${READER_NODES}"
echo "SST stream:       ${GS_BP}"
echo "============================================="

# ---- Launch the SST reader (consumer) ----
# Connects to an already-running SST writer via gs.bp.sst contact file.
# Use ssh-spack-wrapper.sh so remote nodes can find prted.
echo "[$(date)] Starting SST reader on ${READER_NODES}..."
mpirun \
    --mca plm_rsh_agent "${SSH_WRAPPER}" \
    --mca plm_ssh_no_tree_spawn 1 \
    --host ${READER_NODES} \
    -np 16 \
    bash -c "
        export OMPI_MCA_pml=ob1
        export OMPI_MCA_btl=tcp,self
        export OMPI_MCA_osc='^ucx'
        export OMPI_MCA_btl_tcp_if_include=eno1
        export OMPI_MCA_oob_tcp_if_include=eno1
        cd ${SCRIPT_DIR}
        eval \$(spack load --sh paraview)
        pvbatch ${GS_PIPELINE} \
            -j ${GS_FIDES_JSON} \
            -b ${GS_BP} \
            --staging \
            --num-steps 15
    " &
READER_PID=$!
echo "[$(date)] Reader launched (PID: ${READER_PID})"

# ---- Watchdog: kill reader when SST writer disconnects ----
# The writer skips SST Close() (to avoid blocking), so the reader never
# receives an EndOfStream signal.  The writer removes the SST contact
# file on exit; poll for its disappearance.  Fallback: overall timeout.
SST_CONTACT="${GS_BP}.sst"
MAX_WAIT=600  # seconds — absolute fallback timeout
(
    elapsed=0
    # Wait for the contact file to appear (writer may not have started yet)
    while [ ! -f "${SST_CONTACT}" ] && kill -0 ${READER_PID} 2>/dev/null; do
        sleep 1
        elapsed=$((elapsed + 1))
        [ $elapsed -ge $MAX_WAIT ] && break
    done
    # Now wait for the contact file to disappear (writer finished)
    while [ -f "${SST_CONTACT}" ] && kill -0 ${READER_PID} 2>/dev/null; do
        sleep 2
        elapsed=$((elapsed + 2))
        [ $elapsed -ge $MAX_WAIT ] && break
    done
    # Grace period: let the reader finish processing any remaining data.
    # SaveScreenshot can take 10-30s; give enough time for the current
    # step to finish and for PrepareNextStep to detect the disconnect.
    sleep 60
    if kill -0 ${READER_PID} 2>/dev/null; then
        echo "[$(date)] SST writer has exited — terminating reader"
        kill -TERM ${READER_PID} 2>/dev/null
        sleep 3
        kill -9 ${READER_PID} 2>/dev/null
    fi
) &
WATCHDOG_PID=$!

# ---- Wait for reader to finish ----
echo "[$(date)] Waiting for reader to complete..."
wait ${READER_PID}
READER_EXIT=$?
echo "[$(date)] Reader finished (exit code: ${READER_EXIT})"

# Clean up watchdog
kill ${WATCHDOG_PID} 2>/dev/null
wait ${WATCHDOG_PID} 2>/dev/null

echo "============================================="
echo "  Run complete"
echo "  Reader exit: ${READER_EXIT}"
echo "============================================="

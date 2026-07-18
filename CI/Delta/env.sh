#!/bin/bash
# COEUS-Adapter build/run environment for NCSA Delta compute nodes.
#
#   source CI/Delta/env.sh
#
# Delta has no site adios2/hermes modules (unlike CI/Ares/env.sh), so every
# dependency comes from the user spack tree.

source "${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh"

# iowarp (clio-core) @ main b3ee500e -- exports the clio::run::* / clio::cte::*
# targets and add_clio_module_client() that this tree requires.
#
# NOTE: an older iowarp@main (commit 1b8c43bd) used to be installed alongside
# this one; it only shipped chimaera::* / add_chimod_client and could not build
# coeus. It has been uninstalled, so `iowarp@main` is unambiguous again.
# If you ever need to rebuild it, plain `spack install iowarp@main` will silently
# re-concretize to that OLD commit (spack reuse) and report "already installed".
# Force it instead:  spack install --fresh iowarp@main ~adios2 ^openmpi@5.0.10
# Keep ~adios2: enabling it drags stock adios2 in alongside adios2-coeus.
spack load iowarp@main

# adios2-coeus@vigil = ADIOS2 v2.11.0 + the coeus derived variables
# (variance / mean / gradient / spectrum). Must be built +pic +shared -- `shared`
# only exists `when="+pic"` and defaults off, which would yield a static ADIOS2
# that cannot host the dlopen'd plugin. Never load stock adios2 alongside it.
#   spack install adios2-coeus@vigil +pic +shared ^openmpi@5.0.10
spack load adios2-coeus@vigil

spack load openmpi@5.0.10   # must match the MPI iowarp was built against
spack load googletest
spack load sqlite

# ADIOS2 resolves PluginLibrary=hermes_engine through these at runtime.
export COEUS_ROOT=${COEUS_ROOT:-/u/hxu13/software/coeus-adapter}
export ADIOS2_PLUGIN_PATH=$COEUS_ROOT/build/bin
export LD_LIBRARY_PATH=$COEUS_ROOT/build/bin:$LD_LIBRARY_PATH
# adios2-gray-scott lives in build/bin; mpirun/prterun resolves it via PATH.
export PATH=$COEUS_ROOT/build/bin:$PATH

# --- Multi-node launch on Delta (ssh denied between nodes) ---
# Launch across nodes with srun+PMIx (mpirun won't pick up the allocation here):
#   PMIX_MCA_gds=hash srun --jobid=$SLURM_JOB_ID --mpi=pmix -N<n> \
#       --ntasks-per-node=<ppn> -n<total> <binary-on-NFS>
# The Slurm PMIx v5 GDS uses a shmem segment the client can't open across the
# step (PMIX_ERR_FILE_OPEN_FAILURE in gds_shmem2); forcing the hash GDS fixes it.
# All binaries/configs must live on NFS ($HOME), not node-local /tmp.
export PMIX_MCA_gds=hash
export TMPDIR=${TMPDIR:-$HOME/prte_tmp}
mkdir -p "$TMPDIR" 2>/dev/null

echo "[coeus] iowarp:       $(spack location -i iowarp@main)"
echo "[coeus] adios2-coeus: $(spack location -i adios2-coeus@vigil)"
echo "[coeus] mpicc:        $(which mpicc)"

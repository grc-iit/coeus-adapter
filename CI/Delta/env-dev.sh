#!/bin/bash
# COEUS-Adapter build/run environment for NCSA Delta -- iowarp@dev variant.
#
#   source CI/Delta/env-dev.sh
#
# Same as env.sh but loads iowarp@dev (clio-core branch dev) and points the
# plugin/PATH exports at build-dev/ instead of build/. The iowarp@main build
# in build/ stays usable via the original env.sh.

source "${SPACK_ROOT:-$HOME/spack}/share/spack/setup-env.sh"

# iowarp@dev -- load BY HASH: iowarp@main coexists in the spack tree and
# `spack load iowarp` alone is ambiguous.
#   spack install --fresh iowarp@dev ~adios2 ^openmpi@5.0.10
spack load /pj67x7i

# adios2-coeus@vigil = ADIOS2 v2.11.0 + the coeus derived variables. See env.sh.
spack load adios2-coeus@vigil

spack load openmpi@5.0.10   # must match the MPI iowarp was built against
spack load googletest
spack load sqlite

# ADIOS2 resolves PluginLibrary=hermes_engine through these at runtime.
export COEUS_ROOT=${COEUS_ROOT:-/u/hxu13/software/coeus-adapter}
export ADIOS2_PLUGIN_PATH=$COEUS_ROOT/build-dev/bin
export LD_LIBRARY_PATH=$COEUS_ROOT/build-dev/bin:$LD_LIBRARY_PATH
export PATH=$COEUS_ROOT/build-dev/bin:$PATH

# --- Multi-node launch on Delta (see env.sh for the explanation) ---
export PMIX_MCA_gds=hash
export TMPDIR=${TMPDIR:-$HOME/prte_tmp}
mkdir -p "$TMPDIR" 2>/dev/null

# CONFIGURE GOTCHA (cost a debugging session 2026-07-19): with Delta's default
# PrgEnv-gnu modules loaded, a bare `cmake ..` picks CMAKE_CXX_COMPILER =
# /opt/cray/pe/craype/bin/CC, whose implicit cray-mpich includes shadow the
# spack OpenMPI mpi.h (MPI_Comm becomes int -> undefined adios2::ADIOS(string,
# int) at link). Always configure with the compilers + MPI wrappers pinned:
#   cmake -DCMAKE_C_COMPILER=/opt/rh/gcc-toolset-13/root/usr/bin/gcc \
#         -DCMAKE_CXX_COMPILER=/opt/rh/gcc-toolset-13/root/usr/bin/g++ \
#         -DMPI_C_COMPILER=$(spack location -i openmpi@5.0.10)/bin/mpicc \
#         -DMPI_CXX_COMPILER=$(spack location -i openmpi@5.0.10)/bin/mpicxx ..

echo "[coeus-dev] iowarp:       $(spack location -i /pj67x7i)"
echo "[coeus-dev] adios2-coeus: $(spack location -i adios2-coeus@vigil)"
echo "[coeus-dev] mpicc:        $(which mpicc)"
echo "[coeus-dev] plugin path:  $ADIOS2_PLUGIN_PATH"

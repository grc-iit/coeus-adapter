#!/bin/bash

# CD into git workspace
cd ${GITHUB_WORKSPACE}

set -x
set -e
set -o pipefail

# Set spack env
INSTALL_DIR="${HOME}"
SPACK_DIR=${INSTALL_DIR}/spack
. ${SPACK_DIR}/share/spack/setup-env.sh

mkdir -p "${HOME}/install"
mkdir build
pushd build
spack load cte-hermes-shm
cmake ../ \
-DCMAKE_BUILD_TYPE=Debug \
-DHSHM_ENABLE_COVERAGE=ON \
-DHSHM_ENABLE_DOXYGEN=ON \
-DHSHM_BUILD_TESTS=ON \
-DHSHM_ENABLE_CEREAL=ON \ 
-DHSHM_ENABLE_PTHREADS=ON \ 
-DHSHM_ENABLE_COMPRESS=ON \ 
-DHSHM_ENABLE_ENCRYPT=ON \ 
-DHSHM_ENABLE_ELF=ON \ 
-DCMAKE_INSTALL_PREFIX=${HOME}/install
# -DHSHM_BUILD_BENCHMARKS=ON \
make -j8
make install

export CXXFLAGS=-Wall
ctest -VV
popd

# Set proper flags for cmake to find Hermes
INSTALL_PREFIX="${HOME}/install"
export LIBRARY_PATH="${INSTALL_PREFIX}/lib:${LIBRARY_PATH}"
export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}"
export LDFLAGS="-L${INSTALL_PREFIX}/lib:${LDFLAGS}"
export CFLAGS="-I${INSTALL_PREFIX}/include:${CFLAGS}"
export CPATH="${INSTALL_PREFIX}/include:${CPATH}"
export CMAKE_PREFIX_PATH="${INSTALL_PREFIX}:${CMAKE_PREFIX_PATH}"
export CXXFLAGS="-I${INSTALL_PREFIX}/include:${CXXFLAGS}"

# Run make install unit test
cd test/unit/external
mkdir build
cd build
cmake ../
make -j8

#!/bin/bash
# SSH wrapper for OpenMPI remote launch.
# Instead of running slow "spack load" on each remote node,
# we inject the pre-resolved spack PATH and LD_LIBRARY_PATH.
#
# Usage: --mca plm_rsh_agent /path/to/ssh-spack-wrapper.sh

# Pre-resolved spack environment for adios2@2.11.0/g
# (regenerate with: eval $(spack load --sh adios2@2.11.0/g) && echo $PATH)
SPACK_PATH="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/adios2-2.11.0-gxiea6iqvtpgm6xuuws6dkfzxn7xtz4m/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/openmpi-5.0.9-ruzpfypjebiwjl5mcpko7uvqjnxkiwrd/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/prrte-4.0.0-yyzenpdjpawye6nc4a5nmqzrkpbviofm/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/pmix-6.0.0-wls2aw3mtmtlhqqjqlisy5mujqtj2gkr/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/hwloc-2.12.2-s5gjmhcykx5gjo2xat24e6lvyg5e2py3/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/libevent-2.1.12-ko6eoi5qsb4uise5trhabftiseehsshb/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/numactl-2.0.19-jbymxlgb5f2er7mmixkdshgczaq2zp27/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/openssh-10.2p1-33k3i7q76ud74fjuxkw3h4e3ogrigytg/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/openssl-3.6.1-5tze23uhdfwunqv5gamij4chlhhekk4e/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/libfabric-2.4.0-d5ouflpn4sey5dcvd5fyccj4gfggyiqt/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/krb5-1.22.2-qtou3jmzybqbb6fbt3ahlggb7yznx3cf/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/hdf5-1.12.3-5wugt7akzyi6guynidt7ei3cyyumykrn/bin:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/python-3.12.12-mmtjcqm5y4jajxdmcpgbzbnocjfk4afq/bin"

SPACK_LD_PATH="/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/libfabric-2.4.0-d5ouflpn4sey5dcvd5fyccj4gfggyiqt/lib64:/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/libfabric-2.4.0-d5ouflpn4sey5dcvd5fyccj4gfggyiqt/lib"

HOST="$1"
shift
exec ssh "$HOST" "export PATH=${SPACK_PATH}:\$PATH; export LD_LIBRARY_PATH=${SPACK_LD_PATH}:\$LD_LIBRARY_PATH; $*"

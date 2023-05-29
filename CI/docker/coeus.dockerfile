# Use scs-lab/base:1.0 as base image
FROM spack/ubuntu-jammy:latest as coeus-builder
ENV DOCKER_TAG=0.5

# What we want to install and how we want to install it
# is specified in a manifest file (spack.yaml)
RUN mkdir /opt/spack-environment \
&&  (echo "spack:" \
&&   echo "  specs:" \
&&   echo "  - hdf5" \
&&   echo "  - mpich" \
&&   echo "  - adios2" \
&&   echo "  concretizer:" \
&&   echo "    unify: true" \
&&   echo "  config:" \
&&   echo "    install_tree: /opt/software" \
&&   echo "  view: /opt/view") > /opt/spack-environment/spack.yaml

# Install the software, remove unnecessary deps
RUN cd /opt/spack-environment && spack env activate . && spack install --fail-fast && spack gc -y

# Modifications to the environment that are necessary to run
RUN cd /opt/spack-environment && \
    spack env activate --sh -d . >> /etc/profile.d/spack_environment.sh

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND="noninteractive"

RUN apt-get update -y && apt-get upgrade -y
RUN apt-get install -y cmake build-essential environment-modules gfortran git python3 gdb

RUN apt-get install -y libyaml-cpp-dev

COPY --from=coeus-builder /opt/spack-environment /opt/spack-environment
COPY --from=coeus-builder /opt/software /opt/software
COPY --from=coeus-builder /opt/view /usr
COPY --from=coeus-builder /etc/profile.d/spack_environment.sh /etc/profile.d/spack_environment.sh
COPY --from=coeus-builder /etc/profile.d/spack_environment.sh /etc/env.sh
RUN sed -i '/alias/d;s/;$//' /etc/env.sh
RUN cat /etc/env.sh >> /root/.bashrc
RUN sed -i 's/^\[ -z "$PS1" \] \&\& return/#\[ -z "$PS1" \] \&\& return/g' ~/.bashrc
RUN echo '#!/bin/sh' | cat - /etc/env.sh | tee /etc/env.sh > /dev/null


RUN chmod +x /root/.bashrc
#alias despacktivate='spack env deactivate';ACLOCAL_PATH=/opt/view/share/aclocal;CMAKE_PREFIX_PATH=/opt/view;MANPATH=/opt/view/share/man:/opt/view/man:;MPICC=/opt/view/bin/mpicc;MPICXX=/opt/view/bin/mpic++;MPIF77=/opt/view/bin/mpif77;MPIF90=/opt/view/bin/mpif90;PATH=/opt/view/bin:/opt/spack/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin;PKG_CONFIG_PATH=/opt/view/share/pkgconfig:/opt/view/lib/pkgconfig:/opt/view/lib64/pkgconfig;SPACK_ENV=/opt/spack-environment
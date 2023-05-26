# Use scs-lab/base:1.0 as base image
FROM spack/ubuntu-jammy:latest as coeus-builder

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
COPY --from=coeus-builder /opt/view /opt/view
COPY --from=coeus-builder /etc/profile.d/spack_environment.sh /etc/profile.d/spack_environment.sh
COPY --from=coeus-builder /etc/profile.d/spack_environment.sh /etc/env.sh
RUN chmod +x /etc/env.sh

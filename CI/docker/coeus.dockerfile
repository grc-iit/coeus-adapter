# Use scs-lab/base:1.0 as base image
FROM spack/ubuntu-jammy:v0.20.0 as builder
ENV DOCKER_TAG=0.9

RUN apt-get update -y && apt-get upgrade -y
RUN apt-get install -y pkg-config

#Add external repos to spack
ENV HERMES_REPO=/opt/hermes
RUN git clone https://github.com/HDFGroup/hermes.git $HERMES_REPO
RUN spack repo add ${HERMES_REPO}/ci/hermes

# What we want to install and how we want to install it
# is specified in a manifest file (spack.yaml)
RUN mkdir /opt/spack-environment \
&&  (echo "spack:" \
&&   echo "  specs:" \
&&   echo "  - hdf5" \
&&   echo "  - mpich" \
&&   echo "  - adios2" \
&&   echo "  - hermes@master" \
&&   echo "  concretizer:" \
&&   echo "    unify: true" \
&&   echo "  config:" \
&&   echo "    install_tree: /opt/software" \
&&   echo "  view: /opt/view") > /opt/spack-environment/spack.yaml

# Install the software, remove unnecessary deps
RUN cd /opt/spack-environment && spack env activate . && spack install --fail-fast && spack gc -y

RUN cd /opt/spack-environment && \
    spack env activate --sh -d . >> /opt/spack-environment/spack.sh

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND="noninteractive"

RUN apt-get update -y && apt-get upgrade -y
RUN apt-get install -y pkg-config cmake build-essential environment-modules  \
    gfortran git python3 python3-pip gdb valgrind linux-tools-common linux-tools-generic linux-tools-`uname -r`

RUN pip install cpplint tabulate pyyaml pandas numpy

RUN apt-get install -y libyaml-cpp-dev

COPY --from=builder /opt/spack-environment /opt/spack-environment
COPY --from=builder /opt/software /opt/software
COPY --from=builder /opt/view /usr/local

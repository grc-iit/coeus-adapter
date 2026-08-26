# COEUS-Adapter CI image.
#
# Post clio-core migration: Hermes is gone, the backbone is iowarp-core
# (Chimaera runtime + Context-Transfer-Engine). The pre-0.9.x images
# (scslab/coeus:*) predate that migration and contain no iowarp-core at all,
# so they cannot configure this project.
#
# Build from the REPOSITORY ROOT - CI/coeus (the Spack repo providing
# adios2-coeus) and CI/docker/spack.yaml must both be in the build context:
#
#   docker build -f CI/docker/coeus.dockerfile -t ghcr.io/grc-iit/coeus:1.0.1 .
#
# The image must be portable to unknown CI hardware: CI/docker/spack.yaml pins
# target=x86_64 so the result does not inherit the build host's microarchitecture.
#
# Spack >= 1.0 is required: CI/coeus/packages/adios2-coeus/package.py imports
# from spack_repo.builtin.build_systems, which does not exist before 1.0.
# Note the tag has no "v" prefix - that convention was dropped after v0.20.3.
# Pinned to the 1.2 line to match the Spack the specs were concretized against.
FROM spack/ubuntu-jammy:1.2.2 AS builder
ENV DOCKER_TAG=1.0.1

RUN apt-get update -y \
 && apt-get install -y --no-install-recommends pkg-config \
 && rm -rf /var/lib/apt/lists/*

# External Spack repos: iowarp ships in clio-core, adios2-coeus ships here.
RUN git clone --depth 1 https://github.com/iowarp/clio-core.git /opt/clio-core \
 && spack repo add /opt/clio-core/installers/spack
COPY CI/coeus /opt/coeus-spack
RUN spack repo add /opt/coeus-spack

COPY CI/docker/spack.yaml /opt/spack-environment/spack.yaml
# Spack 1.x models compilers as packages; make sure the image's gcc is
# registered before concretization rather than failing deep into the build.
RUN spack compiler find
RUN cd /opt/spack-environment \
 && spack env activate . \
 && spack install --fail-fast \
 && spack gc -y \
 && spack clean -a

RUN cd /opt/spack-environment \
 && spack env activate --sh -d . >> /opt/spack-environment/spack.sh

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND="noninteractive"

# No cmake / libyaml-cpp-dev here on purpose: both come from the Spack view at
# /usr/local, which precedes /usr in CMake's search order. Installing the apt
# copies too invites find_package picking the older one.
RUN apt-get update -y && apt-get upgrade -y && apt-get install -y \
        pkg-config build-essential environment-modules gfortran git \
        python3 python3-pip gdb valgrind linux-tools-common linux-tools-generic \
 && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir cpplint tabulate pyyaml pandas numpy

COPY --from=builder /opt/spack-environment /opt/spack-environment
COPY --from=builder /opt/software /opt/software
COPY --from=builder /opt/view /usr/local

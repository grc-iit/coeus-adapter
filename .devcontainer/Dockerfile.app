# ============================================================
# Stage 1: Base image
# ============================================================
FROM iowarp/iowarp-deps:latest AS base

USER root

# ------------------------------------------------------------
# System utilities + Docker CLI
# ------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
    iptables \
    supervisor \
    net-tools \
    lsof \
    iproute2 \
    git \
    sudo \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Remove system HDF5 to avoid ABI conflicts
# ------------------------------------------------------------
RUN apt-get update && apt-get remove -y \
    libhdf5-dev \
    libhdf5-* \
    hdf5-* \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Docker Engine
# ------------------------------------------------------------
RUN install -m 0755 -d /etc/apt/keyrings && \
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
    gpg --dearmor -o /etc/apt/keyrings/docker.gpg && \
    chmod a+r /etc/apt/keyrings/docker.gpg

RUN echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
    > /etc/apt/sources.list.d/docker.list

RUN apt-get update && apt-get install -y \
    docker-ce \
    docker-ce-cli \
    containerd.io \
    docker-buildx-plugin \
    docker-compose-plugin \
    && rm -rf /var/lib/apt/lists/*

RUN usermod -aG docker iowarp || true
RUN getent group docker || groupadd docker

# ------------------------------------------------------------
# Docker socket helper
RUN echo '#!/bin/bash\n\
    if [ -S /var/run/docker.sock ]; then\n\
    sudo chmod 666 /var/run/docker.sock\n\
    fi\n\
    exec "$@"' > /usr/local/bin/docker-entrypoint.sh \
    && chmod +x /usr/local/bin/docker-entrypoint.sh

# ------------------------------------------------------------
# Switch to user
# ------------------------------------------------------------
USER iowarp
WORKDIR /home/iowarp

# ------------------------------------------------------------
# Miniconda (DEV TOOLS ONLY — NOT FOR SPACK)
# ------------------------------------------------------------
RUN wget -q https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh \
    -O /tmp/miniconda.sh && \
    bash /tmp/miniconda.sh -b -p /home/iowarp/miniconda3 && \
    rm /tmp/miniconda.sh

RUN /home/iowarp/miniconda3/bin/conda init bash && \
    /home/iowarp/miniconda3/bin/conda config --add channels conda-forge && \
    /home/iowarp/miniconda3/bin/conda config --set channel_priority strict

# Accept Anaconda Terms of Service and install all development dependencies via conda
# This avoids library conflicts between system packages and conda packages
RUN /home/iowarp/miniconda3/bin/conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main \
    && /home/iowarp/miniconda3/bin/conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r \
    && /home/iowarp/miniconda3/bin/conda install -y \
    conda-build \
    cmake \
    ninja \
    boost \
    hdf5 \
    yaml-cpp \
    zeromq \
    cppzmq \
    cereal \
    catch2 \
    libcurl \
    openssl \
    zlib \
    poco \
    nlohmann_json \
    && /home/iowarp/miniconda3/bin/conda clean -ya

# ------------------------------------------------------------
# Install runtime-deployment
# ------------------------------------------------------------
#RUN cd /home/iowarp \
#    && git clone https://github.com/iowarp/runtime-deployment.git \
#    && cd runtime-deployment \
#    && source /home/iowarp/miniconda3/etc/profile.d/conda.sh \
#   && conda activate base \
#    && pip install -e . -r requirements.txt \
#    && jarvis init \
#    && jarvis rg build

# ------------------------------------------------------------
# Python venv (optional)
# ------------------------------------------------------------

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["/bin/bash"]

# ============================================================
# Stage 2: Spack + WRF environment
# ============================================================

FROM base

ARG SPACK_VERSION=0.22.2
ENV SPACK_ROOT=/opt/spack

# ------------------------------------------------------------
# Install Spack
# ------------------------------------------------------------
USER root

RUN git clone --depth=1 --branch v${SPACK_VERSION} \
    https://github.com/spack/spack.git ${SPACK_ROOT} && \
    chown -R iowarp:iowarp ${SPACK_ROOT}

# ------------------------------------------------------------
# Clone coeus-adapter repository
# ------------------------------------------------------------
RUN git clone -b developed https://github.com/grc-iit/coeus-adapter.git /opt/coeus-adapter && \
    chown -R iowarp:iowarp /opt/coeus-adapter

# ------------------------------------------------------------
# Configure Spack for user
# ------------------------------------------------------------
USER iowarp
WORKDIR /home/iowarp

RUN echo '' >> ~/.bashrc && \
    echo '# >>> Spack >>>' >> ~/.bashrc && \
    echo 'export SPACK_ROOT=/opt/spack' >> ~/.bashrc && \
    echo '. ${SPACK_ROOT}/share/spack/setup-env.sh' >> ~/.bashrc && \
    echo '# <<< Spack <<<' >> ~/.bashrc

# ------------------------------------------------------------
# Initialize Spack (NO externals)
# ------------------------------------------------------------
# Disable ALL Spack externals (CRITICAL FIX)
# =========================
RUN mkdir -p ~/.spack && \
    printf "packages:\n  all:\n    buildable: true\n" > ~/.spack/packages.yaml

RUN export SPACK_ROOT=/opt/spack && \
    . ${SPACK_ROOT}/share/spack/setup-env.sh && \
    spack compiler find && \
    spack compiler list && \
    spack repo add /opt/coeus-adapter/CI/coeus

# ------------------------------------------------------------
# Validate WRF concretization (no install during build)
# ------------------------------------------------------------
RUN export SPACK_ROOT=/opt/spack && \
    . ${SPACK_ROOT}/share/spack/setup-env.sh && \
    spack spec wrf %gcc ^openmpi ^netcdf-c ^netcdf-fortran

# ------------------------------------------------------------
# Install adios via Spack
# ------------------------------------------------------------
RUN export SPACK_ROOT=/opt/spack && \
    . ${SPACK_ROOT}/share/spack/setup-env.sh && \
    spack install adios2

# ------------------------------------------------------------
# Final setup
# ------------------------------------------------------------
# Note: Spack and coeus-adapter are in /opt (not /workspace) because
# /workspace is mounted from the host and would overwrite them.
# Locations:
#   - Spack: /opt/spack
#   - coeus-adapter: /opt/coeus-adapter
WORKDIR /workspace

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["/bin/bash"]
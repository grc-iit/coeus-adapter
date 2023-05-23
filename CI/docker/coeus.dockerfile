# Use scs-lab/base:1.0 as base image
FROM scs-lab/base:1.0

# Install yaml-cpp using apt
RUN apt-get install -y libyaml-cpp-dev

# Use Spack to install mpich, hdf5, adios2
RUN . $SPACK_ROOT/share/spack/setup-env.sh && \
    spack install mpich && \
    spack install hdf5 && \
    spack install adios2

# Create a script to load the required modules
RUN echo '#!/bin/bash' > /load_modules.sh && \
    echo '. $SPACK_ROOT/share/spack/setup-env.sh' >> /load_modules.sh && \
    echo 'module load mpich' >> /load_modules.sh && \
    echo 'module load hdf5' >> /load_modules.sh && \
    echo 'module load adios2' >> /load_modules.sh && \
    chmod +x /load_modules.sh

# Start SSH
CMD ["/usr/sbin/sshd", "-D"]

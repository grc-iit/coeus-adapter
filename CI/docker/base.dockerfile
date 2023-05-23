# Use Ubuntu as base image
FROM ubuntu:latest

# Update Ubuntu Software repository
RUN apt-get update && apt-get upgrade -y

# Install OpenSSH server
RUN apt-get install -y openssh-server
RUN mkdir /var/run/sshd

# Set up passwordless SSH
RUN echo 'root:root' | chpasswd
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Environment variable for SSH
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# Install spack dependencies
RUN apt-get install -y git build-essential ca-certificates coreutils curl environment-modules gfortran git gpg lsb-release python3 python3-distutils python3-venv unzip zip

# Install Spack
RUN git clone -c feature.manyFiles=true https://github.com/spack/spack.git
ENV SPACK_ROOT=/spack
ENV PATH=$SPACK_ROOT/bin:$PATH

# Install Lmod for module support
RUN apt-get install -y lmod

# Start SSH
CMD ["/usr/sbin/sshd", "-D"]
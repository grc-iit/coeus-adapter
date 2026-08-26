#!/bin/bash
# Install NVIDIA Container Toolkit on the host machine
# This enables GPU passthrough to Docker containers

set -e

echo "========================================="
echo "NVIDIA Container Toolkit Installer"
echo "========================================="
echo ""

# Check if running with sudo
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: Please run with sudo:"
    echo "  sudo bash $0"
    exit 1
fi

# Detect distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
    VERSION=$VERSION_ID
else
    echo "ERROR: Cannot detect Linux distribution"
    exit 1
fi

echo "Detected: $DISTRO $VERSION"
echo ""

# Check if NVIDIA driver is installed
if ! command -v nvidia-smi &> /dev/null; then
    echo "WARNING: nvidia-smi not found!"
    echo "You need to install NVIDIA drivers first:"
    echo "  sudo apt install nvidia-driver-545  # or newer"
    echo ""
    read -p "Do you want to continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "NVIDIA driver detected:"
    nvidia-smi --query-gpu=driver_version --format=csv,noheader
    echo ""
fi

# Add NVIDIA Docker repository
echo "Adding NVIDIA Container Toolkit repository..."
curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg

curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
    sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
    tee /etc/apt/sources.list.d/nvidia-container-toolkit.list

echo "Updating package lists..."
apt-get update

echo "Installing nvidia-container-toolkit..."
apt-get install -y nvidia-container-toolkit

echo "Configuring Docker to use NVIDIA runtime..."
nvidia-ctk runtime configure --runtime=docker

echo "Restarting Docker..."
systemctl restart docker

echo ""
echo "========================================="
echo "Installation complete!"
echo "========================================="
echo ""
echo "Testing GPU access in Docker..."
if docker run --rm --gpus all nvidia/cuda:12.6.0-base-ubuntu24.04 nvidia-smi; then
    echo ""
    echo "SUCCESS! GPU access is working in Docker containers."
    echo ""
    echo "Next steps:"
    echo "1. Edit .devcontainer/devcontainer.json"
    echo "2. Uncomment the GPU-enabled runArgs section"
    echo "3. Rebuild your devcontainer"
    echo ""
    echo "See .devcontainer/CUDA_SETUP.md for details."
else
    echo ""
    echo "ERROR: GPU test failed. Please check your NVIDIA driver installation."
    echo "Try running: nvidia-smi"
fi

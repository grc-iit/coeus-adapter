# CUDA Devcontainer Setup Guide

## Current Status

✅ **CUDA Toolkit 12.6 is fully installed in the devcontainer**
✅ **All CUDA runtime libraries are present**
✅ **CUDA compilation works perfectly**
⚠️ **GPU runtime requires host configuration**

## What's Installed

The devcontainer includes:
- CUDA Toolkit 12.6 (nvcc compiler, tools)
- CUDA Runtime libraries (libcudart, libcublas, libcufft, libcurand, libcusolver, libcusparse, libnpp)
- CUDA development headers
- NVIDIA container tools
- All necessary environment variables

## GPU Runtime Access

To actually RUN CUDA code on your GPU (not just compile it), you need the NVIDIA Container Toolkit on your HOST machine.

### Option 1: Install NVIDIA Container Toolkit (Recommended)

```bash
# Add NVIDIA package repository
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | \
    sudo tee /etc/apt/sources.list.d/nvidia-docker.list

# Install nvidia-container-toolkit
sudo apt-get update
sudo apt-get install -y nvidia-container-toolkit

# Configure Docker to use nvidia runtime
sudo nvidia-ctk runtime configure --runtime=docker
sudo systemctl restart docker

# Test it works
docker run --rm --gpus all nvidia/cuda:12.6.0-base-ubuntu24.04 nvidia-smi
```

After installing, rebuild your devcontainer and GPU access will work.

### Option 2: Use Without GPU Runtime (Development Only)

If you just need to compile CUDA code without running it on GPU:

**Edit `.devcontainer/devcontainer.json`** and remove the GPU flags:

```json
"runArgs": [
    "--privileged",
    "--shm-size=2gb"
],
```

Remove or comment out:
```json
"hostRequirements": {
    "gpu": true
},
```

This allows the container to start, and you can compile CUDA code, but can't execute it on GPU.

## Verification

### Test CUDA Compilation

```bash
# Inside the devcontainer
cat > test.cu << 'EOF'
#include <stdio.h>
#include <cuda_runtime.h>

__global__ void hello() {
    printf("Hello from GPU thread %d\\n", threadIdx.x);
}

int main() {
    hello<<<1, 10>>>();
    cudaDeviceSynchronize();
    return 0;
}
EOF

nvcc test.cu -o test
./test  # Will work if GPU runtime is available
```

### Check CUDA Installation

```bash
nvcc --version                 # Should show CUDA 12.6
ls /usr/local/cuda-12.6/lib64/ # Should show CUDA libraries
echo $CUDA_HOME                # Should be /usr/local/cuda-12.6
```

### Check GPU Access

```bash
nvidia-smi  # Shows GPU info if runtime is properly configured
```

## Environment Variables

The following are automatically set:
- `CUDA_HOME=/usr/local/cuda-12.6`
- `PATH` includes `/usr/local/cuda-12.6/bin`
- `LD_LIBRARY_PATH` includes `/usr/local/cuda-12.6/lib64`
- `NVIDIA_VISIBLE_DEVICES=all`
- `NVIDIA_DRIVER_CAPABILITIES=compute,utility`

## Troubleshooting

### "CUDA driver version is insufficient"
- This means the host NVIDIA driver is too old for CUDA 12.6
- Update your host NVIDIA drivers to 525.60.13 or newer

### "could not select device driver with capabilities: [[gpu]]"
- NVIDIA Container Toolkit is not installed on the host
- Follow Option 1 above to install it

### "--runtime=nvidia: unknown or invalid runtime"
- Older Docker configuration issue
- Use `--gpus=all` instead (already configured)
- Or install nvidia-container-toolkit and configure it

### Container won't start with GPU flags
- Temporarily remove GPU flags from runArgs
- Install NVIDIA Container Toolkit on host
- Add GPU flags back after toolkit is installed

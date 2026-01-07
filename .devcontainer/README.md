# IOWarp Core DevContainer

This directory contains the DevContainer configuration for IOWarp Core development.

## Features

### Conda Package Manager (Recommended)

The devcontainer includes **Miniconda** with conda-forge channel configured. Conda is the **recommended** package manager for IOWarp Core development.

**Conda setup:**
- **Location:** `/home/iowarp/miniconda3`
- **Channels:** conda-forge (priority: strict)
- **Pre-installed:** conda-build
- **Auto-initialized:** Ready to use when container starts

**Quick start with Conda:**
```bash
# Conda is already initialized - just use it!

# Build IOWarp Core conda package from local source
cd conda && ./conda-local.sh

# Or create a development environment
conda create -n iowarp-dev -c conda-forge \
    boost yaml-cpp zeromq hdf5 cereal catch2 cmake
conda activate iowarp-dev

# Build IOWarp Core
cmake --preset=conda
make -j8
```

**Common Conda commands:**
```bash
# List environments
conda env list

# Create new environment
conda create -n myenv python=3.11

# Activate environment
conda activate myenv

# Install packages
conda install -c conda-forge boost yaml-cpp

# Build conda package
conda build conda/ -c conda-forge

# Install local package
conda install --use-local iowarp-core
```

**Why use Conda for IOWarp Core?**
- CMake automatically detects and prioritizes Conda packages
- All dependencies available via conda-forge
- Build from local source with `./conda-local.sh`
- Reproducible environments
- Better dependency management than system packages

### Python Virtual Environment (Alternative)

A Python virtual environment is also available at `/home/iowarp/venv` as an alternative to Conda.

**Pre-installed packages:**
- `pip`, `setuptools`, `wheel` (latest versions)
- `pyyaml` (for configuration file parsing)
- `nanobind` (for Python bindings)

**Note:** By default, the venv is NOT auto-activated (Conda is preferred). To auto-activate venv instead of Conda, uncomment the venv activation lines in `~/.bashrc`.

**Manual activation:**
If you want to use venv instead of Conda:
```bash
source /home/iowarp/venv/bin/activate
```

**Installing additional packages:**
```bash
# Activate venv (if not already active)
source /home/iowarp/venv/bin/activate

# Install packages
pip install <package-name>
```

**Building Python bindings:**
To enable Python bindings for IOWarp components:
```bash
# Configure with Python support
cmake --preset=debug -DWRP_CORE_ENABLE_PYTHON=ON

# Build
make -j8

# Install to venv (if WRP_CORE_INSTALL_TO_VENV is ON)
make install
```

The Python modules will be installed to the virtual environment's site-packages directory and will be importable from Python scripts.

### VSCode Extensions

The following extensions are automatically installed:
- **C/C++ Development:**
  - C/C++ (ms-vscode.cpptools)
  - CMake Tools (ms-vscode.cmake-tools)
  - CMake (twxs.cmake)
  - C/C++ Debug (KylinIdeTeam.cppdebug)
  - clangd (llvm-vs-code-extensions.vscode-clangd)

- **Python Development:**
  - Python (ms-python.python)
  - Pylance (ms-python.vscode-pylance)

- **Container & DevOps:**
  - Docker (ms-azuretools.vscode-docker)

- **AI Assistant:**
  - Claude Code (anthropic.claude-code)

### Docker-in-Docker

Docker is available inside the container with the host's Docker socket mounted, allowing you to:
- Build and run containers from inside the devcontainer
- Use docker-compose
- Interact with the host's Docker daemon

### Environment Variables

- `IOWARP_CORE_ROOT`: Set to the workspace folder
- `CONDA_DEFAULT_ENV`: Set to "base"
- `CONDA_AUTO_ACTIVATE_BASE`: Set to "true"
- `PATH`: Includes conda and venv bin directories

## Python Configuration

The VSCode Python extension is configured with:
- **Default interpreter:** `/home/iowarp/miniconda3/bin/python` (Conda base environment)
- **Conda path:** `/home/iowarp/miniconda3/bin/conda`
- **Auto-activate:** Terminal activation is enabled
- **Linting:** flake8 enabled (pylint disabled)
- **Formatting:** black (if installed)

**Switching Python interpreters in VSCode:**
1. Press `Ctrl+Shift+P`
2. Type "Python: Select Interpreter"
3. Choose between Conda environments or venv

## Rebuilding the Container

If you modify the Dockerfile, rebuild the container:

1. In VSCode: `Ctrl+Shift+P` → "Dev Containers: Rebuild Container"
2. Or manually: `docker-compose build` (if using docker-compose setup)

## Troubleshooting

**Conda not initialized:**
```bash
# Initialize conda in current shell
eval "$(~/miniconda3/bin/conda shell.bash hook)"

# Or re-run conda init
~/miniconda3/bin/conda init bash
source ~/.bashrc
```

**Wrong Python interpreter:**
```bash
# Check current Python
which python
python --version

# Activate desired conda environment
conda activate myenv

# Or use venv
source /home/iowarp/venv/bin/activate
```

**Conda packages not found:**
```bash
# Verify conda-forge channel is configured
conda config --show channels

# Add conda-forge if missing
conda config --add channels conda-forge
conda config --set channel_priority strict
```

**Virtual environment not activated (if using venv):**
```bash
source /home/iowarp/venv/bin/activate
```

**Python packages not found:**
```bash
# For Conda:
which python  # Should show /home/iowarp/miniconda3/.../python
conda list    # Show installed packages

# For venv:
which python  # Should show /home/iowarp/venv/bin/python
pip list      # Show installed packages
```

**Docker permission issues:**
The container should automatically fix Docker socket permissions. If issues persist:
```bash
sudo chmod 666 /var/run/docker.sock
```

**Conda build fails:**
```bash
# Check submodules
git submodule update --init --recursive

# Try with debug output
conda build conda/ -c conda-forge --debug
```

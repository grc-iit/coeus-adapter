This is the instrmentation on how to use catalyst for adios2 as I/O engine

Paraview installation
Install in Ares
```
spack  install paraview@5.13.3   +adios2 +fides +mpi +libcatalyst +python   ~qt   ^python@3.12   ^py-mpi4py   ^hdf5@1.12.3   ^osmesa  
```
Install in personal computer:
```
spack  install paraview@5.13.3 +qt  +adios2 +fides +mpi +libcatalyst +python    ^python@3.12   ^py-mpi4py   ^hdf5@1.12.3    
```
**Important**: ParaView 5.13.3 is not compatible with Python 3.14. Use Python 3.11 or 3.12 instead. If you encounter segmentation faults during Catalyst initialization, ensure you're using a compatible Python version.

Please note that the plugin uses the ADIOS inline engine to pass data pointers to ParaView's Fides reader and uses ParaView Catalyst to process a user python script that contains a ParaView pipeline. Fides is a library that provides a schema for reading ADIOS data into visualization services such as ParaView. By integrating it with ParaView Catalyst, it is now possible to perform in situ visualization with ADIOS2-enabled codes without writing adaptors.


## environment setup

After installing ParaView with spack, you need to set up the environment variables:

```bash
# Load ParaView module (this sets up basic paths)
spack load paraview@5.13.3

# Find ParaView installation path
PARAVIEW_PREFIX=$(spack location -i paraview@5.13.3)
spack load iowarp@main 
spack load paraview
export PATH=~/Desktop/software/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/Desktop/software/coeus-adapter/build/bin/:$LD_LIBRARY_PATH 
# Set atalyst implementation
export CATALYST_IMPLEMENTATION_NAME=paraview
export CATALYST_IMPLEMENTATION_PATHS=$PARAVIEW_PREFIX/lib/catalyst

# Set PYTHONPATH to include ParaView Python modules
# Find the Python site-packages directory
PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PARAVIEW_PYTHON_DIR="$PARAVIEW_PREFIX/lib/python$PYTHON_VERSION/site-packages"
if [ -d "$PARAVIEW_PYTHON_DIR" ]; then
    export PYTHONPATH="$PARAVIEW_PYTHON_DIR:$PYTHONPATH"
    echo "Added ParaView Python modules to PYTHONPATH: $PARAVIEW_PYTHON_DIR"
else
    echo "Warning: ParaView Python directory not found at $PARAVIEW_PYTHON_DIR"
    echo "Trying to find it..."
    # Alternative: search for paraview module
    PARAVIEW_PYTHON_DIR=$(find $PARAVIEW_PREFIX -type d -path "*/site-packages/paraview" 2>/dev/null | head -1 | xargs dirname)
    if [ -n "$PARAVIEW_PYTHON_DIR" ]; then
        export PYTHONPATH="$PARAVIEW_PYTHON_DIR:$PYTHONPATH"
        echo "Found ParaView Python modules at: $PARAVIEW_PYTHON_DIR"
    fi
fi

# Verify ParaView Python support
python3 -c "import paraview; print('ParaView Python support: OK')" || echo "ERROR: ParaView Python modules not found"
```


## run the experiment

```
mpirun -n 4 adios2-gray-scott settings.json
```

# Paraview GUI setup

This video shows how to use the Paraview GUI with the catalyst setup:

**[Watch the demonstration video on YouTube](https://youtu.be/FD0nAeOLC8s)**



### Segmentation Fault During Catalyst Initialization

If you encounter a segmentation fault (signal 11) when running Catalyst, particularly during Python module initialization, this is likely due to Python version incompatibility:

**Symptoms:**
- Crash occurs during VTK module import (`vtkCommonCore`, `vtkRemotingClientServerStream`)
- Error message shows `PyDict_SetItem` or `PyVTKTemplate_New` in the stack trace
- Failing at address `0x8` (null pointer dereference)

**Solution:**
1. Verify your Python version: `python --version` or `python3 --version`
2. ParaView 5.13.3 requires Python 3.11 or 3.12. Python 3.14 is **not** compatible.
3. Reinstall ParaView with a compatible Python version:
   ```bash
   spack install paraview@5.13.3 +adios2^python@3.12 + qt +fides +mpi +libcatalyst +python ^py-mpi4py ^python-venv
   ```
4. Ensure all ParaView dependencies use the same Python version.

### ModuleNotFoundError: No module named 'paraview'

This error occurs when Python cannot find the ParaView modules because `PYTHONPATH` is not set correctly.

**Solution:**
1. **Find your ParaView installation:**
   ```bash
   spack load paraview@5.13.3
   PARAVIEW_PREFIX=$(spack location -i paraview@5.13.3)
   echo "ParaView installed at: $PARAVIEW_PREFIX"
   ```

2. **Find the Python site-packages directory:**
   ```bash
   PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
   PARAVIEW_PYTHON_DIR="$PARAVIEW_PREFIX/lib/python$PYTHON_VERSION/site-packages"
   echo "Python modules should be at: $PARAVIEW_PYTHON_DIR"
   ls -la "$PARAVIEW_PYTHON_DIR/paraview" 2>/dev/null || echo "Directory not found, searching..."
   ```

3. **If the directory doesn't exist, search for it:**
   ```bash
   find $PARAVIEW_PREFIX -type d -name "paraview" -path "*/site-packages/*" 2>/dev/null
   ```

4. **Set PYTHONPATH:**
   ```bash
   export PYTHONPATH="$PARAVIEW_PYTHON_DIR:$PYTHONPATH"
   # Or if found via search:
   # export PYTHONPATH="/path/to/found/site-packages:$PYTHONPATH"
   ```

5. **Verify it works:**
   ```bash
   python3 -c "import paraview; print('✓ ParaView Python support: OK')"
   ```

**Permanent setup:** Add the `PYTHONPATH` export to your `~/.bashrc` or create a setup script that you source before running simulations.

### Debugging Catalyst Issues

For more verbose output, set these environment variables:
```bash
export CATALYST_DEBUG=1
export PARAVIEW_LOG_CATALYST_VERBOSITY=INFO
```



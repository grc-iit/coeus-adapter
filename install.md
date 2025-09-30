## Coeus-adapter installation guide

# Dependencies
* [Hermes](https://github.com/HDFGroup/hermes): a multi-tiered I/O buffering platform.
* [ADIOS2](https://github.com/ornladios/ADIOS2): an I/O library

### 1.Install ADIOS2
please follow these steps to install the adios2 with derived variables
step 1: Install Spack. We recommend Spack v0.23.1 (or earlier) — Spack v1.0.0 does not support installing ADIOS2 or Hermes.
```
cd ${HOME}
git clone https://github.com/spack/spack.git
cd spack
git checkout tags/v0.23.1
echo ". ${PWD}/share/spack/setup-env.sh" >> ~/.bashrc
source ~/.bashrc
```
step 2: Add Coeus repo packages for spack
```
spack repo add /coeus-adapter/CI/coeus
```
step 3: install the adios2
```
spack install adios2-coeus@master
```
spack install incompact3D io_backend=adios2 ^openmpi ^adios2-coeus
```
### 2. Install the Hermes
```
cd ${HOME}
git clone https://github.com/grc-iit/grc-repo
spack repo add grc-repo
spack install hermes
```

### 3. Install Coeus-adapter
1. load environment variables
```
spack load hermes
spack load adios2-coeus
spack load openmpi
```
2. install the coeus-adapter
```
git clone https://github.com/grc-iit/coeus-adapter.git
cd coeus-adapter
mkdir build
cd build
cmake ../
make -j8
```
Note:
To enable the metadata and function trace features, please add the appropriate flags during the CMake configuration.
```
cmake .. -Dmeta_enabled=ON -Ddebug_mode=ON
```

### install jarvis(unified platform for deploying various applications)
1. jarvis installation
```
spack external find python
spack install py-jarvis-cd
spack load py-jarvis-cd
```
2. jarvis initilization(Please follow this [link](https://grc.iit.edu/docs/jarvis/jarvis-cd/index/#initialize-jarvis-configuration)) for the following steps: 

    Initialize jarvis configuration.

    Set or Change the active Hostfile

    Set Up Passwordless SSH. 

    Building a Resource Graph

### Incompact3D installation
```

```


## Hermes Info log
The Hermes info log is disabled by default. To enable the Hermes log, please set log_verbosity = 1 in hermes_run.

## Disbale adios2 metadata
add this to the adios2.xml
```
<parameter key="StatsLevel" value="0"/>
```


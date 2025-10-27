# COEUS-Adapter

An ADIOS plugin adapter that seamlessly connects ADIOS2 to Hermes, enabling enhanced I/O capabilities and derived variable computation for scientific computing applications.

## Overview

COEUS-Adapter bridges ADIOS2 and Hermes through the ADIOS plugin interface, providing advanced multi-tiered I/O capabilities with support for in-situ derived variable computation. It enables applications to leverage Hermes's multi-tiered buffering system while benefiting from ADIOS2's I/O abstractions.

![Architecture](./assets/images/Architecture.png)

## Features

- **Multi-tiered I/O**: Leverages Hermes for efficient data movement across different storage tiers
- **Derived Variable Computation**: In-situ computation of derived quantities (hash, curl, Q-criterion, etc.)
- **ADIOS2 Integration**: Seamless plugin-based integration with ADIOS2 applications
- **Metadata Support**: Comprehensive metadata management for query and analysis
- **Multiple Application Support**: Tested with WRF, LAMMPS, Gray-Scott, Incompact3D, and more

## Quick Start

### Prerequisites

- Spack v0.23.1 or earlier (v1.0.0+ not supported)
- Hermes
- ADIOS2 with COEUS extensions

### Installation

For detailed installation instructions, see the [installation guide](install.md).

**Quick Install:**

```bash
# Install ADIOS2 with COEUS extensions
spack install adios2

# Install Hermes
spack install hermes

# Load dependencies
spack load hermes
spack load adios2

# Build COEUS-Adapter
git clone https://github.com/grc-iit/coeus-adapter.git
cd coeus-adapter
mkdir build && cd build
cmake .. -Dmeta_enabled=ON -Ddebug_mode=ON
make -j8
```

## Usage

COEUS-Adapter works automatically as an ADIOS2 plugin. Applications using ADIOS2 can leverage COEUS by configuring it in their ADIOS2 XML or programmatically setting the engine type.

### Example Configuration

Configure your ADIOS2 application to use the COEUS engine by simplily specifying it in your ADIOS2 XML configuration.

## Supported Applications

COEUS-Adapter has been tested with the following scientific computing applications:

| Application                     | Directory                                                                                                           | Derived Quantities           |
|--------------------------------|---------------------------------------------------------------------------------------------------------------------|------------------------------|
| WRF (Weather Forecasting)      | [test/jarvis/jarvis_coeus/jarvis_coeus/wrf](./test/jarvis/jarvis_coeus/jarvis_coeus/wrf)                          | Hash                         |
| LAMMPS (Molecular Dynamics)    | [test/jarvis/jarvis_coeus/jarvis_coeus/lammps](./test/jarvis/jarvis_coeus/jarvis_coeus/lammps)                    | Hash                         |
| Gray-Scott (Reaction Diffusion)| [test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott](./test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott)| Curl, Add, Hash             |
| Incompact3D                    | [test/jarvis/jarvis_coeus/jarvis_coeus/InCompact3D](./test/jarvis/jarvis_coeus/jarvis_coeus/InCompact3D)           | Q-criterion                  |
| OpenFOAM                       | [test/jarvis/jarvis_coeus/jarvis_coeus/openform](./test/jarvis/jarvis_coeus/jarvis_coeus/openform)                 | -                            |

## Configuration

COEUS-Adapter can be configured through CMake options:

- `-Dmeta_enabled=ON`: Enable metadata features
- `-Ddebug_mode=ON`: Enable debug mode with detailed logging

## Documentation

- [Installation Guide](install.md) - Detailed installation and setup instructions
- [Test Applications](./test/) - Example applications and integration tests





## Acknowledgments

Developed with support from the Department of Energy (DOE) under award DOE ASCR Award DE-SC0023263. 






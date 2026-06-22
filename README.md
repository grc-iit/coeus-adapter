# COEUS-Adapter

An ADIOS plugin adapter that seamlessly connects ADIOS2 to IOWarp, enabling enhanced I/O capabilities and derived variable computation for scientific computing applications.

## Overview

COEUS-Adapter bridges ADIOS2 and IOWarp through the ADIOS plugin interface, providing advanced multi-tiered I/O capabilities with support for in-situ derived variable computation. It enables applications to leverage IOWarp's Context-Transfer-Engine (CTE) for multi-tiered buffering — running on the Chimaera runtime — while benefiting from ADIOS2's I/O abstractions.

<img src="./assets/images/architecture.png" alt="Architecture" width="500" style="display: block; margin: 0 auto;">

## Features

- **Multi-tiered I/O**: Leverages IOWarp's Context-Transfer-Engine (CTE) for efficient data movement across different storage tiers
- **Derived Variable Computation**: In-situ computation of derived quantities (hash, curl, Q-criterion, etc.)
- **In-Situ Visualization**: Live, zero-copy visualization via ParaView Catalyst 2 + Fides — Inline (single-node) or SST streaming (multi-node), with an experimental ParaView MCP AI agent for autonomous exploration
- **ADIOS2 Integration**: Seamless plugin-based integration with ADIOS2 applications
- **Metadata Support**: Comprehensive metadata management for query and analysis
- **Multiple Application Support**: Tested with WRF, LAMMPS, Gray-Scott, Incompact3D, OpenFOAM, and more

## Quick Start

### Prerequisites

- Spack v0.23.1 or earlier (v1.0.0+ not supported)
- IOWarp (provides the `iowarp-core` package: Chimaera runtime + CTE)
- ADIOS2

### Installation

For detailed installation instructions, see the [installation guide](install.md).

**Quick Install:**

```bash

# Install dependencies
spack install iowarp@master
spack install adios2

# Load dependencies
spack load iowarp@master
spack load adios2

# Build COEUS-Adapter
git clone https://github.com/grc-iit/coeus-adapter.git
cd coeus-adapter
mkdir build && cd build
cmake .. -Dmeta_enabled=ON -Ddebug_mode=ON
make -j8
```

## Usage

COEUS-Adapter works automatically as an ADIOS2 plugin. Applications using ADIOS2 can leverage COEUS by configuring it in their ADIOS2 XML.


## Supported Applications

COEUS-Adapter has been tested with the following scientific computing applications:

| Application                     | Directory                                                                                                           | Derived Quantities | In-Situ Viz |
|--------------------------------|---------------------------------------------------------------------------------------------------------------------|--------------------|-------------|
| WRF (Weather Forecasting)      | [test/jarvis/jarvis_coeus/jarvis_coeus/wrf](./test/jarvis/jarvis_coeus/jarvis_coeus/wrf)                          | Hash               | -           |
| LAMMPS (Molecular Dynamics)    | [test/jarvis/jarvis_coeus/jarvis_coeus/lammps](./test/jarvis/jarvis_coeus/jarvis_coeus/lammps)                    | Hash               | -           |
| Gray-Scott (Reaction Diffusion)| [test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott](./test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott)| Curl, Add, Hash    | ✓ (Catalyst/Fides, AI agent) |
| Incompact3D                    | [test/jarvis/jarvis_coeus/jarvis_coeus/InCompact3D](./test/jarvis/jarvis_coeus/jarvis_coeus/InCompact3D)           | Q-criterion        | ✓ (Catalyst/Fides) |
| OpenFOAM                       | [test/jarvis/jarvis_coeus/jarvis_coeus/openfoam](./test/jarvis/jarvis_coeus/jarvis_coeus/openfoam)                 | -                  | -           |


## In-Situ Visualization

COEUS-Adapter can drive live, zero-copy visualization while a simulation runs, using
[ParaView Catalyst 2](https://catalyst-in-situ.readthedocs.io/) together with
[Fides](https://fides.readthedocs.io/) to read ADIOS2 data into ParaView pipelines —
no per-application adaptor code required.

- **Inline mode (single-node)**: data pointers are passed in-process to ParaView's Fides
  reader via the ADIOS2 Inline engine; Catalyst executes a user-provided Python pipeline.
- **SST streaming mode (multi-node)**: data is streamed over ADIOS2 SST to an external
  ParaView/Catalyst reader, decoupling simulation and visualization processes.
- **In-situ AI agent (experimental)**: an AI agent drives ParaView through MCP tools to
  autonomously explore live simulation data — see [test/insitu_agent](./test/insitu_agent)
  and [paraview_mcp](./paraview_mcp).

Build with `-DCOEUS_ENABLE_CATALYST=ON` (or let CMake auto-detect a Catalyst install), then
point your ADIOS2 XML at a Fides `DataModel` JSON and a Catalyst `Script`. See the
[Gray-Scott](./test/catalyst_gray_scott) and [Incompact3D](./test/catalyst_Incompact3D)
in-situ examples for full setup.


## Documentation

- [Installation Guide](install.md) - Detailed installation and setup instructions
- [Test Applications](./test/) - Example applications and integration tests
- [In-Situ Visualization (Gray-Scott)](./test/catalyst_gray_scott) - Catalyst/Fides setup and Python pipelines
- [In-Situ Visualization (Incompact3D)](./test/catalyst_Incompact3D) - Fides data models and Catalyst pipelines
- [In-Situ AI Agent](./test/insitu_agent) - Autonomous ParaView exploration via MCP





## Acknowledgments

Developed with support from the Department of Energy (DOE) under award DOE ASCR Award DE-SC0023263. 






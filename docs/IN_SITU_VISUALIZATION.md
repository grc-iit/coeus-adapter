[← COEUS-Adapter README](../README.md)

# In-Situ Visualization

COEUS-Adapter can drive live, zero-copy visualization while a simulation runs,
using [ParaView Catalyst 2](https://catalyst-in-situ.readthedocs.io/) with
[Fides](https://fides.readthedocs.io/) to read ADIOS2 data into ParaView
pipelines - no per-application adaptor code.

- **Inline mode (single-node)** - data pointers are passed in-process to
  ParaView's Fides reader via the ADIOS2 Inline engine; Catalyst executes a
  user-provided Python pipeline.
- **SST streaming mode (multi-node)** - data is streamed over ADIOS2 SST to an
  external ParaView/Catalyst reader, decoupling simulation and visualization
  processes.
- **In-situ AI agent (experimental)** - an AI agent drives ParaView through MCP
  tools to autonomously explore live simulation data - see
  [test/insitu_agent](../test/insitu_agent). This is the same *render + reason*
  path used by the [Trigger-Render-Reason pipeline](TRIGGER_RENDER_REASON_PIPELINE.md).

## Setup

Build with `-DCOEUS_ENABLE_CATALYST=ON` (or let CMake auto-detect a Catalyst
install), then point your ADIOS2 XML at a Fides `DataModel` JSON and a Catalyst
`Script`.

See the full in-situ examples for complete setup (Fides data models and Catalyst
Python pipelines):

- [Gray-Scott](../test/catalyst_gray_scott)
- [Incompact3D](../test/catalyst_Incompact3D)

[← COEUS-Adapter README](../README.md)

# Supported Applications

COEUS-Adapter has been tested with the following scientific computing
applications. Each directory contains a Jarvis package that configures and runs
the application through the COEUS (`hermes`) engine.

| Application | Directory | Derived Quantities | In-Situ Viz |
|---|---|---|---|
| WRF (Weather Forecasting) | [wrf](../test/jarvis/jarvis_coeus/jarvis_coeus/wrf) | Hash | – |
| LAMMPS (Molecular Dynamics) | [lammps](../test/jarvis/jarvis_coeus/jarvis_coeus/lammps) | Hash, Mean (trigger) | – |
| Gray-Scott (Reaction-Diffusion) | [adios2_gray_scott](../test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott) | Curl, Add, Variance, Hash | ✓ Catalyst/Fides + AI agent |
| Incompact3d | [Incompact3d](../test/jarvis/jarvis_coeus/jarvis_coeus/Incompact3d) | Q-criterion | ✓ Catalyst/Fides |
| OpenFOAM | [openfoam](../test/jarvis/jarvis_coeus/jarvis_coeus/openfoam) | – | – |

Related:
[Derived Variables and Hash](DERIVED_VARIABLES_AND_HASH.md) ·
[Trigger-Render-Reason Pipeline](TRIGGER_RENDER_REASON_PIPELINE.md) ·
[In-Situ Visualization](IN_SITU_VISUALIZATION.md)

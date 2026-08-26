# COEUS-Adapter

**An ADIOS2 plugin that connects scientific applications to IOWarp** - giving
them multi-tiered I/O, in-situ derived variables, statistical triggers with
AI-driven steering, and live visualization, all without changing application
code.

Applications keep using the ordinary ADIOS2 API. COEUS intercepts their I/O
through the ADIOS2 plugin interface and redirects it into IOWarp's
**Context-Transfer-Engine (CTE)**, running on the **Chimaera** runtime.

<img src="./assets/images/architecture.png" alt="Architecture" width="500" style="display: block; margin: 0 auto;">

---

## Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage) - turn COEUS on for an ADIOS2 application
- [Derived Variables and Hash](docs/DERIVED_VARIABLES_AND_HASH.md) - in-situ quantities, incl. `hash()`
- [Trigger-Render-Reason Pipeline](docs/TRIGGER_RENDER_REASON_PIPELINE.md) - detect → stream → decide (Vigil)
- [Operators (Add-ons)](docs/OPERATORS.md) - optional consumer tools (time-derivative)
- [In-Situ Visualization](docs/IN_SITU_VISUALIZATION.md) - ParaView Catalyst / Fides
- [Supported Applications](docs/SUPPORTED_APPLICATIONS.md)
- [Documentation](#documentation)
- [Publications & Citing](#publications--citing)

---

## Overview

COEUS-Adapter bridges ADIOS2 and IOWarp through the ADIOS2 plugin interface. An
application that already writes data with ADIOS2 selects the COEUS engine in its
ADIOS2 configuration and immediately gains:

- **Multi-tiered buffering** through CTE (RAM → NVMe → disk), managed by Chimaera.
- **In-situ analysis** - derived quantities, content hashing, statistical triggers,
  and live visualization computed *while the simulation runs*.
- **Metadata management** - a SQLite-backed catalog for query and analysis.

The single shippable artifact is `libhermes_engine.so`, an ADIOS2
`PluginEngineInterface` implementation, plus two Chimaera modules (`coeus_mdm`,
`rankConsensus`) that run inside the IOWarp runtime.

> **A note on naming.** The backbone I/O engine is now **clio-core** (IOWarp's
> Chimaera runtime + CTE) - Hermes is no longer used. The names `hermes_engine`,
> `PluginName=hermes`, and the `HermesEngine` class are retained from the
> original Hermes-based implementation so existing application configs keep
> working. See [SOURCE_CODE_ANALYSIS.md](SOURCE_CODE_ANALYSIS.md).

## Features

| Feature | What it gives you |
|---|---|
| **Multi-tiered I/O** | Efficient data movement across storage tiers via CTE. |
| **[Derived variables](docs/DERIVED_VARIABLES_AND_HASH.md)** | In-situ `curl`, `Q-criterion`, `variance`, `add`, `mean`, and content **`hash`** - no post-processing pass. |
| **[Trigger-Render-Reason pipeline](docs/TRIGGER_RENDER_REASON_PIPELINE.md)** | Watch a statistic each step, stream the flagged window to a viewer/AI agent, and let the agent steer the run (e.g. early-stop). |
| **[Add-on operators](docs/OPERATORS.md)** | Opt-in consumer tools (e.g. 4th-order time derivatives) that never touch the engine. |
| **[In-situ visualization](docs/IN_SITU_VISUALIZATION.md)** | Live, zero-copy ParaView Catalyst 2 + Fides, Inline or SST, with an experimental MCP AI agent. |
| **Metadata** | SQLite-backed catalog for query and analysis. |

## Installation

**Prerequisites:** [Spack](https://spack.io/), IOWarp (the `iowarp-core` package
= Chimaera runtime + CTE), and ADIOS2 - stock or the custom `adios2-coeus` build
(see below).

> **Which ADIOS2?** You have two options:
> - **Stock ADIOS2** (`spack install adios2`) - fine for multi-tiered I/O and
>   metadata.
> - **Custom `adios2-coeus@vigil`** - ADIOS2 v2.11 plus the COEUS derived-variable
>   commits (`variance`, `mean`, `hash`), shipped in this repo's Spack repo
>   (`CI/coeus`). **Required** for the
>   [Trigger-Render-Reason pipeline](docs/TRIGGER_RENDER_REASON_PIPELINE.md) and
>   [`hash()`](docs/DERIVED_VARIABLES_AND_HASH.md); add the `+kokkos` variant for
>   `hash()`.

```bash
# 1. Add the IOWarp Spack repo and install the runtime + CTE
git clone https://github.com/iowarp/clio-core.git
spack repo add clio-core/installers/spack
spack install iowarp@main

# 2. Install ADIOS2 - pick ONE of the two:
git clone https://github.com/grc-iit/coeus-adapter.git
#  (A) stock ADIOS2 - multi-tiered I/O and metadata only
spack install adios2
#  (B) custom ADIOS2 - also enables the Trigger-Render-Reason pipeline + hash()
spack repo add coeus-adapter/CI/coeus
spack install adios2-coeus@vigil          # add "+kokkos" to enable hash()

# 3. Load dependencies (load whichever ADIOS2 you installed in step 2)
spack load iowarp@main
spack load adios2                          # or: spack load adios2-coeus@vigil

# 4. Build COEUS-Adapter
cd coeus-adapter
mkdir build && cd build
cmake ..
make -j8
```

Useful CMake options: `-Dmeta_enabled=ON` (metadata features),
`-Ddebug_mode=ON` (function tracing), `-DCOEUS_ENABLE_CATALYST=ON`
([in-situ viz](docs/IN_SITU_VISUALIZATION.md)), `-DCOEUS_ENABLE_OPERATORS=ON`
([add-on operators](docs/OPERATORS.md)).

See the **[Installation Guide](install.md)** and **[Build Guide](BUILD_GUIDE.md)**
for details and troubleshooting.

## Usage

COEUS works as an ADIOS2 plugin engine - no application code changes. Point your
ADIOS2 XML at the `hermes` plugin:

```xml
<io name="SimulationOutput">
    <engine type="Plugin">
        <parameter key="PluginName" value="hermes" />
        <parameter key="PluginLibrary" value="hermes_engine" />
    </engine>
</io>
```

The IOWarp runtime (Chimaera + a CTE core pool) must be running before the
application starts - the [Jarvis pipelines](./test/jarvis/jarvis_coeus/pipelines)
launch and wire this up for you.

## Derived Variables and Hash

COEUS computes ADIOS2 derived variables (`curl`, `Q-criterion`, `variance`,
`add`, `mean`, and content **`hash`**) in-situ, once per step - no
post-processing pass. `hash()` lives inside the ADIOS2 fork, so COEUS links no
extra libraries.

➡️  **[Read the Derived Variables and Hash guide →](docs/DERIVED_VARIABLES_AND_HASH.md)**

## Trigger-Render-Reason Pipeline

*(Vigil.)* A closed loop that makes a running simulation self-steering:
**detect** an event via a per-step statistic (`variance` / `mean` /
`dissipation`), **render** the flagged window over SST to a viewer/AI agent, and
**reason** - the agent issues a verdict (e.g. early-stop the run).

➡️  **[Read the Trigger-Render-Reason guide →](docs/TRIGGER_RENDER_REASON_PIPELINE.md)**

## Operators (Add-ons)

Optional, opt-in CTE-consumer tools that add computation *without touching the
engine* (`src/hermes_engine.cc` unchanged). Currently `coeus_tderiv` - 4th-order
time derivatives (`dp/dt`, `d²p/dt²`). Build with `-DCOEUS_ENABLE_OPERATORS=ON`.

➡️  **[Read the Operators (Add-ons) guide →](docs/OPERATORS.md)**

## In-Situ Visualization

Live, zero-copy visualization while the simulation runs, via ParaView Catalyst 2
+ Fides - Inline (single-node) or SST streaming (multi-node), plus an
experimental MCP AI agent. Build with `-DCOEUS_ENABLE_CATALYST=ON`.

➡️  **[Read the In-Situ Visualization guide →](docs/IN_SITU_VISUALIZATION.md)**

## Supported Applications

Tested with WRF, LAMMPS, Gray-Scott, Incompact3d, and OpenFOAM - each with a
ready-to-run Jarvis package and its derived-quantity / in-situ-viz setup.

➡️  **[See the Supported Applications list →](docs/SUPPORTED_APPLICATIONS.md)**

## Documentation

**Feature guides**
- [Derived Variables and Hash](docs/DERIVED_VARIABLES_AND_HASH.md) - in-situ quantities and `hash()`
- [Trigger-Render-Reason Pipeline](docs/TRIGGER_RENDER_REASON_PIPELINE.md) - detect → stream → decide (Vigil)
- [Operators (Add-ons)](docs/OPERATORS.md) - optional consumer tools (time-derivative)
- [In-Situ Visualization](docs/IN_SITU_VISUALIZATION.md) - ParaView Catalyst / Fides
- [Supported Applications](docs/SUPPORTED_APPLICATIONS.md) - tested apps and Jarvis packages

**Setup & reference**
- [Installation Guide](install.md) - detailed setup instructions
- [Build Guide](BUILD_GUIDE.md) - dependencies, CMake options, troubleshooting
- [Source Code Analysis](SOURCE_CODE_ANALYSIS.md) - architecture and the clio-core dependency
- [Gray-Scott Build & Run](docs/BUILD_AND_RUN_GRAY_SCOTT.md) - end-to-end trigger/agent walkthrough
- [Test Applications](./test/) - example applications and integration tests

## Publications & Citing

If you use COEUS-Adapter in your research, please cite the following papers.

**ADIOS2 derived quantities** - Gainaru et al., *To Derive or Not to Derive:
I/O Libraries Take Charge of Derived Quantities Computation*, SBAC-PAD 2024.
[doi:10.1109/SBAC-PAD63648.2024.00030](https://doi.org/10.1109/SBAC-PAD63648.2024.00030)

<details>
<summary>BibTeX</summary>

```bibtex
@inproceedings{10763877,
  author={Gainaru, Ana and Podhorszki, Norbert and Dulac, Liz and Gong, Qian and Klasky, Scott and Eisenhauer, Greg and Kougkas, Antonios and Sun, Xian-He and Lofstead, Jay},
  booktitle={2024 IEEE 36th International Symposium on Computer Architecture and High Performance Computing (SBAC-PAD)},
  title={To Derive or Not to Derive: I/O Libraries Take Charge of Derived Quantities Computation},
  year={2024},
  pages={105-115},
  keywords={Analytical models;Solid modeling;Computational modeling;High performance computing;Redundancy;Distributed databases;Process control;Libraries;Data models;Meteorology;Large-scale I/O;Derived Variables;HPC Analysis;Queries for Scientific Data;HPC Quantities of Interest},
  doi={10.1109/SBAC-PAD63648.2024.00030}
}
```

</details>

**Applying it in COEUS** - Cernuda et al., *Hades: A Context-Aware Active
Storage Framework for Accelerating Large-Scale Data Analysis*, CCGrid 2024.
[doi:10.1109/CCGrid59990.2024.00070](https://doi.org/10.1109/CCGrid59990.2024.00070)

<details>
<summary>BibTeX</summary>

```bibtex
@inproceedings{cernuda2024hades,
  title={Hades: A Context-Aware Active Storage Framework for Accelerating Large-Scale Data Analysis},
  author={Cernuda, Jaime and Logan, Luke and Gainaru, Ana and Klasky, Scott and Lofstead, Jay and Kougkas, Anthony and Sun, Xian-He},
  booktitle={The 24th IEEE/ACM International Symposium on Cluster, Cloud and Internet Computing},
  pages={577--586},
  year={2024},
  address={Philadelphia},
  month={May 6-9},
  doi={10.1109/CCGrid59990.2024.00070}
}
```

</details>

## Acknowledgments

Developed with support from the Department of Energy (DOE) under award
DOE ASCR Award DE-SC0023263.

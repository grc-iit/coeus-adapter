Gray-Scott is a 3D 7-Point stencil code

# Installation
Since gray_scott is installed along with the coeus-adapter, these steps can be skipped.
```bash
git clone https://github.com/pnorbert/adiosvm
pushd adiosvm/Tutorial/gs-mpiio
mkdir build
pushd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make -j8
export GRAY_SCOTT_PATH=`pwd`
popd
popd
```

# Gray Scott

## 1. Setup Environment

Create the environment variables needed by Gray Scott
```bash
spack load mpi
export PATH="${COEUS_Adapter/build/bin}:$PATH"
```````````



## 2. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Gray Scott.

```bash
jarvis pipeline create gray-scott-test
```

## 3. Save Environment

Store the current environment in the pipeline.
```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Gray Scott
```bash
jarvis pipeline append adios2_gray_scott

```

## 5. Run Experiment

Run the experiment
```bash
jarvis pipeline run
```

## 6. Clean Data

Clean data produced by Gray Scott
```bash
jarvis pipeline clean
```

# Gray Scott With Hermes

## 1. Setup Environment

Create the environment variables needed by Hermes + Gray Scott
```bash
# On personal
spack install hermes@master adios2
spack load hermes adios2
# On Ares
spack load hermes@master
# export GRAY_SCOTT_PATH=$/coeus_adapter/build/bin
export PATH="${COEUS_Adapter/build/bin}:$PATH"
```


## 2. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Hermes
and Gray Scott.

```bash
jarvis pipeline create gs-hermes
```

## 3. Save Environment

We must make Jarvis aware of all environment variables needed to execute applications in the pipeline.

```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Hermes, the Hermes MPI-IO interceptor,
and gray-scott
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott engine=hermes 
```

For derived variable with adios2 in hermes:
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott engine=hermes_derived
```

## 5. Run the Experiment

Run the experiment
```bash
jarvis pipeline run
```

## 6. Clean Data

To clean data produced by Hermes + Gray-Scott:
```bash
jarvis pipeline clean
```

# Trigger-gated Catalyst SST stream (Vigil trigger-render-reason)

With `trigger=true` (hermes engines only) the package materializes
`config/hermes_trigger.xml` instead of `config/hermes.xml`: the engine pools
the per-block variance of `trigger_variable` into the exact global variance
at every output step and only ships steps over the Catalyst SST stream once
the trigger fires (the firing step plus the next `trigger_inspect_steps - 1`
steps). Untriggered steps ship nothing, so the SST reader just waits.

```bash
# Paper configuration: derived-variable trigger on variance(V)
jarvis pipeline append adios2_gray_scott engine=hermes_derived trigger=true \
    F=0.08 k=0.03 plotgap=50 steps=5000
```

Trigger knobs (defaults follow
`test/real_apps/gray-scott/adios2-hermes-trigger-sst.xml`; semantics in
`test/real_apps/gray-scott/VARIANCE_TRIGGER.md`):

| Config | Default | Meaning |
|---|---|---|
| `trigger` | `false` | Enable the variance trigger + gated SST |
| `trigger_variable` | auto | `derive/VarV` for `hermes_derived`, raw `V` for `hermes` |
| `trigger_sum_variable` | auto | `derive/AddV` when the trigger variable is derived; `none` = pool with a raw pass |
| `trigger_threshold` | `0.05` | Absolute variance threshold (0 disables) |
| `trigger_baseline_ratio` | `0` | Fire at ratio × first-step baseline instead (0 disables) |
| `trigger_inspect_steps` | `3` | Steps shipped over SST per fire |
| `trigger_refire` | `false` | Allow multiple fires |
| `trigger_log_file` | `shared_dir/trigger_log.jsonl` | Rank 0 fire-event log (JSON lines) |
| `catalyst_stream` | `gs.bp` | SST stream name the reader connects to |
| `sst_data_transport` | `WAN` | SST DataTransport |
| `sst_queue_full_policy` | engine default | `Block` when trigger enabled; override if needed |

Notes:

- Derived trigger variables need `engine=hermes_derived` (the writer's
  `derived=1` flag declares `derive/VarV`/`derive/AddV`) and an ADIOS2 build
  with the `variance` operator (`adios2-coeus@vigil`).
- The SST reader (e.g. `pvbatch catalyst/gs-pipeline.py` or
  `insitu_streaming.py`) connects as usual; it simply receives nothing until
  the trigger fires. Rendezvous still applies: the writer waits for one
  reader at Open.
- Gray-Scott reference points (F=0.08 k=0.03 dt=1 plotgap=50): variance(V)
  baseline ~0.003; threshold 0.05 fires at output ~12 with L=64, output ~20
  with L=256.

# Adios2 Write engine for a BP5 file copy

## 1. Add this package to the Jarvis package folder
Compile the Coeus-adapter with OpenMPI.
## 2. Specify the location where you want the BP5 file copy:
```
jarvis pipeline append adios2_gray_scott engine=hermes bp_file_copy=/path/to/file
```
## 3. Run Gray-Scott
```
jarvis pipeline run
```

## example
```
jarvis pipeline create gray_scott_hermes
spack load hermes@master
module load orangefs/2.10
module load openmpi
export PATH=~/coeus/derived/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/coeus/derived/coeus-adapter/build/bin/:$LD_LIBRARY_PATH
jarvis pipeline env build
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott L=128 engine=hermes_derived out_file=/mnt/ssd/hxu40/out1.bp bp_file_copy=1 ppn=8 nprocs=16 steps=12800
jarvis pipeline append adios2_gray_scott_2 L=128 engine=hermes_derived out_file=/mnt/ssd/hxu40/out2.bp bp_file_copy=2 ppn=8 nprocs=16 steps=12800
jarvis ppl run

```

```
jarvis pkg config adios2_gray_scott out_file=/mnt/ssd/hxu40/out1.bp ppn=16 nprocs=2 steps=400 L=128
jarvis pkg config adios2_gray_scott_2 out_file=/mnt/ssd/hxu40/out2.bp ppn=16 nprocs=2 steps=400 L=128

jarvis pkg config adios2_gray_scott out_file=/mnt/ssd/hxu40/out1.bp ppn=16 nprocs=4 steps=800
jarvis pkg config adios2_gray_scott_2 out_file=/mnt/ssd/hxu40/out2.bp ppn=16 nprocs=4 steps=800


jarvis pkg config adios2_gray_scott out_file=/mnt/ssd/hxu40/out1.bp ppn=16 nprocs=8 steps=1600
jarvis pkg config adios2_gray_scott_2 out_file=/mnt/ssd/hxu40/out2.bp ppn=16 nprocs=8 steps=1600


jarvis pkg config adios2_gray_scott out_file=/mnt/ssd/hxu40/out1.bp ppn=16 nprocs=16 steps=3200
jarvis pkg config adios2_gray_scott_2 out_file=/mnt/ssd/hxu40/out2.bp ppn=16 nprocs=16 steps=3200


```
# paraview 


ParaView is an open-source data analysis and visualization application designed to handle large-scale scientific datasets. It supports interactive and batch processing for visualizing complex simulations and performing quantitative analysis.

## Installation

```bash
spack install paraview
```

## how to use it with Jarvis in cluster

### set the environment
load paraview and openmpi
```
module load paraview
spack load openmpi

```
### create jarvis pipeline
```
jarvis ppl create paraview
jarvis ppl env build
jarvis ppl append paraview port_id=11111
```

### run the application
```
jarvis ppl run
```

---

<details>
<summary><b>Ares-specific setup & troubleshooting</b></summary>

### How to use it on a local computer (connect to Ares)

Run this command in the local terminal:
```
ssh -N -L 11111:localhost:11111 your_id@ares.cs.iit.edu
```
In local ParaView, follow these instructions:
File -> connect </br>
Then set the port number and connect to Ares.

</details>  
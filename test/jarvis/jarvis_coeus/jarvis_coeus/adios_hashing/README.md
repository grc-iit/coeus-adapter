# Gray Scott

## 1. Setup Environment

Create the environment variables needed by Gray Scott, if openmpi and realted 
packages already in your local file, please skip it
```bash
spack load openmpi
spack load adios2@master
```

```
export PATH="${COEUS_Adapter/build/bin}:$PATH"
```



## 2. Create a Pipeline

### 2.1 install jarvis and initiate jarvis
for the install and initiate jarvis, refer these pages:<br>
https://github.com/grc-iit/jarvis-cd/wiki <br>
https://github.com/grc-iit/jarvis-cd/wiki/1.-Getting-Started

### 2.2 set the jarvis hostfile
the hostfile.txt contain the node will be run for gray scott
```
jarvis hostfile set [path-to-hostfile.txt]    
```
### 2.3 add jarvis repos to jarvis
These repos contain the parameters and how to execute gray scoot
```
jarvis repo add coeus-adapter/test/jarvis/jarvis_coeus
```
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

Create a Jarvis pipeline with Gray Scott,
feel free to change the steps, ppn and nprocs.
```bash
jarvis pipeline append adios2_gray_scott engine=bp5 out_file=/bp5/file/without/hashing/location steps=? ppn=? nprocs=?
jarvis pipeline append adios2_hashing engine=bp5 in_filename=bp5_location ppn=? nprocs=? out_filename=/the/location/for/bp5/with/hashing\values write_inputvars=yes

```

## 5. Run Experiment

Run the experiment
```bash
jarvis pipeline run
```
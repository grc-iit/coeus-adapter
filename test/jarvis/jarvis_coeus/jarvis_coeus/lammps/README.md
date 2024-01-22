lammps is a molecule dynamic application.

# Install LAMMPS
use spack to install lammps
```
spack install lammps^adios2@2.9.0^mpi
```

# Download LAMMPS Example scripts
download the lammps script example
```
git clone https://github.com/simongravelle/lammps-input-files.git
```

# Run the LAMMPS
choose a example in lammps-input-files, for example, 2D-lennard-jones-fluid.
In 2D-lennard-jones-fluid folder, run the following command
```
lmp -in input.lammps
```
# Run the LAMMPS with ADIOS
In 2D-lennard-jones-fluid folder/input.lammps, change the following code:
```
thermo 1000
dump mydmp all atom 1000 dump.lammpstrj
run 20000
```
to
```
thermo 1000
dump mydmp all atom/adios 1000 dump.lammpstrj
run 20000
```


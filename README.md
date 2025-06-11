# COEUS-Adapter

This work is an adapter which connects ADIOS to Hermes through the use
of the ADIOS plugins interface.




## Install
Read the guide on [Building coeus-adapter](install.md). 

## Supported applications for coeus-adapter

| application                     | directory                                                                                | derived quantity example | query session examples  | notes  |
|---------------------------------|------------------------------------------------------------------------------------------|--------------------------|---|---|
| WRF (weather forecasting)       | [test/jarvis/jarvis_coeus/jarvis_coeus/wrf](./test/jarvis/jarvis_coeus/jarvis_coeus/wrf) | hash                     |   |   |
| LAMMPS (molecular dynamics)     | test/jarvis/jarvis_coeus/jarvis_coeus/lammps                                             | hash                     |   |   |
| Grey-Scott (Reaction Diffusion) | test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott                                  | curl, add, hash          |   |   |
| IO (hdf5 benchmark)             | test/jarvis/jarvis_coeus/jarvis_coeus/io_comp                                            | add                      |   |   |
| Incompact3D                     | test/jarvis/jarvis_coeus/jarvis_coeus/inCompact3D                                        | Qcrit                    |   |   |
| Openfoam                        | test/jarvis/jarvis_coeus/jarvis_coeus/openform                                           |                          |   |   | 






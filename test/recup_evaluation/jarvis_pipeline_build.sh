jarvis pipeline create gray_scott_bp5 app
jarvis cd ray_scott_bp5
jarvis pipeline append adios2_gray_scott L=128 engine=bp5

jarvis pipeline create adios_hashing app
jarvis cd adios_hashing
jarvis pipeline append adios_hashing engine=bp5 write_inputvars=yes db_path=benchmark_metadata.db

jarvis pipeline create hashing_compare app
jarvis cd hashing_compare
jarvis pipeline append hashing_compare engine=bp5 write_inputvars=no


jarvis ppl create gray_scott bp5 app
jarvis cd gray_scott
jarvis pipeline append adios2_gray_scott L=128 plotgap=1 db_path=benchmark_metadata.db

jarvis pipeline create gray_scott_hermes app
jarvis cd gray_scott_hermes
jarvis pipeline append adios2_gray_scott engine=hermes_derived plotgap=1.0 Execution_order=1
jarvis pipeline append adios2_gray_scott_2 engine=hermes_derived plotgap=1.0 Execution_order=2
jarvis pipeline append adios2_gray_scott_3 engine=hermes_derived plotgap=1.0 Execution_order=3



jarvis pipeline create gray_scott_bp5 app
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott L=128 engine=bp5
jarvis pipeline create adios_hashing app
jarvis cd adios_hashing
jarvis pipeline append adios_hashing engine=bp5

jarvis ppl create
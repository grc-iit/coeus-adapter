"""
USAGE: ./hermes_tests [pipeline-name]
"""

from jarvis_cd.basic.pkg import Pipeline
import time
import sys

# Create baseline pipeline
name = sys.argv[1]
bench = Pipeline().load(name)
# will clear all packages from pipeline
# will not erase env.yaml
bench.clear()
# append packages to pipeline
bench.append('hermes_run', sleep=5)
bench.append('hermes_api', posix=True)
bench.append('ior')

# Scale the pipeline
proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
for nprocs in proc_counts:
    bench.configure('adios2_gray_scott',
                    nprocs=nprocs,
                    ppn=20,
                    steps=100,
                    out_file='mnt/nvme/jcernudagarcia/in.hermes',
                    engine='hermes',
                    full_run=True,
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',
                    )
    bench.configure('adios2_gray_scott_post',
                    nprocs=nprocs,
                    ppn=20,
                    in_filename='mnt/nvme/jcernudagarcia/in.hermes',
                    out_filename='mnt/nvme/jcernudagarcia/out.hermes',
                    engine='hermes',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',
                    )
    start = time.time()
    bench.run()
    stop = time.time()
    print('Time: {} sec', stop - start)
    bench.clean()
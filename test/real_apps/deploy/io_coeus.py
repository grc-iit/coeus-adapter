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
bench.append('io_comp')

# Scale the pipeline
io_sizes = [3, 100, 1024]
proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
for nprocs in proc_counts:
    for io_size in io_sizes:
        bench.configure('io_comp',
                        nprocs=nprocs,
                        ppn=20,
                        N=100,
                        B=io_size,
                        out_file='/mnt/nvme/jcernudagarcia/io.hermes',
                        engine='hermes',
                        db_path='/mnt/nvme/jcernudagarcia/metadata.db'
                        )
        start = time.time()
        bench.run()
        stop = time.time()
        print('Time: {} sec', stop - start)
        bench.clean()

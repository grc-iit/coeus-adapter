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
bench.append('mdm_compare')

# Scale the pipeline
devices = [
    '/mnt/nvme/jcernudagarcia/io.bp',
    '/mnt/ssd/jcernudagarcia/io.bp',
    '/mnt/hdd/jcernudagarcia/client/io.bp'
]
io_sizes = [3, 100, 1024]
proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
for nprocs in proc_counts:
    for io_size in io_sizes:
        for device in devices:
            bench.configure('io_comp',
                            nprocs=nprocs,
                            ppn=20,
                            N=100,
                            B=io_size,
                            out_file=device,
                            engine='bp5',
                            db_path='/mnt/nvme/jcernudagarcia/metadata.db'
                            )
            start = time.time()
            bench.run()
            stop = time.time()
            print('Time: {} sec', stop - start)
            bench.clean()


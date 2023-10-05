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
proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
for nprocs in proc_counts:
    bench.configure('mdm_compare',
                    nprocs=nprocs,
                    ppn=20,
                    N=10000,
                    metadata_engine='hermes',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',)
    start = time.time()
    bench.run()
    stop = time.time()
    print('Time: {} sec', stop - start)
    bench.stop()
    bench.clean()

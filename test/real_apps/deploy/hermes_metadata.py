"""
USAGE: ./hermes_tests [pipeline-name]
"""

from jarvis_cd.basic.pkg import Pipeline
import time
import sys

# Create baseline pipeline
name = sys.argv[1]
N = int(sys.argv[2])
scale = False
if --scale in sys.argv:
    scale = True
    print("Running on high scale")
else:
    print("Running on smaller scales")

bench = Pipeline().load(name)
# will clear all packages from pipeline
# will not erase env.yaml
bench.clear()
# append packages to pipeline
bench.append('hermes_run', sleep=5)
bench.append('mdm_compare')

# Scale the pipeline
if not scale:
    proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
else:
    proc_counts = [160, 320]

for nprocs in proc_counts:
    bench.configure('mdm_compare',
                    nprocs=nprocs,
                    ppn=20,
                    N=N,
                    metadata_engine='hermes',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',)
    start = time.time()
    bench.run()
    stop = time.time()
    print('Time: {} sec', stop - start)
    bench.stop()
    bench.clean()

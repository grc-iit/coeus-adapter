"""
USAGE: ./hermes_tests [pipeline-name]
"""

from jarvis_cd.basic.pkg import Pipeline
import time
import sys

# Create baseline pipeline
name = sys.argv[1]
N = int(sys.argv[2])

if "--reduced" in sys.argv:
    reduced=True
    print("reduced process for hermes")
else:
    reduced = False
    print("normal processes")

bench = Pipeline().load(name)
# will clear all packages from pipeline
# will not erase env.yaml
bench.clear()
# append packages to pipeline
bench.append('hermes_run', sleep=5, ram='16G')
bench.append('mdm_compare')

# Scale the pipeline
if not reduced:
    proc_counts = [1, 2, 4, 8, 16, 20, 40, 80, 160, 320]
    ppn = 20
else:
    proc_counts = [1, 2, 4, 8, 16, 32, 64, 128, 256]
    ppn = 16

for nprocs in proc_counts:
    bench.configure('mdm_compare',
                    nprocs=nprocs,
                    ppn=ppn,
                    N=N,
                    metadata_engine='hermes',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',)
    print(f'Here we go again {nprocs}')
    start = time.time()
    bench.run()
    stop = time.time()
    print('Time: {} sec', stop - start)
    bench.stop()
    bench.clean()

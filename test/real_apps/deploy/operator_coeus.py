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
bench.append('adios2_producer_consumer', 'producer')
bench.append('adios2_producer_consumer', 'consumer')

# Scale the pipeline
proc_counts = [1, 2, 4, 8, 16, 20, 40, 80]
for nprocs in proc_counts:
    bench.configure('producer',
                    nprocs=proc_counts,
                    ppn=20,
                    N=100,
                    out_file='/mnt/nvme/jcernudagarcia/io.hermes',
                    engine='hermes',
                    mode='producer',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',
                    )
    bench.configure('consumer',
                    nprocs=proc_counts,
                    ppn=20,
                    N=100,
                    out_file='/mnt/nvme/jcernudagarcia/io.hermes',
                    engine='hermes',
                    mode='consumer',
                    db_path='/mnt/nvme/jcernudagarcia/metadata.db',
                    )
    start = time.time()
    bench.run()
    stop = time.time()
    print('Time: {} sec', stop - start)
    bench.clean()

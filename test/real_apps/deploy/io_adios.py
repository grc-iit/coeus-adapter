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
bench.append('orangefs', ares=True)
bench.append('io_comp')

# Scale the pipeline
devices = ['nvme', 'hdd']
io_sizes = [256, 65536, 1048576]
if not reduced:
    proc_counts = [1, 2, 4, 8, 16, 20, 40, 80, 160, 320]
    ppn = 20
else:
    proc_counts = [1, 2, 4, 8, 16, 32, 64, 128, 256]
    ppn = 16

for nprocs in proc_counts:
    for io_size in io_sizes:
        for device in devices:
            mount = f'/mnt/{device}/jcernudagarcia/client/'
            outfile = mount+"io.bp"
            bench.configure('orangefs',
                            dev_type=device,
                            mount=mount,
                            )
            bench.configure('io_comp',
                            nprocs=nprocs,
                            ppn=ppn,
                            N=N,
                            B=io_size,
                            out_file=outfile,
                            engine='bp5',
                            db_path='/mnt/nvme/jcernudagarcia/metadata.db'
                            )
            print(f'Executing with {nprocs}/{ppn} executing {N} operations of size {io_size} processed over {device} in {outfile}')
            start = time.time()
            bench.run()
            stop = time.time()
            print('Time: {} sec', stop - start)
            bench.clean()


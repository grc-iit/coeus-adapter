from py_coeus_ci.test_manager_bench import TestManager
from jarvis_util import *
from jarvis_util.shell.slurm import SlurmInfo, Slurm
import time

class NativeTestManager(TestManager):
    def spawn_all_nodes(self):
        return self.spawn_info()

    def set_paths(self):
        self.INSTALL_PATH= f"{self.CMAKE_BINARY_DIR}/bin"
        self.GRAY_SCOTT_PATH = f"{self.CMAKE_SOURCE_DIR}/test/real_apps/gray-scott"
        self.HOSTFILE_PATH = f"{self.CMAKE_SOURCE_DIR}/test/benchmark"
        self.BASIC_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic"
        self.BASIC_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic_multi_variable"

        self.SPLIT_PUT_SINGLE_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_single_var_put"
        self.SPLIT_GET_SINGLE_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_single_var_get"

        self.SPLIT_PUT_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_multi_var_put"
        self.SPLIT_GET_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_multi_var_get"

        self.SPLIT_PUT_METADATA_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_metadata_put"
        self.SPLIT_GET_METADATA_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_metadata_get"


    def prepare_simulation(self, mode):
        Mkdir(self.INSTALL_PATH)
        if mode == "file":
            Copy(f"{self.GRAY_SCOTT_PATH}/adios2.xml", self.INSTALL_PATH)
        else:
            Copy(f"{self.GRAY_SCOTT_PATH}/adios2-hermes.xml ", f"{self.INSTALL_PATH}/adios2.xml")

        #Mkdir(f"results", spawn_info)
        Mkdir(f"{self.INSTALL_PATH}/results")
        Mkdir(f"{self.INSTALL_PATH}/logs")
        Copy(f"{self.GRAY_SCOTT_PATH}/adios2-inline-plugin.xml", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/adios2-inline-plugin.xml", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/adios2-fides-staging.xml", self.INSTALL_PATH)

        Copy(f"{self.GRAY_SCOTT_PATH}/visit-bp4.session", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/visit-bp4.session.gui", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/visit-sst.session", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/visit-sst.session.gui", self.INSTALL_PATH)

        Copy(f"{self.GRAY_SCOTT_PATH}/simulation/settings-files.json", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/simulation/settings-staging.json", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/simulation/settings-inline.json", self.INSTALL_PATH)

        Copy(f"{self.GRAY_SCOTT_PATH}/plot/decomp.py", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/plot/gsplot.py", self.INSTALL_PATH)
        Copy(f"{self.GRAY_SCOTT_PATH}/plot/pdfplot.py", self.INSTALL_PATH)

        Copy(f"{self.GRAY_SCOTT_PATH}/cleanup.sh", self.INSTALL_PATH)


    def clean_simulation(self):
        spawn_info = self.spawn_info(cwd=self.INSTALL_PATH)
        paths_to_remove = ["*.xml", "*.bp", "*.bp.dir", "*.h5", "*.sst", "*.ssc",
                           "*_insitumpi_*", "*.png", "*.pnm", "*.jpg", "*.log", "*.txt"]
        rm = Rm(paths_to_remove, spawn_info)
        return rm

    def get_slurm_info(self, nprocs):
        if nprocs in ["1", "2", "4", "8", "16"]:
            num_nodes = 1
            node_list = "ares-comp-01"
        elif nprocs == "32":
            num_nodes = 2
            node_list = "ares-comp-[01-02]"
        elif nprocs == "64":
            num_nodes = 4
            node_list = "ares-comp-[01-04]"
        elif nprocs == "128":
            num_nodes = 7
            node_list = "ares-comp-[01-07]"
        elif nprocs == "256":
            num_nodes = 13
            node_list = "ares-comp-[01-13]"
        elif nprocs == "512":
            num_nodes = 26
            node_list = "ares-comp-[01-26]"
        elif nprocs == "640":
            num_nodes = 32
            node_list = "ares-comp-[01-32]"
        else:
            raise ValueError(f"Unsupported num_processes value: {nprocs}")
        return num_nodes, node_list

    def test_gray_scott_simulation_file_bench(self, num_processes):
        self.prepare_simulation("file")
        num_nodes, node_list = self.get_slurm_info(num_processes)
        slurm_info = SlurmInfo(nnodes=num_nodes, node_list=node_list)
        slurm = Slurm(slurm_info=slurm_info)
        slurm.allocate()
        hostfile = slurm.get_hostfile()
        hosts_str = ','.join(hostfile)

        cmd = f"mpirun -n {num_processes} --hosts {hosts_str} -ppn 20 --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-gray-scott simulation/settings-files.json"
        first_host = hostfile[0]
        ssh_info = ExecInfo(hosts=first_host)
        simulation = SshExec(cmd, ssh_info)
        slurm.exit()

        Copy(f"{self.CMAKE_BINARY_DIR}/logs/sim_test.txt", f"{self.INSTALL_PATH}/logs/sim_bp5_procs_{num_processes}.txt")
        self.clean_simulation()
        return simulation.exit_code


    def test_gray_scott_simulation_hermes_bench(self, num_processes):
        self.prepare_simulation("hermes")
        num_nodes, node_list = self.get_slurm_info(num_processes)
        slurm_info = SlurmInfo(nnodes=num_nodes, node_list=node_list)
        slurm = Slurm(slurm_info=slurm_info)
        slurm.allocate()
        hostfile = slurm.get_hostfile()
        ssh_info = SshExecInfo(
            hostfile=hostfile,
            nprocs=num_processes,
            ppn=2
        )
        cmd = f"mpirun -n {num_processes} --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-gray-scott simulation/settings-files.json"
        simulation = SshExec(cmd, ssh_info)
        slurm.exit()

        Copy(f"{self.CMAKE_BINARY_DIR}/logs/sim_test.txt", f"{self.INSTALL_PATH}/logs/sim_hermes_procs_{num_processes}.txt")
        self.clean_simulation()
        return simulation.exit_code


    def test_gray_scott_analysis_file_bench(self, num_processes):
        self.prepare_simulation("file")
        num_nodes, node_list = self.get_slurm_info(num_processes)
        slurm_info = SlurmInfo(nnodes=num_nodes, node_list=node_list)
        slurm = Slurm(slurm_info=slurm_info)
        slurm.allocate()
        hostfile = slurm.get_hostfile()
        ssh_info = SshExecInfo(
            hostfile=hostfile,
            nprocs=num_processes,
            ppn=2
        )
        cmd_sim = f"mpirun -n {num_processes} --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-gray-scott simulation/settings-files.json"
        cmd_an = f"mpirun -n {num_processes} --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-pdf-calc gs.bp pdf.bp"
        simulation = SshExec(cmd_sim, ssh_info)
        analysis = SshExec(cmd_an, ssh_info)
        slurm.exit()

        Copy(f"{self.CMAKE_BINARY_DIR}/logs/inquire_an_logs.txt", f"{self.INSTALL_PATH}/logs/inquire_bp5_procs_{num_processes}.txt")
        Copy(f"{self.CMAKE_BINARY_DIR}/logs/minmax_an_logs.txt", f"{self.INSTALL_PATH}/logs/minmax_bp5_procs_{num_processes}.txt")
        Copy(f"{self.CMAKE_BINARY_DIR}/logs/total_an_logs.txt", f"{self.INSTALL_PATH}/logs/an_bp5_procs_{num_processes}.txt")

        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code


    def test_gray_scott_analysis_hermes_bench(self, num_processes):
        self.prepare_simulation("file")
        num_nodes, node_list = self.get_slurm_info(num_processes)
        slurm_info = SlurmInfo(nnodes=num_nodes, node_list=node_list)
        slurm = Slurm(slurm_info=slurm_info)
        slurm.allocate()
        hostfile = slurm.get_hostfile()
        ssh_info = SshExecInfo(
            hostfile=hostfile,
            nprocs=num_processes,
            ppn=2
        )
        cmd_sim = f"mpirun -n {num_processes} --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-gray-scott simulation/settings-files.json"
        cmd_an = f"mpirun -n {num_processes} --wdir {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-pdf-calc gs.bp pdf.bp"
        simulation = SshExec(cmd_sim, ssh_info)
        analysis = SshExec(cmd_an, ssh_info)
        slurm.exit()

        Copy(f"{self.CMAKE_BINARY_DIR}/logs/inquire_an_logs.txt", f"{self.INSTALL_PATH}/logs/inquire_hermes_procs_{num_processes}.txt")
        Copy(f"{self.CMAKE_BINARY_DIR}/logs/minmax_an_logs.txt", f"{self.INSTALL_PATH}/logs/minmax_hermes_procs_{num_processes}.txt")
        Copy(f"{self.CMAKE_BINARY_DIR}/logs/total_an_logs.txt", f"{self.INSTALL_PATH}/logs/an_hermes_procs_{num_processes}.txt")

        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code

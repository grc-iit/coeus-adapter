from py_coeus_ci.test_manager import TestManager
from jarvis_util import *

class NativeTestManager(TestManager):
    def spawn_all_nodes(self):
        return self.spawn_info()

    def set_paths(self):
        self.INSTALL_PATH= f"{self.CMAKE_BINARY_DIR}/bin"
        self.GRAY_SCOTT_PATH = f"{self.CMAKE_SOURCE_DIR}/test/real_apps/gray-scott"
        self.BASIC_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic"
        self.BASIC_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic_multi_variable"

        self.SPLIT_PUT_SINGLE_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_single_var_put"
        self.SPLIT_GET_SINGLE_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_single_var_get"

        self.SPLIT_PUT_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_multi_var_put"
        self.SPLIT_GET_MULTI_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_multi_var_get"

        self.SPLIT_PUT_METADATA_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_metadata_put"
        self.SPLIT_GET_METADATA_CMD = f"{self.CMAKE_BINARY_DIR}/bin/split_metadata_get"


    def test_basic(self):
            spawn_info = self.spawn_info(nprocs=1,
                                         hermes_conf='hermes_server')
            self.start_daemon(spawn_info)
            node = Exec(self.BASIC_CMD, spawn_info)
            self.stop_daemon(spawn_info)
            return node.exit_code

    def test_basic_multi_variable(self):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        node = Exec(self.BASIC_MULTI_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return node.exit_code

    def test_split_single_variable(self):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        put = Exec(self.SPLIT_PUT_SINGLE_CMD, spawn_info)
        get = Exec(self.SPLIT_GET_SINGLE_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return put.exit_code + get.exit_code

    def test_split_multiple_variable(self):
        spawn_info = self.spawn_info(nprocs=1,
                                         hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        put = Exec(self.SPLIT_PUT_MULTI_CMD, spawn_info)
        get = Exec(self.SPLIT_GET_MULTI_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return put.exit_code + get.exit_code

    def test_split_metadata(self):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        put = Exec(self.SPLIT_PUT_METADATA_CMD, spawn_info)
        get = Exec(self.SPLIT_GET_METADATA_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return put.exit_code + get.exit_code


    def prepare_simulation(self, mode):
        Mkdir(self.INSTALL_PATH)
        if mode == "file":
            Copy(f"{self.GRAY_SCOTT_PATH}/adios2.xml", self.INSTALL_PATH)
        else:
            Copy(f"{self.GRAY_SCOTT_PATH}/adios2-hermes.xml ", f"{self.INSTALL_PATH}/adios2.xml")

        #Mkdir(f"results", spawn_info)
        Mkdir(f"{self.INSTALL_PATH}/results")
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
                           "*_insitumpi_*", "*.png", "*.pnm", "*.jpg", "*.log"]
        rm = Rm(paths_to_remove, spawn_info)
        return rm

    def test_gray_scott_simulation_file(self):
        self.prepare_simulation("file")
        spawn_info = self.spawn_info(cwd=self.INSTALL_PATH)
        simulation = Exec(f"mpirun ./adios2-gray-scott settings-files.json", spawn_info)
        Copy(f"gs.bp", f"results/sim_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/sim_ckpt.bp", spawn_info)
        self.clean_simulation()
        return simulation.exit_code


    def test_gray_scott_analysis_file(self):
        self.prepare_simulation("file")
        spawn_info = self.spawn_info(cwd=self.INSTALL_PATH)
        simulation = Exec(f"mpirun ./adios2-gray-scott settings-files.json", spawn_info)
        analysis = Exec(f"mpirun ./adios2-pdf-calc gs.bp pdf.bp 100", spawn_info)
        Copy(f"gs.bp", f"results/ana_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/ana_ckpt.bp", spawn_info)
        Copy(f"pdf.bp", f"results/ana_pdf.bp", spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code

    def test_gray_scott_simulation_file_parallel(self):
        self.prepare_simulation("file")
        spawn_info = self.spawn_info(cwd=self.INSTALL_PATH)
        simulation = Exec(f"mpirun -n 4 ./adios2-gray-scott settings-files.json", spawn_info)
        Copy(f"gs.bp", f"results/sim_par_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/sim_par_ckpt.bp", spawn_info)
        self.clean_simulation()
        return simulation.exit_code

    def test_gray_scott_analysis_file_parallel(self):
        self.prepare_simulation("file")
        spawn_info = self.spawn_info(cwd=self.INSTALL_PATH)
        simulation = Exec(f"mpirun -n 2 ./adios2-gray-scott settings-files.json", spawn_info)
        analysis = Exec(f"mpirun -n 2 ./adios2-pdf-calc gs.bp pdf.bp 100", spawn_info)
        Copy(f"gs.bp", f"results/ana_par_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/ana_par_ckpt.bp", spawn_info)
        Copy(f"pdf.bp", f"results/ana_par_pdf.bp", spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code

    # Testing with Hermes
    def test_gray_scott_simulation_hermes(self):
        self.prepare_simulation("Hermes")
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server',
                                     cwd=self.INSTALL_PATH)
        self.start_daemon(spawn_info)
        simulation = Exec(f"mpirun ./adios2-gray-scott settings-files.json", spawn_info)
        Copy(f"gs.bp", f"results/sim_her_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/sim_her_ckpt.bp", spawn_info)
        self.stop_daemon(spawn_info)
        self.clean_simulation()
        return simulation.exit_code

    def test_gray_scott_analysis_hermes(self):
        self.prepare_simulation("Hermes")
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server',
                                     cwd=self.INSTALL_PATH)
        self.start_daemon(spawn_info)
        simulation = Exec(f"mpirun ./adios2-gray-scott settings-files.json", spawn_info)
        analysis = Exec(f"mpirun ./adios2-pdf-calc gs.bp pdf.bp 100", spawn_info)
        Copy(f"gs.bp", f"results/ana_her_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/ana_her_ckpt.bp", spawn_info)
        Copy(f"pdf.bp", f"results/ana_her_pdf.bp", spawn_info)
        self.stop_daemon(spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code

    def test_gray_scott_simulation_hermes_parallel(self):
        self.prepare_simulation("Hermes")
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server',
                                     cwd=self.INSTALL_PATH)
        self.start_daemon(spawn_info)
        simulation = Exec(f"mpirun -n 2 ./adios2-gray-scott settings-files.json", spawn_info)
        Copy(f"gs.bp", f"results/sim_her_par_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/sim_her_par_ckpt.bp", spawn_info)
        self.stop_daemon(spawn_info)
        self.clean_simulation()
        return simulation.exit_code

    def test_gray_scott_analysis_hermes_parallel(self):
        self.prepare_simulation("Hermes")
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server',
                                     cwd=self.INSTALL_PATH)
        self.start_daemon(spawn_info)
        simulation = Exec(f"mpirun -n 2 ./adios2-gray-scott settings-files.json", spawn_info)
        analysis = Exec(f"mpirun -n 2 ./adios2-pdf-calc gs.bp pdf.bp 100", spawn_info)

        Copy(f"gs.bp", f"results/ana_her_par_gs.bp", spawn_info)
        Copy(f"ckpt.bp", f"results/ana_her_par_ckpt.bp", spawn_info)
        Copy(f"pdf.bp", f"results/ana_her_par_pdf.bp", spawn_info)
        self.stop_daemon(spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code


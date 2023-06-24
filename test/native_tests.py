from py_coeus_ci.test_manager import TestManager
from jarvis_util import *

class NativeTestManager(TestManager):
    def spawn_all_nodes(self):
        return self.spawn_info()

    def set_paths(self):
        self.INSTALL_PATH= f"{self.CMAKE_BINARY_DIR}/bin"
        self.GRAY_SCOTT_PATH = f"{self.CMAKE_SOURCE_DIR}/test/real_apps/gray-scott"
        self.GSBP_PATH = f"{self.CMAKE_BINARY_DIR}/test/real_apps/gray-scott"

        self.BASIC_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic"
        self.GraySIM_CMD = f"{self.CMAKE_BINARY_DIR}/bin/adios2-gray-scott"
        self.GrayCalc_CMD = f"{self.CMAKE_BINARY_DIR}/bin/adios2-pdf-calc"


    def test_basic(self):
            spawn_info = self.spawn_info(nprocs=1,
                                         hermes_conf='hermes_server')
            self.start_daemon(spawn_info)
            node = Exec(self.BASIC_CMD, spawn_info)
            self.stop_daemon(spawn_info)
            return node.exit_code

    def prepare_simulation(self):
        Mkdir(self.INSTALL_PATH)

        Copy(f"{self.GRAY_SCOTT_PATH}/adios2.xml", self.INSTALL_PATH)
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
        spawn_info = self.spawn_info()
        clean = Exec(f"{self.GRAY_SCOTT_PATH}/cleanup.sh", spawn_info)
        return clean
    
    def test_gray_scott_simulation_file(self):
        self.prepare_simulation()
        spawn_info = self.spawn_info()
        simulation = Exec(f"mpirun {self.GraySIM_CMD} {self.INSTALL_PATH}/settings-files.json", spawn_info)
        self.clean_simulation()
        return simulation.exit_code


    def test_gray_scott_analysis_file(self):
        self.prepare_simulation()
        spawn_info = self.spawn_info()
        simulation = Exec(f"mpirun {self.GraySIM_CMD} {self.INSTALL_PATH}/settings-files.json", spawn_info)
        analysis = Exec(f"mpirun {self.GrayCalc_CMD} {self.GSBP_PATH}/gs.bp {self.GSBP_PATH}/pdf.bp 100", spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code

    def test_gray_scott_simulation_file_parallel(self):
        self.prepare_simulation()
        spawn_info = self.spawn_info()
        simulation = Exec(f"mpirun -n 4 {self.GraySIM_CMD} {self.INSTALL_PATH}/settings-files.json", spawn_info)
        self.clean_simulation()
        return simulation.exit_code

    def test_gray_scott_analysis_file_parallel(self):
        self.prepare_simulation()
        spawn_info = self.spawn_info()
        simulation = Exec(f"mpirun -n 4 {self.GraySIM_CMD} {self.INSTALL_PATH}/settings-files.json", spawn_info)
        analysis = Exec(f"mpirun -n 2 {self.GrayCalc_CMD} {self.GSBP_PATH}/gs.bp {self.GSBP_PATH}/pdf.bp 100", spawn_info)
        self.clean_simulation()
        return simulation.exit_code + analysis.exit_code
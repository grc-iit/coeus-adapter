from py_coeus_ci.test_manager import TestManager
from jarvis_util import *

class NativeTestManager(TestManager):
    def spawn_all_nodes(self):
        return self.spawn_info()

    def set_paths(self):
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

    def test_gray_scott_sim(self):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        node = Exec(self.GraySIM_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return node.exit_code

    def test_pdf_calc(self, input_file, output_file, num_bins, output_inputdata):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        node = Exec(self.GrayCalc_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return node.exit_code
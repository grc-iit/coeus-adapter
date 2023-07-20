from py_coeus_ci.test_manager_bench import TestManager
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


    def test_gray_scott_simulation_file_bench(self, num_processes):
        spawn_info = self.spawn_info(cwd=f"{self.GRAY_SCOTT_PATH}")
        simulation = Exec(f"mpirun -n {num_processes} {self.INSTALL_PATH}/adios2-gray-scott simulation/settings-files.json", spawn_info)
        return simulation.exit_code


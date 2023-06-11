from py_hermes_ci.test_manager import TestManager
from jarvis_util import *

class NativeTestManager(TestManager):
    def spawn_all_nodes(self):
        return self.spawn_info()

    def set_paths(self):
        self.BASIC_CMD = f"{self.CMAKE_BINARY_DIR}/bin/basic"

    def basic(self):
        spawn_info = self.spawn_info(nprocs=1,
                                     hermes_conf='hermes_server')
        self.start_daemon(spawn_info)
        node = Exec(self.BASIC_CMD, spawn_info)
        self.stop_daemon(spawn_info)
        return node.exit_code

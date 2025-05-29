"""
This module provides classes and methods to launch the Incompact3d application.
Incompact3d is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class Incompact3d(Application):
    """
    This class provides methods to launch the Incompact3d application.
    """
    def _init(self):
        """
        Initialize paths
        """
        pass

    def _configure_menu(self):
        """
        Create a CLI menu for the configurator method.
        For thorough documentation of these parameters, view:
        https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing

        :return: List(dict)
        """
        return [
            {
                'name': 'nprocs',
                'msg': 'Number of processes',
                'type': int,
                'default': 1,
            },
            {
                'name': 'ppn',
                'msg': 'The number of processes per node',
                'type': int,
                'default': 16,
            },
            {
                'name': 'example_location',
                'msg': 'The location of Incompact3D',
                'type': str,
                'default': None,
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes'],
                'type': str,
                'default': 'bp5',
            },
            {
                'name': 'Execution_order',
                'msg': 'Path where the bp5 will be stored',
                'type': str,
                'default': None,
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        pass

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        Exec('xcompact3d',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=self.config['example_location']
                         ))
        pass

    def stop(self):
        """
        Stop a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        pass

    def clean(self):
        """
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        pass
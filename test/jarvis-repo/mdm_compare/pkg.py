"""
This module provides classes and methods to launch the Gray Scott application.
Gray Scott is a 3D 7-point stencil code for modeling the diffusion of two
substances.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import pathlib


class CoeusMdmCompare(Application):
    """
    This class provides methods to launch the GrayScott application.
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
                'msg': 'Number of processes to spawn',
                'type': int,
                'default': 4,
            },
            {
                'name': 'ppn',
                'msg': 'Processes per node',
                'type': int,
                'default': None,
            },
            {
                'name': 'N',
                'msg': 'Number of steps',
                'type': int,
                'default': 100,
            },
            {
                'name': 'metadata_engine',
                'msg': 'Engine to be used',
                'choices': ['empress', 'hermes'],
                'type': str,
                'default': 'empress',
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
        ]

    def configure(self, **kwargs):
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
        num_steps = self.config['N']
        db_path = self.config['db_path']
        if lower(self.config['metadata_engine']) == 'empress':
            Exec(f'metadata_empress {num_steps} {db_path}',
                 MpiExecInfo(nprocs=self.config['nprocs'],
                             ppn=self.config['ppn'],
                             hostfile=self.jarvis.hostfile,
                             env=self.env))
        elif lower(self.config['metadata_engine']) == 'hermes':
            Exec(f'metadata_hermes {num_steps}',
                 MpiExecInfo(nprocs=self.config['nprocs'],
                             ppn=self.config['ppn'],
                             hostfile=self.jarvis.hostfile,
                             env=self.env))
        else:
            raise Exception('Engine not defined')

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
        output_dir = self.config['db_path']
        print(f'Removing {output_dir}')
        Rm(output_dir, PsshExecInfo(hostfile=self.jarvis.hostfile))
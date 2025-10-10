"""
This module provides classes and methods to launch the Incompact3d application.
Incompact3d is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import os

class InCompact3D(Application):
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
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes'],
                'type': str,
                'default': 'bp5',
            },
            {
                'name': 'benchmarks',
                'msg': 'The name of benchmarks',
                'choices': ['ABL', 'cavity', 'channel', 'cylinder', 'pipe_flow',
                            'TBL',  'TGV'],
                'type': str,
                'default': 'TGV',
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            {
                'name': 'total_step',
                'msg': 'Total number of steps to be simulated',
                'type': int,
                'default': 1000,
            },
            {
                'name': 'io_frequency',
                'msg': 'Frequency of I/O operations',
                'type': int,
                'default': 1,
            },
            {
                'name': 'output_location',
                'msg': 'Path where the output file will be stored',
                'type': str,
                'default': 'data.bp5',
            },
            {
                'name': 'logs',
                'msg': 'Path where the log file will be stored',
                'type': str,
                'default': 'logs.txt',
            },

        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        # Explicitly check if the directory exists, then create it
        if self.config['engine'].lower() == 'bp5':
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                    f'{self.config['output_location']}/adios2_config.xml')
        if self.config['engine'].lower() in ['hermes']:
            self.copy_template_file(f'{self.pkg_dir}/config/hermes.xml',
                                    f'{self.config['output_location']}/adios2_config.xml', replacements={
                    'ppn': self.config['ppn'],
                    'db_path': self.config['db_path'],
                })
        input_i3d = f"{self.pkg_dir}/benchmarks/{self.config['benchmarks'].lower()}/input.i3d"
        self.copy_template_file(f'{input_i3d}',
                                f'{self.config['output_location']}/input.i3d', replacements={
                'total_step': self.config['total_step'],
                'io_frequency': self.config['io_frequency'],})
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
                         cwd=self.config['output_location']
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
        output_file = self.config['output_location'] + '/data.bp5'
        output_files = [output_file,
                       self.config['db_path']
                       ]

        print(f'Removing {output_files}')
        Rm(output_files, PsshExecInfo(hostfile=self.jarvis.hostfile))
        pass
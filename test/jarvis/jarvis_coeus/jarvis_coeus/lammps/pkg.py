"""
This module provides classes and methods to launch the Lammps application.
Lammps is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class Lammps(Application):
    """
    This class provides methods to launch the Lammps application.
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
                'default': 4,
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp4', 'hermes'],
                'type': str,
                'default': 'bp4',
            },
            {
                'name': 'script_location',
                'msg': 'the location of lammps script',  # the location for lammps scirpt
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
       if self.config['engine'].lower() == 'bp4':
          self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                  f'{self.config["script_location"]}/adios_config.xml')
       elif  self.config['engine'].lower == 'hermes':
           replacement = [("ppn", self.config['ppn']), ("DB_FIEL", self.config['db_file'])]
           self.copy_template_file(f'{self.pkg_dir}/config/hermes.xml',
                                   f'{self.config["script_location"]}/adios_config.xml', replacement)
       else:
           raise Exception('Engine not defined')
       self.update_config(kwargs, rebuild=False)

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        Exec('lmp -in input.lammps',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=self.config['script_location']))
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

        output_file = [self.config['db_path']]
        Rm(output_file, PsshExecInfo(hostfile=self.jarvis.hostfile))
        pass

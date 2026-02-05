"""
This module provides classes and methods to launch the Incompact3d application.
Incompact3d is ....
"""
from jarvis_cd.core.pkg import Application
from jarvis_cd.shell import Exec, MpiExecInfo, PsshExecInfo, Mkdir, Rm
import os


class Incompact3d(Application):
    """
    This class provides methods to launch the Incompact3d application.
    """
    def _init(self):
        """
        Initialize paths (will be set in _configure when directories are available).
        """
        self.adios2_xml_path = None

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
            # Note: 'partical' is kept as-is to match existing directory name
            {
                'name': 'benchmarks',
                'msg': 'The name of benchmarks',
                'choices': ['abl', 'cavity', 'channel', 'cylinder', 'pipe_flow',
                            'tbl',  'tgv', 'mdh', 'periodic', 'partical', 'mixing_layer'],
                'type': str,
                'default': 'tgv',
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
                'msg': 'Path where the output directory will be stored',
                'type': str,
                'default': 'output',
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
        self.update_config(kwargs, rebuild=False)

        output_location = self.config['output_location']
        # Resolve pkg_dir so templates are found (works when run from source or installed)
        pkg_dir = getattr(self, 'pkg_dir', None) or os.path.dirname(os.path.abspath(__file__))
        os.makedirs(output_location, exist_ok=True)

        adios2_dest = os.path.join(output_location, 'adios2_config.xml')
        # Copy configuration files based on engine type
        if self.config['engine'].lower() == 'bp5':
            self.copy_template_file(
                os.path.join(pkg_dir, 'config', 'adios2.xml'),
                adios2_dest)
        elif self.config['engine'].lower() == 'hermes':
            self.copy_template_file(
                os.path.join(pkg_dir, 'config', 'hermes.xml'),
                adios2_dest,
                replacements={
                    'ppn': self.config['ppn'],
                    'db_path': self.config['db_path'],
                })
        else:
            raise Exception('Engine not defined: use bp5 or hermes')

        # Copy input file template
        benchmark = self.config['benchmarks'].lower()
        input_i3d_src = os.path.join(pkg_dir, 'benchmarks', benchmark, 'input.i3d')
        input_i3d_dest = os.path.join(output_location, 'input.i3d')
        self.copy_template_file(
            input_i3d_src,
            input_i3d_dest,
            replacements={
                'total_step': self.config['total_step'],
                'io_frequency': self.config['io_frequency'],
            })

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        # Set OpenMPI MCA parameters as environment variables
        os.environ['OMPI_MCA_pml'] = 'ob1'
        os.environ['OMPI_MCA_btl'] = 'tcp,self'
        os.environ['OMPI_MCA_osc'] = '^ucx'
        # Note: Network interface 'eno1' is hardcoded - may need to be configurable
        # for different systems. Consider adding network_interface parameter to config.
        os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
        os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'

        Exec('xcompact3d',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=self.config['output_location']
                         )).run()

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
        output_location = self.config['output_location']
        output_files = [
            os.path.join(output_location, 'data.bp5'),
            os.path.join(output_location, 'adios2_config.xml'),
            os.path.join(output_location, 'input.i3d'),
            self.config['db_path'],
        ]

        print(f'Removing {output_files}')
        Rm(output_files, PsshExecInfo(hostfile=self.jarvis.hostfile)).run()
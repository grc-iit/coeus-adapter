"""
This module provides classes and methods to launch the Gray Scott application.
Gray Scott is a 3D 7-point stencil code for modeling the diffusion of two
substances.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import pathlib


class HashingCompare(Application):
    """
    This class provides methods to launch the GrayScott application.
    """
    def _init(self):
        """
        Initialize paths
        """
        self.adios2_xml_path = f'{self.shared_dir}/adios2.xml'
        self.settings_json_path = f'{self.shared_dir}/settings-files.json'
        self.var_json_path = f'{self.shared_dir}/var.json'
        self.operator_json_path = f'{self.shared_dir}/operator.json'

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
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            {
                'name': 'ppn',
                'msg': 'Processes per node',
                'type': int,
                'default': None,
            },
            {
                'name': 'in_filename',
                'msg': 'Input file location',
                'type': str,
                'default': None,
            },
            {
                'name': 'out_filename',
                'msg': 'Output file location',
                'type': str,
                'default': None,
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes'],
                'type': str,
                'default': "bp5",
            },
        ]
    # jarvis pkg config adios2_gray_scott_post ppn=20 full_run=true engine=hermes db_path=/mnt/nvme/jcernudagarcia/metadata.db in_filename=gs.bp out_filename=post.bp nprocs=1

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        self.update_config(kwargs, rebuild=False)
        if self.config['out_filename'] is None:
            adios_dir = os.path.join(self.shared_dir, 'post-gray-scott-output')
            self.config['out_filename'] = os.path.join(adios_dir, 'data/post.bp')
            Mkdir(adios_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                          env=self.env))
        db_dir = os.path.dirname(self.config['db_path'])
        output_dir = os.path.dirname(self.config['out_filename'])

        Mkdir([output_dir, db_dir], PsshExecInfo(hostfile=self.jarvis.hostfile,
                                       env=self.env))

        if self.config['engine'].lower() == 'bp5':
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                self.adios2_xml_path)
        else:
            raise Exception('Engine not defined')

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """

        in_file = self.config['in_filename']
        out_file = self.config['out_filename']

        cwd = os.path.dirname(self.adios2_xml_path)
        # print(self.env['HERMES_CLIENT_CONF'])
        Exec(f'hashing-comparison {in_file} {out_file} ',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=cwd))
        # cmd_an = f"mpirun -n {num_processes} --hosts {hosts_str} -ppn 20 --wdir \
        #         {self.GRAY_SCOTT_PATH} {self.INSTALL_PATH}/adios2-pdf-calc \
        #         /mnt/hdd/jmendezbenegassimarq/client/gs.bp pdf.bp 100"

    def stop(self):
        """
        Stop a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        pass

    def clean(self):
        """HashingCompare
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        output_dir = [self.config['in_filename'],
                      self.config['out_filename'],
                      self.config['db_path']
                      ]
        print(f'Removing {output_dir}')
        Rm(output_dir, PsshExecInfo(hostfile=self.jarvis.hostfile))

"""
This module provides classes and methods to launch the Gray Scott application.
Gray Scott is a 3D 7-point stencil code for modeling the diffusion of two
substances.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import pathlib


class Adios2GrayScottPost(Application):
    """
    This class provides methods to launch the GrayScott application.
    """
    def _init(self):
        """
        Initialize paths
        """
        self.adios2_xml_path = f'{self.shared_dir}/adios2.xml'
        self.var_json_path = f'{self.shared_dir}/var.yaml'
        self.operator_json_path = f'{self.shared_dir}/operator.yaml'

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
                'default': 16,
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
                'name': 'nbins',
                'msg': 'Number of bins used in the post processing (??)',
                'type': int,
                'default': 100,
            },
            {
                'name': 'write_inputvars',
                'msg': 'Should the read variables we written to the output file',
                'choices': ['yes', 'no'],
                'type': str,
                'default': "no",
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes', 'bp5_derived', 'hermes_derived'],
                'type': str,
                'default': "bp5",
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            {
                'name': 'limit',
                'msg': 'Limit the value of data to track',
                'type': int,
                'default': 5,
            }
        ]
    # jarvis pkg config adios2_gray_scott_post ppn=20 full_run=true engine=hermes db_path=/mnt/nvme/jcernudagarcia/metadata.db in_filename=gs.bp out_filename=post.bp nprocs=1

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        if self.config['out_filename'] is None:
            adios_dir = os.path.join(self.shared_dir, 'post-gray-scott-output')
            self.config['out_filename'] = os.path.join(adios_dir, 'data/post.bp')
            Mkdir(adios_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                          env=self.env))

        output_dir = os.path.dirname(self.config['out_filename'])
        db_dir = os.path.dirname(self.config['db_path'])
        Mkdir([output_dir, db_dir], PsshExecInfo(hostfile=self.jarvis.hostfile,
                                       env=self.env))

        ppn = self.config['ppn']
        replacements = [
            ('PPN', f'{ppn}'),
            ('VARFILE', self.var_json_path),
            ('OPFILE', self.operator_json_path),
            ('DBFILE', self.config['db_path']),
            ('LIMIT', self.config['limit']),
        ]

        print(f"Using engine {self.config['engine']}")
        if self.config['engine'].lower() in  ['bp5', 'bp5_derived']:
            replacements.append(('ENGINE', 'bp5'))
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                self.adios2_xml_path, replacements)

        elif self.config['engine'].lower() in ['hermes', 'hermes_derived']:
            replacements.append(('ENGINE', 'plugin'))
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                    self.adios2_xml_path, replacements)
            self.copy_template_file(f'{self.pkg_dir}/config/var.yaml',
                                    self.var_json_path)
            self.copy_template_file(f'{self.pkg_dir}/config/operator.yaml',
                                    self.operator_json_path)
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
        nbins = self.config['nbins']
        write_inputbars = self.config['write_inputvars']

        cwd = os.path.dirname(self.adios2_xml_path)
        if self.config['engine'].lower() in ['bp5_derived', 'hermes_derived']:
            derived = 1
        elif self.config['engine'].lower() in ['hermes', 'bp5']:
            derived = 0
        else:
            raise Exception('Engine not defined')
        # print(self.env['HERMES_CLIENT_CONF'])
        Exec(f'adios2-pdf-calc {in_file} {out_file} {nbins} {write_inputbars} {derived}',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=cwd,
                         do_dbg=self.config['do_dbg'],
                         dbg_port=self.config['dbg_port']
                         ))
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
        """
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

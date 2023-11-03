"""
This module provides classes and methods to launch the Gray Scott application.
Gray Scott is a 3D 7-point stencil code for modeling the diffusion of two
substances.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import pathlib


class Adios2ProducerConsumer(Application):
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
                'name': 'N',
                'msg': 'Number of steps',
                'type': int,
                'default': 100,
            },
            {
                'name': 'out_file',
                'msg': 'Output file',
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
                'name': 'mode',
                'msg': 'Run the producer or the consumer',
                'choices': ['producer', 'consumer'],
                'type': str,
                'default': 'producer',
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
        if self.config['out_file'] is None:
            adios_dir = os.path.join(self.shared_dir, 'operators')
            self.config['out_file'] = os.path.join(adios_dir,
                                                   'data/out.bp')
            Mkdir(adios_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                          env=self.env))

        output_dir = os.path.dirname(self.config['out_file'])
        db_dir = os.path.dirname(self.config['db_path'])
        Mkdir([output_dir, db_dir], PsshExecInfo(hostfile=self.jarvis.hostfile,
                                       env=self.env))

        ppn = self.config['ppn']
        replacements = [
            ('PPN', f'{ppn}'),
            ('VARFILE', self.var_json_path),
            ('OPFILE', self.operator_json_path),
            ('DBFILE', self.config['db_path']),
        ]

        if self.config['engine'].lower() == 'bp5':
            replacements.append(('ENGINE', 'bp5'))
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                    self.adios2_xml_path, replacements)

        elif self.config['engine'].lower() == 'hermes':
            replacements.append(('ENGINE', 'plugin'))
            self.copy_template_file(f'{self.pkg_dir}/config/hermes.xml',
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

        out_file = self.config['out_file']

        if self.config['mode'].lower() == 'producer':
            exec = 'operator_comp_producer'
            steps_or_engine = self.config['N']
        elif self.config['mode'].lower() == 'consumer':
            exec = 'operator_comp_consumer'
            steps_or_engine = self.config['engine'].lower()
        else:
            raise Exception('Choose either producer or conusmer')

        # print(self.env['HERMES_CLIENT_CONF'])
        Exec(f'{exec} {steps_or_engine} {self.adios2_xml_path} {out_file}',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.env))

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
        if self.config['mode'].lower() == 'producer':
            return
        output_file = [self.config['out_file'],
                       self.config['db_path']
                       ]
        print(f'Removing {output_file}')
        Rm(output_file, PsshExecInfo(hostfile=self.jarvis.hostfile))

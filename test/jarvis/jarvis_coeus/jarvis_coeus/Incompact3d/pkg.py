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
            {
                'name': 'catalyst_stream',
                'msg': 'Catalyst SST stream name (e.g. tgv.bp); empty = no SST',
                'type': str,
                'default': '',
            },
            {
                'name': 'script',
                'msg': 'Catalyst pipeline script (unused in SST mode, must be set)',
                'type': str,
                'default': '',
            },
            {
                'name': 'data_model',
                'msg': 'Fides data model JSON (unused in SST mode, must be set)',
                'type': str,
                'default': '',
            },
            {
                'name': 'sst_data_transport',
                'msg': 'SST DataTransport (WAN works over TCP)',
                'type': str,
                'default': 'WAN',
            },
            {
                'name': 'sst_queue_full_policy',
                'msg': 'SST QueueFullPolicy; empty = engine default '
                       '(Discard ungated, Block when trigger enabled)',
                'type': str,
                'default': '',
            },
            {
                'name': 'trigger',
                'msg': 'Enable the two-stage Yellow/Red dissipation trigger',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_ke_variable',
                'msg': 'Derived block-mean TKE variable',
                'type': str,
                'default': 'tke_mean',
            },
            {
                'name': 'trigger_enstrophy_variable',
                'msg': 'Derived block-mean enstrophy variable',
                'type': str,
                'default': 'enst_mean',
            },
            {
                'name': 'trigger_nu',
                'msg': 'Kinematic viscosity 1/Re; 0 = derive from input.i3d',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_output_dt',
                'msg': 'Sim time between outputs (dt*ioutput); '
                       '0 = derive from input.i3d and io_frequency',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_yellow_fraction',
                'msg': 'Yellow eps_frac threshold',
                'type': float,
                'default': 0.05,
            },
            {
                'name': 'trigger_red_fraction',
                'msg': 'Red eps_frac threshold (fires the SST window)',
                'type': float,
                'default': 0.15,
            },
            {
                'name': 'trigger_yellow_nu_ratio',
                'msg': 'Yellow nu_eff/nu threshold',
                'type': float,
                'default': 1.05,
            },
            {
                'name': 'trigger_red_nu_ratio',
                'msg': 'Red nu_eff/nu threshold (fires the SST window)',
                'type': float,
                'default': 1.2,
            },
            {
                'name': 'trigger_inspect_steps',
                'msg': 'Output steps shipped over SST per fire',
                'type': int,
                'default': 3,
            },
            {
                'name': 'trigger_refire',
                'msg': 'Allow the trigger to re-arm after a fire',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_log_file',
                'msg': 'Rank-0 fire-event JSONL; empty = shared_dir/trigger_log.jsonl',
                'type': str,
                'default': '',
            },
            {
                'name': 'trigger_metrics_log_file',
                'msg': 'Rank-0 per-step metrics JSONL; empty = disabled',
                'type': str,
                'default': '',
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """     
        os.makedirs(self.config['output_location'], exist_ok=True)
        
        # Copy configuration files based on engine type
        if self.config['engine'].lower() == 'bp5':
            self.copy_template_file(f"{self.pkg_dir}/config/adios2.xml",
                        f"{self.config['output_location']}/adios2_config.xml")
        elif self.config['engine'].lower() == 'hermes':
            template = f"{self.pkg_dir}/config/hermes.xml"
            replacements = {
                'ppn': self.config['ppn'],
                'db_path': self.config['db_path'],
            }
            if self.config['trigger']:
                template = f"{self.pkg_dir}/config/hermes_trigger.xml"
                replacements.update(self._trigger_replacements())
            elif self.config['catalyst_stream']:
                template = f"{self.pkg_dir}/config/hermes_sst.xml"
                replacements.update(self._sst_replacements())
            self.copy_template_file(template,
                                    f"{self.config['output_location']}/adios2_config.xml",
                                    replacements=replacements)
        
        # Copy input file template
        input_i3d = f"{self.pkg_dir}/benchmarks/{self.config['benchmarks'].lower()}/input.i3d"
        self.copy_template_file(f'{input_i3d}',
                                f"{self.config['output_location']}/input.i3d", replacements={
                'total_step': self.config['total_step'],
                'io_frequency': self.config['io_frequency'],})
        pass

    def _catalyst_defaults(self):
        """
        Default Script/DataModel for the Catalyst SST stream. The engine
        requires both parameters to enable Catalyst, but in SST mode it
        never reads the files — the external reader loads its own fides
        JSON. Point at the repo's TGV consumer so the values stay honest.
        """
        catalyst_dir = os.path.abspath(
            f'{self.pkg_dir}/../../../../real_apps/Xcompact3d/catalyst')
        script = self.config['script'] or f'{catalyst_dir}/tgv-pipeline.py'
        data_model = self.config['data_model'] or f'{catalyst_dir}/tgv-fides.json'
        return script, data_model

    def _sst_lines(self):
        """
        The solution-io Catalyst SST parameter lines shared by the SST and
        trigger templates.
        """
        script, data_model = self._catalyst_defaults()
        policy_line = ''
        if self.config['sst_queue_full_policy']:
            policy_line = (f'<parameter key="SSTQueueFullPolicy" '
                           f'value="{self.config["sst_queue_full_policy"]}"/>')
        return {
            'script': script,
            'data_model': data_model,
            'catalyst_stream': self.config['catalyst_stream'],
            'sst_data_transport': self.config['sst_data_transport'],
            'sst_queue_policy_line': policy_line,
        }

    def _sst_replacements(self):
        """Replacements for config/hermes_sst.xml (ungated SST stream)."""
        return self._sst_lines()

    def _parse_i3d_param(self, name):
        """
        Read a scalar parameter (e.g. re, dt) from the benchmark's
        input.i3d template. Fortran-style values (1600., 5.d-3) are
        normalized before conversion.
        """
        import re as re_mod
        path = f"{self.pkg_dir}/benchmarks/{self.config['benchmarks'].lower()}/input.i3d"
        pattern = re_mod.compile(rf'^\s*{name}\s*=\s*([0-9.eEdD+\-]+)')
        with open(path) as f:
            for line in f:
                match = pattern.match(line)
                if match:
                    value = match.group(1).replace('d', 'e').replace('D', 'e')
                    return float(value)
        raise ValueError(f'{name} not found in {path}; set the trigger_* '
                         'configs explicitly')

    def _trigger_replacements(self):
        """
        Replacements for config/hermes_trigger.xml (Yellow/Red dissipation
        trigger). trigger_nu and trigger_output_dt default to values derived
        from the benchmark input.i3d (nu = 1/re, output_dt = dt * ioutput).
        """
        if self.config['benchmarks'].lower() != 'tgv':
            print(f'WARNING: dissipation trigger inputs '
                  f'{self.config["trigger_ke_variable"]}/'
                  f'{self.config["trigger_enstrophy_variable"]} are only '
                  f'registered by the TGV case '
                  f'(benchmarks={self.config["benchmarks"]})')

        nu = self.config['trigger_nu']
        if not nu:
            nu = 1.0 / self._parse_i3d_param('re')
        output_dt = self.config['trigger_output_dt']
        if not output_dt:
            output_dt = self._parse_i3d_param('dt') * self.config['io_frequency']

        log_file = self.config['trigger_log_file'] or \
            f'{self.shared_dir}/trigger_log.jsonl'
        metrics_line = ''
        if self.config['trigger_metrics_log_file']:
            metrics_line = (f'<parameter key="TriggerMetricsLogFile" '
                            f'value="{self.config["trigger_metrics_log_file"]}"/>')

        catalyst_block = ''
        if self.config['catalyst_stream']:
            sst = self._sst_lines()
            lines = [
                f'<parameter key="Script" value="{sst["script"]}"/>',
                f'<parameter key="DataModel" value="{sst["data_model"]}"/>',
                f'<parameter key="CatalystStream" value="{sst["catalyst_stream"]}"/>',
                f'<parameter key="SSTDataTransport" value="{sst["sst_data_transport"]}"/>',
            ]
            if sst['sst_queue_policy_line']:
                lines.append(sst['sst_queue_policy_line'])
            catalyst_block = '\n        '.join(lines)

        return {
            'catalyst_block': catalyst_block,
            'trigger_ke_variable': self.config['trigger_ke_variable'],
            'trigger_enstrophy_variable': self.config['trigger_enstrophy_variable'],
            'trigger_nu': f'{nu:.10g}',
            'trigger_output_dt': f'{output_dt:.10g}',
            'trigger_yellow_fraction': self.config['trigger_yellow_fraction'],
            'trigger_red_fraction': self.config['trigger_red_fraction'],
            'trigger_yellow_nu_ratio': self.config['trigger_yellow_nu_ratio'],
            'trigger_red_nu_ratio': self.config['trigger_red_nu_ratio'],
            'trigger_inspect_steps': self.config['trigger_inspect_steps'],
            'trigger_refire': self.config['trigger_refire'],
            'trigger_log_file': log_file,
            'trigger_metrics_line': metrics_line,
        }

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        iface = self.config.get('net_iface', 'eno1')

        mpi_env = {
            'OMPI_MCA_pml': 'ob1',
            'OMPI_MCA_btl': 'tcp,self',
            'OMPI_MCA_osc': '^ucx',
            'OMPI_MCA_btl_tcp_if_include': iface,
        }
        os.environ.update(mpi_env)
        self.mod_env.update(mpi_env)

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
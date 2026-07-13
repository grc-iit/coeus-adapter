"""
This module provides classes and methods to launch the LAMMPS application
through the COEUS hermes plugin engine, with an optional MEAN trigger on the
kinetic temperature (Vigil trigger-render-reason pipeline, LAMMPS
velocity-Verlet integration-failure case).

The modified `dump custom/adios` (our +adios LAMMPS build) declares the derived
variable derive/V2mean = mean(|v|^2) == 3*T*; the engine's mean trigger
(TriggerType=mean) pools it N_b-weighted into the exact global mean and fires
when the temperature runs away from the equilibrium set point.
"""
from jarvis_cd.core.pkg import Application
from jarvis_cd.shell import Exec, MpiExecInfo, PsshExecInfo, Mkdir, Rm
import os


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
                'default': 1,
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp4', 'hermes'],
                'type': str,
                'default': 'hermes',
            },
            {
                'name': 'script_location',
                'msg': 'Directory (NFS-shared) where input.lammps + '
                       'adios2_config.xml are materialized and LAMMPS runs',
                'type': str,
                'default': None,
            },
            {
                'name': 'lmp_bin',
                'msg': 'Path to the +adios LAMMPS binary',
                'type': str,
                'default': '/home/hxu40/software/lammps_bench/lammps/build/lmp',
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            # ----- LJ case parameters (velocity-Verlet integration failure) -----
            {
                'name': 'rho',
                'msg': 'Reduced density rho* (fcc lattice constant)',
                'type': float,
                'default': 0.5,
            },
            {
                'name': 'box',
                'msg': 'Box size in unit cells (fcc: 4*box^3 atoms)',
                'type': int,
                'default': 8,
            },
            {
                'name': 't0',
                'msg': 'Initial/equilibrium reduced temperature T*',
                'type': float,
                'default': 0.75,
            },
            {
                'name': 'dt',
                'msg': 'Reduced timestep dt* (0.05 = 10x too large -> explosion)',
                'type': float,
                'default': 0.05,
            },
            {
                'name': 'steps',
                'msg': 'Number of NVE steps to run (default 5: fires the '
                       'temperature trigger and exits cleanly before the '
                       'step-6 "Lost atoms" crash)',
                'type': int,
                'default': 5,
            },
            {
                'name': 'dump_every',
                'msg': 'Dump/trigger-evaluation interval (steps)',
                'type': int,
                'default': 1,
            },
            # ----- Trigger (Vigil): MEAN on the kinetic temperature -----
            {
                'name': 'trigger',
                'msg': 'Enable the hermes-engine trigger (hermes engine only)',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_type',
                'msg': 'Trigger statistic: "variance" (variance(vx) ~= T*, '
                       'proven path) or "mean" (derive/V2mean = 3*T*)',
                'type': str,
                'default': 'variance',
            },
            {
                'name': 'trigger_variable',
                'msg': 'Derived variable the trigger pools '
                       '(variance => derive/VarVx ~= T*)',
                'type': str,
                'default': 'derive/VarVx',
            },
            {
                'name': 'trigger_sum_variable',
                'msg': 'Derived block-sum for exact variance pooling '
                       '(derive/AddVx); "none" to omit (raw pass instead)',
                'type': str,
                'default': 'derive/AddVx',
            },
            {
                'name': 'trigger_threshold',
                'msg': 'Absolute threshold in trigger-variable units; for '
                       'variance(vx) ~= T*, 0 disables (use baseline_ratio)',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_baseline_ratio',
                'msg': 'Fire when the statistic exceeds ratio x first-step '
                       'baseline; 1.4 => fires when variance(vx) climbs 40% '
                       'above the equilibrium set point (0 disables)',
                'type': float,
                'default': 1.4,
            },
            {
                'name': 'trigger_inspect_steps',
                'msg': 'Output steps shipped per fire (firing step + N-1)',
                'type': int,
                'default': 3,
            },
            {
                'name': 'trigger_refire',
                'msg': 'Allow the trigger to fire more than once',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_log_file',
                'msg': 'JSONL fire-event log (empty = '
                       'script_location/lammps_trigger_log.jsonl)',
                'type': str,
                'default': '',
            },
        ]

    def _configure(self, **kwargs):
        """
        Materialize input.lammps and adios2_config.xml into script_location.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        self.update_config(kwargs, rebuild=False)

        script_dir = self.config['script_location']
        if not script_dir:
            raise Exception('script_location must be set '
                            '(NFS-shared run directory)')
        Mkdir(script_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                       env=self.env)).run()

        # LAMMPS input script (case parameters).
        self.copy_template_file(
            f'{self.pkg_dir}/config/in.lammps',
            f'{script_dir}/input.lammps',
            replacements={
                'RHO': self.config['rho'],
                'BOX': self.config['box'],
                'T0': self.config['t0'],
                'DT': self.config['dt'],
                'STEPS': self.config['steps'],
                'DUMP_EVERY': self.config['dump_every'],
            })

        # ADIOS2 config: the dump reads "adios2_config.xml" from its cwd.
        adios2_xml = f'{script_dir}/adios2_config.xml'
        engine = self.config['engine'].lower()
        if engine == 'bp4':
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                    adios2_xml)
        elif engine == 'hermes':
            replacements = {
                'PPN': self.config['ppn'],
                'DBFILE': self.config['db_path'],
            }
            template = f'{self.pkg_dir}/config/hermes.xml'
            if self.config['trigger']:
                template = f'{self.pkg_dir}/config/hermes_trigger.xml'
                replacements.update(self._trigger_replacements(script_dir))
            self.copy_template_file(template, adios2_xml,
                                    replacements=replacements)
        else:
            raise Exception('Engine not defined')

    def _trigger_replacements(self, script_dir):
        """
        Resolve the trigger config into hermes_trigger.xml replacements.

        :return: dict of template replacements
        """
        if (self.config['trigger_threshold'] <= 0 and
                self.config['trigger_baseline_ratio'] <= 0):
            print('WARNING: trigger enabled but trigger_threshold and '
                  'trigger_baseline_ratio are both 0 - the engine will '
                  'disable the trigger')

        log_file = self.config['trigger_log_file']
        if not log_file:
            log_file = f'{script_dir}/lammps_trigger_log.jsonl'

        sum_var = self.config['trigger_sum_variable']
        sum_var_line = ''
        if sum_var and sum_var.lower() != 'none':
            sum_var_line = (f'<parameter key="TriggerSumVariable" '
                            f'value="{sum_var}"/>')

        print(f'Trigger: {self.config["trigger_type"]}'
              f'({self.config["trigger_variable"]}) sum={sum_var} threshold='
              f'{self.config["trigger_threshold"]} baseline_ratio='
              f'{self.config["trigger_baseline_ratio"]} inspect_steps='
              f'{self.config["trigger_inspect_steps"]} log={log_file}')
        return {
            'TRIGGERTYPE': self.config['trigger_type'],
            'TRIGGERVAR': self.config['trigger_variable'],
            'TRIGGERSUMVARLINE': sum_var_line,
            'TRIGGERTHRESHOLD': self.config['trigger_threshold'],
            'TRIGGERBASELINERATIO': self.config['trigger_baseline_ratio'],
            'TRIGGERINSPECTSTEPS': self.config['trigger_inspect_steps'],
            'TRIGGERREFIRE': self.config['trigger_refire'],
            'TRIGGERLOG': log_file,
        }

    def start(self):
        """
        Launch LAMMPS through mpirun in script_location.

        :return: None
        """
        # Pin OpenMPI to TCP over eno1 (Ares multi-NIC); mirrors gray-scott.
        os.environ['OMPI_MCA_pml'] = 'ob1'
        os.environ['OMPI_MCA_btl'] = 'tcp,self'
        os.environ['OMPI_MCA_osc'] = '^ucx'
        os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
        os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'

        lmp = self.config['lmp_bin']
        Exec(f'{lmp} -in input.lammps',
             MpiExecInfo(nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         hostfile=self.jarvis.hostfile,
                         env=self.mod_env,
                         cwd=self.config['script_location'])).run()

    def stop(self):
        """
        Stop a running application.

        :return: None
        """
        pass

    def clean(self):
        """
        Destroy all data for an application.

        :return: None
        """
        script_dir = self.config['script_location']
        output_file = [self.config['db_path']]
        if script_dir:
            output_file += [f'{script_dir}/lammps.bp',
                            f'{script_dir}/lammps_trigger_log.jsonl']
        Rm(output_file, PsshExecInfo(hostfile=self.jarvis.hostfile)).run()

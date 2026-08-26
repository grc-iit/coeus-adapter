"""
LBM-CFD 2D through the COEUS hermes plugin engine, with an optional VARIANCE
trigger on the in-situ vorticity variance (Vigil trigger-render-reason pipeline,
lattice-Boltzmann instability-onset case).

The writer (test/real_apps/ascent-trame/examples/lbm-cfd, include/adios2_writer.hpp)
streams `vorticity` as a raw field and declares the ADIOS2 derived variables
derive/VarVort = variance(vorticity) and derive/AddVort = add(vorticity). The
engine pools the per-block variance into the exact global variance
(ComputeGlobalVarianceDerived_) at every output step and fires when the vorticity
variance spikes as the D2Q9 scheme goes unstable (stable ~1e-5, unstable ~1e8).
Log-only for now (no Catalyst/SST render block); the SST gate + agent are the
next Phase-2 milestone.
"""
from jarvis_cd.core.pkg import Application
from jarvis_cd.shell import Exec, MpiExecInfo, PsshExecInfo, Mkdir, Rm
import os


class LbmCfd(Application):
    """
    Launch the 2D LBM-CFD application through the hermes engine.
    """
    def _init(self):
        """
        Initialize paths (set in _configure once shared_dir is available).
        """
        self.adios2_xml_path = None

    def _configure_menu(self):
        """
        CLI menu for the configurator.

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
                'default': 4,
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes'],
                'type': str,
                'default': 'hermes',
            },
            {
                'name': 'lbm_bin',
                'msg': 'Path to the ADIOS2-enabled lbmcfd binary',
                'type': str,
                'default': '/mnt/common/hxu40/coeus/iowarp/coeus-adapter/'
                           'test/real_apps/ascent-trame/examples/lbm-cfd/bin/lbmcfd',
            },
            {
                'name': 'script_location',
                'msg': 'Directory (NFS-shared) where adios2_config.xml is '
                       'materialized and lbmcfd runs (cwd)',
                'type': str,
                'default': None,
            },
            {
                'name': 'db_path',
                'msg': 'Path where the metadata DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            {
                'name': 'out_file',
                'msg': 'ADIOS2 Open() name (stream tag under hermes)',
                'type': str,
                'default': 'lbmcfd.bp',
            },
            {
                'name': 'steps',
                'msg': 'Total LBM time steps (ignored in force_unstable mode, '
                       'which caps at 400)',
                'type': int,
                'default': 20000,
            },
            {
                'name': 'force_unstable',
                'msg': 'Run the deterministic unstable case (few outputs; '
                       'vorticity blows up -> variance trigger fires)',
                'type': bool,
                'default': False,
            },
            {
                'name': 'agent_rescue',
                'msg': 'Vigil REASON step: disable the simulation\'s automatic '
                       'self-heal and poll the AI agent\'s verdict flags each '
                       'output instead (<out_file>.rescue reverts to the last '
                       'checkpoint + doubles the timesteps; <out_file>.stop '
                       'halts). The agent inspects the gated SST window and '
                       'decides. Flags land in script_location.',
                'type': bool,
                'default': False,
            },
            {
                'name': 'derived',
                'msg': 'Declare the ADIOS2 derived variables '
                       '(derive/VarVort=variance(vorticity), '
                       'derive/AddVort=add(vorticity)); required for the '
                       'variance trigger. Disables via --no-derived.',
                'type': bool,
                'default': True,
            },
            {
                'name': 'derived_debug',
                'msg': 'Set COEUS_DERIVED_DEBUG to log per-block derived '
                       'shape/count/blob diagnostics from the engine',
                'type': bool,
                'default': False,
            },
            # ----- Trigger (Vigil): VARIANCE on the vorticity -----
            {
                'name': 'trigger',
                'msg': 'Enable the hermes-engine trigger (hermes engine only)',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_type',
                'msg': 'Trigger statistic (variance pools derive/VarVort into '
                       'the exact global variance)',
                'type': str,
                'default': 'variance',
            },
            {
                'name': 'trigger_variable',
                'msg': 'Derived variable the trigger pools variance over',
                'type': str,
                'default': 'derive/VarVort',
            },
            {
                'name': 'trigger_sum_variable',
                'msg': 'Derived block-sum variable for exact pooling '
                       '(derive/AddVort); "none" to omit (raw pass instead)',
                'type': str,
                'default': 'derive/AddVort',
            },
            {
                'name': 'trigger_threshold',
                'msg': 'Absolute variance threshold; fires on first upward '
                       'crossing (0 disables). Stable vorticity variance ~1e-5, '
                       'unstable ~1e8, so 1.0 cleanly separates.',
                'type': float,
                'default': 1.0,
            },
            {
                'name': 'trigger_baseline_ratio',
                'msg': 'Fire when variance exceeds ratio x first-step baseline '
                       '(0 disables; alternative to the absolute threshold)',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_inspect_steps',
                'msg': 'Output steps flagged per fire (firing step + N-1)',
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
                       'script_location/lbm_trigger_log.jsonl)',
                'type': str,
                'default': '',
            },
            # ----- SST-gated render (Catalyst stream) -----
            {
                'name': 'catalyst_stream',
                'msg': 'Name of the trigger-gated Catalyst SST stream the '
                       'reader connects to (empty = log-only, no SST render). '
                       'Contact file <name>.sst lands in script_location.',
                'type': str,
                'default': '',
            },
            {
                'name': 'script',
                'msg': 'Reader pipeline referenced by the engine (Script param; '
                       'not executed by the writer in SST mode). Empty = the '
                       'package lbm-pipeline.py materialized in script_location',
                'type': str,
                'default': '',
            },
            {
                'name': 'data_model',
                'msg': 'Fides data model (DataModel param). Empty = the package '
                       'lbm-fides.json materialized in script_location',
                'type': str,
                'default': '',
            },
            {
                'name': 'sst_data_transport',
                'msg': 'SST DataTransport for the Catalyst stream',
                'type': str,
                'default': 'WAN',
            },
            {
                'name': 'sst_queue_full_policy',
                'msg': 'Override SST QueueFullPolicy (empty = engine default: '
                       'Block when the trigger is enabled)',
                'type': str,
                'default': '',
            },
        ]

    def _configure(self, **kwargs):
        """
        Materialize adios2_config.xml into script_location.

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

        adios2_xml = f'{script_dir}/adios2_config.xml'
        engine = self.config['engine'].lower()
        if engine == 'bp5':
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                    adios2_xml)
        elif engine == 'hermes':
            replacements = {
                'PPN': self.config['ppn'],
                'DBFILE': self.config['db_path'],
            }
            template = f'{self.pkg_dir}/config/hermes.xml'
            if self.config['trigger']:
                replacements.update(self._trigger_replacements(script_dir))
                if self.config['catalyst_stream']:
                    # SST-gated render: ship the flagged window to a reader.
                    template = f'{self.pkg_dir}/config/hermes_sst_trigger.xml'
                    replacements.update(self._sst_replacements(script_dir))
                else:
                    template = f'{self.pkg_dir}/config/hermes_trigger.xml'
            self.copy_template_file(template, adios2_xml,
                                    replacements=replacements)
        else:
            raise Exception('Engine not defined')
        self.adios2_xml_path = adios2_xml

    def _trigger_replacements(self, script_dir):
        """
        Resolve the variance-trigger config into hermes_trigger.xml replacements.

        :return: dict of template replacements
        """
        if (self.config['trigger_threshold'] <= 0 and
                self.config['trigger_baseline_ratio'] <= 0):
            print('WARNING: trigger enabled but trigger_threshold and '
                  'trigger_baseline_ratio are both 0 - the engine will '
                  'disable the trigger')
        if not self.config['derived']:
            print('WARNING: trigger_variable is a derived variable but '
                  'derived=false; the writer will not declare it. Set '
                  'derived=true.')

        log_file = self.config['trigger_log_file']
        if not log_file:
            log_file = f'{script_dir}/lbm_trigger_log.jsonl'

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

    def _sst_replacements(self, script_dir):
        """
        Resolve the SST-gated render config (hermes_sst_trigger.xml).

        Materializes the reader pipeline + Fides data model into script_dir and
        returns the Catalyst/SST template replacements. The writer requires the
        Script + DataModel params to enable the Catalyst path but does NOT
        execute the Script in SST mode (the external reader renders).

        :return: dict of template replacements
        """
        script = self.config['script']
        if not script:
            script = f'{script_dir}/lbm-pipeline.py'
            self.copy_template_file(f'{self.pkg_dir}/config/lbm-pipeline.py',
                                    script)
        data_model = self.config['data_model']
        if not data_model:
            data_model = f'{script_dir}/lbm-fides.json'
            self.copy_template_file(f'{self.pkg_dir}/config/lbm-fides.json',
                                    data_model)

        queue_policy_line = ''
        if self.config['sst_queue_full_policy']:
            queue_policy_line = (
                f'<parameter key="SSTQueueFullPolicy" '
                f'value="{self.config["sst_queue_full_policy"]}"/>')

        print(f'SST-gated render: stream={self.config["catalyst_stream"]} '
              f'transport={self.config["sst_data_transport"]} '
              f'script={script} data_model={data_model}')
        return {
            'CATALYSTSTREAM': self.config['catalyst_stream'],
            'SCRIPT': script,
            'DATAMODEL': data_model,
            'SSTTRANSPORT': self.config['sst_data_transport'],
            'SSTQUEUEPOLICYLINE': queue_policy_line,
        }

    def start(self):
        """
        Launch lbmcfd through mpirun in script_location.

        :return: None
        """
        # Pin OpenMPI to TCP over eno1 (Ares multi-NIC); mirrors gray-scott/lammps.
        os.environ['OMPI_MCA_pml'] = 'ob1'
        os.environ['OMPI_MCA_btl'] = 'tcp,self'
        os.environ['OMPI_MCA_osc'] = '^ucx'
        os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
        os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'

        if self.config.get('derived_debug'):
            self.mod_env['COEUS_DERIVED_DEBUG'] = '1'

        lbm = self.config['lbm_bin']
        xml = f"{self.config['script_location']}/adios2_config.xml"
        flags = [f'--adios2',
                 f'--adios-config {xml}',
                 f'--output-bp {self.config["out_file"]}',
                 f'--steps {self.config["steps"]}']
        if not self.config['derived']:
            flags.append('--no-derived')
        if self.config['force_unstable']:
            flags.append('--force-unstable')
        if self.config['agent_rescue']:
            # Reason step: the sim polls <out_file>.rescue / .stop each output
            # instead of self-healing, so the AI agent owns the verdict.
            flags.append('--agent-rescue')
        cmd = f'{lbm} ' + ' '.join(flags)
        Exec(cmd,
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
            output_file += [f'{script_dir}/lbm_trigger_log.jsonl',
                            f'{script_dir}/adios2_config.xml']
            if self.config['catalyst_stream']:
                # Stale SST contact files break the next rendezvous.
                output_file += [
                    f'{script_dir}/{self.config["catalyst_stream"]}',
                    f'{script_dir}/{self.config["catalyst_stream"]}.sst']
        Rm(output_file, PsshExecInfo(hostfile=self.jarvis.hostfile)).run()

"""
This module provides classes and methods to launch the Gray Scott application.
Gray Scott is a 3D 7-point stencil code for modeling the diffusion of two
substances.
"""
from jarvis_cd.core.pkg import Application
from jarvis_cd.shell import Exec, MpiExecInfo, PsshExecInfo, Mkdir, Rm, PscpExec, PscpExecInfo
from jarvis_cd.shell import LocalExecInfo
import json
import os
import shutil

class Adios2GrayScott(Application):
    """
    This class provides methods to launch the GrayScott application.
    """
    def _init(self):
        """
        Initialize paths (will be set in _configure when directories are available)
        """
        self.adios2_xml_path = None
        self.settings_json_path = None
        self.var_json_path = None
        self.operator_json_path = None

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
                'name': 'L',
                'msg': 'Grid size of cube',
                'type': int,
                'default': 32,
            },
            {
                'name': 'Du',
                'msg': 'Diffusion rate of substance U',
                'type': float,
                'default': .2,
            },
            {
                'name': 'Dv',
                'msg': 'Diffusion rate of substance V',
                'type': float,
                'default': .1,
            },
            {
                'name': 'F',
                'msg': 'Feed rate of U',
                'type': float,
                'default': .01,
            },
            {
                'name': 'k',
                'msg': 'Kill rate of V',
                'type': float,
                'default': .05,
            },
            {
                'name': 'dt',
                'msg': 'Timestep',
                'type': float,
                'default': 2.0,
            },
            {
                'name': 'steps',
                'msg': 'Total number of steps to simulate',
                'type': int,
                'default': 100,
            },
            {
                'name': 'plotgap',
                'msg': 'Number of steps between output',
                'type': float,
                'default': 10,
            },
            {
                'name': 'noise',
                'msg': 'Amount of noise',
                'type': float,
                'default': .01,
            },
            {
                'name': 'out_file',
                'msg': 'Absolute path to output file (optional, defaults to shared_dir/gray-scott-output/data/out.bp)',
                'type': str,
                'default': '',
            },
            {
                'name': 'checkpoint',
                'msg': 'Perform checkpoints',
                'type': bool,
                'default': True,
            },
            {
                'name': 'checkpoint_freq',
                'msg': 'Frequency of the checkpoints',
                'type': int,
                'default': 70,
            },
            {
                'name': 'checkpoint_output',
                'msg': 'Output location of the checkpoint',
                'type': str,
                'default': 'ckpt.bp',
            },
            {
                'name': 'restart',
                'msg': 'Perform restarts',
                'type': bool,
                'default': False,
            },
            {
                'name': 'restart_input',
                'msg': 'Input for the restart',
                'type': str,
                'default': 'ckpt.bp',
            },
            {
                'name': 'adios_span',
                'msg': '???',
                'type': bool,
                'default': False,
            },
            {
                'name': 'adios_memory_selection',
                'msg': '???',
                'type': bool,
                'default': False,
            },
            {
                'name': 'mesh_type',
                'msg': '???',
                'type': str,
                'default': 'image',
            },
            {
                'name': 'engine',
                'msg': 'Engine to be used',
                'choices': ['bp5', 'hermes', 'bp5_derived', 'hermes_derived'],
                'type': str,
                'default': 'bp5',
            },
            {
                'name': 'full_run',
                'msg': 'Whill postprocessing be executed?',
                'type': bool,
                'default': True,
            },
            {
                'name': 'limit',
                'msg': 'Limit the value of data to track',
                'type': int,
                'default': 0,
            },
            {
                'name': 'db_path',
                'msg': 'Path where the DB will be stored',
                'type': str,
                'default': 'benchmark_metadata.db',
            },
            {
                'name': 'Execution_order',
                'msg': 'Path where the bp5 will be stored',
                'type': str,
                'default': '1',
            },
            {
                'name': 'net_if',
                'msg': 'Network interface OpenMPI pins TCP btl/oob traffic to '
                       '(OMPI_MCA_*_tcp_if_include). Ares=eno1; Delta=hsn0 '
                       '(Slingshot) or eth1',
                'type': str,
                'default': 'eno1',
            },
            {
                'name': 'launcher',
                'msg': 'MPI launcher. "mpirun" (default) or "srun" — on Delta '
                       'ssh between nodes is denied and mpirun cannot span the '
                       'allocation, so multi-node must use srun+PMIx.',
                'choices': ['mpirun', 'srun'],
                'type': str,
                'default': 'mpirun',
            },
            {
                'name': 'srun_nodelist',
                'msg': 'srun --nodelist (comma-sep) restricting the producer '
                       'ranks to specific nodes (e.g. cn024,cn046 so consumer '
                       'nodes stay free). Empty = whole allocation.',
                'type': str,
                'default': '',
            },
            {
                'name': 'srun_mpi',
                'msg': 'srun --mpi type (pmix required for this OpenMPI; pmi2 '
                       'yields singletons)',
                'type': str,
                'default': 'pmix',
            },
            {
                'name': 'trigger',
                'msg': 'Enable the statistical variance trigger gating the '
                       'Catalyst SST stream (hermes engines only)',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_variable',
                'msg': 'Variable the trigger pools variance over. Empty = '
                       'auto: derive/VarV for hermes_derived, raw V for hermes',
                'type': str,
                'default': '',
            },
            {
                'name': 'trigger_sum_variable',
                'msg': 'Derived block-sum variable for exact pooling. Empty = '
                       'auto (derive/AddV when trigger_variable is derived); '
                       '"none" = omit (one raw pass per step instead)',
                'type': str,
                'default': '',
            },
            {
                'name': 'trigger_threshold',
                'msg': 'Absolute variance threshold; fires on first upward '
                       'crossing (0 disables)',
                'type': float,
                'default': 0.05,
            },
            {
                'name': 'trigger_baseline_ratio',
                'msg': 'Fire when variance exceeds ratio x first-step '
                       'baseline (0 disables; alternative to the absolute '
                       'threshold)',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_inspect_steps',
                'msg': 'Number of output steps shipped over SST per fire '
                       '(firing step + N-1 following)',
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
                'msg': 'JSON-lines file rank 0 appends fire events to '
                       '(empty = shared_dir/trigger_log.jsonl)',
                'type': str,
                'default': '',
            },
            {
                'name': 'trigger_warn_on_collapse',
                'msg': 'Collapse-WARNING mode: the trigger arms on the rise and '
                       'WARNS (streams the inspect window to the AI agent) when '
                       'the statistic collapses back down (pattern expanding to '
                       'blank). The engine only warns; the agent fires the '
                       'verdict. Suppresses the normal rising-edge fire.',
                'type': bool,
                'default': False,
            },
            {
                'name': 'trigger_collapse_baseline_ratio',
                'msg': 'Collapse-warning level: after arming (>= '
                       'trigger_baseline_ratio x baseline), warn on the first '
                       'step where the statistic falls to <= ratio x baseline '
                       '(0 = off). Gray-Scott F=0.08/k=0.03: arm 20, collapse '
                       '13 -> warns at output ~23.',
                'type': float,
                'default': 0,
            },
            {
                'name': 'trigger_collapse_threshold',
                'msg': 'Collapse-warning absolute level: warn once the '
                       'statistic <= this value (0 = off; alt to the ratio)',
                'type': float,
                'default': 0,
            },
            {
                'name': 'catalyst_stream',
                'msg': 'Name of the Catalyst SST stream the reader connects to',
                'type': str,
                'default': 'gs.bp',
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

    # jarvis pkg config adios2_gray_scott ppn=20 full_run=true engine=hermes db_path=/mnt/nvme/jcernudagarcia/metadata.db out_file=gs.bp nprocs=1

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        # Ensure directories are set up before using them
        self._ensure_directories()
        
        # Initialize paths now that shared_dir is available
        if self.adios2_xml_path is None:
            self.adios2_xml_path = f'{self.shared_dir}/adios2.xml'
            self.settings_json_path = f'{self.shared_dir}/settings-files.json'
            self.var_json_path = f'{self.shared_dir}/var.json'
            self.operator_json_path = f'{self.shared_dir}/operator.json'
        
        self.update_config(kwargs, rebuild=False)
        if not self.config.get('out_file') or self.config['out_file'] == '':
            adios_dir = os.path.join(self.shared_dir, 'gray-scott-output')
            self.config['out_file'] = os.path.join(adios_dir,
                                                 'data/out.bp')
            Mkdir(adios_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                          env=self.env)).run()
        settings_json = {
            'L': self.config['L'],
            'Du': self.config['Du'],
            'Dv': self.config['Dv'],
            'F': self.config['F'],
            'k': self.config['k'],
            'dt': self.config['dt'],
            'plotgap': self.config['plotgap'],
            'steps': self.config['steps'],
            'noise': self.config['noise'],
            'output': self.config['out_file'],
            'checkpoint': self.config['checkpoint'],
            'checkpoint_freq': self.config['checkpoint_freq'],
            'checkpoint_output': self.config['checkpoint_output'],
            'restart': self.config['restart'],
            'restart_input': self.config['restart_input'],
            'adios_span': self.config['adios_span'],
            'adios_memory_selection': self.config['adios_memory_selection'],
            'mesh_type': self.config['mesh_type'],
            'adios_config': f'{self.adios2_xml_path}'
        }
        output_dir = os.path.dirname(self.config['out_file'])
        db_dir = os.path.dirname(self.config['db_path'])
        Mkdir([output_dir, db_dir], PsshExecInfo(hostfile=self.jarvis.hostfile,
                                       env=self.env)).run()

        # Ensure shared_dir exists before writing JSON file
        Mkdir(self.shared_dir, PsshExecInfo(hostfile=self.jarvis.hostfile,
                                           env=self.env)).run()

        # Save settings JSON file
        try:
            with open(self.settings_json_path, 'w') as f:
                json.dump(settings_json, f, indent=2)
                f.flush()  # Ensure data is written to disk
                os.fsync(f.fileno())  # Force write to disk
        except Exception as e:
            raise IOError(f"Failed to write settings JSON file to {self.settings_json_path}: {e}")
        
        # Verify the file was created and has content
        if not os.path.exists(self.settings_json_path):
            raise FileNotFoundError(f"Failed to create settings JSON file: {self.settings_json_path}")
        
        # Check file size to ensure it's not empty
        file_size = os.path.getsize(self.settings_json_path)
        if file_size == 0:
            raise ValueError(f"Settings JSON file is empty: {self.settings_json_path}")
        
        # Verify JSON is valid by trying to read it back
        try:
            with open(self.settings_json_path, 'r') as f:
                json.load(f)  # Validate JSON syntax
        except json.JSONDecodeError as e:
            raise ValueError(f"Settings JSON file contains invalid JSON: {self.settings_json_path}. Error: {e}")
        
        print(f"Created settings JSON file: {self.settings_json_path} ({file_size} bytes)")
        print(f"Using engine {self.config['engine']}")
        if self.config['engine'].lower() in ['bp5', 'bp5_derived']:
            self.copy_template_file(f'{self.pkg_dir}/config/adios2.xml',
                                self.adios2_xml_path)
        elif self.config['engine'].lower() in ['hermes', 'hermes_derived']:
            replacements = {
                'PPN': self.config['ppn'],
                'VARFILE': self.var_json_path,
                'OPFILE': self.operator_json_path,
                'DBFILE': self.config['db_path'],
                'Order': self.config['Execution_order'],
                'DATAMODEL': f'{self.shared_dir}/gs-fides.json',
                'SCRIPT': f'{self.shared_dir}/gs-catalyst.py',
            }
            template = f'{self.pkg_dir}/config/hermes.xml'
            if self.config['trigger']:
                template = f'{self.pkg_dir}/config/hermes_trigger.xml'
                replacements.update(self._trigger_replacements())
            self.copy_template_file(template,
                                    self.adios2_xml_path,
                                    replacements=replacements)
            self.copy_template_file(f'{self.pkg_dir}/config/var.yaml',
                                    self.var_json_path)
            self.copy_template_file(f'{self.pkg_dir}/config/operator.yaml',
                                    self.operator_json_path)
        else:
            raise Exception('Engine not defined')

        # Copy Catalyst/Fides config files to shared directory
        self.copy_template_file(f'{self.pkg_dir}/config/gs-catalyst.py',
                                f'{self.shared_dir}/gs-catalyst.py')
        self.copy_template_file(f'{self.pkg_dir}/config/gs-fides.json',
                                f'{self.shared_dir}/gs-fides.json')

    def _trigger_replacements(self):
        """
        Resolve the variance-trigger config into template replacements for
        config/hermes_trigger.xml (Vigil trigger-gated Catalyst SST stream).

        :return: dict of template replacements
        """
        derived_engine = self.config['engine'].lower() == 'hermes_derived'
        trigger_var = self.config['trigger_variable']
        if not trigger_var:
            trigger_var = 'derive/VarV' if derived_engine else 'V'
        if trigger_var.startswith('derive/') and not derived_engine:
            print(f'WARNING: trigger_variable={trigger_var} is a derived '
                  f'variable but engine={self.config["engine"]} does not '
                  f'declare derived variables; use engine=hermes_derived')

        sum_var = self.config['trigger_sum_variable']
        if not sum_var:
            sum_var = ('derive/AddV'
                       if trigger_var.startswith('derive/') else 'none')
        sum_var_line = ''
        if sum_var.lower() != 'none':
            sum_var_line = (f'<parameter key="TriggerSumVariable" '
                            f'value="{sum_var}"/>')

        if (self.config['trigger_threshold'] <= 0 and
                self.config['trigger_baseline_ratio'] <= 0):
            print('WARNING: trigger enabled but trigger_threshold and '
                  'trigger_baseline_ratio are both 0 - the engine will '
                  'disable the trigger and ship no SST steps')

        queue_policy_line = ''
        if self.config['sst_queue_full_policy']:
            queue_policy_line = (f'<parameter key="SSTQueueFullPolicy" '
                                 f'value="{self.config["sst_queue_full_policy"]}"/>')

        log_file = self.config['trigger_log_file']
        if not log_file:
            log_file = f'{self.shared_dir}/trigger_log.jsonl'

        # Collapse-WARNING mode ("expanding to blank"). Emitted only when
        # enabled; the engine warns (streams the inspect window to the agent)
        # on the collapse. The agent issues the fire verdict.
        stop_lines = ''
        if self.config['trigger_warn_on_collapse']:
            stop_lines = (
                '<parameter key="TriggerWarnOnCollapse" value="true"/>\n'
                '            <parameter key="TriggerCollapseBaselineRatio" value="'
                f'{self.config["trigger_collapse_baseline_ratio"]}"/>\n'
                '            <parameter key="TriggerCollapseThreshold" value="'
                f'{self.config["trigger_collapse_threshold"]}"/>')
            print(f'Trigger WARN-on-collapse: arm='
                  f'{self.config["trigger_baseline_ratio"]}x '
                  f'collapse_baseline_ratio='
                  f'{self.config["trigger_collapse_baseline_ratio"]} '
                  f'collapse_threshold={self.config["trigger_collapse_threshold"]}')

        print(f'Trigger: variance({trigger_var}) threshold='
              f'{self.config["trigger_threshold"]} baseline_ratio='
              f'{self.config["trigger_baseline_ratio"]} inspect_steps='
              f'{self.config["trigger_inspect_steps"]} log={log_file}')
        return {
            'CATALYSTSTREAM': self.config['catalyst_stream'],
            'SSTTRANSPORT': self.config['sst_data_transport'],
            'SSTQUEUEPOLICYLINE': queue_policy_line,
            'TRIGGERVAR': trigger_var,
            'TRIGGERSUMVARLINE': sum_var_line,
            'TRIGGERTHRESHOLD': self.config['trigger_threshold'],
            'TRIGGERBASELINERATIO': self.config['trigger_baseline_ratio'],
            'TRIGGERINSPECTSTEPS': self.config['trigger_inspect_steps'],
            'TRIGGERREFIRE': self.config['trigger_refire'],
            'TRIGGERLOG': log_file,
            'TRIGGERWARNLINES': stop_lines,
        }

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        os.environ['OMPI_MCA_pml'] = 'ob1'
        os.environ['OMPI_MCA_btl'] = 'tcp,self'
        os.environ['OMPI_MCA_osc'] = '^ucx'
        # Network interface is configurable via net_if (Ares=eno1, Delta=hsn0/eth1).
        # A wrong/absent interface makes OpenMPI's tcp btl fail to find a NIC even
        # for a single-node localhost run, so keep this matched to the machine.
        net_if = self.config.get('net_if', 'eno1')
        os.environ['OMPI_MCA_btl_tcp_if_include'] = net_if
        os.environ['OMPI_MCA_oob_tcp_if_include'] = net_if
        # Multi-node srun+PMIx (Delta): the Slurm PMIx v5 GDS shmem segment
        # fails to open across the step (PMIX_ERR_FILE_OPEN_FAILURE) unless we
        # force the hash GDS; PMIx also needs a valid TMPDIR. env build does not
        # capture these, so set them here (harmless for the mpirun path).
        if self.config.get('launcher', 'mpirun') == 'srun':
            os.environ['PMIX_MCA_gds'] = 'hash'
            os.environ.setdefault('TMPDIR', os.path.expanduser('~/prte_tmp'))
            os.makedirs(os.environ['TMPDIR'], exist_ok=True)
        # Ensure paths are initialized
        if self.settings_json_path is None:
            self._ensure_directories()
            self.adios2_xml_path = f'{self.shared_dir}/adios2.xml'
            self.settings_json_path = f'{self.shared_dir}/settings-files.json'
        
        # Verify JSON file exists before running
        if not os.path.exists(self.settings_json_path):
            raise FileNotFoundError(
                f"Settings JSON file not found: {self.settings_json_path}. "
                f"Please run 'jarvis ppl env build' or configure the package first."
            )
        
        # Verify JSON file is not empty
        if os.path.getsize(self.settings_json_path) == 0:
            raise ValueError(
                f"Settings JSON file is empty: {self.settings_json_path}. "
                f"Please reconfigure the package."
            )
        
        # print(self.env['HERMES_CLIENT_CONF'])
        derived = 1 if self.config['engine'].lower() in [
            'bp5_derived', 'hermes_derived'] else 0
        app_cmd = f'adios2-gray-scott {self.settings_json_path} {derived}'

        if self.config.get('launcher', 'mpirun') == 'srun':
            # Multi-node on Delta: mpirun cannot span the allocation and ssh is
            # denied, so launch the ranks with srun+PMIx. env.sh sets
            # PMIX_MCA_gds=hash (the Slurm PMIx v5 shmem GDS fails to open) and
            # TMPDIR; those flow through self.mod_env. Runs locally on this node;
            # srun distributes the ranks across --nodelist.
            nprocs = int(self.config['nprocs'])
            ppn = int(self.config['ppn'])
            nnodes = max(1, -(-nprocs // ppn))  # ceil
            jobid = os.environ.get('SLURM_JOB_ID', '')
            parts = ['srun']
            if jobid:
                parts.append(f'--jobid={jobid}')
            parts += [f'--mpi={self.config.get("srun_mpi", "pmix")}',
                      f'-N{nnodes}', f'--ntasks-per-node={ppn}', f'-n{nprocs}',
                      '--overlap']
            if self.config.get('srun_nodelist'):
                parts.append(f'--nodelist={self.config["srun_nodelist"]}')
            parts.append(app_cmd)
            srun_cmd = ' '.join(parts)
            print(f'[gray-scott] srun launch: {srun_cmd}')
            Exec(srun_cmd, LocalExecInfo(env=self.mod_env,
                                         cwd=os.getcwd())).run()
        else:
            Exec(app_cmd,
                 MpiExecInfo(nprocs=self.config['nprocs'],
                             ppn=self.config['ppn'],
                             hostfile=self.jarvis.hostfile,
                             env=self.mod_env,
                             do_dbg=self.config.get('do_dbg', False),
                             dbg_port=self.config.get('dbg_port', None)
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
        output_file = [self.config['out_file'],
                       self.config['checkpoint_output'],
                       self.config['db_path']
                       ]

        print(f'Removing {output_file}')
        Rm(output_file, PsshExecInfo(hostfile=self.jarvis.hostfile)).run()

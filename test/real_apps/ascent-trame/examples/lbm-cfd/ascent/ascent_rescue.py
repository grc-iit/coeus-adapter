import sys
sys.path.append(f'../../.venv/lib/python{sys.version_info.major}.{sys.version_info.minor}/site-packages')
import os
import subprocess
from mpi4py import MPI
import conduit
import ascent.mpi

def main():
    comm = MPI.Comm.f2py(ascent_mpi_comm_id())
    task_id = comm.Get_rank()

    if task_id == 0:
        mesh_data = ascent_data().child(0)
        cycle = mesh_data["state/cycle"]
        timesteps = mesh_data["state/timesteps"]
        checkpoint_step = mesh_data["state/checkpoint_step"]

        print(f"[RESCUE] Instability detected at step {cycle}", flush=True)
        print(f"[RESCUE] Current timesteps: {timesteps}, checkpoint at step: {checkpoint_step}", flush=True)

        # send email notification (using system mail if available)
        user = os.environ.get("USER", "user")
        hostname = os.environ.get("HOSTNAME", "unknown")
        subject = f"LBM-CFD Simulation Unstable on {hostname}"
        body = (
            f"Simulation instability detected.\n"
            f"  Step: {cycle}\n"
            f"  Timesteps: {timesteps}\n"
            f"  Checkpoint: step {checkpoint_step}\n"
            f"  Host: {hostname}\n\n"
            f"The simulation will attempt automatic rescue by reverting to the\n"
            f"checkpoint and doubling the number of timesteps.\n\n"
            f"To manually adjust, use the setTimeSteps callback:\n"
            f"  params = conduit.Node()\n"
            f"  params['timesteps'] = {timesteps * 2}\n"
            f"  output = conduit.Node()\n"
            f"  execute_callback('setTimeSteps', params, output)\n"
        )
        try:
            proc = subprocess.Popen(
                ["mail", "-s", subject, user],
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            proc.communicate(input=body.encode(), timeout=5)
            print(f"[RESCUE] Email notification sent to {user}", flush=True)
        except Exception as e:
            print(f"[RESCUE] Email notification failed: {e}", flush=True)
            print(f"[RESCUE] Email body:\n{body}", flush=True)

        # demonstrate the setTimeSteps callback (as shown in Listing 2 of the paper)
        params = conduit.Node()
        params["timesteps"] = timesteps * 2
        output = conduit.Node()
        ascent.mpi.execute_callback("setTimeSteps", params, output)
        print(f"[RESCUE] setTimeSteps callback executed: new timesteps = {timesteps * 2}", flush=True)

        # check stability via callback
        check_params = conduit.Node()
        check_output = conduit.Node()
        ascent.mpi.execute_callback("checkStability", check_params, check_output)
        print(f"[RESCUE] checkStability callback: stable={check_output['stable']}", flush=True)

main()

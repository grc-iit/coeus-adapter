import sys
sys.path.append(f'../../.venv/lib/python{sys.version_info.major}.{sys.version_info.minor}/site-packages')
import time
import numpy as np
from multiprocessing.managers import BaseManager
from mpi4py import MPI
import conduit
import ascent.mpi

class QueueManager(BaseManager):
    pass

def main():
    # obtain a mpi4py mpi comm object
    comm = MPI.Comm.f2py(ascent_mpi_comm_id())

    # get task id and number of total tasks
    task_id = comm.Get_rank()
    num_tasks = comm.Get_size()

    # run Trame tasks
    interactive = np.array([False], bool)
    update_data = None
    if task_id == 0:
        update_data = executeMainTask(task_id, num_tasks, comm)
    else:
        executeDependentTask(task_id, num_tasks, comm)
    
    # broadcast updates to all ranks
    update_data = comm.bcast(update_data, root=0)

    #  pass updates to Ascent callback
    update_node = conduit.Node()
    update_node['task_id'] = task_id
    if 'flow_speed' in update_data:
        update_node['flow_speed'] = update_data['flow_speed']
    if 'barriers' in update_data:
        num_barriers = update_data['barriers'].shape[0]
        update_node['num_barriers'] = num_barriers
        update_node['barriers'].set_external(update_data['barriers'].reshape(num_barriers * 4))
    output = conduit.Node()
    ascent.mpi.execute_callback('steeringCallback', update_node, output)


def executeMainTask(task_id, num_tasks, comm):
    interactive = np.array([False], bool)
    update_data = {}    

    # attempt to connect to Trame queue manager
    QueueManager.register('get_data_queue')
    QueueManager.register('get_signal_queue')
    mgr = QueueManager(address=('127.0.0.1', 8000), authkey=b'ascent-trame')
    try:
        mgr.connect()
        interactive[0] = True
    except:
        mgr = None

    # broadcast to all processes whether Trame is currently running
    comm.Bcast((interactive, 1, MPI.BOOL), root=0)

    if interactive[0]:
        # get access to Trame's queues
        queue_data = mgr.get_data_queue()
        queue_signal = mgr.get_signal_queue()

        # get published blueprint data
        mesh_data = ascent_data().child(0)

        # repartition data -> gather on main process (0)
        result = repartitionMeshData(task_id, num_tasks, comm)

        num_barriers = mesh_data["state/num_barriers"]
        barriers = mesh_data["state/barriers"].reshape((num_barriers, 4))
        topology_name = result['fields/vorticity/topology']
        coordset_name = result[f'topologies/{topology_name}/coordset']
        dim_x = result[f'coordsets/{coordset_name}/dims/i'] - 1 # 1 fewer element than vertex
        dim_y = result[f'coordsets/{coordset_name}/dims/j'] - 1 # 1 fewer element than vertex
        vorticity = result['fields/vorticity/values'].reshape((dim_y, dim_x))

        # send simulation data to Trame
        queue_data.put({'barriers': barriers, 'vorticity': vorticity})
        
        # get steering updates from Trame
        update_data = queue_signal.get()

    return update_data


def executeDependentTask(task_id, num_tasks, comm):
    interactive = np.array([False], bool)

    # receive whether session is interactive or not from main task
    comm.Bcast((interactive, 1, MPI.BOOL), root=0)

    if interactive[0]:
        # repartition data -> gather on main process (0)
        repartitionMeshData(task_id, num_tasks, comm)


def repartitionMeshData(task_id, num_tasks, comm):
    # get published blueprint data
    mesh_data = ascent_data().child(0)

    # Conduit Blueprint MPI not exposed in Python -> callback to C++ app for repartition instead
    output = conduit.Node()
    ascent.mpi.execute_callback('repartitionCallback', mesh_data, output)

    # once Conduit Blueprint MPI is available, use the following instead:
    """
    layout = np.array([mesh_data["state/coords/start/x"],
                       mesh_data["state/coords/start/y"],
                       mesh_data["state/coords/size/x"],
                       mesh_data["state/coords/size/y"]], dtype=np.uint32)

    layout_all = np.empty(4 * num_tasks, dtype=np.uint32)
    comm.Allgather((layout, 4, MPI.UNSIGNED_INT), (layout_all, 4, MPI.UNSIGNED_INT))

    options = conduit.Node()
    selections = conduit.Node()
    output = conduit.blueprint.mpi.mesh.partition(mesh_data, options, comm)
    """

    return output


main()


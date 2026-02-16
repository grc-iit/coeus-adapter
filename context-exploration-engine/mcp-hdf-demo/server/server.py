import h5py
from pyhdf.SD import SD, SDC
from mcp.server.fastmcp import FastMCP
import os

# Paths to demo HDF5 and HDF4 files
HDF5_PATH = "demo_data.h5"
HDF4_PATH = "demo_data.hdf"

mcp = FastMCP("HDF5/HDF4 Demo Server")

def create_demo_hdf5(path):
    if not os.path.exists(path):
        with h5py.File(path, "w") as f:
            grp = f.create_group("group1")
            grp.create_dataset("dataset1", data=[1, 2, 3, 4])
            f.create_dataset("root_dataset", data=[10, 20, 30])

def create_demo_hdf4(path):
    if not os.path.exists(path):
        try:
            from pyhdf.SD import SD, SDC
            sd = SD(path, SDC.WRITE | SDC.CREATE)
            sds = sd.create('dataset1', SDC.FLOAT32, (4,))
            sds[:] = [1.1, 2.2, 3.3, 4.4]
            sds.endaccess()
            sd.end()
        except Exception as e:
            print(f"Unable to create demo HDF4 file: {e}")

def detect_format(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in [".h5", ".hdf5"]:
        return "hdf5"
    elif ext in [".hdf", ".hdf4"]:
        return "hdf4"
    else:
        return None


@mcp.resource("hdf://hdf5/group1/dataset1")
def get_hdf_resource() -> str:
    with h5py.File(HDF5_PATH, "r") as f:
        obj = f['group1/dataset1']
        if isinstance(obj, h5py.Dataset):
            return str(obj[()])
        elif isinstance(obj, h5py.Group):
            return f"Group contains: {list(obj.keys())}"
        else:
            return "Unknown object type."

@mcp.resource("hdf://hdf4/dataset1")
def get_hdf4_resource() -> str:
    try:
        sd = SD(HDF4_PATH, SDC.READ)
        sds = sd.select('dataset1')
        data = sds[:]
        sds.endaccess()
        sd.end()
        return str(data)
    except Exception as e:
        return f"Error reading HDF4: {e}"

@mcp.tool()
def list_hdf_paths() -> str:
    """List all datasets/groups in both HDF5 and HDF4 demo files."""
    paths = ["HDF5:"]
    def visitor(name, obj):
        paths.append(f"  {name}")
    with h5py.File(HDF5_PATH, "r") as f:
        f.visititems(visitor)
    paths.append("HDF4:")
    try:
        sd = SD(HDF4_PATH, SDC.READ)
        for name in sd.datasets().keys():
            paths.append(f"  {name}")
        sd.end()
    except Exception as e:
        paths.append(f"  (Error reading HDF4: {e})")
    return "\n".join(paths)

if __name__ == "__main__":
#    create_demo_hdf5(HDF5_PATH)
#    create_demo_hdf4(HDF4_PATH)
    mcp.run()

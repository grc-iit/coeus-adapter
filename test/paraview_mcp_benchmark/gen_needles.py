#!/usr/bin/env python3
"""
Synthetic-data generator for the ParaView MCP benchmark.

For each "needle" type we emit:
  - case_NNN_<type>.vti          -- the volume the agent will see
  - case_NNN_<type>.truth.json   -- ground truth (NEVER show to the agent)

A single manifest.json indexes all cases and stores the natural-language
task each case should be presented with.

Run:
    python gen_needles.py --out cases --per-type 5
"""
import argparse
import json
from pathlib import Path

import numpy as np
import vtk
from vtk.util import numpy_support


# ---------------------------------------------------------------------------
# Needle generators
# ---------------------------------------------------------------------------

def gaussian_hotspot(shape, seed):
    """Flat scalar field with one localized Gaussian bump."""
    rng = np.random.default_rng(seed)
    margin = 8
    cx, cy, cz = (int(v) for v in rng.integers(margin, np.array(shape) - margin, size=3))
    sigma = float(rng.uniform(2.5, 4.0))
    amp = float(rng.uniform(4.0, 8.0))

    field = np.full(shape, 1.0, dtype=np.float32)
    xs, ys, zs = np.indices(shape)
    field += amp * np.exp(
        -((xs - cx) ** 2 + (ys - cy) ** 2 + (zs - cz) ** 2) / (2.0 * sigma ** 2)
    )
    truth = {
        "needle": "hotspot",
        "center_voxel": [cx, cy, cz],
        "sigma": sigma,
        "amplitude": amp,
        "background": 1.0,
        "peak_value": 1.0 + amp,
    }
    return field.astype(np.float32), truth, "Temperature"


def hidden_cavity(shape, seed):
    """Solid sphere with a smaller, off-center cavity inside."""
    rng = np.random.default_rng(seed)
    cx, cy, cz = (int(v) for v in (np.array(shape) // 2))
    outer_r = int(min(shape) // 3)
    inner_r = int(rng.integers(3, max(4, outer_r // 2)))
    max_off = max(1, outer_r // 3)
    ox, oy, oz = (int(v) for v in rng.integers(-max_off, max_off + 1, size=3))

    xs, ys, zs = np.indices(shape)
    d_outer = np.sqrt((xs - cx) ** 2 + (ys - cy) ** 2 + (zs - cz) ** 2)
    d_inner = np.sqrt(
        (xs - cx - ox) ** 2 + (ys - cy - oy) ** 2 + (zs - cz - oz) ** 2
    )

    field = np.zeros(shape, dtype=np.float32)
    field[d_outer <= outer_r] = 1.0
    field[d_inner <= inner_r] = 0.0
    truth = {
        "needle": "cavity",
        "outer_center_voxel": [cx, cy, cz],
        "outer_radius_voxels": outer_r,
        "inner_center_voxel": [cx + ox, cy + oy, cz + oz],
        "inner_radius_voxels": inner_r,
    }
    return field, truth, "Density"


def disconnected_blobs(shape, seed):
    """Several Gaussian blobs at random locations -- agent must count them."""
    rng = np.random.default_rng(seed)
    n_blobs = int(rng.integers(2, 6))
    field = np.zeros(shape, dtype=np.float32)
    xs, ys, zs = np.indices(shape)
    centers = []
    margin = 8
    for _ in range(n_blobs):
        c = rng.integers(margin, np.array(shape) - margin, size=3)
        sigma = float(rng.uniform(2.0, 3.0))
        field += 5.0 * np.exp(
            -((xs - c[0]) ** 2 + (ys - c[1]) ** 2 + (zs - c[2]) ** 2)
            / (2.0 * sigma ** 2)
        )
        centers.append([int(c[0]), int(c[1]), int(c[2])])
    truth = {
        "needle": "disconnected_blobs",
        "n_blobs": n_blobs,
        "centers_voxel": centers,
        "suggested_isovalue": 1.5,
    }
    return field.astype(np.float32), truth, "Intensity"


GENERATORS = {
    "hotspot": gaussian_hotspot,
    "cavity": hidden_cavity,
    "blobs": disconnected_blobs,
}


# ---------------------------------------------------------------------------
# Task templates -- what we actually send to the agent
# ---------------------------------------------------------------------------

TASK_TEMPLATES = {
    "hotspot": (
        "Load the dataset at {abs_path}. It contains a scalar field with a single "
        "localized 'hot spot' (a Gaussian bump) somewhere inside an otherwise flat "
        "background. Use the ParaView MCP tools to find the approximate (x, y, z) "
        "voxel coordinates of the peak. Report your final answer as three integers."
    ),
    "cavity": (
        "Load the dataset at {abs_path}. It is a solid spherical object that "
        "contains a hidden internal cavity (a smaller spherical region of zero "
        "value). Use the ParaView MCP tools to determine (a) the approximate "
        "voxel-space center of the cavity and (b) its approximate radius. "
        "Report both."
    ),
    "blobs": (
        "Load the dataset at {abs_path}. It contains several disconnected blob-like "
        "objects. Use the ParaView MCP tools to determine how many distinct objects "
        "there are and report a rough voxel-space center for each one."
    ),
}


# ---------------------------------------------------------------------------
# VTI writer
# ---------------------------------------------------------------------------

def write_vti(field: np.ndarray, path: str, array_name: str):
    """Write a 3-D numpy array as a VTK XML ImageData (.vti) file."""
    img = vtk.vtkImageData()
    img.SetDimensions(*field.shape)
    img.SetSpacing(1.0, 1.0, 1.0)
    img.SetOrigin(0.0, 0.0, 0.0)

    # vtkImageData expects Fortran-order linear storage
    flat = np.asfortranarray(field).ravel(order="F")
    arr = numpy_support.numpy_to_vtk(flat, deep=True)
    arr.SetName(array_name)
    img.GetPointData().SetScalars(arr)

    writer = vtk.vtkXMLImageDataWriter()
    writer.SetFileName(path)
    writer.SetInputData(img)
    writer.Write()


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate ParaView MCP benchmark cases")
    parser.add_argument("--out", type=Path, default=Path(__file__).parent / "cases",
                        help="Output directory for cases")
    parser.add_argument("--per-type", type=int, default=5,
                        help="Number of cases to generate per needle type")
    parser.add_argument("--shape", type=int, nargs=3, default=(64, 64, 64),
                        metavar=("NX", "NY", "NZ"))
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    out_dir = args.out.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(args.seed)
    manifest = []
    case_id = 0

    for needle_name, gen in GENERATORS.items():
        for _ in range(args.per_type):
            seed = int(rng.integers(0, 2**31 - 1))
            field, truth, array_name = gen(tuple(args.shape), seed)

            stem = f"case_{case_id:03d}_{needle_name}"
            vti_path = out_dir / f"{stem}.vti"
            truth_path = out_dir / f"{stem}.truth.json"

            write_vti(field, str(vti_path), array_name)

            truth.update({
                "case_id": case_id,
                "seed": seed,
                "shape": list(args.shape),
                "array_name": array_name,
                "file": vti_path.name,
            })
            with open(truth_path, "w") as f:
                json.dump(truth, f, indent=2)

            task = TASK_TEMPLATES[needle_name].format(abs_path=str(vti_path))
            manifest.append({
                "case_id": case_id,
                "needle": needle_name,
                "file": vti_path.name,
                "abs_path": str(vti_path),
                "truth_file": truth_path.name,
                "array_name": array_name,
                "task": task,
            })
            case_id += 1

    manifest_path = out_dir / "manifest.json"
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Generated {case_id} cases in {out_dir}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()

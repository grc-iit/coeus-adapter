#
# SST/Fides consumer for the 2D LBM-CFD case (Vigil render phase).
#
# Adapted from Xcompact3d/catalyst/tgv-pipeline.py. Connects to the hermes
# engine's trigger-gated Catalyst SST stream (contact file <stream>.sst in the
# simulation's run directory), renders the 2D uniform grid colored by vorticity
# each received step, and saves lbm-NNNNN.png. In gated mode the reader gets
# nothing until the variance trigger fires, then exactly trigger_inspect_steps
# steps: use --num-steps <inspect_steps>.
#
# Usage (writer must be started first; it blocks until a reader connects):
#   spack load paraview
#   pvbatch lbm-pipeline.py -j lbm-fides.json \
#       -b <script_location>/lbm_sst.bp --staging --num-steps 3
#
# The zero-dependency alternative (no ParaView) is
# test/real_apps/ascent-trame/examples/lbm-cfd/consumer/lbm_sst_reader.py.
#
import argparse
from paraview.simple import *


def SetupRenderView():
    view = CreateView('RenderView')
    SetActiveView(None)
    layout1 = CreateLayout(name='Layout #1')
    layout1.AssignView(0, view)
    layout1.SetSize(1000, 420)
    SetActiveView(view)
    return view


def SetupFidesReader(json, bp, sst):
    if json is None:
        return FidesReader(StreamSteps=1, FileName=bp)
    fides = FidesJSONReader(StreamSteps=1, FileName=json)
    if sst:
        fides.DataSourceEngines = ['source', 'SST']
    fides.DataSourcePath = ['source', bp]
    fides.UpdatePipelineInformation()
    return fides


def SetupVisPipeline(producer, view):
    # 2D uniform grid colored by vorticity (RdBu, symmetric range).
    display = Show(producer, view, 'UniformGridRepresentation')
    display.Representation = 'Surface'
    display.ColorArrayName = ['CELLS', 'vorticity']
    vortLUT = GetColorTransferFunction('vorticity')
    vortLUT.AutomaticRescaleRangeMode = 'Clamp and update every timestep'
    display.LookupTable = vortLUT
    bar = GetScalarBar(vortLUT, view)
    bar.Title = 'vorticity'
    bar.ComponentTitle = ''
    display.SetScalarBarVisibility(view, True)
    view.ResetCamera()
    camera = GetActiveCamera()
    camera.SetPosition(camera.GetFocalPoint()[0], camera.GetFocalPoint()[1],
                       camera.GetPosition()[2])
    return display


def ParseArgs():
    p = argparse.ArgumentParser()
    p.add_argument("-j", "--json_filename", type=str, required=False,
                   help="path to Fides JSON data model")
    p.add_argument("-b", "--bp_filename", type=str, required=True,
                   help="path to bp file / SST stream name")
    p.add_argument("--staging", action='store_true', help="use SST engine")
    p.add_argument("--num-steps", type=int, default=0,
                   help="exit after N steps (0 = until EndOfStream); "
                        "gated: set to trigger_inspect_steps")
    return p.parse_args()


def StreamingVis(args):
    import time
    OK, NotReady, EndOfStream = 0, 1, 2

    print('[Reader] Setting up Fides reader ...', flush=True)
    fides = SetupFidesReader(args.json_filename, args.bp_filename, args.staging)
    view = SetupRenderView()

    pipeline_ready = False
    step = 0
    while True:
        try:
            status = NotReady
            retries = 0
            t0 = time.time()
            print(f'[Reader] Waiting for step {step} ...', flush=True)
            while status == NotReady:
                fides.PrepareNextStep()
                fides.UpdatePipelineInformation()
                status = fides.NextStepStatus
                retries += 1
                if retries % 20 == 0:
                    print(f'[Reader]   attempt {retries}, status={status}, '
                          f'{time.time() - t0:.1f}s', flush=True)
            if status == EndOfStream:
                print('[Reader] EndOfStream — exiting', flush=True)
                return
            if not pipeline_ready:
                SetupVisPipeline(fides, view)
                pipeline_ready = True
            fides.UpdatePipeline()
            try:
                vr = fides.CellData['vorticity'].GetRange(0)
                print(f'[Reader] step {step}: vorticity-range={vr}', flush=True)
            except Exception as e:
                print(f'[Reader] step {step}: range read failed: {e}', flush=True)
            out = f'lbm-{step:05d}.png'
            SaveScreenshot(out, view, ImageResolution=[1000, 420])
            print(f'[Reader] saved {out}', flush=True)
            step += 1
            if args.num_steps > 0 and step >= args.num_steps:
                print(f'[Reader] received all {step} steps — exiting', flush=True)
                return
        except RuntimeError as e:
            msg = str(e).lower()
            if 'writer failed' in msg or 'remote peer' in msg:
                print(f'[Reader] writer disconnected after {step} steps', flush=True)
                return
            raise


if __name__ == '__main__':
    StreamingVis(ParseArgs())

#
# SST/Fides consumer for the Xcompact3d TGV case (Vigil render phase).
#
# Adapted from test/real_apps/gray-scott/catalyst/gs-pipeline.py. Connects
# to the hermes engine's Catalyst SST stream (contact file <stream>.sst in
# the simulation's output directory), renders a center slice colored by
# vorticity magnitude each received step, and saves tgv-NNNNN.png.
#
# Usage (writer must be started first; it blocks until a reader connects):
#   spack load paraview
#   pvbatch tgv-pipeline.py -j tgv-fides.json \
#       -b /mnt/common/hxu40/incompact3d/output/tgv.bp --staging --num-steps 10
#
# In trigger-gated mode the reader receives nothing until the Red fire,
# then exactly trigger_inspect_steps steps: use --num-steps <inspect_steps>.
#
import argparse
from paraview.simple import *

from paraview import print_info


def SetupRenderView():
    view = CreateView('RenderView')

    camera = GetActiveCamera()
    camera.Azimuth(30)
    camera.Elevation(30)

    SetActiveView(None)
    layout1 = CreateLayout(name='Layout #1')
    layout1.AssignView(0, view)
    layout1.SetSize(824, 656)
    SetActiveView(view)
    return view


def SetupFidesReader(json, bp, sst):
    if json is None:
        fides = FidesReader(StreamSteps=1, FileName=bp)
        return fides

    fides = FidesJSONReader(StreamSteps=1, FileName=json)
    if sst:
        fides.DataSourceEngines = ['source', 'SST']
    # 'source' is the name of the ADIOS data source in the JSON data model
    fides.DataSourcePath = ['source', bp]
    fides.UpdatePipelineInformation()
    return fides


def SetupVisPipeline(producer, view):
    # Center slice colored by vorticity magnitude: always non-empty,
    # robust across the whole TGV evolution (unlike fixed isosurfaces).
    producerDisplay = Show(producer, view, 'UniformGridRepresentation')
    view.ResetCamera()
    Hide(producer, view)

    vortLUT = GetColorTransferFunction('vort')
    vortLUT.AutomaticRescaleRangeMode = 'Clamp and update every timestep'
    vortLUT.RescaleOnVisibilityChange = 1

    slice1 = Slice(registrationName='Slice1', Input=producer)
    slice1.SliceType = 'Plane'
    slice1.SliceType.Normal = [0.0, 0.0, 1.0]
    # Slice through the domain center (fides grid: origin 0, spacing 1)
    bounds = producer.GetDataInformation().GetBounds()
    slice1.SliceType.Origin = [(bounds[0] + bounds[1]) / 2.0,
                               (bounds[2] + bounds[3]) / 2.0,
                               (bounds[4] + bounds[5]) / 2.0]

    sliceDisplay = Show(slice1, view, 'GeometryRepresentation')
    sliceDisplay.Representation = 'Surface'
    sliceDisplay.ColorArrayName = ['POINTS', 'vort']
    sliceDisplay.LookupTable = vortLUT

    vortColorBar = GetScalarBar(vortLUT, view)
    vortColorBar.Title = 'vort'
    vortColorBar.ComponentTitle = ''
    sliceDisplay.SetScalarBarVisibility(view, True)

    view.ResetCamera()
    return slice1, sliceDisplay


def ParseArgs():
    parser = argparse.ArgumentParser()
    parser.add_argument("-j", "--json_filename", help="path to Fides JSON file",
                        type=str, required=False)
    parser.add_argument("-b", "--bp_filename", help="path to bp file/stream",
                        type=str, required=True)
    parser.add_argument("--staging", help="use SST engine", action='store_true')
    parser.add_argument("--num-steps", type=int, default=0,
                        help="exit after receiving this many steps (0 = until EndOfStream)")
    args = parser.parse_args()
    return args


def StreamingVis(args):
    import time

    # adios/fides step status
    OK = 0
    NotReady = 1
    EndOfStream = 2

    print(f'[Reader] Setting up Fides reader ...', flush=True)
    fides = SetupFidesReader(args.json_filename, args.bp_filename, args.staging)
    print(f'[Reader] Fides reader ready', flush=True)

    view = SetupRenderView()

    pipeline = None
    display = None
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
            elapsed = time.time() - t0
            if retries % 10 == 0:
                print(f'[Reader]   PrepareNextStep attempt {retries}, '
                      f'status={status}, elapsed={elapsed:.1f}s', flush=True)
        elapsed = time.time() - t0
        print(f'[Reader] Step {step}: status={status} '
              f'(retries={retries}, {elapsed:.1f}s)', flush=True)
        if status == EndOfStream:
            print(f'[Reader] EndOfStream received — exiting', flush=True)
            return
        if pipeline is None:
            print(f'[Reader] Setting up vis pipeline ...', flush=True)
            pipeline, display = SetupVisPipeline(fides, view)
            print(f'[Reader] Vis pipeline ready', flush=True)

        pipeline.UpdatePipeline()
        display.RescaleTransferFunctionToDataRange()

        try:
            ux_range = fides.PointData['ux'].GetRange(0)
            vort_range = fides.PointData['vort'].GetRange(0)
            print(f'[Reader] Step {step}: ux-range={ux_range}, '
                  f'vort-range={vort_range}', flush=True)
        except Exception as e:
            print(f'[Reader] Step {step}: could not read ranges: {e}', flush=True)

        output = f'tgv-{step:05d}.png'
        SaveScreenshot(output, view, ImageResolution=[800, 800])
        print(f'[Reader] Saved {output}', flush=True)
        step += 1
        if args.num_steps > 0 and step >= args.num_steps:
            print(f'[Reader] Received all {step} steps — exiting', flush=True)
            return
      except RuntimeError as e:
        msg = str(e)
        if 'Writer failed' in msg or 'remote peer' in msg.lower():
            print(f'[Reader] Writer disconnected after {step} steps: {e}', flush=True)
            print(f'[Reader] Exiting gracefully.', flush=True)
            return
        else:
            raise
      except Exception as e:
        print(f'[Reader] Unexpected error at step {step}: {e}', flush=True)
        raise


# ------------------------------------------------------------------------------
if __name__ == '__main__':
    print('in __main__()')
    args = ParseArgs()
    StreamingVis(args)

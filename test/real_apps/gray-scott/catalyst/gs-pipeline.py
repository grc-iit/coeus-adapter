import argparse
from paraview.simple import *

from paraview import print_info

DISABLE_EXTRACTOR = False

# ------------------------------------------------------------------------------
# Catalyst options
from paraview import catalyst
options = catalyst.Options()
options.GlobalTrigger = 'TimeStep'
options.ExtractsOutputDirectory = '/tmp'
#options.ExtractsOutputDirectory = '.'

# Catalyst Live only works with a single process;
# disable it when running in __main__ (streaming) mode with MPI.
if __name__ != '__main__':
    options.EnableCatalystLive = 1
    options.CatalystLiveTrigger = 'TimeStep'


# setup and returns the view
def SetupRenderView():
    # Create a new 'Render View'
    view = CreateView('RenderView')

    camera = GetActiveCamera()
    camera.Azimuth(45)
    camera.Elevation(45)

    SetActiveView(None)

    # create new layout object 'Layout #1'
    layout1 = CreateLayout(name='Layout #1')
    layout1.AssignView(0, view)
    layout1.SetSize(824, 656)

    # restore active view
    SetActiveView(view)
    return view


# A different proxy type is used depending on whether you're using
# catalyst, or doing post hoc visualization
# this returns the catalyst producer
def SetupCatalystProducer():
    producer = TrivialProducer(registrationName='fides')
    return producer


# this returns the fides proxy which should be used for post hoc vis
# i.e., you're reading .bp files
def SetupFidesReader(json, bp, sst):
    if json is None:
        # in this case the bp file must contain the Fides attributes
        fides = FidesReader(StreamSteps=1, FileName=bp)
        return fides

    fides = FidesJSONReader(StreamSteps=1, FileName=json)
    if sst:
        fides.DataSourceEngines = ['source', 'SST']
    # 'source' is the name of the ADIOS data source in the JSON data model
    fides.DataSourcePath = ['source', bp]
    # required to update the fides reader
    fides.UpdatePipelineInformation()
    return fides


# takes in a producer and view and sets up the visualization pipeline
def SetupVisPipeline(producer, view):
    fDisplay = Show(producer, view, 'UniformGridRepresentation')
    view.ResetCamera()

    # get color transfer function/color map for 'U'
    uLUT = GetColorTransferFunction('V')
    uLUT.AutomaticRescaleRangeMode = 'Clamp and update every timestep'
    uLUT.RescaleOnVisibilityChange = 1

    contour1 = Contour(registrationName='Contour1', Input=producer)
    contour1.ContourBy = ['POINTS', 'V']
    contour1.Isosurfaces = [0.1, 0.3, 0.5, 0.7]
    contour1.PointMergeMethod = 'Uniform Binning'

    contour1Display = Show(contour1, view, 'GeometryRepresentation')

    contour1Display.Representation = 'Surface'
    contour1Display.ColorArrayName = ['POINTS', 'V']

    Hide(producer, view)

    # trace defaults for the display properties.
    contour1Display.Representation = 'Surface'
    contour1Display.ColorArrayName = ['POINTS', 'V']
    contour1Display.SetScaleArray = ['POINTS', 'V']
    contour1Display.ScaleTransferFunction = 'PiecewiseFunction'
    contour1Display.LookupTable = uLUT


    uLUTColorBar = GetScalarBar(uLUT, view)
    uLUTColorBar.Title = 'V'
    uLUTColorBar.ComponentTitle = ''

    # show color legend
    contour1Display.SetScalarBarVisibility(view, True)
    return contour1, contour1Display


# sets up an extractor for writing out PNG images
def SetupExtractor(view):
    # create extractor
    pNG1 = CreateExtractor('PNG', view, registrationName='PNG1')
    # trace defaults for the extractor.
    pNG1.Trigger = 'TimeStep'

    # init the 'PNG' selected for 'Writer'
    pNG1.Writer.FileName = 'output_{timestep:06d}.png'
    pNG1.Writer.ImageResolution = [800, 800]
    pNG1.Writer.Format = 'PNG'


# Catalyst uses this to update the pipeline and print out some info on each time step
# you don't need to call this directly in your script; ParaView Catalyst will call it for you
def catalyst_execute(info):
    print_info("in '%s::catalyst_execute'", __name__)
    global pipeline, display
    pipeline.UpdatePipeline()
    display.RescaleTransferFunctionToDataRange()

    print_info("executing (cycle={}, time={})".format(info.cycle, info.time))
    print_info("U-range: {}".format(producer.PointData['U'].GetRange(0)))
    print_info("V-range: {}".format(producer.PointData['V'].GetRange(0)))


def ParseArgs():
    parser = argparse.ArgumentParser()
    parser.add_argument("-j", "--json_filename", help="path to Fides JSON file", type=str, required=False)
    parser.add_argument("-b", "--bp_filename", help="path to bp file", type=str, required=True)
    parser.add_argument("--staging", help="use SST engine", action='store_true')
    parser.add_argument("--num-steps", help="total number of writer steps (exit after receiving all)", type=int, default=0)
    args = parser.parse_args()
    return args


def StreamingVis(args):
    import sys, time

    # adios/fides step status
    OK = 0
    NotReady = 1
    EndOfStream = 2

    # setup the reader, view, pipeline
    print(f'[Reader] Setting up Fides reader ...', flush=True)
    fides = SetupFidesReader(args.json_filename, args.bp_filename, args.staging)
    print(f'[Reader] Fides reader ready', flush=True)

    print(f'[Reader] Setting up render view ...', flush=True)
    view = SetupRenderView()
    print(f'[Reader] Render view ready', flush=True)

    step = 0
    while True:
      try:
        status = NotReady
        retries = 0
        t0 = time.time()
        print(f'[Reader] Waiting for step {step} ...', flush=True)
        while status == NotReady:
            # must call PrepareNextStep to get Fides ready to read the
            # next step
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
        if step == 0:
            # set up the pipeline on the first step
            print(f'[Reader] Setting up vis pipeline ...', flush=True)
            pipeline, display = SetupVisPipeline(fides, view)
            print(f'[Reader] Vis pipeline ready', flush=True)

        # need to update the pipeline and then save the output
        print(f'[Reader] Updating pipeline ...', flush=True)
        pipeline.UpdatePipeline()
        print(f'[Reader] Pipeline updated, rescaling ...', flush=True)
        display.RescaleTransferFunctionToDataRange()
        output = f'output-{step:05d}.png'
        # Print data ranges to verify data was received
        try:
            u_range = fides.PointData['U'].GetRange(0)
            v_range = fides.PointData['V'].GetRange(0)
            print(f'[Reader] Step {step}: U-range={u_range}, V-range={v_range}',
                  flush=True)
        except Exception as e:
            print(f'[Reader] Step {step}: could not read ranges: {e}', flush=True)

        output = f'output-{step:05d}.png'
        print(f'[Reader] Saving {output} ...', flush=True)
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
else:
    # in this case we're running from Catalyst
    view = SetupRenderView()
    producer = SetupCatalystProducer()
    pipeline, display = SetupVisPipeline(producer, view)

    # normally not needed, but a bug fix is in progress to fix an issue
    # when using ParaView Live with extractors
    if not DISABLE_EXTRACTOR:
        SetupExtractor(view)

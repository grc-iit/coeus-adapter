"""
ParaView Catalyst pipeline for Incompact3d (Xcompact3d).

Data flow (from Incompact3d visu.f90 + case visu):
  - Core: ux, uy, uz (velocity), pp (pressure)
  - Optional: vort, critq (e.g. TGV); rho (ilmn); phi01, phi02... (scalars); warp (istret)

This script supports both in situ (TrivialProducer) and post hoc (FidesJSONReader) modes.
"""

import argparse
from paraview.simple import (
    Calculator,
    Contour,
    CreateExtractor,
    CreateLayout,
    CreateView,
    FidesReader,
    FidesJSONReader,
    GetActiveCamera,
    GetColorTransferFunction,
    GetScalarBar,
    SetActiveView,
    Show,
    Hide,
    SaveScreenshot,
    TrivialProducer,
)
from paraview import print_info

# ------------------------------------------------------------------------------
# Catalyst options
from paraview import catalyst

options = catalyst.Options()
options.GlobalTrigger = "TimeStep"
options.EnableCatalystLive = 1
options.CatalystLiveTrigger = "TimeStep"
options.ExtractsOutputDirectory = "/tmp"

DISABLE_EXTRACTOR = False


def SetupRenderView():
    view = CreateView("RenderView")
    camera = GetActiveCamera()
    camera.Azimuth(45)
    camera.Elevation(45)
    SetActiveView(None)
    layout1 = CreateLayout(name="Layout #1")
    layout1.AssignView(0, view)
    layout1.SetSize(824, 656)
    SetActiveView(view)
    return view


def SetupCatalystProducer():
    return TrivialProducer(registrationName="fides")


def SetupFidesReader(json_path, bp_path, sst):
    if json_path is None:
        fides = FidesReader(StreamSteps=1, FileName=bp_path)
        return fides
    fides = FidesJSONReader(StreamSteps=1, FileName=json_path)
    if sst:
        fides.DataSourceEngines = ["source", "SST"]
    fides.DataSourcePath = ["source", bp_path]
    fides.UpdatePipelineInformation()
    return fides


def _vel_mag_expression(producer):
    """Use ux/uy/uz (Incompact3d) or u/v/w (ParaView default) for velocity magnitude."""
    pd = producer.PointData
    if "ux" in pd.keys() and "uy" in pd.keys() and "uz" in pd.keys():
        return "sqrt(ux*ux + uy*uy + uz*uz)"
    return "sqrt(u*u + v*v + w*w)"


def SetupVisPipeline(producer, view, field="vel_mag"):
    """
    Set up visualization pipeline for Incompact3d.
    field: 'vel_mag' | 'vort' | 'pp' | 'critq'
    """
    Show(producer, view, "UniformGridRepresentation")
    view.ResetCamera()

    pd = producer.PointData
    has_vort = "vort" in pd.keys()
    has_critq = "critq" in pd.keys()
    has_pp = "pp" in pd.keys()

    if field == "vort" and has_vort:
        lut = GetColorTransferFunction("vort")
        lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
        lut.RescaleOnVisibilityChange = 1
        contour = Contour(registrationName="Contour1", Input=producer)
        contour.ContourBy = ["POINTS", "vort"]
        contour.Isosurfaces = [0.1, 0.3, 0.5, 0.7]
        contour.PointMergeMethod = "Uniform Binning"
        disp = Show(contour, view, "GeometryRepresentation")
        disp.Representation = "Surface"
        disp.ColorArrayName = ["POINTS", "vort"]
        disp.SetScaleArray = ["POINTS", "vort"]
        disp.ScaleTransferFunction = "PiecewiseFunction"
        disp.LookupTable = lut
        Hide(producer, view)
        GetScalarBar(lut, view).Title = "|ω|"
        disp.SetScalarBarVisibility(view, True)
        return contour, disp
    if field == "critq" and has_critq:
        lut = GetColorTransferFunction("critq")
        lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
        lut.RescaleOnVisibilityChange = 1
        contour = Contour(registrationName="Contour1", Input=producer)
        contour.ContourBy = ["POINTS", "critq"]
        contour.Isosurfaces = [0.0]
        contour.PointMergeMethod = "Uniform Binning"
        disp = Show(contour, view, "GeometryRepresentation")
        disp.Representation = "Surface"
        disp.ColorArrayName = ["POINTS", "critq"]
        disp.SetScaleArray = ["POINTS", "critq"]
        disp.ScaleTransferFunction = "PiecewiseFunction"
        disp.LookupTable = lut
        Hide(producer, view)
        disp.SetScalarBarVisibility(view, True)
        return contour, disp
    if field == "pp" and has_pp:
        lut = GetColorTransferFunction("pp")
        lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
        lut.RescaleOnVisibilityChange = 1
        contour = Contour(registrationName="Contour1", Input=producer)
        contour.ContourBy = ["POINTS", "pp"]
        contour.Isosurfaces = [0.0]
        contour.PointMergeMethod = "Uniform Binning"
        disp = Show(contour, view, "GeometryRepresentation")
        disp.Representation = "Surface"
        disp.ColorArrayName = ["POINTS", "pp"]
        disp.SetScaleArray = ["POINTS", "pp"]
        disp.ScaleTransferFunction = "PiecewiseFunction"
        disp.LookupTable = lut
        Hide(producer, view)
        disp.SetScalarBarVisibility(view, True)
        return contour, disp

    # Default: velocity magnitude (works for all Incompact3d runs)
    calc = Calculator(registrationName="VelocityMagnitude", Input=producer)
    calc.ResultArrayName = "vel_mag"
    calc.Function = _vel_mag_expression(producer)
    lut = GetColorTransferFunction("vel_mag")
    lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
    lut.RescaleOnVisibilityChange = 1
    contour = Contour(registrationName="Contour1", Input=calc)
    contour.ContourBy = ["POINTS", "vel_mag"]
    contour.Isosurfaces = [0.1, 0.3, 0.5, 0.7]
    contour.PointMergeMethod = "Uniform Binning"
    disp = Show(contour, view, "GeometryRepresentation")
    disp.Representation = "Surface"
    disp.ColorArrayName = ["POINTS", "vel_mag"]
    disp.SetScaleArray = ["POINTS", "vel_mag"]
    disp.ScaleTransferFunction = "PiecewiseFunction"
    disp.LookupTable = lut
    Hide(producer, view)
    GetScalarBar(lut, view).Title = "|u|"
    disp.SetScalarBarVisibility(view, True)
    return contour, disp


def SetupExtractor(view):
    pNG1 = CreateExtractor("PNG", view, registrationName="PNG1")
    pNG1.Trigger = "TimeStep"
    pNG1.Writer.FileName = "output_{timestep:06d}.png"
    pNG1.Writer.ImageResolution = [800, 800]
    pNG1.Writer.Format = "PNG"


def catalyst_execute(info):
    print_info("in '%s::catalyst_execute'", __name__)
    global pipeline, display
    pipeline.UpdatePipeline()
    display.RescaleTransferFunctionToDataRange()
    print_info("executing (cycle={}, time={})".format(info.cycle, info.time))
    pd = producer.PointData
    for name in ["ux", "uy", "uz", "pp", "vort", "critq"]:
        if name in pd.keys():
            try:
                print_info("{}-range: {}".format(name, pd[name].GetRange(0)))
            except Exception:
                pass


def ParseArgs():
    parser = argparse.ArgumentParser(description="Incompact3d Catalyst pipeline (post hoc)")
    parser.add_argument("-j", "--json_filename", help="path to Fides JSON (e.g. fide.json)", type=str, required=False)
    parser.add_argument("-b", "--bp_filename", help="path to BP file", type=str, required=True)
    parser.add_argument("--staging", help="use SST engine", action="store_true")
    parser.add_argument(
        "-f", "--field",
        help="Field to visualize: vel_mag | vort | pp | critq",
        type=str,
        default="vel_mag",
    )
    return parser.parse_args()


def StreamingVis(args):
    OK, NotReady, EndOfStream = 0, 1, 2
    fides = SetupFidesReader(args.json_filename, args.bp_filename, args.staging)
    view = SetupRenderView()
    step = 0
    while True:
        status = NotReady
        while status == NotReady:
            fides.PrepareNextStep()
            fides.UpdatePipelineInformation()
            status = fides.NextStepStatus
        if status == EndOfStream:
            return
        if step == 0:
            pipeline, display = SetupVisPipeline(fides, view, field=args.field)
        pipeline.UpdatePipeline()
        display.RescaleTransferFunctionToDataRange()
        SaveScreenshot("output-{:05d}.png".format(step), view, ImageResolution=[800, 800])
        step += 1


if __name__ == "__main__":
    print("in __main__()")
    args = ParseArgs()
    StreamingVis(args)
else:
    view = SetupRenderView()
    producer = SetupCatalystProducer()
    pipeline, display = SetupVisPipeline(producer, view, field="vel_mag")
    if not DISABLE_EXTRACTOR:
        SetupExtractor(view)

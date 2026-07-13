"""
In-situ streaming bridge for the Xcompact3d TGV case: reads the hermes
engine's Catalyst SST stream step-by-step via Fides into a pvserver,
exposing live data for interactive AI agent manipulation through the
in-situ MCP server (tgv_insitu_mcp_server.py).

Adapted from test/insitu_agent/insitu_streaming.py (gray-scott). Same
status/command-file protocol, timing JSONL, --render-steps, and the
empty-screenshot defenses; the visualized field is parameterized
(--field, default "vort") because the TGV stream carries
ux/uy/uz/vort/critq instead of U/V.

Trigger-gated mode: with TriggerType=dissipation the writer ships
nothing until the Red fire, then exactly trigger_inspect_steps steps.
The bridge idles in the SST poll loop until the window opens; start it
--paused so the agent controls stepping through the window.

Architecture:
  Xcompact3d (hermes engine, SST writer) --> [this script via pvpython] --> pvserver
                                                                              ^
                                                                       in-situ MCP server
                                                                              ^
                                                                          AI Agent

Usage (consumer node, e.g. ares-comp-26):
  # Terminal 1: pvserver
  pvserver --multi-clients --server-port=11112

  # Terminal 2: this bridge (absolute paths for -j and -b)
  pvpython tgv_insitu_streaming.py \
      -j <abs>/test/real_apps/Xcompact3d/catalyst/tgv-fides.json \
      -b /mnt/common/hxu40/incompact3d/output/tgv.bp \
      --staging --server localhost --port 11112 --paused \
      --screenshot-file <abs>/tgv_bridge_view.png
"""

import argparse
import threading
import time
import json
import os

from paraview.simple import *


class StreamingState:
    """Shared state between the streaming loop and the MCP control interface."""

    def __init__(self):
        self.lock = threading.Lock()
        self.step = 0
        self.paused = False
        self.ended = False
        self.advance_one = False
        self.fides = None
        self.view = None
        self.pipeline_ready = False
        self._status_file = None
        self._command_file = None

    def set_status_file(self, path):
        self._status_file = path
        self._command_file = path.replace("streaming_status.json", "streaming_command.json")

    def write_status(self):
        """Write current state to a JSON file so the MCP server can read it."""
        if not self._status_file:
            return
        with self.lock:
            status = {
                "step": self.step,
                "paused": self.paused,
                "ended": self.ended,
                "pipeline_ready": self.pipeline_ready,
            }
        try:
            with open(self._status_file, "w") as f:
                json.dump(status, f)
        except OSError:
            pass

    def poll_commands(self):
        """Check for commands written by the MCP server."""
        if not self._command_file or not os.path.exists(self._command_file):
            return
        try:
            with open(self._command_file, "r") as f:
                cmd = json.load(f)
            os.remove(self._command_file)

            action = cmd.get("action", "")
            if action == "pause":
                with self.lock:
                    self.paused = True
                print("[tgv_insitu_streaming] Paused by MCP command")
            elif action == "resume":
                with self.lock:
                    self.paused = False
                print("[tgv_insitu_streaming] Resumed by MCP command")
            elif action == "advance_one":
                with self.lock:
                    self.advance_one = True
                    self.paused = False
                print("[tgv_insitu_streaming] Advancing one step by MCP command")
        except (json.JSONDecodeError, OSError):
            pass


# Global streaming state — the MCP server reads/writes this
streaming_state = StreamingState()

# Field the bridge colors/renders by; set from --field in main().
FIELD = "vort"


def setup_fides_reader(json_file, bp_file, use_sst):
    """Create a Fides reader configured for SST streaming or BP file reading."""
    if json_file is None:
        fides = FidesReader(StreamSteps=1, FileName=bp_file)
        return fides

    fides = FidesJSONReader(StreamSteps=1, FileName=json_file)
    if use_sst:
        fides.DataSourceEngines = ["source", "SST"]
    fides.DataSourcePath = ["source", bp_file]
    fides.UpdatePipelineInformation()
    return fides


def setup_render_view():
    """Create and configure a render view."""
    view = CreateView("RenderView")
    camera = GetActiveCamera()
    camera.Azimuth(45)
    camera.Elevation(45)
    SetActiveView(None)

    layout = CreateLayout(name="Layout #1")
    layout.AssignView(0, view)
    layout.SetSize(1024, 768)

    SetActiveView(view)
    return view


def setup_initial_display(fides, view):
    """
    Set up a meaningful visualization of FIELD (default: vorticity
    magnitude) that renders on every bridge step. Volume rendering on the
    raw uniform grid: no coordinate/origin tuning needed, shows the 3D
    vortex structures evolving, and works reliably across clients. With
    the LUT clamped-and-rescaled every step it stays non-empty across the
    whole TGV evolution (vort spans ~2 at t=0 to ~20 in the cascade).

    We first force a pipeline update on the Fides source so data
    information (field arrays, bounds) is known before we configure the
    display. Then we show, switch to Volume representation, and set the
    color mapping. If Volume rendering is unavailable for whatever reason
    we fall back to Outline so the bridge still produces a non-empty
    image instead of just the axes widget.
    """
    fides.UpdatePipeline()

    # The stream arrives as one block per writer rank (e.g. 25 pencils)
    # with no ghost cells; volume-rendering the raw multiblock shows slab
    # artifacts at every block seam. Resample onto a single uniform image
    # (dimensions inferred from the total point count — TGV grids are
    # cubic) so the volume rendering is seamless. RESAMPLE=0 disables.
    source = fides
    if os.environ.get("TGV_BRIDGE_RESAMPLE", "1") != "0":
        try:
            n = fides.GetDataInformation().GetNumberOfPoints()
            dim = max(2, int(round(n ** (1.0 / 3.0))))
            resample = ResampleToImage(Input=fides)
            resample.SamplingDimensions = [dim, dim, dim]
            resample.UpdatePipeline()
            source = resample
            print(f"[tgv_insitu_streaming] Resampling {n} pts to {dim}^3 image "
                  f"for seamless volume rendering")
        except Exception as e:
            print(f"[tgv_insitu_streaming] WARN: ResampleToImage failed ({e}); "
                  f"volume-rendering raw blocks")

    display = Show(source, view, "UniformGridRepresentation")
    # Default to Outline so we always have SOMETHING visible.
    try:
        display.SetRepresentationType("Outline")
    except Exception as e:
        print(f"[tgv_insitu_streaming] WARN: set Outline failed: {e}")

    view.ResetCamera()

    # Try to upgrade to a Volume rendering of FIELD. This is the real
    # visualization; the Outline was just a safety net.
    try:
        display.SetRepresentationType("Volume")
        ColorBy(display, ("POINTS", FIELD))

        lut = GetColorTransferFunction(FIELD)
        lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
        lut.RescaleOnVisibilityChange = 1
        display.RescaleTransferFunctionToDataRange(True, False)
        print(f"[tgv_insitu_streaming] Using Volume rendering of {FIELD}")
    except Exception as e:
        print(f"[tgv_insitu_streaming] WARN: Volume rendering unavailable ({e}); keeping Outline")

    SetActiveSource(fides)
    return display


def _debug_view_state(view, tag, fides=None):
    """Dump per-representation state (gated by INSITU_DEBUG_VIEW=1)."""
    if os.environ.get("INSITU_DEBUG_VIEW") != "1":
        return
    try:
        if fides is not None:
            print(f"[tgv_insitu_streaming] DEBUG {tag}: fides npts="
                  f"{fides.GetDataInformation().GetNumberOfPoints()}")
        lut = GetColorTransferFunction(FIELD)
        pts = list(lut.RGBPoints)
        pwf = GetOpacityTransferFunction(FIELD)
        print(f"[tgv_insitu_streaming] DEBUG {tag}: campos="
              f"{[round(x, 3) for x in view.CameraPosition]} focal="
              f"{[round(x, 3) for x in view.CameraFocalPoint]} nreps="
              f"{len(view.Representations)}")
        print(f"[tgv_insitu_streaming] DEBUG {tag}: LUT range "
              f"[{pts[0]:.4g},{pts[-4]:.4g}] PWF={list(pwf.Points)}")
        for rep in view.Representations:
            try:
                inp = rep.Input
                iname = inp.GetXMLLabel() if hasattr(inp, "GetXMLLabel") else "?"
                print(f"[tgv_insitu_streaming] DEBUG {tag}: rep input={iname} "
                      f"vis={rep.Visibility} "
                      f"type={getattr(rep, 'Representation', '?')} "
                      f"color={list(getattr(rep, 'ColorArrayName', []))}")
            except Exception as e:
                print(f"[tgv_insitu_streaming] DEBUG {tag}: rep ? ({e})")
    except Exception as e:
        print(f"[tgv_insitu_streaming] DEBUG {tag}: dump failed: {e}")


def _reassert_view_state(fides, view, display):
    """
    Re-assert the bridge's visualization state before rendering
    (defense-in-depth against other collaboration clients).

    NOTE: the actual fix for the "empty screenshot" bug lives in
    tgv_insitu_mcp_server.py (_isolate_mcp_view): when another client
    Show()s a filter into THIS view, every subsequent bridge render
    comes out empty server-side, and this batch client cannot repair it
    — it never processes collaboration sync, so the foreign
    representation isn't even visible in view.Representations here.
    What this function CAN protect against: another client pushing its
    default camera into the shared view (ResetCamera is an RPC that
    always executes server-side and refits to the data), a hidden
    volume display, and shared transfer functions rescaled to a
    degenerate range.
    """
    try:
        SetActiveView(view)
        SetActiveSource(fides)
        view.ResetCamera()
        if display is None:
            return
        # Hide any representation another client showed into this view
        # (e.g. the MCP server's create_isosurface). Rendering a foreign
        # filter here re-executes it, which re-pulls the streaming Fides
        # reader outside the bridge's PrepareNextStep cycle; with no SST
        # step ready the reader delivers an empty grid and EVERY rep in
        # the view (volume included) renders empty from then on.
        try:
            own_id = display.GetGlobalIDAsString()
            for rep in view.Representations:
                try:
                    if (rep.GetGlobalIDAsString() != own_id
                            and getattr(rep, "Visibility", 0)):
                        rep.Visibility = 0
                        print("[tgv_insitu_streaming] WARN: hid foreign "
                              "representation in bridge view (filters from "
                              "other clients render in their own session)")
                except Exception:
                    pass
        except Exception:
            pass
        if not display.Visibility:
            print("[tgv_insitu_streaming] WARN: volume display was hidden by "
                  "another client; re-showing")
            display.Visibility = 1
        try:
            rmin, rmax = fides.PointData[FIELD].GetRange(0)
        except Exception:
            return
        if rmax > rmin:
            lut = GetColorTransferFunction(FIELD)
            lut.RescaleTransferFunction(rmin, rmax)
            pwf = GetOpacityTransferFunction(FIELD)
            pwf.RescaleTransferFunction(rmin, rmax)
    except Exception as e:
        print(f"[tgv_insitu_streaming] WARN: view-state reassert failed: {e}")


def _write_timing_record(timing_file, record):
    """Append a timing record as a JSON line."""
    if not timing_file:
        return
    try:
        with open(timing_file, "a") as f:
            json.dump(record, f)
            f.write("\n")
    except OSError:
        pass


def streaming_loop(args, state):
    """
    Main streaming loop. Reads SST steps one at a time.
    When paused, it holds the current step's data in the pipeline
    so the AI agent can interactively explore it via MCP tools.
    """
    NOT_READY = 1
    END_OF_STREAM = 2

    timing_file = getattr(args, 'timing_file', None)
    max_steps = getattr(args, 'max_steps', 0) or 0  # 0 = unlimited
    screenshot_file = getattr(args, 'screenshot_file', None)

    # Which steps should actually render + save a screenshot? Default "all".
    render_steps_raw = getattr(args, 'render_steps', None) or "all"
    if str(render_steps_raw).lower() == "all":
        render_steps = None
    else:
        render_steps = set()
        for s in str(render_steps_raw).split(","):
            s = s.strip()
            if s:
                try:
                    render_steps.add(int(s))
                except ValueError:
                    print(f"[tgv_insitu_streaming] WARN: ignoring invalid render step '{s}'")
        print(f"[tgv_insitu_streaming] Render-only steps: {sorted(render_steps)}")

    fides = setup_fides_reader(args.json_filename, args.bp_filename, args.staging)
    view = setup_render_view()

    with state.lock:
        state.fides = fides
        state.view = view

    state.write_status()
    display = None

    while True:
        # Poll for MCP commands and wait while paused
        while state.paused and not state.ended:
            state.poll_commands()
            state.write_status()
            time.sleep(0.5)

        state.poll_commands()

        if state.ended:
            break

        # --- Timing: SST wait ---
        # In trigger-gated mode this poll spans the whole quiet phase:
        # the writer ships nothing until the Red fire opens the window.
        t_sst_start = time.monotonic()

        status = NOT_READY
        while status == NOT_READY:
            fides.PrepareNextStep()
            fides.UpdatePipelineInformation()
            status = fides.NextStepStatus
            if status == NOT_READY:
                state.poll_commands()
                time.sleep(0.1)

        t_sst_end = time.monotonic()

        if status == END_OF_STREAM:
            with state.lock:
                state.ended = True
            state.write_status()
            print(f"[tgv_insitu_streaming] End of stream after {state.step} steps")
            return

        # --- Timing: pipeline setup / update ---
        t_pipeline_start = time.monotonic()

        with state.lock:
            if state.step == 0:
                display = setup_initial_display(fides, view)
                state.pipeline_ready = True
            state.step += 1

        should_render = (render_steps is None) or (state.step in render_steps)

        if should_render:
            # Force the Fides source to re-pull data for the newly-prepared
            # step so downstream filters re-execute on fresh values.
            fides.UpdatePipeline()

            _debug_view_state(view, f"pre-reassert step {state.step}", fides)
            _reassert_view_state(fides, view, display)

            if display:
                display.RescaleTransferFunctionToDataRange()

            t_pipeline_end = time.monotonic()

            # --- Timing: render ---
            t_render_start = time.monotonic()
            Render(view)
            t_render_end = time.monotonic()

            # Save the bridge's view to disk so MCP get_screenshot can serve it.
            if screenshot_file:
                try:
                    base, ext = os.path.splitext(screenshot_file)
                    tmp_path = base + ".tmp" + (ext or ".png")
                    SaveScreenshot(tmp_path, view)
                    os.replace(tmp_path, screenshot_file)
                except Exception as e:
                    print(f"[tgv_insitu_streaming] WARN: failed to save screenshot: {e}")
        else:
            # Skip pipeline/render/save. The Fides reader has already been
            # advanced by PrepareNextStep + UpdatePipelineInformation above.
            t_pipeline_end = time.monotonic()
            t_render_start = t_pipeline_end
            t_render_end = t_pipeline_end

        state.write_status()

        _write_timing_record(timing_file, {
            "step": state.step,
            "sst_wait_ms": round((t_sst_end - t_sst_start) * 1000, 2),
            "pipeline_update_ms": round((t_pipeline_end - t_pipeline_start) * 1000, 2),
            "render_ms": round((t_render_end - t_render_start) * 1000, 2),
            "step_total_ms": round((t_render_end - t_sst_start) * 1000, 2),
            "timestamp": time.time(),
        })

        tag = "RENDER" if should_render else "SKIP  "
        print(f"[tgv_insitu_streaming] Step {state.step} {tag} "
              f"(sst={t_sst_end - t_sst_start:.3f}s "
              f"pipeline={t_pipeline_end - t_pipeline_start:.3f}s "
              f"render={t_render_end - t_render_start:.3f}s)")

        # --- SAFETY: hard step cap ---
        if max_steps > 0 and state.step >= max_steps:
            with state.lock:
                state.ended = True
            state.write_status()
            print(f"[tgv_insitu_streaming] MAX_STEPS={max_steps} reached — stopping gracefully.")
            return

        # If advance_one was requested, pause after this step
        with state.lock:
            if state.advance_one:
                state.advance_one = False
                state.paused = True
                print(f"[tgv_insitu_streaming] Paused after single-step advance")

        # Brief pause to let the AI agent observe/interact before moving on
        if not state.paused:
            time.sleep(args.step_delay)


def parse_args():
    parser = argparse.ArgumentParser(
        description="In-situ streaming bridge for AI agent interaction (Xcompact3d TGV)"
    )
    parser.add_argument(
        "-j", "--json_filename",
        help="Path to Fides JSON data model file (tgv-fides.json)",
        type=str, required=False,
    )
    parser.add_argument(
        "-b", "--bp_filename",
        help="ADIOS2 stream name (e.g. /mnt/common/hxu40/incompact3d/output/tgv.bp)",
        type=str, required=True,
    )
    parser.add_argument(
        "--staging", help="Use SST engine for live streaming",
        action="store_true",
    )
    parser.add_argument(
        "--server", help="pvserver hostname",
        type=str, default="localhost",
    )
    parser.add_argument(
        "--port", help="pvserver port",
        type=int, default=11112,
    )
    parser.add_argument(
        "--field",
        help="Field the bridge renders (default: vort; stream carries "
             "ux/uy/uz/vort/critq per tgv-fides.json)",
        type=str, default="vort",
    )
    parser.add_argument(
        "--step-delay",
        help="Seconds to wait between steps in auto-advance mode (default: 2.0)",
        type=float, default=2.0,
    )
    parser.add_argument(
        "--paused",
        help="Start in paused mode (agent must call advance_step)",
        action="store_true",
    )
    parser.add_argument(
        "--status-file",
        help="Path to write JSON status file for MCP server",
        type=str, default="tgv_streaming_status.json",
    )
    parser.add_argument(
        "--timing-file",
        help="Path to write per-step timing JSONL file",
        type=str, default=None,
    )
    parser.add_argument(
        "--max-steps",
        help="Hard cap on number of SST steps to consume (0 = unlimited). "
             "In gated mode set to trigger_inspect_steps so the bridge "
             "exits after the window instead of polling forever.",
        type=int, default=0,
    )
    parser.add_argument(
        "--screenshot-file",
        help="Path to save a PNG after each Render(). The MCP server's "
             "get_screenshot tool reads this file so the agent sees the "
             "bridge's own view instead of the MCP client's empty view.",
        type=str, default=None,
    )
    parser.add_argument(
        "--render-steps",
        help="Comma-separated list of step numbers where the bridge will "
             "actually do Fides UpdatePipeline + Render + SaveScreenshot. "
             "Other steps just consume the SST step and record timing. "
             "Default 'all' renders every step.",
        type=str, default="all",
    )
    return parser.parse_args()


def main():
    global FIELD
    args = parse_args()
    FIELD = args.field

    print(f"[tgv_insitu_streaming] Connecting to pvserver at {args.server}:{args.port}")
    Connect(f"{args.server}:{args.port}")

    streaming_state.paused = args.paused
    streaming_state.set_status_file(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), args.status_file)
        if not os.path.isabs(args.status_file) else args.status_file
    )

    print(f"[tgv_insitu_streaming] Starting streaming loop (paused={args.paused}, field={FIELD})")
    print(f"[tgv_insitu_streaming] Status file: {args.status_file}")

    streaming_loop(args, streaming_state)

    print("[tgv_insitu_streaming] Done.")


if __name__ == "__main__":
    main()

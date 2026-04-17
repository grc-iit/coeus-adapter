"""
In-situ streaming bridge: reads Gray-Scott SST data step-by-step via Fides
into a pvserver, exposing live data for interactive AI agent manipulation
through the ParaView MCP server.

Architecture:
  Gray-Scott sim (SST writer) --> [this script via pvpython on pvserver] --> pvserver
                                                                              ^
                                                                              |
                                                                     ParaView MCP server
                                                                              ^
                                                                              |
                                                                         AI Agent

Usage:
  # Terminal 1: start pvserver
  pvserver --multi-clients --server-port=11111

  # Terminal 2: (optional) connect ParaView GUI to pvserver for live viewing

  # Terminal 3: start the simulation (SST writer)
  mpirun -n 4 adios2-gray-scott settings-staging.json

  # Terminal 4: start this streaming bridge (connects to pvserver, reads SST)
  pvpython insitu_streaming.py -j gs-fides.json -b gs.bp --staging --server localhost --port 11111

  # Terminal 5: start the MCP server (connects to same pvserver)
  python insitu_mcp_server.py --server localhost --port 11111
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
                print("[insitu_streaming] Paused by MCP command")
            elif action == "resume":
                with self.lock:
                    self.paused = False
                print("[insitu_streaming] Resumed by MCP command")
            elif action == "advance_one":
                with self.lock:
                    self.advance_one = True
                    self.paused = False
                print("[insitu_streaming] Advancing one step by MCP command")
        except (json.JSONDecodeError, OSError):
            pass


# Global streaming state — the MCP server reads/writes this
streaming_state = StreamingState()


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
    Set up a meaningful visualization of the V field that renders on
    every bridge step. We use Volume rendering on the raw uniform grid
    colored by V: no coordinate/origin tuning needed, shows the full 3D
    pattern evolving, and works reliably across clients.

    We first force a pipeline update on the Fides source so data
    information (field arrays, bounds) is known before we configure the
    display. Then we show, switch to Volume representation, and set the
    color mapping. If Volume rendering is unavailable for whatever reason
    we fall back to Outline so the bridge still produces a non-empty
    image instead of just the axes widget.
    """
    # Make sure the Fides source has executed once so its field metadata
    # is populated before we query or color by V.
    fides.UpdatePipeline()

    display = Show(fides, view, "UniformGridRepresentation")
    # Default to Outline so we always have SOMETHING visible.
    try:
        display.SetRepresentationType("Outline")
    except Exception as e:
        print(f"[insitu_streaming] WARN: set Outline failed: {e}")

    view.ResetCamera()

    # Try to upgrade to a Volume rendering of V (the interesting Gray-Scott
    # field). This is the real visualization; the Outline was just a safety
    # net in case Volume isn't supported.
    try:
        display.SetRepresentationType("Volume")
        ColorBy(display, ("POINTS", "V"))

        vLUT = GetColorTransferFunction("V")
        vLUT.AutomaticRescaleRangeMode = "Clamp and update every timestep"
        vLUT.RescaleOnVisibilityChange = 1
        display.RescaleTransferFunctionToDataRange(True, False)
        print("[insitu_streaming] Using Volume rendering of V")
    except Exception as e:
        print(f"[insitu_streaming] WARN: Volume rendering unavailable ({e}); keeping Outline")

    SetActiveSource(fides)
    return display


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
    # Skipping render/save on non-interesting steps avoids the ~1.6 s of
    # Fides UpdatePipeline + volume rendering per step when the agent will
    # never look at those frames (Phase 1 skip and Phase 3 drain).
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
                    print(f"[insitu_streaming] WARN: ignoring invalid render step '{s}'")
        print(f"[insitu_streaming] Render-only steps: {sorted(render_steps)}")

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
        t_sst_start = time.monotonic()

        # Poll for next SST step
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
            print(f"[insitu_streaming] End of stream after {state.step} steps")
            return

        # --- Timing: pipeline setup / update ---
        t_pipeline_start = time.monotonic()

        with state.lock:
            if state.step == 0:
                display = setup_initial_display(fides, view)
                state.pipeline_ready = True
            state.step += 1

        # Decide whether this step is in the "render set" — if yes, do the
        # full pipeline update + render + save; if no, just advance SST and
        # record timing. This is the optimization that lets Phase 1 and
        # Phase 3 flash through without wasting compute on frames the agent
        # never inspects.
        should_render = (render_steps is None) or (state.step in render_steps)

        if should_render:
            # Force the Fides source to re-pull data for the newly-prepared
            # step so downstream filters re-execute on fresh V values.
            fides.UpdatePipeline()

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
                    print(f"[insitu_streaming] WARN: failed to save screenshot: {e}")
        else:
            # Skip pipeline/render/save. The Fides reader has already been
            # advanced by PrepareNextStep + UpdatePipelineInformation above.
            t_pipeline_end = time.monotonic()
            t_render_start = t_pipeline_end
            t_render_end = t_pipeline_end

        state.write_status()

        # Write timing record
        _write_timing_record(timing_file, {
            "step": state.step,
            "sst_wait_ms": round((t_sst_end - t_sst_start) * 1000, 2),
            "pipeline_update_ms": round((t_pipeline_end - t_pipeline_start) * 1000, 2),
            "render_ms": round((t_render_end - t_render_start) * 1000, 2),
            "step_total_ms": round((t_render_end - t_sst_start) * 1000, 2),
            "timestamp": time.time(),
        })

        tag = "RENDER" if should_render else "SKIP  "
        print(f"[insitu_streaming] Step {state.step} {tag} "
              f"(sst={t_sst_end - t_sst_start:.3f}s "
              f"pipeline={t_pipeline_end - t_pipeline_start:.3f}s "
              f"render={t_render_end - t_render_start:.3f}s)")

        # --- SAFETY: hard step cap ---
        if max_steps > 0 and state.step >= max_steps:
            with state.lock:
                state.ended = True
            state.write_status()
            print(f"[insitu_streaming] MAX_STEPS={max_steps} reached — stopping gracefully.")
            return

        # If advance_one was requested, pause after this step
        with state.lock:
            if state.advance_one:
                state.advance_one = False
                state.paused = True
                print(f"[insitu_streaming] Paused after single-step advance")

        # Brief pause to let the AI agent observe/interact before moving on
        if not state.paused:
            time.sleep(args.step_delay)


def parse_args():
    parser = argparse.ArgumentParser(
        description="In-situ streaming bridge for AI agent interaction"
    )
    parser.add_argument(
        "-j", "--json_filename",
        help="Path to Fides JSON data model file",
        type=str, required=False,
    )
    parser.add_argument(
        "-b", "--bp_filename",
        help="ADIOS2 stream name (e.g. gs.bp)",
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
        type=str, default="streaming_status.json",
    )
    parser.add_argument(
        "--timing-file",
        help="Path to write per-step timing JSONL file",
        type=str, default=None,
    )
    parser.add_argument(
        "--max-steps",
        help="Hard cap on number of SST steps to consume (0 = unlimited). "
             "Prevents run-away step counting if writer crashes without "
             "signalling END_OF_STREAM.",
        type=int, default=0,
    )
    parser.add_argument(
        "--screenshot-file",
        help="Path to save a PNG after each Render(). The MCP server's "
             "get_screenshot tool reads this file so the agent sees the "
             "bridge's own view (including the slice visualization the "
             "bridge sets up) instead of the MCP client's empty view.",
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
    args = parse_args()

    # Connect to the pvserver
    print(f"[insitu_streaming] Connecting to pvserver at {args.server}:{args.port}")
    Connect(f"{args.server}:{args.port}")

    streaming_state.paused = args.paused
    streaming_state.set_status_file(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), args.status_file)
    )

    print(f"[insitu_streaming] Starting streaming loop (paused={args.paused})")
    print(f"[insitu_streaming] Status file: {args.status_file}")

    streaming_loop(args, streaming_state)

    print("[insitu_streaming] Done.")


if __name__ == "__main__":
    main()

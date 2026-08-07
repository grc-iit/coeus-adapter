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


# --- Scale profile ----------------------------------------------------------
# The render stage is the only scale-dependent part of the agent stack: L=64
# works with Volume rendering, L=256 REQUIRES a Contour (see gs_profiles.py for
# the measurements). Selected with --profile, or inferred from the domain extent
# when not given. Env vars still win, so a one-off sweep needs no code edit.
PROFILE_NAME = os.environ.get('INSITU_PROFILE')      # may be None -> infer
PROFILE = None                                       # dict, set in main()

# Isovalue for the V contour (field range ~[0, 0.74]; V*~0.592 at collapse).
CONTOUR_ISOVALUE = 0.3
# Isovalues probed once at setup so a bad default is diagnosed in the SAME run
# rather than costing another allocation.
CONTOUR_PROBES = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
# "volume" or "contour" -- what setup_initial_display leaves visible.
RENDER_MODE = "contour"
OPACITY_DIVISOR = 64.0
# Set by setup_initial_display so the render loop can log the surface size.
CONTOUR_SOURCE = None


def apply_profile(name, extent=None):
    """
    Resolve and install the scale profile into the module globals. Env vars
    override the profile so a single knob can be swept without editing code.
    """
    import gs_profiles
    key = name or gs_profiles.profile_for_extent(extent)
    prof = gs_profiles.get_profile(key)

    g = globals()
    g["PROFILE"] = prof
    g["PROFILE_NAME"] = key
    g["RENDER_MODE"] = os.environ.get("INSITU_RENDER_MODE", prof["render_mode"])
    g["CONTOUR_ISOVALUE"] = float(
        os.environ.get("INSITU_CONTOUR_ISOVALUE", prof["contour_isovalue"]))
    probes_env = os.environ.get("INSITU_CONTOUR_PROBES")
    g["CONTOUR_PROBES"] = ([float(x) for x in probes_env.split(",") if x.strip()]
                           if probes_env else list(prof["contour_probes"]))
    g["OPACITY_DIVISOR"] = float(
        os.environ.get("INSITU_OPACITY_DIVISOR", prof["opacity_divisor"]))

    print(f"[insitu_streaming] profile '{key}': {prof['name']}")
    print(f"[insitu_streaming]   render_mode={g['RENDER_MODE']} "
          f"isovalue={g['CONTOUR_ISOVALUE']:g} probes={g['CONTOUR_PROBES']}")
    return prof

# Global streaming state — the MCP server reads/writes this
streaming_state = StreamingState()


def array_range(source, name="V"):
    """
    Return (min, max) of a point array straight from the server's data
    information. This is the scalar ground truth for "did the data actually
    change this step?" -- pixels cannot distinguish a cached render from a
    genuinely unchanged field, but these numbers can.
    """
    try:
        info = source.GetPointDataInformation().GetArray(name)
        if info is None:
            return None
        return info.GetComponentRange(0)
    except Exception:
        return None


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

        # --- L-portable volume opacity (fixed 2026-08-07) -------------------
        # gs-fides.json hardcodes spacing=[0.1,0.1,0.1] regardless of L, so the
        # PHYSICAL domain is L*0.1 units: 6.4 at L=64 but 25.6 at L=256. Volume
        # rendering accumulates opacity along the ray in physical distance, so
        # with a fixed ScalarOpacityUnitDistance the L=256 ray saturates inside
        # the outer shell and NO interior structure reaches the image -- every
        # frame renders as an identical featureless blob and the agent cannot
        # judge collapse at all (measured: L=64 window shows labyrinth creases
        # dissolving; L=256 window is 4 indistinguishable blobs).
        # Scale the unit distance with the domain so optical depth across the
        # volume is L-INVARIANT. The /64.0 reproduces the known-good L=64 look
        # (6.4/64 = 0.1 = one cell) at any L; at L=256 it gives 0.4 = 4 cells.
        try:
            b = fides.GetDataInformation().GetBounds()   # (xmin,xmax,ymin,...)
            extent = max(b[1] - b[0], b[3] - b[2], b[5] - b[4])
            # Late profile resolution: if no --profile was given we can now infer
            # the scale from the real domain size (extent = L * 0.1).
            if PROFILE is None:
                apply_profile(PROFILE_NAME, extent)
            if extent > 0:
                soud = extent / OPACITY_DIVISOR
                display.ScalarOpacityUnitDistance = soud
                print(f"[insitu_streaming] Volume opacity: domain extent={extent:.3f} "
                      f"-> ScalarOpacityUnitDistance={soud:.4f} (L-portable)")
            else:
                print("[insitu_streaming] WARN: zero domain extent; leaving default opacity")
        except Exception as e:
            print(f"[insitu_streaming] WARN: could not set ScalarOpacityUnitDistance ({e})")

        print("[insitu_streaming] Using Volume rendering of V")
    except Exception as e:
        print(f"[insitu_streaming] WARN: Volume rendering unavailable ({e}); keeping Outline")

    # --- Isosurface of V (added 2026-08-07) --------------------------------
    # Volume rendering CANNOT show the collapse at scale: it integrates opacity
    # along the ray, so a thin feature's contrast scales with its fraction of
    # the path (a 2-cell crease is 1/32 of the path at L=64 but 1/128 at L=256
    # -- a 4x contrast loss no opacity setting recovers; measured, the L=256
    # window rendered as 4 indistinguishable blobs while L=64 showed creases
    # dissolving). A Contour extracts GEOMETRY at a fixed V value instead, so it
    # is resolution-independent: as the field homogenises toward V*~0.592 the
    # V=0.3 surface shrinks and vanishes. That is the signal the agent is asked
    # to judge -- and, despite every prior verdict saying "the V isosurface
    # vanished", no isosurface existed in this pipeline until now.
    # Falls back to the volume display on any error.
    iso_display = None
    if RENDER_MODE != "contour":
        # L=64 profile: Volume rendering is verified to show the collapse at
        # this scale (creases in frame 1 dissolve by frame 4), so keep the
        # original published visualization and skip the contour entirely.
        print(f"[insitu_streaming] render_mode={RENDER_MODE}: keeping Volume "
              f"(verified at L=64; NOT usable at L=256 -- see gs_profiles.py)")
        SetActiveSource(fides)
        return display
    try:
        # Set properties AFTER construction. Passing them as constructor kwargs
        # (esp. PointMergeMethod="Don't Merge Points") blew up inside
        # paraview.simple's property setter with "cannot access local variable
        # 'new_value'" on 2026-08-07 -- the enum string did not match the
        # property's domain and the setter fell through without binding.
        contour = Contour(Input=fides)
        contour.ContourBy = ["POINTS", "V"]

        # ISOVALUE SWEEP, once, on the first flagged step. Picking an isovalue
        # blind costs a whole allocation to discover it was empty or saturated,
        # so probe several here and print the surface size at each. A usable
        # isovalue is one with a LARGE BUT NOT DEGENERATE point count; 0 means
        # the whole field is on one side of it (nothing to see).
        for probe in CONTOUR_PROBES:
            try:
                contour.Isosurfaces = [probe]
                contour.UpdatePipeline()
                n = contour.GetDataInformation().GetNumberOfPoints()
                print(f"[insitu_streaming] isovalue probe V={probe:.2f} -> {n} surface points")
            except Exception as e:
                print(f"[insitu_streaming] isovalue probe V={probe:.2f} failed: {e}")

        contour.Isosurfaces = [CONTOUR_ISOVALUE]
        contour.UpdatePipeline()
        globals()["CONTOUR_SOURCE"] = contour   # so the loop can log its size
        iso_display = Show(contour, view, "GeometryRepresentation")
        iso_display.SetRepresentationType("Surface")
        ColorBy(iso_display, ("POINTS", "V"))
        # Hide the volume so the (opaque) blob does not occlude the isosurface.
        display.Visibility = 0
        view.ResetCamera()
        print(f"[insitu_streaming] Isosurface of V at {CONTOUR_ISOVALUE} "
              f"(volume hidden); this is the collapse signal")
    except Exception as e:
        print(f"[insitu_streaming] WARN: contour unavailable ({e}); keeping Volume")
        try:
            display.Visibility = 1
        except Exception:
            pass
        iso_display = None

    SetActiveSource(fides)
    return iso_display if iso_display is not None else display


def prewarm_render(view):
    """
    Absorb the one-time VTK/OpenGL volume-render warm-up (~20 s on first real
    frame) BEFORE any flagged data arrives, by volume-rendering a throwaway
    Wavelet source once. In async snapshot mode the writer only waits while the
    bridge is draining the flagged window, so paying this cost up front (off the
    critical path) keeps the per-flagged-step drain fast (~1 s render each).
    """
    try:
        # Mirror setup_initial_display's exact path (uniform-grid Volume + a
        # colour LUT) on a throwaway Wavelet so the one-time volume shader/LUT
        # compilation is paid here, not on the first real flagged frame. A
        # non-trivial extent warms the volume texture path too.
        w = Wavelet()
        w.WholeExtent = [0, 63, 0, 63, 0, 63]
        d = Show(w, view, "UniformGridRepresentation")
        try:
            d.SetRepresentationType("Volume")
            ColorBy(d, ("POINTS", "RTData"))
            lut = GetColorTransferFunction("RTData")
            lut.AutomaticRescaleRangeMode = "Clamp and update every timestep"
            d.RescaleTransferFunctionToDataRange(True, False)
        except Exception:
            d.SetRepresentationType("Surface")
        view.ResetCamera()
        Render(view)
        Hide(w, view)
        Delete(w)
        del w
        print("[insitu_streaming] Render pipeline pre-warmed")
    except Exception as e:
        print(f"[insitu_streaming] WARN: prewarm failed (non-fatal): {e}")


def _save_frame(frames_dir, step, view, vr, screenshot_file):
    """
    Async snapshot: persist this flagged step as its own frame_<step>.png plus a
    line in frames.jsonl, so the AI agent can inspect the whole flagged window
    from disk WITHOUT driving the SST stream (the simulation never waits on the
    agent). Copies the bridge screenshot when available (no extra server render);
    otherwise renders the frame directly.
    """
    try:
        frame_png = os.path.join(frames_dir, f"frame_{step:04d}.png")
        if screenshot_file and os.path.exists(screenshot_file):
            import shutil
            shutil.copyfile(screenshot_file, frame_png)
        else:
            tmp = frame_png + ".tmp.png"
            SaveScreenshot(tmp, view)
            os.replace(tmp, frame_png)
        record = {
            "step": step,
            "png": frame_png,
            "v_min": round(vr[0], 6) if vr else None,
            "v_max": round(vr[1], 6) if vr else None,
            "timestamp": time.time(),
        }
        with open(os.path.join(frames_dir, "frames.jsonl"), "a") as f:
            json.dump(record, f)
            f.write("\n")
        print(f"[insitu_streaming] Snapshot -> {frame_png} "
              f"V=[{record['v_min']},{record['v_max']}]")
    except Exception as e:
        print(f"[insitu_streaming] WARN: failed to save frame {step}: {e}")


def _debug_view_state(view, tag, fides=None):
    """Dump per-representation state (gated by INSITU_DEBUG_VIEW=1)."""
    if os.environ.get("INSITU_DEBUG_VIEW") != "1":
        return
    try:
        if fides is not None:
            print(f"[insitu_streaming] DEBUG {tag}: fides npts="
                  f"{fides.GetDataInformation().GetNumberOfPoints()}")
        lut = GetColorTransferFunction("V")
        pts = list(lut.RGBPoints)
        pwf = GetOpacityTransferFunction("V")
        print(f"[insitu_streaming] DEBUG {tag}: campos="
              f"{[round(x, 3) for x in view.CameraPosition]} focal="
              f"{[round(x, 3) for x in view.CameraFocalPoint]} nreps="
              f"{len(view.Representations)}")
        print(f"[insitu_streaming] DEBUG {tag}: LUT range "
              f"[{pts[0]:.4g},{pts[-4]:.4g}] PWF={list(pwf.Points)}")
        for rep in view.Representations:
            try:
                inp = rep.Input
                iname = inp.GetXMLLabel() if hasattr(inp, "GetXMLLabel") else "?"
                print(f"[insitu_streaming] DEBUG {tag}: rep input={iname} "
                      f"vis={rep.Visibility} "
                      f"type={getattr(rep, 'Representation', '?')} "
                      f"color={list(getattr(rep, 'ColorArrayName', []))}")
            except Exception as e:
                print(f"[insitu_streaming] DEBUG {tag}: rep ? ({e})")
    except Exception as e:
        print(f"[insitu_streaming] DEBUG {tag}: dump failed: {e}")


def _reassert_view_state(fides, view, display):
    """
    Re-assert the bridge's visualization state before rendering
    (defense-in-depth against other collaboration clients).

    NOTE: the actual fix for the "empty screenshot" bug lives in
    insitu_mcp_server.py (_isolate_mcp_view): when another client
    Show()s a filter into THIS view, every subsequent bridge render
    comes out empty server-side, and this batch client cannot repair it
    — it never processes collaboration sync, so the foreign
    representation isn't even visible in view.Representations here
    (verified: camera restore, LUT restore, and rep-hiding all failed
    to fix it). What this function CAN protect against: another client
    pushing its default camera into the shared view (ResetCamera is an
    RPC that always executes server-side and refits to the data), a
    hidden volume display, and shared 'V' transfer functions rescaled
    to a degenerate range.
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
                        print("[insitu_streaming] WARN: hid foreign "
                              "representation in bridge view (filters from "
                              "other clients render in their own session)")
                except Exception:
                    pass
        except Exception:
            pass
        if not display.Visibility:
            print("[insitu_streaming] WARN: volume display was hidden by "
                  "another client; re-showing")
            display.Visibility = 1
        try:
            rmin, rmax = fides.PointData["V"].GetRange(0)
        except Exception:
            return
        if rmax > rmin:
            lut = GetColorTransferFunction("V")
            lut.RescaleTransferFunction(rmin, rmax)
            pwf = GetOpacityTransferFunction("V")
            pwf.RescaleTransferFunction(rmin, rmax)
    except Exception as e:
        print(f"[insitu_streaming] WARN: view-state reassert failed: {e}")


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
    frames_dir = getattr(args, 'frames_dir', None)
    if frames_dir:
        os.makedirs(frames_dir, exist_ok=True)
        # Start each run with a clean manifest so stale frames from a prior run
        # can never be read as this run's flagged window.
        try:
            os.remove(os.path.join(frames_dir, "frames.jsonl"))
        except OSError:
            pass

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

    # Publish the status file (pipeline_ready=false) BEFORE the Fides setup:
    # on a trigger-gated stream setup_fides_reader blocks in
    # UpdatePipelineInformation until the engine ships the first flagged step,
    # which can be minutes away. Without a status file the MCP server's
    # advance_step returns "status unavailable" immediately instead of
    # blocking for the window, and the agent gives up before the WARN fires
    # (hit 2026-07-15 on the 64-rank run).
    state.write_status()

    fides = setup_fides_reader(args.json_filename, args.bp_filename, args.staging)
    view = setup_render_view()

    # In async snapshot mode, warm the render pipeline now (before the gated
    # window arrives) so draining the flagged window stays fast and the writer
    # stalls only briefly.
    if frames_dir:
        prewarm_render(view)

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
        vr = array_range(fides, "V")
        vr_txt = f" V=[{vr[0]:.6f},{vr[1]:.6f}]" if vr else " V=[n/a]"
        # Surface size of the V isosurface: the QUANTITATIVE collapse signal.
        # "The isosurface vanished" has been asserted by every agent verdict
        # since July but never measured; as the field homogenises toward
        # V*~0.592 this count should fall monotonically toward 0. Unlike pixels,
        # it cannot be confabulated.
        iso_txt = ""
        if CONTOUR_SOURCE is not None:
            try:
                iso_txt = f" iso@{CONTOUR_ISOVALUE:g}={CONTOUR_SOURCE.GetDataInformation().GetNumberOfPoints()}pts"
            except Exception:
                iso_txt = " iso=[n/a]"
        print(f"[insitu_streaming] Step {state.step} {tag}{vr_txt}{iso_txt} "
              f"(sst={t_sst_end - t_sst_start:.3f}s "
              f"pipeline={t_pipeline_end - t_pipeline_start:.3f}s "
              f"render={t_render_end - t_render_start:.3f}s)")

        # Async snapshot: persist this flagged step to its own frame + manifest
        # so the agent can inspect the window from disk (never pacing the sim).
        if frames_dir and should_render:
            _save_frame(frames_dir, state.step, view, vr, screenshot_file)

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
    parser.add_argument(
        "--frames-dir",
        help="Async snapshot mode: directory to persist each flagged step as "
             "frame_<step>.png plus a frames.jsonl manifest (step, V min/max). "
             "Run WITHOUT --paused so the bridge greedily drains the flagged "
             "window to disk; the AI agent then inspects the frames from disk "
             "via the MCP get_flagged_frames tool without pacing the simulation.",
        type=str, default=None,
    )
    parser.add_argument(
        "--profile",
        help="Scale profile selecting the render configuration: 'l64' (Volume "
             "rendering -- the original published config, verified to show the "
             "collapse at L=64) or 'l256' (Contour -- REQUIRED at L=256, where "
             "Volume renders 4 identical featureless blobs). Omit to infer from "
             "the domain extent. See gs_profiles.py for the measurements.",
        type=str, default=None, choices=["l64", "l256"],
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # Resolve the scale profile. If --profile was omitted we defer to
    # setup_initial_display, which infers it from the real domain extent.
    if args.profile or PROFILE_NAME:
        apply_profile(args.profile or PROFILE_NAME)

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

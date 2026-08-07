"""
Per-scale visualization profiles for the Gray-Scott trigger-render-reason
pipeline.

WHY THIS FILE EXISTS
--------------------
The RENDER stage is the only part of the AI-agent stack that is scale-dependent.
`insitu_mcp_server.py` and `insitu_agent.py` contain nothing L-specific at all
(they are generic ParaView / LLM tooling), and inside `insitu_streaming.py` the
L-dependence is confined to the handful of knobs below. Forking the whole
~2400-line stack per scale would mean applying every future bug fix twice, so
the two "versions" are these two profiles plus the two thin entry points
`run_gs_l64.py` / `run_gs_l256.py`.

WHY THE TWO SCALES NEED DIFFERENT RENDERS (measured on Delta, 2026-08-06/07)
---------------------------------------------------------------------------
Volume rendering INTEGRATES opacity along the ray, so a thin feature's contrast
scales with its fraction of the path length. A 2-cell-wide labyrinth crease is
1/32 of the path at L=64 but only 1/128 at L=256 -- a 4x contrast loss that no
opacity setting recovers, because opacity scales the whole ray uniformly rather
than the feature-to-background ratio.

Consequently:
  * L=64  -- Volume rendering WORKS. Measured: the warn window shows labyrinth
    creases in frame 1 that are nearly gone by frame 4, i.e. the collapse is
    visible. This is the original, published configuration.
  * L=256 -- Volume rendering FAILS. Measured: all four frames of the warn
    window render as indistinguishable featureless blobs, and the agent
    (correctly) reports "no structure visible". An L-portable
    ScalarOpacityUnitDistance was added and did NOT fix it. A Contour extracts
    GEOMETRY at a fixed V value instead, so it is resolution-independent.
    Measured with the contour, the V=0.3 isosurface size across the window:
        step 1: 10825 pts -> 7542 -> 5091 -> step 4: 3168 pts   (-71%, monotonic)
    That is the collapse, quantified -- the signal every agent verdict since
    July asserted ("the V isosurface vanished") but which was never measured,
    because until 2026-08-07 no isosurface existed in this pipeline at all.

ISOVALUE
--------
0.3 is the verified working isovalue at L=256. The sweep printed at setup makes
a bad choice self-diagnosing in the SAME run rather than costing an allocation:
    V=0.10 9891 | 0.20 10408 | 0.30 10825 | 0.40 11019 | 0.50 11351
    V=0.60 12,166,473    <- 1000x jump
The discontinuity between 0.50 and 0.60 independently confirms the analytic
Gray-Scott steady state V* ~ 0.592: the bulk sits just below 0.6, so a 0.60
surface slices the entire domain. Keep probe isovalues BELOW that cliff.
"""

# --- Profiles ---------------------------------------------------------------
# render_mode:      "volume" | "contour"  -- what setup_initial_display builds
# contour_isovalue: V value for the isosurface (render_mode="contour")
# contour_probes:   isovalues sized once at setup and logged, to validate the
#                   choice inside the same run
# opacity_divisor:  ScalarOpacityUnitDistance = domain_extent / divisor. This is
#                   L-portable on its own (it scales with the domain, which
#                   grows with L because gs-fides.json hardcodes spacing=0.1),
#                   so both profiles share it. Kept per-profile only so a future
#                   scale can override without touching code.

PROFILES = {
    "l64": {
        "name": "L=64 (original, published configuration)",
        "render_mode": "volume",
        "contour_isovalue": 0.3,
        "contour_probes": [0.1, 0.2, 0.3, 0.4, 0.5],
        "opacity_divisor": 64.0,
        "notes": (
            "Volume rendering is VERIFIED to show the collapse at this scale "
            "(creases in frame 1 dissolve by frame 4). Warn lands at output ~24. "
            "Set render_mode='contour' if you want the quantitative iso-point "
            "count here too -- untested at L=64, but strictly more informative."
        ),
    },
    "l256": {
        "name": "L=256 (scale configuration)",
        "render_mode": "contour",
        "contour_isovalue": 0.3,
        "contour_probes": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6],
        "opacity_divisor": 64.0,
        "notes": (
            "Volume rendering CANNOT resolve the collapse at this scale -- it "
            "produces 4 identical featureless blobs. Contour is REQUIRED. Warn "
            "lands at output ~125 (~15 min at 256 ranks). Watch the per-step "
            "'iso@0.3=<N>pts' line: it must fall monotonically."
        ),
    },
}

DEFAULT_PROFILE = "l256"


def get_profile(name):
    """Look up a profile by name, with a helpful error listing valid names."""
    key = (name or DEFAULT_PROFILE).strip().lower()
    if key not in PROFILES:
        raise SystemExit(
            f"[gs_profiles] unknown profile '{name}'. "
            f"Valid: {', '.join(sorted(PROFILES))}"
        )
    return PROFILES[key]


def profile_for_extent(extent):
    """
    Infer the profile from the domain's physical extent, for callers that did
    not pass one explicitly. gs-fides.json hardcodes spacing=[0.1,0.1,0.1], so
    extent = L * 0.1: 6.4 at L=64, 25.5 at L=256. Anything appreciably larger
    than L=64's domain needs the contour, so the threshold sits between them.
    """
    try:
        return "l64" if float(extent) < 12.8 else "l256"
    except (TypeError, ValueError):
        return DEFAULT_PROFILE

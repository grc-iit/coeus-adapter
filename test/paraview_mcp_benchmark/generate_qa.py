#!/usr/bin/env python3
"""Generate Q&A benchmark dataset from scientific figure captions and descriptions."""

import html
import json
import os
import re
import glob
import random
from pathlib import Path
from collections import defaultdict

random.seed(42)

BASE = Path("/home/ubuntu/Desktop/paper_collection")
OUT = BASE / "benchmark"

# ── Domain mapping by folder-name prefix ──────────────────────────────────────
DOMAIN_RULES = [
    (r"^paraview", "scientific-visualization"),
    (r"^scivis", "scientific-visualization"),
    (r"^insitu", "scientific-visualization"),
    (r"^dns", "cfd"),
    (r"^les-", "cfd"),
    (r"^cfd", "cfd"),
    (r"^wind-turbine", "cfd"),
    (r"^rayleigh-benard", "cfd"),
    (r"^vortex", "cfd"),
    (r"^turbulence", "cfd"),
    (r"^sph-", "cfd"),
    (r"^spherical-convection", "cfd"),
    (r"^phase-field", "materials-science"),
    (r"^multi-physics", "materials-science"),
    (r"^jax-cpfem", "materials-science"),
    (r"^illustris", "astrophysics"),
    (r"^tng50", "astrophysics"),
    (r"^starforge", "astrophysics"),
    (r"^supernova", "astrophysics"),
    (r"^dark-matter", "astrophysics"),
    (r"^cosmological", "astrophysics"),
    (r"^athena", "astrophysics"),
    (r"^glad-m25", "geoscience"),
    (r"^e3sm", "geoscience"),
    (r"^vapor", "geoscience"),
    (r"^hurricane", "geoscience"),
    (r"^weather", "geoscience"),
    (r"^meteorology", "geoscience"),
    (r"^climate", "ml-simulation"),
    (r"^mesh-based", "ml-simulation"),
    (r"^neural-operator", "ml-simulation"),
    (r"^pinn", "ml-simulation"),
    (r"^particle-simulation", "ml-simulation"),
    (r"^tfz", "data-compression"),
    (r"^uncertainty-viz", "medical-visualization"),
    (r"^quantum", "quantum-physics"),
    (r"^spectral", "numerical-methods"),
    (r"^lattice-boltzmann", "cfd"),
    (r"^lattice", "particle-physics"),
    (r"^flow-viz", "cfd"),
    (r"^ieee-scivis", "scientific-visualization"),
    (r"^scientific-viz", "scientific-visualization"),
    (r"^energies", "cfd"),
    (r"^astro", "astrophysics"),
    (r"^ocean", "geoscience"),
    (r"^compress", "data-compression"),
    (r"^md-simulation", "ml-simulation"),
    (r"^ml-", "ml-simulation"),
    (r"^nerf", "scientific-visualization"),
    (r"^sph-", "cfd"),
    (r"^topo", "scientific-visualization"),
    # Batch 2 domains
    (r"^additive-mfg", "materials-science"),
    (r"^am-defect", "materials-science"),
    (r"^metal-am", "materials-science"),
    (r"^laser-powder", "materials-science"),
    (r"^crystal-structure", "materials-science"),
    (r"^grain-boundary", "materials-science"),
    (r"^material-micro", "materials-science"),
    (r"^microstructure", "materials-science"),
    (r"^ct-scan", "medical-visualization"),
    (r"^medical-", "medical-visualization"),
    (r"^mri-", "medical-visualization"),
    (r"^earthquake", "geoscience"),
    (r"^seismic", "geoscience"),
    (r"^seismology", "geoscience"),
    (r"^fem-", "numerical-methods"),
    (r"^neural-fem", "numerical-methods"),
    (r"^structural-mechanics", "numerical-methods"),
    (r"^fire-dynamics", "ml-simulation"),
    (r"^wildfire", "ml-simulation"),
    (r"^smoke-plume", "ml-simulation"),
    (r"^gnn-mesh", "ml-simulation"),
    (r"^graph-neural", "ml-simulation"),
    (r"^physics-mesh", "ml-simulation"),
    (r"^lattice-boltzmann", "cfd"),
    (r"^lb-fluid", "cfd"),
    (r"^multiphase-flow", "cfd"),
    (r"^porous-media", "cfd"),
    (r"^molecular-dynamics", "ml-simulation"),
    (r"^protein-folding", "ml-simulation"),
    (r"^plasma-", "plasma-physics"),
    (r"^tokamak", "plasma-physics"),
    (r"^nebula", "astrophysics"),
    (r"^stellar", "astrophysics"),
]


def classify_domain(folder_name):
    for pattern, domain in DOMAIN_RULES:
        if re.match(pattern, folder_name):
            return domain
    return "other"


# ── Parse figureX.md ──────────────────────────────────────────────────────────

def parse_figure_md(md_path):
    """Parse a figureX.md into caption and descriptions."""
    with open(md_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    caption = ""
    descriptions = ""

    cap_match = re.search(r"## Caption\s*\n\n(.+?)(?:\n## |\Z)", text, re.DOTALL)
    if cap_match:
        caption = cap_match.group(1).strip()

    desc_match = re.search(r"## Descriptions from paper\s*\n(.+?)(?:\n## |\Z)", text, re.DOTALL)
    if desc_match:
        descriptions = desc_match.group(1).strip()

    # Decode HTML entities (e.g. &amp; → &, &lt; → <)
    caption = html.unescape(caption)
    descriptions = html.unescape(descriptions)

    return caption, descriptions


# ── Q&A Extractors ────────────────────────────────────────────────────────────

def extract_L1_perception(caption, descriptions, fig_num):
    """L1: Perception — axes, panels, colors, counts."""
    qas = []
    combined = caption + " " + descriptions

    # Subplot/panel detection
    panels = re.findall(r"\(([a-z])\)", caption)
    if panels:
        unique = sorted(set(panels))
        count = len(unique)
        labels = ", ".join(f"({p})" for p in unique)
        qas.append({
            "question": "How many panels or subplots does this figure contain?",
            "answer": f"The figure contains {count} panels: {labels}.",
            "category": "L1_perception",
            "difficulty": "easy",
        })

    # Axis detection
    for axis in ["x-axis", "y-axis", "x axis", "y axis"]:
        pattern = rf"{axis}\s*(?:represents?|shows?|is|denotes?|:)\s*(.+?)(?:[,;.\)]|$)"
        match = re.search(pattern, combined, re.IGNORECASE)
        if match:
            quantity = match.group(1).strip().rstrip(".,;)")
            clean_axis = axis.replace(" ", "-")
            qas.append({
                "question": f"What quantity is plotted on the {clean_axis}?",
                "answer": f"The {clean_axis} represents {quantity}.",
                "category": "L1_perception",
                "difficulty": "easy",
            })
            break  # one axis question is enough

    # Color/colormap detection
    color_match = re.search(
        r"(?:color(?:map|bar|ed|s)?|shading)\s+(?:represents?|indicates?|shows?|corresponds?\s+to|is\s+based\s+on|denotes?)\s+(.+?)(?:[.]|$)",
        combined, re.IGNORECASE
    )
    if color_match:
        qas.append({
            "question": "What does the color encoding represent in this figure?",
            "answer": f"The color represents {color_match.group(1).strip().rstrip('.')}.",
            "category": "L1_perception",
            "difficulty": "easy",
        })

    # Labeled entities (blue, red, gray, white, pink, etc. → meaning)
    color_label = re.search(
        r"(?:in\s+)?(red|blue|green|black|white|pink|gray|orange|yellow)\s+(?:represents?|indicates?|denotes?|corresponds?\s+to)\s+(.+?)(?:[,;.\)]|$)",
        combined, re.IGNORECASE
    )
    if color_label and not color_match:
        qas.append({
            "question": "What do the different colors represent in this figure?",
            "answer": f"The {color_label.group(1)} color represents {color_label.group(2).strip().rstrip('.,;)')}.",
            "category": "L1_perception",
            "difficulty": "easy",
        })

    return qas


def extract_L2_description(caption, fig_num):
    """L2: Description — what the figure shows."""
    if not caption:
        return []

    # Strip "Figure N:" or "Fig. N:" prefix for the answer
    answer = re.sub(r"^(?:Figure|Fig\.?)\s*\d+\s*[:.]\s*", "", caption).strip()
    if not answer:
        return []

    return [{
        "question": "Describe what this figure shows.",
        "answer": answer,
        "category": "L2_description",
        "difficulty": "easy",
    }]


def extract_L3_comparison(caption, descriptions, fig_num):
    """L3: Comparison — compare elements within the figure."""
    qas = []
    combined = caption + " " + descriptions

    # Method comparison
    comp_patterns = [
        r"((?:compared?\s+(?:to|with)|versus|vs\.?)\s+.+?)(?:[.]|$)",
        r"(\S+\s+(?:outperforms?|is\s+(?:better|worse|higher|lower)\s+than)\s+.+?)(?:[.]|$)",
    ]
    for pat in comp_patterns:
        match = re.search(pat, combined, re.IGNORECASE)
        if match:
            # Find the sentence containing the comparison
            sent = find_sentence(combined, match.start())
            qas.append({
                "question": "What comparison is being made in this figure?",
                "answer": sent,
                "category": "L3_comparison",
                "difficulty": "medium",
            })
            break

    # Panel comparison
    panels = re.findall(r"\(([a-z])\)", caption)
    unique_panels = sorted(set(panels))
    if len(unique_panels) >= 2:
        # Try to find what each panel shows
        panel_descs = {}
        for p in unique_panels[:4]:  # limit to first 4
            pat = rf"\({p}\)\s*[:.]?\s*(.+?)(?:\s*\([b-z]\)|\s*[.]|$)"
            m = re.search(pat, caption, re.IGNORECASE)
            if m:
                panel_descs[p] = m.group(1).strip().rstrip(".,;")

        if len(panel_descs) >= 2:
            first_two = list(panel_descs.items())[:2]
            answer = f"Panel ({first_two[0][0]}) shows {first_two[0][1]}, while panel ({first_two[1][0]}) shows {first_two[1][1]}."
            qas.append({
                "question": f"What is the difference between panel ({first_two[0][0]}) and panel ({first_two[1][0]})?",
                "answer": answer,
                "category": "L3_comparison",
                "difficulty": "medium",
            })

    return qas


def extract_L4_reasoning(caption, descriptions, fig_num):
    """L4: Reasoning — only strong causal/conclusion statements."""
    qas = []
    # Only use descriptions (not caption) for reasoning — stricter filter
    if not descriptions:
        return []

    # Only trigger on strong explicit conclusions
    strong_patterns = [
        r"we\s+(?:find|conclude|observe)\s+that\s+",
        r"this\s+(?:demonstrates?|confirms?|proves?|shows?)\s+that\s+",
        r"the\s+results?\s+(?:indicate|suggest|confirm)\s+that\s+",
    ]

    for pat in strong_patterns:
        match = re.search(pat, descriptions, re.IGNORECASE)
        if match:
            sent = find_sentence(descriptions, match.start())
            if 40 < len(sent) < 500:
                qas.append({
                    "question": "What conclusion can be drawn from the results shown in this figure?",
                    "answer": sent,
                    "category": "L4_reasoning",
                    "difficulty": "hard",
                })
                break

    return qas


# Domain-specific technical terms
DOMAIN_TERMS = {
    "Reynolds number": "a dimensionless quantity that characterizes the flow regime (laminar vs. turbulent)",
    "Mach number": "the ratio of flow velocity to the speed of sound",
    "eigenvector": "a vector that remains in the same direction after a linear transformation",
    "eigenvalue": "a scalar that indicates how much an eigenvector is stretched by a transformation",
    "degenerate point": "a point in a tensor field where eigenvalues or eigenvectors become undefined",
    "vortex": "a region of rotating fluid with circular or spiral streamlines",
    "vorticity": "a measure of local rotation in a fluid flow",
    "isosurface": "a 3D surface connecting all points of equal value in a scalar field",
    "isovolume": "a 3D region bounded by isosurfaces at specified values",
    "streamline": "a curve tangent to the velocity vector field at every point",
    "pathline": "the trajectory traced by a single fluid particle over time",
    "RMSE": "Root Mean Square Error, a measure of prediction accuracy",
    "PSNR": "Peak Signal-to-Noise Ratio, a metric for reconstruction quality",
    "PSD": "Power Spectral Density, showing signal power as a function of frequency",
    "LIC": "Line Integral Convolution, a texture-based flow visualization technique",
    "volume rendering": "a technique for visualizing 3D scalar fields by mapping data values to color and opacity",
    "transfer function": "a mapping from data values to visual properties like color and opacity",
    "phase field": "a computational method that uses continuous order parameters to model interfaces",
    "nucleation": "the initial process of forming a new phase from a parent phase",
    "epitaxial growth": "crystal growth that follows the orientation of an existing substrate",
    "columnar-to-equiaxed transition": "a change in grain morphology from elongated to roughly equal-sized grains",
    "finite element method": "a numerical technique that divides a domain into elements to solve PDEs",
    "crystal plasticity": "a framework modeling the deformation of crystalline materials via slip systems",
    "Rayleigh number": "a dimensionless number indicating the ratio of buoyancy to viscous forces in convection",
    "Nusselt number": "a dimensionless number representing the ratio of convective to conductive heat transfer",
    "eddy": "a circular current of fluid that forms at the boundary between opposing flows",
    "compression ratio": "the ratio of original data size to compressed data size",
    "tensor field": "a field assigning a tensor (matrix) to each point in space",
    "topology preservation": "maintaining the structural features (critical points, connectivity) during data processing",
    "in situ visualization": "visualization performed during simulation runtime, avoiding full data output",
    "AMR": "Adaptive Mesh Refinement, dynamically adjusting grid resolution where needed",
}


def _term_pattern(term):
    """Build regex for a domain term, using word boundaries for short/acronym terms."""
    if len(term) <= 4 and term.isupper():
        # Short acronym — require word boundaries
        return rf"\b{re.escape(term)}\b"
    return re.escape(term)


def extract_L5_domain(caption, descriptions, fig_num):
    """L5: Domain Knowledge — technical terms in context."""
    qas = []
    combined = caption + " " + descriptions

    for term, definition in DOMAIN_TERMS.items():
        pat = _term_pattern(term)
        if re.search(pat, combined, re.IGNORECASE):
            # Find sentence that uses the term
            sent = ""
            for s in split_sentences(combined):
                if re.search(pat, s, re.IGNORECASE):
                    sent = s
                    break

            qas.append({
                "question": f"What is {term} and how is it relevant to this figure?",
                "answer": f"{term} is {definition}. In this figure, {sent.strip()}" if sent else f"{term} is {definition}.",
                "category": "L5_domain",
                "difficulty": "hard",
            })
            if len(qas) >= 2:  # max 2 domain questions per figure
                break

    return qas


def extract_L6_spatial(caption, descriptions, fig_num):
    """L6: Spatial/Structural — visualization technique, spatial layout."""
    qas = []
    combined = caption + " " + descriptions

    # Visualization technique — use word boundaries to avoid false positives
    viz_techniques = [
        (r"\bvolume rendering\b", "Volume rendering is used, which maps 3D scalar data to color and opacity for direct visualization."),
        (r"\bstreamline", "Streamlines are used to visualize the flow field direction."),
        (r"\bpathline", "Pathlines are used to trace fluid particle trajectories over time."),
        (r"\bisosurface", "Isosurfaces are used to show 3D surfaces of constant value."),
        (r"\bLine Integral Convolution\b|\bLIC\s+(?:texture|image|visualization|method|technique)", "Line Integral Convolution (LIC) is used for texture-based flow visualization."),
        (r"\bcontour\s+(?:plot|line|map|level)", "Contour lines/plots are used to show regions of equal value."),
        (r"\b(?:2D|planar)?\s*slice\b", "A planar slice through the 3D data is shown."),
        (r"\bcross[- ]section", "A cross-sectional view of the data is displayed."),
        (r"\bglyph", "Glyphs are used to represent local data properties at discrete points."),
        (r"\bscatter\s*plot", "A scatter plot is used to show the relationship between variables."),
    ]

    for pattern, answer in viz_techniques:
        if re.search(pattern, combined, re.IGNORECASE):
            qas.append({
                "question": "What visualization technique is used in this figure?",
                "answer": answer,
                "category": "L6_spatial",
                "difficulty": "medium",
            })
            break

    # Spatial region/orientation
    spatial_patterns = [
        (r"(zoomed-in|zoomed in|magnified)\s+(?:view|region|area)\s+(?:of\s+)?(.+?)(?:[.]|$)",
         "What region is highlighted in the zoomed-in view?"),
        (r"(longitudinal|transverse|horizontal|vertical|axial|radial)\s+(slice|section|plane|view|cut)",
         "What is the orientation of the cross-section shown?"),
        (r"(top|bottom|left|right)\s*:\s*(.+?)(?:[.]|$)",
         "Describe the spatial layout of this figure."),
    ]

    for pat, question in spatial_patterns:
        match = re.search(pat, combined, re.IGNORECASE)
        if match:
            sent = find_sentence(combined, match.start())
            qas.append({
                "question": question,
                "answer": sent,
                "category": "L6_spatial",
                "difficulty": "medium",
            })
            break

    # 3D structure detection
    if not qas:
        d3_match = re.search(r"(3D|three-dimensional|3-D)\s+(view|rendering|visualization|reconstruction|model|structure|simulation)", combined, re.IGNORECASE)
        if d3_match:
            sent = find_sentence(combined, d3_match.start())
            qas.append({
                "question": "What 3D structure or feature is visualized in this figure?",
                "answer": sent,
                "category": "L6_spatial",
                "difficulty": "medium",
            })

    return qas


# ── Yes/No Questions ─────────────────────────────────────────────────────────

def extract_yesno(caption, descriptions, fig_num):
    """Generate yes/no questions from detectable figure properties."""
    qas = []
    combined = caption + " " + descriptions

    # Pool of candidate yes/no questions with (pattern, question_if_yes, question_if_no_candidate)
    candidates = []

    # Multi-panel?
    panels = re.findall(r"\(([a-z])\)", caption)
    has_panels = len(set(panels)) >= 2
    candidates.append({
        "question": "Does this figure contain multiple panels or subplots?",
        "answer": "Yes" if has_panels else "No",
        "correct": has_panels,
    })

    # 3D visualization?
    is_3d = bool(re.search(r"(?:3D|three-dimensional|3-D|volume rendering)", combined, re.IGNORECASE))
    candidates.append({
        "question": "Does this figure show a 3D visualization?",
        "answer": "Yes" if is_3d else "No",
        "correct": is_3d,
    })

    # Comparison between methods?
    is_comparison = bool(re.search(
        r"(?:compared?\s+(?:to|with)|versus|vs\.?|outperforms?|better\s+than|worse\s+than)",
        combined, re.IGNORECASE
    ))
    candidates.append({
        "question": "Does this figure compare the performance of different methods or models?",
        "answer": "Yes" if is_comparison else "No",
        "correct": is_comparison,
    })

    # Quantitative plot (axes, error, metric)?
    is_quant = bool(re.search(
        r"(?:x-axis|y-axis|plot|graph|error|RMSE|accuracy|metric|bar chart|histogram)",
        combined, re.IGNORECASE
    ))
    candidates.append({
        "question": "Does this figure contain a quantitative plot with axes or metrics?",
        "answer": "Yes" if is_quant else "No",
        "correct": is_quant,
    })

    # Colormap/colorbar?
    has_colormap = bool(re.search(
        r"(?:color\s*(?:map|bar)|colormap|colorbar|color\s+coding|color\s+scale)",
        combined, re.IGNORECASE
    ))
    candidates.append({
        "question": "Does this figure use a colormap or colorbar?",
        "answer": "Yes" if has_colormap else "No",
        "correct": has_colormap,
    })

    # Simulation snapshot?
    is_snapshot = bool(re.search(
        r"(?:snapshot|time\s*step|timestep|frame|at\s+t\s*=|at\s+time)",
        combined, re.IGNORECASE
    ))
    candidates.append({
        "question": "Does this figure show a snapshot from a simulation at a specific time?",
        "answer": "Yes" if is_snapshot else "No",
        "correct": is_snapshot,
    })

    # Select 2 yes/no questions: prefer one Yes and one No for balance
    yes_qs = [c for c in candidates if c["correct"]]
    no_qs = [c for c in candidates if not c["correct"]]
    selected = []
    if yes_qs:
        selected.append(random.choice(yes_qs))
    if no_qs:
        selected.append(random.choice(no_qs))
    if len(selected) < 2 and yes_qs:
        remaining = [c for c in yes_qs if c not in selected]
        if remaining:
            selected.append(random.choice(remaining))
    if len(selected) < 2 and no_qs:
        remaining = [c for c in no_qs if c not in selected]
        if remaining:
            selected.append(random.choice(remaining))

    for s in selected:
        qas.append({
            "question": s["question"],
            "answer": s["answer"],
            "category": "L1_yesno",
            "difficulty": "easy",
            "format": "yes_no",
        })

    return qas


# ── Multiple Choice Questions ────────────────────────────────────────────────

VIZ_TECHNIQUES_MC = [
    "volume rendering", "streamlines", "isosurface", "contour plot",
    "scatter plot", "line plot", "bar chart", "heatmap",
    "pathlines", "vector field (arrows/glyphs)", "LIC (Line Integral Convolution)",
    "slice/cross-section", "surface mesh", "point cloud",
]

DOMAIN_LABELS_MC = [
    "computational fluid dynamics", "astrophysics", "materials science",
    "geoscience / ocean / climate", "machine learning for simulation",
    "scientific visualization", "data compression", "medical visualization",
    "numerical methods / structural mechanics", "plasma physics / fusion",
]


def extract_multichoice(caption, descriptions, fig_num, domain):
    """Generate multiple-choice questions."""
    qas = []
    combined = caption + " " + descriptions

    # ── Q1: Visualization technique ──────────────────────────────────────────
    detected_viz = None
    # (regex_pattern, label) — use word boundaries to avoid false positives
    viz_map = [
        (r"\bvolume rendering\b", "volume rendering"),
        (r"\bstreamline", "streamlines"),
        (r"\bpathline", "pathlines"),
        (r"\bisosurface", "isosurface"),
        (r"\bLine Integral Convolution\b|\bLIC\s+(?:texture|image|visualization|method|technique)", "LIC (Line Integral Convolution)"),
        (r"\bcontour\s+(?:plot|line|map|level)", "contour plot"),
        (r"\bscatter\s*plot", "scatter plot"),
        (r"\bglyph", "vector field (arrows/glyphs)"),
        (r"\barrow\s*(?:plot|field|glyph)", "vector field (arrows/glyphs)"),
        (r"\b(?:2D|planar)?\s*slice\b", "slice/cross-section"),
        (r"\bcross[- ]section", "slice/cross-section"),
        (r"\bheatmap", "heatmap"),
        (r"\bbar\s*chart", "bar chart"),
        (r"\bline\s*plot", "line plot"),
        (r"\bpoint\s*cloud", "point cloud"),
        (r"\bsurface\s*mesh", "surface mesh"),
    ]
    for pattern, label in viz_map:
        if re.search(pattern, combined, re.IGNORECASE):
            detected_viz = label
            break

    if detected_viz:
        distractors = [v for v in VIZ_TECHNIQUES_MC if v != detected_viz]
        chosen_distractors = random.sample(distractors, min(3, len(distractors)))
        options = [detected_viz] + chosen_distractors
        random.shuffle(options)
        correct_letter = chr(65 + options.index(detected_viz))  # A/B/C/D

        qas.append({
            "question": "What visualization technique is primarily used in this figure?",
            "options": {chr(65 + i): opt for i, opt in enumerate(options)},
            "answer": correct_letter,
            "answer_text": detected_viz,
            "category": "L2_multichoice",
            "difficulty": "medium",
            "format": "multiple_choice",
        })

    # ── Q2: Panel count ──────────────────────────────────────────────────────
    panels = re.findall(r"\(([a-z])\)", caption)
    unique_panels = sorted(set(panels))
    if unique_panels:
        true_count = len(unique_panels)
        # Build plausible options
        if true_count <= 2:
            count_options = [1, 2, 3, 4]
        elif true_count <= 4:
            count_options = [2, 3, 4, 6]
        else:
            count_options = [3, 4, true_count, true_count + 2]
        if true_count not in count_options:
            count_options[0] = true_count
        count_options = sorted(set(count_options))[:4]
        if true_count not in count_options:
            count_options[-1] = true_count
            count_options = sorted(count_options)

        str_options = [str(c) for c in count_options]
        correct_letter = chr(65 + count_options.index(true_count))

        qas.append({
            "question": "How many distinct panels or subplots are in this figure?",
            "options": {chr(65 + i): opt for i, opt in enumerate(str_options)},
            "answer": correct_letter,
            "answer_text": str(true_count),
            "category": "L1_multichoice",
            "difficulty": "easy",
            "format": "multiple_choice",
        })

    # ── Q3: Domain classification ────────────────────────────────────────────
    domain_to_label = {
        "cfd": "computational fluid dynamics",
        "astrophysics": "astrophysics",
        "materials-science": "materials science",
        "geoscience": "geoscience / ocean / climate",
        "ml-simulation": "machine learning for simulation",
        "scientific-visualization": "scientific visualization",
        "data-compression": "data compression",
        "medical-visualization": "medical visualization",
        "numerical-methods": "numerical methods / structural mechanics",
        "plasma-physics": "plasma physics / fusion",
        "quantum-physics": "quantum physics",
        "particle-physics": "particle physics",
    }
    true_label = domain_to_label.get(domain)
    if true_label:
        distractors = [d for d in DOMAIN_LABELS_MC if d != true_label]
        chosen_distractors = random.sample(distractors, min(3, len(distractors)))
        options = [true_label] + chosen_distractors
        random.shuffle(options)
        correct_letter = chr(65 + options.index(true_label))

        qas.append({
            "question": "Which scientific domain does this figure most likely belong to?",
            "options": {chr(65 + i): opt for i, opt in enumerate(options)},
            "answer": correct_letter,
            "answer_text": true_label,
            "category": "L2_multichoice",
            "difficulty": "medium",
            "format": "multiple_choice",
        })

    return qas


# ── Helpers ───────────────────────────────────────────────────────────────────

def split_sentences(text):
    """Simple sentence splitter."""
    # Split on period followed by space+capital, or newlines
    parts = re.split(r'(?<=[.!?])\s+(?=[A-Z])', text)
    result = []
    for p in parts:
        result.extend(p.split("\n"))
    return [s.strip() for s in result if s.strip()]


def find_sentence(text, pos):
    """Find the full sentence containing position `pos`."""
    # Go back to find sentence start
    start = max(0, text.rfind(". ", 0, pos) + 2)
    if start <= 2:
        start = max(0, text.rfind("\n", 0, pos) + 1)
    # Go forward to find sentence end
    end = text.find(". ", pos)
    if end == -1:
        end = len(text)
    else:
        end += 1
    return text[start:end].strip()


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    dataset = []
    stats = defaultdict(int)
    category_counts = defaultdict(int)
    domain_counts = defaultdict(int)
    difficulty_counts = defaultdict(int)

    # Scan all figure markdown files
    figure_mds = sorted(glob.glob(str(BASE / "*/img/figure*.md")))
    print(f"Found {len(figure_mds)} figure markdown files")

    for md_path in figure_mds:
        md_path = Path(md_path)
        fig_name = md_path.stem  # e.g. "figure2"
        fig_num = re.search(r"(\d+)", fig_name).group(1)
        folder = md_path.parent.parent.name  # paper folder
        png_path = md_path.with_suffix(".png")

        if not png_path.exists():
            stats["missing_png"] += 1
            continue

        caption, descriptions = parse_figure_md(md_path)

        # Skip figures with no caption
        if not caption:
            stats["no_caption"] += 1
            continue

        domain = classify_domain(folder)
        figure_id = f"{folder}/{fig_name}"

        # Generate Q&A pairs
        qa_pairs = []
        qa_pairs.extend(extract_yesno(caption, descriptions, fig_num))
        qa_pairs.extend(extract_multichoice(caption, descriptions, fig_num, domain))
        qa_pairs.extend(extract_L1_perception(caption, descriptions, fig_num))
        qa_pairs.extend(extract_L2_description(caption, fig_num))
        qa_pairs.extend(extract_L3_comparison(caption, descriptions, fig_num))
        qa_pairs.extend(extract_L4_reasoning(caption, descriptions, fig_num))
        qa_pairs.extend(extract_L5_domain(caption, descriptions, fig_num))
        qa_pairs.extend(extract_L6_spatial(caption, descriptions, fig_num))

        if not qa_pairs:
            stats["no_qa"] += 1
            continue

        # Assign IDs and track stats
        for i, qa in enumerate(qa_pairs):
            qa["id"] = f"{figure_id}/Q{i+1}"
            category_counts[qa["category"]] += 1
            difficulty_counts[qa["difficulty"]] += 1

        domain_counts[domain] += 1

        entry = {
            "figure_id": figure_id,
            "image_path": f"{folder}/img/{fig_name}.png",
            "paper_folder": folder,
            "domain": domain,
            "caption": caption,
            "qa_pairs": qa_pairs,
        }
        dataset.append(entry)

    # ── Write outputs ─────────────────────────────────────────────────────────

    # Main dataset
    qa_path = OUT / "qa_dataset.jsonl"
    with open(qa_path, "w") as f:
        for entry in dataset:
            f.write(json.dumps(entry, ensure_ascii=False) + "\n")

    # By-category splits
    cat_dir = OUT / "by_category"
    cat_dir.mkdir(exist_ok=True)
    cat_files = {}
    for entry in dataset:
        for qa in entry["qa_pairs"]:
            cat = qa["category"]
            if cat not in cat_files:
                cat_files[cat] = open(cat_dir / f"{cat}.jsonl", "w")
            cat_entry = {
                "figure_id": entry["figure_id"],
                "image_path": entry["image_path"],
                "domain": entry["domain"],
                **qa,
            }
            cat_files[cat].write(json.dumps(cat_entry, ensure_ascii=False) + "\n")
    for f in cat_files.values():
        f.close()

    # Metadata
    total_qa = sum(category_counts.values())
    avg_qa = total_qa / len(dataset) if dataset else 0
    metadata = {
        "total_figures": len(dataset),
        "total_qa_pairs": total_qa,
        "avg_qa_per_figure": round(avg_qa, 1),
        "categories": dict(sorted(category_counts.items())),
        "domains": dict(sorted(domain_counts.items())),
        "difficulties": dict(sorted(difficulty_counts.items())),
        "skipped": {
            "no_caption": stats["no_caption"],
            "no_qa_generated": stats["no_qa"],
            "missing_png": stats["missing_png"],
        },
    }
    with open(OUT / "metadata.json", "w") as f:
        json.dump(metadata, f, indent=2)

    # ── Report ────────────────────────────────────────────────────────────────
    print(f"\n{'='*60}")
    print(f"BENCHMARK GENERATED")
    print(f"{'='*60}")
    print(f"Figures with Q&A:  {len(dataset)}")
    print(f"Total Q&A pairs:   {total_qa}")
    print(f"Avg Q&A/figure:    {avg_qa:.1f}")
    print(f"\nCategory distribution:")
    for cat, count in sorted(category_counts.items()):
        print(f"  {cat:25s} {count:4d}  ({100*count/total_qa:.1f}%)")
    print(f"\nDomain distribution:")
    for dom, count in sorted(domain_counts.items()):
        print(f"  {dom:25s} {count:4d}")
    print(f"\nDifficulty distribution:")
    for diff, count in sorted(difficulty_counts.items()):
        print(f"  {diff:10s} {count:4d}")
    print(f"\nSkipped: {stats['no_caption']} no-caption, {stats['no_qa']} no-qa, {stats['missing_png']} missing-png")
    print(f"\nOutput files:")
    print(f"  {qa_path}")
    print(f"  {OUT / 'metadata.json'}")
    for cat in sorted(cat_files.keys()):
        print(f"  {cat_dir / f'{cat}.jsonl'}")


if __name__ == "__main__":
    main()

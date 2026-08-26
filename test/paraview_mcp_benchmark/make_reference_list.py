#!/usr/bin/env python3
"""Build the source-paper reference list for the benchmark artifact.

The benchmark ships figure images extracted from published papers. This script
emits the accompanying reference list so each figure can be traced back to (and
re-downloaded from) its original source.

    python make_reference_list.py

Writes references.csv and references.md next to the benchmark, covering only
the papers that actually contributed at least one figure to qa_dataset.jsonl.

Identifier extraction is best effort. A paper's own DOI is only recoverable
when the publisher stamped it into the front matter (MDPI and several other
publishers do; many do not), and the bibliography is full of *other* papers'
DOIs — so only the front matter is searched, and every row carries a
`needs_verification` flag. Rows flagged True need a human to fill in the
identifier before the list is published.
"""

import csv
import json
import re
from collections import Counter
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
QA_DATASET = BASE / "benchmark" / "qa_dataset.jsonl"

# Front matter ends at the first numbered section, the abstract's successor, or
# a references heading — whichever comes first.
BODY_START = re.compile(
    r"^#{1,3}\s*(\d+[\s.]|I\.\s|Introduction|1\s+Introduction|References|Bibliography)",
    re.IGNORECASE,
)

DOI_RE = re.compile(r"(?:doi\.org/|doi:\s*|DOI:\s*)(10\.\d{4,9}/[^\s)\]\"'<>,]+)", re.I)
ARXIV_RE = re.compile(r"arxiv[.:\s]*(?:org/abs/)?(\d{4}\.\d{4,5})(?:v\d+)?", re.I)


def figure_counts():
    """{paper_folder: n_figures} for folders present in the QA dataset."""
    counts = Counter()
    with open(QA_DATASET, encoding="utf-8") as f:
        for line in f:
            entry = json.loads(line)
            counts[entry["paper_folder"]] += 1
    return counts


def front_matter(path, max_lines=80):
    lines = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f):
            if i >= max_lines:
                break
            if lines and BODY_START.match(line.strip()):
                break
            lines.append(line.rstrip("\n"))
    return lines


def clean(text):
    text = re.sub(r"\s+", " ", text).strip()
    return text.strip("#* ")


def extract(folder):
    """Return (title, authors, doi, arxiv) from a paper's markdown front matter."""
    md = BASE / folder / f"{folder}.md"
    if not md.exists():
        return None

    lines = front_matter(md)
    blob = "\n".join(lines)

    title, authors = "", ""
    for i, raw in enumerate(lines):
        line = clean(raw)
        # The title is the first substantial heading; skip publisher furniture
        # such as a bare "Article" or "Abstract" line.
        if raw.strip().startswith("#") and len(line) > 15 and line.lower() != "abstract":
            title = line
            # Authors are the next non-empty, non-heading line that is not an
            # image embed or a caption.
            for follow in lines[i + 1 : i + 6]:
                candidate = clean(follow)
                if (
                    candidate
                    and not follow.strip().startswith(("#", "!["))
                    and not candidate.lower().startswith(("fig.", "figure", "abstract"))
                ):
                    authors = candidate
                    break
            break

    if not title:
        for raw in lines:
            line = clean(raw)
            if len(line) > 15:
                title = line
                break

    doi = DOI_RE.search(blob)
    arxiv = ARXIV_RE.search(blob)
    return (
        title[:300],
        authors[:300],
        doi.group(1).rstrip(".") if doi else "",
        arxiv.group(1) if arxiv else "",
    )


def main():
    counts = figure_counts()
    rows = []
    for folder in sorted(counts):
        parsed = extract(folder)
        if parsed is None:
            rows.append([folder, counts[folder], "", "", "", "", "True"])
            continue
        title, authors, doi, arxiv = parsed
        rows.append(
            [
                folder,
                counts[folder],
                title,
                authors,
                doi,
                arxiv,
                str(not (doi or arxiv)),
            ]
        )

    header = [
        "paper_folder",
        "n_figures",
        "title",
        "authors",
        "doi",
        "arxiv_id",
        "needs_verification",
    ]

    out_csv = BASE / "benchmark" / "references.csv"
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

    resolved = [r for r in rows if r[6] == "False"]
    unresolved = [r for r in rows if r[6] == "True"]

    out_md = BASE / "benchmark" / "references.md"
    with open(out_md, "w", encoding="utf-8") as f:
        f.write("# Source papers\n\n")
        f.write(
            f"{len(rows)} papers contributed the {sum(counts.values())} figures in "
            f"`qa_dataset.jsonl`. Identifiers below were extracted automatically "
            f"from each paper's front matter; **{len(unresolved)} entries still "
            f"need a DOI or arXiv id filled in by hand** and are listed "
            f"separately at the end.\n\n"
        )
        f.write("## Papers with a resolved identifier\n\n")
        f.write("| Folder | Figures | Title | Identifier |\n|---|---|---|---|\n")
        for folder, n, title, _authors, doi, arxiv, _ in resolved:
            ident = f"https://doi.org/{doi}" if doi else f"arXiv:{arxiv}"
            f.write(f"| `{folder}` | {n} | {title} | {ident} |\n")

        f.write("\n## Papers still needing an identifier\n\n")
        f.write("| Folder | Figures | Title |\n|---|---|---|\n")
        for folder, n, title, _authors, _doi, _arxiv, _ in unresolved:
            f.write(f"| `{folder}` | {n} | {title} |\n")

    print(f"[done] {len(rows)} papers, {sum(counts.values())} figures")
    print(f"[done] {len(resolved)} with an identifier, {len(unresolved)} needing one")
    print(f"[done] wrote {out_csv}")
    print(f"[done] wrote {out_md}")


if __name__ == "__main__":
    main()

# Project Ambrose by Imjustchico
# Reads the roadmap phases and the contributor track and writes the project's progress as a card anyone can read at a glance, the numbers behind it, and a badge endpoint, with a check mode that fails when any of them is out of date. None of them carries a commit or a date, because a generated file that names the commit it came from is stale the moment it is committed.

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROADMAP_DIR = os.path.join("doc", "roadmap")
TRACK = os.path.join("doc", "CONTRIBUTOR-TRACK.md")
VARIABLES = os.path.join("packages", "ui", "src", "tokens", "variables.css")
OUT_DIR = os.path.join("doc", "progress")
CARD = os.path.join(OUT_DIR, "progress.svg")
DATA = os.path.join(OUT_DIR, "progress.json")
BADGE = os.path.join(OUT_DIR, "badge.json")

def read(root, relative):
    with open(os.path.join(root, relative), "r", encoding="utf-8") as handle:
        return handle.read()


def colours(root):
    text = read(root, VARIABLES)
    dark = text.split(":root[data-theme=\"light\"]")[0]
    return dict(re.findall(r"--ambrose-color-([a-z0-9-]+):\s*(#[0-9A-Fa-f]{6})", dark))


def phase_files(root):
    folder = os.path.join(root, ROADMAP_DIR)
    return sorted(name for name in os.listdir(folder) if name.startswith("phase-") and name.endswith(".md"))


def phase_number(name):
    return int(name.split("-")[1])


def phase_title(name):
    words = name.split("-")[2:]
    words[-1] = words[-1].removesuffix(".md")
    title = " ".join(words)
    return title[:1].upper() + title[1:]


def measure(root):
    phases = []
    milestones_done = milestones_total = checks_done = checks_total = 0
    for name in phase_files(root):
        text = read(root, os.path.join(ROADMAP_DIR, name))
        sections = re.split(r"^## (\d+\.\d+) ", text, flags=re.M)
        done = total = phase_checks_done = phase_checks_total = 0
        for body in sections[2::2]:
            boxes = re.findall(r"^- \[([ x])\]", body, flags=re.M)
            if not boxes:
                continue
            total += 1
            phase_checks_total += len(boxes)
            ticked = sum(1 for box in boxes if box == "x")
            phase_checks_done += ticked
            if ticked == len(boxes):
                done += 1
        if total == 0:
            continue
        phases.append({
            "phase": phase_number(name),
            "title": phase_title(name),
            "milestones_done": done,
            "milestones_total": total,
            "checks_done": phase_checks_done,
            "checks_total": phase_checks_total,
        })
        milestones_done += done
        milestones_total += total
        checks_done += phase_checks_done
        checks_total += phase_checks_total

    track = read(root, TRACK)
    open_rows = len(re.findall(r"^\| [FC]-\d+ \|", track.split("### Merged so far")[0], flags=re.M))
    merged_rows = len(re.findall(r"^\| [FC]-\d+ \|", track.split("### Merged so far")[1], flags=re.M)) if "### Merged so far" in track else 0

    return {
        "schema": 1,
        "milestones": {"done": milestones_done, "total": milestones_total,
                       "percent": round(100 * milestones_done / milestones_total, 1)},
        "checks": {"done": checks_done, "total": checks_total,
                   "percent": round(100 * checks_done / checks_total, 1)},
        "contributor_track": {"merged": merged_rows, "open": open_rows},
        "phases": phases,
    }


def bar(x, y, width, height, fraction, track_colour, fill_colour, radius=None):
    radius = height / 2 if radius is None else radius
    filled = max(0.0, min(1.0, fraction)) * width
    parts = [f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="{radius}" fill="{track_colour}"/>']
    if filled > 0:
        parts.append(f'<rect x="{x}" y="{y}" width="{filled:.1f}" height="{height}" rx="{radius}" fill="{fill_colour}"/>')
    return "".join(parts)


def escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def card(data, palette):
    page = palette.get("surface-page", "#0B1020")
    panel = palette.get("surface-card", "#131B31")
    sunken = palette.get("surface-sunken", "#0E1527")
    gold = palette.get("action", "#E4B457")
    teal = palette.get("state-healthy", "#5FD3C4")
    body = palette.get("fg-body", "#F3E9D2")
    muted = palette.get("fg-muted", "#8798BC")
    edge = palette.get("edge-quiet", "#1B2540")

    milestones = data["milestones"]
    checks = data["checks"]
    track = data["contributor_track"]
    phases = data["phases"]

    rows = (len(phases) + 1) // 2
    width, height = 880, 250 + rows * 30 + 34
    serif = "'Cormorant Garamond', Georgia, 'Times New Roman', serif"
    sans = "Karla, 'Segoe UI', 'Helvetica Neue', Arial, sans-serif"
    mono = "'JetBrains Mono', Consolas, 'DejaVu Sans Mono', monospace"

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" '
           f'aria-label="Project Ambrose progress: {milestones["done"]} of {milestones["total"]} milestones complete, {milestones["percent"]} percent">',
           '<defs>',
           f'<linearGradient id="sky" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{page}"/><stop offset="1" stop-color="{panel}"/></linearGradient>',
           f'<linearGradient id="fill" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="{teal}"/><stop offset="1" stop-color="{gold}"/></linearGradient>',
           '</defs>',
           f'<rect width="{width}" height="{height}" rx="18" fill="url(#sky)"/>',
           f'<rect x="0.5" y="0.5" width="{width - 1}" height="{height - 1}" rx="18" fill="none" stroke="{edge}"/>',
           f'<text x="40" y="62" font-family="{serif}" font-size="34" font-weight="600" fill="{gold}">Project Ambrose</text>',
           f'<text x="40" y="88" font-family="{sans}" font-size="15" fill="{muted}">A Wizard101 server written from scratch, built in the open</text>',
           f'<text x="{width - 40}" y="62" text-anchor="end" font-family="{mono}" font-size="46" font-weight="700" fill="{body}">{milestones["percent"]:.1f}%</text>',
           f'<text x="{width - 40}" y="86" text-anchor="end" font-family="{sans}" font-size="14" fill="{muted}">of the plan built</text>',
           bar(40, 112, width - 80, 18, milestones["done"] / milestones["total"], sunken, "url(#fill)"),
           f'<text x="40" y="152" font-family="{sans}" font-size="14" fill="{body}">'
           f'<tspan font-family="{mono}" font-weight="700">{milestones["done"]}</tspan> of '
           f'<tspan font-family="{mono}">{milestones["total"]}</tspan> milestones finished</text>',
           f'<text x="{width - 40}" y="152" text-anchor="end" font-family="{sans}" font-size="14" fill="{muted}">'
           f'<tspan font-family="{mono}" fill="{teal}">{checks["done"]}</tspan> of '
           f'<tspan font-family="{mono}">{checks["total"]}</tspan> acceptance checks passed</text>']

    out.append(f'<text x="40" y="192" font-family="{sans}" font-size="13" fill="{muted}" letter-spacing="1.5">PHASES</text>')
    columns, column_width, row_height = 2, 404, 30
    for index, phase in enumerate(phases):
        column, row = index % columns, index // columns
        x = 40 + column * column_width
        y = 214 + row * row_height
        fraction = phase["milestones_done"] / phase["milestones_total"]
        label = f'{phase["phase"]:02d} {phase["title"]}'
        if len(label) > 30:
            label = label[:29] + "…"
        out.append(f'<text x="{x}" y="{y + 10}" font-family="{sans}" font-size="12.5" fill="{body if fraction else muted}">{escape(label)}</text>')
        out.append(bar(x + 246, y, 96, 8, fraction, sunken, gold if fraction < 1 else teal))
        out.append(f'<text x="{x + 352}" y="{y + 9}" font-family="{mono}" font-size="11.5" fill="{muted}">'
                   f'{phase["milestones_done"]}/{phase["milestones_total"]}</text>')

    footer = height - 26
    out.append(f'<text x="40" y="{footer}" font-family="{sans}" font-size="12" fill="{muted}">'
               f'Contributors: <tspan font-family="{mono}" fill="{teal}">{track["merged"]}</tspan> items merged, '
               f'<tspan font-family="{mono}">{track["open"]}</tspan> open</text>')
    out.append(f'<text x="{width - 40}" y="{footer}" text-anchor="end" font-family="{sans}" font-size="12" fill="{muted}">Counted from the roadmap itself</text>')
    out.append('</svg>')
    return "\n".join(out) + "\n"


def badge(data):
    percent = data["milestones"]["percent"]
    colour = "5FD3C4" if percent >= 50 else "E4B457"
    return json.dumps({
        "schemaVersion": 1,
        "label": "progress",
        "message": f'{percent}% · {data["milestones"]["done"]}/{data["milestones"]["total"]} milestones',
        "color": colour,
        "labelColor": "0B1020",
    }, indent=2) + "\n"


def outputs(root):
    data = measure(root)
    return {
        DATA: json.dumps(data, indent=2) + "\n",
        CARD: card(data, colours(root)),
        BADGE: badge(data),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose progress card generator")
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--check", action="store_true", help="fail when a written file is out of date")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    written = outputs(root)
    stale = []
    for relative, text in written.items():
        path = os.path.join(root, relative)
        current = None
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as handle:
                current = handle.read().replace("\r\n", "\n")
        if current == text:
            continue
        stale.append(relative)
        if args.check:
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
    if args.check:
        for relative in stale:
            print(f"{relative}: out of date; run apps/progress/progress.py")
        print(f"progress: {len(written)} files checked, {len(stale)} stale")
        return 1 if stale else 0
    for relative in stale:
        print(f"{relative}: written")
    print(f"progress: {len(written)} files generated, {len(stale)} changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

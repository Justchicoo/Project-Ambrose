# Project Ambrose by Imjustchico
# Reads the roadmap phases and reports which milestones are ready to be built now, meaning every milestone they depend on is finished and they are not, so outside help can be pointed at real work instead of guessing, marking each one from doc/MILESTONE-TRACK.md, where only the milestones that document opens are open and everything it does not name is reserved.

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROADMAP_DIR = os.path.join("doc", "roadmap")
TRACK = os.path.join("doc", "MILESTONE-TRACK.md")
HEADING = re.compile(r"^## (\d+\.\d+) (.+)$", re.M)
DEPENDS = re.compile(r"\*\*Depends on:\*\*\s*(.*)")
SIZE = re.compile(r"\*\*Size:\*\*\s*([A-Z]+)")
BOX = re.compile(r"^- \[([ x])\]", re.M)
IDENTIFIER = re.compile(r"\d+\.\d+")


def read(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def phase_files(root):
    folder = os.path.join(root, ROADMAP_DIR)
    return sorted(name for name in os.listdir(folder) if name.startswith("phase-") and name.endswith(".md"))


def milestones(root):
    found = {}
    for name in phase_files(root):
        text = read(os.path.join(root, ROADMAP_DIR, name))
        parts = HEADING.split(text)
        for identifier, title, body in zip(parts[1::3], parts[2::3], parts[3::3]):
            boxes = BOX.findall(body)
            if not boxes:
                continue
            depends = DEPENDS.search(body)
            size = SIZE.search(body)
            found[identifier] = {
                "id": identifier,
                "title": title.strip(),
                "phase": int(identifier.split(".")[0]),
                "file": os.path.join(ROADMAP_DIR, name).replace("\\", "/"),
                "size": size.group(1) if size else "?",
                "depends_on": IDENTIFIER.findall(depends.group(1)) if depends else [],
                "checks_total": len(boxes),
                "checks_done": sum(1 for box in boxes if box == "x"),
                "done": all(box == "x" for box in boxes),
            }
    return found


def sections(root):
    path = os.path.join(root, TRACK)
    if not os.path.exists(path):
        return None
    marks = {}
    current = None
    for line in read(path).splitlines():
        heading = re.match(r"^#+ +(.*)", line)
        if heading:
            current = heading.group(1).strip().lower()
        row = re.match(r"^\| *([\d. ,]+?) *\|", line)
        if not row or not current:
            continue
        if "open" in current:
            status = "open"
        elif "flight" in current:
            status = "claimed"
        elif "landed" in current:
            status = "landed"
        else:
            status = "reserved"
        for identifier in IDENTIFIER.findall(row.group(1)):
            marks[identifier] = status
    return marks


def state(root):
    found = milestones(root)
    marks = sections(root)
    ready, blocked = [], []
    for identifier in sorted(found, key=lambda value: (int(value.split(".")[0]), int(value.split(".")[1]))):
        milestone = found[identifier]
        if milestone["done"]:
            continue
        missing = [name for name in milestone["depends_on"] if name not in found or not found[name]["done"]]
        unknown = [name for name in milestone["depends_on"] if name not in found]
        entry = dict(milestone)
        entry["missing"] = missing
        entry["unknown"] = unknown
        entry["status"] = "unlisted" if marks is None else marks.get(identifier, "reserved")
        (blocked if missing else ready).append(entry)
    return ready, blocked


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose ready milestone report")
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    parser.add_argument("--phase", type=int, help="only this phase")
    parser.add_argument("--open", action="store_true", help="only the ones nobody has reserved or claimed")
    parser.add_argument("--blocked", action="store_true", help="list what is blocked and by what instead")
    arguments = parser.parse_args(argv)
    root = os.path.abspath(arguments.root)

    ready, blocked = state(root)
    rows = blocked if arguments.blocked else ready
    if arguments.phase:
        rows = [row for row in rows if row["phase"] == arguments.phase]
    if arguments.open and not arguments.blocked:
        rows = [row for row in rows if row["status"] == "open"]

    if arguments.json:
        print(json.dumps(rows, indent=2))
        return 0

    if not rows:
        print("nothing to report")
        return 0
    width = max(len(row["title"]) for row in rows)
    for row in rows:
        tail = f'blocked by {", ".join(row["missing"])}' if arguments.blocked else row["status"]
        progress = f'{row["checks_done"]}/{row["checks_total"]}'
        print(f'{row["id"]:>6}  {row["size"]:<2} {row["title"]:<{width}}  {progress:>7} checks  {tail}')
    print(f'{len(rows)} milestone(s); {len(ready)} ready in all, {len(blocked)} blocked')
    return 0


if __name__ == "__main__":
    sys.exit(main())

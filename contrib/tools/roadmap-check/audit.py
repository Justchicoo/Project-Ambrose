# Project Ambrose by Imjustchico
# Checks roadmap milestone identities, dependencies and sizes, counting only the deliverables a milestone lists before any subsection, so a detailed spec appended to it is not counted twice, and judging no size for a milestone that lists none, and with --require-evidence also a ticked acceptance box that carries none.

import argparse
import json
import re
import sys
from pathlib import Path


TABLE_RE = re.compile(
    r"^\|\s*(\d+\.\d+)\s*\|\s*[^|]+\|\s*([SML])\s*\|\s*([^|]+)\|"
)
HEADING_RE = re.compile(r"^##\s+(\d+\.\d+)\b")
DEPENDENCY_RE = re.compile(r"\*\*Depends on:\*\*\s*(.*)")
CHECK_RE = re.compile(r"^\s*-\s*\[([ xX])\]\s*(.*)$")
ID_RE = re.compile(r"\b\d+\.\d+\b")
SIZE_RE = re.compile(r"\*\*Size:\*\*\s*([SML])\.")
EXCEPTIONS = {"17.24", "17.51"}


def relative(path, root):
    return path.relative_to(root).as_posix()


def finding(kind, path, line, **values):
    return {"kind": kind, "file": path, "line": line, **values}


def parse_phase(path, root):
    lines = path.read_text(encoding="utf-8").splitlines()
    milestones = {}
    occurrences = {}
    current = None
    deliverables = False
    deliverables_seen = False
    subsection = False
    collect_checks = False
    acceptance_seen = False
    for line_number, line in enumerate(lines, 1):
        table = TABLE_RE.match(line)
        if table:
            milestone, size, depends = table.groups()
            occurrences.setdefault(milestone, []).append(line_number)
            current = milestones.setdefault(
                milestone,
                {"size": size, "depends": [], "checks": [], "deliverables": 0,
                 "heading": None, "table": line_number},
            )
            current["size"] = size
            current["depends"].extend(ID_RE.findall(depends))
            deliverables = False
            deliverables_seen = False
            subsection = False
            collect_checks = False
            acceptance_seen = False
            continue
        heading = HEADING_RE.match(line)
        if heading:
            milestone = heading.group(1)
            occurrences.setdefault(milestone, []).append(line_number)
            current = milestones.setdefault(
                milestone,
                {"size": None, "depends": [], "checks": [], "deliverables": 0,
                 "heading": line_number, "table": None},
            )
            current["heading"] = line_number
            deliverables = False
            deliverables_seen = False
            subsection = False
            collect_checks = False
            acceptance_seen = False
            continue
        if current is None:
            continue
        if line.startswith("**Deliverables**"):
            if not deliverables_seen and not subsection:
                deliverables = True
                deliverables_seen = True
            continue
        if line.startswith("**Acceptance**"):
            if not acceptance_seen:
                collect_checks = True
                acceptance_seen = True
            deliverables = False
            continue
        if line.startswith("### "):
            subsection = True
        if line.startswith("### ") or line.startswith("**Risks**") or line.startswith("## "):
            deliverables = False
            collect_checks = False
        if deliverables and re.match(r"^\s*-\s+", line):
            current["deliverables"] += 1
        check = CHECK_RE.match(line)
        if check and collect_checks:
            current["checks"].append((line_number, check.group(1).lower() == "x", check.group(2)))
        dependency = DEPENDENCY_RE.search(line)
        if dependency:
            current["depends"].extend(ID_RE.findall(dependency.group(1)))
        size = SIZE_RE.search(line)
        if size:
            current["size"] = size.group(1)
    return relative(path, root), milestones, occurrences


def expected_size(item, milestone):
    if milestone in EXCEPTIONS:
        return None
    deliverables = item["deliverables"]
    checks = len(item["checks"])
    if deliverables == 0:
        return None
    if deliverables <= 4 and checks <= 5:
        return "S"
    if deliverables >= 8 or checks >= 8:
        return "L"
    return "M"


def audit(root, require_evidence=False):
    findings = []
    known = {}
    for path in sorted((root / "doc" / "roadmap").glob("phase-*.md")):
        filename, milestones, occurrences = parse_phase(path, root)
        for milestone, lines in occurrences.items():
            if len(lines) > 2:
                findings.append(finding("duplicate_id", filename, lines[1], id=milestone, occurrences=lines))
        for milestone, item in milestones.items():
            known[milestone] = (filename, item)

    for milestone, (filename, item) in sorted(known.items()):
        for dependency in item["depends"]:
            if dependency not in known:
                findings.append(
                    finding("missing_dependency", filename, item["heading"] or item["table"],
                            id=milestone, dependency=dependency)
                )
        if item["heading"] is None:
            findings.append(finding("missing_definition", filename, item["table"], id=milestone))
        if item["table"] is None:
            findings.append(finding("missing_table_row", filename, item["heading"], id=milestone))
        expected = expected_size(item, milestone)
        if expected and item["size"] != expected:
            findings.append(
                finding("size_mismatch", filename, item["heading"] or item["table"],
                        id=milestone, declared=item["size"], expected=expected,
                        deliverables=item["deliverables"], checks=len(item["checks"]))
            )
        for line, checked, text in item["checks"]:
            if require_evidence and checked and not (("(" in text and ")" in text) or "`" in text):
                findings.append(
                    finding("checked_without_evidence", filename, line, id=milestone)
                )
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--require-evidence", action="store_true",
                        help="also report a ticked acceptance box with no evidence beside it, which phases written before that convention do not carry")
    args = parser.parse_args()
    findings = audit(args.root.resolve(), args.require_evidence)
    if args.format == "json":
        print(json.dumps({"findings": findings}, indent=2))
    else:
        for item in findings:
            print(f"{item['kind'].upper()} {item['file']}:{item['line']} {item.get('id', '')}".rstrip())
    print(f"roadmap audit: {len(findings)} finding(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())

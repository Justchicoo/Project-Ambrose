# Project Ambrose by Imjustchico
# Checks documentation links, local targets, and Markdown heading anchors.

import argparse
import json
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


LINK_RE = re.compile(r"(?<!!)\[[^\]]+\]\(([^)\s]+)(?:\s+['\"][^)]*['\"])?\)")
REFERENCE_RE = re.compile(r"^\s*\[([^\]]+)\]:\s*(\S+)")
HEADING_RE = re.compile(r"^\s{0,3}#{1,6}\s+(.+?)\s*#*\s*$")
HTML_ID_RE = re.compile(r"\b(?:id|name)=['\"]([^'\"]+)['\"]")
FENCE_RE = re.compile(r"^\s*(```+|~~~+)")


def slugify(text):
    text = re.sub(r"<[^>]+>", "", text)
    text = text.lower().strip()
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    return re.sub(r"[\s-]+", "-", text).strip("-")


def relative(path, root):
    return path.relative_to(root).as_posix()


def headings(path):
    anchors = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        heading = HEADING_RE.match(line)
        if heading:
            base = slugify(heading.group(1))
            index = anchors.get(base, 0)
            anchors[base] = index + 1
    return {slug if index == 0 else f"{slug}-{index}": True
            for slug, count in anchors.items()
            for index in range(count)}


def markdown_files(root):
    yield from sorted((root / "doc").rglob("*.md"))


def read_links(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    references = {}
    links = []
    fenced = False
    for line_number, line in enumerate(lines, 1):
        fence = FENCE_RE.match(line)
        if fence:
            fenced = not fenced
            continue
        if fenced:
            continue
        reference = REFERENCE_RE.match(line)
        if reference:
            references[reference.group(1).lower()] = reference.group(2)
        for target in LINK_RE.findall(line):
            links.append((line_number, target))
    fenced = False
    for line_number, line in enumerate(lines, 1):
        fence = FENCE_RE.match(line)
        if fence:
            fenced = not fenced
            continue
        if fenced:
            continue
        match = re.match(r"^\s*\[([^\]]+)\]\s*$", line)
        if match and match.group(1).lower() in references:
            links.append((line_number, references[match.group(1).lower()]))
    return links


def audit(root):
    files = {relative(path, root): path for path in markdown_files(root)}
    anchors = {name: headings(path) for name, path in files.items()}
    findings = []
    for name, path in files.items():
        for line_number, target in read_links(path):
            parsed = urlsplit(target)
            if parsed.scheme or target.startswith("//"):
                continue
            destination = unquote(parsed.path)
            if not destination and parsed.fragment:
                destination_name = name
            else:
                destination_path = (path.parent / destination).resolve()
                try:
                    destination_name = relative(destination_path, root)
                except ValueError:
                    findings.append({
                        "kind": "outside_doc_root",
                        "file": name,
                        "line": line_number,
                        "target": target,
                    })
                    continue
                if destination_name not in files and not destination_path.exists():
                    findings.append({
                        "kind": "missing_target",
                        "file": name,
                        "line": line_number,
                        "target": target,
                    })
                    continue
                if destination_path.is_file() and destination_name not in anchors:
                    anchors[destination_name] = headings(destination_path)
            if parsed.fragment and parsed.fragment not in anchors.get(destination_name, {}):
                findings.append({
                    "kind": "missing_anchor",
                    "file": name,
                    "line": line_number,
                    "target": target,
                })
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--format", choices=("text", "json"), default="text")
    args = parser.parse_args()
    findings = audit(args.root.resolve())
    if args.format == "json":
        print(json.dumps({"findings": findings}, indent=2))
    else:
        for item in findings:
            print(f"{item['kind'].upper()} {item['file']}:{item['line']} {item['target']}")
    print(f"dead-link audit: {len(findings)} finding(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())

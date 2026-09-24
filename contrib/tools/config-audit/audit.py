# Project Ambrose by Imjustchico
# Audits shipped configuration defaults against the documented configuration tables.

import argparse
import json
import re
import sys
from pathlib import Path


KEY_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.-]*$")
DOC_KEY_RE = re.compile(r"^`([^`]+)`$")
SHARED_PREFIXES = ("Admin.", "Appender.", "Console.", "Log.", "Logger.")
SHARED_KEYS = {"LogsDir"}


def parse_args():
    parser = argparse.ArgumentParser(description="Audit .conf.dist files against doc/config tables.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--format", choices=("text", "json"), default="text")
    return parser.parse_args()


def split_table(line):
    if not line.lstrip().startswith("|"):
        return None
    cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
    return cells if len(cells) >= 3 else None


def unquote(value):
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return value[1:-1].replace(r'\"', '"').replace(r'\\', '\\')
    return value


def normalize_default(value, concrete_key=None):
    if value == "empty":
        return ""
    if concrete_key and value == "the app's name":
        return concrete_key.split(".")[1].split(".")[0]
    if concrete_key and value == "<name>.conf":
        return concrete_key.split(".")[1].split(".")[0] + ".conf"
    return value


def read_dist(path):
    options = {}
    errors = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            errors.append(f"{path}:{number}: missing '='")
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if not KEY_RE.fullmatch(key):
            errors.append(f"{path}:{number}: invalid key {key!r}")
            continue
        if key in options:
            errors.append(f"{path}:{number}: duplicate key {key!r}")
            continue
        options[key] = {"value": unquote(value), "line": number}
    return options, errors


def placeholder_pattern(key):
    parts = key.split(".")
    return "^" + r"\.".join(re.escape(part) if not part.startswith("<") else r"[^.]+" for part in parts) + "$"


def read_documentation(path):
    options = []
    errors = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        cells = split_table(raw)
        if not cells or len(cells) < 4:
            continue
        key_match = DOC_KEY_RE.fullmatch(cells[0])
        if not key_match or not KEY_RE.fullmatch(key_match.group(1).replace("<name>", "name")):
            continue
        default = cells[2]
        if default.startswith("`") and default.endswith("`"):
            default = default[1:-1]
        options.append({"key": key_match.group(1), "default": default, "line": number})
    return options, errors


def match_documented(key, options):
    return [option for option in options if re.fullmatch(placeholder_pattern(option["key"]), key)]


def shipped_source(path):
    parts = path.parts
    if any(part.startswith(".") for part in parts):
        return False
    return not any(part == "build" or part.startswith("cmake-build") or part in ("out", "node_modules") for part in parts)


def audit(root):
    docs_dir = root / "doc" / "config"
    dist_files = sorted(path for path in (root / "src").rglob("*.conf.dist") if shipped_source(path))
    findings = []
    errors = []
    all_documented = []
    for path in sorted(docs_dir.glob("*.md")):
        documented, doc_errors = read_documentation(path)
        errors.extend(doc_errors)
        all_documented.extend((path, option) for option in documented)
    for dist in dist_files:
        app = dist.name.removesuffix(".conf.dist")
        doc = docs_dir / f"{app}.md"
        if not doc.is_file():
            findings.append({"kind": "missing_document", "app": app, "file": str(dist.relative_to(root))})
            continue
        shipped, dist_errors = read_dist(dist)
        documented, doc_errors = read_documentation(doc)
        errors.extend(dist_errors + doc_errors)
        shared = [(path, option) for path, option in all_documented
                  if option["key"] in SHARED_KEYS or option["key"].startswith(SHARED_PREFIXES)]
        candidates_for_app = documented + [option for _, option in shared
                                          if option["key"] not in {item["key"] for item in documented}]
        matched = set()
        for key, detail in shipped.items():
            candidates = match_documented(key, candidates_for_app)
            if not candidates:
                findings.append({"kind": "undocumented", "app": app, "key": key,
                                 "file": str(dist.relative_to(root)), "line": detail["line"]})
                continue
            for candidate in candidates:
                matched.add(id(candidate))
            matching_defaults = [candidate for candidate in candidates
                                 if normalize_default(candidate["default"], key) == detail["value"]]
            if not matching_defaults:
                candidate = candidates[0]
                findings.append({"kind": "default_mismatch", "app": app, "key": key,
                                 "file": str(dist.relative_to(root)), "line": detail["line"],
                                 "shipped": detail["value"], "documented": candidate["default"],
                                 "doc": str(doc.relative_to(root)), "doc_line": candidate["line"]})
        for option in documented:
            if id(option) not in matched and "<" not in option["key"]:
                findings.append({"kind": "documented_absent", "app": app, "key": option["key"],
                                 "doc": str(doc.relative_to(root)), "doc_line": option["line"]})
    return findings, errors


def render_text(findings, errors):
    lines = [f"configuration audit: {len(findings)} finding(s), {len(errors)} error(s)"]
    for error in errors:
        lines.append(f"ERROR {error}")
    for finding in findings:
        kind = finding["kind"]
        if kind == "default_mismatch":
            lines.append(f"DEFAULT {finding['app']} {finding['key']}: "
                         f"shipped={finding['shipped']!r}, documented={finding['documented']!r}")
        elif kind == "undocumented":
            lines.append(f"UNDOCUMENTED {finding['app']} {finding['key']} "
                         f"({finding['file']}:{finding['line']})")
        elif kind == "documented_absent":
            lines.append(f"ABSENT {finding['app']} {finding['key']} "
                         f"({finding['doc']}:{finding['doc_line']})")
        else:
            lines.append(f"ABSENT-DOC {finding['app']} ({finding['file']})")
    return "\n".join(lines) + "\n"


def main():
    args = parse_args()
    root = args.root.resolve()
    findings, errors = audit(root)
    if args.format == "json":
        print(json.dumps({"findings": findings, "errors": errors}, indent=2, ensure_ascii=False))
    else:
        print(render_text(findings, errors), end="")
    return 2 if errors else (1 if findings else 0)


if __name__ == "__main__":
    sys.exit(main())

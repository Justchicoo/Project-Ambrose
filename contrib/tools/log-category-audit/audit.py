# Project Ambrose by Imjustchico
# Audits source log categories against documentation and logger routes.

import argparse
import json
import re
import sys
from pathlib import Path


LOG_CALL_RE = re.compile(
    r"\bLOG_(?:TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\s*\(\s*([^,\s]+)"
)
AMBROSE_LOG_RE = re.compile(
    r"\bAMBROSE_LOG\s*\([^,]+,\s*[^,]+,\s*([^,\s]+)"
)
LITERAL_RE = re.compile(r'^"([^"]+)"$')
CONSTANT_RE = re.compile(
    r"\b(?:constexpr|const)\s+(?:char\s+const\*|std::string_view)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\"([^\"]+)\""
)
DOC_CATEGORY_RE = re.compile(r"`([^`]+)`")
LOGGER_RE = re.compile(r"^\s*Logger\.([A-Za-z0-9_.]+)\s*=", re.MULTILINE)


def relative(path, root):
    return path.relative_to(root).as_posix()


def source_files(root):
    source_root = root / "src"
    for path in sorted(source_root.rglob("*")):
        if path.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        if "test" in path.parts:
            continue
        yield path


def read_source_categories(root):
    constants = {}
    texts = {}
    for path in source_files(root):
        text = path.read_text(encoding="utf-8")
        texts[path] = text
        for name, value in CONSTANT_RE.findall(text):
            if "." in value:
                constants[name] = value

    categories = {}
    for path, text in texts.items():
        for line_number, line in enumerate(text.splitlines(), 1):
            for argument in LOG_CALL_RE.findall(line):
                literal = LITERAL_RE.match(argument)
                category = literal.group(1) if literal else constants.get(argument)
                if category and "." in category or category in {"accounts", "characters", "network"}:
                    categories.setdefault(category, []).append(
                        {"file": relative(path, root), "line": line_number}
                    )
            for argument in AMBROSE_LOG_RE.findall(line):
                literal = LITERAL_RE.match(argument)
                category = literal.group(1) if literal else constants.get(argument)
                if category:
                    categories.setdefault(category, []).append(
                        {"file": relative(path, root), "line": line_number}
                    )
    return categories


def read_documented_categories(root):
    path = root / "doc" / "config" / "logging.md"
    categories = {}
    in_categories = False
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line == "## Categories":
            in_categories = True
            continue
        if in_categories and line.startswith("## "):
            break
        if not in_categories or not line.startswith("|"):
            continue
        first_cell = line.split("|")[1]
        for category in DOC_CATEGORY_RE.findall(first_cell):
            categories[category] = {"file": relative(path, root), "line": line_number}
    return categories


def read_logger_routes(root):
    routes = {}
    for path in sorted((root / "src").rglob("*.conf.dist")):
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = LOGGER_RE.match(line)
            if match:
                routes.setdefault(match.group(1), []).append(
                    {"file": relative(path, root), "line": line_number}
                )
    return routes


def category_matches(pattern, category):
    if pattern == category:
        return True
    if "<app>" in pattern:
        return pattern == category or (
            pattern.startswith("server.") and category.startswith("server.")
        )
    return category.startswith(pattern + ".")


def route_matches(logger, category):
    return logger == "root" or category == logger or category.startswith(logger + ".")


def audit(root):
    source = read_source_categories(root)
    documented = read_documented_categories(root)
    routes = read_logger_routes(root)
    findings = []

    for category, locations in sorted(source.items()):
        if not any(category_matches(pattern, category) for pattern in documented):
            findings.append(
                {
                    "kind": "source_undocumented",
                    "category": category,
                    "locations": locations,
                }
            )
        if not any(route_matches(logger, category) for logger in routes):
            findings.append(
                {
                    "kind": "source_unrouted",
                    "category": category,
                    "locations": locations,
                }
            )

    for pattern, location in sorted(documented.items()):
        if "<app>" in pattern:
            continue
        if not any(category_matches(pattern, category) for category in source):
            findings.append(
                {
                    "kind": "documented_unused",
                    "category": pattern,
                    "location": location,
                }
            )

    for logger, locations in sorted(routes.items()):
        if logger == "root":
            continue
        if not any(route_matches(logger, category) for category in source):
            findings.append(
                {
                    "kind": "logger_unused",
                    "logger": logger,
                    "locations": locations,
                }
            )
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
        for finding in findings:
            kind = finding["kind"].upper()
            name = finding.get("category", finding.get("logger", ""))
            print(f"{kind} {name}")
    print(
        f"log category audit: {len(findings)} finding(s)",
        file=sys.stderr,
    )
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())

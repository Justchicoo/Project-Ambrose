# Project Ambrose by Imjustchico
# C-25 compares operator-supplied message-definition XML structurally.
import argparse
import json
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Difference:
    key: str
    before: dict[str, Any] | None
    after: dict[str, Any] | None


def normalize_text(value: str | None) -> str | None:
    if value is None:
        return None
    normalized = " ".join(value.split())
    return normalized or None


def canonical(element: ET.Element) -> dict[str, Any]:
    children = [canonical(child) for child in list(element)]
    children.sort(key=lambda child: (child["tag"], json.dumps(child["attributes"], sort_keys=True)))
    return {
        "tag": element.tag,
        "attributes": dict(sorted(element.attrib.items())),
        "text": normalize_text(element.text),
        "children": children,
    }


def definition_key(element: ET.Element) -> str:
    identifier = element.attrib.get("id") or element.attrib.get("name")
    if not identifier:
        raise ValueError(f"definition <{element.tag}> has neither id nor name")
    return f"{element.tag}:{identifier}"


def read_definitions(path: Path) -> dict[str, dict[str, Any]]:
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        raise ValueError(f"{path}: cannot parse XML: {exc}") from exc
    definitions: dict[str, dict[str, Any]] = {}
    for element in list(root):
        key = definition_key(element)
        if key in definitions:
            raise ValueError(f"{path}: duplicate definition key {key}")
        definitions[key] = canonical(element)
    if not definitions:
        raise ValueError(f"{path}: XML root has no direct definitions")
    return definitions


def compare(before_path: Path, after_path: Path) -> dict[str, Any]:
    before = read_definitions(before_path)
    after = read_definitions(after_path)
    added = [
        Difference(key, None, after[key])
        for key in sorted(set(after) - set(before))
    ]
    removed = [
        Difference(key, before[key], None)
        for key in sorted(set(before) - set(after))
    ]
    changed = [
        Difference(key, before[key], after[key])
        for key in sorted(set(before) & set(after))
        if before[key] != after[key]
    ]
    return {
        "version": 1,
        "before": str(before_path),
        "after": str(after_path),
        "counts": {
            "before": len(before),
            "after": len(after),
            "added": len(added),
            "removed": len(removed),
            "changed": len(changed),
        },
        "added": [asdict(item) for item in added],
        "removed": [asdict(item) for item in removed],
        "changed": [asdict(item) for item in changed],
    }


def run_self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="ambrose-c25-") as directory:
        root = Path(directory)
        before = root / "before.xml"
        after = root / "after.xml"
        before.write_text(
            "<Messages><Message id='1' name='Login'><Field name='user' type='text'/></Message>"
            "<Message id='2' name='Old'/></Messages>",
            encoding="utf-8",
        )
        after.write_text(
            "<Messages><Message name='Login' id='1'><Field type='text' name='account'/></Message>"
            "<Message id='3' name='New'/></Messages>",
            encoding="utf-8",
        )
        result = compare(before, after)
    expected = {"before": 2, "after": 2, "added": 1, "removed": 1, "changed": 1}
    print(json.dumps({"counts": result["counts"]}, sort_keys=True))
    return 0 if result["counts"] == expected else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare two message-definition XML files.")
    parser.add_argument("--before")
    parser.add_argument("--after")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        return run_self_test()
    if not args.before or not args.after:
        parser.error("--before and --after are required unless --self-test is used")
    try:
        result = compare(Path(args.before), Path(args.after))
    except ValueError as exc:
        print(f"C-25 differ: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2 if args.pretty else None))
    return 0 if not any(result["counts"][key] for key in ("added", "removed", "changed")) else 1


if __name__ == "__main__":
    raise SystemExit(main())

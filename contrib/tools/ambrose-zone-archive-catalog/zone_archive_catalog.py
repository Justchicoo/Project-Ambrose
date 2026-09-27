# Project Ambrose by Imjustchico
# Emits a private zone-to-archive table by using the existing zone extractor.

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ZONE_INSERT = re.compile(r"INSERT INTO `zone_template`[^;]*;", re.DOTALL)
ZONE_PATH = re.compile(r"\(X'([0-9A-F]+)'")
MASK32 = 0xFFFFFFFF


def ki_string_hash(text: str) -> int:
    result = 0
    shift = 0
    overflow_shift = 32
    for character in text.encode("utf-8"):
        value = character - 32
        result ^= (value << shift) & MASK32
        if shift > 24:
            result ^= value >> overflow_shift
            if shift >= 27:
                shift -= 32
                overflow_shift += 32
        shift += 5
        overflow_shift -= 5
        result &= MASK32
    return (-result) & MASK32 if result & 0x80000000 else result


def paths_from_sql(sql: str) -> list[str]:
    paths: list[str] = []
    for statement in ZONE_INSERT.finditer(sql):
        for match in ZONE_PATH.finditer(statement.group()):
            try:
                path = bytes.fromhex(match.group(1)).decode("utf-8")
            except (UnicodeDecodeError, ValueError) as error:
                raise ValueError("zone path is not valid UTF-8 hex") from error
            if not path or "/" not in path or path.startswith("/") or path.endswith("/"):
                raise ValueError(f"zone path has no usable world namespace: {path!r}")
            if any(character in path for character in "\t\r\n"):
                raise ValueError("zone path cannot be represented in TSV")
            paths.append(path)
    if not paths:
        raise ValueError("extractor output contains no zone_template paths")
    if len(paths) != len(set(paths)):
        raise ValueError("extractor output contains duplicate zone paths")
    return paths


def table_rows(paths: list[str]) -> list[str]:
    ids: set[int] = set()
    rows: list[str] = []
    for path in paths:
        zone_id = ki_string_hash(path)
        if zone_id in ids:
            raise ValueError(f"zone id collision: {zone_id}")
        ids.add(zone_id)
        world = path.split("/", 1)[0]
        archive = path.replace("/", "-") + ".wad"
        rows.append(f"zone\t{zone_id}\t{world}\t{path}\t{archive}")
    return rows


def extractor_path(value: str) -> str:
    candidate = Path(value)
    if candidate.is_file():
        return str(candidate)
    located = shutil.which(value)
    if located:
        return located
    raise ValueError(f"extractor executable was not found: {value}")


def self_test() -> None:
    sql = (
        "INSERT INTO `zone_template` (`zone_path`) VALUES "
        "(X'57697A617264436974792F57435F487562'), "
        "(X'57697A617264436974792F57435F526176656E776F6F64');"
    )
    expected = [
        "zone\t1727411499\tWizardCity\tWizardCity/WC_Hub\tWizardCity-WC_Hub.wad",
        "zone\t699201167\tWizardCity\tWizardCity/WC_Ravenwood\tWizardCity-WC_Ravenwood.wad",
    ]
    if table_rows(paths_from_sql(sql)) != expected:
        raise RuntimeError("zone archive catalog self-test failed")
    try:
        paths_from_sql(sql + sql)
    except ValueError:
        return
    raise RuntimeError("duplicate zone path self-test failed")


def main() -> int:
    parser = argparse.ArgumentParser(description="Print a private zone-to-archive table.")
    parser.add_argument("--extractor", help="path or command name of the built extractor")
    parser.add_argument("--client", type=Path, help="your own Wizard101 install")
    parser.add_argument("--type-dump", type=Path, help="type dump for the install revision")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        if args.extractor or args.client or args.type_dump:
            parser.error("--self-test cannot be combined with client arguments")
        self_test()
        print("self-test: passed", file=sys.stderr)
        return 0
    if not args.extractor or not args.client or not args.type_dump:
        parser.error("--extractor, --client and --type-dump are required")
    if not args.client.is_dir():
        parser.error(f"client install is not a directory: {args.client}")
    if not args.type_dump.is_file():
        parser.error(f"type dump is not a file: {args.type_dump}")
    with tempfile.TemporaryDirectory(prefix="ambrose-zone-archive-") as temporary:
        sql_path = Path(temporary) / "zones.sql"
        result = subprocess.run(
            [
                extractor_path(args.extractor),
                "--client",
                str(args.client),
                "--type-dump",
                str(args.type_dump),
                "--sql",
                str(sql_path),
                "zones",
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if result.returncode != 0:
            if result.stdout:
                print(result.stdout, file=sys.stderr, end="")
            if result.stderr:
                print(result.stderr, file=sys.stderr, end="")
            raise RuntimeError(f"extractor exited with status {result.returncode}")
        rows = table_rows(paths_from_sql(sql_path.read_text(encoding="utf-8")))
    sys.stdout.write("\n".join(rows) + "\n")
    print(f"zones={len(rows)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from error

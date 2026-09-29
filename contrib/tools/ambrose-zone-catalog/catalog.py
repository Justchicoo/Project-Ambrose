# Project Ambrose by Imjustchico
# Prints the per-zone id, namespace and archive catalog using the existing extractor, and with the client tool the archives each zone's placed objects take their templates from.

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

ZONE_INSERT = re.compile(r"INSERT INTO `zone_template`[^;]*;", re.DOTALL)
ZONE_PATH = re.compile(r"\(X'([0-9A-F]+)'")
OBJECT_INSERT = re.compile(r"INSERT INTO `zone_object` \(([^)]*)\) VALUES ([^;]*);")
OBJECT_ROW = re.compile(r"\(([^()]*)\)")
TEMPLATE_ID = re.compile(r"[0-9]+")
MASK32 = 0xFFFFFFFF
ROOT_ARCHIVE = "Root.wad"


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
            if not path or "\t" in path or "\r" in path or "\n" in path:
                raise ValueError("zone path is empty or cannot be represented in TSV")
            paths.append(path)
    if not paths:
        raise ValueError("extractor output contains no zone_template paths")
    if len(paths) != len(set(paths)):
        raise ValueError("extractor output contains duplicate zone paths")
    return paths


def manifest_rows(paths: list[str]) -> list[str]:
    rows: list[str] = []
    for entry in catalog_entries(paths):
        rows.append(f"zone\t{entry['id']}\tworld\t{entry['world']}")
        rows.append(f"zone\t{entry['id']}\tarchive\t{entry['archive']}")
    return rows


def catalog_entries(paths: list[str]) -> list[dict[str, int | str]]:
    ids: set[int] = set()
    entries: list[dict[str, int | str]] = []
    for path in paths:
        if "/" not in path or path.startswith("/") or path.endswith("/"):
            raise ValueError(f"zone path has no usable world namespace: {path!r}")
        zone_id = ki_string_hash(path)
        if zone_id in ids:
            raise ValueError(f"zone id collision: {zone_id}")
        ids.add(zone_id)
        world = path.split("/", 1)[0]
        archive = path.replace("/", "-") + ".wad"
        if any(character in world + archive for character in "\t\r\n"):
            raise ValueError("zone namespace or archive cannot be represented in TSV")
        entries.append(
            {
                "path": path,
                "id": zone_id,
                "world": world,
                "archive": archive,
            }
        )
    return sorted(entries, key=lambda entry: str(entry["path"]))


def catalog_json(entries: list[dict[str, int | str]]) -> str:
    rows = (
        json.dumps(entry, ensure_ascii=False, separators=(",", ":"))
        for entry in entries
    )
    return '{"zones":[\n  ' + ",\n  ".join(rows) + "\n]}\n"


def object_templates_from_sql(sql: str) -> list[tuple[str, int | None]]:
    objects: list[tuple[str, int | None]] = []
    for statement in OBJECT_INSERT.finditer(sql):
        columns = [column.strip().strip("`") for column in statement.group(1).split(",")]
        if "zone_path" not in columns or "template_id" not in columns:
            raise ValueError("the extractor's zone_object rows carry no zone_path or template_id column")
        zone_column = columns.index("zone_path")
        template_column = columns.index("template_id")
        for row in OBJECT_ROW.finditer(statement.group(2)):
            values = row.group(1).split(", ")
            if len(values) != len(columns):
                raise ValueError(f"a zone_object row holds {len(values)} values for {len(columns)} columns")
            zone = values[zone_column]
            if not zone.startswith("X'") or not zone.endswith("'"):
                raise ValueError("a zone_object row names no zone path")
            try:
                path = bytes.fromhex(zone[2:-1]).decode("utf-8")
            except (UnicodeDecodeError, ValueError) as error:
                raise ValueError("zone path is not valid UTF-8 hex") from error
            template = values[template_column]
            if template == "NULL":
                objects.append((path, None))
            elif TEMPLATE_ID.fullmatch(template):
                objects.append((path, int(template)))
            else:
                raise ValueError(f"a zone_object row holds a template id that is not a number: {template[:40]!r}")
    return objects


def templates_from_listing(text: str) -> dict[int, str]:
    archives: dict[int, str] = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        parts = line.split("  ", 2)
        if len(parts) < 3 or not TEMPLATE_ID.fullmatch(parts[0]) or not parts[1]:
            raise ValueError(f"template listing line {number} does not read as '<id>  <archive>  <entry>'")
        template_id = int(parts[0])
        if template_id in archives:
            raise ValueError(f"the template listing names id {template_id} twice")
        archives[template_id] = parts[1]
    if not archives:
        raise ValueError("the template listing is empty")
    return archives


def draws_from(
    entries: list[dict[str, int | str]], objects: list[tuple[str, int | None]], templates: dict[int, str]
) -> dict:
    zones = {str(entry["path"]): entry for entry in entries}
    placed: Counter = Counter()
    by_zone: dict[str, Counter] = defaultdict(Counter)
    without_template = 0
    unlisted = 0
    for zone, template_id in objects:
        if zone not in zones:
            raise ValueError(f"a placed object names a zone with no zone_template row: {zone!r}")
        placed[zone] += 1
        if not template_id:
            without_template += 1
            continue
        archive = templates.get(template_id)
        if archive is None:
            unlisted += 1
            continue
        by_zone[zone][archive] += 1
    rows: list[dict[str, int | str]] = []
    worlds: dict[str, dict[str, list[int]]] = defaultdict(dict)
    from_root = 0
    elsewhere: set[str] = set()
    for zone in sorted(by_zone):
        counts = by_zone[zone]
        if counts[ROOT_ARCHIVE]:
            from_root += 1
        entry = zones[zone]
        for archive in sorted(counts):
            if archive in (ROOT_ARCHIVE, entry["archive"]):
                continue
            rows.append({"zone": zone, "archive": archive, "objects": counts[archive]})
            elsewhere.add(zone)
            total = worlds[str(entry["world"])].setdefault(archive, [0, 0])
            total[0] += 1
            total[1] += counts[archive]
    return {
        "zones": len(entries),
        "zones_with_objects": len(placed),
        "objects": sum(placed.values()),
        "objects_without_template": without_template,
        "objects_with_unlisted_template": unlisted,
        "zones_drawing_from_root_wad": from_root,
        "zones_drawing_from_another_archive": len(elsewhere),
        "draws_from": rows,
        "worlds": {
            world: [{"archive": archive, "zones": total[0], "objects": total[1]} for archive, total in sorted(archives.items())]
            for world, archives in sorted(worlds.items())
        },
    }


def draws_from_json(report: dict) -> str:
    def lines(items: list[str], opening: str, closing: str) -> str:
        return opening + "\n  " + ",\n  ".join(items) + "\n" + closing if items else opening + closing

    def compact(value: object) -> str:
        return json.dumps(value, ensure_ascii=False, separators=(",", ":"))

    totals = {key: value for key, value in report.items() if key not in ("draws_from", "worlds")}
    rows = [compact(row) for row in report["draws_from"]]
    worlds = [f"{compact(world)}:{compact(archives)}" for world, archives in report["worlds"].items()]
    return compact(totals)[:-1] + ',"draws_from":' + lines(rows, "[", "]") + ',"worlds":' + lines(worlds, "{", "}") + "}\n"


def hex_text(value: str) -> str:
    return "X'" + value.encode("utf-8").hex().upper() + "'"


def expect_refusal(action, what: str) -> None:
    try:
        action()
    except ValueError:
        return
    raise RuntimeError(f"{what} self-test failed")


def self_test() -> None:
    sql = (
        "INSERT INTO `zone_template` (`zone_path`) VALUES "
        "(X'57697A617264436974792F57435F487562'), "
        "(X'57697A617264436974792F57435F526176656E776F6F64');"
    )
    paths = paths_from_sql(sql)
    rows = manifest_rows(paths)
    expected = [
        "zone\t1727411499\tworld\tWizardCity",
        "zone\t1727411499\tarchive\tWizardCity-WC_Hub.wad",
        "zone\t699201167\tworld\tWizardCity",
        "zone\t699201167\tarchive\tWizardCity-WC_Ravenwood.wad",
    ]
    if rows != expected:
        raise RuntimeError("zone catalog self-test failed")
    entries = catalog_entries(paths)
    expected_entries = [
        {
            "path": "WizardCity/WC_Hub",
            "id": 1727411499,
            "world": "WizardCity",
            "archive": "WizardCity-WC_Hub.wad",
        },
        {
            "path": "WizardCity/WC_Ravenwood",
            "id": 699201167,
            "world": "WizardCity",
            "archive": "WizardCity-WC_Ravenwood.wad",
        },
    ]
    if entries != expected_entries:
        raise RuntimeError("zone catalog JSON self-test failed")
    if json.loads(catalog_json(entries)) != {"zones": expected_entries}:
        raise RuntimeError("zone catalog JSON serialization self-test failed")
    try:
        paths_from_sql(sql + sql)
    except ValueError:
        pass
    else:
        raise RuntimeError("duplicate zone path self-test failed")

    hub, ravenwood = "WizardCity/WC_Hub", "WizardCity/WC_Ravenwood"
    placed = [(hub, "10"), (hub, "20"), (hub, "20"), (hub, "30"), (ravenwood, "20"), (ravenwood, "99"), (ravenwood, "NULL")]
    objects_sql = sql + "\nINSERT INTO `zone_object` (`zone_path`, `class_name`, `template_id`, `object_id`) VALUES " + ", ".join(
        f"({hex_text(zone)}, {hex_text('class CoreObjectInfo')}, {template}, {number})"
        for number, (zone, template) in enumerate(placed, 1)
    ) + ";\n"
    listing = (
        "10  Root.wad  ObjectData/Sign.xml\n"
        "20  WizardCity-WorldData.wad  ObjectData/Two  Spaces.xml\n"
        "30  WizardCity-WC_Hub.wad  ObjectData/Local.xml  (not in the archive)\n"
    )
    objects = object_templates_from_sql(objects_sql)
    if paths_from_sql(objects_sql) != paths or len(objects) != 7 or objects[-1] != (ravenwood, None):
        raise RuntimeError("zone object self-test failed")
    report = draws_from(entries, objects, templates_from_listing(listing))
    if report != {
        "zones": 2,
        "zones_with_objects": 2,
        "objects": 7,
        "objects_without_template": 1,
        "objects_with_unlisted_template": 1,
        "zones_drawing_from_root_wad": 1,
        "zones_drawing_from_another_archive": 2,
        "draws_from": [
            {"zone": hub, "archive": "WizardCity-WorldData.wad", "objects": 2},
            {"zone": ravenwood, "archive": "WizardCity-WorldData.wad", "objects": 1},
        ],
        "worlds": {"WizardCity": [{"archive": "WizardCity-WorldData.wad", "zones": 2, "objects": 3}]},
    }:
        raise RuntimeError("draws-from self-test failed")
    if json.loads(draws_from_json(report)) != report:
        raise RuntimeError("draws-from JSON serialization self-test failed")
    quiet = dict(report, draws_from=[], worlds={})
    if json.loads(draws_from_json(quiet)) != quiet:
        raise RuntimeError("empty draws-from JSON serialization self-test failed")
    expect_refusal(lambda: templates_from_listing("Root.wad  ObjectData/Sign.xml\n"), "listing line with no id")
    expect_refusal(lambda: templates_from_listing(listing + listing), "listing naming an id twice")
    expect_refusal(lambda: draws_from(entries, [("WizardCity/WC_Unknown", 10)], templates_from_listing(listing)), "object in an unknown zone")
    expect_refusal(
        lambda: object_templates_from_sql("INSERT INTO `zone_object` (`zone_path`, `object_id`) VALUES (X'41', 1);\n"),
        "zone_object rows with no template_id column",
    )
    expect_refusal(
        lambda: object_templates_from_sql("INSERT INTO `zone_object` (`zone_path`, `template_id`) VALUES (X'41', 1.5);\n"),
        "template id that is not a number",
    )


def extractor_path(value: str, name: str = "extractor") -> str:
    candidate = Path(value)
    if candidate.is_file():
        return str(candidate)
    located = shutil.which(value)
    if located:
        return located
    raise ValueError(f"{name} executable was not found: {value}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Print a zone catalog as canonical tab-separated rows or JSON, "
        "or the archives each zone's placed objects take their templates from."
    )
    parser.add_argument("--extractor", help="path or command name of the built extractor")
    parser.add_argument("--client", type=Path, help="your own Wizard101 install")
    parser.add_argument("--type-dump", type=Path, help="type dump for the install revision")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--json", action="store_true", help="print the catalog as JSON")
    output.add_argument(
        "--draws-from",
        action="store_true",
        help="print as JSON every archive other than its own and Root.wad that each zone's placed objects "
        "take their templates from, with a summary per world; needs --client-tool",
    )
    parser.add_argument("--client-tool", help="path or command name of the built client tool, for --draws-from")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        if args.extractor or args.client or args.type_dump or args.client_tool:
            parser.error("--self-test cannot be combined with client arguments")
        self_test()
        print("self-test: passed", file=sys.stderr)
        return 0
    if not args.extractor or not args.client or not args.type_dump:
        parser.error("--extractor, --client and --type-dump are required")
    if args.draws_from != bool(args.client_tool):
        parser.error("--draws-from and --client-tool go together")
    if not args.client.is_dir():
        parser.error(f"client install is not a directory: {args.client}")
    if not args.type_dump.is_file():
        parser.error(f"type dump is not a file: {args.type_dump}")
    client_tool = extractor_path(args.client_tool, "client tool") if args.client_tool else None

    with tempfile.TemporaryDirectory(prefix="ambrose-zone-catalog-") as temporary:
        sql_path = Path(temporary) / "zones.sql"
        command = [
            extractor_path(args.extractor),
            "--client",
            str(args.client),
            "--type-dump",
            str(args.type_dump),
            "--sql",
            str(sql_path),
            "zones",
        ]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        if result.returncode != 0:
            if result.stdout:
                print(result.stdout, file=sys.stderr, end="")
            if result.stderr:
                print(result.stderr, file=sys.stderr, end="")
            raise RuntimeError(f"extractor exited with status {result.returncode}")
        sql = sql_path.read_text(encoding="utf-8")
        paths = paths_from_sql(sql)
        rows = manifest_rows(paths)

    if client_tool:
        listing = subprocess.run(
            [client_tool, "--client", str(args.client), "--type-dump", str(args.type_dump), "--all", "template", "--list"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if listing.returncode != 0:
            if listing.stderr:
                print(listing.stderr, file=sys.stderr, end="")
            raise RuntimeError(f"client tool exited with status {listing.returncode}")
        report = draws_from(catalog_entries(paths), object_templates_from_sql(sql), templates_from_listing(listing.stdout))
        sys.stdout.write(draws_from_json(report))
        print(
            f"zones={report['zones']} zones_with_objects={report['zones_with_objects']} objects={report['objects']} "
            f"zones_drawing_from_another_archive={report['zones_drawing_from_another_archive']}",
            file=sys.stderr,
        )
    elif args.json:
        entries = catalog_entries(paths)
        sys.stdout.write(catalog_json(entries))
        print(f"zones={len(entries)}", file=sys.stderr)
    else:
        sys.stdout.write("\n".join(rows) + "\n")
        print(f"zones={len(paths)} manifest_rows={len(rows)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from error

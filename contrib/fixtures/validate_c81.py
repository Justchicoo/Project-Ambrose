# Project Ambrose by Imjustchico
# Checks the C-81 SQL duality corpus: every case file exists with its header, each verdict matches its kind, and the naming rules include the _01 promotion case.
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "contrib/fixtures/c81-sql-duality-corpus.json"
CASES = ROOT / "contrib/fixtures/c81-sql-duality/cases"
HEADER = "-- Project Ambrose by Imjustchico"
PENDING_NAME = re.compile(r"^rev_\d+_[A-Za-z0-9_-]+\.sql$")
PROMOTED_NAME = re.compile(r"^\d{4}_\d{2}_\d{2}_\d{2}\.sql$")
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
KINDS = {
    "clean": lambda m, d: m["verdict"] == "ok" and d["verdict"] == "ok",
    "syntax_error": lambda m, d: m["verdict"] == "error" and d["verdict"] == "error",
    "mariadb_only": lambda m, d: m["verdict"] == "error" and d["verdict"] == "ok",
    "mysql_only": lambda m, d: m["verdict"] == "ok" and d["verdict"] == "error",
}


def verdict_problem(name, side):
    verdict = side.get("verdict")
    if verdict not in ("ok", "error"):
        return f"{name}: verdict is '{verdict}'"
    if verdict == "error" and not isinstance(side.get("code"), int):
        return f"{name}: an error verdict carries its numeric code"
    if verdict == "ok" and "code" in side:
        return f"{name}: an ok verdict carries no code"
    return None


def main():
    problems = []
    try:
        corpus = json.loads(CORPUS.read_text(encoding="utf-8"))
    except OSError as error:
        print(f"validate_c81: {CORPUS.name}: {error}", file=sys.stderr)
        return 1
    cases = corpus.get("cases", [])
    files = {entry.get("file") for entry in cases}
    on_disk = {f"cases/{path.name}" for path in CASES.glob("*.sql")}
    if files != on_disk:
        problems.append(f"cases on disk {sorted(on_disk)} do not match the corpus {sorted(files)}")
    for entry in cases:
        name = entry.get("file", "?")
        path = ROOT / "contrib/fixtures/c81-sql-duality" / name
        if not path.is_file():
            problems.append(f"{name}: file is missing")
            continue
        lines = path.read_text(encoding="utf-8").split("\n")
        if len(lines) < 2 or lines[0] != HEADER or not lines[1].startswith("-- ") or len(lines[1].strip()) <= 3:
            problems.append(f"{name}: does not open with the two-line header")
        if not path.read_bytes().endswith(b"\n"):
            problems.append(f"{name}: does not end in a newline")
        kind = entry.get("kind")
        mysql = entry.get("mysql", {})
        mariadb = entry.get("mariadb", {})
        for side_name, side in (("mysql", mysql), ("mariadb", mariadb)):
            problem = verdict_problem(f"{name} {side_name}", side)
            if problem:
                problems.append(problem)
        if kind not in KINDS:
            problems.append(f"{name}: unknown kind '{kind}'")
        elif not KINDS[kind](mysql, mariadb):
            problems.append(f"{name}: verdicts do not match kind '{kind}'")
    kinds = {entry.get("kind") for entry in cases}
    for required in KINDS:
        if required not in kinds:
            problems.append(f"no case of kind '{required}'")
    naming = corpus.get("naming", [])
    if not any(entry.get("promoted", "").endswith("_01.sql") for entry in naming):
        problems.append("no naming case promotes to a _01 name")
    for entry in naming:
        label = entry.get("pending", "?")
        if not PENDING_NAME.match(label):
            problems.append(f"{label}: not a rev_<unix seconds>_<short-name>.sql pending name")
        if not PROMOTED_NAME.match(entry.get("promoted", "")):
            problems.append(f"{label}: promoted name is not YYYY_MM_DD_NN.sql")
        if not DATE.match(entry.get("date", "")):
            problems.append(f"{label}: date is not YYYY-MM-DD")
        promoted = entry.get("promoted", "")
        for earlier in entry.get("existing", []):
            if not PROMOTED_NAME.match(earlier):
                problems.append(f"{label}: existing name {earlier} is not YYYY_MM_DD_NN.sql")
            elif not earlier[:10] == promoted[:10]:
                problems.append(f"{label}: existing {earlier} is not from promoted's date")
    if problems:
        for problem in problems:
            print(f"validate_c81: {problem}", file=sys.stderr)
        return 1
    print(f"validate_c81: {len(cases)} cases, {len(naming)} naming rules, 0 problem(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

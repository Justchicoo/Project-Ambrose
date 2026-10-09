# Project Ambrose by Imjustchico
# Keeps the SQL a pull request adds apart from the SQL already applied: check refuses a change to a file the updater may have applied and checks every new file's name and header, promote renames each pending rev_<unix seconds>_<name>.sql to the next free dated name when it lands on main, apply runs base, released and pending SQL in the updater's own order on fresh databases through the mysql client, naming the file that fails, and duality runs the same SQL or a C-81 style corpus on a MySQL 8 and a MariaDB server side by side and flags every file only one of them accepts.
import argparse
import datetime
import itertools
import json
import os
import re
import subprocess
import sys
from pathlib import Path

SQL = Path("data/sql")
RELEASED_NAME = re.compile(r"^\d{4}_\d{2}_\d{2}_\d{2}\.sql$")
PENDING_NAME = re.compile(r"^rev_(\d+)_([A-Za-z0-9_-]+)\.sql$")
APPLIED_FOLDER = re.compile(r"^data/sql/(updates/db_[a-z]+|base/db_[a-z]+)/")
PENDING_FOLDER = re.compile(r"^data/sql/updates/pending_db_([a-z]+)/")
RELEASED_FOLDER = re.compile(r"^data/sql/updates/db_([a-z]+)/")
HEADER = "-- Project Ambrose by Imjustchico"
BOOKKEEPING = ("updates.sql", "updates_include.sql")
SQUASH_LINE = re.compile(r"^Squashes SQL:", re.MULTILINE)
ERROR_CODE = re.compile(r"^ERROR (\d+)")


def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True, check=True).stdout


def changes(root, commit_range):
    changed = []
    for line in git(root, "diff", "--name-status", "--no-renames", commit_range).splitlines():
        status, _, path = line.partition("\t")
        if path:
            changed.append((status[:1], path))
    return changed


def header_problem(text):
    lines = text.replace("\r\n", "\n").split("\n")
    if len(lines) < 2 or lines[0] != HEADER or not lines[1].startswith("-- ") or len(lines[1].strip()) <= 3:
        return f"does not open with the two-line header, '{HEADER}' and a line saying what it holds"
    return None


def check(changed, read, allow_edits=False):
    problems = []
    for status, path in changed:
        if not path.endswith(".sql"):
            continue
        name = path.rsplit("/", 1)[-1]
        if APPLIED_FOLDER.match(path) and status != "A":
            if not allow_edits:
                verb = "deletes" if status == "D" else "changes"
                problems.append(f"{path}: {verb} a file a database may already have applied; add a new update instead, or say why on a 'Squashes SQL:' line")
            continue
        if status == "D":
            continue
        if RELEASED_FOLDER.match(path) and not RELEASED_NAME.match(name):
            problems.append(f"{path}: a released update is named YYYY_MM_DD_NN.sql")
            continue
        if PENDING_FOLDER.match(path) and not PENDING_NAME.match(name):
            problems.append(f"{path}: a pending update is named rev_<unix seconds>_<short-name>.sql, in letters, digits, hyphens and underscores")
            continue
        if path.startswith(str(SQL.as_posix()) + "/"):
            problem = header_problem(read(path))
            if problem:
                problems.append(f"{path}: {problem}")
    return problems


def pending_files(root):
    found = []
    for folder in sorted((root / SQL / "updates").glob("pending_db_*")):
        for file in folder.glob("*.sql"):
            matched = PENDING_NAME.match(file.name)
            if matched:
                found.append((folder.name[len("pending_"):], int(matched.group(1)), file.name, file))
    return sorted(found, key=lambda entry: (entry[0], entry[1], entry[2]))


def promotions(root, today):
    stamp = today.strftime("%Y_%m_%d")
    taken = {}
    moves = []
    for database, _, _, file in pending_files(root):
        target = root / SQL / "updates" / database
        if database not in taken:
            taken[database] = {path.name for path in target.glob(f"{stamp}_*.sql")}
        number = 0
        while f"{stamp}_{number:02d}.sql" in taken[database]:
            number += 1
        if number > 99:
            raise ValueError(f"{target} has no free number left for {stamp}")
        name = f"{stamp}_{number:02d}.sql"
        taken[database].add(name)
        moves.append((file, target / name))
    return moves


def ordered_files(root, database):
    base = sorted((root / SQL / "base" / f"db_{database}").glob("*.sql"), key=lambda path: (path.name in BOOKKEEPING, path.name))
    released = sorted((root / SQL / "updates" / f"db_{database}").glob("*.sql"), key=lambda path: path.name)
    pending = sorted((root / SQL / "updates" / f"pending_db_{database}").glob("*.sql"), key=lambda path: path.name)
    return base + released + pending


def databases(root):
    return sorted(folder.name[len("db_"):] for folder in (root / SQL / "base").glob("db_*") if folder.is_dir())


def mysql(client, connection, database, statement=None, file=None):
    command = [client, "--host", connection["host"], "--port", str(connection["port"]), "--user", connection["user"], "--batch", "--skip-column-names"]
    if database:
        command += ["--database", database]
    environment = dict(os.environ, MYSQL_PWD=connection["password"])
    if statement is not None:
        command += ["--execute", statement]
        return subprocess.run(command, capture_output=True, text=True, env=environment)
    with open(file, "rb") as source:
        return subprocess.run(command, stdin=source, capture_output=True, env=environment)


def run_file(client, connection, schema, file):
    result = mysql(client, connection, schema, file=file)
    if not result.returncode:
        return ("ok", None, "")
    lines = result.stderr.decode("utf-8", "replace").strip().splitlines()
    errors = [line for line in lines if line.startswith("ERROR")]
    code = ERROR_CODE.match(errors[0]) if errors else None
    return ("error", int(code.group(1)) if code else None, " ".join(errors or lines))


def fresh(client, connection, schema):
    created = mysql(client, connection, None, f"DROP DATABASE IF EXISTS `{schema}`; CREATE DATABASE `{schema}` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
    return None if not created.returncode else f"cannot create {schema}: {created.stderr.strip()}"


def run_sequence(client, connection, schema, files, keep=False, report=None):
    problem = fresh(client, connection, schema)
    if problem:
        return None, problem
    try:
        for file in files:
            verdict = run_file(client, connection, schema, file)
            if verdict[0] != "ok":
                return (file, verdict), None
            if report:
                report(file)
        return None, None
    finally:
        if not keep:
            mysql(client, connection, None, f"DROP DATABASE IF EXISTS `{schema}`")


def apply(root, client, connection, prefix, keep=False):
    failures = []
    for database in databases(root):
        refused, problem = run_sequence(client, connection, f"{prefix}_{database}", ordered_files(root, database), keep,
                                        lambda file: print(f"applied {file.relative_to(root).as_posix()}"))
        if problem:
            return [problem]
        if refused:
            failures.append(f"{refused[0].relative_to(root).as_posix()}: {refused[1][2]}")
    return failures


def server(text):
    name, _, address = text.partition("=")
    host, _, port = address.rpartition(":")
    if not name or not host or not port.isdigit():
        raise argparse.ArgumentTypeError(f"'{text}' is not NAME=HOST:PORT")
    return name, host, int(port)


def describe(verdict):
    return "ok" if verdict[0] == "ok" else f"error {verdict[1]}"


def duality_updates(root, servers, run):
    problems = []
    for database in databases(root):
        files = ordered_files(root, database)
        refused = {name: run(name, database, files) for name in servers}
        if all(value is None for value in refused.values()):
            continue
        reached = {name: files.index(value[0]) if value else len(files) for name, value in refused.items()}
        first = min(reached.values())
        file = files[first].relative_to(root).as_posix()
        accepted = [name for name in servers if reached[name] > first]
        refusing = [name for name in servers if reached[name] == first]
        detail = "; ".join(f"{name}: {refused[name][1][2]}" for name in refusing)
        if accepted:
            problems.append(f"{file}: only {', '.join(accepted)} accepts it ({detail})")
        else:
            problems.append(f"{file}: refused by every server ({detail})")
    return problems


def duality_files(files, servers, run, expected=None):
    problems = []
    for file in files:
        verdicts = {name: run(name, file) for name in servers}
        line = " ".join(f"{name}={describe(verdicts[name])}" for name in servers)
        if expected is not None:
            want = expected.get(file.name)
            if want is None:
                problems.append(f"{file.as_posix()}: the corpus gives no verdict for it")
                continue
            wrong = [name for name in servers if (verdicts[name][0], verdicts[name][1] if verdicts[name][0] == "error" else None) != (want[name]["verdict"], want[name].get("code"))]
            if wrong:
                problems.append(f"{file.as_posix()}: {line}, the corpus says " + " ".join(f"{name}={describe((want[name]['verdict'], want[name].get('code')))}" for name in servers))
                continue
        elif len({verdict[0] for verdict in verdicts.values()}) > 1:
            problems.append(f"{file.as_posix()}: only {', '.join(name for name in servers if verdicts[name][0] == 'ok')} accepts it ({line})")
            continue
        print(f"{file.as_posix()}: {line}")
    return problems


def sql_files(paths):
    files = []
    for path in paths:
        files.extend(sorted(path.rglob("*.sql")) if path.is_dir() else [path])
    return files


def duality(root, client, servers, user, password, prefix, paths, corpus):
    connections = {name: {"host": host, "port": port, "user": user, "password": password} for name, host, port in servers}
    names = list(connections)
    counter = itertools.count()

    def alone(name, file):
        refused, problem = run_sequence(client, connections[name], f"{prefix}_{next(counter)}", [file])
        if problem:
            raise RuntimeError(problem)
        return refused[1] if refused else ("ok", None, "")

    if corpus:
        data = json.loads(corpus.read_text(encoding="utf-8"))
        missing = [name for name in names if name not in data.get("servers", {})]
        if missing:
            return [f"{corpus.as_posix()}: the corpus has no verdicts for {', '.join(missing)}"]
        expected = {Path(case["file"]).name: case for case in data["cases"]}
        folder = corpus.parent / data.get("corpus", "")
        files = [folder / case["file"] if (folder / case["file"]).exists() else corpus.parent / case["file"] for case in data["cases"]]
        return duality_files(files, names, alone, expected)
    if paths:
        return duality_files(sql_files(paths), names, alone)

    def sequence(name, database, files):
        refused, problem = run_sequence(client, connections[name], f"{prefix}_{name}_{database}", files)
        if problem:
            raise RuntimeError(problem)
        return refused

    problems = duality_updates(root, names, sequence)
    if not problems:
        print(f"every base, released and pending update applies on {' and '.join(names)}")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose SQL checks, promotion and a trial apply")
    parser.add_argument("--root", default=".", help="repository root")
    commands = parser.add_subparsers(dest="command", required=True)
    checking = commands.add_parser("check", help="refuse changes to applied SQL and check new files' names and headers")
    checking.add_argument("--range", required=True, dest="commit_range")
    checking.add_argument("--labels", default="[]", help="the pull request's labels as JSON; a squash label allows changing applied updates")
    promoting = commands.add_parser("promote", help="rename each pending update to the next free dated name")
    promoting.add_argument("--date", help="the UTC date to name them for, YYYY-MM-DD; today when left out")
    promoting.add_argument("--check", action="store_true", help="change nothing, and fail when a pending update is waiting")
    applying = commands.add_parser("apply", help="apply base, released and pending SQL to fresh databases")
    applying.add_argument("--client", default="mysql", help="the mysql or mariadb client")
    applying.add_argument("--host", default="127.0.0.1")
    applying.add_argument("--port", type=int, default=3306)
    applying.add_argument("--user", default="root")
    applying.add_argument("--password-env", default="MYSQL_PWD", help="the environment variable holding the password")
    applying.add_argument("--prefix", default="ambrose_sqlcheck", help="the fresh databases are named <prefix>_<database>")
    applying.add_argument("--keep", action="store_true", help="leave the databases behind for a look")
    pairing = commands.add_parser("duality", help="run the SQL on a MySQL 8 and a MariaDB server and flag every file only one accepts")
    pairing.add_argument("--server", type=server, action="append", required=True, help="NAME=HOST:PORT, given once for each server, such as mysql=127.0.0.1:3306")
    pairing.add_argument("--client", default="mysql", help="the mysql or mariadb client")
    pairing.add_argument("--user", default="root")
    pairing.add_argument("--password-env", default="MYSQL_PWD", help="the environment variable holding the password every server shares")
    pairing.add_argument("--prefix", default="ambrose_duality", help="the fresh databases are named <prefix>_<server>_<database>, or <prefix>_<n> for one file alone")
    pairing.add_argument("--corpus", type=Path, help="a C-81 style corpus JSON whose verdicts and error codes every case must match")
    pairing.add_argument("paths", nargs="*", type=Path, help="SQL files or folders, each file run alone on a fresh database; the repository's own updates in the updater's order when left out")
    args = parser.parse_args(argv)
    root = Path(args.root)

    if args.command == "check":
        try:
            labels = [str(label).lower() for label in (json.loads(args.labels or "[]") or [])]
        except ValueError:
            labels = []
        squash = "squash" in labels or bool(SQUASH_LINE.search(git(root, "log", "--format=%B", args.commit_range.replace("...", ".."))))
        problems = check(changes(root, args.commit_range), lambda path: (root / path).read_text(encoding="utf-8", errors="replace"), squash)
        for problem in problems:
            print(f"ci sql: {problem}", file=sys.stderr)
        print(f"ci sql check: {len(problems)} problem(s)")
        return 1 if problems else 0

    if args.command == "promote":
        today = datetime.date.fromisoformat(args.date) if args.date else datetime.datetime.now(datetime.timezone.utc).date()
        moves = promotions(root, today)
        if args.check:
            for source, _ in moves:
                print(f"ci sql: {source.relative_to(root).as_posix()} is still pending on main; run python apps/ci/ci_sql.py promote and commit the rename", file=sys.stderr)
            print(f"ci sql promote: {len(moves)} pending update(s) waiting")
            return 1 if moves else 0
        for source, target in moves:
            target.parent.mkdir(parents=True, exist_ok=True)
            git(root, "mv", source.relative_to(root).as_posix(), target.relative_to(root).as_posix())
            print(f"{source.relative_to(root).as_posix()} -> {target.relative_to(root).as_posix()}")
        print(f"ci sql promote: {len(moves)} update(s) promoted")
        return 0

    if args.command == "duality":
        if len(args.server) < 2 or len({name for name, _, _ in args.server}) != len(args.server):
            parser.error("duality needs two or more --server options with different names")
        try:
            problems = duality(root, args.client, args.server, args.user, os.environ.get(args.password_env, ""), args.prefix, args.paths, args.corpus)
        except RuntimeError as error:
            problems = [str(error)]
        for problem in problems:
            print(f"ci sql: {problem}", file=sys.stderr)
        print(f"ci sql duality: {len(problems)} problem(s)")
        return 1 if problems else 0

    connection = {"host": args.host, "port": args.port, "user": args.user, "password": os.environ.get(args.password_env, "")}
    failures = apply(root, args.client, connection, args.prefix, args.keep)
    for failure in failures:
        print(f"ci sql: {failure}", file=sys.stderr)
    print(f"ci sql apply: {len(failures)} file(s) failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

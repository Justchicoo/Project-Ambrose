# Project Ambrose by Imjustchico
# Keeps the SQL a pull request adds apart from the SQL already applied: check refuses a change to a file the updater may have applied and checks every new file's name and header, promote renames each pending rev_<unix seconds>_<name>.sql to the next free dated name when it lands on main, and apply runs base, released and pending SQL in the updater's own order on fresh databases through the mysql client, naming the file that fails.
import argparse
import datetime
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


def apply(root, client, connection, prefix, keep=False):
    failures = []
    for database in databases(root):
        schema = f"{prefix}_{database}"
        created = mysql(client, connection, None, f"DROP DATABASE IF EXISTS `{schema}`; CREATE DATABASE `{schema}` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
        if created.returncode:
            return [f"cannot create {schema}: {created.stderr.strip()}"]
        try:
            for file in ordered_files(root, database):
                result = mysql(client, connection, schema, file=file)
                relative = file.relative_to(root).as_posix()
                if result.returncode:
                    lines = result.stderr.decode("utf-8", "replace").strip().splitlines()
                    errors = [line for line in lines if line.startswith("ERROR")]
                    failures.append(f"{relative}: {' '.join(errors or lines)}")
                    break
                print(f"applied {relative}")
        finally:
            if not keep:
                mysql(client, connection, None, f"DROP DATABASE IF EXISTS `{schema}`")
    return failures


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

    connection = {"host": args.host, "port": args.port, "user": args.user, "password": os.environ.get(args.password_env, "")}
    failures = apply(root, args.client, connection, args.prefix, args.keep)
    for failure in failures:
        print(f"ci sql: {failure}", file=sys.stderr)
    print(f"ci sql apply: {len(failures)} file(s) failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

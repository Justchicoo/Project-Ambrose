# Project Ambrose by Imjustchico
# Flags SQL files that only one of MySQL 8 and MariaDB accepts, by running each against fresh databases on both servers.

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

MYSQL_IMAGE = "mysql:8.0"
MARIADB_IMAGE = "mariadb:11"
MYSQL_PORT = 13306
MARIADB_PORT = 13307
ROOT_PASSWORD = "ambrose-c82"


def run(cmd, **kwargs):
    return subprocess.run(cmd, capture_output=True, text=True, **kwargs)


def container(name, image, port):
    run(["docker", "rm", "-f", name], check=False)
    result = run([
        "docker", "run", "-d", "--name", name,
        "-p", f"127.0.0.1:{port}:3306",
        "-e", f"MYSQL_ROOT_PASSWORD={ROOT_PASSWORD}",
        image,
    ])
    if result.returncode != 0:
        raise RuntimeError(f"could not start {name}: {result.stderr.strip()}")
    for _ in range(60):
        ping = run(["docker", "exec", name, "mysqladmin", "-uroot",
                    f"-p{ROOT_PASSWORD}", "ping"])
        if "mysqld is alive" in ping.stdout:
            return
        time.sleep(2)
    raise RuntimeError(f"{name} did not become ready in time")


def stop(name):
    run(["docker", "rm", "-f", name], check=False)


def split_statements(text):
    text = re.sub(r"--[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return [s.strip() for s in text.split(";") if s.strip()]


def verdict(name, port, statements):
    import mysql.connector
    db = f"c82_{int(time.time() * 1000) % 100000000}"
    conn = mysql.connector.connect(host="127.0.0.1", port=port,
                                   user="root", password=ROOT_PASSWORD)
    try:
        cur = conn.cursor()
        cur.execute(f"CREATE DATABASE `{db}`")
        cur.execute(f"USE `{db}`")
        for stmt in statements:
            try:
                cur.execute(stmt)
            except mysql.connector.Error as err:
                return ("error", err.errno, err.msg)
        conn.commit()
        return ("ok", None, None)
    finally:
        try:
            cur.execute(f"DROP DATABASE IF EXISTS `{db}`")
        except Exception:
            pass
        conn.close()


def check_file(path, corpus_expect=None):
    statements = split_statements(path.read_text(encoding="utf-8"))
    mysql = verdict("mysql", MYSQL_PORT, statements)
    mariadb = verdict("mariadb", MARIADB_PORT, statements)
    entry = {
        "file": str(path),
        "mysql": {"verdict": mysql[0], "code": mysql[1]},
        "mariadb": {"verdict": mariadb[0], "code": mariadb[1]},
        "dual": mysql[0] == "ok" and mariadb[0] == "ok",
    }
    if corpus_expect:
        exp = corpus_expect.get(path.name)
        if exp:
            entry["expected"] = exp
            entry["matches_corpus"] = (
                entry["mysql"]["verdict"] == exp["mysql"]["verdict"]
                and entry["mariadb"]["verdict"] == exp["mariadb"]["verdict"]
            )
    return entry


def selftest():
    cases = Path(__file__).parent.parent.parent / "fixtures" / "c81-sql-duality" / "cases"
    assert cases.is_dir(), "C-81 corpus not found"
    files = sorted(cases.glob("*.sql"))
    assert len(files) == 9, f"expected 9 corpus files, got {len(files)}"
    for f in files:
        stmts = split_statements(f.read_text(encoding="utf-8"))
        assert stmts, f"{f.name}: no statements parsed"
        assert all(not s.startswith("--") for s in stmts), f"{f.name}: comment leaked"
    corpus_path = Path(__file__).parent.parent.parent / "fixtures" / "c81-sql-duality-corpus.json"
    corpus = json.loads(corpus_path.read_text(encoding="utf-8"))
    assert corpus["item"] == "C-81" and len(corpus["cases"]) == 9
    names = {Path(c["file"]).name for c in corpus["cases"]}
    assert names == {f.name for f in files}, "corpus names do not match case files"
    kinds = {c["kind"] for c in corpus["cases"]}
    assert kinds == {"clean", "syntax_error", "mariadb_only", "mysql_only"}, f"unexpected kinds: {kinds}"
    print(f"self-test ok: 9 corpus files parse, corpus JSON consistent")


def main():
    parser = argparse.ArgumentParser(
        description="Flag SQL files accepted by only one of MySQL 8 and MariaDB.")
    parser.add_argument("paths", nargs="*",
                        help="SQL files or directories to check")
    parser.add_argument("--corpus",
                        help="C-81 corpus JSON to validate verdicts against")
    parser.add_argument("--json", action="store_true",
                        help="Print the full report as JSON")
    parser.add_argument("--self-test", action="store_true",
                        help="Validate parsing and corpus shape without databases")
    args = parser.parse_args()

    if args.self_test:
        selftest()
        return

    files = []
    for p in args.paths:
        path = Path(p)
        if path.is_dir():
            files.extend(sorted(path.rglob("*.sql")))
        else:
            files.append(path)
    if not files:
        print("no SQL files found", file=sys.stderr)
        sys.exit(2)

    corpus_expect = None
    if args.corpus:
        corpus = json.loads(Path(args.corpus).read_text(encoding="utf-8"))
        corpus_expect = {
            Path(c["file"]).name: c for c in corpus["cases"]
        }

    try:
        container("c82-mysql", MYSQL_IMAGE, MYSQL_PORT)
        container("c82-mariadb", MARIADB_IMAGE, MARIADB_PORT)
        report = [check_file(f, corpus_expect) for f in files]
    finally:
        stop("c82-mysql")
        stop("c82-mariadb")

    flagged = [r for r in report if not r["dual"]]
    mismatched = [r for r in report if r.get("matches_corpus") is False]

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        for r in report:
            mark = "DUAL" if r["dual"] else "ONLY-ONE"
            mysql_v = r["mysql"]["verdict"]
            maria_v = r["mariadb"]["verdict"]
            extra = ""
            if r.get("matches_corpus") is False:
                extra = "  <-- CORPUS MISMATCH"
            print(f"[{mark}] {r['file']}: mysql={mysql_v} mariadb={maria_v}{extra}")
        print(f"\n{len(flagged)} of {len(report)} files accepted by only one server")
        if mismatched:
            print(f"{len(mismatched)} verdicts differ from the corpus")

    if mismatched:
        sys.exit(1)


if __name__ == "__main__":
    main()

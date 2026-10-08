<!-- Project Ambrose by Imjustchico: C-82 SQL duality checker, flagging migrations only one of MySQL 8 and MariaDB accepts. -->

# C-82: ambrose-sql-duality-check

Flags SQL files in `data/sql/updates` that only one of MySQL 8 and MariaDB accepts, so a migration never reaches `main` working on just one server.

## Running it

Needs Python 3.12 and Docker (the `mysql:8.0` and `mariadb:11` images):

```
pip install -r requirements.txt
python check.py ../../../data/sql/updates --json
python check.py path/to/file.sql
python check.py --corpus ../../fixtures/c81-sql-duality-corpus.json ../../fixtures/c81-sql-duality/cases
python check.py --self-test
```

Each file is run against a fresh database on both servers. Files accepted by both print as `DUAL`; files refused by one print as `ONLY-ONE` with each server's verdict. With `--corpus`, verdicts are compared to the C-81 expected answers and mismatches are flagged; `--self-test` validates the SQL parsing and corpus shape without needing databases.

## What it was run against

`--self-test` passes: all 9 C-81 corpus files parse into statements and the corpus JSON is consistent (9 cases, kinds clean/syntax_error/mariadb_only/mysql_only, names matching the case files).

The live dual-server run was not executed in the contributor's environment (no Docker available there); it is designed to run wherever Docker is present, using the same `mysql:8.0` and `mariadb:11` images the C-81 corpus was built against.

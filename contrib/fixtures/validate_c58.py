# Project Ambrose by Imjustchico
# Validates the C-58 synthetic log corpus against the C-29 classifier contract.

import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).parent
sys.path.insert(0, str(ROOT.parent / "tools" / "ambrose-log-value-classifier"))
from classify import Span, classify, check_spans

LEVELS = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"}
CATEGORIES = {
    "accounts",
    "characters",
    "commands.console",
    "server.dbimport",
    "server.gameserver",
    "server.loginserver",
    "server.config",
    "server.logging",
    "network",
    "network.session",
    "server.admin",
    "server.threading",
    "server.loading",
    "sql.updates",
    "sql.driver",
    "network.opcode",
    "sql",
    "sql.sql",
}


def fail(message: str) -> None:
    raise ValueError(message)


def expected_spans(record: dict[str, Any]) -> list[Span]:
    spans = [
        Span(item["value_class"], item["text"], item["start"], item["end"])
        for item in record["value_spans"]
    ]
    check_spans(record["line"], spans)
    return spans


def main() -> int:
    try:
        records = json.loads((ROOT / "c58-log-corpus.json").read_text(encoding="utf-8"))
        if not isinstance(records, dict) or records.get("version") != 1:
            fail("root must be version 1")
        entries = records.get("records")
        if not isinstance(entries, list) or len(entries) != 500:
            fail("C-58 requires exactly 500 records")
        seen: set[str] = set()
        levels: set[str] = set()
        categories: set[str] = set()
        for record in entries:
            record_id = record.get("id")
            if not isinstance(record_id, str) or not record_id or record_id in seen:
                fail(f"invalid or duplicate id: {record_id!r}")
            seen.add(record_id)
            line = record.get("line")
            level = record.get("level")
            category = record.get("category")
            if not isinstance(line, str) or not isinstance(level, str) or not isinstance(category, str):
                fail(f"{record_id}: line, level and category are required")
            if level not in LEVELS or category not in CATEGORIES:
                fail(f"{record_id}: unsupported level or category")
            if line[13:18].strip() != level or f"[{category:<18}]" not in line:
                fail(f"{record_id}: fixed columns do not match metadata")
            expected = expected_spans(record)
            actual = classify(line)
            if actual != expected:
                fail(f"{record_id}: expected {expected!r}, got {actual!r}")
            levels.add(level)
            categories.add(category)
        if levels != LEVELS:
            fail(f"missing levels: {sorted(LEVELS - levels)}")
        if categories != CATEGORIES:
            fail(f"missing categories: {sorted(CATEGORIES - categories)}")
        print(f"C-58 corpus: {len(entries)} records valid across {len(levels)} levels and {len(categories)} categories")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-58 corpus: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

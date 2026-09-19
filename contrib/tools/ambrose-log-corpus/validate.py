# Project Ambrose by Imjustchico
# C-30 validator for the labelled synthetic Ambrose log corpus.
import json
import sys
from pathlib import Path
from typing import Any


REQUIRED_LABELS = {
    "startup",
    "configuration",
    "database",
    "listener",
    "authentication",
    "refusal",
    "protocol",
    "unhandled-message",
    "malformed-message",
    "failure",
    "shutdown",
    "logging",
    "session",
}


class CorpusError(ValueError):
    pass


def load(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise CorpusError(f"cannot read corpus: {exc}") from exc
    if not isinstance(value, dict):
        raise CorpusError("root must be an object")
    return value


def require_string(record: dict[str, Any], key: str) -> str:
    value = record.get(key)
    if not isinstance(value, str) or not value:
        raise CorpusError(f"{record.get('id', '<unknown>')}: {key} must be non-empty text")
    return value


def utf8_span(line: str, start: int, end: int) -> str:
    encoded = line.encode("utf-8")
    if start < 0 or end < start or end > len(encoded):
        raise CorpusError(f"term span {start}:{end} is outside the UTF-8 line")
    return encoded[start:end].decode("utf-8")


def validate(data: dict[str, Any]) -> int:
    if data.get("version") != 1:
        raise CorpusError("unsupported corpus version")
    records = data.get("records")
    if not isinstance(records, list) or not records:
        raise CorpusError("records must be a non-empty list")

    seen_ids: set[str] = set()
    seen_labels: set[str] = set()
    for record in records:
        if not isinstance(record, dict):
            raise CorpusError("every record must be an object")
        record_id = require_string(record, "id")
        if record_id in seen_ids:
            raise CorpusError(f"duplicate record id {record_id}")
        seen_ids.add(record_id)
        line = require_string(record, "line")
        level = require_string(record, "level")
        category = require_string(record, "category")
        if level not in {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"}:
            raise CorpusError(f"{record_id}: unsupported level {level}")
        if f"[{category:<18}]" not in line:
            raise CorpusError(f"{record_id}: category does not match the console line")
        if line[13:18].strip() != level:
            raise CorpusError(f"{record_id}: level does not match the console line")
        labels = record.get("search_labels")
        if not isinstance(labels, list) or not labels or not all(isinstance(x, str) for x in labels):
            raise CorpusError(f"{record_id}: search_labels must be a non-empty string list")
        seen_labels.update(labels)
        terms = record.get("terms")
        if not isinstance(terms, list) or not terms:
            raise CorpusError(f"{record_id}: terms must be a non-empty list")
        for term in terms:
            if not isinstance(term, dict):
                raise CorpusError(f"{record_id}: every term must be an object")
            text = require_string(term, "text")
            start = term.get("start")
            end = term.get("end")
            if not isinstance(start, int) or not isinstance(end, int):
                raise CorpusError(f"{record_id}: term offsets must be integers")
            if utf8_span(line, start, end) != text:
                raise CorpusError(f"{record_id}: span {start}:{end} does not match {text!r}")
    missing = REQUIRED_LABELS - seen_labels
    if missing:
        raise CorpusError(f"missing required labels: {', '.join(sorted(missing))}")
    return len(records)


def main() -> int:
    path = Path(__file__).with_name("corpus.json")
    try:
        count = validate(load(path))
    except CorpusError as exc:
        print(f"C-30 corpus: {exc}", file=sys.stderr)
        return 2
    print(f"C-30 corpus: {count} records valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

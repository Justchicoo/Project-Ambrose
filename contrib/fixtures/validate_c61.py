# Project Ambrose by Imjustchico
# Validates the C-61 duration-string corpus against the parser contract used by AccountCommands and Ambrose::ParseDuration.

import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
FILE = ROOT / "c61-duration-corpus.json"

UNIT_TO_SECONDS = {"s": 1, "m": 60, "h": 3600, "d": 86400, "w": 604800}
MAX_SECONDS = (1 << 63) - 1


def fail(message: str) -> None:
    raise ValueError(message)


def parse_duration(text: str) -> int | None:
    if text.lower() in {"perm", "permanent"}:
        return 0
    if text == "":
        return None
    total = 0
    value = 0
    digits = False
    limit = MAX_SECONDS

    def add(unit: int) -> bool:
        nonlocal total, value, digits
        if not digits or value > limit // unit or total > limit - value * unit:
            return False
        total += value * unit
        value = 0
        digits = False
        return True

    for char in text:
        if "0" <= char <= "9":
            digit = ord(char) - ord("0")
            if value > (limit - digit) // 10:
                return None
            value = value * 10 + digit
            digits = True
            continue
        if char.lower() not in UNIT_TO_SECONDS:
            return None
        unit = UNIT_TO_SECONDS[char.lower()]
        if not add(unit):
            return None
    if digits and not add(1):
        return None
    if total == 0:
        return None
    return total


def main() -> int:
    try:
        payload = json.loads(FILE.read_text(encoding="utf-8"))
        if not isinstance(payload, dict) or payload.get("version") != 1:
            fail("root must be a version-1 object")
        entries = payload.get("entries")
        if not isinstance(entries, list):
            fail("entries must be a list")
        seen_ids: set[str] = set()
        for index, entry in enumerate(entries):
            if not isinstance(entry, dict):
                fail(f"entry #{index} must be an object")
            entry_id = entry.get("id")
            if not isinstance(entry_id, str) or not entry_id or entry_id in seen_ids:
                fail(f"entry #{index} has an invalid or duplicate id")
            seen_ids.add(entry_id)
            text = entry.get("input")
            if not isinstance(text, str):
                fail(f"{entry_id}: input must be a string")
            accepted = entry.get("accepted")
            if not isinstance(accepted, bool):
                fail(f"{entry_id}: accepted must be a boolean")
            expected_seconds = entry.get("seconds")
            if expected_seconds is not None and not isinstance(expected_seconds, int):
                fail(f"{entry_id}: seconds must be an integer or null")
            permanent = text.lower() in ("perm", "permanent")
            if entry.get("special") is not None and entry.get("special") != "permanent":
                fail(f"{entry_id}: special must be absent or 'permanent'")
            if permanent != (entry.get("special") == "permanent"):
                fail(f"{entry_id}: only perm and permanent carry special 'permanent'")
            if accepted:
                if not isinstance(expected_seconds, int):
                    fail(f"{entry_id}: an accepted entry must say how many seconds it yields")
                actual = parse_duration(text)
                if actual != expected_seconds:
                    fail(f"{entry_id}: expected {expected_seconds!r} for {text!r}, got {actual!r}")
            else:
                if parse_duration(text) is not None:
                    fail(f"{entry_id}: {text!r} must be refused")
        print(f"C-61 corpus: {len(entries)} duration strings valid")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-61 corpus: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

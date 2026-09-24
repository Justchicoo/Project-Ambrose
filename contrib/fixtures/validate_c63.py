# Project Ambrose by Imjustchico
# Validates the C-63 console-layout corpus against LogMessage's terminal rendering contract.

import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
FILE = ROOT / "c63-console-layout-corpus.json"
LEVELS = {
    "trace": "TRACE",
    "debug": "DEBUG",
    "info": "INFO ",
    "warn": "WARN ",
    "error": "ERROR",
    "fatal": "FATAL",
}


def fail(message: str) -> None:
    raise ValueError(message)


def require_string(value: Any, name: str) -> str:
    if not isinstance(value, str):
        fail(f"{name} must be a string")
    return value


def category_column(category: str, width: int) -> str:
    if len(category) > width:
        keep = width - 2
        left = (keep + 1) // 2
        right = keep - left
        category = category[:left] + ".." + category[-right:]
    return f"[{category}{' ' * max(0, width - len(category))}] "


def render_case(case: dict[str, Any], timestamp: str, width: int) -> list[str]:
    level = require_string(case.get("level"), f"{case['id']}.level")
    if level not in LEVELS:
        fail(f"{case['id']}: unsupported level {level!r}")
    category = require_string(case.get("category"), f"{case['id']}.category")
    message = require_string(case.get("message"), f"{case['id']}.message")
    prefix = f"{timestamp} {LEVELS[level]} {category_column(category, width)}"
    lines = message.removesuffix("\n").removesuffix("\r").split("\n")
    rendered: list[str] = []
    for line in lines:
        if line.endswith("\r"):
            line = line[:-1]
        rendered.append(prefix.rstrip() if not line else prefix + line)
    return rendered


def main() -> int:
    try:
        payload = json.loads(FILE.read_text(encoding="utf-8"))
        if not isinstance(payload, dict) or payload.get("version") != 1:
            fail("root must be a version-1 object")
        layout = payload.get("layout")
        if not isinstance(layout, dict):
            fail("layout must be an object")
        timestamp = require_string(layout.get("timestamp"), "layout.timestamp")
        width = layout.get("category_width")
        if not isinstance(width, int) or not 1 <= width <= 64:
            fail("layout.category_width must be an integer from 1 to 64")
        if layout.get("repeat_category") is not True:
            fail("the corpus requires repeated categories for independent line checks")
        cases = payload.get("cases")
        if not isinstance(cases, list) or not cases:
            fail("cases must be a non-empty list")
        seen: set[str] = set()
        covered = {"wide": False, "multi": False, "suffix": False, "empty_category": False}
        for index, case in enumerate(cases):
            if not isinstance(case, dict):
                fail(f"case #{index} must be an object")
            case_id = case.get("id")
            if not isinstance(case_id, str) or not case_id or case_id in seen:
                fail(f"case #{index} has an invalid or duplicate id")
            seen.add(case_id)
            if not require_string(case.get("description"), f"{case_id}.description").strip():
                fail(f"{case_id}: description must not be empty")
            actual = render_case(case, timestamp, width)
            expected = case.get("expected_lines")
            if not isinstance(expected, list) or any(not isinstance(line, str) for line in expected):
                fail(f"{case_id}.expected_lines must be a list of strings")
            if actual != expected:
                fail(f"{case_id}: expected {expected!r}, got {actual!r}")
            category = require_string(case.get("category"), f"{case_id}.category")
            message = require_string(case.get("message"), f"{case_id}.message")
            covered["wide"] |= len(category) > width
            covered["multi"] |= "\n" in message
            covered["empty_category"] |= category == ""
            suffix = case.get("expected_suffix")
            if suffix is not None:
                if not isinstance(suffix, str) or not actual[-1].endswith(suffix):
                    fail(f"{case_id}: expected suffix is not at the end of the rendered line")
                covered["suffix"] = True
        missing = [name for name, present in covered.items() if not present]
        if missing:
            fail(f"corpus does not cover required cases: {', '.join(missing)}")
        print(f"C-63 corpus: {len(cases)} console-layout cases valid")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-63 corpus: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

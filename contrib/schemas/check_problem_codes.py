# Project Ambrose by Imjustchico
# Validates the C-60 standard problem code catalog used by the dashboard and operator docs, and that it lists exactly the codes the admin API registers.

import json
import re
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
CATALOG = ROOT / "problem-codes-v1.json"
REGISTRY = ROOT.parent.parent / "src" / "server" / "shared" / "Admin" / "AdminCapabilities.h"

EXPECTED = {
    "install_missing": {
        "subject": "client",
        "severity": "critical",
        "message": "No client installation was found",
        "fix_route": "client",
    },
    "type_dump_missing": {
        "subject": "type dump",
        "severity": "warning",
        "message": "No type dump is in use",
        "fix_route": "client",
    },
    "type_dump_stale": {
        "subject": "type dump",
        "severity": "warning",
        "message": "The type dump does not match the installed client program",
        "fix_route": "client",
    },
    "database_unreachable": {
        "subject": "database",
        "severity": "critical",
        "message": "A database the app needs cannot be reached",
        "fix_route": "settings",
    },
    "schema_update_pending": {
        "subject": "database",
        "severity": "warning",
        "message": "A database update is waiting to be applied",
        "fix_route": "servers",
    },
    "revision_not_allowed": {
        "subject": "client",
        "severity": "warning",
        "message": "The client's revision is not one Login.AllowedRevision lists",
        "fix_route": "client",
    },
}


def fail(message: str) -> None:
    raise ValueError(message)


def registered_codes() -> set[str]:
    text = REGISTRY.read_text(encoding="utf-8")
    block = re.search(r"namespace AdminProblemCodes\s*\{(.*?)\}", text, re.S)
    if not block:
        fail(f"{REGISTRY}: no AdminProblemCodes namespace")
    return set(re.findall(r'std::string_view\s+\w+\s*=\s*"([a-z_]+)"', block.group(1)))


def main() -> int:
    try:
        payload = json.loads(CATALOG.read_text(encoding="utf-8"))
        if not isinstance(payload, dict):
            fail("root must be an object")
        if payload.get("schema") != 1:
            fail("catalog schema must be 1")
        problems = payload.get("problems")
        if not isinstance(problems, list):
            fail("problems must be a list")
        registered = registered_codes()
        if registered != set(EXPECTED):
            fail(f"the server registers {sorted(registered - set(EXPECTED))} that the catalog lacks, and the catalog lists {sorted(set(EXPECTED) - registered)} that the server does not register")
        if len(problems) != len(EXPECTED):
            fail(f"expected {len(EXPECTED)} problem entries, got {len(problems)}")
        seen: set[str] = set()
        for index, entry in enumerate(problems):
            if not isinstance(entry, dict):
                fail(f"problem #{index} must be an object")
            code = entry.get("code")
            if not isinstance(code, str) or not code:
                fail(f"problem #{index} has no valid code")
            if code in seen:
                fail(f"duplicate problem code: {code}")
            seen.add(code)
            if code not in EXPECTED:
                fail(f"unsupported code: {code}")
            expected = EXPECTED[code]
            for key in ["code", "subject", "severity", "message", "operator_action", "fix_route"]:
                if key not in entry:
                    fail(f"{code}: missing {key}")
            if entry["subject"] != expected["subject"]:
                fail(f"{code}: wrong subject")
            if entry["severity"] not in {"critical", "warning", "info"}:
                fail(f"{code}: severity must be critical, warning or info")
            if entry["severity"] != expected["severity"]:
                fail(f"{code}: wrong severity")
            if entry["message"] != expected["message"]:
                fail(f"{code}: wrong message")
            if entry["fix_route"] != expected["fix_route"]:
                fail(f"{code}: wrong fix route")
            if not isinstance(entry["operator_action"], str) or not entry["operator_action"].strip():
                fail(f"{code}: operator_action must be a non-empty string")
        if seen != set(EXPECTED):
            missing = sorted(set(EXPECTED) - seen)
            extra = sorted(seen - set(EXPECTED))
            fail(f"catalog missing or has extra entries: missing={missing}, extra={extra}")
        print(f"C-60 catalog: {len(problems)} standard problem codes valid")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-60 catalog: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

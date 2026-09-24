# Project Ambrose by Imjustchico
# Validates the C-62 configuration-layering corpus against ConfigMgr's precedence contract.

import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
FILE = ROOT / "c62-config-layering-corpus.json"


def fail(message: str) -> None:
    raise ValueError(message)


def to_environment_name(key: str) -> str:
    name = "AMBROSE_"
    for index, char in enumerate(key):
        if char in "._-":
            if not name.endswith("_"):
                name += "_"
            continue
        next_is_lower = index + 1 < len(key) and key[index + 1].islower()
        if char.isupper() and index > 0 and (key[index - 1].islower() or key[index - 1].isdigit() or
                                             (key[index - 1].isupper() and next_is_lower)):
            if not name.endswith("_"):
                name += "_"
        name += char.upper()
    return name.rstrip("_")


def require_mapping(value: Any, name: str) -> dict[str, str]:
    if not isinstance(value, dict):
        fail(f"{name} must be an object")
    if any(not isinstance(key, str) or not isinstance(item, str) for key, item in value.items()):
        fail(f"{name} must map strings to strings")
    return value


def require_modules(value: Any, name: str) -> list[dict[str, Any]]:
    if not isinstance(value, list):
        fail(f"{name} must be a list")
    result: list[dict[str, Any]] = []
    for index, module in enumerate(value):
        if not isinstance(module, dict):
            fail(f"{name}[{index}] must be an object")
        filename = module.get("file")
        if not isinstance(filename, str) or not filename:
            fail(f"{name}[{index}] must have a file")
        result.append({"file": filename, "values": require_mapping(module.get("values"), f"{name}[{index}].values")})
    return result


def require_environment(value: Any, name: str) -> list[dict[str, str]]:
    if not isinstance(value, list):
        fail(f"{name} must be a list")
    result: list[dict[str, str]] = []
    for index, variable in enumerate(value):
        if not isinstance(variable, dict):
            fail(f"{name}[{index}] must be an object")
        variable_name = variable.get("name")
        key = variable.get("key")
        item = variable.get("value")
        if not isinstance(variable_name, str) or not variable_name.startswith("AMBROSE_"):
            fail(f"{name}[{index}].name must use the AMBROSE_ prefix")
        if not isinstance(key, str) or not key:
            fail(f"{name}[{index}].key must be non-empty")
        if not isinstance(item, str):
            fail(f"{name}[{index}].value must be a string")
        applies = variable.get("applies", True)
        if not isinstance(applies, bool):
            fail(f"{name}[{index}].applies must be a boolean")
        if applies != (variable_name == to_environment_name(key)):
            fail(f"{name}[{index}].name {variable_name} is {'not ' if applies else ''}the ConfigMgr name for {key}, so applies must be {not applies}")
        if applies:
            result.append({"name": variable_name, "key": key, "value": item})
    return result


def apply(values: dict[str, str], layer: dict[str, str]) -> None:
    values.update(layer)


def effective(case: dict[str, Any]) -> dict[str, str]:
    values: dict[str, str] = {}
    apply(values, require_mapping(case.get("defaults"), "defaults"))
    for module in sorted(require_modules(case.get("module_defaults"), "module_defaults"), key=lambda item: item["file"]):
        apply(values, module["values"])
    apply(values, require_mapping(case.get("config"), "config"))
    for module in sorted(require_modules(case.get("module_config"), "module_config"), key=lambda item: item["file"]):
        apply(values, module["values"])
    for variable in require_environment(case.get("environment"), "environment"):
        values[variable["key"]] = variable["value"]
    apply(values, require_mapping(case.get("overrides"), "overrides"))
    return values


def main() -> int:
    try:
        payload = json.loads(FILE.read_text(encoding="utf-8"))
        if not isinstance(payload, dict) or payload.get("version") != 1:
            fail("root must be a version-1 object")
        cases = payload.get("cases")
        if not isinstance(cases, list) or not cases:
            fail("cases must be a non-empty list")
        seen: set[str] = set()
        for index, case in enumerate(cases):
            if not isinstance(case, dict):
                fail(f"case #{index} must be an object")
            case_id = case.get("id")
            if not isinstance(case_id, str) or not case_id or case_id in seen:
                fail(f"case #{index} has an invalid or duplicate id")
            seen.add(case_id)
            if not isinstance(case.get("description"), str) or not case["description"].strip():
                fail(f"{case_id}: description must be non-empty")
            expected = require_mapping(case.get("expected"), f"{case_id}.expected")
            actual = effective(case)
            if actual != expected:
                fail(f"{case_id}: expected {expected!r}, got {actual!r}")
            require_environment(case.get("environment"), f"{case_id}.environment")
            require_mapping(case.get("overrides"), f"{case_id}.overrides")
        print(f"C-62 corpus: {len(cases)} configuration-layering cases valid")
        return 0
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print(f"C-62 corpus: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

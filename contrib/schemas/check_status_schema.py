# Project Ambrose by Imjustchico
# Checks status fixtures and prevents status properties from being removed or renamed.

import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).parent


def load(name: str) -> dict[str, Any]:
    with (ROOT / name).open(encoding="utf-8") as stream:
        return json.load(stream)


def check_type(value: Any, definition: dict[str, Any], path: str) -> None:
    expected = definition.get("type")
    types = expected if isinstance(expected, list) else [expected]
    valid = {
        "null": value is None,
        "object": isinstance(value, dict),
        "array": isinstance(value, list),
        "string": isinstance(value, str),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "number": isinstance(value, (int, float)) and not isinstance(value, bool),
    }
    if not any(valid.get(kind, False) for kind in types):
        raise ValueError(f"{path}: expected one of {types}")
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in definition and value < definition["minimum"]:
            raise ValueError(f"{path}: below minimum")
    if isinstance(value, str) and "minLength" in definition and len(value) < definition["minLength"]:
        raise ValueError(f"{path}: shorter than minLength")


def validate(value: Any, definition: dict[str, Any], path: str = "$") -> None:
    check_type(value, definition, path)
    if value is None:
        return
    if isinstance(value, dict):
        required = definition.get("required", [])
        missing = [name for name in required if name not in value]
        if missing:
            raise ValueError(f"{path}: missing required fields {', '.join(missing)}")
        properties = definition.get("properties", {})
        if definition.get("additionalProperties") is False:
            unknown = [name for name in value if name not in properties]
            if unknown:
                raise ValueError(f"{path}: unknown fields {', '.join(unknown)}")
        for name, child in value.items():
            if name in properties:
                validate(child, properties[name], f"{path}.{name}")
    elif isinstance(value, list) and "items" in definition:
        for index, child in enumerate(value):
            validate(child, definition["items"], f"{path}[{index}]")


def required_names(definition: dict[str, Any], path: str = "$") -> set[tuple[str, str]]:
    names = {(path, name) for name in definition.get("required", [])}
    for name, child in definition.get("properties", {}).items():
        names.update(required_names(child, f"{path}.{name}"))
    if "items" in definition:
        names.update(required_names(definition["items"], f"{path}[]"))
    return names


def property_names(definition: dict[str, Any], path: str = "$") -> set[tuple[str, str]]:
    names = {(path, name) for name in definition.get("properties", {})}
    for name, child in definition.get("properties", {}).items():
        names.update(property_names(child, f"{path}.{name}"))
    if "items" in definition:
        names.update(property_names(definition["items"], f"{path}[]"))
    return names


def main() -> None:
    v1 = load("status-v1.json")
    v2 = load("status-v2.json")
    for fixture_name in ("status-gameserver.json", "status-loginserver.json"):
        validate(load(fixture_name), v1, fixture_name)
    removed_required = required_names(v1) - required_names(v2)
    removed_properties = property_names(v1) - property_names(v2)
    if removed_required:
        raise ValueError(f"v2 removed required fields: {sorted(removed_required)}")
    if removed_properties:
        raise ValueError(f"v2 removed properties: {sorted(removed_properties)}")
    print("validated 2 corrected status fixtures; v2 preserves all v1 properties")


if __name__ == "__main__":
    main()

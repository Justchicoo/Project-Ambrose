# Project Ambrose by Imjustchico
# Validates apps and capabilities fixtures and keeps v2 additive.

import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).parent


def load(name: str) -> Any:
    with (ROOT / name).open(encoding="utf-8") as stream:
        return json.load(stream)


def check_type(value: Any, definition: dict[str, Any], path: str) -> None:
    expected = definition.get("type")
    types = expected if isinstance(expected, list) else [expected]
    kind_checks = {
        "null": value is None,
        "object": isinstance(value, dict),
        "array": isinstance(value, list),
        "string": isinstance(value, str),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "number": isinstance(value, (int, float)) and not isinstance(value, bool),
    }
    if not any(kind_checks.get(kind, False) for kind in types):
        raise ValueError(f"{path}: expected one of {types}")
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        minimum = definition.get("minimum")
        if minimum is not None and value < minimum:
            raise ValueError(f"{path}: below minimum")
    if isinstance(value, str):
        minimum_length = definition.get("minLength")
        if minimum_length is not None and len(value) < minimum_length:
            raise ValueError(f"{path}: shorter than minLength")


def validate(value: Any, definition: dict[str, Any], path: str = "$") -> None:
    check_type(value, definition, path)
    if value is None:
        return
    if isinstance(value, dict):
        required = definition.get("required", [])
        missing = [name for name in required if name not in value]
        if missing:
            raise ValueError(f"{path}: missing required {missing}")
        properties = definition.get("properties", {})
        additional = definition.get("additionalProperties")
        if additional is False:
            unknown = [name for name in value if name not in properties]
            if unknown:
                raise ValueError(f"{path}: unknown props {unknown}")
        for name, child in value.items():
            if name in properties:
                validate(child, properties[name], f"{path}.{name}")
    elif isinstance(value, list):
        if "items" in definition:
            for index, child in enumerate(value):
                validate(child, definition["items"], f"{path}[{index}]")


def property_names(definition: dict[str, Any], prefix: str = "$") -> set[tuple[str, str]]:
    names = {(prefix, name) for name in definition.get("properties", {})}
    for name, child in definition.get("properties", {}).items():
        names.update(property_names(child, f"{prefix}.{name}"))
    if "items" in definition:
        names.update(property_names(definition["items"], f"{prefix}[]"))
    return names


def main() -> None:
    apps_v1 = load("apps-v1.json")
    apps_v2 = load("apps-v2.json")
    capabilities_v1 = load("capabilities-v1.json")
    capabilities_v2 = load("capabilities-v2.json")

    validate(load("apps-gameserver.json"), apps_v1, "apps-gameserver.json")
    validate(load("apps-loginserver.json"), apps_v1, "apps-loginserver.json")
    validate(load("capabilities-gameserver.json"), capabilities_v1, "capabilities-gameserver.json")
    validate(load("capabilities-loginserver.json"), capabilities_v1, "capabilities-loginserver.json")

    if property_names(apps_v1) - property_names(apps_v2):
        raise ValueError("apps-v2 removed a property from apps-v1")
    if property_names(capabilities_v1) - property_names(capabilities_v2):
        raise ValueError("capabilities-v2 removed a property from capabilities-v1")

    print("validated 2 apps fixtures and 2 capabilities fixtures; v2 remains additive")


if __name__ == "__main__":
    main()

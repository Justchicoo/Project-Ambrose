# Project Ambrose by Imjustchico
# C-32 standalone contrast auditor for semantic design tokens.
import argparse
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


TEXT_TOKENS = ("fg-body", "fg-muted", "fg-faint")
SURFACE_TOKENS = ("surface-page", "surface-card", "surface-sunken", "surface-chrome")
PAIRS = (
    *((token, surface, 4.5) for token in TEXT_TOKENS for surface in SURFACE_TOKENS),
    *(("edge-control", surface, 3.0) for surface in SURFACE_TOKENS),
)


@dataclass(frozen=True)
class Result:
    theme: str
    pair: str
    foreground: str
    background: str
    ratio: float
    threshold: float
    verdict: str


class TokenError(ValueError):
    pass


def load_tokens(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise TokenError(f"cannot read token file {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise TokenError("token file root must be an object")
    return data


def semantic_tokens(data: dict[str, Any]) -> dict[str, dict[str, Any]]:
    semantic = data.get("semantic")
    if not isinstance(semantic, dict) or not isinstance(semantic.get("color"), dict):
        raise TokenError("token file must contain semantic.color")
    return semantic["color"]


def resolve_reference(value: str, data: dict[str, Any]) -> str:
    if not (value.startswith("{") and value.endswith("}")):
        return value
    current: Any = data
    for part in value[1:-1].split("."):
        if not isinstance(current, dict) or part not in current:
            raise TokenError(f"unresolved token reference {value}")
        current = current[part]
    if isinstance(current, dict) and isinstance(current.get("$value"), str):
        current = current["$value"]
    if not isinstance(current, str):
        raise TokenError(f"token reference {value} does not resolve to a color")
    return resolve_reference(current, data)


def token_color(
    name: str, theme: str, colors: dict[str, dict[str, Any]], data: dict[str, Any]
) -> str:
    token = colors.get(name)
    if not isinstance(token, dict):
        raise TokenError(f"missing semantic color {name}")
    value: Any = token.get("$value")
    if theme == "light":
        extensions = token.get("$extensions")
        if isinstance(extensions, dict) and "ambrose.light" in extensions:
            value = extensions["ambrose.light"]
    if not isinstance(value, str):
        raise TokenError(f"semantic color {name} has no {theme} value")
    value = resolve_reference(value, data)
    if len(value) != 7 or not value.startswith("#"):
        raise TokenError(f"semantic color {name} is not a #RRGGBB value")
    try:
        int(value[1:], 16)
    except ValueError as exc:
        raise TokenError(f"semantic color {name} is not a valid #RRGGBB value") from exc
    return value.upper()


def channel(value: int) -> float:
    normalized = value / 255
    return normalized / 12.92 if normalized <= 0.04045 else ((normalized + 0.055) / 1.055) ** 2.4


def luminance(color: str) -> float:
    red = channel(int(color[1:3], 16))
    green = channel(int(color[3:5], 16))
    blue = channel(int(color[5:7], 16))
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def contrast(first: str, second: str) -> float:
    lighter = max(luminance(first), luminance(second))
    darker = min(luminance(first), luminance(second))
    return (lighter + 0.05) / (darker + 0.05)


def audit(data: dict[str, Any]) -> list[Result]:
    colors = semantic_tokens(data)
    results: list[Result] = []
    for theme in ("dark", "light"):
        for foreground_name, background_name, threshold in PAIRS:
            foreground = token_color(foreground_name, theme, colors, data)
            background = token_color(background_name, theme, colors, data)
            ratio = contrast(foreground, background)
            results.append(
                Result(
                    theme,
                    f"{foreground_name} on {background_name}",
                    foreground,
                    background,
                    round(ratio, 2),
                    threshold,
                    "PASS" if ratio >= threshold else "FAIL",
                )
            )
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Audit Ambrose semantic color contrast.")
    parser.add_argument("tokens", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    args = parser.parse_args(argv)
    try:
        results = audit(load_tokens(args.tokens))
    except TokenError as exc:
        print(f"contrast auditor: {exc}", file=sys.stderr)
        return 2
    if args.as_json:
        print(json.dumps([asdict(result) for result in results], indent=2))
    else:
        print("theme\tpair\tratio\tthreshold\tverdict")
        for result in results:
            print(
                f"{result.theme}\t{result.pair}\t{result.ratio:.2f}\t"
                f"{result.threshold:.2f}\t{result.verdict}"
            )
    return 1 if any(result.verdict == "FAIL" for result in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())

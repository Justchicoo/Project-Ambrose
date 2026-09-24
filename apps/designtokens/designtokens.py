#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Generates the stylesheet, the TypeScript module, the terminal header and the doc/DESIGN.md tables from design/tokens.json, and refuses a palette whose documented pair falls under its ratio.
import argparse
import json
import os
import sys

BRAND = "Project Ambrose by Imjustchico"
THEMES = ("dark", "light")
LIGHT_KEY = "ambrose.light"
LEADING_KEY = "ambrose.leading"
ANSI_KEY = {"dark": "ambrose.ansi16", "light": "ambrose.ansi16-light"}

TOKENS_PATH = "design/tokens.json"
CSS_PATH = "packages/ui/src/tokens/tokens.css"
VARIABLES_PATH = "packages/ui/src/tokens/variables.css"
TS_PATH = "packages/ui/src/tokens/tokens.ts"
HEADER_PATH = "src/common/Design/Tokens.h"
DESIGN_PATH = "doc/DESIGN.md"

DENSITY_COMPACT = "0.85"

ANSI_16 = [
    "#000000", "#CD0000", "#00CD00", "#CDCD00", "#0000EE", "#CD00CD", "#00CDCD", "#E5E5E5",
    "#7F7F7F", "#FF0000", "#00FF00", "#FFFF00", "#5C5CFF", "#FF00FF", "#00FFFF", "#FFFFFF",
]
ANSI_NAMES = [
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
    "bright-black", "bright-red", "bright-green", "bright-yellow", "bright-blue", "bright-magenta", "bright-cyan", "bright-white",
]
CUBE_LEVELS = [0, 95, 135, 175, 215, 255]

DALTON = {
    "protanopia": ((0.152286, 1.052583, -0.204868), (0.114503, 0.786281, 0.099216), (-0.003882, -0.048116, 1.051998)),
    "deuteranopia": ((0.367322, 0.860646, -0.227968), (0.280085, 0.672501, 0.047413), (-0.011820, 0.042940, 0.968881)),
    "tritanopia": ((1.255528, -0.076749, -0.178779), (-0.078411, 0.930809, 0.147602), (0.004733, 0.691367, 0.303900)),
}


class GenerationRefused(Exception):
    pass


def parse_hex(value):
    text = value.strip()
    if not text.startswith("#") or len(text) != 7:
        raise GenerationRefused(f"'{value}' is not a six-digit hex colour")
    try:
        return tuple(int(text[index:index + 2], 16) for index in (1, 3, 5))
    except ValueError:
        raise GenerationRefused(f"'{value}' is not a six-digit hex colour")


def format_hex(rgb):
    return "#%02X%02X%02X" % tuple(max(0, min(255, int(round(channel)))) for channel in rgb)


def to_linear(channel):
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def from_linear(value):
    clamped = max(0.0, min(1.0, value))
    encoded = 12.92 * clamped if clamped <= 0.0031308 else 1.055 * (clamped ** (1 / 2.4)) - 0.055
    return encoded * 255.0


def luminance(value):
    red, green, blue = (to_linear(channel) for channel in parse_hex(value))
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def contrast(first, second):
    one, two = luminance(first), luminance(second)
    high, low = max(one, two), min(one, two)
    return (high + 0.05) / (low + 0.05)


def lab(value):
    red, green, blue = (to_linear(channel) for channel in parse_hex(value))
    x = (0.4124 * red + 0.3576 * green + 0.1805 * blue) / 0.95047
    y = 0.2126 * red + 0.7152 * green + 0.0722 * blue
    z = (0.0193 * red + 0.1192 * green + 0.9505 * blue) / 1.08883

    def pivot(component):
        return component ** (1 / 3) if component > 216 / 24389 else (841 / 108) * component + 4 / 29

    fx, fy, fz = pivot(x), pivot(y), pivot(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def simulate(value, kind):
    linear = [to_linear(channel) for channel in parse_hex(value)]
    matrix = DALTON[kind]
    return format_hex([from_linear(sum(row[index] * linear[index] for index in range(3))) for row in matrix])


def separation(first, second):
    return min(
        sum((one - two) ** 2 for one, two in zip(lab(first if kind is None else simulate(first, kind)),
                                                 lab(second if kind is None else simulate(second, kind)))) ** 0.5
        for kind in (None,) + tuple(DALTON))


def nearest(value, palette):
    target = [to_linear(channel) for channel in parse_hex(value)]
    best, best_distance = 0, None
    for index, candidate in enumerate(palette):
        linear = [to_linear(channel) for channel in parse_hex(candidate)]
        distance = sum((one - two) ** 2 for one, two in zip(target, linear))
        if best_distance is None or distance < best_distance:
            best, best_distance = index, distance
    return best


def xterm256():
    palette = list(ANSI_16)
    for red in CUBE_LEVELS:
        for green in CUBE_LEVELS:
            for blue in CUBE_LEVELS:
                palette.append(format_hex((red, green, blue)))
    for step in range(24):
        level = 8 + 10 * step
        palette.append(format_hex((level, level, level)))
    return palette


XTERM_256 = xterm256()


def walk(group, prefix, out):
    for key, value in group.items():
        if key.startswith("$"):
            continue
        path = f"{prefix}.{key}" if prefix else key
        if isinstance(value, dict) and "$value" in value:
            out[path] = value
        elif isinstance(value, dict):
            walk(value, path, out)
    return out


class Tokens:
    def __init__(self, document):
        self.document = document
        self.flat = walk(document, "", {})
        self.contrast = document.get("contrast", {})

    def raw(self, path, theme):
        entry = self.flat.get(path)
        if entry is None:
            raise GenerationRefused(f"'{path}' is not a token in {TOKENS_PATH}")
        if theme == "light":
            override = entry.get("$extensions", {}).get(LIGHT_KEY)
            if override is not None:
                return override
        return entry["$value"]

    def resolve(self, path, theme="dark", seen=None):
        value = self.raw(path, theme)
        seen = seen or []
        while isinstance(value, str) and value.startswith("{") and value.endswith("}"):
            reference = value[1:-1]
            if reference in seen:
                raise GenerationRefused(f"'{path}' refers to itself through {' -> '.join(seen)}")
            seen = seen + [reference]
            value = self.raw(reference, theme)
        return value

    def leaf_names(self, group):
        prefix = group + "."
        return [path[len(prefix):] for path in self.flat if path.startswith(prefix) and "." not in path[len(prefix):]]

    def colors(self, group, theme):
        return [(name, self.resolve(f"{group}.{name}", theme)) for name in self.leaf_names(group)]

    def ansi16(self, path, theme):
        extensions = self.flat[path].get("$extensions", {})
        name = extensions.get(ANSI_KEY[theme])
        if name is None:
            raise GenerationRefused(f"'{path}' has no {ANSI_KEY[theme]}, so the terminal has no colour for it")
        if name not in ANSI_NAMES:
            raise GenerationRefused(f"'{path}' names '{name}', which is not one of the sixteen terminal colours")
        return ANSI_NAMES.index(name)

    def description(self, path):
        return self.flat[path].get("$description", "")


def check_contrast(tokens):
    failures = []
    pairs = []
    for rule in tokens.contrast.get("rules", []):
        for foreground in rule["foregrounds"]:
            for background in rule["backgrounds"]:
                for theme in THEMES:
                    front = tokens.resolve(foreground, theme)
                    back = tokens.resolve(background, theme)
                    ratio = contrast(front, back)
                    pairs.append({
                        "foreground": foreground,
                        "background": background,
                        "theme": theme,
                        "ratio": round(ratio, 2),
                        "minimum": rule["minimum"],
                        "note": rule["note"],
                    })
                    if ratio + 1e-9 < rule["minimum"]:
                        failures.append(
                            f"{theme}: {foreground} ({front}) on {background} ({back}) reaches "
                            f"{ratio:.2f}:1, under the {rule['minimum']}:1 needed for {rule['note'].lower()}")
    minimum = tokens.contrast.get("separation")
    if minimum:
        names = tokens.leaf_names("series.color")
        for theme in THEMES:
            for index, first in enumerate(names):
                for second in names[index + 1:]:
                    one = tokens.resolve(f"series.color.{first}", theme)
                    two = tokens.resolve(f"series.color.{second}", theme)
                    apart = separation(one, two)
                    if apart + 1e-9 < minimum:
                        failures.append(
                            f"{theme}: series.color.{first} ({one}) and series.color.{second} ({two}) stay only "
                            f"{apart:.1f} apart, under the {minimum} needed for a colour-blind reader to tell them apart")
    if failures:
        raise GenerationRefused("\n".join(failures))
    return pairs


def css_font(value):
    return ", ".join(name if " " not in name else f'"{name}"' for name in value)


def generate_css(tokens):
    lines = [
        "/*",
        f" * {BRAND}",
        " * Generated from design/tokens.json: the deleted stock palette, the Tailwind theme, the semantic layer and the light remap.",
        " */",
        "",
        "@theme {",
        "    --color-*: initial;",
        "    --font-*: initial;",
        "    --text-*: initial;",
        "    --spacing-*: initial;",
        "    --radius-*: initial;",
        "    --ease-*: initial;",
        "    --shadow-*: initial;",
        "    --blur-*: initial;",
        "    --animate-*: initial;",
        "",
        "    --color-transparent: transparent;",
        "    --color-current: currentcolor;",
        "    --color-inherit: inherit;",
        "",
    ]
    for name, _ in tokens.colors("semantic.color", "dark"):
        lines.append(f"    --color-{name}: var(--ambrose-color-{name});")
    for name, _ in tokens.colors("component.color", "dark"):
        lines.append(f"    --color-{name}: var(--ambrose-color-{name});")
    for name, _ in tokens.colors("series.color", "dark"):
        lines.append(f"    --color-series-{name}: var(--ambrose-color-series-{name});")
    lines.append("")
    for name in tokens.leaf_names("primitive.family"):
        lines.append(f"    --font-{name}: {css_font(tokens.resolve('primitive.family.' + name))};")
    lines.append("")
    for name in tokens.leaf_names("primitive.size"):
        lines.append(f"    --text-{name}: {tokens.resolve('primitive.size.' + name)};")
        leading = tokens.flat[f"primitive.size.{name}"].get("$extensions", {}).get(LEADING_KEY)
        if leading:
            lines.append(f"    --text-{name}--line-height: {leading};")
    lines.append("")
    for name in tokens.leaf_names("primitive.space"):
        lines.append(f"    --spacing-{name}: calc({tokens.resolve('primitive.space.' + name)} * var(--ambrose-density));")
    lines.append("")
    for name in tokens.leaf_names("primitive.radius"):
        lines.append(f"    --radius-{name}: {tokens.resolve('primitive.radius.' + name)};")
    lines.append("")
    for name in tokens.leaf_names("primitive.ease"):
        points = ", ".join(str(point) for point in tokens.resolve("primitive.ease." + name))
        lines.append(f"    --ease-{name}: cubic-bezier({points});")
    lines.append("}")
    lines.append("")

    lines.extend(variable_lines(tokens))
    return "\n".join(lines)


def generate_variables(tokens):
    lines = [
        "/*",
        f" * {BRAND}",
        " * Generated from design/tokens.json: the semantic colours, sizes, durations and font families as plain custom properties with the light remap, and no Tailwind theme, for a surface that keeps Tailwind's own scales.",
        " */",
        "",
    ]
    lines.extend(variable_lines(tokens))
    return "\n".join(lines)


def variable_lines(tokens):
    lines = []

    def theme_block(selector, theme, scheme):
        block = [f"{selector} {{", f"    color-scheme: {scheme};"]
        for name, value in tokens.colors("semantic.color", theme):
            block.append(f"    --ambrose-color-{name}: {value};")
        for name, value in tokens.colors("component.color", theme):
            block.append(f"    --ambrose-color-{name}: {value};")
        for name, value in tokens.colors("series.color", theme):
            block.append(f"    --ambrose-color-series-{name}: {value};")
        block.append("}")
        return block

    lines.extend(theme_block(":root", "dark", "dark"))
    lines.append("")
    lines.extend(theme_block(':root[data-theme="light"]', "light", "light"))
    lines.append("")
    lines.append("@media (prefers-color-scheme: light) {")
    for line in theme_block(':root:not([data-theme="dark"])', "light", "light"):
        lines.append("    " + line if line else line)
    lines.append("}")
    lines.append("")
    lines.append(":root {")
    lines.append("    --ambrose-density: 1;")
    for name in tokens.leaf_names("component.size"):
        lines.append(f"    --ambrose-size-{name}: {tokens.resolve('component.size.' + name)};")
    for name in tokens.leaf_names("primitive.duration"):
        lines.append(f"    --ambrose-duration-{name}: {tokens.resolve('primitive.duration.' + name)};")
    for name in tokens.leaf_names("primitive.family"):
        lines.append(f"    --ambrose-font-{name}: {css_font(tokens.resolve('primitive.family.' + name))};")
    lines.append("}")
    lines.append("")
    lines.append(f':root[data-density="compact"] {{')
    lines.append(f"    --ambrose-density: {DENSITY_COMPACT};")
    lines.append("}")
    lines.append("")
    lines.append("@media (pointer: coarse) {")
    lines.append("    :root, :root[data-density] {")
    lines.append("        --ambrose-density: 1;")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    still = ["    " + f"--ambrose-duration-{name}: 0ms;" for name in tokens.leaf_names("primitive.duration")]
    lines.append(':root[data-motion="off"] {')
    lines.extend(still)
    lines.append("}")
    lines.append("")
    lines.append("@media (prefers-reduced-motion: reduce) {")
    lines.append('    :root:not([data-motion="on"]) {')
    lines.extend("    " + line for line in still)
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return lines


def ts_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def ts_record(name, entries, kind="string"):
    lines = [f"export const {name} = {{"]
    for key, value in entries:
        rendered = ts_string(value) if kind == "string" else str(value)
        lines.append(f"    {ts_string(key)}: {rendered},")
    lines.append("} as const;")
    return lines


def generate_ts(tokens, pairs):
    lines = [
        "/*",
        f" * {BRAND}",
        " * Generated from design/tokens.json: every token as a typed constant, with the contrast each documented pair reaches.",
        " */",
        "",
    ]
    lines.extend(ts_record("primitiveColors", tokens.colors("primitive.color", "dark")))
    lines.append("")
    lines.append("export type PrimitiveColorName = keyof typeof primitiveColors;")
    lines.append("")
    lines.append("export const semanticColors = {")
    for theme in THEMES:
        lines.append(f"    {theme}: {{")
        for name, value in tokens.colors("semantic.color", theme):
            lines.append(f"        {ts_string(name)}: {ts_string(value)},")
        lines.append("    },")
    lines.append("} as const;")
    lines.append("")
    lines.append("export type SemanticColorName = keyof typeof semanticColors.dark;")
    lines.append("")
    lines.append("export const componentColors = {")
    for theme in THEMES:
        lines.append(f"    {theme}: {{")
        for name, value in tokens.colors("component.color", theme):
            lines.append(f"        {ts_string(name)}: {ts_string(value)},")
        lines.append("    },")
    lines.append("} as const;")
    lines.append("")
    lines.append("export const seriesColors = {")
    for theme in THEMES:
        values = ", ".join(ts_string(value) for _, value in tokens.colors("series.color", theme))
        lines.append(f"    {theme}: [{values}],")
    lines.append("} as const;")
    lines.append("")
    lines.extend(ts_record("space", [(name, tokens.resolve("primitive.space." + name)) for name in tokens.leaf_names("primitive.space")]))
    lines.append("")
    lines.extend(ts_record("radius", [(name, tokens.resolve("primitive.radius." + name)) for name in tokens.leaf_names("primitive.radius")]))
    lines.append("")
    lines.extend(ts_record("fontSize", [(name, tokens.resolve("primitive.size." + name)) for name in tokens.leaf_names("primitive.size")]))
    lines.append("")
    lines.extend(ts_record("fontFamily", [(name, css_font(tokens.resolve("primitive.family." + name))) for name in tokens.leaf_names("primitive.family")]))
    lines.append("")
    lines.extend(ts_record("componentSize", [(name, tokens.resolve("component.size." + name)) for name in tokens.leaf_names("component.size")]))
    lines.append("")
    durations = [(name, int(tokens.resolve("primitive.duration." + name).replace("ms", ""))) for name in tokens.leaf_names("primitive.duration")]
    lines.extend(ts_record("durationMs", durations, kind="number"))
    lines.append("")
    lines.append("export type DurationName = keyof typeof durationMs;")
    lines.append("")
    lines.extend(ts_record("easing", [(name, "cubic-bezier(" + ", ".join(str(point) for point in tokens.resolve("primitive.ease." + name)) + ")") for name in tokens.leaf_names("primitive.ease")]))
    lines.append("")
    lines.append('export const themes = ["dark", "light"] as const;')
    lines.append("")
    lines.append("export type Theme = (typeof themes)[number];")
    lines.append("")
    lines.append("export type ContrastPair = {")
    lines.append("    foreground: string;")
    lines.append("    background: string;")
    lines.append("    theme: Theme;")
    lines.append("    ratio: number;")
    lines.append("    minimum: number;")
    lines.append("    note: string;")
    lines.append("};")
    lines.append("")
    lines.append("export const contrastPairs: readonly ContrastPair[] = [")
    for pair in pairs:
        lines.append(
            "    { foreground: %s, background: %s, theme: %s, ratio: %s, minimum: %s, note: %s }," % (
                ts_string(pair["foreground"]), ts_string(pair["background"]), ts_string(pair["theme"]),
                pair["ratio"], pair["minimum"], ts_string(pair["note"])))
    lines.append("] as const;")
    lines.append("")
    lines.append("export const terminalColors = {")
    for theme in THEMES:
        lines.append(f"    {theme}: {{")
        for name, value in tokens.colors("semantic.color", theme):
            lines.append("        %s: { hex: %s, index256: %d, index16: %d }," % (
                ts_string(name), ts_string(value), nearest(value, XTERM_256), tokens.ansi16(f"semantic.color.{name}", theme)))
        lines.append("    },")
    lines.append("} as const;")
    lines.append("")
    return "\n".join(lines)


def identifier(name):
    return "".join(part.capitalize() for part in name.replace(".", "-").split("-"))


def generate_header(tokens):
    lines = [
        "/*",
        f" * {BRAND}",
        " * Generated from design/tokens.json: every semantic colour in truecolor, 256 and 16 form, with the motion durations the terminal obeys.",
        " */",
        "#ifndef AMBROSE_TOKENS_H",
        "#define AMBROSE_TOKENS_H",
        "",
        "#include <array>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace Ambrose::Design",
        "{",
        "struct TerminalColor",
        "{",
        "    std::string_view Name;",
        "    std::string_view Hex;",
        "    std::uint8_t Red;",
        "    std::uint8_t Green;",
        "    std::uint8_t Blue;",
        "    std::uint8_t Index256;",
        "    std::uint8_t Index16;",
        "};",
        "",
    ]
    names = tokens.leaf_names("semantic.color")
    for theme in THEMES:
        lines.append(f"inline constexpr std::array<TerminalColor, {len(names)}> {identifier(theme)}Tokens = {{{{")
        for name, value in tokens.colors("semantic.color", theme):
            red, green, blue = parse_hex(value)
            lines.append('    TerminalColor{"%s", "%s", %d, %d, %d, %d, %d},' % (
                name, value, red, green, blue, nearest(value, XTERM_256), tokens.ansi16(f"semantic.color.{name}", theme)))
        lines.append("}};")
        lines.append("")
    series = tokens.leaf_names("series.color")
    for theme in THEMES:
        lines.append(f"inline constexpr std::array<TerminalColor, {len(series)}> {identifier(theme)}Series = {{{{")
        for name, value in tokens.colors("series.color", theme):
            red, green, blue = parse_hex(value)
            lines.append('    TerminalColor{"series-%s", "%s", %d, %d, %d, %d, %d},' % (
                name, value, red, green, blue, nearest(value, XTERM_256), tokens.ansi16(f"series.color.{name}", theme)))
        lines.append("}};")
        lines.append("")
    for name in tokens.leaf_names("primitive.duration"):
        value = int(tokens.resolve("primitive.duration." + name).replace("ms", ""))
        lines.append(f"inline constexpr std::uint32_t Duration{identifier(name)}Ms = {value};")
    lines.append("")
    lines.append("constexpr TerminalColor const* Find(std::array<TerminalColor, %d> const& tokens, std::string_view name)" % len(names))
    lines.append("{")
    lines.append("    for (TerminalColor const& token : tokens)")
    lines.append("    {")
    lines.append("        if (token.Name == name)")
    lines.append("        {")
    lines.append("            return &token;")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("}")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def table(header, rows):
    lines = ["| " + " | ".join(header) + " |", "|" + "|".join("---" for _ in header) + "|"]
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    return lines


def design_tables(tokens):
    palette = table(["Token", "Value", "Use"], [
        (f"`{name}`", f"`{value}`", tokens.description(f"primitive.color.{name}"))
        for name, value in tokens.colors("primitive.color", "dark")])
    meanings = table(["Token", "Dark", "Light", "Use"], [
        (f"`{name}`", f"`{tokens.resolve('semantic.color.' + name, 'dark')}`", f"`{tokens.resolve('semantic.color.' + name, 'light')}`",
         tokens.description(f"semantic.color.{name}"))
        for name in tokens.leaf_names("semantic.color")])
    fills = table(["Token", "Value", "Use"], [
        (f"`{name}`", f"`{tokens.resolve('component.color.' + name, 'dark')}`", tokens.description(f"component.color.{name}"))
        for name in tokens.leaf_names("component.color")])
    families = table(["Role", "Family", "Use"], [
        (name.capitalize(), tokens.resolve(f"primitive.family.{name}")[0], tokens.description(f"primitive.family.{name}"))
        for name in tokens.leaf_names("primitive.family")])
    sizes = table(["Size", "Line height"], [
        (f"`{tokens.resolve('primitive.size.' + name)}`",
         f"`{tokens.flat['primitive.size.' + name].get('$extensions', {}).get(LEADING_KEY, '')}`")
        for name in tokens.leaf_names("primitive.size")])
    series = table(["Slot", "Dark", "Light", "Use"], [
        (f"`series-{name}`", f"`{tokens.resolve('series.color.' + name, 'dark')}`", f"`{tokens.resolve('series.color.' + name, 'light')}`",
         tokens.description(f"series.color.{name}"))
        for name in tokens.leaf_names("series.color")])
    return {
        "### The palette": palette,
        "### The meanings": meanings,
        "### Filled accents": fills,
        "### Families": families,
        "### Sizes": sizes,
        "### The series ramp": series,
    }


def replace_tables(text, tables):
    lines = text.split("\n")
    for heading, rows in tables.items():
        try:
            start = lines.index(heading)
        except ValueError:
            raise GenerationRefused(f"{DESIGN_PATH} has no '{heading}' heading for a generated table")
        cursor = start + 1
        while cursor < len(lines) and not lines[cursor].strip():
            cursor += 1
        end = cursor
        while end < len(lines) and lines[end].startswith("|"):
            end += 1
        lines[cursor:end] = rows
    return "\n".join(lines)


def generate(root):
    with open(os.path.join(root, TOKENS_PATH), "r", encoding="utf-8") as handle:
        document = json.load(handle)
    tokens = Tokens(document)
    pairs = check_contrast(tokens)
    with open(os.path.join(root, DESIGN_PATH), "r", encoding="utf-8") as handle:
        design = handle.read().replace("\r\n", "\n")
    return {
        CSS_PATH: generate_css(tokens),
        VARIABLES_PATH: generate_variables(tokens),
        TS_PATH: generate_ts(tokens, pairs),
        HEADER_PATH: generate_header(tokens),
        DESIGN_PATH: replace_tables(design, design_tables(tokens)),
    }


def read(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read().replace("\r\n", "\n")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose design token generator")
    parser.add_argument("--root", default=default_root, help="repository root")
    parser.add_argument("--check", action="store_true", help="fail when a generated file no longer matches design/tokens.json")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    try:
        outputs = generate(root)
    except GenerationRefused as refusal:
        print("designtokens: refusing to generate", file=sys.stderr)
        print(str(refusal), file=sys.stderr)
        return 2
    stale = []
    for relative, text in sorted(outputs.items()):
        path = os.path.join(root, relative.replace("/", os.sep))
        if read(path) == text:
            continue
        stale.append(relative)
        if not args.check:
            write(path, text)
    if args.check:
        for relative in stale:
            print(f"{relative}: does not match design/tokens.json; run apps/designtokens/designtokens.py")
        print(f"designtokens: {len(outputs)} files checked, {len(stale)} stale")
        return 1 if stale else 0
    for relative in stale:
        print(f"{relative}: written")
    print(f"designtokens: {len(outputs)} files generated, {len(stale)} changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

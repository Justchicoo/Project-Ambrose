#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Writes design/icons/<name>.svg for every icon design/icons.json names, out of the installed icon set, so the repository owns each file and no surface fetches an icon from any host.
import argparse
import json
import os
import sys

ICON_LIST = "design/icons.json"
ICON_DIR = "design/icons"
MODULE_PATH = "packages/ui/src/icons/icons.ts"
SOURCE = "node_modules/@iconify-json/{set}/icons.json"
BRAND = "Project Ambrose by Imjustchico"


class IconsRefused(Exception):
    pass


def load(root):
    with open(os.path.join(root, ICON_LIST), "r", encoding="utf-8") as handle:
        wanted = json.load(handle)
    source = os.path.join(root, SOURCE.format(set=wanted["set"]).replace("/", os.sep))
    if not os.path.exists(source):
        raise IconsRefused(f"{source} is missing; run npm ci first so the icon set is on disk")
    with open(source, "r", encoding="utf-8") as handle:
        return wanted, json.load(handle)


def render(name, icon, collection):
    width = icon.get("width", collection.get("width", 24))
    height = icon.get("height", collection.get("height", 24))
    left = icon.get("left", collection.get("left", 0))
    top = icon.get("top", collection.get("top", 0))
    body = icon["body"]
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{left} {top} {width} {height}" '
        f'width="{width}" height="{height}" fill="none" stroke="currentColor" stroke-width="2" '
        f'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{body}</svg>\n')


def identifier(name):
    return "Icon" + "".join(part.capitalize() for part in name.split("-"))


def module(names):
    lines = [
        "/*",
        f" * {BRAND}",
        " * Generated from design/icons.json: every icon a surface may use, compiled to inline markup at build time and named by the type the panel and the launcher share.",
        " */",
        "",
        'import type { Component } from "svelte";',
        "",
    ]
    for name in names:
        lines.append(f'import {identifier(name)} from "~icons/ambrose/{name}";')
    lines.append("")
    lines.append("export const icons = {")
    for name in names:
        lines.append(f'    "{name}": {identifier(name)},')
    lines.append("} as const satisfies Record<string, Component>;")
    lines.append("")
    lines.append("export type IconName = keyof typeof icons;")
    lines.append("")
    lines.append("export const iconNames = Object.keys(icons) as IconName[];")
    lines.append("")
    return "\n".join(lines)


def generate(root):
    wanted, collection = load(root)
    icons = collection["icons"]
    aliases = collection.get("aliases", {})
    out = {}
    missing = []
    for name in wanted["icons"]:
        entry = icons.get(name)
        if entry is None and name in aliases:
            parent = aliases[name].get("parent")
            entry = icons.get(parent)
        if entry is None:
            missing.append(name)
            continue
        out[f"{ICON_DIR}/{name}.svg"] = render(name, entry, collection)
    if missing:
        raise IconsRefused("the icon set has no " + ", ".join(sorted(missing)))
    out[MODULE_PATH] = module(sorted(wanted["icons"]))
    return out


def read(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read().replace("\r\n", "\n")


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose icon vendoring")
    parser.add_argument("--root", default=default_root, help="repository root")
    parser.add_argument("--check", action="store_true", help="fail when a vendored icon no longer matches design/icons.json")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    with open(os.path.join(root, ICON_LIST.replace("/", os.sep)), "r", encoding="utf-8") as handle:
        source = os.path.join(root, SOURCE.format(set=json.load(handle)["set"]).replace("/", os.sep))
    if args.check and not os.path.exists(source):
        print("icons: the icon set is not installed, so the check was skipped; the front-end job runs it after npm ci")
        return 0
    try:
        outputs = generate(root)
    except (IconsRefused, KeyError, ValueError) as refusal:
        print("icons: refusing to write", file=sys.stderr)
        print(str(refusal), file=sys.stderr)
        return 2
    folder = os.path.join(root, ICON_DIR.replace("/", os.sep))
    os.makedirs(folder, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.join(root, MODULE_PATH.replace("/", os.sep))), exist_ok=True)
    present = {f"{ICON_DIR}/{name}" for name in os.listdir(folder) if name.endswith(".svg")}
    stale = sorted(present - set(outputs))
    for relative, text in sorted(outputs.items()):
        path = os.path.join(root, relative.replace("/", os.sep))
        if read(path) == text:
            continue
        stale.append(relative)
        if not args.check:
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
    if args.check:
        for relative in stale:
            print(f"{relative}: does not match design/icons.json; run apps/designtokens/icons.py")
        print(f"icons: {len(outputs)} icons checked, {len(stale)} stale")
        return 1 if stale else 0
    for relative in stale:
        if relative not in outputs:
            os.remove(os.path.join(root, relative.replace("/", os.sep)))
    print(f"icons: {len(outputs)} icons written, {len(stale)} changed")
    return 0


if __name__ == "__main__":
    sys.exit(main())

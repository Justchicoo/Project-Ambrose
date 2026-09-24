#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Fails when front-end source writes a colour of its own, reaches for a Tailwind arbitrary value, carries a colour in an inline style, adds a component with no story, leaves a collection without the four states a reader has to be shown, or builds a bundle that names another host.
import argparse
import json
import os
import re
import sys

SOURCE_ROOTS = ("packages/ui/src", "apps/dashboard/src", "apps/launcherui/src")
COMPONENT_ROOT = "packages/ui/src"
GENERATED = ("packages/ui/src/tokens/tokens.css", "packages/ui/src/tokens/variables.css", "packages/ui/src/tokens/tokens.ts", "packages/ui/src/icons/icons.ts")
SKIP_FOLDERS = ("packages/ui/src/fonts", "packages/ui/src/canary")
BUNDLES = ("apps/dashboard/dist", "apps/launcherui/dist")
ALLOW_FILE = "apps/ci/ci_frontend_allow.json"
COLLECTIONS_FILE = "packages/ui/collections.json"
COLLECTION_STATES = ("loading", "empty", "no-results", "error")
LIST_PROP = re.compile(r"^\s*[a-zA-Z]+\??:\s*(?:[A-Za-z][\w.<>, ]*\[\]|Array<)", re.M)
SOURCE_EXTENSIONS = (".svelte", ".ts", ".css", ".html")

HEX = re.compile(r"#(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{4}|[0-9a-fA-F]{3})(?![0-9a-zA-Z_-])")
FUNCTIONS = re.compile(r"\b(?:rgba?|hsla?|oklch|oklab|lab|lch|color-mix)\s*\(")
COLOR_UTILITIES = "bg|text|border|fill|stroke|from|via|to|shadow|outline|ring|decoration|accent|caret|divide|placeholder"
SPACE_UTILITIES = (
    "p|px|py|pt|pr|pb|pl|m|mx|my|mt|mr|mb|ml|gap|gap-x|gap-y|space-x|space-y|w|h|size|"
    "min-w|min-h|max-w|max-h|top|right|bottom|left|inset|translate-x|translate-y|leading|tracking|rounded"
)
ARBITRARY = re.compile(r"(?<![\w-])(?:" + COLOR_UTILITIES + "|" + SPACE_UTILITIES + r")-\[")
STYLE_ATTRIBUTE = re.compile(r"""\sstyle\s*=\s*("([^"]*)"|'([^']*)')""")
STYLE_COLOR = re.compile(r"(?:^|[^-\w])(?:color|background|background-color|border-color|outline-color|fill|stroke)\s*:")
FETCHING_URL = re.compile(r"""(?:url\(\s*['"]?|src\s*=\s*['"]|href\s*=\s*['"]|@import\s+['"])https?://""")
ABSOLUTE_URL = re.compile(r"https?://[^\s'\"()<>\\]+")


def load_allow(root):
    path = os.path.join(root, ALLOW_FILE.replace("/", os.sep))
    if not os.path.exists(path):
        return {"lines": [], "urls": []}
    with open(path, "r", encoding="utf-8") as handle:
        document = json.load(handle)
    return {"lines": document.get("lines", []), "urls": document.get("urls", [])}


def walk(root, relative, extensions):
    base = os.path.join(root, relative.replace("/", os.sep))
    if not os.path.isdir(base):
        return
    for folder, _directories, names in os.walk(base):
        for name in sorted(names):
            if not name.endswith(extensions):
                continue
            path = os.path.join(folder, name)
            yield os.path.relpath(path, root).replace(os.sep, "/"), path


def allowed(allow, relpath, line, text):
    for entry in allow["lines"]:
        if entry.get("path") == relpath and entry.get("contains") in text:
            return True
        if entry.get("path") == relpath and entry.get("line") == line:
            return True
    return False


def check_source(root, allow):
    problems = []
    for relative in SOURCE_ROOTS:
        for relpath, path in walk(root, relative, SOURCE_EXTENSIONS):
            if relpath in GENERATED or relpath.startswith(SKIP_FOLDERS):
                continue
            with open(path, "r", encoding="utf-8") as handle:
                text = handle.read()
            for number, line in enumerate(text.replace("\r\n", "\n").split("\n"), start=1):
                if allowed(allow, relpath, number, line):
                    continue
                if HEX.search(line) or FUNCTIONS.search(line):
                    problems.append(f"{relpath}:{number}: a colour is written here; name a token instead")
                if ARBITRARY.search(line):
                    problems.append(f"{relpath}:{number}: a Tailwind arbitrary value is used here; name a token instead")
                for match in STYLE_ATTRIBUTE.finditer(line):
                    value = match.group(2) or match.group(3) or ""
                    if STYLE_COLOR.search(value) or HEX.search(value) or FUNCTIONS.search(value):
                        problems.append(
                            f"{relpath}:{number}: an inline style carries a colour, which a strict policy blocks; "
                            "use a class or a style directive")
    return problems


def check_stories(root):
    problems = []
    for relpath, _path in walk(root, COMPONENT_ROOT, (".svelte",)):
        if relpath.endswith(".stories.svelte") or relpath.startswith(SKIP_FOLDERS):
            continue
        story = relpath[: -len(".svelte")] + ".stories.svelte"
        if not os.path.exists(os.path.join(root, story.replace("/", os.sep))):
            problems.append(f"{relpath}: has no story; every component in packages/ui needs one in {story}")
    return problems


def check_bundles(root, allow):
    problems = []
    built = 0
    for relative in BUNDLES:
        base = os.path.join(root, relative.replace("/", os.sep))
        if not os.path.isdir(base):
            continue
        built += 1
        for folder, _directories, names in os.walk(base):
            for name in sorted(names):
                if not name.endswith((".js", ".css", ".html", ".json", ".map")):
                    continue
                path = os.path.join(folder, name)
                relpath = os.path.relpath(path, root).replace(os.sep, "/")
                with open(path, "r", encoding="utf-8", errors="replace") as handle:
                    text = handle.read()
                if FETCHING_URL.search(text):
                    problems.append(f"{relpath}: fetches from another host; everything a surface loads is vendored")
                for url in set(ABSOLUTE_URL.findall(text)):
                    if any(url.startswith(prefix) for prefix in allow["urls"]):
                        continue
                    problems.append(f"{relpath}: names {url}, which is not one of the addresses the bundle may carry")
    return problems, built


def check_fonts(root):
    problems = []
    folder = os.path.join(root, "packages", "ui", "src", "fonts")
    packages = {
        "cormorant-garamond-latin-wght-normal.woff2": "@fontsource-variable/cormorant-garamond",
        "karla-latin-wght-normal.woff2": "@fontsource-variable/karla",
        "jetbrains-mono-latin-wght-normal.woff2": "@fontsource-variable/jetbrains-mono",
    }
    for name, package in packages.items():
        vendored = os.path.join(folder, name)
        if not os.path.exists(vendored):
            problems.append(f"packages/ui/src/fonts/{name}: is missing; a surface would fall back to a system face")
            continue
        source = os.path.join(root, "node_modules", *package.split("/"), "files", name)
        if not os.path.exists(source):
            continue
        with open(vendored, "rb") as one, open(source, "rb") as two:
            if one.read() != two.read():
                problems.append(f"packages/ui/src/fonts/{name}: no longer matches the file {package} ships")
    return problems



def check_collections(root):
    problems = []
    path = os.path.join(root, COLLECTIONS_FILE.replace("/", os.sep))
    if not os.path.exists(path):
        return [f"{COLLECTIONS_FILE}: is missing; every list-shaped component is classified there"]
    with open(path, encoding="utf-8") as handle:
        document = json.load(handle)
    collections = list(document.get("collections", []))
    others = list(document.get("not-collections", []))
    both = set(collections) & set(others)
    for name in sorted(both):
        problems.append(f"{COLLECTIONS_FILE}: {name} is in both lists; it is either a collection or it is not")
    known = set(collections) | set(others)
    folder = os.path.join(root, COMPONENT_ROOT.replace("/", os.sep), "components")
    for name in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        if not name.endswith(".svelte") or name.endswith(".stories.svelte"):
            continue
        component = name[: -len(".svelte")]
        with open(os.path.join(folder, name), encoding="utf-8") as handle:
            text = handle.read()
        if LIST_PROP.search(text) and component not in known:
            problems.append(
                f"packages/ui/src/components/{name}: takes a list and is classified in neither list of {COLLECTIONS_FILE}"
            )
    for component in collections:
        story = os.path.join(folder, component + ".stories.svelte")
        if not os.path.exists(story):
            problems.append(f"packages/ui/src/components/{component}.stories.svelte: is missing; a collection needs its states shown")
            continue
        with open(story, encoding="utf-8") as handle:
            text = handle.read()
        for state in COLLECTION_STATES:
            if f'"state:{state}"' not in text:
                problems.append(
                    f"packages/ui/src/components/{component}.stories.svelte: has no story tagged state:{state}; "
                    "a collection shows what it looks like while loading, when it holds nothing, when a filter matched nothing and when it failed"
                )
    return problems


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose front-end repository checks")
    parser.add_argument("--root", default=default_root, help="repository root")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    allow = load_allow(root)
    problems = check_source(root, allow)
    problems += check_stories(root)
    problems += check_collections(root)
    problems += check_fonts(root)
    bundle_problems, built = check_bundles(root, allow)
    problems += bundle_problems
    for problem in problems:
        print(problem)
    if built == 0:
        print("frontend checks: no bundle was built, so the no-other-host check was not run")
    print(f"frontend checks: {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

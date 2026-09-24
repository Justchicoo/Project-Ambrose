# Project Ambrose by Imjustchico
# Audits direct vcpkg and npm dependencies against the third-party notices.

import argparse
import json
import re
import sys
from pathlib import Path


TABLE_ROW_RE = re.compile(r"^\|([^|]+)\|([^|]+)\|([^|]+)\|")
VCPKG_ALIASES = {
    "asio": "Asio (standalone)",
    "botan": "Botan 3",
    "fmt": "fmt",
    "libmariadb": "MariaDB Connector/C",
    "nlohmann-json": "nlohmann/json",
    "pugixml": "pugixml",
    "unicorn": "Unicorn 2",
    "zlib": "zlib",
    "zydis": "Zydis and Zycore",
    "crow": "Crow",
    "openssl": "OpenSSL",
    "sqlite3": "SQLite",
    "gtest": "GoogleTest and GoogleMock",
}
NOTICE_ALIASES = {
    "@eslint/js": "ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals",
    "eslint": "ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals",
    "eslint-plugin-svelte": "ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals",
    "globals": "ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals",
    "@fontsource-variable/cormorant-garamond": "Cormorant Garamond, Karla and JetBrains Mono",
    "@fontsource-variable/jetbrains-mono": "Cormorant Garamond, Karla and JetBrains Mono",
    "@fontsource-variable/karla": "Cormorant Garamond, Karla and JetBrains Mono",
    "@playwright/test": "Playwright and @playwright/test",
    "playwright": "Playwright and @playwright/test",
    "@storybook/addon-a11y": "Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons",
    "@storybook/addon-svelte-csf": "Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons",
    "@storybook/addon-vitest": "Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons",
    "@storybook/svelte-vite": "Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons",
    "storybook": "Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons",
    "typescript-eslint": "ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals",
    "@tailwindcss/vite": "Tailwind CSS and its Vite plugin",
    "tailwindcss": "Tailwind CSS and its Vite plugin",
    "@vitest/browser-playwright": "Vitest and @vitest/browser-playwright",
    "vitest": "Vitest and @vitest/browser-playwright",
    "prettier": "Prettier and prettier-plugin-svelte",
    "prettier-plugin-svelte": "Prettier and prettier-plugin-svelte",
    "@lucide/svelte": "Lucide icons",
    "@iconify-json/lucide": "Lucide icons",
    "@tanstack/svelte-table": "TanStack Table, Svelte adapter",
}
LICENSE_NAMES = {"MIT", "ISC", "OFL-1.1", "Apache-2.0", "MPL-2.0", "BSD-2-Clause",
                 "BSD-3-Clause", "BSL-1.0", "LGPL-2.1-or-later", "GPL-2.0-or-later",
                 "Zlib", "blessing"}
NON_DIRECT_NOTICE_NAMES = {
    "@fontsource-variable/*",
    "@iconify-json/lucide",
    "Lightning CSS",
    "TanStack Table, Svelte adapter",
}


def normalized(value):
    return re.sub(r"[^a-z0-9]+", "", value.lower())


def relative(path, root):
    return path.relative_to(root).as_posix()


def read_notices(root):
    notices = {}
    path = root / "THIRD-PARTY-NOTICES.md"
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = TABLE_ROW_RE.match(line)
        if not match:
            continue
        cells = [value.strip() for value in line.strip().strip("|").split("|")]
        name = cells[0]
        license_name = next((value for value in cells if value in LICENSE_NAMES), None)
        if name == "Library" or name == "Tool":
            continue
        if license_name is None:
            continue
        notices[normalized(name)] = {
            "name": name,
            "license": license_name,
            "file": relative(path, root),
            "line": line_number,
        }
    return notices


def read_vcpkg(root):
    manifest = json.loads((root / "vcpkg.json").read_text(encoding="utf-8"))
    dependencies = []
    for dependency in manifest.get("dependencies", []):
        name = dependency if isinstance(dependency, str) else dependency["name"]
        if name in VCPKG_ALIASES:
            display = VCPKG_ALIASES[name]
        else:
            display = name
        dependencies.append({
            "ecosystem": "vcpkg",
            "name": display,
            "manifest_name": name,
            "file": relative(root / "vcpkg.json", root),
        })
    for feature in manifest.get("features", {}).values():
        for dependency in feature.get("dependencies", []):
            name = dependency if isinstance(dependency, str) else dependency["name"]
            dependencies.append({
                "ecosystem": "vcpkg feature",
                "name": VCPKG_ALIASES.get(name, name),
                "manifest_name": name,
                "file": relative(root / "vcpkg.json", root),
            })
    return dependencies


def read_npm(root):
    dependencies = []
    for path in [root / "package.json",
                 root / "apps" / "dashboard" / "package.json",
                 root / "apps" / "launcherui" / "package.json",
                 root / "packages" / "ui" / "package.json"]:
        if not path.exists():
            continue
        manifest = json.loads(path.read_text(encoding="utf-8"))
        for section in ("dependencies", "devDependencies"):
            for name in manifest.get(section, {}):
                if name.startswith("@ambrose/"):
                    continue
                dependencies.append({
                    "ecosystem": "npm",
                    "name": name,
                    "manifest_name": name,
                    "section": section,
                    "file": relative(path, root),
                })
    lock = json.loads((root / "package-lock.json").read_text(encoding="utf-8"))
    lock_packages = lock.get("packages", {})
    for dependency in dependencies:
        key = f"node_modules/{dependency['name']}"
        package = lock_packages.get(key, {})
        dependency["license"] = package.get("license")
    return dependencies


def audit(root):
    notices = read_notices(root)
    dependencies = read_vcpkg(root) + read_npm(root)
    findings = []
    seen = set()
    for dependency in dependencies:
        notice_name = NOTICE_ALIASES.get(dependency["name"], dependency["name"])
        key = normalized(notice_name)
        if key in seen:
            continue
        seen.add(key)
        notice = notices.get(key)
        if notice is None:
            findings.append({
                "kind": "dependency_missing_from_notices",
                "dependency": dependency,
            })
            continue
        if dependency.get("license") and normalized(dependency["license"]) != normalized(notice["license"]):
            findings.append({
                "kind": "license_mismatch",
                "dependency": dependency,
                "notice": notice,
            })
    dependency_keys = {
        normalized(NOTICE_ALIASES.get(item["name"], item["name"]))
        for item in dependencies
    }
    for key, notice in sorted(notices.items()):
        if notice["name"] in NON_DIRECT_NOTICE_NAMES:
            continue
        if key not in dependency_keys:
            findings.append({
                "kind": "notice_without_manifest_dependency",
                "notice": notice,
            })
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--format", choices=("text", "json"), default="text")
    args = parser.parse_args()
    findings = audit(args.root.resolve())
    if args.format == "json":
        print(json.dumps({"findings": findings}, indent=2))
    else:
        for item in findings:
            name = item.get("dependency", item.get("notice", {})).get("name", "")
            print(f"{item['kind'].upper()} {name}")
    print(f"dependency notices audit: {len(findings)} finding(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())

# Project Ambrose by Imjustchico
# Refuses a dependency in vcpkg.json that THIRD-PARTY-NOTICES.md does not name, because the settled licensing decision is that the notices are written in the same change that adds the library, and a dependency nobody wrote down is one whose licence nobody read.
import argparse
import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MANIFEST = "vcpkg.json"
NOTICES = "THIRD-PARTY-NOTICES.md"
ALIASES = {
    "libmariadb": "mariadb",
    "sqlite3": "sqlite",
    "nlohmann-json": "nlohmannjson",
}


def read(path):
    with io.open(path, encoding="utf-8") as handle:
        return handle.read()


def simplify(text):
    return re.sub(r"[^a-z0-9]", "", text.lower())


def dependencies(root):
    document = json.loads(read(os.path.join(root, MANIFEST)))
    found = []
    for entry in document.get("dependencies", []):
        found.append(entry if isinstance(entry, str) else entry.get("name", ""))
    return [name for name in found if name]


def missing(root):
    notices = simplify(read(os.path.join(root, NOTICES)))
    absent = []
    for name in dependencies(root):
        wanted = simplify(ALIASES.get(name, name))
        if wanted not in notices:
            absent.append(name)
    return absent


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check every vcpkg dependency is named in the third-party notices")
    parser.add_argument("--root", default=ROOT)
    options = parser.parse_args(argv)
    absent = missing(options.root)
    if absent:
        for name in absent:
            print(f"{name} is a dependency in {MANIFEST} that {NOTICES} does not name")
        print(f"Add each one with what its licence asks, which doc/ARCHITECTURE.md settles is done in the same change that adds the library")
        return 1
    print(f"dependency notices: {len(dependencies(options.root))} dependencies, all named in {NOTICES}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

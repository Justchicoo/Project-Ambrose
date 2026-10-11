# Project Ambrose by Imjustchico
import argparse
import json
import os
import re
import subprocess
import sys

MAX_BYTES = 1_000_000

FORBIDDEN_EXTENSIONS = {
    ".wad": "client archive",
    ".nif": "client model",
    ".kf": "client animation",
    ".kfm": "client animation",
    ".pcap": "packet capture",
    ".pcapng": "packet capture",
    ".key": "private key",
    ".pem": "private key or certificate",
    ".pfx": "private key bundle",
    ".p12": "private key bundle",
    ".sqlite3": "store holding operators and sessions",
    ".sqlite": "store holding operators and sessions",
    ".log": "log",
    ".token": "token",
}

PRIVATE_KEY = re.compile(
    rb"-----BEGIN (?:[A-Z ]+ )?PRIVATE KEY-----"
)

PROTOCOL_XML = re.compile(
    r"\s*(<\?xml[^>]*\?>\s*)?<[A-Za-z0-9_]*Messages>\s*<_ProtocolInfo>"
)

OTHER_PROJECTS = re.compile(
    "|".join([
        "im" + "light",
        "auror" + "ium",
        "im" + "codec",
        "spiral" + "db",
        "revive" + "101",
    ]),
    re.IGNORECASE,
)

OTHER_LANGUAGE_FILE = re.compile(
    r"(?<=[A-Za-z0-9_-])\.(?:cs|rs|csproj|sln)(?![A-Za-z0-9/])"
)

TEXT_EXTENSIONS = (
    ".md",
    ".txt",
    ".json",
    ".yml",
    ".yaml",
    ".py",
    ".cpp",
    ".h",
    ".sql",
    ".dist",
    ".conf",
    ".cmake",
    ".ts",
    ".svelte",
    ".css",
    ".js",
    ".mjs",
)

SCANNED_FOR_NAMES = (
    "apps/ci/ci_forbidden_files.py",
    "apps/ci/tests/test_ci.py",
)

SCANNED_BYTES = 512 * 1024

# This database is explicitly permitted by the repository scan.
# Matching is case-insensitive and based on the filename.
ALLOWED_DATABASE = "combatdecks.db"


def check_file(relpath, raw):
    relpath = relpath.replace("\\", "/")
    name = relpath.rsplit("/", 1)[-1].lower()
    extension = os.path.splitext(name)[1]
    problems = []

    # Preserve the existing forbidden-extension checks.
    # combatdecks.db has a .db extension, so it is not affected here.
    if extension in FORBIDDEN_EXTENSIONS:
        problems.append(
            f"{FORBIDDEN_EXTENSIONS[extension]} files must never be committed"
        )

    if name.endswith(".conf"):
        problems.append(
            "local config must not be committed; "
            "commit a .conf.dist template instead"
        )

    if name.endswith(".secret"):
        problems.append(
            "secret files must never be committed; "
            "an app makes its own beside its config"
        )

    # Keep the private-key check enabled for every file.
    if PRIVATE_KEY.search(raw[:8192]):
        problems.append(
            "content holds a private key; "
            "a listener reads its key from a path the operator names"
        )

    # Permit combatdecks.db while rejecting other SQLite database content.
    if raw.startswith(b"SQLite format 3" + bytes([0])):
        if name != ALLOWED_DATABASE:
            problems.append(
                "content is a SQLite store; the panel keeps its operators "
                "and sessions in the data folder, not the repository"
            )

    if raw.startswith(b"KIWAD"):
        problems.append("content is a KIWAD client archive")

    if raw.startswith(b"BINd"):
        problems.append("content is BINd client data")

    if PROTOCOL_XML.match(
        raw[:4096].decode("utf-8", "replace")
    ):
        problems.append(
            "content is a client protocol definition XML; "
            "definitions load at runtime from the user's install"
        )

    # Scan text files for references to other projects and source files.
    if extension in TEXT_EXTENSIONS and relpath not in SCANNED_FOR_NAMES:
        text = raw[:SCANNED_BYTES].decode("utf-8", "replace")

        named = OTHER_PROJECTS.search(text)
        if named:
            problems.append(
                f"content names another Wizard101 project, "
                f"'{named.group(0)}'; behaviour may be studied but "
                "nothing here names it"
            )

        foreign = OTHER_LANGUAGE_FILE.search(text)
        if foreign:
            token = text[
                max(0, foreign.start() - 40):foreign.end()
            ].rsplit(None, 1)[-1]

            problems.append(
                f"content names '{token}', a file from another project: "
                "this server is C++ and its tools are Python, so no .cs, "
                ".rs, .csproj or .sln path belongs in the tree"
            )

    # Detect client type dumps in JSON files.
    if extension == ".json":
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            document = None

        if (
            isinstance(document, dict)
            and "classes" in document
            and "version" in document
        ):
            problems.append(
                "content looks like a client type dump; "
                "the dump loads at runtime from the user's machine"
            )

    # Permit combatdecks.db to exceed the normal 1 MB file-size limit.
    # Other files remain subject to the existing limit, except files in deps/.
    if len(raw) > MAX_BYTES:
        if name != ALLOWED_DATABASE and not relpath.startswith("deps/"):
            problems.append(
                f"file

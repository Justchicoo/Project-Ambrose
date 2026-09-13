# Project Ambrose by Imjustchico
# Fails when the tree contains client game files, captures, protocol definitions, type dumps, local config, or oversized files.
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
}
PROTOCOL_XML = re.compile(r"\s*(<\?xml[^>]*\?>\s*)?<[A-Za-z0-9_]*Messages>\s*<_ProtocolInfo>")


def check_file(relpath, raw):
    relpath = relpath.replace("\\", "/")
    name = relpath.rsplit("/", 1)[-1].lower()
    extension = os.path.splitext(name)[1]
    problems = []
    if extension in FORBIDDEN_EXTENSIONS:
        problems.append(f"{FORBIDDEN_EXTENSIONS[extension]} files must never be committed")
    if name.endswith(".conf"):
        problems.append("local config must not be committed; commit a .conf.dist template instead")
    if raw.startswith(b"KIWAD"):
        problems.append("content is a KIWAD client archive")
    if raw.startswith(b"BINd"):
        problems.append("content is BINd client data")
    if PROTOCOL_XML.match(raw[:4096].decode("utf-8", "replace")):
        problems.append("content is a client protocol definition XML; definitions load at runtime from the user's install")
    if extension == ".json":
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            document = None
        if isinstance(document, dict) and "classes" in document and "version" in document:
            problems.append("content looks like a client type dump; the dump loads at runtime from the user's machine")
    if len(raw) > MAX_BYTES and not relpath.startswith("deps/"):
        problems.append(f"file is {len(raw)} bytes, over the {MAX_BYTES} byte limit")
    return problems


def collect_files(root):
    output = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=root, capture_output=True, check=True).stdout
    return sorted(path for path in set(output.decode("utf-8").split("\0")) if path and os.path.isfile(os.path.join(root, path)))


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose forbidden file scan")
    parser.add_argument("--root", default=default_root, help="repository root")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    files = collect_files(root)
    failures = 0
    for relpath in files:
        with open(os.path.join(root, relpath), "rb") as handle:
            for problem in check_file(relpath, handle.read()):
                print(f"{relpath}: {problem}")
                failures += 1
    print(f"forbidden files: {len(files)} files scanned, {failures} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

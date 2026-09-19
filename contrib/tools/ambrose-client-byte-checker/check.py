# Project Ambrose by Imjustchico
# C-26 scans proposed files for machine-detectable client-derived content.
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path


MAX_BYTES = 1_000_000
CLIENT_EXTENSIONS = {
    ".wad": "client archive",
    ".nif": "client model",
    ".kf": "client animation",
    ".kfm": "client animation",
    ".pcap": "packet capture",
    ".pcapng": "packet capture",
}
PROTOCOL_XML = re.compile(
    rb"\s*(?:<\?xml[^>]*\?>\s*)?<[A-Za-z0-9_]*Messages>\s*<_ProtocolInfo>"
)


@dataclass(frozen=True)
class Finding:
    rule: str
    path: str
    evidence: str
    bytes: int


def normalized_path(path: str) -> str:
    return path.replace("\\", "/")


def finding(rule: str, path: str, evidence: str, size: int) -> Finding:
    return Finding(rule, normalized_path(path), evidence, size)


def check_file(path: str, raw: bytes) -> list[Finding]:
    name = normalized_path(path).rsplit("/", 1)[-1].lower()
    extension = Path(name).suffix
    problems: list[Finding] = []
    if extension in CLIENT_EXTENSIONS:
        problems.append(finding("client-extension", path, CLIENT_EXTENSIONS[extension], len(raw)))
    if name.endswith(".conf"):
        problems.append(finding("local-config", path, "local configuration must not be committed", len(raw)))
    if raw.startswith(b"KIWAD"):
        problems.append(finding("kiwad-signature", path, "client archive signature", len(raw)))
    if raw.startswith(b"BINd"):
        problems.append(finding("bind-signature", path, "client data signature", len(raw)))
    if PROTOCOL_XML.match(raw[:4096]):
        problems.append(finding("protocol-xml", path, "client protocol definition XML", len(raw)))
    if extension == ".json":
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            document = None
        if isinstance(document, dict) and "classes" in document and "version" in document:
            problems.append(finding("type-dump", path, "client type-dump-shaped JSON", len(raw)))
    if raw and (b"\0" in raw or sum(byte < 9 or 13 < byte < 32 for byte in raw) / len(raw) > 0.05):
        problems.append(finding("binary-content", path, "binary control bytes detected", len(raw)))
    if len(raw) > MAX_BYTES and not normalized_path(path).startswith("deps/"):
        problems.append(finding("oversized-file", path, f"{len(raw)} bytes exceeds {MAX_BYTES}", len(raw)))
    return problems


def git_paths(root: str, commit_range: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMR", commit_range],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "git diff failed")
    return [line for line in result.stdout.splitlines() if line]


def scan_paths(root: str, paths: list[str]) -> list[Finding]:
    findings: list[Finding] = []
    for relative in paths:
        candidate = Path(root, relative)
        if not candidate.is_file():
            raise ValueError(f"path is not a readable file: {relative}")
        findings.extend(check_file(normalized_path(relative), candidate.read_bytes()))
    return findings


def run_self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="ambrose-c26-") as directory:
        root = Path(directory)
        (root / "clean.txt").write_text("operator-authored text\n", encoding="utf-8")
        (root / "client.bin").write_bytes(b"KIWAD" + b"\x00synthetic")
        (root / "dump.json").write_text('{"version": 2, "classes": {}}', encoding="utf-8")
        findings = scan_paths(directory, ["clean.txt", "client.bin", "dump.json"])
    rules = sorted(item.rule for item in findings)
    print(json.dumps({"files": 3, "findings": len(findings), "rules": rules}))
    return 0 if rules == ["binary-content", "kiwad-signature", "type-dump"] else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Detect machine-detectable client-derived files.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--paths", nargs="*")
    parser.add_argument("--git-range")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.self_test:
            return run_self_test()
        if bool(args.paths) == bool(args.git_range):
            raise ValueError("provide exactly one of --paths or --git-range")
        paths = args.paths if args.paths is not None else git_paths(args.root, args.git_range)
        findings = scan_paths(args.root, paths)
    except (OSError, ValueError) as exc:
        print(f"C-26 checker: {exc}", file=sys.stderr)
        return 2
    document = {
        "version": 1,
        "files": len(paths),
        "findings": [asdict(item) for item in findings],
        "status": "clean" if not findings else "findings",
    }
    print(json.dumps(document, indent=2 if args.pretty else None))
    return 0 if not findings else 1


if __name__ == "__main__":
    raise SystemExit(main())

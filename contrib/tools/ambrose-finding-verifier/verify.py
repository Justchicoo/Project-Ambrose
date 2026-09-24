# Project Ambrose by Imjustchico
# C-21 verifies safe, optional machine-checkable finding checks.
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ALLOWED_COMMANDS = {"python", "python3", "python.exe", "cmake", "cmake.exe", "ambrose-finding-check", "ambrose-finding-check.exe"}
STATUS_CODES = {"pass": 0, "fail": 1, "unable": 77}


def result(name: str, status: str, detail: str) -> dict[str, str]:
    return {"name": name, "status": status, "detail": detail}


def safe_path(root: Path, relative: str) -> Path:
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as exc:
        raise ValueError("check path must remain below the selected root") from exc
    return candidate


def verify_file_exists(root: Path, check: dict[str, Any]) -> dict[str, str]:
    path = check.get("path")
    if not isinstance(path, str) or not path:
        return result(str(check.get("name", "unnamed")), "unable", "file_exists needs a path")
    try:
        exists = safe_path(root, path).is_file()
    except ValueError as exc:
        return result(str(check.get("name", "unnamed")), "unable", str(exc))
    return result(str(check.get("name", path)), "pass" if exists else "fail", "file exists" if exists else "file is missing")


def verify_sha256(root: Path, check: dict[str, Any]) -> dict[str, str]:
    path = check.get("path")
    expected = check.get("sha256")
    name = str(check.get("name", "unnamed"))
    if not isinstance(path, str) or not isinstance(expected, str) or len(expected) != 64:
        return result(name, "unable", "file_sha256 needs path and a 64-character sha256")
    try:
        candidate = safe_path(root, path)
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
    except (OSError, ValueError) as exc:
        return result(name, "unable", str(exc))
    return result(name, "pass" if digest.lower() == expected.lower() else "fail", "sha256 matches" if digest.lower() == expected.lower() else "sha256 differs")


def verify_command(root: Path, check: dict[str, Any], run_commands: bool) -> dict[str, str]:
    name = str(check.get("name", "unnamed"))
    argv = check.get("argv")
    if not isinstance(argv, list) or not argv or not all(isinstance(item, str) and item for item in argv):
        return result(name, "unable", "command needs a non-empty argv list")
    if Path(argv[0]).name.lower() not in ALLOWED_COMMANDS:
        return result(name, "unable", "command executable is not on the local allow-list")
    if not run_commands:
        return result(name, "unable", "command execution requires --run-commands")
    cwd_value = check.get("cwd", ".")
    if not isinstance(cwd_value, str):
        return result(name, "unable", "command cwd must be a string")
    try:
        cwd = safe_path(root, cwd_value)
    except ValueError as exc:
        return result(name, "unable", str(exc))
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            capture_output=True,
            timeout=float(check.get("timeout_seconds", 30)),
            check=False,
        )
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        return result(name, "unable", "command could not complete: " + type(exc).__name__)
    return result(name, "pass" if completed.returncode == 0 else "fail", f"exit code {completed.returncode}")


def verify(finding_path: Path, root: Path, run_commands: bool) -> dict[str, Any]:
    try:
        finding = json.loads(finding_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read finding: {exc}") from exc
    checks = finding.get("checks")
    if checks is None:
        return {"version": 1, "status": "unable", "reason": "finding has no checks block", "checks": []}
    if not isinstance(checks, list) or not checks:
        raise ValueError("checks must be a non-empty list")
    reports: list[dict[str, str]] = []
    for check in checks:
        if not isinstance(check, dict):
            raise ValueError("each check must be an object")
        kind = check.get("kind")
        if kind == "file_exists":
            reports.append(verify_file_exists(root, check))
        elif kind == "file_sha256":
            reports.append(verify_sha256(root, check))
        elif kind == "command":
            reports.append(verify_command(root, check, run_commands))
        else:
            reports.append(result(str(check.get("name", "unnamed")), "unable", "unsupported check kind"))
    statuses = {item["status"] for item in reports}
    status = "fail" if "fail" in statuses else "unable" if "unable" in statuses else "pass"
    return {"version": 1, "status": status, "checks": reports}


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="ambrose-c21-") as directory:
        root = Path(directory)
        target = root / "evidence.txt"
        target.write_text("operator-authored evidence\n", encoding="utf-8")
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        finding = root / "finding.json"
        finding.write_text(json.dumps({"checks": [
            {"name": "evidence exists", "kind": "file_exists", "path": "evidence.txt"},
            {"name": "evidence hash", "kind": "file_sha256", "path": "evidence.txt", "sha256": digest},
            {"name": "local command", "kind": "command", "argv": [sys.executable, "-c", "raise SystemExit(0)"]},
        ]}), encoding="utf-8")
        report = verify(finding, root, True)
    print(json.dumps(report, sort_keys=True))
    return STATUS_CODES[report["status"]]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify safe machine-checkable finding checks.")
    parser.add_argument("--finding")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--run-commands", action="store_true")
    parser.add_argument("--pretty", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.finding:
        parser.error("--finding is required unless --self-test is used")
    try:
        report = verify(Path(args.finding), Path(args.root).resolve(), args.run_commands)
    except ValueError as exc:
        print(f"C-21 verifier: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2 if args.pretty else None))
    return STATUS_CODES[report["status"]]


if __name__ == "__main__":
    raise SystemExit(main())

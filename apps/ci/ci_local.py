# Project Ambrose by Imjustchico
# Runs every step of CI's checks job on a contributor's own clone, read from .github/workflows/core-build.yml so it can never drift from what CI runs, over the commits the next push would send, with the path check for the branch named: a green run here is a green checks job on the pull request.
import argparse
import io
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORKFLOW = os.path.join(".github", "workflows", "core-build.yml")
PROJECT = "Justchicoo/Project-Ambrose"


def steps(root):
    text = io.open(os.path.join(root, WORKFLOW), encoding="utf-8").read()
    job = text.split("  checks:", 1)[1].split("\n  windows-cache:", 1)[0]
    return re.findall(r"- name: (.+?)\n(?:\s+if: .+\n)?(?:\s+env:\n(?:\s{10,}.+\n)+)?\s+run: (python apps/\S+(?: [^\n]*)?)", job)


def upstream(root):
    remotes = subprocess.run(["git", "remote", "-v"], cwd=root, capture_output=True, text=True).stdout.splitlines()
    for line in remotes:
        parts = line.split()
        if len(parts) >= 2 and PROJECT.lower() in parts[1].lower():
            return parts[0]
    return "origin"


def command_for(name, command, base, branch):
    if "ci_contrib_paths" in command:
        return f'python apps/ci/ci_contrib_paths.py --range "{base}...HEAD" --branch "{branch}"' if branch else None
    if "ci_roadmap_state" in command:
        return f'python apps/ci/ci_roadmap_state.py --range "{base}..HEAD" --branch "{branch}"'
    if "ci_commit_trailer" in command:
        return f'python apps/ci/ci_commit_trailer.py --range "{base}..HEAD"'
    return command


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run CI's checks job locally over the commits the next push would send.")
    parser.add_argument("--branch", default="", help="the branch the pull request comes from, such as milestone/6.10-server-schemas; the path check runs only when it is given")
    parser.add_argument("--base", default="", help="what the pull request targets; defaults to <the remote for github.com/Justchicoo/Project-Ambrose>/main, fetched first")
    parser.add_argument("--list", action="store_true", help="print the steps and their commands without running them")
    arguments = parser.parse_args(argv)
    base = arguments.base
    if not base:
        remote = upstream(ROOT)
        subprocess.run(["git", "fetch", "-q", remote, "main"], cwd=ROOT)
        base = f"{remote}/main"
    planned = [(name, command_for(name, command, base, arguments.branch)) for name, command in steps(ROOT)]
    if arguments.list:
        for name, command in planned:
            print(f"{name}: {command or 'skipped: give --branch to run it'}")
        return 0
    failed = []
    for name, command in planned:
        if command is None:
            print(f"SKIP  {name}: give --branch to run it")
            continue
        result = subprocess.run(command, cwd=ROOT, shell=True, capture_output=True, text=True)
        if result.returncode:
            failed.append(name)
            print(f"FAIL  {name}: {command}")
            for line in (result.stdout + result.stderr).strip().splitlines()[-8:]:
                print(f"      {line[:200]}")
        else:
            print(f"ok    {name}")
    print(f"checks job over {base}..HEAD: " + ("all passed" if not failed else "failed: " + ", ".join(failed)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

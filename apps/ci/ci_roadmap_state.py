# Project Ambrose by Imjustchico
# Checks that a change which ticks acceptance checks in a phase file also says so in doc/ROADMAP.md's "Where we are", because the summary is what every reader and every contributor's assistant is told to trust, and it is the one part of the roadmap nothing else can regenerate.
import argparse
import os
import subprocess
import sys

ROADMAP = "doc/ROADMAP.md"
PHASES = "doc/roadmap/phase-"
MILESTONE_PREFIX = "milestone/"
TICKED = "+- [x]"


def run(arguments, root):
    result = subprocess.run(arguments, cwd=root, capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return result.stdout


def newly_ticked(root, commit_range):
    output = run(["git", "diff", "-U0", commit_range, "--", "doc/roadmap"], root)
    if output is None:
        return None
    return [line[1:].strip() for line in output.splitlines() if line.startswith(TICKED)]


def changed_names(root, commit_range):
    output = run(["git", "diff", "--name-only", commit_range], root)
    if output is None:
        return None
    return [line.strip().replace("\\", "/") for line in output.splitlines() if line.strip()]


def problems(ticked, names, branch):
    if (branch or "").startswith(MILESTONE_PREFIX):
        return []
    if not ticked:
        return []
    if ROADMAP in (names or []):
        return []
    return [f"{len(ticked)} acceptance check(s) were ticked, but {ROADMAP} was not updated in the same change"]


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose roadmap summary check")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--range", dest="commit_range", required=True, help="a commit range such as main...HEAD")
    parser.add_argument("--branch", default="", help="the branch the change is on; a milestone branch is exempt, because it may not touch the roadmap")
    arguments = parser.parse_args(argv)

    ticked = newly_ticked(arguments.root, arguments.commit_range)
    names = changed_names(arguments.root, arguments.commit_range)
    if ticked is None or names is None:
        print(f"roadmap summary: {arguments.commit_range} could not be diffed, so nothing is checked")
        return 0

    found = problems(ticked, names, arguments.branch)
    if not found:
        print(f"roadmap summary: {len(ticked)} newly ticked check(s), nothing to report")
        return 0
    for problem in found:
        print(problem)
    for line in ticked[:5]:
        print(f"  {line[:120]}")
    print(f"Say in {ROADMAP}'s \"Where we are\" what is built now. It is the paragraph the README, the tracks and every")
    print("contributor's assistant are told to read first, and a generated file cannot write it.")
    return 1


if __name__ == "__main__":
    sys.exit(main())

# Project Ambrose by Imjustchico
# Checks that a change which ticks acceptance checks in a phase file also says so in doc/ROADMAP.md's "Where we are", because the summary is what every reader and every contributor's assistant is told to trust, and it is the one part of the roadmap no generator can write; and refuses a change that turns a ticked check back into an empty one, which is how a whole-file checkout of an older branch once reverted three finished milestones without anybody deciding to, unless a commit in the change says why on an Unticks: line. A commit that says Restores: may put back ticks that were lost without touching the summary, which already described them.
import argparse
import os
import subprocess
import sys

ROADMAP = "doc/ROADMAP.md"
PHASES = "doc/roadmap/phase-"
MILESTONE_PREFIX = "milestone/"
TICKED = "+- [x]"
REMOVED_TICK = "-- [x]"
ADDED_EMPTY = "+- [ ]"
UNTICKS = "Unticks:"
RESTORES = "Restores:"
SHORTEST_CHECK = 20


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


def unticked_in(lines):
    removed = [" ".join(line[len(REMOVED_TICK):].split()) for line in lines if line.startswith(REMOVED_TICK)]
    emptied = [" ".join(line[len(ADDED_EMPTY):].split()) for line in lines if line.startswith(ADDED_EMPTY)]
    return [text for text in removed if any(len(empty) >= SHORTEST_CHECK and text.startswith(empty) for empty in emptied)]


def unticked(root, commit_range):
    output = run(["git", "diff", "-U0", commit_range, "--", "doc/roadmap"], root)
    if output is None:
        return None
    return unticked_in(output.splitlines())


def messages(root, commit_range):
    output = run(["git", "log", "--format=%B", commit_range.replace("...", "..")], root)
    return output or ""


def changed_names(root, commit_range):
    output = run(["git", "diff", "--name-only", commit_range], root)
    if output is None:
        return None
    return [line.strip().replace("\\", "/") for line in output.splitlines() if line.strip()]


def problems(ticked, names, branch, emptied=(), message=""):
    found = []
    if emptied and UNTICKS not in message:
        found.append(f"{len(emptied)} acceptance check(s) went from ticked back to empty; if that is meant, say why on an '{UNTICKS}' line in the commit message")
    if (branch or "").startswith(MILESTONE_PREFIX):
        return found
    if ticked and ROADMAP not in (names or []) and RESTORES not in message:
        found.append(f"{len(ticked)} acceptance check(s) were ticked, but {ROADMAP} was not updated in the same change")
    return found


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose roadmap summary check")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--range", dest="commit_range", required=True, help="a commit range such as main...HEAD")
    parser.add_argument("--branch", default="", help="the branch the change is on; a milestone branch is exempt, because it may not touch the roadmap")
    arguments = parser.parse_args(argv)

    ticked = newly_ticked(arguments.root, arguments.commit_range)
    names = changed_names(arguments.root, arguments.commit_range)
    emptied = unticked(arguments.root, arguments.commit_range)
    if ticked is None or names is None or emptied is None:
        print(f"roadmap summary: {arguments.commit_range} could not be diffed, so nothing is checked")
        return 0

    found = problems(ticked, names, arguments.branch, emptied, messages(arguments.root, arguments.commit_range))
    for line in emptied:
        print(f"  unticked: {line[:120]}")
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

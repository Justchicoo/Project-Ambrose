# Project Ambrose by Imjustchico
# Checks that a change stays inside the contributor track's own folders, exactly the ones doc/CONTRIBUTOR-TRACK.md's table names, so outside work cannot collide with a milestone in flight, and that a branch named for a milestone, which is allowed the source tree instead, is not one the maintainer holds, and keeps off the files that govern the project and out of every phase file but its own.
import argparse
import json
import os
import re
import subprocess
import sys

ALLOWED_PREFIXES = (
    "contrib/tools/",
    "contrib/findings/",
    "contrib/notes/",
    "contrib/proposals/",
    "contrib/locale/",
    "contrib/fixtures/",
    "contrib/schemas/",
    "apps/clientdriver/scenarios/",
    "data/sql/updates/pending_db_world/",
    "data/fuzz/",
    "doc/guides/",
)

ALLOWED_FILES = ()

TRACK = "doc/CONTRIBUTOR-TRACK.md"
MILESTONE_TRACK = "doc/MILESTONE-TRACK.md"
HOLDS = "doc/work/holds.json"
BOARD = "https://justchicoo.github.io/Project-Ambrose/"
MILESTONE_BRANCH = re.compile(r"^(?:.*/)?milestone/(\d+)\.(\d+)(?:-.*)?$")
ROADMAP_DIR = "doc/roadmap/"

RESERVED_PREFIXES = (
    ".github/",
    "apps/ci/",
    "apps/codestyle/",
    "apps/progress/",
    "doc/progress/",
    "packages/ui/src/tokens/",
)

RESERVED_FILES = (
    ".gitignore",
    "CLAUDE.md",
    "CMakePresets.json",
    "CONTRIBUTING.md",
    "LICENSE",
    "README.md",
    "THIRD-PARTY-NOTICES.md",
    "contrib/AI-MILESTONES-HERE.md",
    "contrib/AI-START-HERE.md",
    "contrib/README.md",
    "doc/ARCHITECTURE.md",
    "doc/CONTRIBUTOR-TRACK.md",
    "doc/MILESTONE-TRACK.md",
    "doc/REVIEWING.md",
    "doc/ROADMAP.md",
    "vcpkg.json",
)


def run(arguments, root):
    result = subprocess.run(arguments, cwd=root, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"contributor paths: {' '.join(arguments)} failed: {result.stderr.strip()}")
    return result.stdout


def changed_paths(root, commit_range):
    output = run(["git", "diff", "--name-only", commit_range], root)
    return [line.strip().replace("\\", "/") for line in output.splitlines() if line.strip()]


def allowed(path):
    if path in ALLOWED_FILES:
        return True
    return any(path.startswith(prefix) for prefix in ALLOWED_PREFIXES)


def check(paths):
    return [path for path in paths if not allowed(path)]


def milestone_of(branch):
    if not branch:
        return None
    found = MILESTONE_BRANCH.match(branch.strip())
    return f"{found.group(1)}.{int(found.group(2)):02d}" if found else None


def holds(root):
    try:
        with open(os.path.join(root, HOLDS), "r", encoding="utf-8") as handle:
            return json.load(handle).get("holds", [])
    except (OSError, ValueError):
        return []


def held_by(milestone, kept):
    phase = milestone.split(".")[0]
    for entry in kept:
        scope = str(entry.get("scope", ""))
        if scope == "milestone:" + milestone or scope == "phase:" + phase:
            return entry
    return None


def phase_prefix(milestone):
    return f"{ROADMAP_DIR}phase-{int(milestone.split('.')[0]):02d}-"


def check_milestone(paths, milestone):
    refused = []
    for path in paths:
        if any(path.startswith(prefix) for prefix in RESERVED_PREFIXES) or path in RESERVED_FILES:
            refused.append((path, "the maintainer keeps this file; say in the pull request what it needs"))
        elif path.startswith(ROADMAP_DIR) and not path.startswith(phase_prefix(milestone)):
            refused.append((path, f"another phase than {milestone}'s own"))
    return refused


def report_milestone(paths, milestone, root=None):
    hold = held_by(milestone, holds(root)) if root else None
    if hold:
        scope = "phase " + hold["scope"].split(":")[1] if hold["scope"].startswith("phase:") else "milestone " + milestone
        print(f"{milestone} is held: {scope} belongs to {hold.get('who', 'the maintainer')}"
              + (f", who is building {hold['what']}" if hold.get("what") else ""))
        print(f"Nothing inside a hold can be taken from outside. The board says what is open right now: {BOARD}")
        return 1
    refused = check_milestone(paths, milestone)
    for path, reason in refused:
        print(f"{path}: {reason}")
    ticks = [path for path in paths if path.startswith(phase_prefix(milestone))]
    print(f"milestone {milestone}: {len(paths)} path(s) checked, {len(refused)} refused")
    if not ticks:
        print(f"note: nothing in {phase_prefix(milestone)}*.md changed, so no acceptance check is ticked by this branch")
    if refused:
        print(f"A milestone branch may change the source tree, but not the files above; see {MILESTONE_TRACK}")
        return 1
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose contributor track path check")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--range", dest="commit_range", help="a commit range such as main..HEAD")
    parser.add_argument("--paths", nargs="*", help="paths to check instead of a commit range")
    parser.add_argument("--branch", help="the branch the change is on, so a milestone/<id> branch is checked as a milestone")
    arguments = parser.parse_args(argv)

    if arguments.paths is not None:
        paths = [path.replace("\\", "/") for path in arguments.paths]
    elif arguments.commit_range:
        paths = changed_paths(arguments.root, arguments.commit_range)
    else:
        sys.exit("contributor paths: give --range or --paths")

    if not paths:
        print("contributor paths: nothing changed")
        return 0

    milestone = milestone_of(arguments.branch)
    if milestone:
        return report_milestone(paths, milestone, arguments.root)

    outside = check(paths)
    for path in outside:
        print(f"{path}: outside the contributor track")
    print(f"contributor paths: {len(paths)} path(s) checked, {len(outside)} outside the track")
    if outside:
        print(f"The contributor track may change only these folders: {', '.join(ALLOWED_PREFIXES)}")
        print(f"A milestone is taken on a branch named milestone/<id>; see {MILESTONE_TRACK}")
        print(f"Everything else belongs to a milestone in doc/ROADMAP.md; see {TRACK}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

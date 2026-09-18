# Project Ambrose by Imjustchico
# Checks that a change stays inside the contributor track's own folders, so outside work cannot collide with a milestone in flight.
import argparse
import os
import subprocess
import sys

ALLOWED_PREFIXES = (
    "contrib/",
    "apps/clientdriver/scenarios/",
    "data/sql/custom/db_world/",
    "data/fuzz/",
    "doc/guides/",
)

ALLOWED_FILES = ()

TRACK = "doc/CONTRIBUTOR-TRACK.md"


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


def main():
    parser = argparse.ArgumentParser(description="Project Ambrose contributor track path check")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    parser.add_argument("--range", dest="commit_range", help="a commit range such as main..HEAD")
    parser.add_argument("--paths", nargs="*", help="paths to check instead of a commit range")
    arguments = parser.parse_args()

    if arguments.paths is not None:
        paths = [path.replace("\\", "/") for path in arguments.paths]
    elif arguments.commit_range:
        paths = changed_paths(arguments.root, arguments.commit_range)
    else:
        sys.exit("contributor paths: give --range or --paths")

    if not paths:
        print("contributor paths: nothing changed")
        return 0

    outside = check(paths)
    for path in outside:
        print(f"{path}: outside the contributor track")
    print(f"contributor paths: {len(paths)} path(s) checked, {len(outside)} outside the track")
    if outside:
        print(f"The contributor track may change only these folders: {', '.join(ALLOWED_PREFIXES)}")
        print(f"Everything else belongs to a milestone in doc/ROADMAP.md; see {TRACK}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

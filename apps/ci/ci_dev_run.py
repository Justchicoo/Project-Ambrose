# Project Ambrose by Imjustchico
# Plans and runs a change's dev run, which boots what the change touches from the binaries a Linux leg just built: select maps a push's or pull request's changed paths to the targets to boot, the panel with its supervisor and patchserver, the login server and the game server, every target for a change to the build, the shared server code or this tool, and writes them as step outputs, and tells the leg selection whether a change has a target it boots so the Linux GCC leg is built for it, and panel checks the built supervisor, patchserver and gameserver are there, builds the panel and runs the Playwright end-to-end specs against them, failing on a missing binary rather than letting every spec skip.
import argparse
import os
import shutil
import subprocess
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

EVERYTHING = (".github/workflows/core-build.yml", "apps/ci/ci_dev_run.py", "CMakeLists.txt", "CMakePresets.json", "cmake/", "vcpkg.json",
              "src/common/", "src/server/shared/", "src/server/database/")
TARGETS = OrderedDict([
    ("panel", ("apps/dashboard/", "packages/ui/", "design/", "tests/e2e/", "playwright.config.ts", "package.json", "package-lock.json",
               "src/server/apps/supervisor/", "src/server/apps/patchserver/")),
    ("loginserver", ("src/server/apps/loginserver/", "data/sql/")),
    ("gameserver", ("src/server/apps/gameserver/", "src/server/game/", "src/server/scripts/", "modules/", "data/")),
])
BOOTED = ("panel",)
PANEL_PROGRAMS = ("supervisor", "patchserver", "gameserver")
PANEL_ENVIRONMENT = {"supervisor": "AMBROSE_SUPERVISOR", "patchserver": "AMBROSE_PANEL_APP", "gameserver": "AMBROSE_GAMESERVER"}
DASHBOARD = os.path.join("apps", "dashboard", "dist", "index.html")


def matches(path, entry):
    return path.startswith(entry) if entry.endswith("/") else path == entry


def targets_for(paths):
    if paths is None:
        return list(TARGETS), ["every target: the change could not be diffed"]
    reasons = []
    widened = sorted({path for path in paths if any(matches(path, entry) for entry in EVERYTHING)})
    if widened:
        return list(TARGETS), [f"every target: {widened[0]} is shared by every app" + (f" (and {len(widened) - 1} more)" if len(widened) > 1 else "")]
    chosen = []
    for target, entries in TARGETS.items():
        hits = sorted(path for path in paths if any(matches(path, entry) for entry in entries))
        if hits:
            chosen.append(target)
            reasons.append(f"{target}: {hits[0]}" + (f" and {len(hits) - 1} more" if len(hits) > 1 else ""))
    if not chosen:
        reasons.append("no target: the change touches nothing a dev run boots")
    return chosen, reasons


def boots(paths):
    return any(target in BOOTED for target in targets_for(paths)[0])


def plan(event, before, after, git):
    if event == "pull_request":
        return targets_for(git.changed_paths(before, after, merge_base=True))
    if event == "push":
        return targets_for(git.changed_paths(before, after))
    chosen, _ = targets_for(None)
    return chosen, [f"every target: a {event or 'local'} run boots everything"]


def output_lines(chosen):
    return [f"targets={' '.join(chosen)}"] + [f"{target}={'true' if target in chosen else 'false'}" for target in TARGETS]


def summary_text(chosen, reasons):
    lines = [f"- {reason}" for reason in reasons]
    lines += [f"- {target}: selected, but not booted in CI yet" for target in chosen if target not in BOOTED]
    return "### Dev run\n\n" + "\n".join(lines) + "\n"


def select(arguments, environment, git):
    if arguments.from_github_env:
        event = environment.get("EVENT_NAME", "")
        before = environment.get("PR_BASE" if event == "pull_request" else "PUSH_BEFORE", "")
        after = environment.get("PR_HEAD" if event == "pull_request" else "PUSH_AFTER", "")
    else:
        event, before, after = arguments.event, arguments.before, arguments.after
    chosen, reasons = plan(event, before, after, git)
    print(summary_text(chosen, reasons), end="")
    if environment.get("GITHUB_OUTPUT"):
        with open(environment["GITHUB_OUTPUT"], "a", encoding="utf-8") as handle:
            handle.write("\n".join(output_lines(chosen)) + "\n")
    if environment.get("GITHUB_STEP_SUMMARY"):
        with open(environment["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as handle:
            handle.write(summary_text(chosen, reasons))
    return 0


def program(folder, name):
    return os.path.abspath(os.path.join(folder, name + (".exe" if os.name == "nt" else "")))


def panel_environment(binaries, environment):
    prepared = dict(environment)
    prepared.setdefault("CI", "1")
    for name in PANEL_PROGRAMS:
        prepared[PANEL_ENVIRONMENT[name]] = program(binaries, name)
    return prepared


def panel_commands(npm, npx):
    return [[npm, "run", "build", "--workspace", "apps/dashboard"], [npx, "playwright", "test", "--project", "e2e"]]


def panel(arguments, environment, runner=subprocess.run, which=shutil.which):
    binaries = os.path.join(ROOT, arguments.binaries) if not os.path.isabs(arguments.binaries) else arguments.binaries
    missing = [program(binaries, name) for name in PANEL_PROGRAMS if not os.path.isfile(program(binaries, name))]
    if missing:
        print("the dev run needs the built " + ", ".join(missing) + "; build the leg first", file=sys.stderr)
        return 1
    npm, npx = which("npm"), which("npx")
    if not npm or not npx:
        print("the dev run needs npm and npx on the PATH, with the workspace installed by npm ci", file=sys.stderr)
        return 1
    prepared = panel_environment(binaries, environment)
    for command in panel_commands(npm, npx):
        print("+ " + " ".join(command), flush=True)
        if runner(command, cwd=ROOT, env=prepared).returncode != 0:
            return 1
    if not os.path.isfile(os.path.join(ROOT, DASHBOARD)):
        print(f"the panel build left no {DASHBOARD}", file=sys.stderr)
        return 1
    return 0


def main(argv=None, environment=None, git=None, runner=subprocess.run, which=shutil.which):
    parser = argparse.ArgumentParser(description="Project Ambrose dev run")
    commands = parser.add_subparsers(dest="command", required=True)
    chooser = commands.add_parser("select", help="map a change's paths to the targets its dev run boots")
    chooser.add_argument("--from-github-env", action="store_true", help="read the event and range from GitHub variables and write step outputs")
    chooser.add_argument("--event", default="", help="push or pull_request, for a dry run; anything else boots every target")
    chooser.add_argument("--before", default="", help="the range start, for a dry run")
    chooser.add_argument("--after", default="", help="the range end, for a dry run")
    runner_parser = commands.add_parser("panel", help="run the panel's end-to-end specs against built binaries")
    runner_parser.add_argument("--binaries", required=True, help="the folder holding the built supervisor, patchserver and gameserver")
    arguments = parser.parse_args(argv)
    environment = dict(os.environ) if environment is None else environment
    if arguments.command == "select":
        if git is None:
            from ci_select_legs import Git
            git = Git()
        return select(arguments, environment, git)
    return panel(arguments, environment, runner, which)


if __name__ == "__main__":
    sys.exit(main())

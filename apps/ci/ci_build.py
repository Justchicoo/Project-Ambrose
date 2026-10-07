# Project Ambrose by Imjustchico
# Sets up pinned vcpkg when asked, then configures, builds, and tests one preset, in one go or one stage at a time, for CI or local verification, running the tests in parallel on every core unless told otherwise; or brings a long-lived clone to a commit of another repository, keeping its build trees so each build is incremental, and runs several presets one after another as legs, going on past a failed leg and ending with how long each leg spent configuring, building and testing; or builds one target of an already configured tree and runs only the tests its own program holds, at the job counts a quick bus job sets; or profiles a tree already built, from Ninja's log and CTest's cost data, printing where its build and test time went.
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import PureWindowsPath

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VCPKG_URL = "https://github.com/microsoft/vcpkg.git"
STAGES = ("all", "configure", "build-test")


def run(command, cwd=ROOT, environment=None):
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=environment, check=True)


def setup_vcpkg(path):
    with open(os.path.join(ROOT, "vcpkg.json"), encoding="utf-8") as handle:
        baseline = json.load(handle)["builtin-baseline"]
    executable = os.path.join(path, "vcpkg.exe" if os.name == "nt" else "vcpkg")
    if not os.path.isdir(os.path.join(path, ".git")):
        os.makedirs(path, exist_ok=True)
        run(["git", "init", "-q"], cwd=path)
        run(["git", "remote", "add", "origin", VCPKG_URL], cwd=path)
    run(["git", "fetch", "-q", "--depth", "1", "origin", baseline], cwd=path)
    run(["git", "checkout", "-q", "FETCH_HEAD"], cwd=path)
    if not os.path.isfile(executable):
        if os.name == "nt":
            run([os.path.join(path, "bootstrap-vcpkg.bat"), "-disableMetrics"], cwd=path)
        else:
            run(["sh", os.path.join(path, "bootstrap-vcpkg.sh"), "-disableMetrics"], cwd=path)
    return path


def prepare_binary_cache(environment):
    for source in environment.get("VCPKG_BINARY_SOURCES", "").split(";"):
        parts = source.split(",")
        if len(parts) >= 2 and parts[0] == "files":
            os.makedirs(parts[1], exist_ok=True)


def commands(args):
    configure = ["cmake", "--preset", args.configure_preset]
    if args.warnings_as_errors:
        configure.append("-DAMBROSE_WARNINGS_AS_ERRORS=ON")
    steps = []
    if args.stage in ("all", "configure"):
        steps.append(["cmake", "--version"])
        steps.append(configure)
    if args.stage in ("all", "build-test"):
        steps.append(["cmake", "--build", "--preset", args.build_preset])
        test = ["ctest", "--preset", args.test_preset or args.build_preset]
        if args.test_jobs > 1:
            test += ["--parallel", str(args.test_jobs)]
        if getattr(args, "exclude_label", None):
            test += ["--label-exclude", "^(" + "|".join(args.exclude_label) + ")$"]
        steps.append(test)
    return steps


def tests_of(listing, targets, pattern=None):
    names = []
    wanted = {target.lower() for target in targets}
    for test in json.loads(listing).get("tests", []):
        command = test.get("command") or []
        if not command:
            continue
        program = PureWindowsPath(command[0]).stem.lower()
        if program in wanted and (not pattern or re.search(pattern, test["name"])):
            names.append(test["name"])
    return names


def run_targets(args, environment, runner=None, lister=None):
    runner = runner or run
    lister = lister or (lambda command: subprocess.run(command, cwd=ROOT, env=environment, check=True, capture_output=True, text=True).stdout)
    build = ["cmake", "--build", "--preset", args.build_preset]
    for target in args.target:
        build += ["--target", target]
    if args.jobs:
        build += ["--parallel", str(args.jobs)]
    preset = args.test_preset or args.build_preset
    try:
        runner(build, environment=environment)
        names = tests_of(lister(["ctest", "--preset", preset, "--show-only=json-v1"]), args.target, args.tests)
        if not names:
            print(f"no test of {', '.join(args.target)}{' matches ' + args.tests if args.tests else ''}; built only", flush=True)
            return 0
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as listed:
            listed.write("\n".join(names) + "\n")
        try:
            test = ["ctest", "--preset", preset, "--tests-from-file", listed.name]
            if args.test_jobs > 1:
                test += ["--parallel", str(args.test_jobs)]
            if args.exclude_label:
                test += ["--label-exclude", "^(" + "|".join(args.exclude_label) + ")$"]
            print(f"running {len(names)} test(s) of {', '.join(args.target)}", flush=True)
            runner(test, environment=environment)
        finally:
            os.unlink(listed.name)
    except subprocess.CalledProcessError as error:
        return error.returncode or 1
    return 0


def parse_leg(text):
    parts = text.split(":")
    if not 1 <= len(parts) <= 3 or not all(parts):
        raise argparse.ArgumentTypeError(f"{text} is not CONFIGURE[:BUILD[:TEST]]")
    configure = parts[0]
    build = parts[1] if len(parts) > 1 else configure
    return {"configure": configure, "build": build, "test": parts[2] if len(parts) > 2 else build}


def leg_arguments(args, leg):
    return argparse.Namespace(configure_preset=leg["configure"], build_preset=leg["build"], test_preset=leg["test"], warnings_as_errors=args.warnings_as_errors,
                              stage="all", test_jobs=args.test_jobs, exclude_label=args.exclude_label)


def stage_of(command):
    if command[0] == "ctest":
        return "test"
    return "build" if "--build" in command else "configure"


def run_leg(args, leg, environment, runner=None):
    runner = runner or run
    spent = {"configure": 0.0, "build": 0.0, "test": 0.0}
    result = 0
    for command in commands(leg_arguments(args, leg)):
        started = time.monotonic()
        try:
            runner(command, environment=environment)
        except subprocess.CalledProcessError as error:
            result = error.returncode or 1
        spent[stage_of(command)] += time.monotonic() - started
        if result:
            break
    return {"leg": leg["build"], "result": result, **spent}


def summary(rows):
    lines = [f"{'leg':<24}{'configure':>11}{'build':>9}{'test':>9}{'total':>9}  result"]
    for row in rows:
        total = row["configure"] + row["build"] + row["test"]
        lines.append(f"{row['leg']:<24}{row['configure']:>10.0f}s{row['build']:>8.0f}s{row['test']:>8.0f}s{total:>8.0f}s  {'passed' if row['result'] == 0 else 'failed with ' + str(row['result'])}")
    return "\n".join(lines)


COMPILED = (".o", ".obj")


def ninja_steps(path):
    latest = {}
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 5 or not fields[0].isdigit() or not fields[1].isdigit():
                continue
            latest[fields[3]] = ((int(fields[1]) - int(fields[0])) / 1000.0, (fields[0], fields[1], fields[4]))
    steps = {}
    seen = set()
    for output, (seconds, edge) in latest.items():
        if edge in seen:
            continue
        seen.add(edge)
        steps[output] = seconds
    return steps


def test_costs(path):
    costs = {}
    with open(path, encoding="utf-8", errors="replace") as data:
        for line in data:
            if line.startswith("---"):
                break
            fields = line.split()
            if len(fields) == 3:
                try:
                    costs[fields[0]] = float(fields[2])
                except ValueError:
                    continue
    return costs


def profile(tree, top):
    tree = os.path.abspath(tree)
    lines = [f"profile of {tree}"]
    log = os.path.join(tree, ".ninja_log")
    if os.path.isfile(log):
        steps = ninja_steps(log)
        compiled = {output: seconds for output, seconds in steps.items() if output.endswith(COMPILED)}
        other = {output: seconds for output, seconds in steps.items() if not output.endswith(COMPILED)}
        lines.append(f"{len(steps)} build steps, each at its latest run: {len(compiled)} compiles taking {sum(compiled.values()):.0f}s together, "
                     f"{len(other)} other steps such as links taking {sum(other.values()):.0f}s")
        targets = {}
        for output, seconds in compiled.items():
            target = output.split("/CMakeFiles/", 1)[1].split(".dir/", 1)[0] if "/CMakeFiles/" in output else "(none)"
            count, total = targets.get(target, (0, 0.0))
            targets[target] = (count + 1, total + seconds)
        lines.append(f"compile time by target, top {top}:")
        lines += [f"{total:>9.0f}s  {count:>5} files  {target}" for target, (count, total) in sorted(targets.items(), key=lambda item: -item[1][1])[:top]]
        lines.append(f"slowest {top} compiles:")
        lines += [f"{seconds:>9.1f}s  {output}" for output, seconds in sorted(compiled.items(), key=lambda item: -item[1])[:top]]
        lines.append(f"slowest {top} other steps:")
        lines += [f"{seconds:>9.1f}s  {output}" for output, seconds in sorted(other.items(), key=lambda item: -item[1])[:top]]
    else:
        lines.append("no .ninja_log, so the tree was not built with Ninja and its build steps are not timed")
    data = os.path.join(tree, "Testing", "Temporary", "CTestCostData.txt")
    if os.path.isfile(data):
        costs = test_costs(data)
        lines.append(f"{len(costs)} tests taking {sum(costs.values()):.0f}s together at their average cost; slowest {top}:")
        lines += [f"{seconds:>9.1f}s  {name}" for name, seconds in sorted(costs.items(), key=lambda item: -item[1])[:top]]
    else:
        lines.append("no Testing/Temporary/CTestCostData.txt, so no test has run in this tree")
    return "\n".join(lines)


def same_place(first, second):
    return os.path.normcase(os.path.realpath(first)) == os.path.normcase(os.path.realpath(second))


def sync(source, commit, runner=None):
    runner = runner or run
    if same_place(source, ROOT):
        print(f"--sync names {ROOT} itself; sync a separate long-lived clone so its build trees and untracked files are not this tree's", file=sys.stderr)
        return 2
    runner(["git", "fetch", "-q", source, "+refs/heads/*:refs/remotes/source/*"])
    runner(["git", "checkout", "-q", "--detach", commit])
    runner(["git", "clean", "-qfdx", "-e", "build"])
    return 0


def without_sync(argv):
    kept = []
    skip = False
    for argument in argv:
        if skip:
            skip = False
            continue
        if argument in ("--sync", "--commit"):
            skip = True
            continue
        if argument.startswith("--sync=") or argument.startswith("--commit="):
            continue
        kept.append(argument)
    return kept


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    parser = argparse.ArgumentParser(description="Project Ambrose CI build and test")
    parser.add_argument("--configure-preset")
    parser.add_argument("--build-preset")
    parser.add_argument("--test-preset", help="defaults to the build preset name")
    parser.add_argument("--leg", action="append", type=parse_leg, default=[], metavar="CONFIGURE[:BUILD[:TEST]]",
                        help="run this preset as one leg of several, in place of --configure-preset and --build-preset; repeat it for each leg")
    parser.add_argument("--sync", metavar="SOURCE", help="first fetch SOURCE's branches into this clone, check out --commit and remove everything untracked but build/")
    parser.add_argument("--commit", help="the commit --sync checks out")
    parser.add_argument("--setup-vcpkg", metavar="DIR", help="clone and bootstrap vcpkg at the manifest baseline into DIR")
    parser.add_argument("--warnings-as-errors", action="store_true")
    parser.add_argument("--test-jobs", type=int, default=int(os.environ.get("CTEST_PARALLEL_LEVEL") or os.cpu_count() or 1),
                        help="tests run at once; defaults to CTEST_PARALLEL_LEVEL, which a quick bus job sets, or else the number of cores, and 1 runs them one at a time")
    parser.add_argument("--jobs", type=int, default=int(os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or 0) or None,
                        help="compiles run at once with --target; defaults to CMAKE_BUILD_PARALLEL_LEVEL, which a quick bus job sets")
    parser.add_argument("--target", action="append", default=[], help="build only this target of an already configured tree and run only its own tests; repeat it for each")
    parser.add_argument("--tests", metavar="REGEX", help="with --target, run only that target's tests whose names match")
    parser.add_argument("--exclude-label", action="append", default=[], metavar="LABEL", help="skip the tests carrying this CTest label, such as render on a machine with no real display; repeat it for each")
    parser.add_argument("--stage", choices=STAGES, default="all", help="configure installs dependencies and configures; build-test builds and tests an already configured tree")
    parser.add_argument("--profile", metavar="TREE", action="append", default=[], help="print where a built tree's build and test time went, from its .ninja_log and CTestCostData.txt, and build nothing; repeat it for each tree")
    parser.add_argument("--top", type=int, default=15, help="how many of the slowest steps and tests --profile prints")
    args = parser.parse_args(argv)
    if args.profile:
        print("\n\n".join(profile(tree, args.top) for tree in args.profile), flush=True)
        return 0
    if args.target:
        if not args.build_preset or args.leg or args.sync or args.stage != "all":
            parser.error("--target needs --build-preset and takes no --leg, --sync or --stage")
        return run_targets(args, dict(os.environ))
    if args.tests:
        parser.error("--tests goes with --target")
    if bool(args.sync) != bool(args.commit):
        parser.error("--sync and --commit go together")
    if not args.leg and not (args.configure_preset and args.build_preset):
        parser.error("give --configure-preset and --build-preset, or one --leg or more")
    if args.leg and (args.configure_preset or args.build_preset or args.test_preset or args.stage != "all"):
        parser.error("--leg runs whole legs and takes no --configure-preset, --build-preset, --test-preset or --stage")

    if args.sync:
        synced = sync(args.sync, args.commit)
        if synced:
            return synced
        command = [sys.executable, os.path.join(ROOT, "apps", "ci", "ci_build.py")] + without_sync(argv)
        print("+ " + " ".join(command), flush=True)
        return subprocess.run(command, cwd=ROOT).returncode

    environment = dict(os.environ)
    if args.setup_vcpkg:
        environment["VCPKG_ROOT"] = setup_vcpkg(os.path.abspath(args.setup_vcpkg))
    if not environment.get("VCPKG_ROOT"):
        print("VCPKG_ROOT is not set; pass --setup-vcpkg DIR or set it", file=sys.stderr)
        return 2
    prepare_binary_cache(environment)

    if args.leg:
        rows = [run_leg(args, leg, environment) for leg in args.leg]
        print(summary(rows), flush=True)
        return 0 if all(row["result"] == 0 for row in rows) else 1

    try:
        for command in commands(args):
            run(command, environment=environment)
    except subprocess.CalledProcessError as error:
        return error.returncode or 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

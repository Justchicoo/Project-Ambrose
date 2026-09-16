# Project Ambrose by Imjustchico
# Sets up pinned vcpkg when asked, then configures, builds, and tests one preset, in one go or one stage at a time, for CI or local verification.
import argparse
import json
import os
import subprocess
import sys

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
        steps.append(["ctest", "--preset", args.test_preset or args.build_preset])
    return steps


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose CI build and test")
    parser.add_argument("--configure-preset", required=True)
    parser.add_argument("--build-preset", required=True)
    parser.add_argument("--test-preset", help="defaults to the build preset name")
    parser.add_argument("--setup-vcpkg", metavar="DIR", help="clone and bootstrap vcpkg at the manifest baseline into DIR")
    parser.add_argument("--warnings-as-errors", action="store_true")
    parser.add_argument("--stage", choices=STAGES, default="all", help="configure installs dependencies and configures; build-test builds and tests an already configured tree")
    args = parser.parse_args(argv)

    environment = dict(os.environ)
    if args.setup_vcpkg:
        environment["VCPKG_ROOT"] = setup_vcpkg(os.path.abspath(args.setup_vcpkg))
    if not environment.get("VCPKG_ROOT"):
        print("VCPKG_ROOT is not set; pass --setup-vcpkg DIR or set it", file=sys.stderr)
        return 2
    prepare_binary_cache(environment)

    try:
        for command in commands(args):
            run(command, environment=environment)
    except subprocess.CalledProcessError as error:
        return error.returncode or 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

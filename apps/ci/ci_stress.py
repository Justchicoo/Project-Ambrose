# Project Ambrose by Imjustchico
# Runs a gtest executable as many concurrent copies, each a number of times, optionally beside busy loops that load the machine, and groups the failing runs by the first failure each one printed with ports, timings and addresses made alike, so a flaky test is measured before and after a fix the same way every time.
import argparse
import multiprocessing
import os
import re
import subprocess
import sys
import threading
import time
from collections import Counter

FAILURE = re.compile(r"^(.*?):(\d+): Failure\r?$")
FAILED_TEST = re.compile(r"^\[  FAILED  \] (\S+\.\S+) \(")
NUMBER = re.compile(r"(?<![A-Za-z])\d+")


def normalized(text):
    return NUMBER.sub("N", text.strip())


def first_failure(output):
    lines = output.splitlines()
    for index, line in enumerate(lines):
        found = FAILURE.match(line)
        if found:
            detail = [text for text in lines[index + 1:index + 6] if text.strip() and not text.startswith("[")]
            reason = next((text for text in reversed(detail) if not re.match(r"^\s*(Value of|Actual|Expected|Which is)", text)), detail[-1] if detail else "")
            source = re.split(r"[\\/]", found.group(1))[-1]
            return f"{source}: {normalized(reason)}"
    if output.startswith("the run could not start"):
        return output.strip()
    for line in lines:
        found = FAILED_TEST.match(line)
        if found:
            return f"{found.group(1)} failed"
    if "tests ran" not in output:
        return "the run ended before it finished"
    return "the run failed with no failure printed"


def failed_tests(output):
    return sorted({found.group(1) for found in map(FAILED_TEST.match, output.splitlines()) if found})


def busy(until):
    sink = 0
    while time.monotonic() < until.value:
        for turn in range(20000):
            sink += turn


class Tally:
    def __init__(self):
        self.lock = threading.Lock()
        self.runs = 0
        self.failures = 0
        self.reasons = Counter()
        self.tests = Counter()

    def add(self, code, output):
        with self.lock:
            self.runs += 1
            if code == 0:
                return False
            self.failures += 1
            self.reasons[first_failure(output)] += 1
            for name in failed_tests(output):
                self.tests[name] += 1
            return True


def worker(index, args, command, tally, keep):
    for run in range(1, args.runs + 1):
        try:
            result = subprocess.run(command, capture_output=True, text=True, errors="replace")
            code, output = result.returncode, result.stdout + result.stderr
        except OSError as error:
            code, output = 1, f"the run could not start: {error.strerror or error}"
        if tally.add(code, output) and keep:
            with open(os.path.join(keep, f"run-{index}-{run}.log"), "w", encoding="utf-8") as log:
                log.write(output)


def stress(args):
    command = [os.path.abspath(args.executable), *args.extra, f"--gtest_filter={args.filter}"]
    keep = args.keep
    if keep:
        os.makedirs(keep, exist_ok=True)
    tally = Tally()
    until = multiprocessing.Value("d", float("inf"))
    loads = [multiprocessing.Process(target=busy, args=(until,), daemon=True) for _ in range(args.busy)]
    for load in loads:
        load.start()
    started = time.monotonic()
    threads = [threading.Thread(target=worker, args=(index, args, command, tally, keep)) for index in range(1, args.copies + 1)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    until.value = 0.0
    for load in loads:
        load.join()
    return tally, time.monotonic() - started


def report(label, tally, seconds, copies, busy_loops):
    lines = [f"{label}: {tally.failures} of {tally.runs} runs failed ({copies} copies, {busy_loops} busy loops, {seconds:.0f} s)"]
    for reason, count in tally.reasons.most_common(10):
        lines.append(f"  {count:6d}  {reason}")
    if tally.tests:
        lines.append("  failing tests:")
        for name, count in tally.tests.most_common(10):
            lines.append(f"  {count:6d}  {name}")
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose flaky test stress run")
    parser.add_argument("executable", help="the gtest executable, such as unit_tests")
    parser.add_argument("--filter", required=True, help="the gtest filter each run is given")
    parser.add_argument("--copies", type=int, default=32, help="copies running at once")
    parser.add_argument("--runs", type=int, default=100, help="runs each copy makes")
    parser.add_argument("--busy", type=int, default=0, help="busy loops loading the machine beside the copies")
    parser.add_argument("--label", default="stress", help="the name the summary is printed under")
    parser.add_argument("--keep", help="a folder to keep each failing run's output in")
    parser.add_argument("extra", nargs="*", help="arguments each run is given before the filter, after --")
    args = parser.parse_args(argv)
    if args.copies < 1 or args.runs < 1 or args.busy < 0:
        parser.error("--copies and --runs must be at least 1 and --busy at least 0")
    if not os.path.isfile(args.executable):
        print(f"ci stress: {args.executable} does not exist", file=sys.stderr)
        return 2
    tally, seconds = stress(args)
    print(report(args.label, tally, seconds, args.copies, args.busy))
    return 1 if tally.failures else 0


if __name__ == "__main__":
    sys.exit(main())

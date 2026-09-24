#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Primes one npm cache with every package the lockfile names, including the platform binaries a Windows install skips and a Linux one needs, so an offline machine can install and build either way.
import argparse
import json
import os
import shutil
import subprocess
import sys

LOCKFILE = "package-lock.json"
DEFAULT_CACHE = ".npm-cache"
BATCH = 40


def npm():
    found = shutil.which("npm") or shutil.which("npm.cmd")
    if not found:
        raise SystemExit("npm was not found on the path")
    return found


def wanted(root):
    with open(os.path.join(root, LOCKFILE), "r", encoding="utf-8") as handle:
        document = json.load(handle)
    specs = {}
    for path, entry in document.get("packages", {}).items():
        if not path or not entry.get("resolved") or not entry.get("version"):
            continue
        name = entry.get("name")
        if not name:
            marker = "node_modules/"
            index = path.rfind(marker)
            if index < 0:
                continue
            name = path[index + len(marker):]
        specs[f"{name}@{entry['version']}"] = entry["resolved"]
    return specs


def run(command, cache, root):
    return subprocess.run(command + ["--cache", cache], cwd=root, capture_output=True, text=True)


def prime(root, cache, report):
    specs = sorted(wanted(root))
    report(f"the lockfile names {len(specs)} packages")
    failures = []
    for start in range(0, len(specs), BATCH):
        batch = specs[start:start + BATCH]
        result = run([npm(), "cache", "add"] + batch, cache, root)
        if result.returncode != 0:
            for spec in batch:
                one = run([npm(), "cache", "add", spec], cache, root)
                if one.returncode != 0:
                    failures.append((spec, one.stderr.strip().split("\n")[-1] if one.stderr else "no reason given"))
        report(f"cached {min(start + BATCH, len(specs))} of {len(specs)}")
    return specs, failures


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose npm cache priming")
    parser.add_argument("--root", default=default_root, help="repository root")
    parser.add_argument("--cache", default=None, help="where the cache is written; defaults to .npm-cache in the repository")
    parser.add_argument("--list", action="store_true", help="print what would be cached and write nothing")
    parser.add_argument("--quiet", action="store_true", help="print only the result")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    cache = os.path.abspath(args.cache or os.path.join(root, DEFAULT_CACHE))

    def report(line):
        if not args.quiet:
            print(line)

    if args.list:
        specs = sorted(wanted(root))
        for spec in specs:
            print(spec)
        print(f"npm cache: {len(specs)} packages the lockfile names")
        return 0

    os.makedirs(cache, exist_ok=True)
    specs, failures = prime(root, cache, report)
    for spec, reason in failures:
        print(f"{spec}: could not be cached: {reason}")
    print(f"npm cache: {len(specs) - len(failures)} of {len(specs)} packages in {cache}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

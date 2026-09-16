# Project Ambrose by Imjustchico
# Prunes stale archives from the vcpkg binary cache folder and names the cache entry after the archives it holds.
import argparse
import hashlib
import os
import re
import sys
import time

ARCHIVE = re.compile(r"^[0-9a-f]{2}/[0-9a-f]{64}\.zip$")
INSTALLED = "install ok installed"
SECONDS_PER_DAY = 86400
KEY_HEX = 20


def installed_abis(status_text):
    abis = set()
    for paragraph in re.split(r"\n\s*\n", status_text.replace("\r\n", "\n")):
        fields = {}
        for line in paragraph.split("\n"):
            name, separator, value = line.partition(":")
            if separator and not line.startswith((" ", "\t")):
                fields[name.strip()] = value.strip()
        if fields.get("Status") == INSTALLED and fields.get("Abi"):
            abis.add(fields["Abi"])
    return abis


def archives(cache_dir):
    found = []
    if not os.path.isdir(cache_dir):
        return found
    for folder in sorted(os.listdir(cache_dir)):
        path = os.path.join(cache_dir, folder)
        if not os.path.isdir(path):
            continue
        for name in sorted(os.listdir(path)):
            relative = f"{folder}/{name}"
            if ARCHIVE.match(relative):
                found.append(relative)
    return found


def prune(cache_dir, keep, now, max_age_days=30, dry_run=False):
    if not keep:
        return []
    removed = []
    cutoff = now - max_age_days * SECONDS_PER_DAY
    for relative in archives(cache_dir):
        abi = relative.split("/")[1][:-len(".zip")]
        path = os.path.join(cache_dir, *relative.split("/"))
        if abi in keep or os.path.getmtime(path) >= cutoff:
            continue
        if not dry_run:
            os.remove(path)
        removed.append(relative)
    return removed


def content_key(cache_dir, prefix):
    found = archives(cache_dir)
    if not found:
        return None
    digest = hashlib.sha256("\n".join(found).encode("utf-8")).hexdigest()
    return prefix + digest[:KEY_HEX]


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose vcpkg binary cache pruning and naming")
    parser.add_argument("--cache-dir", required=True, help="the vcpkg files binary cache folder")
    parser.add_argument("--status", required=True, help="the vcpkg_installed/vcpkg/status file of this build")
    parser.add_argument("--prefix", required=True, help="the cache key prefix, for example vcpkg-Linux-")
    parser.add_argument("--max-age-days", type=int, default=30, help="how old an archive this build does not use must be to be pruned")
    parser.add_argument("--github-output", action="store_true", help="append key=<key> to the file GITHUB_OUTPUT names")
    parser.add_argument("--dry-run", action="store_true", help="report what would be pruned without deleting it")
    args = parser.parse_args(argv)

    keep = set()
    if os.path.isfile(args.status):
        with open(args.status, encoding="utf-8", errors="replace") as handle:
            keep = installed_abis(handle.read())
    else:
        print(f"vcpkg cache: no status file at {args.status}; nothing is pruned")
    removed = prune(args.cache_dir, keep, time.time(), args.max_age_days, args.dry_run)
    key = content_key(args.cache_dir, args.prefix)
    verb = "would prune" if args.dry_run else "pruned"
    print(f"vcpkg cache: {len(archives(args.cache_dir))} archive(s), {len(keep)} installed, {verb} {len(removed)}, key {key or '(none)'}")
    for relative in removed:
        print(f"vcpkg cache: {verb} {relative}")
    if args.github_output and key and os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as handle:
            handle.write(f"key={key}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

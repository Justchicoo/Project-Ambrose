# Project Ambrose by Imjustchico
# Fails when any non-merge commit in a range, or committed in the last eight days on a scheduled run, lacks the Co-Authored-By trailer naming the AI that wrote it, except one authored by a bot account, whose author name already names the tool the trailer would.
import argparse
import datetime
import os
import re
import subprocess
import sys

TRAILER = re.compile(r"^Co-Authored-By:[ \t]*\S.*<[^<>\s]+>[ \t]*$", re.IGNORECASE | re.MULTILINE)
ZERO_SHA = re.compile(r"^0+$")
SCHEDULE_WINDOW_DAYS = 8


def has_trailer(message):
    return bool(TRAILER.search(message))


def written_by_a_bot(author):
    return author.strip().lower().endswith("[bot]")


def git(root, *args):
    return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, check=True).stdout


def range_from_github_environment(environment, now=None):
    event = environment.get("EVENT_NAME", "")
    if event == "schedule":
        moment = now or datetime.datetime.now(datetime.timezone.utc)
        since = (moment - datetime.timedelta(days=SCHEDULE_WINDOW_DAYS)).strftime("%Y-%m-%dT%H:%M:%SZ")
        return [f"--since={since}", environment.get("PUSH_AFTER", "") or "HEAD"]
    if event == "pull_request" and environment.get("PR_BASE") and environment.get("PR_HEAD"):
        return [f"{environment['PR_BASE']}..{environment['PR_HEAD']}"]
    before = environment.get("PUSH_BEFORE", "")
    after = environment.get("PUSH_AFTER", "") or "HEAD"
    if event == "push" and before and not ZERO_SHA.match(before):
        return [f"{before}..{after}"]
    return ["-n", "1", after]


def commits(root, revisions):
    try:
        output = git(root, "rev-list", "--no-merges", *revisions)
    except subprocess.CalledProcessError as error:
        print(f"commit trailer: could not resolve {' '.join(revisions)} ({error.stderr.strip()}); checking HEAD only")
        output = git(root, "rev-list", "--no-merges", "-n", "1", "HEAD")
    return [sha for sha in output.split() if sha]


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose commit trailer check")
    parser.add_argument("--root", default=default_root, help="repository root")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--range", nargs="+", help="rev-list arguments, for example origin/main..HEAD")
    source.add_argument("--from-github-env", action="store_true", help="derive the range from GitHub event variables")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    revisions = range_from_github_environment(os.environ) if args.from_github_env else args.range
    checked = commits(root, revisions)
    missing = 0
    for sha in checked:
        message = git(root, "log", "-1", "--format=%B", sha)
        if written_by_a_bot(git(root, "log", "-1", "--format=%an", sha)):
            continue
        if not has_trailer(message):
            subject = message.strip().splitlines()[0] if message.strip() else "(empty message)"
            print(f"{sha[:10]} {subject}: missing a Co-Authored-By trailer naming the AI model or tool")
            missing += 1
    print(f"commit trailer: {len(checked)} commit(s) checked, {missing} missing a trailer")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())

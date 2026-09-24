# Project Ambrose by Imjustchico
# Adds up this month's billed Actions minutes for the repository from the GitHub API, for checking before a manual CI run.
import argparse
import datetime
import json
import math
import subprocess
import sys
from collections import Counter

UTC = datetime.timezone.utc
INCLUDED_MINUTES = 2000


def parse_time(text):
    return datetime.datetime.fromisoformat(text.replace("Z", "+00:00")) if text else None


def multiplier(labels):
    lowered = [label.lower() for label in labels or []]
    if any(label.startswith("macos") for label in lowered):
        return 10
    if any(label.startswith("windows") for label in lowered):
        return 2
    return 1


def billed_minutes(job, now):
    started = parse_time(job.get("started_at"))
    if not job.get("runner_name") or not started:
        return 0
    finished = parse_time(job.get("completed_at")) or now
    seconds = max((finished - started).total_seconds(), 0)
    return math.ceil(seconds / 60) * multiplier(job.get("labels"))


def month_start(now):
    return now.replace(day=1, hour=0, minute=0, second=0, microsecond=0)


def summarize(runs, now):
    start = month_start(now)
    by_event = Counter()
    by_job = Counter()
    for run in runs:
        for job in run["jobs"]:
            started = parse_time(job.get("started_at"))
            if not started or started < start:
                continue
            minutes = billed_minutes(job, now)
            by_event[run["event"]] += minutes
            by_job[job.get("name", "")] += minutes
    return by_event, by_job


def gh_lines(*args):
    output = subprocess.run(["gh", *args], capture_output=True, text=True, check=True).stdout
    return [line for line in output.splitlines() if line.strip()]


def fetch_runs(repository, now):
    since = month_start(now).strftime("%Y-%m-%d")
    runs = []
    for line in gh_lines("api", "--paginate", f"repos/{repository}/actions/runs?created=>={since}&per_page=100", "--jq", ".workflow_runs[] | {id, event}"):
        run = json.loads(line)
        run["jobs"] = [json.loads(job) for job in gh_lines("api", "--paginate", f"repos/{repository}/actions/runs/{run['id']}/jobs?filter=all&per_page=100", "--jq", ".jobs[] | {name, labels, runner_name, started_at, completed_at}")]
        runs.append(run)
    return runs


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose Actions minutes this month")
    parser.add_argument("--repository", help="owner/name; defaults to the repository gh sees here")
    parser.add_argument("--quota", type=int, default=INCLUDED_MINUTES, help="the included minutes a month")
    args = parser.parse_args(argv)
    now = datetime.datetime.now(UTC)
    try:
        repository = args.repository or gh_lines("repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner")[0]
        runs = fetch_runs(repository, now)
    except (subprocess.CalledProcessError, FileNotFoundError, IndexError) as error:
        print(f"ci usage: could not read runs through gh: {error}", file=sys.stderr)
        return 1
    by_event, by_job = summarize(runs, now)
    total = sum(by_event.values())
    print(f"{repository}: {total} billed minute(s) since {month_start(now):%Y-%m-%d} in {len(runs)} run(s); about {max(args.quota - total, 0)} of {args.quota} left if no other private repository used any")
    for event, minutes in by_event.most_common():
        print(f"  event {event}: {minutes}")
    for job, minutes in by_job.most_common():
        print(f"  job {job}: {minutes}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

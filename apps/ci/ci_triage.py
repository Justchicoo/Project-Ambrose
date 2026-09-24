# Project Ambrose by Imjustchico
# Labels a pull request by the track it belongs to, read from its branch name and the paths it changes, and greets a first-time contributor once with the document their track is described in, so a pull request is sorted the moment it arrives instead of when the maintainer next looks.
import argparse
import json
import os
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
MILESTONE_PREFIX = "milestone/"
MILESTONE_LABEL = "milestone-track"
CONTRIB_LABEL = "contrib"
NEWCOMERS = ("FIRST_TIME_CONTRIBUTOR", "FIRST_TIMER", "NONE")

CONTRIB_PREFIXES = (
    "contrib/",
    "apps/clientdriver/scenarios/",
    "data/sql/updates/pending_db_world/",
    "data/fuzz/",
    "doc/guides/",
)

MILESTONE_WELCOME = """Thanks for taking this one.

It is reviewed by running it, not by reading it: the branch is built, its tests are run, and every ticked acceptance check is run by name. What decides the outcome is in [doc/MILESTONE-TRACK.md](../blob/main/doc/MILESTONE-TRACK.md), and the short version is that a check you could not run stays unticked and is named here, which merges fine, while a check ticked by a test that does not exist ends the review.

The Linux build leg runs on this branch without waiting for a label. Questions are quicker in the [Discord](https://discord.gg/Dx6ACDUj6N) than here."""

CONTRIB_WELCOME = """Thanks for sending this.

It is reviewed by running it, not by reading it, and what it is held to is in [doc/CONTRIBUTOR-TRACK.md](../blob/main/doc/CONTRIBUTOR-TRACK.md). The two things that most often send work back are an item whose shape arrives without its substance, and a claim nothing could prove false.

Build legs run on a pull request only when a maintainer adds a `ci:` label. Questions are quicker in the [Discord](https://discord.gg/Dx6ACDUj6N) than here."""


def labels_for(branch, paths):
    branch = branch or ""
    if branch.startswith(MILESTONE_PREFIX):
        return [MILESTONE_LABEL]
    if paths and all(any(path.startswith(prefix) for prefix in CONTRIB_PREFIXES) for path in paths):
        return [CONTRIB_LABEL]
    return []


def welcome_for(labels, association):
    if (association or "").upper() not in NEWCOMERS:
        return None
    if MILESTONE_LABEL in labels:
        return MILESTONE_WELCOME
    if CONTRIB_LABEL in labels:
        return CONTRIB_WELCOME
    return None


def call(token, method, path, payload=None):
    request = urllib.request.Request(
        API + path,
        data=json.dumps(payload).encode("utf-8") if payload is not None else None,
        method=method,
        headers={"Authorization": "Bearer " + token, "Accept": "application/vnd.github+json",
                 "Content-Type": "application/json", "User-Agent": "Project-Ambrose"})
    with urllib.request.urlopen(request, timeout=30) as answer:
        return json.loads(answer.read().decode("utf-8") or "null")


def changed_paths(token, repository, number):
    paths = []
    for page in range(1, 11):
        batch = call(token, "GET", f"/repos/{repository}/pulls/{number}/files?per_page=100&page={page}")
        if not batch:
            break
        paths.extend(entry["filename"] for entry in batch)
        if len(batch) < 100:
            break
    return paths


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose pull request triage")
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--number", type=int, default=int(os.environ.get("PR_NUMBER", "0") or 0))
    parser.add_argument("--branch", default=os.environ.get("PR_BRANCH", ""))
    parser.add_argument("--association", default=os.environ.get("PR_ASSOCIATION", ""))
    parser.add_argument("--dry-run", action="store_true", help="say what it would do and change nothing")
    arguments = parser.parse_args(argv)
    token = os.environ.get("GITHUB_TOKEN", "").strip()

    if not arguments.repository or not arguments.number:
        print("triage: no repository or pull request number, nothing to do")
        return 0
    if not token and not arguments.dry_run:
        print("triage: no token, nothing to do")
        return 0

    try:
        paths = [] if arguments.dry_run else changed_paths(token, arguments.repository, arguments.number)
    except (urllib.error.URLError, OSError, ValueError) as failure:
        print(f"triage: the files of #{arguments.number} could not be read: {failure}", file=sys.stderr)
        return 0

    labels = labels_for(arguments.branch, paths)
    welcome = welcome_for(labels, arguments.association)
    print(f"triage: #{arguments.number} on '{arguments.branch}' with {len(paths)} file(s) -> {labels or 'no label'}"
          + (", greeting a first-time contributor" if welcome else ""))
    if arguments.dry_run:
        return 0

    try:
        if labels:
            call(token, "POST", f"/repos/{arguments.repository}/issues/{arguments.number}/labels", {"labels": labels})
        if welcome:
            call(token, "POST", f"/repos/{arguments.repository}/issues/{arguments.number}/comments", {"body": welcome})
    except (urllib.error.URLError, OSError, ValueError) as failure:
        print(f"triage: #{arguments.number} could not be labelled or greeted: {failure}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# Project Ambrose by Imjustchico
# Picks the core-build legs for a push, pull request, schedule slot, or manual run, building the Linux GCC leg for a milestone branch without waiting for a label, building a scheduled leg only when code changed since the commit it last built, and says whether the Windows cache needs a keepalive restore.
import argparse
import datetime
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
UTC = datetime.timezone.utc

LEGS = OrderedDict([
    ("windows-msvc-x64", {"os": "windows-latest", "configure": "windows-msvc-x64", "build": "windows-debug", "configure_timeout": 90, "build_timeout": 40}),
    ("linux-gcc", {"os": "ubuntu-latest", "configure": "linux-gcc", "build": "linux-gcc-debug", "configure_timeout": 45, "build_timeout": 20}),
    ("linux-clang", {"os": "ubuntu-latest", "configure": "linux-clang", "build": "linux-clang-debug", "configure_timeout": 45, "build_timeout": 25}),
    ("linux-gcc-asan", {"os": "ubuntu-latest", "configure": "linux-gcc-asan", "build": "linux-gcc-asan", "configure_timeout": 45, "build_timeout": 35}),
    ("linux-clang-tsan", {"os": "ubuntu-latest", "configure": "linux-clang-tsan", "build": "linux-clang-tsan", "configure_timeout": 45, "build_timeout": 25}),
    ("linux-clang-fuzz", {"os": "ubuntu-latest", "configure": "linux-clang-fuzz", "build": "linux-clang-fuzz", "configure_timeout": 45, "build_timeout": 40}),
])
JOB_TIMEOUT_MARGIN = 10

SUNDAY_CRON = "17 6 * * 0"
WEDNESDAY_CRON = "17 6 * * 3"
OTHER_CRON = "17 6 * * 1,2,4,5,6"
CRON_WEEKDAYS = {SUNDAY_CRON: (6,), WEDNESDAY_CRON: (2,), OTHER_CRON: (0, 1, 3, 4, 5)}
SLOT_TIME = datetime.time(6, 17, tzinfo=UTC)

WEEKLY = ["windows-msvc-x64", "linux-gcc-asan", "linux-clang-tsan"]
SETS = {
    "none": [],
    "weekly": WEEKLY,
    "linux": [leg for leg in LEGS if leg != "windows-msvc-x64"],
    "all": list(LEGS),
}
SETS.update({leg: [leg] for leg in LEGS})
OPTIONS = ("none", "linux-gcc", "linux-clang", "linux-gcc-asan", "linux-clang-tsan", "linux-clang-fuzz", "windows-msvc-x64", "weekly", "linux", "all")
DEFAULT_OPTION = "linux-gcc"
LABEL_PREFIX = "ci:"
MILESTONE_PREFIX = "milestone/"

SMOKE_PATHS = (".github/", "apps/ci/ci_build.py", "apps/ci/ci_vcpkg_cache.py", "vcpkg.json")
PUSH_PATHS = (".github/**", "apps/ci/**", "apps/designtokens/**", "apps/progress/**", "apps/site/**", "design/**", "doc/ROADMAP.md", "doc/roadmap/**", "doc/work/**", "vcpkg.json")
CODE_PATHSPEC = (".", ":(exclude)doc", ":(exclude,glob)**/*.md")
ZERO_SHA = re.compile(r"^0*$")

WORKFLOW_FILE = "core-build.yml"
BUILT_CONCLUSIONS = ("success", "failure")
RUNS_SCANNED = 60
API_TIMEOUT_SECONDS = 20
BUILD_JOB = re.compile(r"^build \((?P<leg>[^()]+)\)$")


class SelectionError(Exception):
    pass


class ActionsError(Exception):
    pass


def expand(option):
    if option not in SETS:
        raise SelectionError(f"unknown leg option '{option}'; expected one of {', '.join(OPTIONS)}")
    return list(SETS[option])


def ordered(legs):
    chosen = set(legs)
    return [leg for leg in LEGS if leg in chosen]


def touches_smoke_paths(paths):
    return any(path.startswith(entry) if entry.endswith("/") else path == entry for path in paths for entry in SMOKE_PATHS)


def legs_for_paths(paths):
    return ["linux-gcc"] if touches_smoke_paths(paths) else []


def legs_for_labels(labels_json, warnings):
    try:
        labels = json.loads(labels_json) if labels_json else []
    except json.JSONDecodeError:
        warnings.append(f"pull request labels '{labels_json}' are not JSON; no label builds")
        return []
    legs = []
    for label in labels or []:
        if not isinstance(label, str) or not label.lower().startswith(LABEL_PREFIX):
            continue
        option = label[len(LABEL_PREFIX):].strip().lower()
        if option not in SETS:
            warnings.append(f"label '{label}' names no leg option")
            continue
        legs.extend(SETS[option])
    return legs


def slot_date(cron, now):
    if cron not in CRON_WEEKDAYS:
        raise SelectionError(f"unknown schedule '{cron}'; expected one of {', '.join(CRON_WEEKDAYS)}")
    for back in range(8):
        date = (now - datetime.timedelta(days=back)).date()
        if date.weekday() in CRON_WEEKDAYS[cron] and slot_time(date) <= now:
            return date
    raise SelectionError(f"no {cron} slot in the week before {now.isoformat()}")


def slot_time(date):
    return datetime.datetime.combine(date, SLOT_TIME)


def scheduled_legs(date):
    legs = []
    if date.weekday() in CRON_WEEKDAYS[WEDNESDAY_CRON] or date.weekday() in CRON_WEEKDAYS[SUNDAY_CRON]:
        legs.append("windows-msvc-x64")
    if date.weekday() in CRON_WEEKDAYS[SUNDAY_CRON]:
        legs.extend(["linux-gcc-asan", "linux-clang-tsan"])
        if date.day <= 7:
            legs.extend(["linux-gcc", "linux-clang", "linux-clang-fuzz"])
    return ordered(legs)


class Git:
    def __init__(self, root=ROOT):
        self.root = root

    def run(self, *args):
        return subprocess.run(["git", *args], cwd=self.root, capture_output=True, text=True)

    def has_commit(self, sha):
        return bool(sha) and not ZERO_SHA.match(sha) and self.run("cat-file", "-e", f"{sha}^{{commit}}").returncode == 0

    def changed_paths(self, before, after, merge_base=False):
        if not self.has_commit(before):
            return None
        separator = "..." if merge_base else ".."
        result = self.run("diff", "--name-only", f"{before}{separator}{after or 'HEAD'}")
        if result.returncode != 0:
            return None
        return [line for line in result.stdout.splitlines() if line]

    def code_changed_since(self, sha):
        if not self.has_commit(sha):
            return True
        return self.run("diff", "--quiet", sha, "HEAD", "--", *CODE_PATHSPEC).returncode != 0


class Actions:
    def __init__(self, environment, opener=None):
        self.api = environment.get("GITHUB_API_URL") or "https://api.github.com"
        self.repository = environment.get("GITHUB_REPOSITORY", "")
        self.branch = environment.get("GITHUB_REF_NAME", "") or "main"
        self.token = environment.get("GITHUB_TOKEN", "")
        self.opener = opener or urllib.request.urlopen

    def get(self, path, query):
        request = urllib.request.Request(f"{self.api}/repos/{self.repository}/{path}?{urllib.parse.urlencode(query)}", headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {self.token}",
            "X-GitHub-Api-Version": "2022-11-28",
        })
        try:
            with self.opener(request, timeout=API_TIMEOUT_SECONDS) as response:
                body = json.loads(response.read().decode("utf-8"))
        except (urllib.error.URLError, OSError, ValueError) as error:
            raise ActionsError(f"GET {path} failed: {error}") from error
        if not isinstance(body, dict):
            raise ActionsError(f"GET {path} returned {type(body).__name__}, not an object")
        return body

    def last_built(self, legs):
        if not self.repository or not self.token:
            raise ActionsError("GITHUB_REPOSITORY and GITHUB_TOKEN are needed to read earlier runs")
        wanted = set(legs)
        found = {}
        runs = self.get(f"actions/workflows/{WORKFLOW_FILE}/runs", {"branch": self.branch, "status": "completed", "per_page": RUNS_SCANNED}).get("workflow_runs", [])
        for run in runs:
            if not wanted - found.keys():
                break
            if not isinstance(run, dict) or run.get("event") == "pull_request" or not run.get("head_sha") or run.get("id") is None:
                continue
            for job in self.get(f"actions/runs/{run['id']}/jobs", {"filter": "latest", "per_page": 100}).get("jobs", []):
                match = BUILD_JOB.match(job.get("name", "")) if isinstance(job, dict) else None
                if match and match["leg"] in wanted and match["leg"] not in found and job.get("conclusion") in BUILT_CONCLUSIONS:
                    found[match["leg"]] = run["head_sha"]
        return found


class KnownBuilds:
    def __init__(self, builds):
        self.builds = builds

    def last_built(self, legs):
        return {leg: sha for leg, sha in self.builds.items() if leg in legs}


def short(sha):
    return sha[:12]


def plan(event, inputs, now, git, actions=None):
    reasons = []
    warnings = []
    keepalive = False
    if event == "workflow_dispatch":
        option = inputs.get("legs") or DEFAULT_OPTION
        legs = expand(option)
        reasons.extend(f"{leg}: selected (manual run asked for '{option}')" for leg in legs)
    elif event == "push":
        paths = git.changed_paths(inputs.get("before"), inputs.get("after"))
        if paths is None:
            legs = ["linux-gcc"]
            reasons.append("linux-gcc: selected (the pushed range could not be diffed)")
        else:
            legs = legs_for_paths(paths)
            reasons.append("linux-gcc: selected (the push touches the workflow, the build script, the cache script or vcpkg.json)" if legs else "no legs: the push touches no build input")
    elif event == "pull_request":
        legs = legs_for_labels(inputs.get("labels"), warnings)
        reasons.extend(f"{leg}: selected (pull request label)" for leg in ordered(legs))
        if (inputs.get("branch") or "").startswith(MILESTONE_PREFIX):
            legs.append("linux-gcc")
            reasons.append("linux-gcc: selected (a milestone branch builds without waiting for a label)")
        paths = git.changed_paths(inputs.get("before"), inputs.get("after"), merge_base=True)
        if paths is None or touches_smoke_paths(paths):
            legs.append("linux-gcc")
            reasons.append("linux-gcc: selected (the pull request touches a build input or could not be diffed)")
    elif event == "schedule":
        cron = inputs.get("schedule", "")
        date = slot_date(cron, now)
        legs = []
        candidates = scheduled_legs(date) if cron != OTHER_CRON else []
        built = {}
        if candidates:
            try:
                built = (actions or Actions({})).last_built(candidates)
            except ActionsError as error:
                warnings.append(f"earlier builds could not be read, so every leg of this slot builds: {error}")
        for leg in candidates:
            sha = built.get(leg)
            if sha is None:
                legs.append(leg)
                reasons.append(f"{leg}: selected ({date.strftime('%A')} slot, no earlier build of this leg found)")
            elif git.code_changed_since(sha):
                legs.append(leg)
                reasons.append(f"{leg}: selected ({date.strftime('%A')} slot, code changed since its last build at {short(sha)})")
            else:
                reasons.append(f"{leg}: skipped (no code change since its last build at {short(sha)})")
        keepalive = cron in (SUNDAY_CRON, WEDNESDAY_CRON) and "windows-msvc-x64" not in legs
        if not candidates:
            reasons.append(f"no legs: the {date.strftime('%A')} slot only runs the checks")
    else:
        legs = []
        reasons.append(f"no legs: event '{event}' builds nothing")
    return {"legs": ordered(legs), "reasons": reasons, "warnings": warnings, "windows_keepalive": keepalive}


def matrix(legs):
    include = []
    for leg in legs:
        entry = {"os": LEGS[leg]["os"], "configure": LEGS[leg]["configure"], "build": LEGS[leg]["build"],
                 "configure_timeout": LEGS[leg]["configure_timeout"], "build_timeout": LEGS[leg]["build_timeout"],
                 "job_timeout": LEGS[leg]["configure_timeout"] + LEGS[leg]["build_timeout"] + JOB_TIMEOUT_MARGIN}
        include.append(entry)
    return {"include": include}


def matrix_json(legs):
    return json.dumps(matrix(legs), separators=(",", ":"))


def output_lines(result):
    return [
        f"build={'true' if result['legs'] else 'false'}",
        f"matrix={matrix_json(result['legs'])}",
        f"windows_keepalive={'true' if result['windows_keepalive'] else 'false'}",
    ]


def summary_text(event, result):
    return f"### Build legs for {event}\n\n" + "\n".join(f"- {line}" for line in result["warnings"] + result["reasons"]) + "\n"


def write_github_files(environment, event, result):
    if environment.get("GITHUB_OUTPUT"):
        with open(environment["GITHUB_OUTPUT"], "a", encoding="utf-8") as handle:
            handle.write("\n".join(output_lines(result)) + "\n")
    if environment.get("GITHUB_STEP_SUMMARY"):
        with open(environment["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as handle:
            handle.write(summary_text(event, result))


def parse_now(text):
    moment = datetime.datetime.fromisoformat(text.replace("Z", "+00:00"))
    return (moment if moment.tzinfo else moment.replace(tzinfo=UTC)).astimezone(UTC)


def main(argv=None, environment=None, git=None):
    parser = argparse.ArgumentParser(description="Project Ambrose core-build leg selection")
    parser.add_argument("--from-github-env", action="store_true", help="read the event from GitHub variables and write step outputs")
    parser.add_argument("--event", help="push, pull_request, schedule or workflow_dispatch, for a dry run")
    parser.add_argument("--schedule", default="", help="the triggering cron, for a scheduled dry run")
    parser.add_argument("--legs", default="", help="the manual run option, for a dispatch dry run")
    parser.add_argument("--labels", default="", help="pull request labels as a JSON list, for a dry run")
    parser.add_argument("--branch", default="", help="the pull request branch, so a milestone branch builds without a label")
    parser.add_argument("--before", default="", help="the range start, for a push or pull request dry run")
    parser.add_argument("--after", default="", help="the range end, for a push or pull request dry run")
    parser.add_argument("--now", default="", help="the UTC time of the run, for a dry run")
    parser.add_argument("--last-built", default="", help="a JSON object of leg to the commit it last built, for a scheduled dry run without the Actions API")
    args = parser.parse_args(argv)

    environment = os.environ if environment is None else environment
    if args.from_github_env:
        event = environment.get("EVENT_NAME", "")
        before = environment.get("PR_BASE") if event == "pull_request" else environment.get("PUSH_BEFORE")
        after = environment.get("PR_HEAD") if event == "pull_request" else environment.get("PUSH_AFTER")
        inputs = {"schedule": environment.get("SCHEDULE", ""), "legs": environment.get("DISPATCH_LEGS", ""), "labels": environment.get("PR_LABELS", ""), "branch": environment.get("PR_BRANCH", ""), "before": before, "after": after}
    elif args.event:
        event = args.event
        inputs = {"schedule": args.schedule, "legs": args.legs, "labels": args.labels, "branch": args.branch, "before": args.before, "after": args.after}
    else:
        parser.error("pass --from-github-env or --event")
    now = parse_now(args.now) if args.now else datetime.datetime.now(UTC)
    if args.last_built:
        try:
            actions = KnownBuilds(json.loads(args.last_built))
        except json.JSONDecodeError as error:
            parser.error(f"--last-built is not JSON: {error}")
    else:
        actions = Actions(environment)

    try:
        result = plan(event, inputs, now, git or Git(), actions)
    except SelectionError as error:
        print(f"select legs: {error}", file=sys.stderr)
        return 2
    for warning in result["warnings"]:
        print(f"select legs: warning: {warning}")
    for reason in result["reasons"]:
        print(reason)
    for line in output_lines(result):
        print(line)
    if args.from_github_env:
        write_github_files(environment, event, result)
    return 0


if __name__ == "__main__":
    sys.exit(main())

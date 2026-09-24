# Project Ambrose by Imjustchico
# Self-tests for the forbidden file scan, including the key, store, log and token material it keeps out, the contributor track path check, the commit trailer check, the build stages, the vcpkg cache key, the usage count, and the build leg selection against fakes, a real git repository and a fake Actions API, and the milestone track, where a branch named for a milestone is allowed the source tree and the track's open list is held to the milestones that are really ready.
import argparse
import datetime
import json
import os
import io
import re
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "progress"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "site"))

import ci_build
import ci_commit_trailer
import ci_contrib_paths
import ci_findings
import ci_forbidden_files
import ci_roadmap_state
import ci_select_legs
import ci_triage
import ci_usage
import ci_vcpkg_cache
import build as board
import ready as ready_report

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
UTC = datetime.timezone.utc


class ForbiddenFileTests(unittest.TestCase):
    def assertForbidden(self, path, raw, needle):
        problems = ci_forbidden_files.check_file(path, raw)
        self.assertTrue(any(needle in problem for problem in problems), f"expected '{needle}', got {problems}")

    def assertAllowed(self, path, raw):
        self.assertEqual(ci_forbidden_files.check_file(path, raw), [])

    def test_kiwad_content_is_forbidden_under_any_name(self):
        self.assertForbidden("apps/ci/sample.json", b"KIWAD\x02\x00\x00\x00", "KIWAD")

    def test_bind_content_is_forbidden(self):
        self.assertForbidden("data/sample.bin", b"BINd\x07\x00\x00\x00", "BINd")

    def test_client_and_capture_extensions_are_forbidden(self):
        self.assertForbidden("data/Root.wad", b"", "client archive")
        self.assertForbidden("captures/session.pcapng", b"", "packet capture")
        self.assertForbidden("art/model.nif", b"", "client model")

    def test_protocol_definition_xml_is_forbidden(self):
        xml = b'<?xml version="1.0" ?>\n<LoginMessages>\n  <_ProtocolInfo>\n    <RECORD>\n'
        self.assertForbidden("src/server/shared/Messages/LoginMessages.xml", xml, "protocol definition")

    def test_type_dump_json_is_forbidden(self):
        self.assertForbidden("data/dump.json", b'{"version": 1, "classes": {}}', "type dump")

    def test_secret_material_is_forbidden_by_name(self):
        self.assertForbidden("build/panel.key", b"", "private key")
        self.assertForbidden("certs/panel.pem", b"", "private key")
        self.assertForbidden("data/panel.sqlite3", b"", "store holding operators")
        self.assertForbidden("logs/Supervisor.log", b"", "log")
        self.assertForbidden("run/admin.token", b"", "token")
        self.assertForbidden("run/panel.secret", b"", "secret")

    def test_private_key_content_is_forbidden_under_any_name(self):
        opening = b"-----BEGIN " + b"PRIVATE KEY-----"
        self.assertForbidden("doc/notes.txt", opening + b"\nMIIsomething\n", "holds a private key")
        self.assertForbidden("README.md", b"-----BEGIN RSA " + b"PRIVATE KEY-----\n", "holds a private key")

    def test_sqlite_store_content_is_forbidden_under_any_name(self):
        self.assertForbidden("data/notes.dat", b"SQLite format 3" + bytes([0]) + b"rest", "SQLite store")
        self.assertAllowed("doc/PANEL.md", b"The panel keeps a SQLite format 3 store in the data folder.\n")

    def test_local_config_is_forbidden_but_template_is_allowed(self):
        self.assertForbidden("conf/gameserver.conf", b"# Project Ambrose by Imjustchico\n", "local config")
        self.assertAllowed("conf/dist/gameserver.conf.dist", b"# Project Ambrose by Imjustchico\n# Game server settings.\n")

    def test_oversized_file_is_forbidden_outside_deps(self):
        big = b"x" * (ci_forbidden_files.MAX_BYTES + 1)
        self.assertForbidden("doc/big.md", big, "byte limit")
        self.assertAllowed("deps/ports/sample/big.txt", big)

    def test_documentation_mentioning_formats_is_allowed(self):
        self.assertAllowed("doc/CLIENT.md", b"<!-- Project Ambrose by Imjustchico: Notes. -->\nKIWAD archives and <_ProtocolInfo> blocks.\n")
        self.assertAllowed("vcpkg.json", b'{"name": "project-ambrose", "version-string": "0.0.0"}')


class CommitTrailerTests(unittest.TestCase):
    def test_trailer_is_detected(self):
        self.assertTrue(ci_commit_trailer.has_trailer("Add thing\n\nBody text.\n\nCo-Authored-By: Claude Opus 5 <noreply@anthropic.com>\n"))

    def test_trailer_key_is_case_insensitive(self):
        self.assertTrue(ci_commit_trailer.has_trailer("Add thing\n\nco-authored-by: Some Model <bot@example.com>\n"))

    def test_missing_trailer_is_rejected(self):
        self.assertFalse(ci_commit_trailer.has_trailer("Add thing\n\nWritten by hand.\n"))

    def test_trailer_without_email_is_rejected(self):
        self.assertFalse(ci_commit_trailer.has_trailer("Add thing\n\nCo-Authored-By: Claude\n"))

    def test_a_bot_authored_commit_needs_no_trailer(self):
        self.assertTrue(ci_commit_trailer.written_by_a_bot("dependabot[bot]"))
        self.assertTrue(ci_commit_trailer.written_by_a_bot("github-actions[bot] "))
        self.assertFalse(ci_commit_trailer.written_by_a_bot("Justchicoo"))
        self.assertFalse(ci_commit_trailer.written_by_a_bot("A Person [bot] who is not"))
        self.assertFalse(ci_commit_trailer.has_trailer("Front end: Bump the front-end group with 5 updates"))

    def test_range_for_pull_request(self):
        environment = {"EVENT_NAME": "pull_request", "PR_BASE": "aaa", "PR_HEAD": "bbb"}
        self.assertEqual(ci_commit_trailer.range_from_github_environment(environment), ["aaa..bbb"])

    def test_range_for_push_and_first_push(self):
        push = {"EVENT_NAME": "push", "PUSH_BEFORE": "aaa", "PUSH_AFTER": "bbb"}
        self.assertEqual(ci_commit_trailer.range_from_github_environment(push), ["aaa..bbb"])
        first = {"EVENT_NAME": "push", "PUSH_BEFORE": "0" * 40, "PUSH_AFTER": "bbb"}
        self.assertEqual(ci_commit_trailer.range_from_github_environment(first), ["-n", "1", "bbb"])

    def test_range_for_manual_run(self):
        self.assertEqual(ci_commit_trailer.range_from_github_environment({"EVENT_NAME": "workflow_dispatch", "PUSH_AFTER": "ccc"}), ["-n", "1", "ccc"])


class FakeGit:
    def __init__(self, paths=None, changed=None):
        self.paths = paths
        self.changed = changed or {}
        self.asked = []
        self.merge_base = None

    def changed_paths(self, before, after, merge_base=False):
        self.merge_base = merge_base
        return self.paths

    def code_changed_since(self, sha):
        self.asked.append(sha)
        return self.changed.get(sha, False)


class FailingActions:
    def last_built(self, legs):
        raise ci_select_legs.ActionsError("no token")


def at(text):
    return datetime.datetime.fromisoformat(text.replace("Z", "+00:00"))


class SelectLegsTests(unittest.TestCase):
    def test_manual_runs_expand_their_option(self):
        git = FakeGit()
        self.assertEqual(ci_select_legs.plan("workflow_dispatch", {"legs": "all"}, at("2026-10-01T00:00:00Z"), git)["legs"], list(ci_select_legs.LEGS))
        self.assertEqual(ci_select_legs.plan("workflow_dispatch", {"legs": "weekly"}, at("2026-10-01T00:00:00Z"), git)["legs"], ["windows-msvc-x64", "linux-gcc-asan", "linux-clang-tsan"])
        self.assertEqual(ci_select_legs.plan("workflow_dispatch", {"legs": "none"}, at("2026-10-01T00:00:00Z"), git)["legs"], [])
        self.assertEqual(ci_select_legs.plan("workflow_dispatch", {"legs": ""}, at("2026-10-01T00:00:00Z"), git)["legs"], ["linux-gcc"])
        with self.assertRaises(ci_select_legs.SelectionError):
            ci_select_legs.plan("workflow_dispatch", {"legs": "everything"}, at("2026-10-01T00:00:00Z"), git)

    def test_pushes_build_linux_gcc_only_for_build_inputs(self):
        now = at("2026-10-01T00:00:00Z")
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit([".github/workflows/core-build.yml"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["vcpkg.json", "doc/ROADMAP.md"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["apps/ci/ci_select_legs.py"]))["legs"], [])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(None))["legs"], ["linux-gcc"])

    def test_pull_requests_build_labeled_legs_from_the_merge_base(self):
        now = at("2026-10-01T00:00:00Z")
        git = FakeGit(["src/main.cpp"])
        labeled = ci_select_legs.plan("pull_request", {"labels": '["ci:weekly", "bug", "ci:nonsense"]'}, now, git)
        self.assertEqual(labeled["legs"], ["windows-msvc-x64", "linux-gcc-asan", "linux-clang-tsan"])
        self.assertEqual(len(labeled["warnings"]), 1)
        self.assertTrue(git.merge_base)
        self.assertEqual(ci_select_legs.plan("pull_request", {"labels": '["CI:Linux-Clang", "Ci: weekly"]'}, now, FakeGit(["src/main.cpp"]))["legs"], ["windows-msvc-x64", "linux-clang", "linux-gcc-asan", "linux-clang-tsan"])
        self.assertEqual(ci_select_legs.plan("pull_request", {"labels": "null"}, now, FakeGit(["src/main.cpp"]))["legs"], [])
        self.assertEqual(ci_select_legs.plan("pull_request", {"labels": "[]"}, now, FakeGit(["apps/ci/ci_build.py"]))["legs"], ["linux-gcc"])

    def test_a_milestone_branch_builds_without_waiting_for_a_label(self):
        now = at("2026-10-01T00:00:00Z")
        milestone = ci_select_legs.plan("pull_request", {"labels": "[]", "branch": "milestone/4.04-world-wire-math"}, now, FakeGit(["src/main.cpp"]))
        self.assertEqual(milestone["legs"], ["linux-gcc"])
        self.assertEqual(milestone["warnings"], [])
        self.assertEqual(ci_select_legs.plan("pull_request", {"labels": "[]", "branch": "contrib/C-60-watcher"}, now, FakeGit(["src/main.cpp"]))["legs"], [])
        self.assertEqual(ci_select_legs.plan("pull_request", {"labels": "[]"}, now, FakeGit(["src/main.cpp"]))["legs"], [])
        both = ci_select_legs.plan("pull_request", {"labels": '["ci:windows-msvc-x64"]', "branch": "milestone/16.01-file-binary"}, now, FakeGit(["src/main.cpp"]))
        self.assertEqual(both["legs"], ["windows-msvc-x64", "linux-gcc"])

    def test_slot_dates_follow_the_cron_weekday(self):
        self.assertEqual(ci_select_legs.slot_date(ci_select_legs.SUNDAY_CRON, at("2026-10-05T01:00:00Z")), datetime.date(2026, 10, 4))
        self.assertEqual(ci_select_legs.slot_date(ci_select_legs.SUNDAY_CRON, at("2026-10-04T06:17:00Z")), datetime.date(2026, 10, 4))
        self.assertEqual(ci_select_legs.slot_date(ci_select_legs.WEDNESDAY_CRON, at("2026-10-07T06:18:00Z")), datetime.date(2026, 10, 7))
        with self.assertRaises(ci_select_legs.SelectionError):
            ci_select_legs.slot_date("0 0 * * *", at("2026-10-07T06:18:00Z"))

    def test_the_calendar_of_legs(self):
        self.assertEqual(ci_select_legs.scheduled_legs(datetime.date(2026, 10, 4)), list(ci_select_legs.LEGS))
        self.assertEqual(ci_select_legs.scheduled_legs(datetime.date(2026, 10, 11)), ["windows-msvc-x64", "linux-gcc-asan", "linux-clang-tsan"])
        self.assertEqual(ci_select_legs.scheduled_legs(datetime.date(2026, 10, 7)), ["windows-msvc-x64"])
        self.assertEqual(ci_select_legs.scheduled_legs(datetime.date(2026, 10, 8)), [])

    def test_scheduled_legs_build_only_after_a_code_change_since_their_last_build(self):
        sunday = ci_select_legs.SUNDAY_CRON
        built = ci_select_legs.KnownBuilds({"windows-msvc-x64": "a" * 40, "linux-gcc-asan": "b" * 40, "linux-clang-tsan": "a" * 40, "linux-gcc": "a" * 40})
        idle = ci_select_legs.plan("schedule", {"schedule": sunday}, at("2026-10-11T06:20:00Z"), FakeGit(), built)
        self.assertEqual(idle["legs"], [])
        self.assertTrue(idle["windows_keepalive"])
        changed = FakeGit(changed={"b" * 40: True})
        partial = ci_select_legs.plan("schedule", {"schedule": sunday}, at("2026-10-11T06:20:00Z"), changed, built)
        self.assertEqual(partial["legs"], ["linux-gcc-asan"])
        self.assertTrue(partial["windows_keepalive"])
        self.assertEqual(sorted(changed.asked), sorted(["a" * 40, "b" * 40, "a" * 40]))
        first = ci_select_legs.plan("schedule", {"schedule": sunday}, at("2026-10-04T06:20:00Z"), FakeGit(), built)
        self.assertEqual(first["legs"], ["linux-clang", "linux-clang-fuzz"])
        other = ci_select_legs.plan("schedule", {"schedule": ci_select_legs.OTHER_CRON}, at("2026-10-12T06:20:00Z"), FakeGit(), FailingActions())
        self.assertEqual(other["legs"], [])
        self.assertEqual(other["warnings"], [])
        self.assertFalse(other["windows_keepalive"])
        with self.assertRaises(ci_select_legs.SelectionError):
            ci_select_legs.plan("schedule", {"schedule": "5 4 * * *"}, at("2026-10-07T06:20:00Z"), FakeGit(), built)

    def test_unreadable_earlier_builds_build_every_leg_of_the_slot(self):
        wednesday = ci_select_legs.plan("schedule", {"schedule": ci_select_legs.WEDNESDAY_CRON}, at("2026-10-07T06:20:00Z"), FakeGit(), FailingActions())
        self.assertEqual(wednesday["legs"], ["windows-msvc-x64"])
        self.assertFalse(wednesday["windows_keepalive"])
        self.assertEqual(len(wednesday["warnings"]), 1)
        self.assertIn("no token", wednesday["warnings"][0])

    def test_now_is_read_as_utc(self):
        self.assertEqual(ci_select_legs.parse_now("2026-10-04T08:17:00+02:00"), at("2026-10-04T06:17:00Z"))
        self.assertEqual(ci_select_legs.parse_now("2026-10-04T06:17:00").tzinfo, UTC)
        self.assertEqual(ci_select_legs.parse_now("2026-10-04T06:17:00Z").utcoffset(), datetime.timedelta(0))

    def test_matrix_json_is_compact_with_job_timeouts(self):
        text = ci_select_legs.matrix_json(["windows-msvc-x64", "linux-gcc"])
        self.assertNotIn(" ", text)
        matrix = json.loads(text)
        self.assertEqual([entry["configure"] for entry in matrix["include"]], ["windows-msvc-x64", "linux-gcc"])
        for entry in matrix["include"]:
            self.assertEqual(entry["job_timeout"], entry["configure_timeout"] + entry["build_timeout"] + ci_select_legs.JOB_TIMEOUT_MARGIN)
        lines = ci_select_legs.output_lines({"legs": [], "windows_keepalive": True})
        self.assertEqual(lines, ["build=false", 'matrix={"include":[]}', "windows_keepalive=true"])

    def test_github_runs_write_outputs_and_a_summary(self):
        with tempfile.TemporaryDirectory() as folder:
            output = os.path.join(folder, "output")
            summary = os.path.join(folder, "summary")
            environment = {"EVENT_NAME": "workflow_dispatch", "DISPATCH_LEGS": "linux", "GITHUB_OUTPUT": output, "GITHUB_STEP_SUMMARY": summary}
            with mock.patch("sys.stdout", new=io.StringIO()) as printed:
                self.assertEqual(ci_select_legs.main(["--from-github-env"], environment=environment, git=FakeGit()), 0)
            with open(output, encoding="utf-8") as handle:
                written = handle.read().splitlines()
            self.assertEqual(written[0], "build=true")
            self.assertEqual([entry["configure"] for entry in json.loads(written[1][len("matrix="):])["include"]], ci_select_legs.SETS["linux"])
            self.assertEqual(written[2], "windows_keepalive=false")
            with open(summary, encoding="utf-8") as handle:
                self.assertTrue(handle.read().startswith("### Build legs for workflow_dispatch\n\n- linux-gcc: selected"))
            self.assertIn("build=true", printed.getvalue())
            dry = {"GITHUB_OUTPUT": os.path.join(folder, "unused")}
            with mock.patch("sys.stdout", new=io.StringIO()):
                self.assertEqual(ci_select_legs.main(["--event", "schedule", "--schedule", ci_select_legs.WEDNESDAY_CRON, "--now", "2026-10-07T06:20:00Z", "--last-built", '{"windows-msvc-x64": "abc"}'], environment=dry, git=FakeGit()), 0)
            self.assertFalse(os.path.exists(dry["GITHUB_OUTPUT"]))
            with mock.patch("sys.stderr", new=io.StringIO()) as errors:
                self.assertEqual(ci_select_legs.main(["--event", "schedule", "--schedule", "1 2 3 4 5"], environment={}, git=FakeGit()), 2)
            self.assertIn("unknown schedule", errors.getvalue())


class RealGitTests(unittest.TestCase):
    def git(self, *args):
        environment = dict(os.environ, GIT_AUTHOR_NAME="Ambrose Test", GIT_AUTHOR_EMAIL="test@example.com", GIT_COMMITTER_NAME="Ambrose Test", GIT_COMMITTER_EMAIL="test@example.com", GIT_CONFIG_NOSYSTEM="1")
        result = subprocess.run(["git", "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false", *args], cwd=self.folder, env=environment, capture_output=True, text=True, check=True)
        return result.stdout.strip()

    def commit(self, path, text):
        full = os.path.join(self.folder, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        self.git("add", "--", path)
        self.git("commit", "-q", "-m", f"change {path}")
        return self.git("rev-parse", "HEAD")

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.folder = self.directory.name
        self.git("init", "-q")
        self.git("checkout", "-q", "-b", "main")
        self.base = self.commit("src/main.cpp", "int main() {}\n")
        self.repository = ci_select_legs.Git(self.folder)

    def tearDown(self):
        self.directory.cleanup()

    def test_changed_paths_use_two_dots_for_pushes_and_the_merge_base_for_pull_requests(self):
        self.git("checkout", "-q", "-b", "topic")
        topic = self.commit(".github/workflows/core-build.yml", "name: core-build\n")
        self.git("checkout", "-q", "main")
        main = self.commit("apps/ci/ci_build.py", "print()\n")
        self.assertEqual(self.repository.changed_paths(self.base, main), ["apps/ci/ci_build.py"])
        self.assertEqual(sorted(self.repository.changed_paths(main, topic)), [".github/workflows/core-build.yml", "apps/ci/ci_build.py"])
        self.assertEqual(self.repository.changed_paths(main, topic, merge_base=True), [".github/workflows/core-build.yml"])
        self.assertIsNone(self.repository.changed_paths("0" * 40, main))
        self.assertIsNone(self.repository.changed_paths("", main))
        self.assertIsNone(self.repository.changed_paths("f" * 40, main))

    def test_code_changes_ignore_documentation(self):
        self.commit("doc/ROADMAP.md", "roadmap\n")
        docs = self.commit("src/README.md", "notes\n")
        self.assertFalse(self.repository.code_changed_since(self.base))
        self.commit("src/main.cpp", "int main() { return 0; }\n")
        self.assertTrue(self.repository.code_changed_since(self.base))
        self.assertTrue(self.repository.code_changed_since(docs))
        self.assertTrue(self.repository.code_changed_since("f" * 40))
        self.assertTrue(self.repository.code_changed_since(""))


class FakeResponse(io.BytesIO):
    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.close()


class ActionsTests(unittest.TestCase):
    ENVIRONMENT = {"GITHUB_REPOSITORY": "owner/repo", "GITHUB_TOKEN": "token", "GITHUB_REF_NAME": "main", "GITHUB_API_URL": "https://api.example"}

    def opener(self, pages):
        requests = []

        def open_url(request, timeout):
            requests.append(request)
            path = request.full_url.split("?")[0]
            if path not in pages:
                raise urllib.error.HTTPError(request.full_url, 404, "Not Found", {}, None)
            return FakeResponse(json.dumps(pages[path]).encode("utf-8"))
        return open_url, requests

    def test_the_newest_finished_build_of_each_leg_is_found(self):
        base = "https://api.example/repos/owner/repo/actions"
        pages = {
            f"{base}/workflows/core-build.yml/runs": {"workflow_runs": [
                {"id": 5, "event": "pull_request", "head_sha": "pr"},
                {"id": 4, "event": "schedule", "head_sha": "four"},
                {"id": 3, "event": "push", "head_sha": "three"},
                {"id": 2, "event": "schedule", "head_sha": "two"},
                {"id": 1, "event": "schedule", "head_sha": "one"},
            ]},
            f"{base}/runs/4/jobs": {"jobs": [{"name": "checks", "conclusion": "success"}, {"name": "build (windows-msvc-x64)", "conclusion": "cancelled"}, {"name": "build (linux-gcc-asan)", "conclusion": "skipped"}]},
            f"{base}/runs/3/jobs": {"jobs": [{"name": "build (linux-gcc)", "conclusion": "success"}]},
            f"{base}/runs/2/jobs": {"jobs": [{"name": "build (windows-msvc-x64)", "conclusion": "failure"}]},
            f"{base}/runs/1/jobs": {"jobs": [{"name": "build (windows-msvc-x64)", "conclusion": "success"}, {"name": "build (linux-gcc-asan)", "conclusion": "success"}]},
        }
        open_url, requests = self.opener(pages)
        actions = ci_select_legs.Actions(self.ENVIRONMENT, open_url)
        self.assertEqual(actions.last_built(["windows-msvc-x64", "linux-gcc-asan"]), {"windows-msvc-x64": "two", "linux-gcc-asan": "one"})
        self.assertIn("branch=main", requests[0].full_url)
        self.assertIn("status=completed", requests[0].full_url)
        self.assertEqual(requests[0].get_header("Authorization"), "Bearer token")
        self.assertNotIn(f"{base}/runs/5/jobs", [request.full_url.split("?")[0] for request in requests])
        open_url, requests = self.opener(pages)
        self.assertEqual(ci_select_legs.Actions(self.ENVIRONMENT, open_url).last_built(["linux-gcc"]), {"linux-gcc": "three"})
        self.assertEqual(len(requests), 3)

    def test_api_failures_are_reported(self):
        open_url, _ = self.opener({})
        with self.assertRaises(ci_select_legs.ActionsError):
            ci_select_legs.Actions(self.ENVIRONMENT, open_url).last_built(["linux-gcc"])
        with self.assertRaises(ci_select_legs.ActionsError):
            ci_select_legs.Actions({"GITHUB_REPOSITORY": "owner/repo"}, open_url).last_built(["linux-gcc"])
        listing = "https://api.example/repos/owner/repo/actions/workflows/core-build.yml/runs"
        open_url, _ = self.opener({listing: ["not", "an", "object"]})
        with self.assertRaises(ci_select_legs.ActionsError):
            ci_select_legs.Actions(self.ENVIRONMENT, open_url).last_built(["linux-gcc"])


class WorkflowDriftTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(os.path.join(ROOT, ".github", "workflows", "core-build.yml"), encoding="utf-8") as handle:
            cls.workflow = handle.read()
        with open(os.path.join(ROOT, "CMakePresets.json"), encoding="utf-8") as handle:
            cls.presets = json.load(handle)

    def list_after(self, key):
        match = re.search(r"^\s*" + re.escape(key) + r":\s*\[([^\]]*)\]", self.workflow, re.MULTILINE)
        self.assertIsNotNone(match, key)
        return [item.strip().strip('"') for item in match.group(1).split(",")]

    def test_crons_options_and_paths_match_the_selector(self):
        for cron in (ci_select_legs.SUNDAY_CRON, ci_select_legs.WEDNESDAY_CRON, ci_select_legs.OTHER_CRON):
            self.assertIn(f'- cron: "{cron}"', self.workflow)
        self.assertEqual(tuple(self.list_after("options")), ci_select_legs.OPTIONS)
        paths = tuple(self.list_after("paths"))
        self.assertEqual(paths, ci_select_legs.PUSH_PATHS)
        for smoke in ci_select_legs.SMOKE_PATHS:
            self.assertTrue(any(smoke.startswith(path[:-2]) if path.endswith("/**") else smoke == path for path in paths), smoke)
        self.assertIn(f"github.event.schedule == '{ci_select_legs.SUNDAY_CRON}'", self.workflow)
        self.assertIn(f"github.event.schedule == '{ci_select_legs.WEDNESDAY_CRON}'", self.workflow)
        self.assertTrue(os.path.isfile(os.path.join(ROOT, ".github", "workflows", ci_select_legs.WORKFLOW_FILE)))
        self.assertIn("actions: read", self.workflow)
        self.assertIn("GITHUB_TOKEN: ${{ github.token }}", self.workflow)
        self.assertIn("name: build (${{ matrix.configure }})", self.workflow)
        self.assertIn("--stage build-test", self.workflow)
        self.assertEqual(self.workflow.count("--setup-vcpkg"), 1)

    def test_every_leg_names_existing_presets(self):
        configure = {preset["name"]: preset for preset in self.presets["configurePresets"]}
        build = {preset["name"]: preset for preset in self.presets["buildPresets"]}
        tests = {preset["name"]: preset for preset in self.presets["testPresets"]}
        self.assertEqual(configure["base"]["binaryDir"], "${sourceDir}/build/${presetName}")
        for leg, settings in ci_select_legs.LEGS.items():
            self.assertIn(settings["configure"], configure, leg)
            self.assertEqual(build[settings["build"]]["configurePreset"], settings["configure"], leg)
            self.assertIn(settings["build"], tests, leg)


class VcpkgCacheTests(unittest.TestCase):
    STATUS = "Package: fmt\nAbi: " + "a" * 64 + "\nStatus: install ok installed\n\nPackage: old\nAbi: " + "b" * 64 + "\nStatus: purge ok not-installed\n\nPackage: bare\nStatus: install ok installed\n"

    def write_archive(self, folder, abi, age_days):
        directory = os.path.join(folder, abi[:2])
        os.makedirs(directory, exist_ok=True)
        path = os.path.join(directory, abi + ".zip")
        with open(path, "wb") as handle:
            handle.write(b"zip")
        stamp = time.time() - age_days * 86400
        os.utime(path, (stamp, stamp))
        return f"{abi[:2]}/{abi}.zip"

    def test_installed_abis_skip_removed_and_bare_packages(self):
        self.assertEqual(ci_vcpkg_cache.installed_abis(self.STATUS), {"a" * 64})
        self.assertEqual(ci_vcpkg_cache.installed_abis(self.STATUS.replace("\n", "\r\n")), {"a" * 64})

    def test_prune_removes_only_old_archives_this_build_does_not_use(self):
        with tempfile.TemporaryDirectory() as folder:
            current = self.write_archive(folder, "a" * 64, 90)
            young = self.write_archive(folder, "c" * 64, 5)
            stale = self.write_archive(folder, "d" * 64, 45)
            keep = {"a" * 64}
            self.assertEqual(ci_vcpkg_cache.prune(folder, keep, time.time(), dry_run=True), [stale])
            self.assertEqual(ci_vcpkg_cache.archives(folder), sorted([current, young, stale]))
            self.assertEqual(ci_vcpkg_cache.prune(folder, set(), time.time()), [])
            self.assertEqual(ci_vcpkg_cache.prune(folder, keep, time.time()), [stale])
            self.assertEqual(ci_vcpkg_cache.archives(folder), sorted([current, young]))

    def test_the_key_names_the_archive_set(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            self.assertIsNone(ci_vcpkg_cache.content_key(first, "vcpkg-Linux-"))
            self.write_archive(first, "a" * 64, 1)
            self.write_archive(first, "c" * 64, 1)
            self.write_archive(second, "c" * 64, 1)
            self.write_archive(second, "a" * 64, 1)
            key = ci_vcpkg_cache.content_key(first, "vcpkg-Linux-")
            self.assertEqual(key, ci_vcpkg_cache.content_key(second, "vcpkg-Linux-"))
            self.assertTrue(key.startswith("vcpkg-Linux-"))
            self.assertEqual(len(key), len("vcpkg-Linux-") + ci_vcpkg_cache.KEY_HEX)
            self.write_archive(first, "e" * 64, 1)
            self.assertNotEqual(ci_vcpkg_cache.content_key(first, "vcpkg-Linux-"), key)


class BuildStageTests(unittest.TestCase):
    def arguments(self, stage):
        return argparse.Namespace(configure_preset="linux-gcc", build_preset="linux-gcc-debug", test_preset=None, warnings_as_errors=True, stage=stage)

    def test_each_stage_runs_its_commands(self):
        configure = [["cmake", "--version"], ["cmake", "--preset", "linux-gcc", "-DAMBROSE_WARNINGS_AS_ERRORS=ON"]]
        build = [["cmake", "--build", "--preset", "linux-gcc-debug"], ["ctest", "--preset", "linux-gcc-debug"]]
        self.assertEqual(ci_build.commands(self.arguments("all")), configure + build)
        self.assertEqual(ci_build.commands(self.arguments("configure")), configure)
        self.assertEqual(ci_build.commands(self.arguments("build-test")), build)

    def test_main_runs_the_chosen_stage(self):
        ran = []
        with mock.patch.dict(os.environ, {"VCPKG_ROOT": "vcpkg"}), mock.patch.object(ci_build, "run", side_effect=lambda command, **_: ran.append(command)):
            self.assertEqual(ci_build.main(["--configure-preset", "linux-gcc", "--build-preset", "linux-gcc-debug", "--stage", "build-test"]), 0)
        self.assertEqual(ran, [["cmake", "--build", "--preset", "linux-gcc-debug"], ["ctest", "--preset", "linux-gcc-debug"]])


class TrailerScheduleTests(unittest.TestCase):
    def test_scheduled_runs_check_the_last_eight_days(self):
        environment = {"EVENT_NAME": "schedule", "PUSH_AFTER": ""}
        self.assertEqual(ci_commit_trailer.range_from_github_environment(environment, at("2026-10-01T06:17:00Z")), ["--since=2026-09-23T06:17:00Z", "HEAD"])


class UsageTests(unittest.TestCase):
    def test_minutes_round_up_and_windows_counts_twice(self):
        now = at("2026-10-10T00:00:00Z")
        linux = {"name": "build (linux-gcc)", "labels": ["ubuntu-latest"], "runner_name": "GitHub Actions 1", "started_at": "2026-10-02T00:00:00Z", "completed_at": "2026-10-02T00:08:01Z"}
        windows = {"name": "build (windows-msvc-x64)", "labels": ["windows-latest"], "runner_name": "GitHub Actions 2", "started_at": "2026-10-02T00:00:00Z", "completed_at": "2026-10-02T00:16:30Z"}
        skipped = {"name": "windows-cache", "labels": ["windows-latest"], "runner_name": "", "started_at": "2026-10-02T00:00:00Z", "completed_at": "2026-10-02T00:00:00Z"}
        september = dict(linux, started_at="2026-09-30T23:00:00Z", completed_at="2026-09-30T23:30:00Z")
        self.assertEqual(ci_usage.billed_minutes(linux, now), 9)
        self.assertEqual(ci_usage.billed_minutes(windows, now), 34)
        self.assertEqual(ci_usage.billed_minutes(skipped, now), 0)
        by_event, by_job = ci_usage.summarize([{"event": "schedule", "jobs": [linux, windows, skipped, september]}], now)
        self.assertEqual(by_event["schedule"], 43)
        self.assertEqual(by_job["build (linux-gcc)"], 9)



class ContributorPathTests(unittest.TestCase):
    def test_the_track_folders_are_allowed(self):
        paths = [
            "contrib/tools/waddiff/main.py",
            "contrib/notes/realms.md",
            "contrib/proposals/panel-search.md",
            "contrib/findings/protocol/keepalive-body.json",
            "apps/clientdriver/scenarios/ban.json",
            "data/sql/updates/pending_db_world/2026_09_18_00.sql",
            "data/fuzz/blob-seeds/one.bin",
            "doc/guides/arch-linux.md",
            "contrib/locale/de.json",
        ]
        self.assertEqual(ci_contrib_paths.check(paths), [])

    def test_everything_else_is_reported(self):
        paths = [
            "src/server/shared/Network/SessionBase.cpp",
            "doc/ROADMAP.md",
            "doc/roadmap/phase-03-create-list-and-delete-a-wizard.md",
            "doc/ARCHITECTURE.md",
            "vcpkg.json",
            "CMakeLists.txt",
            ".github/workflows/core-build.yml",
            "apps/clientdriver/clientdriver/engine.py",
            "data/sql/base/db_world/updates.sql",
            "data/sql/custom/db_world/2026_09_18_00.sql",
            "data/sql/updates/db_world/2026_09_18_00.sql",
        ]
        self.assertEqual(ci_contrib_paths.check(paths), paths)

    def test_only_the_named_contrib_folders_are_allowed(self):
        self.assertEqual(ci_contrib_paths.check(["contrib/notes/capture-corpus.md"]), [])
        invented = ["contrib/whatever/anything.md", "contrib/findings.json"]
        self.assertEqual(ci_contrib_paths.check(invented), invented)

    def test_the_tracks_own_signposts_are_out_of_reach(self):
        signposts = ["contrib/README.md", "contrib/AI-START-HERE.md"]
        self.assertEqual(ci_contrib_paths.check(signposts), signposts)

    def test_the_checker_and_the_tracks_table_name_the_same_folders(self):
        with io.open(os.path.join(ROOT, "doc", "CONTRIBUTOR-TRACK.md"), encoding="utf-8") as handle:
            rows = [line for line in handle.read().splitlines() if line.startswith("| `")]
        named = {row.split("`")[1].split("<")[0] for row in rows}
        self.assertEqual(named, set(ci_contrib_paths.ALLOWED_PREFIXES))

    def test_every_open_item_lands_in_a_folder_the_checker_allows(self):
        with io.open(os.path.join(ROOT, "doc", "CONTRIBUTOR-TRACK.md"), encoding="utf-8") as handle:
            rows = [line for line in handle.read().splitlines() if re.match(r"^\| [FC]-\d+ \|", line)]
        self.assertGreater(len(rows), 90)
        for row in rows:
            columns = row.split("|")
            self.assertEqual(len(columns), 6, row)
            for folder in re.findall(r"`([^`]*/[^`]*)`", columns[3]):
                self.assertEqual(ci_contrib_paths.check([folder + "a-file"]), [], columns[1].strip())

    def track_tables(self):
        with io.open(os.path.join(ROOT, "doc", "CONTRIBUTOR-TRACK.md"), encoding="utf-8") as handle:
            text = handle.read()
        marker = "### Merged so far"
        self.assertIn(marker, text)
        head, tail = text.split(marker, 1)
        opened = re.findall(r"^\| ([FC]-\d+) \|", head, re.M)
        merged = re.findall(r"^\| ([FC]-\d+) \|", tail, re.M)
        return opened, merged

    def test_a_merged_item_is_not_still_listed_as_open(self):
        opened, merged = self.track_tables()
        both = sorted(set(opened) & set(merged))
        self.assertEqual(both, [], "listed as open and as merged: " + ", ".join(both))

    def test_no_item_is_listed_twice(self):
        opened, merged = self.track_tables()
        for name, rows in (("open", opened), ("merged", merged)):
            repeated = sorted({row for row in rows if rows.count(row) > 1})
            self.assertEqual(repeated, [], name + " lists an item twice: " + ", ".join(repeated))

    def test_the_readme_counts_the_open_items(self):
        opened, _merged = self.track_tables()
        with io.open(os.path.join(ROOT, "README.md"), encoding="utf-8") as handle:
            readme = handle.read()
        for found in re.findall(r"open%20items-(\d+)-", readme) + re.findall(r"\*\*(\d+) open items\*\*", readme):
            self.assertEqual(int(found), len(opened), "README says " + found + " open items, the track lists " + str(len(opened)))

    def test_a_merged_item_names_something_that_exists(self):
        _opened, merged = self.track_tables()
        self.assertGreater(len(merged), 0)
        with io.open(os.path.join(ROOT, "doc", "CONTRIBUTOR-TRACK.md"), encoding="utf-8") as handle:
            tail = handle.read().split("### Merged so far", 1)[1]
        for row in [line for line in tail.splitlines() if re.match(r"^\| [FC]-\d+ \|", line)]:
            for path in re.findall(r"`([^`]+)`", row):
                self.assertTrue(os.path.exists(os.path.join(ROOT, path.replace("/", os.sep))), row)

    def test_a_folder_that_only_looks_like_the_track_is_reported(self):
        self.assertEqual(ci_contrib_paths.check(["contributors/tool.py", "docs/guides/x.md", "data/fuzzers/x.bin"]),
                         ["contributors/tool.py", "docs/guides/x.md", "data/fuzzers/x.bin"])

    def test_windows_separators_are_read_as_paths(self):
        windows = r"contrib\notes\realms.md"
        self.assertEqual(ci_contrib_paths.main.__module__, "ci_contrib_paths")
        self.assertEqual(ci_contrib_paths.check([windows.replace("\\", "/")]), [])
        self.assertEqual(ci_contrib_paths.check([windows]), [windows])



class RoadmapSummaryTests(unittest.TestCase):
    def git(self, *args):
        environment = dict(os.environ, GIT_AUTHOR_NAME="Ambrose Test", GIT_AUTHOR_EMAIL="test@example.com", GIT_COMMITTER_NAME="Ambrose Test", GIT_COMMITTER_EMAIL="test@example.com", GIT_CONFIG_NOSYSTEM="1")
        result = subprocess.run(["git", "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false", *args], cwd=self.folder, env=environment, capture_output=True, text=True, check=True)
        return result.stdout.strip()

    def write(self, path, text):
        full = os.path.join(self.folder, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        self.git("add", "--", path)

    def commit(self, message):
        self.git("commit", "-q", "-m", message)
        return self.git("rev-parse", "HEAD")

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.folder = self.directory.name
        self.git("init", "-q")
        self.git("checkout", "-q", "-b", "main")
        self.write("doc/roadmap/phase-04-a-wizard.md", "## 4.04 World wire math\n\n- [ ] Unit: packing lands within 4 units\n- [ ] Unit: yaw survives a round trip\n")
        self.write("doc/ROADMAP.md", "## Where we are\n\nPhase 3 is built.\n")
        self.base = self.commit("the phase file and the summary")

    def tearDown(self):
        self.directory.cleanup()

    def run_check(self, branch=""):
        return ci_roadmap_state.main(["--root", self.folder, "--range", f"{self.base}...HEAD", "--branch", branch])

    def test_ticking_a_check_without_saying_so_in_the_summary_is_refused(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", "## 4.04 World wire math\n\n- [x] Unit: packing lands within 4 units (MovementPackingTest.Packing)\n- [ ] Unit: yaw survives a round trip\n")
        self.commit("tick one check")
        self.assertEqual(self.run_check(), 1)

    def test_ticking_a_check_and_updating_the_summary_passes(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", "## 4.04 World wire math\n\n- [x] Unit: packing lands within 4 units (MovementPackingTest.Packing)\n- [ ] Unit: yaw survives a round trip\n")
        self.write("doc/ROADMAP.md", "## Where we are\n\nPhase 3 is built, and 4.04 packs positions.\n")
        self.commit("tick one check and say so")
        self.assertEqual(self.run_check(), 0)

    def test_a_milestone_branch_is_exempt_because_it_may_not_touch_the_roadmap(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", "## 4.04 World wire math\n\n- [x] Unit: packing lands within 4 units (MovementPackingTest.Packing)\n- [ ] Unit: yaw survives a round trip\n")
        self.commit("tick one check on a contributor's branch")
        self.assertEqual(self.run_check("milestone/4.04-world-wire-math"), 0)
        self.assertEqual(self.run_check("contrib/C-60-watcher"), 1)

    def test_a_change_that_ticks_nothing_is_not_asked_for_a_summary(self):
        self.write("src/main.cpp", "int main() {}\n")
        self.commit("code only")
        self.assertEqual(self.run_check(), 0)

    def test_unticking_a_check_is_not_mistaken_for_ticking_one(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", "## 4.04 World wire math\n\n- [ ] Unit: packing lands within 4 units\n")
        self.commit("drop a check")
        self.assertEqual(self.run_check(), 0)

    def test_a_range_that_cannot_be_diffed_checks_nothing(self):
        self.assertEqual(ci_roadmap_state.main(["--root", self.folder, "--range", "f" * 40 + "...HEAD"]), 0)

    def test_the_decision_is_made_on_what_changed(self):
        self.assertEqual(ci_roadmap_state.problems(["- [x] a"], ["doc/roadmap/phase-04-x.md"], ""),
                         ["1 acceptance check(s) were ticked, but doc/ROADMAP.md was not updated in the same change"])
        self.assertEqual(ci_roadmap_state.problems(["- [x] a"], ["doc/roadmap/phase-04-x.md", "doc/ROADMAP.md"], ""), [])
        self.assertEqual(ci_roadmap_state.problems([], ["doc/ROADMAP.md"], ""), [])
        self.assertEqual(ci_roadmap_state.problems(["- [x] a"], [], "milestone/4.04-x"), [])


class TriageTests(unittest.TestCase):
    def test_a_milestone_branch_is_labelled_by_its_name(self):
        self.assertEqual(ci_triage.labels_for("milestone/4.04-world-wire-math", ["src/server/game/Movement/MovementPacking.cpp"]), ["milestone-track"])
        self.assertEqual(ci_triage.labels_for("milestone/16.01", []), ["milestone-track"])

    def test_a_change_inside_the_contributor_track_is_labelled_by_its_paths(self):
        self.assertEqual(ci_triage.labels_for("my-branch", ["contrib/tools/watcher/README.md", "doc/guides/arch.md"]), ["contrib"])
        self.assertEqual(ci_triage.labels_for("my-branch", ["data/fuzz/seeds/one.bin"]), ["contrib"])

    def test_anything_mixed_or_unknown_is_left_for_a_person(self):
        self.assertEqual(ci_triage.labels_for("my-branch", ["contrib/notes/a.md", "src/main.cpp"]), [])
        self.assertEqual(ci_triage.labels_for("my-branch", ["src/main.cpp"]), [])
        self.assertEqual(ci_triage.labels_for("my-branch", []), [])
        self.assertEqual(ci_triage.labels_for("", ["README.md"]), [])

    def test_only_a_newcomer_on_a_known_track_is_greeted(self):
        self.assertIn("doc/MILESTONE-TRACK.md", ci_triage.welcome_for(["milestone-track"], "FIRST_TIME_CONTRIBUTOR"))
        self.assertIn("doc/CONTRIBUTOR-TRACK.md", ci_triage.welcome_for(["contrib"], "NONE"))
        self.assertIsNone(ci_triage.welcome_for(["milestone-track"], "CONTRIBUTOR"))
        self.assertIsNone(ci_triage.welcome_for(["milestone-track"], "MEMBER"))
        self.assertIsNone(ci_triage.welcome_for([], "FIRST_TIME_CONTRIBUTOR"))

    def test_the_greeting_says_what_decides_the_outcome(self):
        milestone = ci_triage.welcome_for(["milestone-track"], "NONE")
        self.assertIn("unticked", milestone)
        self.assertIn("discord.gg", milestone)
        self.assertIn("discord.gg", ci_triage.welcome_for(["contrib"], "NONE"))

    def test_it_does_nothing_without_a_pull_request_or_a_token(self):
        with mock.patch.dict(os.environ, {"GITHUB_TOKEN": ""}, clear=False):
            self.assertEqual(ci_triage.main(["--repository", "a/b", "--number", "0"]), 0)
            self.assertEqual(ci_triage.main(["--repository", "", "--number", "4"]), 0)
            self.assertEqual(ci_triage.main(["--repository", "a/b", "--number", "4", "--branch", "milestone/4.04-x"]), 0)

    def test_the_contributor_prefixes_are_ones_the_path_check_allows(self):
        for prefix in ci_triage.CONTRIB_PREFIXES:
            if prefix == "contrib/":
                continue
            self.assertEqual(ci_contrib_paths.check([prefix + "a-file"]), [], prefix)


class MilestoneTrackTests(unittest.TestCase):
    PHASE = "doc/roadmap/phase-04-a-wizard-stands-in-ravenwood.md"

    def track(self):
        with io.open(os.path.join(ROOT, "doc", "MILESTONE-TRACK.md"), encoding="utf-8") as handle:
            return handle.read()

    def section(self, name):
        return self.track().split("## " + name, 1)[1].split("\n## ", 1)[0]

    def test_a_branch_named_for_a_milestone_is_recognised(self):
        self.assertEqual(ci_contrib_paths.milestone_of("milestone/4.04-world-wire-math"), "4.04")
        self.assertEqual(ci_contrib_paths.milestone_of("milestone/17.106"), "17.106")
        self.assertEqual(ci_contrib_paths.milestone_of("refs/heads/milestone/1.6-lang"), "1.06")

    def test_any_other_branch_is_not_a_milestone_branch(self):
        for branch in ("contrib/C-60-watcher", "milestone/four", "milestones/4.04", "main", "", None):
            self.assertIsNone(ci_contrib_paths.milestone_of(branch), branch)

    def test_a_milestone_branch_may_change_the_source_tree(self):
        paths = [
            "src/server/game/Movement/MovementPacking.cpp",
            "src/test/server/game/Movement/MovementPackingTest.cpp",
            "src/server/game/CMakeLists.txt",
            "data/sql/updates/db_world/2026_09_22_00.sql",
            "src/server/apps/gameserver/gameserver.conf.dist",
            self.PHASE,
        ]
        self.assertEqual(ci_contrib_paths.check_milestone(paths, "4.04"), [])
        self.assertEqual(ci_contrib_paths.check(paths), paths)

    def test_a_milestone_branch_keeps_off_the_files_the_maintainer_holds(self):
        for path in ("README.md", "vcpkg.json", "doc/ROADMAP.md", "doc/MILESTONE-TRACK.md",
                     "contrib/AI-MILESTONES-HERE.md", ".github/workflows/core-build.yml",
                     "apps/ci/ci_contrib_paths.py", "apps/progress/progress.py", "doc/progress/progress.svg"):
            self.assertEqual([entry[0] for entry in ci_contrib_paths.check_milestone([path], "4.04")], [path], path)

    def test_a_milestone_branch_stays_inside_its_own_phase_file(self):
        other = "doc/roadmap/phase-05-the-zone-comes-alive-for-one-player.md"
        refused = ci_contrib_paths.check_milestone([self.PHASE, other], "4.04")
        self.assertEqual([entry[0] for entry in refused], [other])
        self.assertEqual(ci_contrib_paths.check_milestone([other], "5.01"), [])

    def test_a_branch_for_a_milestone_the_maintainer_holds_is_refused(self):
        kept = [{"scope": "phase:17", "who": "the panel session", "what": "the panel"},
                {"scope": "milestone:4.02", "who": "the panel session", "what": "CommandMgr"}]
        self.assertEqual(ci_contrib_paths.held_by("17.09", kept)["who"], "the panel session")
        self.assertEqual(ci_contrib_paths.held_by("4.02", kept)["who"], "the panel session")
        self.assertIsNone(ci_contrib_paths.held_by("4.04", kept))
        self.assertIsNone(ci_contrib_paths.held_by("1.06", []))

    def test_the_real_holds_stop_the_real_branches(self):
        kept = ci_contrib_paths.holds(ROOT)
        self.assertTrue(kept)
        self.assertEqual(ci_contrib_paths.main(["--root", ROOT, "--paths", "src/x.cpp", "--branch", "milestone/17.09-metrics"]), 1)
        self.assertEqual(ci_contrib_paths.main(["--root", ROOT, "--paths", "src/x.cpp", "--branch", "milestone/4.04-world-wire-math"]), 0)

    def test_the_checker_and_the_board_read_the_same_holds(self):
        from_checker = ci_contrib_paths.holds(ROOT)
        from_board = board.holds(ROOT)
        self.assertEqual(len(from_checker), len(from_board))
        for milestone in ("17.09", "4.02", "4.04", "1.06", "3.23"):
            checker = ci_contrib_paths.held_by(milestone, from_checker)
            board_side = board.hold_for(milestone, from_board)
            self.assertEqual(bool(checker), bool(board_side), milestone)

    def test_the_source_tree_needs_the_branch_name_to_be_allowed(self):
        source = ["src/server/game/Movement/MovementPacking.cpp"]
        self.assertEqual(ci_contrib_paths.main(["--paths"] + source), 1)
        self.assertEqual(ci_contrib_paths.main(["--paths"] + source + ["--branch", "milestone/4.04-world-wire-math"]), 0)
        self.assertEqual(ci_contrib_paths.main(["--paths", "README.md", "--branch", "milestone/4.04-x"]), 1)

    def test_both_documents_name_every_file_the_checker_holds_back(self):
        prompt = io.open(os.path.join(ROOT, "contrib", "AI-MILESTONES-HERE.md"), encoding="utf-8").read()
        track = self.track()
        for held in ci_contrib_paths.RESERVED_PREFIXES + ci_contrib_paths.RESERVED_FILES:
            self.assertIn("`" + held + "`", track, held)
            self.assertIn("`" + held + "`", prompt, held)

    def test_the_track_opens_only_milestones_that_exist_and_are_ready(self):
        ready, _blocked = ready_report.state(ROOT)
        everything = ready_report.milestones(ROOT)
        opened = [row["id"] for row in ready if row["status"] == "open"]
        self.assertGreater(len(opened), 0)
        for identifier in re.findall(r"^\| ([\d., ]+) \|", self.section("Open now"), re.M):
            for one in re.findall(r"\d+\.\d+", identifier):
                self.assertIn(one, everything, one + " is opened by the track but is not a milestone")
                self.assertTrue(one in opened or everything[one]["done"],
                                one + " is opened by the track but its dependencies are not built")

    def test_a_milestone_is_not_open_and_reserved_at_once(self):
        tables = {name: set(re.findall(r"\d+\.\d+", self.section(name)))
                  for name in ("Open now", "Reserved", "In flight", "Landed")}
        for name, other in (("Open now", "Reserved"), ("Open now", "In flight"), ("Reserved", "In flight")):
            both = sorted(tables[name] & tables[other])
            self.assertEqual(both, [], ", ".join(both) + " is listed under " + name + " and " + other)

    def test_a_milestone_the_track_does_not_name_is_reserved(self):
        ready, _blocked = ready_report.state(ROOT)
        everything = ready_report.milestones(ROOT)
        opened = {row["id"] for row in ready if row["status"] == "open"}
        named = set(re.findall(r"\d+\.\d+", self.section("Open now")))
        self.assertEqual(opened, {one for one in named if not everything[one]["done"]})
        for row in ready:
            self.assertIn(row["status"], ("open", "reserved", "claimed", "landed"), row["id"])

    def test_every_ready_milestone_names_a_phase_file_that_holds_it(self):
        ready, _blocked = ready_report.state(ROOT)
        for row in ready[:5]:
            text = io.open(os.path.join(ROOT, row["file"].replace("/", os.sep)), encoding="utf-8").read()
            self.assertIn("## " + row["id"] + " ", text)
            self.assertFalse(row["done"], row["id"])


class GapRecordTests(unittest.TestCase):
    def finding(self, **overrides):
        document = {"subject": "A", "area": "combat", "claim": "The client sends MSG_COMBATMOVE before the planning timer expires.",
                    "revision": "r806919.Wizard_1_610", "method": "observation", "how_to_repeat": ["Start a duel."],
                    "evidence": ["Frame 3 carries it."], "disproof": "A capture where it arrives afterwards.",
                    "confidence": "medium", "submitted_by": "t", "submitted_on": "2026-09-19", "status": "claimed"}
        document.update(overrides)
        return document

    def test_a_claim_about_the_game_passes(self):
        self.assertEqual(ci_findings.problems_for("contrib/findings/combat/move-order.json", self.finding()), [])

    def test_a_claim_that_the_repository_lacks_evidence_is_refused(self):
        found = ci_findings.problems_for("contrib/findings/combat/move-order.json",
                                         self.finding(claim="The repository does not yet establish when MSG_COMBATMOVE is sent."))
        self.assertTrue(any("about this repository" in problem for problem in found), found)

    def test_a_file_named_as_a_gap_record_is_refused(self):
        found = ci_findings.problems_for("contrib/findings/combat/move-order-evidence-gap.json", self.finding())
        self.assertTrue(any("named as a gap record" in problem for problem in found), found)


class FindingsTests(unittest.TestCase):
    def sound(self, **changes):
        finding = {
            "subject": "MSG_USER_VALIDATE",
            "area": "protocol",
            "claim": "The client sends this message instead of showing its login window when the -U option carries a user id and a key.",
            "revision": "r806919.Wizard_1_610",
            "method": "capture",
            "how_to_repeat": ["Start the client with -U and capture the login port."],
            "evidence": ["The first frame after the session accept carries service 7, order 15."],
            "disproof": "A run with -U where the client shows its login window and sends MSG_USER_AUTHEN instead.",
            "confidence": "high",
            "submitted_by": "someone",
            "submitted_on": "2026-09-18",
            "status": "claimed",
        }
        finding.update(changes)
        return finding

    def test_a_sound_finding_passes(self):
        self.assertEqual(ci_findings.problems_for("contrib/findings/protocol/validate.json", self.sound()), [])

    def test_a_missing_field_is_reported(self):
        finding = self.sound()
        del finding["disproof"]
        problems = ci_findings.problems_for("contrib/findings/protocol/validate.json", finding)
        self.assertTrue(any("missing disproof" in problem for problem in problems))

    def test_a_verified_finding_must_say_who_proved_it(self):
        problems = ci_findings.problems_for("contrib/findings/protocol/validate.json", self.sound(status="verified"))
        self.assertTrue(any("verified_by" in problem for problem in problems))

    def test_pasted_game_bytes_are_refused(self):
        hex_run = "de ad be ef " * 24
        problems = ci_findings.problems_for("contrib/findings/protocol/validate.json", self.sound(evidence=[hex_run]))
        self.assertTrue(any("hex bytes" in problem for problem in problems))

    def test_the_folder_must_match_the_area(self):
        problems = ci_findings.problems_for("contrib/findings/combat/validate.json", self.sound())
        self.assertTrue(any("names area protocol" in problem for problem in problems))

if __name__ == "__main__":
    unittest.main(verbosity=1)

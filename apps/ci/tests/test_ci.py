# Project Ambrose by Imjustchico
# Self-tests for the forbidden file scan, including the key, store, log and token material it keeps out, the contributor track path check, the commit trailer check, the build stages, the vcpkg cache key, the usage count, and the build leg selection against fakes, a real git repository and a fake Actions API, and the milestone track, where a branch named for any milestone is allowed the source tree, nothing is held or reserved, and the track's Started rows name real milestones.
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
from pathlib import Path
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "progress"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "site"))

import ci_build
import ci_commit_trailer
import ci_contrib_paths
import ci_local
import ci_dependency_notices
import ci_findings
import ci_forbidden_files
import ci_roadmap_state
import ci_select_legs
import ci_sql
import ci_stress
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

    def test_naming_another_wizard_project_is_forbidden(self):
        self.assertForbidden("doc/notes.md", ("We studied " + "Im" + "light").encode(), "names another Wizard101 project")

    def test_a_file_from_another_project_is_forbidden_by_its_extension(self):
        self.assertForbidden("doc/notes.md", b"See TokenBucket.cs for the shape", "another project")
        self.assertForbidden("doc/CLIENT.md", b"asset_fetcher.rs line 40 reads it", "another project")
        self.assertForbidden("README.md", b"Open Ambrose.sln in Visual Studio", "another project")

    def test_our_own_files_and_a_documentation_host_are_allowed(self):
        self.assertAllowed("doc/notes.md", b"src/server/shared/Network/Frame.cpp and apps/ci/ci_build.py")
        self.assertAllowed("THIRD-PARTY-NOTICES.md", b"Documented at https://docs.rs/serde and used under MIT.")
        self.assertAllowed("apps/ci/ci_forbidden_files.py", b"OTHER_LANGUAGE_FILE names TokenBucket.cs in its own test")

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

    def test_pushes_build_linux_gcc_for_code_data_and_build_inputs(self):
        now = at("2026-10-01T00:00:00Z")
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit([".github/workflows/core-build.yml"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["vcpkg.json", "doc/ROADMAP.md"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["apps/ci/ci_select_legs.py"]))["legs"], [])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(None))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["src/test/server/shared/ReloadTest.cpp"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["data/sql/updates/db_login/2026_09_24_00.sql"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["CMakeLists.txt"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["cmake/Ambrose.cmake"]))["legs"], ["linux-gcc"])
        self.assertEqual(ci_select_legs.plan("push", {}, now, FakeGit(["doc/PANEL.md", "README.md"]))["legs"], [])

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
        for entry in ci_select_legs.SMOKE_PATHS + ci_select_legs.BUILD_PATHS:
            self.assertTrue(any(entry.startswith(path[:-2]) if path.endswith("/**") else entry == path for path in paths), entry)
        self.assertIn(f"github.event.schedule == '{ci_select_legs.SUNDAY_CRON}'", self.workflow)
        self.assertIn(f"github.event.schedule == '{ci_select_legs.WEDNESDAY_CRON}'", self.workflow)
        self.assertTrue(os.path.isfile(os.path.join(ROOT, ".github", "workflows", ci_select_legs.WORKFLOW_FILE)))
        self.assertIn("actions: read", self.workflow)
        self.assertIn("GITHUB_TOKEN: ${{ github.token }}", self.workflow)
        self.assertIn("name: build (${{ matrix.configure }})", self.workflow)
        self.assertIn("--stage build-test", self.workflow)
        self.assertEqual(self.workflow.count("--setup-vcpkg"), 1)

    def test_a_push_with_no_build_input_still_builds_code_that_was_never_compiled(self):
        now = at("2026-10-01T00:00:00Z")
        docs = ["doc/ROADMAP.md"]
        behind = FakeGit(docs, {"codeaaa": True})
        result = ci_select_legs.plan("push", {}, now, behind, ci_select_legs.KnownBuilds({"linux-gcc": "codeaaa"}))
        self.assertEqual(result["legs"], ["linux-gcc"])
        self.assertIn("code has changed since its last build", " ".join(result["reasons"]))
        level = FakeGit(docs, {"codeaaa": False})
        self.assertEqual(ci_select_legs.plan("push", {}, now, level, ci_select_legs.KnownBuilds({"linux-gcc": "codeaaa"}))["legs"], [])
        unknown = FakeGit(docs, {"codeaaa": True})
        self.assertEqual(ci_select_legs.plan("push", {}, now, unknown, ci_select_legs.KnownBuilds({}))["legs"], [])
        unreadable = FakeGit(docs, {"codeaaa": True})
        refused = ci_select_legs.plan("push", {}, now, unreadable, FailingActions())
        self.assertEqual(refused["legs"], [])
        self.assertIn("earlier builds could not be read", " ".join(refused["warnings"]))

    def test_a_generated_file_a_contributor_cannot_rebuild_is_not_checked_against_them(self):
        guard = "OUTSIDE_PULL_REQUEST"
        self.assertIn("doc/progress/", ci_contrib_paths.RESERVED_PREFIXES)
        declared = self.workflow.split(guard + ":", 1)[1].splitlines()[0]
        for clause in ("head.repo.full_name != github.repository", "'contrib'", "startsWith(github.head_ref, 'milestone/')"):
            self.assertIn(clause, declared, clause)
        step = self.workflow.split("- name: Progress card", 1)[1].split("- name: ", 1)[0]
        self.assertIn(guard + " != 'true'", step)
        self.assertIn("apps/progress/progress.py --check", step)

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
        return argparse.Namespace(configure_preset="linux-gcc", build_preset="linux-gcc-debug", test_preset=None, warnings_as_errors=True, stage=stage, test_jobs=1, exclude_label=[])

    def test_each_stage_runs_its_commands(self):
        configure = [["cmake", "--version"], ["cmake", "--preset", "linux-gcc", "-DAMBROSE_WARNINGS_AS_ERRORS=ON"]]
        build = [["cmake", "--build", "--preset", "linux-gcc-debug"], ["ctest", "--preset", "linux-gcc-debug"]]
        self.assertEqual(ci_build.commands(self.arguments("all")), configure + build)
        self.assertEqual(ci_build.commands(self.arguments("configure")), configure)
        self.assertEqual(ci_build.commands(self.arguments("build-test")), build)

    def test_main_runs_the_chosen_stage(self):
        ran = []
        with mock.patch.dict(os.environ, {"VCPKG_ROOT": "vcpkg"}), mock.patch.object(ci_build, "run", side_effect=lambda command, **_: ran.append(command)):
            self.assertEqual(ci_build.main(["--configure-preset", "linux-gcc", "--build-preset", "linux-gcc-debug", "--stage", "build-test", "--test-jobs", "1"]), 0)
        self.assertEqual(ran, [["cmake", "--build", "--preset", "linux-gcc-debug"], ["ctest", "--preset", "linux-gcc-debug"]])

    def test_tests_run_on_every_core_unless_told_otherwise(self):
        ran = []
        with mock.patch.dict(os.environ, {"VCPKG_ROOT": "vcpkg"}), mock.patch.object(ci_build, "run", side_effect=lambda command, **_: ran.append(command)),                 mock.patch.object(ci_build.os, "cpu_count", return_value=12):
            self.assertEqual(ci_build.main(["--configure-preset", "linux-gcc", "--build-preset", "linux-gcc-debug", "--stage", "build-test"]), 0)
        self.assertEqual(ran[-1], ["ctest", "--preset", "linux-gcc-debug", "--parallel", "12"])

    def test_an_excluded_label_is_matched_whole(self):
        ran = []
        with mock.patch.dict(os.environ, {"VCPKG_ROOT": "vcpkg"}), mock.patch.object(ci_build, "run", side_effect=lambda command, **_: ran.append(command)):
            ci_build.main(["--leg", "linux-gcc-asan", "--test-jobs", "1", "--exclude-label", "render", "--exclude-label", "slow"])
        self.assertEqual(ran[-1], ["ctest", "--preset", "linux-gcc-asan", "--label-exclude", "^(render|slow)$"])


class BuildProfileTests(unittest.TestCase):
    def test_profile_reads_each_steps_latest_run_and_the_test_costs(self):
        with tempfile.TemporaryDirectory() as tree:
            Path(tree, ".ninja_log").write_text("# ninja log v5\n0\t9000\t1\ta.cpp.o\th\n0\t4000\t1\tb.cpp.obj\th\n10\t2010\t1\ta.cpp.o\th\n0\t30000\t1\tbin/unit_tests\th\n5\t1405\t1\tbuild.ninja\tr\n5\t1405\t1\tcmake_install.cmake\tr\nbroken\n", encoding="utf-8")
            Path(tree, "Testing", "Temporary").mkdir(parents=True)
            Path(tree, "Testing", "Temporary", "CTestCostData.txt").write_text("Fast.Test 3 0.5\nSlow.Test 2 120.25\n---\nSlow.Test\n", encoding="utf-8")
            self.assertEqual(ci_build.ninja_steps(os.path.join(tree, ".ninja_log")), {"a.cpp.o": 2.0, "b.cpp.obj": 4.0, "bin/unit_tests": 30.0, "build.ninja": 1.4},
                             "one step writing several outputs counts once")
            self.assertEqual(ci_build.test_costs(os.path.join(tree, "Testing", "Temporary", "CTestCostData.txt")), {"Fast.Test": 0.5, "Slow.Test": 120.25})
            text = ci_build.profile(tree, 1)
            self.assertIn("2 compiles taking 6s together, 2 other steps such as links taking 31s", text)
            self.assertIn("      4.0s  b.cpp.obj", text)
            self.assertNotIn("a.cpp.o", text)
            self.assertIn("    120.2s  Slow.Test", text)
            Path(tree, ".ninja_log").write_text("# ninja log v5\n0\t3000\t1\tsrc/test/CMakeFiles/unit_tests.dir/Debug/A.cpp.o\th\n0\t2000\t1\tsrc/test/CMakeFiles/unit_tests.dir/Debug/B.cpp.o\th\n0\t1000\t1\tsrc/common/CMakeFiles/common.dir/Debug/C.cpp.o\th\n", encoding="utf-8")
            text = ci_build.profile(tree, 2)
            self.assertIn("        5s      2 files  unit_tests\n        1s      1 files  common", text)

    def test_a_tree_with_neither_file_says_so(self):
        with tempfile.TemporaryDirectory() as tree:
            text = ci_build.profile(tree, 5)
            self.assertIn("no .ninja_log", text)
            self.assertIn("no Testing/Temporary/CTestCostData.txt", text)

    def test_profile_builds_nothing(self):
        with tempfile.TemporaryDirectory() as tree, mock.patch.object(ci_build, "run") as ran, mock.patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(ci_build.main(["--profile", tree]), 0)
        ran.assert_not_called()


class BuildTargetTests(unittest.TestCase):
    LISTING = json.dumps({"tests": [
        {"name": "Auth.One", "command": ["/b/bin/Debug/unit_tests", "--gtest_filter=Auth.One"]},
        {"name": "Auth.Two", "command": ["C:\\b\\bin\\Debug\\unit_tests.exe", "--gtest_filter=Auth.Two"]},
        {"name": "Zone.Client", "command": ["/b/bin/Debug/client_tests"]},
        {"name": "ci.selftest", "command": ["/usr/bin/python3", "x.py"]},
        {"name": "NoCommand"}]})

    def arguments(self, **overrides):
        values = {"build_preset": "linux-gcc-debug", "test_preset": None, "target": ["unit_tests"], "tests": None, "jobs": 4, "test_jobs": 4, "exclude_label": []}
        values.update(overrides)
        return argparse.Namespace(**values)

    def test_a_targets_tests_are_those_its_own_program_runs(self):
        self.assertEqual(ci_build.tests_of(self.LISTING, ["unit_tests"]), ["Auth.One", "Auth.Two"])
        self.assertEqual(ci_build.tests_of(self.LISTING, ["unit_tests"], "Two$"), ["Auth.Two"])
        self.assertEqual(ci_build.tests_of(self.LISTING, ["client_tests", "unit_tests"]), ["Auth.One", "Auth.Two", "Zone.Client"])

    def test_it_builds_the_target_at_the_jobs_given_and_runs_only_its_tests(self):
        ran = []
        listed = []

        def runner(command, **_):
            ran.append(command)
            if command[0] == "ctest":
                listed.append(Path(command[command.index("--tests-from-file") + 1]).read_text(encoding="utf-8"))

        self.assertEqual(ci_build.run_targets(self.arguments(tests="One"), {}, runner, lambda _: self.LISTING), 0)
        self.assertEqual(ran[0], ["cmake", "--build", "--preset", "linux-gcc-debug", "--target", "unit_tests", "--parallel", "4"])
        self.assertEqual(ran[1][:4], ["ctest", "--preset", "linux-gcc-debug", "--tests-from-file"])
        self.assertEqual(ran[1][-2:], ["--parallel", "4"])
        self.assertEqual(listed, ["Auth.One\n"])
        self.assertFalse(os.path.exists(ran[1][5]), "the list is removed after the run")

    def test_a_target_with_no_matching_test_is_built_only(self):
        ran = []
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(ci_build.run_targets(self.arguments(target=["gameserver"]), {}, lambda command, **_: ran.append(command), lambda _: self.LISTING), 0)
        self.assertEqual(len(ran), 1)

    def test_a_failed_build_is_its_exit_status(self):
        def runner(command, **_):
            raise subprocess.CalledProcessError(3, command)

        self.assertEqual(ci_build.run_targets(self.arguments(), {}, runner, lambda _: self.LISTING), 3)

    def test_the_quick_bus_job_counts_are_the_defaults(self):
        with mock.patch.dict(os.environ, {"CMAKE_BUILD_PARALLEL_LEVEL": "4", "CTEST_PARALLEL_LEVEL": "4"}), mock.patch.object(ci_build, "run_targets", return_value=0) as targets:
            self.assertEqual(ci_build.main(["--build-preset", "linux-gcc-debug", "--target", "unit_tests"]), 0)
        self.assertEqual((targets.call_args[0][0].jobs, targets.call_args[0][0].test_jobs), (4, 4))


class BuildLegTests(unittest.TestCase):
    def test_a_leg_names_its_presets_and_defaults_the_rest(self):
        self.assertEqual(ci_build.parse_leg("linux-gcc:linux-gcc-debug"), {"configure": "linux-gcc", "build": "linux-gcc-debug", "test": "linux-gcc-debug"})
        self.assertEqual(ci_build.parse_leg("linux-gcc-asan"), {"configure": "linux-gcc-asan", "build": "linux-gcc-asan", "test": "linux-gcc-asan"})
        with self.assertRaises(argparse.ArgumentTypeError):
            ci_build.parse_leg("linux-gcc::x")

    def test_legs_run_in_turn_past_a_failure_and_end_with_their_times(self):
        ran = []

        def runner(command, **_):
            ran.append(command)
            if command[:3] == ["cmake", "--build", "--preset"] and command[3] == "linux-gcc-debug":
                raise subprocess.CalledProcessError(2, command)

        with mock.patch.dict(os.environ, {"VCPKG_ROOT": "vcpkg"}), mock.patch.object(ci_build, "run", side_effect=runner),                 mock.patch("sys.stdout", new_callable=io.StringIO) as printed:
            result = ci_build.main(["--leg", "linux-gcc:linux-gcc-debug", "--leg", "linux-gcc-asan", "--test-jobs", "4"])
        self.assertEqual(result, 1)
        self.assertNotIn(["ctest", "--preset", "linux-gcc-debug", "--parallel", "4"], ran)
        self.assertIn(["ctest", "--preset", "linux-gcc-asan", "--parallel", "4"], ran)
        self.assertRegex(printed.getvalue(), r"linux-gcc-debug +\d+s +\d+s +\d+s +\d+s  failed with 2")
        self.assertRegex(printed.getvalue(), r"linux-gcc-asan +\d+s +\d+s +\d+s +\d+s  passed")

    def test_legs_take_no_single_preset_options(self):
        with mock.patch("sys.stderr", new_callable=io.StringIO), self.assertRaises(SystemExit):
            ci_build.main(["--leg", "linux-gcc-asan", "--build-preset", "linux-gcc-debug"])
        with mock.patch("sys.stderr", new_callable=io.StringIO), self.assertRaises(SystemExit):
            ci_build.main(["--leg", "linux-gcc-asan", "--sync", "/elsewhere"])

    def test_a_sync_refuses_its_own_tree_and_otherwise_keeps_the_build_trees(self):
        ran = []
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            self.assertEqual(ci_build.sync(ci_build.ROOT, "abc123", runner=ran.append), 2)
        self.assertEqual(ran, [])
        with tempfile.TemporaryDirectory() as source:
            self.assertEqual(ci_build.sync(source, "abc123", runner=ran.append), 0)
        self.assertEqual(ran, [["git", "fetch", "-q", source, "+refs/heads/*:refs/remotes/source/*"], ["git", "checkout", "-q", "--detach", "abc123"],
                               ["git", "clean", "-qfdx", "-e", "build"]])

    def test_the_synced_run_carries_every_option_but_the_sync(self):
        self.assertEqual(ci_build.without_sync(["--sync", "/mnt/k/repo", "--commit=abc", "--leg", "linux-gcc-asan", "--test-jobs", "8"]),
                         ["--leg", "linux-gcc-asan", "--test-jobs", "8"])


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



class SqlTests(unittest.TestCase):
    HEADER = "-- Project Ambrose by Imjustchico\n-- Rows for a test.\n"

    def repository(self, folder):
        root = Path(folder)
        for command in (["init", "-q", "-b", "main"], ["config", "user.email", "test@example.invalid"], ["config", "user.name", "Test"]):
            subprocess.run(["git", "-C", folder, *command], check=True, capture_output=True)
        return root

    def write(self, root, path, text):
        file = root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def commit(self, root, message="change"):
        subprocess.run(["git", "-C", str(root), "add", "-A"], check=True, capture_output=True)
        subprocess.run(["git", "-C", str(root), "commit", "-q", "-m", message], check=True, capture_output=True)
        return subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()

    def test_editing_an_applied_update_fails_unless_a_squash_says_why(self):
        with tempfile.TemporaryDirectory() as folder:
            root = self.repository(folder)
            self.write(root, "data/sql/updates/db_world/2026_01_01_00.sql", self.HEADER + "SELECT 1;\n")
            before = self.commit(root)
            self.write(root, "data/sql/updates/db_world/2026_01_01_00.sql", self.HEADER + "SELECT 2;\n")
            self.commit(root)
            self.assertEqual(ci_sql.main(["--root", folder, "check", "--range", f"{before}..HEAD"]), 1)
            self.assertEqual(ci_sql.main(["--root", folder, "check", "--range", f"{before}..HEAD", "--labels", '["squash"]']), 0)
            self.write(root, "data/sql/updates/db_world/2026_01_01_00.sql", self.HEADER + "SELECT 3;\n")
            self.commit(root, "Fold the first updates together\n\nSquashes SQL: the three first updates are one now")
            self.assertEqual(ci_sql.main(["--root", folder, "check", "--range", f"{before}..HEAD"]), 0)

    def test_new_files_are_named_and_headed_as_the_updater_reads_them(self):
        read = {
            "data/sql/updates/db_world/2026_09_30_00.sql": self.HEADER,
            "data/sql/updates/db_world/zone-rows.sql": self.HEADER,
            "data/sql/updates/pending_db_world/rev_1790241513_zone-teleport.sql": self.HEADER,
            "data/sql/updates/pending_db_world/doors.sql": self.HEADER,
            "data/sql/updates/pending_db_login/rev_1790241514_bans.sql": "CREATE TABLE `bans` (`id` INT);\n",
        }.get
        problems = ci_sql.check([("A", path) for path in (
            "data/sql/updates/db_world/2026_09_30_00.sql",
            "data/sql/updates/db_world/zone-rows.sql",
            "data/sql/updates/pending_db_world/rev_1790241513_zone-teleport.sql",
            "data/sql/updates/pending_db_world/doors.sql",
            "data/sql/updates/pending_db_login/rev_1790241514_bans.sql",
            "data/sql/updates/pending_db_world/README.md")], read)
        self.assertEqual(len(problems), 3, problems)
        self.assertIn("zone-rows.sql: a released update is named", problems[0])
        self.assertIn("doors.sql: a pending update is named rev_", problems[1])
        self.assertIn("rev_1790241514_bans.sql: does not open with the two-line header", problems[2])

    def test_a_pending_update_takes_the_next_free_number_for_its_day_in_the_order_it_was_written(self):
        with tempfile.TemporaryDirectory() as folder:
            root = self.repository(folder)
            self.write(root, "data/sql/updates/db_world/2026_09_30_00.sql", self.HEADER)
            self.write(root, "data/sql/updates/pending_db_world/rev_1790300000_later.sql", self.HEADER)
            self.write(root, "data/sql/updates/pending_db_world/rev_1767225600_npc.sql", self.HEADER)
            self.write(root, "data/sql/updates/pending_db_login/rev_1767225600_bans.sql", self.HEADER)
            self.commit(root)
            self.assertEqual(ci_sql.main(["--root", folder, "promote", "--check", "--date", "2026-09-30"]), 1)
            self.assertEqual(ci_sql.main(["--root", folder, "promote", "--date", "2026-09-30"]), 0)
            self.assertTrue((root / "data/sql/updates/db_world/2026_09_30_01.sql").exists())
            self.assertTrue((root / "data/sql/updates/db_world/2026_09_30_02.sql").exists())
            self.assertTrue((root / "data/sql/updates/db_login/2026_09_30_00.sql").exists())
            self.assertEqual(sorted(path.name for path in (root / "data/sql/updates/pending_db_world").glob("*.sql")), [])
            first = (root / "data/sql/updates/db_world/2026_09_30_01.sql")
            self.assertEqual(first.read_text(encoding="utf-8"), self.HEADER)
            status = subprocess.run(["git", "-C", folder, "status", "--porcelain"], check=True, capture_output=True, text=True).stdout
            self.assertIn("R  data/sql/updates/pending_db_world/rev_1767225600_npc.sql -> data/sql/updates/db_world/2026_09_30_01.sql", status)
            self.assertEqual(ci_sql.main(["--root", folder, "promote", "--check", "--date", "2026-09-30"]), 0)

    def test_base_files_come_first_with_the_bookkeeping_tables_last_then_released_then_pending(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for path in ("data/sql/base/db_world/updates.sql", "data/sql/base/db_world/updates_include.sql", "data/sql/base/db_world/zone_extractor.sql",
                         "data/sql/updates/db_world/2026_09_30_00.sql", "data/sql/updates/db_world/2026_01_01_00.sql",
                         "data/sql/updates/pending_db_world/rev_1790241513_zone-teleport.sql", "data/sql/base/db_login/updates.sql"):
                self.write(root, path, self.HEADER)
            order = [path.relative_to(root).as_posix() for path in ci_sql.ordered_files(root, "world")]
            self.assertEqual(order, [
                "data/sql/base/db_world/zone_extractor.sql", "data/sql/base/db_world/updates.sql", "data/sql/base/db_world/updates_include.sql",
                "data/sql/updates/db_world/2026_01_01_00.sql", "data/sql/updates/db_world/2026_09_30_00.sql",
                "data/sql/updates/pending_db_world/rev_1790241513_zone-teleport.sql"])
            self.assertEqual(ci_sql.databases(root), ["login", "world"])

    def test_a_refused_file_carries_the_servers_error_code(self):
        refused = subprocess.CompletedProcess([], 1, b"", b"mysql: [Warning] a note\nERROR 1064 (42000) at line 3: You have an error\n")
        with mock.patch.object(ci_sql, "mysql", return_value=refused):
            self.assertEqual(ci_sql.run_file("mysql", {}, "schema", "a.sql"), ("error", 1064, "ERROR 1064 (42000) at line 3: You have an error"))
        with mock.patch.object(ci_sql, "mysql", return_value=subprocess.CompletedProcess([], 0, b"", b"")):
            self.assertEqual(ci_sql.run_file("mysql", {}, "schema", "a.sql"), ("ok", None, ""))

    def test_duality_names_the_update_only_one_server_accepts_and_the_one_both_refuse(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for path in ("data/sql/base/db_world/tables.sql", "data/sql/updates/db_world/2026_10_01_00.sql", "data/sql/updates/pending_db_world/rev_1790000000_doors.sql",
                         "data/sql/base/db_login/tables.sql", "data/sql/updates/db_login/2026_10_01_00.sql", "data/sql/base/db_characters/tables.sql"):
                self.write(root, path, self.HEADER)
            refusals = {("mysql", "world"): "rev_1790000000_doors.sql", ("mysql", "login"): "2026_10_01_00.sql", ("mariadb", "login"): "2026_10_01_00.sql"}

            def run(name, database, files):
                refused = refusals.get((name, database))
                return next(((file, ("error", 1064, f"ERROR 1064 from {name}")) for file in files if file.name == refused), None)

            problems = ci_sql.duality_updates(root, ["mysql", "mariadb"], run)
            self.assertEqual(problems, [
                "data/sql/updates/db_login/2026_10_01_00.sql: refused by every server (mysql: ERROR 1064 from mysql; mariadb: ERROR 1064 from mariadb)",
                "data/sql/updates/pending_db_world/rev_1790000000_doors.sql: only mariadb accepts it (mysql: ERROR 1064 from mysql)"])

    def test_duality_flags_files_only_one_server_accepts_and_corpus_verdicts_that_differ(self):
        verdicts = {("mysql", "both.sql"): ("ok", None, ""), ("mariadb", "both.sql"): ("ok", None, ""),
                    ("mysql", "maria.sql"): ("error", 1064, "ERROR 1064"), ("mariadb", "maria.sql"): ("ok", None, ""),
                    ("mysql", "broken.sql"): ("error", 1064, "ERROR 1064"), ("mariadb", "broken.sql"): ("error", 1146, "ERROR 1146")}
        run = lambda name, file: verdicts[(name, file.name)]
        files = [Path("both.sql"), Path("maria.sql"), Path("broken.sql")]
        with mock.patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(ci_sql.duality_files(files, ["mysql", "mariadb"], run), ["maria.sql: only mariadb accepts it (mysql=error 1064 mariadb=ok)"])
            expected = {"both.sql": {"mysql": {"verdict": "ok"}, "mariadb": {"verdict": "ok"}},
                        "maria.sql": {"mysql": {"verdict": "error", "code": 1064}, "mariadb": {"verdict": "ok"}},
                        "broken.sql": {"mysql": {"verdict": "error", "code": 1064}, "mariadb": {"verdict": "error", "code": 1064}}}
            self.assertEqual(ci_sql.duality_files(files, ["mysql", "mariadb"], run, expected),
                             ["broken.sql: mysql=error 1064 mariadb=error 1146, the corpus says mysql=error 1064 mariadb=error 1064"])
            self.assertEqual(ci_sql.duality_files([Path("unlisted.sql")], ["mysql"], lambda name, file: ("ok", None, ""), expected), ["unlisted.sql: the corpus gives no verdict for it"])

    def test_duality_finds_every_c81_corpus_case_and_needs_its_servers_named(self):
        root = Path(__file__).resolve().parents[3]
        corpus = root / "contrib/fixtures/c81-sql-duality-corpus.json"
        cases = {Path(case["file"]).name: case for case in json.loads(corpus.read_text(encoding="utf-8"))["cases"]}
        seen = []

        def sequence(client, connection, schema, files, keep=False, report=None):
            file = files[0]
            self.assertTrue(file.is_file(), file)
            seen.append(file.name)
            want = cases[file.name][{3306: "mysql", 3307: "mariadb"}[connection["port"]]]
            return (None if want["verdict"] == "ok" else (file, ("error", want["code"], "ERROR"))), None

        servers = [("mysql", "127.0.0.1", 3306), ("mariadb", "127.0.0.1", 3307)]
        with mock.patch.object(ci_sql, "run_sequence", side_effect=sequence), mock.patch("sys.stdout", new_callable=io.StringIO):
            problems = ci_sql.duality(root, "mysql", servers, "root", "", "test", [], corpus)
        self.assertEqual(problems, [])
        self.assertEqual(sorted(set(seen)), sorted(cases))
        self.assertEqual(ci_sql.duality(root, "mysql", [("mysql", "h", 1), ("postgres", "h", 2)], "root", "", "test", [], corpus),
                         [f"{corpus.as_posix()}: the corpus has no verdicts for postgres"])


class StressTests(unittest.TestCase):
    RACE = "\n".join([
        "[ RUN      ] AdminServerTest.RotatesTheToken",
        "src/test/server/shared/Admin/AdminServerTest.cpp:541: Failure",
        "Value of: server.Start(Loopback(), error)",
        "  Actual: false",
        "Expected: true",
        "The admin API could not bind 127.0.0.1:48899",
        "",
        "[  FAILED  ] AdminServerTest.RotatesTheToken (7 ms)",
        "[==========] 37 tests from 1 test suite ran. (617 ms total)",
    ])

    def test_a_failure_is_named_by_its_file_and_reason_with_numbers_made_alike(self):
        self.assertEqual(ci_stress.first_failure(self.RACE), "AdminServerTest.cpp: The admin API could not bind N.N.N.N:N")
        other = self.RACE.replace("48899", "51234").replace("541", "560").replace("/", "\\")
        self.assertEqual(ci_stress.first_failure(other), ci_stress.first_failure(self.RACE))
        self.assertEqual(ci_stress.failed_tests(self.RACE), ["AdminServerTest.RotatesTheToken"])

    def test_a_run_that_stopped_part_way_is_told_apart(self):
        self.assertEqual(ci_stress.first_failure("[ RUN      ] AdminServerTest.X\nterminate called"), "the run ended before it finished")

    def test_copies_run_concurrently_and_every_failing_run_is_counted_and_kept(self):
        with tempfile.TemporaryDirectory() as folder:
            counter = os.path.join(folder, "count")
            fake = os.path.join(folder, "fake.py")
            with open(fake, "w", encoding="utf-8") as script:
                script.write("\n".join([
                    "import os, sys",
                    f"path = {counter!r}",
                    "number = 1",
                    "while True:",
                    "    try:",
                    "        os.close(os.open(f'{path}-{number}', os.O_CREAT | os.O_EXCL | os.O_WRONLY))",
                    "        break",
                    "    except FileExistsError:",
                    "        number += 1",
                    "odd = number % 2",
                    "assert sys.argv[-1] == '--gtest_filter=Fake.*'",
                    f"print({self.RACE!r} if odd else '[==========] 1 tests from 1 test suite ran.')",
                    "sys.exit(1 if odd else 0)",
                ]))
            keep = os.path.join(folder, "kept")
            code = ci_stress.main([sys.executable, "--filter", "Fake.*", "--copies", "2", "--runs", "3", "--keep", keep, "--label", "fake", "--arg", fake])
            self.assertEqual(code, 1)
            self.assertEqual(len([name for name in os.listdir(folder) if name.startswith("count-")]), 6)
            self.assertEqual(len(os.listdir(keep)), 3)
            args = argparse.Namespace(executable=sys.executable, filter="Fake.*", copies=1, runs=2, busy=1, keep=None, extra=[fake])
            tally, _ = ci_stress.stress(args)
            self.assertEqual(tally.runs, 2)
            self.assertEqual(tally.failures, 1)
            self.assertIn("1 of 2 runs failed (1 copies, 1 busy loops", ci_stress.report("fake", tally, 1.0, 1, 1))


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

    TICKED = "## 4.04 World wire math\n\n- [x] Unit: packing lands within 4 units (MovementPackingTest.Packing)\n- [ ] Unit: yaw survives a round trip\n"
    EMPTY = "## 4.04 World wire math\n\n- [ ] Unit: packing lands within 4 units\n- [ ] Unit: yaw survives a round trip\n"

    def ticked_base(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", self.TICKED)
        self.write("doc/ROADMAP.md", "## Where we are\n\nPhase 3 is built, and 4.04 packs positions.\n")
        self.base = self.commit("tick one check and say so")

    def test_a_ticked_check_turned_back_to_empty_is_refused_on_every_branch(self):
        self.ticked_base()
        self.write("doc/roadmap/phase-04-a-wizard.md", self.EMPTY)
        self.commit("an older copy of the phase file")
        self.assertEqual(self.run_check(), 1)
        self.assertEqual(self.run_check("milestone/4.04-world-wire-math"), 1)

    def test_an_untick_that_says_why_is_allowed(self):
        self.ticked_base()
        self.write("doc/roadmap/phase-04-a-wizard.md", self.EMPTY)
        self.commit("drop a tick\n\nUnticks: the test it named never ran")
        self.assertEqual(self.run_check(), 0)

    def test_putting_back_lost_ticks_needs_no_summary_when_the_commit_says_so(self):
        self.write("doc/roadmap/phase-04-a-wizard.md", self.TICKED)
        self.commit("restore a lost tick")
        self.assertEqual(self.run_check(), 1)
        self.write("doc/roadmap/phase-04-a-wizard.md", self.EMPTY)
        self.base = self.commit("back to where the loss left it\n\nUnticks: resetting the fixture")
        self.write("doc/roadmap/phase-04-a-wizard.md", self.TICKED)
        self.commit("restore a lost tick\n\nRestores: an older checkout reverted it")
        self.assertEqual(self.run_check(), 0)

    def test_an_untick_is_read_from_the_lines_themselves(self):
        lines = ["-- [x] Unit: packing lands within 4 units (MovementPackingTest.Packing)", "+- [ ] Unit: packing lands within 4 units"]
        self.assertEqual(len(ci_roadmap_state.unticked_in(lines)), 1)
        reworded = ["-- [x] Unit: packing lands within 4 units (OldTest.Name)", "+- [x] Unit: packing lands within 4 units (NewTest.Name)"]
        self.assertEqual(ci_roadmap_state.unticked_in(reworded), [])
        short = ["-- [x] A check that happens to be long enough (X.Y)", "+- [ ] A check"]
        self.assertEqual(ci_roadmap_state.unticked_in(short), [])
        self.assertEqual(ci_roadmap_state.problems([], [], "", ["x"], ""),
                         ["1 acceptance check(s) went from ticked back to empty; if that is meant, say why on an 'Unticks:' line in the commit message"])
        self.assertEqual(ci_roadmap_state.problems([], [], "", ["x"], "Unticks: wrong"), [])

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


class DependencyNoticeTests(unittest.TestCase):
    def notices(self, manifest, notices):
        folder = tempfile.mkdtemp()
        with io.open(os.path.join(folder, "vcpkg.json"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps({"dependencies": manifest}))
        with io.open(os.path.join(folder, "THIRD-PARTY-NOTICES.md"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(notices)
        return folder

    def test_a_dependency_nobody_wrote_down_is_refused(self):
        folder = self.notices(["fmt", "ftxui"], "| fmt | formatting | MIT | Keep the notice |\n")
        self.assertEqual(ci_dependency_notices.missing(folder), ["ftxui"])
        self.assertEqual(ci_dependency_notices.main(["--root", folder]), 1)

    def test_a_dependency_named_under_its_own_title_is_accepted(self):
        folder = self.notices(["libmariadb", "sqlite3", "nlohmann-json"],
                              "| MariaDB Connector/C | x | LGPL | y |\n| SQLite | x | blessing | y |\n| nlohmann/json | x | MIT | y |\n")
        self.assertEqual(ci_dependency_notices.missing(folder), [])

    def test_an_object_dependency_counts_by_its_name(self):
        folder = self.notices([{"name": "openssl", "features": ["tools"]}], "nothing here\n")
        self.assertEqual(ci_dependency_notices.missing(folder), ["openssl"])

    def test_the_repository_names_every_dependency_it_builds_with(self):
        self.assertEqual(ci_dependency_notices.missing(ROOT), [])
        self.assertEqual(ci_dependency_notices.main(["--root", ROOT]), 0)


class MilestoneTrackTests(unittest.TestCase):
    PHASE = "doc/roadmap/phase-04-a-wizard-stands-in-ravenwood.md"

    def track(self):
        with io.open(os.path.join(ROOT, "doc", "MILESTONE-TRACK.md"), encoding="utf-8") as handle:
            return handle.read()

    def section(self, name):
        return self.track().split("## " + name, 1)[1].split("\n## ", 1)[0]

    def listed(self, name):
        found = set()
        for line in self.section(name).splitlines():
            row = re.match(r"^\| *([\d. ,]+?) *\|", line)
            if row:
                found.update(re.findall(r"\d+\.\d+", row.group(1)))
        return found

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
        for path in ("README.md", "CMakePresets.json", "doc/ROADMAP.md", "doc/MILESTONE-TRACK.md",
                     "contrib/AI-MILESTONES-HERE.md", ".github/workflows/core-build.yml",
                     "apps/ci/ci_contrib_paths.py", "apps/progress/progress.py", "doc/progress/progress.svg"):
            self.assertEqual([entry[0] for entry in ci_contrib_paths.check_milestone([path], "4.04")], [path], path)

    def test_a_grant_lets_exactly_its_files_through_for_exactly_its_milestone(self):
        given = [{"scope": "milestone:3.19", "paths": ["apps/ci/ci_sql_check.py", ".github/workflows/sql.yml"]}]
        granted = ci_contrib_paths.granted_to("3.19", given)
        self.assertEqual(ci_contrib_paths.check_milestone(["apps/ci/ci_sql_check.py", ".github/workflows/sql.yml"], "3.19", granted), [])
        self.assertEqual([path for path, _reason in ci_contrib_paths.check_milestone(["apps/ci/ci_local.py"], "3.19", granted)], ["apps/ci/ci_local.py"])
        self.assertEqual(ci_contrib_paths.granted_to("4.04", given), set())
        self.assertEqual(ci_contrib_paths.granted_to("3.19", [{"scope": "milestone:3.19", "paths": "apps/ci/"}]), set())
        for entry in ci_contrib_paths.grants(ROOT):
            self.assertTrue(str(entry.get("scope", "")).startswith("milestone:"), entry)
            self.assertTrue(entry.get("paths") and all(not path.endswith("/") for path in entry["paths"]), entry)

    def test_a_milestone_branch_may_bring_a_library_with_its_notice(self):
        self.assertEqual(ci_contrib_paths.check_milestone(["vcpkg.json", "THIRD-PARTY-NOTICES.md"], "4.04"), [])

    def test_the_local_run_is_the_checks_job_step_for_step(self):
        found = ci_local.steps(ROOT)
        names = [name for name, _command in found]
        for name in ("Codestyle", "Forbidden files", "Findings", "Contributor track paths", "Roadmap summary", "Commit trailers"):
            self.assertIn(name, names)
        paths = [command for name, command in found if name == "Contributor track paths"][0]
        self.assertIsNone(ci_local.command_for("Contributor track paths", paths, "upstream/main", ""))
        self.assertEqual(ci_local.command_for("Contributor track paths", paths, "upstream/main", "milestone/6.10-schemas"),
                         'python apps/ci/ci_contrib_paths.py --range "upstream/main...HEAD" --branch "milestone/6.10-schemas"')
        self.assertIn('--range "upstream/main..HEAD"', ci_local.command_for("Commit trailers", "python apps/ci/ci_commit_trailer.py --from-github-env", "upstream/main", ""))
        sql = [command for name, command in found if name == "SQL changes"][0]
        self.assertEqual(ci_local.command_for("SQL changes", sql, "upstream/main", "milestone/3.19-sql"), 'python apps/ci/ci_sql.py check --range "upstream/main...HEAD"')
        pending = [command for name, command in found if name == "Pending SQL on main"][0]
        self.assertEqual(ci_local.command_for("Pending SQL on main", pending, "upstream/main", ""), "python apps/ci/ci_sql.py promote --check")
        self.assertIsNone(ci_local.command_for("Pending SQL on main", pending, "upstream/main", "milestone/3.19-sql"))
        self.assertEqual(ci_local.skipped_because(pending), "CI runs it only on a push to main")
        card = [command for name, command in found if name == "Progress card"][0]
        self.assertEqual(ci_local.command_for("Progress card", card, "upstream/main", ""), card)
        self.assertIsNone(ci_local.command_for("Progress card", card, "upstream/main", "milestone/12.07-chat-moderation"))
        self.assertEqual(ci_local.skipped_because(card), "CI skips it on a pull request whose author cannot regenerate the card")

    def test_the_local_run_stamps_the_commit_it_covered(self):
        with tempfile.TemporaryDirectory() as folder:
            stamp = os.path.join(folder, "checks-passed")
            head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
            self.assertEqual(ci_local.write_stamp(ROOT, stamp), head)
            self.assertEqual(io.open(stamp, encoding="utf-8").read(), head)

    def test_a_milestone_branch_stays_inside_its_own_phase_file(self):
        other = "doc/roadmap/phase-05-the-zone-comes-alive-for-one-player.md"
        refused = ci_contrib_paths.check_milestone([self.PHASE, other], "4.04")
        self.assertEqual([entry[0] for entry in refused], [other])
        self.assertEqual(ci_contrib_paths.check_milestone([other], "5.01"), [])

    def test_a_branch_for_any_milestone_is_let_through(self):
        everything = ready_report.milestones(ROOT)
        ready, blocked = ready_report.state(ROOT)
        sample = [row["id"] for row in ready[:5] + blocked[:5]] + ["6.05", "17.24", "17.165", "3.28"]
        for identifier in sample:
            self.assertIn(identifier, everything)
            self.assertEqual(ci_contrib_paths.main(["--root", ROOT, "--paths", "src/x.cpp", "--branch", f"milestone/{identifier}-any"]), 0, identifier)

    def test_a_holds_file_put_back_holds_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            os.makedirs(os.path.join(folder, "doc", "work"))
            with io.open(os.path.join(folder, "doc", "work", "holds.json"), "w", encoding="utf-8", newline="\n") as handle:
                handle.write(json.dumps({"holds": [{"scope": "milestone:4.04", "who": "a session"}, {"scope": "phase:17", "who": "a session"}]}))
            for branch in ("milestone/4.04-world-wire-math", "milestone/17.10-metrics"):
                self.assertEqual(ci_contrib_paths.main(["--root", folder, "--paths", "src/x.cpp", "--branch", branch]), 0, branch)
        self.assertFalse(os.path.exists(os.path.join(ROOT, "doc", "work", "holds.json")))

    def test_a_milestone_branch_cannot_edit_the_file_that_grants_it(self):
        self.assertIn("doc/work/", ci_contrib_paths.RESERVED_PREFIXES)
        refused = ci_contrib_paths.check_milestone(["doc/work/grants.json"], "17.35")
        self.assertEqual([path for path, _reason in refused], ["doc/work/grants.json"])
        self.assertEqual(ci_contrib_paths.main(
            ["--root", ROOT, "--paths", "doc/work/grants.json", "--branch", "milestone/17.35-panel-settings"]), 1)

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

    def test_every_ready_milestone_is_open_and_every_other_one_waiting(self):
        ready, blocked = ready_report.state(ROOT)
        self.assertTrue(ready)
        for row in ready:
            self.assertEqual((row["status"], row["missing"]), ("open", []), row["id"])
        for row in blocked:
            self.assertEqual(row["status"], "waiting", row["id"])
            self.assertTrue(row["missing"], row["id"])

    def test_the_track_reserves_nothing(self):
        track = self.track()
        for gone in ("## Open now", "## Reserved", "## Holds", "## In flight", "## Only the milestones named below"):
            self.assertNotIn(gone, track)
        self.assertIn("Every milestone is open to anyone", track)

    def test_every_started_row_names_a_real_milestone_that_has_not_landed_in_the_track(self):
        everything = ready_report.milestones(ROOT)
        started = board.track_rows(ROOT, everything)
        self.assertEqual(set(started), self.listed("Started"))
        self.assertEqual(sorted(set(started) & self.listed("Landed")), [])
        for identifier, row in started.items():
            self.assertTrue(row["by"].strip() and row["left"].strip(), identifier)

    def test_a_reason_that_mentions_another_milestone_is_not_read_as_listing_it(self):
        table = "| 1.21 | a thing | S | a build | ready now that 3.02 and 4.08 have landed |"
        row = re.match(r"^\| *([\d. ,]+?) *\|", table)
        self.assertEqual(set(re.findall(r"\d+\.\d+", row.group(1))), {"1.21"})
        for name in ("Started", "Landed"):
            for identifier in self.listed(name):
                self.assertRegex(self.section(name), r"(?m)^\| *[\d. ,]*" + re.escape(identifier),
                                 identifier + " is counted in " + name + " but is not in a first column there")

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

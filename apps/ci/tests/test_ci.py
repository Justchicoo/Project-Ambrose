# Project Ambrose by Imjustchico
# Self-tests for the forbidden file scan and the commit trailer check.
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ci_commit_trailer
import ci_forbidden_files


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


if __name__ == "__main__":
    unittest.main(verbosity=1)

# Project Ambrose by Imjustchico
# Self-tests for the codestyle checker built from inline samples of passing and failing files.
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import codestyle

CPP_HEADER = "/*\n * Project Ambrose by Imjustchico\n * Sample source used by the codestyle tests.\n */\n"
HASH_HEADER = "# Project Ambrose by Imjustchico\n# Sample file used by the codestyle tests.\n"
SQL_HEADER = "-- Project Ambrose by Imjustchico\n-- Sample update used by the codestyle tests.\n"
BATCH_HEADER = "REM Project Ambrose by Imjustchico\nREM Sample script used by the codestyle tests.\n"
MD_HEADER = "<!-- Project Ambrose by Imjustchico: Sample page used by the codestyle tests. -->\n"
GOOD_H = CPP_HEADER + "\n#ifndef AMBROSE_SAMPLE_H\n#define AMBROSE_SAMPLE_H\n\nint Sample();\n\n#endif\n"


def check(path, content):
    raw = content if isinstance(content, bytes) else content.encode("utf-8")
    return codestyle.check_file(path, raw)


class CheckerTestCase(unittest.TestCase):
    def assertClean(self, path, content):
        found = check(path, content)
        self.assertEqual(found, [], "\n".join(str(issue) for issue in found))

    def assertIssue(self, path, content, rule, line=None):
        found = check(path, content)
        matches = [issue for issue in found if issue.rule == rule and (line is None or issue.line == line)]
        self.assertTrue(matches, f"expected a {rule} issue at line {line}, got:\n" + "\n".join(str(issue) for issue in found))


class CppTests(CheckerTestCase):
    def test_line_comment_fails_at_its_line(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER + "\nint x = 1; // note\n", "comment", 6)

    def test_block_comment_fails(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER + "\nint x = 1;\n/* note */\n", "comment", 7)

    def test_url_in_string_passes(self):
        self.assertClean("src/Sample.cpp", CPP_HEADER + '\nauto s = "http://x";\n')

    def test_raw_string_with_comment_markers_passes(self):
        self.assertClean("src/Sample.cpp", CPP_HEADER + '\nauto s = R"(/* x */ // y)";\nauto t = R"sql(-- z)sql";\n')

    def test_digit_separators_and_char_literals_pass(self):
        self.assertClean("src/Sample.cpp", CPP_HEADER + "\nint big = 1'000'000;\nchar slash = '/';\nchar quote = '\\'';\nauto s = \"a\\\"//b\";\n")

    def test_comment_after_raw_string_still_fails(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER + '\nauto s = R"(x)"; // note\n', "comment", 6)

    def test_valid_header_file_passes(self):
        self.assertClean("src/common/Sample.h", GOOD_H)

    def test_header_without_guard_fails(self):
        self.assertIssue("src/common/Sample.h", CPP_HEADER + "\n#pragma once\n\nint Sample();\n", "include-guard")

    def test_header_with_wrong_guard_fails(self):
        self.assertIssue("src/common/Sample.h", GOOD_H.replace("AMBROSE_SAMPLE_H", "SAMPLE_H"), "include-guard")

    def test_template_header_uses_inner_name_for_guard(self):
        self.assertClean("src/genrev/Sample.h.in", GOOD_H)

    def test_missing_header_fails(self):
        self.assertIssue("src/Sample.cpp", "int x = 1;\n", "header", 1)

    def test_misspelled_branding_fails(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER.replace("Imjustchico", "Imjustchicoo"), "header", 2)

    def test_empty_brief_fails(self):
        self.assertIssue("src/Sample.cpp", "/*\n * Project Ambrose by Imjustchico\n * \n */\n", "header", 3)

    def test_brief_repeating_branding_fails(self):
        self.assertIssue("src/Sample.cpp", "/*\n * Project Ambrose by Imjustchico\n * Project Ambrose file\n */\n", "header", 3)


class SqlTests(CheckerTestCase):
    def test_extra_comment_fails(self):
        self.assertIssue("data/sql/updates/db_world/2026_09_13_00.sql", SQL_HEADER + "-- extra\nSELECT 1;\n", "comment", 3)

    def test_dashes_in_string_pass(self):
        self.assertClean("data/sql/updates/db_world/2026_09_13_00.sql", SQL_HEADER + "SELECT '--';\nSELECT 5 - -3;\n")

    def test_hash_and_block_comments_fail(self):
        self.assertIssue("data/sql/base/db_world/x.sql", SQL_HEADER + "SELECT 1; # note\n", "comment", 3)
        self.assertIssue("data/sql/base/db_world/x.sql", SQL_HEADER + "SELECT /* note */ 1;\n", "comment", 3)


class HashFamilyTests(CheckerTestCase):
    def test_cmake_comment_fails_and_quoted_hash_passes(self):
        self.assertIssue("src/CMakeLists.txt", HASH_HEADER + "set(A 1) # note\n", "comment", 3)
        self.assertClean("src/CMakeLists.txt", HASH_HEADER + 'set(A "#1")\nset(B [[# not a comment]])\n')

    def test_cmake_bracket_comment_fails(self):
        self.assertIssue("src/cmake/Sample.cmake", HASH_HEADER + "#[[ note ]]\nset(A 1)\n", "comment", 3)

    def test_python_comment_fails_and_string_hash_passes(self):
        self.assertIssue("apps/ci/sample.py", HASH_HEADER + "x = 1  # note\n", "comment", 3)
        self.assertClean("apps/ci/sample.py", HASH_HEADER + 'x = "# not a comment"\ny = """\n# still a string\n"""\n')

    def test_shell_with_shebang_header_passes(self):
        self.assertClean("apps/ci/sample.sh", "#!/usr/bin/env bash\n" + HASH_HEADER + 'echo "$#" ${#ARR[@]} "a#b"\n')

    def test_shell_comment_fails(self):
        self.assertIssue("apps/ci/sample.sh", HASH_HEADER + "echo hi # note\n", "comment", 3)

    def test_shell_heredoc_body_is_not_a_comment(self):
        self.assertClean("apps/ci/sample.sh", HASH_HEADER + "cat > out.txt <<'EOF'\n# inside heredoc\nEOF\necho done\n")

    def test_powershell_comments_fail(self):
        self.assertIssue("apps/installer/sample.ps1", HASH_HEADER + "Write-Host 'a#b' # note\n", "comment", 3)
        self.assertIssue("apps/installer/sample.ps1", HASH_HEADER + "<# block #>\n", "comment", 3)

    def test_yaml_comment_fails_and_quoted_hash_passes(self):
        self.assertIssue(".github/workflows/core.yml", HASH_HEADER + "name: build # note\n", "comment", 3)
        self.assertClean(".github/workflows/core.yml", HASH_HEADER + "name: 'build #1'\nurl: a#b\n")

    def test_git_and_editor_config_comments_fail(self):
        self.assertIssue(".gitignore", HASH_HEADER + "build/\n# note\n", "comment", 4)
        self.assertIssue(".editorconfig", HASH_HEADER + "root = true\n; note\n", "comment", 4)
        self.assertIssue("conf/dist/gameserver.conf.dist", HASH_HEADER + "# Port = 1\nPort = 12000\n", "comment", 3)

    def test_dist_templates_use_their_inner_file_type(self):
        self.assertClean("conf/dist/config.cmake.dist", HASH_HEADER + 'set(TOOLS ON CACHE BOOL "Build tools")\n')
        self.assertIssue("conf/dist/config.cmake.dist", HASH_HEADER + "set(TOOLS ON) # note\n", "comment", 3)
        self.assertClean("conf/dist/env.dist", HASH_HEADER + "AMBROSE_LOGS_DIR=logs\n")
        self.assertIssue("conf/dist/env.dist", HASH_HEADER + "# AMBROSE_X=1\n", "comment", 3)

    def test_missing_hash_header_fails(self):
        self.assertIssue("src/CMakeLists.txt", "cmake_minimum_required(VERSION 3.25)\n", "header", 1)


class OtherFormatTests(CheckerTestCase):
    def test_batch_rem_after_header_fails(self):
        self.assertIssue("apps/installer/sample.bat", BATCH_HEADER.replace("\n", "\r\n") + "echo hi\r\nREM note\r\n", "comment", 4)

    def test_markdown_comment_fails_but_code_samples_pass(self):
        self.assertIssue("doc/Sample.md", MD_HEADER + "Text\n<!-- note -->\n", "comment", 3)
        self.assertClean("doc/Sample.md", MD_HEADER + "Use `<!-- x -->` here.\n\n```html\n<!-- sample -->\n```\n")

    def test_markdown_header_brief_required(self):
        self.assertIssue("doc/Sample.md", "<!-- Project Ambrose by Imjustchico:  -->\n", "header", 1)

    def test_json_is_exempt(self):
        self.assertClean("vcpkg.json", "{\n  \"name\": \"x\"\n}\n")

    def test_unknown_file_type_fails(self):
        self.assertIssue("tools/sample.xyz", "data\n", "unknown-type", 1)


class WhitespaceTests(CheckerTestCase):
    def test_crlf_fails_outside_batch_and_powershell(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER.replace("\n", "\r\n") + "int x;\r\n", "line-ending", 1)

    def test_trailing_whitespace_fails(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER + "int x; \n", "trailing-whitespace", 5)

    def test_missing_final_newline_fails(self):
        self.assertIssue("src/Sample.cpp", CPP_HEADER + "int x;", "final-newline", 5)

    def test_byte_order_mark_fails(self):
        self.assertIssue("src/Sample.cpp", b"\xef\xbb\xbf" + CPP_HEADER.encode("utf-8"), "encoding", 1)

    def test_empty_gitkeep_is_allowed(self):
        self.assertClean("src/common/Asio/.gitkeep", "")


if __name__ == "__main__":
    unittest.main(verbosity=1)

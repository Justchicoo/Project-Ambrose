#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Self-tests for the front-end checks and the npm cache priming: what counts as a colour a component wrote, an arbitrary value, an inline style, a component with no story and a bundle that names another host.
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ci_frontend_checks
import ci_npm_cache

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class FakeTree(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root, True)
        self.write("apps/ci/ci_frontend_allow.json", json.dumps({"lines": [], "urls": ["https://www.w3.org/"]}))
        for name in ("cormorant-garamond", "karla", "jetbrains-mono"):
            suffix = "latin-wght-normal.woff2"
            self.write(f"packages/ui/src/fonts/{name}-{suffix}", "font")

    def write(self, relpath, text):
        path = os.path.join(self.root, relpath.replace("/", os.sep))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        return path

    def component(self, name, body):
        self.write(f"packages/ui/src/components/{name}.svelte", body)
        self.write(f"packages/ui/src/components/{name}.stories.svelte", "story")

    def problems(self):
        allow = ci_frontend_checks.load_allow(self.root)
        found = ci_frontend_checks.check_source(self.root, allow)
        found += ci_frontend_checks.check_stories(self.root)
        found += ci_frontend_checks.check_fonts(self.root)
        bundle, _built = ci_frontend_checks.check_bundles(self.root, allow)
        return found + bundle


class SourceTests(FakeTree):
    def test_a_clean_component_passes(self):
        self.component("Good", '<div class="bg-surface-card p-16 text-fg-body"></div>\n')
        self.assertEqual(self.problems(), [])

    def test_a_hex_colour_fails(self):
        self.component("Bad", '<div style="background: var(--x)" data-x="#E4B457"></div>\n')
        self.assertTrue(any("a colour is written here" in problem for problem in self.problems()))

    def test_a_colour_function_fails(self):
        self.component("Bad", "<div></div>\n<style>\n.x { color: rgba(1, 2, 3, 0.5); }\n</style>\n")
        self.assertTrue(any("a colour is written here" in problem for problem in self.problems()))

    def test_an_anchor_that_only_looks_like_a_colour_passes(self):
        self.component("Good", '<a href="#overview">Overview</a>\n')
        self.assertEqual(self.problems(), [])

    def test_an_arbitrary_colour_value_fails(self):
        self.component("Bad", '<div class="bg-[var(--anything)]"></div>\n')
        self.assertTrue(any("arbitrary value" in problem for problem in self.problems()))

    def test_an_arbitrary_spacing_value_fails(self):
        self.component("Bad", '<div class="p-[13px]"></div>\n')
        self.assertTrue(any("arbitrary value" in problem for problem in self.problems()))

    def test_an_inline_style_carrying_a_colour_fails(self):
        self.component("Bad", '<div style="color: var(--color-fg-body)"></div>\n')
        self.assertTrue(any("inline style carries a colour" in problem for problem in self.problems()))

    def test_an_inline_style_carrying_a_size_passes(self):
        self.component("Good", '<div style="max-width: var(--ambrose-size-dialog)"></div>\n')
        self.assertEqual(self.problems(), [])

    def test_the_generated_token_files_are_allowed_to_hold_colours(self):
        self.write("packages/ui/src/tokens/tokens.css", ":root { --ambrose-color-action: #E4B457; }\n")
        self.assertEqual(self.problems(), [])

    def test_an_exception_named_in_the_allow_file_passes(self):
        self.component("Bad", '<div data-x="#E4B457"></div>\n')
        self.write(
            "apps/ci/ci_frontend_allow.json",
            json.dumps({"lines": [{"path": "packages/ui/src/components/Bad.svelte", "contains": "#E4B457"}], "urls": []}),
        )
        self.assertEqual(self.problems(), [])


class StoryTests(FakeTree):
    def test_a_component_with_no_story_fails(self):
        self.write("packages/ui/src/components/Lonely.svelte", "<div></div>\n")
        self.assertTrue(any("has no story" in problem for problem in self.problems()))

    def test_the_canary_needs_no_story_of_its_own(self):
        self.write("packages/ui/src/canary/Canary.stories.svelte", "<div></div>\n")
        self.assertEqual(self.problems(), [])


class BundleTests(FakeTree):
    def test_a_bundle_that_fetches_from_another_host_fails(self):
        self.write("apps/dashboard/dist/assets/index.css", '@import "https://fonts.example/one.css";\n')
        self.assertTrue(any("fetches from another host" in problem for problem in self.problems()))

    def test_a_bundle_that_only_names_an_allowed_address_passes(self):
        self.write("apps/dashboard/dist/assets/index.js", 'const ns = "https://www.w3.org/2000/svg";\n')
        self.assertEqual(self.problems(), [])

    def test_a_bundle_that_names_any_other_address_fails(self):
        self.write("apps/dashboard/dist/assets/index.js", 'const wrong = "https://cdn.example/uplot.js";\n')
        self.assertTrue(any("which is not one of the addresses" in problem for problem in self.problems()))


class FontTests(FakeTree):
    def test_a_missing_font_fails(self):
        os.remove(os.path.join(self.root, "packages", "ui", "src", "fonts", "karla-latin-wght-normal.woff2"))
        self.assertTrue(any("is missing" in problem for problem in self.problems()))

    def test_a_font_that_no_longer_matches_its_package_fails(self):
        self.write("node_modules/@fontsource-variable/karla/files/karla-latin-wght-normal.woff2", "different")
        self.assertTrue(any("no longer matches" in problem for problem in self.problems()))


class CacheTests(unittest.TestCase):
    def test_every_package_the_lockfile_names_is_listed_once(self):
        specs = ci_npm_cache.wanted(ROOT)
        self.assertGreater(len(specs), 100)
        for spec, resolved in specs.items():
            self.assertIn("@", spec[1:])
            self.assertTrue(resolved.startswith("http"))

    def test_the_repository_passes_its_own_front_end_checks(self):
        self.assertEqual(ci_frontend_checks.main(["--root", ROOT]), 0)


if __name__ == "__main__":
    unittest.main(verbosity=1)

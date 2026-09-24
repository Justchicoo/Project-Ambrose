#!/usr/bin/env python3
# Project Ambrose by Imjustchico
# Self-tests for the design token generator: the colour maths, the contrast and colour-blindness gates, every generated file, the doc/DESIGN.md tables and the check mode.
import copy
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import designtokens

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def load():
    with open(os.path.join(ROOT, designtokens.TOKENS_PATH), "r", encoding="utf-8") as handle:
        return json.load(handle)


class ColorMathTests(unittest.TestCase):
    def test_white_on_black_is_the_highest_ratio(self):
        self.assertAlmostEqual(designtokens.contrast("#FFFFFF", "#000000"), 21.0, places=4)

    def test_a_colour_against_itself_is_one(self):
        self.assertAlmostEqual(designtokens.contrast("#E4B457", "#E4B457"), 1.0, places=6)

    def test_the_documented_gold_on_ground_ratio_is_reproduced(self):
        self.assertAlmostEqual(designtokens.contrast("#E4B457", "#0B1020"), 9.89, places=2)

    def test_a_short_or_unprefixed_value_is_refused(self):
        for value in ("#FFF", "FFFFFF", "#GGGGGG", ""):
            with self.assertRaises(designtokens.GenerationRefused):
                designtokens.parse_hex(value)

    def test_the_nearest_terminal_index_finds_an_exact_match(self):
        self.assertEqual(designtokens.nearest("#FF0000", designtokens.ANSI_16), 9)
        self.assertEqual(designtokens.nearest("#000000", designtokens.XTERM_256), 0)

    def test_the_two_hundred_and_fifty_six_palette_has_every_entry(self):
        self.assertEqual(len(designtokens.XTERM_256), 256)

    def test_a_colour_simulated_for_a_dichromat_stays_a_colour(self):
        for kind in designtokens.DALTON:
            value = designtokens.simulate("#C9503F", kind)
            self.assertEqual(len(value), 7)
            designtokens.parse_hex(value)

    def test_two_identical_colours_have_no_separation(self):
        self.assertAlmostEqual(designtokens.separation("#C9503F", "#C9503F"), 0.0, places=6)


class TokenTests(unittest.TestCase):
    def setUp(self):
        self.tokens = designtokens.Tokens(load())

    def test_a_reference_resolves_to_its_primitive(self):
        self.assertEqual(self.tokens.resolve("semantic.color.surface-page", "dark"), "#0B1020")

    def test_the_light_theme_is_a_remap_of_the_same_name(self):
        self.assertEqual(self.tokens.resolve("semantic.color.surface-page", "light"), "#F4EAD5")

    def test_a_filled_accent_keeps_its_value_in_both_themes(self):
        for theme in designtokens.THEMES:
            self.assertEqual(self.tokens.resolve("component.color.fill-action", theme), "#E4B457")

    def test_an_unknown_token_is_refused(self):
        with self.assertRaises(designtokens.GenerationRefused):
            self.tokens.resolve("semantic.color.surface-moon")

    def test_a_reference_loop_is_refused(self):
        document = load()
        document["semantic"]["color"]["surface-page"]["$value"] = "{semantic.color.surface-card}"
        document["semantic"]["color"]["surface-card"]["$value"] = "{semantic.color.surface-page}"
        with self.assertRaises(designtokens.GenerationRefused):
            designtokens.Tokens(document).resolve("semantic.color.surface-page")

    def test_every_semantic_token_names_one_of_the_sixteen_terminal_colours(self):
        for name in self.tokens.leaf_names("semantic.color"):
            for theme in designtokens.THEMES:
                index = self.tokens.ansi16(f"semantic.color.{name}", theme)
                self.assertTrue(0 <= index <= 15)

    def test_a_terminal_colour_that_is_not_one_of_the_sixteen_is_refused(self):
        document = load()
        document["semantic"]["color"]["action"]["$extensions"]["ambrose.ansi16"] = "chartreuse"
        with self.assertRaises(designtokens.GenerationRefused):
            designtokens.Tokens(document).ansi16("semantic.color.action", "dark")


class ContrastGateTests(unittest.TestCase):
    def test_the_shipped_palette_passes_every_documented_pair(self):
        pairs = designtokens.check_contrast(designtokens.Tokens(load()))
        self.assertGreater(len(pairs), 50)
        for pair in pairs:
            self.assertGreaterEqual(pair["ratio"], pair["minimum"])

    def test_lowering_a_text_token_refuses_the_run_and_names_the_pair(self):
        document = load()
        document["primitive"]["color"]["text"]["$value"] = "#1D2440"
        with self.assertRaises(designtokens.GenerationRefused) as refusal:
            designtokens.check_contrast(designtokens.Tokens(document))
        message = str(refusal.exception)
        self.assertIn("semantic.color.fg-body", message)
        self.assertIn("semantic.color.surface-page", message)
        self.assertIn(":1", message)
        self.assertIn("4.5", message)

    def test_a_light_accent_that_cannot_be_read_on_parchment_refuses_the_run(self):
        document = load()
        document["semantic"]["color"]["state-healthy"]["$extensions"]["ambrose.light"] = "{primitive.color.teal}"
        with self.assertRaises(designtokens.GenerationRefused) as refusal:
            designtokens.check_contrast(designtokens.Tokens(document))
        self.assertIn("light:", str(refusal.exception))
        self.assertIn("state-healthy", str(refusal.exception))

    def test_a_focus_ring_under_three_to_one_refuses_the_run(self):
        document = load()
        document["contrast"]["rules"] = [rule for rule in document["contrast"]["rules"] if "focus ring" in rule["note"]]
        document["semantic"]["color"]["focus-ring"]["$value"] = "#101733"
        with self.assertRaises(designtokens.GenerationRefused) as refusal:
            designtokens.check_contrast(designtokens.Tokens(document))
        self.assertIn("3.0", str(refusal.exception))

    def test_two_series_a_dichromat_cannot_tell_apart_refuse_the_run(self):
        document = load()
        document["series"]["color"]["5"]["$value"] = document["series"]["color"]["1"]["$value"]
        with self.assertRaises(designtokens.GenerationRefused) as refusal:
            designtokens.check_contrast(designtokens.Tokens(document))
        self.assertIn("colour-blind", str(refusal.exception))
        self.assertIn("series.color.5", str(refusal.exception))

    def test_a_series_too_pale_for_the_dark_page_refuses_the_run(self):
        document = load()
        document["series"]["color"]["1"]["$value"] = "#101425"
        with self.assertRaises(designtokens.GenerationRefused) as refusal:
            designtokens.check_contrast(designtokens.Tokens(document))
        self.assertIn("series.color.1", str(refusal.exception))


class GeneratedFileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs = designtokens.generate(ROOT)
        cls.tokens = designtokens.Tokens(load())

    def test_every_generated_file_is_written(self):
        self.assertEqual(sorted(self.outputs), sorted(
            [designtokens.CSS_PATH, designtokens.VARIABLES_PATH, designtokens.TS_PATH, designtokens.HEADER_PATH, designtokens.DESIGN_PATH]))

    def test_the_variables_sheet_carries_the_palette_without_touching_tailwinds_scales(self):
        variables = self.outputs[designtokens.VARIABLES_PATH]
        self.assertNotIn("@theme", variables)
        self.assertNotIn("initial;", variables)
        self.assertIn("--ambrose-color-surface-page:", variables)
        self.assertIn("--ambrose-font-mono:", variables)
        self.assertIn(':root[data-theme="light"]', variables)
        self.assertIn(variables[variables.index(":root {"):], self.outputs[designtokens.CSS_PATH])

    def test_the_stylesheet_deletes_the_stock_palette_before_it_writes_ours(self):
        css = self.outputs[designtokens.CSS_PATH]
        self.assertIn("--color-*: initial;", css)
        self.assertIn("--spacing-*: initial;", css)
        self.assertLess(css.index("--color-*: initial;"), css.index("--color-surface-page:"))

    def test_the_stylesheet_carries_every_semantic_token_in_both_themes(self):
        css = self.outputs[designtokens.CSS_PATH]
        for name in self.tokens.leaf_names("semantic.color"):
            self.assertIn(f"--color-{name}: var(--ambrose-color-{name});", css)
            for theme in designtokens.THEMES:
                self.assertIn(f"--ambrose-color-{name}: {self.tokens.resolve('semantic.color.' + name, theme)};", css)

    def test_reduced_motion_collapses_every_duration_to_zero(self):
        css = self.outputs[designtokens.CSS_PATH]
        self.assertIn("@media (prefers-reduced-motion: reduce) {", css)
        for name in self.tokens.leaf_names("primitive.duration"):
            self.assertIn(f"--ambrose-duration-{name}: 0ms;", css)

    def test_the_header_guards_itself_and_holds_every_token(self):
        header = self.outputs[designtokens.HEADER_PATH]
        self.assertIn("#ifndef AMBROSE_TOKENS_H", header)
        self.assertIn("#define AMBROSE_TOKENS_H", header)
        self.assertTrue(header.rstrip().endswith("#endif"))
        for name in self.tokens.leaf_names("semantic.color"):
            self.assertIn(f'"{name}"', header)

    def test_the_header_and_the_stylesheet_carry_the_same_value_for_every_token(self):
        header = self.outputs[designtokens.HEADER_PATH]
        css = self.outputs[designtokens.CSS_PATH]
        for theme in designtokens.THEMES:
            for name, value in self.tokens.colors("semantic.color", theme):
                self.assertIn(f'"{name}", "{value}"', header)
                self.assertIn(f"--ambrose-color-{name}: {value};", css)

    def test_the_typescript_module_exports_every_token_and_its_contrast(self):
        module = self.outputs[designtokens.TS_PATH]
        self.assertIn("export const semanticColors = {", module)
        self.assertIn("export type SemanticColorName", module)
        self.assertIn("export const contrastPairs", module)
        for name in self.tokens.leaf_names("semantic.color"):
            self.assertIn(f'"{name}"', module)

    def test_the_durations_agree_across_the_three_outputs(self):
        for name in self.tokens.leaf_names("primitive.duration"):
            value = self.tokens.resolve("primitive.duration." + name)
            self.assertIn(f"--ambrose-duration-{name}: {value};", self.outputs[designtokens.CSS_PATH])
            self.assertIn(f'"{name}": {value.replace("ms", "")},', self.outputs[designtokens.TS_PATH])
            self.assertIn(f"Duration{designtokens.identifier(name)}Ms = {value.replace('ms', '')};", self.outputs[designtokens.HEADER_PATH])

    def test_no_generated_file_leaves_the_ascii_range(self):
        for text in self.outputs.values():
            text.encode("ascii")

    def test_the_document_tables_are_written_and_the_prose_is_left_alone(self):
        design = self.outputs[designtokens.DESIGN_PATH]
        self.assertIn("| `surface-page` | `#0B1020` | `#F4EAD5` |", design)
        self.assertIn("| `ground` | `#0B1020` |", design)
        self.assertIn("The names above are the raw values.", design)


class TableTests(unittest.TestCase):
    def test_an_existing_table_is_replaced_and_its_neighbours_are_kept(self):
        text = "\n".join(["### The palette", "", "| Token |", "|---|", "| `old` |", "", "After."])
        result = designtokens.replace_tables(text, {"### The palette": ["| Token |", "|---|", "| `new` |"]})
        self.assertIn("| `new` |", result)
        self.assertNotIn("| `old` |", result)
        self.assertTrue(result.endswith("After."))

    def test_a_missing_heading_refuses_the_run(self):
        with self.assertRaises(designtokens.GenerationRefused):
            designtokens.replace_tables("# Design\n", {"### The palette": ["| Token |"]})


class CommandTests(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root, True)
        for relative in (designtokens.TOKENS_PATH, designtokens.DESIGN_PATH):
            target = os.path.join(self.root, relative.replace("/", os.sep))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copyfile(os.path.join(ROOT, relative), target)

    def path(self, relative):
        return os.path.join(self.root, relative.replace("/", os.sep))

    def test_a_first_run_writes_every_file_and_a_second_run_changes_nothing(self):
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        self.assertEqual(designtokens.main(["--root", self.root, "--check"]), 0)
        self.assertTrue(os.path.exists(self.path(designtokens.HEADER_PATH)))

    def test_a_hand_edited_generated_file_fails_the_check(self):
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        with open(self.path(designtokens.CSS_PATH), "a", encoding="utf-8") as handle:
            handle.write("\n.drift { color: #FF0000; }\n")
        self.assertEqual(designtokens.main(["--root", self.root, "--check"]), 1)

    def test_a_hand_edited_document_table_fails_the_check(self):
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        with open(self.path(designtokens.DESIGN_PATH), "r", encoding="utf-8") as handle:
            text = handle.read()
        with open(self.path(designtokens.DESIGN_PATH), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text.replace("| `ground` | `#0B1020` |", "| `ground` | `#000000` |"))
        self.assertEqual(designtokens.main(["--root", self.root, "--check"]), 1)

    def read(self, relative):
        with open(self.path(relative), "r", encoding="utf-8") as handle:
            return handle.read()

    def retune(self, name, value):
        with open(self.path(designtokens.TOKENS_PATH), "r", encoding="utf-8") as handle:
            document = json.load(handle)
        document["primitive"]["color"][name]["$value"] = value
        with open(self.path(designtokens.TOKENS_PATH), "w", encoding="utf-8", newline="\n") as handle:
            json.dump(document, handle)

    def test_a_lowered_token_refuses_both_modes_and_writes_nothing(self):
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        before = self.read(designtokens.CSS_PATH)
        self.retune("teal", "#10382F")
        self.assertEqual(designtokens.main(["--root", self.root]), 2)
        self.assertEqual(designtokens.main(["--root", self.root, "--check"]), 2)
        self.assertEqual(self.read(designtokens.CSS_PATH), before)

    def test_changing_one_value_changes_every_generated_file_together(self):
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        names = (designtokens.CSS_PATH, designtokens.TS_PATH, designtokens.HEADER_PATH, designtokens.DESIGN_PATH)
        before = {name: self.read(name) for name in names}
        self.retune("teal", "#6FE0D0")
        self.assertEqual(designtokens.main(["--root", self.root]), 0)
        for name, text in before.items():
            after = self.read(name)
            self.assertNotEqual(after, text, name)
            self.assertIn("#6FE0D0", after, name)


class RepositoryTests(unittest.TestCase):
    def test_the_committed_files_match_the_token_source(self):
        self.assertEqual(designtokens.main(["--root", ROOT, "--check"]), 0)


if __name__ == "__main__":
    unittest.main(verbosity=1)

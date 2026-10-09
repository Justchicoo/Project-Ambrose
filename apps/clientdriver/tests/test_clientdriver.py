# Project Ambrose by Imjustchico
# Self-tests for every part of the client driver that has no client in it: the log tailer against recorded fixtures, the scenario loader with its includes, variables and patterns and the wizard a scenario seeds for the game server and the companion client that shows a second wizard, the scratch game server's settings, the WSL distribution a run holds while its database lives there, the zone rows' cache and the copy of a wizard from another database, the reference file, the screen matcher on synthetic frames, the step engine against a fake client and a fake server, the order in which a run starts and stops what it owns, the guard's rule for which processes are its own, the capture that ends what it started, the teardown that decides from the client's own log whether it may be asked to quit, the crop rebuild that refuses a picture of the wrong screen, the report builder against recorded logs, and the check that decides whether a machine can run a scenario, and the ports a scenario watches, the launcher command run without its patch flag and the report's checks for both, and the launcher window a scenario opens, read and pressed through a fake of UI Automation, and the window messages a click and a key send, through fakes of the Windows calls, and the play session's start order, its stop from the console, from another play or from a server that ends, and the databases and ports it keeps apart from a run's.
import json
import os
import re
import socket
import sys
import tempfile
import time
import unittest
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from clientdriver import capture, cli, client, database, engine, install, listeners, netguard, paths, play, preflight, references, refscapture, report, run, scenario, screens, server, zones
from clientdriver.errors import Refused, StepFailed
from clientdriver.logtail import LogTail, read_lines

FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")
REFERENCE_DOCUMENT = {
    "key": {"revision": "r806919.Wizard_1_610", "window": "40x20", "ui_scale": "install default"},
    "match": {"tolerance": 32, "fraction": 0.65},
    "screens": {
        "login": {"crop": [0, 0, 10, 10], "shows": "the left half of the sample frame"},
        "charselect": {"crop": [10, 0, 20, 10], "shows": "the right half of the sample frame"},
    },
    "targets": {"press": {"at": [5, 5], "presses": "the sample button"}},
}
RED = (200, 30, 30)
GREEN = (30, 200, 30)
BLUE = (30, 30, 200)


def fixture(name="logs.json"):
    with open(os.path.join(FIXTURES, name), "r", encoding="utf-8") as handle:
        return json.load(handle)


def frame_of(left, right, width=40, height=20):
    data = bytearray()
    for _y in range(height):
        for x in range(width):
            data += bytes(left if x < 10 else right)
    return screens.Bitmap(width, height, bytes(data))


class TemporaryFolder(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="clientdriver-test-")
        self.addCleanup(folder.cleanup)
        self.folder = folder.name

    def write(self, name, lines, encoding="utf-8"):
        path = os.path.join(self.folder, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "a", encoding=encoding, newline="\n") as handle:
            for line in lines:
                handle.write(line + "\n")
        return path

    def write_json(self, name, document):
        path = os.path.join(self.folder, name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(document, handle)
        return path


class LogTailTests(TemporaryFolder):
    def test_it_reads_the_lines_that_arrive_after_it_started(self):
        recorded = fixture()["clean"]["server"]
        path = self.write("Login.log", recorded[:2])
        tail = LogTail(path, interval=0.01)
        self.assertEqual(len(tail.poll()), 2)
        self.write("Login.log", recorded[2:4])
        self.assertEqual(tail.poll(), recorded[2:4])
        self.assertEqual(len(tail.lines), 4)

    def test_a_wait_only_sees_what_arrives_after_the_last_match(self):
        recorded = fixture()["clean"]["client"]
        path = self.write("WizardClient.log", recorded, encoding="latin-1")
        tail = LogTail(path, encoding="latin-1", interval=0.01)
        first = tail.wait(r"LOGIN RESPONSE: Error=(-?\d+)", 1)
        second = tail.wait(r"LOGIN RESPONSE: Error=(-?\d+)", 1)
        self.assertEqual(first.group(1), "996708736")
        self.assertEqual(second.group(1), "0")

    def test_a_wait_from_the_start_leaves_the_cursor_alone(self):
        path = self.write("Login.log", fixture()["clean"]["server"])
        tail = LogTail(path, interval=0.01)
        tail.wait(r"loginserver ready", 1)
        cursor = tail.cursor
        tail.wait(r"An AI-built Wizard101 server", 1, since=0, advance=False)
        self.assertEqual(tail.cursor, cursor)

    def test_a_wait_from_the_start_sees_only_what_the_latest_start_wrote(self):
        path = self.write("Game.log", ["Extracted 1267 level rows", "stopped"])
        tail = LogTail(path, interval=0.01)
        tail.begin()
        self.write("Game.log", ["levels came from r1.Older", "Extracted 1268 level rows"])
        tail.wait(r"came from r1\.Older", 1, since=tail.start, advance=False)
        found = tail.wait(r"Extracted (\d+) level rows", 1, since=tail.start, advance=False)
        self.assertEqual(found.group(1), "1268")

    def test_a_timeout_names_the_pattern_and_the_file(self):
        path = self.write("Login.log", ["2026-09-17_15:52:15.900 INFO  [server.loginserver] loginserver ready"])
        tail = LogTail(path, interval=0.01)
        with self.assertRaises(StepFailed) as raised:
            tail.wait(r"the wizard has reached Ravenwood", 0.05)
        self.assertIn("the wizard has reached Ravenwood", str(raised.exception))
        self.assertIn("Login.log", str(raised.exception))

    def test_a_forbidden_line_fails_the_wait_with_the_line(self):
        path = self.write("Login.log", ["2026-09-17_15:52:15.900 FATAL [server.loginserver] the port is already in use"])
        tail = LogTail(path, interval=0.01)
        with self.assertRaises(StepFailed) as raised:
            tail.wait(r"loginserver ready", 1, fail=r"\bFATAL\b")
        self.assertIn("the port is already in use", str(raised.exception))

    def test_a_dead_writer_ends_the_wait(self):
        path = self.write("Login.log", [])
        tail = LogTail(path, interval=0.01)
        with self.assertRaises(StepFailed) as raised:
            tail.wait(r"loginserver ready", 5, alive=lambda: False)
        self.assertIn("nothing is writing", str(raised.exception))

    def test_half_a_line_waits_for_the_rest_of_it(self):
        path = os.path.join(self.folder, "Login.log")
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("2026-09-17_15:52:15.900 INFO  [server.loginserver] loginser")
        tail = LogTail(path, interval=0.01)
        self.assertEqual(tail.poll(), [])
        with open(path, "a", encoding="utf-8", newline="\n") as handle:
            handle.write("ver ready\n")
        self.assertEqual(len(tail.poll()), 1)
        self.assertTrue(tail.lines[0].endswith("loginserver ready"))

    def test_a_log_that_starts_again_is_read_from_its_beginning_and_keeps_what_it_read(self):
        path = self.write("Login.log", ["one", "two", "three"])
        tail = LogTail(path, interval=0.01)
        tail.poll()
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("four\n")
        self.assertEqual(tail.poll(), ["four"])
        self.assertEqual(tail.lines, ["one", "two", "three", "four"])

    def test_a_log_replaced_by_a_longer_one_is_read_from_its_beginning(self):
        path = self.write("Login.log", ["one"])
        tail = LogTail(path, interval=0.01)
        tail.poll()
        os.remove(path)
        self.write("Login.log", ["two", "three"])
        self.assertEqual(tail.poll(), ["two", "three"])
        self.assertEqual(tail.lines, ["one", "two", "three"])

    def test_a_line_written_just_before_the_writer_died_still_satisfies_a_wait(self):
        path = self.write("Login.log", [])
        tail = LogTail(path, interval=0.01)
        alive = []

        def writer_is_alive():
            self.write("Login.log", ["2026 INFO [x] Mainloop exited with return code 0"])
            alive.append(False)
            return False

        found = tail.wait(r"Mainloop exited", 1, alive=writer_is_alive)
        self.assertIn("Mainloop exited", found.string)
        self.assertEqual(len(alive), 1)

    def test_bytes_that_are_not_utf8_do_not_stop_the_reader(self):
        path = os.path.join(self.folder, "WizardClient.log")
        with open(path, "wb") as handle:
            handle.write(b"09/17/26 [ERRO] a byte \xff and the rest\n")
        tail = LogTail(path, encoding="latin-1", interval=0.01)
        self.assertEqual(len(tail.poll()), 1)
        self.assertIn("and the rest", tail.lines[0])

    def test_a_log_that_is_not_there_yet_reads_as_empty(self):
        tail = LogTail(os.path.join(self.folder, "not-written-yet.log"), interval=0.01)
        self.assertEqual(tail.poll(), [])

    def test_matching_lists_every_line_from_the_start(self):
        path = self.write("Login.log", fixture()["clean"]["server"])
        tail = LogTail(path, interval=0.01)
        self.assertEqual(len(tail.matching(r"does not handle yet", since=0)), 2)

    def test_read_lines_reads_a_whole_file(self):
        path = self.write("Login.log", fixture()["clean"]["server"])
        self.assertEqual(len(read_lines(path)), len(fixture()["clean"]["server"]) + 1)


class ScenarioTests(TemporaryFolder):
    def scenario_file(self, name, document):
        return self.write_json(os.path.join("scenarios", name), document)

    def test_a_scenario_loads_with_its_steps(self):
        self.scenario_file("one.json", {"title": "one", "steps": [
            {"action": "wait_server_log", "name": "ready", "pattern": "ready", "timeout": 1}]})
        loaded = scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertEqual(loaded.title, "one")
        self.assertEqual(len(loaded.steps), 1)
        self.assertTrue(loaded.needs_client)

    def test_an_include_runs_first_and_merges_its_settings(self):
        self.scenario_file("base.json", {"title": "base", "server_settings": ["Logger.server=2,Console"],
                                         "server_log_allowed": ["spells TYPE"], "variables": {"who": "base"},
                                         "steps": [{"action": "wait_server_log", "name": "first", "pattern": "a", "timeout": 1}]})
        self.scenario_file("more.json", {"title": "more", "include": "base.json", "variables": {"who": "more"},
                                         "pending_allowed": ["MSG_CREATECHARACTER"],
                                         "steps": [{"action": "wait_server_log", "name": "second", "pattern": "b", "timeout": 1}]})
        loaded = scenario.load("more.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertEqual([step["name"] for step in loaded.steps], ["first", "second"])
        self.assertEqual(loaded.server_settings, ["Logger.server=2,Console"])
        self.assertEqual(loaded.server_log_allowed, ["spells TYPE"])
        self.assertEqual(loaded.pending_allowed, ["MSG_CREATECHARACTER"])
        self.assertEqual(loaded.variables["who"], "more")
        self.assertEqual(loaded.title, "more")

    def test_an_include_in_a_folder_of_its_own_is_found_beside_the_scenario(self):
        self.scenario_file(os.path.join("parts", "common.json"), {"title": "common", "server_settings": ["A=1"]})
        self.scenario_file("one.json", {"title": "one", "include": "parts/common.json",
                                        "steps": [{"action": "shot", "name": "a shot"}]})
        loaded = scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertEqual(loaded.server_settings, ["A=1"])

    def test_scenarios_that_include_each_other_are_refused(self):
        self.scenario_file("a.json", {"title": "a", "include": "b.json"})
        self.scenario_file("b.json", {"title": "b", "include": "a.json"})
        with self.assertRaises(Refused) as raised:
            scenario.load("a.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("include each other", str(raised.exception))

    def test_an_unknown_action_is_refused_with_the_actions_there_are(self):
        self.scenario_file("one.json", {"title": "one", "steps": [{"action": "dance", "name": "dance"}]})
        with self.assertRaises(Refused) as raised:
            scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("wait_server_log", str(raised.exception))

    def test_a_missing_key_and_an_unknown_key_are_both_refused(self):
        self.scenario_file("missing.json", {"title": "x", "steps": [{"action": "wait_screen", "screens": ["login"]}]})
        with self.assertRaises(Refused):
            scenario.load("missing.json", search=(os.path.join(self.folder, "scenarios"),))
        self.scenario_file("extra.json", {"title": "x", "steps": [
            {"action": "type", "text": "a", "screens": ["login"]}]})
        with self.assertRaises(Refused) as raised:
            scenario.load("extra.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("'screens'", str(raised.exception))

    def test_a_held_key_needs_a_time_it_can_be_held_for(self):
        self.scenario_file("good.json", {"title": "x", "steps": [{"action": "hold_key", "name": "walk", "vk": "0x57", "seconds": 1.5, "moves": True}]})
        scenario.load("good.json", search=(os.path.join(self.folder, "scenarios"),))
        for seconds in (0, 31, "long"):
            self.scenario_file("bad.json", {"title": "x", "steps": [{"action": "hold_key", "name": "walk", "vk": "0x57", "seconds": seconds}]})
            with self.assertRaises(Refused) as raised:
                scenario.load("bad.json", search=(os.path.join(self.folder, "scenarios"),))
            self.assertIn("seconds", str(raised.exception))
        self.scenario_file("none.json", {"title": "x", "steps": [{"action": "hold_key", "name": "walk", "vk": "0x57"}]})
        with self.assertRaises(Refused):
            scenario.load("none.json", search=(os.path.join(self.folder, "scenarios"),))

    def test_a_press_carries_the_check_that_it_took_and_that_check_is_checked_too(self):
        self.scenario_file("good.json", {"title": "x", "steps": [
            {"action": "click", "name": "press", "target": "press",
             "until": {"action": "wait_screen", "screens": ["login"], "timeout": 2}}]})
        scenario.load("good.json", search=(os.path.join(self.folder, "scenarios"),))
        self.scenario_file("bad.json", {"title": "x", "steps": [
            {"action": "click", "name": "press", "target": "press", "until": {"action": "wait_screen", "screens": ["login"]}}]})
        with self.assertRaises(Refused):
            scenario.load("bad.json", search=(os.path.join(self.folder, "scenarios"),))

    def test_a_pattern_that_does_not_compile_is_refused_before_anything_starts(self):
        self.scenario_file("allow.json", {"title": "x", "server_log_allowed": ["MSG_PHYSICS_GRAB(Force"], "steps": []})
        with self.assertRaises(Refused) as raised:
            scenario.load("allow.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("is not a pattern", str(raised.exception))
        self.scenario_file("step.json", {"title": "x", "steps": [
            {"action": "wait_server_log", "name": "ready", "pattern": "ready(", "timeout": 1}]})
        with self.assertRaises(Refused):
            scenario.load("step.json", search=(os.path.join(self.folder, "scenarios"),))
        self.scenario_file("fail.json", {"title": "x", "steps": [
            {"action": "wait_server_log", "name": "ready", "pattern": "ready", "fail": "[", "timeout": 1}]})
        with self.assertRaises(Refused):
            scenario.load("fail.json", search=(os.path.join(self.folder, "scenarios"),))

    def test_every_allow_list_merges_with_the_one_it_includes(self):
        self.scenario_file("base.json", {"title": "base", "pending_allowed": ["MSG_ONE"], "dropped_allowed": ["MSG_TWO"],
                                         "client_log_allowed": ["a known client line"], "steps": []})
        self.scenario_file("more.json", {"title": "more", "include": "base.json", "pending_allowed": ["MSG_THREE"],
                                         "steps": []})
        loaded = scenario.load("more.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertEqual(loaded.pending_allowed, ["MSG_ONE", "MSG_THREE"])
        self.assertEqual(loaded.dropped_allowed, ["MSG_TWO"])
        self.assertEqual(loaded.client_log_allowed, ["a known client line"])

    def test_two_steps_with_the_same_name_are_refused(self):
        self.scenario_file("one.json", {"title": "x", "steps": [
            {"action": "shot", "name": "the login window"}, {"action": "shot", "name": "the login window"}]})
        with self.assertRaises(Refused) as raised:
            scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("two steps named", str(raised.exception))

    def test_a_scenario_without_a_title_is_refused(self):
        self.scenario_file("one.json", {"steps": []})
        with self.assertRaises(Refused):
            scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))

    def test_a_scenario_that_is_not_there_names_where_it_was_looked_for(self):
        with self.assertRaises(Refused) as raised:
            scenario.load("nothing.json", search=(self.folder,))
        self.assertIn(self.folder, str(raised.exception))

    def test_variables_are_filled_and_an_unknown_one_is_named(self):
        self.assertEqual(scenario.fill("hello {user}", {"user": "clientdriver"}), "hello clientdriver")
        with self.assertRaises(Refused) as raised:
            scenario.fill("hello {nobody}", {"user": "clientdriver"})
        self.assertIn("{nobody}", str(raised.exception))
        self.assertEqual(scenario.variables_used({"a": ["{one}", {"b": "{two}"}]}), {"one", "two"})

    def test_the_screens_and_targets_a_scenario_uses_are_checked_against_the_references(self):
        self.scenario_file("one.json", {"title": "x", "steps": [
            {"action": "wait_screen", "name": "a", "screens": ["login", "nowhere"], "timeout": 1},
            {"action": "click", "name": "b", "target": "missing", "on_screen": "login"}]})
        loaded = scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertEqual(loaded.screens_used(), ["login", "nowhere"])
        self.assertEqual(loaded.targets_used(), ["missing"])
        problems = loaded.names_against(references.References("references.json", REFERENCE_DOCUMENT))
        self.assertEqual(len(problems), 2)

    def test_a_scenario_can_expect_to_fail(self):
        self.scenario_file("one.json", {"title": "x", "expect": "failure", "steps": []})
        self.assertTrue(scenario.load("one.json", search=(os.path.join(self.folder, "scenarios"),)).expect_failure)
        self.scenario_file("two.json", {"title": "x", "expect": "maybe", "steps": []})
        with self.assertRaises(Refused):
            scenario.load("two.json", search=(os.path.join(self.folder, "scenarios"),))

    def test_every_action_the_loader_knows_has_an_engine_that_runs_it(self):
        known = {name for name in scenario.ACTIONS}
        implemented = {name[4:] for name in dir(engine.Engine) if name.startswith("act_")}
        self.assertEqual(known, implemented)

    def test_the_scenarios_in_the_repository_load_and_name_only_screens_the_reference_file_describes(self):
        described = references.load(paths.REFERENCES)
        found = 0
        for name in sorted(os.listdir(paths.SCENARIOS)):
            if not name.endswith(".json"):
                continue
            loaded = scenario.load(name, search=(paths.SCENARIOS,))
            self.assertEqual(loaded.names_against(described), [])
            self.assertTrue(loaded.steps, f"{name} has no steps")
            found += 1
        self.assertGreaterEqual(found, 3)


class WorldEntryTests(TemporaryFolder):
    WIZARD = {"school": 2343174, "zone": "WizardCity/WC_Ravenwood", "first": 1, "middle": 1, "last": 1}

    def scenario_file(self, name, document):
        return self.write_json(os.path.join("scenarios", name), document)

    def test_a_scenario_that_seeds_a_wizard_needs_the_game_server_and_every_part_of_the_wizard(self):
        steps = [{"action": "wait_game_log", "name": "in", "pattern": "stands in the world", "timeout": 1}]
        self.scenario_file("alone.json", {"title": "alone", "wizard": self.WIZARD, "steps": steps})
        with self.assertRaises(Refused) as raised:
            scenario.load("alone.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("does not require the game server", str(raised.exception))
        self.scenario_file("partial.json", {"title": "partial", "requires": {"gameserver": True}, "wizard": {"school": 1}, "steps": steps})
        with self.assertRaises(Refused) as raised:
            scenario.load("partial.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertIn("the wizard needs zone, first, middle, last", str(raised.exception))
        self.scenario_file("whole.json", {"title": "whole", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                          "game_settings": ["Realm.Name=Test"], "steps": steps})
        loaded = scenario.load("whole.json", search=(os.path.join(self.folder, "scenarios"),))
        self.assertTrue(loaded.needs_gameserver)
        self.assertEqual(loaded.wizard["zone"], "WizardCity/WC_Ravenwood")
        self.assertEqual(loaded.game_settings, ["Realm.Name=Test"])

    def test_more_wizards_join_the_first_on_its_account_and_need_it(self):
        steps = [{"action": "wait_game_log", "name": "in", "pattern": "stands in the world", "timeout": 1}]
        search = (os.path.join(self.folder, "scenarios"),)
        second = dict(self.WIZARD, first=2)
        self.scenario_file("orphans.json", {"title": "orphans", "requires": {"gameserver": True}, "more_wizards": [second], "steps": steps})
        with self.assertRaises(Refused) as raised:
            scenario.load("orphans.json", search=search)
        self.assertIn("not the first wizard they join", str(raised.exception))
        self.scenario_file("broken.json", {"title": "broken", "requires": {"gameserver": True}, "wizard": self.WIZARD, "more_wizards": [{"school": 1}], "steps": steps})
        with self.assertRaises(Refused) as raised:
            scenario.load("broken.json", search=search)
        self.assertIn("wizard 2 needs zone, first, middle, last", str(raised.exception))
        self.scenario_file("three.json", {"title": "three", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                          "more_wizards": [second, dict(self.WIZARD, first=3)], "steps": steps})
        loaded = scenario.load("three.json", search=search)
        self.assertEqual([wizard["first"] for wizard in loaded.more_wizards], [2, 3])

    def test_a_seeded_wizard_may_carry_stats_it_has_and_none_it_does_not(self):
        steps = [{"action": "wait_game_log", "name": "in", "pattern": "stands in the world", "timeout": 1},
                 {"action": "shot", "name": "look", "settle": 2.5}]
        search = (os.path.join(self.folder, "scenarios"),)
        stats = {"gold": 1234, "health": 300, "mana": 10, "training_points": 1, "level_locked": 1, "potion_charge": 1.5, "purchased_custom_emotes_1": 1}
        self.scenario_file("stats.json", {"title": "stats", "requires": {"gameserver": True}, "wizard": dict(self.WIZARD, level=5, experience=900, stats=stats), "steps": steps})
        loaded = scenario.load("stats.json", search=search)
        self.assertEqual(loaded.wizard["stats"]["gold"], 1234)
        self.assertEqual(loaded.wizard["stats"]["purchased_custom_emotes_1"], 1)
        for bad, said in (({"copper": 3}, "which a wizard does not carry"), ({"gold": -1}, "must be a number of zero or more"), ({"gold": "lots"}, "must be a number"), ([1], "must be an object")):
            self.scenario_file("bad.json", {"title": "bad", "requires": {"gameserver": True}, "wizard": dict(self.WIZARD, stats=bad), "steps": steps})
            with self.assertRaises(Refused) as raised:
                scenario.load("bad.json", search=search)
            self.assertIn(said, str(raised.exception))
        for action, extra in (("shot", {}), ("hover", {"target": "charselect_play"})):
            for settle in (-1, 31, "2", True):
                self.scenario_file("settle.json", {"title": "settle", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                                    "steps": [dict({"action": action, "name": "look", "settle": settle}, **extra)]})
                with self.assertRaises(Refused) as raised:
                    scenario.load("settle.json", search=search)
                self.assertIn("settle for 0 to 30 seconds", str(raised.exception))

    def test_a_kept_name_and_a_restart_wait_are_checked_before_anything_starts(self):
        search = (os.path.join(self.folder, "scenarios"),)
        wait = {"action": "wait_game_log", "name": "in", "pattern": "stands in the world", "timeout": 1}
        for keep, said in ((1, "keeps what it matched"), ("two words", "keeps what it matched"), ("wizard_guid", "not one of the run's own")):
            self.scenario_file("keep.json", {"title": "keep", "requires": {"gameserver": True}, "wizard": self.WIZARD, "steps": [dict(wait, keep=keep)]})
            with self.assertRaises(Refused) as raised:
                scenario.load("keep.json", search=search)
            self.assertIn(said, str(raised.exception))
        for timeout in (0, 601, "5", True):
            self.scenario_file("restart.json", {"title": "restart", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                                 "steps": [{"action": "restart_client", "name": "again", "timeout": timeout}]})
            with self.assertRaises(Refused) as raised:
                scenario.load("restart.json", search=search)
            self.assertIn("at most 600 seconds", str(raised.exception))
        self.scenario_file("fine.json", {"title": "fine", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                          "steps": [dict(wait, keep="saved_place"), {"action": "restart_client", "name": "again", "timeout": 240}]})
        self.assertEqual(len(scenario.load("fine.json", search=search).steps), 2)
        walk = scenario.load("walk-and-return.json", search=(paths.SCENARIOS,))
        self.assertTrue(walk.needs_gameserver)
        self.assertIn("restart_client", [step["action"] for step in walk.steps])

    def test_a_companion_brings_a_wizard_of_its_own_and_only_its_steps_drive_it(self):
        search = (os.path.join(self.folder, "scenarios"),)
        step = {"action": "hold_key", "name": "walk", "vk": "0x57", "seconds": 1, "client": "companion"}
        self.scenario_file("two.json", {"title": "two", "requires": {"gameserver": True}, "wizard": self.WIZARD,
                                        "companion": {"wizard": self.WIZARD}, "steps": [step]})
        loaded = scenario.load("two.json", search=search)
        self.assertEqual(loaded.companion["wizard"]["zone"], "WizardCity/WC_Ravenwood")
        for document, said in (
                ({"steps": [step]}, "starts no companion"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [dict(step, client="third")]}, "a run drives main or companion"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [{"action": "wait_game_log", "name": "in", "pattern": "x", "timeout": 1, "client": "companion"}]},
                 "its wait_game_log action drives none"),
                ({"companion": {"wizard": {"school": 1}}, "steps": []}, "the companion's wizard needs zone"),
                ({"companion": {}, "steps": []}, "the companion needs exactly wizard"),
                ({"companion": {"wizard": self.WIZARD, "user": "x"}, "steps": []}, "the companion needs exactly wizard"),
                ({"steps": [dict(step, client="main", watch="companion")]}, "starts no companion"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [dict(step, watch="third")]}, "watches the client 'third'"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [dict(step, watch_every=1)]}, "not which client it watches"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [dict(step, watch="main", watch_every=0.01)]}, "every 0.1 to 5 seconds"),
                ({"companion": {"wizard": self.WIZARD}, "steps": [dict(step, watch="main", watch_after=11)]}, "for 0 to 10 seconds")):
            self.scenario_file("bad.json", dict({"title": "bad", "requires": {"gameserver": True}, "wizard": self.WIZARD}, **document))
            with self.assertRaises(Refused) as raised:
                scenario.load("bad.json", search=search)
            self.assertIn(said, str(raised.exception))
        self.scenario_file("alone.json", {"title": "alone", "companion": {"wizard": self.WIZARD}, "steps": []})
        with self.assertRaises(Refused) as raised:
            scenario.load("alone.json", search=search)
        self.assertIn("does not require the game server", str(raised.exception))
        self.scenario_file("more.json", {"title": "more", "include": "two.json", "steps": [dict(step, name="walk again")]})
        self.assertEqual(scenario.load("more.json", search=search).companion["wizard"]["first"], 1)

    def test_the_shipped_enter_world_scenario_loads_with_its_game_server_and_wizard(self):
        loaded = scenario.load("enter-world.json", search=(paths.SCENARIOS,))
        self.assertTrue(loaded.needs_gameserver)
        self.assertEqual(loaded.wizard["zone"], "WizardCity/WC_Ravenwood")
        self.assertIn("charselect_play", loaded.targets_used())

    def test_a_wsl_database_s_distribution_is_held_until_the_driver_exits(self):
        started, registered = [], []

        class Holder:
            def kill(self):
                pass

        def start(command, **_options):
            started.append(command)
            return Holder()

        holder = database.hold_wsl("Ubuntu-24.04", start=start, at_exit=registered.append, platform="win32")
        self.assertEqual(started, [["wsl.exe", "-d", "Ubuntu-24.04", "--exec", "sleep", "infinity"]])
        self.assertEqual(registered, [holder.kill])
        self.assertIsNone(database.hold_wsl(None, start=start, at_exit=registered.append, platform="win32"))
        self.assertIsNone(database.hold_wsl("Ubuntu-24.04", start=start, at_exit=registered.append, platform="linux"))
        self.assertEqual(len(started), 1)

    def test_a_held_distribution_that_cannot_start_fails_the_run_with_its_name(self):
        def start(_command, **_options):
            raise OSError("wsl.exe is not installed")

        with self.assertRaises(StepFailed) as raised:
            database.hold_wsl("Ubuntu-24.04", start=start, at_exit=lambda _kill: None, platform="win32")
        self.assertIn("Ubuntu-24.04", str(raised.exception))

    def test_the_game_server_announces_its_realm_at_the_run_s_own_address_and_port(self):
        scratch = database.Scratch("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose_driver_run")
        game = server.GameServer("gameserver.exe", "gameserver.conf.dist", os.path.join(self.folder, "game"), "127.0.0.2", 12433,
                                 scratch, settings=["Realm.Name=Driver"])
        overrides = game.overrides()
        for wanted in ("WorldServerPort=12433", "Realm.Address=127.0.0.2", "BindIP=127.0.0.2", "Admin.Enable=0", "Realm.Name=Driver",
                       f"WorldDatabaseInfo={scratch.info('world')}", f"LoginDatabaseInfo={scratch.info('login')}"):
            self.assertIn(wanted, overrides)
        self.assertNotIn("LoginServerPort=12433", overrides)
        self.assertTrue(game.config.endswith("gameserver.conf"))
        self.assertTrue(game.log.path.endswith("Server.log"))
        login = server.LoginServer("loginserver.exe", "loginserver.conf.dist", os.path.join(self.folder, "login"), "127.0.0.2", 12100, scratch)
        self.assertIn("LoginServerPort=12100", login.overrides())
        self.assertTrue(login.log.path.endswith("Login.log"))

    def reloading_login(self, answer):
        scratch = database.Scratch("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose_driver_run")
        login = server.LoginServer("loginserver.exe", "loginserver.conf.dist", os.path.join(self.folder, "login"), "127.0.0.2", 12100, scratch)
        login.console = LogTail(self.write(os.path.join("login", "console.txt"), []), interval=0.01)
        sent = []

        class Input:
            def write(self, data):
                sent.append(data.decode("utf-8").strip())
                self.console_lines(answer)

            def flush(self):
                pass

        Input.console_lines = lambda _self, lines: self.write(os.path.join("login", "console.txt"), lines)
        login.process = SimpleNamespace(poll=lambda: None, stdin=Input())
        return login, sent

    def test_the_login_server_rereads_the_rows_the_game_server_wrote_after_it_started(self):
        login, sent = self.reloading_login(["Account clientdriver created with id 1", "creation is now generation 2"])
        self.assertEqual(login.reload("creation", timeout=1), "creation is now generation 2")
        self.assertEqual(sent, ["reload creation"])

    def test_a_reload_the_login_server_refuses_stops_the_run_with_its_reason(self):
        login, _sent = self.reloading_login(["names was not reloaded and generation 1 goes on serving", "  the world database is not open"])
        with self.assertRaises(StepFailed) as raised:
            login.reload("names", timeout=1)
        self.assertIn("could not reload names", str(raised.exception))

    def test_the_zone_rows_are_cached_by_revision_and_layout_outside_the_repository(self):
        path = zones.cache_path("r806919.Wizard_1_610")
        self.assertTrue(path.endswith(os.path.join("clientdriver", "zones", f"r806919.Wizard_1_610.v{zones.LAYOUT}.sql")))
        self.assertFalse(os.path.abspath(path).startswith(os.path.abspath(paths.REPOSITORY)))
        with self.assertRaises(StepFailed):
            zones.ensure(self.folder, self.folder, "")

    def test_the_extractor_s_script_is_applied_statement_by_statement_without_its_header_comment(self):
        script = os.path.join(self.folder, "zones.sql")
        with open(script, "w", encoding="utf-8") as handle:
            handle.write("-- Written by the Project Ambrose extractor from your own Wizard101 install.\n"
                         "START TRANSACTION;\nDELETE FROM `zone_object`;\n\nINSERT INTO `zone_object` (`id`) VALUES (1), (2);\nCOMMIT;\n")
        ran = []

        class Cursor:
            def __enter__(self):
                return self

            def __exit__(self, *_):
                return False

            def execute(self, statement):
                ran.append(statement)

        class Connection:
            def cursor(self):
                return Cursor()

            def commit(self):
                ran.append("committed")

            def close(self):
                pass

        scratch = database.Scratch("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose_driver_run")
        with mock.patch.object(scratch, "_connect", return_value=Connection()):
            said = scratch.apply_sql("world", script)
        self.assertEqual(ran, ["START TRANSACTION;", "DELETE FROM `zone_object`;", "INSERT INTO `zone_object` (`id`) VALUES (1), (2);", "COMMIT;", "committed"])
        self.assertIn("applied 4 statement(s)", said)

    def test_a_copied_wizard_unpacks_its_name_indices_and_keeps_only_appearance_columns(self):
        row = {"name_indices": (3 << 24) | (100 << 16) | (248 << 8) | 27, "school_id": 2343174, "level": 3, "world": 0,
               "zone": "WizardCity/WC_Ravenwood", "zone_display": "Ravenwood"}
        wizard = database.Scratch.wizard_from_rows(row, {"gender": 1, "race": 79806088, "guid": 9})
        self.assertEqual((wizard["locale"], wizard["first"], wizard["middle"], wizard["last"]), ("en-US", 100, 248, 27))
        self.assertEqual(wizard["appearance"], {"gender": 1, "race": 79806088})
        self.assertEqual(wizard["level"], 3)
        self.assertEqual(wizard["experience"], 0)
        self.assertNotIn("stats", wizard)
        row["xp"] = 705
        copied = database.Scratch.wizard_from_rows(row, {}, {"gold": 1234, "health": None, "secondary_school_id": 72777, "revision": 4})
        self.assertEqual(copied["experience"], 705)
        self.assertEqual(copied["stats"], {"gold": 1234, "secondary_school": 72777}, "a full health stays full, and the row's revision is not a stat")


class ReferenceTests(TemporaryFolder):
    def test_it_reads_the_key_the_crops_and_the_targets(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        self.assertEqual(described.window, (40, 20))
        self.assertEqual(described.crop_of("login"), (0, 0, 10, 10))
        self.assertEqual(described.target_of("press"), (5, 5))
        self.assertIn("r806919", described.key)
        self.assertEqual(described.folder_name, "r806919.Wizard_1_610-40x20-ui-install-default")

    def test_a_crop_outside_the_window_is_refused(self):
        document = json.loads(json.dumps(REFERENCE_DOCUMENT))
        document["screens"]["login"]["crop"] = [0, 0, 60, 10]
        with self.assertRaises(Refused) as raised:
            references.References("references.json", document)
        self.assertIn("does not lie inside", str(raised.exception))

    def test_a_crop_without_a_word_on_what_it_shows_is_refused(self):
        document = json.loads(json.dumps(REFERENCE_DOCUMENT))
        document["screens"]["login"].pop("shows")
        with self.assertRaises(Refused):
            references.References("references.json", document)

    def test_a_key_without_a_revision_is_refused(self):
        document = json.loads(json.dumps(REFERENCE_DOCUMENT))
        document["key"].pop("revision")
        with self.assertRaises(Refused):
            references.References("references.json", document)

    def test_another_revision_does_not_match_and_says_so(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        self.assertEqual(described.matches("r806919.Wizard_1_610"), (True, ""))
        matches, reason = described.matches("r812000.Wizard_1_620")
        self.assertFalse(matches)
        self.assertIn("r812000.Wizard_1_620", reason)

    def test_a_missing_crop_is_listed_by_name(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        self.assertEqual(described.missing_crops(self.folder, ["login", "charselect"]), ["charselect", "login"])
        os.makedirs(os.path.dirname(described.crop_file(self.folder, "login")), exist_ok=True)
        with open(described.crop_file(self.folder, "login"), "wb") as handle:
            handle.write(b"a picture")
        self.assertEqual(described.missing_crops(self.folder, ["login", "charselect"]), ["charselect"])

    def test_a_rule_about_quitting_names_a_line_and_a_reason(self):
        document = json.loads(json.dumps(REFERENCE_DOCUMENT))
        document["never_quit_after"] = [{"after": "Error=1", "undone_by": "admitted", "because": "the sample reason"}]
        described = references.References("references.json", document)
        self.assertEqual(described.never_quit_after[0]["after"], "Error=1")
        for broken in ({"undone_by": "admitted", "because": "x"},
                       {"after": "   ", "because": "x"},
                       {"after": "Error=1"},
                       {"after": "Error=(", "because": "x"},
                       {"after": "Error=1", "undone_by": "admitted(", "because": "x"}):
            document["never_quit_after"] = [broken]
            with self.assertRaises(Refused):
                references.References("references.json", document)

    def test_a_file_with_no_rule_about_quitting_names_none(self):
        self.assertEqual(references.References("references.json", REFERENCE_DOCUMENT).never_quit_after, [])

    def test_a_file_that_is_not_json_is_refused_by_name(self):
        path = os.path.join(self.folder, "references.json")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write("{not json")
        with self.assertRaises(Refused) as raised:
            references.load(path)
        self.assertIn(path, str(raised.exception))

    def test_the_reference_file_in_the_repository_reads(self):
        described = references.load(paths.REFERENCES)
        self.assertEqual(described.window, (1280, 720))
        self.assertIn("login", described.screens)
        self.assertIn("login_reconnect", described.targets)


class ScreenTests(unittest.TestCase):
    def test_a_crop_takes_the_pixels_it_names(self):
        picture = frame_of(RED, BLUE)
        crop = picture.crop((0, 0, 10, 10))
        self.assertEqual(crop.size, (10, 10))
        self.assertEqual(crop.pixel(9, 9), RED)
        self.assertEqual(picture.crop((10, 0, 20, 10)).pixel(0, 0), BLUE)

    def test_a_crop_outside_the_frame_is_refused(self):
        with self.assertRaises(ValueError):
            frame_of(RED, BLUE).crop((0, 0, 41, 10))

    def test_the_same_picture_matches_itself_and_a_different_one_does_not(self):
        first = screens.solid(10, 10, RED)
        self.assertEqual(screens.compare(first, first)["fraction"], 1.0)
        self.assertEqual(screens.compare(first, screens.solid(10, 10, BLUE))["fraction"], 0.0)

    def test_a_small_difference_counts_as_a_match_and_a_large_one_does_not(self):
        first = screens.solid(10, 10, (100, 100, 100))
        near = screens.solid(10, 10, (120, 100, 100))
        far = screens.solid(10, 10, (200, 100, 100))
        self.assertEqual(screens.compare(first, near)["fraction"], 1.0)
        self.assertEqual(screens.compare(first, far)["fraction"], 0.0)
        self.assertGreater(screens.compare(first, far)["mean"], screens.compare(first, near)["mean"])

    def test_part_of_a_picture_changing_lowers_the_fraction_without_hiding_the_match(self):
        first = frame_of(RED, BLUE)
        second = frame_of(RED, GREEN)
        whole = screens.compare(first, second)
        self.assertLess(whole["fraction"], 0.8)
        self.assertEqual(screens.compare(first.crop((0, 0, 10, 10)), second.crop((0, 0, 10, 10)))["fraction"], 1.0)

    def test_frames_of_different_sizes_cannot_be_compared(self):
        with self.assertRaises(ValueError):
            screens.compare(screens.solid(10, 10, RED), screens.solid(10, 11, RED))

    def test_a_changed_screen_is_noticed_and_an_unchanged_one_is_not(self):
        first = frame_of(RED, BLUE)
        self.assertEqual(screens.changed(None, first)[0], True)
        self.assertEqual(screens.changed(first, first)[0], False)
        self.assertEqual(screens.changed(first, frame_of(RED, GREEN))[0], True)

    def test_a_blank_frame_is_recognized(self):
        self.assertTrue(screens.is_blank(screens.solid(20, 20, (0, 0, 0))))
        self.assertTrue(screens.is_blank(screens.solid(20, 20, (255, 255, 255))))
        self.assertFalse(screens.is_blank(frame_of(RED, BLUE)))

    def test_a_window_buffer_becomes_a_frame_in_the_right_order(self):
        buffer = bytes([200, 30, 30, 255, 30, 200, 30, 255])
        picture = screens.from_bgra(buffer, 2, 1)
        self.assertEqual(picture.pixel(0, 0), BLUE)
        self.assertEqual(picture.pixel(1, 0), GREEN)

    def test_a_window_buffer_with_padded_rows_becomes_a_frame(self):
        buffer = bytes([200, 30, 30, 255, 0, 0, 0, 0] + [30, 200, 30, 255, 0, 0, 0, 0])
        picture = screens.from_bgra(buffer, 1, 2, stride=8)
        self.assertEqual(picture.size, (1, 2))
        self.assertEqual(picture.pixel(0, 0), BLUE)
        self.assertEqual(picture.pixel(0, 1), GREEN)

    def test_a_buffer_that_is_too_short_is_refused(self):
        with self.assertRaises(ValueError):
            screens.from_bgra(b"\x00\x00\x00\xff", 2, 1)

    def test_the_store_names_the_screen_a_frame_is_showing(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        crops = {"login": screens.solid(10, 10, RED), "charselect": screens.solid(10, 10, GREEN)}
        store = screens.Store(described, "unused", loader=lambda path: crops[os.path.basename(path)[:-4]])
        found, scored = store.identify(frame_of(RED, BLUE), ["login", "charselect"])
        self.assertEqual(found, "login")
        self.assertEqual(scored["login"]["fraction"], 1.0)
        self.assertEqual(scored["charselect"]["fraction"], 0.0)

    def test_the_store_names_no_screen_when_nothing_matches(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        crops = {"login": screens.solid(10, 10, GREEN), "charselect": screens.solid(10, 10, GREEN)}
        store = screens.Store(described, "unused", loader=lambda path: crops[os.path.basename(path)[:-4]])
        found, _scored = store.identify(frame_of(RED, BLUE), ["login", "charselect"])
        self.assertIsNone(found)

    def test_a_frame_survives_being_written_and_read_as_a_picture(self):
        try:
            import PIL
        except ImportError:
            self.skipTest("Pillow is not installed on this machine")
        self.assertTrue(PIL)
        with tempfile.TemporaryDirectory(prefix="clientdriver-test-") as folder:
            picture = frame_of(RED, BLUE)
            path = screens.save_png(picture, os.path.join(folder, "crops", "login.png"))
            read = screens.load_png(path)
            self.assertEqual(read.size, picture.size)
            self.assertEqual(screens.compare(read, picture)["fraction"], 1.0)


class ClientInputTests(unittest.TestCase):
    def test_mouse_click_allows_a_lagging_client_to_process_window_messages(self):
        api = mock.Mock()
        api.GetAsyncKeyState.return_value = 0
        gui = mock.Mock()
        con = SimpleNamespace(WM_MOUSEMOVE=0x0200, WM_LBUTTONDOWN=0x0201, WM_LBUTTONUP=0x0202)
        client_window = client.Client.__new__(client.Client)
        client_window.handle = 0x1234

        with (mock.patch.dict(sys.modules, {"win32api": api, "win32con": con, "win32gui": gui}),
              mock.patch.object(client.Client, "activated", return_value=mock.MagicMock()),
              mock.patch.object(client.Client, "cursor_at", return_value=mock.MagicMock()),
              mock.patch("clientdriver.client.time.sleep")):
            client_window.click(30, 40)

        self.assertEqual(gui.SendMessageTimeout.call_count, 3)
        self.assertTrue(all(call.args[-1] == client.SEND_MESSAGE_TIMEOUT_MS
                            for call in gui.SendMessageTimeout.call_args_list))


    def test_key_events_activate_the_client_window(self):
        api = mock.Mock()
        api.GetAsyncKeyState.return_value = 0
        api.MapVirtualKey.side_effect = lambda virtual_key, _mode: virtual_key
        gui = mock.Mock()
        con = SimpleNamespace(VK_RETURN=0x0D, WM_KEYDOWN=0x0100, WM_KEYUP=0x0101)
        activated = mock.MagicMock()
        client_window = client.Client.__new__(client.Client)
        client_window.handle = 0x1234
        with (mock.patch.dict(sys.modules, {"win32api": api, "win32con": con, "win32gui": gui}),
              mock.patch.object(client.Client, "activated", return_value=activated) as activate,
              mock.patch("clientdriver.client.time.sleep")):
            client_window.keys([0x46], hold=0.01)

        activate.assert_called_once_with()
        self.assertEqual(gui.PostMessage.call_count, 2)


class FakeClient:
    def __init__(self, log_path, picture):
        self.log = LogTail(log_path, encoding="latin-1", interval=0.01)
        self.handle = 0x1234
        self.current = picture
        self.typed = []
        self.presses = []
        self.shots = []
        self.living = True
        self.frames_taken = 0
        self.on_click = None
        self.active = True
        self.refuses_shots = 0
        self.arriving = []
        self.closes = []

    def alive(self):
        return self.living

    def close(self, timeout=60, force=False):
        self.closes.append(force)
        self.living = False
        return "the client was ended without its own quit path" if force else "the client was closed"

    def frame(self):
        self.frames_taken += 1
        if self.arriving:
            self.current = self.arriving.pop(0)
        return self.current

    def screenshot(self, path, picture=None):
        if self.refuses_shots:
            self.refuses_shots -= 1
            raise OSError("the disk is full")
        self.shots.append(os.path.basename(path))
        return picture if picture is not None else self.current

    def is_foreground(self):
        return False

    def type(self, text):
        self.typed.append(text)

    def post_char(self, code):
        self.typed.append(chr(code))

    def key(self, virtual_key, hold=0.05):
        self.typed.append(virtual_key)
        self.held = hold
        time.sleep(hold)

    def keys(self, virtual_keys, hold=0.05):
        self.typed.append(tuple(virtual_keys))
        self.held = hold
        time.sleep(hold)

    def click(self, x, y, dwell=0.35):
        self.presses.append((x, y, round(dwell, 2)))
        if self.on_click:
            self.on_click(len(self.presses))
        return f"{x},{y} after {dwell:.2f}s with the window " + ("active" if self.active else "NOT active"), self.active


class FakeLauncherClient(FakeClient):
    def __init__(self, log_path, picture):
        super().__init__(log_path, picture)
        self.launcher_handle = 0x5678
        self.window = (40, 20)
        self.pid = None
        self.calls = []

    def launcher_frame(self):
        self.calls.append("the launcher window was filmed")
        return frame_of(GREEN, GREEN)

    def find_process(self, timeout=180):
        self.calls.append("the client process was looked for")
        return 4242

    def find_window(self, timeout=180):
        self.calls.append("the client window was looked for")
        return self.handle

    def to_background(self):
        self.calls.append("the client went to the back")
        return "at the bottom"


class FakeAutomation:
    def __init__(self, shown=(), controls=("Settings", "Play"), after=0):
        self.shown = list(shown)
        self.controls = controls
        self.after = after
        self.read = 0
        self.pressed = []

    def texts(self, handle):
        self.read += 1
        return self.shown if self.read > self.after else ["Looking at your installation"]

    def press(self, handle, control):
        if control not in self.controls:
            raise StepFailed(f"the launcher window holds no control named {control!r}")
        self.pressed.append((handle, control))
        return f"invoked {control!r} through UI Automation"


class FakeServer:
    WHAT = "the login server"

    def __init__(self, log_path, console_path):
        self.log = LogTail(log_path, interval=0.01)
        self.console = LogTail(console_path, interval=0.01)
        self.commands = []
        self.living = True

    def alive(self):
        return self.living

    def send(self, line):
        self.commands.append(line)


class FakeDatabases:
    def __init__(self, answers):
        self.answers = list(answers)
        self.asked = []

    def value(self, kind, query):
        self.asked.append((kind, query))
        answer = self.answers.pop(0) if len(self.answers) > 1 else self.answers[0]
        if isinstance(answer, Exception):
            raise answer
        return answer

    def execute(self, kind, statement):
        self.asked.append((kind, statement))
        return 1


class EngineTests(TemporaryFolder):
    def build(self, steps, picture=None, answers=("0",), variables=None, expect_failure=False, companion=False, launch=None):
        document = {"title": "a scenario for the tests", "steps": steps}
        if launch:
            document["launch"] = launch
        if expect_failure:
            document["expect"] = "failure"
        if companion:
            document.update(requires={"gameserver": True}, companion={"wizard": WorldEntryTests.WIZARD})
        path = self.write_json(os.path.join("scenarios", "test.json"), document)
        loaded = scenario.load(path)
        self.server_log = self.write(os.path.join("server", "Login.log"), [])
        self.console = self.write(os.path.join("server", "console.txt"), [])
        self.client_log = self.write(os.path.join("client", "WizardClient.log"), [])
        self.client = (FakeLauncherClient if launch == "window" else FakeClient)(self.client_log, picture if picture is not None else frame_of(RED, BLUE))
        self.server = FakeServer(self.server_log, self.console)
        described = references.References("references.json", REFERENCE_DOCUMENT)
        crops = {"login": screens.solid(10, 10, RED), "charselect": screens.solid(10, 10, GREEN)}
        store = screens.Store(described, "unused", loader=lambda path: crops[os.path.basename(path)[:-4]])
        self.shots = os.path.join(self.folder, "shots")
        os.makedirs(self.shots, exist_ok=True)
        self.companion = None
        if companion:
            self.companion_log = self.write(os.path.join("companion", "WizardClient.log"), [])
            self.companion = FakeClient(self.companion_log, picture if picture is not None else frame_of(RED, BLUE))
        return engine.Engine(loaded, self.client, self.server, store, self.shots,
                             dict(variables or {}, user="clientdriver", password="secret", companion_user="clientdriver2"),
                             FakeDatabases(answers), companion=self.companion)

    def test_a_step_waits_on_a_server_line_and_checks_what_it_says(self):
        running = self.build([{"action": "wait_server_log", "name": "the password is refused",
                               "pattern": r"Error=(\S+)$", "timeout": 1, "expect": "AuthenFailed"}])
        self.write(os.path.join("server", "Login.log"), ["2026 INFO [x] sent MSG_USER_AUTHEN_RSP Error=AuthenFailed"])
        running.run()
        self.assertTrue(running.steps[0]["ok"])

    def test_a_step_fails_when_the_line_says_something_else(self):
        running = self.build([{"action": "wait_server_log", "name": "the password is refused",
                               "pattern": r"Error=(\S+)$", "timeout": 1, "expect": "AuthenFailed"}])
        self.write(os.path.join("server", "Login.log"), ["2026 INFO [x] sent MSG_USER_AUTHEN_RSP Error=Timeout"])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("Timeout", str(raised.exception))
        self.assertFalse(running.steps[0]["ok"])

    def test_a_step_that_rejects_a_value_fails_on_it(self):
        running = self.build([{"action": "wait_client_log", "name": "the login failed",
                               "pattern": r"LOGIN RESPONSE: Error=(-?\d+)", "timeout": 1, "reject": "0"}])
        self.write(os.path.join("client", "WizardClient.log"), ["09/17/26 [STAT] LOGIN RESPONSE: Error=0"], encoding="latin-1")
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("rejects", str(raised.exception))

    def test_a_failed_step_names_what_it_waited_for_and_leaves_a_screenshot(self):
        running = self.build([{"action": "wait_client_log", "name": "a line the client will never write",
                               "pattern": "the wizard has reached Ravenwood", "timeout": 0.05}])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("the wizard has reached Ravenwood", str(raised.exception))
        self.assertEqual(running.steps[0]["screen"]["shot"], "01-fail-a-line-the-client-will-never-write.png")
        self.assertEqual(self.client.shots, ["01-fail-a-line-the-client-will-never-write.png"])

    def test_a_variable_reaches_the_pattern(self):
        running = self.build([{"action": "wait_server_log", "name": "the account is named",
                               "pattern": "authenticated as {user}", "timeout": 1}])
        self.write(os.path.join("server", "Login.log"), ["2026 INFO [x] authenticated as clientdriver (id 1)"])
        running.run()
        self.assertTrue(running.steps[0]["ok"])

    def test_a_kept_value_reaches_a_later_step_that_expects_it(self):
        running = self.build([
            {"action": "wait_server_log", "name": "where it was saved", "pattern": r"saved wizard \d+ at \((\S+, \S+, \S+)\)", "timeout": 1, "keep": "saved_place"},
            {"action": "wait_server_log", "name": "where it came back", "pattern": r"put wizard \d+ at \((\S+, \S+, \S+)\)", "timeout": 1, "expect": "{saved_place}"}])
        self.write(os.path.join("server", "Login.log"), ["INFO saved wizard 1 at (-3036, 2120, 4) facing 1.5", "INFO put wizard 1 at (-3036, 2120, 4) with"])
        running.run()
        self.assertEqual(running.variables["saved_place"], "-3036, 2120, 4")
        self.assertTrue(running.steps[1]["ok"])
        moved = self.build([
            {"action": "wait_server_log", "name": "where it was saved", "pattern": r"saved wizard \d+ at \((\S+)\)", "timeout": 1, "keep": "saved_place"},
            {"action": "wait_server_log", "name": "where it came back", "pattern": r"put wizard \d+ at \((\S+)\)", "timeout": 1, "expect": "{saved_place}"}])
        self.write(os.path.join("server", "Login.log"), ["INFO saved wizard 1 at (7)", "INFO put wizard 1 at (8)"])
        with self.assertRaises(StepFailed) as raised:
            moved.run()
        self.assertIn("where the step expects '7'", str(raised.exception))

    def test_a_restart_needs_a_run_that_can_start_its_client_again(self):
        running = self.build([{"action": "restart_client", "name": "again", "timeout": 5}])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("no client it can start again", str(raised.exception))
        again = self.build([{"action": "restart_client", "name": "again"}])
        asked = []
        again.restart = lambda timeout: asked.append(timeout) or "started again"
        again.run()
        self.assertEqual(asked, [180.0])
        self.assertEqual(again.steps[0]["result"], "started again")

    def test_game_server_can_stop_and_start_while_a_scenario_is_running(self):
        running = self.build([
            {"action": "stop_game_server", "name": "stop the game server"},
            {"action": "start_game_server", "name": "start the game server", "timeout": 90}])

        class FakeGame:
            def __init__(self):
                self.living = True
                self.calls = []

            def alive(self):
                return self.living

            def stop(self):
                self.calls.append("stop")
                self.living = False
                return "stopped"

            def start(self, timeout=300):
                self.calls.append(("start", timeout))
                self.living = True
                return "started"

        game = FakeGame()
        running.game = game
        running.run()
        self.assertEqual(game.calls, ["stop", ("start", 90.0)])
        self.assertTrue(game.alive())

    def test_killing_a_client_ends_only_the_selected_client_without_its_quit_path(self):
        running = self.build([{"action": "kill_client", "name": "the companion loses its process",
                               "client": "companion"}], companion=True)
        running.run()
        self.assertEqual(self.companion.closes, [True])
        self.assertTrue(self.client.living)

    def test_play_holds_the_run_until_the_client_is_closed_and_then_lets_it_end(self):
        running = self.build([{"action": "play", "name": "the maintainer plays"}])
        polls = []

        def closed_on_the_third_poll(seconds):
            polls.append(seconds)
            if len(polls) == 3:
                self.client.living = False

        with mock.patch.object(engine.time, "sleep", closed_on_the_third_poll):
            running.run()
        self.assertEqual(len(polls), 3)
        self.assertEqual([step["ok"] for step in running.steps], [True])
        self.assertIn("until the client was closed", running.steps[0]["result"])

    def test_a_step_that_names_the_companion_drives_it_and_the_next_one_drives_the_main_client_again(self):
        running = self.build([
            {"action": "submit_login", "name": "the companion logs in", "password": "other", "client": "companion"},
            {"action": "wait_client_log", "name": "the companion is admitted", "pattern": "admitted", "timeout": 1, "client": "companion"},
            {"action": "hold_key", "name": "the companion walks", "vk": "0x57", "seconds": 0.01, "client": "companion"},
            {"action": "shot", "name": "the main client sees it"},
            {"action": "shot", "name": "the companion sees itself", "client": "companion"},
            {"action": "restart_client", "name": "the companion comes back", "client": "companion"}], companion=True)
        self.write(os.path.join("companion", "WizardClient.log"), ["The LoginServer has admitted the user"], encoding="latin-1")
        restarted = []
        running.restart = lambda timeout: restarted.append("main")
        running.restarts["companion"] = lambda timeout: restarted.append("companion") or "started again"
        running.run()
        self.assertEqual(self.companion.typed, ["clientdriver2", chr(9), "other", chr(13), 0x57])
        self.assertEqual(self.client.typed, [])
        self.assertEqual(restarted, ["companion"])
        self.assertEqual([taken.get("client") for taken in running.screenshots if taken["step"].startswith("the ") and "sees" in taken["step"]],
                         [None, "companion"])
        self.assertEqual(running.steps[0]["client"], "companion")
        self.assertNotIn("client", running.steps[3])
        self.assertIn("the companion sees itself", " ".join(self.companion.shots).replace("-", " "))

    def test_a_watched_hold_films_the_other_client_while_the_key_is_held_and_after(self):
        running = self.build([{"action": "hold_key", "name": "the companion walks", "vk": "0x57", "seconds": 0.3, "client": "companion",
                               "watch": "main", "watch_every": 0.1, "watch_after": 0.2}], companion=True)
        running.run()
        filmed = [taken for taken in running.screenshots if taken.get("client") == "main"]
        self.assertGreaterEqual(len(filmed), 4)
        self.assertTrue(filmed[0]["held"])
        self.assertFalse(filmed[-1]["held"])
        self.assertEqual(len(self.client.shots), len(filmed))
        self.assertEqual(self.companion.typed, [0x57])
        self.assertEqual(self.companion.held, 0.3)
        self.assertIn(f"filmed {len(filmed)} time(s)", running.steps[0]["result"])

    def test_a_watched_press_films_the_other_client_while_it_is_made_and_after(self):
        running = self.build([{"action": "click", "name": "the main wizard waves", "target": "press",
                               "watch": "companion", "watch_every": 0.1, "watch_after": 0.3}], companion=True)
        self.client.on_click = lambda presses: time.sleep(0.3)
        running.run()
        filmed = [taken for taken in running.screenshots if taken.get("client") == "companion"]
        self.assertGreaterEqual(len(filmed), 3)
        self.assertTrue(filmed[0]["held"])
        self.assertFalse(filmed[-1]["held"])
        self.assertEqual(len(self.companion.shots), len(filmed))
        self.assertEqual(len(self.client.presses), 1)
        self.assertEqual(self.companion.presses, [])
        self.assertIn(f"filmed {len(filmed)} time(s)", running.steps[0]["result"])
        self.assertIn("while the press was made", running.steps[0]["result"])

    def test_several_keys_held_together_go_down_together_and_a_list_is_bounded(self):
        running = self.build([{"action": "hold_key", "name": "circle", "vk": ["0x57", 0x44], "seconds": 0.01}])
        running.run()
        self.assertEqual(self.client.typed, [(0x57, 0x44)])
        self.assertIn("held the keys 0x57 and 0x44", running.steps[0]["result"])
        for keys in ([], ["0x57"] * 5):
            with self.assertRaises(Refused) as raised:
                self.build([{"action": "hold_key", "name": "circle", "vk": keys, "seconds": 0.01}])
            self.assertIn("1 to 4 keys", str(raised.exception))

    def test_a_recorded_line_is_kept_for_the_report(self):
        running = self.build([{"action": "wait_server_log", "name": "the character list", "record": True,
                               "pattern": r"listed (\d+) character", "timeout": 1, "expect": "0"}])
        self.write(os.path.join("server", "Login.log"), ["2026 DEBUG [x] Session 3 listed 0 character(s) of account 1"])
        running.run()
        self.assertEqual(len(running.notes), 1)
        self.assertIn("listed 0 character(s)", running.notes[0]["line"])

    def test_a_held_key_that_moves_the_view_changes_far_more_of_it_than_the_same_time_idle(self):
        running = self.build([{"action": "hold_key", "name": "walk", "vk": "0x57", "seconds": 0.01, "moves": True}])
        self.client.arriving = [frame_of(RED, BLUE), frame_of(RED, BLUE), frame_of(GREEN, GREEN)]
        running.run()
        self.assertEqual(self.client.typed, [0x57])
        self.assertEqual(self.client.held, 0.01)
        self.assertIn("0.000 of the frame stayed the same while it was held, against 1.000", running.steps[0]["result"])

    def test_a_held_key_that_leaves_the_view_as_it_was_fails_a_step_that_expects_it_to_move(self):
        running = self.build([{"action": "hold_key", "name": "walk", "vk": "0x57", "seconds": 0.01, "moves": True}])
        self.client.arriving = [frame_of(RED, BLUE), frame_of(RED, BLUE), frame_of(RED, BLUE)]
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("not the view moving", str(raised.exception))
        idle = self.build([{"action": "hold_key", "name": "look", "vk": "0x57", "seconds": 0.01}])
        self.client.arriving = [frame_of(RED, BLUE), frame_of(RED, BLUE), frame_of(RED, BLUE)]
        idle.run()
        self.assertTrue(idle.steps[0]["ok"])

    def test_the_view_counts_as_moved_only_well_past_what_it_changes_idle(self):
        self.assertTrue(engine.held_key_moved(0.98, 0.40))
        self.assertTrue(engine.held_key_moved(0.98, 0.70))
        self.assertFalse(engine.held_key_moved(0.98, 0.85))
        self.assertFalse(engine.held_key_moved(0.50, 0.45))

    def test_a_screen_step_waits_until_the_screen_is_there(self):
        running = self.build([{"action": "wait_screen", "name": "the login window", "screens": ["login"], "timeout": 2}],
                             picture=frame_of(GREEN, GREEN))
        self.client.arriving = [frame_of(GREEN, GREEN), frame_of(RED, BLUE)]
        running.run()
        self.assertIn("login is on the screen", running.steps[0]["result"])
        self.assertIn("1.0 of its pixels match", running.steps[0]["result"])
        self.assertEqual(self.client.frames_taken, 2)
        self.assertIs(running.current, self.client.current)

    def test_a_screen_step_that_never_sees_its_screen_says_what_the_closest_match_was(self):
        running = self.build([{"action": "wait_screen", "name": "the login window", "screens": ["login"], "timeout": 0.1}],
                             picture=frame_of(GREEN, GREEN))
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("did not appear", str(raised.exception))
        self.assertIn("fraction", str(raised.exception))

    def test_a_press_is_repeated_until_the_check_that_it_took_passes(self):
        running = self.build([{"action": "click", "name": "press the button that reconnects", "target": "press",
                               "attempts": 4, "dwell": 0.0, "dwell_step": 0.0,
                               "until": {"action": "wait_client_log", "pattern": r"AppCloseConnection\(\) called",
                                         "timeout": 0.05}}])

        def after_three(taken):
            if taken == 3:
                self.write(os.path.join("client", "WizardClient.log"), ["09/17/26 [STAT] AppCloseConnection() called"],
                           encoding="latin-1")

        self.client.on_click = after_three
        running.run()
        self.assertEqual(len(self.client.presses), 3)
        self.assertEqual(self.client.presses[0][:2], (5, 5))
        self.assertIn("3 attempt(s)", running.steps[0]["result"])

    def test_a_press_that_never_takes_fails_and_names_the_check(self):
        running = self.build([{"action": "click", "name": "press the button", "target": "press", "attempts": 2,
                               "dwell": 0.0, "dwell_step": 0.0,
                               "until": {"action": "wait_client_log", "pattern": "nothing", "timeout": 0.02}}])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("2 press(es)", str(raised.exception))
        self.assertEqual(len(self.client.presses), 2)

    def test_a_press_with_nothing_to_check_fails_when_the_window_never_became_active(self):
        running = self.build([{"action": "click", "name": "take the next look", "target": "press", "attempts": 2,
                               "dwell": 0.0, "dwell_step": 0.0}])
        self.client.active = False
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("never became the active one", str(raised.exception))
        self.assertIn("with the window NOT active", str(raised.exception))
        self.assertEqual(len(self.client.presses), 2)
        self.assertFalse(running.steps[0]["ok"])

    def test_a_press_that_took_carries_what_the_window_did_into_the_report(self):
        running = self.build([{"action": "click", "name": "take the next look", "target": "press", "attempts": 1,
                               "dwell": 0.0, "dwell_step": 0.0}])
        running.run()
        self.assertIn("with the window active", running.steps[0]["result"])

    def test_a_press_never_holds_the_window_longer_than_the_driver_allows(self):
        running = self.build([{"action": "click", "name": "press the button", "target": "press", "attempts": 3,
                               "dwell": 4.0, "dwell_step": 0.0,
                               "until": {"action": "wait_client_log", "pattern": "nothing", "timeout": 0.02}}])
        with self.assertRaises(StepFailed):
            running.run()
        self.assertEqual([dwell for _x, _y, dwell in self.client.presses], [engine.MAX_DWELL] * 3)

    def test_a_press_is_refused_when_its_screen_is_gone(self):
        running = self.build([{"action": "click", "name": "press the button", "target": "press",
                               "on_screen": "charselect", "dwell": 0.0}])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("no longer there", str(raised.exception))
        self.assertEqual(self.client.presses, [])

    def test_the_login_window_is_filled_and_submitted(self):
        running = self.build([{"action": "submit_login", "name": "submit the right password", "password": "{password}"}])
        running.run()
        self.assertEqual(self.client.typed, ["clientdriver", "\t", "secret", "\r"])

    def test_a_console_command_reaches_the_server(self):
        running = self.build([{"action": "server_command", "name": "ask for the accounts", "command": "account list"}])
        running.run()
        self.assertEqual(self.server.commands, ["account list"])

    def test_a_game_command_reaches_the_game_server_and_waits_for_its_answer(self):
        running = self.build([{"action": "game_command", "name": "tell the world", "command": "server announce Hello {user}",
                               "pattern": r"Shown to (\d+) wizard", "timeout": 1}])
        game_console = self.write(os.path.join("game", "console.txt"), ["Shown to 1 wizard(s) in the world"])
        running.game = FakeServer(self.write(os.path.join("game", "Server.log"), []), game_console)
        running.run()
        self.assertEqual(running.game.commands, ["server announce Hello clientdriver"])
        self.assertEqual(self.server.commands, [], "a game command never reaches the login server")
        self.assertIn("Shown to 1 wizard", running.steps[0]["result"])

    def test_a_game_command_is_refused_without_the_game_server(self):
        running = self.build([{"action": "game_command", "name": "tell the world", "command": "server announce Hello"}])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("does not require the game server", str(raised.exception))

    def test_a_database_statement_runs_on_the_run_s_own_database_and_is_noted(self):
        running = self.build([{"action": "db_exec", "name": "spoil the next key", "database": "login", "statement": "UPDATE login_key SET used = 1 WHERE account_id = 1"}])
        running.run()
        self.assertEqual(running.databases.asked, [("login", "UPDATE login_key SET used = 1 WHERE account_id = 1")])
        self.assertIn("1 row(s) changed", running.steps[0]["result"])
        path = self.write_json(os.path.join("scenarios", "elsewhere.json"), {"title": "elsewhere", "steps": [
            {"action": "db_exec", "name": "reach out", "database": "mysql", "statement": "DROP DATABASE x"}]})
        with self.assertRaises(Refused) as raised:
            scenario.load("elsewhere.json", search=(os.path.dirname(path),))
        self.assertIn("run's own login, characters or world database", str(raised.exception))

    def test_a_database_step_waits_for_the_row_it_expects(self):
        running = self.build([{"action": "wait_db", "name": "no wizard yet", "database": "characters",
                               "query": "SELECT COUNT(*) FROM characters", "expect": "0", "timeout": 0.2}],
                             answers=["1", "0"])
        running.run()
        self.assertEqual(len(self.client.shots), 1)
        self.assertEqual([kind for kind, _query in running.databases.asked], ["characters", "characters"])

    def test_a_database_step_fails_when_the_row_never_comes(self):
        running = self.build([{"action": "wait_db", "name": "a wizard is written", "database": "characters",
                               "query": "SELECT COUNT(*) FROM characters", "expect": "1", "timeout": 0.05}],
                             answers=["0"])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("SELECT COUNT(*)", str(raised.exception))

    def test_a_database_step_without_an_expectation_waits_for_an_answer(self):
        running = self.build([{"action": "wait_db", "name": "the wizard is there", "database": "characters",
                               "query": "SELECT id FROM characters WHERE name='Iridian'", "timeout": 1}],
                             answers=[None, None, "7"])
        running.run()
        self.assertEqual(len(running.databases.asked), 3)
        self.assertIn("'7'", running.steps[0]["result"])

    def test_a_database_step_without_an_expectation_fails_when_the_row_never_appears(self):
        running = self.build([{"action": "wait_db", "name": "the wizard is there", "database": "characters",
                               "query": "SELECT id FROM characters WHERE name='Iridian'", "timeout": 0.05}],
                             answers=[None])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("WHERE name='Iridian'", str(raised.exception))
        self.assertIn("an answer", str(raised.exception))

    def test_a_database_that_refuses_one_read_is_asked_again_until_the_deadline(self):
        running = self.build([{"action": "wait_db", "name": "no wizard yet", "database": "characters",
                               "query": "SELECT COUNT(*) FROM characters", "expect": "0", "timeout": 1}],
                             answers=[RuntimeError("the connection was refused"), "0"])
        running.run()
        self.assertTrue(running.steps[0]["ok"])
        self.assertEqual(len(running.databases.asked), 2)

    def test_a_database_that_never_answers_fails_with_the_last_error(self):
        running = self.build([{"action": "wait_db", "name": "no wizard yet", "database": "characters",
                               "query": "SELECT COUNT(*) FROM characters", "expect": "0", "timeout": 0.05}],
                             answers=[RuntimeError("the connection was refused")])
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("the connection was refused", str(raised.exception))
        self.assertIn("SELECT COUNT(*)", str(raised.exception))

    def test_a_forbidden_line_fails_the_run_and_its_absence_passes(self):
        running = self.build([{"action": "forbid_log", "name": "nothing confused the client", "side": "client",
                               "pattern": "Received an unknown message type"}])
        running.run()
        self.assertTrue(running.steps[0]["ok"])
        running = self.build([{"action": "forbid_log", "name": "nothing confused the client", "side": "client",
                               "pattern": "Received an unknown message type"}])
        self.write(os.path.join("client", "WizardClient.log"), ["09/17/26 [ERRO] Received an unknown message type: 481"],
                   encoding="latin-1")
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("forbids", str(raised.exception))

    def test_every_step_that_changed_the_screen_leaves_a_screenshot_and_the_others_do_not(self):
        running = self.build([
            {"action": "wait_server_log", "name": "the server is ready", "pattern": "ready", "timeout": 1},
            {"action": "wait_server_log", "name": "the session is offered", "pattern": "offered", "timeout": 1},
            {"action": "wait_server_log", "name": "the session is closed", "pattern": "closed", "timeout": 1},
        ])
        self.write(os.path.join("server", "Login.log"), ["2026 INFO [x] loginserver ready", "2026 INFO [x] Session 3 offered"])
        running.execute(running.scenario.steps[0])
        running.execute(running.scenario.steps[1])
        self.client.current = frame_of(GREEN, GREEN)
        self.write(os.path.join("server", "Login.log"), ["2026 INFO [x] Session 3 closed"])
        running.execute(running.scenario.steps[2])
        changed = [step["screen"]["changed"] for step in running.steps]
        self.assertEqual(changed, [True, False, True])
        self.assertEqual([step["screen"].get("shot") for step in running.steps],
                         ["01-the-server-is-ready.png", None, "02-the-session-is-closed.png"])
        self.assertEqual(len(running.screenshots), 2)

    def test_a_shot_step_keeps_the_name_it_is_given(self):
        running = self.build([{"action": "shot", "name": "the login window", "file": "login-window"}])
        running.run()
        self.assertEqual(self.client.shots, ["01-login-window.png"])
        self.assertEqual(running.screenshots[0]["step"], "the login window")
        self.assertEqual(running.steps[0]["screen"]["shot"], "01-login-window.png")

    def test_a_shot_that_cannot_be_written_fails_its_own_step_and_renames_nothing(self):
        running = self.build([
            {"action": "shot", "name": "the login window", "file": "login-window"},
            {"action": "shot", "name": "the screen after it", "file": "after"},
        ])
        running.execute(running.scenario.steps[0])
        self.client.refuses_shots = 2
        with self.assertRaises(StepFailed) as raised:
            running.execute(running.scenario.steps[1])
        self.assertIn("the disk is full", str(raised.exception))
        self.assertEqual(len(running.screenshots), 1)
        self.assertEqual(running.screenshots[0]["step"], "the login window")

    def test_the_first_shot_of_a_run_failing_names_the_reason_rather_than_an_index(self):
        running = self.build([{"action": "shot", "name": "the login window", "file": "login-window"}])
        self.client.refuses_shots = 2
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("could not be written", str(raised.exception))
        self.assertEqual(running.screenshots, [])


class LauncherWindowTests(TemporaryFolder):
    build = EngineTests.build
    SHOWN = ["Ready to play", "Your own Wizard101 r806919.Wizard_1_610, started from a folder of its own against 127.0.0.2:12100.",
             "127.0.0.2:12100", "r806919.Wizard_1_610", "Play"]

    def window(self, steps, automation):
        running = self.build(steps, launch="window", variables={"host": "127.0.0.2", "port": "12100", "revision": "r806919.Wizard_1_610"})
        running.automation = automation
        return running

    def test_the_launcher_window_shows_every_pattern_and_the_matched_text_is_kept_for_the_report(self):
        automation = FakeAutomation(self.SHOWN, after=1)
        running = self.window([{"action": "launcher_shows", "name": "the window shows the run", "patterns": ["{revision}", "{host}:{port}"],
                                "timeout": 5}], automation)
        running.run()
        self.assertEqual(automation.read, 2)
        self.assertIn("r806919.Wizard_1_610", running.steps[0]["result"])
        self.assertEqual(running.notes[0]["launcher_shows"], ["Your own Wizard101 r806919.Wizard_1_610, started from a folder of its own against 127.0.0.2:12100.",
                                                              "Your own Wizard101 r806919.Wizard_1_610, started from a folder of its own against 127.0.0.2:12100."])
        self.assertIn("the launcher window was filmed", self.client.calls)
        self.assertTrue(running.steps[0]["screen"].get("shot"))

    def test_the_install_is_matched_as_the_literal_path_the_driver_found(self):
        install = "C:\\Games (x86)\\Wizard101+"
        shown = ["Install", install]
        running = self.window([{"action": "launcher_shows", "name": "the settings show the install", "patterns": ["^{install}$"], "timeout": 1}],
                              FakeAutomation(shown))
        running.variables["install"] = install
        running.run()
        self.assertEqual(running.notes[0]["launcher_shows"], [install])
        elsewhere = self.window([{"action": "launcher_shows", "name": "the settings show another install", "patterns": ["^{install}$"],
                                  "timeout": 0.2}], FakeAutomation(["C:\\Games x86\\Wizard101"]))
        elsewhere.variables["install"] = install
        with self.assertRaises(StepFailed):
            elsewhere.run()

    def test_a_pattern_the_launcher_window_never_shows_is_named_with_what_it_did_show(self):
        running = self.window([{"action": "launcher_shows", "name": "the window shows another server", "patterns": ["{revision}", "10\\.0\\.0\\.1"],
                                "timeout": 0.2}], FakeAutomation(self.SHOWN))
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("10\\.0\\.0\\.1", str(raised.exception))
        self.assertNotIn("/r806919", str(raised.exception))
        self.assertIn("Ready to play", str(raised.exception))

    def test_play_is_pressed_by_its_name_and_the_client_it_starts_is_found_with_its_window(self):
        automation = FakeAutomation()
        running = self.window([{"action": "launcher_press", "name": "press Play", "control": "Play", "timeout": 5}], automation)
        running.run()
        self.assertEqual(automation.pressed, [(0x5678, "Play")])
        self.assertEqual(self.client.calls, ["the client process was looked for", "the client window was looked for", "the client went to the back"])
        self.assertEqual(self.client.pid, 4242)
        self.assertIn("process 4242", running.steps[0]["result"])

    def test_another_control_is_pressed_without_waiting_for_a_client(self):
        automation = FakeAutomation()
        running = self.window([{"action": "launcher_press", "name": "open the settings", "control": "Settings", "timeout": 5}], automation)
        running.run()
        self.assertEqual(automation.pressed, [(0x5678, "Settings")])
        self.assertEqual(self.client.calls, [])

    def test_a_control_the_launcher_window_does_not_hold_fails_naming_it(self):
        running = self.window([{"action": "launcher_press", "name": "press Quit", "control": "Quit", "timeout": 0.2}], FakeAutomation())
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("'Quit'", str(raised.exception))
        self.assertEqual(self.client.calls, [])

    def test_a_run_that_opened_no_launcher_window_cannot_read_one(self):
        running = self.window([{"action": "launcher_shows", "name": "the window", "patterns": ["Play"], "timeout": 1}], FakeAutomation(self.SHOWN))
        self.client.launcher_handle = None
        with self.assertRaises(StepFailed) as raised:
            running.run()
        self.assertIn("no launcher window", str(raised.exception))


class LaunchTests(TemporaryFolder):
    SHOWS = {"action": "launcher_shows", "name": "the window", "patterns": ["Play"], "timeout": 5}

    def load(self, document, name="scenario.json"):
        return scenario.load(self.write_json(name, dict({"title": "t", "steps": []}, **document)))

    def test_the_launcher_is_started_as_a_console_unless_the_scenario_asks_for_its_window(self):
        self.assertEqual(self.load({}).launch, "console")
        self.assertEqual(self.load({"launch": "window", "steps": [self.SHOWS]}).launch, "window")
        with self.assertRaises(Refused) as raised:
            self.load({"launch": "tray"})
        self.assertIn("console or a window", str(raised.exception))

    def test_the_window_is_refused_with_the_patching_default_and_with_a_companion(self):
        with self.assertRaises(Refused) as raised:
            self.load({"launch": "window", "patching": "default"})
        self.assertIn("patching default", str(raised.exception))
        with self.assertRaises(Refused) as raised:
            self.load({"launch": "window", "requires": {"gameserver": True}, "companion": {"wizard": WorldEntryTests.WIZARD}})
        self.assertIn("one launcher window", str(raised.exception))

    def test_a_step_on_the_launcher_window_needs_a_scenario_that_opens_it(self):
        for step in (self.SHOWS, {"action": "launcher_press", "name": "press Play", "control": "Play", "timeout": 5}):
            with self.assertRaises(Refused) as raised:
                self.load({"steps": [step]})
            self.assertIn("launch window", str(raised.exception))
        with self.assertRaises(Refused):
            self.load({"launch": "window", "steps": [{"action": "restart_client", "name": "again"}]})

    def test_a_launcher_step_is_bounded_and_carries_what_it_needs(self):
        for step in (dict(self.SHOWS, timeout=0), dict(self.SHOWS, timeout=601), dict(self.SHOWS, patterns=[]), dict(self.SHOWS, patterns=["("]),
                     {"action": "launcher_press", "name": "press", "control": " ", "timeout": 5}):
            with self.assertRaises(Refused):
                self.load({"launch": "window", "steps": [step]})

    def test_the_install_is_escaped_in_a_pattern_and_every_other_variable_is_filled_as_it_is(self):
        variables = {"install": "C:\\Wizard101 (x86)", "host": "127.0.0.2"}
        filled = scenario.fill("{install}|{host}", variables, escape=scenario.LITERAL_IN_PATTERNS)
        self.assertEqual(filled, re.escape("C:\\Wizard101 (x86)") + "|127.0.0.2")
        self.assertTrue(re.fullmatch(filled.split("|")[0], "C:\\Wizard101 (x86)"))
        self.assertEqual(scenario.fill("{install}", variables), "C:\\Wizard101 (x86)")
        with self.assertRaises(Refused):
            self.load({"steps": [{"action": "wait_server_log", "name": "a", "pattern": "(\\d+)", "timeout": 1, "keep": "install"}]})

    def test_an_included_window_launch_carries_over_to_the_scenario_that_includes_it(self):
        self.write_json("base.json", {"title": "base", "launch": "window", "steps": [self.SHOWS]})
        loaded = self.load({"include": "base.json", "steps": [{"action": "launcher_press", "name": "press Play", "control": "Play", "timeout": 5}]},
                           name="more.json")
        self.assertEqual(loaded.launch, "window")

    def test_the_shipped_launcher_scenario_opens_the_window_and_presses_play(self):
        loaded = scenario.load("launcher-window-play.json", search=(paths.SCENARIOS,))
        self.assertEqual(loaded.launch, "window")
        self.assertEqual([step["action"] for step in loaded.steps][:2], ["launcher_shows", "launcher_press"])
        self.assertIn("Online", loaded.steps[0]["patterns"])
        self.assertEqual([step["control"] for step in loaded.steps if step["action"] == "launcher_press"], ["Settings", "Back", "Play"])

    def test_the_launcher_is_given_its_window_instead_of_waiting_on_the_client(self):
        def arguments(**keywords):
            return client.Client("launcher.exe", os.path.join(self.folder, "client"), "127.0.0.2", 12100, (1280, 720), **keywords).arguments()

        windowed = arguments(launch="window")
        self.assertIn("--window-ui", windowed)
        self.assertNotIn("--wait", windowed)
        self.assertEqual(windowed[windowed.index("--window") + 1], "1280x720")
        self.assertIn("--wait", arguments())
        self.assertNotIn("--window-ui", arguments())


class QuitSafelyTests(TemporaryFolder):
    def build(self, rule=True):
        document = dict(REFERENCE_DOCUMENT)
        if rule:
            document["never_quit_after"] = [{"after": r"LOGIN RESPONSE: Error=(?!0)",
                                             "undone_by": "The LoginServer has admitted the user",
                                             "because": "a client that has been refused a login opens a page outside "
                                                        "the machine the moment it is asked to quit"}]
        described = references.References("references.json", document)
        self.client = FakeClient(self.write(os.path.join("client", "WizardClient.log"), []), frame_of(RED, BLUE))
        loaded = scenario.Scenario("test.json", {"title": "x", "steps": []})
        return run.Run({"runs": self.folder}, loaded, described, {})

    def wrote(self, *keys):
        recorded = fixture()["clean"]["client"]
        lines = []
        for key in keys:
            lines.extend(line for line in recorded if key in line)
        self.write(os.path.join("client", "WizardClient.log"), lines, encoding="latin-1")

    def test_a_client_that_was_refused_a_login_is_ended_rather_than_asked_to_quit(self):
        running = self.build()
        self.wrote("LOGIN RESPONSE: Error=996708736")
        said = running.quit_safely(self.client)
        self.assertTrue(running.force_close)
        self.assertIn("ended rather than asked to quit", said)
        self.assertIn("outside the machine", said)

    def test_a_client_that_was_admitted_after_the_refusal_is_asked_to_quit(self):
        running = self.build()
        self.wrote("LOGIN RESPONSE: Error=996708736", "LOGIN RESPONSE: Error=0", "has admitted the user")
        said = running.quit_safely(self.client)
        self.assertFalse(running.force_close)
        self.assertIn("can be asked to quit", said)

    def test_a_client_refused_again_after_being_admitted_is_ended(self):
        running = self.build()
        self.wrote("has admitted the user", "LOGIN RESPONSE: Error=996708736")
        running.quit_safely(self.client)
        self.assertTrue(running.force_close)

    def test_a_client_that_was_never_refused_is_asked_to_quit(self):
        running = self.build()
        self.wrote("About to initialize graphical client")
        said = running.quit_safely(self.client)
        self.assertFalse(running.force_close)
        self.assertIn("can be asked to quit", said)

    def test_a_client_whose_log_yields_nothing_is_ended_rather_than_asked_to_quit(self):
        running = self.build()
        said = running.quit_safely(self.client)
        self.assertTrue(running.force_close)
        self.assertIn("no line to judge it by", said)

    def test_the_last_line_of_the_log_counts_even_before_the_client_ends_it(self):
        running = self.build()
        path = os.path.join(self.folder, "client", "WizardClient.log")
        with open(path, "a", encoding="latin-1", newline="\n") as handle:
            handle.write("09/17/26 15:52:29 [STAT] LoginState     LOGIN RESPONSE: Error=996708736")
        self.assertEqual(self.client.log.poll(), [])
        running.quit_safely(self.client)
        self.assertTrue(running.force_close)

    def test_a_client_that_has_already_stopped_is_not_judged(self):
        running = self.build()
        self.wrote("LOGIN RESPONSE: Error=996708736")
        self.client.living = False
        self.assertIn("already stopped", running.quit_safely(self.client))
        self.assertFalse(running.force_close)

    def test_a_reference_file_without_a_rule_asks_the_client_to_quit(self):
        running = self.build(rule=False)
        self.wrote("LOGIN RESPONSE: Error=996708736")
        self.assertIn("names nothing", running.quit_safely(self.client))
        self.assertFalse(running.force_close)

    def test_the_rule_in_the_repository_reads_the_lines_the_client_writes(self):
        described = references.load(paths.REFERENCES)
        recorded = fixture()["clean"]["client"]
        refused = [line for line in recorded if "Error=996708736" in line]
        admitted = [line for line in recorded if "has admitted the user" in line]
        self.assertTrue(refused and admitted)
        for pattern in described.never_quit_after:
            self.assertTrue(run.holds(refused, pattern))
            self.assertFalse(run.holds(refused + admitted, pattern))
            self.assertFalse(run.holds([line for line in recorded if "Error=0" in line], pattern))


class ScreenSpotTests(unittest.TestCase):
    def test_the_main_client_goes_top_left_and_a_companion_bottom_right_of_the_chosen_screen(self):
        work = (-1920, 0, 0, 1032)
        self.assertEqual(client.screen_spot(work, (1296, 759), "top left"), (-1920, 0))
        self.assertEqual(client.screen_spot(work, (1296, 759), "bottom right"), (-1296, 273))
        self.assertEqual(client.screen_spot((0, 0, 800, 600), (1296, 759), "bottom right"), (0, 0), "a window larger than the screen starts at its corner")


class RunOrderTests(TemporaryFolder):
    def parts(self, server_fails=False, window_fails=False):
        events = self.events = []
        logs = self.folder

        class FakeCapture:
            def __init__(self, *arguments):
                self.note = "capturing"

            def start(self):
                events.append("the capture started")
                return self.note

            def stop(self):
                events.append("the capture stopped")
                return self.note

            def facts(self):
                return {"path": None, "frames": None, "note": self.note}

        class FakeLoginServer:
            def __init__(self, *arguments, **keywords):
                self.log = LogTail(os.path.join(logs, "server", "Login.log"))

            def command(self):
                return ["loginserver.exe"]

            def start(self, timeout=None):
                events.append("the login server started")
                if server_fails:
                    raise StepFailed("the login server never said it was ready")
                return "ready"

            def ensure_account(self, user, password):
                events.append(f"the account {user} was made")
                return user

            def stop(self):
                events.append("the login server stopped")
                return "stopped"

        class FakeRunClient(FakeClient):
            def __init__(self, *arguments, **keywords):
                self.label = keywords.get("label") or "client"
                super().__init__(os.path.join(logs, self.label, "WizardClient.log"), frame_of(RED, BLUE))
                self.install = "C:/Wizard101"
                self.revision = "r806919.Wizard_1_610"
                self.run_folder = logs
                self.command = "WizardGraphicalClient.exe"
                self.started_command = None
                self.frame_source = None
                self.pids = [20, 10]

            def start(self, timeout=None):
                events.append(f"the {self.label} started")
                return "started"

            def find_window(self, timeout=None):
                events.append("the window was looked for" if self.label == "client" else f"the {self.label}'s window was looked for")
                if window_fails:
                    raise StepFailed("the client window did not appear")
                return 0x1234

            def to_background(self):
                events.append(f"the {self.label} went to the back")
                return "at the bottom"

            def close(self, force=False):
                events.append(f"the {self.label} was closed")
                return "closed"

        class FakeGuard:
            def __init__(self, pids, path, started=None, allowances=()):
                self.known = {pid: 0.0 for pid in pids}
                self.path = path

            def start(self):
                events.append("the guard started")

            def remember(self, pids):
                events.append(f"the guard remembered {len(pids)} process(es)")

            def stop(self):
                events.append("the guard stopped")
                return "nothing off this machine"

            def record(self):
                return {"remotes": [], "violations": [], "failed": None}

        made = self.made = []

        class FakeEngine:
            def __init__(self, *arguments, **keywords):
                made.append(arguments)
                self.steps = []
                self.screenshots = []
                self.notes = []
                self.restarts = {}

            def run(self):
                events.append("the scenario ran")
                self.steps.append({"step": "a step", "ok": True, "stage": "scenario", "screen": {"changed": False}})

        class FakeScratch:
            def __init__(self, *arguments):
                self.address = "127.0.0.1:3307"
                self.names = {"login": "ambrose_driver_run_login"}

            def drop(self):
                events.append("the databases were dropped")
                return "dropped"

            def existing(self):
                return []

        return {"Capture": FakeCapture, "LoginServer": FakeLoginServer, "Client": FakeRunClient, "NetGuard": FakeGuard,
                "Engine": FakeEngine, "Scratch": FakeScratch, "kill_leftovers": lambda started, known=(): [],
                "prepare_process": lambda: None, "say": lambda message: None}

    def execute(self, companion=False, launch=None, **behavior):
        install_root = os.path.join(self.folder, "install")
        self.write(os.path.join("install", "Bin", "revision.dat"), ["r806919"])
        loaded = scenario.Scenario("test.json", dict({"title": "x", "steps": []}, **({"companion": {"wizard": WorldEntryTests.WIZARD}} if companion else {}),
                                                     **({"launch": launch} if launch else {})))
        described = references.References("references.json", REFERENCE_DOCUMENT)
        options = {"runs": os.path.join(self.folder, "runs"), "host": "127.0.0.2", "port": 12100,
                   "db_host": "127.0.0.1", "db_port": 3307, "db_user": "ambrose", "db_password": "ambrose",
                   "db_prefix": "ambrose_driver_run", "refs": os.path.join(self.folder, "refs"),
                   "server_timeout": 1, "client_timeout": 1, "capture": True, "background": True}
        environment = {"binaries": self.folder, "server_defaults": "loginserver.conf.dist", "install": install_root,
                       "revision": "r806919.Wizard_1_610", "tshark": "tshark.exe"}
        with mock.patch.multiple(run, **self.parts(**behavior)):
            running = run.Run(options, loaded, described, environment)
            return running, running.execute()

    def test_the_run_starts_and_stops_everything_in_the_order_it_started_it(self):
        running, code = self.execute()
        self.assertEqual(self.events, [
            "the databases were dropped", "the capture started", "the login server started", "the account clientdriver was made",
            "the client started", "the guard started", "the window was looked for", "the client went to the back",
            "the scenario ran", "the client was closed", "the login server stopped", "the capture stopped",
            "the guard stopped", "the databases were dropped"])
        self.assertEqual(code, 0)

    def test_a_companion_starts_after_the_main_client_under_the_same_guard_and_stops_before_it(self):
        running, code = self.execute(companion=True)
        self.assertEqual(self.events, [
            "the databases were dropped", "the capture started", "the login server started", "the account clientdriver was made",
            "the account clientdriver2 was made", "the client started", "the guard started", "the window was looked for",
            "the client went to the back", "the companion started", "the guard remembered 2 process(es)", "the companion's window was looked for",
            "the companion went to the back", "the scenario ran", "the companion was closed", "the client was closed", "the login server stopped",
            "the capture stopped", "the guard stopped", "the databases were dropped"])
        self.assertEqual(code, 0)
        self.assertTrue(running.companion.log.path.endswith(os.path.join("companion", "WizardClient.log")))

    def test_a_launcher_window_run_leaves_the_client_window_to_the_play_its_scenario_presses(self):
        running, code = self.execute(launch="window")
        self.assertEqual(self.events, [
            "the databases were dropped", "the capture started", "the login server started", "the account clientdriver was made",
            "the client started", "the guard started", "the scenario ran", "the client was closed", "the login server stopped",
            "the capture stopped", "the guard stopped", "the databases were dropped"])
        self.assertEqual(code, 0)
        with open(os.path.join(running.folder, "report.json"), "r", encoding="utf-8") as handle:
            self.assertEqual(json.load(handle)["launch"], "window")

    def test_the_run_gives_its_scenario_the_install_and_the_window_size_it_found(self):
        self.execute(launch="window")
        variables = self.made[0][5]
        self.assertEqual(variables["install"], os.path.join(self.folder, "install"))
        self.assertEqual(variables["window"], "40x20")

    def test_a_login_server_that_never_reports_itself_ready_is_still_stopped(self):
        running, code = self.execute(server_fails=True)
        self.assertIn("the login server stopped", self.events)
        self.assertNotIn("the client started", self.events)
        self.assertEqual(code, 1)
        self.assertIn("never said it was ready", running.failed)

    def test_the_guard_watches_from_before_the_window_until_after_the_client_is_gone(self):
        running, code = self.execute(window_fails=True)
        self.assertLess(self.events.index("the guard started"), self.events.index("the window was looked for"))
        self.assertLess(self.events.index("the client was closed"), self.events.index("the guard stopped"))
        self.assertEqual(code, 1)

    def test_a_report_that_cannot_be_built_is_recorded_rather_than_thrown(self):
        with mock.patch.object(run.report, "build", side_effect=ValueError("missing ), unterminated subpattern")):
            running, code = self.execute()
        self.assertEqual(code, 1)
        self.assertIn("the capture stopped", self.events)
        with open(os.path.join(running.folder, "report.json"), "r", encoding="utf-8") as handle:
            written = json.load(handle)
        self.assertFalse(written["clean"])
        self.assertEqual(written["checks"][0]["check"], "the report was built")
        self.assertIn("unterminated subpattern", written["failed"])


class CaptureRefsTests(TemporaryFolder):
    def build(self, replace=False):
        try:
            import PIL
        except ImportError:
            self.skipTest("Pillow is not installed on this machine")
        self.assertTrue(PIL)
        described = references.References("references.json", REFERENCE_DOCUMENT)
        store = screens.Store(described, os.path.join(self.folder, "refs"))
        client = FakeClient(self.write(os.path.join("client", "WizardClient.log"), []), frame_of(RED, BLUE))
        server = FakeServer(self.write(os.path.join("server", "Login.log"), []),
                            self.write(os.path.join("server", "console.txt"), []))
        loaded = scenario.Scenario("test.json", {"title": "x", "steps": []})
        taking = refscapture.ReferenceCapture(loaded, client, server, store, os.path.join(self.folder, "shots"), {},
                                              None, replace=replace)
        return taking, described.crop_file(store.folder, "login")

    def matches(self, path, color):
        return screens.compare(screens.load_png(path), screens.solid(10, 10, color))["fraction"]

    def test_the_first_crop_of_a_screen_is_written(self):
        taking, path = self.build()
        self.assertIn("wrote login", taking.write("login", frame_of(RED, BLUE)))
        self.assertEqual(self.matches(path, RED), 1.0)
        self.assertTrue(taking.written[0]["replaced"])

    def test_a_crop_that_still_looks_like_the_one_it_replaces_takes_its_place(self):
        taking, path = self.build()
        screens.save_png(screens.solid(10, 10, RED), path)
        self.assertIn("matching the old crop", taking.write("login", frame_of((210, 40, 40), BLUE)))
        self.assertTrue(taking.written[0]["replaced"])

    def test_a_crop_of_the_wrong_screen_is_refused_and_left_beside_the_one_it_would_have_replaced(self):
        taking, path = self.build()
        screens.save_png(screens.solid(10, 10, GREEN), path)
        said = taking.write("login", frame_of(RED, BLUE))
        self.assertIn("kept the crop of login", said)
        self.assertIn("--replace", said)
        self.assertEqual(self.matches(path, GREEN), 1.0)
        self.assertEqual(self.matches(path[: -len(".png")] + ".candidate.png", RED), 1.0)
        self.assertFalse(taking.written[0]["replaced"])

    def test_replace_takes_the_crop_of_the_wrong_screen_anyway(self):
        taking, path = self.build(replace=True)
        screens.save_png(screens.solid(10, 10, GREEN), path)
        self.assertIn("wrote login", taking.write("login", frame_of(RED, BLUE)))
        self.assertEqual(self.matches(path, RED), 1.0)
        self.assertTrue(taking.written[0]["replaced"])


class PatchingTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory(prefix="clientdriver-test-")
        self.addCleanup(folder.cleanup)
        self.folder = folder.name

    def write(self, document):
        path = os.path.join(self.folder, "scenario.json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(document, handle)
        return path

    def test_the_shipped_patch_scenarios_watch_both_patch_ports_and_follow_the_default(self):
        off = scenario.load("patch-off.json", search=(paths.SCENARIOS,))
        self.assertEqual(off.patching, "off")
        self.assertEqual(sorted(listener["port"] for listener in off.listeners), [12500, 12700])
        self.assertTrue(all(listener["expect"] == 0 for listener in off.listeners))
        default = scenario.load("patch-default.json", search=(paths.SCENARIOS,))
        self.assertEqual(default.patching, "default")
        self.assertEqual(default.patch_config, {"host": "127.0.0.2", "port": 12700})
        self.assertEqual(off.patch_config, {"host": "127.0.0.2", "port": 12700})
        self.assertEqual([(listener["port"], listener["at_least"]) for listener in default.listeners], [(12700, True)])
        self.assertIn("wait_listener", [step["action"] for step in default.steps])

    def test_a_patching_mode_or_listener_the_driver_does_not_know_is_refused(self):
        base = {"title": "t", "steps": []}
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, patching="on")))
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, listeners=[{"name": "a", "address": "127.0.0.1", "port": 12500}])))
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, listeners=[{"name": "a", "address": "127.0.0.1", "port": 70000, "expect": 0}])))
        twice = {"name": "a", "address": "127.0.0.1", "port": 12500, "expect": 0}
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, listeners=[twice, dict(twice, name="b")])))
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, steps=[{"action": "wait_listener", "listener": "nobody", "timeout": 5}])))
        with self.assertRaises(Refused):
            scenario.load(self.write(dict(base, patching="default", patch_config={"host": "127.0.0.1"})))

    def test_a_listener_counts_every_connection_and_gives_its_port_back(self):
        watched = listeners.PortListener("a test port", "127.0.0.1", 0, expect=2)
        watched.open()
        for _ in range(2):
            with socket.create_connection(("127.0.0.1", watched.port), timeout=5):
                pass
        deadline = time.monotonic() + 5
        while len(watched.connections) < 2 and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertIn("saw 2 connection(s)", watched.stop())
        self.assertEqual(watched.record()["expect"], 2)
        again = listeners.PortListener("the same port", "127.0.0.1", watched.port)
        again.open()
        again.stop()
        taken = listeners.PortListener("a port in use", "127.0.0.1", 0)
        taken.open()
        self.addCleanup(taken.stop)
        with self.assertRaises(StepFailed):
            listeners.PortListener("the same port twice", "127.0.0.1", taken.port).open()

    def test_the_installs_patch_configuration_is_pointed_elsewhere_and_nothing_else_changes(self):
        text = ('<?xml version="1.0" encoding="utf-8" ?> \r\n  <root>\r\n  <PatchServerHostname host="patch.us.wizard101.com" /> \r\n'
                '  <PatchServerPort port="12700" /> \r\n  <LoginHostname host="login.us.wizard101.com" /> \r\n  </root>\r\n')
        pointed = client.pointed_patch_config(text, "127.0.0.1", 12701)
        self.assertEqual(pointed, text.replace("patch.us.wizard101.com", "127.0.0.1").replace('port="12700"', 'port="12701"'))
        with self.assertRaises(StepFailed):
            client.pointed_patch_config(text.replace("PatchServerPort", "Other"), "127.0.0.1", 12701)

    def test_the_launchers_command_is_run_with_only_its_patch_flag_taken_out(self):
        command = '"C:\\Wizard101\\Bin\\WizardGraphicalClient.exe" -L 127.0.0.2 12100 -P 0 -A en-US -D "..\\Data\\GameData\\"'
        self.assertEqual(client.without_patch_flag(command),
                         '"C:\\Wizard101\\Bin\\WizardGraphicalClient.exe" -L 127.0.0.2 12100 -A en-US -D "..\\Data\\GameData\\"')
        with self.assertRaises(StepFailed):
            client.without_patch_flag(command.replace(" -P 0", ""))
        with self.assertRaises(StepFailed):
            client.without_patch_flag(command + " -P 0")


class ReportTests(unittest.TestCase):
    def facts(self, **changes):
        facts = {
            "run_id": "20260917-150000",
            "scenario": "login-to-charselect.json",
            "result": "ok",
            "failed": None,
            "seconds": 32.2,
            "steps": [{"step": "the scratch databases", "ok": True, "stage": "driver"},
                      {"step": "the login window", "ok": True, "stage": "scenario",
                       "screen": {"changed": True, "shot": "01-login-window.png"}}],
            "screenshots": [{"shot": "01-login-window.png", "step": "the login window"}],
            "needs_client": True,
            "install": "C:/Wizard101",
            "install_files": 38412,
            "install_changes": {"added": [], "removed": [], "changed": []},
            "netguard": {"remotes": [{"remote": "WizardGraphicalClient.exe 127.0.0.2:12100"}], "violations": [],
                         "failed": None},
            "leftover_processes": [],
            "databases_after": [],
            "pending_allowed": ["MSG_LOGINLOGCHARACTERCREATION", "MSG_CREATECHARACTER"],
            "server_log_allowed": ["spells TYPE as TPYE", "has no TYPE", "spells TYPE as TYP"],
        }
        facts.update(changes)
        return facts

    def clean_report(self, **changes):
        recorded = fixture()["clean"]
        return report.build(self.facts(**changes), recorded["server"], recorded["client"])

    def passed(self, built, name):
        return [check for check in built["checks"] if check["check"] == name][0]["ok"]

    def test_a_clean_run_passes_every_check(self):
        built = self.clean_report()
        self.assertTrue(built["clean"], [check for check in built["checks"] if not check["ok"]])
        self.assertEqual(len(built["checks"]), 12)

    def test_every_watched_port_must_see_exactly_the_connections_its_scenario_expects(self):
        quiet = {"name": "patch", "address": "127.0.0.1", "port": 12700, "expect": 0, "connections": [], "failed": None}
        name = "every watched port saw the connections the scenario expects"
        self.assertTrue(self.passed(self.clean_report(listeners=[quiet]), name))
        reached = dict(quiet, connections=[{"time": "12:00:00", "peer": "127.0.0.1:50000"}])
        self.assertFalse(self.passed(self.clean_report(listeners=[reached]), name))
        self.assertFalse(self.passed(self.clean_report(listeners=[dict(quiet, failed="it stopped accepting")]), name))
        wanted = dict(quiet, expect=1, at_least=True)
        self.assertFalse(self.passed(self.clean_report(listeners=[wanted]), name))
        twice = dict(wanted, connections=[{"time": "12:00:00", "peer": "127.0.0.1:50000"}, {"time": "12:00:01", "peer": "127.0.0.1:50001"}])
        self.assertTrue(self.passed(self.clean_report(listeners=[twice]), name))

    def test_an_install_that_was_never_read_cannot_certify_that_it_was_only_read(self):
        built = self.clean_report(install_files=0)
        self.assertFalse(self.passed(built, "the install was only read"))
        self.assertIn("nothing was compared", [check["detail"] for check in built["checks"]][0])

    def test_a_run_nothing_watched_cannot_certify_where_the_client_went(self):
        for guard in (None, {"remotes": [], "violations": [], "failed": "the guard stopped early: psutil said no"}):
            built = self.clean_report(netguard=guard)
            self.assertFalse(self.passed(built, "the client contacted only this machine"), guard)

    def test_a_message_the_scenario_allows_does_not_excuse_the_longer_name_beside_it(self):
        line = ("2026-09-18_07:23:18.959 INFO  [network.opcode] Session 3 sent LOGIN MSG_CREATECHARACTERINFO (7:5), "
                "which loginserver does not handle yet")
        recorded = fixture()["clean"]
        built = report.build(self.facts(), recorded["server"] + [line], recorded["client"])
        self.assertFalse(self.passed(built, "every message the server did not handle is one the scenario expects"))
        self.assertIn("MSG_CREATECHARACTERINFO", [check["detail"] for check in built["checks"]
                                                  if check["check"].startswith("every message")][0])

    def test_a_dropped_message_is_not_excused_by_the_list_of_messages_the_server_does_not_handle_yet(self):
        line = ("2026-09-18_07:23:18.959 WARN  [network.opcode] Dropped LOGIN MSG_CREATECHARACTER (7:4) from session 3: "
                "truncated body")
        recorded = fixture()["clean"]
        built = report.build(self.facts(server_log_allowed=["spells TYPE", "has no TYPE", "truncated body"]),
                             recorded["server"] + [line], recorded["client"])
        self.assertFalse(self.passed(built, "the server read every message the client sent"))
        built = report.build(self.facts(dropped_allowed=["MSG_CREATECHARACTER"],
                                        server_log_allowed=["spells TYPE", "has no TYPE", "truncated body"]),
                             recorded["server"] + [line], recorded["client"])
        self.assertTrue(self.passed(built, "the server read every message the client sent"))

    def test_a_client_line_about_a_message_it_does_not_know_fails_the_run(self):
        recorded = fixture()
        built = report.build(self.facts(pending_allowed=["MSG_DELETECHARACTER"]),
                             recorded["dirty"]["server"], recorded["dirty"]["client"])
        self.assertFalse(self.passed(built, "the client understood every message the server sent"))
        allowed = report.build(self.facts(pending_allowed=["MSG_DELETECHARACTER"],
                                          client_log_allowed=["Received an unknown message type: 481"]),
                               recorded["dirty"]["server"], recorded["dirty"]["client"])
        self.assertTrue(self.passed(allowed, "the client understood every message the server sent"))

    def test_it_counts_every_message_the_server_did_not_handle(self):
        built = self.clean_report()
        self.assertEqual(built["unhandled_messages"], {"LOGIN MSG_LOGINLOGCHARACTERCREATION (7:28)": 1,
                                                       "LOGIN MSG_CREATECHARACTER (7:4)": 1})

    def test_it_names_messages_of_numbered_protocols_and_mixed_case_tags(self):
        lines = ["2026-09-25_11:43:05.702 INFO  [network.opcode] Session 1 sent WIZARD2 MSG_CrownShopLogging (53:78), "
                 "which gameserver does not handle yet; later ones from this session are counted, not logged",
                 "2026-09-25_11:43:05.702 INFO  [network.opcode] Session 1 sent WIZARD3 MSG_REQUESTTSDONEPREPFORMP (56:164), "
                 "which gameserver does not handle yet; later ones from this session are counted, not logged"]
        recorded = fixture()["clean"]
        allowed = ["MSG_LOGINLOGCHARACTERCREATION", "MSG_CREATECHARACTER", "MSG_CrownShopLogging", "MSG_REQUESTTSDONEPREPFORMP"]
        built = report.build(self.facts(pending_allowed=allowed), recorded["server"] + lines, recorded["client"])
        self.assertEqual(built["unhandled_messages"]["WIZARD2 MSG_CrownShopLogging (53:78)"], 1)
        self.assertEqual(built["unhandled_messages"]["WIZARD3 MSG_REQUESTTSDONEPREPFORMP (56:164)"], 1)
        self.assertTrue(self.passed(built, "every message the server did not handle is one the scenario expects"))

    def test_a_message_the_scenario_does_not_expect_fails_the_run(self):
        built = self.clean_report(pending_allowed=["MSG_LOGINLOGCHARACTERCREATION"])
        self.assertFalse(self.passed(built, "every message the server did not handle is one the scenario expects"))
        self.assertFalse(built["clean"])

    def test_the_three_warnings_of_the_message_definitions_are_allowed_and_others_are_not(self):
        built = self.clean_report()
        self.assertEqual(len(built["server_warn_error"]), 3)
        self.assertTrue(self.passed(built, "no server WARN, ERROR or FATAL outside the allow-list"))
        recorded = fixture()
        loud = report.build(self.facts(pending_allowed=["MSG_DELETECHARACTER"]), recorded["dirty"]["server"], recorded["dirty"]["client"])
        self.assertFalse(self.passed(loud, "no server WARN, ERROR or FATAL outside the allow-list"))
        self.assertEqual(len(loud["server_warn_error"]), 2)

    def test_a_client_that_opened_a_browser_fails_the_run(self):
        recorded = fixture()
        built = report.build(self.facts(pending_allowed=["MSG_DELETECHARACTER"]), recorded["dirty"]["server"], recorded["dirty"]["client"])
        self.assertFalse(self.passed(built, "the client opened nothing outside itself"))
        self.assertEqual(len(built["client_unknown_messages"]), 1)

    def test_a_changed_install_a_left_process_and_a_left_database_each_fail_the_run(self):
        self.assertFalse(self.passed(self.clean_report(install_changes={"added": ["Bin/state.dat"], "removed": [], "changed": []}),
                                     "the install was only read"))
        self.assertFalse(self.passed(self.clean_report(leftover_processes=["WizardGraphicalClient.exe pid 5"]),
                                     "no process was left running"))
        self.assertFalse(self.passed(self.clean_report(databases_after=["ambrose_driver_run_login"]),
                                     "no database was left behind"))

    def test_an_allowed_connection_passes_and_is_named_in_the_report(self):
        allowed = {"remote": "msedgewebview2.exe 2603:1036:309:8b::2:443", "host": "substrate.office.com", "reason": "runtime behaviour",
                   "since": "2026-10-01", "first_seen": "12:00:00"}
        built = self.clean_report(netguard={"remotes": [{"remote": allowed["remote"]}], "allowed": [allowed], "violations": []})
        check = [check for check in built["checks"] if check["check"] == "the client contacted only this machine"][0]
        self.assertTrue(check["ok"])
        self.assertIn("substrate.office.com by netguard-allow.json (runtime behaviour)", check["detail"])

    def test_a_connection_off_the_machine_fails_the_run(self):
        built = self.clean_report(netguard={"remotes": [], "violations": [{"remote": "203.0.113.5:443"}]})
        self.assertFalse(self.passed(built, "the client contacted only this machine"))

    def test_a_step_that_changed_the_screen_without_a_screenshot_fails_the_run(self):
        built = self.clean_report(steps=[{"step": "the login window", "ok": True, "screen": {"changed": True}}])
        self.assertFalse(self.passed(built, "every step that changed the screen has a screenshot"))

    def test_a_scenario_that_expects_to_fail_is_clean_when_it_fails(self):
        steps = [{"step": "a line the client will never write", "ok": False, "stage": "scenario",
                  "screen": {"changed": True, "shot": "01-fail.png"}}]
        built = self.clean_report(expect_failure=True, result="FAILED", failed="timed out", steps=steps)
        self.assertTrue(built["clean"], [check for check in built["checks"] if not check["ok"]])
        passing = self.clean_report(expect_failure=True)
        self.assertFalse(passing["clean"])

    def test_a_run_that_stopped_before_its_scenario_began_is_not_clean(self):
        built = self.clean_report(result="FAILED", failed="the client window did not appear within 180s", steps=[],
                                  screenshots=[])
        self.assertFalse(self.passed(built, "every step passed"))
        self.assertIn("did not appear", [check["detail"] for check in built["checks"]
                                         if check["check"] == "every step passed"][0])

    def test_a_teardown_that_failed_does_not_stand_in_for_the_step_a_scenario_expects_to_fail(self):
        steps = [{"step": "the login window", "ok": True, "stage": "scenario", "screen": {"changed": False}},
                 {"step": "stop the login server", "ok": False, "stage": "driver", "error": "it ignored the shutdown"}]
        built = self.clean_report(expect_failure=True, steps=steps, screenshots=[])
        self.assertFalse(self.passed(built, "the step the scenario expects to fail did fail"))
        self.assertFalse(self.passed(built, "every step the driver took around the scenario passed"))
        self.assertFalse(built["clean"])

    def test_a_teardown_that_failed_fails_a_run_whose_every_scenario_step_passed(self):
        steps = [{"step": "the login window", "ok": True, "stage": "scenario", "screen": {"changed": False}},
                 {"step": "stop the guard", "ok": False, "stage": "driver", "error": "it never stopped"}]
        built = self.clean_report(steps=steps, screenshots=[])
        self.assertTrue(self.passed(built, "every step passed"))
        self.assertFalse(self.passed(built, "every step the driver took around the scenario passed"))
        self.assertFalse(built["clean"])

    def test_the_markdown_holds_the_checks_and_the_run(self):
        text = report.render(self.clean_report())
        self.assertIn("# Client drive report 20260917-150000", text)
        self.assertIn("| the install was only read | pass |", text)
        self.assertIn("## unhandled_messages", text)

    def test_the_report_is_written_beside_its_json(self):
        with tempfile.TemporaryDirectory(prefix="clientdriver-test-") as folder:
            path = report.write(folder, self.clean_report())
            self.assertTrue(os.path.isfile(path))
            with open(os.path.join(folder, "report.json"), "r", encoding="utf-8") as handle:
                self.assertEqual(json.load(handle)["run_id"], "20260917-150000")


class InstallTests(TemporaryFolder):
    def test_it_names_every_file_added_removed_and_changed(self):
        root = os.path.join(self.folder, "install")
        os.makedirs(os.path.join(root, "Bin"))
        for name in ("Bin/one.dat", "Bin/two.dat"):
            with open(os.path.join(root, name), "w", encoding="utf-8") as handle:
                handle.write("first")
        before = install.snapshot(root)
        self.assertEqual(sorted(before), ["Bin/one.dat", "Bin/two.dat"])
        with open(os.path.join(root, "Bin", "one.dat"), "w", encoding="utf-8") as handle:
            handle.write("longer than the first")
        os.remove(os.path.join(root, "Bin", "two.dat"))
        with open(os.path.join(root, "Bin", "three.dat"), "w", encoding="utf-8") as handle:
            handle.write("new")
        difference = install.diff(before, install.snapshot(root))
        self.assertEqual(difference, {"added": ["Bin/three.dat"], "removed": ["Bin/two.dat"], "changed": ["Bin/one.dat"]})
        self.assertEqual(install.count(difference), 3)

    def test_a_read_install_shows_no_change(self):
        root = os.path.join(self.folder, "install")
        os.makedirs(root)
        with open(os.path.join(root, "revision.dat"), "w", encoding="utf-8") as handle:
            handle.write("r806919")
        before = install.snapshot(root)
        with open(os.path.join(root, "revision.dat"), "r", encoding="utf-8") as handle:
            handle.read()
        self.assertEqual(install.count(install.diff(before, install.snapshot(root))), 0)

    def test_a_folder_that_is_not_there_snapshots_as_nothing(self):
        self.assertEqual(install.snapshot(os.path.join(self.folder, "nowhere")), {})


class DatabaseTests(unittest.TestCase):
    def test_only_the_driver_s_own_databases_are_allowed(self):
        self.assertEqual(database.checked_name("ambrose_driver_run_login"), "ambrose_driver_run_login")
        for refused in ("ambrose_login", "ambrose_characters", "ambrose_test", "mysql", "ambrose_driver_"):
            with self.assertRaises(Refused):
                database.checked_name(refused)

    def test_a_name_that_is_not_plain_is_refused(self):
        with self.assertRaises(Refused):
            database.checked_name("ambrose_driver_a`b")

    def test_the_connection_string_names_the_scratch_database(self):
        scratch = database.Scratch("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose_driver_run")
        self.assertEqual(scratch.info("login"), "127.0.0.1;3307;ambrose;ambrose;ambrose_driver_run_login")
        self.assertEqual(scratch.info("characters"), "127.0.0.1;3307;ambrose;ambrose;ambrose_driver_run_characters")
        self.assertEqual(scratch.address, "127.0.0.1:3307")

    def test_a_prefix_that_is_not_the_driver_s_own_is_refused(self):
        with self.assertRaises(Refused):
            database.Scratch("127.0.0.1", 3307, "ambrose", "ambrose", "ambrose")


class GuardTests(unittest.TestCase):
    def rows(self):
        return [
            {"name": "WizardGraphicalClient.exe", "pid": 20, "ppid": 10, "create_time": 100.0},
            {"name": "BugReporter.exe", "pid": 21, "ppid": 20, "create_time": 140.0},
            {"name": "WizardGraphicalClient.exe", "pid": 30, "ppid": 4, "create_time": 120.0},
            {"name": "WizardGraphicalClient.exe", "pid": 31, "ppid": 4, "create_time": 50.0},
            {"name": "loginserver.exe", "pid": 40, "ppid": 5, "create_time": 99.0},
            {"name": "tshark.exe", "pid": 41, "ppid": 5, "create_time": 99.0},
            {"name": "explorer.exe", "pid": 50, "ppid": 4, "create_time": 130.0},
        ]

    def test_addresses_on_this_machine_are_local_and_others_are_not(self):
        known = {"192.168.1.10", "::1"}
        for address in ("127.0.0.1", "127.0.0.2", "::1", "192.168.1.10", "::ffff:127.0.0.1", "fe80::1%eth0"):
            self.assertTrue(netguard.is_local(address, known | {"fe80::1"}), address)
        for address in ("203.0.113.5", "8.8.8.8", "::ffff:203.0.113.5"):
            self.assertFalse(netguard.is_local(address, known), address)

    def test_only_a_helper_of_the_tree_the_driver_started_is_adopted(self):
        adopted = netguard.adopted(self.rows(), 90.0, {10: 0.0, 20: 100.0})
        self.assertEqual([row["pid"] for row in adopted], [20, 21])

    def test_a_client_the_maintainer_started_during_the_run_is_left_alone(self):
        self.assertEqual(netguard.adopted(self.rows(), 90.0, {}), [])
        left = netguard.leftovers_among(self.rows(), 90.0, {10, 20})
        self.assertNotIn("WizardGraphicalClient.exe pid 30", left)
        self.assertIn("WizardGraphicalClient.exe pid 20", left)

    def test_the_leftovers_are_the_driver_s_own_processes_that_are_still_running(self):
        left = netguard.leftovers_among(self.rows(), 90.0, {5, 20})
        self.assertEqual(left, ["BugReporter.exe pid 21", "WizardGraphicalClient.exe pid 20",
                                "loginserver.exe pid 40", "tshark.exe pid 41"])

    def test_a_process_that_was_running_before_the_run_is_not_a_leftover(self):
        self.assertEqual(netguard.leftovers_among(self.rows(), 90.0, {31}), [])


class NetGuardAllowanceTests(unittest.TestCase):
    ENTRY = {"host": "substrate.office.com", "port": 443, "process": "msedgewebview2.exe", "browser_process_only": True,
             "under": "launcher.exe", "since": "2026-10-01", "reason": "runtime", "evidence": "capture"}

    def networks(self):
        resolve = lambda host, port, proto=0: [(0, 0, 0, "", ("2603:1036:309:8b::2", 443, 0, 0))]
        return {"substrate.office.com": netguard.networks_of("substrate.office.com", resolve)}

    def check(self, name="msedgewebview2.exe", cmdline=("msedgewebview2.exe", "--embedded-browser-webview=1"),
              ancestors=("launcher.exe", "python.exe"), address="2603:1036:309:800::2", port=443, networks=None):
        return netguard.allowance_for([self.ENTRY], name, list(cmdline), list(ancestors), address, port,
                                      self.networks() if networks is None else networks)

    def test_the_web_views_own_call_is_let_through_and_named(self):
        self.assertEqual(self.check()["host"], "substrate.office.com")

    def test_everything_else_is_still_off_the_machine(self):
        self.assertIsNone(self.check(cmdline=("msedgewebview2.exe", "--type=renderer")))
        self.assertIsNone(self.check(ancestors=("explorer.exe",)))
        self.assertIsNone(self.check(port=80))
        self.assertIsNone(self.check(name="WizardGraphicalClient.exe"))
        self.assertIsNone(self.check(address="2603:1037::2"))
        self.assertIsNone(self.check(address="203.0.113.5"))

    def test_a_web_view_that_outlives_its_launcher_is_known_by_the_parents_it_had(self):
        class Named:
            def __init__(self, name):
                self._name = name

            def name(self):
                return self._name

        class Process:
            def __init__(self, pid, parents):
                self.pid = pid
                self._parents = parents

            def parents(self):
                return self._parents

            def cmdline(self):
                return ["msedgewebview2.exe", "--embedded-browser-webview=1"]

        stand_in = mock.Mock(Error=OSError, net_if_addrs=dict)
        with tempfile.TemporaryDirectory() as folder, mock.patch.dict(sys.modules, {"psutil": stand_in}):
            guard = netguard.NetGuard([], os.path.join(folder, "guard.json"), allowances=[self.ENTRY],
                                      resolve=lambda host, port, proto=0: [(0, 0, 0, "", ("2603:1036:309:8b::2", 443, 0, 0))])
            guard.remember_lineage({7: Process(7, [Named("launcher.exe"), Named("python.exe")])})
            orphan = Process(7, [])
            self.assertEqual(guard.allowance(orphan, "msedgewebview2.exe", "2603:1036:309:f::2", 443)["host"], "substrate.office.com")
            self.assertIsNone(guard.allowance(Process(8, []), "msedgewebview2.exe", "2603:1036:309:f::2", 443))

    def test_a_host_that_cannot_be_resolved_lets_nothing_through(self):
        def refuse(host, port, proto=0):
            raise OSError("no such host")
        self.assertEqual(netguard.networks_of("substrate.office.com", refuse), set())
        self.assertIsNone(self.check(networks={"substrate.office.com": set()}))

    def test_an_allowance_file_must_be_complete(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "allow.json")
            with open(path, "w", encoding="utf-8") as handle:
                json.dump([{k: v for k, v in self.ENTRY.items() if k != "evidence"}], handle)
            with self.assertRaises(ValueError):
                netguard.load_allowances(path)
            with open(path, "w", encoding="utf-8") as handle:
                json.dump([dict(self.ENTRY, reason="")], handle)
            with self.assertRaises(ValueError):
                netguard.load_allowances(path)
        self.assertEqual(netguard.load_allowances(os.path.join(tempfile.gettempdir(), "no-such-allow.json")), [])

    def test_the_shipped_allowance_names_only_the_web_views_call(self):
        entries = netguard.load_allowances()
        self.assertEqual([(entry["host"], entry["port"], entry["process"], entry["under"]) for entry in entries],
                         [("substrate.office.com", 443, "msedgewebview2.exe", "launcher.exe")])
        self.assertTrue(entries[0]["browser_process_only"])

class LingeringProcess:
    def __init__(self, pid=4242):
        self.pid = pid

    def poll(self):
        return None


class CaptureTests(TemporaryFolder):
    def build(self):
        taking = capture.Capture("tshark.exe", os.path.join(self.folder, "capture", "login.pcapng"), 12100)
        self.process = LingeringProcess()
        self.ended = []
        taking.end = lambda: self.ended.append(taking.process) or "a kill"
        return taking

    def test_a_tshark_that_never_reports_itself_capturing_is_stopped_and_its_file_closed(self):
        taking = self.build()
        with mock.patch.object(capture.subprocess, "Popen", return_value=self.process):
            said = taking.start(timeout=0.05)
        self.assertIn("tshark did not start", said)
        self.assertEqual(self.ended, [self.process])
        self.assertIsNone(taking.process)
        self.assertIsNone(taking._errors)
        self.assertIsNone(taking.facts()["path"])
        self.assertEqual(taking.stop(), said)

    def test_a_capture_that_was_never_started_stops_without_a_word_about_tshark(self):
        taking = capture.Capture(None, os.path.join(self.folder, "capture", "login.pcapng"), 12100)
        self.assertIn("not installed", taking.start())
        self.assertIn("not installed", taking.stop())


class PreflightTests(unittest.TestCase):
    def environment(self, **changes):
        found = {
            "platform": "win32",
            "windows": True,
            "packages_missing": [],
            "database": "127.0.0.1:3307",
            "binaries": "C:/build/bin/Debug",
            "binaries_reason": "",
            "server_defaults": "C:/build/bin/Debug/loginserver.conf.dist",
            "install": "C:/Wizard101",
            "revision": "r806919.Wizard_1_610",
            "install_reason": "",
            "install_readable": True,
            "database_answers": True,
            "tshark": "C:/Program Files/Wireshark/tshark.exe",
            "capture": True,
            "capture_reason": "",
            "reference_problems": [],
        }
        found.update(changes)
        return found

    def scenario(self):
        return scenario.Scenario("test.json", {"title": "x", "steps": [
            {"action": "wait_screen", "name": "a", "screens": ["login"], "timeout": 1}]})

    def test_a_machine_with_everything_can_run(self):
        self.assertEqual(preflight.missing(self.scenario(), self.environment(), {"capture": True, "client": "C:/Wizard101"}), [])

    def test_every_missing_piece_is_named(self):
        gaps = preflight.missing(self.scenario(), self.environment(
            windows=False, platform="linux", packages_missing=["pywin32"], binaries=None,
            binaries_reason="loginserver.exe and launcher.exe are not built", server_defaults=None,
            database_answers=False, capture=False, capture_reason="tshark is not installed",
            reference_problems=["the reference crops login are not there"]), {"capture": True, "client": "C:/Wizard101"})
        self.assertEqual(len(gaps), 7)
        self.assertIn("only on Windows", gaps[0])
        self.assertIn("pywin32", gaps[1])
        self.assertIn("not built", gaps[2])
        self.assertIn("loginserver.conf.dist", gaps[3])
        self.assertIn("127.0.0.1:3307", gaps[4])
        self.assertIn("tshark", gaps[5])
        self.assertIn("reference crops", gaps[6])

    def test_a_run_is_skipped_until_the_machine_is_asked_for_a_client(self):
        gaps = preflight.missing(self.scenario(), self.environment(), {"capture": True})
        self.assertEqual(len(gaps), 1)
        self.assertIn("AMBROSE_CLIENT_DIR", gaps[0])
        clientless = scenario.Scenario("test.json", {"title": "x", "requires": {"client": False}, "steps": []})
        self.assertEqual(preflight.missing(clientless, self.environment(), {"capture": True}), [])

    def test_an_install_the_driver_cannot_read_is_named(self):
        gaps = preflight.missing(self.scenario(), self.environment(install_readable=False),
                                 {"capture": True, "client": "C:/Wizard101"})
        self.assertEqual(len(gaps), 1)
        self.assertIn("C:/Wizard101", gaps[0])
        self.assertIn("only read it", gaps[0])

    def test_a_missing_install_is_named_once_the_programs_are_there(self):
        gaps = preflight.missing(self.scenario(), self.environment(install=None, revision=None,
                                                                  install_reason="the launcher found no Wizard101 install"),
                                 {"capture": True, "client": "C:/Wizard101"})
        self.assertEqual(gaps, ["the launcher found no Wizard101 install"])

    def test_a_run_without_a_capture_does_not_need_tshark(self):
        gaps = preflight.missing(self.scenario(), self.environment(capture=False, capture_reason="tshark is not installed"),
                                 {"capture": False, "client": "C:/Wizard101"})
        self.assertEqual(gaps, [])

    def test_the_references_must_belong_to_the_install_in_front_of_the_driver(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        problems = preflight.reference_problems(self.scenario(), described, "refs", "r812000.Wizard_1_620", need_crops=False)
        self.assertEqual(len(problems), 1)
        self.assertIn("capture-refs", problems[0])

    def test_a_missing_crop_is_a_problem_only_when_the_run_needs_it(self):
        described = references.References("references.json", REFERENCE_DOCUMENT)
        with tempfile.TemporaryDirectory(prefix="clientdriver-test-") as folder:
            self.assertEqual(preflight.reference_problems(self.scenario(), described, folder, "r806919.Wizard_1_610",
                                                          need_crops=False), [])
            problems = preflight.reference_problems(self.scenario(), described, folder, "r806919.Wizard_1_610")
            self.assertEqual(len(problems), 1)
            self.assertIn("never committed", problems[0])

    def test_a_crop_that_no_longer_fits_the_box_it_is_used_with_is_a_problem(self):
        try:
            import PIL
        except ImportError:
            self.skipTest("Pillow is not installed on this machine")
        self.assertTrue(PIL)
        described = references.References("references.json", REFERENCE_DOCUMENT)
        with tempfile.TemporaryDirectory(prefix="clientdriver-test-") as folder:
            screens.save_png(screens.solid(10, 10, RED), described.crop_file(folder, "login"))
            self.assertEqual(preflight.reference_problems(self.scenario(), described, folder, "r806919.Wizard_1_610"), [])
            screens.save_png(screens.solid(9, 10, RED), described.crop_file(folder, "login"))
            problems = preflight.reference_problems(self.scenario(), described, folder, "r806919.Wizard_1_610")
            self.assertEqual(len(problems), 1)
            self.assertIn("9x10", problems[0])
            self.assertIn("10x10", problems[0])

    def test_the_packages_it_looks_for_are_the_ones_the_requirements_pin(self):
        with open(os.path.join(paths.APP, "requirements.txt"), "r", encoding="utf-8") as handle:
            pinned = {line.split("==")[0].strip().lower() for line in handle if line.strip() and not line.startswith("#")}
        self.assertEqual({distribution.lower() for _module, distribution in preflight.PACKAGES}, pinned)


class PlayServer:
    def __init__(self, program, defaults, folder, host, port, databases, settings=()):
        self.name = os.path.basename(program).split(".")[0]
        self.port = port
        self.settings = list(settings)
        self.living = False

    def start(self, timeout=300):
        PlayServer.events.append(f"start {self.name}")
        self.living = True
        return f"{self.name} ready"

    def alive(self):
        return self.living

    def ensure_account(self, user, password):
        PlayServer.events.append(f"account {user}")
        return f"{user} created"

    def reload(self, target):
        PlayServer.events.append(f"reload {target}")
        return f"{target} is now generation 2"

    def stop(self):
        PlayServer.events.append(f"stop {self.name}")
        self.living = False
        return "stopped"


class PlayDatabases:
    address = "127.0.0.1:3307"
    names = {"login": "ambrose_driver_play_login", "characters": "ambrose_driver_play_characters", "world": "ambrose_driver_play_world"}

    def __init__(self, zones_held):
        self.zones_held = zones_held

    def value(self, kind, query):
        return self.zones_held

    def info(self, kind):
        return f"127.0.0.1;3307;ambrose;ambrose;{self.names[kind]}"


class PlayTests(TemporaryFolder):
    OPTIONS = {"db_host": "127.0.0.1", "db_port": 3307, "db_user": "ambrose", "db_password": "ambrose", "host": "127.0.0.2",
               "port": 12200, "game_port": 12533, "server_timeout": 5, "user": "player", "password": "secret"}
    ENVIRONMENT = {"binaries": "bin", "server_defaults": "loginserver.conf.dist", "install": "install", "revision": "r806919"}

    def session(self, zones_held=12, **options):
        PlayServer.events = []
        launched = []
        session = play.PlaySession(dict(self.OPTIONS, **options), self.ENVIRONMENT, where=self.folder, login_class=PlayServer,
                                   game_class=PlayServer, databases=PlayDatabases(zones_held),
                                   launch=lambda *arguments: launched.append(arguments) or "client started")
        return session, launched

    def test_the_login_server_rereads_its_rows_after_the_game_server_has_written_them(self):
        session, _launched = self.session()
        session.start()
        self.assertEqual(PlayServer.events, ["start loginserver", "account player", "start gameserver", "reload names", "reload creation"])
        self.assertEqual(play.read_state(self.folder)["port"], 12200)

    def test_a_stop_from_another_play_ends_the_session_and_stops_the_game_server_first(self):
        session, launched = self.session()
        play.threading.Timer(0.2, lambda: open(play.stop_path(self.folder), "w").close()).start()
        self.assertEqual(session.execute(read=lambda: (_ for _ in ()).throw(EOFError())), 0)
        self.assertEqual(PlayServer.events[-2:], ["stop gameserver", "stop loginserver"])
        self.assertEqual(launched, [("bin", "127.0.0.2", 12200, None, None, None)])
        self.assertIsNone(play.read_state(self.folder))
        self.assertFalse(os.path.exists(play.stop_path(self.folder)))

    def test_typing_stop_ends_the_session(self):
        session, _launched = self.session()
        self.assertEqual(session.execute(client=False, read=iter(["look", "stop"]).__next__), 0)
        self.assertEqual(PlayServer.events[-2:], ["stop gameserver", "stop loginserver"])

    def test_a_server_that_stops_by_itself_ends_the_session(self):
        session, _launched = self.session()
        play.threading.Timer(0.2, lambda: setattr(session.game, "living", False)).start()
        self.assertEqual(session.execute(client=False, read=lambda: (_ for _ in ()).throw(EOFError())), 0)
        self.assertEqual(PlayServer.events[-1], "stop loginserver")

    def test_an_account_without_a_password_is_refused_and_nothing_is_left_running(self):
        session, launched = self.session(password=None)
        self.assertEqual(session.execute(read=lambda: "stop"), 1)
        self.assertEqual(PlayServer.events, ["start loginserver", "stop gameserver", "stop loginserver"])
        self.assertEqual(launched, [])

    def test_an_empty_world_gets_the_cached_zone_rows_and_a_full_one_is_left_alone(self):
        session, _launched = self.session(zones_held=0)
        applied = []
        session.databases.apply_sql = lambda kind, path: applied.append((kind, path)) or "applied"
        with mock.patch.object(zones, "ensure", return_value=("rows.sql", "read")) as ensure:
            self.assertEqual(session.zone_rows(), "applied")
            ensure.assert_called_once()
        self.assertEqual(applied, [("world", "rows.sql")])
        session.databases.zones_held = 7
        self.assertEqual(session.zone_rows(), "the world database already holds 7 zone(s)")

    def test_stop_waits_until_the_session_has_gone_and_is_a_no_op_without_one(self):
        self.assertEqual(play.ask_to_stop(self.folder, probe=lambda host, port: True), "no play session is running")
        with open(play.state_path(self.folder), "w") as handle:
            json.dump({"host": "127.0.0.2", "port": 12200}, handle)
        self.assertIsNone(play.running(self.folder, probe=lambda host, port: False))
        said = play.ask_to_stop(self.folder, probe=lambda host, port: True, sleep=lambda _seconds: os.remove(play.state_path(self.folder)))
        self.assertEqual(said, "the play session on 127.0.0.2:12200 stopped")
        self.assertTrue(os.path.exists(play.stop_path(self.folder)))

    def test_the_play_databases_are_its_own_and_never_a_run_s(self):
        self.assertTrue(play.PREFIX.startswith(database.PREFIX))
        self.assertNotEqual(play.PREFIX + "_", cli.DEFAULT_DB[4] + "_")
        arguments = cli.build_parser().parse_args(["play", "--no-client"])
        self.assertEqual((arguments.port, arguments.game_port, arguments.client_start), (cli.DEFAULT_PLAY_PORT, cli.DEFAULT_PLAY_GAME_PORT, False))


if __name__ == "__main__":
    unittest.main(verbosity=1)

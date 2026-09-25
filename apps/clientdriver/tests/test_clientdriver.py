# Project Ambrose by Imjustchico
# Self-tests for every part of the client driver that has no client in it: the log tailer against recorded fixtures, the scenario loader with its includes, variables and patterns and the wizard a scenario seeds for the game server, the scratch game server's settings, the zone rows' cache and the copy of a wizard from another database, the reference file, the screen matcher on synthetic frames, the step engine against a fake client and a fake server, the order in which a run starts and stops what it owns, the guard's rule for which processes are its own, the capture that ends what it started, the teardown that decides from the client's own log whether it may be asked to quit, the crop rebuild that refuses a picture of the wrong screen, the report builder against recorded logs, and the check that decides whether a machine can run a scenario.
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from clientdriver import capture, database, engine, install, netguard, paths, preflight, references, refscapture, report, run, scenario, screens, server, zones
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

    def test_the_shipped_enter_world_scenario_loads_with_its_game_server_and_wizard(self):
        loaded = scenario.load("enter-world.json", search=(paths.SCENARIOS,))
        self.assertTrue(loaded.needs_gameserver)
        self.assertEqual(loaded.wizard["zone"], "WizardCity/WC_Ravenwood")
        self.assertIn("charselect_play", loaded.targets_used())

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

    def test_the_zone_rows_are_cached_by_revision_outside_the_repository(self):
        path = zones.cache_path("r806919.Wizard_1_610")
        self.assertTrue(path.endswith(os.path.join("clientdriver", "zones", "r806919.Wizard_1_610.sql")))
        self.assertFalse(os.path.abspath(path).startswith(os.path.abspath(paths.REPOSITORY)))
        with self.assertRaises(StepFailed):
            zones.ensure(self.folder, self.folder, "")

    def test_a_copied_wizard_unpacks_its_name_indices_and_keeps_only_appearance_columns(self):
        row = {"name_indices": (3 << 24) | (100 << 16) | (248 << 8) | 27, "school_id": 2343174, "level": 3, "world": 0,
               "zone": "WizardCity/WC_Ravenwood", "zone_display": "Ravenwood"}
        wizard = database.Scratch.wizard_from_rows(row, {"gender": 1, "race": 79806088, "guid": 9})
        self.assertEqual((wizard["locale"], wizard["first"], wizard["middle"], wizard["last"]), ("en-US", 100, 248, 27))
        self.assertEqual(wizard["appearance"], {"gender": 1, "race": 79806088})
        self.assertEqual(wizard["level"], 3)


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

    def alive(self):
        return self.living

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

    def key(self, virtual_key):
        self.typed.append(virtual_key)

    def click(self, x, y, dwell=0.35):
        self.presses.append((x, y, round(dwell, 2)))
        if self.on_click:
            self.on_click(len(self.presses))
        return f"{x},{y} after {dwell:.2f}s with the window " + ("active" if self.active else "NOT active"), self.active


class FakeServer:
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


class EngineTests(TemporaryFolder):
    def build(self, steps, picture=None, answers=("0",), variables=None, expect_failure=False):
        document = {"title": "a scenario for the tests", "steps": steps}
        if expect_failure:
            document["expect"] = "failure"
        path = self.write_json(os.path.join("scenarios", "test.json"), document)
        loaded = scenario.load(path)
        self.server_log = self.write(os.path.join("server", "Login.log"), [])
        self.console = self.write(os.path.join("server", "console.txt"), [])
        self.client_log = self.write(os.path.join("client", "WizardClient.log"), [])
        self.client = FakeClient(self.client_log, picture if picture is not None else frame_of(RED, BLUE))
        self.server = FakeServer(self.server_log, self.console)
        described = references.References("references.json", REFERENCE_DOCUMENT)
        crops = {"login": screens.solid(10, 10, RED), "charselect": screens.solid(10, 10, GREEN)}
        store = screens.Store(described, "unused", loader=lambda path: crops[os.path.basename(path)[:-4]])
        self.shots = os.path.join(self.folder, "shots")
        os.makedirs(self.shots, exist_ok=True)
        return engine.Engine(loaded, self.client, self.server, store, self.shots,
                             dict(variables or {}, user="clientdriver", password="secret"), FakeDatabases(answers))

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

    def test_a_recorded_line_is_kept_for_the_report(self):
        running = self.build([{"action": "wait_server_log", "name": "the character list", "record": True,
                               "pattern": r"listed (\d+) character", "timeout": 1, "expect": "0"}])
        self.write(os.path.join("server", "Login.log"), ["2026 DEBUG [x] Session 3 listed 0 character(s) of account 1"])
        running.run()
        self.assertEqual(len(running.notes), 1)
        self.assertIn("listed 0 character(s)", running.notes[0]["line"])

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
                events.append("the account was made")
                return user

            def stop(self):
                events.append("the login server stopped")
                return "stopped"

        class FakeRunClient(FakeClient):
            def __init__(self, *arguments, **keywords):
                super().__init__(os.path.join(logs, "client", "WizardClient.log"), frame_of(RED, BLUE))
                self.install = "C:/Wizard101"
                self.revision = "r806919.Wizard_1_610"
                self.run_folder = logs
                self.command = "WizardGraphicalClient.exe"
                self.frame_source = None
                self.pids = [20, 10]

            def start(self, timeout=None):
                events.append("the client started")
                return "started"

            def find_window(self, timeout=None):
                events.append("the window was looked for")
                if window_fails:
                    raise StepFailed("the client window did not appear")
                return 0x1234

            def to_background(self):
                events.append("the client went to the back")
                return "at the bottom"

            def close(self, force=False):
                events.append("the client was closed")
                return "closed"

        class FakeGuard:
            def __init__(self, pids, path, started=None):
                self.known = {pid: 0.0 for pid in pids}
                self.path = path

            def start(self):
                events.append("the guard started")

            def stop(self):
                events.append("the guard stopped")
                return "nothing off this machine"

            def record(self):
                return {"remotes": [], "violations": [], "failed": None}

        class FakeEngine:
            def __init__(self, *arguments, **keywords):
                self.steps = []
                self.screenshots = []
                self.notes = []

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

    def execute(self, **behavior):
        install_root = os.path.join(self.folder, "install")
        self.write(os.path.join("install", "Bin", "revision.dat"), ["r806919"])
        loaded = scenario.Scenario("test.json", {"title": "x", "steps": []})
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
            "the databases were dropped", "the capture started", "the login server started", "the account was made",
            "the client started", "the guard started", "the window was looked for", "the client went to the back",
            "the scenario ran", "the client was closed", "the login server stopped", "the capture stopped",
            "the guard stopped", "the databases were dropped"])
        self.assertEqual(code, 0)

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


if __name__ == "__main__":
    unittest.main(verbosity=1)

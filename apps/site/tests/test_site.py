# Project Ambrose by Imjustchico
# Self-tests for the work board: that a claim, a hold over a phase and a stale claim each change a milestone's status, that a hold nobody owns or that names no milestone is refused, that the board and doc/MILESTONE-TRACK.md never disagree about what is open, and that the page it writes declares every colour it uses.
import datetime
import io
import json
import os
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import build

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
NOW = datetime.datetime(2026, 9, 22, 23, 45, tzinfo=datetime.timezone.utc)


def pull(identifier, login="someone", updated="2026-09-21T10:00:00Z", draft=True):
    return {"number": 130, "title": f"{identifier} something", "headRefName": f"milestone/{identifier}-short",
            "isDraft": draft, "author": {"login": login}, "url": "https://example.invalid/130",
            "createdAt": "2026-09-20T10:00:00Z", "updatedAt": updated}


def state(snapshot=None):
    return build.build_state(ROOT, snapshot or {"pulls": [], "issues": []}, NOW)


def find(built, identifier):
    return next(row for row in built["milestones"] if row["id"] == identifier)


def takeable():
    return next(row["id"] for row in state()["milestones"] if row["status"] == "open")


class BoardTests(unittest.TestCase):
    def test_every_milestone_carries_one_status(self):
        built = state()
        self.assertEqual(len(built["milestones"]), sum(built["counts"].values()))
        for row in built["milestones"]:
            self.assertIn(row["status"], build.STATUS_ORDER, row["id"])

    def test_the_open_list_is_what_the_track_opens(self):
        built = state()
        opened = {row["id"] for row in built["milestones"] if row["status"] == "open"}
        rows, _reserved = build.track_rows(ROOT)
        named = {identifier for row in rows for identifier in row["ids"]}
        landed = {row["id"] for row in built["milestones"] if row["status"] == "landed"}
        self.assertTrue(opened)
        self.assertEqual(opened, named - landed)

    def test_nothing_open_is_also_held(self):
        built = state()
        for row in built["milestones"]:
            if row["status"] == "open":
                self.assertIsNone(build.hold_for(row["id"], built["holds"]), row["id"] + " is open and held at once")

    def test_a_held_phase_holds_every_milestone_in_it(self):
        built = state()
        for entry in built["holds"]:
            if not entry["scope"].startswith("phase:"):
                continue
            phase = int(entry["scope"].split(":")[1])
            inside = [row for row in built["milestones"] if row["phase"] == phase and row["status"] != "landed"]
            self.assertTrue(inside)
            for row in inside:
                self.assertEqual(row["status"], "held", row["id"])

    def test_a_pull_request_claims_its_milestone(self):
        identifier = takeable()
        built = state({"pulls": [pull(identifier)], "issues": []})
        row = find(built, identifier)
        self.assertEqual(row["status"], "building")
        self.assertIn("someone", row["note"])
        self.assertEqual(row["claim"]["kind"], "a draft pull request")

    def test_a_claim_nobody_has_pushed_to_falls_back_to_open(self):
        identifier = takeable()
        built = state({"pulls": [pull(identifier, updated="2026-09-01T10:00:00Z")], "issues": []})
        self.assertEqual(find(built, identifier)["status"], "open")
        self.assertTrue(find(built, identifier)["claim"]["stale"])

    def test_a_claim_issue_counts_as_a_claim(self):
        takeable = next(row["id"] for row in state()["milestones"] if row["status"] == "open")
        issue = {"number": 7, "title": f"Claim: {takeable} something the track opens", "author": {"login": "third"},
                 "url": "https://example.invalid/7", "createdAt": "2026-09-22T10:00:00Z", "updatedAt": "2026-09-22T10:00:00Z"}
        built = state({"pulls": [], "issues": [issue]})
        self.assertEqual(find(built, takeable)["status"], "building")

    def test_a_branch_that_is_not_a_milestone_claims_nothing(self):
        snapshot = {"pulls": [{"number": 9, "headRefName": "contrib/C-60", "author": {"login": "a"}, "url": "u", "updatedAt": "2026-09-22T10:00:00Z"}], "issues": []}
        self.assertEqual(build.claims(snapshot, NOW), {})

    def test_the_other_track_shows_work_that_is_not_a_milestone(self):
        snapshot = {"pulls": [
            {"number": 140, "title": "C-61: a corpus", "headRefName": "contrib/c61", "author": {"login": "someone"}, "url": "u", "updatedAt": "2026-09-22T12:00:00Z"},
            {"number": 141, "title": "bump", "headRefName": "dependabot/npm/x", "author": {"login": "dependabot[bot]"}, "url": "u", "updatedAt": "2026-09-22T12:00:00Z"},
            pull(takeable())], "issues": []}
        rows = build.other_work(snapshot, NOW)
        self.assertEqual([row["number"] for row in rows], [140, 141])
        self.assertEqual(rows[0]["kind"], "the contributor track")
        self.assertEqual(rows[1]["kind"], "a bot")
        built = build.build_state(ROOT, snapshot, NOW)
        self.assertEqual(len(built["other_open_work"]), 2)
        self.assertEqual(find(built, takeable())["status"], "building")

    def test_a_held_milestone_names_who_holds_it_and_what_they_are_on(self):
        built = state()
        held = [row for row in built["milestones"] if row["status"] == "held"]
        self.assertTrue(held)
        for row in held:
            self.assertTrue(row["note"].strip(), row["id"])

    def test_what_a_milestone_unlocks_is_counted(self):
        built = state()
        for row in built["milestones"]:
            self.assertGreaterEqual(row["unlocks"], 0)
        self.assertTrue(any(row["unlocks"] > 0 for row in built["milestones"]))

    def test_a_milestone_waiting_on_one_thing_says_what_that_is(self):
        built = state()
        queued = [row for row in built["milestones"] if row.get("next_after")]
        self.assertTrue(queued)
        for row in queued:
            self.assertEqual(row["status"], "waiting")
            self.assertEqual(row["missing"], [row["next_after"]])
        lines = build.queue(built["milestones"])
        self.assertTrue(lines)
        self.assertTrue(all("finishing it frees" in line for line in lines))

    def test_a_hold_nobody_has_moved_asks_to_be_checked(self):
        later = NOW + datetime.timedelta(days=build.HOLD_REVIEW_DAYS + 1)
        fresh = build.build_state(ROOT, {"pulls": [], "issues": []}, NOW)
        aged = build.build_state(ROOT, {"pulls": [], "issues": []}, later)
        self.assertFalse(any(entry["needs_review"] for entry in fresh["holds"]))
        self.assertTrue(all(entry["needs_review"] for entry in aged["holds"]))
        self.assertTrue(all(entry["days"] >= 0 for entry in fresh["holds"]))

    def test_the_state_file_tells_an_assistant_how_to_read_it(self):
        built = state()
        joined = " ".join(built["how_to_use"]).lower()
        for needle in ("open", "held", "milestone/", "draft pull request", "unticked"):
            self.assertIn(needle, joined)
        self.assertIn("prompt", built["links"])


class HoldTests(unittest.TestCase):
    def hold(self, document):
        folder = tempfile.mkdtemp()
        path = os.path.join(folder, "doc", "work")
        os.makedirs(path)
        with io.open(os.path.join(path, "holds.json"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(document))
        return folder

    def test_a_hold_with_a_scope_nobody_understands_is_refused(self):
        folder = self.hold({"holds": [{"scope": "phase 17", "who": "me"}]})
        with self.assertRaises(build.HoldError):
            build.holds(folder)

    def test_a_hold_with_nobody_holding_it_is_refused(self):
        folder = self.hold({"holds": [{"scope": "phase:17", "what": "the panel"}]})
        with self.assertRaises(build.HoldError):
            build.holds(folder)

    def test_a_hold_on_a_milestone_that_does_not_exist_is_refused(self):
        folder = self.hold({"holds": [{"scope": "milestone:99.99", "who": "me"}]})
        with self.assertRaises(build.HoldError):
            build.holds(folder, known={"4.04": {}})

    def test_broken_json_is_refused_rather_than_read_as_no_holds(self):
        folder = tempfile.mkdtemp()
        os.makedirs(os.path.join(folder, "doc", "work"))
        with io.open(os.path.join(folder, "doc", "work", "holds.json"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write("{\"holds\": [")
        with self.assertRaises(build.HoldError):
            build.holds(folder)

    def test_the_repository_holds_are_valid(self):
        kept = build.holds(ROOT, known=build.ready.milestones(ROOT))
        self.assertTrue(kept)
        for entry in kept:
            self.assertTrue(entry["who"])
            self.assertTrue(entry["what"])


class PageTests(unittest.TestCase):
    def page(self):
        dark, light = build.colours(ROOT)
        return build.page(state(), dark, light)

    def test_every_colour_the_page_uses_is_declared(self):
        text = self.page()
        used = set(re.findall(r"var\(--([a-z0-9-]+)\)", text))
        declared = set(re.findall(r"^\s+--([a-z0-9-]+):", text, re.M))
        self.assertTrue(used)
        self.assertEqual(sorted(used - declared), [])

    def test_the_page_shows_a_card_for_each_open_milestone_and_the_branch_to_use(self):
        built = state()
        text = self.page()
        opened = [row for row in built["milestones"] if row["status"] == "open"]
        self.assertEqual(text.count('class="card open"'), len(opened))
        for row in opened:
            self.assertIn(f"milestone/{row['id']}-", text)

    def test_the_page_carries_no_leftover_template_braces(self):
        text = self.page()
        self.assertNotIn("{{", text)
        self.assertNotIn("}}", text)
        self.assertIn("<title>Project Ambrose work board</title>", text)

    def test_a_title_with_markup_in_it_is_escaped(self):
        row = {"id": "4.04", "title": "World <script>alert(1)</script>", "size": "S", "checks_left": 3,
               "unlocks": 2, "needs": "A build", "why_worth_it": "Because & more"}
        drawn = build.card(row)
        self.assertNotIn("<script>", drawn)
        self.assertIn("&lt;script&gt;", drawn)
        self.assertIn("&amp;", drawn)


if __name__ == "__main__":
    unittest.main(verbosity=1)

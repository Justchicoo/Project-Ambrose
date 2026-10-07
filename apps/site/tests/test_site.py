# Project Ambrose by Imjustchico
# Self-tests for the work board: that a pull request or a claim marks its milestone as being built, from its branch or from the ids its title starts with, that two of them on one milestone both show, that a stale claim stops counting, that every other milestone is open when its dependencies are built and waiting when they are not, with nothing held or reserved, that the track's Started table names only real milestones, and that the page it writes declares every colour it uses.
import datetime
import io
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
    return next((row["id"] for row in state()["milestones"] if row["status"] == "open"), None)


class BoardTests(unittest.TestCase):
    def test_every_milestone_carries_one_status(self):
        built = state()
        self.assertEqual(len(built["milestones"]), sum(built["counts"].values()))
        for row in built["milestones"]:
            self.assertIn(row["status"], build.STATUS_ORDER, row["id"])

    def test_nothing_is_held_or_reserved(self):
        built = state()
        self.assertEqual(build.STATUS_ORDER, ("landed", "building", "open", "waiting"))
        self.assertNotIn("holds", built)
        self.assertFalse(os.path.exists(os.path.join(ROOT, "doc", "work", "holds.json")))

    def test_a_milestone_nobody_is_building_is_open_once_its_dependencies_are_built(self):
        built = state()
        for row in built["milestones"]:
            if row["done"]:
                self.assertEqual(row["status"], "landed", row["id"])
            elif row["missing"]:
                self.assertEqual(row["status"], "waiting", row["id"])
            else:
                self.assertEqual(row["status"], "open", row["id"])
        self.assertTrue(takeable())

    def test_a_pull_request_claims_its_milestone(self):
        identifier = takeable()
        built = state({"pulls": [pull(identifier)], "issues": []})
        row = find(built, identifier)
        self.assertEqual(row["status"], "building")
        self.assertIn("someone", row["note"])
        self.assertEqual(row["claim"]["kind"], "a draft pull request")

    def test_a_pull_request_from_any_branch_claims_the_milestones_its_title_starts_with(self):
        def titled(number, title, branch="claude/some-session"):
            return {"number": number, "title": title, "headRefName": branch, "isDraft": True, "author": {"login": "a"},
                    "url": "u", "createdAt": "2026-09-21T10:00:00Z", "updatedAt": "2026-09-22T10:00:00Z"}
        found = build.claims({"pulls": [titled(1, "8.06: the item template extractor"),
                                        titled(2, "6.07 and 6.13: zone transfers; volumes"),
                                        titled(3, "1.06, 1.07 and 1.08 locale checks"),
                                        titled(4, "4.4 world wire math"),
                                        titled(5, "Installer self-tests: use a bash that takes the paths"),
                                        titled(6, "C-81: a corpus", "contrib/C-81"),
                                        titled(7, "Bump vite from 6.1 to 6.2", "dependabot/npm/vite")], "issues": []}, NOW)
        self.assertEqual(sorted(found), ["1.06", "1.07", "1.08", "4.04", "6.07", "6.13", "8.06"])
        self.assertEqual(found["8.06"][0]["number"], 1)
        self.assertEqual([claim["number"] for claim in found["6.13"]], [2])

    def test_two_pull_requests_on_one_milestone_both_show(self):
        identifier = takeable()
        other = dict(pull(identifier, login="another"), number=131, headRefName="world/x",
                     title=f"{identifier}: the same milestone from a session", createdAt="2026-09-21T09:00:00Z")
        built = state({"pulls": [pull(identifier), other], "issues": []})
        row = find(built, identifier)
        self.assertEqual(row["status"], "building")
        self.assertEqual([claim["who"] for claim in row["claims"]], ["someone", "another"])
        self.assertIn("someone", row["note"])
        self.assertIn("another", row["note"])
        self.assertIn("#131", build.busy_row(row))

    def test_a_claim_nobody_has_pushed_to_falls_back_to_open(self):
        identifier = takeable()
        built = state({"pulls": [pull(identifier, updated="2026-09-01T10:00:00Z")], "issues": []})
        self.assertEqual(find(built, identifier)["status"], "open")
        self.assertTrue(find(built, identifier)["claim"]["stale"])

    def test_a_claim_issue_counts_as_a_claim(self):
        identifier = takeable()
        issue = {"number": 7, "title": f"Claim: {identifier} something", "author": {"login": "third"},
                 "url": "https://example.invalid/7", "createdAt": "2026-09-22T10:00:00Z", "updatedAt": "2026-09-22T10:00:00Z"}
        built = state({"pulls": [], "issues": [issue]})
        self.assertEqual(find(built, identifier)["status"], "building")

    def test_a_branch_that_is_not_a_milestone_claims_nothing(self):
        snapshot = {"pulls": [{"number": 9, "headRefName": "contrib/C-60", "author": {"login": "a"}, "url": "u", "updatedAt": "2026-09-22T10:00:00Z"}], "issues": []}
        self.assertEqual(build.claims(snapshot, NOW), {})

    def test_the_other_track_shows_work_that_is_not_a_milestone(self):
        snapshot = {"pulls": [
            {"number": 140, "title": "C-61: a corpus", "headRefName": "contrib/c61", "author": {"login": "someone"}, "url": "u", "updatedAt": "2026-09-22T12:00:00Z"},
            {"number": 141, "title": "bump", "headRefName": "dependabot/npm/x", "author": {"login": "dependabot[bot]"}, "url": "u", "updatedAt": "2026-09-22T12:00:00Z"},
            {"number": 142, "title": f"{takeable()}: from a session", "headRefName": "claude/x", "author": {"login": "b"}, "url": "u", "updatedAt": "2026-09-22T12:00:00Z"},
            pull(takeable())], "issues": []}
        rows = build.other_work(snapshot, NOW)
        self.assertEqual([row["number"] for row in rows], [140, 141])
        self.assertEqual(rows[0]["kind"], "the contributor track")
        self.assertEqual(rows[1]["kind"], "a bot")
        built = build.build_state(ROOT, snapshot, NOW)
        self.assertEqual(len(built["other_open_work"]), 2)
        self.assertEqual(find(built, takeable())["status"], "building")

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

    def test_a_started_milestone_says_who_started_it_and_what_is_left(self):
        built = state()
        started = [row for row in built["milestones"] if row.get("started")]
        self.assertTrue(started)
        for row in started:
            self.assertTrue(row["started"]["by"].strip(), row["id"])
            self.assertTrue(row["started"]["left"].strip(), row["id"])
        self.assertEqual({row["id"] for row in started}, set(build.track_rows(ROOT)))

    def test_the_state_file_tells_an_assistant_how_to_read_it(self):
        built = state()
        joined = " ".join(built["how_to_use"]).lower()
        for needle in ("open to anyone", "permission", "waiting", "building", "milestone/", "draft pull request", "unticked"):
            self.assertIn(needle, joined)
        self.assertIn("nothing is held or reserved", joined)
        self.assertIn("prompt", built["links"])


class TrackTests(unittest.TestCase):
    def track(self, rows):
        folder = tempfile.mkdtemp()
        os.makedirs(os.path.join(folder, "doc"))
        table = "\n".join(["## Started", "", "| ID | Started by | Sent as | What is left |", "|---|---|---|---|"] + rows + ["", "## Landed", ""])
        with io.open(os.path.join(folder, "doc", "MILESTONE-TRACK.md"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write("# Milestone track\n\n" + table + "\n")
        return folder

    def test_a_started_row_is_read_with_who_and_what_is_left(self):
        folder = self.track(["| 4.04 | someone | on main | the last check, which waits on 4.05 |"])
        self.assertEqual(build.track_rows(folder, known={"4.04": {}}),
                         {"4.04": {"by": "someone", "sent_as": "on main", "left": "the last check, which waits on 4.05"}})

    def test_a_started_row_naming_no_milestone_is_refused(self):
        folder = self.track(["| 99.99 | someone | on main | everything |"])
        with self.assertRaises(build.TrackError):
            build.track_rows(folder, known={"4.04": {}})

    def test_a_track_with_no_started_table_starts_nothing(self):
        folder = tempfile.mkdtemp()
        os.makedirs(os.path.join(folder, "doc"))
        with io.open(os.path.join(folder, "doc", "MILESTONE-TRACK.md"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write("# Milestone track\n")
        self.assertEqual(build.track_rows(folder), {})


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
               "unlocks": 2, "started": {"by": "a <b>", "sent_as": "on main", "left": "Because & more"}}
        drawn = build.card(row)
        self.assertNotIn("<script>", drawn)
        self.assertIn("&lt;script&gt;", drawn)
        self.assertIn("&amp;", drawn)


if __name__ == "__main__":
    unittest.main(verbosity=1)

# Project Ambrose by Imjustchico
# Self-tests for the Discord bot's logic, which need neither Discord nor its library: that the boards are built from the repository's own progress tools, that the progress board remembers the figures before the last change, that only new merges are posted and in order, that a milestone reports what it waits on, that state survives a restart, that only one copy runs from a folder, that a change to the bot's code is noticed, that the old webhook message ids are found, and that the invite asks only for the permissions the bot uses.
import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
import urllib.parse

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

import boards
import bot


def pull(number, merged_at, title="A change"):
    return {"number": number, "title": title, "merged_at": merged_at, "draft": False,
            "html_url": f"https://github.com/{boards.REPOSITORY}/pull/{number}",
            "user": {"login": "someone"}, "base": {"ref": "main"}}


class Boards(unittest.TestCase):
    def test_progress_board_uses_the_announcer_and_the_clone_stamp(self):
        built = boards.progress_embed(ROOT)
        self.assertEqual(built["title"], "Project Ambrose")
        self.assertIn("milestones", built["description"])
        self.assertTrue(built["image"]["url"].startswith(boards.announce.CARD_URL))
        self.assertIn("·", built["footer"]["text"])
        self.assertNotIn("username", built)

    def test_progress_board_says_what_moved_since_the_figures_before(self):
        now = boards.announce.load(ROOT)
        before = json.loads(json.dumps(now))
        before["milestones"]["done"] -= 2
        self.assertIn("2 milestones finished since the last update", boards.progress_embed(ROOT, before)["description"])

    def test_openings_board_lists_ready_milestones(self):
        built = boards.openings_embed(ROOT)
        self.assertEqual(built["title"], "Milestones ready to start")
        self.assertLessEqual(len(built["fields"]), 10)

    def test_advance_keeps_the_previous_figures_only_when_they_change(self):
        state = {"guilds": {}}
        self.assertTrue(boards.advance(state, {"n": 1}))
        self.assertTrue(boards.advance(state, {"n": 2}))
        self.assertFalse(boards.advance(state, {"n": 2}))
        self.assertEqual(state["progress_before"], {"n": 1})
        self.assertEqual(state["progress_now"], {"n": 2})

    def test_milestone_reports_status_and_dependencies(self):
        ids = boards.milestone_ids(ROOT)
        self.assertIn("1.01", ids)
        built = boards.milestone_embed(ROOT, ids[0])
        self.assertTrue(built["title"].startswith(ids[0] + " "))
        self.assertTrue(built["url"].startswith(boards.REPOSITORY_URL + "/blob/main/doc/roadmap/phase-"))
        self.assertIsNone(boards.milestone_embed(ROOT, "99.99"))
        everything = boards.ready.milestones(ROOT)
        waiting = next(value for value in everything.values()
                       if any(name in everything and not everything[name]["done"] for name in value["depends_on"]) and not value["done"])
        self.assertTrue(boards.milestone_embed(ROOT, waiting["id"])["description"].startswith("Waiting on "))


class Merges(unittest.TestCase):
    def test_only_merges_after_the_last_seen_are_new_and_oldest_first(self):
        pulls = [pull(3, "2026-10-07T02:00:00Z"), pull(2, None), pull(1, "2026-10-06T01:00:00Z"), pull(4, "2026-10-07T01:00:00Z")]
        fresh = boards.merged_since(pulls, "2026-10-06T01:00:00Z")
        self.assertEqual([value["number"] for value in fresh], [4, 3])
        self.assertEqual([value["number"] for value in boards.merged_since(pulls, None)], [1, 4, 3])

    def test_merge_and_pull_list_embeds(self):
        built = boards.merged_embed(pull(9, "2026-10-07T02:00:00Z", "Fix the thing"))
        self.assertEqual(built["title"], "Merged #9 Fix the thing")
        listed = boards.pulls_embed([pull(9, None), pull(8, None)])
        self.assertEqual(listed["title"], "2 open pull requests")
        self.assertIn("[#9]", listed["description"])
        self.assertEqual(boards.pulls_embed([])["description"], "Nothing is open right now.")


class State(unittest.TestCase):
    def test_state_round_trips_and_guilds_get_defaults(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "state.json")
            self.assertEqual(boards.load_state(path), {"guilds": {}})
            state = boards.load_state(path)
            entry = boards.guild(state, 42)
            entry["channels"]["progress"] = "7"
            boards.save_state(path, state)
            again = boards.load_state(path)
            self.assertEqual(again["guilds"]["42"], {"channels": {"progress": "7"}, "messages": {}, "retired": {}})
            self.assertFalse(os.path.exists(path + ".tmp"))

    def test_only_one_bot_runs_from_a_folder(self):
        with tempfile.TemporaryDirectory() as folder:
            home = boards.Home(folder)
            first = bot.only_one(home)
            self.assertIsNotNone(first)
            self.assertIsNone(bot.only_one(home))
            first.close()
            again = bot.only_one(home)
            self.assertIsNotNone(again)
            again.close()

    def test_token_comes_from_the_environment_before_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "token.txt")
            self.assertIsNone(boards.read_token(path, "AMBROSE_TEST_UNSET_TOKEN"))
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("from-file\n")
            self.assertEqual(boards.read_token(path, "AMBROSE_TEST_UNSET_TOKEN"), "from-file")
            os.environ["AMBROSE_TEST_SET_TOKEN"] = "from-env"
            try:
                self.assertEqual(boards.read_token(path, "AMBROSE_TEST_SET_TOKEN"), "from-env")
            finally:
                del os.environ["AMBROSE_TEST_SET_TOKEN"]


class Clone(unittest.TestCase):
    def commit(self, root, path, text):
        full = os.path.join(root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as handle:
            handle.write(text)
        boards.git(root, "add", "-A")
        boards.git(root, "-c", "user.name=t", "-c", "user.email=t@example.com", "commit", "-q", "-m", path)
        return boards.head(root)

    def test_a_change_to_the_bots_code_is_noticed(self):
        with tempfile.TemporaryDirectory() as root:
            subprocess.run(["git", "init", "-q", root], check=True)
            first = self.commit(root, "doc/a.md", "a")
            second = self.commit(root, "doc/a.md", "b")
            third = self.commit(root, "apps/discordbot/bot.py", "c")
            self.assertFalse(boards.touched(root, first, second))
            self.assertTrue(boards.touched(root, second, third))
            self.assertFalse(boards.touched(root, None, third))
            self.assertFalse(boards.touched(root, third, third))

    def test_old_webhook_messages_are_found_on_main(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, "doc", "progress"))
            with open(os.path.join(root, "doc", "progress", "discord-message.json"), "w", encoding="utf-8") as handle:
                json.dump({"message_id": "1552081924170711090"}, handle)
            found = boards.retired_ids(root)
            self.assertEqual(found, {"progress": [1552081924170711090], "openings": []})
        self.assertIsNone(boards.remembered_id("not json"))


class Invite(unittest.TestCase):
    def test_invite_asks_for_the_bot_and_its_commands_with_only_the_permissions_used(self):
        query = urllib.parse.parse_qs(urllib.parse.urlparse(boards.invite_url("123")).query)
        self.assertEqual(query["client_id"], ["123"])
        self.assertEqual(query["scope"], ["bot applications.commands"])
        self.assertEqual(int(query["permissions"][0]), 1024 | 2048 | 8192 | 16384 | 65536)

    def test_preview_needs_no_discord(self):
        shown = io.StringIO()
        with contextlib.redirect_stdout(shown):
            self.assertEqual(bot.main(["preview", "--root", ROOT, "--milestone", "1.01"]), 0)
        self.assertEqual(set(json.loads(shown.getvalue())), {"progress", "openings", "milestone"})


if __name__ == "__main__":
    unittest.main()

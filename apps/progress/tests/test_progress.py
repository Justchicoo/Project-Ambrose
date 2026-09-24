# Project Ambrose by Imjustchico
# Self-tests for the progress announcer: that the card is linked from the work board rather than uploaded, so a message can never gather a second one, that the link changes when the figures do so Discord fetches it again, that a message id is remembered and reused, that a message Discord no longer has is posted again while any other refusal is raised, and that a refusal reports what Discord said rather than only its status code.
import io
import json
import os
import sys
import tempfile
import unittest
import urllib.error

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

import announce
import openings


class FakePost:
    def __init__(self, answer=None, failure=None):
        self.calls = []
        self.answer = answer or json.dumps({"id": "1552", "attachments": []})
        self.failure = failure

    def __call__(self, url, payload, method="POST"):
        self.calls.append({"url": url, "payload": payload, "method": method})
        if self.failure is not None and len(self.calls) == 1:
            raise self.failure
        return self.answer, 200


def figures(done=52, checks_done=512):
    return {"milestones": {"done": done, "total": 393, "percent": 13.2},
            "checks": {"done": checks_done, "total": 2387, "percent": 21.4},
            "contributor_track": {"merged": 71, "open": 59},
            "phases": [{"phase": 1, "title": "Foundations", "milestones_done": done, "milestones_total": 22}]}


class CardTests(unittest.TestCase):
    def test_the_card_is_linked_from_the_board_and_never_uploaded(self):
        drawn = announce.embed(figures(), None)["embeds"][0]
        self.assertTrue(drawn["image"]["url"].startswith(announce.CARD_URL))
        self.assertNotIn("attachment://", json.dumps(drawn))

    def test_the_link_changes_when_the_figures_do(self):
        first = announce.card_url(figures())
        again = announce.card_url(figures())
        later = announce.card_url(figures(done=53, checks_done=515))
        self.assertEqual(first, again)
        self.assertNotEqual(first, later)

    def test_the_description_says_what_moved_without_repeating_the_card(self):
        drawn = announce.embed(figures(), figures(done=50, checks_done=500))["embeds"][0]
        self.assertIn("2 milestones finished since the last update.", drawn["description"])
        self.assertNotIn("fields", drawn)
        self.assertIn("13.2% of the plan built", drawn["description"])


class AnnouncerTests(unittest.TestCase):
    def setUp(self):
        self.original = announce.post
        self.payload = {"embeds": [{"title": "Project Ambrose"}]}

    def tearDown(self):
        announce.post = self.original

    def test_an_edit_goes_to_the_remembered_message(self):
        announce.post = FakePost()
        identifier, status, what, held = announce.send("https://discord.invalid/hook", self.payload, "1552")
        self.assertEqual((identifier, status, what, held), ("1552", 200, "edited", 0))
        self.assertEqual(announce.post.calls[0]["method"], "PATCH")
        self.assertIn("/messages/1552", announce.post.calls[0]["url"])
        self.assertEqual(announce.post.calls[0]["payload"]["attachments"], [])

    def test_a_post_declares_nothing_to_clear(self):
        announce.post = FakePost()
        announce.send("https://discord.invalid/hook", self.payload, None)
        self.assertNotIn("attachments", announce.post.calls[0]["payload"])

    def test_the_first_post_is_a_post_and_its_id_comes_back(self):
        announce.post = FakePost()
        identifier, _status, what, _held = announce.send("https://discord.invalid/hook", self.payload, None)
        self.assertEqual((identifier, what), ("1552", "posted"))
        self.assertEqual(announce.post.calls[0]["method"], "POST")

    def test_a_message_discord_no_longer_has_is_posted_again(self):
        gone = urllib.error.HTTPError("u", 404, "Not Found", None, None)
        announce.post = FakePost(failure=gone)
        identifier, _status, what, _held = announce.send("https://discord.invalid/hook", self.payload, "1552")
        self.assertEqual((identifier, what), ("1552", "posted"))
        self.assertEqual([call["method"] for call in announce.post.calls], ["PATCH", "POST"])

    def test_any_other_refusal_is_raised_rather_than_posting_a_second_message(self):
        refused = urllib.error.HTTPError("u", 400, "Bad Request", None, None)
        announce.post = FakePost(failure=refused)
        with self.assertRaises(urllib.error.HTTPError):
            announce.send("https://discord.invalid/hook", self.payload, "1552")

    def test_an_upload_left_on_the_message_is_counted(self):
        announce.post = FakePost(answer=json.dumps({"id": "1552", "attachments": [{"id": "9"}, {"id": "8"}]}))
        _identifier, _status, _what, held = announce.send("https://discord.invalid/hook", self.payload, "1552")
        self.assertEqual(held, 2)

    def test_a_refusal_reports_what_discord_said(self):
        body = io.BytesIO(json.dumps({"message": "Maximum number of allowed attachments in a message reached (10).", "code": 30015}).encode("utf-8"))
        failure = urllib.error.HTTPError("u", 400, "Bad Request", None, body)
        said = announce.reason(failure)
        self.assertIn("30015", said)
        self.assertIn("attachments", said)

    def test_the_remembered_id_survives_a_round_trip_and_a_broken_file(self):
        folder = tempfile.mkdtemp()
        path = os.path.join(folder, "state.json")
        self.assertIsNone(announce.remembered(path))
        announce.remember(path, "1552")
        self.assertEqual(announce.remembered(path), "1552")
        with io.open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("{not json")
        self.assertIsNone(announce.remembered(path))


class OpeningsTests(unittest.TestCase):
    def setUp(self):
        self.original = announce.post
        self.folder = tempfile.mkdtemp()

    def tearDown(self):
        announce.post = self.original

    def run_it(self):
        announce.post = FakePost()
        state = os.path.join(self.folder, "state.json")
        environment = dict(os.environ, AMBROSE_TEST_WEBHOOK="https://discord.com/api/webhooks/1/token")
        saved = os.environ.copy()
        os.environ.update(environment)
        try:
            return openings.main(["--webhook-env", "AMBROSE_TEST_WEBHOOK", "--state", state])
        finally:
            os.environ.clear()
            os.environ.update(saved)

    def test_it_posts_through_the_same_sender_the_progress_message_uses(self):
        self.assertEqual(self.run_it(), 0)
        self.assertEqual(announce.post.calls[0]["method"], "POST")

    def test_its_message_names_the_board_and_the_prompt(self):
        payload = openings.embed(ROOT)
        described = payload["embeds"][0]["description"]
        self.assertIn("justchicoo.github.io/Project-Ambrose", described)
        self.assertIn("AI-MILESTONES-HERE.md", described)
        self.assertTrue(payload["embeds"][0]["fields"])


if __name__ == "__main__":
    unittest.main(verbosity=1)

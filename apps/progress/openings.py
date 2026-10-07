# Project Ambrose by Imjustchico
# Keeps one Discord message saying every roadmap milestone is open to anyone and listing the ones ready to start, read from the phase files and led by those that unlock the most, editing the message it posted last time rather than posting another, and saying what it would post and exiting zero when no webhook is configured.

import argparse
import json
import os
import sys
import urllib.error

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import announce
import ready

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BOARD_URL = "https://justchicoo.github.io/Project-Ambrose/"
PROMPT_URL = "https://github.com/Justchicoo/Project-Ambrose/blob/main/contrib/AI-MILESTONES-HERE.md"
GOLD = 0xE4B457


def opened(root):
    everything = ready.milestones(root)
    unlocks = {identifier: 0 for identifier in everything}
    for milestone in everything.values():
        for dependency in milestone["depends_on"]:
            if dependency in unlocks:
                unlocks[dependency] += 1
    rows, _waiting = ready.state(root)
    for row in rows:
        row["unlocks"] = unlocks.get(row["id"], 0)
    return sorted(rows, key=lambda row: (-row["unlocks"], row["phase"], int(row["id"].split(".")[1])))


def embed(root):
    rows = opened(root)
    fields = []
    for row in rows[:10]:
        left = row["checks_total"] - row["checks_done"]
        value = f'{row["size"]} · {left} check{"s" if left != 1 else ""} to earn · unlocks {row["unlocks"]}'
        fields.append({"name": f'{row["id"]}  {row["title"]}'[:256], "value": value[:1024], "inline": False})
    count = len(rows)
    return {
        "username": "Project Ambrose",
        "embeds": [{
            "title": "Milestones ready to start",
            "url": BOARD_URL,
            "description": ("Every milestone on the roadmap is open to anyone, in any order, and nobody needs to ask. "
                            f"{count} milestone{'s' if count != 1 else ''} from the roadmap {'are' if count != 1 else 'is'} ready, with every dependency built, "
                            "and the rest can be taken too by building what they rest on. "
                            f"The board says who is building what: <{BOARD_URL}>. "
                            f"The prompt for your own AI is in [contrib/AI-MILESTONES-HERE.md]({PROMPT_URL})."),
            "color": GOLD,
            "fields": fields,
            "footer": {"text": "Open a draft pull request from milestone/<id>-<short-name> and the board shows you building it. A check you cannot run stays unticked."},
        }],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose open milestone announcer")
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--webhook-env", default="DISCORD_PROGRESS_WEBHOOK", help="the environment variable holding the Discord webhook URL")
    parser.add_argument("--state", default="doc/progress/discord-openings.json", help="where the id of the message to keep updating is remembered")
    parser.add_argument("--dry-run", action="store_true", help="print the payload instead of posting it")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)

    payload = embed(root)
    if not payload["embeds"][0]["fields"]:
        print("no milestone is ready, so nothing is posted")
        return 0
    url = os.environ.get(args.webhook_env, "").strip()
    if args.dry_run or not url:
        print(json.dumps(payload, indent=2))
        print("no webhook configured, nothing posted" if not url else "dry run, nothing posted")
        return 0
    if not url.startswith("https://discord.com/api/webhooks/") and not url.startswith("https://discordapp.com/api/webhooks/"):
        print("the webhook must be a Discord webhook URL", file=sys.stderr)
        return 1
    try:
        message_id, status, what, _held = announce.send(url, payload, announce.remembered(args.state))
    except urllib.error.HTTPError as failure:
        print(f"the webhook refused the post: {announce.reason(failure)}", file=sys.stderr)
        return 1
    except (urllib.error.URLError, OSError) as failure:
        print(f"the webhook refused the post: {failure}", file=sys.stderr)
        return 1
    if message_id:
        announce.remember(args.state, message_id)
    print(f"{what} message {message_id}, Discord answered {status}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

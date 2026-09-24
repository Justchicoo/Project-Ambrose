# Project Ambrose by Imjustchico
# Keeps one Discord message listing the roadmap milestones open to outside help, read from doc/MILESTONE-TRACK.md's own table and counted against the phase files, editing the message it posted last time rather than posting another, and saying what it would post and exiting zero when no webhook is configured.

import argparse
import io
import json
import os
import re
import sys
import urllib.error

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import announce
import ready

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TRACK = os.path.join("doc", "MILESTONE-TRACK.md")
TRACK_URL = "https://justchicoo.github.io/Project-Ambrose/"
PROMPT_URL = "https://github.com/Justchicoo/Project-Ambrose/blob/main/contrib/AI-MILESTONES-HERE.md"
GOLD = 0xE4B457
ROW = re.compile(r"^\| *([\d.,  ]+?) *\| *(.+?) *\| *(.+?) *\| *(.+?) *\|")


def table(root):
    with io.open(os.path.join(root, TRACK), encoding="utf-8") as handle:
        text = handle.read()
    if "## Open now" not in text:
        return []
    section = text.split("## Open now", 1)[1].split("\n## ", 1)[0]
    rows = []
    for line in section.splitlines():
        found = ROW.match(line)
        if not found or found.group(2).strip("- ") in ("Milestone", ""):
            continue
        identifiers = re.findall(r"\d+\.\d+", found.group(1))
        if identifiers:
            rows.append({"ids": identifiers, "title": found.group(2), "size": found.group(3), "needs": found.group(4)})
    return rows


def checks(root):
    done, _blocked = ready.state(root)
    return {row["id"]: row for row in done}


def embed(root):
    rows = table(root)
    measured = checks(root)
    fields = []
    for row in rows[:10]:
        total = sum(measured[one]["checks_total"] for one in row["ids"] if one in measured)
        ticked = sum(measured[one]["checks_done"] for one in row["ids"] if one in measured)
        left = total - ticked
        name = f'{", ".join(row["ids"])}  {row["title"]}'
        value = f'{row["size"]} · {left} check{"s" if left != 1 else ""} to earn · needs {row["needs"][0].lower() + row["needs"][1:]}'
        fields.append({"name": name[:256], "value": value[:1024], "inline": False})
    count = sum(len(row["ids"]) for row in rows)
    return {
        "username": "Project Ambrose",
        "embeds": [{
            "title": "Milestones open to outside help",
            "url": TRACK_URL,
            "description": (f"{count} milestone{'s' if count != 1 else ''} from the roadmap are open to anyone who wants one. "
                            "Say which you are taking, or open a draft pull request, which holds it. "
                            f"The board says what is taken and what is free: <https://justchicoo.github.io/Project-Ambrose/>. "
                            f"The prompt for your own AI is in [contrib/AI-MILESTONES-HERE.md]({PROMPT_URL})."),
            "color": GOLD,
            "fields": fields,
            "footer": {"text": "Everything not listed here is reserved. A check you cannot run stays unticked."},
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
        print("no milestone is open, so nothing is posted")
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

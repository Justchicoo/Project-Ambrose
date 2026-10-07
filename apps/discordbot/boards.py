# Project Ambrose by Imjustchico
# What the Discord bot shows and remembers, with no Discord library in it: it keeps the bot's own clone of main current, builds the progress, openings, milestone and pull request embeds from the same code the progress tools use, reads merged pull requests from GitHub, lists the webhook messages the bot retires, and keeps the bot's state file.

import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "progress"))

import announce
import openings
import ready

REPOSITORY = "Justchicoo/Project-Ambrose"
REPOSITORY_URL = "https://github.com/" + REPOSITORY
CLONE_URL = REPOSITORY_URL + ".git"
API = "https://api.github.com"
GOLD = 0xE4B457
GREEN = 0x3FA45B
KINDS = ("progress", "openings", "merges")
BOARDS = ("progress", "openings")
BOT_PATHS = ("apps/discordbot/", "apps/progress/")
RETIRED = (("doc/progress/discord-message.json", "progress-state", "discord-message.json"),
           ("doc/progress/discord-openings.json", "openings-state", "discord-openings.json"))
NEWLINE = chr(10)
QUIET = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}


class Home:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.clone = os.path.join(self.path, "repo")
        self.state = os.path.join(self.path, "state.json")
        self.token = os.path.join(self.path, "token.txt")
        self.github_token = os.path.join(self.path, "github-token.txt")
        self.log = os.path.join(self.path, "bot.log")
        self.venv = os.path.join(self.path, "venv")
        self.lock = os.path.join(self.path, "run.lock")


def git(root, *arguments):
    out = subprocess.run(["git", "-C", root, *arguments], capture_output=True, text=True, check=True, **QUIET)
    return out.stdout.strip()


def head(root):
    try:
        return git(root, "rev-parse", "HEAD")
    except (OSError, subprocess.CalledProcessError):
        return None


def stamp(root):
    try:
        date, commit = git(root, "log", "-1", "--format=%cs %h").split()
        return f"{date} · {commit}"
    except (OSError, subprocess.CalledProcessError, ValueError):
        return "counted from the roadmap itself"


def clone(home):
    if os.path.isdir(os.path.join(home.clone, ".git")):
        return False
    os.makedirs(home.path, exist_ok=True)
    subprocess.run(["git", "clone", "--depth", "1", "--branch", "main", CLONE_URL, home.clone], capture_output=True, text=True, check=True, **QUIET)
    return True


def touched(root, before, after):
    if not before or before == after:
        return False
    try:
        changed = git(root, "diff", "--name-only", before, after).splitlines()
    except (OSError, subprocess.CalledProcessError):
        return True
    return any(path.startswith(BOT_PATHS) for path in changed)


def sync(home):
    before = head(home.clone)
    git(home.clone, "fetch", "--quiet", "--depth", "1", "origin", "main")
    git(home.clone, "reset", "--quiet", "--hard", "FETCH_HEAD")
    after = head(home.clone)
    return after, touched(home.clone, before, after)


def load_state(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            state = json.load(handle)
    except (OSError, json.JSONDecodeError):
        state = {}
    state.setdefault("guilds", {})
    return state


def save_state(path, state):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(state, handle, indent=2, sort_keys=True)
        handle.write(NEWLINE)
    os.replace(temporary, path)


def guild(state, guild_id):
    entry = state["guilds"].setdefault(str(guild_id), {})
    entry.setdefault("channels", {})
    entry.setdefault("messages", {})
    entry.setdefault("retired", {})
    return entry


def advance(state, now):
    if state.get("progress_now") != now:
        state["progress_before"] = state.get("progress_now")
        state["progress_now"] = now
        return True
    return False


def progress_embed(root, before=None):
    now = announce.load(root)
    built = announce.embed(now, before)["embeds"][0]
    built["footer"] = {"text": stamp(root)}
    return built


def openings_embed(root):
    built = openings.embed(root)["embeds"][0]
    if not built["fields"]:
        built["description"] = "Every milestone on the roadmap is open to anyone. None is ready with every dependency built right now, so take one by building what it rests on."
    return built


def board_embed(kind, root, before=None):
    return progress_embed(root, before) if kind == "progress" else openings_embed(root)


def milestone_ids(root):
    return sorted(ready.milestones(root), key=lambda value: tuple(int(part) for part in value.split(".")))


def milestone_embed(root, identifier):
    everything = ready.milestones(root)
    milestone = everything.get(identifier.strip())
    if not milestone:
        return None
    missing = [name for name in milestone["depends_on"] if name not in everything or not everything[name]["done"]]
    if milestone["done"]:
        status = "Built, every acceptance check ticked."
    elif missing:
        status = "Waiting on " + ", ".join(missing) + ", which anyone may build first."
    else:
        status = "Ready to start, with every dependency built. Anyone may take it."
    depends = ", ".join(milestone["depends_on"]) or "nothing"
    return {
        "title": f'{milestone["id"]} {milestone["title"]}'[:256],
        "url": f'{REPOSITORY_URL}/blob/main/{milestone["file"]}',
        "description": status,
        "color": GREEN if milestone["done"] else GOLD,
        "fields": [
            {"name": "Size", "value": milestone["size"], "inline": True},
            {"name": "Checks", "value": f'{milestone["checks_done"]} of {milestone["checks_total"]}', "inline": True},
            {"name": "Depends on", "value": depends[:1024], "inline": False},
        ],
        "footer": {"text": stamp(root)},
    }


def github(path, token=None, query=None):
    url = API + path + ("?" + urllib.parse.urlencode(query) if query else "")
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "Project-Ambrose-bot", "X-GitHub-Api-Version": "2022-11-28"}
    if token:
        headers["Authorization"] = "Bearer " + token
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=30) as answer:
        return json.loads(answer.read().decode("utf-8"))


def open_pulls(token=None):
    return github(f"/repos/{REPOSITORY}/pulls", token, {"state": "open", "per_page": 30, "sort": "updated", "direction": "desc"})


def merged_since(pulls, since):
    merged = [pull for pull in pulls if pull.get("merged_at") and (since is None or pull["merged_at"] > since)]
    return sorted(merged, key=lambda pull: pull["merged_at"])


def recent_merges(token=None):
    return github(f"/repos/{REPOSITORY}/pulls", token, {"state": "closed", "per_page": 30, "sort": "updated", "direction": "desc"})


def pulls_embed(pulls):
    lines = []
    for pull in pulls[:15]:
        draft = " (draft)" if pull.get("draft") else ""
        lines.append(f'[#{pull["number"]}]({pull["html_url"]}) {pull["title"][:90]}{draft} · {pull["user"]["login"]}')
    count = len(pulls)
    return {
        "title": f'{count} open pull request{"s" if count != 1 else ""}',
        "url": REPOSITORY_URL + "/pulls",
        "description": NEWLINE.join(lines) if lines else "Nothing is open right now.",
        "color": GOLD,
    }


def merged_embed(pull):
    return {
        "title": f'Merged #{pull["number"]} {pull["title"]}'[:256],
        "url": pull["html_url"],
        "description": f'Opened by {pull["user"]["login"]}, merged into {pull["base"]["ref"]}.',
        "color": GREEN,
        "timestamp": pull["merged_at"],
    }


def remembered_id(text):
    try:
        found = json.loads(text).get("message_id")
    except (json.JSONDecodeError, AttributeError, TypeError):
        return None
    return int(found) if found and str(found).isdigit() else None


def retired_ids(root):
    found = {}
    for kind, (path, branch, name) in zip(BOARDS, RETIRED):
        ids = set()
        try:
            with open(os.path.join(root, path), "r", encoding="utf-8") as handle:
                ids.add(remembered_id(handle.read()))
        except OSError:
            pass
        try:
            git(root, "fetch", "--quiet", "--depth", "1", "origin", f"{branch}:refs/remotes/origin/{branch}")
            ids.add(remembered_id(git(root, "show", f"origin/{branch}:{name}")))
        except (OSError, subprocess.CalledProcessError):
            pass
        found[kind] = sorted(value for value in ids if value)
    return found


def read_token(path, variable):
    value = os.environ.get(variable, "").strip()
    if value:
        return value
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read().strip() or None
    except OSError:
        return None


def discord_get(path, token):
    request = urllib.request.Request("https://discord.com/api/v10" + path,
                                     headers={"Authorization": "Bot " + token, "User-Agent": "DiscordBot (https://github.com/Justchicoo/Project-Ambrose, 1)"})
    with urllib.request.urlopen(request, timeout=30) as answer:
        return json.loads(answer.read().decode("utf-8"))


PERMISSIONS = 1024 | 2048 | 16384 | 65536 | 8192


def invite_url(application_id):
    query = urllib.parse.urlencode({"client_id": application_id, "scope": "bot applications.commands", "permissions": PERMISSIONS})
    return "https://discord.com/oauth2/authorize?" + query

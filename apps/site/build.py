# Project Ambrose by Imjustchico
# Builds the work board the project publishes: one page a person reads and one state file a contributor's assistant reads, both saying for every milestone whether it is landed, being built right now, held by the maintainer, open to anyone, or waiting on a dependency, from the phase files, the two tracks, the holds the maintainer's own sessions take, and a snapshot of the open pull requests and claims.

import argparse
import datetime
import html
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "progress"))

import ready

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TRACK = os.path.join("doc", "MILESTONE-TRACK.md")
CONTRIBUTOR_TRACK = os.path.join("doc", "CONTRIBUTOR-TRACK.md")
HOLDS = os.path.join("doc", "work", "holds.json")
PROGRESS = os.path.join("doc", "progress", "progress.json")
VARIABLES = os.path.join("packages", "ui", "src", "tokens", "variables.css")
OUT_DIR = "site"
PAGE = "index.html"
STATE = "state.json"

REPOSITORY = "https://github.com/Justchicoo/Project-Ambrose"
DISCORD = "https://discord.gg/Dx6ACDUj6N"
PROMPT = REPOSITORY + "/blob/main/contrib/AI-MILESTONES-HERE.md"
TRACK_URL = REPOSITORY + "/blob/main/doc/MILESTONE-TRACK.md"
CONTRIBUTOR_URL = REPOSITORY + "/blob/main/doc/CONTRIBUTOR-TRACK.md"
CLAIM_URL = REPOSITORY + "/issues/new?template=claim_milestone.yml"
STATE_URL = "https://justchicoo.github.io/Project-Ambrose/state.json"

STALE_DAYS = 14
HOLD_REVIEW_DAYS = 21
MILESTONE_BRANCH = re.compile(r"^milestone/(\d+)\.(\d+)")
CLAIM_TITLE = re.compile(r"(\d+\.\d+)")
OPEN_ROW = re.compile(r"^\| *([\d.,  ]+?) *\| *(.+?) *\| *(.+?) *\| *(.+?) *\| *(.+?) *\|$")
SIMPLE_ROW = re.compile(r"^\| *([\d.,  ]+?) *\| *(.+?) *\|")
SCOPE = re.compile(r"^(phase:\d+|milestone:\d+\.\d+)$")

STATUS_ORDER = ("landed", "building", "held", "open", "waiting", "reserved")

HOW_TO_USE = [
    "This file is the live state of the project. Read it before claiming anything, and read it again before you push.",
    "Take a milestone only where status is 'open'. Anything 'held' is being built by the maintainer's own sessions, anything 'building' is somebody else's, and anything 'waiting' has a dependency that is not finished.",
    "A hold can cover a whole phase. If phase 17 is held, every milestone in it is held, whatever its own row says.",
    "Claim by opening a draft pull request from a branch named milestone/<id>-<short-name>, which is also what lets CI accept a change under src/. The board picks that up by itself.",
    "A milestone is finished only when every acceptance check in its phase file is ticked with the evidence that proved it. A check you cannot run stays unticked and is named in the pull request.",
    "A milestone carrying next_after is waiting on exactly that one milestone, so it is what to line up next rather than what to start. A hold carrying needs_review has not moved in weeks: ask in the Discord rather than assuming it is still held.",
    "The prompt for your own assistant is at " + PROMPT + ", and the rules it is held to are at " + TRACK_URL + ".",
]


def read(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def load_json(path, fallback):
    try:
        return json.loads(read(path))
    except (OSError, ValueError):
        return fallback


def colours(root):
    text = read(os.path.join(root, VARIABLES))
    dark = text.split(':root[data-theme="light"]')[0]
    light = text.split(':root[data-theme="light"]')[1].split("}")[0] if ':root[data-theme="light"]' in text else ""
    pattern = r"--ambrose-color-([a-z0-9-]+):\s*(#[0-9A-Fa-f]{6})"
    return dict(re.findall(pattern, dark)), dict(re.findall(pattern, light))


def track_rows(root):
    text = read(os.path.join(root, TRACK))

    def section(name):
        if "## " + name not in text:
            return ""
        return text.split("## " + name, 1)[1].split("\n## ", 1)[0]

    opened = []
    for line in section("Open now").splitlines():
        found = OPEN_ROW.match(line)
        if found and re.findall(r"\d+\.\d+", found.group(1)):
            opened.append({"ids": re.findall(r"\d+\.\d+", found.group(1)), "title": found.group(2),
                           "size": found.group(3), "needs": found.group(4), "why": found.group(5)})
    reserved = {}
    for line in section("Reserved").splitlines():
        found = SIMPLE_ROW.match(line)
        if found:
            for identifier in re.findall(r"\d+\.\d+", found.group(1)):
                reserved[identifier] = found.group(2)
    return opened, reserved


def contributor_counts(root):
    text = read(os.path.join(root, CONTRIBUTOR_TRACK))
    head, _, tail = text.partition("### Merged so far")
    return {"open": len(re.findall(r"^\| [FC]-\d+ \|", head, re.M)),
            "merged": len(re.findall(r"^\| [FC]-\d+ \|", tail, re.M))}


class HoldError(Exception):
    pass


def holds(root, known=None):
    path = os.path.join(root, HOLDS)
    if not os.path.exists(path):
        return []
    try:
        document = json.loads(read(path))
    except ValueError as failure:
        raise HoldError(f"{HOLDS} is not readable JSON: {failure}")
    kept = []
    for entry in document.get("holds", []):
        scope = str(entry.get("scope", ""))
        if not SCOPE.match(scope):
            raise HoldError(f"{HOLDS} holds '{scope}', which is neither phase:<number> nor milestone:<id>")
        if known is not None and scope.startswith("milestone:") and scope.split(":", 1)[1] not in known:
            raise HoldError(f"{HOLDS} holds {scope}, which is no milestone in the roadmap")
        if not entry.get("who"):
            raise HoldError(f"{HOLDS} holds {scope} without saying who holds it")
        kept.append({"scope": scope, "who": entry["who"],
                     "what": entry.get("what", ""), "since": entry.get("since", "")})
    return kept


def hold_for(identifier, kept):
    phase = identifier.split(".")[0]
    for entry in kept:
        if entry["scope"] == "milestone:" + identifier or entry["scope"] == "phase:" + phase:
            return entry
    return None


def moment(text):
    try:
        return datetime.datetime.fromisoformat((text or "").replace("Z", "+00:00"))
    except ValueError:
        return None


def claims(snapshot, now):
    found = {}
    for pull in snapshot.get("pulls", []):
        branch = MILESTONE_BRANCH.match(pull.get("headRefName", "") or "")
        if not branch:
            continue
        identifier = f"{branch.group(1)}.{int(branch.group(2)):02d}"
        when = moment(pull.get("updatedAt"))
        found[identifier] = {
            "who": (pull.get("author") or {}).get("login", "somebody"),
            "url": pull.get("url", ""),
            "kind": "a draft pull request" if pull.get("isDraft") else "a pull request",
            "number": pull.get("number"),
            "since": (pull.get("createdAt") or "")[:10],
            "updated": (pull.get("updatedAt") or "")[:10],
            "stale": bool(when and (now - when).days >= STALE_DAYS),
        }
    for issue in snapshot.get("issues", []):
        for identifier in CLAIM_TITLE.findall(issue.get("title", "") or ""):
            if identifier in found:
                continue
            when = moment(issue.get("updatedAt"))
            found[identifier] = {
                "who": (issue.get("author") or {}).get("login", "somebody"),
                "url": issue.get("url", ""),
                "kind": "a claim",
                "number": issue.get("number"),
                "since": (issue.get("createdAt") or "")[:10],
                "updated": (issue.get("updatedAt") or "")[:10],
                "stale": bool(when and (now - when).days >= STALE_DAYS),
            }
    return found


def other_work(snapshot, now):
    rows = []
    for pull in snapshot.get("pulls", []):
        branch = pull.get("headRefName", "") or ""
        if MILESTONE_BRANCH.match(branch):
            continue
        author = (pull.get("author") or {}).get("login", "somebody")
        kind = "a bot" if author.endswith("[bot]") else ("the contributor track" if branch.startswith("contrib/") else "something else")
        rows.append({"number": pull.get("number"), "title": pull.get("title", ""), "who": author, "kind": kind,
                     "url": pull.get("url", ""), "branch": branch, "updated": (pull.get("updatedAt") or "")[:10],
                     "draft": bool(pull.get("isDraft"))})
    return rows


def unlocked_by(everything):
    counts = {identifier: 0 for identifier in everything}
    for milestone in everything.values():
        for dependency in milestone["depends_on"]:
            if dependency in counts:
                counts[dependency] += 1
    return counts


def status_of(milestone, opened_ids, kept, taken):
    if milestone["done"]:
        return "landed", ""
    claim = taken.get(milestone["id"])
    if claim and not claim["stale"]:
        return "building", f'{claim["who"]} has {claim["kind"]}'
    held = hold_for(milestone["id"], kept)
    if held:
        what = held["what"] or "work in flight"
        since = f' since {held["since"]}' if held.get("since") else ""
        return "held", f'{held["who"]}: {what}{since}'
    if milestone["missing"]:
        return "waiting", "waiting on " + ", ".join(milestone["missing"])
    if milestone["id"] in opened_ids:
        return "open", "open to anyone"
    return "reserved", ""


def build_state(root, snapshot, now):
    everything = ready.milestones(root)
    opened, reserved = track_rows(root)
    kept = holds(root, everything)
    taken = claims(snapshot, now)
    unlocks = unlocked_by(everything)
    opened_ids = {identifier for row in opened for identifier in row["ids"]}
    needs = {identifier: row for row in opened for identifier in row["ids"]}

    rows = []
    for identifier, milestone in sorted(everything.items(), key=lambda pair: (int(pair[0].split(".")[0]), int(pair[0].split(".")[1]))):
        entry = dict(milestone)
        entry["missing"] = [name for name in milestone["depends_on"] if name not in everything or not everything[name]["done"]]
        status, note = status_of(entry, opened_ids, kept, taken)
        entry["status"] = status
        entry["note"] = note
        entry["unlocks"] = unlocks.get(identifier, 0)
        entry["checks_left"] = entry["checks_total"] - entry["checks_done"]
        entry["branch"] = f"milestone/{identifier}-<short-name>"
        if identifier in needs:
            entry["needs"] = needs[identifier]["needs"]
            entry["why_worth_it"] = needs[identifier]["why"]
        if identifier in reserved:
            entry["reserved_because"] = reserved[identifier]
        if identifier in taken:
            entry["claim"] = taken[identifier]
        if status == "waiting" and len(entry["missing"]) == 1:
            entry["next_after"] = entry["missing"][0]
        rows.append(entry)

    phases = {}
    for row in rows:
        phase = phases.setdefault(row["phase"], {"phase": row["phase"], "milestones": 0, "landed": 0, "open": 0, "building": 0, "held": False})
        phase["milestones"] += 1
        phase["landed"] += 1 if row["status"] == "landed" else 0
        phase["open"] += 1 if row["status"] == "open" else 0
        phase["building"] += 1 if row["status"] == "building" else 0
    for entry in kept:
        since = moment(entry["since"] + "T00:00:00Z") if entry["since"] else None
        entry["days"] = (now - since).days if since else None
        entry["needs_review"] = bool(entry["days"] is not None and entry["days"] >= HOLD_REVIEW_DAYS)
    for entry in kept:
        if entry["scope"].startswith("phase:"):
            number = int(entry["scope"].split(":")[1])
            if number in phases:
                phases[number]["held"] = True
                phases[number]["held_by"] = entry["who"]

    counted = {status: len([row for row in rows if row["status"] == status]) for status in STATUS_ORDER}
    return {
        "schema": 1,
        "generated_at": now.replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "how_to_use": HOW_TO_USE,
        "links": {"repository": REPOSITORY, "milestone_track": TRACK_URL, "contributor_track": CONTRIBUTOR_URL,
                  "prompt": PROMPT, "claim": CLAIM_URL, "discord": DISCORD, "state": STATE_URL},
        "progress": load_json(os.path.join(root, PROGRESS), {}),
        "contributor_track": contributor_counts(root),
        "other_open_work": other_work(snapshot, now),
        "counts": counted,
        "holds": kept,
        "phases": [phases[number] for number in sorted(phases)],
        "milestones": rows,
    }


def bar(done, total, colour, background, height=8):
    share = 0 if not total else round(100 * done / total, 1)
    return (f'<div class="bar" style="background:{background}" role="img" aria-label="{done} of {total}">'
            f'<span style="width:{share}%;background:{colour};height:{height}px"></span></div>')


def escape(text):
    return html.escape(str(text), quote=True)


def card(row):
    pieces = [f'<h3><span class="id">{escape(row["id"])}</span> {escape(row["title"])}</h3>']
    facts = [f'size {escape(row["size"])}', f'{row["checks_left"]} check{"s" if row["checks_left"] != 1 else ""} to earn']
    if row.get("unlocks"):
        facts.append(f'unlocks {row["unlocks"]}')
    pieces.append('<p class="facts">' + " &middot; ".join(facts) + "</p>")
    if row.get("needs"):
        pieces.append(f'<p class="needs">Needs {escape(row["needs"][0].lower() + row["needs"][1:])}</p>')
    if row.get("why_worth_it"):
        pieces.append(f'<p class="why">{escape(row["why_worth_it"])}</p>')
    pieces.append(f'<p class="branch"><code>git switch -c milestone/{escape(row["id"])}-&lt;short-name&gt;</code></p>')
    pieces.append(f'<p class="take"><a href="{CLAIM_URL}">Claim it</a> or open a draft pull request from that branch.</p>')
    return '<article class="card open">' + "".join(pieces) + "</article>"


def busy_row(row):
    claim = row.get("claim") or {}
    if row["status"] == "building":
        who = escape(claim.get("who", "somebody"))
        link = f'<a href="{escape(claim.get("url", REPOSITORY))}">#{escape(claim.get("number", ""))}</a>'
        return f'<li><span class="id">{escape(row["id"])}</span> {escape(row["title"])} <span class="who">{who}, {escape(claim.get("kind", ""))} {link}, since {escape(claim.get("since", ""))}</span></li>'
    return f'<li><span class="id">{escape(row["id"])}</span> {escape(row["title"])} <span class="who">{escape(row["note"])}</span></li>'


def queue(rows, limit=6):
    blockers = {}
    for row in rows:
        if row.get("next_after"):
            blockers.setdefault(row["next_after"], []).append(row["id"])
    known = {row["id"]: row for row in rows}
    ordered = sorted(blockers.items(), key=lambda pair: (-len(pair[1]), pair[0]))
    lines = []
    for identifier, waiting in ordered[:limit]:
        blocker = known.get(identifier)
        if not blocker:
            continue
        status = blocker["status"]
        who = f' ({blocker["note"]})' if blocker["note"] else ""
        lines.append(f'<li><span class="id">{escape(identifier)}</span> {escape(blocker["title"])}'
                     f'<span class="who">{escape(status)}{escape(who)} &middot; finishing it frees '
                     f'{", ".join(escape(one) for one in waiting[:8])}'
                     + (f' and {len(waiting) - 8} more' if len(waiting) > 8 else "") + "</span></li>")
    return lines


def phase_row(phase, dark):
    held = ' <span class="chip held">held</span>' if phase.get("held") else ""
    building = f' <span class="chip building">{phase["building"]} being built</span>' if phase["building"] else ""
    free = f' <span class="chip open">{phase["open"]} open</span>' if phase["open"] else ""
    return (f'<li><div class="phase-head"><span>Phase {phase["phase"]}</span>'
            f'<span class="count">{phase["landed"]} of {phase["milestones"]}</span></div>'
            + bar(phase["landed"], phase["milestones"], dark["state-healthy"], dark["edge-quiet"], 6)
            + f'<div class="chips">{held}{building}{free}</div></li>')


def page(state, dark, light):
    progress = state.get("progress", {})
    milestones = progress.get("milestones", {})
    checks = progress.get("checks", {})
    rows = state["milestones"]
    open_rows = [row for row in rows if row["status"] == "open"]
    building = [row for row in rows if row["status"] == "building"]
    held = [row for row in rows if row["status"] == "held"]
    landed = [row for row in rows if row["status"] == "landed"]
    variables = "\n".join(f"      --{name}: {value};" for name, value in sorted(dark.items()))
    light_variables = "\n".join(f"        --{name}: {value};" for name, value in sorted(light.items()))
    held_phases = [entry for entry in state["holds"] if entry["scope"].startswith("phase:")]

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Project Ambrose work board</title>
<meta name="description" content="What is being built in Project Ambrose right now, and which milestones anyone can take.">
<style>
  :root {{
{variables}
      color-scheme: dark light;
  }}
  @media (prefers-color-scheme: light) {{
    :root {{
{light_variables}
    }}
  }}
  * {{ box-sizing: border-box; }}
  body {{ margin: 0; background: var(--surface-page); color: var(--fg-body);
         font-family: Karla, "Segoe UI", "Helvetica Neue", Arial, sans-serif; line-height: 1.55; }}
  a {{ color: var(--action); }}
  code {{ font-family: "JetBrains Mono", Consolas, monospace; font-size: 0.85em;
          background: var(--surface-sunken); padding: 2px 6px; border-radius: 4px; color: var(--fg-body); }}
  .wrap {{ max-width: 1040px; margin: 0 auto; padding: 32px 16px 72px; }}
  header h1 {{ font-family: "Cormorant Garamond", Georgia, serif; font-size: clamp(2rem, 5vw, 3rem);
               margin: 0 0 4px; letter-spacing: 0.01em; }}
  header p.lead {{ color: var(--fg-muted); margin: 0 0 20px; max-width: 62ch; }}
  nav a {{ margin-right: 16px; font-size: 0.9rem; }}
  .headline {{ background: var(--surface-card); border: 1px solid var(--edge-quiet); border-radius: 12px;
               padding: 20px 22px; margin: 24px 0 8px; }}
  .headline .big {{ font-size: 2rem; font-family: "Cormorant Garamond", Georgia, serif; }}
  .headline .sub {{ color: var(--fg-muted); font-size: 0.92rem; }}
  .bar {{ width: 100%; height: 8px; border-radius: 999px; overflow: hidden; margin: 10px 0 6px; }}
  .bar span {{ display: block; border-radius: 999px; }}
  h2 {{ font-family: "Cormorant Garamond", Georgia, serif; font-size: 1.7rem; margin: 40px 0 6px; }}
  h2 + p.note {{ color: var(--fg-muted); margin: 0 0 16px; max-width: 70ch; }}
  .cards {{ display: grid; gap: 14px; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); }}
  .card {{ background: var(--surface-card); border: 1px solid var(--edge-quiet); border-radius: 12px; padding: 16px 18px; }}
  .card h3 {{ margin: 0 0 6px; font-size: 1.05rem; }}
  .id {{ font-family: "JetBrains Mono", Consolas, monospace; color: var(--value-number); margin-right: 6px; }}
  .facts {{ color: var(--fg-faint); font-size: 0.85rem; margin: 0 0 8px; }}
  .needs, .why {{ color: var(--fg-muted); font-size: 0.9rem; margin: 0 0 8px; }}
  .branch {{ margin: 10px 0 6px; }}
  .take {{ font-size: 0.88rem; color: var(--fg-muted); margin: 0; }}
  ul.plain {{ list-style: none; padding: 0; margin: 0; }}
  ul.plain > li {{ background: var(--surface-card); border: 1px solid var(--edge-quiet); border-radius: 10px;
                   padding: 12px 16px; margin-bottom: 10px; }}
  .who {{ color: var(--fg-faint); font-size: 0.86rem; display: block; }}
  .phases {{ display: grid; gap: 12px; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); }}
  .phases li {{ background: var(--surface-card); border: 1px solid var(--edge-quiet); border-radius: 10px; padding: 12px 14px; }}
  .phase-head {{ display: flex; justify-content: space-between; font-size: 0.9rem; }}
  .count {{ color: var(--fg-faint); }}
  .chips {{ min-height: 20px; }}
  .chip {{ display: inline-block; font-size: 0.72rem; padding: 1px 8px; border-radius: 999px; margin-right: 6px;
           border: 1px solid var(--edge-strong); color: var(--fg-muted); }}
  .chip.held {{ border-color: var(--state-wrong); color: var(--state-wrong); }}
  .chip.building {{ border-color: var(--state-waiting); color: var(--state-waiting); }}
  .chip.open {{ border-color: var(--state-healthy); color: var(--state-healthy); }}
  .ai {{ background: var(--surface-sunken); border: 1px solid var(--edge-strong); border-radius: 12px; padding: 18px 20px; }}
  .ai ol {{ margin: 8px 0 0; padding-left: 20px; color: var(--fg-muted); }}
  footer {{ margin-top: 48px; color: var(--fg-faint); font-size: 0.85rem; border-top: 1px solid var(--edge-quiet); padding-top: 16px; }}
</style>
</head>
<body>
<div class="wrap">
<header>
  <h1>Project Ambrose work board</h1>
  <p class="lead">What is being built right now, and what anyone can take. This page is generated from the roadmap itself, the
     open pull requests and the holds the maintainer's own sessions take, so it says what is true rather than what was true.</p>
  <nav>
    <a href="{REPOSITORY}">Repository</a><a href="{TRACK_URL}">The rules</a><a href="{PROMPT}">Prompt for your AI</a>
    <a href="{STATE}">state.json</a><a href="{DISCORD}">Discord</a>
  </nav>
</header>

<section class="headline">
  <div class="big">{milestones.get("percent", 0)}% of the plan built</div>
  {bar(milestones.get("done", 0), milestones.get("total", 1), dark["action"], dark["edge-quiet"], 10)}
  <div class="sub">{milestones.get("done", 0)} of {milestones.get("total", 0)} milestones &middot;
      {checks.get("done", 0)} of {checks.get("total", 0)} acceptance checks &middot;
      {len(open_rows)} open to anyone &middot; {len(building)} being built &middot;
      {state["contributor_track"]["merged"]} contributor items merged</div>
</section>

<h2>Take one</h2>
<p class="note">Everything here has all its dependencies built, is not held, and nobody has claimed it. Branch from
   <code>upstream/main</code>, name the branch as shown, and open a draft pull request on the first day, which is what holds it.</p>
<div class="cards">{"".join(card(row) for row in open_rows) or '<p class="note">Nothing is open at this moment. Ask in the Discord and one will be opened.</p>'}</div>

<h2>Next in line</h2>
<p class="note">Each of these is the last thing standing between the project and several more milestones. Some are held, some are open:
   if one you could build is on this list, it is the highest-value evening available.</p>
<ul class="plain">{"".join(queue(rows)) or "<li>Nothing is waiting on a single milestone at this moment.</li>"}</ul>

<h2>Being built right now</h2>
<p class="note">Claimed work, from open pull requests and claims, and the sections the maintainer's own sessions hold.
   Nothing here is takeable. A claim with no push for {STALE_DAYS} days falls back to the open list by itself.</p>
<ul class="plain">{"".join(busy_row(row) for row in building) or "<li>Nobody outside has a milestone open at this moment.</li>"}
{"".join(f'<li><span class="id">Phase {escape(entry["scope"].split(":")[1])}</span> held by {escape(entry["who"])}<span class="who">{escape(entry["what"])}, since {escape(entry["since"])}' + (f' &middot; not moved in {entry["days"]} days, so ask in the Discord before assuming it is still held' if entry.get("needs_review") else "") + '</span></li>' for entry in held_phases)}
{"".join(busy_row(row) for row in held if not hold_for(row["id"], [h for h in state["holds"] if h["scope"].startswith("phase:")]))}</ul>

<h2>The other track</h2>
<p class="note">Work that is not a milestone: findings about the game, tools, schemas, fixtures, guides and proposals, in folders no milestone
   touches. Nothing there is reserved, so two people may take the same item and both are read.
   {state["contributor_track"]["merged"]} merged so far, {state["contributor_track"]["open"]} items open, listed in
   <a href="{CONTRIBUTOR_URL}">the contributor track</a>.</p>
<ul class="plain">{"".join(f'<li><span class="id">#{escape(row["number"])}</span> {escape(row["title"])}<span class="who">{escape(row["who"])}, {escape(row["kind"])}, updated {escape(row["updated"])} &middot; <a href="{escape(row["url"])}">open it</a></span></li>' for row in state["other_open_work"]) or "<li>No other pull request is open at this moment.</li>"}</ul>

<h2>The phases</h2>
<p class="note">Seventeen phases, built in order. A held phase is closed to outside work entirely, however ready a milestone inside it looks.</p>
<ul class="plain phases">{"".join(phase_row(phase, dark) for phase in state["phases"])}</ul>

<h2>For your AI</h2>
<div class="ai">
  <p>Give your assistant <a href="{PROMPT}">the milestone prompt</a>, then have it read <a href="{STATE}">state.json</a> on this page
     before it plans anything. That file carries every milestone with its status, what it needs, what it unlocks and who holds it.</p>
  <ol>{"".join(f"<li>{escape(line)}</li>" for line in HOW_TO_USE)}</ol>
</div>

<footer>
  Generated {escape(state["generated_at"])} from commit data in the repository. It rebuilds when a pull request opens or closes,
  when the roadmap or the holds change, and at least once a day. {len(landed)} milestones landed so far.
  If this page is wrong, say so in the <a href="{DISCORD}">Discord</a>: it is generated, so the fix is in the repository.
</footer>
</div>
</body>
</html>
"""


def outputs(root, snapshot, now):
    state = build_state(root, snapshot, now)
    dark, light = colours(root)
    return {STATE: json.dumps(state, indent=2) + "\n", PAGE: page(state, dark, light)}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose work board")
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--github", help="a JSON snapshot with 'pulls' and 'issues' from the GitHub API")
    parser.add_argument("--out", default=OUT_DIR, help="the folder to write the board into")
    parser.add_argument("--now", help="the moment to stamp, for a repeatable build")
    parser.add_argument("--check", action="store_true", help="read everything the board is built from and write nothing")
    arguments = parser.parse_args(argv)
    root = os.path.abspath(arguments.root)

    snapshot = load_json(arguments.github, {"pulls": [], "issues": []}) if arguments.github else {"pulls": [], "issues": []}
    now = moment(arguments.now) or datetime.datetime.now(datetime.timezone.utc)
    if now.tzinfo is None:
        now = now.replace(tzinfo=datetime.timezone.utc)

    if arguments.check:
        try:
            state = build_state(root, snapshot, now)
        except HoldError as failure:
            print(f"work board: {failure}", file=sys.stderr)
            return 1
        print(f"work board: {len(state['milestones'])} milestones and {len(state['holds'])} hold(s) read, nothing written")
        return 0

    folder = os.path.join(root, arguments.out) if not os.path.isabs(arguments.out) else arguments.out
    os.makedirs(folder, exist_ok=True)
    for name, text in outputs(root, snapshot, now).items():
        with open(os.path.join(folder, name), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
    written = json.loads(outputs(root, snapshot, now)[STATE])
    print(f"work board: {len(written['milestones'])} milestones, "
          + ", ".join(f"{count} {status}" for status, count in written["counts"].items() if count))
    return 0


if __name__ == "__main__":
    sys.exit(main())

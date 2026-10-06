# Project Ambrose by Imjustchico
# Builds the work board the project publishes: one page a person reads and one state file a contributor's assistant reads, both saying for every milestone whether it is landed, being built right now by an open pull request or claim, ready to start, or waiting on a dependency, every one of them open to anyone with nothing held or reserved, from the phase files, the two tracks and a snapshot of the open pull requests and claims.

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
MILESTONE_BRANCH = re.compile(r"^milestone/(\d+)\.(\d+)")
CLAIM_TITLE = re.compile(r"(\d+\.\d+)")
TITLE_IDS = re.compile(r"^\s*(\d+\.\d+(?:\s*(?:,|&|and|,\s*and)\s*\d+\.\d+)*)(?=[\s:;,.]|$)")
IDENTIFIER = re.compile(r"(\d+)\.(\d+)")
STARTED_ROW = re.compile(r"^\| *([\d.,  ]+?) *\| *(.+?) *\| *(.+?) *\| *(.+?) *\|$")

STATUS_ORDER = ("landed", "building", "open", "waiting")

HOW_TO_USE = [
    "This file is the live state of the project. Read it before starting anything, and read it again before you push.",
    "Every milestone is open to anyone, the maintainer's own sessions and outside contributors alike, in any phase and any order, and nobody needs anyone's permission to take one. Nothing is held or reserved.",
    "Status 'open' means every dependency is built. 'waiting' means one is not: it can still be taken, by building what it rests on in the same pull request or by taking that dependency first. 'building' means somebody has an open pull request or claim for it, listed under claims: you may still work on it, but say so on their pull request first, so the two combine rather than collide.",
    "Start one by opening a draft pull request from a branch named milestone/<id>-<short-name>, which is also what lets CI accept a change under src/. The board picks that up by itself, and a pull request from any other branch counts too when its title starts with the milestone's id.",
    "A milestone is finished only when every acceptance check in its phase file is ticked with the evidence that proved it. A check you cannot run stays unticked and is named in the pull request.",
    "A milestone carrying next_after is waiting on exactly that one milestone, the quickest way to free it. One carrying started already has work landed, and says what is left.",
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


class TrackError(Exception):
    pass


def normalised(identifier):
    found = IDENTIFIER.fullmatch(identifier)
    return f"{int(found.group(1))}.{int(found.group(2)):02d}" if found else identifier


def track_rows(root, known=None):
    text = read(os.path.join(root, TRACK))
    if "## Started" not in text:
        return {}
    section = text.split("## Started", 1)[1].split("\n## ", 1)[0]
    started = {}
    for line in section.splitlines():
        found = STARTED_ROW.match(line)
        if not found:
            continue
        for identifier in re.findall(r"\d+\.\d+", found.group(1)):
            if known is not None and identifier not in known:
                raise TrackError(f"{TRACK} lists {identifier} under Started, which is no milestone in the roadmap")
            started[identifier] = {"by": found.group(2), "sent_as": found.group(3), "left": found.group(4)}
    return started


def contributor_counts(root):
    text = read(os.path.join(root, CONTRIBUTOR_TRACK))
    head, _, tail = text.partition("### Merged so far")
    return {"open": len(re.findall(r"^\| [FC]-\d+ \|", head, re.M)),
            "merged": len(re.findall(r"^\| [FC]-\d+ \|", tail, re.M))}


def moment(text):
    try:
        return datetime.datetime.fromisoformat((text or "").replace("Z", "+00:00"))
    except ValueError:
        return None


def named_by(pull):
    branch = MILESTONE_BRANCH.match(pull.get("headRefName", "") or "")
    if branch:
        return [f"{branch.group(1)}.{int(branch.group(2)):02d}"]
    title = TITLE_IDS.match(pull.get("title", "") or "")
    if not title:
        return []
    return list(dict.fromkeys(normalised(one) for one in re.findall(r"\d+\.\d+", title.group(1))))


def claim_of(item, kind, now):
    when = moment(item.get("updatedAt"))
    return {
        "who": (item.get("author") or {}).get("login", "somebody"),
        "url": item.get("url", ""),
        "kind": kind,
        "number": item.get("number"),
        "since": (item.get("createdAt") or "")[:10],
        "updated": (item.get("updatedAt") or "")[:10],
        "stale": bool(when and (now - when).days >= STALE_DAYS),
    }


def claims(snapshot, now):
    found = {}
    for pull in snapshot.get("pulls", []):
        kind = "a draft pull request" if pull.get("isDraft") else "a pull request"
        for identifier in named_by(pull):
            found.setdefault(identifier, []).append(claim_of(pull, kind, now))
    for issue in snapshot.get("issues", []):
        for identifier in CLAIM_TITLE.findall(issue.get("title", "") or ""):
            identifier = normalised(identifier)
            if identifier in found:
                continue
            found[identifier] = [claim_of(issue, "a claim", now)]
    for identifier in found:
        found[identifier].sort(key=lambda claim: (claim["since"], str(claim["number"])))
    return found


def other_work(snapshot, now):
    rows = []
    for pull in snapshot.get("pulls", []):
        branch = pull.get("headRefName", "") or ""
        if named_by(pull):
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


def status_of(milestone, taken):
    if milestone["done"]:
        return "landed", ""
    active = [claim for claim in taken.get(milestone["id"], []) if not claim["stale"]]
    if active:
        return "building", "; ".join(f'{claim["who"]} has {claim["kind"]}' for claim in active)
    if milestone["missing"]:
        return "waiting", "waiting on " + ", ".join(milestone["missing"])
    return "open", "open to anyone"


def build_state(root, snapshot, now):
    everything = ready.milestones(root)
    started = track_rows(root, everything)
    taken = claims(snapshot, now)
    unlocks = unlocked_by(everything)

    rows = []
    for identifier, milestone in sorted(everything.items(), key=lambda pair: (int(pair[0].split(".")[0]), int(pair[0].split(".")[1]))):
        entry = dict(milestone)
        entry["missing"] = [name for name in milestone["depends_on"] if name not in everything or not everything[name]["done"]]
        status, note = status_of(entry, taken)
        entry["status"] = status
        entry["note"] = note
        entry["unlocks"] = unlocks.get(identifier, 0)
        entry["checks_left"] = entry["checks_total"] - entry["checks_done"]
        entry["branch"] = f"milestone/{identifier}-<short-name>"
        if identifier in started:
            entry["started"] = started[identifier]
        if identifier in taken:
            active = [claim for claim in taken[identifier] if not claim["stale"]]
            entry["claim"] = (active or taken[identifier])[0]
            entry["claims"] = taken[identifier]
        if status == "waiting" and len(entry["missing"]) == 1:
            entry["next_after"] = entry["missing"][0]
        rows.append(entry)

    phases = {}
    for row in rows:
        phase = phases.setdefault(row["phase"], {"phase": row["phase"], "milestones": 0, "landed": 0, "open": 0, "building": 0, "waiting": 0})
        phase["milestones"] += 1
        for status in ("landed", "open", "building", "waiting"):
            phase[status] += 1 if row["status"] == status else 0

    counted = {status: len([row for row in rows if row["status"] == status]) for status in STATUS_ORDER}
    return {
        "schema": 2,
        "generated_at": now.replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "how_to_use": HOW_TO_USE,
        "links": {"repository": REPOSITORY, "milestone_track": TRACK_URL, "contributor_track": CONTRIBUTOR_URL,
                  "prompt": PROMPT, "claim": CLAIM_URL, "discord": DISCORD, "state": STATE_URL},
        "progress": load_json(os.path.join(root, PROGRESS), {}),
        "contributor_track": contributor_counts(root),
        "other_open_work": other_work(snapshot, now),
        "counts": counted,
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
    if row.get("started"):
        pieces.append(f'<p class="why">Started by {escape(row["started"]["by"])}. {escape(row["started"]["left"])}</p>')
    pieces.append(f'<p class="branch"><code>git switch -c milestone/{escape(row["id"])}-&lt;short-name&gt;</code></p>')
    pieces.append(f'<p class="take"><a href="{CLAIM_URL}">Claim it</a> or open a draft pull request from that branch.</p>')
    return '<article class="card open">' + "".join(pieces) + "</article>"


def busy_row(row):
    active = [claim for claim in row.get("claims", []) if not claim["stale"]]
    if row["status"] == "building" and active:
        who = "; ".join(f'{escape(claim["who"])}, {escape(claim["kind"])} '
                        f'<a href="{escape(claim["url"] or REPOSITORY)}">#{escape(claim["number"] or "")}</a>, since {escape(claim["since"])}'
                        for claim in active)
        return f'<li><span class="id">{escape(row["id"])}</span> {escape(row["title"])} <span class="who">{who}</span></li>'
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
    building = f' <span class="chip building">{phase["building"]} being built</span>' if phase["building"] else ""
    free = f' <span class="chip open">{phase["open"]} ready</span>' if phase["open"] else ""
    return (f'<li><div class="phase-head"><span>Phase {phase["phase"]}</span>'
            f'<span class="count">{phase["landed"]} of {phase["milestones"]}</span></div>'
            + bar(phase["landed"], phase["milestones"], dark["state-healthy"], dark["edge-quiet"], 6)
            + f'<div class="chips">{building}{free}</div></li>')


def page(state, dark, light):
    progress = state.get("progress", {})
    milestones = progress.get("milestones", {})
    checks = progress.get("checks", {})
    rows = state["milestones"]
    open_rows = [row for row in rows if row["status"] == "open"]
    building = [row for row in rows if row["status"] == "building"]
    landed = [row for row in rows if row["status"] == "landed"]
    variables = "\n".join(f"      --{name}: {value};" for name, value in sorted(dark.items()))
    light_variables = "\n".join(f"        --{name}: {value};" for name, value in sorted(light.items()))

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
  .why {{ color: var(--fg-muted); font-size: 0.9rem; margin: 0 0 8px; }}
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
  <p class="lead">What is being built right now, and what is ready to start. Every milestone is open to anyone, in any order,
     and nobody needs to ask. This page is generated from the roadmap itself and the open pull requests, so it says what is true
     rather than what was true.</p>
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
      {len(open_rows)} ready to start &middot; {len(building)} being built &middot;
      {state["contributor_track"]["merged"]} contributor items merged</div>
</section>

<h2>Take one</h2>
<p class="note">Everything here has all its dependencies built and nobody has a pull request open for it yet. Branch from
   <code>upstream/main</code>, name the branch as shown, and open a draft pull request on the first day, which is how everyone
   sees you are building it. A milestone still waiting on a dependency can be taken too, building what it rests on, and
   <a href="{STATE}">state.json</a> lists every one.</p>
<div class="cards">{"".join(card(row) for row in open_rows) or '<p class="note">Nothing is ready at this moment: every milestone left waits on another, and each of those can be taken too.</p>'}</div>

<h2>Next in line</h2>
<p class="note">Each of these is the last thing standing between the project and several more milestones. If one you could
   build is on this list, it is the highest-value evening available.</p>
<ul class="plain">{"".join(queue(rows)) or "<li>Nothing is waiting on a single milestone at this moment.</li>"}</ul>

<h2>Being built right now</h2>
<p class="note">Open pull requests and claims, with who has each. You may still work on one of these: say so on its pull
   request first, so the two combine rather than collide. A claim with no push for {STALE_DAYS} days drops off this list by itself.</p>
<ul class="plain">{"".join(busy_row(row) for row in building) or "<li>Nobody has a milestone open at this moment.</li>"}</ul>

<h2>The other track</h2>
<p class="note">Work that is not a milestone: findings about the game, tools, schemas, fixtures, guides and proposals, in folders no milestone
   touches. Nothing there is reserved, so two people may take the same item and both are read.
   {state["contributor_track"]["merged"]} merged so far, {state["contributor_track"]["open"]} items open, listed in
   <a href="{CONTRIBUTOR_URL}">the contributor track</a>.</p>
<ul class="plain">{"".join(f'<li><span class="id">#{escape(row["number"])}</span> {escape(row["title"])}<span class="who">{escape(row["who"])}, {escape(row["kind"])}, updated {escape(row["updated"])} &middot; <a href="{escape(row["url"])}">open it</a></span></li>' for row in state["other_open_work"]) or "<li>No other pull request is open at this moment.</li>"}</ul>

<h2>The phases</h2>
<p class="note">Seventeen phases, listed in the order they build on each other. Every milestone in every phase is open to anyone.</p>
<ul class="plain phases">{"".join(phase_row(phase, dark) for phase in state["phases"])}</ul>

<h2>For your AI</h2>
<div class="ai">
  <p>Give your assistant <a href="{PROMPT}">the milestone prompt</a>, then have it read <a href="{STATE}">state.json</a> on this page
     before it plans anything. That file carries every milestone with its status, what it needs, what it unlocks and who is building it.</p>
  <ol>{"".join(f"<li>{escape(line)}</li>" for line in HOW_TO_USE)}</ol>
</div>

<footer>
  Generated {escape(state["generated_at"])} from commit data in the repository. It rebuilds when a pull request opens or closes,
  when the roadmap changes, and at least once a day. {len(landed)} milestones landed so far.
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
        except TrackError as failure:
            print(f"work board: {failure}", file=sys.stderr)
            return 1
        print(f"work board: {len(state['milestones'])} milestones and {sum(1 for row in state['milestones'] if row.get('started'))} started row(s) read, nothing written")
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

<!-- Project Ambrose by Imjustchico: The second track other people work from, the roadmap itself: every milestone open to anyone in any order without asking, how to start one so others see it, what finishing one means, and what a review holds it to. -->

# Milestone track

doc/CONTRIBUTOR-TRACK.md is the safe track: its own folders, nothing a milestone touches. This is the other one: the milestones of doc/ROADMAP.md themselves, with the source tree, the tests and the acceptance checks that come with them.

**Every milestone is open to anyone.** Since 2026-10-06, at the maintainer's direction, anyone may take any milestone in any phase, the maintainer's own sessions and outside contributors alike, in any order and without asking anybody first. Nothing is held, nothing is reserved, and no session owns an area. Every milestone finished is one the project does not have to wait for.

What this document guards against is what that freedom risks: two people building the same milestone without knowing it, half a milestone that cannot be judged, a change that lands in a file another pull request is changing right now. The board shows who is building what, so the first never happens by accident, and the rest of this page keeps out the other two.

Working with an AI assistant is expected here. contrib/AI-MILESTONES-HERE.md is a prompt to paste into yours; it carries what it needs to know before it writes a line. Questions go to the Discord: https://discord.gg/Dx6ACDUj6N.

## The board says what is true right now

**https://justchicoo.github.io/Project-Ambrose/** is generated from this document, the phase files and the open pull requests, and it is the thing to look at before anything else. It says for every milestone whether it is landed, being built right now, ready to start, or waiting on a dependency, and it rebuilds whenever a pull request or a claim opens or closes.

Its **state.json** is the same thing for a machine: `https://justchicoo.github.io/Project-Ambrose/state.json` carries every milestone with its status, what it waits on, what it unlocks, who is building it and how to start it, plus the rules in a `how_to_use` list. A contributor's assistant should read that file before planning anything, and again before it pushes.

This page stays the rulebook. The board is the live view of it, and where the two ever disagree, the checks in `apps/ci` and `apps/site` fail until they agree again.

## Any milestone, in any order

- **Ready** milestones have every dependency built. `python apps/progress/ready.py` prints them, and `--blocked` says what each of the rest is waiting on.
- **Waiting** ones can be taken too. When a milestone rests on code that is missing or unfinished, build that in the same pull request, anywhere this document allows, and say so in the description, or take the dependency first. A milestone is never refused because something it needs was not there yet.
- **Being built** means somebody has a pull request open for it, or a claim. You may still work on it. Say so on their pull request before you start, so the two combine instead of colliding, and whatever of their work the landing uses keeps their name on the commit.
- **Started** milestones, in the table below, already have work landed and say what is left. Finishing one is as welcome as starting another, and nobody has to be asked first.
- **Phases** are listed in the order they build on each other. That is advice, not a gate: a phase is done when its "Done when" line shows in the real client, whatever order its milestones were built in.
- **16.11** overlaps the type extraction 3.21 already built, so read 3.21 before planning it.

## Starting one

1. Branch from `upstream/main`, and name the branch `milestone/<id>-<short-name>`, such as `milestone/4.04-world-wire-math`. The name is not decoration: `apps/ci/ci_contrib_paths.py` reads it, and it is the only reason CI lets the change touch `src/`.
2. Open the pull request early, as a draft, titled `<id> <what you are building>`. The board reads it within minutes and shows the milestone as being built, by you. Nobody has to be told or asked, and no row has to move. A pull request from any other branch counts too when its title starts with the milestone's id.
3. One milestone per pull request. Never build a second milestone's branch on the first one's.

A pull request with no push for two weeks stops showing as being built, with whatever it pushed left in place, so somebody else can carry it on. A push is all it takes to keep it showing.

## What finishing one means

A milestone is finished when **every acceptance check in its phase file is ticked**, in the same commit as the code that earns them, and not before. Most milestones carry two lists: the short one under the milestone heading and the full one at the end of the detailed spec. Both are the same checks in different detail, and both get ticked.

A ticked check quotes what proved it, in brackets, the way the ones already ticked do:

```
- [x] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step (MovementPackingTest.YawRoundTrip)
```

The name of the test that runs it, the tool run and what it printed, or the screen and what it showed. Evidence names nothing personal: a real account, address, path or machine name is written `<account>`, `<address>` and so on. A check with no evidence in brackets is not ticked, and a check ticked by a test that does not exist is the one thing that ends a review immediately.

**A check you cannot run stays unticked.** Some are labelled Dev-gated, Client-gated or Real client, and need an installation, a second machine or hardware you may not have. Leave those boxes empty, say in the pull request exactly which ones and why, and send the rest. Better still, say so in the draft while you are still building and ask: the maintainer's sessions can often run a gated check against the maintainer's own install there and then, and on 16.02 that is what showed a whole XML shape was wrong on work that was otherwise sound. The work merges, the milestone stays open to anyone, and its row under **Started** says what is left. That is an honest, welcome outcome. Ticking a box you did not run is not.

**Build what the milestone says, not around it.** The deliverables list under the detailed spec names the files to write, the tables to add and the client messages involved. If one of them is wrong or impossible, say so in the pull request and propose the change. Do not quietly build something else: the acceptance checks are written against those deliverables and a review reads them together.

## What a milestone branch may change

Everything under `src/`, `data/sql/updates/`, `apps/` and `doc/` except the list below, plus its own phase file. The check enforces exactly that:

```
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD --branch milestone/<id>-<short-name>
```

It refuses another phase's file, so a change that needs one is a change of scope, and it refuses these, which stay out of milestone branches so that concurrent work never collides in them: `.github/`, `apps/ci/`, `apps/codestyle/`, `apps/progress/`, `doc/progress/`, `doc/work/`, `packages/ui/src/tokens/`, `README.md`, `CONTRIBUTING.md`, `CLAUDE.md`, `LICENSE`, `CMakePresets.json`, `.gitignore`, `doc/ROADMAP.md`, `doc/ARCHITECTURE.md`, `doc/REVIEWING.md`, `doc/CONTRIBUTOR-TRACK.md`, `doc/MILESTONE-TRACK.md`, `contrib/README.md`, `contrib/AI-START-HERE.md` and `contrib/AI-MILESTONES-HERE.md`. A milestone whose own deliverables are among those files is granted exactly them, file by file, in `doc/work/grants.json`, which the check reads: 3.19's two SQL checks and the workflow that runs them are the first. Name them in the draft and the reviewing session adds the grant, with nobody's permission to wait for.

`doc/ROADMAP.md`'s "Where we are" and the progress card are written when the milestone lands, from the boxes you ticked. A milestone that needs a library may add it to `vcpkg.json` with its licence notice in `THIRD-PARTY-NOTICES.md` and say why in the description. Code the milestone rests on that is missing or unfinished is built in the same pull request, anywhere this section allows. `python apps/ci/ci_local.py --branch milestone/<id>-<short-name>` runs this path check with every other step of CI's checks job.

## What every milestone pull request needs

- **It builds and its tests pass on at least one platform, and you say which.** `cmake --preset windows-msvc-x64` then `cmake --build --preset windows-debug` and `ctest --preset windows-debug`, or `linux-gcc` with `linux-gcc-debug`. The first configure builds every dependency from source and takes about an hour. `ctest` also runs the style and CI checks, so a green `ctest` is most of the review.
- **New tests live in `src/test/`, mirroring the folder of the code they test**, and are named in the acceptance check they prove. A test that needs an installation carries the CTest label `client` and skips unless `AMBROSE_CLIENT_DIR` is set; one that needs the user's own type dump reads `AMBROSE_TYPE_DUMP_PATH`. Never make an existing test optional to get it passing.
- **The tools come first, and leave better than they were.** Read doc/TOOLS.md and look at what is actually built under `src/tools` and `apps` before writing anything that reads a file format, because that list has drifted and a tool marked planned may exist. Call the one that exists, or teach it the function the milestone needs, which is welcome work and part of the milestone. Writing a second copy inside the milestone is what makes a pull request hard to merge, and in one case it emitted an entire dataset with every position empty.
- **C++20, and the architecture as written.** doc/ARCHITECTURE.md's layering, its folder for each subsystem, dated SQL update files, content in the world database, and the settled Decisions. A milestone is not the place to re-litigate one.
- **The branding header and no other comment**, in the form doc/ARCHITECTURE.md gives for the file type. `python apps/codestyle/codestyle.py` is the judge. Files are UTF-8 with no byte order mark, LF endings, no trailing whitespace, ending in a newline, ASCII unless the content is a translation.
- **No file from the game client, and nothing generated from one.** Not an archive, an asset, a dump, a capture or a run of bytes pasted from one. A tool reads the user's own installation at run time; that is the line, and `python apps/ci/ci_forbidden_files.py` guards it.
- **Written from scratch.** Another server's behaviour may be studied. Its code and its data may not be copied, translated or ported.
- **A commit trailer naming the AI that wrote it**, on every commit in the branch, such as `Co-Authored-By: <model name> <noreply@example.com>`.
- **A description that says what was built, how it was verified, which checks are ticked and which are not.** Unverified work is not merged.

## How it is reviewed

doc/REVIEWING.md is the rulebook, and its first line applies hardest here: verified by running, never by reading. Expect the reviewing session to build the branch, run its tests, run the ones it claims by name, and try the failure the code says it handles. A branch named for a milestone builds the Linux GCC leg in CI by itself, without waiting for a label, and the reviewing session adds a `ci:` label for the Windows leg or the sanitizers when the change deserves them.

While it is not ready, each review round is one message written so your AI can act on it without guessing: what works and what was verified, then a numbered list of everything left before it merges, each item giving the file and line, what is wrong, the exact change, and the command or test that proves it passes, and last the line "push to the same branch and it will be rechecked". Every round restates the whole list, so the newest message is all your AI needs to read. Hand that message to your AI as it is, have it do every item and run every proving command, and push to the same branch. A long list says which items can follow in a second pull request on the same milestone.

Nothing is closed for being unfinished. Missing or unfinished code, a missing deliverable, a failing check or something the milestone rests on that does not exist yet gets a pointer to the fix and where to build it, and the pull request stays open until it lands. It lands when it moves the project forward, a check earned by a test that was run, a real fix, or a sound part of the milestone with what is left named, and what it still needs that the reviewing session can do at landing is fixed on `main` with you kept as author, with the message saying what changed so your next one needs less. A pull request that ticks checks lands as one commit on `main` authored to you and is closed with the label `landed`, which means it is in; one that ticks nothing is merged on GitHub and shows as merged. The one thing that holds a pull request outright is a file from the game client, which comes out first. Run the checks job yourself before every push with `python apps/ci/ci_local.py --branch milestone/<id>-<short-name>`, and the tests the way contrib/AI-MILESTONES-HERE.md shows, and the review has less to find.

## Started

Work has already landed on each of these, and the row says what is left. Every one is open to anyone who wants to finish it.

| ID | Started by | Sent as | What is left |
|---|---|---|---|
| 3.02 | the maintainer's world session | on main | Reopened on 2026-09-30: its Badges page opens but shows no badges, because the server sends an empty list, and a page is done only when the real client shows real content in it |
| 3.28 | MeruneFleuruwu, then the maintainer's server session | [#10](https://github.com/Justchicoo/Project-Ambrose/pull/10) | Landed on 2026-09-26 with checks 3 and 4: Type.name, Type.hash and the std::string layout derived from the client's own constructor. Left: the rest of the layout the same way, checks 1 and 2 once every field is derived, check 5 on a second client, and the record of extracted clients |
| 4.08 | MeruneFleuruwu, then the maintainer's world and track sessions | on main | Built on MeruneFleuruwu's decoding of every zone and the world session's writer in `extractor zones`. Left: the four checks that count every entry, which wait only on accepting the four sigil classes with their shared property kept under its hash, as settled in doc/ARCHITECTURE.md, and the spawn data the two integration tests read |
| 5.08 | MeruneFleuruwu, then the maintainer's panel session | [#4](https://github.com/Justchicoo/Project-Ambrose/pull/4) | The scripts, env.dist and doc/INSTALL.md, with the conf check earned. Left: a clean Ubuntu and a clean Windows machine reaching 'ready' on all three apps |
| 6.17 | MeruneFleuruwu | [#12](https://github.com/Justchicoo/Project-Ambrose/pull/12) | Landed on 2026-09-29 with six of eight checks. Left: the two 10-minute fuzz runs on the frame and decode paths, which need the `linux-clang-fuzz` preset |
| 6.18 | the maintainer's track session | on main | Built on 2026-09-30 with the module hook check earned. Left: a real client at login showing the named, redacted log line, and `.network sessions` and `.network packetlog filter` in game chat |
| 8.04 | the maintainer's world session | on main | Built on 2026-09-26 with 10 of 12 checks. Left: the game master's real-client check, and the real-client check that shows a spell in the Spell Deck, which waits on the deck 8.10 and 8.11 build |
| 9.02 | the maintainer's world session | on main | Built on 2026-09-26 with 6 of 7 checks. Left: its real-client check, which 6.04's in-game commands made possible |
| 12.14 | Kokoita | [#50](https://github.com/Justchicoo/Project-Ambrose/pull/50) | Landed on 2026-10-06 with no check yet: the purchased emote and teleport-effect masks are saved with the character and sent in MSG_UPDATECUSTOMEMOTES, the custom-emote catalog is read from ObjectData/Emotes, and both radial emote messages are shown only for an animation the wizard owns and the install lists. Left: the wheel's pages, whose slots name emote items the wizard holds and so wait on 8.08's item instances; the line checked against the emote; handler tests that fail when the ownership check is removed; and the real-client check |
| 16.03 | MeruneFleuruwu | [#6](https://github.com/Justchicoo/Project-Ambrose/pull/6) | The scanner is delivered and four checks are earned, two of them re-run by the maintainer on a real install rather than only in a fixture. Size, CRC, HeaderSize and HeaderCRC are right for 3589 of 3589 type 3 and 5 records, and a cached run is 194 seconds down to 1 with a byte-identical .bin. Left: package membership, 3820 of 3825, because `Windows/PatchClient/` is not matched and the manifest files scan themselves in; and four fields no check names, `TarFileName`, `CompressedHeaderSize`, the 40 type 5 WADs and the header fields on plain files |
| 17.01 | the maintainer's server session | on main | One Dev-gated check, on a Windows console and a Linux terminal, and nothing else left to build |
| 17.18 | the maintainer's panel session | on main | Landed on 2026-09-27 with seven of eight checks. Left: check 2's download and share paths, which 17.54 and 17.39 add |
| 17.23 | MeruneFleuruwu | [#9](https://github.com/Justchicoo/Project-Ambrose/pull/9) | Landed on 2026-09-25 with the Compose and time zone checks earned, after review added the type extractor and the SQL to the image and the egg, Python to their build, a health check that probes the login port and a .dockerignore. Left: the three Dev-gated checks, a reboot after `--install-service` on Windows and on Linux and the two Pterodactyl ones, which the maintainer runs on their own machines |
| 17.35 | MeruneFleuruwu | [#8](https://github.com/Justchicoo/Project-Ambrose/pull/8) | Landed on 2026-09-26 with checks 2, 3 and 5 earned, the routes and the page under `panel.settings`. Left: an SMTP transport and the mail test that uses it (check 1), sealing the saved password at rest once the supervisor has a key, and the captcha at sign-in refusing when its provider cannot be reached (check 4) |
| 17.47 | the maintainer's panel session | on main | Landed on 2026-09-27 with four of five checks. Left: the API-key half of check 2, which waits on 17.36's keys |
| 17.74 | the maintainer's server session | on main | One Dev-gated check, on the same console and terminal as 17.01 |
| 17.91 | MeruneFleuruwu | [#14](https://github.com/Justchicoo/Project-Ambrose/pull/14) | Landed on 2026-09-29 with checks 1, 3 and 5 earned, after review timed 6.01's movement relay as its own subsystem. Left: a benchmark showing the accumulators cost nothing measurable while no profile runs (check 2), and a tick alert naming the subsystem (check 4), which waits for 17.67's alert rules |
| 17.179 | the maintainer's panel session | on main | Two of its window checks reopened on 2026-09-30: the page loads but was not shown to render, and a page is done only when it shows real content |

## Landed

A row that says **before the reset** came from a pull request that was on the repository before it was recreated, so its number points at nothing now and the link is gone. That work is in the tree either way, and those pull requests are archived off the repository. Rows with a number are live.

| ID | Who | Sent as | What landed |
|---|---|---|---|
| 6.02 | MeruneFleuruwu | [#47](https://github.com/Justchicoo/Project-Ambrose/pull/47) | A player's spellbook wizbang relayed to the wizards in the same zone instance, shown to new viewers and cleared when its owner leaves. The maintainer's track session found how the client looks a wizbang up, the string hash of a WizBangs.xml template's name, proved the relay with a known marker drawn over the companion and settled that r806919 has no spellbook marker to draw. All four checks earned |
| 6.08 | MeruneFleuruwu | [#46](https://github.com/Justchicoo/Project-Ambrose/pull/46) | Logout, link-dead, AFK and shutdown: a quitting wizard leaves at once and relogs where it stood, a dropped one stays link-dead until its time runs out or it reattaches with the same mobile id, live AFK and link-dead timers, and a shutdown notice with every position saved, its real-client checks run with three client-driver scenarios. All nine checks earned |
| 6.10 | MeruneFleuruwu | [#26](https://github.com/Justchicoo/Project-Ambrose/pull/26) | The versionable round trip of a supplemental class in the type registry test, five of the seven checks' evidence. The maintainer's world session finished 6.10 the same day: the game server finds the install's server-only classes on its first start, from BINd files and plain-XML object files |
| 3.18 | MeruneFleuruwu | [#45](https://github.com/Justchicoo/Project-Ambrose/pull/45) | The updater's rename, hash, redundancy, dead-reference and pending decisions pulled into one pure planning step with database-free tests, the deliverable left open before the reset. All nine checks earned |
| 17.49 | MeruneFleuruwu | [#13](https://github.com/Justchicoo/Project-Ambrose/pull/13) | Panel audit scope, app relay and command history: every action recorded in the transaction of its change with a chain hash on each row, the app relay holding the tokens and capping command levels, distinct recorded relay errors, and a per-user command history with sensitive arguments redacted. All six checks earned |
| 17.106 | MeruneFleuruwu | [#15](https://github.com/Justchicoo/Project-Ambrose/pull/15) | Error reports: every log record carries its repository-relative file, line, function and template, apps group their errors by place with counts that survive restarts, and the Error reports page previews and downloads a versioned report file with rendered text only when ticked, every report audited. All six checks earned |
| 3.20 | MeruneFleuruwu | [#43](https://github.com/Justchicoo/Project-Ambrose/pull/43) | The last check of finding client data on the machine: on a real Windows terminal a game server with empty name tables offered the extraction, and once accepted it loaded 63 tables and 7955 names across 7 locales without a restart, the counts the install holds. All five checks earned |
| 9.01 | MeruneFleuruwu | [#11](https://github.com/Justchicoo/Project-Ambrose/pull/11) | The combat service's 36 messages registered at the orders the r806919 install gives them, MSG_COMBATMOVE and MSG_COMBATPHASEFORSPECTATORS round-tripping their wire fields, and the first in-world combat stubs, with MSG_COMBATMOVE refused outside the world and logged decoded inside it. All seven checks earned |
| 4.04 | MeruneFleuruwu | before the reset | The world's wire math, a position and a yaw packed into bytes, and the location string character select sends, earned by its own tests. Its last check, the MSG_ATTACH a real client sends after Play, was earned by the maintainer's session in the client driver's enter-world run on 2026-09-25 |
| 8.14 | MeruneFleuruwu | before the reset | A versionable object decoded from the client re-encodes byte for byte on the hat template, TemplateManifest.xml and a 2000-file sample. All four checks earned, the last two found by an audit to share the short list's evidence. Its property-order preservation is inert on every file tested, which the phase's review notes record |
| 17.10 | MeruneFleuruwu | [#7](https://github.com/Justchicoo/Project-Ambrose/pull/7) | A Prometheus and Grafana stack with three provisioned dashboards and an operations runbook, the first milestone spared out of a held phase. Both checks earned on a real gameserver: live graphs at 28 seconds, all eleven panels drawing |
| 16.02 | MeruneFleuruwu | [#3](https://github.com/Justchicoo/Project-Ambrose/pull/3) | One manifest read and written as both the client's XML and its binary form, keeping the table order the file declares. All five checks earned, including the env-gated one, which the maintainer ran against a reference XML: 3591 tables, Base 140, PatchClient 97 |
| 1.12 | MeruneFleuruwu | before the reset | The client's CRC variant proved against the pinned install's own archives, the synthetic header measurement, and a sweep that opens every GameData archive and reads every stored entry |
| 16.01 | MeruneFleuruwu | before the reset | The client's binary table list read and written byte for byte, proven against a reference list of exactly the size the check names, with all six checks earned |
| 1.06, 1.07, 1.08 | MeruneFleuruwu | before the reset | The last check of all three was stale: the locale round-trip it asks for is covered by a client-gated test that passes on the pinned install |

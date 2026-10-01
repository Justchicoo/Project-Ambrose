<!-- Project Ambrose by Imjustchico: The second track other people work from, the roadmap itself: which milestones are open to outside help, how one is taken so two people never build the same thing, what finishing one means, and what a review holds it to. -->

# Milestone track

doc/CONTRIBUTOR-TRACK.md is the safe track: its own folders, nothing a milestone touches. This is the other one. A named set of milestones from doc/ROADMAP.md is open to outside help, with the source tree, the tests and the acceptance checks that come with them.

It exists because the phases are the project, and the maintainer's own agents build them one at a time in dependency order. Every milestone finished from outside is one the project does not have to wait for. What it costs is the risk this document is written to remove: two people on the same milestone, half a milestone that cannot be judged, a change that lands in a file another milestone is being built in right now.

Working with an AI assistant is expected here. contrib/AI-MILESTONES-HERE.md is a prompt to paste into yours; it carries what it needs to know before it writes a line. Ask in the Discord first if anything is unclear: https://discord.gg/Dx6ACDUj6N.

## The board says what is true right now

**https://justchicoo.github.io/Project-Ambrose/** is generated from this document, the phase files, the holds below and the open pull requests, and it is the thing to look at before anything else. It says for every milestone whether it is landed, being built right now, held, open to anyone, or waiting on a dependency, and it rebuilds whenever a claim opens or closes.

Its **state.json** is the same thing for a machine: `https://justchicoo.github.io/Project-Ambrose/state.json` carries every milestone with its status, what it needs, what it unlocks, who holds it and how to claim it, plus the rules in a `how_to_use` list. A contributor's assistant should read that file before planning anything, and again before it pushes.

This page stays the rulebook. The board is the live view of it, and where the two ever disagree, the checks in `apps/ci` and `apps/site` fail until they agree again.

## Only the milestones named below

**Open now** is the whole list, and since 2026-09-27 it names every milestone whose dependencies are all built, in every phase including the panel, except two kinds: the few one of the maintainer's own sessions is building right now, which each carry a hold of their own, and those whose remaining checks can only be run with the maintainer's own game client or capture. A milestone goes into Open now as soon as it becomes ready. A pull request for a held milestone cannot pass its checks, so take one from the table, and ask in the Discord if the one you want is missing from it.

`python apps/progress/ready.py` prints every milestone whose dependencies are all finished and marks each one from this document's tables, counting anything it does not name as reserved, so the tool and this page can never drift apart. `--open` narrows it to the ones nobody holds, `--blocked` says what is waiting and on what.

## Holds

`doc/work/holds.json` is where the maintainer's own sessions say what they are building. A hold names a scope, who holds it and what they are on, and it comes in two sizes:

- `milestone:4.02` closes one milestone.
- `phase:17` would close a whole phase. None does: since 2026-09-27 every hold names one milestone that one of the maintainer's own sessions is building right now, so the rest of every phase, the panel included, is open as soon as it is ready.
- A phase hold may carry `"except": ["17.10"]`, which opens exactly those milestones out of it. That is how a piece that collides with nothing gets handed out while the rest of the phase stays closed, and it is deliberate each time rather than a default. Only a phase hold may carry one, every id in it has to be a real milestone of that phase, and a milestone the same file also holds by name stays held, so an exception can never override a hold meant for it.

A hold is not advice. `apps/ci/ci_contrib_paths.py` refuses a branch named for a held milestone and says who holds it, so a pull request for one cannot pass its checks, and the board never lists it as open. When a session finishes and moves on, the hold goes and whatever it covered becomes takeable in the next build of the board.

If a hold is in the way of something you want to build, say so in the Discord. Holds are there to stop collisions, not to hoard work.

## Taking one

1. Say in the Discord which one you are taking, or open the pull request as a draft straight away. Whoever opens a draft first holds it.
2. Branch from `upstream/main`, and name the branch `milestone/<id>-<short-name>`, such as `milestone/4.04-world-wire-math`. The name is not decoration: `apps/ci/ci_contrib_paths.py` reads it, and it is the only reason CI lets the change touch `src/`.
3. Open the pull request early, as a draft, titled `<id> <what you are building>`. That is what reserves it. The maintainer moves the row into **In flight** with your name on it.
4. One milestone per pull request. Never build a second milestone's branch on the first one's.

If you go quiet for two weeks the row goes back to **Open now**, with whatever you pushed left in place, so somebody else can carry it.

The maintainer's sessions may also take a milestone over when the roadmap needs it sooner, for example when it blocks the milestone they are building or a check needs the maintainer's own machines. You hear it in one message on your pull request, and whatever of your work the landing uses keeps your name on the commit.

## What finishing one means

A milestone is finished when **every acceptance check in its phase file is ticked**, in the same commit as the code that earns them, and not before. Most milestones carry two lists: the short one under the milestone heading and the full one at the end of the detailed spec. Both are the same checks in different detail, and both get ticked.

A ticked check quotes what proved it, in brackets, the way the ones already ticked do:

```
- [x] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step (MovementPackingTest.YawRoundTrip)
```

The name of the test that runs it, the tool run and what it printed, or the screen and what it showed. Evidence names nothing personal: a real account, address, path or machine name is written `<account>`, `<address>` and so on. A check with no evidence in brackets is not ticked, and a check ticked by a test that does not exist is the one thing that ends a review immediately.

**A check you cannot run stays unticked.** Some are labelled Dev-gated, Client-gated or Real client, and need an installation, a second machine or hardware you may not have. Leave those boxes empty, say in the pull request exactly which ones and why, and send the rest. Better still, say so in the draft while you are still building and ask: the maintainer can often run a gated check against their own install there and then, and on 16.02 that is what showed a whole XML shape was wrong on work that was otherwise sound. The work merges, the milestone stays open, and the row moves to **In flight** with what is left written next to it. That is an honest, welcome outcome. Ticking a box you did not run is not.

**Build what the milestone says, not around it.** The deliverables list under the detailed spec names the files to write, the tables to add and the client messages involved. If one of them is wrong or impossible, say so in the pull request and propose the change. Do not quietly build something else: the acceptance checks are written against those deliverables and a review reads them together.

## What a milestone branch may change

Everything under `src/`, `data/sql/updates/`, `apps/` and `doc/` except the list below, plus its own phase file. The check enforces exactly that:

```
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD --branch milestone/<id>-<short-name>
```

It refuses another phase's file, so a change that needs one is a change of scope, and it refuses these, which the maintainer keeps so that concurrent work never collides in them: `.github/`, `apps/ci/`, `apps/codestyle/`, `apps/progress/`, `doc/progress/`, `doc/work/`, `packages/ui/src/tokens/`, `README.md`, `CONTRIBUTING.md`, `CLAUDE.md`, `LICENSE`, `CMakePresets.json`, `.gitignore`, `doc/ROADMAP.md`, `doc/ARCHITECTURE.md`, `doc/REVIEWING.md`, `doc/CONTRIBUTOR-TRACK.md`, `doc/MILESTONE-TRACK.md`, `contrib/README.md`, `contrib/AI-START-HERE.md` and `contrib/AI-MILESTONES-HERE.md`. A milestone whose own deliverables are among those files is granted exactly them, file by file, in `doc/work/grants.json`, which the check reads: 3.19's two SQL checks and the workflow that runs them are the first.

`doc/ROADMAP.md`'s "Where we are" and the progress card are written by the maintainer when the milestone lands, from the boxes you ticked. A milestone that needs a library may add it to `vcpkg.json` with its licence notice in `THIRD-PARTY-NOTICES.md` and say why in the description. When the milestone rests on code that is missing or unfinished, build it in the same pull request, anywhere this section allows, and say so in the description: a milestone is never refused because something it needs was not there yet. `python apps/ci/ci_local.py --branch milestone/<id>-<short-name>` runs this path check with every other step of CI's checks job.

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

doc/REVIEWING.md is the rulebook, and its first line applies hardest here: verified by running, never by reading. Expect the maintainer to build the branch, run its tests, run the ones it claims by name, and try the failure the code says it handles. A branch named for a milestone builds the Linux GCC leg in CI by itself, without waiting for a label, and the maintainer adds a `ci:` label for the Windows leg or the sanitizers when the change deserves them.

While it is not ready, each review round is one message written so your AI can act on it without guessing: what works and what was verified, then a numbered list of everything left before it merges, each item giving the file and line, what is wrong, the exact change, and the command or test that proves it passes, and last the line "push to the same branch and it will be rechecked". Every round restates the whole list, so the newest message is all your AI needs to read. Hand that message to your AI as it is, have it do every item and run every proving command, and push to the same branch. A long list says which items can follow in a second pull request on the same milestone.

Nothing is closed for being unfinished. Missing or unfinished code, a missing deliverable, a failing check or something the milestone rests on that does not exist yet gets a pointer to the fix and where to build it, and the pull request stays open until it lands. It lands when it moves the project forward, a check earned by a test that was run, a real fix, or a sound part of the milestone with what is left named, and what it still needs that the maintainer can do at landing is fixed on `main` with you kept as author, with the message saying what changed so your next one needs less. A pull request that ticks checks lands as one commit on `main` authored to you and is closed with the label `landed`, which means it is in; one that ticks nothing is merged on GitHub and shows as merged. The one thing that holds a pull request outright is a file from the game client, which comes out first. Run the checks job yourself before every push with `python apps/ci/ci_local.py --branch milestone/<id>-<short-name>`, and the tests the way contrib/AI-MILESTONES-HERE.md shows, and the review has less to find.

## Open now

| ID | Milestone | Size | What you need | Why it is a good one to take |
|---|---|---|---|---|
| 12.07 | Chat moderation | M | A build, MySQL or MariaDB, and a client of my own for its real-client check, which stays unticked without it | Accounts get open, filtered or closed chat, filtered words are handled the way the client expects, and a GM can mute a player for a time, on the chat 6.03 and the commands 6.04 built. The facts 6.03 read from the client are in doc/ARCHITECTURE.md under "Chat and emotes" and "Commands in chat": a line's Message is a u16 count of UTF-16 units, Filter is the speaker's chat level (2 open, 1 menu, 0 none), the receiving client filters with Root.wad's ChatFilter lists, and command lines never reach the relay |
| 12.01 | Friends | M | A build, MySQL or MariaDB, and two clients of my own for its real-client check, which stays unticked without them | Wizards add, accept, deny and remove each other as friends and see who is online and where, on the chat and presence 6.03 and 6.01 built: the buddy messages in a handler of their own under `src/server/game/Handlers`, the friend rows in a new `data/sql/updates/db_characters` file, and `Social.MaxFriends` as a live setting. Leave `ChatHandler.cpp` alone, which the maintainer's world session is changing for 6.04, and keep additions to `GameSession` small and named in the pull request |
| 12.14 | Custom emotes and pet rename | M | A build, MySQL or MariaDB, and two clients of my own for its real-client check, which stays unticked without them | An unlocked emote appears in the wheel and plays for everyone near. Start from the review note at the top of `doc/roadmap/phase-12-social-wizards-instances-and-realms.md`, which records what 6.03 read of the radial menu's emote messages; the first thing to run down is why the quick chat menu's Emotes entry did not open the wheel for our wizard. Leave `ChatHandler.cpp` alone while 6.04 changes it |

## Reserved

Everything not in the table above, including every milestone whose dependencies are met but which is listed here, so `apps/progress/ready.py` says so rather than leaving it to be guessed.

| ID | Why |
|---|---|
| 16.11 | Overlaps the type extraction already built in 3.21 and is being rethought |
| 17.01 | One Dev-gated check, on the maintainer's own Windows console and Linux terminal, and nothing else left to build |
| 3.26 | Landed on 2026-09-30 by the maintainer's panel session with six of seven checks. Left: the Dev-gated Play check, which waits for a real-client window on the maintainer's machine |
| 17.47 | Landed on 2026-09-27 by the maintainer's panel session with four of five checks. Left: the API-key half of check 2, which waits on 17.36's keys |
| 17.18 | Landed on 2026-09-27 by the maintainer's panel session with seven of eight checks. Left: check 2's download and share paths, which 17.54 and 17.39 add |
| 17.24 | Held by the maintainer's panel session: hosting a game on this computer, built on the panel program of 17.181 |
| 17.28 | Being built by the maintainer's panel session, which the maintainer asked to finish the panel first |
| 17.165 | Being built by the maintainer's panel session: the decoded data routes every game data page in 17.166-17.177 rests on |
| 17.178 | Kept for the maintainer's panel session, after the game data pages |
| 17.179 | Two of its window checks reopened on 2026-09-30 by the maintainer's panel session, which builds the rest: the page loads but was not shown to render, and a page is done only when it shows real content |
| 17.181 | Queued for the maintainer's panel session now that 17.180 and 17.179 have landed: the panel program's own window and its list of panels |
| 17.166 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the archive browser |
| 17.167 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the locale text browser |
| 17.168 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the template explorer |
| 17.169 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the protocol browser |
| 17.170 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: client scans from the panel |
| 17.171 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the opt-in client program reader |
| 17.172 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the progression and character creation pages |
| 17.173 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the world tables browser |
| 17.174 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the zone catalog |
| 17.175 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the live world pages |
| 17.176 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the spells and sigils pages |
| 17.177 | Queued for the maintainer's panel session, one of the game data pages it builds after 17.165 in order from 17.166 to 17.177: the characters and online players pages |
| 17.182 | Queued for the maintainer's panel session after 17.178, 3.26 and 17.24, in order from 17.182 to 17.186: the panel program in the tray |
| 17.183 | Queued for the maintainer's panel session after 17.178, 3.26 and 17.24, in order from 17.182 to 17.186: installing the panel program |
| 17.184 | Queued for the maintainer's panel session after 17.178, 3.26 and 17.24, in order from 17.182 to 17.186: signed updates for the desktop programs |
| 17.185 | Queued for the maintainer's panel session after 17.178, 3.26 and 17.24, in order from 17.182 to 17.186: the Ambrose service from the panel program |
| 17.186 | Queued for the maintainer's panel session after 17.178, 3.26 and 17.24, in order from 17.182 to 17.186: reaching a panel through SSH |
| 8.04 | Built on 2026-09-26 by the maintainer's world session, 10 of 12 checks. The game master's real-client check is next in that session now that 6.04's in-game commands have landed. The real-client check that shows a spell in the Spell Deck is waiting on the deck 8.10 and 8.11 build, which needs 8.09, 8.07, 7.04 and 8.05 first |
| 9.02 | Built on 2026-09-26 by the maintainer's world session, 6 of 7 checks. Left: its real-client check, which 6.04's in-game commands unblocked and which is queued in that session |
| 8.02 | Queued for the maintainer's world session after 8.01: experience and level-up, the next step on the way to a playable game |
| 6.13 | Queued for the maintainer's world session after its real-client batch since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished: volumes and walk-in trigger events, which 7.07 needs. Every dependency is built since 6.12 landed on 2026-09-30 |
| 3.02 | Reopened on 2026-09-30 by the maintainer's world session, which builds the rest: its Badges page opens but shows no badges, because the server sends an empty list, and a page is done only when the real client shows real content in it |
| 7.02 | Queued for the maintainer's world session after 6.13: the quest, dialog and madlib model the quest engine rests on. 3.02, which it depends on, closed on 2026-09-30 |
| 7.03 | Queued for the maintainer's world session after 7.02: the quest schema, sQuestMgr and its validator |
| 7.07 | Queued for the maintainer's world session after 7.03: the NPC service menu. Waiting on 6.13 and 7.02 |
| 7.04 | Queued for the maintainer's world session after 7.07: the requirement engine |
| 7.06 | Queued for the maintainer's world session after 7.04: character quest persistence |
| 7.08 | Queued for the maintainer's world session after 7.06: the wizbang indicators above quest givers |
| 7.09 | Queued for the maintainer's world session after 7.08: the quest offer |
| 7.10 | Queued for the maintainer's world session after 7.09: accepting a quest and the quest book |
| 8.06 | Queued for the maintainer's world session after 7.10: the item template extractor |
| 8.08 | Queued for the maintainer's world session after 8.06: the backpack's item instances |
| 8.09 | Queued for the maintainer's world session after 8.08: the backpack's stacks, locks and overflow |
| 4.08 | Being finished by the maintainer's track session since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, on MeruneFleuruwu's decoding of every zone and the world session's writer in `extractor zones`. Left: the four checks that count every entry, which wait only on accepting the four sigil classes with their shared property kept under its hash, as settled in doc/ARCHITECTURE.md, and the spawn data the two integration tests read |
| 8.01 | Parked by the maintainer's world session since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, until its real-client batch and the phase 3 to 6 milestones queued before it are done: live health, mana, gold and potions on the HUD |
| 3.28 | Taken over by the maintainer's track session since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, building on MeruneFleuruwu's [#10](https://github.com/Justchicoo/Project-Ambrose/pull/10), which landed on 2026-09-26 with checks 3 and 4: Type.name, Type.hash and the std::string layout derived from the client's own constructor. Left: the rest of the layout the same way, checks 1 and 2 once every field is derived, check 5 on a second client, and the record of extracted clients |
| 6.17 | Taken over by the maintainer's track session since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, building on MeruneFleuruwu's [#12](https://github.com/Justchicoo/Project-Ambrose/pull/12), which landed on 2026-09-29 with six of eight checks. Left: the two 10-minute fuzz runs on the frame and decode paths, which need the `linux-clang-fuzz` preset |
| 6.18 | Built on 2026-09-30 by the maintainer's track session, with the module hook check earned. Left: a real client at login showing the named, redacted log line, and `.network sessions` and `.network packetlog filter` in game chat, for the next driver run |
| 5.06 | Being built by the maintainer's world session since 2026-10-01, after 3.23 landed: MSG_USER_VALIDATE checks a PassKey3 against the session key a login stored, now sealed like a verifier, so quitting from the world lands on character select with no password |
| 6.05 | Queued for the maintainer's world session after its real-client batch since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, after 3.17 |
| 6.06 | Queued for the maintainer's world session after its real-client batch since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished |
| 6.07 | Queued for the maintainer's world session after its real-client batch since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, after 6.06 |
| 6.14 | Queued for the maintainer's world session after its real-client batch since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, after 6.07 and 6.13 |
| 6.16 | Queued for the maintainer's world session after its real-client batch: area of interest in the real client, on the grid and visibility sets 6.15 landed on 2026-09-30, with the two real-client checks 6.15 handed over |
| 3.27 | Queued for the maintainer's panel session after 3.26 since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished; it also waits on 17.105, 17.183 and 17.184 |
| 5.08 | Taken over by the maintainer's panel session since 2026-09-30, when the maintainer asked for every milestone before 8.01 to be finished, after 3.26 and 3.27, building on MeruneFleuruwu's [#4](https://github.com/Justchicoo/Project-Ambrose/pull/4): the scripts, env.dist and doc/INSTALL.md, with the conf check earned. Left: a clean Ubuntu and a clean Windows machine reaching 'ready' on all three apps |

## In flight

A row that says **before the reset** came from a pull request that was on the repository before it was recreated, so its number points at nothing now and the link is gone. That work is in the tree either way, and those pull requests are archived off the repository. Rows with a number are live.

| ID | Who | Sent as | What is left |
|---|---|---|---|
| 17.91 | MeruneFleuruwu | [#14](https://github.com/Justchicoo/Project-Ambrose/pull/14) | Landed on 2026-09-29 with checks 1, 3 and 5 earned, after review timed 6.01's movement relay as its own subsystem. Left: a benchmark showing the accumulators cost nothing measurable while no profile runs (check 2), and a tick alert naming the subsystem (check 4), which waits for 17.67's alert rules |
| 17.23 | MeruneFleuruwu | [#9](https://github.com/Justchicoo/Project-Ambrose/pull/9) | Landed on 2026-09-25 with the Compose and time zone checks earned, after review added the type extractor and the SQL to the image and the egg, Python to their build, a health check that probes the login port and a .dockerignore. Left: the three Dev-gated checks, a reboot after `--install-service` on Windows and on Linux and the two Pterodactyl ones, which the maintainer runs on their own machines |
| 17.35 | MeruneFleuruwu | [#8](https://github.com/Justchicoo/Project-Ambrose/pull/8) | Landed on 2026-09-26 with checks 2, 3 and 5 earned, the routes and the page under `panel.settings`. Left: an SMTP transport and the mail test that uses it (check 1), sealing the saved password at rest once the supervisor has a key, and the captcha at sign-in refusing when its provider cannot be reached (check 4) |
| 16.03 | MeruneFleuruwu | [#6](https://github.com/Justchicoo/Project-Ambrose/pull/6) | The scanner is delivered and four checks are earned, two of them re-run by the maintainer on a real install rather than only in a fixture. Size, CRC, HeaderSize and HeaderCRC are right for 3589 of 3589 type 3 and 5 records, and a cached run is 194 seconds down to 1 with a byte-identical .bin. Left: package membership, 3820 of 3825, because `Windows/PatchClient/` is not matched and the manifest files scan themselves in; and four fields no check names, `TarFileName`, `CompressedHeaderSize`, the 40 type 5 WADs and the header fields on plain files |

## Landed

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

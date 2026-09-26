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

**Open now** is the whole list. A milestone that is not in it is reserved, whatever its dependencies say, because it is being built right now, it is next in the maintainer's own queue, or its acceptance can only be run on the maintainer's machine. A pull request for a reserved milestone is closed, and that is a waste of your evening, so take one from the table or ask in the Discord for another to be opened.

`python apps/progress/ready.py` prints every milestone whose dependencies are all finished and marks each one from this document's tables, counting anything it does not name as reserved, so the tool and this page can never drift apart. `--open` narrows it to the ones nobody holds, `--blocked` says what is waiting and on what.

## Holds, and why a whole phase can be closed

`doc/work/holds.json` is where the maintainer's own sessions say what they are building. A hold names a scope, who holds it and what they are on, and it comes in two sizes:

- `milestone:4.02` closes one milestone.
- `phase:17` closes a whole phase, every milestone in it, however ready one of them looks on its own. The panel is built as one long thread of work, so a milestone taken out of the middle of it collides with something being built the same week.
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

It refuses another phase's file, so a change that needs one is a change of scope, and it refuses these, which the maintainer keeps so that concurrent work never collides in them: `.github/`, `apps/ci/`, `apps/codestyle/`, `apps/progress/`, `doc/progress/`, `doc/work/`, `packages/ui/src/tokens/`, `README.md`, `CONTRIBUTING.md`, `CLAUDE.md`, `LICENSE`, `THIRD-PARTY-NOTICES.md`, `CMakePresets.json`, `vcpkg.json`, `.gitignore`, `doc/ROADMAP.md`, `doc/ARCHITECTURE.md`, `doc/REVIEWING.md`, `doc/CONTRIBUTOR-TRACK.md`, `doc/MILESTONE-TRACK.md`, `contrib/README.md`, `contrib/AI-START-HERE.md` and `contrib/AI-MILESTONES-HERE.md`.

`doc/ROADMAP.md`'s "Where we are" and the progress card are written by the maintainer when the milestone lands, from the boxes you ticked. A new dependency in `vcpkg.json` is a proposal in the pull request, not a commit.

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

While it is not ready, each review round is one message: what works and what was verified, then everything left before it merges, each item with how to fix it and how to check the fix, so the next push can be the one that merges. A long list says which items can follow in a second pull request on the same milestone.

Then one of four things happens, each with one message saying which and why: it merges and the milestone is marked landed; it merges with the milestone left open because gated checks remain; it merges and the maintainer fixes what review found on `main`, with you kept as co-author; or it is closed with the reason and what would make it mergeable.

## Open now

| ID | Milestone | Size | What you need | Why it is a good one to take |
|---|---|---|---|---|
| 6.17 | Network hardening | M | A build. No client: every check is a fuzz run, a test client or an integration test. The fuzz targets want Clang with libFuzzer, the `linux-clang-fuzz` preset, and a randomized test where the compiler has none | Malformed, oversized and abusive traffic disconnected predictably instead of crashing or stalling a server: a per-session token bucket, per-address connection and accept-rate caps, strict DML length checks and a send-queue high-water mark, all live settings, with fuzz targets for the frame and decode paths beside `src/test/fuzz/ObjectPropertyFuzzer.cpp`. Keep to this boundary: the bucket and the caps belong in the shared session and `SocketMgr` layer, so both servers get them, and nothing goes in `GameSession.{h,cpp}`, `src/server/game/Handlers`, `src/server/game/Entities` or the login server's session handlers. New keys go in the Network block of `src/server/shared/Settings/SettingDeclarations.cpp`, and `SettingsDocTest` holds `doc/config/settings.md` to that table, so regenerate the page rather than editing it. Opened on 2026-09-25 |
| 17.49 | Panel audit scope, app relay and command history | M | A build, Node 20+ and a browser. No MySQL or MariaDB: the supervisor keeps its own store | Nothing the panel does happens without a record written in the same transaction as the change, the browser never holds an app's token, and one command history follows a user between browsers. The permission catalog, roles and grants it depends on landed in 17.48 and the audit tables in 17.14, so this wires parts that already exist. All six checks run on your own machine. The level the relay passes is settled under Panel operations in doc/ARCHITECTURE.md. Spared from the phase hold on 2026-09-25 |
| 17.91 | Tick breakdown and on-demand profiles | M | A build, Node 20+ and a browser | A slow tick says which subsystem took the time, published through the metrics registry 17.09 built and drawn under the graphs 17.19 built. Measure at the tick's own call sites in `src/server/game/World`: `GameSession` is still being changed for 4.06, 4.11 and 4.16, so leave it and `src/server/game/Handlers` alone. All five checks run on your own machine. The capture writes a Chrome trace event file with no profiler library, as Panel operations in doc/ARCHITECTURE.md settles. Spared from the phase hold on 2026-09-25 |
| 17.106 | Error reports: source locations, grouping and a report file | L | A build, Node 20+ and a browser | An operator's errors reach the maintainer as a file naming the build, file and line each was raised at, so a bug report can be fixed without a screen share. It is large and it changes the logging macros every file uses, so read the note on macros with commas in contrib/AI-MILESTONES-HERE.md before touching them. All six checks run on your own machine. Spared from the phase hold on 2026-09-25 |

## Reserved

Everything not in the table above, including every milestone whose dependencies are met but which is listed here, so `apps/progress/ready.py` says so rather than leaving it to be guessed.

| ID | Why |
|---|---|
| 1.18 | Answered against the maintainer's own capture of a session |
| 2.14 | Built but for its real-client checks: a banned account refused with a visible message, which contributor item C-77 turns into a scenario, and an optional sniffer check |
| 3.02 | Built but for one check, which waits on a real client being sent MSG_BADGES, which NET and WIZ have not built yet |
| 3.20 | Built but for one check, which has to be run on a real terminal against a real install |
| 3.12 | Its remaining checks wait for 6.10 and for a real client session |
| 3.23 | Next in the maintainer's own queue |
| 4.02 | Held by the panel session, which needs it for the panel's roles. It is built and passes every automated check; its one real-client check waits on another milestone rather than on anybody's time |
| 4.06 | Built but for one check, a bad key refused with MSG_ATTACHFAILED on a real client, which is next in the maintainer's world session |
| 4.11 | Being built now, and the world entry after it runs through the same files |
| 16.11 | Overlaps the type extraction already built in 3.21 and is being rethought |
| 17.01 | One Dev-gated check, on the maintainer's own Windows console and Linux terminal, and nothing else left to build |
| 3.26 | Unblocked by the design system landing, and next in the maintainer's own queue after the terminal dashboard |
| 17.12 | Held with phase 17 for the panel session, which consumes it. The registry already has Set, Reset, Get, List, Describe and History; its batch route needs an all-or-nothing SetMany that validates every key before persisting any |
| 17.47 | Kept for the maintainer's panel session, beside the roles it builds on |
| 8.04 | Built on 2026-09-26 by the maintainer's world session, 10 of 12 checks. Left: the game master's real-client check, which waits on 6.04's in-game commands, and the real-client check that shows a spell in the Spell Deck, which waits on the deck 8.10 and 8.11 build |
| 9.02 | Built on 2026-09-26 by the maintainer's world session, 6 of 7 checks. Left: its real-client check, which waits on 6.04 |
| 5.02 | Kept for the maintainer's world session: it starts the chain through 6.01, 6.03 and 6.04 to 8.01 vitals behind the same menus |
| 4.08 | Taken over by the maintainer's world session on 2026-09-26 for 5.02, which spawns from its zone_object rows and needs every eligible object: the writer skips an object it cannot read without a word, so WC_Hub writes 177 rows for 183 objects, and spawn data is not extracted. MeruneFleuruwu's decoding of every zone, with real positions and display keys, is in the tree and is what it builds on |
| 6.09 | Taken over by the maintainer's world session on 2026-09-26, because it is the only road to the journal's inventory, through 6.10, 7.01, 8.06 and 8.08. MeruneFleuruwu's sweep and its two measurable checks are in the tree; left is the oracle that names the 104 unknown classes |

## In flight

A row that says **before the reset** came from a pull request that was on the repository before it was recreated, so its number points at nothing now and the link is gone. That work is in the tree either way, and those pull requests are archived off the repository. Rows with a number are live.

| ID | Who | Sent as | What is left |
|---|---|---|---|
| 3.28 | MeruneFleuruwu | [#10](https://github.com/Justchicoo/Project-Ambrose/pull/10) | Landed on 2026-09-26 with checks 3 and 4 earned: Type.name, Type.hash and the std::string layout derived from the client's own constructor with chosen values, the other 27 fields reported as assumed, and strict mode refusing by name. Left: the rest of the layout the same way (the list initializer, an enum option through the race adder, a property once its registering function is found, the map node), checks 1 and 2 once every field is derived (the maintainer runs the r801440 half), check 5 on a second client, and the record of extracted clients |
| 17.23 | MeruneFleuruwu | [#9](https://github.com/Justchicoo/Project-Ambrose/pull/9) | Landed on 2026-09-25 with the Compose and time zone checks earned, after review added the type extractor and the SQL to the image and the egg, Python to their build, a health check that probes the login port and a .dockerignore. Left: the three Dev-gated checks, a reboot after `--install-service` on Windows and on Linux and the two Pterodactyl ones, which the maintainer runs on their own machines |
| 17.35 | MeruneFleuruwu | [#8](https://github.com/Justchicoo/Project-Ambrose/pull/8) | Landed on 2026-09-26 with checks 2, 3 and 5 earned, the routes and the page under `panel.settings`. Left: an SMTP transport and the mail test that uses it (check 1), sealing the saved password at rest once the supervisor has a key, and the captcha at sign-in refusing when its provider cannot be reached (check 4) |
| 16.03 | MeruneFleuruwu | [#6](https://github.com/Justchicoo/Project-Ambrose/pull/6) | The scanner is delivered and four checks are earned, two of them re-run by the maintainer on a real install rather than only in a fixture. Size, CRC, HeaderSize and HeaderCRC are right for 3589 of 3589 type 3 and 5 records, and a cached run is 194 seconds down to 1 with a byte-identical .bin. Left: package membership, 3820 of 3825, because `Windows/PatchClient/` is not matched and the manifest files scan themselves in; and four fields no check names, `TarFileName`, `CompressedHeaderSize`, the 40 type 5 WADs and the header fields on plain files |
| 1.21 | MeruneFleuruwu | [#5](https://github.com/Justchicoo/Project-Ambrose/pull/5) | doc/PATCHING.md now matches what is built and the deliverable line names src/tools/launcher. Left: all three acceptance checks, which watch a real client, one listener seeing no patch connection, one recording what the client does with no `-P`, and one seeing no 'Patch failed' dialog |
| 5.08 | MeruneFleuruwu | [#4](https://github.com/Justchicoo/Project-Ambrose/pull/4) | The scripts, env.dist and doc/INSTALL.md are delivered and the conf check is earned, verified by running both of them. Left: the check that a clean Ubuntu and a clean Windows machine reach 'ready' on all three apps, which needs those machines. The maintainer added the self-tests and fixed a relative install prefix that resolved against the working directory |
| 5.07 | MeruneFleuruwu | before the reset | The cache is built, wired into the login server and measured at a third of the JSON path's time on the pinned install, and a truncated, bit-flipped or random cache is refused by name. Left: a client-gated comparison of every class, property and enum table, and a measurement showing a load under 200 ms |
| 3.18 | MeruneFleuruwu | before the reset | The four acceptance checks are earned and ticked, but the deliverable asking for unit tests of the decision logic against an in-memory applied set, with no database, is not delivered: the three cases it names are covered by an integration test that skips wherever no database is configured. ARCHIVED files and module includes are also still to come |

## Landed

| ID | Who | Sent as | What landed |
|---|---|---|---|
| 9.01 | MeruneFleuruwu | [#11](https://github.com/Justchicoo/Project-Ambrose/pull/11) | The combat service's 36 messages registered at the orders the r806919 install gives them, MSG_COMBATMOVE and MSG_COMBATPHASEFORSPECTATORS round-tripping their wire fields, and the first in-world combat stubs, with MSG_COMBATMOVE refused outside the world and logged decoded inside it. All seven checks earned |
| 4.04 | MeruneFleuruwu | before the reset | The world's wire math, a position and a yaw packed into bytes, and the location string character select sends, earned by its own tests. Its last check, the MSG_ATTACH a real client sends after Play, was earned by the maintainer's session in the client driver's enter-world run on 2026-09-25 |
| 8.14 | MeruneFleuruwu | before the reset | A versionable object decoded from the client re-encodes byte for byte on the hat template, TemplateManifest.xml and a 2000-file sample. All four checks earned, the last two found by an audit to share the short list's evidence. Its property-order preservation is inert on every file tested, which the phase's review notes record |
| 17.10 | MeruneFleuruwu | [#7](https://github.com/Justchicoo/Project-Ambrose/pull/7) | A Prometheus and Grafana stack with three provisioned dashboards and an operations runbook, the first milestone spared out of a held phase. Both checks earned on a real gameserver: live graphs at 28 seconds, all eleven panels drawing |
| 16.02 | MeruneFleuruwu | [#3](https://github.com/Justchicoo/Project-Ambrose/pull/3) | One manifest read and written as both the client's XML and its binary form, keeping the table order the file declares. All five checks earned, including the env-gated one, which the maintainer ran against a reference XML: 3591 tables, Base 140, PatchClient 97 |
| 1.12 | MeruneFleuruwu | before the reset | The client's CRC variant proved against the pinned install's own archives, the synthetic header measurement, and a sweep that opens every GameData archive and reads every stored entry |
| 16.01 | MeruneFleuruwu | before the reset | The client's binary table list read and written byte for byte, proven against a reference list of exactly the size the check names, with all six checks earned |
| 1.06, 1.07, 1.08 | MeruneFleuruwu | before the reset | The last check of all three was stale: the locale round-trip it asks for is covered by a client-gated test that passes on the pinned install |

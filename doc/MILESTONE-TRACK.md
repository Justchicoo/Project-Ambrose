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

A hold is not advice. `apps/ci/ci_contrib_paths.py` refuses a branch named for a held milestone and says who holds it, so a pull request for one cannot pass its checks, and the board never lists it as open. When a session finishes and moves on, the hold goes and whatever it covered becomes takeable in the next build of the board.

If a hold is in the way of something you want to build, say so in the Discord. Holds are there to stop collisions, not to hoard work.

## Taking one

1. Say in the Discord which one you are taking, or open the pull request as a draft straight away. Whoever opens a draft first holds it.
2. Branch from `upstream/main`, and name the branch `milestone/<id>-<short-name>`, such as `milestone/4.04-world-wire-math`. The name is not decoration: `apps/ci/ci_contrib_paths.py` reads it, and it is the only reason CI lets the change touch `src/`.
3. Open the pull request early, as a draft, titled `<id> <what you are building>`. That is what reserves it. The maintainer moves the row into **In flight** with your name on it.
4. One milestone per pull request. Never build a second milestone's branch on the first one's.

If you go quiet for two weeks the row goes back to **Open now**, with whatever you pushed left in place, so somebody else can carry it.

## What finishing one means

A milestone is finished when **every acceptance check in its phase file is ticked**, in the same commit as the code that earns them, and not before. Most milestones carry two lists: the short one under the milestone heading and the full one at the end of the detailed spec. Both are the same checks in different detail, and both get ticked.

A ticked check quotes what proved it, in brackets, the way the ones already ticked do:

```
- [x] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step (MovementPackingTest.YawRoundTrip)
```

The name of the test that runs it, the tool run and what it printed, or the screen and what it showed. Evidence names nothing personal: a real account, address, path or machine name is written `<account>`, `<address>` and so on. A check with no evidence in brackets is not ticked, and a check ticked by a test that does not exist is the one thing that ends a review immediately.

**A check you cannot run stays unticked.** Some are labelled Dev-gated, Client-gated or Real client, and need an installation, a second machine or hardware you may not have. Leave those boxes empty, say in the pull request exactly which ones and why, and send the rest. The work merges, the milestone stays open, and the row moves to **In flight** with what is left written next to it. That is an honest, welcome outcome. Ticking a box you did not run is not.

**Build what the milestone says, not around it.** The deliverables list under the detailed spec names the files to write, the tables to add and the client messages involved. If one of them is wrong or impossible, say so in the pull request and propose the change. Do not quietly build something else: the acceptance checks are written against those deliverables and a review reads them together.

## What a milestone branch may change

Everything under `src/`, `data/sql/updates/`, `apps/` and `doc/` except the list below, plus its own phase file. The check enforces exactly that:

```
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD --branch milestone/<id>-<short-name>
```

It refuses another phase's file, so a change that needs one is a change of scope, and it refuses these, which the maintainer keeps so that concurrent work never collides in them: `.github/`, `apps/ci/`, `apps/codestyle/`, `apps/progress/`, `doc/progress/`, `packages/ui/src/tokens/`, `README.md`, `CONTRIBUTING.md`, `CLAUDE.md`, `LICENSE`, `THIRD-PARTY-NOTICES.md`, `CMakePresets.json`, `vcpkg.json`, `.gitignore`, `doc/ROADMAP.md`, `doc/ARCHITECTURE.md`, `doc/REVIEWING.md`, `doc/CONTRIBUTOR-TRACK.md`, `doc/MILESTONE-TRACK.md`, `contrib/README.md`, `contrib/AI-START-HERE.md` and `contrib/AI-MILESTONES-HERE.md`.

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

Then one of four things happens, each with one message saying which and why: it merges and the milestone is marked landed; it merges with the milestone left open because gated checks remain; it merges and the maintainer fixes what review found on `main`, with you kept as co-author; or it is closed with the reason and what would make it mergeable.

## Open now

| ID | Milestone | Size | What you need | Why it is a good one to take |
|---|---|---|---|---|
| 1.12 | Crypto basics: the CRC32 the client uses | S | Your own client installation | Three checks left and the code is already there: prove which CRC32 variant the client's archives use by checking a stored entry against its bytes, add the synthetic archive unit test, and sweep every archive in your own install |
| 16.02 | LatestFileList model and XML | S | A build; the gated check needs a reference XML you obtained yourself | The natural step after 16.01, which is landed: the same table list as a model that round-trips through both XML and the binary form, with the exact counts named, 3590 tables, 140 records in Base and 97 in PatchClient |

## Reserved

Everything not in the table above, including every milestone whose dependencies are met but which is listed here, so `apps/progress/ready.py` says so rather than leaving it to be guessed.

| ID | Why |
|---|---|
| 1.18 | Answered against the maintainer's own capture of a session |
| 1.21 | Run against the maintainer's own client and launcher |
| 3.12 | Its remaining checks wait for 6.10 and for a real client session |
| 3.23 | Next in the maintainer's own queue |
| 5.08 | The installer touches packaging and CI, which are the maintainer's |
| 16.11 | Overlaps the type extraction already built in 3.21 and is being rethought |
| 17.01 | One Dev-gated check, on the maintainer's own Windows console and Linux terminal |
| 17.09 | Next in the maintainer's own queue |
| 17.23 | Dev-gated on a reboot and a Pterodactyl install |
| 17.47 | Being built now |
| 17.73 | The panel's design system, being built now |
| 17.106 | Being built now |

## In flight

| ID | Who | Pull request | What is left |
|---|---|---|---|
| 4.08 | MeruneFleuruwu | [#126](https://github.com/Justchicoo/Project-Ambrose/pull/126) | Every zone decodes and the rows carry real positions and display keys. Left: the writer skips an object it cannot read without a word, so WC_Hub writes 177 rows for 183 objects and WC_Ravenwood 93 for 97, and spawn data is not extracted, so HalloweenSpawner1 and its requirement are absent |
| 4.04 | MeruneFleuruwu | [#125](https://github.com/Justchicoo/Project-Ambrose/pull/125) | Its own work is landed and eight checks are ticked. The two left are not its own: the handoff integration belongs to 4.05, which has since landed and may already earn it, and the last needs a real client session. That session closes three at once, this check, 4.05's and 4.06's, since all three want what a real client sends after Play, so they are run together rather than one at a time |
| 5.07 | MeruneFleuruwu | [#124](https://github.com/Justchicoo/Project-Ambrose/pull/124) | The cache is built, wired into the login server and measured at a third of the JSON path's time on the pinned install, and a truncated, bit-flipped or random cache is refused by name. Left: a client-gated comparison of every class, property and enum table, and a measurement showing a load under 200 ms |
| 6.09 | MeruneFleuruwu | [#128](https://github.com/Justchicoo/Project-Ambrose/pull/128) | The sweep is delivered and its two measurable checks are earned, but the oracle the milestone exists for is not: a run collects no unknown properties at all and names nothing for any of the 104 unknown classes, so 520243970 is still unnamed and the draft schema the next milestone feeds on comes out empty |
| 8.14 | MeruneFleuruwu | [#129](https://github.com/Justchicoo/Project-Ambrose/pull/129) | Both acceptance checks are earned and the byte comparison has teeth, but the order preservation this adds to PropertyObject is never what makes the bytes match: disabling it leaves every test passing, because on each file tested the client's order is already the ordinal one. It needs a file that requires it, or it should be removed with the per-object memory it costs |
| 3.18 | MeruneFleuruwu | [#127](https://github.com/Justchicoo/Project-Ambrose/pull/127) | The four acceptance checks are earned and ticked, but the deliverable asking for unit tests of the decision logic against an in-memory applied set, with no database, is not delivered: the three cases it names are covered by an integration test that skips wherever no database is configured. ARCHIVED files and module includes are also still to come |

## Landed

| ID | Who | Pull request | What landed |
|---|---|---|---|
| 16.01 | MeruneFleuruwu | [#131](https://github.com/Justchicoo/Project-Ambrose/pull/131) | The client's binary table list read and written byte for byte, proven against a reference list of exactly the size the check names, with all six checks earned |
| 1.06, 1.07, 1.08 | MeruneFleuruwu | [#130](https://github.com/Justchicoo/Project-Ambrose/pull/130) | The last check of all three was stale: the locale round-trip it asks for is covered by a client-gated test that passes on the pinned install |

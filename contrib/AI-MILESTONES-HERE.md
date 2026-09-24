<!-- Project Ambrose by Imjustchico: The prompt an outside contributor pastes into their own AI assistant to build one of the roadmap milestones that are open to outside help, with everything that assistant needs to know about this repository and the standard the work is held to. -->

# Start here, with your AI, to build a milestone

This is the harder door. contrib/AI-START-HERE.md is for the contributor track, which adds files in folders no milestone touches. This one is for the roadmap itself: real server code, in `src/`, judged against a milestone's own acceptance checks.

Everything inside the fence below is meant to be copied whole into any AI assistant, in one gesture. It is written as you speaking to that assistant. Paste it, answer its first question, and it has what it needs to work here without guessing. The short section after the fence is for you.

````
I am contributing to Project Ambrose, a Wizard101 server written from scratch in C++20 (github.com/Justchicoo/Project-Ambrose, MIT licensed). Help me finish one milestone from its roadmap, to the standard below. This is the project's own plan, not a side track: the code lands in `src/`, and the milestone is finished only when every acceptance check in its phase file is ticked with the evidence that proved it.

Read this whole prompt before you answer. Then ask me the questions at the end, and nothing before them.

## Read these before you plan anything

- **https://justchicoo.github.io/Project-Ambrose/state.json first, before anything else.** It is the project's live state, generated from the roadmap and the open pull requests, and it carries every milestone with a `status` of `landed`, `building`, `held`, `open`, `waiting` or `reserved`, plus what each needs, what it unlocks, who holds it and a `how_to_use` list of the rules. Fetch it, and take only a milestone whose status is `open`. If mine is anything else, stop and tell me what it says: a pull request for a held or claimed milestone is closed unread, and CI refuses the branch outright. The board a person reads is https://justchicoo.github.io/Project-Ambrose/ .
- `doc/MILESTONE-TRACK.md` - the rulebook behind that state: how one is claimed, what finishing means, what a milestone branch may change, and how holds work. A hold can cover a whole phase, so a milestone with every dependency built can still be closed to me.
- The phase file of my milestone, `doc/roadmap/phase-NN-*.md`, whole. Not only my milestone's section: the phase's **Review notes** at the top name faults the roadmap's own critic found, and the ones that name my milestone are mine to resolve.
- `doc/ARCHITECTURE.md` - the layering, the folder each subsystem belongs to, the file-header form per file type, the SQL update convention, and the settled Decisions. It is long; read the parts my milestone lands in.
- `CONTRIBUTING.md` and `doc/REVIEWING.md`. The second is the maintainer's own rulebook for judging this work, so it tells us exactly what will be checked and how.
- `doc/ROADMAP.md`'s "Where we are" paragraph, which says in one pass what actually runs today.
- Then only what my milestone needs: `doc/TOOLS.md`, `doc/CLIENT.md`, `doc/CAPTURE.md`, `doc/PATCHING.md`, `doc/PANEL.md`, `doc/config/*.md`, `doc/guides/logging.md`, `doc/guides/linux.md`.

## How a milestone is written, so you read it right

In the phase file my milestone is a section headed `## <id> <title>`, and it holds, in order: a one-line **Goal**; a **Size** and **Depends on** line; a short **Acceptance** list; then a detailed spec carrying **Deliverables** (the files to write, the tables to add, the settings to register), **Client messages**, **Data sources**, a fuller **Acceptance** list, and often **Risks**.

- The two Acceptance lists are the same checks at different detail. **Both get ticked.**
- The Deliverables list is binding. It names the files and the folders. If one of them is wrong or impossible, say so in the pull request and propose the change; do not quietly build something else, because the checks are written against those deliverables.
- A check that starts Dev-gated, Client-gated or Real client needs an installation, hardware or a client session. Some I can run, some I cannot.
- Everything the milestone depends on is already built, or the milestone would not be open to me. Everything after it is not. If my milestone seems to need something that does not exist, you have probably found a dependency the roadmap missed: say so rather than building it as well.

## The state of the project, so you assume neither more nor less than is true

- Phases 1 to 3 are built, and phase 4 has its core: 4.01 landed the world tick and the script and module loader. A real client, started by Ambrose's own launcher, reaches character select and stops. The session handshake, authentication, a wrong password and a retry, the character list and the shutdown notice are all that can be watched against Ambrose today. There is no world, no quest, no pet, no housing and no combat traffic yet, on either side.
- The pinned client revision is r806919 (Wizard 1.610). Read mine from `Bin/revision.dat` in my own install rather than copying an example.
- **These exist. Do not build them again.** `src/common/` has configuration, logging, threading, cryptography, encoding and utilities. `src/server/shared/Archives` reads KIWAD. `src/server/shared/Messages` loads the client's own message XML at run time into a registry. `src/server/shared/ObjectProperty` encodes and decodes both the compact wire format and the versionable BINd format, with a type registry, property enums, defaults, a JSON view and a fuzz test. `src/server/shared/ClientData` finds the install, keeps a per-revision type dump and extracts name tables. `src/server/shared/Network` has frames, the socket layer and the session handshake. `src/server/database` has the connection pool, the dated SQL updater and extraction. `src/server/game` has the world tick that owns the world thread, `ScriptMgr` and its hook classes, and `GameSession`, whose queue drains on that thread; a script or a module joins the build by existing, because CMake writes the loader from every `AddSC_` in `src/server/scripts` and every module under `modules/`. `ScriptLoader.h` lives in `src/server/scripts`, not in the scripting library, and `ScriptMgr::LoadScripts` takes the loader as an argument rather than reaching for it, so the hooks never depend on the content that uses them. `src/server/shared/Admin` serves the panel's API, and `apps/dashboard` is the panel. `src/tools` has `typeextract`, `bindecode`, `localetool`, `extractor`, `dbimport`, `launcher` and `template_extractor`.
- **These do not exist yet**, and a milestone that needs one is blocked rather than open: `CommandMgr` and security levels (4.02), the zone manager, the template store, and everything gameplay after them.
- The type dump `src/tools/typeextract` writes is version 2: an object with `version`, `revision`, `executable_sha256`, `extractor` and `classes`, keyed by class name, each class holding `name`, `bases`, `hash` and `properties`. It is built from my own client, into my own data folder, and is never committed.
- Tests are GoogleTest, in `src/test/` mirroring `src/`, in one executable run by CTest. A test that needs my installation carries the CTest label `client` and skips unless `AMBROSE_CLIENT_DIR` is set; one that needs my type dump reads `AMBROSE_TYPE_DUMP_PATH`; database tests run only when `AMBROSE_TEST_DB` holds a connection string such as `127.0.0.1;3306;root;root;ambrose_test`, and each creates uniquely named databases and drops them. `ctest` also runs the style and CI checks, so a green `ctest` is most of the review.
- A running server logs each message it refused or did not handle under `network.opcode`, but only up to `Network.DroppedMessageBurst = 64` per session and `Network.DroppedMessagesPerSecond = 16`, after which it adds a strike instead, and `Network.MaxStrikes = 10` closes the connection. A quiet log is not proof of quiet traffic.

## Hard rules. Breaking one closes the pull request, and each is there for a reason

1. **Paths.** My branch is named `milestone/<id>-<short-name>` and that name is what lets CI accept a change under `src/`. I may change the source tree, `data/sql/updates/`, `apps/` and `doc/`, plus **my own phase file and no other**. I may not touch `.github/`, `apps/ci/`, `apps/codestyle/`, `apps/progress/`, `doc/progress/`, `packages/ui/src/tokens/`, `README.md`, `CONTRIBUTING.md`, `CLAUDE.md`, `LICENSE`, `THIRD-PARTY-NOTICES.md`, `CMakePresets.json`, `vcpkg.json`, `.gitignore`, `doc/ROADMAP.md`, `doc/ARCHITECTURE.md`, `doc/REVIEWING.md`, `doc/CONTRIBUTOR-TRACK.md`, `doc/MILESTONE-TRACK.md`, `contrib/README.md`, `contrib/AI-START-HERE.md` or `contrib/AI-MILESTONES-HERE.md`. `python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD --branch <my branch>` enforces exactly that. A new dependency in `vcpkg.json` is a proposal in the pull request, not a commit.
2. **No game data in the repository, ever.** That rule is why this repository can exist in public. No file from the client, no extracted asset, no dump, no capture, no run of hex or base64 pasted from one. `apps/ci/ci_forbidden_files.py` refuses `.wad`, `.nif`, `.kf`, `.kfm`, `.pcap` and `.pcapng`, any file beginning `KIWAD` or `BINd`, any JSON holding both `classes` and `version`, client protocol XML, and anything over 1,000,000 bytes. Offsets, field names, sizes, counts and hashes are facts about the data and are welcome; the bytes are not. Code reads my own installation at run time, behind an environment variable, and skips when it is absent. That is the line.
3. **Clean room.** Nothing copied, translated or ported from another Wizard101 server, emulator, wiki or site. Behaviour may be studied; code and text are written from scratch. AzerothCore may be studied for structure, which is where the layout comes from, and not for code.
4. **House style.** Every file I add starts with the branding header and a one-line brief of what it holds and does, and carries **no other comment anywhere**. The forms are exactly: C and C++, a block comment whose lines are ` * Project Ambrose by Imjustchico` then ` * <brief>`; CMake, shell, PowerShell, Python, YAML and conf, `# Project Ambrose by Imjustchico` then `# <brief>`; SQL, the same with `--`; Markdown, one HTML comment on line 1 reading `Project Ambrose by Imjustchico: <brief>`. The handle is the maintainer's and never changes to mine. JSON and binary files carry no header. Files are UTF-8 with no byte order mark, LF endings, no trailing whitespace, ending in a newline, ASCII unless the content is itself a translation. `python apps/codestyle/codestyle.py` is the judge. Write the brief as a sentence about what the file does, not a label: read a neighbouring file's header and match it.
5. **C++20, and the architecture as written.** The folder a subsystem belongs to, the layering order, dated SQL update files, content in the world database, custom content in scripts or modules rather than core edits. A milestone is not the place to re-litigate a settled Decision.
6. **Evidence names nothing personal.** This repository is public. A ticked check that quotes a path, account, address or machine name writes `<account>`, `<address>` and so on instead. What matters is the behaviour the line shows.
7. **Never** run KingsIsle's launcher or patcher against an install I want kept at a fixed revision, and never capture or contact KingsIsle's own servers. Everything here is done against my own installation and my own server.

## What finishing the milestone means

**Every acceptance check in both lists is ticked, in the same commit as the code that earns it, and each one quotes what proved it**, in brackets, the way the ticked ones already do:

```
- [x] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step (MovementPackingTest.YawRoundTrip)
```

The name of the test that runs it, or the tool run and what it printed, or the screen and what it showed. **A check ticked by a test that does not exist ends the review immediately**, so before I push, grep the tree for every name you wrote in brackets and show me each one.

**A check I cannot run stays unticked.** If it needs an installation, a second machine, a client session or hardware I do not have, leave the box empty, name it in the pull request and say why. The work merges, the milestone stays open with what is left written next to it, and that is an honest outcome. Ticking a box we did not run is not, and it is the fastest way to have everything else in the branch distrusted.

**The shape of the work is not the work.** A header with no implementation, a test that asserts nothing, a table with no rows: each of those has been sent here before and none of them closed anything. If only part of the milestone is possible, build that part properly and say which part.

## The first hour, in order

1. Read the board's `state.json` and pick a milestone whose status is `open`. Tell me its id, its size and what it needs from me.
2. `git fetch upstream`, branch from `upstream/main` with the name the table below gives, and **open the draft pull request straight away**, with the plan in its description rather than an empty body. That reserves the milestone within minutes and, more importantly, puts the approach where a reviewer can see it before a week of work rests on it. One milestone here was rebuilt from scratch after review because nobody saw the design until it was finished.
3. Read the milestone's whole section in its phase file, then the phase's review notes, then whatever `doc/TOOLS.md` and `src/tools` already have for the format it touches.
4. Write the plan out for me: each acceptance check, what will earn it, and which I cannot earn on this machine. That list is the pull request description at the end, so writing it now costs nothing.
5. Build, so a broken toolchain surfaces before the work, not after it: `cmake --preset windows-msvc-x64` then `cmake --build --preset windows-debug` and `ctest --preset windows-debug`.
6. Then write the failing test, then the code.

## How I want you to work

1. **Find out whether Ambrose already has it, before writing that it lacks it.** This is the single most common fault in contributions here: two proposals in a row set out to add what the project had already built. Read "Where we are", then the phase file, then grep `src/` for the type or the file name, then grep `src/test/` for a test that already proves it. The cheapest disproof of "nothing does X" is the test that does X, and it takes five minutes.
2. **Before writing a parser, a decoder or anything that reads a file format, find what already reads it.** `doc/TOOLS.md` lists the suite and `src/tools` and `apps` hold it, and that list has drifted: something marked planned may exist, and something marked built may do more than its line says, so look at the folder rather than trusting the line. Using one of ours is the fast path, and **upgrading one is a welcome part of a milestone**: if a tool almost does what my milestone needs, teaching it that function is better work than writing a second copy inside the milestone, and it leaves the suite able to read more than it could before.

This is not a style preference, and one pull request in this batch shows why. 4.08's zone extractor wrote its own decode path for the client's zone data, and every position and orientation in its output came out empty, across all 3356 zones. The project already had a reader for exactly that format, in `bindecode` and the ObjectProperty serializer, and 8.14 had just made the versionable path byte-exact. Built on those, it would have inherited working vector decoding. The duplicate did not merely repeat work: it produced wrong data, and the milestone did not land.

So, before I write: name the format, say what in this repository already reads it, and tell me whether the plan is to call it, to extend it, or to explain why neither fits.
3. **Plan before writing.** What each acceptance check will be satisfied by, which file each deliverable lands in, and what the smallest failing test is. Put the cheapest experiment that could kill the approach first, so I do not spend a week on something wrong.
4. **Ask whose behaviour each check is about before deciding what would earn it.** A check about the client, the archive or the dump is about something outside my code, so a test that builds that input itself cannot earn it, however green it is: asserting fields arrive unchanged after setting them from the same source proves only that my test copies fields. If I cannot produce the real input, the honest move is to earn the narrower check my work does prove, leave the other unticked, and say which and why.
5. **Write the test before or with the code**, and make it fail first for the right reason. A check says the exact numbers to expect, such as packing (-2408.09, 2609.10, -7.13) landing within 4 units, or the first bytes of a written table. Assert those numbers, not a re-derivation of them, because a test that computes its own expectation proves only that the code agrees with itself.
6. **Verify by running, never by reading.** Build it, run the test, run the whole `ctest` suite, and run the tool on real input where there is one. Read the output rather than assuming it. When I paste output, read that too rather than agreeing with it.
7. **If the milestone's point is that something gets faster, smaller or quieter, measure it against what it replaces, and write the test that fails when it does not.** The first milestone sent here was a startup cache that loaded correctly and took twice as long as the JSON file it was replacing, because its payload was decoded and then handed back through the same parser. Nothing in it was careless; there was simply no measurement, so the one thing the milestone existed for was the one thing nobody checked. Time the old way and the new way on real input, print both, and keep the comparison as a test.
8. **Make it fail usefully.** Name the file and the reason, return a typed error rather than a bare code, carry on past what can be skipped rather than losing a whole run to one unreadable input, and say what would make the output wrong. A decoder that reports zero problems on a corrupt file is worse than one that stops.
9. **Respect the limits already in the code.** Decoding is bounded by depth, object, list, memory and inflation limits read from settings; a new path through it keeps those bounds. New settings go in the app's `.conf.dist` with the same naming as its neighbours.
10. **Keep the diff to the milestone.** Renaming, reformatting or improving code on the way past makes the change unreviewable and is the most common reason a sound pull request is sent back. If you find a real bug outside the milestone, say so and leave it.
11. **Resolve the phase's review notes that name my milestone**, in the milestone or in the pull request, saying which and how.
12. Write the files, then have me run every check below and fix whatever they print.
13. Write the pull request description: the milestone id, what was built, how it was verified, which checks are ticked, which are not and why.

## Where code goes, so a reviewer never has to move it

- `src/common/` is everything with no game in it: configuration, logging, threading, cryptography, encoding, small utilities.
- `src/server/shared/` is what more than one app needs: the archive reader, the message registry, the ObjectProperty codec, the network layer, the admin API, client data.
- `src/server/game/` is the world: the tick, sessions, scripting, movement, zones, entities, chat.
- `src/server/database/` is the pool, the dated updater and extraction, and `data/sql/updates/` holds the dated files themselves.
- `src/tools/` is a program somebody runs by hand, and `apps/` is the Python and front-end tooling beside it.
- `src/test/` mirrors whichever of those the code lives in, and the test for `src/server/game/Movement/X.cpp` belongs at `src/test/server/game/Movement/XTest.cpp`.

A milestone's deliverables name the folder. Where they and this disagree, follow the deliverables and say so in the description.

## Setting the whole thing up on my machine

Walk me through this once, step by step, waiting for what I actually see at each one. It ends with the servers, the panel and my own client running against each other, which is the setup the maintainer's own sessions work in. **Only the steps my milestone needs are worth doing first**, and the end of this section says which those are. None of it changes what I may take: the board decides that, and a machine set up beautifully gives me no claim on a held milestone.

**1. The toolchain.** CMake 3.25 or newer, vcpkg with `VCPKG_ROOT` set, and Visual Studio 2022+ or GCC 13+. Node 20+ only if I touch the panel, and a MySQL or MariaDB for anything that stores something.

```
cmake --preset windows-msvc-x64
cmake --build --preset windows-debug
ctest --preset windows-debug
```

On Linux the presets are `linux-gcc` and `linux-gcc-debug`, and `doc/guides/linux.md` is a guide somebody walked on Ubuntu 24.04. The first configure builds every dependency from source and takes about an hour; later ones are fast. The build is warnings-as-errors on both compilers, and MSVC and GCC disagree about what is a warning, so tell me which platform I built on and we say so in the pull request.

**2. The databases.** Three of them, `ambrose_login`, `ambrose_characters` and `ambrose_world`, created by the `dbimport` tool in the build's output folder rather than by hand. The default connection string for each is `127.0.0.1;3306;ambrose;ambrose;ambrose_login` and so on, meaning host, port, user, password, database, so the quickest start is a MySQL user named `ambrose` with password `ambrose` that may create databases. Anything else goes in `AMBROSE_LOGIN_DATABASE_INFO`, `AMBROSE_CHARACTER_DATABASE_INFO` and `AMBROSE_WORLD_DATABASE_INFO`, or in `dbimport.conf`. `dbimport` opens each pool once to prove it works, then exits 0, or 1 naming the first failure. The database tests are separate: they run only when `AMBROSE_TEST_DB` holds a connection string such as `127.0.0.1;3306;root;root;ambrose_test`, and each one makes uniquely named databases and drops them.

**3. The configuration files.** Every app needs its own `<app>.conf` next to the executable, copied from the `<app>.conf.dist` the build puts there. Started without one, an app exits naming the full path it wanted and the `.dist` to copy, so the error is the instruction. `doc/config/README.md` has the file format and the layers, and `doc/config/<app>.md` documents every option that app takes, because the files themselves carry no comments beyond their header.

**4. The client data, which sets itself up.** On a first start with `Setup.Mode = auto`, which is the default, a game server finds the newest Wizard101 installation on my machine, builds its type dump by emulating the client's own program, extracts the name tables, and saves the choice to `conf.d/client-data.conf`. `AMBROSE_CLIENT_DIR` names the install explicitly: the folder holding `Bin` and `Data`. The dump lands in the Ambrose data folder, `%LOCALAPPDATA%/ProjectAmbrose` on Windows or `~/.local/share/project-ambrose` otherwise, as `types/<revision>.json`, and is never committed. **Never run KingsIsle's launcher or patcher against that install**: it moves the revision under me and every recorded fact about it.

**5. The servers.** Either start them one at a time, the login server on port 12000 and the game server on 12333, or start `supervisor`, which runs the login, game and patch servers in order, takes back the ones still running if it is restarted, and captures each one's output. `ctest` already proves the apps reach readiness and shut down cleanly against disposable databases, so a failure here is usually configuration rather than code.

**6. The panel.** In `supervisor.conf` set `Panel.Enable = 1`. It listens on `Panel.Port`, 12080, which is deliberately clear of the 12000 the login server takes for game clients on the same machine. `supervisor --panel-self-signed` writes a certificate that signs itself and prints its fingerprint, which is what a panel on my own machine wants. On its first start with no operator, the panel prints a one-time link, once, good for thirty minutes and only from the machine it runs on, that makes the owner account with a name and password of my choosing, so no default password ever exists. If I clear the console before using it, it is in the log rather than gone. The panel serves the built page from `Panel.DashboardDir`, which defaults to a `dashboard` folder beside the executable:

```
npm install
npm run build --workspace apps/dashboard
```

Then point `Panel.DashboardDir` at `apps/dashboard/dist`. For work on the panel's own page there is a development server instead, which proxies its API calls to a running panel:

```
AMBROSE_PANEL_API=https://127.0.0.1:12080 npm run dev --workspace apps/dashboard
```

**7. My own client.** The `launcher` tool starts my own installation against my own login server, always with patching off, from a folder of its own, and never through KingsIsle's launcher: `launcher --client <the install folder>`. It needs Windows. With a login server running, that is the whole loop: my client reaches the login screen, authenticates against my server, and lands on character select.

**What my milestone actually needs.** Ask me for only these. A milestone whose checks are unit tests needs nothing past step 1. Anything that stores something adds step 2. Anything reading the client's own archives, dumps or zones adds step 4 and my installation. A check marked Real client needs steps 5 and 7 and somebody at the keyboard, and a check marked Dev-gated may need a second machine or hardware I do not have, which stays unticked and is named in the pull request. Panel milestones are held by the maintainer's own sessions, so step 6 is for watching the thing run, not for work I may take.

A branch named `milestone/<id>-<short-name>` builds the Linux GCC leg in CI by itself, so an open pull request tells us both whether it compiles there, and the maintainer adds a label for the Windows leg when it is worth one. The first run from a new contributor waits for a maintainer to approve it.

## Running one test rather than all of them

The suite builds into one executable, so a single test is a filter rather than a separate target:

```
cmake --build --preset windows-debug --target unit_tests
./build/windows-msvc-x64/bin/Debug/unit_tests.exe --gtest_filter=MovementPackingTest.*
ctest --preset windows-debug -R MovementPacking
```

Tests that need my installation are a second executable, `client_tests`, and skip unless `AMBROSE_CLIENT_DIR` names the install; ones that need my type dump read `AMBROSE_TYPE_DUMP_PATH`; database ones run only when `AMBROSE_TEST_DB` holds a connection string. A test that skips prints why, and a skipped test is not a passed one, so read the count.

Three build traps on Windows, each of which has cost somebody an hour here:

- Two builds in the same tree fail with `C1041`, cannot open program database. Let one finish, or build in separate trees.
- A link failing with `LNK1104` for no visible reason is usually a build worker from an earlier run still holding the file, not a broken change.
- The first configure builds every dependency from source and takes about an hour. Later ones are fast, so do it before I need it rather than while I wait.

## What has actually gone wrong here, so we do not repeat it

Every one of these came from real pull requests on this track. None of them was carelessness, and each cost a round trip.

**The Linux leg fails on warnings MSVC never mentions.** The build is warnings-as-errors on both compilers, and GCC refuses things MSVC accepts. Three that have already bitten:

- A range loop that binds by value: `for (auto const [tag, expected] : std::array<std::pair<std::string_view, uint8>, 8>{...})` copies each pair, and GCC calls that `-Werror=range-loop-construct`. Bind by reference, `auto const&`.
- An aggregate initialised with fewer members than it has: GCC calls that `-Werror=missing-field-initializers`. Before changing my call site, look at the structure: this has bitten three times here, and each time the real cause was a few members with no default initialiser, so naming any one field could never compile on GCC whatever the caller wrote. Giving those members defaults fixes it for everyone, and is the maintainer's to take if the structure is not mine to change.
- A macro whose expansion contains a top-level comma, used inside another macro. GCC says so plainly. MSVC accepts it and builds code that reads off the end of itself, which surfaces later as a crash in a test that passed for weeks. Braces do not protect commas from the preprocessor, only parentheses do. If Windows crashes where Linux compiles, and anything near the logging macros changed, suspect the macro before the build system.

If I can only build on one platform, say so and let the leg tell us, but expect this class of thing rather than being surprised by it.

**Every acceptance check the work earns gets ticked, in the same pull request.** Seven pull requests in one night ticked nothing at all, which reads as a milestone half-built even when the code is complete, and makes a reviewer guess what was claimed. Three rules fall out of it:

- A check another milestone shares word for word is earned by the same run, so tick every milestone that shares it and say so. One locale round-trip test closed three milestones at once because their last check was the same sentence.
- A check that describes work belonging to a different milestone cannot be earned here. Say which, and why, in the description. Do not tick it and do not leave it silent: 4.04 carries three checks that describe 4.05, and that was a flaw in the roadmap rather than in the contribution.
- A check may already be earned by a test that exists. That is a real finding and worth a pull request of its own: run the test, tick the box, and quote the run.

**The pull request description is not the template.** Arriving with the template's own prompts still in it tells the reviewer nothing, and it is the first thing read. Fill every section: which milestone, what it adds, the platform built on and what `ctest` ended with, which boxes this ticks and what proves each, and which boxes stay empty and why.

**Say when the work deviates from the deliverables.** Putting a file somewhere other than the deliverables list says is sometimes right, and a reviewer will keep it when the reason holds. LocationString landed in `shared/Util` rather than `game/Movement` because the login server needs it too, which was the better call, but it was left to the reviewer to work out whether it was deliberate. One sentence in the description settles it.

**A closed pull request is not a rejected one.** When the work lands, the maintainer often commits it together with the acceptance ticks, the roadmap summary and the regenerated card in one commit, authored to me, and closes the pull request naming that commit. That keeps the repository's own checks green, which a bare merge would not. Look for the commit before assuming anything went wrong.

## What the reviewer will do to your work, so nothing is a surprise

It is reviewed by running, never by reading, and by somebody who assumes the tests might be empty:

- Your branch is built and its whole suite run, then every ticked check is run by name and read for what it actually asserts.
- The central claim is broken on purpose to see whether a test notices. One codec had a flag byte flipped by one bit; all three of its tests failed, which is why it merged.
- If the milestone is about something being faster, smaller or quieter, it is measured against what it replaces, on real input. One cache was correct, well tested, and loaded twice as slowly as the file it replaced.
- If the milestone adds a behaviour, that behaviour is switched off to see whether anything notices. One encoder's whole ordering feature turned out never to change a byte on any input it was tested against.
- Malformed input is fed to anything that parses: truncated, bit-flipped, and random bytes. Each should be refused by name rather than crashing.

None of that is adversarial for its own sake. A test that cannot fail is worse than no test, because it makes the next change look safe.

## Before the pull request

Have me commit first, because these read committed work, then run these from the repository root (`python` may be `py` on Windows):

```
git fetch upstream
curl -s https://justchicoo.github.io/Project-Ambrose/state.json | python -c "import json,sys; print([m for m in json.load(sys.stdin)['milestones'] if m['id']=='<my milestone>'])"
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD --branch <my branch>
python apps/codestyle/codestyle.py
python apps/ci/ci_forbidden_files.py
ctest --preset windows-debug
git status --porcelain
```

That first line is the board again: my milestone should still say `building` with my name on it. If it now says `held`, the maintainer's own sessions have taken that area since I started, and I should stop and ask in the Discord rather than push into it.

Three dots, and the remote branch my pull request targets, never a local `main`, because a stale or moved-on `main` makes that check flag files I never touched. `upstream` is whichever of my remotes is github.com/Justchicoo/Project-Ambrose; a clone of my own fork has none until I add it with `git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git`. `git status` must be clean: an extracted file, a dump or a generated database file left in the tree is the thing rule 2 exists to stop, and several milestones generate exactly those.

Every commit on the branch needs a trailer naming you, such as `Co-Authored-By: <your model name> <noreply@example.com>`; the checker fails any commit in the range without one, not only the last.

Then write the description, replacing the template's prompts rather than leaving them. It needs, in order: the milestone id and title; what the change adds; the platform I built on and the line `ctest` ended with, pasted; which acceptance boxes this ticks and what proves each; which boxes stay empty and why; and any place the work departs from the milestone's deliverables, with the reason. A reviewer reads that before the code, and every question it leaves open is a round trip.

## One milestone per branch, claimed before it is built

```
git fetch upstream
git switch --detach upstream/main
git switch -c milestone/<id>-<short-name>
```

Always from `upstream/main`, never from another branch that has an open pull request, because that turns two independent contributions into a chain where revising the first breaks the second.

**The names have to be exactly these, because machines read them:**

| What | Form | Example |
|---|---|---|
| Branch | `milestone/<id>-<short-name>`, lower case, hyphens | `milestone/16.01-filebinary-table-codec` |
| Pull request title | `<id> <what you are building>` | `16.01 FileBinary table codec` |
| Claim issue, if I use one instead | `Claim: <id> <title>`, from the claim form | `Claim: 16.01 FileBinary table codec` |

The id is exactly as the board writes it, with its leading zero where it has one, so `4.04` and not `4.4`. The branch name is the one that matters: `apps/ci/ci_contrib_paths.py` reads it, and it is the only reason CI accepts a change under `src/`, so a branch named anything else fails every source file at once with a wall of refusals. The board reads it too, which is how the claim appears without anybody being told. The title is for people.

**Open the pull request as a draft on the first day, before the work is done.** That is what reserves the milestone, and nobody has to be told: the board reads the open pull requests, so within minutes it shows my milestone as being built, by me, and stops anybody else taking it. Building for a week in silence risks somebody else landing it first. Title it `<id> <what you are building>`.

A claim is not forever. Fourteen days with no push and the board puts the milestone back on the open list, with whatever I pushed left in place, so somebody else can carry it. A push is all it takes to keep it.

Check before opening it: `git log --oneline upstream/main..HEAD` shows only this milestone's commits, and `git diff --name-only upstream/main...HEAD` only its files.

After a merge the branch holds nothing git can apply again, because pull requests are squashed: `git checkout main`, `git reset --hard upstream/main`, `git push --force origin main`, and branch again from there.

## Keep me honest

If I tell you something I only remember or assume, mark it unproven rather than writing it as a fact. If a test fails, tell me what actually happened rather than adjusting the test until it passes, and never weaken or skip an existing test to get a green run: that is the one change nobody here will merge. If the milestone turns out to need something that does not exist yet, say so and we tell the maintainer rather than building it too. Questions go to the project's Discord, https://discord.gg/Dx6ACDUj6N, or into the pull request itself.

## Now ask me

1. Which milestone id am I taking, and is it in doc/MILESTONE-TRACK.md's "Open now" table?
2. What do I have already: Windows or Linux, a working build, a MySQL or MariaDB, my own client installation, a type dump? Anything missing that my milestone needs, walk me through the matching step of the setup section before we plan the work, and skip the steps it does not need.
3. If I am unsure which to take, recommend one that fits what I have, and say which of its acceptance checks I will not be able to run.
````

## After you paste it

Answer its questions, then read `doc/MILESTONE-TRACK.md` yourself before it starts. Two things there decide whether the evening is wasted: the milestone has to be in the "Open now" table, and the branch has to be named `milestone/<id>-<short-name>`, because that name is the only reason CI lets the change touch `src/`.

Open the pull request as a draft on the first day. It is what holds the milestone for you.

If what you want to build is not on the roadmap at all, or the milestone you want is reserved, ask in the Discord rather than sending it: https://discord.gg/Dx6ACDUj6N. The other door, contrib/AI-START-HERE.md, is open for tools, findings, guides, fixtures and proposals, and nothing there can collide with a milestone.

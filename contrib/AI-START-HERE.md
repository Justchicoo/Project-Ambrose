<!-- Project Ambrose by Imjustchico: The prompt an outside contributor pastes into their own AI assistant to take an item from the contributor track, with everything that assistant needs to know about this repository. -->

# Start here, with your AI

Everything inside the fence below is meant to be copied whole into any AI assistant, in one gesture. It is written as you speaking to that assistant. Paste it, answer its first question, and it has what it needs to work here without guessing. The short section after the fence is for you.

````
I am contributing to Project Ambrose, a Wizard101 server written from scratch in C++20 (github.com/Justchicoo/Project-Ambrose, MIT licensed). I work only on its contributor track, which is kept separate from the maintainer's roadmap so our work can never collide. Help me finish one item from that track, to the standard below.

**What I get for it.** A finding is cited by the milestone that proves it. A tool stays mine in `contrib/tools/`. Nothing is reserved, so two people may take the same item and both are read. A finding that turns out false still merges, because it stops the next person chasing it.

## Read these before you plan anything

- `doc/CONTRIBUTOR-TRACK.md` - the track, the folders it may touch, the open items and the merged ones.
- `contrib/findings/README.md` - the shape of a finding, with a worked example.
- `CONTRIBUTING.md` - the house rules for every change.
- Then only what my item needs: `doc/TOOLS.md` (what each tool is, and what is only planned), `doc/CAPTURE.md` (how a capture is recorded and what is still open in it), `doc/CLIENT.md`, `doc/guides/logging.md` (how to read the server's own output), `doc/config/loginserver.md` (pointing a server at my install and database), and the phase files for the area I am working in.
- `README.md`'s status and `doc/ROADMAP.md`'s "Where we are" say what actually runs today. `doc/ARCHITECTURE.md` is long and describes code I may not touch: read it only for the file-header table and the SQL update convention.

## The state of the project, so you assume neither more nor less than is true

- Phases 1 to 3 are built. A real client, started by Ambrose's own launcher, reaches character select and stops. The handshake, authentication, a wrong password and a retry, the character list and the shutdown notice are all that can be watched against Ambrose today. No world, quest, pet, housing or combat traffic exists yet, on either side.
- The pinned client revision is r806919 (Wizard 1.610). A finding names the revision it was found on; read mine from `Bin/revision.dat` in my own install rather than copying an example.
- The type dump `src/tools/typeextract` writes is version 2: an object with `version`, `revision`, `executable_sha256`, `extractor`, and `classes`, itself an object keyed by class name, each class holding `name`, `bases`, `hash` and `properties`. A tool that reads a dump should refuse a version it does not know rather than report nothing.
- A running server logs each message it refused or did not handle under the `network.opcode` category, but only up to `Network.DroppedMessageBurst = 64` in a session and `Network.DroppedMessagesPerSecond = 16`. Past that it stops logging and adds a strike instead, a refused message strikes every time, and `Network.MaxStrikes = 10` closes the connection. Probing with unknown messages runs out, and a quiet log is not proof of quiet traffic.
- Setup is already automatic, which a proposal about setup has to start from: a first start finds the client installation, builds its type dump and extracts the name tables without asking, a server with no local configuration exits naming the full path it wanted and the `.conf.dist` to copy, and an installer is a scheduled milestone. The direction is to remove steps, so anything that documents a manual sequence should say why automation cannot cover it.
- The console writes fixed columns on a terminal: a millisecond time, the level word padded to five, the category in a column of its own, and the message from a fixed column, with only the level word and the values inside a message carrying colour. Redirected output keeps the plain older form, full date and no escape bytes, so anything parsing a log file is unaffected.

## What already exists, so I do not start from nothing

- Built server tools in `src/tools/`: `typeextract` (builds my own client's type dump by emulating its program, without launching the game; takes `--client` and `--out`, and finds the install from `AMBROSE_CLIENT_DIR` or the newest revision), `bindecode` (prints the named BINd entries of one KIWAD archive of my own install as JSON; also `--list <pattern>` and `--sweep`; it reads an archive, not a capture, and has no `--blob` flag and no standard input), `localetool` (the `.lang` text of my own Root.wad), `extractor` (name tables into a world database), `dbimport` (creates the databases a server needs), `launcher` (starts my own client against an Ambrose login server without ever running KingsIsle's launcher).
- Nothing in the repository reads zones yet. `src/tools/wad_extractor` and `src/tools/zone_extractor` are empty folders that doc/TOOLS.md names as future work, so an item about zone contents starts from no tool at all.
- Seven contributor items are merged already, four of them tools and guides you can read as worked examples, three of them proposals that settle shapes other items build against. Read them before building anything similar, both to avoid redoing them and to see the standard: `contrib/tools/ambrose-message-watcher` (reports the messages a server refused or did not handle, from a log or by following one), `contrib/tools/ambrose-install-diff` (the archive, zone and locale files that differ between two installations), `contrib/tools/ambrose-type-diff` (the metadata, classes and properties that changed between two type dumps), `doc/guides/logging.md` (reading Ambrose's own logs) and `doc/guides/linux.md` (building and running on Linux, walked on Ubuntu 24.04). The merged proposals are in `contrib/proposals/`.
- `apps/clientdriver` drives my own client through a scenario with nobody at the keyboard. `python apps/clientdriver/drive.py check` says in one line whether my machine can run it and exits 77 when it cannot; have me run that before planning anything that needs it. It requires Windows, `AMBROSE_CLIENT_DIR` or `--client`, the built server and launcher, a reachable MySQL or MariaDB, `tshark` with an Npcap loopback adapter, the pinned packages in `apps/clientdriver/requirements.txt`, and reference crops rebuilt by `drive.py capture-refs` at my own revision, window size and interface scale, because crops are pictures of the client and are never committed.
- A scenario may name only screens already in `apps/clientdriver/references.json`: `login`, `invalid`, `charselect`, `dialog_next`, `dialog_back`, `test_book`, `school_list`, `school_chosen`, `appearance`, `name`. That file is outside the track, so a scenario needing a new screen has to be a proposal instead.

## What it costs to run any of it

There are no prebuilt binaries. Everything above needs CMake 3.25+, vcpkg with `VCPKG_ROOT` set, and Visual Studio 2022+ or GCC 13+: `cmake --preset windows-msvc-x64` then `cmake --build --preset windows-debug`, or `linux-gcc` and `linux-gcc-debug`. The first configure builds every dependency from source and takes about an hour. A login server also needs a MySQL or MariaDB it can reach - the default is `127.0.0.1;3306;ambrose;ambrose;ambrose_login`, and an unreachable one stops startup - and `dbimport` creates the databases. A tool of my own in `contrib/tools/` needs none of that: it stands alone with its own build file or script, and pins its own dependencies inside its own folder rather than touching the repository's manifests.

## Match the item to what I have before recommending one

- My own install and nothing built: the client, data and revision findings, and the tools that read an installation.
- Nothing but thought: the proposals.
- A full C++ build: anything that runs the servers or their tools.
- A build and a running login server: the protocol findings and anything that watches or generates traffic, with `tshark` on top for anything that captures.
- A build, MySQL, Windows, an install and reference crops: a client driver scenario.

Check the item's own row in doc/CONTRIBUTOR-TRACK.md for where it lands, and tell me plainly if it needs something I do not have.

## Hard rules. Breaking one closes the pull request, and each is there for a reason

1. **Paths.** I may add or edit only under these nine prefixes, which is exactly what `apps/ci/ci_contrib_paths.py` enforces: `contrib/tools/`, `contrib/findings/`, `contrib/notes/`, `contrib/proposals/`, `contrib/locale/`, `apps/clientdriver/scenarios/`, `data/sql/updates/pending_db_world/`, `data/fuzz/`, `doc/guides/`. Everything else belongs to a milestone being built right now, including `contrib/README.md` and this file.
2. **No game data in the repository, ever.** That rule is why this repository can exist in public. No file from the client, no extracted asset, no run of hex or base64 pasted from a capture. `apps/ci/ci_forbidden_files.py` refuses `.wad`, `.nif`, `.kf`, `.kfm`, `.pcap` and `.pcapng` by extension, any file beginning `KIWAD` or `BINd`, any JSON holding both `classes` and `version`, client protocol XML, and anything over 1,000,000 bytes. Offsets, field names, sizes, counts and hashes are facts about the data and are welcome; the bytes are not. A tool may read my own installation at run time - that is the line.
3. **Clean room.** Nothing copied from another Wizard101 server, emulator, wiki or site. Behaviour may be studied; code and text are written from scratch. A public source is named with its licence and waits for the maintainer to accept it.
4. **House style, where it applies.** Every Markdown, SQL, Python, shell, CMake, YAML, `.conf.dist` and C++ file I add starts with the branding header and a one-line brief of what it holds, and carries no other comment anywhere. The forms are exactly: Markdown, one HTML comment on line 1 reading `Project Ambrose by Imjustchico: <brief>`; C and C++, a block comment whose lines are ` * Project Ambrose by Imjustchico` then ` * <brief>`; CMake, shell, PowerShell, Python, YAML and conf, `# Project Ambrose by Imjustchico` then `# <brief>`; SQL, the same with `--`. The handle is the maintainer's and never changes to mine. JSON and binary files are exempt and must carry no header and no "comment" key: a finding starts with `{`. Files are UTF-8 with no byte order mark, LF endings, no trailing whitespace, ending in a newline, and ASCII except where the content itself is a translation. `python apps/codestyle/codestyle.py` is the judge.
5. **One thing per pull request**, so it can be reviewed on its own.
6. **Everything asserted says how it was checked and what would prove it wrong.** A claim nothing could falsify is an opinion, and one wrong fact costs more later than it saved.
7. **Never** run KingsIsle's launcher or patcher against an install I want kept at a fixed revision, and never capture or contact KingsIsle's own servers. Everything here is done against my own installation and my own server.

## The shape of a finding, exactly

One JSON file at `contrib/findings/<area>/<subject>.json`, where the folder matches the `area` field. `python apps/ci/ci_findings.py` enforces all of this:

- `subject` - what the claim is about.
- `area` - one of `protocol`, `objects`, `world`, `quests`, `combat`, `client`, `data`, `patching`.
- `claim` - one statement that could be shown false, at least 20 characters.
- `revision` - the client it was found on, matching `r<digits>.<name>`, such as `r806919.Wizard_1_610`.
- `method` - `capture` (my own session against my own server), `observation` (what my own client does on screen), `static` (reading my own copy of a file or program with my own tools), `experiment` (I changed something and watched the result), or `reasoning` (derived from other findings, which I name).
- `how_to_repeat` - a list of steps, each a sentence, naming the tool, the screen or the capture. Not "I remember seeing".
- `evidence` - a list of what was actually observed: frame numbers, byte counts, field names, offsets, log lines, screen behaviour. Never pasted bytes.
- `disproof` - what would show the claim false, at least 20 characters.
- `confidence` - `low`, `medium` or `high`.
- `submitted_by`, `submitted_on` (`YYYY-MM-DD`), and `status`, which is `claimed` for anything new.

A merged finding stays `claimed` and nothing is built on it until Ambrose re-derives it with its own tools. Extra keys are allowed for anything that would help a later proof.

## How I want you to work

1. Ask which item id I am taking and what I already have: a client installation, a packet capture, a running Ambrose server, a build, or nothing yet. If I am unsure, recommend one that fits what I have and takes an evening.
2. **Before writing that Ambrose lacks anything, look for whether it has it.** This has been the single most common fault in contributions so far: two proposals in a row set out to add something the project had already built. Search `doc/ROADMAP.md`'s "Where we are" paragraph, which says what is done in one pass, then the phase file for the area, then grep the tests. A guard usually exists as a CTest entry: `src/test/apps/AppSmokeTest.cmake` alone already proves the servers reach readiness and shut down cleanly with disposable databases. The cheapest disproof of "nothing proves X" is to find the test that proves X, and it takes five minutes.
3. Turn it into a plan before any writing: what exactly to observe or build, what evidence would prove the claim, and what would disprove it. Put the cheapest experiment that could kill the idea first, so I do not spend a week on something wrong.
4. Walk me through it one step at a time, waiting for what I actually see rather than assuming the result. When I paste output, read it rather than agreeing with it.
5. Prefer the smallest thing that answers the question. A tool that reads only what it needs beats one that reads everything: classify before you hash, compare cheap fields before expensive ones, and never read a whole installation where a size comparison would do.
6. Make a tool fail usefully. Name the file and the reason, exit non-zero, and carry on past what can be skipped rather than losing a whole report to one unreadable file. Say in its README what would make its output wrong.
7. Write the files in the required shape, then have me run every check below and fix whatever they print.
8. Write the pull request description: the item id, what the change is, how it was verified, and what would disprove it.

## Before the pull request

Have me commit first, because these read committed work, then run these from the repository root (`python` may be `py` on Windows):

```
git fetch upstream
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD
python apps/ci/ci_findings.py
python apps/codestyle/codestyle.py
python apps/ci/ci_forbidden_files.py
```

Three dots, and the remote branch my pull request targets - never a local `main`, because a stale or moved-on `main` makes that check flag dozens of files I never touched. `upstream` is whichever of my remotes is github.com/Justchicoo/Project-Ambrose; a clone of my own fork has no such remote until I add it with `git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git`. Before I commit, `python apps/ci/ci_contrib_paths.py --paths <files>` works on uncommitted ones. If I touched a scenario, also run `python apps/clientdriver/tests/test_clientdriver.py`.

Every commit on the branch needs a trailer naming you, such as `Co-Authored-By: <your model name> <noreply@example.com>`; the checker fails any commit in the range without one, not only the last.

CI runs the path check on every pull request from a fork, and a maintainer approves the first run from a new contributor. Green checks do not mean my change compiles: the build legs run only when a maintainer adds a `ci:` label.

## One branch per item, and never wait for a review

Every item gets its own branch, taken from `upstream/main` and from nothing else:

```
git fetch upstream
git switch --detach upstream/main
git switch -c contrib/<item-id>-<short-name>
```

**Never branch from a branch that has an open pull request.** That is the mistake that turns a queue of independent contributions into a chain: the second pull request then contains the first one's changes, a review that revises the first breaks the second, and both have to be rebuilt. Branching from `upstream/main` every time keeps each one reviewable and mergeable on its own, in any order.

Check it before opening the pull request. `git log --oneline upstream/main..HEAD` must show only this item's commits, and `git diff --name-only upstream/main...HEAD` only this item's files. If either shows another item's work, the branch was taken from the wrong place.

**Do not wait for a review.** The moment the pull request is open, start the next item: fetch `upstream`, branch from `upstream/main` again, and carry on. Several open pull requests at once is the intended way to work here, because they are reviewed together rather than one at a time, and waiting on each one in turn is the slowest possible path through the list. Tell me which item is next and keep going.

When a review does ask for a change, switch back to that item's branch, make the change, and push it. Nothing else is affected, because no other branch was built on it.

Two things make an item worth doing in sequence rather than in parallel: it edits a file another open pull request of mine already edits, which on this track is rare because items land in different folders, or it depends on a shape another item is still settling. Say so if you spot either, and I will hold it back.

A merged pull request is squashed into one commit, so its branch holds nothing git can apply again and reusing it opens an empty pull request. After a merge: `git checkout main`, `git reset --hard upstream/main`, `git push --force origin main`, and branch again from there.

## Keep me honest

If I tell you something I only remember or assume, mark it unproven instead of writing it as a claim. If the evidence does not support the claim, say so and we submit a refuted finding - that still helps, because it stops the next person chasing it. If a step fails, tell me what actually happened rather than moving on. Questions go to the project's Discord, https://discord.gg/Dx6ACDUj6N, or into the pull request itself.
````

## After you paste it

Answer the first question with the item id and what you have, or say you are unsure and let it pick. Everything it writes lands in the folders above and nowhere else; if it proposes a change under `src/`, a phase file, CMake or CI, it has lost the thread, and the path check will say so.

Open the pull request against `main` from your own fork, say in the description which item you took, and say how you checked it. A finding that turns out to be wrong is still worth sending.

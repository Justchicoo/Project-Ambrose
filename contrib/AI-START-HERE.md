<!-- Project Ambrose by Imjustchico: The prompt an outside contributor pastes into their own AI assistant to take an item from the contributor track, with what that assistant needs to know about this repository. -->

# Start here, with your AI

Everything inside the fence below is meant to be copied whole into any AI assistant, in one gesture. It is written as you speaking to that assistant. Paste it, answer its first question, and it has what it needs to work here without guessing. The short section after the fence is for you.

````
I'm contributing to Project Ambrose, a Wizard101 server written from scratch in C++20 (github.com/Justchicoo/Project-Ambrose, MIT licensed). I work only on its contributor track, which is kept separate from the maintainer's roadmap so our work can never collide. Help me finish one item from that track.

**What I get for it.** A finding is cited by the milestone that proves it. A tool stays mine in `contrib/tools/`. Nothing is reserved, so two people may take the same item and both are read. A finding that turns out false still merges, because it stops the next person chasing it.

**Read these in the repository and follow them over anything you assume.** If you cannot see the repository, ask me to paste them.

- `doc/CONTRIBUTOR-TRACK.md` - the track, the folders it may touch, and the numbered list of open items.
- `contrib/findings/README.md` - the shape of a finding: one proven claim about how the game behaves.
- `CONTRIBUTING.md` - the house rules for every change.
- Then only what my item needs: `doc/TOOLS.md` (what each tool is, and what is only planned), `doc/CAPTURE.md` (how a capture is recorded and what is still open in it), `doc/CLIENT.md`, `doc/config/loginserver.md` (pointing a server at my install and database), `doc/roadmap/phase-04.md`, `-06`, `-09` and `-10` (the zone formats F-06 is about). `doc/ARCHITECTURE.md` is 555 lines about code I may not touch: read it only for the file-header table and the SQL update convention.
- `README.md`'s Status line says how far a real client gets today. `doc/ROADMAP.md`'s "Where we are" says which milestones that is, in detail.

**What already exists, so I do not start from nothing.**

- Built tools in `src/tools/`: `typeextract` (builds my own client's type dump by emulating its program, without launching the game), `bindecode` (prints the named BINd entries of one KIWAD archive of my own install as JSON; also `--list <pattern>` and `--sweep`), `localetool` (the `.lang` text of my own Root.wad), `extractor` (name tables into a world database), `dbimport` (creates the databases a server needs), `launcher` (starts my own client against an Ambrose login server without ever running KingsIsle's launcher).
- `bindecode` reads an archive, not a capture: it has no `--blob` flag and no standard input. A captured payload is read against the type dump `typeextract` builds, which is what the worked example in `contrib/findings/README.md` does. `contrib/findings/protocol/example-character-info-hair-colour.json` is the shape to copy.
- Nothing in the repository reads zones yet. `src/tools/wad_extractor` and `src/tools/zone_extractor` are empty folders that doc/TOOLS.md names as future work, so F-06 starts from no tool at all.
- `apps/clientdriver` drives my own client through a scenario with nobody at the keyboard. `python apps/clientdriver/drive.py check` says in one line whether my machine can run it and exits 77 when it cannot; have me run that before planning anything that needs it. It requires Windows, `AMBROSE_CLIENT_DIR` or `--client`, the built server and launcher, a reachable MySQL or MariaDB, `tshark` with an Npcap loopback adapter, the pinned packages in `apps/clientdriver/requirements.txt`, and reference crops rebuilt by `drive.py capture-refs` at my own revision, window size and interface scale, because crops are pictures of the client and are never committed.
- A scenario may name only screens already in `apps/clientdriver/references.json`: `login`, `invalid`, `charselect`, `dialog_next`, `dialog_back`, `test_book`, `school_list`, `school_chosen`, `appearance`, `name`. That file is outside the track, so a scenario needing a new screen has to be a proposal instead.
- A running Ambrose prints each message it refused or did not handle, but only up to `Network.DroppedMessageBurst = 64` per session and `Network.DroppedMessagesPerSecond = 16`. Past that it stops logging and adds a strike instead, a refused message strikes every time, and `Network.MaxStrikes = 10` closes the connection. Probing with unknown messages runs out.
- Four items are already merged and are not to be redone, and each is worth reading before building anything like it: `contrib/tools/ambrose-message-watcher` reports every message a running server refused or did not handle, `contrib/tools/ambrose-install-diff` reports the archive, zone and locale files that differ between two installations, `contrib/tools/ambrose-type-diff` reports what changed between two type dumps, and `doc/guides/logging.md` explains how to read Ambrose's own logs, console columns and all.
- The console writes a fixed layout on a terminal: a millisecond time, the level word padded to five, the category in a column of its own, and the message from a fixed column, with only the level word and the values inside a message carrying color. Redirected output is the old plain form, full date and no escape bytes, so anything parsing a log file is unaffected. doc/guides/logging.md is the guide to it.
- Only phases 1 to 3 are built. A real client reaches character select and stops, so the handshake, authentication, the character list and the shutdown notice are all that can be watched against Ambrose today. World, quest, pet, housing and combat traffic never appears yet.

**What it costs to run any of that.** There are no prebuilt binaries. Everything above needs CMake 3.25+, vcpkg with `VCPKG_ROOT` set, and Visual Studio 2022+ or GCC 13+: `cmake --preset windows-msvc-x64` then `cmake --build --preset windows-debug`, or `linux-gcc` and `linux-gcc-debug`. The first configure builds every dependency from source and takes about an hour. A login server also needs a MySQL or MariaDB it can reach - the default is `127.0.0.1;3306;ambrose;ambrose;ambrose_login`, and an unreachable one stops startup - and `dbimport` creates the databases.

**So match the item to what I have before recommending one.**

- My own install and nothing built: F-12, F-13, F-15, C-04, C-07, C-08, C-16, and F-06 if I write the reader myself. F-01 to F-05, F-07 and F-16 also land here today, as observation of my own client, because Ambrose cannot show that behaviour yet.
- Nothing but thought: C-18, C-19, C-20.
- A full C++ build: F-11, C-05, C-12, C-14, C-15.
- A build and a running login server: F-08, F-09, F-10, C-02, C-06, C-10, C-11, with `tshark` on top for anything that captures. C-09 needs a database but no client; C-13 needs a client, a server and Wine or Proton.
- A build, MySQL, Windows, an install and reference crops: C-03.

Good first evenings: F-13, F-12, or one of the proposals.

**Hard rules. Breaking one closes the pull request, and each one is there for a reason.**

1. I may add or edit only under `contrib/`, `apps/clientdriver/scenarios/`, `data/sql/updates/pending_db_world/`, `data/fuzz/` or `doc/guides/`, and inside `contrib/` only the folders doc/CONTRIBUTOR-TRACK.md's table names: `tools/`, `findings/`, `notes/`, `proposals/`, `locale/`. Everything else belongs to a milestone being built right now. The path checker tests exactly those five prefixes, so an invented `contrib/whatever/` fails the check, and so do `contrib/README.md` and this file, which are the track's own signposts.
2. No game data in the repository, ever - that rule is why this repository can exist in public. No file from the client, no extracted asset, no run of hex or base64 pasted from a capture. Offsets, field names, sizes, counts and hashes are facts about the data and are welcome; the bytes are not. A tool may read my own installation at run time - that is the line.
3. Nothing copied from another Wizard101 server, emulator, wiki or site, so the project stays clean-room. Behaviour may be studied; code and text are written from scratch. A public source is named with its licence and waits for the maintainer to accept it.
4. House style, where it applies. Every Markdown, SQL, Python, shell, CMake, YAML, `.conf.dist` and C++ file I add starts with the branding header and a one-line brief of what it holds, and carries no other comment anywhere. For Markdown that is exactly `<!-- Project Ambrose by Imjustchico: <brief> -->` as line 1; the handle is the maintainer's and never changes to mine; doc/ARCHITECTURE.md gives the form for every other type. JSON and binary files are exempt and must carry no header and no "comment" key: a finding starts with `{`. Files are UTF-8 with no byte order mark, LF endings, no trailing whitespace, ending in a newline, and ASCII except where the content itself is a translation.
5. One thing per pull request, so it can be reviewed on its own.
6. Everything asserted says how it was checked and what would prove it wrong. A claim nothing could falsify is an opinion, and one wrong fact costs more later than it saved.

**Never** run KingsIsle's launcher or patcher against an install I want kept at a fixed revision, and never capture or contact KingsIsle's own servers: doc/ARCHITECTURE.md says plainly that this may break their terms and put my account at risk. A `capture` finding is my own client against my own Ambrose server on loopback.

**What I want from you, in this order.**

1. Ask which item id I am taking - F- is a finding, C- is code, data, a guide or a proposal, except C-17, which lands in `contrib/findings/protocol/` and is written as findings - and what I actually have from the list above. If I am unsure, recommend one that fits and takes an evening, and tell me what it will cost me before I start.
2. Turn it into a short plan: exactly what to observe or build, what evidence would prove the claim, and what would disprove it. Put the cheapest experiment that could kill the idea first, so I do not spend a week on something wrong.
3. Walk me through it one step at a time, waiting for what I actually see rather than assuming the result.
4. Write the files in the required shape.
   - A finding is one JSON file at `contrib/findings/<area>/<subject>.json`, in the folder that matches its `area`, holding at least these keys: `subject`, `area` (`protocol`, `objects`, `world`, `quests`, `combat`, `client`, `data` or `patching`), `claim`, `revision`, `method` (`capture`, `observation`, `static`, `experiment` or `reasoning`), `how_to_repeat` (a non-empty list of sentences), `evidence` (a non-empty list), `disproof`, `confidence` (`low`, `medium` or `high`), `submitted_by`, `submitted_on` (`YYYY-MM-DD`) and `status`: `"claimed"`. `claim` and `disproof` must each say at least 20 characters' worth. `revision` is the text in `Bin/revision.dat` of MY OWN install, such as `r806919.Wizard_1_610` - ask me for it and never copy the example's. The checker also fails any finding holding a run of 120 base64 characters or 48 hex byte pairs, so evidence describes the bytes instead of carrying them.
   - Anything else goes where the track's table says, one file, with its header.
   - If my item needs a table, a screen or a convention that does not exist yet, do not invent it. Write a proposal in `contrib/proposals/` or ask in the pull request. SQL goes in `data/sql/updates/pending_db_world/`, whose README gives the naming and the door table's starter shape, and follows doc/ARCHITECTURE.md's dated `YYYY_MM_DD_NN.sql` naming.
5. Have me commit first, because these read committed work, then run these from the repository root and fix whatever they print (`python` may be `py` on Windows):
   git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git
   git fetch upstream
   python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD
   python apps/ci/ci_findings.py --paths contrib/findings/<area>/<file>.json
   python apps/codestyle/codestyle.py
   python apps/ci/ci_forbidden_files.py
   Three dots, and the remote branch my pull request targets - never a local `main`, because a stale or moved-on `main` makes that check flag dozens of files I never touched. A clone of my own fork has no `upstream` remote until the first line above adds it, which is why that line is there; run it once and the fetch alone after that. Before I commit, `python apps/ci/ci_contrib_paths.py --paths <files>` works on uncommitted ones.
   `ci_forbidden_files.py` refuses `.pcap`, `.pcapng`, `.wad`, `.nif`, `.kf`, `.kfm`, any file beginning `KIWAD` or `BINd`, any `.json` holding both `classes` and `version`, any `.conf`, client protocol XML, and anything over 1,000,000 bytes. C-11 fuzz seeds must respect all of that.
   If I touched a scenario, also `python apps/clientdriver/tests/test_clientdriver.py`.
   CONTRIBUTING.md asks for a build and `ctest` before every push. If my change is data or documents only, say so in the pull request and say these checks were run instead.
6. Write the pull request description: the item id, what the change is, how it was verified, what would disprove it, and that I ran the checks above locally. Every commit on the branch needs a trailer naming you, such as `Co-Authored-By: <your model name> <noreply@example.com>` - the checker fails any commit in the range without one, not only the last. CI runs the path check on every pull request from a fork, so it runs on mine without a label; the `contrib` label only forces it on a branch inside the repository, which I cannot add anyway. A `ci:` label is what makes CI compile anything, so green checks do not mean my change builds. Questions go to the project's Discord, https://discord.gg/Dx6ACDUj6N, or into the pull request itself.

**Keep me honest.** If I tell you something I only remember or assume, mark it unproven instead of writing it as a claim. If the evidence does not support the claim, say so and we submit a refuted finding - that still helps, because it stops the next person chasing it.
````

## After you paste it

Answer the first question with the item id and what you have, or say you are unsure and let it pick. Everything it writes lands in the folders above and nowhere else; if it proposes a change under `src/`, a phase file, CMake or CI, it has lost the thread, and the path check will say so.

Open the pull request against `main` from your own fork, say in the description which item you took, and say how you checked it. A finding that turns out to be wrong is still worth sending.

Start each item on a branch of its own, taken from an up-to-date `upstream/main`. A merged pull request is squashed into one commit, so a branch that has been merged holds nothing git can apply again: reusing it produces an empty pull request. After a merge, reset your `main` to `upstream/main` and branch again.

CI needs a maintainer to approve the first run from a new contributor, and the `contrib` label is added for you. Neither is anything to chase.

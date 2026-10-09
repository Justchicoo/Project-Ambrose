<!-- Project Ambrose by Imjustchico: The separate track other people work from: what it holds, the folders it may touch, and the open items. -->

# Contributor track

Every milestone of the phases in doc/ROADMAP.md is open to anyone on the second track, doc/MILESTONE-TRACK.md, in any order and without asking.

This track is the other door, and the safe one. This track holds work that helps the project finish sooner and cannot collide with a milestone: it adds files in folders no milestone builds in, it needs no change to a phase file, and it can be reviewed on its own.

Ask in the Discord before you start if anything here is unclear: https://discord.gg/Dx6ACDUj6N. It is also where a finding gets discussed before it is written up.

Read this document first, then contrib/findings/README.md and CONTRIBUTING.md. Working with an AI assistant is expected here: contrib/AI-MILESTONES-HERE.md is the one prompt to paste into yours, for a milestone or an item here, and it carries what that assistant needs to know about this repository before it writes anything.

## The rule that makes it safe

A change on this track touches only these paths:

| Path | What goes there |
|---|---|
| `contrib/tools/<name>/` | A self-contained tool of your own, in any language, that reads a user's own installation or an Ambrose server and prints or writes its own output |
| `contrib/findings/` | One proven claim per file about how the game behaves, in the shape contrib/findings/README.md gives, which Ambrose later verifies or refutes |
| `contrib/notes/` | Longer research writing that is not one claim: a survey, a walkthrough of a system, a summary of what you tried |
| `contrib/proposals/` | A proposal for something in the phases, as a document. The maintainer folds an accepted one into the roadmap; you never edit a phase file yourself |
| `apps/clientdriver/scenarios/` | Scenarios for the client driver, which are data, not code |
| `data/sql/updates/pending_db_world/` | World rows you authored yourself, such as a table of door destinations, as a pending update file named `rev_<unix seconds>_<short-name>.sql`, which is renamed into `data/sql/updates/db_world/` when it is merged |
| `data/fuzz/` | Seed inputs for the fuzzers that already exist |
| `doc/guides/` | Guides: running on a distribution, a graphics card, a language, a setup that needed a workaround |
| `contrib/locale/` | Translations of Ambrose's own text, never the game's |
| `contrib/fixtures/` | Sample data in the shapes the panel and the servers exchange, hand-written or generated and never captured: status responses, app lists, log records, metric series |
| `contrib/schemas/` | JSON Schemas for those shapes, and the corpora with expected answers a loader or parser is tested against |

Nothing else. Not `src/`, not `apps/` beyond the scenarios folder, not `doc/` beyond guides, not the phase files, not doc/ARCHITECTURE.md, doc/DESIGN.md, doc/PANEL.md, doc/UI-STACK.md, doc/ROADMAP.md, CMake files, CI files or the vcpkg manifest. A pull request that touches anything else is closed with a pointer to this document, because it cannot be merged without stopping a milestone in flight.

`python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD` says whether a change stays inside the track. Three dots, and the branch on the remote your pull request targets. A clone of your own fork has no `upstream` remote, so add it once with `git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git` and run `git fetch upstream` before the check, or it fails with `unknown revision`. Never your own `main`: a local `main` that is stale, or an upstream one that moved on while you worked, makes the check flag dozens of files you never touched. Before your first commit, `python apps/ci/ci_contrib_paths.py --paths <files>` checks files that are not committed yet. `python apps/ci/ci_findings.py` checks that every finding carries what Ambrose needs to prove it. CI runs the findings check on every pull request, and the path check on every pull request from a fork; on a branch in this repository the `contrib` label turns it on from the next push, because only a `ci:` label makes the labelling itself start a run.

## What every change needs

- **One thing at a time.** One tool, one note, one scenario, one guide. A pull request that does two things gets split.
- **Say where it came from.** Your own observation of your own installation, your own capture of your own session, a public source you name, or your own reasoning. Never a file from the game client, never code from another server project, and never text from a wiki or a site without naming its licence and waiting for the maintainer to accept it.
- **Say how it was checked.** A tool says what it was run against and what it printed. A note says how it was observed and what would disprove it. A scenario says it ran and what it asserted. SQL says the query that shows the rows are right.
- **No game files, ever.** No archive, asset, text, image or dump from the client, and nothing generated from one that carries its content. A tool reads the user's own installation at run time; that is the line.
- **Keep the house style.** Every file starts with the Project Ambrose branding header and a one-line brief, and carries no other comments. UTF-8 with no byte order mark, LF endings, no trailing whitespace, ending in a newline, which is what `python apps/codestyle/codestyle.py` enforces and it must pass. Text outside ASCII is fine where the content needs it, such as a translation.
- **Name your tool in the commit.** AI tools are expected here; add the trailer CONTRIBUTING.md asks for.
- **A tool brings its own dependencies.** Pin them inside `contrib/tools/<name>/`, and do not add anything to the repository's own manifests.

## The biggest way to help: proven findings

Ambrose is built on data it has proven. The type registry came from running the client's own program; the name tables came from the install's own files; the wire format was checked byte for byte against captures. Nothing is believed because it sounds right, because one wrong fact costs more later than it saved.

Most of what the game does is still undocumented here, and that is the work that shortens this project the most. A finding is one claim about how the game behaves, written so somebody else can repeat it and so Ambrose can prove or refute it later with its own tools. `contrib/findings/README.md` gives the shape, the fields and the rules; `python apps/ci/ci_findings.py` checks a file before you send it.

Three things make a finding worth merging:

- **It could be wrong.** Say what would disprove it. A claim nothing could falsify is an opinion.
- **Somebody else can repeat it.** The steps name the tool, the screen or the capture, not "I remember seeing".
- **It carries no game data.** Offsets, field names, sizes, counts and hashes are facts about the data and are welcome. The bytes themselves never are.

A merged finding is marked `claimed` and nothing is built on it. When a milestone needs it, Ambrose re-derives it with its own capture, its own decode or a test written to fail if the claim is wrong, and the file records the outcome as `verified` with the milestone that proved it, or `refuted` with what actually happens. Both outcomes are worth merging: a refuted finding stops the next person chasing it.

A refuted finding is as welcome as a verified one and is merged the same way. What is not welcome is a claim nothing could disprove, because it costs the next person the time it saved you. A finding that carries a check a machine can run is worth more again, since it is proven the next time the suite runs rather than the next time somebody has an afternoon; what that check looks like is not settled yet, which is what C-22 proposes and C-51 builds, so until then a finding says in words what would prove or disprove it.

## The open items

Each item is worth doing, needs nothing from the phases, and lands inside the track. Take one by opening a pull request; say in it which item you took. The F items are findings, which follow contrib/findings/README.md; the C items are code, data, guides and proposals. None of them is reserved: if two people send the same item, both are read and the better evidence wins, so say in your pull request what you are starting.

Start with the third list. Every row in it names the milestone it takes work off.

### The third list: work a milestone would otherwise do itself

Each row names the milestone it shortens and asks for something that milestone would otherwise have to do itself, so a merged item is time taken off the roadmap rather than work beside it. They come in three kinds:

- **Scenarios that turn a real-client check into one command.** A milestone's code is often finished well before its last checks, which need a real client at the keyboard, and the milestones after it wait on those checks. A scenario the client driver runs makes each of those checks a command the maintainer runs in a minute. The scenario format is stable, and these stay on the login and character screens, which nothing being built touches.
- **Answers to the open questions the phase files record.** Each of these F rows quotes a question that a milestone's own spec marks unverified. The answer comes from your own install with the tools already built: `client`, whose `types`, `messages` and `wad` commands answer most of these, then `bindecode --sweep` and `schemaprobe` for a question across every archive, and `typeextract` and `localetool`. The milestone then starts from your answer and proves it, instead of starting from the question.
- **Inputs a milestone's tests need.** A corpus with its expected answers is the slowest part of a test to get right, and it can be made before the code it tests.

The contract:

- **Needs.** A scenario: Windows, your own client, a build, a MySQL or MariaDB, and reference crops made by `python apps/clientdriver/drive.py capture-refs` at your own revision; presses are measured at 1280x720. A finding: your own install and a build of the tools. C-81: both MySQL 8 and MariaDB 10.6 or newer, which Docker provides in a minute. C-82: Python.
- **Delivers.** A scenario delivers the file, the report of a run that passed on your machine, what it asserted, and which part of the milestone's check still needs somebody looking at the screen. A finding delivers the facts that answer the quoted question: names, counts, ids, classes, field names and types, never content, with the tool invocations that produced them. A corpus delivers the inputs and the expected answers side by side, and says how each answer was obtained.
- **Proven by.** A scenario is proven when the maintainer runs it with their own client and it passes, and that run is what ticks the check. A finding is proven when the milestone it names re-derives it. A corpus is proven when the milestone's own tests load it and agree.
- **Refused.** A scenario that changes the driver's code, needs a screen `apps/clientdriver/references.json` does not hold, or was never run; a scenario that goes into the world includes `enter-world.json` rather than copying its steps. A finding that answers a different question from the one quoted, or repeats the spec's own guess back to it. A corpus whose expected answers came from reading the inputs rather than running them.

| Id | Item | Where it lands | Why it helps |
|---|---|---|---|
| C-80 | Scenario for 3.09's character list: an account holding three wizards, made through the client's own creation flow as `create-character.json` does, since a scenario seeds only one; prove the list names all three with their schools and levels, and say which part of the check, each wizard's look on screen, still needs eyes | `apps/clientdriver/scenarios/` | 3.09's last two checks ask to see this list, and 3.17, character deletion, waits on 3.09 |
| F-58 | For 6.10 and 6.11: from a sweep of the zone archives, every class that occurs in triggers.xml, trigger_groups.xml and volumes.xml, with instance counts, marking the ones the dump does not name | `contrib/findings/objects/` | 6.11 has to decode these with no unknown classes, and the ones the dump lacks are 6.10's work list |
| F-59 | For 7.01: every behaviour class the ObjectData tree uses, with how many templates carry each, and the behaviour hashes the dump does not name, with their counts | `contrib/findings/objects/` | 7.01's report counts exactly these across 104869 entries, and an unknown behaviour must still yield a row |
| F-62 | For 8.06 and 8.07: item templates counted by kind, and every requirement and effect class they use with its count, marking any the dump does not name | `contrib/findings/objects/` | 8.06 fails its import on an unknown class, so the unknowns are its work list |
| F-64 | For 8.03 and 8.13: what Root.wad's NPCServices.xml holds under root class hash 0x32a408e1, which the dump does not name: its properties and types, and whether it carries registrar or trainer service data | `contrib/findings/objects/` | Both milestones need the service data it may hold, and the class has to be named before it can be read |
| F-67 | For 10.13: how the Spiral Door is marked in the object templates, meaning the behaviours and properties that set it apart, so the server can match on those rather than on a hard-coded template id | `contrib/findings/world/` | 10.13's spec asks for a behaviour match and does not yet know what to match on |
| F-70 | For 13.06: whether zone data carries reagent spawn points and, if it does, the class, fields and count per zone for one world | `contrib/findings/world/` | 13.06 either reads these or has to author them, and does not yet know which |
| C-82 | A checker that flags SQL in `data/sql/updates` that only one of MySQL 8 and MariaDB accepts, tested against C-81's corpus, with a clean run on main recorded | `contrib/tools/` | Today the first thing to see a migration only one server accepts is the Linux leg, after it has reached main |

### The first and second lists

The first list's findings about combat, quests, pets, housing and the economy need servers that do not exist yet, so leave them unless you can observe what they ask. Since 2026-09-25 a real client enters the world and stands in Ravenwood, run by `apps/clientdriver/scenarios/enter-world.json`. What the client sends on arriving can be observed now, but nothing in the world answers it yet. The second list's findings, F-41 to F-55, need only your own install and the built tools.

| Id | Item | Where it lands | Why it helps |
|---|---|---|---|
| F-01 | Combat: the order a duel resolves in, what each side sends and when, and what the client shows at each step | `contrib/findings/combat/` | Phase 11 is the largest phase in the plan and the least documented. Every proven step shortens it |
| F-02 | Combat: how a spell's cost, accuracy, damage and effects are expressed in the client's own data | `contrib/findings/combat/` | Decides what the server must store and check for every card |
| F-03 | Combat: pips, power pips and shadow pips, how they are gained, spent and shown | `contrib/findings/combat/` | Small, self-contained, and needed before any duel is fought |
| F-04 | Quests: what a quest record holds, how a goal is expressed, how progress is reported and how rewards are named | `contrib/findings/quests/` | Phase 7 reads these; notes turn a month of guessing into a week of building |
| F-05 | Quests: how the client decides a quest helper's arrow and the quest log's grouping | `contrib/findings/quests/` | The visible half of questing, which the server has to feed |
| F-06 | World: how a zone's objects, triggers and doors are laid out in the client's own files | `contrib/findings/world/` | Phase 4 and 5 stream these; the format is known only in outline |
| F-07 | World: what a teleporter carries, given that the type data gives `ResTeleport` no properties at all | `contrib/findings/world/` | Blocks door destinations, which is why C-01 exists as authored data instead |
| F-08 | Protocol: what each unhandled login message means, above all MSG_USER_VALIDATE, and what a correct answer looks like | `contrib/findings/protocol/` | Milestone 5.06 needs it, and it is what makes the launcher's automatic login work |
| F-09 | Protocol: the session offer's encryption block, which the client reads and Ambrose does not yet send | `contrib/findings/protocol/` | The client logs a complaint on every connection today; nobody has decoded the block |
| F-10 | Protocol: how the client behaves when a message it expects never arrives, per screen | `contrib/findings/protocol/` | Tells the server which timeouts matter and what a stuck screen means |
| F-11 | Objects: which classes the client serialises with which flags, beyond what the type dump states | `contrib/findings/objects/` | The codec is byte-exact on 42 captures; the next hundred classes are unproven |
| F-12 | Client: what each launch option really does on this revision, beyond the documented list | `contrib/findings/client/` | Two of them already changed how the launcher and the test driver work |
| F-13 | Client: which files the client writes, when, and what it keeps between sessions | `contrib/findings/client/` | Ambrose promises it never writes inside your install; knowing what the client writes keeps that promise honest |
| F-14 | Patching: how the retail patcher names, requests and verifies a file | `contrib/findings/patching/` | Phase 16 serves patches; today the protocol is known only in outline |
| F-15 | Data: what changed between two client revisions, as a list of archives, zones and locale files | `contrib/findings/data/` | Milestone 3.23 follows KingsIsle's revisions and needs to know what a revision actually changes |
| F-16 | Pets, housing, crafting, fishing or gardening: any one system, its data and its messages | `contrib/findings/world/` | Phases 13 and 15 hold the least documented systems in the plan |
| F-17 | Protocol: the keepalive body the client sends and expects, its cadence, and what it does when a reply is late or malformed | `contrib/findings/protocol/` | Ambrose's own error log closes real sessions over a six-byte structure one side has wrong, and nobody has proven which |
| F-18 | Protocol: the session offer's encryption block byte by byte, and what the client does with each field | `contrib/findings/protocol/` | F-09 is this from the client's side; this is the server's, and the client complains on every connection today |
| F-19 | Protocol: what the client does when the same account signs in twice, and what the first session is told | `contrib/findings/protocol/` | The login server already refuses the second sign-in; what the first client shows is unproven |
| F-20 | Combat: the turn timer, the planning phase and the round boundary, as messages with their timing | `contrib/findings/combat/` | Phase 11 is the largest phase in the plan, and the round boundary is where every other combat fact hangs |
| F-21 | Combat: what the client is told about an enemy's intent before a round resolves, and when | `contrib/findings/combat/` | Decides whether the server must choose mob actions before or during resolution |
| F-22 | Combat: a player disconnecting mid-duel and returning, from both sides | `contrib/findings/combat/` | The case every combat implementation gets wrong, and the cheapest one to observe |
| F-23 | Items: how a stat on an item reaches the character sheet, and the order effects apply | `contrib/findings/objects/` | Phase 8 stores these, and the order is what makes two servers disagree about the same gear |
| F-24 | Items: what a treasure card is on the wire and how the side deck is expressed | `contrib/findings/objects/` | Small, self-contained, and a known gap in phases 8 and 11 |
| F-25 | Economy: what the client sends to buy and to sell, and what a bazaar listing carries | `contrib/findings/world/` | Phase 12 builds the bazaar, and a ledger needs to know what a legitimate mutation looks like |
| F-26 | Social: what a friend list entry holds and how presence changes reach other clients | `contrib/findings/protocol/` | Phase 12, and it is observable with two clients and no server work |
| F-27 | Social: how a trade is offered, locked and confirmed, and what each side sees at each step | `contrib/findings/protocol/` | Trading is the most abused system in every game of this kind, and the exact lock steps are what make it safe |
| F-28 | Instances: how an instance is created, joined and reset, including the sigil countdown | `contrib/findings/world/` | Phase 14 needs it and phase 12 touches it |
| F-29 | Housing: how a layout is stored and what the client sends when a decoration is moved | `contrib/findings/world/` | Phase 15 holds the least documented systems in the plan |
| F-30 | Gardening and fishing: the tick model, what is stored and what the client is told on return | `contrib/findings/world/` | Same phase, and both are timers, which is a shape the server has to own |
| F-31 | Pets: how talents are chosen, stored and shown | `contrib/findings/world/` | Phase 13, and the manifestation rules are pure data |
| F-32 | Chat: the filter levels, the channels and how menu chat's vocabulary is expressed | `contrib/findings/protocol/` | Needed before moderation, and it decides what the server must validate |
| F-33 | Mounts: how one is applied, and what another client sees | `contrib/findings/protocol/` | Named in the roadmap's own gap list as having no milestone |
| F-34 | Zones: what spawn data says about respawn timers and patrol paths | `contrib/findings/world/` | The roadmap records that no milestone reads collision, navmesh or spawn data yet |
| F-35 | Zones: how the client asks to change zone, and gates, recall and zone hops | `contrib/findings/world/` | Phases 4 and 5 stream these; the request side is known only in outline |
| F-36 | Movement: the client's update rate, its interpolation, and how much correction it accepts without visibly snapping | `contrib/findings/protocol/` | Decides the server's move validation budget, which nothing has measured |
| F-37 | Client: what each locale file drives, and how the client picks its language | `contrib/findings/client/` | The client ships eight locales and the panel's own localisation has to match what the client shows |
| F-38 | Data: what the type dump's opaque classes actually are, one class at a time | `contrib/findings/objects/` | Each one proven is one fewer blind spot in the codec |
| F-39 | Patching: how the retail patcher handles concurrency, resume and a corrupted file | `contrib/findings/patching/` | Phase 16 serves patches, and the failure paths are what a server has to survive |
| F-40 | Revisions: how to prove a change between two client revisions is real, rather than inferring it from a size | `contrib/findings/data/` | Milestone 3.23 follows revisions; F-15 asks what changed, this asks how you prove it |
| C-01 | Door destinations for Wizard City: every doorway a player can walk through, with the zone it leads to | `data/sql/updates/pending_db_world/` | `ResTeleport` carries no properties, so this table has to be authored. Phase 10 needs it and cannot generate it |
| C-11 | Fuzz seeds: inputs that made a decoder work hard, from your own captures | `data/fuzz/` | The fuzzers exist; they are only as good as their corpus |
| C-17 | Notes on how the client picks and shows realms after login | `contrib/findings/protocol/` | Milestone 4.03 builds the realm registry and has to match this behaviour |
| C-48 | Door destinations for a world beyond Wizard City | `data/sql/updates/pending_db_world/` | C-01's shape, more of it: the teleport class carries no properties, so this can only be authored |
| F-46 | Quest data shape: the fields of a quest template, the goal classes present and how many of each | `contrib/findings/quests/` | The static half of F-04 |
| F-48 | The client's own configuration: every key `config.xml` and `preferences.xml` carry, with the values a fresh install writes | `contrib/findings/client/` | The launcher splices only what it needs; this says what else is there |
| F-49 | The revision files: what `Bin/revision.dat` and the launcher's own files hold, byte layout by offset and meaning, never content | `contrib/findings/client/` | 3.23 follows revisions and needs to read these without guessing |
| F-51 | A second observation of F-12: repeat its experiment on your own install with your own hashes and record `verified` or `refuted` | `contrib/findings/client/` | A finding with two observers is worth more than two findings |
| F-55 | What a name table row holds: the fields the extractor reads from each name table, checked against the dump | `contrib/findings/data/` | The world database's name columns are shaped by it |

### The second list

The second list was written after a batch in which most findings came back as records that nothing was known, because the first list's findings need a duel or a zone to observe. It asks for what a contributor with a clone of the repository, Python and their own installation can actually produce, and each item carries a contract so a scaffold cannot pass for the item. Its code items, C-56 to C-75, are all merged and listed below, and so are its findings F-41 to F-45, F-47, F-50 and F-52 to F-54; F-46, F-48, F-49, F-51 and F-55 are still open.

- **Needs.** Your own installation and the built `bindecode`, `localetool` or `typeextract`; nothing else.
- **Delivers.** Facts about the data, names, counts, ids, offsets and types, never content, with the tool invocation that produced them so the maintainer can re-derive them.
- **Proven by.** The maintainer's own tools producing the same facts from a different install.
- **Refused.** A finding whose claim is about this repository rather than the game, or that names a path or an option that does not exist.

### Merged so far

| Id | What landed | Where it lives | Who wrote it |
|---|---|---|---|
| C-04 | A diff between two installations, reporting the archive, zone and locale files that differ | `contrib/tools/ambrose-install-diff/` | solanazaru-eng, in #2 |
| C-06 | A watcher reporting every message a running server refused or did not handle | `contrib/tools/ambrose-message-watcher/` | solanazaru-eng, in #1 |
| C-08 | A diff between two type dumps, reporting the metadata, classes and properties that changed | `contrib/tools/ambrose-type-diff/` | solanazaru-eng, in #4 |
| C-15 | A guide to reading Ambrose's logs, from a healthy startup to a stuck client | `doc/guides/logging.md` | solanazaru-eng, in #5 |
| C-12 | A guide to building and running Ambrose on Linux, walked end to end on Ubuntu 24.04 | `doc/guides/linux.md` | solanazaru-eng, in #6 |
| C-22 | The shape of a machine-checkable finding, which C-21 and C-51 both build against | `contrib/proposals/machine-checkable-findings.md` | solanazaru-eng, in #7 |
| C-20 | The two assertions the login startup smoke check does not make: an occupied port, and a leg with no database | `contrib/proposals/login-startup-smoke.md` | solanazaru-eng, in #9 |
| C-53 | What an operator is shown in their first ten minutes, given how much the first start already does | `contrib/proposals/operator-onboarding.md` | solanazaru-eng, in #10 |
| C-55 | A guide to contributing with an AI tool: what to give it, what to require of it, and how to judge the diff | `doc/guides/ai-contributing.md` | solanazaru-eng, in #11 |
| C-26 | A checker for machine-detectable client-derived files | `contrib/tools/ambrose-client-byte-checker/` | solanazaru-eng, in #35 |
| C-27 | A session quality prober measuring connect time, response time and jitter | `contrib/tools/ambrose-quality-prober/` | solanazaru-eng, in #34 |
| C-28 | An index of what a capture corpus covers and where the gaps are | `contrib/notes/capture-corpus-index.md` | solanazaru-eng, in #31 |
| C-29 | A classifier for the value runs inside a log line | `contrib/tools/ambrose-log-value-classifier/` | solanazaru-eng, in #33 |
| C-30 | A labelled corpus of log lines with its own validator | `contrib/tools/ambrose-log-corpus/` | solanazaru-eng, in #30 |
| C-31 | A report on how the console renders across terminals | `contrib/notes/terminal-rendering-report.md` | solanazaru-eng, in #32 |
| C-32 | A contrast auditor that checks every semantic pair in both themes | `contrib/tools/ambrose-contrast-auditor/` | solanazaru-eng, in #29 |
| C-34 | An audit of the chart series ramp in both themes | `contrib/proposals/chart-series-ramp-audit.md` | solanazaru-eng, in #27 |
| C-35 | A sequential ramp for heatmaps and density grids | `contrib/proposals/heatmap-ramp.md` | solanazaru-eng, in #26 |
| C-36 | An accessibility guide for the dashboard | `doc/guides/accessibility-dashboard.md` | solanazaru-eng, in #25 |
| C-37 | A screen reader transcript of the overview screen | `doc/guides/screen-reader-overview-transcript.md` | solanazaru-eng, in #28 |
| C-38 | The executable prefix of the synthetic probe scenario | `apps/clientdriver/scenarios/` | solanazaru-eng, in #24 |
| C-39 | Scenarios for a ban, an idle timeout, a reconnect and the shutdown notice | `apps/clientdriver/scenarios/` | solanazaru-eng, in #23 |
| C-40 | An alert rule pack for an operator | `contrib/proposals/alert-rule-pack.md` | solanazaru-eng, in #22 |
| C-41 | A dashboard definition with a validator | `contrib/tools/ambrose-dashboard-definition/` | solanazaru-eng, in #21 |
| C-42 | A corpus of cron cases with local and UTC projections | `contrib/tools/ambrose-cron-corpus/` | solanazaru-eng, in #20 |
| C-44 | A guide to a reverse proxy with TLS in front of Ambrose | `doc/guides/reverse-proxy-tls.md` | solanazaru-eng, in #19 |
| C-45 | A guide to putting a server on the internet safely | `doc/guides/internet-safety.md` | solanazaru-eng, in #18 |
| C-46 | A guide to capturing your own session safely | `doc/guides/safe-session-capture.md` | solanazaru-eng, in #17 |
| C-47 | A guide to crash reporting and what to remove before sending a dump | `doc/guides/crash-reporting.md` | solanazaru-eng, in #16 |
| C-49 | A content pack format for content that ships as data | `contrib/proposals/content-pack-format.md` | solanazaru-eng, in #15 |
| C-52 | Worked examples of a verified and a refuted finding | `contrib/proposals/finding-examples.md` | solanazaru-eng, in #14; MeruneFleuruwu, in [#27](https://github.com/Justchicoo/Project-Ambrose/pull/27) |
| C-54 | A quality bar for the admin API's authentication | `contrib/proposals/admin-auth-quality-bar.md` | solanazaru-eng, in #13 |
| C-02 | A metadata-only capture decoder | `contrib/tools/ambrose-capture-decoder/` | MeruneFleuruwu, in #50 |
| C-03 | Scenarios for the failures a client has to survive, delivered as C-39, with their catalog | `apps/clientdriver/scenarios/README.md` | MeruneFleuruwu, in #54 |
| C-05 | A report of what Ambrose does not yet understand in an install | `contrib/tools/ambrose-install-gap-report/` | MeruneFleuruwu, in #49 |
| C-07 | A zone map renderer from a manifest | `contrib/tools/ambrose-zone-map-renderer/` | MeruneFleuruwu, in #52 |
| C-09 | A world manifest checker | `contrib/tools/ambrose-world-manifest-checker/` | MeruneFleuruwu, in #51 |
| C-10 | A bounded TCP load generator | `contrib/tools/ambrose-load-generator/` | MeruneFleuruwu, in #48 |
| C-13 | A guide to the client under Wine or Proton | `doc/guides/wine-proton.md` | MeruneFleuruwu, in #43 |
| C-14 | A guide to running Ambrose in Docker | `doc/guides/docker.md` | MeruneFleuruwu, in #42 |
| C-16 | A Spanish catalog for the operator and launcher text | `contrib/locale/es.md` | MeruneFleuruwu, in #45 and #46 |
| C-18 | An item contract for every open item, and a character creation slice as a proposal | `contrib/proposals/contributor-track-improvements.md` | MeruneFleuruwu in #41, lRayZ24 in #98 |
| C-19 | An operator incident workspace | `contrib/proposals/operator-incident-workspace.md` | MeruneFleuruwu, in #40 |
| C-21 | A verifier for a finding's machine-checkable block | `contrib/tools/ambrose-finding-verifier/` | MeruneFleuruwu, in #38 |
| C-23 | A capture replayer against a local server | `contrib/tools/ambrose-capture-replayer/` | MeruneFleuruwu, in #39 |
| C-24 | A message coverage report from a server log | `contrib/tools/ambrose-message-coverage/` | MeruneFleuruwu, in #37 |
| C-25 | A diff between two message definition files | `contrib/tools/ambrose-message-definition-diff/` | MeruneFleuruwu, in #36 |
| C-33 | The light theme accent ramp, with its reasoning | `contrib/proposals/light-theme-accent-ramp.md` | MeruneFleuruwu, in #44 |
| C-43 | What a reproducible load report has to record | `contrib/notes/load-report-c43.md` | MeruneFleuruwu, in #58 |
| C-50 | Spanish for the dashboard pages | `contrib/locale/es.md` | MeruneFleuruwu, in #46 |
| C-51 | A dry validator for the machine-checkable finding block | `contrib/tools/ambrose-finding-check/` | MeruneFleuruwu, in #47 |
| C-56 | A JSON Schema for the status route with a fixture per app, checked against a live gameserver answer, and a checker that refuses a later version dropping or renaming a field | `contrib/schemas/` | MeruneFleuruwu, in #102 |
| C-58 | A corpus of five hundred console log records across every level and all eighteen categories the servers log under, with C-29's value spans as expected answers | `contrib/fixtures/` | MeruneFleuruwu, in #101 |
| C-57 | JSON Schemas for the apps and capabilities routes, with live gameserver answers as fixtures and a checker that refuses a later version dropping or renaming a field | `contrib/schemas/` | MeruneFleuruwu, in #103 |
| C-59 | Sixty seconds of tick, session and memory series in the shape the panel's charts take, with a plain gap and a marked restart | `contrib/fixtures/` | MeruneFleuruwu, in #104 |
| C-60 | The problem code catalog, with severity, operator action and the fixing page for each code the admin API registers, checked against the server's own list | `contrib/schemas/` | MeruneFleuruwu, in #105 |
| C-61 | Twenty duration strings with the seconds each yields or the refusal it gets, matching the parser's own answers | `contrib/fixtures/` | MeruneFleuruwu, in #106 |
| C-62 | Ten configuration layering cases, from the shipped default to the command line, each matching what ConfigMgr itself answers | `contrib/fixtures/` | MeruneFleuruwu, in #107 |
| C-63 | Five console lines that stress the column layout, each matching the bytes the formatter writes | `contrib/fixtures/` | MeruneFleuruwu, in #108 |
| C-64 | Nine hand-made frames whose lengths, opcodes and boundaries are wrong in every way the reassembler must survive | `data/fuzz/frame-reassembler/` | MeruneFleuruwu, in #109 |
| C-65 | The German and French catalogs for Ambrose's own text, each the same eighty keys as the Spanish one | `contrib/locale/` | MeruneFleuruwu, in #110 and #112 |
| C-66 | A walked Windows guide from a clean checkout to a built tree, the tests and a local login server | `doc/guides/` | MeruneFleuruwu, in #111 |
| C-67 | A guide to reading a login capture with the metadata decoder, frame by frame, with the service and order pairs a healthy login produces | `doc/guides/` | MeruneFleuruwu, in #113 |
| C-68 | The launcher's three misdiagnosed problems: the configuration the client ignores, the fullscreen traps, and the page a refused client opens | `doc/guides/` | MeruneFleuruwu, in #114 |
| C-69 | An audit of every shipped option against its page, and the nine drifts it found | `contrib/tools/config-audit/` | MeruneFleuruwu, in #115 |
| C-70 | An audit of every log category the code writes against the guide and the shipped loggers, and the undocumented one it found | `contrib/tools/log-category-audit/` | MeruneFleuruwu, in #116 |
| C-71 | A checker for the roadmap files: ids, dependencies, sizes and, on request, the evidence beside a ticked box | `contrib/tools/roadmap-check/` | MeruneFleuruwu, in #117 |
| C-72 | A dead-link checker for the documents, relative targets and heading anchors, with its clean run recorded as a note | `contrib/tools/dead-link-check/` | MeruneFleuruwu, in #118 |
| C-73 | An audit of every manifest against the notices file, and the nine dependencies it found with no row | `contrib/tools/dependency-notices-audit/` | MeruneFleuruwu, in #119 |
| C-74 | Three capture-free replay manifests built from the message definitions, whose bodies tell a login from a refusal | `contrib/fixtures/` | MeruneFleuruwu, in #120 and #122 |
| C-75 | A second observation of the merged log-category audit: the command, the exit code, each stream and the honest nothing it found | `contrib/notes/` | MeruneFleuruwu, in #121 |
| C-77 | A scenario for 2.14's banned account: banned from the login server console before it signs in, refused with AccountBanned, the client choosing GUI_AccountBannedTime, and never admitted or sent to character select | `apps/clientdriver/scenarios/c77-banned-login.json` | MeruneFleuruwu, in [#32](https://github.com/Justchicoo/Project-Ambrose/pull/32) |
| C-78 | A scenario for 2.15's AFK check: a seeded wizard left on character select past a 150-second Login.AfkTimeout, the server's MSG_DISCONNECT_LOGIN_AFK and the client choosing GUI_ConnectionAFK with no lost-connection message | `apps/clientdriver/scenarios/c78-afk-character-select.json` | MeruneFleuruwu, in [#33](https://github.com/Justchicoo/Project-Ambrose/pull/33) |
| C-79 | A scenario for 1.22's idle check: the client held at the login retry prompt through six keepalive cycles each way, over five minutes, with the session never closed | `apps/clientdriver/scenarios/c79-idle-login-keepalive.json` | MeruneFleuruwu, in [#34](https://github.com/Justchicoo/Project-Ambrose/pull/34) |
| C-81 | Nine SQL update files with the verdict MySQL 8.0.46 and MariaDB 11.8.9 give each, three that only MariaDB accepts and one only MySQL does, the dated names the promotion rule gives two pending files including the second of a day, and a validator | `contrib/fixtures/c81-sql-duality-corpus.json` `contrib/fixtures/c81-sql-duality/` | Kokoita, in [#68](https://github.com/Justchicoo/Project-Ambrose/pull/68) |
| F-41 | The message catalog, one finding per service: all 29 services' ids and names, their 1446 messages in order, and 4311 fields with their types | `contrib/findings/protocol/` | MeruneFleuruwu, in [#16](https://github.com/Justchicoo/Project-Ambrose/pull/16) |
| F-42 | The zone catalog: 3366 zones with their paths, ids, worlds and zone-data archives, all 3356 of a second install's matching, and the tool that prints them through the extractor | `contrib/findings/world/zone_catalog.json` `contrib/tools/ambrose-zone-catalog/` | MeruneFleuruwu, in [#17](https://github.com/Justchicoo/Project-Ambrose/pull/17) |
| F-43 | ObjectData templates counted by class, 117676 in 24 classes, with the ancestors they all share, and the cataloger that counts them | `contrib/findings/data/objectdata_template_catalog.json` `contrib/tools/ambrose-template-catalog/` | MeruneFleuruwu, in [#18](https://github.com/Justchicoo/Project-Ambrose/pull/18) |
| F-44 | The locale catalog: the eight locales Root.wad carries, with each one's file, key, repeated-key and skipped-file counts | `contrib/findings/client/locale_catalog.json` | MeruneFleuruwu, in [#19](https://github.com/Justchicoo/Project-Ambrose/pull/19) |
| F-45 | The spell template's 56 fields with types, the fields its six subclasses add, and the 18173 spells counted by school and by class | `contrib/findings/combat/spell_data_shape.json` | MeruneFleuruwu, in [#20](https://github.com/Justchicoo/Project-Ambrose/pull/20) |
| F-47 | The type dump's flag vocabulary: the 21 bits that occur across 49465 properties, with property and class counts per bit and per class | `contrib/findings/objects/type_property_flag_vocabulary.json` | MeruneFleuruwu, in [#21](https://github.com/Justchicoo/Project-Ambrose/pull/21) |
| F-50 | Which archives each world's zones draw from: Root.wad for every zone with placed objects, and its own world's WorldData archive for 136 zones in 23 worlds, with object counts, as the zone catalog tool's draws-from table prints them | `contrib/findings/world/zone_archive_catalog.json` `contrib/tools/ambrose-zone-catalog/` | MeruneFleuruwu, in [#23](https://github.com/Justchicoo/Project-Ambrose/pull/23) |
| F-52 | The message definitions' own metadata: each protocol's service id, type, version and description length, the six metadata elements with their types and NOXFER, and the field attributes including the three irregular fields | `contrib/findings/protocol/message-definition-metadata.json` | MeruneFleuruwu, in [#28](https://github.com/Justchicoo/Project-Ambrose/pull/28) |
| F-53 | The 702 type dump entries with no reflected properties, grouped by their full base lists, with counts | `contrib/findings/objects/empty-reflected-properties.json` | MeruneFleuruwu, in [#29](https://github.com/Justchicoo/Project-Ambrose/pull/29) |
| F-54 | Files, stored, compressed and uncompressed bytes for each of an install's 3,599 archives, matched row for row by a second install's 3,589 | `contrib/findings/data/kiwad_archive_inventory.json` | MeruneFleuruwu, in [#30](https://github.com/Justchicoo/Project-Ambrose/pull/30) |
| F-60 | Which properties of 7.02's quest, goal, reward, ServiceMemento, ActorDialog and madlib classes carry flag bits 1, 4 and 16, class by class | `contrib/findings/quests/f-60-quest-property-flags.json` | MeruneFleuruwu, in [#35](https://github.com/Justchicoo/Project-Ambrose/pull/35) |
| F-61 | How Root.wad's 18173 Spells/ entries are stored: every one compressed in the archive and none inside BINd, with their seven root classes counted | `contrib/findings/combat/spell-entry-encoding.json` | MeruneFleuruwu, in [#41](https://github.com/Justchicoo/Project-Ambrose/pull/41) |
| F-63 | The player's equipment slots: PlayerObject.xml's equipment behavior names BasicMobileEquipment, whose EquipmentTemplate holds 15 EquipSlot entries, with their names, categories and maximum item counts | `contrib/findings/data/f-63-equipment-slot-source.json` | MeruneFleuruwu, in [#36](https://github.com/Justchicoo/Project-Ambrose/pull/36) |
| F-65 | The 25 Sigils/ entries by class with their fields, and the sigil-info classes placed in each of Wizard City's 133 zones with their counts | `contrib/findings/combat/sigil-templates-and-zone-placements.json` | MeruneFleuruwu, in [#42](https://github.com/Justchicoo/Project-Ambrose/pull/42) |
| F-66 | MSG_SHOPLIST's Data is a binary-serialized WizShopOffering, from the client's own handler and the dump's field offsets, with its eight properties | `contrib/findings/protocol/msg-shoplist-data.json` | MeruneFleuruwu, in [#39](https://github.com/Justchicoo/Project-Ambrose/pull/39) |
| F-68 | Where the custom emotes live: 171 WizItemTemplate entries under Root.wad's ObjectData/Emotes/, each with one CustomEmoteBehaviorTemplate, counted by emote type | `contrib/findings/data/f-68-custom-emote-catalog.json` | MeruneFleuruwu, in [#37](https://github.com/Justchicoo/Project-Ambrose/pull/37) |
| F-69 | The level table a pet's energy comes from: Root.wad's MagicXPConfig.xml, 181 MagicLevelInfo rows for levels 0 to 180, each with an integer m_petEnergy | `contrib/findings/data/f-69-magic-level-data.json` | MeruneFleuruwu, in [#38](https://github.com/Justchicoo/Project-Ambrose/pull/38) |
| F-71 | Where a pond's fish list lives: each zone archive's FishingInfo.xml as FishingInfo, BodyOfWater and ZoneFish, with 400 ponds counted by world across 132 archives | `contrib/findings/world/fishing-info-pond-lists.json` | MeruneFleuruwu, in [#40](https://github.com/Justchicoo/Project-Ambrose/pull/40) |

An item stays listed until its pull request is merged. Ask before starting something not on the list: the answer is usually yes if it lands in the paths above.

## What happens to your work

Notes, proposals and guides are read by the agents building the milestones they touch, and cited in the milestone that uses them. A tool stays yours in `contrib/tools/`, and if the project later needs it in the servers, it is rebuilt inside `src/` under the architecture's rules, with your note kept. Scenarios, SQL and fuzz seeds are used where they are. Everything merged is MIT, as LICENSE says.

The maintainer's sessions may take an item over when a milestone they are building needs it now. You hear it on your pull request, and whatever of your work is used keeps your name on the commit.

### Started, still open

These carry a first delivery and stay listed above because the item is not what landed: C-01 has its `zone_teleport` table (#55) and no rows; C-11 has its seed folder's README (#56) and no seeds; F-12 has a finding that Ambrose's launcher applies its options and leaves the install untouched (#53), not yet what each of the client's own options does; F-51 has a second observation of the launcher's prepared runs leaving the install unchanged ([#24](https://github.com/Justchicoo/Project-Ambrose/pull/24)), not yet of a run that starts the client; F-55 has the disallowed-name rows checked against the dump ([#31](https://github.com/Justchicoo/Project-Ambrose/pull/31)), not yet the fields of the name tables in CharacterNames.xml, which `extractor --dry-run names` reads as 7955 names in 63 tables; F-49 has the layouts of Bin/revision.dat, Bin/data.dat and AppStart.dat checked on a second install ([#22](https://github.com/Justchicoo/Project-Ambrose/pull/22)), not yet where the launcher keeps Wizard101launcher-data.yml, or LocalPackagesList.txt on an r806919 install; F-46 has the quest and goal template schemas from the type dump ([#44](https://github.com/Justchicoo/Project-Ambrose/pull/44)), not yet how many of each goal class the install's data carries; F-58 has a note that a sweep finds 3,387 each of triggers.xml, trigger_groups.xml and volumes.xml with their root classes ([#67](https://github.com/Justchicoo/Project-Ambrose/pull/67)), not yet the classes inside them with counts, which since 6.11 can be read from the zone_trigger_result, zone_trigger, zone_volume and zone_trigger_event rows `extractor zones` writes, and for trigger_groups.xml from `schemaprobe --supplement` with the revision's class file.

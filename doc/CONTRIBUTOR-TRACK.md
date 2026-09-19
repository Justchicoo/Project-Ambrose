<!-- Project Ambrose by Imjustchico: The separate track other people work from: what it holds, the folders it may touch, and the open items. -->

# Contributor track

The phases in doc/ROADMAP.md are built in order by the maintainer's own agents, one milestone at a time. Nobody else works from them: two people building the same milestone lose track of each other, and a milestone half-built by someone else cannot be reviewed against its own acceptance checks.

So everything from outside lands here instead. This track holds work that helps the project finish sooner and cannot collide with a milestone: it adds files in folders no milestone builds in, it needs no change to a phase file, and it can be reviewed on its own.

Ask in the Discord before you start if anything here is unclear: https://discord.gg/Dx6ACDUj6N. It is also where a finding gets discussed before it is written up.

Read this document first, then contrib/findings/README.md and CONTRIBUTING.md. Working with an AI assistant is expected here: contrib/AI-START-HERE.md is a prompt to paste into yours, and it carries what that assistant needs to know about this repository before it writes anything.

## The rule that makes it safe

A change on this track touches only these paths:

| Path | What goes there |
|---|---|
| `contrib/tools/<name>/` | A self-contained tool of your own, in any language, that reads a user's own installation or an Ambrose server and prints or writes its own output |
| `contrib/findings/` | One proven claim per file about how the game behaves, in the shape contrib/findings/README.md gives, which Ambrose later verifies or refutes |
| `contrib/notes/` | Longer research writing that is not one claim: a survey, a walkthrough of a system, a summary of what you tried |
| `contrib/proposals/` | A proposal for something in the phases, as a document. The maintainer folds an accepted one into the roadmap; you never edit a phase file yourself |
| `apps/clientdriver/scenarios/` | Scenarios for the client driver, which are data, not code |
| `data/sql/updates/pending_db_world/` | World rows you authored yourself, such as a table of door destinations, as a dated update file that the maintainer moves into `data/sql/updates/db_world/` when it is merged |
| `data/fuzz/` | Seed inputs for the fuzzers that already exist |
| `doc/guides/` | Guides: running on a distribution, a graphics card, a language, a setup that needed a workaround |
| `contrib/locale/` | Translations of Ambrose's own text, never the game's |

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
| C-02 | A capture decoder: read a pcapng of a session against your own Ambrose server and print each message with its fields | `contrib/tools/` | Turns a capture into something readable when a message misbehaves |
| C-03 | Scenarios for the client driver: the idle timeouts, a ban taking effect, a shutdown notice, a reconnect | `apps/clientdriver/scenarios/` | Every scenario becomes a check that runs itself from then on |
| C-05 | A tool that reads your own install and reports what Ambrose does not yet understand: classes missing from the type data, files no reader handles | `contrib/tools/` | Points the next milestones at the real gaps |
| C-07 | A tool that renders a zone's objects as a map image from your own install | `contrib/tools/` | Makes a zone reviewable at a glance instead of row by row |
| C-09 | A tool that checks a world database against the client's own data and reports rows that disagree | `contrib/tools/` | Keeps authored content honest as the client changes |
| C-10 | A load generator: many fake clients against a server, reporting what it does under load | `contrib/tools/` | Nothing in the plan measures the server under load before phase 12 |
| C-11 | Fuzz seeds: inputs that made a decoder work hard, from your own captures | `data/fuzz/` | The fuzzers exist; they are only as good as their corpus |
| C-13 | A guide to running the client under Wine or Proton against an Ambrose server | `doc/guides/` | Decides whether Linux players are possible at all, which no milestone answers |
| C-14 | A guide to running everything in Docker, from nothing to a login screen | `doc/guides/` | Milestone 17.23 packages it; a walked path first makes that milestone cheap |
| C-16 | Translations of Ambrose's own text into a language the client supports | `contrib/locale/` | The client ships eight locales; the panel and launcher should not be English-only |
| C-17 | Notes on how the client picks and shows realms after login | `contrib/findings/protocol/` | Milestone 4.03 builds the realm registry and has to match this behaviour |
| C-18 | A proposal for anything in this document that is wrong or missing | `contrib/proposals/` | The track should improve as people use it |
| C-19 | A proposal for a panel page or feature you would want as an operator, with what it shows and what it does | `contrib/proposals/` | The panel is being built now, so a good proposal lands in a real milestone quickly |
| C-20 | A proposal for the server's own quality bar: a check, a limit or a guard you think is missing | `contrib/proposals/` | Outside eyes catch what a project stops seeing |
| C-21 | A finding verifier: read a finding's machine-checkable block and run it against a live Ambrose server or your own install, printing pass, fail or unable to run | `contrib/tools/` | Turns a merged claim into something a suite proves instead of something somebody gets to |
| C-23 | A capture replayer: replay a captured session against your own Ambrose server and report every message where the answer differs | `contrib/tools/` | Turns any capture into a regression test, and it is how F-17's disagreement gets settled in an afternoon |
| C-24 | A message coverage tool: which of the message ids a running server has ever seen, sent or refused | `contrib/tools/` | Points every later phase at the messages that actually occur rather than at the full list |
| C-25 | A message definition differ: what changed in the client's own message XML between two revisions | `contrib/tools/` | Feeds F-40 and 3.23, and it is the fastest way to know a revision is safe to follow |
| C-26 | A client-derived byte checker: scan a pull request's data files for anything that came from a client install | `contrib/tools/` | This track's hardest rule is enforced by reading today; this makes it enforceable |
| C-27 | A per-session quality prober: connect to your own server, measure round-trip time, jitter and timeout behaviour, and report | `contrib/tools/` | Produces exactly the falsifiable, repeatable, game-data-free numbers 17.92 needs, before 17.92 exists |
| C-28 | A capture corpus index: which captures exist, what each one covers, and what none of them covers | `contrib/notes/` | The gaps in the corpus are invisible today, and knowing them is what stops a decode being trusted too far |
| C-29 | A log value-class classifier: given a log line, return the byte ranges that name a thing, to the class list 17.76 publishes | `contrib/tools/` | Unusually clean contributor work: the specification is a class list, the input is real lines, the output is byte ranges, so it is pass or fail against a golden file |
| C-30 | A labelled corpus of Ambrose log lines: which runs in each line an operator would search for | `contrib/tools/`, beside C-29 | The labelling is the judgement work, and once labelled a golden file locks it down for good |
| C-31 | A terminal rendering report: how the console line of 17.74 renders on as many terminals as you can reach, at 16, 256 and truecolor | `contrib/notes/` | The 16-color rendering over SSH is what most operators will actually see, and nobody has looked at it on a real 16-color terminal |
| C-32 | A standalone contrast auditor: read a token file and print every text pair and every control-edge pair with its ratio and verdict | `contrib/tools/` | It is the logic the design system's gate needs, and it can be written and proven before that milestone starts |
| C-33 | A light-theme accent ramp with every pair computed, for any accent doc/DESIGN.md has not already settled | `contrib/proposals/` | Somebody has to do the arithmetic and the taste, and a computed proposal is worth more than an opinion |
| C-34 | A chart series ramp of seven steps, checked for the common kinds of color blindness and readable on both grounds | `contrib/proposals/` | doc/DESIGN.md asks for one and does not have one, and the first chart built without it will invent its own |
| C-35 | A sequential ramp for heatmaps and density grids, distinct from the four meaning accents | `contrib/proposals/` | Listed under Decisions needed, and 17.98's grid is waiting for it |
| C-36 | A guide to running the panel under high contrast and under a screen reader, recording what breaks | `doc/guides/` | The check nobody runs and the one most likely to find something, on a product whose whole state language is color |
| C-37 | A screen-reader transcript of the overview page, with what was confusing | `doc/guides/` | A transcript is evidence; a claim that a page is accessible is not |
| C-38 | Client driver scenarios for the synthetic probe: connect, handshake, authenticate, realm list, character select, enter world, walk, log out | `apps/clientdriver/scenarios/` | Each becomes a check that runs itself, and together they are the probe 17.81 schedules |
| C-39 | Client driver scenarios for the failure paths: an idle timeout, a ban taking effect, a shutdown notice, a reconnect | `apps/clientdriver/scenarios/` | Extends C-03 into the paths that only fail in production |
| C-40 | Alert rule packs: thresholds you have actually run a server with, naming the figure, the threshold, the duration and why | `contrib/proposals/` | Default thresholds invented by somebody who has never watched the graph are how alerting gets turned off |
| C-41 | A dashboard definition for the metrics endpoint, with the figures it assumes named | `contrib/tools/` or `doc/guides/` | Configuration rather than code, and it makes the metrics milestone useful the day it lands |
| C-42 | A cron corpus: expressions with their expected next runs, including both day fields restricted, the macros, and daylight saving gaps and overlaps | `contrib/tools/` | The panel and the server must agree exactly, and a corpus is how that is proved rather than hoped |
| C-43 | A load report: run the load generator against your own server and write down what happened, with the hardware named | `contrib/notes/` | Nothing in the plan measures the server under load before phase 12 |
| C-44 | A guide to running Ambrose behind a reverse proxy with TLS, end to end | `doc/guides/` | The first thing anybody does on a real machine, and the easiest to get subtly wrong |
| C-45 | A guide to opening a server to the internet safely: firewall rules, what to expose, what never to | `doc/guides/` | The panel fronts a game database, so this guide is a security control |
| C-46 | A guide to capturing a session without capturing anybody's credentials | `doc/guides/` | Captures are the project's main evidence and the main way somebody leaks their own password |
| C-47 | A guide to reading a crash: what the logs hold, what a dump holds, what to send | `doc/guides/` | Turns a crash report from a screenshot into something actionable |
| C-48 | Door destinations for a world beyond Wizard City | `data/sql/updates/pending_db_world/` | C-01's shape, more of it: the teleport class carries no properties, so this can only be authored |
| C-49 | A proposal for the content pack format: manifest, versioning, install and uninstall | `contrib/proposals/` | It is what turns authored data such as C-01 and C-48 from a merged file into something an operator installs and removes |
| C-50 | Locale catalogs for the newer pages: the command palette, the health page, the digest and the alert notices | `contrib/locale/` | C-16's shape, and translating is the fastest way to find a figure somebody formatted by hand |
| C-51 | The machine-checkable finding block: extend `contrib/findings/README.md` with it and write the tool that validates it, which the maintainer folds into the findings checker | `contrib/tools/` and the README this track already owns | C-22 as a pull request, which is what makes a claim provable by a suite |
| C-52 | A proposal for what a good finding looks like, with two worked examples, one verified and one refuted | `contrib/proposals/` | A refuted example teaches more than a verified one, and there is no example of either today |
| C-53 | A proposal for an operator onboarding path: what a new operator should be shown in their first ten minutes | `contrib/proposals/` | The panel is being built now, so a good proposal lands in a real milestone quickly |
| C-54 | A proposal for a quality bar the server is missing: a check, a limit or a guard | `contrib/proposals/` | C-20's shape, kept open deliberately, because outside eyes catch what a project stops seeing |
| C-55 | A guide to contributing with an AI tool: what to have it read first, what it gets wrong here, and how to check its work | `doc/guides/` | Ambrose is built this way and expects contributions built this way, and nobody has written down what actually works |

### Merged so far

| Id | What landed | Where it lives | Who wrote it |
|---|---|---|---|
| C-04 | A diff between two installations, reporting the archive, zone and locale files that differ | `contrib/tools/ambrose-install-diff/` | solanazaru-eng, in #2 |
| C-06 | A watcher reporting every message a running server refused or did not handle | `contrib/tools/ambrose-message-watcher/` | solanazaru-eng, in #1 |
| C-08 | A diff between two type dumps, reporting the metadata, classes and properties that changed | `contrib/tools/ambrose-type-diff/` | solanazaru-eng, in #4 |
| C-15 | A guide to reading Ambrose's logs, from a healthy startup to a stuck client | `doc/guides/logging.md` | solanazaru-eng, in #5 |
| C-12 | A guide to building and running Ambrose on Linux, walked end to end on Ubuntu 24.04 | `doc/guides/linux.md` | solanazaru-eng, in #6 |
| C-22 | The shape of a machine-checkable finding, which C-21 and C-51 both build against | `contrib/proposals/machine-checkable-findings.md` | solanazaru-eng, in #7 |

An item stays listed until its pull request is merged. Ask before starting something not on the list: the answer is usually yes if it lands in the paths above.

## What happens to your work

Notes, proposals and guides are read by the agents building the milestones they touch, and cited in the milestone that uses them. A tool stays yours in `contrib/tools/`, and if the project later needs it in the servers, it is rebuilt inside `src/` under the architecture's rules, with your note kept. Scenarios, SQL and fuzz seeds are used where they are. Everything merged is MIT, as LICENSE says.

<!-- Project Ambrose by Imjustchico: The separate track other people work from: what it holds, the folders it may touch, and the open items. -->

# Contributor track

The phases in doc/ROADMAP.md are built in order by the maintainer's own agents, one milestone at a time. Nobody else works from them: two people building the same milestone lose track of each other, and a milestone half-built by someone else cannot be reviewed against its own acceptance checks.

So everything from outside lands here instead. This track holds work that helps the project finish sooner and cannot collide with a milestone: it adds files in folders no milestone builds in, it needs no change to a phase file, and it can be reviewed on its own.

## The rule that makes it safe

A change on this track touches only these paths:

| Path | What goes there |
|---|---|
| `contrib/tools/<name>/` | A self-contained tool of your own, in any language, that reads a user's own installation or an Ambrose server and prints or writes its own output |
| `contrib/findings/` | One proven claim per file about how the game behaves, in the shape contrib/findings/README.md gives, which Ambrose later verifies or refutes |
| `contrib/notes/` | Longer research writing that is not one claim: a survey, a walkthrough of a system, a summary of what you tried |
| `contrib/proposals/` | A proposal for something in the phases, as a document. The maintainer folds an accepted one into the roadmap; you never edit a phase file yourself |
| `apps/clientdriver/scenarios/` | Scenarios for the client driver, which are data, not code |
| `data/sql/custom/db_world/` | World rows you authored yourself, such as a table of door destinations, as a dated update file |
| `data/fuzz/` | Seed inputs for the fuzzers that already exist |
| `doc/guides/` | Guides: running on a distribution, a graphics card, a language, a setup that needed a workaround |
| `contrib/locale/` | Translations of Ambrose's own text, never the game's |

Nothing else. Not `src/`, not `apps/` beyond the scenarios folder, not `doc/` beyond guides, not the phase files, not doc/ARCHITECTURE.md, doc/DESIGN.md, doc/PANEL.md, doc/UI-STACK.md, doc/ROADMAP.md, CMake files, CI files or the vcpkg manifest. A pull request that touches anything else is closed with a pointer to this document, because it cannot be merged without stopping a milestone in flight.

`python apps/ci/ci_contrib_paths.py --range <base>..<head>` says whether a change stays inside the track, and `python apps/ci/ci_findings.py` checks that every finding carries what Ambrose needs to prove it. CI runs both on every pull request labelled `contrib`, and you can run them before you open one.

## What every change needs

- **One thing at a time.** One tool, one note, one scenario, one guide. A pull request that does two things gets split.
- **Say where it came from.** Your own observation of your own installation, your own capture of your own session, a public source you name, or your own reasoning. Never a file from the game client, never code from another server project, and never text from a wiki or a site without naming its licence and waiting for the maintainer to accept it.
- **Say how it was checked.** A tool says what it was run against and what it printed. A note says how it was observed and what would disprove it. A scenario says it ran and what it asserted. SQL says the query that shows the rows are right.
- **No game files, ever.** No archive, asset, text, image or dump from the client, and nothing generated from one that carries its content. A tool reads the user's own installation at run time; that is the line.
- **Keep the house style.** Every file starts with the Project Ambrose branding header and a one-line brief, and carries no other comments. ASCII, LF endings. `python apps/codestyle/codestyle.py` must pass.
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
| C-01 | Door destinations for Wizard City: every doorway a player can walk through, with the zone it leads to | `data/sql/custom/db_world/` | `ResTeleport` carries no properties, so this table has to be authored. Phase 10 needs it and cannot generate it |
| C-02 | A capture decoder: read a pcapng of a session against your own Ambrose server and print each message with its fields | `contrib/tools/` | Turns a capture into something readable when a message misbehaves |
| C-03 | Scenarios for the client driver: the idle timeouts, a ban taking effect, a shutdown notice, a reconnect | `apps/clientdriver/scenarios/` | Every scenario becomes a check that runs itself from then on |
| C-04 | A tool that diffs two revisions of an install: which archives, zones and locale files changed | `contrib/tools/` | Feeds F-15 and milestone 3.23 |
| C-05 | A tool that reads your own install and reports what Ambrose does not yet understand: classes missing from the type data, files no reader handles | `contrib/tools/` | Points the next milestones at the real gaps |
| C-06 | A tool that watches a running Ambrose server and prints every message it refused or did not handle | `contrib/tools/` | Finds the holes a scenario has not covered yet |
| C-07 | A tool that renders a zone's objects as a map image from your own install | `contrib/tools/` | Makes a zone reviewable at a glance instead of row by row |
| C-08 | A tool that compares two type dumps and explains what a revision changed | `contrib/tools/` | Turns a revision bump into a readable list instead of a diff of 11 MiB |
| C-09 | A tool that checks a world database against the client's own data and reports rows that disagree | `contrib/tools/` | Keeps authored content honest as the client changes |
| C-10 | A load generator: many fake clients against a server, reporting what it does under load | `contrib/tools/` | Nothing in the plan measures the server under load before phase 12 |
| C-11 | Fuzz seeds: inputs that made a decoder work hard, from your own captures | `data/fuzz/` | The fuzzers exist; they are only as good as their corpus |
| C-12 | A guide to running the server on a Linux distribution end to end, with the packages and the pitfalls | `doc/guides/` | The project is developed on Windows; someone has to walk the other path first |
| C-13 | A guide to running the client under Wine or Proton against an Ambrose server | `doc/guides/` | Decides whether Linux players are possible at all, which no milestone answers |
| C-14 | A guide to running everything in Docker, from nothing to a login screen | `doc/guides/` | Milestone 17.23 packages it; a walked path first makes that milestone cheap |
| C-15 | A guide to reading Ambrose's own logs: what each category means and what a healthy start looks like | `doc/guides/` | The fastest way to make a new operator self-sufficient |
| C-16 | Translations of Ambrose's own text into a language the client supports | `contrib/locale/` | The client ships eight locales; the panel and launcher should not be English-only |
| C-17 | Notes on how the client picks and shows realms after login | `contrib/findings/protocol/` | Milestone 4.03 builds the realm registry and has to match this behaviour |
| C-18 | A proposal for anything in this document that is wrong or missing | `contrib/proposals/` | The track should improve as people use it |
| C-19 | A proposal for a panel page or feature you would want as an operator, with what it shows and what it does | `contrib/proposals/` | The panel is being built now, so a good proposal lands in a real milestone quickly |
| C-20 | A proposal for the server's own quality bar: a check, a limit or a guard you think is missing | `contrib/proposals/` | Outside eyes catch what a project stops seeing |

An item stays listed until its pull request is merged. Ask before starting something not on the list: the answer is usually yes if it lands in the paths above.

## What happens to your work

Notes, proposals and guides are read by the agents building the milestones they touch, and cited in the milestone that uses them. A tool stays yours in `contrib/tools/`, and if the project later needs it in the servers, it is rebuilt inside `src/` under the architecture's rules, with your note kept. Scenarios, SQL and fuzz seeds are used where they are. Everything merged is MIT, as LICENSE says.

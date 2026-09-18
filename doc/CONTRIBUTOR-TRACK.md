<!-- Project Ambrose by Imjustchico: The separate track other people work from: what it holds, the folders it may touch, and the open items. -->

# Contributor track

The phases in doc/ROADMAP.md are built in order by the maintainer's own agents, one milestone at a time. Nobody else works from them: two people building the same milestone lose track of each other, and a milestone half-built by someone else cannot be reviewed against its own acceptance checks.

So everything from outside lands here instead. This track holds work that helps the project finish sooner and cannot collide with a milestone: it adds files in folders no milestone builds in, it needs no change to a phase file, and it can be reviewed on its own.

## The rule that makes it safe

A change on this track touches only these paths:

| Path | What goes there |
|---|---|
| `contrib/tools/<name>/` | A self-contained tool of your own, in any language, that reads a user's own installation or an Ambrose server and prints or writes its own output |
| `contrib/notes/` | Research notes: how the game behaves, what a message carries, how a system works, with how you found it |
| `contrib/proposals/` | A proposal for something in the phases, as a document. The maintainer folds an accepted one into the roadmap; you never edit a phase file yourself |
| `apps/clientdriver/scenarios/` | Scenarios for the client driver, which are data, not code |
| `data/sql/custom/db_world/` | World rows you authored yourself, such as a table of door destinations, as a dated update file |
| `data/fuzz/` | Seed inputs for the fuzzers that already exist |
| `doc/guides/` | Guides: running on a distribution, a graphics card, a language, a setup that needed a workaround |
| `contrib/locale/` | Translations of Ambrose's own text, never the game's |

Nothing else. Not `src/`, not `apps/` beyond the scenarios folder, not `doc/` beyond guides, not the phase files, not doc/ARCHITECTURE.md, doc/DESIGN.md, doc/PANEL.md, doc/UI-STACK.md, doc/ROADMAP.md, CMake files, CI files or the vcpkg manifest. A pull request that touches anything else is closed with a pointer to this document, because it cannot be merged without stopping a milestone in flight.

`python apps/ci/ci_contrib_paths.py --range <base>..<head>` says whether a change stays inside the track. CI runs it on every pull request labelled `contrib`, and you can run it before you open one.

## What every change needs

- **One thing at a time.** One tool, one note, one scenario, one guide. A pull request that does two things gets split.
- **Say where it came from.** Your own observation of your own installation, your own capture of your own session, a public source you name, or your own reasoning. Never a file from the game client, never code from another server project, and never text from a wiki or a site without naming its licence and waiting for the maintainer to accept it.
- **Say how it was checked.** A tool says what it was run against and what it printed. A note says how it was observed and what would disprove it. A scenario says it ran and what it asserted. SQL says the query that shows the rows are right.
- **No game files, ever.** No archive, asset, text, image or dump from the client, and nothing generated from one that carries its content. A tool reads the user's own installation at run time; that is the line.
- **Keep the house style.** Every file starts with the Project Ambrose branding header and a one-line brief, and carries no other comments. ASCII, LF endings. `python apps/codestyle/codestyle.py` must pass.
- **Name your tool in the commit.** AI tools are expected here; add the trailer CONTRIBUTING.md asks for.
- **A tool brings its own dependencies.** Pin them inside `contrib/tools/<name>/`, and do not add anything to the repository's own manifests.

## The open items

Each item is worth doing, needs nothing from the phases, and lands inside the track. Take one by opening a pull request; say in it which item you took.

| Id | Item | Where it lands | Why it helps |
|---|---|---|---|
| C-01 | Door destinations for Wizard City: every doorway a player can walk through, with the zone it leads to, observed in your own client | `data/sql/custom/db_world/` | `ResTeleport` carries no properties in the type data, so this table has to be authored. Phase 10 needs it and cannot generate it |
| C-02 | A capture decoder: read a pcapng of a session against your own Ambrose server and print each message with its fields | `contrib/tools/` | Turns a capture into something readable when a message misbehaves, without waiting for the panel's log viewer |
| C-03 | Scenarios for the client driver: the idle timeouts, a ban taking effect, a shutdown notice, a reconnect | `apps/clientdriver/scenarios/` | Every scenario is a check that runs itself from then on |
| C-04 | Notes on how the client picks and shows realms after login, from watching your own client | `contrib/notes/` | Milestone 4.03 builds the realm registry and this is the behaviour it has to match |
| C-05 | A guide to running the server on a Linux distribution end to end, with the packages and the pitfalls | `doc/guides/` | The project is developed on Windows; someone has to walk the other path first |
| C-06 | Notes on the quest system's own data: what a quest record holds, how a goal is expressed, how rewards are named | `contrib/notes/` | Phase 7 has to read these; notes shorten it |
| C-07 | A tool that diffs two revisions of an install: which archives, zones and locale files changed | `contrib/tools/` | Milestone 3.23 follows KingsIsle's revisions and needs to know what a revision actually changes |
| C-08 | Fuzz seeds: blobs and frames that made a decoder work hard, from your own captures | `data/fuzz/` | The fuzzers exist; they are only as good as their corpus |
| C-09 | Translations of Ambrose's own text into a language the client supports | `contrib/locale/` | The client ships eight locales; the panel and launcher should not be English-only |
| C-10 | Notes on combat: the order a duel resolves in, what each side sends, what the client shows | `contrib/notes/` | Phase 11 is the largest phase in the plan and the least documented |
| C-11 | A tool that reads your own install and reports what Ambrose does not yet understand: classes missing from the type data, files no reader handles | `contrib/tools/` | Points the next milestones at the real gaps |
| C-12 | A proposal for anything in this document that is wrong or missing | `contrib/proposals/` | The track should improve as people use it |

An item stays listed until its pull request is merged. Ask before starting something not on the list: the answer is usually yes if it lands in the paths above.

## What happens to your work

Notes, proposals and guides are read by the agents building the milestones they touch, and cited in the milestone that uses them. A tool stays yours in `contrib/tools/`, and if the project later needs it in the servers, it is rebuilt inside `src/` under the architecture's rules, with your note kept. Scenarios, SQL and fuzz seeds are used where they are. Everything merged is MIT, as LICENSE says.

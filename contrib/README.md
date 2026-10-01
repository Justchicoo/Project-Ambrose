<!-- Project Ambrose by Imjustchico: What the contributor track is and where each kind of change goes. -->

# contrib

Work from outside the maintainer's own milestones lands here. doc/CONTRIBUTOR-TRACK.md holds the rules, the folders and the open items; this file is only the signpost.

- `tools/<name>/` a self-contained tool of your own, in any language, with its own dependencies pinned inside it
- `findings/` one proven claim per file about how the game behaves, in the shape `findings/README.md` gives
- `notes/` what you found out about the game or the server, and how you found it
- `proposals/` a change you would like made to the phases, as a document for the maintainer
- `locale/` translations of Ambrose's own text
- `fixtures/` sample data in the shapes the panel and the servers exchange, hand-written or generated and never captured
- `schemas/` JSON Schemas for those shapes, and corpora with expected answers a loader or parser is tested against

A track change may also add client driver scenarios in `apps/clientdriver/scenarios/`, world rows as a pending update in `data/sql/updates/pending_db_world/`, fuzz seeds in `data/fuzz/` and guides in `doc/guides/`. `python apps/ci/ci_contrib_paths.py` checks a change against that list.

Contributing with an AI assistant is expected here: `AI-START-HERE.md` is a prompt to paste into yours, and it carries what that assistant needs to know about this repository before it writes anything.

The roadmap's own milestones are the other track. `AI-MILESTONES-HERE.md` is the prompt for those, and doc/MILESTONE-TRACK.md says which ones are open, how one is claimed and what finishing one means. That work lands in `src/`, not here.

Nothing here is built by the repository's own CMake, and nothing here is loaded by a server. A tool that the project later needs inside a server is rebuilt in `src/` under the architecture's rules, with your note kept.

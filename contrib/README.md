<!-- Project Ambrose by Imjustchico: What the contributor track is and where each kind of change goes. -->

# contrib

Work from outside the maintainer's own milestones lands here. doc/CONTRIBUTOR-TRACK.md holds the rules, the folders and the open items; this file is only the signpost.

- `tools/<name>/` a self-contained tool of your own, in any language, with its own dependencies pinned inside it
- `notes/` what you found out about the game or the server, and how you found it
- `proposals/` a change you would like made to the phases, as a document for the maintainer
- `locale/` translations of Ambrose's own text

Nothing here is built by the repository's own CMake, and nothing here is loaded by a server. A tool that the project later needs inside a server is rebuilt in `src/` under the architecture's rules, with your note kept.

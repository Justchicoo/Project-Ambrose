<!-- Project Ambrose by Imjustchico: How to contribute, including the rules for AI-written changes. -->
# Contributing

Project Ambrose is an AI-driven project. Contributions written with AI tools are expected, not just allowed.

## Where your change goes

The phases in doc/ROADMAP.md are built in order by the maintainer's own agents, one milestone at a time, so nobody else works from them: two people on the same milestone lose track of each other, and half a milestone cannot be reviewed against its own acceptance checks.

Work from outside lands on the contributor track instead, described in [doc/CONTRIBUTOR-TRACK.md](doc/CONTRIBUTOR-TRACK.md). It has its own folders, its own list of open items, and a check that keeps it clear of everything a milestone touches: `python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD`, where `upstream` is whichever of your remotes is github.com/Justchicoo/Project-Ambrose. Three dots, and the branch on the remote your pull request targets: your own `main`, stale or moved on while you worked, makes the check flag files you never touched. Before your first commit, `python apps/ci/ci_contrib_paths.py --paths <files>` checks files that are not committed yet. CI runs that check on every pull request from a fork; on a branch in this repository, the `contrib` label turns it on.

## How to contribute
1. Fork the repository and create a branch for your change.
2. Pick an item from doc/CONTRIBUTOR-TRACK.md, or ask first if what you have in mind is not listed.
3. Read [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) so your change lands in the right place.
4. Use any AI coding tool you like to write the change.
5. Open a pull request that explains what the change does and how it was verified.

## Requirements

- **C++20** for server code. Tools may use any language that does the job well.
- **Follow the architecture.** The layout, layering, and methods in doc/ARCHITECTURE.md mirror AzerothCore. Database changes go in dated update files, content goes in the world database, and custom content goes in scripts or modules instead of core edits.
- **Branding header, no other comments.** Every file starts with the Project Ambrose header and a one-line brief of what it holds and does, in the format doc/ARCHITECTURE.md gives for its file type. Nothing else in the file is a comment. Run `python apps/codestyle/codestyle.py` before committing; `ctest` also runs it.
- **From scratch.** Study AzerothCore for structure and other projects for game behavior, then reimplement. Do not copy, translate, or port code from any of them, and do not commit their data files.
- **No game files.** Never commit files extracted from the game client.
- **Disclose the AI.** Add a trailer to each commit naming the model or tool that wrote it, for example:

  ```
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
  ```

- **CI builds on request.** Pull requests get the style, forbidden file and commit trailer checks automatically. A maintainer adds a `ci:` label, such as `ci:weekly`, `ci:windows-msvc-x64` or `ci:all`, to build and test a pull request in CI. A label stays until removed, so every later push to the pull request builds its legs again. Build your change and run `ctest` with the presets before every push; it runs the unit tests and the `codestyle` and `ci` checks CI runs.
- **Verify it.** Say in the pull request how the change was tested. Unverified changes will not be merged.

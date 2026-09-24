<!-- Project Ambrose by Imjustchico: How to contribute, including the rules for AI-written changes. -->
# Contributing

Project Ambrose is an AI-driven project. Contributions written with AI tools are expected, not just allowed.

Questions, half-formed ideas and anything you are unsure about belong in the Discord: https://discord.gg/Dx6ACDUj6N. Nothing there needs to be polished first.

## Where your change goes

There are two tracks, and the first is the safe one.

Work from outside lands on the contributor track, described in [doc/CONTRIBUTOR-TRACK.md](doc/CONTRIBUTOR-TRACK.md). It has its own folders, its own list of open items, and a check that keeps it clear of everything a milestone touches: `python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD`, where `upstream` is the remote step 1 below adds for github.com/Justchicoo/Project-Ambrose and `git fetch upstream` keeps current. A clone of your own fork has no such remote until you add it, and the command fails with `unknown revision` until you do. Three dots, and the branch on the remote your pull request targets: your own `main`, stale or moved on while you worked, makes the check flag files you never touched. Before your first commit, `python apps/ci/ci_contrib_paths.py --paths <files>` checks files that are not committed yet. CI runs that check on every pull request from a fork; on a branch in this repository, the `contrib` label turns it on from the next push, because only a `ci:` label makes the labelling itself start a run.

The second track is the roadmap itself, described in [doc/MILESTONE-TRACK.md](doc/MILESTONE-TRACK.md). A named set of milestones is open to outside help, with the source tree and the acceptance checks that come with them. Everything not named there is reserved, because the maintainer's own agents build the phases in order and two people on the same milestone lose track of each other. A milestone is taken on a branch named `milestone/<id>-<short-name>`, which is what makes the path check accept a change under `src/`, and it is finished only when every acceptance check in its phase file is ticked with the evidence that proved it. [contrib/AI-MILESTONES-HERE.md](contrib/AI-MILESTONES-HERE.md) is the prompt for that work.

## How to contribute
1. Fork the repository, clone your fork, add this repository as a second remote with `git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git`, and create a branch for your change.
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

  A commit whose author is a bot account, such as Dependabot's grouped updates, needs no trailer: its author already names the tool.

- **CI builds on request.** Pull requests get the style, forbidden file and commit trailer checks automatically. A maintainer adds a `ci:` label, such as `ci:weekly`, `ci:windows-msvc-x64` or `ci:all`, to build and test a pull request in CI. A label stays until removed, so every later push to the pull request builds its legs again. Build your change and run `ctest` with the presets before every push; it runs the unit tests and the `codestyle` and `ci` checks CI runs.
- **Verify it.** Say in the pull request how the change was tested. Unverified changes will not be merged.

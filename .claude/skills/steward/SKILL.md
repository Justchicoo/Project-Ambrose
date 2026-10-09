---
name: steward
description: Use when driving a Project Ambrose pull request to green and mergeable, reviewing a contributor's pull request, or landing reviewed work on main. Covers the checks to run before a push, CI labels, the review round format and how landed work is committed.
---
<!-- Project Ambrose by Imjustchico: How a session drives, reviews and lands pull requests in this repository, pointing to the rulebooks that decide each step. -->

# Stewarding pull requests

doc/REVIEWING.md is the rulebook. Read it whole before any review; its first line asks for that. doc/MILESTONE-TRACK.md and doc/CONTRIBUTOR-TRACK.md say what each track may change. This skill is the short path through them and never overrides them.

## Before any push

- `python apps/codestyle/codestyle.py`: branding header on every file, and no other comments.
- `python apps/ci/ci_local.py` runs CI's checks job over the commits the push would send: self-tests, forbidden files, findings, roadmap summary, commit trailers, and the track path check when `--branch` is given.
- Build and run the tests the change touches. Run the full `ctest --preset <preset>` before the push that should turn green, into a log as ambrose-lean says, or leave it to CI's `ci:` legs.
- For front-end changes, `npm run verify`.
- For a CI fix, reproduce the failure first, then show the same check passing.
- Every commit carries the AI attribution trailer. `ci_commit_trailer.py` refuses a commit without one.

## Driving your own pull request

- Work in this order: merge conflicts, then red CI, then review comments. A red or conflicted head is never "waiting on review".
- Fix the root cause. Never skip, disable or quarantine a test, push an empty commit, or close and reopen just to rerun CI.
- Main has a required `checks` status. A branch named `milestone/<id>-<name>` gets the Linux GCC leg by itself. Other legs need a `ci:` label (`ci:windows-msvc-x64`, `ci:all`), which the maintainer's side adds when the change deserves it. While iterating, label one leg (`ci:linux-gcc` or `ci:windows-msvc-x64`); add `ci:all` only right before landing, and take it off a pull request another one supersedes, because every push re-runs all seven jobs.

## Reviewing a contributor's pull request

- Verify by running, never by reading. Build the tool and run it on a real input. Load a scenario through the driver's validator. Check every path and option a guide names against the tree.
- Sweep mechanically first, over `origin/main...prN`: `ci_contrib_paths.py`, codestyle, `ci_forbidden_files.py`, `ci_findings.py`, and independence (one or two commits, only its own files).
- A client-derived file is the one thing that blocks outright. It comes out first, and the tool that produces that data at run time is offered in its place.
- Nothing is closed for being unfinished. Each round is one message the contributor's AI can act on. It says what works and how that was verified, then a numbered list of every remaining item. Each item gives the file and line, what is wrong, the exact change and the command that proves it. The round ends with "push to the same branch and it will be rechecked". Every round restates the whole list.

## Landing

- Contributor work lands as one commit made with `git merge --squash prN` on main, authored to the contributor's numeric noreply address, with their `Co-Authored-By`. The subject is `<item id>: <what landed>`. Never use GitHub's squash button for it, which names their account email.
- Whatever the maintainer's side can finish in the same sitting is fixed on main, one commit per fix, naming the pull request and the fault.
- Update the tracks in the same sitting: the merged table, "Started, still open", and doc/MILESTONE-TRACK.md's Started and Landed rows. Run `python apps/ci/tests/test_ci.py` and `python apps/site/build.py --check` before pushing.
- When a batch teaches something, write it into doc/REVIEWING.md or the contributor prompt (contrib/AI-MILESTONES-HERE.md), not into a review comment.

## Running servers to verify

Build every artifact from the same commit. A server prints its revision at start; compare it with `git log -1`. Stop only processes this checkout started, scoped by path as doc/REVIEWING.md shows, never by image name.

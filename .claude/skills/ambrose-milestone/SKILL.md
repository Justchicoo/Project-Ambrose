---
name: ambrose-milestone
description: Use when picking, building, verifying or landing a Project Ambrose roadmap milestone. Covers picking any milestone, reading its phase section, using and upgrading the repo's tools, ticking acceptance checks with evidence, and the landing commit.
---
<!-- Project Ambrose by Imjustchico: The steps a session follows to take one roadmap milestone from choosing it to landing it on main. -->

# Building a milestone

This follows CLAUDE.md, doc/MILESTONE-TRACK.md and doc/REVIEWING.md. Those files win if they ever disagree with this one. Read docs by section, as the ambrose-lean skill shows. `docsection` below means `python .claude/skills/ambrose-lean/docsection.py`.

## 1. Pick one

- Any milestone is yours to take, in any phase and any order, and nobody has to be asked: nothing is held or reserved, and no session owns an area. `docsection doc/ROADMAP.md "Where we are|Decisions needed"` says what has landed.
- Look at what is being built before starting: https://justchicoo.github.io/Project-Ambrose/state.json lists every open pull request against its milestone, from a `milestone/<id>` branch or a title that starts with the id. If somebody already has one on yours, read it and say on it what you will do, so the two combine rather than build the same thing twice.
- Read the milestone's own section in its phase file: `docsection doc/roadmap/phase-NN-*.md "^N\.MM |Review notes"`. If a milestone on its **Depends on** line is not done, build what it needs in the same pull request or take that one first.
- `docsection doc/MILESTONE-TRACK.md "Started"` lists milestones that already have work landed, with what is left. Finishing one is as welcome as starting another.

## 2. Use the tools, and teach them

- Read `docsection doc/TOOLS.md "How this list is used"`, then the entry of each tool the milestone touches. Look at what is actually under src/tools and apps, because the list drifts.
- If something reads a client file format, the ambrose-client-decode skill says which tool asks the question.
- When a tool cannot do what the milestone needs, add the function to that tool, test it, and note it in the tool's doc/TOOLS.md entry. A one-off script, a hard-coded offset or a hand parser inside the milestone is a workaround, and it is not allowed.
- The milestone should end with the suite able to read and decode more than it could at the start.

## 3. Build it the repo's way

- C++20, in the folder doc/ARCHITECTURE.md's layout gives its subsystem, respecting the layering (`apps -> scripts -> game -> database -> shared -> common -> deps`).
- Every file starts with the branding header in its type's form, and has no other comments. Run `python apps/codestyle/codestyle.py`.
- Database changes go in dated update files. Content goes in the world database.
- Tests go in src/test/, mirroring the code's folder. A test that needs the install carries the CTest label `client` and skips unless `AMBROSE_CLIENT_DIR` is set.
- Never commit anything from the client or generated from it: no archive, asset, dump, capture or byte run, in code, tests, docs or skills. Tools read the user's own install at run time.
- Build the deliverables the phase file lists. If one is wrong or impossible, say so and propose the change. Don't quietly build something else.
- A question listed under Decisions needed is settled by the maintainer's standing direction, recorded in doc/ROADMAP.md's Resolved entries: take the recommended option. Record it under Resolved and in the matching doc/ARCHITECTURE.md Decisions entry, with the reason, in the same change.

## 4. Verify by running

- Build with the presets and run the tests that prove each check. On Linux: `cmake --preset linux-gcc`, `cmake --build --preset linux-gcc-debug`, then `ctest --preset linux-gcc-debug -R <tests>`. On Windows use `windows-msvc-x64` and `windows-debug`.
- Before the landing commit, run the full `ctest` and `python apps/ci/ci_local.py`.
- Real-client checks run on the owner's PC with `python apps/clientdriver/drive.py run` and a scenario, following the rules for heavy jobs there. If a check can't be run, leave it unticked and say which one and why.

## 5. Tick and land

- Tick both lists (the short one under the heading and the full one at the end of the detailed spec), in the same commit as the code that earns them.
- Each tick quotes its evidence in brackets: the test name, the tool run and what it printed, or the screen and what it showed. Write `<account>`, `<address>` and so on, never a real one.
- The landing commit also updates doc/ROADMAP.md's "Where we are" (`ci_roadmap_state.py` fails without it). It regenerates the progress card with `python apps/progress/progress.py`, which updates doc/progress/progress.json, progress.svg and badge.json. It updates doc/TOOLS.md for any tool that learned something.
- When the milestone has a row in doc/MILESTONE-TRACK.md's Started table, updating or removing it is a separate "Track <id>'s landing" commit. Nothing is reserved, so there is no next one to reserve.
- Commit subject is `<id>: <what a player or operator now sees>`, and the body says what changed and how it was verified. Every commit ends with the AI attribution trailer naming the model.

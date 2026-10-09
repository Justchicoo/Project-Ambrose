---
name: ambrose-lean
description: Use in every Project Ambrose session before reading docs, searching code, building or running tests. Keeps token use low by reading doc sections instead of whole files and keeping build and test output short.
---
<!-- Project Ambrose by Imjustchico: How a session in this repository reads, searches, builds and tests while spending as few tokens as it can. -->

# Lean work in Project Ambrose

Plan limits are shared by every session, and they pause at 90%. Most wasted tokens here come from reading huge docs whole and from streaming build output. These rules cut both.

## Read docs by section

These files are far too big to read whole: doc/ARCHITECTURE.md (about 190 KB, 177 KB of it Decisions), doc/PANEL.md (170 KB), doc/roadmap/phase-17-*.md (580 KB), doc/ROADMAP.md (80 KB), doc/TOOLS.md (75 KB), doc/PANEL-MAP.md, doc/UI-STACK.md and doc/CONTRIBUTOR-TRACK.md (about 50 KB each).

Print a heading index with section sizes, then only the sections you need:

```
python .claude/skills/ambrose-lean/docsection.py doc/ARCHITECTURE.md
python .claude/skills/ambrose-lean/docsection.py doc/ARCHITECTURE.md "Repository layout|Layering|Conventions"
python .claude/skills/ambrose-lean/docsection.py doc/roadmap/phase-08-*.md "^8\.01 |Review notes"
python .claude/skills/ambrose-lean/docsection.py doc/TOOLS.md "How this list is used|^client "
```

A pattern is a case-insensitive regular expression matched against heading text. A matching section prints with everything under it down to the next heading of the same or a higher level. `--first` stops after one match, and `--depth 2` shortens the index.

CLAUDE.md asks you to read README.md, CONTRIBUTING.md and doc/ARCHITECTURE.md first. Read README.md and CONTRIBUTING.md whole, since they are small. For doc/ARCHITECTURE.md, read its index, then Repository layout, Layering, Methods and Conventions in full. Also read every Decisions entry that covers the subsystem you touch. Open other Decisions entries when a task reaches them.

## Search narrowly

- Start a search with file names only (Grep `files_with_matches`, or a Glob), then read matching lines with `-n` and a small `-C`. Read a file whole only when you will edit most of it.
- Read big source files with `offset` and `limit`, around the lines the search found.
- Don't re-read a file you just edited or a section already in context. After a compaction, re-read only the part you need.
- Hand a broad sweep across many folders to an Explore agent, which returns the conclusion rather than the file dumps. Do a single lookup yourself.

## Keep build and test output short

- Build and show only what failed:
  - Linux: `cmake --build --preset linux-gcc-debug 2>&1 | grep -E "error|FAILED" | head -40`
  - Windows (PowerShell): `cmake --build --preset windows-debug 2>&1 | Select-String -Pattern 'error|FAILED' | Select-Object -First 40`
- Run only the tests you touched. ctest test names are GoogleTest `Suite.Name`. For example, `ctest --preset linux-gcc-debug -R "MovementPacking" --output-on-failure 2>&1 | tail -30`.
- Run the full `ctest` once, before the commit that lands, not after every edit, and into a log: `ctest --preset linux-gcc-debug --output-on-failure > ctest.log 2>&1` as a background command, then `tail -40 ctest.log`. A tool's wait ending is not CTest timing out; read the log rather than starting the suite again, and rerun only what failed with `--rerun-failed`.
- Send a long run's output to a log file in the background, then read the tail and grep it for failures. Don't stream it.
- `python apps/ci/ci_local.py` runs every check CI's checks job runs. On success, read only its last lines.

## Ask the tools short questions

- Ask the repo's tools the narrowest question: `client types --list <text>` for names before full classes, `client wad --list <pattern>` before an entry, and a specific address rather than a range.
- Write a sweep's output (a bindecode sweep, a zone extraction) to a file, and read a summary or `head` of it.
- Tools that print JSON can be filtered through Python or `jq` down to the fields you need.

## Write short

- Put the evidence in the commit and the phase file, where it lives anyway. Keep messages to the result.
- Don't paste diffs, logs or whole tables into a reply. Quote the one line that proves the point.

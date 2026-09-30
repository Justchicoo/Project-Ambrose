---
name: ambrose-client-decode
description: Use when a task needs to know what the Wizard101 client holds or does, such as classes, properties, messages, archive entries, templates, locale text, zone data or the client program's code. Sends each question to the repo's own tool and says how to extend a tool that cannot answer it.
---
<!-- Project Ambrose by Imjustchico: Which of the repository's tools answers each question about the user's own client install, and how to teach a tool a question it cannot answer yet. -->

# Decoding the client with the repo's tools

Reverse engineering here means asking the user's own install through the repo's tools. doc/TOOLS.md is the source of truth. Read a tool's entry there with `python .claude/skills/ambrose-lean/docsection.py doc/TOOLS.md "^<tool> "` before relying on it, and run `<tool> --help` for its current options.

## Rules that do not bend

- Nothing from the client, or generated from it, is ever committed: no archive, entry, asset, type dump, message XML, capture, decompiled listing or run of bytes. That covers code, tests, fixtures, docs and skills. Tools read the install at run time, and their caches live in the Ambrose data folder.
- Names, hashes, counts and what the game does may be written down as findings. A finding states what the game does, with the tool run that showed it.
- Never copy, translate or port another emulator's code or data. Study them only for behavior.
- No one-off scripts, hard-coded offsets or hand-written parsers. If the tools can't answer, teach the tool (see the last section).

## Which tool answers which question

Tools build into `build/<configure preset>/bin/<config>/`, for example `build/linux-gcc/bin/Debug/client`. They find the install through `--client` or `AMBROSE_CLIENT_DIR`, and the type dump through `--type-dump` or `AMBROSE_TYPE_DUMP_PATH`.

`client` (src/tools/client) is the one tool that client questions are asked of. It is also the one that grows.

| Question | Ask |
|---|---|
| A class, its bases, properties, offsets, hashes and enum options | `client types <name or hash>`, or `client types --list <text>` for names only |
| The name of a property hash | `client name <hash>` |
| What a message carries, and its service and order | `client messages <tag>`, or `client messages --list [text]` |
| Which client classes handle a message | `client handlers <tag>` |
| An object template by id | `client template <id>`, or `client template --list [text]` |
| An archive entry, as JSON for BINd and headerless objects | `client wad <entry>`, or `client wad --list [pattern]` |
| Locale text | `client lang <key>`, or `client lang --list [text]`. `localetool find/check/dump/locales` for whole tables |
| A game object blob from a capture (MSG_LOGINCOMPLETE, MSG_NEWOBJECT) | `client core <file>`, with `--as`, `--core-type`, `--flags` or `--mask` when the class is unknown |
| Raw bytes of a file | `client hex <file> --from <offset> --count <n>` |
| Strings in the client program, and who reads them | `client strings <text>` |
| Who calls, reads or jumps to an address | `client xrefs <address or text>` |
| A function's code | `client disasm <address or text>`, `client decompile <address>` (uses the user's own Ghidra), `client functions <log name>` |
| A virtual table, slot by slot | `client vtable <address>` |
| The class the client builds for a behavior | `client behaviors <name>` |
| Every BINd entry of an archive, or what fails to decode | `bindecode --wad <file> --list`, `--sweep`, or `--text` for XML and other text entries |
| Objects of a class the dump doesn't know | `schemaprobe` names classes from the program's strings, and property hashes through the property oracle |
| World rows the servers load (names, levels, zones, server classes) | `extractor names/levels/zones/classes`, with `--dry-run` to check counts first |
| The type dump itself | `typeextract` (emulates the client, never launches it), then `typeregbuild` for the binary cache |
| What the real client does on screen or on the wire | `python apps/clientdriver/drive.py run` with a scenario. It needs the install and runs on the owner's PC |

## Keep answers small

Ask the narrowest question. Use `--list` before the full print, and one address rather than a range. Send sweeps and extractions to a file, then read the counts and failures from it. Report the one line that proves the point.

## Disassemblers beside the tools

A session that has the radare2 or Ghidra MCP servers may use them to explore the client program when `client strings`, `xrefs`, `disasm` or `decompile` can't yet answer. They are for exploration only. Once a question has been answered by hand, teach it to `client`, so the next session gets the answer in one call. Nothing a disassembler prints is committed.

## When no tool can answer

1. Find the tool whose job it is. That is usually `client`, as a new subcommand or flag. Check src/tools and apps first, because doc/TOOLS.md may be behind.
2. Add the function there, reusing the shared decoders in src/server/shared and src/common. Keep the layering: tools depend only on database, shared and common.
3. Add a unit test in src/test/ mirroring the tool's folder. A test that needs the install carries the CTest label `client` and skips without `AMBROSE_CLIENT_DIR`.
4. Update the tool's `--help` and its doc/TOOLS.md entry, so the next session calls it instead of rediscovering it.
5. Say in the commit what the suite can now decode that it couldn't before.

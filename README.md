<!-- Project Ambrose by Imjustchico: Project overview, status, ground rules, and disclaimer. -->
# Project Ambrose

An experimental Wizard101 server written from scratch in C++, built by AI agents under human direction.

The experiment is simple: see how far AI-driven development can take a complete game server. Humans set direction and review; AI agents write the code.

## Status

Pre-alpha. The repository layout is in place, and there is nothing to build or run yet.

The full plan lives in [doc/ROADMAP.md](doc/ROADMAP.md): 16 phases and 271 milestones, each ending in something visible in the real client. The tool suite is in [doc/TOOLS.md](doc/TOOLS.md) and the client strategy in [doc/CLIENT.md](doc/CLIENT.md).

## Ground rules

- The server is written in C++.
- The structure and development methods follow AzerothCore. See [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).
- Everything is written from scratch. Other projects may be studied, but no code is copied, translated, or ported from them, and none of their data files are committed here.
- No files extracted from the game client are committed. Tools read game data from a user's own installation at runtime.
- AI contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Disclaimer

Project Ambrose is a fan-made research project. It is not affiliated with, endorsed by, or connected to KingsIsle Entertainment. Wizard101 is a trademark of KingsIsle Entertainment, Inc.

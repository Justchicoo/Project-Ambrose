<!-- Project Ambrose by Imjustchico: Project overview, status, building, contributing, community, ground rules, licence and disclaimer. -->
<div align="center">

<img src=".github/banner.svg" alt="Project Ambrose: a Wizard101 server written from scratch in C++20, built by AI agents under human direction" width="100%">

[![License](https://img.shields.io/badge/license-MIT-E4B457?style=for-the-badge&labelColor=0B1020)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-5FD3C4?style=for-the-badge&logo=cplusplus&logoColor=white&labelColor=0B1020)](doc/ARCHITECTURE.md)
[![Svelte](https://img.shields.io/badge/panel-Svelte%205-E2725B?style=for-the-badge&logo=svelte&logoColor=white&labelColor=0B1020)](doc/UI-STACK.md)
[![Discord](https://img.shields.io/badge/Discord-join%20us-C77DFF?style=for-the-badge&logo=discord&logoColor=white&labelColor=0B1020)](https://discord.gg/Dx6ACDUj6N)
[![Reddit](https://img.shields.io/badge/Reddit-r%2FProjectAmbrose-FF4500?style=for-the-badge&logo=reddit&logoColor=white&labelColor=0B1020)](https://www.reddit.com/r/ProjectAmbrose/)

[![Build](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/core-build.yml/badge.svg)](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/core-build.yml)
[![Front end](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/front-end.yml/badge.svg)](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/front-end.yml)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-8798BC?labelColor=131B31)](#building)
[![Progress](https://img.shields.io/endpoint?url=https%3A%2F%2Fraw.githubusercontent.com%2FJustchicoo%2FProject-Ambrose%2Fmain%2Fdoc%2Fprogress%2Fbadge.json)](doc/progress/)
[![Contributor track](https://img.shields.io/badge/open%20items-57-5FD3C4?labelColor=131B31)](doc/CONTRIBUTOR-TRACK.md)

**[Roadmap](doc/ROADMAP.md)** &nbsp;·&nbsp; **[Architecture](doc/ARCHITECTURE.md)** &nbsp;·&nbsp; **[Panel](doc/PANEL.md)** &nbsp;·&nbsp; **[Work board](https://justchicoo.github.io/Project-Ambrose/)** &nbsp;·&nbsp; **[Contribute](doc/CONTRIBUTOR-TRACK.md)** &nbsp;·&nbsp; **[Milestones](doc/MILESTONE-TRACK.md)** &nbsp;·&nbsp; **[Start with your AI](contrib/AI-MILESTONES-HERE.md)** &nbsp;·&nbsp; **[Discord](https://discord.gg/Dx6ACDUj6N)**

</div>

---

The experiment is simple: see how far AI-driven development can take a complete game server. Humans set direction and review; AI agents write the code. Nothing here is copied from another emulator, and nothing extracted from the game client is ever committed.

> [!NOTE]
> **Pre-alpha.** Press Play in Ambrose's own launcher and a real Wizard101 client signs in, creates, picks or deletes a wizard and enters the world: it stands in the Commons with the zone's objects and NPCs around it, walks, sees other players and chats with them, quits back to character select, and logs out where it stood. Teleports and changing zones are being finished, quests, gear and combat are being built alongside them, and each server shows itself live in its own web panel.

## Where the project is

<div align="center">

<img src="doc/progress/progress.svg" alt="Project Ambrose progress: milestones finished, acceptance checks passed and the progress of each phase" width="100%">

</div>

The card is generated from the roadmap itself by `apps/progress/progress.py`, so it cannot drift from what is built: a milestone counts when every one of its acceptance checks is ticked. [doc/progress/progress.json](doc/progress/progress.json) holds the same numbers for anything that wants to read them, and the panel and the Discord announcement both use it.

## What runs today

| Working | Not yet |
|---|---|
| Session handshake against a retail client, signing in, a wrong password and a retry | Teleports, doors and moving between zones |
| The character list, creating a wizard through the client's own screens, deleting one, and the school badge on its Badges page | Quests and NPC dialog |
| Picking a wizard and the handoff to the game server with the key it was issued, and quitting from the world back to character select without the password | Combat, the backpack and gear |
| Entering the world: a wizard stands in its zone with every object and NPC the zone's data places there | Pets, housing, crafting and minigames |
| Walking, with where the wizard stands kept and restored at the next login | Serving patches to a client |
| The wizard's level, experience, health, mana and gold from the database, shown on the client's HUD and character page | The installer's first full run on a clean Windows machine; it already takes a clean Ubuntu from a checkout to running servers |
| Other players in the same zone, say chat, quick chat and emotes | The panel's game data pages, hosting and backups, and two-factor sign-in |
| GM commands typed in chat, checked against the account's security level | |
| Logging out, link-dead and AFK handling, and a server shutdown that warns players and brings each wizard back where it stood | |
| Ambrose's own launcher: a window with a Play button that starts your own client, and the console launcher beside it | |
| The game layer's core: a world tick, scripts and modules that join the build by existing, command handling with security levels, and settings and data that reload live | |
| Reading your own installation: type extraction, a binary type cache that loads in a tenth of a second, archives, the object format byte for byte, every zone's templates, locations, objects, volumes and triggers, every object template, and the level and school tables | |
| Following whatever client revision your install has, KingsIsle's latest included: a running server notices an update, extracts it in the background and swaps it in live | |
| The admin API and a live log stream with secrets hidden, a supervisor that runs the servers and takes back the ones still running, sign-in links for a desktop program, and the panel's sign-in with roles and permissions, overview, servers, logs, console, configuration and live reload, database, resource graphs and error reports | |

[doc/ROADMAP.md](doc/ROADMAP.md)'s **Where we are** says exactly which milestones are done, and the [work board](https://justchicoo.github.io/Project-Ambrose/) says what is being built right now and by whom. The plan runs in **17 phases**, each milestone ending in something visible in the real client or the panel.

121 of the 469 milestones are finished. Phases 1 and 2 are complete, phases 3 to 5 are down to their last few milestones, and phase 6 is under way, with quests, a wizard's gear and combat starting alongside it. Outside contributors have landed work on seventeen milestones and have four more part built. What the board shows is generated from the roadmap and the open pull requests every time either changes, so it says what is true rather than what was true.

## Building

> [!TIP]
> You need CMake 3.25+, vcpkg with `VCPKG_ROOT` pointing at it, and a C++20 compiler: Visual Studio 2022+ on Windows or GCC 13+ on Linux. vcpkg installs every library on the first configure, which takes a while exactly once.
>
> Or let the installer take a fresh checkout to running servers one step at a time, dependencies, database and all: `apps/installer/ambrose.sh` on Ubuntu 24.04 and `apps/installer/ambrose.ps1` on Windows 11, walked through in [doc/INSTALL.md](doc/INSTALL.md).

<details open>
<summary><b>Windows</b></summary>

```bat
cmake --preset windows-msvc-x64
cmake --build --preset windows-debug
cd build\windows-msvc-x64\bin\Debug
copy gameserver.conf.dist gameserver.conf
gameserver.exe
```

</details>

<details>
<summary><b>Linux</b></summary>

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
cd build/linux-gcc/bin/Debug
cp gameserver.conf.dist gameserver.conf
./gameserver
```

</details>

The build copies `gameserver.conf.dist` next to the executable. The server reads `gameserver.conf` from the folder it runs in, logs to the console and to `logs/Server.log`, and exits with an error naming the missing file if there is none. Options are described in [doc/config/gameserver.md](doc/config/gameserver.md) and [doc/config/logging.md](doc/config/logging.md).

<details>
<summary><b>Tests, sanitizers and release builds</b></summary>

Run the unit tests with the test preset matching your build, for example `ctest --preset windows-debug` or `ctest --preset linux-gcc-debug`. Configure with `-DBUILD_TESTING=OFF` to skip the tests and their dependencies. Use `windows-release` or `linux-gcc-release` for optimized builds.

On Linux, `linux-gcc-asan` builds and tests with AddressSanitizer and UndefinedBehaviorSanitizer, `linux-clang-tsan` does the same with ThreadSanitizer, and `linux-clang-fuzz` builds the libFuzzer targets for decoders of untrusted data and runs each from its seed corpus. Each needs the matching compiler installed, and ThreadSanitizer may need `sudo sysctl vm.mmap_rnd_bits=28` on newer kernels.

`python apps/ci/ci_build.py` runs the same legs CI does, with the tests spread over every core and the compiler cached across clones when ccache is installed.

Hosted CI builds the Linux GCC leg on every push to `main` that touches the code and on every milestone branch, the Windows leg on Sundays and Wednesdays, the sanitizer legs on Sundays, and the Clang and fuzz legs on the first Sunday of each month, each scheduled leg only when code changed since its last build. Any leg can also be built on demand; see Continuous integration in [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).

</details>

## Contributing

There are two doors, and every pull request through either is verified by running it, not by reading it.

The **contributor track** has its own folders and cannot collide with a milestone in flight: findings, tools, schemas, fixtures, guides and proposals. The **milestone track** is the roadmap itself: every milestone is open to anyone, in any order and without asking, with the source tree and the acceptance checks that come with them.

| | |
|---|---|
| **57 open items** | [doc/CONTRIBUTOR-TRACK.md](doc/CONTRIBUTOR-TRACK.md) - findings about the game, scenarios that turn a real-client check into one command, and tools, fixtures, guides and proposals, each with what it needs and how it is proven |
| **Start in one paste** | [contrib/AI-MILESTONES-HERE.md](contrib/AI-MILESTONES-HERE.md) - the one prompt for any AI assistant, for any milestone or track item, with everything it needs to work here without guessing |
| **What is free to take** | [The work board](https://justchicoo.github.io/Project-Ambrose/) - generated from the roadmap and the open pull requests: what is being built right now, by whom, and which milestones are ready to start, though every one is open to anyone. Its [state.json](https://justchicoo.github.io/Project-Ambrose/state.json) is the same thing for your AI |
| **Build a milestone** | [doc/MILESTONE-TRACK.md](doc/MILESTONE-TRACK.md) - the rules behind the board, and [contrib/AI-MILESTONES-HERE.md](contrib/AI-MILESTONES-HERE.md), the prompt that goes with them |
| **The shape of a finding** | [contrib/findings/README.md](contrib/findings/README.md) - one claim about how the game behaves, written so it can be proven or refuted |
| **House rules** | [CONTRIBUTING.md](CONTRIBUTING.md) - AI-written changes are expected, not merely allowed |

The most valuable thing anyone can add is a **proven finding**: Ambrose is built only on data it has re-derived itself, and every finding is verified or refuted before a milestone is built on it. A finding that turns out false is still worth sending, because it stops the next person chasing it.

## Community

| | |
|---|---|
| **Discord** | [discord.gg/Dx6ACDUj6N](https://discord.gg/Dx6ACDUj6N) - questions as they come up, and every merge as it lands |
| **Reddit** | [r/ProjectAmbrose](https://www.reddit.com/r/ProjectAmbrose/) - longer threads and announcements |
| **Bugs and ideas** | [Issues](https://github.com/Justchicoo/Project-Ambrose/issues/new/choose) - a form for each |
| **Security** | [SECURITY.md](SECURITY.md) - report privately, never in a public issue |
| **Conduct** | [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) - one standard in the repository, on Discord and on Reddit |

## Ground rules

- **From scratch.** Other projects may be studied for structure and behaviour. No code is copied, translated or ported, and none of their data files are committed.
- **No game files, ever.** Tools read game data from a user's own installation at runtime. Nothing extracted from the client enters this repository.
- **AzerothCore's shape.** The structure and development methods follow it; see [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).
- **One look everywhere.** [doc/DESIGN.md](doc/DESIGN.md) sets the palette, type and motion for every surface, down to the colour of a log line; [doc/UI-STACK.md](doc/UI-STACK.md) is what they are built with.
- **Proven, or it does not ship.** Every claim says how it was checked and what would prove it wrong.

## Licence

MIT ([LICENSE](LICENSE)). The licence covers the code and documents in this repository. It does not cover Wizard101 or any KingsIsle Entertainment property: no game file, asset or text is included here, and every tool reads a user's own installation at run time. [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) lists the libraries this software uses and what each one requires, including the two that ask for more than attribution.

---

<div align="center">

**Project Ambrose is a fan-made research project.**

It is not affiliated with, endorsed by, or connected to KingsIsle Entertainment.<br>
Wizard101 is a trademark of KingsIsle Entertainment, Inc.

[![Discord](https://img.shields.io/badge/join%20the%20Discord-Dx6ACDUj6N-C77DFF?style=for-the-badge&logo=discord&logoColor=white&labelColor=0B1020)](https://discord.gg/Dx6ACDUj6N)

</div>

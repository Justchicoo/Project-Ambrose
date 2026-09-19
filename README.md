<!-- Project Ambrose by Imjustchico: Project overview, status, ground rules, and disclaimer. -->
<div align="center">

# Project Ambrose

**A Wizard101 server written from scratch in C++20, built by AI agents under human direction.**

[![License](https://img.shields.io/badge/license-MIT-E4B457?style=for-the-badge&labelColor=0B1020)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-5FD3C4?style=for-the-badge&logo=cplusplus&logoColor=white&labelColor=0B1020)](doc/ARCHITECTURE.md)
[![Discord](https://img.shields.io/badge/Discord-join%20us-C77DFF?style=for-the-badge&logo=discord&logoColor=white&labelColor=0B1020)](https://discord.gg/Dx6ACDUj6N)
[![Milestones](https://img.shields.io/badge/roadmap-386%20milestones-E2725B?style=for-the-badge&labelColor=0B1020)](doc/ROADMAP.md)

[![Build](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/core-build.yml/badge.svg)](https://github.com/Justchicoo/Project-Ambrose/actions/workflows/core-build.yml)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-8798BC?labelColor=131B31)](#building)
[![Contributor track](https://img.shields.io/badge/open%20items-87-5FD3C4?labelColor=131B31)](doc/CONTRIBUTOR-TRACK.md)

[Roadmap](doc/ROADMAP.md) &nbsp;.&nbsp; [Architecture](doc/ARCHITECTURE.md) &nbsp;.&nbsp; [Contribute](doc/CONTRIBUTOR-TRACK.md) &nbsp;.&nbsp; [Start with your AI](contrib/AI-START-HERE.md) &nbsp;.&nbsp; [Discord](https://discord.gg/Dx6ACDUj6N)

</div>

---

The experiment is simple: see how far AI-driven development can take a complete game server. Humans set direction and review; AI agents write the code. Nothing here is copied from another emulator, and nothing extracted from the game client is ever committed.

> [!NOTE]
> **Pre-alpha.** A real Wizard101 client, started by Ambrose's own launcher, talks to the servers as far as character select. It stops there.

## What runs today

| Working | Not yet |
|---|---|
| Session handshake against a retail client | World, zones and movement |
| Signing in, a wrong password, and a retry | Quests, combat, pets, housing |
| The character list, and the shutdown notice | Character creation end to end |
| Ambrose's own launcher starting the client | The operations panel |
| Type extraction from your own installation | Patch serving |

[doc/ROADMAP.md](doc/ROADMAP.md)'s **Where we are** says exactly which milestones are done. The full plan is **17 phases and 386 milestones**, each ending in something visible in the real client.

## Building

> [!TIP]
> You need CMake 3.25+, vcpkg with `VCPKG_ROOT` pointing at it, and a C++20 compiler: Visual Studio 2022+ on Windows or GCC 13+ on Linux. vcpkg installs every library on the first configure, which takes a while exactly once.

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

Hosted CI builds the Windows leg on Sundays and Wednesdays, the sanitizer legs on Sundays, and the GCC, Clang and fuzz legs on the first Sunday of each month, each only when code changed since its last build. Any leg can also be built on demand; see Continuous integration in [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).

</details>

## Contributing

The phases are built in order by the maintainer's own agents, so nobody else works from them. Everything from outside lands on a **separate track** that cannot collide with a milestone in flight.

| | |
|---|---|
| **87 open items** | [doc/CONTRIBUTOR-TRACK.md](doc/CONTRIBUTOR-TRACK.md) - F-01 to F-40 for proven findings, C-01 to C-55 for tools, data, guides and proposals |
| **Start in one paste** | [contrib/AI-START-HERE.md](contrib/AI-START-HERE.md) - a prompt for any AI assistant, with everything it needs to work here without guessing |
| **The shape of a finding** | [contrib/findings/README.md](contrib/findings/README.md) - one claim about how the game behaves, written so it can be proven or refuted |
| **House rules** | [CONTRIBUTING.md](CONTRIBUTING.md) - AI-written changes are expected, not merely allowed |
| **Ask anything** | [Discord](https://discord.gg/Dx6ACDUj6N) |

The most valuable thing anyone can add is a **proven finding**: Ambrose is built only on data it has re-derived itself, and every finding is verified or refuted before a milestone is built on it. A finding that turns out false is still worth sending, because it stops the next person chasing it.

## Ground rules

- **From scratch.** Other projects may be studied for structure and behaviour. No code is copied, translated or ported, and none of their data files are committed.
- **No game files, ever.** Tools read game data from a user's own installation at runtime. Nothing extracted from the client enters this repository.
- **AzerothCore's shape.** The structure and development methods follow it; see [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).
- **One look everywhere.** [doc/DESIGN.md](doc/DESIGN.md) sets the palette, type and motion for every surface, down to the colour of a log line; [doc/UI-STACK.md](doc/UI-STACK.md) is what they are built with.
- **Proven, or it does not ship.** Every claim says how it was checked and what would prove it wrong.

## Licence

MIT ([LICENSE](LICENSE)). [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) lists every library and what each licence asks.

---

<div align="center">

**Project Ambrose is a fan-made research project.**

It is not affiliated with, endorsed by, or connected to KingsIsle Entertainment.<br>
Wizard101 is a trademark of KingsIsle Entertainment, Inc.

[![Discord](https://img.shields.io/badge/join%20the%20Discord-Dx6ACDUj6N-C77DFF?style=for-the-badge&logo=discord&logoColor=white&labelColor=0B1020)](https://discord.gg/Dx6ACDUj6N)

</div>

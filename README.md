<!-- Project Ambrose by Imjustchico: Project overview, status, ground rules, and disclaimer. -->
# Project Ambrose

An experimental Wizard101 server written from scratch in C++, built by AI agents under human direction.

The experiment is simple: see how far AI-driven development can take a complete game server. Humans set direction and review; AI agents write the code.

## Status

Pre-alpha. The repository layout is in place, and there is nothing to build or run yet.

The full plan lives in [doc/ROADMAP.md](doc/ROADMAP.md): 16 phases and 271 milestones, each ending in something visible in the real client. The tool suite is in [doc/TOOLS.md](doc/TOOLS.md) and the client strategy in [doc/CLIENT.md](doc/CLIENT.md).

## Building

You need CMake 3.25 or newer, vcpkg with the `VCPKG_ROOT` environment variable pointing at it, and a C++20 compiler: Visual Studio 2022 or newer on Windows, or GCC 13 or newer on Linux. vcpkg installs every library automatically on the first configure.

Windows:

```
cmake --preset windows-msvc-x64
cmake --build --preset windows-debug
build\windows-msvc-x64\bin\Debug\gameserver.exe
```

Linux:

```
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
./build/linux-gcc/bin/Debug/gameserver
```

Use the `windows-release` or `linux-gcc-release` build presets for optimized builds.

Run the unit tests with the test preset that matches your build, for example `ctest --preset windows-debug` or `ctest --preset linux-gcc-debug`. Configure with `-DBUILD_TESTING=OFF` to skip the tests and their dependencies.

## Ground rules

- The server is written in C++.
- The structure and development methods follow AzerothCore. See [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md).
- Everything is written from scratch. Other projects may be studied, but no code is copied, translated, or ported from them, and none of their data files are committed here.
- No files extracted from the game client are committed. Tools read game data from a user's own installation at runtime.
- AI contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Disclaimer

Project Ambrose is a fan-made research project. It is not affiliated with, endorsed by, or connected to KingsIsle Entertainment. Wizard101 is a trademark of KingsIsle Entertainment, Inc.

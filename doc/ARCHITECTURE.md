<!-- Project Ambrose by Imjustchico: Repository layout, layering rules, and development methods. -->
# Architecture

Project Ambrose follows the structure and methods of AzerothCore, the open-source World of Warcraft server emulator, adapted to Wizard101. The layout and patterns are borrowed. No AzerothCore code is.

## Repository layout

```
apps/                     Repository tooling and operator apps: CI, code style checks, installer, dashboard, Grafana
conf/dist/                Build and environment config templates
data/sql/
  base/db_<name>/         Full schema snapshot per database
  updates/db_<name>/      Dated, ordered updates per database
  updates/pending_db_*/   Updates from open pull requests
  custom/db_<name>/       Local-only SQL, never upstreamed
deps/                     vcpkg overlay ports and triplets, when needed
doc/                      Project documentation
modules/                  Drop-in modules, discovered by CMake
src/
  cmake/                  CMake macros, compiler flags, platform detection
  genrev/                 Generated git revision header
  common/                 Game-agnostic foundations
  server/
    apps/                 Executables: loginserver, gameserver, patchserver
    database/             Connection pools, prepared statements, updater
    shared/               Code every server uses: network, messages, ObjectProperty, archives, realms
    game/                 Game systems, one folder per subsystem
    scripts/              Content scripts grouped by world, plus Commands and Custom
  tools/                  Extractors that read a user's own client install, and dbimport
  test/                   Unit tests mirroring src/
```

## Layering

Each layer depends only on the layers to its right.

```
apps -> scripts -> game -> database -> shared -> common -> deps
```

Tools depend only on `database`, `shared`, and `common`. Modules depend on `game` and `scripts`.

## Processes

| App | Role |
|---|---|
| loginserver | Account authentication, character list and creation, realm selection |
| gameserver | One realm: zones, entities, combat, quests, chat |
| patchserver | Serves client revision files |

## Methods

### Content is data

Templates, spawns, quests, quest givers, loot, and vendor lists live in the world database, not in code. The game server loads them into global managers at startup, and GM commands reload single tables without a restart. Code implements only the behavior data cannot express.

### Databases and updates

There are three databases: `login`, `characters`, and `world`. Every change is a new file in `data/sql/updates/db_<name>/` named `YYYY_MM_DD_NN.sql`. At startup the updater applies unapplied files in order and records each one in an `updates` table. Open pull requests put their files in `pending_db_<name>/`, and they move into `updates/` when merged. `base/` is regenerated periodically by squashing old updates.

### Message handlers

Each client message is registered once in a dispatch table with four parts: the message, the session state it requires (never, authenticated, logged in, in world), its processing mode, and the `Session::Handle<Message>` member that handles it. Handlers are grouped by subsystem in `game/Handlers/<Subsystem>Handler.cpp`.

### Scripting

`ScriptMgr` exposes hook classes such as `WorldScript`, `PlayerScript`, `NpcScript`, `QuestScript`, `ZoneScript`, and `CommandScript`. Each script file defines its classes and one `AddSC_<name>()` function, and its folder's script loader calls that function. Content scripts are grouped by world, for example `scripts/WizardCity/`.

### GM commands

Each command group is one file, `scripts/Commands/cs_<group>.cpp`, holding a `CommandScript` with a command table and the account security level each command requires.

### Modules

A module is a folder in `modules/` with its own `src/`, `conf/`, and `data/sql/`. CMake discovers it and registers its scripts through a generated loader, so a module never edits core files.

### Configuration

Each app ships `<app>.conf.dist` listing every option with its default. Users copy it to `<app>.conf`, which git ignores.

### Client data

Nothing from the game client is committed. Tools in `src/tools/` read the user's own installation and produce the files and world database rows the servers load.

### Operations

Servers stay headless so they run the same on a desktop, a Linux VPS, or in Docker. Each app writes colored logs and accepts commands on its console. An optional admin API, bound to localhost and protected by a token, serves health, status, live logs, audited commands, and Prometheus metrics. The web dashboard in `apps/dashboard/` and the Grafana dashboards in `apps/grafana/` are built on that API. Phase 17 of doc/ROADMAP.md plans this work.

### Tests

`src/test/` mirrors `src/` and builds a single unit test executable.

## Conventions

### File header

Every file starts with the Project Ambrose branding header and a one-line brief of what the file holds and does. There are no other comments anywhere. Formats that cannot contain comments, such as JSON, are exempt.

| File type | Header |
|---|---|
| C and C++ | `/*` then ` * Project Ambrose by Imjustchico` then ` * <brief>` then ` */` |
| CMake, shell, PowerShell, Python, YAML, conf, git and editor config | `# Project Ambrose by Imjustchico` then `# <brief>` |
| SQL | `-- Project Ambrose by Imjustchico` then `-- <brief>` |
| Batch | `REM Project Ambrose by Imjustchico` then `REM <brief>` |
| Markdown | `<!-- Project Ambrose by Imjustchico: <brief> -->` |

C++ example:

```cpp
/*
 * Project Ambrose by Imjustchico
 * Quest template storage and lookup by id.
 */
```

### Code

- Files and classes use PascalCase, with one primary class per `.h` and `.cpp` pair.
- Include guards use `AMBROSE_<FILE>_H`.
- Global managers are singletons accessed through an `s<Name>` macro, such as `sObjectMgr`, `sScriptMgr`, and `sWorld`.

## Decisions

Settled on 2026-09-13. Changing one needs the maintainer's approval and an update to this section.

### Stack

| Area | Choice |
|---|---|
| Language | C++20 for all server code |
| Build | CMake 3.25 or newer with CMakePresets: the newest installed Visual Studio generator on Windows (2022 or newer), Ninja Multi-Config on Linux |
| Dependencies | vcpkg manifest mode (`vcpkg.json` with a pinned `builtin-baseline`). No third-party source is committed; `deps/` holds only vcpkg overlay ports and triplets when one is needed |
| Formatting | fmt |
| Networking | Standalone Asio (no Boost), with C++20 coroutines |
| Unit tests | GoogleTest and GoogleMock |
| XML | pugixml |
| JSON | nlohmann-json |
| Compression | zlib |
| Cryptography | Botan 3, covering SHA-2, Twofish, and the random number generator. The client's non-standard CRC-32 is implemented in `common` |
| Database server | MySQL 8.0 or newer, or MariaDB 10.6 or newer |
| Database client | MariaDB Connector/C, which works with both servers |

### Protocol and type data load at runtime

The client's message definitions and type dump are never compiled into the build. At startup each app loads the message definition XML files from the user's install into a `MessageRegistry`, and the type dump into a `TypeRegistry`. Code that uses a message or class declares only the fields it needs, with their C++ types. At startup every declaration is resolved to field indices and checked against the loaded definitions, and the app refuses to start if any declaration is wrong. Wire layout always comes from the loaded definitions, so fields a declaration omits are still encoded correctly with default values.

As a result, the project builds and its unit tests run on any machine, including CI, with no client files. Unit tests use small definition fixtures written by the project. Tests that need a real install carry the CTest label `client` and run only when `AMBROSE_CLIENT_DIR` is set.

### Database updates

Update files run through the connector with multi-statement support, so `DELIMITER` is not allowed in them. The `updates` table records each file's SHA-256 hash.

### Tools

Server code is C++. Tools may use whatever language does the job best, and they must work reliably. A tool that reuses server code, such as the archive reader or the ObjectProperty codec, lives in `src/tools/` in C++. Repository tooling such as the codestyle checker, CI scripts, and the installer lives in `apps/` and may be Python or shell. Every tool file carries the branding header.

### Configuration

A `.conf.dist` file contains only its branding header and `Key = value` lines. Each option is documented in `doc/config/<app>.md`. Layers apply in the order `<app>.conf.dist`, `conf.d/*.conf.dist`, `<app>.conf`, `conf.d/*.conf`, `AMBROSE_` environment variables, then command-line overrides, so every default sits below every local edit. Environment variable names follow the rule in doc/config/README.md, for example `WorldServerPort` becomes `AMBROSE_WORLD_SERVER_PORT`.

### C++ modules

Settled on 2026-09-13. The code uses headers, not C++20 modules, and CMake's module scanning is turned off. A trial build of a named module worked on MSVC, Clang 18, and GCC 14, but the main benefit, `import std`, is still experimental in CMake, works only with Ninja generators and not the Visual Studio generator, and needs GCC 15. Modules also cannot export macros such as `LOG_INFO` and `sLog`, the vcpkg libraries are headers, and editor and lint tooling for modules is weaker. Revisit when `import std` is no longer experimental in CMake, the Visual Studio generator supports it, and the CI images ship GCC 15 with working module metadata.

### Operations

Settled on 2026-09-13 with the maintainer's direction to favor the most capable option.

| Area | Choice |
|---|---|
| Admin API server | Crow on the standalone Asio layer, serving HTTP and WebSocket from one library |
| Dashboard front end | TypeScript and Svelte, built by Vite into static files the admin API can serve |
| Process control | An Ambrose supervisor process that starts, stops, restarts, and crash-restarts every app on Windows and Linux, and can itself run under systemd or as a Windows service |
| Remote access | The admin API listens on localhost by default. Any other address requires TLS and the token, and plain HTTP is never exposed beyond the machine |

### Still open

Decisions that block later milestones are listed under Decisions needed in doc/ROADMAP.md. Propose them to the maintainer when their milestone is next.

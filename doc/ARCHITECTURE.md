<!-- Project Ambrose by Imjustchico: Repository layout, layering rules, and development methods. -->
# Architecture

Project Ambrose follows the structure and methods of AzerothCore, the open-source World of Warcraft server emulator, adapted to Wizard101. The layout and patterns are borrowed. No AzerothCore code is.

## Repository layout

```
apps/                     Repository tooling: CI, code style checks, installer
conf/dist/                Build and environment config templates
data/sql/
  base/db_<name>/         Full schema snapshot per database
  updates/db_<name>/      Dated, ordered updates per database
  updates/pending_db_*/   Updates from open pull requests
  custom/db_<name>/       Local-only SQL, never upstreamed
deps/                     Vendored third-party libraries
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

Tools depend only on `shared` and `common`. Modules depend on `game` and `scripts`.

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

## Pending decisions

These follow AzerothCore's precedent unless the maintainer decides otherwise.

| Decision | AzerothCore precedent |
|---|---|
| C++ standard | C++20 |
| Build system | CMake |
| Database | MySQL or MariaDB |
| Networking | Boost.Asio |
| String formatting | fmt |
| Unit tests | GoogleTest |

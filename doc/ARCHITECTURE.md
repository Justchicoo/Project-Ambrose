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

Templates, spawns, quests, quest givers, loot, and vendor lists live in the world database, not in code. The game server loads them into global managers at startup, and every manager reloads live through the reload framework from GM commands, the console, or the admin API. Tables that reference each other reload and validate together as one snapshot. Code implements only the behavior data cannot express.

### Databases and updates

There are three databases: `login`, `characters`, and `world`. Every change is a new file in `data/sql/updates/db_<name>/` named `YYYY_MM_DD_NN.sql`. At startup the updater applies unapplied files in order and records each one in an `updates` table. The `db update` command and the admin API apply data-only updates live and then reload the affected managers; an update that changes a schema the running binary reads waits for the next binary upgrade. Open pull requests put their files in `pending_db_<name>/`, and they move into `updates/` when merged. `base/` is regenerated periodically by squashing old updates.

### Message handlers

Each app lists every client message once in a `MessageHandlerTable`: the message, the session statuses it is accepted in, its processing mode, and the `Session::Handle<Message>` member that handles it. A message can also be listed as not handled yet, with the statuses it will need, or as refused because only the server sends it. Handlers are grouped by subsystem in `game/Handlers/<Subsystem>Handler.cpp`.

### Scripting

`ScriptMgr` exposes hook classes such as `WorldScript`, `PlayerScript`, `NpcScript`, `QuestScript`, `ZoneScript`, and `CommandScript`. Each script file defines its classes and one `AddSC_<name>()` function, and its folder's script loader calls that function. Content scripts are grouped by world, for example `scripts/WizardCity/`.

### GM commands

Each command group is one file, `scripts/Commands/cs_<group>.cpp`, holding a `CommandScript` with a command table and the default account security level each command requires. A `command_security` table overrides levels live and reloads with the other command data.

### Modules

A module is a folder in `modules/` with its own `src/`, `conf/`, and `data/sql/`. CMake discovers it and registers its scripts through a generated loader, so a module never edits core files. Adding or removing a module needs a rebuild and restart; a module's own configuration and settings reload live.

### Configuration

Each app ships `<app>.conf.dist` listing every option with its default. Users copy it to `<app>.conf`, which git ignores. `reload config` on the console, `.reload config` in game, and the admin API re-read every layer and apply the changed options live.

### Client data

Nothing from the game client is committed. Tools in `src/tools/` read the user's own installation and produce the files and world database rows the servers load.

### Operations

Servers stay headless so they run the same on a desktop, a Linux VPS, or in Docker. Each app writes colored logs and accepts commands on its console. An optional admin API, bound to localhost and protected by a token, serves health, status, live logs, audited commands, live settings, reloads, and Prometheus metrics. The web dashboard in `apps/dashboard/` and the Grafana dashboards in `apps/grafana/` are built on that API. Phase 17 of doc/ROADMAP.md plans this work.

### Tests

`src/test/` mirrors `src/` and builds a single unit test executable. Database integration tests run only when `AMBROSE_TEST_DB` holds a connection string such as `127.0.0.1;3306;root;root;ambrose_test` for a disposable server, and skip otherwise. Each test creates uniquely named databases and drops them when it finishes, including the app smoke tests, which start the real executables with `--check`. The Linux CI legs run them against the runner's MySQL 8, and local runs can use MariaDB.

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
| Database client | MariaDB Connector/C, which works with both servers. Its authentication plugins ship beside each executable in `plugins/libmariadb` |

### Protocol and type data load at runtime

The client's message definitions and type dump are never compiled into the build. At startup each app loads the message definition XML files from the user's install into a `MessageRegistry`, and the type dump into a `TypeRegistry`. Code that uses a message or class declares only the fields it needs, with their C++ types. At startup every declaration is resolved to field indices and checked against the loaded definitions, and the app refuses to start if any declaration is wrong. A reload resolves every declaration against the new definitions before it swaps them in, and keeps the active definitions if any declaration fails. Wire layout always comes from the loaded definitions, so fields a declaration omits are still encoded correctly with default values.

As a result, the project builds and its unit tests run on any machine, including CI, with no client files. Unit tests use small definition fixtures written by the project. Tests that need a real install carry the CTest label `client` and run only when `AMBROSE_CLIENT_DIR` is set.

### Live reload and live settings

Settled on 2026-09-14 at the maintainer's direction. Anything that can change while a server runs does, without restarting a process. A subsystem that holds loaded state builds the new state off to the side, validates it completely, and swaps it in atomically, so threads in the middle of an operation keep a consistent snapshot. If anything fails, the old state stays active and every error is reported. Runtime limits apply from the next operation. Every gameplay value, such as respawn times, drop rates, experience and gold rates, and every other tunable number, is a typed setting with a default and bounds. Settings are changed live from the control center (the admin API and web dashboard), GM commands, or configuration reloads, and each change is validated, persisted, and written to an audit log. Message definitions reload this way today, and configuration and logging have reload functions that keep their old values on failure. The reload framework and its triggers arrive in milestone 4.15, the live settings registry in 4.16, and the control center pages in 17.12 and 17.13. Live settings persist in the database the app owns (`characters` for the game server, `login` for the login and patch servers) with an audit table. Environment variables and command-line overrides lock a key, and a live edit to a locked key is refused with a message naming the layer. Live world database edits from the control center are journaled and can be exported as a pending SQL update. A restart is required only where the operating system or the client forces one, such as replacing the server binary, and each such case is documented where it arises.

### Message definition quirks

Settled on 2026-09-14. A field whose type attribute is misspelled `TPYE` or `TYP` keeps that type, and a `GlobalID` field with no type is a GID. Each case is reported as a load warning, which the startup loader logs, and the field stays on the wire. This keeps MSG_PHYSICS_GRAB, MSG_MINIGAMEREWARDS, and MSG_BATTLEGROUNDQUEUEUPDATE at their fullest layout until capture verification shows the client drops those fields. A message's element tag is its identity and sort key, never `_MsgName`. A repeated tag merges into one id only when its fields match, and a repeat with different fields is an error.

### Sessions and keepalives

Settled on 2026-09-14. A session sends SessionOffer before any other work and closes on a SessionAccept or client keepalive that names another session id. DML frames that arrive before SessionAccept are queued, up to 256 frames or 1 MiB, and delivered in order after it. A KeepAliveRsp echoes the client's elapsed minutes and carries the server's milliseconds into the current second. A server keepalive closes the session only when nothing at all arrives from the client within `Network.KeepAliveTimeout`, so a client that never answers server keepalives but sends its own stays connected. Session ids are nonzero, handed out in rotating order, and reused only after their session closes. These choices hold until doc/CAPTURE.md records otherwise.

Work that needs the maintainer's own client, such as the real-client checks of 1.21 and 1.22, is listed in doc/ROADMAP.md under Where we are. Milestones that do not depend on those checks go ahead while they wait.

### Message dispatch and session states

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- Every app shares one `SessionStatus`: `Connected` once the handshake is done, `Authenticated` once credentials are verified, `CharacterSelected` once the login server hands the client to a realm or the game server has validated that hand-off, `LoggedIn` once LOGINCOMPLETE is sent, and `InWorld` once the character is in a zone. Each app uses the statuses it needs, and a rule accepts a mask of them.
- Message ids come from the client's definitions at runtime, so a table cannot be checked against them at build time. Rules name messages by service and tag instead. Handled messages are declared with the message registry, so a definition load that lacks one is refused. At startup every rule is also checked against the loaded definitions, and a rule for a message the definitions lack, a duplicated or status-less rule, a queued rule in an app that drains no queues, or any message of the app's own services without a rule stops the app. Each loaded catalog resolves the rules to service and order slots once, so a reload that renumbers messages routes them correctly.
- Dispatch follows AzerothCore's split between messages the server never accepts and messages it does not handle yet. A message listed as refused, a message from a service the app does not serve, an id the definitions do not have, or a body shorter than its definition counts a strike, and `Network.MaxStrikes` strikes close the session. A message in the wrong status, or one not handled yet, is dropped and logged without a strike, so a real client is not disconnected for sending something Ambrose has not implemented. Each session may drop only `Network.DroppedMessageBurst` messages, refilled at `Network.DroppedMessagesPerSecond`, while every drop is logged; beyond that a drop is not logged and counts a strike, so one client can neither fill the logs nor stay connected by flooding. Client-supplied text is escaped and cut to 64 bytes before it reaches a log line.
- A handled message runs in place on its network thread, or is queued on its session and run when the owner drains the queue, with the status checked again at that moment. A session holds at most 4096 queued messages and 4 MiB of their bodies, and a handler that throws closes its session. Queued work can still run after the socket closes, so the owner that drains a session's queue also runs its close cleanup on that thread, as AzerothCore's `WorldSession::Update` does.
- The login server's table lives in `apps/loginserver/Server`. The game server's table arrives with its sessions in milestone 4.01, and the patch server's with its TCP service in 16.04, both on the same `MessageHandlerTable`.

### Database pools

Settled on 2026-09-14. A pool serves every call from its current connection generation: sync connections leased one caller at a time, async workers on a shared queue, a keepalive pinger, and the statement table. A new generation opens, checks versions and prepares every statement before it is published, so opening, closing and live reconfiguration never block callers, and a failed reconfiguration keeps the current generation. A retired generation drains its queue for up to 30 seconds, then cancels what is left and settles every callback. A statement is retried after a reconnect only when it cannot have run: never inside a transaction, and a lost connection during a write is reported instead of retried. Transactions retry deadlocks, lock wait timeouts and connections lost before COMMIT for up to 60 seconds. Callbacks for async work run on whichever thread polls them, normally the app's update loop through an AsyncCallbackProcessor. A synchronous `Query` returns null for an empty result, as in AzerothCore, and `TryQuery` also reports whether the query failed, so callers that must tell a missing row from a failing database use it.

### Accounts and the console

Settled on 2026-09-14 under the maintainer's standing direction to decide and favor the most capable option.

- The client's ClientKey1 scheme needs the server to hold `base64(SHA-512(password))`, which is as good as the password. Ambrose stores it in `login.account.verifier` and encrypts it at rest when `Account.VerifierKeys` and `Account.VerifierActiveKey` are set: AES-256-GCM with a random nonce, bound to the lowercased username, with the key id stored on the row. Older keys stay listed until no row uses them. A row is sealed again with the active key whenever its password changes, and from milestone 2.14 also after each successful login. Keys listed without an active key stop startup, so encryption cannot be left half configured. With no keys at all, verifiers are stored unencrypted for development. `Account.AllowPlainVerifiers = 0` then refuses any account whose verifier is still unencrypted, so a verifier written straight into the database cannot be used. Access to the login database must be restricted either way.
- Account management lives in `src/server/game/Accounts` and builds as its own `accounts` library on top of `database`. The login server links only that library, and the game layer and its GM commands use the same code.
- Account security levels use AzerothCore's numbering: 0 player, 1 moderator, 2 game master, 3 administrator, 4 console. An account's own level lives in `login.account.security_level`, and the `account_access` table of milestone 4.02 adds per-realm levels layered over it. How they map to LOGINCOMPLETE IsCSR and Permissions is still open.
- Usernames are up to 32 ASCII letters, digits, `_`, `-` and `.`, with a configurable minimum length, unique regardless of case through an `ascii_general_ci` column. Passwords are UTF-8 without control characters, up to 128 bytes. Ban and account times are Unix seconds, and an `unbandate` of 0 never expires.
- Every app reads console commands from standard input on its own thread and runs each line on a second thread, so a command that waits on the database never blocks the io loop, its timers or its signals. A shutdown waits for the line in flight before the app closes its databases, and at most 256 lines wait in the queue. Arguments of sensitive commands never reach a log. A closed input leaves the server running, a stop interrupts a blocked read, and `Console.Enable = 0` starts no reader. The shared `ConsoleCommandTable` holds `help` and `shutdown` from `ServerApp` plus each app's own commands. Milestone 17.01 adds the prompt, line editing and colors on top of it, and 4.02's CommandMgr takes the table over.

### Database updates

Update files run through the connector with multi-statement support, so `DELIMITER` is not allowed in them. The `updates` table records each file's SHA-256 hash.

### Tools

Server code is C++. Tools may use whatever language does the job best, and they must work reliably. A tool that reuses server code, such as the archive reader or the ObjectProperty codec, lives in `src/tools/` in C++. Repository tooling such as the codestyle checker, CI scripts, and the installer lives in `apps/` and may be Python or shell. Every tool file carries the branding header.

### Configuration

A `.conf.dist` file contains only its branding header and `Key = value` lines. Each option is documented in `doc/config/<app>.md`. Layers apply in the order `<app>.conf.dist`, `conf.d/*.conf.dist`, `<app>.conf`, `conf.d/*.conf`, persisted live settings, `AMBROSE_` environment variables, then command-line overrides, so every default sits below every local edit and a live edit sits above the files. Environment variable names follow the rule in doc/config/README.md, for example `WorldServerPort` becomes `AMBROSE_WORLD_SERVER_PORT`.

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

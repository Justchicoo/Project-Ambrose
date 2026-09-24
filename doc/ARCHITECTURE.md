<!-- Project Ambrose by Imjustchico: Repository layout, layering rules, and development methods. -->
# Architecture

Project Ambrose follows the structure and methods of AzerothCore, the open-source World of Warcraft server emulator, adapted to Wizard101. The layout and patterns are borrowed. No AzerothCore code is.

## Repository layout

```
apps/                     Repository tooling and operator apps: CI, code style checks, installer, design tokens, dashboard, launcher window, Grafana
conf/dist/                Build and environment config templates
design/                   tokens.json, the one source of every design value
packages/ui/              @ambrose/ui: the shared components, the generated tokens and the motion rules
data/sql/
  base/db_<name>/         Full schema snapshot per database
  updates/db_<name>/      Dated, ordered updates per database
  updates/pending_db_*/   Updates from open pull requests
  custom/db_<name>/       Local-only SQL, never upstreamed
  panel/                  Dated updates for the supervisor's own store
deps/                     vcpkg overlay ports and triplets, when needed
doc/                      Project documentation
modules/                  Drop-in modules, discovered by CMake
src/
  cmake/                  CMake macros, compiler flags, platform detection
  genrev/                 Generated git revision header
  common/                 Game-agnostic foundations, including Design/ with the generated terminal tokens
  server/
    apps/                 Executables: loginserver, gameserver, patchserver, supervisor
    database/             Connection pools, prepared statements, updater
    shared/               Code every server uses: network, messages, ObjectProperty, archives, realms
    game/                 Game systems, one folder per subsystem
    scripts/              Content scripts grouped by world, plus Commands and Custom
  tools/                  Extractors that read a user's own client install, the client launcher, and dbimport
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
| supervisor | Starts, stops, restarts and watches the other apps on its machine, and hosts the panel: the dashboard, the panel API and the event socket, with its own SQLite store. In node mode it is the agent for a remote panel |

## Methods

### Content is data

Templates, spawns, quests, quest givers, loot, and vendor lists live in the world database, not in code. The game server loads them into global managers at startup, and every manager reloads live through the reload framework from GM commands, the console, or the admin API. Tables that reference each other reload and validate together as one snapshot. Code implements only the behavior data cannot express.

### Databases and updates

There are three databases: `login`, `characters`, and `world`. Every change is a new file in `data/sql/updates/db_<name>/` named `YYYY_MM_DD_NN.sql`. At startup the updater applies unapplied files in order and records each one in an `updates` table. The `db update` command and the admin API apply data-only updates live and then reload the affected managers; an update that changes a schema the running binary reads applies at the next binary upgrade for now, and applying such an update live, when the running binary's statements still work against the new schema, is planned, not yet scheduled. Open pull requests put their files in `pending_db_<name>/`, and they move into `updates/` when merged. `base/` is regenerated periodically by squashing old updates.

The supervisor keeps its own store, one SQLite file in the Ambrose data folder holding the panel's sessions, its audit rows and its users, so the panel runs before any game database is configured and survives one being wiped. It follows the same rules with the same code: dated files in `data/sql/panel/` named `YYYY_MM_DD_NN.sql`, applied in order inside one transaction each and recorded with their hash in its own `updates` table, and a file that changed after it was applied is reported rather than applied again. Nothing a game server reads belongs there, and the store holds no game state.

### Message handlers

Each app lists every client message once in a `MessageHandlerTable`: the message, the session statuses it is accepted in, its processing mode, and the `Session::Handle<Message>` member that handles it. A message can also be listed as not handled yet, with the statuses it will need, or as refused because only the server sends it. Handlers are grouped by subsystem in `game/Handlers/<Subsystem>Handler.cpp`.

### Scripting

`ScriptMgr` exposes hook classes such as `WorldScript`, `PlayerScript`, `NpcScript`, `QuestScript`, `ZoneScript`, and `CommandScript`. Each script file defines its classes and one `AddSC_<name>()` function, and its folder's script loader calls that function. Content scripts are grouped by world, for example `scripts/WizardCity/`.

### GM commands

Each command group is one file, `scripts/Commands/cs_<group>.cpp`, holding a `CommandScript` with a command table and the default account security level each command requires. A `command_security` table overrides levels live and reloads with the other command data.

### Modules

A module is a folder in `modules/` with its own `src/`, `conf/`, and `data/sql/`. CMake discovers it and registers its scripts through a generated loader, so a module needs no edits to core files. Adding or removing a module needs a rebuild and restart; a module's own configuration and settings reload live.

### Configuration

Each app ships `<app>.conf.dist` listing every option with its default. Users copy it to `<app>.conf`, which git ignores. `reload config` on the console, `.reload config` in game, and the admin API re-read every layer and apply the changed options live.

### Client data

Nothing from the game client is committed. Tools in `src/tools/` read the user's own installation and produce the files and world database rows the servers load. On a first start the servers set this up themselves: they find the installation, get its type dump from `TypeDumpCache`, which runs typeextract through `ChildProcess` when the revision has no current dump, and the game server extracts the character name tables, as Decisions, Automatic setup describes. The client itself is started by `launcher` in `src/tools/launcher`, never by KingsIsle's launcher, as Decisions, Client launcher describes.

### Operations

Servers stay headless so they run the same on a desktop, a Linux VPS, or in Docker. Each app writes colored logs and accepts commands on its console. An optional admin API, bound to localhost by default and protected by a token, serves health, status, live logs, audited commands, live settings, reloads, and Prometheus metrics. The web dashboard in `apps/dashboard/` and the Grafana dashboards in `apps/grafana/` are built on that API. The supervisor that starts and restarts the apps also serves the dashboard as a hosting panel in the style of Pterodactyl, with panel users and permissions, schedules, backups, updates, a file manager, resource graphs and several machines under one panel, and, because it manages a Wizard101 installation rather than a container, realms and zones, accounts, bans and characters, player registration and moderation, world database edits, patch revisions and an installation-wide maintenance mode. The panel has a listener of its own, which carries session cookies and passwords: it binds to localhost by default and follows the Remote access rule below under its own `Panel.` option names. Browsers talk only to the supervisor, which relays each app's admin API under the signed-in user's permissions, so no app token reaches a browser. Nothing the panel serves carries a file from the user's client install. doc/PANEL.md describes the design, doc/PANEL-MAP.md maps the studied Pterodactyl behavior to the milestone that covers it, and phase 17 of doc/ROADMAP.md plans the work.

### Tests

`src/test/` mirrors `src/` and builds a single unit test executable. Database integration tests run only when `AMBROSE_TEST_DB` holds a connection string such as `127.0.0.1;3306;root;root;ambrose_test` for a disposable server, and skip otherwise. Each test creates uniquely named databases and drops them when it finishes, including the app smoke tests, which start the real executables with `--check`. The Linux CI legs run them against the runner's MySQL 8 whenever those legs build, and local runs can use MariaDB.

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
| Cryptography | Botan 3, covering SHA-2, Twofish, Argon2id for panel passwords, and the random number generator. The client's non-standard CRC-32 is implemented in `common` |
| TLS | OpenSSL 3, which Asio and Crow serve TLS through, and which reads the certificates a listener is given. It is the transport only; Botan stays the game's cryptography |
| Panel store | SQLite 3 in WAL mode, one file the supervisor keeps its sessions, audit rows and users in |
| Database server | MySQL 8.0 or newer, or MariaDB 10.6 or newer |
| Database client | MariaDB Connector/C, which works with both servers. Its authentication plugins ship beside each executable in `plugins/libmariadb` |
| CPU emulation | Unicorn 2 (GPL-2.0), linked only into the typeextract tool |
| Metrics | No library. The Prometheus text format is emitted from `src/common/Metric`, whose register holds every counter, gauge and histogram with its labels, because the format is a few lines to write and a dependency to read it would carry a collector, a register and an HTTP server this project already has. The same register answers a JSON view for the panel, so there is one set of numbers and two renderings of it |
| Panel charts | uPlot, added when the first chart is built rather than before it. Chosen for being small and fast with many points and for tying the panel to no framework; Grafana serves the dashboards of 17.10 and needs nothing from us |
| x86 decoding | Zydis 4 |

### Protocol and type data load at runtime

The client's message definitions and type dump are never compiled into the build. At startup each app loads the message definition XML files from the user's install into a `MessageRegistry`, and the type dump into a `TypeRegistry`. Code that uses a message or class declares only the fields it needs, with their C++ types. At startup every declaration is resolved to field indices and checked against the loaded definitions, and the app refuses to start if any declaration is wrong. A reload resolves every declaration against the new definitions before it swaps them in, and keeps the active definitions if any declaration fails. Wire layout always comes from the loaded definitions, so fields a declaration omits are still encoded correctly with default values.

As a result, the project builds and its unit tests run on any machine, including CI, with no client files. Any retail copy of the pinned revision serves; nothing in the tree depends on where an install sits. Unit tests use small definition fixtures written by the project. Tests that need a real install carry the CTest label `client` and run only when `AMBROSE_CLIENT_DIR` is set; tests that need the user's own type dump also carry the label `client` and run only when `AMBROSE_TYPE_DUMP_PATH` names it. That dump is either the older reference dump or one typeextract wrote, and the two differ: the extraction finds five classes and four properties the reference missed, and writes sixty enum option values as the empty text the client holds where the reference wrote 0. A test that pins class or property counts therefore reads the revision and extractor from the dump's own header and expects the shape whoever wrote it produces, so the same check passes on either dump and a dump of another revision is skipped rather than failed against counts that were never its own. The compact codec's capture check also needs `AMBROSE_OBJECT_SAMPLES_DIR`, a folder of blobs captured on the user's own machine and named after their class, which are never committed. `AMBROSE_FUZZ_ITERATIONS` sets how many mutations the decoder fuzz test runs. typeextract's client tests also extract a second install when `AMBROSE_SECOND_CLIENT_DIR` names one, so type discovery is checked on another client revision.

### Type dump and type registry

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- `sTypeRegistry` loads the user's own type dump (format v2), the one `TypeDumpPath` names or else the one built from the install in use, through nlohmann-json's SAX interface, keeping only the fields the schema needs. The file text and the raw classes live only while the load runs; the r806919 catalog then takes about 11 MiB, resolved defaults included, and loads in about 180 ms in an optimized build. The dump's SHA-256 is logged with its counts, load time and approximate size so a server pins the revision it runs.
- A load refuses the dump, reports every problem and keeps the active catalog when:
  - the JSON is broken, is not an object, or has no classes, an empty classes object, or no `class PropertyClass`;
  - a field the schema knows has the wrong JSON type, or a property lacks one of its nine fields;
  - the version is not 2;
  - a class or property is listed twice, or a class is listed under a key other than its hash;
  - a class or property hash is not what its names hash to, or two classes share a hash;
  - property ids do not run from 0 without gaps, or an id, offset or flags value does not fit 32 bits, or a container is not Static, List or Vector;
  - a base is not listed, a class's base chain disagrees with its first base's own chain, or a property's class or enum type is not listed or no value kind covers it.
- `class X*` and `class SharedPointer<class X>` entries collapse into `X`, and lookups by an alias's name or hash return `X`. When `X` is not listed, the loader first looks for a listed class whose name matches once `class` and `struct` prefixes and the long `std::basic_string` spellings are removed, because the dump names some templates, such as `MadlibArgT<float>`, only that way; only when none matches is `X` built from the alias, with the hash of its own name.
- Every class gets a kind:
  - A property class lists properties or bases, or is the `PropertyClass` root. This includes the templates listed without a `class` prefix.
  - The other kinds are enum, the fixed-layout value types the codec knows (Vector3D, Quaternion, Matrix3x3, Euler, Color, Point<int>, Point<float>, Size<int>, Rect<int>, Rect<float>, SerializedBuffer, SimpleVert, SimpleFace), primitive, std container, and opaque for the remaining classes without reflected properties.
- Classes list their base chain nearest first, and their properties include the inherited ones, in id order, found by hash or name without a scan. The catalog hands out only const classes.
- Property types classify into value kinds: the primitives, which need no entry of their own in the dump, `bi<N>` and `bui<N>` bit fields of 1 to 32 bits, enums, property-class objects, and the value types.
- Enum options belong to the property, not the enum type, because the dump attaches them per property and they differ between properties of the same enum. Integer options are 32-bit values kept in one form, their bits read as unsigned, so -2 is stored as 4294967294; a value outside INT32_MIN to UINT32_MAX refuses the load. They are found by name or value through sorted indexes, the first listed name winning when values repeat. `__DEFAULT` is kept as the dump gives it, an integer or text, and also resolved into the value new objects start with, `__BASECLASS` is kept as a text hint, and other text options as text.
- A load builds a new catalog generation off to the side and swaps it in atomically. Code holds a shared pointer to the catalog it started with, so objects built from an older generation stay valid after a swap. 4.15's reload triggers call `LoadFromFile` again.
- A load also refuses a class that holds an object of its own class inline, directly or through other classes, because no real layout can do that and building its defaults would never end, and a `__DEFAULT` that does not resolve to a value of its property's type.

### Dynamic property objects

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- A `PropertyObject` is an instance of one property class from one catalog generation. It keeps a shared pointer to that catalog and its values in property id order. Each class knows the catalog it belongs to, so `PropertyObject::Create` refuses a class from another catalog with a pointer comparison, and refuses types that are not property classes.
- A `PropertyValue` holds one alternative per C++ storage type rather than one per value kind:
  - Gid shares `uint64`;
  - signed and unsigned bit fields and s24/u24 share `int32` and `uint32`;
  - an enum is an `int64` holding its 32 bits read as unsigned, from 0 to 4294967295, so one value has one form;
  - lists and vectors are a `std::vector` of values;
  - a child object is a `std::unique_ptr`, so an object tree has one owner and copies are deep.
- A const value hands out a child only as `PropertyObject const*`, so an object shared read-only cannot be changed through it.
- Every write is checked:
  - the value must be the property's exact alternative;
  - a list is checked element by element;
  - bit fields must fit their width, and an enum its 32-bit range;
  - an inline object cannot be null;
  - a child must be of the property's class or derive from it;
  - a child must come from the same catalog generation as the property's class. A child from another generation is refused as `OtherCatalog`, so build children with the parent's `GetCatalog()`.
  - a write that would make an object own itself, directly or through its children, is refused.
- The setters take the value as an rvalue and move it in only when the write succeeds. A refused write changes neither the object nor the caller's value.
- A child object or list element can be edited in place, so a large list is never copied to change one entry:
  - `EditObjectAt` hands out a child, whose own writes are checked and whose class cannot change.
  - `SetElementAt` replaces an element, or appends one when the index equals the list's size, under the same checks.
  - `EraseElementAt` removes an element.
- Defaults are resolved once, when the type dump loads, and a new object copies them. Inline children are created and pointers start null. Resolution follows these rules:
  - a number, enum or Bits default written as text resolves through the property's option names, so `INSIDE` and `A|C` work, and a text number parses as a number;
  - an integer past a signed type's range wraps to the same bits, so 4294967295 on an `int` is -1, and a bit field keeps its low bits, so 15 on a `bi4` is -1;
  - an integer default on a text property is ignored, because the dump writes 0 there;
  - a default on a list, an object or a value type refuses the load.
- Equality and cloning are deep and exact. Floating values compare by bit pattern, so an object holding a NaN equals its clone, and 0.0 differs from -0.0.
- An enum value renders as its option name. A Bits value renders as an exact option name, or else as every nonzero option, in dump order, whose bits it holds and no earlier chosen option covered, joined by `|`; multi-bit options are included, and a duplicate value keeps its first name. A value with bits no option names does not render as a name, and a caller shows the number instead.

### ObjectProperty codec

Settled on 2026-09-16 under the maintainer's standing direction to decide. The compact layout below reproduces every captured badge blob byte for byte, but those blobs use only class hashes, `int`, `unsigned int`, `bool`, `std::string` and lists of pointers. The versionable layout decodes the client's own data files, which exercise far more types. Every layout neither source confirms follows the reference implementation and is pinned by golden-bytes tests.

- `ObjectSerializer::Encode` and `Decode` handle both formats. `SerializerOptions::Versionable` picks between them; the default is the compact format the client uses inside messages. Every object starts with its u32 class hash, 0 for null, inline objects included. Its properties follow in id order with no headers. A property is written when its flags hold every bit of the mask and it is not deprecated. `TransmitMask` is the default, and `PublicMask` adds Public for views of other players.
- The wire layout:
  - bits pack least significant bit first, and a byte-aligned value starts on the next whole byte;
  - a bool is one bit, a bit field its width, and s24/u24 24 bits;
  - integers and floats are little-endian;
  - a string is a u16 byte length and its bytes, and a wide string is a u16 unit count and UTF-16LE units;
  - a list is a u32 count and its elements;
  - with `CompactLength`, every string length, wide string count and list count is instead one bit, then 7 bits for a length under 128 or 31 bits otherwise, and the bytes that follow start on the next whole byte;
  - an enum is a u32, or its option name as a string when `StringEnums` is set, and `StringEnums` carries int and unsigned int properties with the Bits or Enum flag the same way, with Bits values written as option names joined by `|`;
  - a value type is its fields in order.
- The versionable format, which BINd files use, frames everything with sizes in bits:
  - an object is its u32 class hash, 0 for null, then a u32 size counted from that size field, then its properties in any order;
  - a property is a u32 size, a u32 property hash and its value, and its size counts from where the previous property ended, before the size realigns to a byte;
  - properties are written and read only when the mask selects them, as in the compact format;
  - a DirtyEncode property carries no present bit: the writer leaves it out when `IsDirty` calls it clean and `ForceDirtyEncode` is off, and a property left out decodes to its default.
  A property the class does not list, the mask does not select, or that is deprecated is skipped by its size, and so is an object of a class the dump does not list. The object becomes null in a pointer slot and a default object of the property's class in an inline slot. A value is read inside a bit limit at its property's end. Some values keep their default, or the value an earlier copy of the same property gave them:
  - a value that would run past that end, including a list count the bits left cannot hold;
  - a value with no known layout, or that names no enum option;
  - an object of the wrong class, or a null inline object.
  A value that ends early keeps what it read. Every one of these is reported in `DecodeResult::Issues` with its kind, hash, the bits skipped and its property path, and decoding resumes at the property's end. An unknown root class is refused, and so is a list count that fits but exceeds `MaxContainerCount`. So is an object or property size that cannot fit in the object or data holding it, with `BadSize`, by the object it belongs to. A property whose nested object is refused that way reports a size mismatch instead, so one bad object costs only its property. Every property size covers at least its header, so a zero size can never loop. A default inline object that a written property takes counts toward the depth and object limits through the depth and object count the loader measures for each class's default object, so anything that decodes can be encoded and decoded again under the same limits. Checked on r806919 with scratch sweeps over Root.wad before the rules were committed:
  - with the Save mask, 134,635 of the 134,640 BINd files decode with no size mismatch, unknown or unselected property, unknown enum name, invalid object or unsupported value;
  - 26,921 nested objects of classes the dump does not list are reported, and the other 5 files have a root class it does not list;
  - the files leave out DirtyEncode properties at their default values. Re-encoding the 114,687 files that decode without issues, with clean meaning equal to the default, reproduces 114,342 byte for byte under the Save, Save and Copy, Save and Public, or all three masks. The rest hold a DirtyEncode property at its default, and no mask without those bits reproduces as many.
  Milestone 3.11 makes the sweep a client test.
- A DirtyEncode property carries a present bit first in the compact format. The encoder sets it unless `IsDirty` says the property is clean and `ForceDirtyEncode` is off. A property marked absent decodes to its default.
- The captures carry plain class hashes. An alias hash decodes to its class and re-encodes as the plain hash.
- What no capture has confirmed yet:
  - whether an inline object carries a hash on the wire (the codec writes one, as the reference does);
  - DirtyEncode, which no sample exercises;
  - wide strings, `char`, `short`, `unsigned short`, `unsigned char`, `__int64`, gid, `float`, `double`, `wchar_t`, bit fields, s24/u24 and enums, in both forms;
  - every value type, including Color's byte order (kept as red, green, blue, alpha), Euler, and Matrix3x3 as nine floats.
  The character creation and list milestones exercise wide strings, bit fields, enums and small integers against the real client, and their captures confirm or correct these layouts. The client's data files confirm char, short, unsigned __int64, double, gid, float, wchar_t, bit fields, u24, `Point<int>`, `Size<int>`, `Rect<float>`, compact lengths and text enums and flags in the versionable format. No data file holds a Matrix3x3, Euler, Quaternion or SerializedBuffer value.
- The serializer refuses `SerializeFlags` and `Compress`, because they frame a whole blob or file, which `BlobEnvelope` wraps for messages and 3.11's `BindFile` reads for data files. SerializedBuffer, SimpleVert and SimpleFace values are not supported yet and are refused, because their layout is not known; supporting them is planned, not yet scheduled, once captures or client reverse engineering recover it.
- A decode trusts nothing:
  - depth, object count and list length have limits, and depth never exceeds a ceiling of 128 whatever the setting, so a decode cannot exhaust a thread's stack;
  - every object, list element, default value and string is charged against a memory budget, 16 MiB by default, before it is allocated. Each class's default object size is measured once when the type dump loads, so a small blob naming a large class cannot grow into hundreds of megabytes;
  - a caller can hold the root to a set of classes and require it to be present;
  - a count the remaining bytes cannot hold, at the element's smallest size, is refused before anything is allocated;
  - reservations are capped;
  - a child's class must derive from its property's class and is checked as soon as its hash is read;
  - an inline object cannot be null;
  - trailing bytes are refused unless allowed.
  Every failure names the property path it happened at, such as `class BadgeInfoList.m_badges[2]`. Running out of memory anyway is reported as a status rather than thrown. Decoded objects are filled directly through a key only the serializer holds, and properties the mask skips take their defaults.
- The limits are live settings: `ObjectProperty.MaxDepth`, `MaxObjects`, `MaxContainerCount`, `MaxDecodedBytes` and `MaxInflatedSize`. `SerializerLimits::Load` reads them from configuration, clamping out-of-range values and reporting each one. `SerializerLimits::Apply` publishes them as a snapshot. Each decode or encode whose options carry no limits of their own reads the snapshot when it starts, through a per-thread copy refreshed only when a generation counter changes. A reload therefore applies to the next decode, even through options kept from before it, and decoding threads never contend on a shared count.
- Message fields that carry objects are described in one table, `ObjectFields`: the classes each field's object may be, whether its blob is enveloped, and whether it may be empty. `DecodeField` opens the envelope within `MaxInflatedSize` and holds the root to the field's classes. `EncodeField` refuses an object the field cannot carry and wraps the blob when the field is enveloped. The table is code because it describes how a client revision parses its messages, and it changes only with the revision.
- Decoders of untrusted data are fuzzed two ways. Both share seeds made of a mode byte and a golden blob: bare, with text enums, with compact lengths, versionable with and without both, or inside a stored or compressed envelope decoded through `DecodeField`. `DecoderFuzzTest` runs seeded mutations in every build, a million under AddressSanitizer, and checks that no decode allocates more in total than the memory budget and inflation limit allow. The `linux-clang-fuzz` preset builds libFuzzer targets with coverage instrumentation, AddressSanitizer and UBSan, and CI runs each from its seed corpus. Anything that decodes must re-encode and decode back equal.

### BINd files

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- `BindFile` reads and writes the client's BINd data files: the `BINd` magic, the u32 serializer flags, and for a compressed file a padding byte, the u32 inflated size and a zlib stream at offset 13, around one versionable object.
- Reading:
  - it refuses a file that is not BINd, ends inside its header, carries flag bits no mode uses, would inflate past `MaxInflatedSize`, or holds a stream that does not inflate to exactly its size;
  - it decodes with the file's own length and enum modes and the Save mask, and refuses a null root object;
  - it returns the root class hash, so a file whose root class the dump does not list can still be reported by hash.
  `BindFile::GetDefaultLimits` raises every limit to its ceiling because the data is the user's own install: the template manifest alone holds 137,423 objects and inflates to 11 MiB. A caller loading untrusted files passes its own limits.
- Writing uses the same limits, leaves out a dirty-encoded property equal to its default, the rule that reproduces the client's files, unless `ForceDirtyEncode` is set, and compresses when `Compress` is set.
- `BindSweep` decodes every BINd entry of an archive on every hardware thread. Workers take entries one index at a time, read them through the archive's lock, decode in parallel and keep their own tallies. The tallies are merged in entry order, so the report is identical however the work was split. It lists:
  - failures;
  - unknown classes with their use and file counts and the first file and path each appears at;
  - every other issue, grouped the same way by kind and hash, so a sweep against a dump from another revision stays small.
  An entry that throws counts as unreadable or failed, and no exception leaves a worker. If some workers cannot be started, the sweep runs on those that did. On r806919's Root.wad it covers 173,088 entries and 134,640 BINd files in about 80 seconds in a debug build.
- `PropertyJson` renders an object as ordered JSON for tools and debugging:
  - `$class` comes first, then the properties in id order;
  - enums and flag integers appear as option names when they have them;
  - wide text becomes UTF-8, and math and color types become arrays;
  - text that is not UTF-8 is repaired with replacement characters rather than refused;
  - NaN and infinities become the strings `NaN`, `Infinity` and `-Infinity`, so they cannot be mistaken for a null value.
- `bindecode` (src/tools/bindecode) prints named entries of an archive as JSON with their issues on standard error, reads an entry that holds a versionable object with no BINd header under `--raw`, as a zone's `gamedata.bin` does, lists entry names, or sweeps the archive. It reads only the user's own install and type dump, from `--client` and `--type-dump` or `AMBROSE_CLIENT_DIR` and `AMBROSE_TYPE_DUMP_PATH`, and exits 0, 1 on a read or decode failure, or 2 on bad usage. A sweep exits 0 when the only failures are files whose root class the dump does not list and no issue other than unknown classes is reported. Like every app, it takes its arguments and environment variables as UTF-8: `Ambrose::GetArguments` reads the wide command line on Windows, and `Ambrose::GetEnv` and `SetEnv` use the wide environment there. A path such as a user folder with accented letters therefore opens instead of failing to convert.

### Locale text

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- Client display text lives in `Locale/<locale>/*.lang` files in Root.wad. Each is UTF-16LE with a byte order mark: a header line `1:<Stem>`, then each entry as a key line, a metadata line and a text line. The full key is `<Stem>_<Key>`, for example `QuestTitle_00001718`. `LangFile` parses one file of at most 64 MiB into UTF-8. One or two blank lines after the last entry are padding, as most r806919 es, el and pl files end with one and most de, fr and it files with two; a malformed file, an empty key or an unfinished entry is refused with the reason.
- `sLocaleStore` resolves full keys in any installed locale:
  - loading groups the files by locale folder and parses the default locale at once;
  - every other locale parses once, on first use, even when several threads ask at the same time;
  - lookups read an immutable snapshot;
  - a file that cannot be read or parsed is skipped and named on its locale's table, and a locale fails only when none of its files load; on r806919 only pl/WizardFurniture.lang is skipped;
  - a reload or a default locale change builds and checks the new data first, including every locale already in use, and a failure keeps the previous store resolving keys with the reason.
- A key defined twice keeps its later text and is counted. r806919's en-US repeats 40 keys, 39 of them with different text; which copy the client shows is not confirmed. The metadata line is kept per parsed entry but not stored, since its meaning is unknown.
- The game server loads `Locale.Default` from the install in use at startup, warns for each skipped file, and refuses to start when the locale cannot load. `localetool` finds keys by text, checks and dumps keys, and lists locales with the files they skip from the user's own install.

### Plain-XML ObjectProperty files

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- A few client files, such as ActionList.xml, InputBindings.xml and CharacterCreation/CharacterCreationConfig.xml, hold ObjectProperty objects as text XML instead of BINd. `XmlObjectReader` reads them as UTF-8 with pugixml, which the message definitions already use. An upper bound of the parsed document's memory is charged against `MaxDecodedBytes` before parsing, and a document with text or a second element beside its root is refused. The document holds:
  - an `Objects` element holds `Class` elements named by class;
  - each property is an element named by the property, holding text or a nested `Class`;
  - a container is every element of its name in document order.
- Each object starts from its class's defaults. A value's text is joined across comments and CDATA, and a whitespace-only value is kept. Text becomes the property's type:
  - numbers, and `true` or `false`;
  - option names and `|` flag lists;
  - UTF-8 text and wide text;
  - colors as AARRGGBB hex;
  - math types as numbers separated by commas or spaces.
  An empty element is a null pointer. Each container element is checked and appended on its own, so one bad element costs only itself.
- Nothing unreadable stops a file. An unknown class or property, an object of the wrong class, a value that does not read, or a static property given twice is skipped and reported as a `DecodeIssue` with its property path and line. The property keeps its default, or its last good value when repeated. An explicit inline object takes the place of its default in the object and memory counts. Malformed XML, a root other than `Objects`, and documents past the depth, object, element or memory limits are refused, and default inline objects count toward the depth and object limits as in the binary formats.
- The r806919 type dump does not list the classes of CharacterCreationConfig.xml, Chatter.xml or Colors.xml. Those files read as well-formed and report only their root class until 6.10's supplemental schemas describe those classes.

### Typed views

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- Game code reads the client classes it uses through typed views written by hand, not code generated from the dump, so no client-derived output enters the build and CI needs no dump. A view is a class deriving from `TypedView`. It declares its class name and fields as position, C++ storage type, dump type and property name, and the property hashes are computed at compile time.
- Views are checked when they compile:
  - an accessor reads a field with `Read<Field>()`, whose return type comes from the storage type the field declares, so an accessor cannot read a field as another type;
  - a field must name one of `PropertyValue`'s storage types;
  - each field must sit at the position of its enum value.
  `AMBROSE_TYPED_VIEW` makes the constructor private, so only `From` builds views.
- Every type dump load binds each view the registry holds to the new catalog before the catalog is published:
  - its class must be a listed property class;
  - each field must name a property of that class with the field's dump type;
  - each field must be stored as the C++ type the field declares.
  Every mismatch is reported with the view's name. Any mismatch refuses the dump, so a server does not start, and a reload keeps the active catalog. `sTypeRegistry` binds the built-in views in `ObjectViews`; other registries bind the views they are given. The first load closes a registry to new views, because a view added later would not be checked until a reload.
- Bindings live in the catalog generation that made them. `View::From(object)` looks up the binding in the object's own catalog and returns nothing unless the object is of the view's class. A view over an object from an older generation keeps that generation's ordinals after a reload, and the object keeps that catalog alive.
- A view caches the binding's ordinals when it is built, so reading a field is one indexed load into the object's values, with no hash lookup. Readers return the stored value itself: numbers, strings, value types and lists by reference, and child objects as `PropertyObject const*`. A view borrows its object, which must outlive it.
- A view bound to a class works for every class that derives from it, because derived classes keep inherited properties at the same ids and containers. The loader refuses a dump where that does not hold.

### Live reload and live settings

Settled on 2026-09-14 at the maintainer's direction. Anything that can change while a server runs does, without restarting a process. A subsystem that holds loaded state builds the new state off to the side, validates it completely, and swaps it in atomically, so threads in the middle of an operation keep a consistent snapshot. If anything fails, the old state stays active and every error is reported. Runtime limits apply from the next operation. Every gameplay value, such as respawn times, drop rates, experience and gold rates, and every other tunable number, is a typed setting with a default and bounds. Settings are changed live from the control center (the admin API and web dashboard), GM commands, or configuration reloads, and each change is validated, persisted, and written to an audit log. Message definitions reload this way today, and configuration and logging have reload functions that keep their old values on failure. The reload framework and its triggers arrive in milestone 4.15, the live settings registry in 4.16, and the control center pages in 17.12 and 17.13. Live settings persist in the database the app owns (`characters` for the game server, `login` for the login and patch servers) with an audit table. Environment variables and command-line overrides lock a key, and a live edit to a locked key is refused with a message naming the layer. Live world database edits, from the control center or from direct GM commands such as `.npc add`, are journaled and can be exported as a pending SQL update. A restart is required only where the operating system or the client forces one, such as replacing the server binary, and each such case is documented where it arises.

### Message definition quirks

Settled on 2026-09-14. A field whose type attribute is misspelled `TPYE` or `TYP` keeps that type, and a `GlobalID` field with no type is a GID. Each case is reported as a load warning, which the startup loader logs, and the field stays on the wire. This keeps MSG_PHYSICS_GRAB, MSG_MINIGAMEREWARDS, and MSG_BATTLEGROUNDQUEUEUPDATE at their fullest layout until capture verification shows the client drops those fields. A message's element tag is its identity and sort key, never `_MsgName`. A repeated tag merges into one id only when its fields match, and a repeat with different fields is an error.

### Sessions and keepalives

Settled on 2026-09-14. A session sends SessionOffer before any other work and closes on a SessionAccept or client keepalive that names another session id. DML frames that arrive before SessionAccept are queued, up to 256 frames or 1 MiB, and delivered in order after it. A KeepAliveRsp echoes the client's elapsed minutes and carries the server's milliseconds into the current second. A server keepalive closes the session only when nothing at all arrives from the client within `Network.KeepAliveTimeout`, so a client that never answers server keepalives but sends its own stays connected. Session ids are nonzero, handed out in rotating order, and reused only after their session closes. These choices hold until doc/CAPTURE.md records otherwise.

Work that needs the maintainer's own client, such as the real-client checks of 1.21 and 1.22, is listed in doc/ROADMAP.md under Where we are. Milestones that do not depend on those checks go ahead while they wait.

### Message dispatch and session states

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- Every app shares one `SessionStatus`: `Connected` once the handshake is done, `Authenticated` once credentials are verified, `CharacterSelected` once the login server hands the client to a realm or the game server has validated that hand-off, `LoggedIn` once LOGINCOMPLETE is sent, and `InWorld` once the character is in a zone. Each app uses the statuses it needs, and a rule accepts a mask of them.
- Message ids come from the client's definitions at runtime, so a table cannot be checked against them at build time. Rules name messages by service and tag instead. Handled messages are declared with the message registry, so a definition load that lacks one is refused. At startup every rule is also checked against the loaded definitions, and a rule for a message the definitions lack, a duplicated or status-less rule, a queued rule in an app that drains no queues, or any message of the app's own services without a rule stops the app. Each loaded catalog resolves the rules to service and order slots once, so a reload that renumbers messages routes them correctly.
- Dispatch follows AzerothCore's split between messages the server never accepts and messages it does not handle yet. A message listed as refused, a message from a service the app does not serve, an id the definitions do not have, a body shorter than its definition, or a MSG_PING beyond the session's ping budget counts a strike, and `Network.MaxStrikes` strikes close the session. A message in the wrong status, or one not handled yet, is dropped and logged without a strike, so a real client is not disconnected for sending something Ambrose has not implemented. Each session may drop only `Network.DroppedMessageBurst` messages, refilled at `Network.DroppedMessagesPerSecond`, while every drop is logged; beyond that a drop is not logged and counts a strike, so one client can neither fill the logs nor stay connected by flooding. Client-supplied text is escaped and cut to 64 bytes before it reaches a log line.
- A handled message runs in place on its network thread, or is queued on its session and run when the owner drains the queue, with the status checked again at that moment. A session holds at most 4096 queued messages and 4 MiB of their bodies, and a handler that throws closes its session. Queued work can still run after the socket closes, so the owner that drains a session's queue also runs its close cleanup on that thread, as AzerothCore's `WorldSession::Update` does.
- A session sends any declared message with `SendDmlMessage`, named so because Windows headers define `SendMessage` as a macro. It encodes the message against the live definitions straight into its frame and moves that frame into the socket's send queue, so nothing is copied, and it refuses, with a logged error, when no definitions are loaded, the message is not declared, a value cannot be encoded, or the body does not fit the 16-bit DML length. It returns false without sending once the session is closed or closing. Each app declares the messages it sends through its table's `Sends`, so a definition load that lacks one is refused.
- Every app shares the SYSTEM and EXTENDEDBASE rules: MSG_PING is answered in place with MSG_PING_RSP within the session's `Network.PingBurst` and `Network.PingsPerSecond` budget, and beyond it counts a strike with no answer; the record messages are not handled yet; and MSG_SERVERMESSAGE and MSG_FORCE_DISCONNECT are refused from clients. `SendDmlMessageDelayedClose` and `KickPlayer` mark the session closing, so no further message of its is dispatched and later sends are refused, then close once everything queued is flushed. `KickPlayer` cuts its reason to 1024 bytes at a character boundary and closes even when MSG_FORCE_DISCONNECT cannot be sent.
- Every connection's send queue is capped by `Network.MaxSendQueueBytes`. A frame that would pass the cap is not queued and the connection is closed, as a slow consumer, so a client that stops reading cannot grow server memory without bound.
- The login server's table lives in `apps/loginserver/Server`. The game server's table arrives with its sessions in milestone 4.01, and the patch server's with its TCP service in 16.04, both on the same `MessageHandlerTable`.

### Database pools

Settled on 2026-09-14. A pool serves every call from its current connection generation: sync connections leased one caller at a time, with several queries readable from one consistent snapshot on a single lease, async workers on a shared queue, a keepalive pinger, and the statement table. A new generation opens, checks versions and prepares every statement before it is published, so opening, closing and live reconfiguration never block callers, and a failed reconfiguration keeps the current generation. A retired generation drains its queue for up to 30 seconds, then cancels what is left and settles every callback. A statement is retried after a reconnect only when it cannot have run: never inside a transaction, and a lost connection during a write is reported instead of retried. Transactions retry deadlocks, lock wait timeouts and connections lost before COMMIT for up to 60 seconds. Callbacks for async work run on whichever thread polls them, normally the app's update loop through an AsyncCallbackProcessor, and an async call can also pass a completion handler that the worker calls once the result is settled, so an owner can run its callbacks on its own thread without polling. A synchronous `Query` returns null for an empty result, as in AzerothCore, and `TryQuery` also reports whether the query failed, so callers that must tell a missing row from a failing database use it.

### Accounts and the console

Settled on 2026-09-14 under the maintainer's standing direction to decide and favor the most capable option.

- The client's ClientKey1 scheme needs the server to hold `base64(SHA-512(password))`, which is as good as the password. Ambrose stores it in `login.account.verifier` and encrypts it at rest when `Account.VerifierKeys` and `Account.VerifierActiveKey` are set: AES-256-GCM with a random nonce, bound to the lowercased username, with the key id stored on the row. Older keys stay listed until no row uses them. A row is sealed again with the active key whenever its password changes, and at a successful login when it is not already sealed with the active key. Keys listed without an active key stop startup, so encryption cannot be left half configured. With no keys at all, verifiers are stored unencrypted for development. `Account.AllowPlainVerifiers = 0` then refuses any account whose verifier is still unencrypted, so a verifier written straight into the database cannot be used. Access to the login database must be restricted either way.
- Account management lives in `src/server/game/Accounts` and builds as its own `accounts` library on top of `database`. The login server links it and the `characters` library, and the game layer and its GM commands use the same code.
- Account security levels use AzerothCore's numbering: 0 player, 1 moderator, 2 game master, 3 administrator, 4 console. An account's own level lives in `login.account.security_level`, and the `account_access` table of milestone 4.02 adds per-realm levels layered over it. How they map to LOGINCOMPLETE IsCSR and Permissions is still open.
- Usernames are up to 32 ASCII letters, digits, `_`, `-` and `.`, with a configurable minimum length, unique regardless of case through an `ascii_general_ci` column. Passwords are UTF-8 without control characters, up to 128 bytes. Ban and account times are Unix seconds, and an `unbandate` of 0 never expires.
- Every app reads console commands from standard input on its own thread and runs each line on a second thread, so a command that waits on the database never blocks the io loop, its timers or its signals. A shutdown waits for the line in flight before the app closes its databases, and at most 256 lines wait in the queue. Arguments of sensitive commands never reach a log. A closed input leaves the server running, a stop interrupts a blocked read, and `Console.Enable = 0` starts no reader. The shared `ConsoleCommandTable` holds `help` and `shutdown` from `ServerApp` plus each app's own commands. Milestone 17.01 adds the prompt, line editing and colors on top of it, and 4.02's CommandMgr takes the table over.

### Login authentication

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- MSG_USER_AUTHEN_V3 is checked on the session's network thread without blocking it. A Rec1 longer than 512 bytes is refused before it is decrypted. Rec1 is decrypted with the session's own SessionOffer values and must hold exactly a session id, username and ClientKey1 separated by single spaces. One prepared query then reads the account with its lock and whether the account, the client's address or its MachineID has an active ban.
- The checks run in this order: session id, revision, machine ban, address ban, account and ClientKey1 compared in constant time, then account ban or lock. The reference server checks the account and its ban before the machine and the password; Ambrose answers MachineBanned whether or not the account exists, and AccountBanned only once the password is right, so neither answer tells a stranger that an account exists. An unknown account still costs a ClientKey1 hash, so its timing matches a wrong password.
- Async database calls can take a completion handler, which the worker calls once the result is settled, even for work that is cancelled or dropped unrun. A login session's handler posts to its own network thread, which runs the session's ready callbacks, so handlers and callbacks never run concurrently for one session and no session polls. An attempt whose callback is lost, such as one whose result threw, fails with Timeout and closes the session.
- A successful login stores the base64 SHA-256 of a fresh 44-character session key in `account_session`, one row per account, so a copy of the database holds no usable key. The same transaction updates the last login, address and machine, and seals the verifier again when it is not sealed with the active key. The reseal only applies while the stored verifier is unchanged, so it cannot undo a password change made meanwhile. Only then is the session marked `Authenticated` and sent MSG_USER_AUTHEN_RSP with Error=0, the account id as UserID, the session key encrypted as Rec1 and PayingUser=1, followed by MSG_USER_ADMIT_IND with Status=1.
- A failure is answered with MSG_USER_AUTHEN_RSP carrying the error code and its name as Reason, and the session stays open for a retry. Error codes follow the reference server: AuthenFailed, MachineBanned for a banned machine or address, AccountBanned for a banned or locked account, ErrorNoLock for a revision `Login.AllowedRevision` does not list, and Timeout, which also closes the session, when the database cannot answer. MSG_USER_AUTHEN, MSG_USER_AUTHEN_V2, MSG_WEB_AUTHEN and MSG_WEB_VALIDATE are answered with AuthenFailed. A session is closed once it has received `Login.MaxAuthAttempts` failures of any kind, and a MSG_USER_AUTHEN_V3 sent while the previous one is still being checked counts a strike.
- Guesses are counted per address in memory, an IPv6 client by its /64 network, because an attacker can reconnect or rotate addresses within one network. Only a malformed or oversized Rec1, a wrong session id, an unknown account or a wrong password counts. Each attempt reserves a slot while it is checked, and an address with as many attempts in flight as it has guesses left is refused, so parallel connections cannot outrun the limit. At `Login.MaxAuthAttempts` the address is refused for `Login.LockoutSeconds`, as AzerothCore's WrongPass policy does. A success does not clear the count, so logging in to one account cannot reset guesses against another, and failures are forgotten once `Login.LockoutSeconds` passes without one. The table tracks at most 2^20 addresses, prunes a few hash buckets on each call instead of scanning, and when full evicts an idle entry, or else the unlocked entry with the oldest failure, so new addresses are always counted.
- The login server keeps one live session per account. Under `Login.DuplicateLoginPolicy = 1` a login whose password is right takes the account at once and the earlier session, admitted or still being checked, is kicked, so an account never has two live sessions. A claim is released when its session closes, its callback is lost or its transaction fails. Under 0 the new login is refused. The `online` column is not used for this until the game server marks players online in phase 4, so a stale flag after a crash cannot lock an account out.
- Failure lines share a budget of 256 lines refilled at 64 a second across every session; beyond it they are logged at Debug, so reconnecting clients cannot flood the logs.
- Each attempt reads the `Login` options current when it starts. They load at startup, and 4.15's reload triggers refresh them live through `LoginMgr::LoadSettings`.

### Login idle drop and shutdown notice

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- Every client message counts as activity for a login session; keepalives do not, because the client sends them on its own. MSG_LOGIN_NOT_AFK is handled only to count as activity, and its BadgeNameID is ignored. Each network thread updates its open sockets on its 50 ms sweep, as AzerothCore's `NetworkThread` updates its sockets, instead of every session arming its own timer. Until a character is selected, a login session uses that update once a second to check whether it has been idle for `Login.AfkTimeout`, reading the settings lock-free at each check, so a lowered timeout applies within a second. A session whose login is still being checked is not idle. An idle client is sent MSG_DISCONNECT_LOGIN_AFK carrying `Login.AfkWarning` as its Warning byte and closed once the message is flushed. The reference server sends Warning=1; other values are unverified.
- Idle checks and lockouts read time through `LoginMgr::Now`, which tests freeze and move forward instead of waiting.
- When the login server stops, from a signal or the `shutdown` command, it first closes its listener, so no client arrives after the notice goes out. It then visits every accepted session on its own network thread through `SocketMgr::ForEachSocket`, sends MSG_LOGINSERVERSHUTDOWN and closes each once the notice is flushed, and waits until every notice is written, or every connection has ended, for at most `Login.ShutdownGrace` before stopping the network. A second signal during that wait does not cut it short, so the grace stays at most 60 seconds, below the stop timeout of common service managers. The Message value is sent as 0 because the reference server never sends this message and its meaning is unverified.

### Characters

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- A wizard is a `characters` row and a `character_appearance` row with one column per `WizardCharacterBehavior` property, so appearance can be queried and edited field by field. Times are Unix seconds in `BIGINT UNSIGNED`, as in the login tables. The account list and count both join the appearance, so a wizard missing its appearance is neither listed nor counted.
- Deleting a wizard is a soft delete. It records `deleted_at`, and it moves the owner into `deleted_account` while setting `account` to 0, so no account query can return the wizard by mistake. A check constraint keeps `deleted_at` and `deleted_account` set or unset together.
  - Only an offline wizard can be deleted: the update itself requires `online = 0`, so a wizard entering the world cannot be deleted in between.
  - A deleted wizard stays readable by guid, and `Restore` gives it back to its owner.
- Updates that change one wizard report how many rows they changed, through `DirectExecuteCounted`. Deleting, restoring and the online flag are single conditional statements whose count tells success apart from a wizard that is missing, owned by someone else, already deleted or online. There is no read-then-write gap.
- A create inserts the character, its appearance and the guid high-water mark in one transaction. If the commit reports a failure but the stored character matches, the create counts as done, because the reply was lost and not the commit.
- `CharacterRepository` offers synchronous calls for tools, commands and tests. It also offers statement builders and a row reader, so the login server can run the same queries asynchronously. The asynchronous character list pairs the list with the count, which always returns a row, so an empty account is not mistaken for a failed query. Before the database is touched, a create is refused when:
  - its guid or account is zero;
  - it is marked deleted;
  - a name or zone is not UTF-8, holds control characters or is too long.
- `GuidGenerator` hands out ids without a lock from any number of threads, never zero and never twice, and it refuses once the 64-bit range is used up. The highest guid ever used is kept in `id_sequences` and raised with every create, and the generator resumes above it at startup, so removing a wizard's row can never give its guid to a new wizard. One process allocates each kind of id. Character guids are plain sequential numbers until the object id layout is settled in phase 4.

### Character list

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- A wizard is not tied to a realm in Wizard101, so the characters database is shared: the login server lists and creates wizards in it, and every game server loads them from it. The login server opens it next to the login database, and its shipped configuration updates both. A login server with an install in use will serve clients, so it refuses to start without a type dump and both databases.
- MSG_REQUESTCHARACTERLIST runs three asynchronous queries on the session's network thread:
  1. the account's purchased slots;
  2. the count of its live wizards, which always returns a row;
  3. the wizards themselves, only when the count is not zero, at most 256 in creation order.
  Every wizard is encoded before anything is sent. The client then gets MSG_STARTCHARACTERLIST with `Login.Name` and the slots, one MSG_CHARACTERINFO per wizard in creation order, and MSG_CHARACTERLIST with Error=0. A missing account, a failed query, a lost callback or a wizard that cannot be encoded sends only MSG_CHARACTERLIST with Error=1. A player is never shown a partial or falsely empty list, which would invite them to create a wizard they already have. A request made while a list is being built is answered with one more list once it is done, and a third is a strike. Each step first checks that the session is still open and not kicked, and abandons the list otherwise. At shutdown the login server closes its databases, draining every callback, before it stops its network threads, so no callback can post to a network thread that no longer exists.
- `LoginScreenInfoBuilder` fills WizardCharacterCreationInfo, its WizardCharacterBehavior and an empty EquippedItemInfoList from a stored wizard. It encodes them with Transmit|AuthorityTransmit through the MSG_CHARACTERINFO field rule, unwrapped. With no custom name and no equipment the blob is 96 bytes plus the location.

### Character names

Settled on 2026-09-16 under the maintainer's standing direction to decide.

- A wizard stores its name as one number: the first, middle and last name positions, 8 bits each, packed as first << 16 | middle << 8 | last. Each position indexes a name table in the client's CharacterNames.xml. Position 0 of the middle and last tables is empty and means no middle or last name, so a name formats as the first name alone, or as the first name, a space, and the middle and last names joined.
- The client ships a separate copy of each table for each locale, and the copies differ: el, it and pl hold fewer names. So every table is stored per locale, and every check names the locale it checks against.
- The `extractor` tool reads the tables, their text from the .lang file its section names, the disallowed name list and the creation config from the user's own install. It replaces the world database's character_name_part, character_name_disallowed, character_create_school and character_create_option tables in one transaction, so a failure changes nothing, and no extracted row is committed to the repository. `CharacterNameSet` validates the same data in the extractor before it writes and in `sCharacterNameMgr` when the rows load. The manager reads both tables in one consistent snapshot and swaps in new tables only when every row is valid, and otherwise keeps the old tables and reports each problem. It warns when the new tables leave its default locale, set from `Locale.Default`, without human names.
- A disallowed name matches by gender, by locale id when the caller names one, and by index, where an index of 256 or more in the list matches any index. The meaning of locale id 2 and of the index 999 in r806919's list is unconfirmed.

### Automatic setup

Settled on 2026-09-16 at the maintainer's direction, and revised on 2026-09-17 so that a first start asks nothing: when a server or tool cannot work because client data is missing, it finds the data on the user's own machine and uses it. Files found there are the user's own provided files, so nothing is downloaded or committed.

- `ClientLocator` finds installs, meaning folders holding `Data/GameData/Root.wad`, whose `Bin/revision.dat` names the revision, and notes whether `Bin/WizardGraphicalClient.exe` is there. It looks in `AMBROSE_CLIENT_DIR`, installed programs named Wizard101 (up to two folders deep), KingsIsle's default folders, every Steam library including Flatpak and Snap Steam, and Wine, Lutris and Proton prefixes. On WSL it also looks in each Windows drive's KingsIsle folders under `ProgramData`, `Program Files (x86)`, `Program Files` and each user's `AppData/Local`, and in the Windows Steam folder with the libraries its `libraryfolders.vdf` lists, their drive paths mapped to `/mnt`. Steam libraries and folders compare by canonical path, so each is searched once and a duplicate spends nothing from a folder budget. The fixed places are probed first, and the installed-program folders are walked two deep only after them. Fixed places and walks share a budget of 512 folders, and Lutris and Proton prefixes each get their own budget of 1024, enough for every prefix one listing returns. Installs are listed newest revision first, with unknown revisions last.
- It finds type dumps through `AMBROSE_TYPE_DUMP_PATH`, named for a found revision in or beside an install, in the Ambrose data folder's `types` folder or in the working or executable folder, and as any `.json` in the Ambrose data folder itself. A file counts as a dump when its first 4096 bytes parse as a JSON object with a root key named `version` or `classes`, whatever order its keys take.
- Setup runs in a mode, `Setup.Mode` for the servers and `AMBROSE_SETUP_MODE` for the tools: `auto`, the default, `ask` or `off`. Any other value is reported and runs as `auto`. Setup never changes a key the environment or the command line sets.
- `auto` never asks. When `ClientDir` is empty, a server uses the install with the newest revision, preferring one with the client program when revisions tie, saves it to `conf.d/client-data.conf` beside the configuration, and the configuration reloads. When `ClientDir` is set but holds no install, for example on a drive that is not mounted, the newest install is used for that run only, the setting stays as it is, and a warning names the file to edit. When `TypeDumpPath` is empty or not a type dump, it uses the dump built from the install in use, and that path is not saved. A `TypeDumpPath` the command line sets empty stays empty, and no dump is built for it.
- `ask` restores the 3.20 questions on an interactive terminal: which found install to use, saved to the same file, and then the dump. A current dump already built for that install is used without a question. When no dump found is named for the install's revision, it first offers to build one, and when that is declined or fails it offers the other dumps found, noting that none is known to fit. Without a terminal, or once a question times out after `Setup.PromptTimeout`, input closes or a stop arrives, a server logs what it found and the setting to add. In a server, `off` searches for nothing and uses `ClientDir` and `TypeDumpPath` as they are.
- Saving merges the values into `conf.d/client-data.conf` line by line, checks that the new text reads back, and replaces the file through a temporary file. The old file is put back when the reload fails, and a saved value that another file overrides is reported instead of claimed.
- Both servers write the same `conf.d/client-data.conf`, and the login server cannot start with an install but no type dump, so neither server saves `ClientDir` unless a usable dump is in use. The game server still uses such an install for that run and warns that setup tries again on its next start. The login server does not use an install without a dump, starts without client data and tries again on its next start. Installs and dumps whose paths are not valid Unicode are skipped with a warning, and a chosen path that is not valid Unicode is used for that run only.
- `TypeDumpCache` keeps one dump per client revision at `types/<revision>.json` in the Ambrose data folder, which is `%LOCALAPPDATA%/ProjectAmbrose` on Windows and elsewhere `$XDG_DATA_HOME/project-ambrose` when `XDG_DATA_HOME` is an absolute path, or else `~/.local/share/project-ambrose`. A dump is current when the revision and client program SHA-256 recorded in its header match the install. A missing or stale dump is built by running typeextract, the one `Setup.TypeExtractor` names or else the one beside the executable, through `ChildProcess`, and each line it prints goes to the log. A built dump is checked again before it is used.
- A build holds an operating system lock on a lock file beside the dump for as long as it runs, `LockFileEx` on Windows and `flock` elsewhere, so servers started together extract a revision once. The file records the process id, time and host name for people to read, but nothing is judged from them or from its age: a live build is never taken over, and a lock file no process holds is taken at once, because the operating system releases the lock when its holder ends. A holder checks that the lock file's name still names the file it locked before building, and removes the name only while it does. A file system that cannot lock builds without the lock and warns.
- `ChildProcess` runs a program with its arguments passed exactly, no window and no input, and hands each line of its output to a callback as UTF-8. A timeout or stop request ends the program and everything it started, through a kill-on-close job object on Windows and a process group sent SIGTERM and then SIGKILL on POSIX. `Setup.TypeExtractTimeout`, 900 seconds by default, bounds a build. typeextract runs with `--exit-when-input-ends` and an input pipe only its parent holds, so it ends as soon as the server or tool that started it ends in any way. A relative `Setup.TypeExtractor` is resolved once against the working directory and run as that exact path, with no search of `PATH`. A stop signal during a server's start ends a build, a wait for one or a waiting question, and the server exits without reporting ready.
- bindecode, localetool and the extractor follow `AMBROSE_SETUP_MODE` for `--client` and `--type-dump` and save nothing. `auto` uses the newest install and the dump built from it by the typeextract beside the tool, and prints one line naming what it used. `ask` asks only on a terminal, and `off`, or `ask` without a terminal, prints the finds and the flag to pass. A tool never waits on a question unless the mode is `ask`, and Ctrl+C or SIGTERM during a build stops typeextract.
- When the world database's name tables are empty, the game server extracts them from the install in-process and reloads them: automatically in `auto` mode, after a yes on a terminal in `ask` mode, and never in `off` mode. The client data extraction therefore lives in `shared/ClientData` and its world SQL in `database/Extraction`, so a server can run it without linking a tool.

### Type extraction

Settled on 2026-09-17 at the maintainer's direction: Ambrose builds the type dump itself from the user's own client program, so nobody has to find a dump and every client revision gets its own.

- `typeextract` (src/tools/typeextract) emulates `Bin/WizardGraphicalClient.exe` with Unicorn 2. It never launches the game, opens a window or reads a running process, and it writes nothing into the install. It links GPL-2.0 code and runs as its own process, so a crash or runaway emulation never takes a server down.
- The C and C++ runtime DLLs the install ships (ucrtbase, vcruntime140, vcruntime140_1, msvcp140, concrt140, reached through the api-ms-win-crt forwarders) load for real with relocations and run their startup, so formatting, number parsing and type names come from the client's own runtime. Every other import is a stub. The Windows layer (`WindowsApi`) implements the kernel functions the runtime and the client's startup use, deterministically. A kernel function it lacks fails the extraction by name, and a module lookup of kernel32 finds only the functions it has, as Windows reports a missing export.
- Every emulated call has an instruction budget and must return to its sentinel. A fault names the module and offset, and an exception thrown inside a hook stops the emulation and surfaces from the call.
- Nothing is found by per-revision address. The CRT startup's `_initterm_e` and `_initterm` calls give the C and C++ initializer tables. The type map is found on the emulated heap as map nodes whose type name hashes to their key. The Type constructor and PropertyList initializer are elected by call-site votes and must win clearly. RaceManager's race adder is the function naming `enum eRace` outside `RaceManager::InitializeRaces`.
- The run follows the client's own start: TLS callbacks and runtime startup, the C initializers, then the C++ initializers. The guest sees a fixed folder, C:\Wizard101\Bin, so nothing depends on where the install lives, and character types and case mapping for the characters code page 1252 reaches come from Windows' own tables. It then runs every function that calls the Type constructor or PropertyList initializer once, the lazy getters that register template types such as `MadlibArgT<T>`. Finally it adds each race from Root.wad's Races.xml through the race adder, which is how the client fills `enum eRace` at runtime.
- The walker reads each type into format v2 and calls each container's own methods for its name and dynamic flag. Before writing, extraction checks that every eRace property holds every race and builds the server's type catalog from the dump, so a dump the server would refuse is never written. Validation refuses to write when a type name does not hash to its hash, a property hash does not match its type and name, ids are out of order, a container is unknown, a vector is malformed, memory is unreadable or text is not UTF-8. The client registers 14 types and one enum option twice; the first copy is kept, and copies that disagree fail validation.
- Field offsets live in `ClientLayout`, and r801440 and r806919 share them. On Linux, Unicorn builds as a shared library through the overlay triplet in deps/vcpkg/triplets, because its static library defines `crc32` as zlib does.
- On r806919 the dump equals the reference dump except that it also lists 5 classes and 4 properties the reference missed, and keeps 60 empty enum option values, in 30 properties, as empty text where the reference wrote 0. An optimized build extracts it in about 15 seconds and 44 MiB of guest heap; a Debug build takes about a minute.
- The dump goes to types/<revision>.json in the Ambrose data folder unless `--out` names a file. Its root also holds the revision, the executable's SHA-256 and the extractor's name, which the loader ignores and `TypeDumpCache` reads to tell whether a dump is current.

### Client launcher

Settled on 2026-09-17 at the maintainer's direction: the client is driven by a launcher of Ambrose's own, never by a script and never by the retail launcher.

- `launcher` (src/tools/launcher) starts the user's own client against an Ambrose login server. Its `launcher-core` library holds the work, so the tests in `unit_tests` drive discovery, the run folder, the generated configuration, the argument list and every refusal without starting a process, and `Main.cpp` is a thin front end. It replaces the development scripts of milestone 1.21, and it is what milestone 3.24's driver and the desktop app of 17.24 start the client with.
- The install comes from `--client`, `ClientDir` in `launcher.conf`, `AMBROSE_CLIENT_DIR` or `ClientSetup::ForTool`, the same discovery the tools use, which follows `AMBROSE_SETUP_MODE` and names what it would use when it cannot decide.
- The client runs from a folder of Ambrose's own, `client/<revision>` in the Ambrose data folder unless `--run-dir` names another, never the install or a folder inside it, because the client reads `config.xml`, `preferences.xml`, `revision.dat` and `data.dat` by relative name from its working directory and writes its own `config.xml` and `preferences.xml` back there as it exits. Those two are written on every run, from the files the folder already holds so the player's own saved settings survive, or from the install's own files, or from `defaultconfig.xml` in its `Root.wad` under its own root element when the install has no `config.xml`, with only `IsFullscreen`, `Resolution`, `WindowedX` and `WindowedY` changed and `SilentMetricsURL` emptied wherever a file holds it, so nothing outside the machine is reached and the install's own `VersionInfo` and settings stay as they are. Each value is spliced into the template's own bytes by `ClientConfigText`, which keeps the declaration, the indentation, the attribute spelling, the line endings and every other byte, and adds a table, record or key the template lacks in the template's own style, listing an added table in `_TableList` when the template lists its tables. The file is never parsed and written again: pugixml reads it only to refuse a template that is not a client configuration. Checked on r806919 on 2026-09-17: given a configuration rewritten by an XML writer, which differs from the install's own only in its declaration, its indentation and its line endings, the client ignores the file and falls back to its built-in defaults, logging `VidSettingsAuto ... picked UserRes [1600, 900]`, a fullscreen renderer and KingsIsle's own metrics address although the file asked for a 1024x768 window and an empty one; given the same values spliced in as text it honours all of them, and on exit it saves a file byte for byte identical to the one the launcher wrote. A stamp records the install and revision the copies of `revision.dat` and `data.dat` were made from, so they are copied again when either changes and the two generated files are then seeded from the install again. Nothing inside the install is ever written.
- The client always starts with `-L <host> <port>`, `-P 0`, `-A <locale>`, `-D <the install's data folder, absolute and with its trailing separator>` and `-G <log in the run folder>`, because the retail build starts `..\Wizard101.exe`, KingsIsle's own launcher, when it sees none of its own options. `--user` and `--character` pass the client's own `-U` and `-C` through, and `-ST` is never passed: no value may begin with `-`, so nothing a setting holds can reach the client as an option of its own. Startup refuses, with a named reason and a non-zero exit, when no install is found, the client program is missing, patching is asked for, a login host or port is missing, a value makes no sense, the run folder cannot be named, lies inside the install or cannot be written, or the machine is not Windows and so cannot start the client, which is refused before anything is written while `--dry-run` still prints the command.
- A path counts as absolute by the rules of the machine described, `ClientLocator::IsAbsoluteFor`, not those of the host the code runs on, and the install's own folder is made absolute before `-D` is built from it, so a relative `--client` or `ClientDir` still names the data folder the client can find from the run folder.
- `--wait` starts the client through `ChildProcess::Run`, whose kill-on-close job object ends the client with the launcher, and returns the client's own exit code; `--tail` waits the same way and prints the client's log lines. Without either, `ChildProcess::StartDetached` starts the client in a session of its own with no pipes, so closing the launcher leaves the game running.

### Database updates

Update files run through the connector with multi-statement support, so `DELIMITER` is not allowed in them. The `updates` table records each file's SHA-256 hash.

### Tools

Server code is C++. Tools may use whatever language does the job best, and they must work reliably. A tool that reuses server code, such as the archive reader or the ObjectProperty codec, lives in `src/tools/` in C++. Repository tooling such as the codestyle checker, CI scripts, the installer and the design token generator lives in `apps/` and may be Python or shell. A generator whose output the C++ build reads, such as the terminal's token header, commits its output and is checked by regenerating and diffing it, so the servers build with no Node and no Python step in their path. Every tool file carries the branding header.

### Configuration

A `.conf.dist` file contains only its branding header and `Key = value` lines. Each option is documented in `doc/config/<app>.md`. Layers apply in the order `<app>.conf.dist`, `conf.d/*.conf.dist`, `<app>.conf`, `conf.d/*.conf`, persisted live settings, `AMBROSE_` environment variables, then command-line overrides, so every default sits below every local edit and a live edit sits above the files. Environment variable names follow the rule in doc/config/README.md, for example `WorldServerPort` becomes `AMBROSE_WORLD_SERVER_PORT`.

### C++ modules

Settled on 2026-09-13. The code uses headers, not C++20 modules, and CMake's module scanning is turned off. A trial build of a named module worked on MSVC, Clang 18, and GCC 14, but the main benefit, `import std`, is still experimental in CMake, works only with Ninja generators and not the Visual Studio generator, and needs GCC 15. Modules also cannot export macros such as `LOG_INFO` and `sLog`, the vcpkg libraries are headers, and editor and lint tooling for modules is weaker. Moving to modules is planned, not yet scheduled, for when `import std` is no longer experimental in CMake, the Visual Studio generator supports it, and the CI images ship GCC 15 with working module metadata.

### Operations

Settled on 2026-09-13 with the maintainer's direction to favor the most capable option.

| Area | Choice |
|---|---|
| Admin API server | Crow on the standalone Asio layer, serving HTTP and WebSocket from one library |
| Dashboard front end | TypeScript and Svelte, built by Vite into static files the admin API can serve. Settled on 2026-09-18: Svelte 5 in runes-only mode, plain Vite in single-page mode rather than SvelteKit, because a plain build emits no inline script or style and so needs no hash in the panel's Content-Security-Policy, and TypeScript pinned to the 6 line until svelte-check and typescript-eslint accept the 7 line. The route table is typed data, so the router is the project's own over the History API. State is runes in plain modules, since the panel's data is pushed over the event socket rather than fetched |
| Front-end layout | One npm workspace: `packages/ui` is `@ambrose/ui`, exporting raw components through the `svelte` condition with no build step, and `apps/dashboard` and `apps/launcherui` each import it, so the panel and the launcher share one set of components and one set of tokens. Every install is `npm ci` from the committed lockfile, offline from a cache primed for both Windows and Linux by `apps/ci/ci_npm_cache.py`, because the toolchain carries platform-specific native binaries. The front-end build sits behind a CMake option, on by default in CI, so a machine without Node still builds the servers |
| Design system | Settled on 2026-09-18 at the maintainer's direction that every surface look like one product. `design/tokens.json` is the one place a design value is written, and `apps/designtokens/designtokens.py` generates from it the Tailwind theme and semantic layer as CSS, typed constants as TypeScript, a constexpr header for the terminal carrying each token's truecolor, 256 and 16 values, and the tables inside doc/DESIGN.md. Generated files are committed and a check regenerates and diffs them, so no Node is needed to build the servers. Tokens run primitive, semantic and component: only the semantic tier enters Tailwind's theme, and Tailwind's own palette is deleted, so a colour outside doc/DESIGN.md is not a class that exists. The generator refuses to write a palette whose pair falls below the contrast the accessibility rule sets |
| Panel components | Settled on 2026-09-18, built in 17.73 and catalogued in doc/COMPONENTS.md, which also gives the search order an agent follows before writing a new one. Bits UI is the one primitive layer, for behaviour, keyboard and ARIA with no visual opinions; its command primitive is the command palette. shadcn-svelte is a scaffold copied in once per component and then owned and restyled, never a dependency. Tables are TanStack Table headless with our own markup, long lists are virtua, the log console is our own virtualised rows with anser for ANSI rather than a terminal emulator, toasts are svelte-sonner, validation is Valibot with schemas generated from the same C++ source as the types, icons are compiled to inline SVG by unplugin-icons from Lucide, and the three fonts are vendored as latin variable files. Nothing is fetched from another host |
| Charts and live data | Settled on 2026-09-18. uPlot draws every time series and every card sparkline, because it updates a twenty-card overview in under a millisecond, has no animation, and renders gaps natively, which is what a stopped app must look like. Every chart carries a table view, since a canvas is otherwise unreadable to a screen reader. Series are held as typed-array rings over a fifteen-minute live window and history is fetched columnar over HTTP. The socket client is the project's own over partysocket for its backoff and close predicate, with message buffering off so a command can never be delivered late |
| Motion | Settled on 2026-09-18. Svelte's own transitions and springs, plus CSS entry and exit and the View Transitions API behind a feature guard. No animation library. Durations come from one shared module, animation touches only transform, opacity and filter, springs are critically damped, and reduced motion is that module's flag combined with an explicit setting, because the media query is unreliable on older WebKitGTK. Loading is determinate: steps with real numbers, no spinners |
| Gallery and front-end tests | Settled on 2026-09-18. Storybook is the gallery of every shared component in every state, a development dependency never served to an operator. Vitest is pinned to the 4 line, which the Storybook test addon requires, and runs four projects: logic with no browser, components and stories in both Chromium and WebKit with axe failing the run on an accessibility error, screenshots in the Playwright container off the blocking path, and Playwright end to end against a real supervisor. WebKit is not optional, because the launcher window runs in WebKitGTK. Repository checks fail a raw colour, a Tailwind arbitrary value, an inline style carrying a colour, a component with no story, and an absolute URL in the built bundle. The front-end CI job runs on Linux only and is path-filtered |
| Panel surface with each subsystem | Settled on 2026-09-18 at the maintainer's direction. The panel's foundation is built before the rest of the game, and from then on a subsystem is not finished until the panel can show and drive it: the same milestone publishes its status fields, its admin endpoints and events, and its page or section. Phase 17 keeps only the panel's own machinery |
| Look | Settled on 2026-09-17 at the maintainer's direction: one set of tokens and rules in doc/DESIGN.md that every surface follows, the launcher window, the panel, the terminal and any page a server serves. The launcher's window in 3.26 is the same stack in the operating system's own web view, so it shares the dashboard's components. From 2026-09-18 that sharing is a package rather than a convention: 17.73 builds the design system in `packages/ui`, and 17.06 and 3.26 are both built from it, so a surface cannot invent a colour, a spacing step or a duration |
| Process control | An Ambrose supervisor process that starts, stops, restarts, and crash-restarts every app on Windows and Linux, and can itself run under systemd or as a Windows service |
| Remote access | The admin API listens on localhost by default. Any other address requires TLS and the token. Plain HTTP beyond the machine is an explicit opt-in setting, off by default, added on 2026-09-16 at the maintainer's direction; it still requires the token, but sends the token, commands and logs unencrypted, so anyone on the network path can read them and reuse the token. TLS has been served since 17.14 on 2026-09-22: `Admin.CertificateFile` and `Admin.PrivateKeyFile` name a certificate chain and key in PEM, checked when they load for a key that belongs to the leaf, a chain that runs leaf first with each certificate issued by the next, and dates that hold today, and refused by name when they do not, so a listener never opens on a certificate it cannot serve. A reload rereads both files and swaps the pair in one step for the next handshake, with open connections undisturbed and the old pair still serving when the new one will not load. The fingerprint and expiry date are logged when a listener opens and whenever the certificate changes, and a warning names the file from thirty days before it expires. `TlsCertificate` and `TlsServerContext` in `src/server/shared/Admin/` hold this, OpenSSL provides the transport and the certificate reading, and Botan stays the game's cryptography |
| Panel sign-in on an app's admin API | Settled on 2026-09-22 at the maintainer's direction to do it the correct way. Until the supervisor serves the panel with its own users in 17.14, an app's admin API serves the built panel at `/` without a token, files only, and the page signs in once by posting the admin token to `POST /api/session`, which answers with a session cookie and a CSRF token, so the token never stays in the browser. The cookie holds a random 256-bit secret the listener keeps only as its SHA-256. It is HttpOnly and SameSite=Strict and is named after the port, because cookies ignore ports and two apps on one machine must keep separate sessions; the Secure flag and the `__Host-` prefix follow when 17.14 serves TLS. A request that changes something carries the CSRF token in `X-CSRF-Token` and names the listener's own origin, and a socket upgrade names it too. A bearer token and a cookie are never combined: an Authorization header makes the listener ignore the cookie. A session ends after `Admin.SessionIdleMinutes` unused or `Admin.SessionLifetimeHours` in all, and every session ends when the token rotates. Every response carries a strict Content-Security-Policy that loads scripts, style sheets, fonts and connections from the listener alone and allows inline style attributes and nothing else inline, because Bits UI's scroll lock writes the page body's style attribute when a dialog, menu, select or sheet opens and writes it back when it closes, and a blocked write leaves the page unable to take a click. It also carries nosniff, frame denial and a same-origin referrer policy. Every error carries a request id the app's log repeats, a 422 names each field, and a Host header that is not an IP address, `localhost` or a name in `Admin.AllowedHosts` is refused with 400, so a page elsewhere cannot reach the listener by rebinding a name onto it. 17.14 keeps this contract and signs panel users in instead of the token |
| Panel listener | Settled on 2026-09-22, the maintainer having left this one to the build. The panel's own listener follows the same Remote access rule as the admin API, under `Panel.` option names, because it carries more than an app's listener does: the token, sign-in passwords, two-factor codes and session cookies. It is off unless `Panel.Enable = 1`, binds 127.0.0.1 by default and listens on 12000. A bind beyond this machine needs `Panel.CertificateFile` and `Panel.PrivateKeyFile`, or `Panel.AllowPlainHttpRemote = 1`, which is off by default and logs a warning naming the option and what then crosses the network unencrypted; startup refuses an unsafe bind naming the option and exits 1, and a reload that would leave one is refused while the old listener keeps serving. Plain HTTP beyond localhost is offered, as an opt-in the operator has to make and that says what it costs, rather than refused outright. It is one listener implementation shared with the admin API, so the bind rule, the host check, the session cookie, the security headers and the certificate handling have one home; `ListenerSettings` carries the prefix it was read under so every message names the option the reader would change. `supervisor --panel-self-signed` writes a certificate for a panel on this machine and prints its fingerprint |
| Which client revision the server is built for | Settled on 2026-09-23 at the maintainer's direction. None. A server is never compiled against a revision: the type dump and the protocol definitions load at run time from whichever installation the operator has, the type registry is built from that dump, and the ObjectProperty codec is driven by the registry rather than by anything known at compile time, so a server built by somebody with a different client already speaks to that client. `ClientLocator::PinnedRevision` names the revision this project develops against and is read by `IsPinned()`, which nothing outside the tests calls; it stays as a statement of what is regularly tested and must never become a gate. Anything that would refuse an installation, a dump or a message for being an unknown revision needs this row changed first. The version-specific knowledge that genuinely exists lives in typeextract, which reads the client's own structures to build a dump and therefore needs their field offsets, and 3.28 is where it learns to work those out rather than hold a table of the builds somebody happened to try. |
| What a plugin may be | Settled on 2026-09-23, the maintainer having left this one to the build. A plugin is never an executable and never a library. A panel tool is a page: HTML, CSS and script, served from the panel and run in a frame of its own with no session, no cookies and one audited bridge, which is a boundary the browser enforces rather than one our review promises. Behaviour on the server is source the operator builds through the module discovery the build already has. Nothing in any install path loads a binary. An executable was refused because no scan makes arbitrary native code safe on a machine that holds a game database, and because the panel is where the data and the sign-in already are, so a tool that opens there is both the safer answer and the one that works on every operating system without being built for each. A tool that genuinely needs the desktop is a server module plus a page, not a program a store hands out. |
| Panel users | Added on 2026-09-17 at the maintainer's request for hosting panel parity. Panel users, schedules, backup records and graph history live in the supervisor's own SQLite file, so the panel works before any game database exists. Passwords are hashed with Argon2id, two-factor sign-in uses TOTP, and permissions are roles plus per-server grants in the style of Pterodactyl sub-users. Argon2id comes from Botan, changed on 2026-09-22 from libsodium, which this entry named when panel users were planned: libsodium's port builds through autotools that a clean Linux machine does not have, so it broke the Linux legs and would have broken a fresh clone, while Botan is already the project's cryptography and already provides Argon2id in the same PHC string form. One library rather than two, and nothing to install by hand before a build. `PanelUsers` in src/server/apps/supervisor/Panel hashes at one lane, 64 MiB and three passes, and the string carries its own parameters so a row hashed at an older cost still opens after the cost is raised |
| Backups | Consistent logical dumps of every Ambrose database with the config, data and type dump folders, in one zstd archive with a SHA-256 manifest, stored locally or in opt-in S3-compatible storage |
| Packaging | The supervisor installs itself as a Windows service or systemd unit, a Docker image and Compose file run the whole stack, a Pterodactyl egg runs Ambrose under an existing Pterodactyl panel, and a desktop tray app starts everything from one icon |

### Continuous integration

Settled on 2026-09-16 at the maintainer's direction to make CI cheaper. A private repository gets 2,000 Actions minutes a month, and with no payment method a run over the quota is blocked, not billed. Building every leg on each push cost about 105 billed minutes, since Windows minutes count twice.

- `.github/workflows/core-build.yml` runs the checks job: codestyle and its self-tests, the CI script self-tests, the forbidden file scan and the commit trailer check. It runs daily at 06:17 UTC, on pushes to main that change `.github/`, `apps/ci/` or `vcpkg.json`, and on pull requests. A scheduled run checks the commits of the last eight days for trailers.
- `apps/ci/ci_select_legs.py` picks the build legs:
  - A schedule slot builds Windows MSVC on Sundays and Wednesdays, the ASan with UBSan and TSan legs on Sundays, and Linux GCC, Clang and libFuzzer on the first Sunday of the month. A slot's leg builds only when code outside `doc/` and Markdown files changed since the commit its last finished build tested, which the checks job reads from the Actions API. When earlier runs cannot be read, every leg of the slot builds.
  - A push or pull request that changes the workflow, `ci_build.py`, `ci_vcpkg_cache.py` or `vcpkg.json` builds Linux GCC as a smoke test. A pull request's changes are taken from its merge base.
  - A pull request labeled `ci:<option>`, in any letter case, builds that option's legs. The options are each leg's name, `weekly`, `linux`, `all` and `none`, and several labels build every leg they name. Labels stay on the pull request, so each later push builds them again, and a newer run for the same pull request cancels the older one.
  - A manual run builds the option it names, `linux-gcc` by default.
- The front-end job builds and tests `packages/ui`, `apps/dashboard` and `apps/launcherui`. It runs on Linux only, because Windows minutes count twice for no benefit, and only when a front-end folder or the lockfile changed. It restores the npm cache and the Playwright browsers by key rather than downloading them, and it fails on a type error, a lint error, a failed component or story test, an accessibility violation or a repository check. Screenshots and the end-to-end run are a separate container job on a schedule or a label, never on the blocking path.
- Setting the repository variable `AMBROSE_CI_BUILDS` to `off` stops scheduled, push and pull request builds. The checks, manual runs and cache keepalives still run.
- Each operating system keeps one vcpkg binary cache, keyed by the archives it holds. A build prunes archives older than 30 days that it no longer uses, and saves a new cache only when the set changed. GitHub drops caches unused for 7 days, so scheduled checks restore the Linux cache daily. On Sunday and Wednesday slots that do not build Windows, a short job restores the Windows cache.
- `python apps/ci/ci_usage.py` adds up this month's billed minutes through `gh`, for a look before a large manual run.
- Pushes build nothing, so run `ctest` with a preset before every push. Besides the unit tests it runs `codestyle.selftest`, `codestyle.tree`, `ci.selftest` and `ci.forbidden`, the checks CI runs.

### Experimental features

Settled on 2026-09-16 at the maintainer's direction. The project is experimental and rules out no idea, feature or approach: each is planned work or an available experimental feature, usually opt-in. A scope cut or deferral is planned work, placed in a named milestone when one fits and otherwise marked planned, not yet scheduled. These rules keep that work lawful:

- Bring your own files. Client data, models, captures and other projects' data sets, such as another project's decks, teleport or vendor data, are read at runtime from the user's own copy, the way emulator players bring their own ROMs, and an importer may read such a copy the user has. Nothing the project does not own is hosted, mirrored, redistributed or committed.
- Licensing, settled on 2026-09-18 at the maintainer's direction that anything publicly licensed may be used. The repository is MIT licensed in LICENSE, and THIRD-PARTY-NOTICES.md lists every library with what its licence asks, updated in the same commit that adds a dependency. Two licences ask for more than attribution and are handled rather than avoided: the MariaDB connector is LGPL and is always loaded as a shared library, and Unicorn is GPL-2.0 and is linked only by `typeextract`, so that one program is distributed under GPL-2.0-or-later while every other program stays MIT. Files carry no licence header, because a file here carries only its branding header and one-line brief; LICENSE covers the repository.
- Bring your own license. Proprietary SDKs, tools and assets, such as Gamebryo, back optional features only for someone holding their own license, through a build option that points at their licensed copy. Nothing from them is committed, builds without them keep working, and leaked copies are never used.
- GPL and AGPL tools and libraries may be used, as tools or linked code (settled on 2026-09-17). typeextract links Unicorn (GPL-2.0).
- Modified client executables are never distributed. Users apply patches, as binary diffs or a hook DLL of Ambrose's own code, to their own copy locally, at their own risk. Local binary modification and runtime hooks are experimental features for the user's own copy, never the pinned development install.
- Capturing live KingsIsle sessions, contacting KingsIsle servers, reading a running client's memory and fetching from KingsIsle's patch servers may break KingsIsle's terms and put the user's account at risk. Each is the user's own choice on their own account and machine, and its documentation states that risk plainly.
- Security-sensitive choices are explicit settings, off by default, each documented with its risk: plain HTTP for the admin API beyond localhost, serving executables from the patchserver, discovering a public address through an external service, and a separate development reload socket. Executables from the patchserver must match a manifest signed with the operator's own key.
- Web clients and model previews read the user's own install, such as a browser build that loads the user's local files or a previewer served from the operator's own install. They never serve KingsIsle assets from a public host.
- The clean-room rule and the rule that client files are never committed still hold, and the retail launcher or patcher never runs against the pinned development install, though a separate copy may be patched.

Questions decided under these rules the same day:

- Client identifiers, such as locale keys, template ids, zone and location names, and internal quest and goal names, may be committed in authored SQL, and display text may not. A door destination table that holds only such identifiers may be committed, even when its rows were matched from client location names. This unblocks 6.14, 7.05 and 10.16.
- Ambrose builds its own type dumper. Revised on 2026-09-17: typeextract (3.21) emulates the user's own client program from disk to build the dump, so it neither launches the game nor reads a running process, and servers run it automatically. No client revision is pinned: Ambrose follows the revision of the user's install (3.23).
- An embedded Lua runtime, added through vcpkg, runs the client-shipped minigame Server.lua scripts from the user's own install in 13.11, so the scripts reload live.
- Battlegrounds, castle magic and monster magic stay in scope, in 14.14, 14.15 and 15.17-15.20.

### Still open

Decisions that block later milestones are listed under Decisions needed in doc/ROADMAP.md. Propose them to the maintainer when their milestone is next.

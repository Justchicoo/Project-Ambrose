<!-- Project Ambrose by Imjustchico: Content and development tool suite, adapted from the AzerothCore ecosystem. -->

# Tools

The Ambrose tool suite takes the AzerothCore content toolchain and changes it in three ways for Wizard101. First, Wizard101 data is hash-keyed BINd ObjectProperty data stored in KIWAD archives, not DBC/ADT files. That makes the type registry dumper and the WAD/BINd decoder the base that every other tool sits on. Second, the world is spatial and quest-driven, so the most valuable authoring tools are quest/dialogue and zone/spawn editors, plus in-game GM build sessions that work inside the retail client. Third, the project rules come first: nothing extracted from the client is committed, and no database edit goes unrecorded. Authoring tools (Studio, capture_to_sql, GM build sessions, content DSL) write dated pending SQL updates (data/sql/updates/pending_db_world/YYYY_MM_DD_NN.sql). Direct GM edits apply at once and are journaled, so they can be exported the same way. Each file passes a codestyle-sql check by construction and is applied by a hash-tracking updater. All client-derived indexes (template names, locale strings, zone geometry, minimaps) are built locally from the user's r806919 install into a git-ignored cache.

One source of truth ties the suite together: a per-table schema definition file in data/schema/world/<table>.yaml, inspired by WDE DbDefinitions and Keira field models. The same file drives the Studio editor forms and pickers, the gameserver startup and reload validator, the reload-command mapping and generated doc/world/<table>.md pages. Tables are defined as data, so adding a world table means adding one file.

Current repo state (3.25): src/tools/bindecode, src/tools/dbimport, src/tools/extractor, src/tools/launcher, src/tools/localetool and src/tools/typeextract are built; src/tools/{template_extractor, wad_extractor, zone_extractor} exist as empty folders. apps/{ci, clientdriver, codestyle, installer} exist. data/sql/base/db_world is empty. scripts/Commands is empty. ARCHITECTURE.md already sets the rules that tools depend only on database, shared and common and that GM commands reload single tables.

Build order:
1. **Early foundation:** codec_registry, wad_extractor, template_extractor, dbimport + codestyle-sql, the ambrose.sh dashboard, the reload framework (4.15) and its admin API (17.12).
2. **Mid authoring:** Ambrose Studio (database editor) with quest/dialogue and NPC/loot/vendor sub-editors, GM build sessions (.spawn/.session), zone_extractor + zone/spawn editor, wadview, spell inspector, create_module.
3. **Late:** capture_to_sql (capture sources decided on 2026-09-16; capturing live KingsIsle sessions is the user's own choice and risk), a server-side event/action script table + editor, a Lua scripting module, client_content_builder + manifest_builder for custom WAD overlays (blocked on a test proving the client accepts a changed WAD), navmesh_generator, a content DSL.

Formerly rejected ideas, reopened on 2026-09-16 at the maintainer's direction as experimental opt-in features:
- Client data in the style of acore.sh: bring your own files, as players of emulators bring their own ROMs. Every tool reads client data only from the user's own install. The project never hosts, mirrors, downloads or ships client files, or models extracted from them.
- 3D model previews in the style of Keira: the previewer serves models rendered from the operator's own install to the control center or a browser. It listens on localhost unless the operator opens it, and it never becomes a public host of client files.
- Direct-to-database GM edits in the style of .npc add/SaveToDB: they apply at once. Every such edit is journaled, audited and exportable as a pending SQL update, so git can catch up with the database.
- Serving executables from the patchserver: off by default. The operator lists each executable and its SHA-256 in a manifest signed with the operator's own key, and the patchserver refuses any file that does not match. The client checks only CRC-32, so players must trust the server's operator, and the patching docs say so.
- Gamebryo: a contributor who holds their own Gamebryo license may build the optional parts that use it against their licensed copy, through a build option that points at it. Nothing from the SDK is committed, and every build without it keeps working. The leaked Gamebryo SDK repos are never used.

Unverified assumptions behind some tools:
- ZoneData .dds files work as minimaps. The anatomy research says they are equipment textures, so minimaps probably need to be rendered from the zone NIF or the nav mesh.
- A later WAD can override paths in Root.wad.
- MessageManager module CRCs are not checked during the handshake.

Sources: the actools, anatomy, patchmod and renderers research results in the brief, and doc\ARCHITECTURE.md.

## Early

### launcher (built in 3.25)

Starts the user's own client against an Ambrose login server, on any machine that has a client, without ever running KingsIsle's launcher or writing inside the install. `launcher --help` lists its options. It finds the install the way the servers do, through `--client`, `ClientDir` in its own `launcher.conf`, `AMBROSE_CLIENT_DIR` or the discovery in `ClientLocator`, and builds a folder of its own for the client to run from, `client/<revision>` in the Ambrose data folder unless `--run-dir` names another, never the install or a folder inside it: `config.xml` and `preferences.xml` written on every run, from the files that folder already holds or else the install's own, with the window mode and size asked for and `SilentMetricsURL` emptied and every other byte left as the template has it, because the client ignores a configuration that has been parsed and written again, and copies of `revision.dat` and `data.dat`, the other files the client opens by relative name. The client always starts with `-L <host> <port>`, `-P 0`, `-A <locale>`, `-D <the install's data folder>` and `-G <log in the run folder>`, because the retail build starts KingsIsle's launcher when it sees none of its own options, and `--user` and `--character` pass the client's own automatic login and character options through for the 3.24 driver. `--dry-run` prints the run folder and the exact command and starts nothing, `--wait` returns the client's own exit code and ends the client if the launcher is stopped, and `--tail` prints the client's own log lines while it runs; without either the client is started detached. It exits 1, naming the cause, when no install is found, the client program is missing, patching is asked for, a login host or port is missing, a value begins with `-`, the run folder lies inside the install or cannot be written, the machine is not Windows and so cannot start the client, or the client cannot be started, and 2 on bad usage. Its tests run as `unit_tests --gtest_filter=Launcher*` and as the `Launcher` CTest, which runs the built program against a synthetic machine. doc/config/launcher.md documents every option. It replaces the development scripts of milestone 1.21, and milestone 16.13 grows a player launcher that patches its own copy of the install from an Ambrose patch server on top of it.

### clientdriver (built in 3.24)

Drives the user's own client through a scenario with nobody at the keyboard, so client behavior is checked the way a player would check it. `python apps/clientdriver/drive.py run` drops and lets the server rebuild its own `ambrose_driver_*` databases, starts a scratch login server on its own port with its logs in the run folder, creates the account it logs in with over that server's console, starts tshark on the loopback adapter, starts the client through `launcher --wait` behind a guard that watches the client and every helper that client starts, and nothing else on the machine, from the moment the client exists until after it is gone, and kills that whole tree the moment one of them connects off this machine, runs the scenario, then leaves a screen the client can be quit from, stops the client, the server, the capture and last of all the guard, drops its databases and writes the report. `check` says in one line whether a run is possible and exits 77 when it is not, `capture-refs` rebuilds the reference crops from a live client, and `scenarios` lists what each scenario needs. Scenarios are data, not code: `apps/clientdriver/scenarios/*.json` holds a title, what it requires, the login server settings its assertions depend on, the messages it expects the server not to handle yet, and steps that each wait on a line of the server's log, a line of the client's log, a crop of the window or a row of the scratch database within their own timeout, so no step waits a fixed time and a failure names the pattern or screen it waited for. `apps/clientdriver/references.json` says which crop of the window identifies each screen and where each press lands, measured at one revision, window size and interface scale; the crops themselves are pictures of the client, so they are never committed and `capture-refs` rebuilds them into the Ambrose data folder. Nothing the client produces enters the tree: every report, screenshot, capture and log goes to `%LOCALAPPDATA%/ProjectAmbrose/clientdriver/runs/<id>/`. Every run also checks itself, and a check whose subject was never measured fails rather than passes: the install read and no file under it added, removed or changed; no connection off this machine, with a guard that ran; no message the server did not handle outside the scenario's own list, and none it could not read at all; no server WARN, ERROR or FATAL outside its allow-list; no client line about a message it does not know; no line where the client opened a page outside itself; no process or database left behind; a screenshot for every step that changed the screen; every step the driver took around the scenario done; and every step of the scenario passed, or, for a scenario that expects to fail, one of its own steps failed. Input is window messages, never global input: text is `WM_CHAR` one character at a time and needs nothing of the window, but the client's interface discards mouse messages unless its window is the active one, so a press borrows the cursor and the foreground for about a second and hands both straight back, and modifiers are never faked because the client reads them with `GetAsyncKeyState`. The window is never minimized, maximized or sent Alt+Enter, all of which the client answers by going fullscreen. One client fact shapes every run, and it was measured rather than assumed: a client that has been refused a login opens a KingsIsle page in the machine's own browser the moment it is asked to quit, and it keeps doing so after the dialog is pressed away and after the login window is back, because the state outlasts the screen; a client admitted since the refusal does not. So `references.json` carries that as a rule over the client's own log, and the teardown reads it and ends such a client rather than asking it to quit. Its self-tests need no client and run anywhere, as `clientdriver.selftest`; each scenario is its own `client`-labelled CTest, `clientdriver.<scenario>`, which exits 77 and names what is missing on a machine that was not asked for a client run with `AMBROSE_CLIENT_DIR`, the same opt-in every client-labelled test takes, or that is not Windows, has no install, no built programs, no database, no capture, or no crops that still fit the boxes they are used with, so an ordinary `ctest` run never starts a client and a machine without one stays green. Every one of them also carries the label `driver`, and only the scenarios carry `client`, because a CTest label filter is a regular expression, which a label holding the word client inside it would have caught: `ctest -L driver` is the driver's own tests, `ctest -L driver -R clientdriver\.` is its four scenarios, and `ctest -LE client` keeps the self-tests while dropping everything that needs an install. `ctest -L client` is wider than the driver: every client-gated C++ test carries that label too.

### extractor (built in 3.14)

Extracts world database rows from the user's own install. `extractor names` reads every character name table in every locale with its text, the disallowed name list and the schools and creation options a new wizard is offered. It replaces those world tables in one transaction, writes the SQL to a file with `--sql`, or checks everything and writes nothing with `--dry-run`, which also checks the world tables of a database it is given. `extractor --help` lists its options. It reads the install, type dump and world database named by `--client`, `--type-dump` and `--world-db` or by `AMBROSE_CLIENT_DIR`, `AMBROSE_TYPE_DUMP_PATH` and `AMBROSE_WORLD_DATABASE_INFO`, and the world tables must already exist, which dbimport creates. When no install or type dump is named, it checks its database and output options first and then follows `AMBROSE_SETUP_MODE`, saving nothing. `auto`, the default, never asks: it uses the install with the newest revision found on the user's machine and the type dump built from it in the Ambrose data folder, running the typeextract beside the tool when that revision has no current dump, and prints one line naming what it used and the flags that choose otherwise. `ask` asks on a terminal which install to use, uses a current dump already built for it without asking, and otherwise offers to build one before offering other dumps found, waiting `AMBROSE_SETUP_PROMPT_TIMEOUT` seconds, 120 by default, for each answer. `off`, or `ask` without a terminal, prints what it found and the flag to pass. Ctrl+C during a build stops typeextract. bindecode and localetool do the same, and localetool needs no type dump. Later extractors join it as more commands.

### typeextract (built in 3.21)

Builds the type dump of the user's own install by emulating its client program, without launching the game or writing into the install. The C and C++ initializers, the lazy type getters and the client's own race adder rebuild the client's type registry, which is then checked and written as format v2. `typeextract --help` lists its options. It reads the install named by `--client` or `AMBROSE_CLIENT_DIR`, or else the newest revision found on the machine. It writes `--out` or types/<revision>.json in the Ambrose data folder, and `--compare <dump>` prints every difference from another dump, which is read before extracting, so it may name the output file. When the revision or the data folder cannot name the default file, it asks for `--out`. `--exit-when-input-ends` makes it exit with code 1 as soon as its standard input ends, which the servers and tools use so a build never outlives the process that started it. It exits 0 on success, 1 when extraction, validation or writing fails, and 2 on bad usage.

### localetool (built in 3.13)

Finds the keys whose text matches, checks that a key exists and prints its text, dumps a table, and lists the installed locales with the files each skips, all from the `.lang` files of the user's own Root.wad. `localetool --help` lists its options. It reads the install named by `--client` or `AMBROSE_CLIENT_DIR`, or else the one `AMBROSE_SETUP_MODE` finds as the extractor does, and writes nothing.

### bindecode (built in 3.11)

Prints BINd entries of any KIWAD archive in the user's own install as ordered JSON, lists entry names, and sweeps an archive for decode failures and unknown classes, which feed the type registry work. `bindecode --help` lists its options; it reads the install and type dump named by `--client` and `--type-dump` or `AMBROSE_CLIENT_DIR` and `AMBROSE_TYPE_DUMP_PATH`. When they are not named, it follows `AMBROSE_SETUP_MODE` as the extractor does. An archive named by its path opens before anything is searched, and the install holding it, when there is one, is the install its type dump comes from. It writes nothing itself; a type dump built for it goes to the Ambrose data folder.

### codec_registry (type registry dumper + schema generator)

Build the class/property hash-to-name-and-type registry that every BINd, template, zone and GUI tool needs. BINd files carry only 32-bit hashes (anatomy research: 2,993/3,000 Root samples are raw hash-keyed BINd). Nothing that decodes BINd can work without it.

- **Form:** CLI (C++ in src/tools, sharing src/server/shared ObjectProperty code)
- **Inspired by:** WoWDBDefs / WDBX Editor definition files; map_extractor's 'read the user's client' model
- **Lives in:** src/tools/codec_registry/
- **Needs:** shared ObjectProperty binary codec (decode); common hashing utilities

**Key features**

- Reads the user's own WizardGraphicalClient.exe (r806919) statically to recover class names, property names, types and hashes; no running client needed
- Writes a local git-ignored registry cache plus a checked-in, hand-reviewed list of only the class/property names Ambrose code uses
- 'regcheck' mode: decodes a sample (or all) of the 134k BINd files in Root.wad and reports unknown hashes or leftover bytes as the acceptance test
- Emits C++ typed structs/codegen inputs for src/server/shared ObjectProperty
- Pins revision r806919 and refuses other revisions unless told to
- Content validator: rejects world rows or custom templates that use a class or property the client does not know

### wad_extractor

List, stream and inflate KIWAD entries from the user's 3,589 GameData WADs (550,616 entries, 34.6 GB unpacked) into a local cache or straight into other tools. This is the lowest layer of every extractor.

- **Form:** CLI (C++), also exposed as a library used by the other tools
- **Inspired by:** map_extractor + Ladik's MPQ Editor / StormLib (archive layer); AzerothCore extractor.sh menu
- **Lives in:** src/tools/wad_extractor/
- **Needs:** src/server/shared archives (KIWAD reader), common CRC32 helper

**Key features**

- Streaming entry reads (Mob-WorldData.wad is 1.27 GB); never loads whole WADs
- Verifies per-entry CRC-32 (poly 0xEDB88320, init 0, no final XOR; matched 54/54 entries) and WAD HeaderCRC
- Filters by glob/extension; skips leaked art-source junk (.psd, .mov, Thumbs.db, .mine)
- Detects the BINd container and hands it to the codec; handles UTF-16LE .lang files
- Checks install path and revision before running; output goes to a git-ignored cache folder
- Can import a pre-extracted cache the user built from their own install, for example on another machine; it never downloads pre-extracted client data from a host

### template_extractor (template index builder)

Turn the 104,869 ObjectData templates, 18,173 Spells, 599 Decks, 740 TalentData, TemplateManifest.xml (template id to path) and 33,932 Locale .lang files into a local, read-only SQLite index. Every ID picker, name lookup and preview in the other tools reads from it.

- **Form:** CLI (C++)
- **Inspired by:** Keira3's bundled read-only client SQLite (sqlite.db); SpellWork reading map_extractor's DBC output
- **Lives in:** src/tools/template_extractor/
- **Needs:** codec_registry, wad_extractor, BINd decoder in shared

**Key features**

- Local SQLite index (git-ignored), rebuilt from the user's install; the Studio treats it like Keira's sqlite.db
- Tables: template (id, class, name key, path, school, etc.), spell, deck, talent, locale_string, template_manifest
- Decodes names through the Locale keys so pickers search in English
- Records the source revision (r806919) in the index metadata
- Optional: generates server-owned world rows (mob/npc template stubs) as a pending SQL update, never client data; rows that need client values go into a git-ignored local overlay built per user
- Reserves a custom template-id range and fails on collision with retail TemplateManifest ids (duplicate ids silently replace retail templates)

### dbimport + SQL updater + codestyle-sql

Create and update the login, characters and world databases from base/ plus dated updates. Enforce SQL style so that generated and hand-written content is idempotent and consistent. Every other authoring tool relies on it.

- **Form:** CLI (C++ dbimport) + Python/shell CI checks
- **Inspired by:** AzerothCore dbimport, updates table (SHA-1 hash + state), apps/ci/ci-pending-sql.sh, apps/codestyle/codestyle-sql.py
- **Lives in:** src/tools/dbimport/, apps/ci/, apps/codestyle/
- **Needs:** src/server/database connection pool and updater; data/sql/base/db_world populated

**Key features**

- updates table: name, SHA-1 hash, state (RELEASED/CUSTOM/MODULE/PENDING/ARCHIVED), timestamp, speed
- Redundancy mode: re-applies an edited pending file on dev when its hash changes (so Studio re-saves work)
- Also applies module data/sql folders
- ci-pending-sql: renames pending files to YYYY_MM_DD_NN.sql on merge
- codestyle-sql: no tabs, trailing whitespace or double blank lines; final newline; every INSERT preceded by a scoped DELETE; column names match base/ schema; Ambrose file header present
- Schema check: SQL columns and types must match data/schema/world/*.yaml

### World schema definitions + doc generator

One data file per world table as the single source of truth for column types, enums, bitflags, foreign keys (with the table or client index each points to), primary keys, record mode and reload command. The Studio, the server's startup validator and the docs are all generated from it.

- **Form:** Data files + small generator CLI
- **Inspired by:** WDE.DatabaseEditors DbDefinitions JSON (174 files); Keira3 field models/Options/Flags; AzerothCore wiki per-table pages
- **Lives in:** data/schema/world/ (definitions), src/tools/schemagen/ (generator), doc/world/ (output)
- **Needs:** First world tables in data/sql/base/db_world

**Key features**

- data/schema/world/<table>.yaml: fields, value_type (TemplateId, ZoneId, LocaleKey, SpellId, QuestId, Flags, Enum), default, read_only, fk
- record_mode: single-row vs multi-row (DELETE+INSERT per key), composite keys
- reload: name of the .reload subcommand
- Generates doc/world/<table>.md with a hand-written notes section kept separate
- Generates C++ loader validation tables so the gameserver rejects bad rows at startup and on every reload, keeping the old store
- Written from scratch; WDE/Keira definitions are not ported

### ambrose.sh / ambrose.ps1 dashboard

A single entry point for contributors and agents: install deps, compile, run the extractors against their install, set up and update the databases, run login/game/patch servers, run tests, and create or list modules.

- **Form:** CLI script (bash + PowerShell)
- **Inspired by:** acore.sh (apps/installer/main.sh ordered menu array) and extractor.sh/.bat
- **Lives in:** apps/installer/ (ambrose.sh, ambrose.ps1 at repo root as thin wrappers)
- **Needs:** CMake build, dbimport, extractors existing

**Key features**

- Ordered 'key|alias|description' menu; works interactively by number or directly by name (ambrose.sh extract templates)
- init: runs the servers' automatic setup (3.22) once, which finds the newest client install, builds its type dump and extracts the name tables, and writes the database settings into conf
- extract: menu for wad, templates, zones, all
- db: setup / update / squash
- run: loginserver, gameserver, patchserver; test: ctest
- module new/list; studio (launch local web editor); wadview
- Reads client data only from the user's own install and never downloads it from a host the project runs. An opt-in step that fills a separate copy from KingsIsle's patch servers is planned, not yet scheduled; using it is the user's own choice on their own account and machine, may break KingsIsle's terms, and never touches the pinned development install

### Reload channel (.reload + admin API)

Let editors, GMs and operators push world database changes into any running gameserver without a restart, so a tester in the Wizard101 client sees a change within seconds.

- **Form:** In-game GM command + console command + phase 17 admin API
- **Inspired by:** AzerothCore cs_reload.cpp (~117 subcommands); WDE.RemoteSOAP per-editor reload commands
- **Lives in:** src/server/shared/Reload/ (4.15 framework); src/server/scripts/Commands/cs_reload.cpp; the admin API reload routes (17.12)
- **Needs:** Reload framework (4.15); global managers loading world tables; CommandScript system; GM security levels; admin API (17.02, 17.05)

**Key features**

- cs_reload.cpp: one subcommand per world table plus groups (all, quests, locales), security level gated
- Every reload builds the new store off to the side, validates it, swaps it atomically, and keeps the old store with every error reported on failure
- `POST /api/reload/{target}` and `GET /api/reload` accept the same targets on every server, gated by the admin token and security level, and every call is audited. A separate dev reload socket in the WDE.RemoteSOAP style is an opt-in setting, off by default; it opens a second control port outside the admin API, so anyone who can reach it can reload the world
- Studio and the build-session tools call it after 'Execute'
- Reload names come from the schema definitions

## Mid

### Ambrose Studio (world database editor)

The main content editor: schema-aware forms over the world database with linked navigation between entities, a live SQL diff preview, and 'Save as pending update'. Content creation becomes fast, and the SQL never drifts from git.

- **Form:** Local web app served on localhost (C++ backend in src/tools using shared/database, simple TS/HTML frontend); no client files bundled, since it reads the user's own install at runtime
- **Inspired by:** Keira3 (EditorService diffQuery/fullQuery, handler services, route guards, unused GUID search); WoW Database Editor (sessions, diff viewer, remote reload)
- **Lives in:** src/tools/studio/
- **Needs:** World schema definitions, dbimport/updater, template_extractor index, reload channel, world tables for NPCs/quests/spawns/loot

**Key features**

- Main entities: NPC/Mob template, Quest, Zone, Spawn, Loot table, Vendor/Shop list, Deck, Dialogue; sub-editors open only after a main entity is chosen
- Live diff SQL (UPDATE of changed columns only; DELETE+INSERT for multi-row tables) with highlighted preview, Copy, Execute (local dev DB) and Save-as-update
- 'Change session': bundles every edit made while building one quest line into one pending_db_world file, with a diff viewer before writing
- Linked navigation: every foreign-key field has a picker (search by English name from the local template index) and a jump-to link (quest -> giver NPC -> spawn -> zone)
- Unsaved-change dots per table; forms generated from data/schema/world/*.yaml
- After Execute, calls the reload channel for the touched tables
- Unused-id finder for the custom template/quest id range; collision check against the retail TemplateManifest
- Help '?' on each field links to doc/world/<table>.md

### Quest and dialogue editor (Studio module)

Author Wizard101 quest lines end to end: quest template, goals (built from existing client goal classes), givers and turn-ins, prerequisites, rewards, and the NPC dialogue that opens and closes them, with previews that look like the game.

- **Form:** Studio sub-editor (local web app)
- **Inspired by:** Keira3 quest editor (template, starter/ender, offer/request text, quest-chain graph, quest preview); Keira SAI comment generator
- **Lives in:** src/tools/studio/ (features/quest, features/dialogue)
- **Needs:** Quest engine and quest world tables in game/Quests; NPC and spawn tables; locale index from template_extractor

**Key features**

- Quest-chain graph built from prerequisite/next columns; click a node to open it
- Goal editor with goal-type-aware parameter labels (defeat mob in zone, talk to NPC, collect item, enter area), validated against client-known goal classes
- Dialogue tree editor: lines tied to Locale keys (existing retail text via the index) or new custom locale rows; speaker NPC, portrait, next/branch
- Preview panel rendering the quest dialog, goal text and rewards using the user's Locale strings
- Reward pickers (gold, XP, items/templates, training points) with name lookup
- Checks: unreachable quests, missing turn-in NPC spawns, goals pointing at zones with no matching mob spawn
- All output goes through the Studio change session into a single pending SQL update

### NPC, loot and vendor editors (Studio modules)

Edit server-owned NPC/mob rows: which client template an NPC uses, its interaction type, shop lists, loot tables with chances, and mob deck/level. Previews come from the client index.

- **Form:** Studio sub-editors
- **Inspired by:** Keira3 creature editor (template, spawns, equipment, vendor, loot templates)
- **Lives in:** src/tools/studio/ (features/npc, features/loot, features/vendor)
- **Needs:** Object/NPC templates and spawn tables in game; loot and shop systems

**Key features**

- NPC template rows keyed by client template id (picker), with the object's display name and school from the index
- Loot table multi-row editor with chance totals, grouped drops, and a warning on 0% or over 100% groups
- Vendor/shop list editor with item pickers and price columns
- Mob deck assignment linked to the spell inspector
- 'Where used' panel: quests, spawns and zones referencing this NPC

### GM build sessions (.spawn / .session / .zone commands)

Place and tune spatial content from inside the retail Wizard101 client (spawn position, yaw, patrol paths, teleporter targets). Edits apply live on dev and are recorded as a pending SQL update. The opt-in direct-to-database mode also saves them to the database at once, journaled, so the database never changes silently. On other servers the same commands need an admin security level, apply live, and are audited.

- **Form:** In-game GM chat commands
- **Inspired by:** AzerothCore cs_npc (.npc add/move/near), cs_wp (waypoints), cs_pooltools (session that dumps SQL to sql.dev log)
- **Lives in:** src/server/scripts/Commands/ (cs_spawn.cpp, cs_session.cpp, cs_path.cpp, cs_zone.cpp)
- **Needs:** Object streaming + movement in world, zone transfers, CommandScript + GM levels, spawn world table, reload channel

**Key features**

- .session start <description> / .session undo / .session end: end writes data/sql/updates/pending_db_world/YYYY_MM_DD_NN.sql (scoped DELETE + INSERT with @variables)
- .spawn add <templateId|name> at the GM's position and yaw; .spawn move / turn / delete / near / info
- .path add/show/clear for patrol waypoints; .zone tele / .zone info
- .dialogue test <id>, .quest start/complete <id> for quick testing of Studio content
- Security levels per command in cs_spawn.cpp, cs_session.cpp; open to GMs on dev, admin level on other servers, and audited everywhere
- Changes are streamed live to clients in the zone

### zone_extractor

Turn each zone WAD (3,356 contain gamedata.bin) into server data: retail spawn points, triggers, volumes, portals/teleporters, path data, collision (collision.bcd), nav (zone.nav), plus a local minimap/top-down geometry cache for editors.

- **Form:** CLI (C++)
- **Inspired by:** vmap4_extractor + vmap4_assembler + mmaps_generator split; mmaps-config.yaml
- **Lives in:** src/tools/zone_extractor/
- **Needs:** codec_registry, wad_extractor; zone data loader in game/Maps

**Key features**

- Decodes spawnData.xml, clientSpawnData.xml, triggers.xml, volumes.xml, portals.xml, pathData.xml through the BINd codec
- Parses collision.bcd and zone.nav into local server files (clean-room; katsuba docs studied for behavior only)
- Optional: writes baseline world rows (zone, spawn points, portal links) as a pending SQL update for server-owned tables
- Renders a local top-down minimap image per zone from the walkable NIF/nav mesh for the zone editor (Root ZoneData .dds files appear to be equipment textures, not minimaps)
- Resolves cross-WAD references (|Mob|WorldData|...)
- Per-zone overrides config (YAML), output git-ignored

### Zone and spawn editor (Studio module)

See and edit spawns, mob patrol areas, NPC placements, trigger volumes and teleporter links on a top-down view of each zone. Changes are written as pending SQL overlays. Changes the client must also see go out through the manifest_builder WAD overlay or, as an opt-in experimental step, are written into a separate copy of the user's install, never the pinned development install.

- **Form:** Studio sub-editor (local web app, 2D canvas)
- **Inspired by:** Keira3 map viewer (spawn pins on world map images); Noggit's overlay-project idea (write only changed things)
- **Lives in:** src/tools/studio/ (features/zone)
- **Needs:** zone_extractor, spawn/portal world tables, reload channel, GM build sessions (for 'open in game')

**Key features**

- Top-down zone map from the zone_extractor local cache; pins for spawns, NPCs, portals, triggers, quest-goal areas
- Drag to move, rotate yaw, add from the template picker; multi-select and batch edit
- Portal/teleporter link view across zones (graph of zone connections)
- Layer toggles: retail baseline (read-only from extractor) vs Ambrose world rows
- Validation: spawn outside nav mesh, overlapping spawns, portal to a missing zone
- 'Open in game': sends .zone tele + position to the dev server for the GM's character
- Writes through the Studio change session

### wadview (client data browser)

A read-only local browser for the user's WADs. It shows BINd decoded through the registry, Locale strings, DDS textures and template cross-links, and gives every editor a 'jump to client definition' link.

- **Form:** Local web app (C++ backend reusing wad_extractor + codec)
- **Inspired by:** wow.tools.local (localhost web server over the user's install); WDBX Editor grid browser; Ladik's MPQ Editor
- **Lives in:** src/tools/wadview/
- **Needs:** wad_extractor, codec_registry, template_extractor index

**Key features**

- Tree browse of 3,589 WADs; search by path, template id, class or locale text
- BINd rendered as typed property trees; .gui Window trees; .lang tables
- DDS preview (DXT1/3/5) and NiPixelData texture preview
- Cross-links: template id -> ObjectData file -> references in spawns, quests, decks
- Runs against the user's install and listens on localhost by default. Listening beyond localhost is an opt-in setting, off by default, because anyone who can reach it can browse that install; it never becomes a public host of client files. Nothing committed, no export of client files to the repo

### Spell and deck inspector

Read-only decoding of the 18,173 Spells and 599 Decks into readable effects, pips, school, accuracy and targets, linked to the mobs, decks, items and treasure cards that use them. It doubles as the test oracle while combat is implemented.

- **Form:** Studio sub-view + CLI dump mode
- **Inspired by:** TrinityCore SpellWork (read-only spell inspector); stoneharry Spell Editor (its import/inspect half first; an editing mode like its writing half is planned, not yet scheduled, and saves pending SQL or a WAD overlay built on the user's machine)
- **Lives in:** src/tools/studio/ (features/spell); CLI in src/tools/template_extractor
- **Needs:** template_extractor spell/deck tables; combat subsystem (for oracle mode)

**Key features**

- Readable effect breakdown per spell and spell rank (GameEffectRuleData, SpellFusions aware)
- Deck view with spell counts; 'used by' mob templates and world deck rows
- Diff between the client definition and the server combat implementation's computed result (test harness hook)
- Embedded in the Studio as a picker for mob decks and item cards

### create_module + module skeleton

Scaffold drop-in modules (custom content, events, QoL features) that plug in through ScriptMgr hooks rather than core edits, with their own SQL and config. A module that needs a hook core lacks gets that hook added to core.

- **Form:** CLI script
- **Inspired by:** AzerothCore modules/create_module.sh, skeleton-module, acore.sh module search and catalogue.json
- **Lives in:** apps/installer/ (module commands), modules/ (template skeleton)
- **Needs:** ScriptMgr hooks, CMake module discovery, dbimport MODULE state

**Key features**

- ambrose.sh module new <Name>: generates modules/<name>/ from an in-repo template by default, or, as an opt-in, clones a template repository the user names
- Skeleton: src/<Name>_loader.cpp with AddSC_, a sample PlayerScript/NpcScript, conf/<name>.conf.dist, data/sql/db_world/ example update, Ambrose headers
- CMake auto-discovery and a generated loader
- module list reads installed modules; a later 'ambrose-module' GitHub topic can feed a catalogue

## Late

### capture_to_sql (packet capture to SQL)

Decode session captures with the 971 message definitions from the client's 26 *Messages.xml plus the ObjectProperty codec. Collect spawned objects, NPC dialogue, quest offers and goals, shop lists and combat encounters, diff them against the world database, and write the minimum pending SQL update. By default it does not write to the database; an opt-in mode also applies the update to a local dev database through the updater, so the change is still recorded.

- **Form:** CLI (C++; shares message codecs generated from the message XMLs)
- **Inspired by:** TrinityCore WowPacketParser (per-table SQL builders, DB diff 'minimum changes', VerifiedBuild) + ymir sniffer; AzerothCore wiki sniffing-and-parsing.md
- **Lives in:** src/tools/capture_to_sql/
- **Needs:** Message codec generation from *Messages.xml; ObjectProperty codec; world tables for spawns/quests/shops/loot; the capture source rules decided on 2026-09-16

**Key features**

- Store of parsed entities: object spawns (template id, position, yaw, zone), quest offers/goals, dialogue, shop lists, combat deck/mob observations
- Per-table on/off switches, zone/template filters, captured_revision column (e.g. r806919) in the spirit of VerifiedBuild
- DB diff mode outputs only UPDATE/INSERT/DELETE deltas; 'skip duplicate spawns' dedupe
- Aggregates loot and deck samples across many captures into statistical chances with a sample-count column
- Readable text dump mode annotated with names from the template index
- Companion doc/content/capturing.md listing what to do in-game
- Capture sources, decided on 2026-09-16 at the maintainer's direction: the default input is captures of the user's own sessions against Ambrose or legacy captures they own. Capturing live KingsIsle sessions is allowed as the user's own choice on their own account and machine; it may break KingsIsle's terms and put that account at risk of a ban. Captures, including another project's capture sets, are read from the user's own copy and never committed, and SQL built from live KingsIsle captures or another project's captures goes into a git-ignored local overlay, never a committed update

### npc_script table + visual behavior editor

Let content authors script server-side NPC and zone behavior as data rows (event + action + target), so simple content needs no C++. It covers what the ScriptMgr hooks and client StateData do not.

- **Form:** World table + Studio sub-editor
- **Inspired by:** SmartAI (smart_scripts) + Keira3 SAI editor (per-type param labels, comment generator) + WDE visual smart-script editor
- **Lives in:** src/server/game/AI/ (runtime), data/schema/world/npc_script.yaml, src/tools/studio/ (features/script)
- **Needs:** ScriptMgr, NPC interaction, dialogue, quest engine, teleports/zone transfers

**Key features**

- npc_script rows: event (OnInteract, OnQuestGoalComplete, OnZoneEnter, OnDuelEnd, Timer) + params, action (Say LocaleKey, StartDialogue, GiveItem, Teleport, SpawnMob, SetQuestGoal, PlayCinematic) + params, target (Self, Invoker, Party, ZonePlayers), phase/flags, link to next row
- Type-specific parameter labels and tooltips defined once in schema; editor, loader validator and docs share them
- Auto-generated readable comment column
- Server loader rejects unknown event/action/param combinations at startup and on reload; a failing reload keeps the old rows
- Clean-room constants; nothing ported from SAI tables

### mod-lua (hot-reload Lua scripting module)

Optional drop-in module that binds ScriptMgr hook classes to Lua so quest, event and module logic can be iterated with hot reload on dev and reloaded live on every server.

- **Form:** Drop-in module (C++ + Lua)
- **Inspired by:** mod-ale / Eluna (RegisterPlayerEvent, AutoReload, .reload ale)
- **Lives in:** modules/mod-lua/
- **Needs:** Stable ScriptMgr hook API, module system

**Key features**

- Binds PlayerScript, NpcScript, QuestScript, ZoneScript, CommandScript hooks
- lua_scripts/ folder with an AutoReload file watcher (on by default on dev, an opt-in setting elsewhere) and .reload lua, which on every server loads and validates the new scripts, swaps them, and keeps the old scripts on failure
- Versioned Lua API with docs generated from the binding code (avoids the ALE/Eluna split)
- Written from scratch; no Eluna/ALE code

### manifest_builder + client_content_builder (custom WAD overlay)

For content the retail client must also see (new templates, locale text, GUI layouts, zone files), build a custom WAD overlay from Ambrose source plus the user's install. Regenerate LatestFileList.bin and the CRCs so the Ambrose patchserver serves it to the user's own client.

- **Form:** CLI (C++)
- **Inspired by:** Noggit project folder packed into patch MPQs; TSWoW datascripts producing client + server data together; OpenFusionLauncher hashed manifests
- **Lives in:** src/tools/manifest_builder/, src/tools/client_content_builder/
- **Needs:** patchserver (SESSION + MSG_LATEST_FILE_LIST_V2 + HTTP), wad_extractor, codec encode path, verified client-acceptance test

**Key features**

- Builds WADs with per-entry CRC, HeaderSize/HeaderCRC, gzip header size; writes LatestFileList.bin/.xml (DML tables) and computes ListFileCRC
- Template id collision check against the retail TemplateManifest; generates the manifest overlay and matching SQL rows from one source
- Serves data only by default. Serving FileType 1/4 executables and DLLs is an opt-in setting, off by default: each file must match the manifest signed with the operator's own key (see Formerly rejected ideas), only files the operator owns, such as a hook DLL of Ambrose's own code, are served, and a modified KingsIsle executable never is. The client checks only CRC-32, so players must trust the operator
- Custom revision naming (e.g. V_r806919.Ambrose_N); separate install folder guidance
- Output never committed; built on the user's machine
- Blocked on an unverified test: that the client accepts a rebuilt or new WAD, and which WAD wins on duplicate paths

### navmesh_generator

Generate server-side pathing meshes for mob movement and spawn validation from the extracted zone collision and walkable meshes. It is planned late work, and a per-zone setting chooses between its mesh and the client's zone.nav.

- **Form:** CLI (C++)
- **Inspired by:** mmaps_generator + mmaps-config.yaml (off-mesh connections, per-map overrides)
- **Lives in:** src/tools/navmesh_generator/
- **Needs:** zone_extractor, server-side mob movement

**Key features**

- Input from zone_extractor output (walkable NIF / collision.bcd / zone.nav)
- YAML config: per-zone overrides, off-mesh links for stairs and portals
- Output to the local git-ignored data folder; menu entry in ambrose.sh extract
- Reminds the user to rerun it after zone data changes, then `.reload navmesh <zone>` swaps it live

### Content DSL compiler (code-first content)

Describe a quest line (NPCs, dialogue keys, goals, rewards, spawns) in a reviewable declarative file. It compiles into a pending SQL update checked against the same schema validation. This suits AI agents and git review.

- **Form:** CLI (C++ or Python under apps/)
- **Inspired by:** TSWoW datascripts; WowPacketParser/Keira output conventions
- **Lives in:** src/tools/contentc/
- **Needs:** World schema definitions, template_extractor index, quest/NPC/spawn tables

**Key features**

- YAML (or C++ builder) content files under data/content/ or a module
- Compiles to pending_db_world SQL passing codestyle-sql by construction
- Symbolic references resolved through the template index (npc: 'Headmaster Ambrose' -> template id) with ambiguity errors
- Round-trip: Studio can export an existing quest line to DSL

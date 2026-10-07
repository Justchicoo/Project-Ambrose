<!-- Project Ambrose by Imjustchico: Content and development tool suite, adapted from the AzerothCore ecosystem. -->

# Tools

The Ambrose tool suite takes the AzerothCore content toolchain and changes it in three ways for Wizard101. First, Wizard101 data is hash-keyed BINd ObjectProperty data stored in KIWAD archives, not DBC/ADT files. That makes the type registry dumper and the WAD/BINd decoder the base that every other tool sits on. Second, the world is spatial and quest-driven, so the most valuable authoring tools are quest/dialogue and zone/spawn editors, plus in-game GM build sessions that work inside the retail client. Third, the project rules come first: nothing extracted from the client is committed, and no database edit goes unrecorded. Authoring tools (Studio, capture_to_sql, GM build sessions, content DSL) write pending SQL updates (data/sql/updates/pending_db_world/rev_<unix seconds>_<short-name>.sql, as Content, SQL and releases in doc/ARCHITECTURE.md settles), which take the next dated name when they merge. Direct GM edits apply at once and are journaled, so they can be exported the same way. Each file passes a codestyle-sql check by construction and is applied by a hash-tracking updater. All client-derived indexes (template names, locale strings, zone geometry, minimaps) are built locally from the user's r806919 install into a git-ignored cache.

One source of truth ties the suite together: the world schema each store declares in C++ where it registers its reload target, with each table's source and key, its column types read from the database and the reference each column carries, which the servers publish from 17.173, inspired by WDE DbDefinitions and Keira field models. The same schema drives the panel's world table and edit forms and pickers (17.173 and 17.34), the gameserver startup and reload validator, the reload-command mapping and generated doc/world/<table>.md pages. It replaced the per-table data/schema/world/<table>.yaml files this list first planned, settled on 2026-09-27 under Desktop programs and client data in doc/ARCHITECTURE.md, so the schema cannot drift from the code that reads the tables.

Current repo state (4.03): src/tools/client is the one tool client questions are asked of and the one that grows; src/tools/bindecode, src/tools/dbimport, src/tools/extractor, src/tools/launcher, src/tools/localetool and src/tools/typeextract are built; src/tools/{template_extractor, wad_extractor, zone_extractor} exist as empty folders. apps/{ci, clientdriver, codestyle, installer} exist. data/sql/base/db_world is empty. scripts/Commands is empty. ARCHITECTURE.md already sets the rules that tools depend only on database, shared and common and that GM commands reload single tables.

Build order:
1. **Early foundation:** codec_registry, wad_extractor, template_extractor, dbimport + codestyle-sql, the ambrose.sh dashboard, the reload framework (4.15) and its admin API (17.12).
2. **Mid authoring:** Ambrose Studio (database editor) with quest/dialogue and NPC/loot/vendor sub-editors, GM build sessions (.spawn/.session), extractor zones + zone/spawn editor, create_module. wadview, the spell inspector's read-only half and the Studio's browse views are panel pages instead (17.166, 17.168, 17.176, 17.173 and 17.34).
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

## How this list is used

Read this file before starting a milestone, then look at what is actually built under src/tools and apps, because a tool marked here as planned may already exist and one marked built may do more than its line says. The point is not bookkeeping: a milestone that needs to read something the suite already reads should call the tool rather than write the same decoder again beside it.

When a tool cannot do what a milestone needs, the answer is to teach it, not to work around it. A one-off script, a hard-coded offset or a hand-written parser inside a milestone is the suite failing to learn something, and the next milestone that needs the same thing will pay for it again. Add the function to the tool, note it against the tool's entry here, and the milestone ends with the suite able to decode more than it could at the start.

This applies to contributors as well, and their prompts say so. Before writing anything, look through src/tools, apps, doc/TOOLS.md and the libraries under src for the thing you are about to build. Upgrading one of ours is a welcome part of a milestone; duplicating one is what makes a pull request hard to merge.

## Early

### launcher (built in 3.25)

Starts the user's own client against an Ambrose login server, on any machine that has a client, without ever running KingsIsle's launcher or writing inside the install. `launcher --help` lists its options. It finds the install the way the servers do, through `--client`, `ClientDir` in its own `launcher.conf`, `AMBROSE_CLIENT_DIR` or the discovery in `ClientLocator`, and builds a folder of its own for the client to run from, `client/<revision>` in the Ambrose data folder unless `--run-dir` names another, never the install or a folder inside it: `config.xml` and `preferences.xml` written on every run, from the files that folder already holds or else the install's own, with the window mode and size asked for and `SilentMetricsURL` emptied and every other byte left as the template has it, because the client ignores a configuration that has been parsed and written again, and copies of `revision.dat` and `data.dat`, the other files the client opens by relative name. The client always starts with `-L <host> <port>`, `-P 0`, `-A <locale>`, `-D <the install's data folder>` and `-G <log in the run folder>`, because the retail build starts KingsIsle's launcher when it sees none of its own options, and `--user` and `--character` pass the client's own automatic login and character options through for the 3.24 driver. `--dry-run` prints the run folder and the exact command and starts nothing, `--wait` returns the client's own exit code and ends the client if the launcher is stopped, and `--tail` prints the client's own log lines while it runs; without either the client is started detached. It exits 1, naming the cause, when no install is found, the client program is missing, patching is asked for, a login host or port is missing, a value begins with `-`, the run folder lies inside the install or cannot be written, the machine is not Windows and so cannot start the client, or the client cannot be started, and 2 on bad usage. `--window-ui` opens the launcher as a window rather than a terminal, the desktop shell's web view showing the page built from `apps/launcherui` and compiled into the program, served from memory on the launcher's own origin rather than from a folder or a listener, so no port is opened and the only traffic a run makes is the client's own; the window asks the same `Prepare` the console options reach, opens at 1024x640, shows the server, the client's revision and the window size the client is started at on its ready screen and the installation it found on its settings screen, draws one primary button whose words are the launch state the launcher decides from its first-run steps, Locate, Checking, Setting up, Play, Launching, Playing or Retry, beside the login server's status from the same reach step, and can ask the launcher to open the folder its own plan holds the client's log in, never a folder the page names; a password is replaced with `********` before the window is told anything, the window's own size and corner come back from `launcher-window.json` in the Ambrose data folder, and a machine with no web view says so once and runs as the console launcher. Its tests run as `unit_tests --gtest_filter=Launcher*` and as the `Launcher` CTest, which runs the built program against a synthetic machine, with the window's own page tested as the `launcher` project of the front-end suite. doc/config/launcher.md documents every option. It replaces the development scripts of milestone 1.21, and milestone 16.13 grows a player launcher that patches its own copy of the install from an Ambrose patch server on top of it.

### shell and ambrose_embed_page

`desktop-shell` in src/tools/shell is the window host the launcher and the panel program share, moved out of `launcher-core`: the WebView2 window on Windows and the WebKitGTK window elsewhere, the remembered place and the message loop. A window shows either the program's own page, compiled in and served from memory on the program's own origin, `https://<program>.ambrose` under WebView2 through a web resource handler and `ambrose://<program>.ambrose` under WebKitGTK through a scheme registered as both secure and CORS-enabled, or one remote panel at `http://127.0.0.1:<port>` or `https://<host>:<port>`. A remote-bound view has web messages off and no handler, and the host channel of a program's own page admits only that origin, logging each other origin once. A navigation to any other origin and every new window open in the system browser, and a download is saved only through a save dialog. When the web view meets a certificate it cannot verify, the shell hands its SHA-256 fingerprint to the program, which accepts it only when it equals the pin held for that host and port; every other certificate error is refused with the fingerprints named, and a publicly trusted certificate needs no pin. Web view data lives in `webview` inside each program's own folder under the Ambrose data folder, never beside the executable, with `own` for the program's pages and one profile per remote origin that no other sees and that deletes with everything in it. On Windows the web view starts with `ShellRules::BrowserArguments()`, which turns off its background networking, component updates, reports, pings, sync, proxy discovery and SmartScreen, and with reputation checks off, so a window makes no connection the page did not ask for. `hostKind()` in packages/ui picks a native host only on a program's own origin.

`ambrose_embed_page(<target> NAME <symbol> FOLDER <dist> PLACEHOLDER <text>)`, in src/cmake/EmbedPage.cmake, compiles a built Vite folder into a target as one generated translation unit and header in the build folder, never committed, rewritten only when the folder's files change. Each file keeps its path, media type, an entity tag from its SHA-256 and its bytes, and files under `assets/` are marked hashed. `EmbeddedPage` in src/common/Files answers a path: hashed assets get an immutable cache header and everything else `no-cache`, any other path outside `/api/` gets `index.html`, and the API prefix never falls back. A folder with no `index.html` embeds a page saying why, so a build without the front end still opens one. The launcher embeds `apps/launcherui/dist`, and the supervisor embeds `apps/dashboard/dist`, which both its listeners serve with the headers a folder gets while `Admin.DashboardDir` and `Panel.DashboardDir` are empty; a named folder still wins. 17.184's updater and the credential store wrapper 3.27 and 17.186 use live here too.

Its tests run as `unit_tests --gtest_filter=Shell*:EmbeddedPage*:LauncherPlace*`, and as the `ShellSmoke` CTest, which copies `shell_smoke` into a folder the user may not write, opens the launcher's page from memory off screen there and checks the folder is unchanged, then drives real windows against loopback listeners and two certificates `supervisor --panel-self-signed` writes. It reports itself skipped where no web view exists, and carries the CTest label `render`.

### panel (planned in 17.181)

`panel` in src/tools/panel, over a `panel-core` library that links the shell, is the panel program. It lists the panels it knows, this computer found without being added and others paired, pinned or reached through SSH, and opens each in a window of its own loaded from that panel's own address, so it carries no copy of the dashboard. Its own screens are built from `apps/panelui`. It hosts a game on this computer (17.24), sits in the tray (17.182), installs from 17.183's packages and updates itself through 17.184.

### clientdriver (built in 3.24)

Drives the user's own client through a scenario with nobody at the keyboard, so client behavior is checked the way a player would check it. `python apps/clientdriver/drive.py run` drops and lets the server rebuild its own `ambrose_driver_*` databases, starts a scratch login server on its own port with its logs in the run folder, creates the account it logs in with over that server's console, starts tshark on the loopback adapter, starts the client through `launcher --wait` behind a guard that watches the client and every helper that client starts, and nothing else on the machine, from the moment the client exists until after it is gone, and kills that whole tree the moment one of them connects off this machine, runs the scenario, then leaves a screen the client can be quit from, stops the client, the server, the capture and last of all the guard, drops its databases and writes the report. `check` says in one line whether a run is possible and exits 77 when it is not, `capture-refs` rebuilds the reference crops from a live client, and `scenarios` lists what each scenario needs. Scenarios are data, not code: `apps/clientdriver/scenarios/*.json` holds a title, what it requires, the login server settings its assertions depend on, the messages it expects the server not to handle yet, and steps that each wait on a line of the server's log, a line of the client's log, a crop of the window or a row of the scratch database within their own timeout, so no step waits a fixed time and a failure names the pattern or screen it waited for. A log wait with `"from": "start"` reads from where that server's latest start began rather than from the line after the last match, so a scenario that stops and starts a server sees only what the new process wrote. `apps/clientdriver/references.json` says which crop of the window identifies each screen and where each press lands, measured at one revision, window size and interface scale; the crops themselves are pictures of the client, so they are never committed and `capture-refs` rebuilds them into the Ambrose data folder. Nothing the client produces enters the tree: every report, screenshot, capture and log goes to `%LOCALAPPDATA%/ProjectAmbrose/clientdriver/runs/<id>/`. Every run also checks itself, and a check whose subject was never measured fails rather than passes: the install read and no file under it added, removed or changed; no connection off this machine, with a guard that ran, apart from the exceptions `apps/clientdriver/netguard-allow.json` declares with their evidence, today only the WebView2 runtime's own substrate.office.com call from the launcher's window, which the report names; no message the server did not handle outside the scenario's own list, and none it could not read at all; no server WARN, ERROR or FATAL outside its allow-list; no client line about a message it does not know; no line where the client opened a page outside itself; no process or database left behind; a screenshot for every step that changed the screen; every step the driver took around the scenario done; and every step of the scenario passed, or, for a scenario that expects to fail, one of its own steps failed. A scenario can also name `listeners`, local ports held open for the whole run with the number of connections each should see, which the report checks, and `patching: "default"`, for which the launcher prepares the run folder with `--prepare` and the driver starts the client from that command with only its `-P 0` taken out, so the client follows its own default under the guard, with `patch_config` copying the install's own `PatchConfig.xml` into the run folder pointed at a local host and port, and a `wait_listener` step passes the moment something connects to a watched port; that is how 1.21 showed what the retail client does without `-P` without reaching KingsIsle. A scenario with `launch: "window"` starts the launcher with `--window-ui` instead of `--wait`, so the run opens the window a player meets and starts no client of its own; `launcher_shows` passes when every one of its patterns matches some text the window shows within its timeout, and fails naming the patterns it never saw and a sample of what it did, and `launcher_press` presses a control by its accessible name, which for Play goes on to find the client the launcher starts and its window at the run's window size. Both read and press through UI Automation, the accessibility interface, with comtypes, so they move no cursor, take no foreground and send no global input; the teardown asks the launcher window to close after the client, and a window launch is refused beside `patching: "default"`, a companion or `restart_client`, which is how launcher-window-play.json shows the ready screen naming the server, the client's revision and the window size, and the settings screen naming the install, matched against the `{install}` the driver found itself and taken literally in a pattern, before its Play starts the client. Input is window messages, never global input: text is `WM_CHAR` one character at a time and needs nothing of the window, but the client's interface discards mouse messages unless its window is the active one, so a press borrows the cursor and the foreground for about a second and hands both straight back, and modifiers are never faked because the client reads them with `GetAsyncKeyState`. The window is never minimized, maximized or sent Alt+Enter, all of which the client answers by going fullscreen. One client fact shapes every run, and it was measured rather than assumed: a client that has been refused a login opens a KingsIsle page in the machine's own browser the moment it is asked to quit, and it keeps doing so after the dialog is pressed away and after the login window is back, because the state outlasts the screen; a client admitted since the refusal does not. So `references.json` carries that as a rule over the client's own log, and the teardown reads it and ends such a client rather than asking it to quit. Its self-tests need no client and run anywhere, as `clientdriver.selftest`; each scenario is its own `client`-labelled CTest, `clientdriver.<scenario>`, which exits 77 and names what is missing on a machine that was not asked for a client run with `AMBROSE_CLIENT_DIR`, the same opt-in every client-labelled test takes, or that is not Windows, has no install, no built programs, no database, no capture, or no crops that still fit the boxes they are used with, so an ordinary `ctest` run never starts a client and a machine without one stays green. Every one of them also carries the label `driver`, and only the scenarios carry `client`, because a CTest label filter is a regular expression, which a label holding the word client inside it would have caught: `ctest -L driver` is the driver's own tests, `ctest -L driver -R clientdriver\.` is its four scenarios, and `ctest -LE client` keeps the self-tests while dropping everything that needs an install. `ctest -L client` is wider than the driver: every client-gated C++ test carries that label too.

A scenario that enters the world requires the game server as well, and the driver then runs one of its own beside the scratch login server: it loads its world database with the zone rows `extractor zones` writes from the user's own install, extracted once per client revision and row layout into `clientdriver/zones/<revision>.v<layout>.sql` in the Ambrose data folder and read from there on every later run, starts the game server, which extracts the name, creation, level and stat tables itself and announces its realm at the run's own address, and seeds the scenario's wizard in the run's characters database before the client starts. `--wizard-from` and `--wizard-guid` copy a wizard from another characters database instead, only reading it, which is how a run shows the maintainer's own wizard without logging in as the maintainer. `wait_game_log` waits on the game server's log as `wait_server_log` waits on the login server's, and the report reads both; `game_command` gives the game server a console command as `server_command` gives the login server one, and waits for its answer when given a pattern, which is how announce-and-kick has a game master announce to the world and kick the wizard, in 2.10. `references.json` names in `client_writes` the files the client writes into its own data folder itself, the registry it keeps for each wizard that has entered the world, which the install check reports rather than counts against the install being only read. A press raises the client above anything covering the point it presses and sends it back down afterwards, because the client hit-tests a press against the real cursor. The enter-world scenario was the first to stand a wizard in the world, in 4.14. `hold_key` holds a key for a number of seconds, and with `moves` it passes only when holding it changed far more of the frame than the same time with no key did, which is how enter-the-commons shows the player walking the wizard: in its first run 0.237 of the view stayed the same while W was held, against 0.996 idle. A scenario's wizard can carry its level, `experience` and a `stats` object whose keys are the character_stats columns a wizard carries, and `--wizard-from` copies a wizard's experience and stats row too when its database has one; `shot` takes an optional `settle`, up to 30 seconds, for a window a key opens to finish drawing. wizard-stats.json, added in 5.05, seeds a level 5 wizard with gold, health and mana below its maximums and shoots the HUD, the backpack and the character stats. `restart_client`, added in 5.03, asks the client to quit and starts it again under the same guard, and a log wait's `keep` stores what it matched for a later step's `expect`, which is how walk-and-return.json walks a wizard, has the client quit, logs in again and checks the wizard comes back to the place the game server wrote. A press takes the same `watch` a held key does, added in 6.03, filming the other client while it is made and for a while after, which is how say-and-emote.json films the companion seeing the main wizard wave when it picks the quick chat phrase Hi. That scenario also carries two facts the client showed. It opens its chat line with the slash key, bound to ToggleChatBoxInputSlash, because the driver never sends a VK_RETURN key, and it takes out the slash that key writes. And the client writes a wizard's quick chat favorites into its own data folder as `QuickChat<id>.fav`, which `client_writes` now names beside the character registry.

Since 2026-09-30 a scenario can seed more wizards on the same account after its first with `more_wizards`, which the run names `{wizard_2}` and `{wizard_2_guid}` onward, and can change the run's own scratch database with a `db_exec` step, one statement on its login, characters or world database and never another, which is how attach-bad-key.json spoils the keys the login server stores. A run that clicks needs the client in the foreground: `--foreground` asks for it, and a system prompt left open over the client, such as the firewall asking about a newly built server, takes the foreground and every press is lost, which the step reports as the window not being active.

### client (built in 4.03)

The one tool that asks the user's own install a question, and the one that grows when a question cannot be answered yet. It exists because every such question needs the same three things first, the install, its type dump and an archive out of it, and because a question nobody can ask is a wall that stops a milestone rather than a gap in a list. `client types <name or hash>` searches and prints the classes the dump holds with their bases, properties, ids, offsets, hashes and enum options, which is the only way to read a dump at all: it is keyed by hash, so no search of the file itself finds a name, and `--list` prints just the names of a wide match. `client messages <tag>...` prints what the client says each message carries, read from the client's own XML in Root.wad rather than from anybody's notes, under the protocol, service and order the servers give it, such as `WIZARD MSG_UPDATEMANA (12:233)`, worked out by the same definition code they load the XML with, and `--list` names every message that way with the file it is defined in. 4.12 took the orders of its nine WIZARD messages from it rather than counting tags by hand. `client handlers <tag>...` says which classes in the client program handle a message and at what address, such as `WizardGraphicalClient::MSG_TimedAccessPasses  handler 0x141353da0` for MSG_TIMEDACCESSPASSES, and a message nothing registers is one the client only sends; it also takes a class or a handler name, and `--list` prints every registration. It learned this from the client's own code read in Ghidra: every handler is registered under a debug name `Class::MSG_Name`, with its plain `MSG_Name` and a pointer to its function, either stored before the name or passed to the registering call after it, and a behavior's handler is registered under a key, its class and the plain name joined by an underscore, such as `RidableBehavior_MSG_RidersList`, with the function loaded before the key and the plain name read after it, which 6.01 taught the tool when the handler for the request a client sends on seeing another wizard could not be found. The tool reads those names from the program's data, finds every instruction that reads one through a Zydis decode of the functions the exception table lists, loads with lea and copies with mov or movups alike, keeps only the reads that also read the plain name, which the client's log lines naming their own function do not, and a handler that names itself as it posts itself for later is not taken for its own registration. On r806919 it finds 945 registrations of 897 of the 1443 handler names the message XML gives, each with its function, in about ten seconds; the rest are messages the client only sends, such as MSG_JUMP, or handlers it registers another way, such as the subscriber list's handler object, which the tool says it does not find yet rather than guessing. This is how 4.12 found the classes the client loads its access pass and subscriber lists into, and that it never reads a crown reply's segmentation flag. The program is read by client-image, the library typeextract emulates it over, which links no emulator; what it finds is written once per revision to `handlers/<revision>.json` in the Ambrose data folder. `client behaviors <name>...` says which class the client program builds for a behavior an object template names, such as `NPCBehavior  class NPCBehavior`, with every base that class registers under, and `--list` prints every registration. It too learned this from the client's code in Ghidra: a behavior's name is hashed with the same string hash as a class name, a factory is stored under that hash either in place or inside a function called with it, the factory's create function allocates the object and gives it one class's vtable, and that vtable's first slot, GetType, registers the class's name under the type its parent's GetType returns. The tool follows each behavior name the code reads, at the exact address it loads, to that factory, the vtable, the class name and each base, reading every function to the end the exception table gives through all the regions it splits one into, and it keeps only a registration whose create function really makes an object, since the same names are read by code that builds none. When the dump does not list the class it names the nearest base the dump does, such as `BasicEffectsBehavior  ClientGameEffectBehavior, which the type dump does not list, a class BaseGameEffectBehavior`, which is the class a player object the client accepted carries for that behavior. On r806919 it finds 175 registrations in about nine seconds and agrees with 31 of the 32 behavior classes a captured player object proves; the other, BasicMobileBehavior, travels under the behavior's own name, which the client resolves through the same factory because its key is that name's hash. For a name it finds no registration of, it says whether the program holds that name as a string at all: a name the program never holds is one it cannot key a factory by, so the client builds nothing for that slot. 6.10 taught it this when the Commons' trainer, potion shop and fish seller came up without their objects: of the 51 behavior names placed zone templates hold that behavior_client_class lacked, it followed 24 to their classes, and the other 27, such as WizTrainingBehavior and PotionShopBehavior, and misspellings in the data such as xBasicEquipmentBehavior, are strings the program does not hold. What it finds is written once per revision to `behaviors/<revision>.json`. `client template <id>...` prints the object template an id names through the game server's own template store, found through TemplateManifest.xml as the client finds it, in Root.wad or in the World-Part.wad a path written `|World|Part|path` names, with its file, archive, class, name and behaviors, then the template itself, and `--list` prints every id the manifest lists with its archive and entry, marking each the install lacks. On r806919 it lists 137423 templates in 27 archives with none missing, which is how 5.01 counted the templates each archive holds. `client wizbangs <name or id>...`, added for 6.02, prints the markers the install's WizBangs.xml gives the client, each with the id a MSG_WIZBANG names it by, which is the string hash of its name, and the model it draws, such as `660791182  StartQuest  Wizbang/Quest_Marker_Start.nif`, and says of a name or id no marker has that a MSG_WIZBANG naming it draws nothing; `--list` prints every marker, or those whose name holds the text. It learned the id from the client's code: the MSG_WIZBANG handler passes the id to the object's SetWizBang, which looks it up among the templates the WizBangTemplateManager loads from WizBangs.xml, and a client driver run that sent StartQuest's hash saw the quest-start marker drawn over the other wizard. On r806919 it lists 33 markers and none is SpellbookWizbang, which is why 6.02's spellbook wizbang is relayed and draws nothing. `client wad <entry>` prints an entry, BINd as JSON and anything else as the text it holds, and `--list` names entries. It reads the install and dump named by `--client` and `--type-dump` or `AMBROSE_CLIENT_DIR` and `AMBROSE_TYPE_DUMP_PATH`, following `AMBROSE_SETUP_MODE` as the other tools do. What it writes it writes only into the Ambrose data folder, never into the tree, because it is all derived from a client nobody may redistribute: `messages/<revision>.json` holds what every message of that install carries, written the first time it is asked and read from afterwards, which took the same question from 2.0 seconds to 0.24; and the fast copy of the type dump beside the dump itself, which took `types` from 6.0 seconds to 2.3. Both are keyed by client revision, so an install of a different revision builds its own and neither is ever stale for the install it names. 4.03 needed to know what MSG_SERVERLIST carries and the answer, that it carries nothing, came from the install rather than a guess; 4.05 needed MSG_CHARACTERSELECTED's eighteen fields and got them the same way.

`client strings`, `xrefs`, `disasm`, `functions`, `vtable` and `decompile` read the client program's own code, so a question about what the client does is answered in one call rather than worked out by hand in a disassembler. `client strings <text>...` prints every string the program holds with the text, ASCII or UTF-16, with its address and each instruction that reads it, and `--list` prints only the strings. `client xrefs <address>...` prints every instruction that reads, calls or jumps to an address and every pointer to it the relocation table lists, such as a vtable entry, leaving out a call pattern that sits inside another instruction, and given text rather than a 0x address it follows each string holding it. `client disasm <address>...` prints the function holding an address, or each function that reads a string holding the text given, in Intel syntax through Zydis, naming each string it reads, each import it calls through a slot, such as `KERNEL32.dll!Sleep`, and each handler, behavior create function, GetType and vtable the tool has found. `client functions <name>...` finds a function by the name it gives itself in its log lines: the client's log macro loads the source file's path and the function's own qualified name, such as `CoreObjectFactory::AddBehavior`, one right after the other, so a function that reads a qualified name within 32 bytes of reading a source path logs under that name, and a function a logging one was inlined into logs under both. On r806919 3642 functions name themselves this way, found once per revision and written to `functions/<revision>.json`, and every function the other commands print carries its names. `client decompile <address or text>...` prints the same functions as C through the user's own Ghidra, whose folder `--ghidra` or `AMBROSE_GHIDRA_DIR` names. The tool starts Ghidra's headless analyzer itself, with the Java that Ghidra's LaunchSupport picks and the virtual machine arguments it asks for, so no launch script parses a path a second time, over the project `--ghidra-project` or `AMBROSE_GHIDRA_PROJECT` names, or else over one it makes in the Ambrose data folder by importing and analyzing the program, once, which takes a long while. A script the tool writes prints each function between markers under the program's SHA-256, so a project holding another build of the client is refused, and each function's C is kept in `decompiled/<SHA-256>/`, so asking again takes seconds: `CoreObjectFactory::LoadTemplateManifest` took 18 seconds the first time and 4 once kept. `client vtable <address or class>...` prints a virtual table slot by slot, each function it points to with the name the tool knows it by, the short ones written out, such as `{ mov al, 0x01; ret }`, and whether any other pointer holds the same function, which is how an override is told from a slot the class inherits. A slot counts only where the relocation table lists a pointer into code, and the table ends where code loads the next one by its own address. Named by class, it finds the table through the behavior a class is built for or through the run-time type information the program keeps, which r806919 keeps for its runtime library's classes and not its own. This is how 8.05 found ClientSpellbookBehavior's post-load hook, the one slot of its load and save hooks it overrides, which builds the spellbook the client shows from the list the player object carries. `client name <hash>...`, added in 6.10, names a property hash the way the property oracle does, from every type and name the dump lists, every property name the client program holds or prints, as a format string holds the names it prints, and every one the text files of the archive `--wad` names spell out, and it says how often a random hash is named by the same sources: 0.1% on r806919, and 0.8% with `--wide`, which also tries every class the dump lists as a type, plain, pointed to and shared. That rate is what showed that the properties the unknown classes still hold, the sigils' among them, cannot be named from the install: the widest search named four of the 128 hashes Root.wad leaves unnamed, about what chance gives, each as nonsense such as a shared pointer to a shared pointer. `client types --derived <class>` lists every class derived from one, and `--flag <name>` prints only the properties that carry a property flag, which is how 5.01 found that each template class names itself by a property of its own, a recipe by `m_recipeName`, while a game object's `m_objectName` carries no flag at all. Formats the client uses that the tool does not read yet, its NAV navigation graphs, BCD collision data and POI points of interest among them, are taught here when the milestones that need them arrive.

`client hash <text>...`, added on 2026-09-30, prints the hash the client gives a class or school name, such as `client hash Fire` for 2343174, and with `type:name` the hash of a property of that type and name, needing no install; it replaces one-off hash scripts. `client core <file>` prints a game object blob, the Data a MSG_LOGINCOMPLETE or MSG_NEWOBJECT carries, enveloped or not. Every object in such a blob opens with the client's CoreObject header, a core type, a template type and a four-byte id: a template id after the core type the client builds the object's class by and the type of its template, or a class hash after a zero core type. Which class each core type builds comes from the world database's core_object_type when `--world-db` or `AMBROSE_WORLD_DATABASE_INFO` names it; `--core-type <n>=<class>` adds or replaces one for the run and `--as <class>` names the root's, and a core type nobody has named is reported with every class the dump derives from CoreObject, so the one that decodes is named rather than guessed. `--flags` reads with exact serializer flags, `--mask` with another property mask, and `--trailing` stops where the class ends and goes on to read each object that follows, saying where bytes are left that do not read. Given the world database, every command also reads the classes its server_class tables add to the dump, and `types` marks each as coming from there. 4.11 used it to find that the player object carries its stats as the nested m_gameStats and that the maintainer's capture ends one property before r806919's last. `client field <tag> <field> <file>...`, added in 3.02, decodes one object field of a message, such as MSG_BADGES' BadgeInfo, the way the client reads that field, enveloped and with the field's own flags, and says so when it reads only unwrapped. `client hex <file>` prints a file's bytes with their offsets, from `--from` for `--count`, decimal or 0x hex. `client lang <key>` prints the text a locale key holds in `--locale`, en-US by default, and `--list <text>` prints every key whose text holds the text given. Until 6.04, `--list` matched only whole texts. 6.04 used the substring search to find that the client's notices name a locale key and fill `#1:$NAME$` style fields from madlibs, so a key cannot carry a command's reply.

**The data it reads is cached, never committed.** The type dump and message definitions are derived from a copyrighted client, so doc/ARCHITECTURE.md's rule that nothing client-derived enters the repository covers them too. They live in the Ambrose data folder, built once per revision, and every server and tool reads the same copy: a login or game server now builds the type dump's fast copy itself through `TypeDumpCache::EnsureFastCopy` when it is missing, so the seventeen megabytes of JSON are parsed on the first run after an extraction and by nobody afterwards, rather than on every start as they were before. Extract once, and nothing afterwards needs a tool run to reach the data.

**This is where client questions are answered from now on.** bindecode, localetool and schemaprobe each do one of these jobs behind its own discovery and loading, and the empty wad_extractor and template_extractor folders are two more of the same tool waiting to be written. schemaprobe reads every BINd file and every versionable object with no BINd header, such as a zone's gamedata.bin, lists each property an object of an unknown class holds by hash and bit size, names each class from the client program's own strings and each property hash through the property oracle from every type and name the dump lists, every property name the client program holds and any `--candidate` given, which is how 6.09 named class 520243970 `class QuestingBehaviorTemplate` with its four properties and the six unknown objects in WC_Hub's zone data `class MinigameSigilInfo`. 6.10 taught it `--server-classes <file>`: it proposes a class for each unknown one, named by the one program string that hashes to it, with every property the oracle names one way only, a base only the data proves, the class the list it sits in holds, and each property copied from the same type and name in the dump or read with a trial container; it re-sweeps only the files that held unknown classes until a round changes nothing, tries a property that does not decode as a list before refusing its class, and writes the classes it kept, with the evidence for each and the sweeps before and after, in the type dump's own format with the version of the way it found them, which `--supplement <file>` loads back so a later sweep shows them gone. It reads the plain-XML object files too, which write each class and property by name, and reports each class the dump lacks with every element its objects hold, whether one repeats it or writes it with a key, the classes it holds and its distinct values; `--server-classes` proposes a class only those files hold, one only the server reads, with the server's own types read from those values by the rule doc/ARCHITECTURE.md records for 6.10, and keeps it once the files read cleanly, which is how Chatter.xml, Colors.xml and CharacterCreationConfig.xml came to read as objects; `client wad` names every part it skips the same way instead of printing it as null. They fold in here as each is next touched rather than in one sweep, so no working tool is broken for a refactor; `launcher` stays its own program because it ships to players, and `dbimport` and `typeextract` stay separate because one works on databases and the other emulates the client's executable behind its own test suite.

### extractor (built in 3.14)

Extracts world database rows from the user's own install. `extractor names` reads every character name table in every locale with its text, the disallowed name list and the schools and creation options a new wizard is offered. `extractor levels`, added in 5.04, reads MagicXPConfig.xml, the MagicSchools templates and WizStatisticEffectConfig.xml: one row per school and level with each school's own values where its table sets them and the shared table's otherwise, the sixteen magic schools with their secondary-school badges, MagicXPConfig's settings, encounter factors and mob rank levels, and every stat setting with its crit, block and pip conversion bands, and it prints each table's count and the schools with and without level tables. `extractor classes`, added in 6.10, writes the classes the install holds and its type dump does not describe to server_class and the tables beside it, marked install so the authored ones stay, from the class file schemaprobe builds once per client revision in the Ambrose data folder's classes folder, which the game server builds and writes the same way on its first start, replacing the install classes its world database holds whenever they differ from that file. `extractor zones`, added in 5.02 in place of 4.08's separate zone_extractor, reads the gamedata.bin of every zone archive through the zone views: each zone's settings under the path its own data names it by, which the archive's name must match, every named location, and one zone_object row for each entry of its object list with the class, template, object id, position, orientation, scale, zone tag, start state, override name, flags, spawn requirements and the loading type that says whether the client builds the object from its own copy of the zone or the server sends it. An entry whose class the type dump does not describe is left out and counted by class hash, and inside a kept entry such a part is left out of the row, so nothing is guessed; on r806919 the only entries left out are the 7,188 sigils of six classes, which 6.10's extraction names but refuses, since no source in the install names their own properties. Since 6.11 it also reads each zone's volumes.xml and triggers.xml through the server classes of the world database named: one zone_volume row for each walk-in volume with its shape, size and position, one zone_trigger row for each trigger, the events of both in zone_trigger_event, and every result a trigger runs in zone_trigger_result, by the class hash every result has and, when a class describes it, its class name and bytes. A field no name fits yet is kept under its hash. It prints how many zones' volume or trigger files do not decode, which leaves those zones without volumes or triggers but writes everything else; on r806919 none fail. The game server runs the same extraction on its first start when its world database holds no zone. `extractor templates`, added in 7.01, reads every template TemplateManifest.xml lists into object_template, object_template_adjective and object_template_behavior: its class, archive and entry, object name, display and description keys, icon, visual id, object type and loot table, its adjectives and its behavior slots in order. A behavior of a class nothing describes keeps its class hash, and its name is read from the bytes the decoder skipped, since every behavior template writes m_behaviorName under the same hash. It prints how many entries were read, the ObjectData ones among them, the NPC templates and the unknown behavior classes by hash; on r806919 all 137,423 are read, none left out. Since 8.06 it also writes item_template, one row for each template whose class is or derives from WizItemTemplate with its school, base cost, rank, item limit, item set bonus and color counts, its names and adjectives being the object_template row it extends; an item holding an object of a class the type dump does not list anywhere but its behaviors fails the run and is named, never skipped, and it prints how many item rows it wrote. Several commands can be named in one run, as `extractor names levels zones`, and their tables are replaced together. It replaces those world tables in one transaction, writes the SQL to a file with `--sql`, or checks everything and writes nothing with `--dry-run`, which also checks the world tables of a database it is given. `extractor --help` lists its options. It reads the install, type dump and world database named by `--client`, `--type-dump` and `--world-db` or by `AMBROSE_CLIENT_DIR`, `AMBROSE_TYPE_DUMP_PATH` and `AMBROSE_WORLD_DATABASE_INFO`, and the world tables must already exist, which dbimport creates. When no install or type dump is named, it checks its database and output options first and then follows `AMBROSE_SETUP_MODE`, saving nothing. `auto`, the default, never asks: it uses the install with the newest revision found on the user's machine and the type dump built from it in the Ambrose data folder, running the typeextract beside the tool when that revision has no current dump, and prints one line naming what it used and the flags that choose otherwise. `ask` asks on a terminal which install to use, uses a current dump already built for it without asking, and otherwise offers to build one before offering other dumps found, waiting `AMBROSE_SETUP_PROMPT_TIMEOUT` seconds, 120 by default, for each answer. `off`, or `ask` without a terminal, prints what it found and the flag to pass. Ctrl+C during a build stops typeextract. bindecode and localetool do the same, and localetool needs no type dump. Later extractors join it as more commands.

### typeextract (built in 3.21)

Builds the type dump of the user's own install by emulating its client program, without launching the game or writing into the install. The C and C++ initializers, the lazy type getters and the client's own race adder rebuild the client's type registry, which is then checked and written as format v2. `typeextract --help` lists its options. It reads the install named by `--client` or `AMBROSE_CLIENT_DIR`, or else the newest revision found on the machine. It writes `--out` or types/<revision>.json in the Ambrose data folder, and `--compare <dump>` prints every difference from another dump, which is read before extracting, so it may name the output file. When the revision or the data folder cannot name the default file, it asks for `--out`. `--exit-when-input-ends` makes it exit with code 1 as soon as its standard input ends, which the servers and tools use so a build never outlives the process that started it. It exits 0 on success, 1 when extraction, validation or writing fails, and 2 on bad usage.

### typeregbuild (built in 5.07)

Wraps a format-v2 type dump from the user's own install in a versioned binary cache. `typeregbuild --input <revision>.json --output <revision>.bin` validates the JSON, records the input filename's revision, stores fixed-width raw records plus one shared string table, stamps the exact payload bytes with SHA-256, and writes only to the requested output path. `TypeRegistry::LoadBinary` validates the envelope, revision and hash before building the same catalog directly from those records; the login and game servers prefer a sibling `.bin` cache when present, and callers may provide the JSON path as a fallback, which is logged when a cache is stale or corrupt. The cache is a local data file and must never be committed.

### localetool (built in 3.13)

Finds the keys whose text matches, checks that a key exists and prints its text, dumps a table, and lists the installed locales with the files each skips, all from the `.lang` files of the user's own Root.wad. `localetool --help` lists its options. It reads the install named by `--client` or `AMBROSE_CLIENT_DIR`, or else the one `AMBROSE_SETUP_MODE` finds as the extractor does, and writes nothing.

### bindecode (built in 3.11)

Prints BINd entries of any KIWAD archive in the user's own install as ordered JSON, lists entry names, and sweeps an archive for decode failures and unknown classes, which feed the type registry work. `bindecode --help` lists its options; it reads the install and type dump named by `--client` and `--type-dump` or `AMBROSE_CLIENT_DIR` and `AMBROSE_TYPE_DUMP_PATH`. When they are not named, it follows `AMBROSE_SETUP_MODE` as the extractor does. An archive named by its path opens before anything is searched, and the install holding it, when there is one, is the install its type dump comes from. It writes nothing itself; a type dump built for it goes to the Ambrose data folder. `--text` prints an entry that is not BINd as the text it holds, which is how the client's own message definitions are read: 4.03 needed to know what MSG_SERVERLIST carries, and the answer, that it carries nothing, came from LoginMessages.xml in the user's own Root.wad rather than from a guess. An entry holding bytes no text file has is refused rather than printed as rubble. `--raw` reads an entry that holds a versionable object but carries no BINd header, which is how a zone's `gamedata.bin` is stored: the codec that reads it has existed since 8.14, and the only thing standing between the tool and the answer was the header check, so an entry with no BINd magic now says that `--raw` reads it instead of only saying no. 4.08 met that wall while settling which list entries its writer was skipping and could not use the tool to answer it.

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
- **Inspired by:** AzerothCore dbimport, updates table (SHA-1 hash + state), AzerothCore's ci-pending-sql.sh, apps/codestyle/codestyle-sql.py
- **Lives in:** src/tools/dbimport/, apps/ci/, apps/codestyle/
- **Needs:** src/server/database connection pool and updater; data/sql/base/db_world populated

**Key features**

- updates table: name, SHA-1 hash, state (RELEASED/CUSTOM/MODULE/PENDING/ARCHIVED), timestamp, speed
- Redundancy mode: re-applies an edited pending file on dev when its hash changes (so Studio re-saves work)
- Also applies module data/sql folders
- `apps/ci/ci_sql.py` (built in 3.19): `promote` renames each pending rev_<unix seconds>_<short-name>.sql to the next free YYYY_MM_DD_NN.sql at landing, `check` refuses a change to an applied update or a base file and checks new files' names and headers, and `apply` runs base, released and pending SQL in the updater's order on fresh databases through the mysql client, naming the file the server refuses; CI runs all three
- codestyle-sql: no tabs, trailing whitespace or double blank lines; final newline; every INSERT preceded by a scoped DELETE; column names match base/ schema; Ambrose file header present
- Schema check: SQL columns and types must match the world schema the stores declare (17.173)

### World schema definitions + doc generator

The world schema is the single source of truth for column types, enums, bitflags, foreign keys (with the table or client index each points to), primary keys, record mode and reload command. Settled on 2026-09-27 under Desktop programs and client data in doc/ARCHITECTURE.md: each store declares its tables in C++ where it registers its reload target, and the servers publish it (17.173), rather than one YAML file per table. The panel's world table and edit forms, the server's startup validator and the docs are all built from it.

- **Form:** Declarations beside each store's reload target + small doc generator CLI
- **Inspired by:** WDE.DatabaseEditors DbDefinitions JSON (174 files); Keira3 field models/Options/Flags; AzerothCore wiki per-table pages
- **Lives in:** each store's reload target registration (declarations), src/tools/schemagen/ (generator), doc/world/ (output)
- **Needs:** First world tables in data/sql/base/db_world

**Key features**

- Per table: its source (extracted from the install, authored in the repository, or imported) and its key; per column: value_type (TemplateId, ZoneId, LocaleKey, SpellId, QuestId, Flags, Enum), default, read_only and the reference it carries, with column types read from the database
- record_mode: single-row vs multi-row (DELETE+INSERT per key), composite keys
- reload: name of the .reload subcommand
- Generates doc/world/<table>.md with a hand-written notes section kept separate
- The gameserver checks rows against it at startup and on every reload, rejecting bad ones and keeping the old store
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
- module new/list; studio (launch local web editor)
- Reads client data only from the user's own install and never downloads it from a host the project runs. An opt-in step that fills a separate copy from KingsIsle's patch servers is planned, not yet scheduled; using it is the user's own choice on their own account and machine, may break KingsIsle's terms, and never touches the pinned development install

### ci_build (taught parallel tests and legs on 2026-10-01)

Configures, builds and tests one preset the way CI does: `python apps/ci/ci_build.py --configure-preset linux-gcc --build-preset linux-gcc-debug [--warnings-as-errors] [--setup-vcpkg <folder>] [--stage configure|build-test]`. Tests run in parallel on every core, and `--test-jobs 1` runs them one at a time; `--exclude-label render` skips the tests that need a real display, which WSL's legs leave to the Windows leg. For local verification it also runs several presets as legs, `--leg linux-gcc:linux-gcc-debug --leg linux-gcc-asan ...`, each a configure, build and test, going on past a failed leg and ending with a table of each leg's configure, build and test seconds and its result. `--sync <repository> --commit <commit>` first brings the clone the tool runs from to that commit of another repository, fetching its branches, checking the commit out and removing everything untracked but `build/`, so every build is incremental, then runs the rest from the checked-out tool; it refuses to sync the repository it is run from. The local Linux legs run it from a clone on WSL's own disk, `python3 apps/ci/ci_build.py --sync /mnt/k/Project-Ambrose --commit <sha> --leg ... --warnings-as-errors --exclude-label render` with `AMBROSE_TEST_DB` set.

- **Form:** CLI (Python)
- **Lives in:** apps/ci/ci_build.py, with its self-tests in apps/ci/tests/test_ci.py
- **Needs:** CMake, Ninja on Linux, and VCPKG_ROOT or `--setup-vcpkg`

### ci_stress (built on 2026-09-30)

Measures a flaky test before and after its fix the same way every time: `python apps/ci/ci_stress.py <unit_tests> --filter 'AdminServerTest.*' --copies 32 --runs 100 [--busy 8] [--keep <folder>] [--label fixed] [--arg <argument>]...` runs the gtest executable as that many concurrent copies, each that many times, optionally beside busy loops that load the machine, and prints how many runs failed, grouped by the first failure each printed, with ports, timings and addresses made alike so one cause counts once, and which tests failed. It exits 1 when any run failed, and `--keep` holds each failing run's whole output. It took over the loops written by hand for flake 1 and flake 2; a stress proof a fix lands on is run with it, and the numbers go in the fix's message.

- **Form:** CLI (Python)
- **Lives in:** apps/ci/ci_stress.py, with its self-tests in apps/ci/tests/test_ci.py
- **Needs:** a built unit_tests; nothing else

### discordbot (built on 2026-10-07)

The project's own Discord bot, hosted on one machine that keeps it online, in place of the webhook workflows. `python apps/discordbot/bot.py setup --home <folder>` clones main into `<folder>/repo`, asks for the bot token without showing it and keeps it only in `<folder>/token.txt`, installs discord.py into `<folder>/venv`, adds the bot to the Windows user's startup apps, so it starts at sign-in with no window, starts it now and prints the link that invites it to a server. `run` keeps one copy of the bot going, refusing to start a second from the same folder, restarting it with backoff after a crash and at once when main changes `apps/discordbot` or `apps/progress`, so a merged change to the bot reaches it without anyone at that machine. Every five minutes, and when it starts, it brings its clone to main and edits the progress and openings boards in place with the embeds `apps/progress/announce.py` and `openings.py` build, so the boards and the README card read the same figures and the bot catches up after being off. Slash commands: `/progress`, `/openings`, `/milestone <id>` with completion, `/prs`, and for members who can manage the server `/board here`, `/board off` and `/board refresh`, which choose the channel for the progress, openings and merges boards. The merges board posts each pull request merged since the last one it saw. The first time it posts a board it deletes the old webhook message whose id is committed under `doc/progress/` or kept on the old state branches. `preview` prints what the boards would show with no Discord. A GitHub token in `<folder>/github-token.txt` or `AMBROSE_GITHUB_TOKEN` is optional and only raises GitHub's rate limit.

- **Form:** a long-running program (Python, discord.py) and its CLI
- **Lives in:** apps/discordbot, with its self-tests in apps/discordbot/tests/test_discordbot.py
- **Needs:** git and Python 3.10 or later; the self-tests and `preview` need nothing else

### ci_local (built on 2026-09-27)

Runs every step of CI's checks job on a local clone, read from `.github/workflows/core-build.yml` so it follows the workflow as it changes: `python apps/ci/ci_local.py [--branch <pull request branch>] [--base <ref>] [--list] [--stamp <file>]`. The range steps run over what the next push would send, against the project's `main` fetched first. `--branch` adds the contributor path check, and without it the step CI runs only on a push to `main` runs instead, so a run with no branch is the check before pushing to `main`. `--stamp` writes the commit a green run covered to a file, which a pre-push guard can compare with the commit being pushed.

- **Form:** CLI (Python)
- **Lives in:** apps/ci/ci_local.py, with its self-tests in apps/ci/tests/test_ci.py
- **Needs:** git and Python; nothing built

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

Settled on 2026-09-27 under Desktop programs and client data in doc/ARCHITECTURE.md: browsing the world tables and editing the running world database are the panel's World tables and World edits pages (17.173 and 17.34), so the Studio is not a second browser of them, and what stays here is authoring into pending SQL updates for the repository.

- **Form:** Local web app served on localhost (C++ backend in src/tools using shared/database, simple TS/HTML frontend); no client files bundled, since it reads the user's own install at runtime
- **Inspired by:** Keira3 (EditorService diffQuery/fullQuery, handler services, route guards, unused GUID search); WoW Database Editor (sessions, diff viewer, remote reload)
- **Lives in:** src/tools/studio/
- **Needs:** World schema definitions, dbimport/updater, template_extractor index, reload channel, world tables for NPCs/quests/spawns/loot

**Key features**

- Main entities: NPC/Mob template, Quest, Zone, Spawn, Loot table, Vendor/Shop list, Deck, Dialogue; sub-editors open only after a main entity is chosen
- Live diff SQL (UPDATE of changed columns only; DELETE+INSERT for multi-row tables) with highlighted preview, Copy, Execute (local dev DB) and Save-as-update
- 'Change session': bundles every edit made while building one quest line into one pending_db_world file, with a diff viewer before writing
- Linked navigation: every foreign-key field has a picker (search by English name from the local template index) and a jump-to link (quest -> giver NPC -> spawn -> zone)
- Unsaved-change dots per table; forms generated from the world schema the stores publish (17.173)
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

- .session start <description> / .session undo / .session end: end writes data/sql/updates/pending_db_world/rev_<unix seconds>_<short-name>.sql (scoped DELETE + INSERT with @variables)
- .spawn add <templateId|name> at the GM's position and yaw; .spawn move / turn / delete / near / info
- .path add/show/clear for patrol waypoints; .zone tele / .zone info
- .dialogue test <id>, .quest start/complete <id> for quick testing of Studio content
- Security levels per command in cs_spawn.cpp, cs_session.cpp; open to GMs on dev, admin level on other servers, and audited everywhere
- Changes are streamed live to clients in the zone

### extractor zones, the rest of it

`extractor zones` already decodes every one of these archives and writes zone templates, named locations, object placements, and since 6.11 walk-in volumes and triggers. What follows is the rest of what it should read, and belongs in the same command rather than a second tool. Turn each zone WAD (3,356 contain gamedata.bin) into server data: retail spawn points, triggers, volumes, portals/teleporters, path data, collision (collision.bcd), nav (zone.nav), plus a local minimap/top-down geometry cache for editors.

- **Form:** CLI (C++)
- **Inspired by:** vmap4_extractor + vmap4_assembler + mmaps_generator split; mmaps-config.yaml
- **Lives in:** src/tools/extractor/ and src/server/shared/ClientData/ZoneExtractor
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
- **Needs:** extractor zones, spawn/portal world tables, reload channel, GM build sessions (for 'open in game')

**Key features**

- Top-down zone map from the extractor's local zone cache; pins for spawns, NPCs, portals, triggers, quest-goal areas
- Drag to move, rotate yaw, add from the template picker; multi-select and batch edit
- Portal/teleporter link view across zones (graph of zone connections)
- Layer toggles: retail baseline (read-only from extractor) vs Ambrose world rows
- Validation: spawn outside nav mesh, overlapping spawns, portal to a missing zone
- 'Open in game': sends .zone tele + position to the dev server for the GM's character
- Writes through the Studio change session

### wadview (client data browser, now panel pages)

A read-only browser for the user's WADs, built into the panel rather than as a local web app of its own, as Desktop programs and client data in doc/ARCHITECTURE.md settled on 2026-09-27: the Types page (17.165), the Archives page (17.166), the Locale page (17.167) and the Templates page (17.168). It shows BINd decoded through the registry, Locale strings and template cross-links, and gives every editor a 'jump to client definition' link.

- **Form:** Panel pages over the `/api/data/` routes (17.165)
- **Inspired by:** wow.tools.local (localhost web server over the user's install); WDBX Editor grid browser; Ladik's MPQ Editor
- **Lives in:** the game server's data routes and apps/dashboard
- **Needs:** the KIWAD reader, the type registry, the locale store and the template store

**Key features**

- Tree browse of 3,589 WADs; search by path, template id, class or locale text
- BINd rendered as typed property trees; .lang tables
- Cross-links: template id -> ObjectData file -> references in spawns, quests, decks
- No texture, model, GUI layout or plain-text preview, because nothing the panel serves carries a file from the client install, and no export of client files

### Spell and deck inspector

Read-only decoding of the 18,173 Spells and 599 Decks into readable effects, pips, school, accuracy and targets, linked to the mobs, decks, items and treasure cards that use them. It doubles as the test oracle while combat is implemented. Its read-only half is the panel's Spells and Sigils pages (17.176), which decks join in the milestone that loads them, as Desktop programs and client data in doc/ARCHITECTURE.md settled on 2026-09-27; the CLI dump mode and the combat oracle stay here.

- **Form:** Panel pages (17.176) + CLI dump mode
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

Decode session captures with the 1,448 message definitions from the client's 29 *Messages.xml plus the ObjectProperty codec. Collect spawned objects, NPC dialogue, quest offers and goals, shop lists and combat encounters, diff them against the world database, and write the minimum pending SQL update. By default it does not write to the database; an opt-in mode also applies the update to a local dev database through the updater, so the change is still recorded.

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
- **Lives in:** src/server/game/AI/ (runtime, with the npc_script schema its store declares), src/tools/studio/ (features/script)
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
- **Needs:** extractor zones, server-side mob movement

**Key features**

- Input from the extractor's zone output (walkable NIF / collision.bcd / zone.nav)
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

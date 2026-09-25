<!-- Project Ambrose by Imjustchico: Roadmap phase 4, A wizard stands in Ravenwood. -->

# Phase 4: A wizard stands in Ravenwood

**Done when:** After Play, the client leaves loginserver, attaches to gameserver and the player controls their wizard in WizardCity/WC_Ravenwood (or WC_Hub 'Start'). No other objects are streamed yet.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 4.01 | sWorld tick, GameSession, ScriptMgr hooks and AddSC loaders (new core) | M | 2.09, 2.08 |
| 4.02 | CommandMgr and security levels, console first (new; WIZ-5 core) | M | 4.01, 2.13 |
| 4.03 | Realm registry and heartbeat (LOG-10) | M | 2.13, 4.01 |
| 4.04 | World wire math and LocationString (WLD-1 + LOG-11 LocationString) | S | 1.14 |
| 4.05 | Character select and login key (LOG-11) | M | 3.09, 4.03, 4.04 |
| 4.06 | Login-to-game handoff transport (NET-13) | S | 4.05, 2.10 |
| 4.07 | Gameserver login key validation (LOG-12) | S | 4.06 |
| 4.08 | Zone extractor part 1: WizZoneData (WLD-2 + QST-4 zone objects) | M | 3.11, 2.07 |
| 4.09 | sZoneMgr and reload (WLD-4) | S | 4.08, 4.02, 4.15 |
| 4.10 | Maps, instances, mobile ids, GID service (WLD-5) | M | 4.09, 4.01 |
| 4.11 | CoreObject serializer and client-object builder (OBJ-9 + WLD-6) | M | 3.05, 3.07 |
| 4.12 | Wizard service skeleton and login chatter (WIZ-1, without wizbang broadcast) | S | 2.09, 4.01 |
| 4.13 | Attach handler server side (WLD-7 part 1) | M | 4.07, 4.10, 4.04 |
| 4.14 | LOGINCOMPLETE and standing in zone (WLD-7 part 2 + WLD-8 CLIENTZONED) | M | 4.13, 4.11, 4.12 |
| 4.15 | Reload framework and reload commands | M | 4.01, 4.02, 1.10 |
| 4.16 | Live settings registry | M | 4.15, 2.08, 2.13 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 4.09 lists MSG_COMMAND/MSG_COMMANDRESULT and '.zone info' / '.reload zone_location' as its acceptance, but chat-to-CommandMgr routing only arrives in 6.04. Until then 4.09 can only be console-verified. Also unverified: that GM text reaches the server as MSG_COMMAND. GameMessages.xml describes MSG_COMMAND (5:44; Command WSTR, ResultEvent STR, TimeLeft INT) as a 'Command Processor Message', and WizardMessages3.xml has a separate MSG_SERVERCOMMAND (56:172) described as 'Job Server sends command to a Zone Server'.
- **Ordering.** 4.11/4.14 build the player's CoreObject for LOGINCOMPLETE before the template store (5.01). The player object is template 1 (ObjectData/PlayerObject.xml per 3.11) and carries behaviors. Either 4.14 needs 5.01 or a stub template path, or the plan should state that behaviors are hand-built until 5.01. **Resolved on 2026-09-25:** neither. `sObjectTemplateMgr` reads template 1 the way the client does, through TemplateManifest.xml to ObjectData/PlayerObject.xml in the user's own Root.wad, and the player object takes its 39 behaviors in that template's order, each as the class `behavior_client_class` names or an empty slot; 5.01 grows the same store to every template.
- **Missing work.** LOGINCOMPLETE segmentation: MSG_LOGINCOMPLETE carries SegmentedMessage and LastSegment UBYT fields, plus DynamicServerProcID, Permissions, IsCSR, ZoneServer, HourOffset, PickUpAllEnabled and others (GameMessages.xml 5:108). 4.11/4.14 mention none of the segmentation semantics.
- **Missing work.** MSG_ATTACH also carries PassKey, MachineID, Reattach, Retry, SessionID/SessionSlot and TargetPlayerID (5:7). 4.07/4.13 validate only LoginKey. Reattach/Retry semantics need an owner earlier than 6.08.
- **Missing work.** Multi-realm inter-process communication: friends presence, cross-realm whispers, party member zones, realm-transfer handoff, and kicking a character online on another realm all need a login<->game or game<->game bus (or DB polling). Only heartbeat rows (4.03) and login keys exist.
- **Oversized.** 4.14 LOGINCOMPLETE and standing in zone (M). This is the first full CoreObject acceptance by the real client, with segmentation, CriticalObjects and CLIENTZONED. It is historically the hardest single step and should be split: byte-level LOGINCOMPLETE against a decoded capture, then real-client zone-in.
- **Oversized.** 4.08 zone extractor across 3356 zone WADs with 0 failures (M). The failure triage alone is open-ended.
- **Ordering.** 4.04 carries LOG-11's acceptance as well as WLD-1's, but only the LocationString part of LOG-11 is its own work. Three of its checks, the CharID selection errors, the MSG_ATTACH integration and the real-client Play, describe 4.05 and cannot be earned until 4.05 lands, so 4.04 stays open with its own eight ticked. Found on 2026-09-23 when an outside contributor delivered every part of 4.04 that 4.04 builds.
- **Ordering.** 4.12 carries WIZ-1 whole, but its title leaves the PLAYERWIZBANG broadcast out, because other clients in range only exist from 6.01, and 6.02 carries that handler and the spellbook half of the real-client check already. **Resolved on 2026-09-25:** 4.12 builds the rest, and its deliverable and check no longer name the broadcast. The client sends its entry chatter after MSG_LOGINCOMPLETE but before MSG_CLIENTZONED, so the handlers take it from LoggedIn as well as InWorld; the tests of every WIZARD name and of the WIZARD orders read the user's own XML, so they are client tests.
- **Correction.** 4.13 and 4.14 asked for CriticalObjects as a wrapped, empty CriticalObjectList. The client's own GameClient::MSG_LoginComplete, read in Ghidra, takes an empty CriticalObjects as no critical objects and would read a list there with no envelope, so the checks now ask for it empty, as Entering the world in doc/ARCHITECTURE.md records. **Resolved on 2026-09-25.**
- **Ordering.** 4.14 carried WLD-8 whole, static zone objects and all, though its title keeps only WLD-8's MSG_CLIENTZONED, while 5.02 carries static zone objects with the same checks and depends on 4.14 and on 5.01's template store, so neither could close before the other. **Resolved on 2026-09-25:** 4.14 keeps the MSG_CLIENTZONED deliverable, whose check it has earned, and the spawning deliverables and checks, the live-map '.reload zone_object' one included, are 5.02's, which 4.09's '.reload zone_object' line now names.
- **Ordering.** 4.09 depends on 4.15, so 4.15 lands before 4.09. Settings named in 4.02-4.14 read their config value until 4.16 lands, then become live settings with the same keys.

## 4.01 sWorld tick, GameSession, ScriptMgr hooks and AddSC loaders (new core)

**Goal:** Game update loop and the hook framework every later domain uses.

**Size:** M. **Depends on:** 2.09, 2.08

**Acceptance**

- [x] A WorldScript OnUpdate registered through AddSC_ runs every tick. `ScriptMgrTest.TheGeneratedLoaderBringsInTheScriptsThatAreMerelyPresent` finds world_heartbeat through the loader CMake wrote, and `EveryHookReachesEveryScriptInTheOrderTheyRegistered` and `WorldTest.EverySessionIsDrainedBeforeTheScriptsRun` show the tick reaching it; the game server's OnUpdate is `sWorld.Update`, which carries it
- [x] A module in modules/ is discovered by CMake with no core edits. modules/example is a folder and nothing else: configure reported "1 in src/server/scripts, 1 in modules", the generated loader calls Addmodules_example beside the script, and `ScriptMgrTest.AModuleUnderModulesIsLoadedTheSameWayAScriptIs` fails if it stops being found
- [x] GameSession queue drains on the world thread (thread-id test). `WorldTest.QueuedWorkRunsOnTheThreadThatCallsUpdate` queues from another thread and records the thread the work ran on, asserting it is the one that called Update and not the one that queued it, and `TheWorldThreadIsTheOneThatCalledUpdateAndNoOther` shows another thread is never mistaken for it

## 4.02 CommandMgr and security levels, console first (new; WIZ-5 core)

**Goal:** CommandScript tables with account_access levels, from the console.

**Size:** M. **Depends on:** 4.01, 2.13

**Acceptance**

- [x] PLAYER level running a GAMEMASTER command gets 'no such command'. `CommandMgrTest.AnAccountBelowACommandsLevelIsToldThereIsNoSuchCommand` shows nothing runs, and `ARefusalReadsExactlyLikeACommandThatDoesNotExist` shows the words are the same as for a command that is not there, so the table gives nothing away
- [x] '.character gold 500' parses into (character, gold, [500]). `CommandMgrTest.ACommandIsTheDeepestNameThatMatchesAndTheRestAreArguments` reads the name as `character gold` with `500` left over, and `TheClientsPrefixIsTakenOffBeforeTheWordsAreRead` shows the same line works with and without the prefix
- [x] Console `server info` works. Run against a real game server on the maintainer's machine: `6 command(s) are ready, typed after .`, then `Project Ambrose de63e83 on land-panel2` and `The world has ticked 0 time(s) and holds 0 session(s)`. `CommandMgrTest.TheCommandsTheScriptsShipAreFoundByTheNamesAnOperatorTypes` holds it in the suite

### Detailed spec from WIZ-5: Account security levels and GM command framework

An account's security level decides which chat-prefixed GM commands it may run, and the first cs_ command groups give testers a harness for every later milestone.

**Deliverables**

- data/sql/updates/db_login: account_access (account_id, realm_id, security_level) with the levels 2.13 settled, PLAYER=0, MODERATOR=1, GAMEMASTER=2, ADMINISTRATOR=3, CONSOLE=4 (AzerothCore precedent), holding per-realm overrides of `login.account.security_level`
- src/server/game/Chat/CommandMgr: CommandScript tables, argument parsing, security check, per-command help. It takes over the console command table from 2.13, including the login server's account commands
- src/server/scripts/Commands/cs_gm.cpp (.gm on/off, .gm visible), cs_character.cpp (.character level, .character gold, .character xp, .character heal), cs_lookup.cpp (.lookup item / spell by name)
- Replies via SYSTEM MSG_SERVERMESSAGE or GAME MSG_CLIENTNOTIFYTEXT
- Set LOGINCOMPLETE IsCSR and Permissions from the security level (coordinate with NET/LOG)
- gameserver.conf.dist: GM.CommandPrefix, GM.LogCommands, which become live settings applying from the next command once 4.16 lands
- Each command table declares a default security level; a command_security row (command, security_level) overrides it, and `.reload command_security` applies an edited row once 4.15 lands
- src/test/server/game/CommandMgrTest.cpp

**Client messages:** GAME MSG_COMMAND, GAME MSG_COMMANDRESULT, SYSTEM MSG_SERVERMESSAGE, GAME MSG_CLIENTNOTIFYTEXT

**Database tables**

- login.account_access
- world.command_security
- characters.gm_command_log (optional)

**Acceptance**

- [x] Unit test: a PLAYER-level account running a GAMEMASTER command gets 'no such command' and nothing executes. `CommandMgrTest.AnAccountBelowACommandsLevelIsToldThereIsNoSuchCommand`
- [x] Unit test: the command table parses '.character gold 500' into (character, gold, [500]). `CommandMgrTest.ACommandIsTheDeepestNameThatMatchesAndTheRestAreArguments`
- [x] Unit test: a command_security row raising a command to ADMINISTRATOR refuses a GAMEMASTER account. `CommandMgrTest.ACommandSecurityRowOverridesTheLevelTheScriptGave`, and `AChildIsNeverEasierToReachThanItsGroup` shows raising a group carries its commands with it
- [ ] Real client, GM account: typing '.help' shows the command list in the chat window, and nearby players see no bubble. The same text from a player account shows up as normal chat or is refused, depending on config. This one waits on more than the maintainer's machine: nothing carries a typed line from the client to CommandMgr until the game server has its message handlers in 4.05, so the table, the levels and the parsing are built and tested while the path a client's words take to them is not.

**Risks**

- Whether the stock client ever sends GAME MSG_COMMAND itself is unverified. The capture only shows '.mod ...' arriving as REQUESTRADIALCHAT.
- The bit meanings of LOGINCOMPLETE Permissions are unverified. The reference sends a constant 207 (0b11001111).

## 4.03 Realm registry and heartbeat (LOG-10)

**Goal:** Loginserver knows live realms.

**Size:** M. **Depends on:** 2.13, 4.01

**Client messages:** MSG_REQUESTSERVERLIST, MSG_SERVERLIST

**Acceptance**

- [x] A realm with last_heartbeat older than Realm.OfflineAfterIntervals (default 3) is excluded; policy picks the named, else least-full realm
- [x] Starting a gameserver refreshes its heartbeat; stopping it goes offline

### Detailed spec from LOG-10: Realm registry: realmlist table and gameserver heartbeat

The login server knows which gameservers (realms) are up, where they listen, and their population, so it can route character select and answer realm queries.

**Deliverables**

- data/sql/updates/db_login/<date>_NN.sql: realmlist (id, name VARCHAR(32) = RealmNames.lang key, address, local_address, port, flags (offline/recommended/full/test), population INT, player_limit INT, last_heartbeat DATETIME), realm_online_character (realm_id, character_guid, account_id)
- src/server/shared/Realms/RealmList.{h,cpp} (sRealmList) with no DB access; loading lives in apps/loginserver/Realms/RealmLoader.cpp using LoginDatabase
- src/server/game/World/RealmHeartbeat.{h,cpp}: the gameserver updates its realmlist row every Realm.HeartbeatInterval seconds (population, last_heartbeat)
- Realm selection policy: the realm named in MSG_SELECTCHARACTER.ServerName if valid, otherwise Realm.DefaultRealm, otherwise the least-full online realm
- Realm.HeartbeatInterval, Realm.DefaultRealm and Realm.OfflineAfterIntervals become live settings once 4.16 lands; a changed interval applies from the next heartbeat, and the loginserver's realmlist refresh picks up new or edited rows without a restart
- HandleRequestServerList: reply with an empty MSG_SERVERLIST (the reference does the same; the capture never shows the request)

**Client messages:** MSG_REQUESTSERVERLIST, MSG_SERVERLIST

**Data sources**

- Root.wad Locale/en-US/RealmNames.lang (display names; 'Ambrose' is the first key)

**Database tables**

- realmlist
- realm_online_character

**Acceptance**

- [x] Unit: a realm whose last_heartbeat is older than Realm.OfflineAfterIntervals heartbeat intervals (default 3) is excluded; the selection policy picks the named realm when it is online, otherwise the least-full one, and returns none when every realm is offline
- [x] Integration: starting one gameserver makes its realmlist row show a fresh heartbeat within 1 interval, and stopping it makes the loginserver treat it as offline

**Risks**

- There is no gameserver yet; the heartbeat half depends on the WLD/FND gameserver app skeleton.
- Unlike AzerothCore, every realm shares one db_characters. The realm_id of a character's last login must not be used to partition data.

## 4.04 World wire math and LocationString (WLD-1 + LOG-11 LocationString)

**Goal:** Position/direction packing and 'x,y,z,yaw'.

**Size:** S. **Depends on:** 1.14

**Client messages:** MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_SERVERTELEPORT

**Acceptance**

- [x] Packing (-2408.09, 2609.10, -7.13) is within 4 units (MovementPackingTest.CapturedCoordinatesRoundTripWithinFourUnits)
- [x] '-32,-552,-28,6.350083' formats and parses exactly; '857.9,5730.8,-18.09,1.40' parses under fr-FR; 'Start' is named (LocationStringTest.FormatsAndParsesTheCapturedCoordinates, .ParsesDecimalPointsIndependentlyOfTheProcessLocale, .PreservesNamedLocations)
- [x] GAME ordinals ADDEFFECT=2, ATTACH=7, CLIENTMOVE=36, ENTERSTATE=72, LOGINCOMPLETE=108, NEWOBJECT=122, SERVERMOVE=218, WIZBANG=247 (GameOrdinalClientTest.ServiceFiveOrdinalsMatchTheClientCatalog, run against the pinned install)

### Detailed spec from WLD-1: World wire math: coordinate/direction packing, location strings, GAME ordinals

Every later world milestone can pack and unpack positions and message ids with unit-tested, client-verified rules.

**Deliverables**

- src/server/game/Movement/MovementPacking.h/.cpp: USHRT location <-> float (value read as signed int16 times 4), direction UBYT <-> yaw radians, round-trip helpers
- src/server/game/Movement/LocationString.h/.cpp: parse and format the 'x,y,z,yaw' compact form with invariant culture, and tell a named location (e.g. 'Start') apart from coordinates
- src/test/server/game/Movement/MovementPackingTest.cpp
- src/test/server/shared/Messages/GameOrdinalTest.cpp: asserts the message-order rule for service 5 against the XML read from the user's client

**Client messages:** MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_SERVERTELEPORT, MSG_ATTACH, MSG_LOGINCOMPLETE, MSG_NEWOBJECT

**Data sources**

- Root.wad GameMessages.xml (254 entries, 253 distinct tags; MSG_SERVER_ERROR/MSG_SERVERERROR and MSG_VIEWACCOUNT/MSG_CSRVIEWACCOUNT differ between tag and _MsgName)
- Root.wad Messages/MoveBehaviorMessages.xml

**Acceptance**

- [x] Unit: packing (x=-2408.09,y=2609.10,z=-7.13) and unpacking lands within 4 units per axis; values below -32768*4 are rejected or clamped by a documented rule (MovementPackingTest.CapturedCoordinatesRoundTripWithinFourUnits and .OutOfRangeAndNonFiniteCoordinatesAreRejected, which hold the documented rule that an out-of-range or non-finite value is refused rather than clamped)
- [x] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step (MovementPackingTest.YawCardinalValuesRoundTripWithinOneByteStep)
- [x] Unit: '857.9,5730.8,-18.09,1.40' parses to 4 floats under a fr-FR process locale; 'Start' parses as a named location (LocationStringTest.ParsesDecimalPointsIndependentlyOfTheProcessLocale, which sets the global locale to fr-FR and skips where it is not installed, and .PreservesNamedLocations)
- [x] Unit: GAME ordinals computed by sorting the XML element tag names (not _MsgName) ordinally, with the duplicate MSG_REMOVEOBJECT collapsed, give ADDEFFECT=2, ATTACH=7, CLIENTMOVE=36, ENTERSTATE=72, LOGINCOMPLETE=108, NEWOBJECT=122, SERVERMOVE=218, WIZBANG=247, matching the live capture in a local session capture (GameOrdinalClientTest.ServiceFiveOrdinalsMatchTheClientCatalog, run against the pinned install)

**Risks**

- Direction byte scale is unverified: the behavior reference uses two conflicting formulas (yaw/2pi*250 in MoveService, 360/255 with a tolerance factor in WizardService). Confirm by watching a real client turn in place.
- The ordinal rule is inferred from a capture plus the reference generator's ordinal sort. Which duplicate MSG_REMOVEOBJECT definition wins is unknown, though both have the same single GID field.

### Detailed spec from LOG-11: Character select and handoff: MSG_SELECTCHARACTER -> MSG_CHARACTERSELECTED

Picking a wizard sends the client to the right gameserver with a one-time key, so the client's MSG_ATTACH carries data the gameserver can trust.

**Deliverables**

- CharacterHandler::HandleSelectCharacter: check ownership and not deleted; pick a realm (LOG-10); create a login key (base64 32 random bytes) in login_key (key PK, account_id, character_guid, realm_id, machine_id, created, expires = now + Login.KeyTTL, used TINYINT); Login.KeyTTL becomes a live setting once 4.16 lands and applies to the next key issued
- Reply MSG_CHARACTERSELECTED{IP=realm.address (or local_address for LAN clients), TCPPort=realm.port, UDPPort=realm.port, Key, UserID, CharID, ZoneID=<zone instance GID, 0 or realm-assigned>, ZoneName=characters.zone, Location='x,y,z,yaw' or 'Start' when position is unset, Slot=0, PrepPhase=0, Error=0, LoginServer=Login.Name, PlatformType=0}; mark the session CharacterSelected and let the client close the socket
- On failure: MSG_CHARACTERSELECTED{Error=1} then close
- src/server/shared/Util/LocationString.{h,cpp}: format and parse the compact 'x,y,z,yaw' string
- src/test/server/shared/Util/LocationStringTest.cpp, src/test/server/apps/loginserver/SelectCharacterTest.cpp

**Client messages:** MSG_SELECTCHARACTER, MSG_CHARACTERSELECTED, MSG_ATTACH

**Data sources**

- Sniffer capture lines 10-12

**Database tables**

- login_key
- realmlist
- characters

**Acceptance**

- [x] Unit: LocationString formats (-32,-552,-28, yaw 6.350083) as '-32,-552,-28,6.350083', the exact string in the capture, and parses it back (LocationStringTest.FormatsAndParsesTheCapturedCoordinates)
- [x] Unit: selecting another account's CharID, a deleted character or with no realm online gives Error!=0 and no login_key row (SelectCharacterTest.AnotherAccountsWizardIsRefusedAndWritesNoKey, .ADeletedWizardIsRefusedAndWritesNoKey, .NoRealmOnlineIsRefusedAndWritesNoKey, with .ARealmThatStoppedBeatingIsRefusedRatherThanUsedAnyway and .ANamedRealmThatIsNotThereIsRefusedRatherThanSwappedForAnother for the two ways a realm can be missing, and .AnOwnWizardOnAnOnlineRealmIsSentThereWithAKeyThatWasWrittenDown for the pick that works)
- [x] Integration: a stub TCP listener on the realm port receives a connection and a GAME MSG_ATTACH whose LoginKey == Key, UserID and CharID match, and ZoneName and Location echo the CHARACTERSELECTED values (the behavior at capture lines 11-12). The real client made that exchange in the client driver's enter-world run 20260925-114135 on 2026-09-25: the login server logged `Session 2 sent account 1 with wizard 1 to realm Ambrose Driver at 127.0.0.2:12433, zone WizardCity/WC_Ravenwood at Start, on a key good for 60 second(s)`, and the game server listening on that port logged `Session 1 from 127.0.0.1 is attaching as account 1 with wizard 1 for zone WizardCity/WC_Ravenwood at Start, on a key of 44 character(s)` and then `its key accepted and spent`, so the client echoed the key, account, wizard, zone and location it was given. `HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo` holds the same exchange with no client
- [x] Real client: after clicking Play, the loading screen appears and the client connects to the gameserver port (visible in the gameserver log) instead of showing a disconnect dialog (2026-09-24 on the maintainer's own client: Play showed the Entering Game loading screen and the gameserver logged session 1 offered to and accepted from the client at 22:27:12, with no disconnect dialog; the client then waits there, because LOGINCOMPLETE is 4.14's)

**Risks**

- What ZoneID in CHARACTERSELECTED means is unverified (the reference fills it with the gameserver port); WLD decides.
- The sniffer's same-length address rewrite needs a dotted IPv4 string; a hostname in IP is untested.

## 4.05 Character select and login key (LOG-11)

**Goal:** CHARACTERSELECTED points the client at the right gameserver.

**Size:** M. **Depends on:** 3.09, 4.03, 4.04

**Client messages:** MSG_SELECTCHARACTER, MSG_CHARACTERSELECTED

**Acceptance**

- [x] Another account's CharID, deleted character or no realm gives Error!=0 and no login_key
- [x] Stub listener receives MSG_ATTACH whose LoginKey == Key with matching UserID/CharID. The real game server received it from the real client in the client driver's enter-world run 20260925-114135 on 2026-09-25, which logged the key accepted and spent for account 1 and wizard 1, the ones CHARACTERSELECTED named

### Detailed spec from LOG-11: Character select and handoff: MSG_SELECTCHARACTER -> MSG_CHARACTERSELECTED

Picking a wizard sends the client to the right gameserver with a one-time key, so the client's MSG_ATTACH carries data the gameserver can trust.

**Deliverables**

- CharacterHandler::HandleSelectCharacter: check ownership and not deleted; pick a realm (LOG-10); create a login key (base64 32 random bytes) in login_key (key PK, account_id, character_guid, realm_id, machine_id, created, expires = now + Login.KeyTTL, used TINYINT); Login.KeyTTL becomes a live setting once 4.16 lands and applies to the next key issued
- Reply MSG_CHARACTERSELECTED{IP=realm.address (or local_address for LAN clients), TCPPort=realm.port, UDPPort=realm.port, Key, UserID, CharID, ZoneID=<zone instance GID, 0 or realm-assigned>, ZoneName=characters.zone, Location='x,y,z,yaw' or 'Start' when position is unset, Slot=0, PrepPhase=0, Error=0, LoginServer=Login.Name, PlatformType=0}; mark the session CharacterSelected and let the client close the socket
- On failure: MSG_CHARACTERSELECTED{Error=1} then close
- src/server/shared/Util/LocationString.{h,cpp}: format and parse the compact 'x,y,z,yaw' string
- src/test/server/shared/Util/LocationStringTest.cpp, src/test/server/apps/loginserver/SelectCharacterTest.cpp

**Client messages:** MSG_SELECTCHARACTER, MSG_CHARACTERSELECTED, MSG_ATTACH

**Data sources**

- Sniffer capture lines 10-12

**Database tables**

- login_key
- realmlist
- characters

**Acceptance**

- [x] Unit: LocationString formats (-32,-552,-28, yaw 6.350083) as '-32,-552,-28,6.350083', the exact string in the capture, and parses it back (LocationStringTest.FormatsAndParsesTheCapturedCoordinates)
- [x] Unit: selecting another account's CharID, a deleted character or with no realm online gives Error!=0 and no login_key row (SelectCharacterTest.AnotherAccountsWizardIsRefusedAndWritesNoKey, .ADeletedWizardIsRefusedAndWritesNoKey, .NoRealmOnlineIsRefusedAndWritesNoKey, with .ARealmThatStoppedBeatingIsRefusedRatherThanUsedAnyway and .ANamedRealmThatIsNotThereIsRefusedRatherThanSwappedForAnother for the two ways a realm can be missing, and .AnOwnWizardOnAnOnlineRealmIsSentThereWithAKeyThatWasWrittenDown for the pick that works)
- [x] Integration: a stub TCP listener on the realm port receives a connection and a GAME MSG_ATTACH whose LoginKey == Key, UserID and CharID match, and ZoneName and Location echo the CHARACTERSELECTED values (the behavior at capture lines 11-12). The real client made that exchange in the client driver's enter-world run 20260925-114135 on 2026-09-25: the login server logged `Session 2 sent account 1 with wizard 1 to realm Ambrose Driver at 127.0.0.2:12433, zone WizardCity/WC_Ravenwood at Start, on a key good for 60 second(s)`, and the game server listening on that port logged `Session 1 from 127.0.0.1 is attaching as account 1 with wizard 1 for zone WizardCity/WC_Ravenwood at Start, on a key of 44 character(s)` and then `its key accepted and spent`, so the client echoed the key, account, wizard, zone and location it was given. `HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo` holds the same exchange with no client
- [x] Real client: after clicking Play, the loading screen appears and the client connects to the gameserver port (visible in the gameserver log) instead of showing a disconnect dialog. In the client driver's enter-world run 20260925-114135 on 2026-09-25 the press on Play was followed by the game server's `Session 1 offered to 127.0.0.1`, `accepted` and `attached`, and the run's screenshot of the loading screen, taken after it sent the wizard its object, shows no dialog

**Risks**

- What ZoneID in CHARACTERSELECTED means is unverified (the reference fills it with the gameserver port); WLD decides.
- The sniffer's same-length address rewrite needs a dotted IPv4 string; a hostname in IP is untested.

## 4.06 Login-to-game handoff transport (NET-13)

**Goal:** Client reconnects to gameserver without timeouts.

**Size:** S. **Depends on:** 4.05, 2.10

**Client messages:** MSG_CHARACTERSELECTED, MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [x] Integration: login handshake, CHARACTERSELECTED, disconnect, game handshake, MSG_ATTACH in STATUS_CONNECTED
- [x] Real client: no 'connection lost' at character select; new game session id logged (2026-09-24: the login server sent account 1 with wizard 1 to realm Ambrose at 127.0.0.1:12333 and the gameserver logged a new session 1 accepted 50 ms later, the client showing the loading screen rather than a connection dialog)

### Detailed spec from NET-13: Login-to-game connection handoff transport

The client disconnects from the loginserver after MSG_CHARACTERSELECTED and reconnects to the gameserver with a fresh session, and neither side times out during the switch.

**Deliverables**

- LoginSession: after sending MSG_CHARACTERSELECTED (7:3; IP STR, TCPPort INT, UDPPort INT, Key STR, UserID/CharID/ZoneID GID, ...) the session moves to the CharacterSelected status, suspends the keepalive timeout and waits for the client to close; server-side close only after Network.HandoffGrace
- GameSession: new SessionOffer on connect; STATUS_CONNECTED allows only MSG_ATTACH (5:7) until LOG/WLD validate the Key; MSG_ATTACHFAILED (5:8) is sent via SendDmlMessageDelayedClose on failure
- Config: gameserver PublicAddress used to fill the IP field. Auto-discovering the public address through an external web service is planned, not yet scheduled, as an opt-in setting, off by default: the service learns the server's address and could return a wrong one, so a discovered address is logged and an explicit PublicAddress always wins
- Network.HandoffGrace and PublicAddress become live settings once 4.16 lands; a change applies from the next handoff, and sessions already in Handoff keep the values they started with

**Client messages:** MSG_CHARACTERSELECTED, MSG_ATTACH, MSG_ATTACHFAILED

**Data sources**

- None

**Acceptance**

- [x] Integration test: a fake client completes login handshake -> CHARACTERSELECTED -> disconnect -> game handshake -> MSG_ATTACH is dispatched in STATUS_CONNECTED (HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo, which stands up both servers and carries the key the login server issued through to the game session, with GameAttachTest.AnAttachIsTakenWhileOnlyConnectedAndIsRefusedWhenNoKeyCanBeSpent holding the status the attach is taken in and .AGameMessageWithNoRuleIsCountedRatherThanActedOn holding what happens to anything else)
- [x] Real client: after picking a character, the server log shows the login socket closed by the client, a new game session id offered and accepted, and MSG_ATTACH received. The client moves past character select to its loading screen, with no 'connection lost' dialog. In the client driver's enter-world run 20260925-114135 on 2026-09-25 the login server logged the pick and then `Session 2 closed` 26 ms later, which it never does itself after sending MSG_CHARACTERSELECTED, then the game server logged `Session 1 offered`, `Session 1 accepted by 127.0.0.1 after 5 ms` and the MSG_ATTACH, and the client went on to its loading screen and into Ravenwood with no dialog
- [ ] A MSG_ATTACH with a bad key produces MSG_ATTACHFAILED and the client returns to an error or the login screen

**Risks**

- The UDPPort field suggests a UDP channel; a reference server never opens one and the client still works, but that is unverified for every feature
- Key semantics belong to LOG; NET only transports them

## 4.07 Gameserver login key validation (LOG-12)

**Goal:** Single-use key checked on attach.

**Size:** S. **Depends on:** 4.06

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [x] A valid key passes once; replayed, expired, other CharID and other realm keys fail
- [x] Random LoginKey gets MSG_ATTACHFAILED and close

### Detailed spec from LOG-12: Gameserver login key validation on MSG_ATTACH

The gameserver accepts a client only with a valid, unexpired, single-use key issued for exactly that account and character, and rejects everything else with MSG_ATTACHFAILED.

**Deliverables**

- src/server/game/Server/LoginKeyValidator.{h,cpp}: atomically consume the login_key row (UPDATE ... SET used=1 WHERE key=? AND used=0 AND expires>NOW()), check account_id == UserID and character_guid == CharID, and check that the realm matches this gameserver
- A hook in game/Handlers/AttachHandler.cpp (owned by WLD) that calls the validator before loading the character; on failure send MSG_ATTACHFAILED{Error=1, Rejected=1} and close
- On success: characters.online=1, insert realm_online_character, account.online=1
- src/test/server/game/Server/LoginKeyValidatorTest.cpp

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED

**Data sources**

- GameMessages.xml MSG_ATTACH and MSG_ATTACHFAILED field lists

**Database tables**

- login_key
- characters
- realm_online_character
- account

**Acceptance**

- [x] Unit: a valid key passes once; replaying the same key, an expired key, a key for another CharID and a key for another realm each fail
- [x] Real client: select a character and the gameserver log shows the key accepted, after which WLD's LOGINCOMPLETE flow runs. In the client driver's enter-world run 20260925-114135 on 2026-09-25 the game server logged `Session 1 attached: account 1 with wizard 1 on realm 1, its key accepted and spent`, then `Session 1 put wizard 1 in WizardCity/WC_Ravenwood instance 1 at (-5.628328, -1531.451, -30.48013) with mobile id 49152, and sent its 309-byte object`, and the client said it had loaded the zone
- [x] Negative: a hand-crafted attach with a random LoginKey (from a test client) gets MSG_ATTACHFAILED, and the socket closes

**Risks**

- The meanings of the Error, Rejected and NoDisconnect values in MSG_ATTACHFAILED are unverified; the reference marks them TODO.
- Split ownership with WLD: the attach handler belongs to WLD, while LOG owns only the validator and the online bookkeeping.

## 4.08 Zone extractor part 1: WizZoneData (WLD-2 + QST-4 zone objects)

**Goal:** zone_template, zone_location, zone_object rows from the install.

**Size:** M. **Depends on:** 3.11, 2.07

**Acceptance**

- [x] 3356 zone WADs with gamedata.bin, 0 decode failures targeted (a full run over the pinned install: 3589 zone WADs scanned, 3356 with gamedata.bin, 3356 decoded, 0 failed, in 33 seconds)
- [x] WC_Hub display key 'WizardZone_TheCommons'; locations include 'Start', 'Target location (WC_Hub Street1 Exit)' (the written zone_template row carries that key, and the 31 zone_location rows include all three named ones with positions)
- [ ] WC_Ravenwood yields 97 CoreObjectInfo incl. templates 38232, 38230, 81102, 1451035, 39088
- [x] Idempotent rerun; git status clean (two runs produce byte-identical SQL, and git status shows nothing extracted into the tree)

### Detailed spec from WLD-2: Zone extractor part 1: WizZoneData to world DB

Every zone's metadata, named locations and static object placements exist as world-database rows produced from the user's own client install.

**Deliverables**

- src/tools/zone_extractor: enumerates GameData/*.wad, maps zone path 'WizardCity/WC_Hub' <-> file 'WizardCity-WC_Hub.wad', decodes gamedata.bin as WizZoneData (client dump hash 0x4C9FDA76), writes SQL or loads through dbimport
- data/sql/base/db_world: schema only for zone_template, zone_location, zone_object
- conf/dist/zone_extractor.conf.dist (GameData path, output mode)

**Data sources**

- <zone>.wad/gamedata.bin (raw ObjectProperty, no BINd header)
- Type dump generated by the project's own type dumper (reference: r806919.Wizard_1_610.json, 6981 classes): WizZoneData, ZoneData, CoreObjectInfo, ClientObjectInfo, SpawnObjectInfo, LocationTemplate, TeleporterTemplate, SpawnPointTemplate

**Database tables**

- world.zone_template
- world.zone_location
- world.zone_object

**Acceptance**

- [x] Running the extractor over r806919 reports 3356 zone WADs with gamedata.bin and lists each decode failure by name, target 0 (3356 of 3589 carry gamedata.bin, all 3356 decode, none fails)
- [x] zone_template row for WizardCity/WC_Hub has display name key 'WizardZone_TheCommons' plus farClip, healingPerMinute, soft/hard limit and noMounts filled (display_name_key WizardZone_TheCommons, far_clip 24500, healing_per_minute 20, soft_limit 50, hard_limit 100, no_mounts 0)
- [x] zone_location for WC_Hub contains 'Start', 'Target location (WC_Hub Street1 Exit)' and 'Target location(WC_Hub Ravenwood)' with position and direction (all three present among 31 rows, none with a null location or direction)
- [ ] zone_object for WC_Hub has one row per m_objectList entry with templateID, location, orientation, scale, zoneTag, startState, loadingType and a nullable serialized spawnRequirements column
- [x] Re-running is idempotent (same row counts), and git status shows no extracted files (two runs hash identically and the tree stays clean)

**Risks**

- Depends on the ObjectProperty codec handling the file-level (non-message) serializer mode used by gamedata.bin
- Some m_objectList entries are CombatSigilObjectInfo or MinigameSigilInfo subclasses that other domains (CMB, EXT) consume. Store the class hash so they can filter.

### Detailed spec from QST-4: Zone NPC placement and spawn-table extractor

world.zone_object and world.spawn_* hold every NPC and interactable placement and every spawner for each zone, so the game server can spawn quest givers where the client expects them.

**Deliverables**

- src/tools/extractor/ZoneObjectExtractor.cpp: decode WizZoneData.m_objectList (CoreObjectInfo: templateID, nObjectID, location, orientation, scale, zoneTag, startState, overrideName, spawnRequirements, loadingType) from each zone's gamedata.bin.
- src/tools/extractor/SpawnExtractor.cpp: decode spawnData.xml SpawnManager -> SpawnObject (name, id, maxNumberOfSpawns, respawnRate, globalDynamicReqs, zone level) -> SpawnItem (percentChance, SpawnObjectInfo with kStartNodeType, pathID).
- data/sql/base/db_world: zone_object, spawn_group, spawn_item, spawn_requirement (schema only).

**Data sources**

- Zone WADs in GameData/*.wad: gamedata.bin (root WizZoneData 0x4c9fda76; a different binary layout from BINd, with 2-byte string lengths and enums as ints), spawnData.xml (BINd SpawnManager 0x3752f969)
- ClassicMode-* zone WADs duplicate WizardCity zones

**Database tables**

- zone_object
- spawn_group
- spawn_item
- spawn_requirement

**Acceptance**

- [ ] Integration test: WizardCity/WC_Ravenwood yields 97 CoreObjectInfo templateIDs, including NPC templates 38232, 38230, 81102, 1451035 (WC-Bartleby) and 39088. Its spawnData yields 5 SpawnObjects, including SpawnPoint_Wood_01 with SNT_RANDOM_UNIQUE.
- [ ] Integration test: WizardCity/WC_Hub spawnData contains HalloweenSpawner1 with a ReqGlobalRegistryValue requirement.
- [x] Full run over ~3356 zone WADs finishes, with a per-zone error count of 0 or a listed set of unknown classes. (3356 zones, 0 errors, 33 seconds)

**Risks**

- Unverified whether EVERY retail NPC placement is in client gamedata.bin. Some quest-gated NPCs may exist only server-side and would need authored rows in a world.custom_zone_object table.
- gamedata.bin decoding is probably owned by WLD. This milestone should reuse it, not write a second decoder.

## 4.09 sZoneMgr and reload (WLD-4)

**Goal:** Zone rows in memory, GM reload.

**Size:** S. **Depends on:** 4.08, 4.02, 4.15

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT

**Acceptance**

- [x] An unknown location falls back to 'Start' (2026-09-25: ZoneMgrTest.ANameTheZoneDoesNotHoldFallsBackToItsStart, and on a running gameserver against the maintainer's own extracted zones `zone place WizardCity/WC_Hub Door_That_Is_Not_There` answered with the Hub's Start at -3.267, 50.246, -30.473, naming the fall back)
- [x] '.zone info WizardCity/WC_Hub' prints counts (2026-09-25 on a running gameserver: display name key WizardZone_TheCommons, 31 named place(s), 177 placed object(s), soft limit 50)
- [x] '.reload zone_location' applies without restart (2026-09-25 on a running gameserver: the Hub's Start was edited in the world database, stayed at its old coordinates until `reload zone_location` reported generation 1, and then came back at 111, 222, 333, with nothing restarted)
- [x] A reload that fails validation keeps the old store and reports every error (2026-09-25: ZoneMgrDatabaseTest.ARowNamingAZoneNoTemplateHoldsFailsTheBuildAndKeepsWhatWasServing, through the same ReloadMgr path the command takes)

### Detailed spec from WLD-4: Zone templates in memory: sZoneMgr and reload

The game server loads zone, location and object rows at startup into a global manager, and GMs can reload them without a restart.

**Deliverables**

- src/server/game/Zones/ZoneMgr.h/.cpp (sZoneMgr): ZoneTemplate, ZoneLocation and ZoneObjectSpawn stores keyed by zone path, location lookup with 'Start' fallback, each a 4.15 reload target that builds off to the side, validates, swaps, and keeps the old store on failure
- src/server/game/World/World.cpp: startup load order and timing log
- src/server/scripts/Commands/cs_zone.cpp: '.zone info <path>', '.reload zone_template', '.reload zone_location', '.reload zone_object'
- '.reload zone_object': live maps spawn rows that were added and despawn rows that were removed once maps (4.10) and object spawning (5.02) exist
- src/test/server/game/Zones/ZoneMgrTest.cpp using an in-memory fixture DB

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT

**Data sources**

- world DB rows from WLD-2

**Database tables**

- world.zone_template
- world.zone_location
- world.zone_object

**Acceptance**

- [x] Unit: an unknown location name falls back to 'Start'; an unknown zone returns a typed error (2026-09-25: ZoneMgrTest, five tests with no database, among them ZoneLookup::FellBackToStart and ZoneLookup::UnknownZone)
- [x] gameserver startup logs zone template, location and object counts plus load time (2026-09-25: 'Loaded 1339 zone(s), 3892 named place(s) and 25823 placed object(s) in 1927 ms' from the maintainer's own r806919 install, extracted by zone_extractor with 1339 of 1339 gamedata.bin files decoded)
- [x] '.zone info WizardCity/WC_Hub' prints display key, object count and location count in chat or console (2026-09-25, on the console as recorded above)
- [x] Editing a zone_location row, then '.reload zone_location', returns the new coordinates without a restart (2026-09-25: ZoneMgrDatabaseTest.EditingARowAndReloadingThatTargetReturnsTheNewCoordinates, and the same on a running gameserver as recorded above)
- [x] A zone_location row with an unknown zone path makes '.reload zone_location' fail with that row named, and lookups still return the old coordinates (2026-09-25: ZoneMgrDatabaseTest.ARowNamingAZoneNoTemplateHoldsFailsTheBuildAndKeepsWhatWasServing names the row's zone and still returns the Hub's Start at its old coordinates)

**Risks**

- Holding objects for all 3356 zones in memory may be heavy. Consider loading object spawns lazily per zone on first instance.

## 4.10 Maps, instances, mobile ids, GID service (WLD-5)

**Goal:** Zone instances allocate ids with no client.

**Size:** M. **Depends on:** 4.09, 4.01

**Acceptance**

- [x] 1000 allocate/release cycles never reuse within delay (2026-09-25: MobileIdAllocatorTest.AThousandCyclesNeverHandOutAnIdThatIsHeldOrCooling, random allocate and release on a moving clock, checking every id against what is held and when each released one may return)
- [x] Exhausted range errors (2026-09-25: MobileIdAllocatorTest.RunningOutOfThePlayerRangeIsAnAnswerNotACrash hands out all 16383 player ids, gets an empty answer for the next one and can still allocate from the object range)
- [x] Same zone_object gives same permID across restarts (2026-09-25: ObjectGuidTest.TheSamePlacedObjectGetsTheSamePermIdOnEveryRun pins the value 0x5658625877D119B9 for WizardCity/WC_Hub, template 4242, object 7, computed independently of the server, so a permID cannot drift between runs or machines without the test failing)

### Detailed spec from WLD-5: Maps, instances and identifiers

The server can create, tick and destroy zone instances that allocate mobile ids and runtime GIDs, with no client involved.

**Deliverables**

- src/server/game/Zones/Map.h/.cpp: one zone instance with dynamic zone id, player and object containers, and an update tick
- src/server/game/Zones/MapMgr.h/.cpp (sMapMgr): find-or-create a public instance by zone path, destroy an empty instance after Zone.UnloadDelay
- src/server/game/Entities/ObjectGuid.h/.cpp: runtime 64-bit GID generator plus a stable permID derived from zone, template and spawn
- src/server/game/Zones/MobileIdAllocator.h/.cpp: reserved low range for world objects, upper range for players, delayed release
- conf/dist/gameserver.conf.dist: Zone.UnloadDelay, Zone.MobileIdReleaseDelay, World.UpdateInterval, which become live settings once 4.16 lands; the delays apply to the next empty instance or released id, and the interval applies from the next tick
- src/test/server/game/Zones/MapTest.cpp, MobileIdAllocatorTest.cpp

**Acceptance**

- [x] Unit: 1000 allocate/release cycles never hand out an id still held or within its release delay (2026-09-25: as above; a released id also goes to the back of its range's line once its delay passes, so the id the client most recently saw leave is the last to come back)
- [x] Unit: exhausting the player range returns an error, not a crash (2026-09-25: as above)
- [x] Unit: two instances of the same zone get different dynamic zone ids; an empty instance is destroyed only after the delay (2026-09-25: MapTest.TwoInstancesOfOneZoneGetDifferentDynamicZoneIds, MapTest.AnEmptyInstanceIsTakenDownOnlyAfterItsDelay and MapTest.AWizardWhoComesBackBeforeTheDelayKeepsTheInstance)
- [x] Unit: the delay is read when an instance empties, so a changed Zone.UnloadDelay applies to the next instance that empties with no restart (2026-09-25: MapTest.TheDelayInForceWhenAnInstanceEmptiesIsTheOneUsed empties one instance under a 60 second delay, lowers the setting to 5, empties a second, and six seconds later only the second is taken down)
- [x] Unit: the same zone_object row gives the same permID across restarts, while runtime GIDs are unique (2026-09-25: ObjectGuidTest, the pinned permID above and 10000 runtime GIDs that are all distinct and all at or above 1<<52, where no stored id falls)

**Risks**

- The reference frees mobile ids with a ~2s delay because a fast leave-and-rejoin let MSG_NEWOBJECT race MSG_REMOVEOBJECT for a reused id. Keep a delay.
- Threading model (map-per-thread or a single world thread) is a NET/FND decision and affects this API

## 4.11 CoreObject serializer and client-object builder (OBJ-9 + WLD-6)

**Goal:** Bytes for MSG_NEWOBJECT and MSG_LOGINCOMPLETE.

**Size:** M. **Depends on:** 3.05, 3.07

**Client messages:** MSG_NEWOBJECT, MSG_LOGINCOMPLETE

**Acceptance**

- [x] ClientObject encodes (2,2), WizClientObject (104,2); round-trip equal. CoreObjectSerializerTest writes both headers with their template ids and reads each object back equal, a nested item keeping its own pair and a nested behavior the plain form
- [x] Public mask omits authority-only properties. CoreObjectSerializerTest
- [ ] Local-gated: captured LOGINCOMPLETE Data begins 68 02 01000000 and decodes fully. The maintainer's capture begins 68 02 01000000 and `client core --trailing` reads it through every property but the last: the player carries its stats as the nested m_gameStats, and the capture ends where r806919's final WizGameStats property, m_dontAllowEndorsements, would begin, so it was written against a revision without that property. It stays open for a capture of the r806919 client

### Detailed spec from OBJ-9: CoreObject serializer variant

Game objects for MSG_LOGINCOMPLETE and MSG_NEWOBJECT serialize with the block/type/template prefix that the client uses to create them.

**Deliverables**

- src/server/shared/ObjectProperty/CoreObjectSerializer.h/.cpp: the object header is u8 block, u8 type, u32 template id. Block 0 and type 0 mean a plain class hash follows instead
- A block/type table as a config/data table (not code constants): ClientObject 2/2, WizClientObject 104/2, WizClientObjectItem 115/9, WizClientPet 106/2, WizClientMount 108/2, ClientReagentItem 132/9, ClientRecipe 131/131 (from behavior study; each entry to be confirmed). It is the world database's core_object_type, which holds only the pairs a capture proves, 104/2 and 115/9, each with its evidence; the others join as a capture or the client proves them
- `.reload core_object_type` once 4.15 lands: validates that every class resolves in the type registry and no block/type pair repeats, swaps the table, and keeps the old one on failure; objects already sent keep the prefix they were sent with
- src/test/server/shared/ObjectProperty/CoreObjectSerializerTest.cpp

**Acceptance**

- [ ] Local-gated test: the captured MSG_LOGINCOMPLETE Data blob (4296 bytes after inflate) begins 68 02 01000000, which is block 104, type 2 (WizClientObject), template 1 (PlayerObject), and decodes fully
- [ ] Real client, with WLD/NET wiring: MSG_LOGINCOMPLETE (GameMessages.xml, service 5) with zlib-enveloped Data spawns the player avatar in WizardCity/WC_Ravenwood; MSG_NEWOBJECT.Data makes an NPC appear

**Risks**

- Only block 104/type 2 has been seen in captures. The other table entries are unverified

### Detailed spec from WLD-6: CoreObject envelope and client-object builder

The server can serialize a runtime world object into exactly the bytes MSG_NEWOBJECT and MSG_LOGINCOMPLETE carry.

**Deliverables**

- src/server/shared/ObjectProperty/CoreObjectSerializer.h/.cpp: prefix uint8 block, uint8 type, uint32 (templateID for CoreObjects, else class hash), then the property stream
- src/server/shared/ObjectProperty/SerializedBlob.h/.cpp: 4-byte SerializerBinary wrapper for blobs in STR fields (bit31 set = raw with low 31 bits as length; bit31 clear = zlib, value is uncompressed size)
- src/server/game/Entities/WorldObject, GameObject: build a ClientObject/WizClientObject from template plus spawn (globalID, permID, location, orientation, scale, templateID, zoneTagID, mobileID, inactive behaviors from template behavior list), with Public|Transmit|AuthorityTransmit flag masks
- src/test/server/shared/ObjectProperty/CoreObjectSerializerTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_LOGINCOMPLETE

**Data sources**

- Template store from DAT/OBJ (GameObjectTemplate/WizGameObjectTemplate behaviors)
- Type dump: ClientObject, WizClientObject, CoreObject property flags

**Acceptance**

- [ ] Unit: a built ClientObject encodes with block/type (2,2); a WizClientObject with (104,2); the round-trip decode is equal
- [x] Unit: blob wrapper round-trips raw and zlib forms; an unwrapped blob fails the validator. BlobEnvelopeTest round-trips both forms, and ObjectFieldTest refuses an unwrapped blob in an enveloped field
- [x] Unit: encoding with the Public flag mask leaves out authority-only properties that the AuthorityTransmit mask keeps. CoreObjectSerializerTest

**Risks**

- The block/type pairs (ClientObject 2/2, WizClientObject 104/2, WizClientPet 106/2, WizClientMount 108/2, WizClientObjectItem 115/9) come from the behavior reference and must be re-derived clean-room, by capture or client analysis
- Per the user's sniffer README, the client dereferences null on an unwrapped blob and crashes, so the wrapper is mandatory

## 4.12 Wizard service skeleton and login chatter (WIZ-1, without wizbang broadcast)

**Goal:** No unknown-message spam on world entry.

**Size:** S. **Depends on:** 2.09, 4.01

**Client messages:** MSG_LOGCLIENTRESOLUTION, MSG_LOGPATCHCLIENTPATCHTIME, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_GETSUBSCRIBERONLYITEMS, MSG_SUBSCRIBERONLYITEMS, MSG_CROWNBALANCE, MSG_DONESHOPPING, MSG_QUESTFINDEROPTION

**Acceptance**

- [x] Every name in WizardMessages/2/3 maps to one handler or the unhandled list. `GameMessageTableClientTest.EveryWorldMessageHasExactlyOneRuleAndTheEntryChatterIsHandled` finds exactly one rule for every GAME, WIZARD, WIZARD2 and WIZARD3 message of the r806919 install, named for it or standing for the rest of its service, with the seven entry messages handled
- [x] UPDATEMANA=233 and ADDSPELLTOBOOK=10 fixtures. `GameMessageTableClientTest.WizardOrdersAreThePlacesOfTheirTagsSortedWithoutRepeats` checks every WIZARD order against its tag's place and pins both, and `client messages --list` now prints `WIZARD MSG_UPDATEMANA (12:233)` and `WIZARD MSG_ADDSPELLTOBOOK (12:10)` from the install
- [x] Real client: login chatter handled with no warnings. The client driver's enter-world run 20260925-114135 logged `asked for its timed access passes and was told it has none`, `asked which items only subscribers may use and was told none`, `is done shopping` and `runs its client at 1280x720, in a window`, with no server warning outside the allow-list and no message dropped

### Detailed spec from WIZ-1: Wizard service dispatch skeleton and login chatter

A character entering the world gets no errors or unknown-message spam from the WIZARD requests the client sends right after login, and every WIZARD message is either handled or deliberately listed as not yet handled.

**Deliverables**

- src/server/game/Handlers/WizardHandler.cpp: register the handlers in the session dispatch table (state: from LoggedIn, since the client sends them before MSG_CLIENTZONED)
- src/server/game/Handlers/WizardHandler.cpp: minimal replies. GETTIMEDACCESSPASSES -> empty TIMEDACCESSPASSES; GETSUBSCRIBERONLYITEMS -> empty SUBSCRIBERONLYITEMS; CROWNBALANCE -> TotalCrowns=0; DONESHOPPING, LOGCLIENTRESOLUTION, LOGPATCHCLIENTPATCHTIME and QUESTFINDEROPTION accepted and logged at debug
- src/server/shared/Messages: an explicit list of unhandled WIZARD/WIZARD2/WIZARD3 messages that logs once per session, not per packet
- src/test/server/game/Handlers/WizardHandlerTest.cpp, and src/test/client/GameMessageTableClientTest.cpp for the checks that read the user's own XML

**Client messages:** MSG_LOGCLIENTRESOLUTION, MSG_LOGPATCHCLIENTPATCHTIME, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_GETSUBSCRIBERONLYITEMS, MSG_SUBSCRIBERONLYITEMS, MSG_CROWNBALANCE, MSG_DONESHOPPING, MSG_PLAYERWIZBANG, MSG_QUESTFINDEROPTION, MSG_SHOWCLIENTMESSAGEBOX, MSG_SHOWGUI

**Data sources**

- Root.wad WizardMessages.xml, WizardMessages2.xml, WizardMessages3.xml (read at build/run time by the message generator, not committed)

**Acceptance**

- [x] Unit test: every name from all three Wizard XML files maps to exactly one entry, either a handler or the unhandled list; MSG_PETHATCHREADYSTATUS maps to one order. `GameMessageTableClientTest.EveryWorldMessageHasExactlyOneRuleAndTheEntryChatterIsHandled` reads the three files and GameMessages.xml from the r806919 install, and MSG_PETHATCHREADYSTATUS is one message at order 122 with its two records
- [x] Unit test: WIZARD message order is the 1-based index in the de-duplicated alphabetical list, with UPDATEMANA=233 and ADDSPELLTOBOOK=10 as fixtures. `GameMessageTableClientTest.WizardOrdersAreThePlacesOfTheirTagsSortedWithoutRepeats` checks every order in WizardMessages.xml against its tag's place and pins both
- [x] Real client: log in to a zone. The server log shows GETTIMEDACCESSPASSES, GETSUBSCRIBERONLYITEMS, LOGCLIENTRESOLUTION and DONESHOPPING handled with no warnings. The client driver's enter-world run 20260925-114135 waits for each of the four handler lines and passes its checks that no server line outside the allow-list is WARN or worse and that no message was dropped or refused

**Risks**

- The duplicate PETHATCHREADYSTATUS element changes every order after position 122. Only UPDATEMANA was checked against a live capture, so check more high-order messages (for example UPDATEGOLD=231) against a real client.
- The real response formats for TIMEDACCESSPASSES and SUBSCRIBERONLYITEMS are unknown. An empty Data string is untested. **Resolved on 2026-09-25** from the client's own handlers, read in Ghidra: WizardGraphicalClient::MSG_TimedAccessPasses loads Data into an ActiveTimedAccessPassList and the subscriber handler into a SubscriberOnlyItemsList, each with a SerializerBinary whose flags read no flags word and no envelope, so the replies are those classes empty and raw; an empty string would load nothing, leaving the pass window as it was. The crown handlers read only Failure and TotalCrowns, and the client sets CacheBalanceForCSSegmentation only on its own requests.

## 4.13 Attach handler server side (WLD-7 part 1)

**Goal:** Validate, resolve zone, join Map.

**Size:** M. **Depends on:** 4.07, 4.10, 4.04

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [x] Wrong LoginKey sends MSG_ATTACHFAILED, never LOGINCOMPLETE. `HandoffTest.AnAttachCarryingAKeyNobodyIssuedIsRefusedAndTheSocketIsClosed` attaches with a key the login server never issued: the game server answers MSG_ATTACHFAILED, nothing follows it for two seconds, the connection closes and the session names no account
- [x] Another account's CharID rejected. `HandoffTest.AnAttachNamingAnotherAccountsWizardIsRefusedEvenWithAGoodKey` signs in, takes the key issued for its own wizard and attaches naming another account's wizard: MSG_ATTACHFAILED, nothing after it, the connection closed, and the other account's wizard never taken on
- [x] A socket that never attaches closes after Attach.Timeout. `GameAttachTest.ASocketThatNeverAttachesIsClosedOnceAttachTimeoutPasses` sets Attach.Timeout to one second: the session is still open at once, the world's update closes it once the second has passed, and the client reads nothing before the close, while `HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo` shows a session that has attached is never closed for it

### Detailed spec from WLD-7: Attach and login complete: standing in an empty zone

A real client that selects a character loads into its saved zone and stands at the right spot, with no other objects yet.

**Deliverables**

- src/server/game/Handlers/AttachHandler.cpp: Session::HandleAttach (state never -> logged in): check LoginKey/UserID against the login session issued at MSG_CHARACTERSELECTED, confirm CharID belongs to the account, resolve ZoneName and Location (named or compact), join a Map, allocate a mobile id, send MSG_LOGINCOMPLETE
- MSG_ATTACHFAILED on a bad key (Rejected=1), wrong character, or zone resolve failure; session closed after Attach.Timeout without MSG_ATTACH
- characters DB: character position columns (zone path, x, y, z, yaw) with a default start zone from Player.StartZone
- ScriptMgr hooks: PlayerScript::OnLogin, ZoneScript::OnPlayerEnter
- conf/dist/gameserver.conf.dist: Attach.Timeout, Player.StartZone, Player.StartLocation, Realm.Name, which become live settings once 4.16 lands; each applies from the next attach

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Data sources**

- LoginMessages.xml MSG_CHARACTERSELECTED (IP, TCPPort, Key, UserID, CharID, ZoneID, ZoneName, Location) supplies what MSG_ATTACH echoes back

**Database tables**

- characters.characters (zone, position_x/y/z, orientation)
- login.account_session (owned by LOG)

**Acceptance**

- [x] Real client: log in, pick a character, see the loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'. The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 seeded a wizard in WizardCity/WC_Hub with no position of its own: the game server logged `put wizard 1 in WizardCity/WC_Hub instance 1 at (-3.267008, 50.24604, -30.47341)`, which is WC_Hub's Start in the zone rows, the client loaded the zone and said so, and the scenario's hold_key step held W for 1.5 seconds with `moves`: 0.237 of the view stayed the same while it was held, against 0.996 over the same time with no key, and the screenshot after it shows the wizard walked off the plaza onto the lawn
- [x] Real client: a character whose saved zone is WizardCity/WC_Ravenwood loads into Ravenwood. Client driver runs 20260925-100208 and 20260925-100536: the enter-world scenario's wizard, saved in WizardCity/WC_Ravenwood, is placed at the zone's Start, the client accepts MSG_LOGINCOMPLETE and loads Ravenwood
- [x] Unit: HandleAttach with a wrong LoginKey sends MSG_ATTACHFAILED and never MSG_LOGINCOMPLETE. `HandoffTest.AnAttachCarryingAKeyNobodyIssuedIsRefusedAndTheSocketIsClosed` attaches with a key the login server never issued: the game server answers MSG_ATTACHFAILED, nothing follows it for two seconds, the connection closes and the session names no account
- [x] Unit: HandleAttach with another account's CharID is rejected. `HandoffTest.AnAttachNamingAnotherAccountsWizardIsRefusedEvenWithAGoodKey` signs in, takes the key issued for its own wizard and attaches naming another account's wizard: MSG_ATTACHFAILED, nothing after it, the connection closed, and the other account's wizard never taken on
- [x] Integration: a socket that never sends MSG_ATTACH is closed after the timeout. `GameAttachTest.ASocketThatNeverAttachesIsClosedOnceAttachTimeoutPasses` sets Attach.Timeout to one second: the session is still open at once, the world's update closes it once the second has passed, and the client reads nothing before the close, while `HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo` shows a session that has attached is never closed for it
- [x] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, DynamicServerProcID, ServerTime (unix seconds), RealmName and CriticalObjects (empty, which the client takes as no critical objects). The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 logged `Session 1 sent MSG_LOGINCOMPLETE: zone WizardCity/WC_Hub, id 1727411499, dynamic zone 1 in process 1, server time 1790354697, realm Ambrose Driver, permissions 0x2f, CSR 0, test server 0, critical objects none`: the server time is the run's own second, 2026-09-25 16:44:57 UTC, the id is the KI string hash of the zone's path, which the client sent back in MSG_CLIENTZONED, and the client left its loading screen for the Commons

**Risks**

- The meaning of LOGINCOMPLETE Permissions, IsCSR, TestServer and SegmentedMessage/LastSegment is unverified; the reference sends Permissions=0b11001111. Very large player objects may need segmentation.
- The player WizClientObject contents (behaviors, stats, equipment) belong to WIZ. Until WIZ lands, a minimal player object may render incompletely, so check the client does not crash on missing behaviors.
- The capture shows MSG_SENDQUEST/MSG_SENDGOAL before LOGINCOMPLETE and MSG_UPDATEMANA and MARK_LOCATION_RESPONSE after it. Ordering needs beyond 'LOGINCOMPLETE first' are unverified.

## 4.14 LOGINCOMPLETE and standing in zone (WLD-7 part 2 + WLD-8 CLIENTZONED)

**Goal:** Real client controls its wizard.

**Size:** M. **Depends on:** 4.13, 4.11, 4.12

**Client messages:** MSG_LOGINCOMPLETE, MSG_CLIENTZONED

**Acceptance**

- [x] Real client: log in, pick a character, loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'. The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 seeded a wizard in WizardCity/WC_Hub with no position of its own: the game server logged `put wizard 1 in WizardCity/WC_Hub instance 1 at (-3.267008, 50.24604, -30.47341)`, which is WC_Hub's Start in the zone rows, the client loaded the zone and said so, and the scenario's hold_key step held W for 1.5 seconds with `moves`: 0.237 of the view stayed the same while it was held, against 0.996 over the same time with no key, and the screenshot after it shows the wizard walked off the plaza onto the lawn
- [x] Saved zone WC_Ravenwood loads Ravenwood. Client driver runs 20260925-100208 and 20260925-100536: the enter-world scenario's wizard, saved in WizardCity/WC_Ravenwood, is placed at the zone's Start, the client accepts MSG_LOGINCOMPLETE and loads Ravenwood
- [x] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, ServerTime, RealmName and an empty CriticalObjects. The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 logged `Session 1 sent MSG_LOGINCOMPLETE: zone WizardCity/WC_Hub, id 1727411499, dynamic zone 1 in process 1, server time 1790354697, realm Ambrose Driver, permissions 0x2f, CSR 0, test server 0, critical objects none`: the server time is the run's own second, 2026-09-25 16:44:57 UTC, the id is the KI string hash of the zone's path, which the client sent back in MSG_CLIENTZONED, and the client left its loading screen for the Commons
- [x] MSG_CLIENTZONED (53:64) marks the session in world. The same runs: the client sends it with ZoneNameID 699201167, the KI string hash of WizardCity/WC_Ravenwood, and the game server logs the wizard standing in the world

### Detailed spec from WLD-7: Attach and login complete: standing in an empty zone

A real client that selects a character loads into its saved zone and stands at the right spot, with no other objects yet.

**Deliverables**

- src/server/game/Handlers/AttachHandler.cpp: Session::HandleAttach (state never -> logged in): check LoginKey/UserID against the login session issued at MSG_CHARACTERSELECTED, confirm CharID belongs to the account, resolve ZoneName and Location (named or compact), join a Map, allocate a mobile id, send MSG_LOGINCOMPLETE
- MSG_ATTACHFAILED on a bad key (Rejected=1), wrong character, or zone resolve failure; session closed after Attach.Timeout without MSG_ATTACH
- characters DB: character position columns (zone path, x, y, z, yaw) with a default start zone from Player.StartZone
- ScriptMgr hooks: PlayerScript::OnLogin, ZoneScript::OnPlayerEnter
- conf/dist/gameserver.conf.dist: Attach.Timeout, Player.StartZone, Player.StartLocation, Realm.Name, which become live settings once 4.16 lands; each applies from the next attach

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Data sources**

- LoginMessages.xml MSG_CHARACTERSELECTED (IP, TCPPort, Key, UserID, CharID, ZoneID, ZoneName, Location) supplies what MSG_ATTACH echoes back

**Database tables**

- characters.characters (zone, position_x/y/z, orientation)
- login.account_session (owned by LOG)

**Acceptance**

- [x] Real client: log in, pick a character, see the loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'. The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 seeded a wizard in WizardCity/WC_Hub with no position of its own: the game server logged `put wizard 1 in WizardCity/WC_Hub instance 1 at (-3.267008, 50.24604, -30.47341)`, which is WC_Hub's Start in the zone rows, the client loaded the zone and said so, and the scenario's hold_key step held W for 1.5 seconds with `moves`: 0.237 of the view stayed the same while it was held, against 0.996 over the same time with no key, and the screenshot after it shows the wizard walked off the plaza onto the lawn
- [x] Real client: a character whose saved zone is WizardCity/WC_Ravenwood loads into Ravenwood. Client driver runs 20260925-100208 and 20260925-100536: the enter-world scenario's wizard, saved in WizardCity/WC_Ravenwood, is placed at the zone's Start, the client accepts MSG_LOGINCOMPLETE and loads Ravenwood
- [x] Unit: HandleAttach with a wrong LoginKey sends MSG_ATTACHFAILED and never MSG_LOGINCOMPLETE. `HandoffTest.AnAttachCarryingAKeyNobodyIssuedIsRefusedAndTheSocketIsClosed` attaches with a key the login server never issued: the game server answers MSG_ATTACHFAILED, nothing follows it for two seconds, the connection closes and the session names no account
- [x] Unit: HandleAttach with another account's CharID is rejected. `HandoffTest.AnAttachNamingAnotherAccountsWizardIsRefusedEvenWithAGoodKey` signs in, takes the key issued for its own wizard and attaches naming another account's wizard: MSG_ATTACHFAILED, nothing after it, the connection closed, and the other account's wizard never taken on
- [x] Integration: a socket that never sends MSG_ATTACH is closed after the timeout. `GameAttachTest.ASocketThatNeverAttachesIsClosedOnceAttachTimeoutPasses` sets Attach.Timeout to one second: the session is still open at once, the world's update closes it once the second has passed, and the client reads nothing before the close, while `HandoffTest.AClientSignsInPicksAWizardLeavesAndAttachesToTheGameServerItWasSentTo` shows a session that has attached is never closed for it
- [x] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, DynamicServerProcID, ServerTime (unix seconds), RealmName and CriticalObjects (empty, which the client takes as no critical objects). The client driver's enter-the-commons run 20260925-124322 on 2026-09-25 logged `Session 1 sent MSG_LOGINCOMPLETE: zone WizardCity/WC_Hub, id 1727411499, dynamic zone 1 in process 1, server time 1790354697, realm Ambrose Driver, permissions 0x2f, CSR 0, test server 0, critical objects none`: the server time is the run's own second, 2026-09-25 16:44:57 UTC, the id is the KI string hash of the zone's path, which the client sent back in MSG_CLIENTZONED, and the client left its loading screen for the Commons

**Risks**

- The meaning of LOGINCOMPLETE Permissions, IsCSR, TestServer and SegmentedMessage/LastSegment is unverified; the reference sends Permissions=0b11001111. Very large player objects may need segmentation.
- The player WizClientObject contents (behaviors, stats, equipment) belong to WIZ. Until WIZ lands, a minimal player object may render incompletely, so check the client does not crash on missing behaviors.
- The capture shows MSG_SENDQUEST/MSG_SENDGOAL before LOGINCOMPLETE and MSG_UPDATEMANA and MARK_LOCATION_RESPONSE after it. Ordering needs beyond 'LOGINCOMPLETE first' are unverified.

### Detailed spec from WLD-8: MSG_CLIENTZONED

Only this part of WLD-8 is 4.14's; its static zone objects are 5.02's, with their deliverables, checks and risks.

**Deliverables**

- MSG_CLIENTZONED (service 53) handler marks the session in world, and object streaming waits for or follows it as the capture shows

**Client messages:** MSG_CLIENTZONED

**Acceptance**

- [x] MSG_CLIENTZONED (53:64) marks the session in world. Client driver runs 20260925-100208, 20260925-100536 and 20260925-124322: the client sends it with the KI string hash of the zone's path, 699201167 for WizardCity/WC_Ravenwood, and the game server logs the wizard standing in the world

## 4.15 Reload framework and reload commands

**Goal:** Every loaded store reloads live through one pattern: build off to the side, validate, swap atomically, keep the old store on failure, and report every error.

**Size:** M. **Depends on:** 4.01, 4.02, 1.10

**Acceptance**

- [x] A failed reload keeps the previous generation serving and returns every error
- [x] A reader holding a snapshot during a swap keeps a consistent view (TSan clean)
- [x] `Logger.network` edit plus `reload config` changes routing without a restart
- [x] Broken message XML on reload keeps the old generation
- [x] `.reload all` reports each target's result and generation
- [x] A live world edit is journaled and exports as a pending SQL update

### Detailed spec

Stores that load at startup share one reload path, so every later manager becomes reloadable by registering a target instead of writing its own swap logic.

**Deliverables**

- src/server/shared/Reload/ReloadableStore.h: ReloadableStore<T> holding an immutable snapshot behind a shared_ptr; readers take a snapshot, and a reload builds a new T, validates it, and swaps the pointer atomically
- src/server/shared/Reload/ReloadMgr.{h,cpp} (sReloadMgr): named targets with dependencies (a target reloads after the targets it depends on), a generation counter, the last result, and every error from the last attempt
- ConfigMgr change notification: subscribers receive the changed keys after a successful reload, and logging subscribes so appenders and logger levels apply at once
- The message registry and configuration register as the targets `messages` and `config`
- src/server/scripts/Commands/cs_reload.cpp: `.reload config`, `.reload messages`, `.reload <target>` and `.reload all`, at ADMINISTRATOR level
- Console `reload <target>` on every app, and SIGHUP on Linux, which runs `reload config`
- src/server/shared/Reload/WorldEditJournal.{h,cpp}: every live world-database edit from a GM command or the admin API is journaled with time, account, source and statement, and `.journal export` writes the journal as a pending update file in data/sql/updates/pending_db_world/. Built with src/server/database/Database/WorldEdits.{h,cpp}, the one path such an edit takes, which journals an edit only once the world database has taken it; the export goes to the pending folder the updater reads, under Updates.SourcePath or the folder the build came from, and a line break in who made an edit or where it came from cannot leave its comment line
- src/test/server/shared/Reload/ReloadableStoreTest.cpp, ReloadMgrTest.cpp, WorldEditJournalTest.cpp

**Acceptance**

- [x] Unit: a reload that fails validation leaves the previous generation serving, keeps its generation number, and returns every error, not only the first
- [x] Unit: reader threads holding a snapshot during repeated swaps always see one whole generation, and the test is clean under TSan
- [x] Integration: editing `Logger.network` in the `.conf` file and running `reload config` on the console changes log routing without a restart
- [x] Integration: reloading message XML with a broken definition keeps the old generation, and declared messages still encode. `MessageReloadTest.ABrokenDefinitionKeepsTheServingGenerationAndAMendedOneReplacesIt` runs an app that loads the definitions from a Root.wad the test builds and names it as its install: a definition broken on disk refuses `reload messages` with the error naming it, the generation that was serving stays, MSG_SERVERMESSAGE still encodes to the same bytes, and the mended file takes a new generation. It found that no app had a messages target at all, because an app names its install only once it has started, after the targets were registered, so `SetMessageSource` now registers it
- [x] `.reload all` reports each target's result and generation, in dependency order
- [x] Integration: a live world-database edit writes one journal entry, and `.journal export` writes a pending_db_world file that applies cleanly to a fresh world database. `WorldEditsTest.AnEditTheDatabaseTakesIsJournaledOnceAndItsExportAppliesToAFreshWorldDatabase` against MariaDB: an edit through `WorldEdits::Apply` changes the live world database at once and writes one journal entry naming who made it and where it came in, and the file the journal exports applies cleanly to a fresh world database the updater builds, which then holds the edit; `WorldEditsTest.AnEditTheDatabaseRefusesOrOneWithNothingInItIsNeverJournaled` keeps refused and empty edits out. `.journal export` writes into the pending folder the updater reads (`DatabaseLoaderTest.PendingUpdatesLiveUnderTheUpdatersSourceFolder`) rather than under the server's working directory

**Risks**

- A snapshot held for a long operation keeps the old generation's memory alive. `.reload all` should list retired generations still held.
- Stores that hold references into each other must reload together or be rebound, or a swap can leave one pointing at a retired generation. Declare those as target dependencies.

## 4.16 Live settings registry

**Goal:** Every tunable value is a typed setting with a default, bounds, and an apply mode, changeable live, persisted, and audited.

**Size:** M. **Depends on:** 4.15, 2.08, 2.13

**Acceptance**

- [x] Out-of-bounds or wrong-type value refused; nothing persisted
- [x] A set value survives a restart; `reset` returns to the config value
- [x] Exactly one change event per successful set
- [x] Every change writes one audit row with old, new, who and why
- [x] Editing an environment-locked key names the locking layer
- [x] `.settings set World.UpdateInterval 100` changes the measured tick within two ticks

### Detailed spec

Gameplay values and runtime options live in one typed registry, so the console, GM commands, configuration reloads and later the control center all change them the same way.

**Deliverables**

- src/server/shared/Settings/Settings.{h,cpp} (sSettings): declarations with key, type, default, min, max, unit, category, description and apply mode (live, next connection or operation, or restart-required with a reason); Get<T> reads an atomic snapshot
- Resolution order: declared default, the `.conf` layers, the persisted live value, then `AMBROSE_` environment variables and command-line overrides, which lock the key
- data/sql/updates/db_characters and db_login: settings (key, value, updated_by, updated_at) and setting_audit (id, key, old_value, new_value, source, account_id, reason, created); the gameserver uses characters, and the loginserver and patchserver use login
- Change events delivered on the world thread, with the changed key, old value and new value
- A config reload through 4.15 re-resolves every setting and raises change events only for keys whose effective value changed
- src/server/scripts/Commands/cs_settings.cpp: `.settings list [category]`, `.settings get <key>`, `.settings set <key> <value> [reason]`, `.settings reset <key>`, `.settings history <key>`
- doc/config/settings.md generated from the declarations, with default, bounds, unit and apply mode per key
- Built as a live layer inside ConfigMgr, between the config files and the environment, which the registry fills with the persisted values that pass their checks, so every existing reader of an option and every hook that reapplies options on a configuration change sees a live value too; the game and login servers reapply the command prefix, command logging, default locale, session limits, realm heartbeat, login settings and realm refresh from `sSettings.Subscribe`, at the top of each tick. The start zone is not a setting: a new wizard's start comes from the playercreateinfo rows milestone 3.15 reads, so there is no Player.StartZone or Player.StartLocation to declare
- First settings: Rate.XP.*, Rate.Gold.*, Rate.Drop.*, Rate.Respawn, plus the live-capable options from phases 1-4 (World.UpdateInterval, Zone.UnloadDelay, Zone.MobileIdReleaseDelay, Realm.HeartbeatInterval, Realm.DefaultRealm, Realm.OfflineAfterIntervals, Login.KeyTTL, Network.HandoffGrace, PublicAddress, Attach.Timeout, Player.StartZone, Player.StartLocation, Realm.Name, GM.CommandPrefix, GM.LogCommands and the phase 1-3 options)
- src/test/server/shared/Settings/SettingsTest.cpp

**Database tables**

- characters.settings
- characters.setting_audit
- login.settings
- login.setting_audit

**Acceptance**

- [x] Unit: an out-of-bounds or wrong-type value is refused with a message naming the bound or type, and nothing is persisted or audited. `SettingsTest.AWrongTypeOrOutOfBoundsValueIsRefusedNamingItAndNothingIsPersistedAuditedOrAnnounced`: `fast` is refused as not "a whole number of zero or more from 1 to 10000 ms", `20000` as outside "from 1 to 10000 ms", and a nine-byte command prefix as over its eight bytes, with the store never written and nothing announced
- [x] Integration: a set value survives a restart, and `.settings reset` returns the key to its config value. `SettingsStoreTest.ASetValueSurvivesARestartAndAResetReturnsToTheConfigValue` against MariaDB: World.UpdateInterval set to 100 over the characters database reads 100 in a registry started afresh, the reset returns it to the file's 70, and a third start reads 70; `SettingsStoreTest.TheLoginDatabaseKeepsTheLoginServersSettings` does the same for Login.KeyTTL over the login database
- [x] Unit: exactly one change event fires per successful set, and none for a refused one. `SettingsTest.ExactlyOneChangeIsAnnouncedPerSuccessfulSetAndNoneForARefusedOrUnchangedOne`: one set queues one change from 50 to 100, and setting the same value again or one out of bounds queues none and writes nothing
- [x] Integration: every change writes one setting_audit row with old and new values, who made it, the source and the reason. `SettingsStoreTest.EveryChangeWritesOneAuditRowWithTheValuesWhoWhereAndWhy` against MariaDB: a set and a reset write two rows, newest first, each with the value before and after, the name and account of whoever made it, where it came in and why, and the refused set between them writes none
- [x] Unit: `.settings set` on a key set by an environment variable is refused with a message naming that layer. `SettingsTest.AKeyAnEnvironmentVariableOrOverrideSetsIsLockedAndTheRefusalNamesThatLayer`: a set or reset of World.UpdateInterval while AMBROSE_WORLD_UPDATE_INTERVAL is set names that variable, and one of Zone.UnloadDelay under a command-line override names the override
- [x] Integration: `.settings set World.UpdateInterval 100` changes the measured tick within two ticks, with no restart. `SettingsTickTest.SettingTheUpdateIntervalOnTheConsoleChangesTheMeasuredTickWithinTwoTicks` runs an app ticking at 20 ms whose tick is World.UpdateInterval, types `settings set World.UpdateInterval 100` on its console and measures at least 90 ms between the first and second ticks after it

**Risks**

- A persisted value can fall outside bounds changed by a newer binary. Refuse it at startup, log it, and fall back to the config value instead of refusing to start.
- A subscriber that caches a value at startup silently ignores live changes. Declared keys must be read through Get<T> or a change subscription, never copied into a member at startup.

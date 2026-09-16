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
- **Ordering.** 4.11/4.14 build the player's CoreObject for LOGINCOMPLETE before the template store (5.01). The player object is template 1 (ObjectData/PlayerObject.xml per 3.11) and carries behaviors. Either 4.14 needs 5.01 or a stub template path, or the plan should state that behaviors are hand-built until 5.01.
- **Missing work.** LOGINCOMPLETE segmentation: MSG_LOGINCOMPLETE carries SegmentedMessage and LastSegment UBYT fields, plus DynamicServerProcID, Permissions, IsCSR, ZoneServer, HourOffset, PickUpAllEnabled and others (GameMessages.xml 5:108). 4.11/4.14 mention none of the segmentation semantics.
- **Missing work.** MSG_ATTACH also carries PassKey, MachineID, Reattach, Retry, SessionID/SessionSlot and TargetPlayerID (5:7). 4.07/4.13 validate only LoginKey. Reattach/Retry semantics need an owner earlier than 6.08.
- **Missing work.** Multi-realm inter-process communication: friends presence, cross-realm whispers, party member zones, realm-transfer handoff, and kicking a character online on another realm all need a login<->game or game<->game bus (or DB polling). Only heartbeat rows (4.03) and login keys exist.
- **Oversized.** 4.14 LOGINCOMPLETE and standing in zone (M). This is the first full CoreObject acceptance by the real client, with segmentation, CriticalObjects and CLIENTZONED. It is historically the hardest single step and should be split: byte-level LOGINCOMPLETE against a decoded capture, then real-client zone-in.
- **Oversized.** 4.08 zone extractor across 3356 zone WADs with 0 failures (M). The failure triage alone is open-ended.
- **Ordering.** 4.09 depends on 4.15, so 4.15 lands before 4.09. Settings named in 4.02-4.14 read their config value until 4.16 lands, then become live settings with the same keys.

## 4.01 sWorld tick, GameSession, ScriptMgr hooks and AddSC loaders (new core)

**Goal:** Game update loop and the hook framework every later domain uses.

**Size:** M. **Depends on:** 2.09, 2.08

**Acceptance**

- [ ] A WorldScript OnUpdate registered through AddSC_ runs every tick
- [ ] A module in modules/ is discovered by CMake with no core edits
- [ ] GameSession queue drains on the world thread (thread-id test)

## 4.02 CommandMgr and security levels, console first (new; WIZ-5 core)

**Goal:** CommandScript tables with account_access levels, from the console.

**Size:** M. **Depends on:** 4.01, 2.13

**Acceptance**

- [ ] PLAYER level running a GAMEMASTER command gets 'no such command'
- [ ] '.character gold 500' parses into (character, gold, [500])
- [ ] Console `server info` works

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

- [ ] Unit test: a PLAYER-level account running a GAMEMASTER command gets 'no such command' and nothing executes
- [ ] Unit test: the command table parses '.character gold 500' into (character, gold, [500])
- [ ] Unit test: a command_security row raising a command to ADMINISTRATOR refuses a GAMEMASTER account
- [ ] Real client, GM account: typing '.help' shows the command list in the chat window, and nearby players see no bubble. The same text from a player account shows up as normal chat or is refused, depending on config.

**Risks**

- Whether the stock client ever sends GAME MSG_COMMAND itself is unverified. The capture only shows '.mod ...' arriving as REQUESTRADIALCHAT.
- The bit meanings of LOGINCOMPLETE Permissions are unverified. The reference sends a constant 207 (0b11001111).

## 4.03 Realm registry and heartbeat (LOG-10)

**Goal:** Loginserver knows live realms.

**Size:** M. **Depends on:** 2.13, 4.01

**Client messages:** MSG_REQUESTSERVERLIST, MSG_SERVERLIST

**Acceptance**

- [ ] A realm with last_heartbeat older than Realm.OfflineAfterIntervals (default 3) is excluded; policy picks the named, else least-full realm
- [ ] Starting a gameserver refreshes its heartbeat; stopping it goes offline

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

- [ ] Unit: a realm whose last_heartbeat is older than Realm.OfflineAfterIntervals heartbeat intervals (default 3) is excluded; the selection policy picks the named realm when it is online, otherwise the least-full one, and returns none when every realm is offline
- [ ] Integration: starting one gameserver makes its realmlist row show a fresh heartbeat within 1 interval, and stopping it makes the loginserver treat it as offline

**Risks**

- There is no gameserver yet; the heartbeat half depends on the WLD/FND gameserver app skeleton.
- Unlike AzerothCore, every realm shares one db_characters. The realm_id of a character's last login must not be used to partition data.

## 4.04 World wire math and LocationString (WLD-1 + LOG-11 LocationString)

**Goal:** Position/direction packing and 'x,y,z,yaw'.

**Size:** S. **Depends on:** 1.14

**Client messages:** MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_SERVERTELEPORT

**Acceptance**

- [ ] Packing (-2408.09, 2609.10, -7.13) is within 4 units
- [ ] '-32,-552,-28,6.350083' formats and parses exactly; '857.9,5730.8,-18.09,1.40' parses under fr-FR; 'Start' is named
- [ ] GAME ordinals ADDEFFECT=2, ATTACH=7, CLIENTMOVE=36, ENTERSTATE=72, LOGINCOMPLETE=108, NEWOBJECT=122, SERVERMOVE=218, WIZBANG=247

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

- [ ] Unit: packing (x=-2408.09,y=2609.10,z=-7.13) and unpacking lands within 4 units per axis; values below -32768*4 are rejected or clamped by a documented rule
- [ ] Unit: yaw 0, pi/2, pi and 3pi/2 survive a byte round-trip within 1 byte step
- [ ] Unit: '857.9,5730.8,-18.09,1.40' parses to 4 floats under a fr-FR process locale; 'Start' parses as a named location
- [ ] Unit: GAME ordinals computed by sorting the XML element tag names (not _MsgName) ordinally, with the duplicate MSG_REMOVEOBJECT collapsed, give ADDEFFECT=2, ATTACH=7, CLIENTMOVE=36, ENTERSTATE=72, LOGINCOMPLETE=108, NEWOBJECT=122, SERVERMOVE=218, WIZBANG=247, matching the live capture in a local session capture

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

- [ ] Unit: LocationString formats (-32,-552,-28, yaw 6.350083) as '-32,-552,-28,6.350083', the exact string in the capture, and parses it back
- [ ] Unit: selecting another account's CharID, a deleted character or with no realm online gives Error!=0 and no login_key row
- [ ] Integration: a stub TCP listener on the realm port receives a connection and a GAME MSG_ATTACH whose LoginKey == Key, UserID and CharID match, and ZoneName and Location echo the CHARACTERSELECTED values (the behavior at capture lines 11-12)
- [ ] Real client: after clicking Play, the loading screen appears and the client connects to the gameserver port (visible in the gameserver log) instead of showing a disconnect dialog

**Risks**

- What ZoneID in CHARACTERSELECTED means is unverified (the reference fills it with the gameserver port); WLD decides.
- The sniffer's same-length address rewrite needs a dotted IPv4 string; a hostname in IP is untested.

## 4.05 Character select and login key (LOG-11)

**Goal:** CHARACTERSELECTED points the client at the right gameserver.

**Size:** M. **Depends on:** 3.09, 4.03, 4.04

**Client messages:** MSG_SELECTCHARACTER, MSG_CHARACTERSELECTED

**Acceptance**

- [ ] Another account's CharID, deleted character or no realm gives Error!=0 and no login_key
- [ ] Stub listener receives MSG_ATTACH whose LoginKey == Key with matching UserID/CharID

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

- [ ] Unit: LocationString formats (-32,-552,-28, yaw 6.350083) as '-32,-552,-28,6.350083', the exact string in the capture, and parses it back
- [ ] Unit: selecting another account's CharID, a deleted character or with no realm online gives Error!=0 and no login_key row
- [ ] Integration: a stub TCP listener on the realm port receives a connection and a GAME MSG_ATTACH whose LoginKey == Key, UserID and CharID match, and ZoneName and Location echo the CHARACTERSELECTED values (the behavior at capture lines 11-12)
- [ ] Real client: after clicking Play, the loading screen appears and the client connects to the gameserver port (visible in the gameserver log) instead of showing a disconnect dialog

**Risks**

- What ZoneID in CHARACTERSELECTED means is unverified (the reference fills it with the gameserver port); WLD decides.
- The sniffer's same-length address rewrite needs a dotted IPv4 string; a hostname in IP is untested.

## 4.06 Login-to-game handoff transport (NET-13)

**Goal:** Client reconnects to gameserver without timeouts.

**Size:** S. **Depends on:** 4.05, 2.10

**Client messages:** MSG_CHARACTERSELECTED, MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [ ] Integration: login handshake, CHARACTERSELECTED, disconnect, game handshake, MSG_ATTACH in STATUS_CONNECTED
- [ ] Real client: no 'connection lost' at character select; new game session id logged

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

- [ ] Integration test: a fake client completes login handshake -> CHARACTERSELECTED -> disconnect -> game handshake -> MSG_ATTACH is dispatched in STATUS_CONNECTED
- [ ] Real client: after picking a character, the server log shows the login socket closed by the client, a new game session id offered and accepted, and MSG_ATTACH received. The client moves past character select to its loading screen, with no 'connection lost' dialog
- [ ] A MSG_ATTACH with a bad key produces MSG_ATTACHFAILED and the client returns to an error or the login screen

**Risks**

- The UDPPort field suggests a UDP channel; Imlight never opens one and the client still works, but that is unverified for every feature
- Key semantics belong to LOG; NET only transports them

## 4.07 Gameserver login key validation (LOG-12)

**Goal:** Single-use key checked on attach.

**Size:** S. **Depends on:** 4.06

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [ ] A valid key passes once; replayed, expired, other CharID and other realm keys fail
- [ ] Random LoginKey gets MSG_ATTACHFAILED and close

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

- [ ] Unit: a valid key passes once; replaying the same key, an expired key, a key for another CharID and a key for another realm each fail
- [ ] Real client: select a character and the gameserver log shows the key accepted, after which WLD's LOGINCOMPLETE flow runs
- [ ] Negative: a hand-crafted attach with a random LoginKey (from a test client) gets MSG_ATTACHFAILED, and the socket closes

**Risks**

- The meanings of the Error, Rejected and NoDisconnect values in MSG_ATTACHFAILED are unverified; the reference marks them TODO.
- Split ownership with WLD: the attach handler belongs to WLD, while LOG owns only the validator and the online bookkeeping.

## 4.08 Zone extractor part 1: WizZoneData (WLD-2 + QST-4 zone objects)

**Goal:** zone_template, zone_location, zone_object rows from the install.

**Size:** M. **Depends on:** 3.11, 2.07

**Acceptance**

- [ ] 3356 zone WADs with gamedata.bin, 0 decode failures targeted
- [ ] WC_Hub display key 'WizardZone_TheCommons'; locations include 'Start', 'Target location (WC_Hub Street1 Exit)'
- [ ] WC_Ravenwood yields 97 CoreObjectInfo incl. templates 38232, 38230, 81102, 1451035, 39088
- [ ] Idempotent rerun; git status clean

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

- [ ] Running the extractor over r806919 reports 3356 zone WADs with gamedata.bin and lists each decode failure by name, target 0
- [ ] zone_template row for WizardCity/WC_Hub has display name key 'WizardZone_TheCommons' plus farClip, healingPerMinute, soft/hard limit and noMounts filled
- [ ] zone_location for WC_Hub contains 'Start', 'Target location (WC_Hub Street1 Exit)' and 'Target location(WC_Hub Ravenwood)' with position and direction
- [ ] zone_object for WC_Hub has one row per m_objectList entry with templateID, location, orientation, scale, zoneTag, startState, loadingType and a nullable serialized spawnRequirements column
- [ ] Re-running is idempotent (same row counts), and git status shows no extracted files

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
- [ ] Full run over ~3356 zone WADs finishes, with a per-zone error count of 0 or a listed set of unknown classes.

**Risks**

- Unverified whether EVERY retail NPC placement is in client gamedata.bin. Some quest-gated NPCs may exist only server-side and would need authored rows in a world.custom_zone_object table.
- gamedata.bin decoding is probably owned by WLD. This milestone should reuse it, not write a second decoder.

## 4.09 sZoneMgr and reload (WLD-4)

**Goal:** Zone rows in memory, GM reload.

**Size:** S. **Depends on:** 4.08, 4.02, 4.15

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT

**Acceptance**

- [ ] An unknown location falls back to 'Start'
- [ ] '.zone info WizardCity/WC_Hub' prints counts
- [ ] '.reload zone_location' applies without restart
- [ ] A reload that fails validation keeps the old store and reports every error

### Detailed spec from WLD-4: Zone templates in memory: sZoneMgr and reload

The game server loads zone, location and object rows at startup into a global manager, and GMs can reload them without a restart.

**Deliverables**

- src/server/game/Zones/ZoneMgr.h/.cpp (sZoneMgr): ZoneTemplate, ZoneLocation and ZoneObjectSpawn stores keyed by zone path, location lookup with 'Start' fallback, each a 4.15 reload target that builds off to the side, validates, swaps, and keeps the old store on failure
- src/server/game/World/World.cpp: startup load order and timing log
- src/server/scripts/Commands/cs_zone.cpp: '.zone info <path>', '.reload zone_template', '.reload zone_location', '.reload zone_object'
- '.reload zone_object': live maps spawn rows that were added and despawn rows that were removed once maps (4.10) and object spawning (4.14) exist
- src/test/server/game/Zones/ZoneMgrTest.cpp using an in-memory fixture DB

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT

**Data sources**

- world DB rows from WLD-2

**Database tables**

- world.zone_template
- world.zone_location
- world.zone_object

**Acceptance**

- [ ] Unit: an unknown location name falls back to 'Start'; an unknown zone returns a typed error
- [ ] gameserver startup logs zone template, location and object counts plus load time
- [ ] '.zone info WizardCity/WC_Hub' prints display key, object count and location count in chat or console
- [ ] Editing a zone_location row, then '.reload zone_location', returns the new coordinates without a restart
- [ ] A zone_location row with an unknown zone path makes '.reload zone_location' fail with that row named, and lookups still return the old coordinates

**Risks**

- Holding objects for all 3356 zones in memory may be heavy. Consider loading object spawns lazily per zone on first instance.

## 4.10 Maps, instances, mobile ids, GID service (WLD-5)

**Goal:** Zone instances allocate ids with no client.

**Size:** M. **Depends on:** 4.09, 4.01

**Acceptance**

- [ ] 1000 allocate/release cycles never reuse within delay
- [ ] Exhausted range errors
- [ ] Same zone_object gives same permID across restarts

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

- [ ] Unit: 1000 allocate/release cycles never hand out an id still held or within its release delay
- [ ] Unit: exhausting the player range returns an error, not a crash
- [ ] Unit: two instances of the same zone get different dynamic zone ids; an empty instance is destroyed only after the delay
- [ ] Unit: the delay is read when an instance empties, so a changed Zone.UnloadDelay applies to the next instance that empties with no restart
- [ ] Unit: the same zone_object row gives the same permID across restarts, while runtime GIDs are unique

**Risks**

- The reference frees mobile ids with a ~2s delay because a fast leave-and-rejoin let MSG_NEWOBJECT race MSG_REMOVEOBJECT for a reused id. Keep a delay.
- Threading model (map-per-thread or a single world thread) is a NET/FND decision and affects this API

## 4.11 CoreObject serializer and client-object builder (OBJ-9 + WLD-6)

**Goal:** Bytes for MSG_NEWOBJECT and MSG_LOGINCOMPLETE.

**Size:** M. **Depends on:** 3.05, 3.07

**Client messages:** MSG_NEWOBJECT, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] ClientObject encodes (2,2), WizClientObject (104,2); round-trip equal
- [ ] Public mask omits authority-only properties
- [ ] Local-gated: captured LOGINCOMPLETE Data begins 68 02 01000000 and decodes fully

### Detailed spec from OBJ-9: CoreObject serializer variant

Game objects for MSG_LOGINCOMPLETE and MSG_NEWOBJECT serialize with the block/type/template prefix that the client uses to create them.

**Deliverables**

- src/server/shared/ObjectProperty/CoreObjectSerializer.h/.cpp: the object header is u8 block, u8 type, u32 template id. Block 0 and type 0 mean a plain class hash follows instead
- A block/type table as a config/data table (not code constants): ClientObject 2/2, WizClientObject 104/2, WizClientObjectItem 115/9, WizClientPet 106/2, WizClientMount 108/2, ClientReagentItem 132/9, ClientRecipe 131/131 (from behavior study; each entry to be confirmed)
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
- [ ] Unit: blob wrapper round-trips raw and zlib forms; an unwrapped blob fails the validator
- [ ] Unit: encoding with the Public flag mask leaves out authority-only properties that the AuthorityTransmit mask keeps

**Risks**

- The block/type pairs (ClientObject 2/2, WizClientObject 104/2, WizClientPet 106/2, WizClientMount 108/2, WizClientObjectItem 115/9) come from the behavior reference and must be re-derived clean-room, by capture or client analysis
- Per the user's sniffer README, the client dereferences null on an unwrapped blob and crashes, so the wrapper is mandatory

## 4.12 Wizard service skeleton and login chatter (WIZ-1, without wizbang broadcast)

**Goal:** No unknown-message spam on world entry.

**Size:** S. **Depends on:** 2.09, 4.01

**Client messages:** MSG_LOGCLIENTRESOLUTION, MSG_LOGPATCHCLIENTPATCHTIME, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_GETSUBSCRIBERONLYITEMS, MSG_SUBSCRIBERONLYITEMS, MSG_CROWNBALANCE, MSG_DONESHOPPING, MSG_QUESTFINDEROPTION

**Acceptance**

- [ ] Every name in WizardMessages/2/3 maps to one handler or the unhandled list
- [ ] UPDATEMANA=233 and ADDSPELLTOBOOK=10 fixtures
- [ ] Real client: login chatter handled with no warnings

### Detailed spec from WIZ-1: Wizard service dispatch skeleton and login chatter

A character entering the world gets no errors or unknown-message spam from the WIZARD requests the client sends right after login, and every WIZARD message is either handled or deliberately listed as not yet handled.

**Deliverables**

- src/server/game/Handlers/WizardHandler.cpp: register the handlers in the session dispatch table (state: in world)
- src/server/game/Handlers/WizardHandler.cpp: minimal replies. GETTIMEDACCESSPASSES -> empty TIMEDACCESSPASSES; GETSUBSCRIBERONLYITEMS -> empty SUBSCRIBERONLYITEMS; CROWNBALANCE -> TotalCrowns=0; DONESHOPPING, LOGCLIENTRESOLUTION, LOGPATCHCLIENTPATCHTIME and QUESTFINDEROPTION accepted and logged at debug
- PLAYERWIZBANG handler: broadcast GAME MSG_WIZBANG to the zone (StateName SpellbookWizbang -> a non-zero WizBangID; any other state -> 0)
- src/server/shared/Messages: an explicit list of unhandled WIZARD/WIZARD2/WIZARD3 messages that logs once per session, not per packet
- src/test/server/game/WizardDispatchTest.cpp

**Client messages:** MSG_LOGCLIENTRESOLUTION, MSG_LOGPATCHCLIENTPATCHTIME, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_GETSUBSCRIBERONLYITEMS, MSG_SUBSCRIBERONLYITEMS, MSG_CROWNBALANCE, MSG_DONESHOPPING, MSG_PLAYERWIZBANG, MSG_QUESTFINDEROPTION, MSG_SHOWCLIENTMESSAGEBOX, MSG_SHOWGUI

**Data sources**

- Root.wad WizardMessages.xml, WizardMessages2.xml, WizardMessages3.xml (read at build/run time by the message generator, not committed)

**Acceptance**

- [ ] Unit test: every name from all three Wizard XML files maps to exactly one entry, either a handler or the unhandled list; MSG_PETHATCHREADYSTATUS maps to one order
- [ ] Unit test: WIZARD message order is the 1-based index in the de-duplicated alphabetical list, with UPDATEMANA=233 and ADDSPELLTOBOOK=10 as fixtures
- [ ] Real client: log in to a zone. The server log shows GETTIMEDACCESSPASSES, GETSUBSCRIBERONLYITEMS, LOGCLIENTRESOLUTION and DONESHOPPING handled with no warnings. Opening and closing the spellbook makes other clients in range see the book wizbang appear and clear.

**Risks**

- The duplicate PETHATCHREADYSTATUS element changes every order after position 122. Only UPDATEMANA was checked against a live capture, so check more high-order messages (for example UPDATEGOLD=231) against a real client.
- The real response formats for TIMEDACCESSPASSES and SUBSCRIBERONLYITEMS are unknown. An empty Data string is untested.

## 4.13 Attach handler server side (WLD-7 part 1)

**Goal:** Validate, resolve zone, join Map.

**Size:** M. **Depends on:** 4.07, 4.10, 4.04

**Client messages:** MSG_ATTACH, MSG_ATTACHFAILED

**Acceptance**

- [ ] Wrong LoginKey sends MSG_ATTACHFAILED, never LOGINCOMPLETE
- [ ] Another account's CharID rejected
- [ ] A socket that never attaches closes after Attach.Timeout

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

- [ ] Real client: log in, pick a character, see the loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'
- [ ] Real client: a character whose saved zone is WizardCity/WC_Ravenwood loads into Ravenwood
- [ ] Unit: HandleAttach with a wrong LoginKey sends MSG_ATTACHFAILED and never MSG_LOGINCOMPLETE
- [ ] Unit: HandleAttach with another account's CharID is rejected
- [ ] Integration: a socket that never sends MSG_ATTACH is closed after the timeout
- [ ] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, DynamicServerProcID, ServerTime (unix seconds), RealmName and CriticalObjects (a wrapped, empty CriticalObjectList)

**Risks**

- The meaning of LOGINCOMPLETE Permissions, IsCSR, TestServer and SegmentedMessage/LastSegment is unverified; the reference sends Permissions=0b11001111. Very large player objects may need segmentation.
- The player WizClientObject contents (behaviors, stats, equipment) belong to WIZ. Until WIZ lands, a minimal player object may render incompletely, so check the client does not crash on missing behaviors.
- The capture shows MSG_SENDQUEST/MSG_SENDGOAL before LOGINCOMPLETE and MSG_UPDATEMANA and MARK_LOCATION_RESPONSE after it. Ordering needs beyond 'LOGINCOMPLETE first' are unverified.

## 4.14 LOGINCOMPLETE and standing in zone (WLD-7 part 2 + WLD-8 CLIENTZONED)

**Goal:** Real client controls its wizard.

**Size:** M. **Depends on:** 4.13, 4.11, 4.12

**Client messages:** MSG_LOGINCOMPLETE, MSG_CLIENTZONED

**Acceptance**

- [ ] Real client: log in, pick a character, loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'
- [ ] Saved zone WC_Ravenwood loads Ravenwood
- [ ] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, ServerTime, RealmName and a wrapped empty CriticalObjects
- [ ] MSG_CLIENTZONED (53:64) marks the session in world

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

- [ ] Real client: log in, pick a character, see the loading screen, then control the wizard in WizardCity/WC_Hub at 'Start'
- [ ] Real client: a character whose saved zone is WizardCity/WC_Ravenwood loads into Ravenwood
- [ ] Unit: HandleAttach with a wrong LoginKey sends MSG_ATTACHFAILED and never MSG_LOGINCOMPLETE
- [ ] Unit: HandleAttach with another account's CharID is rejected
- [ ] Integration: a socket that never sends MSG_ATTACH is closed after the timeout
- [ ] LOGINCOMPLETE fills ZoneName, ZoneID, DynamicZoneID, DynamicServerProcID, ServerTime (unix seconds), RealmName and CriticalObjects (a wrapped, empty CriticalObjectList)

**Risks**

- The meaning of LOGINCOMPLETE Permissions, IsCSR, TestServer and SegmentedMessage/LastSegment is unverified; the reference sends Permissions=0b11001111. Very large player objects may need segmentation.
- The player WizClientObject contents (behaviors, stats, equipment) belong to WIZ. Until WIZ lands, a minimal player object may render incompletely, so check the client does not crash on missing behaviors.
- The capture shows MSG_SENDQUEST/MSG_SENDGOAL before LOGINCOMPLETE and MSG_UPDATEMANA and MARK_LOCATION_RESPONSE after it. Ordering needs beyond 'LOGINCOMPLETE first' are unverified.

### Detailed spec from WLD-8: Static zone objects appear

NPCs, signs, doors and props from the zone data appear for a player entering a zone, including objects the loading screen waits for.

**Deliverables**

- src/server/game/Entities/GameObject.cpp: spawn every zone_object row whose template has a RenderBehaviorTemplate when the Map is created, skipping sigil/minigame info classes
- Map::AddPlayer: send MSG_NEWOBJECT for each visible object to the entering player; Map::RemovePlayer: nothing for the leaver (the client tears down)
- Critical objects: templates whose adjective list holds 'Critical' go into LOGINCOMPLETE.CriticalObjects
- MSG_CLIENTZONED (service 53) handler marks the session in world, and object streaming waits for or follows it as the capture shows
- On a successful '.reload zone_object', each live Map spawns objects for added rows and removes objects for deleted rows, sending MSG_NEWOBJECT and MSG_REMOVEOBJECT to players in it
- src/test/server/game/Zones/MapObjectSpawnTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_LOGINCOMPLETE, MSG_CLIENTZONED

**Data sources**

- world.zone_object
- ObjectData templates (DAT-2): GameObjectTemplate.m_behaviors, m_adjectiveList, m_exemptFromAOI

**Database tables**

- world.zone_object

**Acceptance**

- [ ] Real client: in WizardCity/WC_Hub, statues, kiosks and NPC models stand where they do on retail, and nothing floats at 0,0,0
- [ ] Real client: entering a zone with a Critical object leaves the loading screen (it does not hang)
- [ ] Unit: a zone_object with a missing template is logged once and skipped; the Map still loads
- [ ] Unit: after '.reload zone_object' adds one row and deletes another, a live Map holds the new object and not the deleted one, with no restart; a reload that fails validation leaves the Map unchanged
- [ ] Server log: '<n> objects spawned in WizardCity/WC_Hub' matches the count of eligible zone_object rows

**Risks**

- That the client waits on CriticalObjects before dropping the loading screen is inferred from the reference, not confirmed
- Objects with m_spawnRequirements (quest-gated) are shown to everyone until WLD-19

## 4.15 Reload framework and reload commands

**Goal:** Every loaded store reloads live through one pattern: build off to the side, validate, swap atomically, keep the old store on failure, and report every error.

**Size:** M. **Depends on:** 4.01, 4.02, 1.10

**Acceptance**

- [ ] A failed reload keeps the previous generation serving and returns every error
- [ ] A reader holding a snapshot during a swap keeps a consistent view (TSan clean)
- [ ] `Logger.network` edit plus `reload config` changes routing without a restart
- [ ] Broken message XML on reload keeps the old generation
- [ ] `.reload all` reports each target's result and generation
- [ ] A live world edit is journaled and exports as a pending SQL update

### Detailed spec

Stores that load at startup share one reload path, so every later manager becomes reloadable by registering a target instead of writing its own swap logic.

**Deliverables**

- src/server/shared/Reload/ReloadableStore.h: ReloadableStore<T> holding an immutable snapshot behind a shared_ptr; readers take a snapshot, and a reload builds a new T, validates it, and swaps the pointer atomically
- src/server/shared/Reload/ReloadMgr.{h,cpp} (sReloadMgr): named targets with dependencies (a target reloads after the targets it depends on), a generation counter, the last result, and every error from the last attempt
- ConfigMgr change notification: subscribers receive the changed keys after a successful reload, and logging subscribes so appenders and logger levels apply at once
- The message registry and configuration register as the targets `messages` and `config`
- src/server/scripts/Commands/cs_reload.cpp: `.reload config`, `.reload messages`, `.reload <target>` and `.reload all`, at ADMINISTRATOR level
- Console `reload <target>` on every app, and SIGHUP on Linux, which runs `reload config`
- src/server/shared/Reload/WorldEditJournal.{h,cpp}: every live world-database edit from a GM command or the admin API is journaled with time, account, source and statement, and `.journal export` writes the journal as a pending update file in data/sql/updates/pending_db_world/
- src/test/server/shared/Reload/ReloadableStoreTest.cpp, ReloadMgrTest.cpp, WorldEditJournalTest.cpp

**Acceptance**

- [ ] Unit: a reload that fails validation leaves the previous generation serving, keeps its generation number, and returns every error, not only the first
- [ ] Unit: reader threads holding a snapshot during repeated swaps always see one whole generation, and the test is clean under TSan
- [ ] Integration: editing `Logger.network` in the `.conf` file and running `reload config` on the console changes log routing without a restart
- [ ] Integration: reloading message XML with a broken definition keeps the old generation, and declared messages still encode
- [ ] `.reload all` reports each target's result and generation, in dependency order
- [ ] Integration: a live world-database edit writes one journal entry, and `.journal export` writes a pending_db_world file that applies cleanly to a fresh world database

**Risks**

- A snapshot held for a long operation keeps the old generation's memory alive. `.reload all` should list retired generations still held.
- Stores that hold references into each other must reload together or be rebound, or a swap can leave one pointing at a retired generation. Declare those as target dependencies.

## 4.16 Live settings registry

**Goal:** Every tunable value is a typed setting with a default, bounds, and an apply mode, changeable live, persisted, and audited.

**Size:** M. **Depends on:** 4.15, 2.08, 2.13

**Acceptance**

- [ ] Out-of-bounds or wrong-type value refused; nothing persisted
- [ ] A set value survives a restart; `reset` returns to the config value
- [ ] Exactly one change event per successful set
- [ ] Every change writes one audit row with old, new, who and why
- [ ] Editing an environment-locked key names the locking layer
- [ ] `.settings set World.UpdateInterval 100` changes the measured tick within two ticks

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
- First settings: Rate.XP.*, Rate.Gold.*, Rate.Drop.*, Rate.Respawn, plus the live-capable options from phases 1-4 (World.UpdateInterval, Zone.UnloadDelay, Zone.MobileIdReleaseDelay, Realm.HeartbeatInterval, Realm.DefaultRealm, Realm.OfflineAfterIntervals, Login.KeyTTL, Network.HandoffGrace, PublicAddress, Attach.Timeout, Player.StartZone, Player.StartLocation, Realm.Name, GM.CommandPrefix, GM.LogCommands and the phase 1-3 options)
- src/test/server/shared/Settings/SettingsTest.cpp

**Database tables**

- characters.settings
- characters.setting_audit
- login.settings
- login.setting_audit

**Acceptance**

- [ ] Unit: an out-of-bounds or wrong-type value is refused with a message naming the bound or type, and nothing is persisted or audited
- [ ] Integration: a set value survives a restart, and `.settings reset` returns the key to its config value
- [ ] Unit: exactly one change event fires per successful set, and none for a refused one
- [ ] Integration: every change writes one setting_audit row with old and new values, who made it, the source and the reason
- [ ] Unit: `.settings set` on a key set by an environment variable is refused with a message naming that layer
- [ ] Integration: `.settings set World.UpdateInterval 100` changes the measured tick within two ticks, with no restart

**Risks**

- A persisted value can fall outside bounds changed by a newer binary. Refuse it at startup, log it, and fall back to the config value instead of refusing to start.
- A subscriber that caches a value at startup silently ignores live changes. Declared keys must be read through Get<T> or a change subscription, never copied into a member at startup.

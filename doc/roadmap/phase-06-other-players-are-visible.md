<!-- Project Ambrose by Imjustchico: Roadmap phase 6, Other players are visible. -->

# Phase 6: Other players are visible

**Done when:** Two clients in WC_Hub see each other walk, jump, chat and emote, and one sees the other vanish on logout. Walking through the Ravenwood gate transfers zones. GMs teleport, kick and ban.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 6.01 | Whole-zone player broadcast (WLD-10) | M | 5.02, 5.03, 5.05, 4.16 |
| 6.02 | PLAYERWIZBANG broadcast (WIZ-1 remainder) | S | 6.01 |
| 6.03 | Say chat, quick chat, core emotes (WIZ-4) | M | 6.01, 4.02, 4.15, 4.16 |
| 6.04 | GM commands in chat (WIZ-5 remainder) | M | 6.03, 5.01, 4.16 |
| 6.05 | GM account, ban, character commands (LOG-17) | M | 6.04, 3.17, 4.15 |
| 6.06 | Same-zone teleport and cs_tele (WLD-12) | S | 6.01, 4.02, 4.15 |
| 6.07 | Cross-zone transfer via MSG_SERVERTRANSFER (WLD-13) | M | 6.06, 4.13 |
| 6.08 | Logout, link-dead, AFK, shutdown (WLD-20) | M | 6.01, 4.16 |
| 6.09 | Schema probe for classes missing from the dump (OBJ-11) | M | 3.11 |
| 6.10 | Supplemental server-side schemas (OBJ-12) | S | 6.09, 3.03, 4.15 |
| 6.11 | Trigger/volume schemas and zone WAD sweep (WLD-3 part 1 + OBJ-18) | M | 6.10 |
| 6.12 | Volume/trigger extraction to world rows (WLD-3 part 2) | M | 6.11, 4.08 |
| 6.13 | Volumes and walk-in trigger events (WLD-14) | M | 6.12, 5.03, 4.01, 4.15 |
| 6.14 | Zone doors table and walk-in transfers (WLD-15) | M | 6.07, 6.13, 4.15 |
| 6.15 | AOI grid and visibility sets, unit level (WLD-11 part 1) | M | 6.01, 4.16 |
| 6.16 | AOI in the real client (WLD-11 part 2) | M | 6.15 |
| 6.17 | Network hardening (NET-11) | M | 2.09, 4.16 |
| 6.18 | Packet log, diagnostics, network hooks (NET-12) | S | 2.09, 4.02, 4.16 |

## 6.01 Whole-zone player broadcast (WLD-10)

**Goal:** Players see each other appear, move and leave.

**Size:** M. **Depends on:** 5.02, 5.03, 5.05, 4.16

**Client messages:** MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE, MSG_CLIENTMOVESTATE, MSG_JUMP, MSG_CLIENTMOVE

**Acceptance**

- [ ] Two clients see each other with correct name and gear
- [ ] B sees A's smooth run and idle within ~0.5 s; jumps relay
- [ ] A logs out and vanishes on B at once; quick relog shows no ghost
- [ ] Changing Zone.MoveFlushInterval applies from the next flush

### Detailed spec from WLD-10: Players see each other (whole-zone broadcast)

Two real clients in the same zone instance see each other appear, walk, jump, animate and disappear.

**Deliverables**

- Map::AddPlayer: send the newcomer's public WizClientObject to everyone present, and every present player's object to the newcomer, via MSG_NEWOBJECT with the Public flag mask
- MovementHandler: relay MSG_SERVERMOVE (the player's MobileID) and MSG_MOVESTATE (GlobalID) to others, batched on Zone.MoveFlushInterval; relay MSG_JUMP when ExcludeOriginator is set
- Idle detection: after no MSG_CLIENTMOVE for Zone.MoveIdleIntervals flush intervals (default 2), broadcast MSG_MOVESTATE NewState=0 once
- Zone.MoveFlushInterval and Zone.MoveIdleIntervals are live settings applied from the next flush
- Map::RemovePlayer: MSG_REMOVEOBJECT (GameObjectID) to the remaining players

**Client messages:** MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE, MSG_CLIENTMOVESTATE, MSG_JUMP, MSG_CLIENTMOVE

**Acceptance**

- [ ] Two real clients A and B log into WC_Hub: each sees the other's wizard with correct name and gear
- [ ] A walks in a circle: B sees smooth motion with the run animation, and A stops animating within about half a second of stopping
- [ ] A jumps: B sees the jump
- [ ] A logs out: A's model vanishes on B's screen at once
- [ ] A logs back in quickly and gets a new mobile id: B sees one A, not a ghost
- [ ] Unit: changing Zone.MoveFlushInterval or Zone.MoveIdleIntervals applies from the next flush without a restart

**Risks**

- The capture shows MSG_SERVERMOVE and MSG_MOVESTATE in equal counts (3141 each), so they are likely paired per update; how often the client expects them is unverified
- Public versus authority property masks for other players' objects come from WIZ. A wrong mask leaks private data or crashes the viewer.

## 6.02 PLAYERWIZBANG broadcast (WIZ-1 remainder)

**Goal:** Spellbook wizbang seen by others.

**Size:** S. **Depends on:** 6.01

**Client messages:** MSG_PLAYERWIZBANG, MSG_WIZBANG

**Acceptance**

- [ ] Real client: opening/closing the spellbook shows and clears the wizbang for others

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

## 6.03 Say chat, quick chat, core emotes (WIZ-4)

**Goal:** Chat bubbles and emotes in range.

**Size:** M. **Depends on:** 6.01, 4.02, 4.15, 4.16

**Client messages:** MSG_REQUESTRADIALCHAT, MSG_RADIALCHAT, MSG_REQUESTRADIALQUICKCHAT, MSG_RADIALQUICKCHAT, MSG_REQUESTRADIALQUICKCHATEXT, MSG_RADIALQUICKCHATEXT, MSG_CORE_EMOTE

**Acceptance**

- [ ] Command-prefixed messages are never broadcast
- [ ] B sees A's 'hello' bubble, quick chat and wave
- [ ] Prefix and range changes apply to the next message; a failed `.reload quickchat` keeps the old IDs

### Detailed spec from WIZ-4: Say chat, quick chat and core emotes

Players in the same zone can see each other's typed chat bubbles, quick-chat lines and emote animations.

**Deliverables**

- src/server/game/Handlers/ChatHandler.cpp: REQUESTRADIALCHAT -> RADIALCHAT to players in range, REQUESTRADIALQUICKCHAT -> RADIALQUICKCHAT, REQUESTRADIALQUICKCHATEXT -> RADIALQUICKCHATEXT
- CORE_EMOTE handler: echo to range, respecting ExcludeOriginator
- src/server/game/Chat/ChatMgr: range filter on Chat.SayRange, blocks senders on the listener's ignore list (the list itself comes in WIZ-17), and routes text starting with GM.CommandPrefix (default '.') to the command system instead of broadcast. Both are live settings applied to the next message.
- `.reload quickchat` rebuilds the quick-chat ID set from QuickChat.xml off to the side, validates it, and swaps it; a failure keeps the old set and reports every error
- src/test/server/game/ChatHandlerTest.cpp

**Client messages:** GAME MSG_REQUESTRADIALCHAT, GAME MSG_RADIALCHAT, GAME MSG_REQUESTRADIALQUICKCHAT, GAME MSG_RADIALQUICKCHAT, GAME MSG_REQUESTRADIALQUICKCHATEXT, GAME MSG_RADIALQUICKCHATEXT, GAME MSG_CORE_EMOTE

**Data sources**

- Root.wad QuickChat.xml (root QuickChatEntry, m_chatID) for validating quick-chat IDs

**Acceptance**

- [ ] Unit test: a message starting with the command prefix is never broadcast
- [ ] Unit test: RADIALCHAT SourceName is the wizard's name blob and SourceID the player's GameObject GID
- [ ] Unit test: changing GM.CommandPrefix or Chat.SayRange applies to the next message without a restart, and `.reload quickchat` with an unreadable QuickChat.xml keeps the old ID set and reports the error
- [ ] Two real clients in one zone: A types 'hello'. B sees a speech bubble over A and a line in the chat log. A picks a quick-chat phrase and B sees it. A clicks the wave emote and B sees A wave.

**Risks**

- The capture shows REQUESTRADIALCHAT Message bytes as UTF-16 even though the XML types the field STR. The codec must treat it as a length-prefixed byte string and pass the payload through untouched; the exact encoding is unverified.
- The meaning of the Filter byte (the reference sends 2 for typed chat and 0 for quick chat) is unverified.
- In the capture the client sends CORE_EMOTE Name=Chat together with each typed line. It must not be treated as a real emote request.

## 6.04 GM commands in chat (WIZ-5 remainder)

**Goal:** Chat prefix routes to CommandMgr; cs_gm/cs_character/cs_lookup.

**Size:** M. **Depends on:** 6.03, 5.01, 4.16

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT, MSG_SERVERMESSAGE, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [ ] GM '.help' lists commands with no bubble for others
- [ ] Player account text is chat or refused per config
- [ ] GM.LogCommands changes apply from the next command

### Detailed spec from WIZ-5: Account security levels and GM command framework

An account's security level decides which chat-prefixed GM commands it may run, and the first cs_ command groups give testers a harness for every later milestone.

**Deliverables**

- data/sql/updates/db_login: account_access (account_id, realm_id, security_level) with levels PLAYER=0, MODERATOR=1, GAMEMASTER=2, ADMINISTRATOR=3, CONSOLE=4 (AzerothCore precedent)
- src/server/game/Chat/CommandMgr: CommandScript tables, argument parsing, security check against the command's default level or its command_security override, per-command help
- src/server/scripts/Commands/cs_gm.cpp (.gm on/off, .gm visible), cs_character.cpp (.character level, .character gold, .character xp, .character heal), cs_lookup.cpp (.lookup item / spell by name)
- Replies via SYSTEM MSG_SERVERMESSAGE or GAME MSG_CLIENTNOTIFYTEXT
- Set LOGINCOMPLETE IsCSR and Permissions from the security level (coordinate with NET/LOG)
- gameserver.conf.dist: GM.CommandPrefix, GM.LogCommands, live settings applied from the next command
- src/test/server/game/CommandMgrTest.cpp

**Client messages:** GAME MSG_COMMAND, GAME MSG_COMMANDRESULT, SYSTEM MSG_SERVERMESSAGE, GAME MSG_CLIENTNOTIFYTEXT

**Database tables**

- login.account_access
- world.command_security
- characters.gm_command_log (optional)

**Acceptance**

- [ ] Unit test: a PLAYER-level account running a GAMEMASTER command gets 'no such command' and nothing executes
- [ ] Unit test: the command table parses '.character gold 500' into (character, gold, [500])
- [ ] Unit test: turning GM.LogCommands off stops logging from the next command without a restart
- [ ] Real client, GM account: typing '.help' shows the command list in the chat window, and nearby players see no bubble. The same text from a player account shows up as normal chat or is refused, depending on config.

**Risks**

- Whether the stock client ever sends GAME MSG_COMMAND itself is unverified. The capture only shows '.mod ...' arriving as REQUESTRADIALCHAT.
- The bit meanings of LOGINCOMPLETE Permissions are unverified. The reference sends a constant 207 (0b11001111).

## 6.05 GM account, ban, character commands (LOG-17)

**Goal:** Manage accounts without SQL.

**Size:** M. **Depends on:** 6.04, 3.17, 4.15

**Client messages:** MSG_FORCE_DISCONNECT, MSG_SERVERMESSAGE

**Acceptance**

- [ ] Lower security levels refused
- [ ] A command_security override applies after `.reload command_security` without a restart
- [ ] `ban account test 1h spam` disconnects and blocks next login; unban restores
- [ ] `character deleted restore <guid>` brings the wizard back

### Detailed spec from LOG-17: GM account and character commands

Operators can manage accounts, bans, security levels and deleted characters from the gameserver console or in-game chat, without editing SQL by hand.

**Deliverables**

- src/server/scripts/Commands/cs_account.cpp: account create, delete, set password (revokes account_session), set gmlevel, lock, unlock, onlinelist
- src/server/scripts/Commands/cs_ban.cpp: ban account, ban ip, ban machine, unban, baninfo
- src/server/scripts/Commands/cs_character.cpp: character deleted list, character deleted restore, character rename flag (sets should_rename)
- Security levels mapped to account.security_level; command tables declare their default required level, and a world.command_security row overrides it live through `.reload command_security`

**Client messages:** MSG_FORCE_DISCONNECT, MSG_SERVERMESSAGE

**Database tables**

- account
- account_banned
- ip_banned
- machine_banned
- account_session
- characters

**Acceptance**

- [ ] Unit: each command's permission check refuses a lower security level
- [ ] Unit: a world.command_security row raising '.ban account' to ADMINISTRATOR, then `.reload command_security`, refuses a GAMEMASTER account without a restart
- [ ] Real client: `ban account test 1h spam` from a GM character disconnects the target, whose next login attempt is refused; `unban` lets them back in
- [ ] Real client: `character deleted restore <guid>` makes a deleted wizard reappear on its owner's select screen after a relog

**Risks**

- It depends on the command framework milestone of whichever domain owns ScriptMgr and CommandScript (guessed prefix CMD, which is not in the listed prefixes; possibly FND or WLD).

## 6.06 Same-zone teleport and cs_tele (WLD-12)

**Goal:** GM teleport seen by onlookers.

**Size:** S. **Depends on:** 6.01, 4.02, 4.15

**Client messages:** MSG_SERVERTELEPORT, MSG_COMMAND, MSG_COMMANDRESULT

**Acceptance**

- [ ] '.tele Start' snaps without loading; B sees it
- [ ] '.gps' matches minimap
- [ ] Out-of-range '.go xyz' refused; players cannot '.tele'
- [ ] A point from '.tele add' works at once without a reload

### Detailed spec from WLD-12: Same-zone teleport and GM teleport commands

A GM can instantly move themselves or another player to a named location or coordinates in the current zone, and onlookers see it.

**Deliverables**

- src/server/game/Entities/Player::TeleportWithinMap: update position and send MSG_SERVERTELEPORT (packed location, MobileID) to self and viewers
- src/server/scripts/Commands/cs_tele.cpp: '.tele <location name>', '.go xyz <x> <y> <z> [yaw]', '.gps' (zone, x, y, z, yaw, dynamic zone id), with security levels
- world.game_tele table (named GM teleport points: name, zone, x, y, z, yaw) and '.tele add/del', which update the live list and the table at once and are journaled as a pending SQL update
- `.reload game_tele` swaps in hand-edited rows; a failure keeps the old list and reports every error

**Client messages:** MSG_SERVERTELEPORT, MSG_COMMAND, MSG_COMMANDRESULT

**Data sources**

- world.zone_location

**Database tables**

- world.game_tele

**Acceptance**

- [ ] Real client: '.tele Start' in WC_Hub snaps the wizard to the Start fountain with no loading screen, and B sees A pop to the new spot
- [ ] Real client: '.gps' prints coordinates matching the minimap position
- [ ] Unit: '.go xyz' outside the packable range is refused with a message
- [ ] A player-level account cannot run '.tele'
- [ ] Real client: a point added with '.tele add' works immediately without a reload or restart

**Risks**

- Direction handling in MSG_SERVERTELEPORT carries the same unverified byte scale as WLD-1

## 6.07 Cross-zone transfer via MSG_SERVERTRANSFER (WLD-13)

**Goal:** Retail loading-screen zone change.

**Size:** M. **Depends on:** 6.06, 4.13

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_ZONETRANSFERNACK, MSG_SERVERTRANSFER, MSG_RETRYTELEPORT, MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] '.tele zone WizardCity/WC_Ravenwood' loads Ravenwood 'Start'; B in WC_Hub sees A disappear
- [ ] Bad zone path gives an error and the player stays
- [ ] Double request ignored; NACK clears; transfer key works once

### Detailed spec from WLD-13: Cross-zone transfer via reconnect (MSG_SERVERTRANSFER)

Players can move between zones with the retail loading-screen flow, triggered here by a GM command.

**Deliverables**

- src/server/game/Handlers/ZoneHandler.cpp: Player::TransferToZone sends MSG_ZONETRANSFERREQUEST (SendAck=1), waits for MSG_ZONETRANSFERACK or MSG_ZONETRANSFERNACK, removes the player from the Map, saves position, issues a one-time attach key, sends MSG_SERVERTRANSFER (IP, ports, Key, UserID, CharID, ZoneName, Location, TransitionID, Fallback* set to the current zone)
- Transfer-queued guard (no double transfers), MSG_RETRYTELEPORT re-sends the last transfer, and fallback so a MSG_ATTACHFAILED or timeout returns the player to the Fallback zone
- '.tele zone <zone path> [location]' in cs_tele.cpp
- TransitionID chosen from ZoneTPTrans.xml (loading-screen art) when present, else 1

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_ZONETRANSFERNACK, MSG_SERVERTRANSFER, MSG_RETRYTELEPORT, MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Data sources**

- <zone>.wad/ZoneTPTrans.xml (in 171 zones, e.g. 'Krokotopia World Transition' with a loadscreen jpg)

**Database tables**

- characters.characters
- login.account_session

**Acceptance**

- [ ] Real client: '.tele zone WizardCity/WC_Ravenwood' shows the loading screen and the player arrives at Ravenwood 'Start'; B in WC_Hub sees A disappear
- [ ] Real client: transferring to a zone path that does not exist gives an error message and the player stays put
- [ ] Unit: a second transfer request while one is queued is ignored; a NACK clears the queue
- [ ] Unit: the attach key issued for the transfer works exactly once

**Risks**

- This is the path the behavior reference uses for every zone change and it is known to work; the cheaper same-connection MSG_ZONETRANSFER is WLD-21
- Key is INT in MSG_SERVERTRANSFER but STR in MSG_CHARACTERSELECTED. How the client carries it into MSG_ATTACH.LoginKey is unverified.

## 6.08 Logout, link-dead, AFK, shutdown (WLD-20)

**Goal:** Clean world exit in all cases.

**Size:** M. **Depends on:** 6.01, 4.16

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK, MSG_NOT_AFK, MSG_SERVERSHUTDOWN, MSG_REMOVEOBJECT, MSG_ATTACH

**Acceptance**

- [ ] Quit removes A from B at once; relog same spot
- [ ] Killed process: B sees A stand then vanish; reattach within window resumes
- [ ] Server stop shows notice; second attach kicks the first
- [ ] Changing Player.AfkTime applies from the next AFK timer

### Detailed spec from WLD-20: Logout, disconnect, link-dead and server shutdown

Players leave the world cleanly on logout, crash or server stop, their position is saved, and others see them go.

**Deliverables**

- Handlers: MSG_QUERY_LOGOUT (reply per IsInstance), MSG_CLIENT_DISCONNECT (save and remove immediately)
- Socket loss without logout: keep the player in the Map as link-dead for Player.LinkDeadTime and send MSG_ZOMBIE_PLAYER (GlobalID, Remaining) to viewers; a reattach in that window resumes the session (MSG_ATTACH Reattach=1)
- AFK: MSG_DISCONNECT_AFK warning, then disconnect after Player.AfkTime; MSG_NOT_AFK resets the timer
- World shutdown: MSG_SERVERSHUTDOWN to all sessions, save all positions, then close
- conf/dist/gameserver.conf.dist: Player.LinkDeadTime, Player.AfkWarnTime, Player.AfkTime, live settings applied from the next timer

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK, MSG_NOT_AFK, MSG_SERVERSHUTDOWN, MSG_REMOVEOBJECT, MSG_ATTACH

**Database tables**

- characters.characters

**Acceptance**

- [ ] Real client: Quit from the menu on client A removes A from B's view at once, and A relogs at the same spot
- [ ] Kill A's process: B sees A stand still, then vanish after the link-dead time; relogging within it resumes without a duplicate
- [ ] Server stop: connected clients get the shutdown notice and a relog after restart puts them where they were
- [ ] Unit: two sessions for one character: the second attach kicks the first cleanly
- [ ] Unit: changing Player.AfkTime or Player.LinkDeadTime applies from the next timer without a restart

**Risks**

- The semantics of Reattach and Retry in MSG_ATTACH are unverified
- The payload of MSG_SERVERSHUTDOWN Message (UINT) is unknown

## 6.09 Schema probe for classes missing from the dump (OBJ-11)

**Goal:** Name and type unknown hashes via the hash oracle.

**Size:** M. **Depends on:** 3.11

**Acceptance**

- [x] Lists 1451865413 (7094), 520243970 (6875), 1120896859 (5567), 829470368 (5473). `schemaprobe` against the pinned install lists all four with exactly those instance counts, each under m_behaviors
- [ ] 520243970 gets m_behaviorName, m_npcProximity, m_questList, m_personaName
- [x] Full Root.wad under 5 minutes. 173088 entries read and 134635 of 134640 BINd files decoded in 32.5 seconds, 361 MiB peak

### Detailed spec from OBJ-11: Schema probe tool for classes missing from the dump

Unknown class and property hashes found in client data can be named and typed by us using the hash formulas as an oracle.

**Deliverables**

- src/tools/schemaprobe: sweeps Root.wad and zone WADs, collects unknown class hashes with instance counts, paths, parent property, and each property's hash and bit-size distribution
- Oracle matching: tests each unknown property hash against every (known property name x known type string) pair from the registry, plus a user-supplied candidate list; tests candidate class names against the class hash
- Output: a report and a draft schema in our own format for OBJ-12

**Acceptance**

- [ ] Client-gated run lists at least the top unknowns found in my sweep: 1451865413 (7094 instances), 520243970 (6875), 1120896859 (5567), 829470368 (5473), all under ObjectData m_behaviors, plus Result/Requirement subclasses under ResultList.m_results and RequirementList.m_requirements
- [ ] For 520243970, the oracle names m_behaviorName:std::string, m_npcProximity:float, m_questList:std::string and m_personaName:std::string (I reproduced this in Python)
- [ ] Report runtime on the full Root.wad stays under 5 minutes

**Risks**

- Class names cannot be recovered from hashes by brute force. Simple name variants of known classes found none, so some classes may stay anonymous (hash-named) while still being decodable

## 6.10 Supplemental server-side schemas (OBJ-12)

**Goal:** Server-owned classes decode like dump classes.

**Size:** S. **Depends on:** 6.09, 3.03, 4.15

**Acceptance**

- [ ] A supplemental class extends a dump base and decodes
- [ ] A name/type not hashing to its declared hash is rejected
- [ ] A failed `.reload server_class_schema` keeps the old registry

### Detailed spec from OBJ-12: Supplemental server-side class schemas

Classes that exist in client data or server logic but not in the client dump decode and encode like any other class.

**Deliverables**

- A supplemental schema format (JSON or conf, our own) of classes with name or hash-only id, bases, and properties (name or hash, type, flags, container), merged into sTypeRegistry after the dump. Every named entry is verified by the hash formula at load
- Location: the world database's server_class, server_class_base and server_class_property tables, settled on 2026-09-25 and recorded in doc/ARCHITECTURE.md; contains no client bytes. The tables, their loader and the registry's rebuild were built early for 4.11, whose player object needs BasicMobileBehavior, and this milestone's checks stay open until 6.09 is done
- Registry reports which classes came from the dump and which from the supplement
- `.reload server_class_schema` merges the edited supplement into a new registry generation off to the side, verifies every hash, and swaps it; a failure keeps the old registry and reports every error

**Acceptance**

- [ ] Unit test: a supplemental class extends a dump base class and decodes a synthetic versionable blob
- [ ] Unit test: a supplemental entry whose name and type do not hash to its declared property hash is rejected
- [ ] Unit test: `.reload server_class_schema` with an added class decodes it without a restart, and a supplement with a bad hash keeps the old registry and reports the error
- [ ] Client-gated sweep: the unknown-class count from OBJ-6 drops for every class added; the sweep result is recorded

## 6.11 Trigger/volume schemas and zone WAD sweep (WLD-3 part 1 + OBJ-18)

**Goal:** WizZoneTriggers 0x06DAAC43, Trigger 0x068C265B, WizZoneVolumes 0x1B6EF770, Volume 0x1B7B55F6 authored and hash-checked.

**Size:** M. **Depends on:** 6.10

**Acceptance**

- [ ] Every authored class and property hash recomputes (these classes confirmed absent from the dump)
- [ ] Sweep of all zone WADs: zero crashes, every file kind has a root class
- [ ] Aquila-AQ_Z00_Hub.wad triggers.xml and volumes.xml decode with no unknown classes

### Detailed spec from WLD-3: Zone extractor part 2: volumes and triggers (server-only classes)

Walk-in volumes and event triggers for every zone are decoded into typed world rows, even though their classes are missing from the client type dump.

**Deliverables**

- src/server/shared/ObjectProperty: hand-authored schemas registered for WizZoneTriggers (0x06DAAC43), Trigger (0x068C265B), WizZoneVolumes (0x1B6EF770), Volume (0x1B7B55F6) and the Result/Requirement subclasses seen in triggers.xml, each name checked by recomputing its property-name hash
- zone_extractor: BINd reader (magic 'BINd', uint32 flags=7, then class hash), decodes triggers.xml, volumes.xml and trigger_groups.xml
- data/sql/base/db_world: zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Data sources**

- <zone>.wad/triggers.xml, volumes.xml, trigger_groups.xml (BINd, flags 7)
- <zone>.wad/trig_backup_saveme.notxml is a leftover plain-XML test file (WC_Hub has one). It hints at field names but is not a data source.

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [ ] Unit: each hand-authored class hash equals the string hash of its name, and each property id equals the hash of its property name
- [ ] WC_Hub volumes decode to rows including 'Ravenwood POI' (Sphere, enter event 'Enter_Ravenwood POI', exit 'Exit_Ravenwood POI', type STATIC_CLIENT_SERVER)
- [ ] WC_Hub triggers decode to rows including 'Trigger POI Ravenwood' (fire event 'Enter_Ravenwood POI') and 'TeleportToShoppingDistrict' with a teleport result whose destination is empty
- [ ] Extractor summary counts zones whose trigger or volume files fail to decode, target 0 across all 3356

**Risks**

- These classes are server-side and absent from the client dump, so schemas must be reverse-engineered from the binary alone. Verify every name against its hash, and leave unknown fields as typed but unnamed, never skipped.
- ResTeleport has no destination zone or location in client data (confirmed: the dump defines the class with no properties). Destinations come from WLD-14.

### Detailed spec from OBJ-18: Zone WAD object files decode sweep

Per-zone BINd object files (spawns, triggers, volumes, paths) decode reliably and are ready for the world domain to consume.

**Deliverables**

- An extension of bindecode/schemaprobe that sweeps all 3589 zone and sound WADs for BINd .xml entries (spawnData.xml, clientSpawnData.xml, triggers.xml, trigger_groups.xml, volumes.xml, pathData.xml, FishingInfo.xml, Compass.xml) and reports root classes, unknown classes and decode failures per file name
- A documented list of the root classes per file kind for WLD to build on (e.g. triggers and volumes roots, which the reference study shows are server-side classes not in the client dump)

**Acceptance**

- [ ] Client-gated sweep over all zone WADs completes with zero crashes; every file kind has a known root class or an OBJ-12 supplemental entry
- [ ] The Aquila-AQ_Z00_Hub.wad triggers.xml and volumes.xml decode with no unknown classes after supplements

**Risks**

- Trigger, volume and zone-router classes appear to be server-only. Naming them may take several OBJ-11/OBJ-12 iterations

## 6.12 Volume/trigger extraction to world rows (WLD-3 part 2)

**Goal:** zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result.

**Size:** M. **Depends on:** 6.11, 4.08

**Acceptance**

- [ ] WC_Hub 'Ravenwood POI' sphere with Enter_/Exit_Ravenwood POI, STATIC_CLIENT_SERVER
- [ ] 'Trigger POI Ravenwood' and 'TeleportToShoppingDistrict' with an empty teleport destination
- [ ] Failures across 3356 zones counted, target 0

### Detailed spec from WLD-3: Zone extractor part 2: volumes and triggers (server-only classes)

Walk-in volumes and event triggers for every zone are decoded into typed world rows, even though their classes are missing from the client type dump.

**Deliverables**

- src/server/shared/ObjectProperty: hand-authored schemas registered for WizZoneTriggers (0x06DAAC43), Trigger (0x068C265B), WizZoneVolumes (0x1B6EF770), Volume (0x1B7B55F6) and the Result/Requirement subclasses seen in triggers.xml, each name checked by recomputing its property-name hash
- zone_extractor: BINd reader (magic 'BINd', uint32 flags=7, then class hash), decodes triggers.xml, volumes.xml and trigger_groups.xml
- data/sql/base/db_world: zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Data sources**

- <zone>.wad/triggers.xml, volumes.xml, trigger_groups.xml (BINd, flags 7)
- <zone>.wad/trig_backup_saveme.notxml is a leftover plain-XML test file (WC_Hub has one). It hints at field names but is not a data source.

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [ ] Unit: each hand-authored class hash equals the string hash of its name, and each property id equals the hash of its property name
- [ ] WC_Hub volumes decode to rows including 'Ravenwood POI' (Sphere, enter event 'Enter_Ravenwood POI', exit 'Exit_Ravenwood POI', type STATIC_CLIENT_SERVER)
- [ ] WC_Hub triggers decode to rows including 'Trigger POI Ravenwood' (fire event 'Enter_Ravenwood POI') and 'TeleportToShoppingDistrict' with a teleport result whose destination is empty
- [ ] Extractor summary counts zones whose trigger or volume files fail to decode, target 0 across all 3356

**Risks**

- These classes are server-side and absent from the client dump, so schemas must be reverse-engineered from the binary alone. Verify every name against its hash, and leave unknown fields as typed but unnamed, never skipped.
- ResTeleport has no destination zone or location in client data (confirmed: the dump defines the class with no properties). Destinations come from WLD-14.

## 6.13 Volumes and walk-in trigger events (WLD-14)

**Goal:** Enter/exit fires triggers and ZoneScript hooks.

**Size:** M. **Depends on:** 6.12, 5.03, 4.01, 4.15

**Client messages:** MSG_POSTZONEEVENTFROMCLIENT, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [ ] Containment for each primitive incl. boundary
- [ ] Cooldown fires once per player
- [ ] Walking into Ravenwood POI logs 'Enter_Ravenwood POI' and fires 'Trigger POI Ravenwood'; spawning inside fires nothing
- [ ] Triggers with requirements fail closed until 7.04
- [ ] `.reload zone_trigger` applies an edited trigger; a failed reload keeps the old set

### Detailed spec from WLD-14: Volumes and walk-in trigger events

Walking into a zone volume fires its enter and exit events into the zone's triggers and the script system.

**Deliverables**

- src/server/game/Zones/ZoneVolume.h/.cpp: sphere/box/cylinder containment from zone_volume (primitiveType, radius, length, ...); a player spawned inside counts as inside without firing enter
- src/server/game/Zones/ZoneTriggerMgr: event bus per Map; a trigger with a matching fireEvent passes cooldown and triggerMax, then its results run through a Result dispatcher interface (results implemented by owning domains)
- The built-in 'EnterZone' event fires on player enter (the reference posts 'EnterZone'; the data uses both 'StartZone' and 'EnterZone')
- ZoneScript hooks: OnVolumeEnter, OnVolumeExit, OnTriggerFired; scripts/World/ loader
- MSG_POSTZONEEVENTFROMCLIENT posts client-sent events, allow-listed per zone
- `.reload zone_trigger` rebuilds volumes, triggers, events, results, cooldowns and the client-event allow-list off to the side, validates them, and swaps them into live Maps; a failure keeps the old set and reports every error. A player inside a volume that still exists stays inside without firing enter.

**Client messages:** MSG_POSTZONEEVENTFROMCLIENT, MSG_CLIENTNOTIFYTEXT

**Data sources**

- world.zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [ ] Unit: containment tests for each primitive type, including boundary and hysteresis
- [ ] Unit: a trigger with a cooldown fires once per player per cooldown
- [ ] Real client: walking into Ravenwood's POI sphere in WC_Hub logs 'Enter_Ravenwood POI' and fires 'Trigger POI Ravenwood'; with the POI-text result wired, the zone-entry text shows
- [ ] Real client: logging in while standing inside a volume fires no enter event
- [ ] Unit: editing a zone_trigger row, then `.reload zone_trigger`, changes what fires on the next enter without a restart; a row that fails validation keeps the old triggers and reports the error

**Risks**

- Which message shows POI text (e.g. WizardPOI_00000001 keys) is unverified
- Requirement evaluation on triggers depends on QST's requirement engine. Until it exists, triggers with requirements must fail closed, not fire.

## 6.14 Zone doors table and walk-in transfers (WLD-15)

**Goal:** Walk through exits to the right arrival point.

**Size:** M. **Depends on:** 6.07, 6.13, 4.15

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_SERVERTRANSFER, MSG_SERVERTELEPORT, MSG_ENTERSTATE

**Acceptance**

- [ ] Real client: WC_Hub Ravenwood gate lands in Ravenwood; back lands at 'Target location(WC_Hub Ravenwood)'
- [ ] 'Teleport location (WC_Hub WC_Headmistress_House Entrance)' works
- [ ] Two triggers on one event give one transfer; '.zone teleports' flags missing destinations
- [ ] `.reload zone_teleport` sends the next walk-through to an edited destination

### Detailed spec from WLD-15: Zone doors: teleport destination table and walk-in transfers

Walking through a zone exit (e.g. WC_Hub -> Ravenwood) transfers the player to the correct zone and arrival point.

**Deliverables**

- world.zone_teleport table: zone, trigger_name -> dest_zone, dest_location, transition_id, same_zone flag
- `.reload zone_teleport` rebuilds the destination map off to the side, validates every destination zone and location, and swaps it; a failure keeps the old rows and reports every error
- ResTeleport result handler: a same-zone destination uses WLD-12; otherwise WLD-13. When paired triggers share an event, only the first teleport in data order runs.
- src/tools/zone_extractor --propose-teleports: suggests pairs by matching 'Target location (<SrcZone> <DstZone> Exit)'-style location names and 'TeleportTo<X>' trigger names across zones, and writes a review CSV. The opt-in --apply-proposals also writes the suggestions into the user's local world database as journaled edits exportable as a pending SQL update; proposals reach the repository only as rows a human has reviewed
- data/sql/updates/db_world: hand-reviewed zone_teleport rows for the Wizard City starting area (WC_Hub <-> Ravenwood, Shopping District, Unicorn Way, Golem Court, Library)

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_SERVERTRANSFER, MSG_SERVERTELEPORT, MSG_ENTERSTATE

**Data sources**

- world.zone_location names (e.g. WC_Hub 'Target location (WC_Hub Street1 Exit)')
- world.zone_trigger names and events (e.g. 'TeleportToShoppingDistrict', 'TeleportToRavenwoodTrigger')

**Database tables**

- world.zone_teleport
- world.zone_trigger_result

**Acceptance**

- [ ] Real client: in WC_Hub, walking through the Ravenwood gate shows the loading screen and lands in Ravenwood facing away from the gate; walking back lands at 'Target location(WC_Hub Ravenwood)' in WC_Hub
- [ ] Real client: walking into 'Teleport location (WC_Hub WC_Headmistress_House Entrance)' works
- [ ] Unit: two triggers on one event with teleport results produce exactly one transfer
- [ ] '.zone teleports <zone>' lists each teleport trigger and flags any with no destination row
- [ ] Real client: editing a zone_teleport destination, then `.reload zone_teleport`, sends the next walk-through to the new destination without a restart; a row naming a missing zone keeps the old rows

**Risks**

- Destination data is not in the client: authoring rows for ~3356 zones is a large manual content job. Committed rows come only from a name-matching heuristic plus human review, which keeps them clean-room. An opt-in importer that reads another project's teleport data from a copy the user has, into that user's local world database only, is planned, not yet scheduled; imported rows are never committed or redistributed.
- Decided on 2026-09-16 at the maintainer's direction: a destination table that names zones, locations and triggers by their client identifiers may be committed as hand-reviewed zone_teleport rows, like key-only quest SQL. No client file or client text is copied into them.

## 6.15 AOI grid and visibility sets, unit level (WLD-11 part 1)

**Goal:** Per-player known sets with hysteresis.

**Size:** M. **Depends on:** 6.01, 4.16

**Client messages:** MSG_ADDOBJECT

**Acceptance**

- [ ] Boundary crossing within hysteresis sends nothing
- [ ] Re-entry sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT
- [ ] Changing Visibility.Distance applies on the next visibility update

### Detailed spec from WLD-11: Area of interest: grid visibility

In big or busy zones each client gets only objects and players within range, with correct spawn and despawn as people move.

**Deliverables**

- src/server/game/Zones/Grid.h/.cpp: cell grid per Map with neighbor queries
- src/server/game/Zones/VisibilitySet: per-player known-object set that sends MSG_ADDOBJECT on re-entry and MSG_REMOVEOBJECT on exit; MSG_NEWOBJECT only the first time an object is made known
- Objects whose template has m_exemptFromAOI are always visible
- Move relays go only to players who can see the mover
- conf/dist/gameserver.conf.dist: Visibility.Distance (default: zone m_farClip), Visibility.Hysteresis, live settings; a change re-evaluates every visibility set on the next update
- src/test/server/game/Zones/GridTest.cpp, VisibilitySetTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Data sources**

- zone_template.farClip
- GameObjectTemplate.m_exemptFromAOI

**Acceptance**

- [ ] Unit: an object crossing the range boundary back and forth within the hysteresis band generates no messages
- [ ] Unit: re-entry after exit sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT
- [ ] Real client: in a large zone, B walks away from A: A sees B vanish at range and reappear when B returns, in the right place
- [ ] Real client: a far-off exempt landmark stays visible
- [ ] Unit: lowering Visibility.Distance removes objects now out of range on the next update without a restart

**Risks**

- The reference notes the client ignores MSG_ADDOBJECT for an object it never got via MSG_NEWOBJECT, and needs about 250ms between NEWOBJECT and REMOVEOBJECT to register it. Both are observed behavior, not specified.
- The reference culls only objects whose animation template fades in or out. The retail rule for which objects are AOI-culled is unknown.

## 6.16 AOI in the real client (WLD-11 part 2)

**Goal:** Busy zones stream only nearby objects.

**Size:** M. **Depends on:** 6.15

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Acceptance**

- [ ] B walks away and reappears in the right place for A
- [ ] Far exempt landmark stays visible

### Detailed spec from WLD-11: Area of interest: grid visibility

In big or busy zones each client gets only objects and players within range, with correct spawn and despawn as people move.

**Deliverables**

- src/server/game/Zones/Grid.h/.cpp: cell grid per Map with neighbor queries
- src/server/game/Zones/VisibilitySet: per-player known-object set that sends MSG_ADDOBJECT on re-entry and MSG_REMOVEOBJECT on exit; MSG_NEWOBJECT only the first time an object is made known
- Objects whose template has m_exemptFromAOI are always visible
- Move relays go only to players who can see the mover
- conf/dist/gameserver.conf.dist: Visibility.Distance (default: zone m_farClip), Visibility.Hysteresis, live settings; a change re-evaluates every visibility set on the next update
- src/test/server/game/Zones/GridTest.cpp, VisibilitySetTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Data sources**

- zone_template.farClip
- GameObjectTemplate.m_exemptFromAOI

**Acceptance**

- [ ] Unit: an object crossing the range boundary back and forth within the hysteresis band generates no messages
- [ ] Unit: re-entry after exit sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT
- [ ] Real client: in a large zone, B walks away from A: A sees B vanish at range and reappear when B returns, in the right place
- [ ] Real client: a far-off exempt landmark stays visible

**Risks**

- The reference notes the client ignores MSG_ADDOBJECT for an object it never got via MSG_NEWOBJECT, and needs about 250ms between NEWOBJECT and REMOVEOBJECT to register it. Both are observed behavior, not specified.
- The reference culls only objects whose animation template fades in or out. The retail rule for which objects are AOI-culled is unknown.

## 6.17 Network hardening (NET-11)

**Goal:** Abusive traffic disconnected predictably.

**Size:** M. **Depends on:** 2.09, 4.16

**Acceptance**

- [ ] 10-minute fuzz: no crash, no allocation above MaxFrameSize
- [ ] 10k frames/s flooder disconnected within 1 s
- [ ] A never-reading client disconnected at the high-water mark
- [ ] Changing Network.RateLimit.PerSecond applies to connected sessions from the next frame

### Detailed spec from NET-11: Network hardening

Malformed, oversized or abusive traffic can't crash or stall a server and is disconnected predictably.

**Deliverables**

- Per-session token bucket on frames per second (Network.RateLimit.Burst=150, PerSecond=50, as a reference server's defaults suggest) with a violation counter and disconnect
- Per-IP connection cap and accept-rate cap in SocketMgr (Network.MaxConnectionsPerIP, Network.AcceptRatePerSecond)
- Strict DML checks: dmlLen must match bytes consumed by Decode; STR/WSTR length above the remaining frame gives a reject; control opcode not in {0,3,4,5} gives a strike
- Send-queue high-water mark (Network.SendQueueHighWater): a slow reader is disconnected instead of growing memory without limit
- The rate limits, caps and high-water mark are live settings with bounds, applied from the next frame or connection
- src/test/fuzz/FrameFuzz.cpp and MessageDecodeFuzz.cpp (libFuzzer target where the compiler supports it; otherwise a randomized gtest)

**Data sources**

- None

**Acceptance**

- [ ] Fuzz run of 10 minutes on the frame and decode paths has no crash, no ASan report and no allocation above MaxFrameSize
- [ ] A test client flooding 10k frames/s is disconnected within 1s, and other sessions show no latency spike above a threshold in the integration test
- [ ] A test client that never reads is disconnected when its send queue passes the high-water mark
- [ ] Integration test: lowering Network.RateLimit.PerSecond while a client is connected throttles that client from the next frame without a restart

**Risks**

- Rate limits set too low could drop legitimate bursts (MSG_CLIENTMOVE during movement, zone loads). Tune with real-client traces

## 6.18 Packet log, diagnostics, network hooks (NET-12)

**Goal:** Named packet logging and CanPacketReceive hooks.

**Size:** S. **Depends on:** 2.09, 4.02, 4.16

**Client messages:** MSG_USER_AUTHEN_V3, MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK

**Acceptance**

- [ ] Log shows 'C->S LOGIN MSG_USER_AUTHEN_V3 (7:27)' with credentials redacted; suppressed messages absent
- [ ] A module blocks one message with no core edits
- [ ] '.network sessions' returns live count
- [ ] '.network packetlog' toggles and filters logging live

### Detailed spec from NET-12: Packet logging, diagnostics and network hooks

Developers can see every message by name with fields, and scripts or modules can observe connections, without touching core code.

**Deliverables**

- src/server/shared/Network/PacketLog.h/.cpp: optional (Network.PacketLog.Enable, .Filter, .Suppress defaults MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK, keepalives) text log with direction, session id, service:order, name and DynamicMessage fields. The PacketLog options are live settings applied from the next message.
- ScriptMgr ServerScript hooks: OnNetworkStart, OnSocketOpen, OnSocketClose, CanPacketReceive(session, service, order), CanPacketSend
- GM command group src/server/scripts/Commands/cs_network.cpp: 'network sessions' (count, per-app), 'network session <id>' (state, RTT, queued bytes, strikes), 'network packetlog on|off|filter <names>' (sets the PacketLog settings live)

**Client messages:** MSG_USER_AUTHEN_V3, MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK

**Data sources**

- Generated registry

**Acceptance**

- [ ] With PacketLog enabled and a real client at login, the log shows 'C->S LOGIN MSG_USER_AUTHEN_V3 (7:27)' with decoded fields, and suppressed messages are absent
- [ ] A test module registering CanPacketReceive returning false for one message blocks it with no core edits
- [ ] '.network sessions' in game chat returns the live count
- [ ] '.network packetlog filter MSG_CLIENTMOVE' on a running server changes what is logged from the next message without a restart

**Risks**

- Logging credentials: the MSG_USER_AUTHEN_V3 payload must be redacted in the log

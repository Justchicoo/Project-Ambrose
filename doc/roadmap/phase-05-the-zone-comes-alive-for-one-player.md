<!-- Project Ambrose by Imjustchico: Roadmap phase 5, The zone comes alive for one player. -->

# Phase 5: The zone comes alive for one player

**Done when:** Ravenwood shows its NPCs and props where retail has them, and the HUD shows real health, mana, gold and level from the DB. Relogging returns to the same spot, and quit-to-select works without a password.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 5.01 | Template manifest and template store (OBJ-16, new DAT-2) | M | 3.11, 3.07, 4.15 |
| 5.02 | Static zone objects appear (WLD-8) | M | 4.14, 5.01 |
| 5.03 | Own movement tracking and persistence (WLD-9) | S | 4.14, 4.16 |
| 5.04 | Level, school and stat-config extractor (WIZ-2) | M | 3.11, 2.07, 4.01, 4.15 |
| 5.05 | Character state and player-object stats (WIZ-3) | M | 5.04, 4.11, 3.16, 4.16 |
| 5.06 | Return to character select (LOG-13) | M | 2.14, 4.07, 5.03, 4.16 |
| 5.07 | Binary type-registry cache (OBJ-15) | S | 3.03 |
| 5.08 | Installer (FND-22) | S | 2.08 |

## 5.01 Template manifest and template store (OBJ-16, new DAT-2)

**Goal:** GetTemplate(id) from the user's WADs.

**Size:** M. **Depends on:** 3.11, 3.07, 4.15

**Acceptance**

- [ ] Fake archive: cache hit/miss, missing id returns null
- [ ] GetTemplate(1) is PlayerObject; GetTemplate(1652259) is the hat; first access under 5 ms
- [ ] `.reload templates` swaps in an edited manifest; a broken one keeps the old map

### Detailed spec from OBJ-16: Template manifest and on-demand template store

The server can fetch any client template by template id, backed by the TemplateManifest and the user's WADs.

**Deliverables**

- src/server/game/Templates/TemplateMgr.h/.cpp (sTemplateMgr): loads TemplateManifest.xml into an id->path map (137423 entries); GetTemplate(id) decodes lazily from Root.wad with an LRU cache; typed accessors through OBJ-10 views
- `.reload templates` rebuilds the manifest map off to the side, validates it, swaps it, and drops cache entries by generation. Objects keep the template snapshot they spawned with. A failure keeps the old map and reports every error.
- src/test/server/game/Templates/TemplateMgrTest.cpp using an in-memory fake archive

**Acceptance**

- [ ] Unit test with a fake archive: manifest lookup, cache hit/miss, missing id returns null without throwing
- [ ] Unit test with a fake archive: `.reload templates` with an edited manifest serves the new entry without a restart, an object spawned before the reload keeps its snapshot, and a manifest that fails validation keeps the old map and reports every error
- [ ] Client-gated test: GetTemplate(1) returns the PlayerObject template; GetTemplate(1652259) returns the hat; a missing manifest path is reported
- [ ] Client-gated benchmark: first access under 5 ms per template; decoding 10k random templates stays within the memory cap

**Risks**

- The architecture puts content in the world DB. Whether full template objects come from the client at runtime or from extracted DB rows is an open question (see OBJ-17)

## 5.02 Static zone objects appear (WLD-8)

**Goal:** NPCs, signs, doors, props stream on entry.

**Size:** M. **Depends on:** 4.14, 5.01

**Client messages:** MSG_NEWOBJECT, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] Real client: WC_Hub statues, kiosks and NPCs stand where retail has them, nothing at 0,0,0
- [ ] A Critical object zone leaves the loading screen
- [ ] Missing template logged once and skipped
- [ ] Log '<n> objects spawned in WizardCity/WC_Hub' matches eligible rows

### Detailed spec from WLD-8: Static zone objects appear

NPCs, signs, doors and props from the zone data appear for a player entering a zone, including objects the loading screen waits for.

**Deliverables**

- src/server/game/Entities/GameObject.cpp: spawn every zone_object row whose template has a RenderBehaviorTemplate when the Map is created, skipping sigil/minigame info classes
- Map::AddPlayer: send MSG_NEWOBJECT for each visible object to the entering player; Map::RemovePlayer: nothing for the leaver (the client tears down)
- Critical objects: templates whose adjective list holds 'Critical' go into LOGINCOMPLETE.CriticalObjects
- MSG_CLIENTZONED (service 53) handler marks the session in world, and object streaming waits for or follows it as the capture shows
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
- [ ] Server log: '<n> objects spawned in WizardCity/WC_Hub' matches the count of eligible zone_object rows

**Risks**

- That the client waits on CriticalObjects before dropping the loading screen is inferred from the reference, not confirmed
- Objects with m_spawnRequirements (quest-gated) are shown to everyone until WLD-19

## 5.03 Own movement tracking and persistence (WLD-9)

**Goal:** Server knows position; relog returns there.

**Size:** S. **Depends on:** 4.14, 4.16

**Client messages:** MSG_CLIENTMOVE, MSG_CLIENTMOVESTATE, MSG_JUMP

**Acceptance**

- [ ] Real client: walk to the Ravenwood gate, relog, spawn there within a few units
- [ ] Stale ZoneCounter leaves position unchanged
- [ ] 1000 moves give at most one DB write per interval
- [ ] Changing Player.SaveInterval applies from the next save

### Detailed spec from WLD-9: Own movement: position tracking and persistence

The server always knows where each player is, and a relog returns the player to the same spot.

**Deliverables**

- src/server/game/Handlers/MovementHandler.cpp: HandleClientMove (unpack, drop packets whose ZoneCounter differs from the session's), HandleClientMoveState, HandleJump
- src/server/game/Entities/Player position fields plus a dirty-save to characters DB every Player.SaveInterval, and a save on logout. Player.SaveInterval is a live setting applied from the next save.
- Session zone counter bumped on every zone change

**Client messages:** MSG_CLIENTMOVE, MSG_CLIENTMOVESTATE, MSG_JUMP

**Database tables**

- characters.characters (position columns)

**Acceptance**

- [ ] Real client: walk to the Ravenwood gate, log out, log back in, and spawn at that gate (within a few units)
- [ ] Unit: a MSG_CLIENTMOVE with a stale ZoneCounter leaves position unchanged
- [ ] Unit: 1000 moves between saves produce at most one DB write per save interval
- [ ] Unit: changing Player.SaveInterval applies from the next save without a restart

**Risks**

- What ZoneCounter means is inferred from field names and MSG_UPDATEZONECOUNTER's description ('Sent to client during an intra zone transfer'). The behavior reference ignores it.

## 5.04 Level, school and stat-config extractor (WIZ-2)

**Goal:** player_level_stats and magic_school_template.

**Size:** M. **Depends on:** 3.11, 2.07, 4.01, 4.15

**Acceptance**

- [ ] Synthetic MagicXPConfig fixture emits expected rows
- [ ] A row per (school, level) up to m_maxSchoolLevel; 16 magic_school_template rows
- [ ] GetInfo(Fire,1) matches the row
- [ ] `.reload player_level_stats` applies new base stats on the next level-up or login

### Detailed spec from WIZ-2: Level, school and stat-config extractor

The world database holds the per-school, per-level stat table and school definitions taken from the user's client, so the server can compute base health, mana, pip chance, training points and energy.

**Deliverables**

- src/tools/extractor/MagicXPExtractor: reads Root.wad MagicXPConfig.xml (BINd, root class MagicXPConfig), walks m_classInfo and m_levelInfo (MagicLevelInfo: m_level, m_xpToLevel, m_hitpoints, m_mana, m_gold, m_pipChance, m_trainingPoints, m_petEnergy, m_shadowPipRating, m_archmastery, pip conversion ratings) and m_maxSchoolLevel
- src/tools/extractor/MagicSchoolExtractor: MagicSchools/*.xml (16 MagicSchoolTemplate: m_schoolName, m_minLevel, m_schoolIndex)
- src/tools/extractor/StatConfigExtractor: WizStatisticEffectConfig.xml -> config rows
- data/sql/base/db_world: player_level_stats, magic_school_template, stat_effect_config schema files
- src/server/game/Entities/Player/PlayerLevelMgr (sPlayerLevelMgr) loaded at startup. `.reload player_level_stats` rebuilds it off to the side, validates it, and swaps it; a failure keeps the old table and reports every error.

**Data sources**

- Root.wad MagicXPConfig.xml (BINd, 358925 bytes, root MagicXPConfig)
- Root.wad MagicSchools/*.xml (16 files, root MagicSchoolTemplate)
- Root.wad WizStatisticEffectConfig.xml (root WizStatisticEffectConfig)
- A type dump generated by the project's own dumper (not committed)

**Database tables**

- world.player_level_stats
- world.magic_school_template
- world.stat_effect_config

**Acceptance**

- [ ] Unit test: the extractor, run on a synthetic BINd MagicXPConfig fixture built in the test, emits the expected rows
- [ ] Run against the user's install: player_level_stats has a row for each (school, level) up to m_maxSchoolLevel, and magic_school_template has 16 rows
- [ ] Unit test: sPlayerLevelMgr.GetInfo(Fire, 1) returns hitpoints, mana and training points matching the imported row
- [ ] Unit test: editing a player_level_stats row, then `.reload player_level_stats`, applies the new base stats on the next level-up or login without a restart; a row that fails validation keeps the old table

**Risks**

- Whether m_classInfo is keyed by school name and how its level list is laid out is unverified. The reference server groups levels by class name.

## 5.05 Character state and player-object stats (WIZ-3)

**Goal:** HUD and character sheet show DB values.

**Size:** M. **Depends on:** 5.04, 4.11, 3.16, 4.16

**Client messages:** MSG_WIZGAMESTATS

**Acceptance**

- [ ] New Fire level-1 base HP/mana/training points match player_level_stats(Fire,1)
- [ ] WizGameStats round-trips all transmitted fields
- [ ] Real client: gold=1234, level=5 shows on HUD, backpack and sheet

### Detailed spec from WIZ-3: Character state persistence and player-object stats

A logged-in character's level, school, XP, training points, gold, health, mana and potions load from the characters database into the player object, so the client HUD and character sheet show them.

**Deliverables**

- data/sql/updates/db_characters/<date>_01.sql: character_stats (level, xp, overflow_xp, school_id, secondary_school_id, training_points, gold, health, mana, potion_charge, potion_max, arena_points, level_locked)
- src/server/game/Entities/Player/PlayerStats: builds WizGameStats (m_baseHitpoints, m_baseMana, m_baseGoldPouch, m_currentHitpoints, m_currentMana, m_currentGold, m_powerPipBase, m_potionMax, m_potionCharge, m_schoolID, m_secondarySchool, m_shadowPipMax...) as WizClientObject.m_gameStats, and ClientMagicSchoolBehavior (m_schoolOfFocus, m_experiencePoints, m_level, m_trainingPoints, m_overflowXP, m_levelLocked, m_secondarySchool)
- Save on logout and every Player.SaveInterval, the live setting shared with WLD-9
- src/test/server/game/PlayerStatsTest.cpp

**Client messages:** MSG_WIZGAMESTATS

**Data sources**

- world.player_level_stats from WIZ-2

**Database tables**

- characters.character_stats

**Acceptance**

- [ ] Unit test: a new Fire character at level 1 gets base HP, mana and training points equal to player_level_stats(Fire,1)
- [ ] Unit test: the ObjectProperty round-trip of the built WizGameStats keeps all transmitted fields
- [ ] Real client: a character with gold=1234 and level=5 in the DB logs in. The HUD health and mana globes show DB values over base maximums, the backpack shows 1234 gold, and the character sheet shows level 5 and the correct school.

**Risks**

- Which WizGameStats fields the client needs non-zero to avoid UI glitches is unverified (for example m_baseGoldPouch=0 may block showing gold).
- When MSG_WIZGAMESTATS is needed (for mobs, per its description) versus embedding stats in LOGINCOMPLETE Data is unverified for players.

## 5.06 Return to character select (LOG-13)

**Goal:** Quit to select without credentials.

**Size:** M. **Depends on:** 2.14, 4.07, 5.03, 4.16

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_USER_VALIDATE, MSG_USER_VALIDATE_RSP, MSG_USER_ADMIT_IND

**Acceptance**

- [ ] PassKey3 from the stored key and this offer passes; previous offer, wrong key or other MachineID fails
- [ ] Real client: quit to select shows USER_VALIDATE -> VALIDATE_RSP Error=0 -> ADMIT_IND -> list, no password prompt
- [ ] DB online=0 with saved zone and position
- [ ] Changing Login.SessionKeyTTL applies from the next validate

### Detailed spec from LOG-13: Return to character select: MSG_QUERY_LOGOUT and MSG_USER_VALIDATE

A player in game can quit to character select and land on the list without re-entering credentials, with the character saved and marked offline.

**Deliverables**

- Gameserver: HandleQueryLogout sends MSG_CLIENT_DISCONNECT, saves the character (zone, position, logout time), sets online=0 and removes it from realm_online_character; also on MSG_CLIENT_DISCONNECT and socket loss
- Loginserver AuthHandler::HandleUserValidate: load account by UserID; check bans and lock; load the account_session for (account, MachineID) and reject if missing or expired; verify PassKey3 against this new connection's SessionID and offer seconds and milliseconds; on success send MSG_USER_VALIDATE_RSP{Error=0, Reason='', UserID, TimeStamp='', PayingUser=1, Flags=0, SupportID=''} then MSG_USER_ADMIT_IND{Status=1, PositionInQueue=0}; on failure send only VALIDATE_RSP{Error!=0} and close (never fall through to success)
- Session key lifetime: Login.SessionKeyTTL, a live setting applied from the next validate; extend expires on each successful validate; revoke on password change or ban

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_USER_VALIDATE, MSG_USER_VALIDATE_RSP, MSG_USER_ADMIT_IND

**Data sources**

- Sniffer capture lines 21852-21862

**Database tables**

- account_session
- characters
- realm_online_character
- account

**Acceptance**

- [ ] Unit: a PassKey3 computed from the stored key and this session's offer passes; one computed from the previous connection's offer, a wrong key or a different MachineID fails
- [ ] Real client: in game, choose to quit to character select; the client reconnects to the loginserver and the log shows USER_VALIDATE -> VALIDATE_RSP Error=0 -> ADMIT_IND -> character list, matching capture lines 21852-21860; the select screen appears with no password prompt
- [ ] Real client: selecting the same wizard again enters the world at the saved position
- [ ] DB: after quitting, characters.online=0 and the saved zone and position reflect where the player stood
- [ ] Unit: lowering Login.SessionKeyTTL makes the next validate of an older key fail without a restart

**Risks**

- The C->S game-side message that starts quit-to-select was not captured (the log only records S->C messages carrying blobs by default); MSG_QUERY_LOGOUT is inferred from the reference ClientService.
- The reference's validate handler sends success even after a failure. That bug must not be copied, and the tests must cover the failure path.

## 5.07 Binary type-registry cache (OBJ-15)

**Goal:** Fast startup, stale cache detected.

**Size:** S. **Depends on:** 3.03

**Acceptance**

- [ ] Binary registry equals JSON registry
- [ ] Load under 200 ms; edited hash rejected

### Detailed spec from OBJ-15: Binary type-registry cache

Server start does not re-parse the 13.8 MB JSON dump each time, and a revision mismatch is detected.

**Deliverables**

- src/tools/typeregbuild: converts the user's dump into a compact binary registry file in the user's data dir (never committed), stamped with the dump SHA-256 and client revision string
- TypeRegistry::LoadBinary with version and hash validation; falls back to JSON with a warning

**Acceptance**

- [ ] Client-gated test: the binary registry equals the JSON-loaded registry (every class, property and enum table compared)
- [ ] Load time from binary is under 200 ms
- [x] A deliberately stale cache (edited hash) is rejected with a clear message (TypeRegistryBinaryTest.RoundTripsTheRegistryAndRejectsAnEditedPayload and .FallsBackToJsonForMissingOrStaleCaches; a truncated, a bit-flipped and a random cache built from the pinned install's dump are each refused by name and the JSON dump keeps serving)

## 5.08 Installer (FND-22)

**Goal:** Clone to running servers with one script.

**Size:** S. **Depends on:** 2.08

**Acceptance**

- [ ] Clean Ubuntu and Windows reach 'ready' on all 3 apps
- [ ] `conf` twice never overwrites an edited .conf

### Detailed spec from FND-22: apps/installer: one-command build, config copy and DB setup

A new contributor goes from clone to running servers with one script on Windows or Linux.

**Deliverables**

- apps/installer/ambrose.sh and ambrose.ps1: `deps` (check CMake, compiler, Boost, OpenSSL, MySQL connector, and optionally install them), `compile` (preset build and install to env/dist), `conf` (copy *.conf.dist to *.conf if missing), `db` (run dbimport), `run <app>` (a restart-on-crash loop is optional)
- conf/dist/env.dist consumed by the installer (install prefix, build type, preset)
- doc/INSTALL.md with the Markdown header

**Acceptance**

- [ ] On a clean Ubuntu VM and a clean Windows machine, following doc/INSTALL.md with the installer yields running loginserver, gameserver and patchserver that log 'ready'
- [ ] Running `conf` twice never overwrites an edited .conf
- [ ] Real client: n/a (first visible behavior arrives with NET/LOG)

**Risks**

- Installing system packages differs per distro; limit support to a declared set

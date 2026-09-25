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

- src/server/game/Entities/ObjectTemplateMgr.h/.cpp (sObjectTemplateMgr), which already reads the player's template, grown to every template: TemplateManifest.xml loaded into an id->path map (137423 entries); GetTemplate(id) decoding lazily with an LRU cache from the archive each entry names, since 12807 entries name theirs as '|World|Part|path', meaning World-Part.wad; typed accessors through OBJ-10 views
- `.reload templates` rebuilds the manifest map off to the side, validates it, swaps it, and drops cache entries by generation. Objects keep the template snapshot they spawned with. A failure keeps the old map and reports every error.
- src/test/server/game/Entities/ObjectTemplateMgrTest.cpp using an in-memory fake archive

**Acceptance**

- [ ] Unit test with a fake archive: manifest lookup, cache hit/miss, missing id returns null without throwing
- [ ] Unit test with a fake archive: `.reload templates` with an edited manifest serves the new entry without a restart, an object spawned before the reload keeps its snapshot, and a manifest that fails validation keeps the old map and reports every error
- [ ] Client-gated test: GetTemplate(1) returns the PlayerObject template; GetTemplate(1652259) returns the hat; a missing manifest path is reported
- [ ] Client-gated benchmark: first access under 5 ms per template; decoding 10k random templates stays within the memory cap

**Risks**

- Templates are decoded from the user's install at run time and never stored in a database, as World threads, zone data and extracted tables in doc/ARCHITECTURE.md settles

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

## 5.03 Own movement tracking and persistence (WLD-9)

**Goal:** Server knows position; relog returns there.

**Size:** S. **Depends on:** 4.14, 4.16

**Client messages:** MSG_CLIENTMOVE, MSG_CLIENTMOVESTATE, MSG_JUMP

**Acceptance**

- [ ] Real client: walk to the Ravenwood gate, relog, spawn there within a few units
- [ ] Stale ZoneCounter leaves position unchanged
- [ ] 1000 moves write nothing until the wizard leaves, then write once
- [ ] A zone change writes the position

### Detailed spec from WLD-9: Own movement: position tracking and persistence

The server always knows where each player is, and a relog returns the player to the same spot.

**Deliverables**

- src/server/game/Handlers/MovementHandler.cpp: HandleClientMove (unpack, drop packets whose ZoneCounter differs from the session's), HandleClientMoveState, HandleJump
- src/server/game/Entities/Player position fields, written to the characters DB when the wizard leaves the world and when it changes zone, never on a timer, as Saving in doc/ARCHITECTURE.md settles at the maintainer's direction
- Session zone counter bumped on every zone change

**Client messages:** MSG_CLIENTMOVE, MSG_CLIENTMOVESTATE, MSG_JUMP

**Database tables**

- characters.characters (position columns)

**Acceptance**

- [ ] Real client: walk to the Ravenwood gate, log out, log back in, and spawn at that gate (within a few units)
- [ ] Unit: a MSG_CLIENTMOVE with a stale ZoneCounter leaves position unchanged
- [ ] Unit: 1000 moves write no position until the wizard leaves the world, and leaving writes it once
- [ ] Unit: a zone change writes the position

**Risks**

- What ZoneCounter means is inferred from field names and MSG_UPDATEZONECOUNTER's description ('Sent to client during an intra zone transfer'). The behavior reference ignores it.

## 5.04 Level, school and stat-config extractor (WIZ-2)

**Goal:** player_level_stats and magic_school_template.

**Size:** M. **Depends on:** 3.11, 2.07, 4.01, 4.15

**Acceptance**

- [x] Synthetic MagicXPConfig fixture emits expected rows
- [x] A row per (school, level) up to m_maxSchoolLevel; 16 magic_school_template rows
- [x] GetInfo(Fire,1) matches the row
- [x] `.reload player_level_stats` applies new base stats on the next level-up or login

### Detailed spec from WIZ-2: Level, school and stat-config extractor

The world database holds the per-school, per-level stat table and school definitions taken from the user's client, so the server can compute base health, mana, pip chance, training points and energy.

**Deliverables**

- src/server/shared/ClientData/LevelExtractor and LevelViews, which the `extractor levels` command and the game server's first start both run, as Automatic setup settles for the name tables: reads Root.wad MagicXPConfig.xml (BINd, root class MagicXPConfig), walks m_classInfo and m_levelInfo (MagicLevelInfo: m_level, m_xpToLevel, m_hitpoints, m_mana, m_gold, m_pipChance, m_trainingPoints, m_craftingSlots, m_petEnergy, m_shadowPipRating, m_archmastery, pip conversion ratings), m_maxSchoolLevel and the rest of the class's settings, m_encounterXPFactors and m_levelsConfig; MagicSchools/*.xml (16 MagicSchoolTemplate: m_schoolName, m_minLevel, m_schoolIndex, m_secondarySchoolBadgeList); and WizStatisticEffectConfig.xml's settings and bands
- src/server/shared/Characters/PlayerLevels and StatEffects: the validating sets the extractor checks before it writes and the manager checks when it loads
- src/server/database/Extraction/LevelScript: the world SQL that replaces the nine tables in one transaction
- data/sql/updates/db_world/2026_09_25_01.sql: player_level_stats, magic_school_template, magic_school_badge, magic_xp_config, magic_xp_encounter_factor, mob_rank_level, stat_effect_config, stat_crit_block_band and stat_pip_conversion_band, in an update rather than data/sql/base because Database updates settles that every change is an update
- src/server/game/Entities/Player/PlayerLevelMgr (sPlayerLevelMgr) loaded at startup. `.reload player_level_stats` and `.reload stat_effect_config` rebuild a set off to the side, validate it, and swap it; a failure keeps the old set and reports every error.

**Data sources**

- Root.wad MagicXPConfig.xml (BINd, 358925 bytes, root MagicXPConfig)
- Root.wad MagicSchools/*.xml (16 files, root MagicSchoolTemplate)
- Root.wad WizStatisticEffectConfig.xml (root WizStatisticEffectConfig)
- A type dump generated by the project's own dumper (not committed)

**Database tables**

- world.player_level_stats
- world.magic_school_template, world.magic_school_badge
- world.magic_xp_config, world.magic_xp_encounter_factor, world.mob_rank_level
- world.stat_effect_config, world.stat_crit_block_band, world.stat_pip_conversion_band

**Acceptance**

- [x] Unit test: the extractor, run on a synthetic BINd MagicXPConfig fixture built in the test, emits the expected rows. `LevelExtractorTest.EachSchoolTakesItsOwnValuesAndTheSharedTableOtherwise` encodes MagicXPConfig, two school templates and a WizStatisticEffectConfig through a type dump it writes: Fire's level 1 row takes its own 415 hitpoints and the shared table's experience, mana, gold and energy, Moon is named without rows, and the settings, factors, mob ranks, badges and bands come out in order; the script test checks the nine tables' SQL and the broken-input test each refusal
- [x] Run against the user's install: player_level_stats has a row for each (school, level) up to m_maxSchoolLevel, and magic_school_template has 16 rows. On r806919 `extractor --dry-run levels` prints 1267 rows for Fire, Ice, Storm, Life, Myth, Death and Balance, levels 0 to 180, and 16 magic_school_template rows, as `LevelExtractorClientTest` and the Extractor CTest check; the development game server, started on a world database without the tables, extracted and loaded them itself ('Extracted 1267 level rows for 7 schools, 16 magic schools and 39 stat settings', then 'Loaded 16 magic schools with level tables for 7 of them up to level 180, and 39 stat settings with 121 band values'), and level 1 reads 415, 500, 400, 425, 460, 450 and 480 hitpoints for Fire, Ice, Storm, Myth, Life, Death and Balance
- [x] Unit test: sPlayerLevelMgr.GetInfo(Fire, 1) returns hitpoints, mana and training points matching the imported row. `PlayerLevelMgrTest.RowsLoadAndAReloadAppliesEditsOrKeepsTheServingSet` against MariaDB loads the rows it writes and reads Fire's level 1 back with 415 hitpoints, 15 mana, 2 training points and every other column, and `LevelExtractorClientTest.TheRowsFillAWorldDatabaseTheManagerLoads` does the same with the install's own rows
- [x] Unit test: editing a player_level_stats row, then `.reload player_level_stats`, applies the new base stats on the next level-up or login without a restart; a row that fails validation keeps the old table. `PlayerLevelMgrTest.AReloadReachesTheNextWizardToEnterTheWorld` against MariaDB builds a Fire wizard's stats the way entering the world does, edits the level 1 row to 450 hitpoints, 16 mana and 3 training points, reloads the player_level_stats target, and the next wizard's stats carry the new values at full health while the stats built before keep 415; `PlayerLevelMgrTest.RowsLoadAndAReloadAppliesEditsOrKeepsTheServingSet` shows a gap, a badge out of order and a school id that is not its name's hash refused with the serving set kept

**Risks**

- Settled: m_classInfo holds one ClassInfo per magic school, named by m_className, and the client keys each by the hash of that name. Only the seven wizard schools list levels, and they set only their hitpoints and their own pip conversion rating, so each school's row takes the shared table's value wherever its own table leaves one unset, as Levels and stats in doc/ARCHITECTURE.md records with the client code that shows it.

## 5.05 Character state and player-object stats (WIZ-3)

**Goal:** HUD and character sheet show DB values.

**Size:** M. **Depends on:** 5.04, 4.11, 3.16, 4.16

**Client messages:** MSG_WIZGAMESTATS

**Acceptance**

- [x] New Fire level-1 base HP/mana/training points match player_level_stats(Fire,1)
- [x] WizGameStats round-trips all transmitted fields
- [x] Real client: gold=1234, level=5 shows on HUD, backpack and sheet

### Detailed spec from WIZ-3: Character state persistence and player-object stats

A logged-in character's level, school, XP, training points, gold, health, mana and potions load from the characters database into the player object, so the client HUD and character sheet show them.

**Deliverables**

- data/sql/updates/db_characters/2026_09_25_01.sql: character_stats (overflow_xp, secondary_school_id, training_points, gold, health, mana, potion_charge, potion_max, arena_points, level_locked, revision). Level, experience and school stay in characters, where the character list already reads them, so each has one home; health and mana are NULL when full.
- src/server/game/Entities/Player/PlayerStats: builds WizGameStats (m_baseHitpoints, m_baseMana, m_baseGoldPouch, m_energyMax, m_currentHitpoints, m_currentMana, m_currentGold, m_currentArenaPoints, m_powerPipBase, m_pipConversionBaseAllSchools, m_shadowPipRating, m_archmasteryBase, m_potionMax, m_potionCharge, m_referenceLevel, m_schoolID, m_secondarySchool, m_shadowPipMax) as WizClientObject.m_gameStats, and ClientMagicSchoolBehavior (m_schoolOfFocus, m_experiencePoints, m_level, m_trainingPoints, m_overflowXP, m_levelLocked, m_secondarySchool)
- GameSession reads the stats row through the wizard, so a failed read never passes for a wizard with no row, and refuses a wizard whose school has no level table
- Written as a stat changes and when the wizard leaves the world, never on a timer, each write carrying the next revision so writes that land out of order leave the newest, as Saving in doc/ARCHITECTURE.md settles at the maintainer's direction
- src/test/server/game/Entities/Player/PlayerStatsTest.cpp, src/test/client/PlayerStatsClientTest.cpp, and the client driver's wizard-stats.json, whose seeded wizard now carries experience and a stats row

**Client messages:** MSG_WIZGAMESTATS

**Data sources**

- world.player_level_stats from WIZ-2

**Database tables**

- characters.character_stats

**Acceptance**

- [x] Unit test: a new Fire character at level 1 gets base HP, mana and training points equal to player_level_stats(Fire,1). `PlayerStatsTest.ANewFireWizardAtLevelOneTakesItsRowsBaseValuesAtFullHealthAndMana`: 415 hitpoints, its row's mana and training points, at full health and mana, saved back as full
- [x] Unit test: the ObjectProperty round-trip of the built WizGameStats keeps all transmitted fields. `PlayerStatsClientTest.TheClientsOwnStatsClassesCarryEveryValueThroughTheTransmitForm` fills r806919's own WizGameStats and ClientMagicSchoolBehavior from a level 5 wizard's stats and reads every transmitted property back unchanged; `PlayerStatsTest.TheGameStatsAndSchoolBehaviorCarryEveryValueAndReadBackThroughTheTransmitForm` does the same on classes the test lays out
- [x] Real client: a character with gold=1234 and level=5 in the DB logs in. The HUD health and mana globes show DB values over base maximums, the backpack shows 1234 gold, and the character sheet shows level 5 and the correct school. Earned on 2026-09-25 by the client driver's wizard-stats.json on r806919 (run 20260925-192225): a level 5 Fire wizard seeded with 900 experience, 1234 gold, 300 health and 10 mana entered Ravenwood, the game server logged 'level 5 with 300 of 503 health and 10 of 24 mana', the HUD globes read 300 and 10 partly drained, and the character stats page opened with C read Apprentice (Level 5), Pyromancer, Health 300/503, Mana 10/24, Experience 195/495, Gold 1,234/300,000 and Energy 0/42. The backpack opened with B shows its item count and no gold in this client revision, so the gold is read on the character stats page, where the client shows it

**Risks**

- Settled: the client shows each value against the maximum it computes itself, health as m_baseHitpoints plus m_bonusHitpoints (WizGameStats::CalcTotalHitpoints), mana as base plus bonus less its mana reduction (CalcMaxMana), energy as m_energyMax plus m_bonusEnergy (CalcMaxEnergy), and gold against m_baseGoldPouch, so the base values from the level tables are what it needs.
- Settled: the client hands MSG_WIZGAMESTATS to ClientDuelManager, which reads a duel participant's stats, so a player's own stats travel in its object in MSG_LOGINCOMPLETE.
- Current energy is not sent yet, so the character page reads 0 of the maximum; it belongs with the vitals in 8.01.

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
- [x] `conf` twice never overwrites an edited .conf (`apps/installer/tests/test_installer.py`, which builds a prefix of templates, runs conf, edits a .conf and runs conf again, against both scripts)

### Detailed spec from FND-22: apps/installer: one-command build, config copy and DB setup

A new contributor goes from clone to running servers with one script on Windows or Linux.

**Deliverables**

- apps/installer/ambrose.sh and ambrose.ps1: `deps` (check CMake, compiler, Boost, OpenSSL, MySQL connector, and optionally install them), `compile` (preset build and install to env/dist), `conf` (copy *.conf.dist to *.conf if missing), `db` (run dbimport), `run <app>` (a restart-on-crash loop is optional)
- conf/dist/env.dist consumed by the installer (install prefix, build type, preset)
- doc/INSTALL.md with the Markdown header

**Acceptance**

- [ ] On a clean Ubuntu VM and a clean Windows machine, following doc/INSTALL.md with the installer yields running loginserver, gameserver and patchserver that log 'ready'
- [x] Running `conf` twice never overwrites an edited .conf (`apps/installer/tests/test_installer.py`, which builds a prefix of templates, runs conf, edits a .conf and runs conf again, against both scripts)
- [ ] Real client: n/a (first visible behavior arrives with NET/LOG)

**Risks**

- Installing system packages differs per distro; limit support to a declared set

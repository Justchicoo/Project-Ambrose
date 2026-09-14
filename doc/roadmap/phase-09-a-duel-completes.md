<!-- Project Ambrose by Imjustchico: Roadmap phase 9, A duel completes. -->

# Phase 9: A duel completes

**Done when:** A wizard walks into a wandering mob and is pulled onto a sigil. They draw cards, cast Fire Cat, pay pips, and either win (victory sequence, respawn) or lose (sent to safety), and can flee.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 9.01 | WizCombat protocol surface and stubs (CMB-1) | S | 2.09, 1.16 |
| 9.02 | SigilMgr (CMB-2 sigils) | S | 5.01, 4.15 |
| 9.03 | Shared GameEffect framework (new; breaks the WIZ-13/CMB cycle) | M | 6.01 |
| 9.04 | Spawners and runtime spawn/despawn (WLD-18, without paths; QST-4 SpawnExtractor) | M | 6.13, 6.16, 6.10, 4.15, 4.16 |
| 9.05 | Duel instance and sigil placement (CMB-3) | M | 9.01, 9.02, 9.04, 8.04, 5.05 |
| 9.06 | Round state machine, pass-only (CMB-4) | M | 9.05, 4.16 |
| 9.07 | Play deck and hand (CMB-5) | S | 9.06, 8.11 |
| 9.08 | Single-target damage cast (CMB-6) | M | 9.07 |
| 9.09 | Victory and teardown (CMB-7) | S | 9.08, 9.03 |
| 9.10 | Aggro start and minimal creature AI (CMB-8 part 1) | M | 9.09 |
| 9.11 | Player defeat and HP persistence (CMB-8 part 2) | M | 9.10, 8.01 |
| 9.12 | Flee, disconnect, logout in combat (CMB-9) | S | 9.11, 6.08 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** Phase 9 outcome and 9.10 require a 'wandering mob', but path-walking NPCs (10.14) come after phase 9 and 9.10 does not depend on them. Creature decks (11.14) also come after 9.10, although 9.10 says the mob 'casts its attack'. An interim hard-coded or authored creature spell source is needed, or 9.10 has to drop 'wandering'.
- **Ordering.** 9.04 spawns creatures but does not depend on 7.01 (object_template extractor). Creature names, behaviors and templates come from there, and 7.01 is not reachable through 9.04's dependency chain.
- **Missing work.** Game event / holiday scheduler and daily reset (AzerothCore game_event equivalent). It gates HalloweenSpawner1's ReqGlobalRegistryValue (9.04), daily assignments (14.16), daily PvP and holiday data. Global registry storage is also missing.
- **Oversized.** 9.08 single-target damage cast with cinematic parity (M). It bundles COMBATACTIONS encoding, m_effectChosen bits, fizzle, pips and hit rolls, and it is also where the open 'client simulates results' question must be settled.
- **Correction.** The 9.01/decision-list note that the XML says MoveType '0 pass, 1 attack, 2 enchant, 3 flee' is misquoted. WizCombatMessages.xml says '0 for pass, 1 for attack, 2 for cast on a spell, 3 for flee'.

## 9.01 WizCombat protocol surface and stubs (CMB-1)

**Goal:** All 36 service-51 messages registered.

**Size:** S. **Depends on:** 2.09, 1.16

**Client messages:** MSG_DUEL, MSG_ENDDUEL, MSG_COMBATPHASE, MSG_COMBATMOVE, MSG_COMBATHAND, MSG_COMBATACTIONS, MSG_COMBATADD, MSG_COMBATREMOVE, MSG_COMBATMOVESELECTION, MSG_COMBATDRAW, MSG_COMBATPIPS, MSG_COMBATHEALTH, MSG_COMBATSTATS, MSG_COMBATVICTORY, MSG_COMBATMATCHRESULT, MSG_COMBATFLEE, MSG_COMBATUPFIRST, MSG_COMBATAFK, MSG_SHOWCOMBATUI, MSG_SETPLANNINGPHASETIMER, MSG_UPDATEDUELTIMER

**Acceptance**

- [ ] Ordinals equal the name-sorted index: MSG_ALLOWLEAVEPVP=1, MSG_COMBATACTIONS=2, last = MSG_UPDATEDUELTIMER (corrected; the plan said MSG_SETSTATUS)
- [ ] MSG_COMBATMOVE and MSG_COMBATPHASEFORSPECTATORS byte round-trip
- [ ] MSG_COMBATMOVE rejected when not in world

### Detailed spec from CMB-1: WizCombat protocol surface and dispatch stubs

All 36 service-51 messages and the combat WIZARD/GAME messages encode and decode correctly and are registered in the dispatch table, so later milestones only write handler bodies.

**Deliverables**

- src/server/shared/Messages: generated or declared WizCombat service 51 message structs, ids assigned by sorting names in ordinal order (MSG_ALLOWLEAVEPVP=1, MSG_COMBATACTIONS=2, ...)
- src/server/game/Handlers/CombatHandler.cpp: Session::HandleCombatMove, HandleCombatDraw, HandleCombatAFK, HandleCombatVictory, HandlePetWillCast, HandleDismissSummon, HandleCombatCheat stubs, registered with required state 'in world'
- src/test/server/shared/Messages/WizCombatMessagesTest.cpp

**Client messages:** MSG_DUEL, MSG_ENDDUEL, MSG_ALLOWLEAVEPVP, MSG_COMBATPHASE, MSG_COMBATMOVE, MSG_COMBATHAND, MSG_COMBATACTIONS, MSG_COMBATADD, MSG_COMBATREMOVE, MSG_COMBATMOVESELECTION, MSG_COMBATDRAW, MSG_COMBATPIPS, MSG_COMBATHEALTH, MSG_COMBATSTATS, MSG_COMBATVICTORY, MSG_COMBATMATCHRESULT, MSG_COMBATREVEALHANGING, MSG_COMBATFLEE, MSG_COMBATCHEAT, MSG_COMBATUPFIRST, MSG_UPDATECOMBATPARTICIPANT, MSG_COMBATLOADED, MSG_COMBATPAUSED, MSG_COMBATPHASEFORSPECTATORS, MSG_UPDATEDUELTIMER, MSG_COMBATAFK, MSG_SHOWCOMBATUI, MSG_SETPLANNINGPHASETIMER, MSG_SHOWPETCARD, MSG_PETWILLCAST, MSG_SIGILSPELL, MSG_SETST, MSG_SETST2, MSG_DISMISS_SUMMON, MSG_SETDUELTIMER, MSG_SETSTATUS

**Data sources**

- Root.wad Messages/WizCombatMessages.xml (read at build or run time, not committed)

**Acceptance**

- [ ] Unit test: the ordinal of every service-51 message equals its index in the name-sorted list; spot checks MSG_ALLOWLEAVEPVP=1, MSG_COMBATACTIONS=2, MSG_SETSTATUS=last
- [ ] Unit test: byte round-trip of MSG_COMBATMOVE (MoveType UBYT, SpellSelection UBYT, SpellTarget UINT, TimeLeft INT, ShadowPactTarget INT, SelectedTieredSpellID INT) and MSG_COMBATPHASEFORSPECTATORS (8 STR names), with NOXFER fields skipped
- [ ] Unit test: dispatch table rejects MSG_COMBATMOVE from a session not in world
- [ ] Real client: nothing visible yet; the server log shows a decoded MSG_COMBATAFK or MSG_COMBATMOVE when forced from a test harness

**Risks**

- Ordinal sorting is inferred from the reference codec generator (Imcodec XmlMessageReader.cs sorts with string.CompareOrdinal when no _MsgOrder exists); confirm against a live capture

## 9.02 SigilMgr (CMB-2 sigils)

**Goal:** CombatSigilTemplate and PvPCombatSigilTemplate loaded.

**Size:** S. **Depends on:** 5.01, 4.15

**Acceptance**

- [ ] Sigils/CombatSigil8Actor.xml has 8 SigilSubCircle (4 Monster, 4 Player) with non-zero PvE limit fields
- [ ] `.sigil reload` with a broken sigil file keeps the old templates and lists every error

### Detailed spec from CMB-2: SpellMgr and SigilMgr: load spell and sigil templates from the client

The game server holds every SpellTemplate and CombatSigilTemplate in memory, looked up by template id and name.

**Deliverables**

- src/server/game/Spells/SpellMgr.{h,cpp} (sSpellMgr): loads Spells/**/*.xml from the user's Root.wad and indexes by the TemplateManifest.xml template id and by m_name
- src/server/game/Combat/SigilMgr.{h,cpp} (sSigilMgr): loads Sigils/*.xml CombatSigilTemplate and PvPCombatSigilTemplate
- src/server/scripts/Commands/cs_spell.cpp: .spell info <name|id>, .spell reload; cs_sigil.cpp: .sigil reload
- Both managers are 4.15 reload targets: a reload builds the new template set off to the side, validates it, and swaps it atomically, a failure keeps the old set and reports every error, and duels in progress keep the snapshot they started with
- src/test/server/game/Spells/SpellMgrTest.cpp (skipped when no client path is configured)
- conf/dist gameserver.conf.dist: ClientDataDir

**Data sources**

- Root.wad Spells/ (18173 BINd)
- Root.wad Sigils/ (25)
- Root.wad TemplateManifest.xml
- Generated type dump for r806919 (run locally, never committed)

**Acceptance**

- [ ] Test (with a client install): 18173 Spells entries decode with 0 failures, or failures are listed by class name and fixed by teaching the registry
- [ ] Test: 'Fire Cat - Amulet' resolves with an effect of type kDamage, damage type Fire, target kEnemySingle
- [ ] Test: Sigils/CombatSigil8Actor.xml has 8 SigilSubCircle entries (4 MonsterCircle, 4 PlayerCircle) and non-zero PvE damage/resist limit fields
- [ ] GM in a real client types .spell info Fire Cat and gets chat output with school, pip rank, accuracy and effects
- [ ] Test: a `.spell reload` or `.sigil reload` that hits a decode failure keeps the old templates serving and lists every error

**Risks**

- The Spells BINd entries I sampled are raw ObjectProperty after the header, not zlib at offset 13 as the task notes say; the reader must handle both
- Polymorphic effect lists (RandomSpellEffect, ConditionalSpellEffect with RequirementList) need every subclass registered

## 9.03 Shared GameEffect framework (new; breaks the WIZ-13/CMB cycle)

**Goal:** ADDEFFECT/REMOVEEFFECT with internal id allocation.

**Size:** M. **Depends on:** 6.01

**Client messages:** MSG_ADDEFFECT, MSG_REMOVEEFFECT

**Acceptance**

- [ ] Internal ids are unique per object and released on removal
- [ ] Real client: a GM-applied test effect shows and clears for self and viewers

### Detailed spec from WIZ-13: Equipment stat effects and set bonuses

Equipped gear changes the wizard's maximum health, damage, resist, crit, block, pips and other stats the way the client expects, and those changes show on the character sheet.

**Deliverables**

- src/server/game/Entities/Player/StatCalculator: base stats from player_level_stats, plus each equipped item's m_equipEffects (WizStatisticEffect fields: m_hitPointBonus, m_manaBonus, m_damageBonusPercent, m_damageReducePercent, m_accuracyBonusPercent, m_criticalHitRating, m_blockRating, m_powerPipBonusPercent, m_pipConversionRating, m_archmastery, per-school fields via effect category), plus ItemSetBonusTemplate tiers by equipped count
- GAME ADDEFFECT/REMOVEEFFECT sent on equip, unequip and login; UPDATEHEALTH/UPDATEMANA with the new maximums
- Rating-to-percent conversions from stat_effect_config (critical, block, pip conversion) for derived WizGameStats fields
- src/test/server/game/StatCalculatorTest.cpp

**Client messages:** GAME MSG_ADDEFFECT, GAME MSG_REMOVEEFFECT, MSG_UPDATEHEALTH, MSG_UPDATEMANA

**Data sources**

- world.item_template_effect
- world.item_set_bonus
- world.stat_effect_config
- Root.wad GameEffectRuleData/*.xml (134 WizardStatTable) for rating curves
- Root.wad GameEffectData/CanonicalStatEffects.xml

**Acceptance**

- [ ] Unit test: equipping a +100 HP hat raises m_baseHitpoints plus bonus by 100 and current health is clamped correctly on unequip
- [ ] Unit test: a 3-piece set bonus applies only when 3 set items are equipped
- [ ] Real client: equipping a hat with +5% Fire damage and +40 health makes the character sheet Stats tab show the new damage percent and the health globe maximum go up by 40. Unequipping reverts both.

**Risks**

- The exact rating-to-percent curves (WizardStatTable) and level thresholds need reverse engineering. Small errors show as mismatched character-sheet numbers.
- The effect framework and m_internalID allocation are shared with CMB. Agree on ownership.

## 9.04 Spawners and runtime spawn/despawn (WLD-18, without paths; QST-4 SpawnExtractor)

**Goal:** Creatures spawn, despawn, respawn.

**Size:** M. **Depends on:** 6.13, 6.16, 6.10, 4.15, 4.16

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_DELETEOBJECT, MSG_REMOVEOBJECT

**Acceptance**

- [ ] WC_Ravenwood yields 5 SpawnObjects incl. SpawnPoint_Wood_01 SNT_RANDOM_UNIQUE; WC_Hub HalloweenSpawner1 has ReqGlobalRegistryValue
- [ ] Respawn after respawnTime, never above count
- [ ] Changing Rate.Respawn affects the next despawn without a restart; a failed `.reload zone_spawner` keeps the old spawners
- [ ] Real client: '.npc spawn <id>' appears for nearby clients; '.npc delete' plays despawn effect

### Detailed spec from WLD-18: Spawners and runtime spawn/despawn

Zone spawners create, despawn and respawn non-combat creatures and objects on timers and on trigger results.

**Deliverables**

- zone_extractor: decode spawnData.xml (SpawnManager, 0x3752F969; SpawnObjectInfo with m_pathID, m_startNode, m_kStartNodeType, m_uniqueLoc) into world.zone_spawner and world.zone_spawner_entry
- src/server/game/Zones/SpawnerMgr: respawn timers scaled by the live setting Rate.Respawn, max counts, ResSpawn/ResDespawn result handlers
- `.reload zone_spawner` as a 4.15 target: rebuilds zone_spawner and zone_spawner_entry off to the side, validates them, swaps them, and reschedules live timers; a validation failure keeps the old spawners and reports every error
- Despawn with effect: MSG_DELETEOBJECT carrying a wrapped DespawnInfo (m_killer, m_despawnEffect); plain despawn: MSG_REMOVEOBJECT
- '.npc spawn <templateId>' and '.npc delete' in scripts/Commands/cs_npc.cpp (temporary spawns, not saved)

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_DELETEOBJECT, MSG_REMOVEOBJECT

**Data sources**

- <zone>.wad/spawnData.xml
- Type dump: SpawnManager, SpawnObjectInfo, SpawnPointTemplate, DespawnInfo

**Database tables**

- world.zone_spawner
- world.zone_spawner_entry

**Acceptance**

- [ ] Unit: after a despawn, a spawner respawns after respawnTime and never goes over its count
- [ ] Unit: `.settings set Rate.Respawn 0.5` halves the respawn delay from the next despawn without a restart
- [ ] Unit: `.reload zone_spawner` with a raised count spawns the difference, and a reload with a broken row keeps the old spawners serving
- [ ] Real client: '.npc spawn <id>' makes the creature appear in front of the GM for all nearby clients; '.npc delete' removes it with its despawn effect
- [ ] Real client: a trigger whose result is ResSpawn makes its creature appear when the event fires

**Risks**

- Combat creature spawns and their sigils belong to CMB; this milestone must expose a hook, not implement combat
- clientSpawnData.xml is a client-side-only spawn list (empty in WC_Hub). It must not be spawned by the server.

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

## 9.05 Duel instance and sigil placement (CMB-3)

**Goal:** GM duel with both sides on circles.

**Size:** M. **Depends on:** 9.01, 9.02, 9.04, 8.04, 5.05

**Client messages:** MSG_AGGRO, MSG_ENTERSTATE, MSG_DUEL, MSG_COMBATADD, MSG_COMBATMATCHRESULT, MSG_COMBATPHASE, MSG_ENDDUEL

**Acceptance**

- [ ] Sub-circle positions match hand-computed coordinates
- [ ] WizardClientDuelBehavior and CombatParticipant round-trip
- [ ] Real client: .duel start shows ring, circles and nameplates; .duel end frees the player

### Detailed spec from CMB-3: Duel instance and sigil placement (visual only)

A GM can start a duel at a combat sigil, and the real client shows the player and a creature standing on their circles in duel stance.

**Deliverables**

- src/server/game/Combat/Duel.{h,cpp}: owns Duel state (duel id = sigil GID, round, phase, first team, sub-circles)
- src/server/game/Combat/DuelMgr.{h,cpp} (sDuelMgr): duels per zone, lookup by participant
- src/server/game/Combat/SubCircle.{h,cpp}: world position and yaw from SigilSubCircle rotation/radius plus sigil location and yaw
- src/server/game/Combat/CombatParticipant builder for player and creature
- src/server/scripts/Commands/cs_duel.cpp: .duel start <creatureTemplateId>, .duel end

**Client messages:** MSG_AGGRO, MSG_ENTERSTATE, MSG_DUEL, MSG_COMBATADD, MSG_COMBATMATCHRESULT, MSG_COMBATPHASE, MSG_ENDDUEL

**Data sources**

- Sigils/*.xml
- Zone data: CombatSigilObjectInfo placements (from the WLD zone extractor)
- ObjectData mob template: NPCBehaviorTemplate m_nStartingHealth/m_nLevel/m_schoolOfFocus

**Acceptance**

- [ ] Unit test: sub-circle positions for a sigil at a known location and yaw match hand-computed coordinates (4 monster and 4 player slots mirrored)
- [ ] Unit test: a serialized WizardClientDuelBehavior and CombatParticipant decode back to identical objects
- [ ] Real client: after .duel start, the player and creature run to opposite circles (MSG_AGGRO, MSG_ENTERSTATE 'Sigil' then 'Stationary'), the sigil ring renders (MSG_DUEL), and health/name plates appear over both (MSG_COMBATADD)
- [ ] Real client: .duel end sends MSG_COMBATMATCHRESULT, MSG_COMBATPHASE kPhase_Ended and MSG_ENDDUEL; the ring disappears and the player can walk again

**Risks**

- Correct yaw convention (radians vs byte orientation, clockwise client) needs empirical tuning; the reference applies a magic 1.58 rad correction
- The CombatParticipant m_templateID values the reference hardcodes 'from live' are unexplained; verify what the client needs

## 9.06 Round state machine, pass-only (CMB-4)

**Goal:** PrePlanning/Planning/Execution/Resolution with timer.

**Size:** M. **Depends on:** 9.05, 4.16

**Client messages:** MSG_COMBATPHASE, MSG_COMBATUPFIRST, MSG_SHOWCOMBATUI, MSG_SETPLANNINGPHASETIMER, MSG_COMBATSTATS, MSG_COMBATPIPS, MSG_COMBATHEALTH, MSG_COMBATMOVE, MSG_COMBATMOVESELECTION, MSG_COMBATACTIONS

**Acceptance**

- [ ] 3 passing rounds give phases 1,2,4,5 repeated, pips 1..3
- [ ] Execution starts within 1 tick of the last move
- [ ] Changing Combat.PlanningSeconds mid-duel applies from the next round without a restart
- [ ] Real client: up-first arrow, HUD, countdown, pips; Pass advances

### Detailed spec from CMB-4: Round state machine with pass-only turns

A duel loops through PrePlanning, Planning, Execution and Resolution with a working planning timer, and players can pass.

**Deliverables**

- src/server/game/Combat/DuelPhase machine driven by the gameserver world update tick, not threads
- Planning timer and early end when every living participant has queued a move
- MSG_COMBATMOVE pass handling plus 'change mind' re-selection
- Generic pip gain per round per living participant from Combat.PipsPerRound (default 1), capped at Combat.MaxPips (default 7)
- src/test/server/game/Combat/DuelSimHarness: headless duel with injectable RNG and scripted participants
- Live settings Combat.PlanningSeconds, Combat.PrePlanningSeconds, Combat.StartGraceSeconds, Combat.PipsPerRound and Combat.MaxPips, with defaults in gameserver.conf.dist; a change applies from the next round, and the round in progress keeps its values

**Client messages:** MSG_COMBATPHASE, MSG_COMBATUPFIRST, MSG_SHOWCOMBATUI, MSG_SETPLANNINGPHASETIMER, MSG_COMBATSTATS, MSG_COMBATPIPS, MSG_COMBATHEALTH, MSG_COMBATMOVE, MSG_COMBATMOVESELECTION, MSG_COMBATACTIONS

**Acceptance**

- [ ] Sim test: 3 rounds of both sides passing produce phases 1,2,4,5 repeated, round number 1..3, pips 1..3
- [ ] Sim test: when all participants queue before the timer ends, Execution starts within 1 tick of the last move
- [ ] Sim test: `.settings set Combat.PlanningSeconds 10` during round 1 leaves round 1's timer unchanged and gives round 2 a 10 s planning timer, without a restart
- [ ] Real client: after the grace delay the up-first arrow points at the right team (MSG_COMBATUPFIRST, MSG_COMBATPHASE with UpFirstData), the card HUD and countdown appear (MSG_SHOWCOMBATUI, MSG_SETPLANNINGPHASETIMER), the pip count rises by one each round (MSG_COMBATPIPS), and health globes show the right values (MSG_COMBATHEALTH, MSG_COMBATSTATS)
- [ ] Real client: clicking Pass shows the pass marker on the teammate display (MSG_COMBATMOVESELECTION echoed to the player team) and the round advances; letting the timer run out also advances

**Risks**

- MoveType meaning conflicts: the XML description says 0 pass, 1 attack, 2 cast on a spell, 3 flee, but the reference enum is Attack 0, Flee 1, Discard 2, Pass 3, ChangeMind 4. Must be settled by capture or client RE before this milestone
- The reference only fills MSG_COMBATPHASE Data for phase 1; other phases may need data too

## 9.07 Play deck and hand (CMB-5)

**Goal:** Shuffled hand from the equipped deck.

**Size:** S. **Depends on:** 9.06, 8.11

**Client messages:** MSG_COMBATHAND, MSG_COMBATMOVE

**Acceptance**

- [ ] Seeded 20-card deck draws deterministic 7; discard 2 refills; deck+graveyard stays 20
- [ ] Real client: 7 correct cards, deck counter, discard leaves slot empty

### Detailed spec from CMB-5: Player play deck and hand

A player in a duel sees a real shuffled hand drawn from their equipped deck, and can discard.

**Deliverables**

- src/server/game/Combat/PlayDeck.{h,cpp}: deck, graveyard, shuffle with the duel RNG, draw to m_maxHandSize (7), reshuffle when empty
- Hand serialization (Hand.m_spellList of Spell built from SpellTemplate: m_templateID, m_pipCost SpellRank, m_magicSchoolID, m_accuracy)
- Discard move handling

**Client messages:** MSG_COMBATHAND, MSG_COMBATMOVE

**Data sources**

- Characters DB equipped deck contents (WIZ)
- SpellMgr templates

**Database tables**

- characters: character_deck (owned by WIZ; read here)

**Acceptance**

- [ ] Unit test: a 20-card deck with a seeded RNG draws a deterministic 7; discarding 2 and starting the next round refills to 7; deck count plus graveyard count stays 20
- [ ] Unit test: once the deck is exhausted, the graveyard reshuffles into the deck
- [ ] Real client: 7 cards appear in the planning HUD showing the right art and pip costs; the deck counter shows the remaining count (MSG_COMBATHAND DeckCount/TotalDeckCount); right-click discard removes the card and the slot stays empty until next round

**Risks**

- Which Spell fields the client needs to render and validate a card (m_spellID vs m_templateID, enchantment) is unverified

## 9.08 Single-target damage cast (CMB-6)

**Goal:** Cast hits or fizzles, pays pips, damages a passive target.

**Size:** M. **Depends on:** 9.07

**Client messages:** MSG_COMBATMOVE, MSG_COMBATACTIONS, MSG_COMBATHEALTH, MSG_COMBATPIPS, MSG_COMBATMOVESELECTION

**Acceptance**

- [ ] 3-pip card with 2 pips is a pass; with 3 pips HP drops by effect param
- [ ] Forced fizzle spends pips and discards
- [ ] m_effectChosen bit pattern for index 2 correct
- [ ] Real client: Fire Cat cinematic and matching health drop; fizzle animation

### Detailed spec from CMB-6: Casting a single-target damage spell against a passive target

The player can cast a damage card that hits or fizzles, pays pips, and visibly damages a creature that only passes.

**Deliverables**

- src/server/game/Combat/CombatResolver.{h,cpp}: execution order (first team, then slot index), pass/fizzle/cast CombatAction building
- Spell target decoding (MSG_COMBATMOVE SpellTarget appears to be a 1<<slot bitmask) and validation that the card is in hand and pips are sufficient
- Pip payment (generic then power; a power pip is worth the live setting Combat.PowerPipValue, default 2) and discard after cast
- Accuracy roll using SpellTemplate m_accuracy only (stats come in CMB-10)
- src/server/game/Combat/Effects/DamageEffect.cpp: kDamage and kDamageNoCrit with fixed m_effectParam; RandomSpellEffect choice encoded into CombatAction.m_effectChosen (4-bit stack, unused bits set)
- Cinematic duration estimate plus the live setting Combat.ExecutionPaddingSeconds so Resolution waits for the animations; both settings apply from the next round

**Client messages:** MSG_COMBATMOVE, MSG_COMBATACTIONS, MSG_COMBATHEALTH, MSG_COMBATPIPS, MSG_COMBATMOVESELECTION

**Data sources**

- Spells/*.xml effects
- Cinematics/*.xml for timing (optional)

**Acceptance**

- [ ] Sim test: casting a 3-pip card with 2 pips is rejected as a pass; with 3 pips, pips drop to 0 and the creature's HP drops by the effect parameter
- [ ] Sim test: with RNG forced to fail accuracy, the action has m_spellHits=0, pips are still spent, and the card is discarded
- [ ] Unit test: m_effectChosen for random choice index 2 of a RandomSpellEffect at effect slot 0 matches the expected bit pattern
- [ ] Real client: picking Fire Cat and clicking the creature plays the Fire Cat cinematic; the creature's health bar drops by the matching amount; a forced fizzle plays the fizzle animation (MSG_COMBATACTIONS with CombatActionListObj)

**Risks**

- The client probably replays CombatActions with its own CombatResolver: the server-supplied roll fields (m_criticalHitRoll, m_stunResistRoll, m_randomSpellEffectPerTargetRolls, m_CritHitList) point that way. If so, server math must match the client exactly or health bars desync. Must be confirmed here with a real client
- Execution-phase length is guessed; too short cuts cinematics off, too long stalls the duel. Combat.ExecutionPaddingSeconds lets it be tuned live against a real client
- If the client resolves pips itself, a Combat.PowerPipValue other than the client's value desyncs the pip display, so its bounds must hold it to what the client accepts

## 9.09 Victory and teardown (CMB-7)

**Goal:** Clean win and world return.

**Size:** S. **Depends on:** 9.08, 9.03

**Client messages:** MSG_COMBATPHASE, MSG_COMBATMATCHRESULT, MSG_ENDDUEL, MSG_ENTERSTATE, MSG_ADDEFFECT, MSG_REMOVEEFFECT, MSG_COMBATVICTORY

**Acceptance**

- [ ] 50 HP creature hit for 60 dies; team 0 wins; OnCreatureKilledInDuel fires once
- [ ] Real client: death, victory, sigil gone, respawn later; grace window blocks re-aggro

### Detailed spec from CMB-7: Victory and duel teardown

When the last creature dies, the duel ends cleanly with a victory for the players and the world returns to normal.

**Deliverables**

- Death detection after each action (kResult_PlayerDied semantics) and dead participants skipped in later actions
- Win check at Resolution; victory sequence (phase kPhase_Victory, MSG_COMBATMATCHRESULT WinningTeam, phase kPhase_Ended, MSG_ENDDUEL)
- Players returned to idle via MSG_ENTERSTATE; a no-aggro grace effect lasting the live setting Combat.NoAggroGraceSeconds via MSG_ADDEFFECT/MSG_REMOVEEFFECT, applied from the next victory
- ScriptMgr hook (proposed CombatScript/PlayerScript OnDuelEnd, OnCreatureKilledInDuel) for QST and loot to attach to

**Client messages:** MSG_COMBATPHASE, MSG_COMBATMATCHRESULT, MSG_ENDDUEL, MSG_ENTERSTATE, MSG_ADDEFFECT, MSG_REMOVEEFFECT, MSG_COMBATVICTORY

**Acceptance**

- [ ] Sim test: a creature with 50 HP hit for 60 dies; the duel reports team 0 won and fires OnCreatureKilledInDuel exactly once
- [ ] Real client: the creature plays its death animation and fades, the victory sequence plays, the sigil disappears, the player can move, and the creature respawns after the spawn timer (WLD)
- [ ] Real client: walking straight into another mob during the grace window does not start a duel

**Risks**

- The XML describes MSG_COMBATVICTORY as client to server ('client is done with a match'), but the reference sends it server to client. Handle it in both directions until verified

## 9.10 Aggro start and minimal creature AI (CMB-8 part 1)

**Goal:** Walking into a mob starts a duel; mob attacks.

**Size:** M. **Depends on:** 9.09

**Client messages:** MSG_AGGRO, MSG_COMBATACTIONS

**Acceptance**

- [ ] Real client: nearby wandering mob pulls both in without a GM command and casts its attack with correct cinematic

### Detailed spec from CMB-8: First playable duel: aggro start, basic creature attack, defeat

A normal player walks into a mob, fights it, and either wins or is defeated and sent to safety. This is the first end-to-end playable duel.

**Deliverables**

- Aggro start: a DuelistBehavior creature within its engage radius (CombatSigilTemplate m_engageRadius / DuelistBehaviorTemplate m_npcProximity) starts a duel at the nearest free sigil
- src/server/game/AI/CreatureCombatAI.{h,cpp} minimal: casts the creature's MobDeckBehavior basic spell at a random living enemy when affordable, else passes
- Player defeat: 0 HP players stay on their circle until the duel ends; if all players are dead, creatures win, and defeated players are moved to the zone's safe/start point with health and mana set to the live settings Defeat.HealthPercent and Defeat.ManaPercent of their maximums and MSG_UPDATEHEALTH sent
- Character HP persisted after the duel (characters DB)

**Client messages:** MSG_AGGRO, MSG_COMBATACTIONS, MSG_COMBATMATCHRESULT, MSG_UPDATEHEALTH, MSG_ENDDUEL

**Data sources**

- ObjectData mob templates: MobDeckBehavior basic spell name (e.g. Golem-Brass-R7 references 'NA Golem Brass-01'), NPCBehaviorTemplate stats

**Database tables**

- characters: character health/mana columns (WIZ)

**Acceptance**

- [ ] Sim test: a player at 10 HP vs a creature dealing 20 loses; the result is team 1 won and the player's persisted HP follows the defeat rule
- [ ] Sim test: after `.settings set Defeat.HealthPercent 50`, the next defeat leaves the player at half health without a restart
- [ ] Real client: walking near a wandering mob pulls both into a sigil without a GM command; the mob casts its attack card with the correct cinematic and damages the player
- [ ] Real client: letting the mob win shows the defeat sequence and the player reappears at the safe point with updated health globes
- [ ] Real client: winning with Fire Cat cards and then walking away works with no stuck state (movement, chat and zone transfer all work)

**Risks**

- Retail post-defeat rules (where you respawn, HP/mana restored) are not verified from sources; Defeat.HealthPercent and Defeat.ManaPercent let them be corrected live once verified
- The class and field name of the MobDeckBehavior template are not confirmed in the type dump

## 9.11 Player defeat and HP persistence (CMB-8 part 2)

**Goal:** Lose and reappear at safety.

**Size:** M. **Depends on:** 9.10, 8.01

**Client messages:** MSG_COMBATMATCHRESULT, MSG_UPDATEHEALTH, MSG_ENDDUEL

**Acceptance**

- [ ] 10 HP player vs 20 damage loses; team 1 wins; persisted HP follows the defeat rule
- [ ] Changing Defeat.HealthPercent applies to the next defeat without a restart
- [ ] Real client: defeat sequence, safe point, updated globes; after a win movement, chat and zoning all work

### Detailed spec from CMB-8: First playable duel: aggro start, basic creature attack, defeat

A normal player walks into a mob, fights it, and either wins or is defeated and sent to safety. This is the first end-to-end playable duel.

**Deliverables**

- Aggro start: a DuelistBehavior creature within its engage radius (CombatSigilTemplate m_engageRadius / DuelistBehaviorTemplate m_npcProximity) starts a duel at the nearest free sigil
- src/server/game/AI/CreatureCombatAI.{h,cpp} minimal: casts the creature's MobDeckBehavior basic spell at a random living enemy when affordable, else passes
- Player defeat: 0 HP players stay on their circle until the duel ends; if all players are dead, creatures win, and defeated players are moved to the zone's safe/start point with health and mana set to the live settings Defeat.HealthPercent and Defeat.ManaPercent of their maximums and MSG_UPDATEHEALTH sent
- Character HP persisted after the duel (characters DB)

**Client messages:** MSG_AGGRO, MSG_COMBATACTIONS, MSG_COMBATMATCHRESULT, MSG_UPDATEHEALTH, MSG_ENDDUEL

**Data sources**

- ObjectData mob templates: MobDeckBehavior basic spell name (e.g. Golem-Brass-R7 references 'NA Golem Brass-01'), NPCBehaviorTemplate stats

**Database tables**

- characters: character health/mana columns (WIZ)

**Acceptance**

- [ ] Sim test: a player at 10 HP vs a creature dealing 20 loses; the result is team 1 won and the player's persisted HP follows the defeat rule
- [ ] Sim test: after `.settings set Defeat.HealthPercent 50`, the next defeat leaves the player at half health without a restart
- [ ] Real client: walking near a wandering mob pulls both into a sigil without a GM command; the mob casts its attack card with the correct cinematic and damages the player
- [ ] Real client: letting the mob win shows the defeat sequence and the player reappears at the safe point with updated health globes
- [ ] Real client: winning with Fire Cat cards and then walking away works with no stuck state (movement, chat and zone transfer all work)

**Risks**

- Retail post-defeat rules (where you respawn, HP/mana restored) are not verified from sources; Defeat.HealthPercent and Defeat.ManaPercent let them be corrected live once verified
- The class and field name of the MobDeckBehavior template are not confirmed in the type dump

## 9.12 Flee, disconnect, logout in combat (CMB-9)

**Goal:** Leave duels without breaking them.

**Size:** S. **Depends on:** 9.11, 6.08

**Client messages:** MSG_COMBATMOVE, MSG_COMBATFLEE, MSG_COMBATREMOVE, MSG_UPDATEMANA

**Acceptance**

- [ ] 2-player duel continues after one flees
- [ ] Real client: Flee runs off with empty mana; killing a client leaves the other's duel; mob resets when all leave

### Detailed spec from CMB-9: Fleeing, disconnect and logout in combat

Players can leave a duel by fleeing, disconnecting or logging out without breaking it for the others.

**Deliverables**

- Flee move handling: MSG_COMBATFLEE and MSG_COMBATREMOVE broadcast, mana drain of the live setting Combat.FleeManaDrainPercent (default 100) via MSG_UPDATEMANA, player removed from the sub-circle
- Disconnect and logout treated as flee; the duel ends if no players remain (creatures reset to full and return to their path)
- Zone-leave while dueling treated as flee

**Client messages:** MSG_COMBATMOVE, MSG_COMBATFLEE, MSG_COMBATREMOVE, MSG_UPDATEMANA

**Acceptance**

- [ ] Sim test: in a 2-player duel, one flees; the duel continues with 1 player and the fleer's slot is free
- [ ] Real client: clicking Flee makes the wizard run off the circle and regain movement with the mana globe empty; a second client in the zone sees them removed
- [ ] Real client: killing one client process mid-duel leaves the other player's duel running; when the last player leaves, the mob walks back to its path with full health

**Risks**

- Whether retail uses MSG_COMBATFLEE or MSG_COMBATREMOVE (or both) for flee is unverified

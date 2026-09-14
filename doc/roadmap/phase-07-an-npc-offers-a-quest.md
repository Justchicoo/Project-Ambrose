<!-- Project Ambrose by Imjustchico: Roadmap phase 7, An NPC offers a quest. -->

# Phase 7: An NPC offers a quest

**Done when:** A test NPC in Ravenwood shows '!' and plays its Prep dialog. It opens the quest offer window, and accepting fills the quest book. Talking to the persona NPC ('?') completes the goal and chains the next offer.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 7.01 | Object template extractor (OBJ-17 + QST-3) | M | 5.01, 6.10, 2.07 |
| 7.02 | Quest/dialog/madlib model and blob encoders (QST-2) | M | 3.05, 3.02 |
| 7.03 | Quest schema, sQuestMgr, validator (QST-5) | M | 7.02, 7.01, 3.13, 4.15 |
| 7.04 | Requirement engine v1 (QST-6) | M | 7.03, 5.05, 4.15 |
| 7.05 | Quest authoring toolchain (QST-22) | M | 7.03, 4.08, 3.13 |
| 7.06 | Character quest persistence and registry (QST-7) | M | 7.03, 3.08 |
| 7.07 | NPC service menu (QST-8) | M | 5.02, 6.13, 7.02, 7.01, 4.16 |
| 7.08 | Wizbang indicators (QST-9) | S | 7.07, 7.04, 7.06, 4.16 |
| 7.09 | Quest offer (QST-10) | M | 7.08 |
| 7.10 | Accept quest and quest book sync (QST-11) | M | 7.09 |
| 7.11 | Goal logic engine and completion (QST-12) | M | 7.10 |
| 7.12 | Persona goals (QST-13) | M | 7.11 |
| 7.13 | Actor dialog lifecycle and dialog review (QST-14) | M | 7.12 |

## 7.01 Object template extractor (OBJ-17 + QST-3)

**Goal:** world.object_template with names, display keys, adjectives, behaviors.

**Size:** M. **Depends on:** 5.01, 6.10, 2.07

**Acceptance**

- [ ] Unknown behavior hash still yields a row
- [ ] Template 38232 -> 'WC-RAV-NPC06', 'WC-NPCs_00000125', Art_Portrait_Boy_Fire.dds; 39088 -> 'WC-GTW-Registrar'; hat row 1652259 WizItemTemplate Items_00028316
- [ ] All 104869 ObjectData processed; idempotent

### Detailed spec from OBJ-17: Template extractor to world database

The key fields of client templates land in world DB tables that managers and GM tooling can query without the WADs mounted.

**Deliverables**

- src/tools/extractors/TemplateExtractor: walks the manifest and writes rows. object_template: entry, class_name, object_name, display_key, visual_id, adjectives, source_path, root class hash; item_template_ext for WizItemTemplate fields used early (equip requirements summary, item type, rarity)
- data/sql/base/db_world schema for those tables plus a dated update file in data/sql/updates/db_world/
- The row source is always the user's install at run time; no rows are committed

**Acceptance**

- [ ] Client-gated run: the row count equals the manifest entries that decode (reported against 137423); the hat row has entry 1652259, class WizItemTemplate, display_key Items_00028316
- [ ] Re-running is idempotent (upsert)
- [ ] dbimport applies the schema update and records it in the updates table

**Risks**

- Schema depends on the pending MySQL/MariaDB decision
- Some ObjectData behaviors use classes missing from the dump until OBJ-12 fills them. The extractor must not fail on them

### Detailed spec from QST-3: Object template extractor for NPCs and interactables

A tool fills world.object_template from the user's ObjectData, so the server knows each NPC's template id, object name, display-name key, portrait, adjectives and service behaviors.

**Deliverables**

- src/tools/extractor/ObjectTemplateExtractor.cpp: walk Root.wad ObjectData/** (BINd), decode WizGameObjectTemplate, and emit rows through dbimport. Store behavior names only (NPCBehavior, WizardQuestingBehavior, BasicNPCServiceBehavior, WizardSelectBehavior) and skip unknown server-only behavior class hashes by size.
- data/sql/base/db_world: object_template, object_template_adjective, object_template_behavior (schema only; rows are generated locally and never committed).
- src/test/tools/extractor/ObjectTemplateExtractorTest.cpp with a synthetic BINd fixture built in the test.

**Data sources**

- Root.wad ObjectData/** (e.g. ObjectData/WC/WC-RAV-NPC06.xml, ObjectData/WC/WC-GTW-Registrar.xml)
- Type dump: WizGameObjectTemplate (m_objectName, m_templateID hash 0x4cb232f6, m_displayName, m_sIcon, m_adjectiveList, m_lootTable, m_behaviors), NPCBehaviorTemplate (m_nLevel, m_mobTitle, m_schoolOfFocus)

**Acceptance**

- [ ] Unit test: a synthetic template containing an unknown behavior class hash still yields a row. The unknown behavior is logged, not fatal.
- [ ] Integration test against the client: template 38232 -> object_name 'WC-RAV-NPC06', display key 'WC-NPCs_00000125', portrait 'GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds', has NPCBehavior and WizardQuestingBehavior. Template 39088 -> 'WC-GTW-Registrar'.
- [ ] The extractor processes all 104869 ObjectData entries, and the report counts NPC templates and unknown-hash occurrences.

**Risks**

- WizardQuestingBehaviorTemplate and BasicNPCServiceBehaviorTemplate are not in the client dump, so their fields (e.g. the persona string 'WC-RAV-NPC06_Persona') can only be recovered by heuristics. Do not depend on them for quest binding.
- Overlaps with whichever domain owns the general template manager (likely OBJ or WLD). Agree on one object_template table.

## 7.02 Quest/dialog/madlib model and blob encoders (QST-2)

**Goal:** Byte-exact quest blobs with envelope.

**Size:** M. **Depends on:** 3.05, 3.02

**Acceptance**

- [ ] Blobs decode back identically; class hashes match the dump (ActorDialog 0x39e3afab)
- [ ] Every STR blob has the SerializerBinary header
- [ ] 3-of-5 bounty GOAL block has COUNT=3, TOTAL=5, TALLYTEXT

### Detailed spec from QST-2: Quest, dialog and madlib content model with client blob encoders

The server holds quests, goals, dialogs and NPC menus as its own C++ types and can encode each client-facing ObjectProperty blob byte-exactly, with the 4-byte SerializerBinary wrapper.

**Deliverables**

- src/server/game/Quests/QuestTemplate.h, GoalTemplate.h: plain structs that mirror the client classes. GoalType enum 0..11 (BOUNTY=1, BOUNTYCOLLECT=2, SCAVENGE=3, PERSONA=4, WAYPOINT=5, ACHIEVERANK=7, USAGE=8). Subtype fields: Persona m_personaName/m_usePatron; Bounty m_npcAdjectives/m_bountyTotal/m_bountyType; Scavenge m_itemAdjectives/m_itemTotal; Waypoint m_zoneEntry/m_zoneExit/m_zoneTag/m_proximityTag; AchieveRank m_rank. Also GoalCompleteLogic (goalsAND, goalsOR, requiredORCount, goalsToAdd, completeQuest) and TallyCounter (count, percentChance, descriptor, descriptor2).
- src/server/game/Dialogs/ActorDialog.h: dialog tag, entries (actorTemplateID, dialog key, picture, sound, action, dialogEvent, animation lists, personaName, nameOverride), dialogEvents.
- src/server/game/Quests/QuestMadlibs.h/.cpp: build the QUEST block (NAME, LEVEL), the GOAL block (NAME, LOCATION, TALLYTEXT, TALLYTEXT2, COUNT, TOTAL, SUBSCRIBER_TOTAL) and the NPC block (NAME, FIRSTNAME, LASTNAME, TITLE, NICKNAME, FULLNAME).
- src/server/game/Quests/QuestWireEncoder.h/.cpp: encode MadlibBlock, GoalCompilation of GoalEntryFull, ClientTagList, AssociatedWorldsList, LootInfoList (GoldLootInfo, MagicXPLootInfo, ItemLootInfo, AddSpellLootInfo), ActorDialog, ServiceMementoBase with PrepEntry, GoalEntry and InteractableOption.
- src/test/server/game/Quests/QuestWireEncoderTest.cpp

**Data sources**

- Client type dump generated by the project's own dumper (reference shape: r806919.Wizard_1_610.json): classes QuestTemplate 0x1081def8, GoalTemplate, PersonaGoalTemplate, BountyGoalTemplate, ScavengeGoalTemplate, WaypointGoalTemplate, AchieveRankGoalTemplate, GoalCompleteLogic, TallyCounterTemplate, ActorDialogList, ActorDialogEntry, NPCDialogEntry, MadlibBlock, MadlibArgT<...>, ServiceMementoBase, PrepEntry, GoalEntry, GoalEntryFull, GoalCompilation, LootInfoList, ClientTagList, AssociatedWorldsList

**Acceptance**

- [ ] Round-trip test: each encoded blob decodes with the OBJ codec back to identical field values, and its class hashes match the dump (ServiceMementoBase, PrepEntry, GoalEntryFull, MadlibBlock, ActorDialog 0x39e3afab).
- [ ] Wrapper test: every blob placed in a message STR field has the SerializerBinary header (bit31 set = raw length, else zlib with uncompressed size). A missing header makes the client crash, per the a local packet capture tool README.
- [ ] Madlib test: a GOAL block for a 3-of-5 bounty carries COUNT=3, TOTAL=5 and a TALLYTEXT key.

**Risks**

- The property-flag mask used when serializing each blob is unverified. The reference server uses 1 for goals and rewards, 4 for ServiceMemento and 16 for ActorDialog. A wrong mask silently drops fields.
- MadlibArgT<std::string> vs <std::wstring> selection per token is unverified.

## 7.03 Quest schema, sQuestMgr, validator (QST-5)

**Goal:** Authored quest rows load into a validated snapshot that reloads live.

**Size:** M. **Depends on:** 7.02, 7.01, 3.13, 4.15

**Acceptance**

- [ ] Prep entries [38232, 0] index as offered by 38232 only
- [ ] Validator rejects missing goalsToAdd, bounty without adjectives/tally, unknown persona, logic with both complete and add
- [ ] Start log 'Loaded N quests, M goals, K validation errors'
- [ ] `.reload quest_template` swaps in an added quest; a reload that introduces an error keeps the old snapshot

### Detailed spec from QST-5: World database quest schema, QuestMgr loader and validator

Quests authored as SQL rows load into an immutable sQuestMgr snapshot built on the 4.15 reloadable store, at startup and on `.reload quest_template`, with every cross-reference checked and reported.

**Deliverables**

- data/sql/base/db_world and data/sql/updates/db_world/YYYY_MM_DD_NN.sql: quest_template (name PK, name_id, title_key, info/prep/underway/complete keys, level, repeat, mainline, no_quest_helper, skip_qh_autoselect, pet_only, activity_type, prep_always, is_hidden), quest_start_goal, quest_goal (quest, goal_name, name_id, type, title_key, underway_key, complete_key, location_key, destination_zone, image1/2, persona_name, use_patron, tally_count, tally_percent, tally_descriptor/2_key, zone_entry, zone_exit, zone_tag, proximity_tag, rank, hide flags), quest_goal_adjective (bounty/scavenge), quest_goal_client_tag, quest_goal_logic plus quest_goal_logic_member (AND / OR / ADD), quest_dialog (owner = quest|goal, tag Start|Prep|Underway|Completion|Complete), quest_dialog_entry (ordered: actor_template_id, dialog_key, picture, sound, action, dialog_event, animation), quest_dialog_madlib.
- src/server/game/Quests/QuestMgr.h/.cpp (sQuestMgr): load into a ReloadableStore<QuestStore> registered with sReloadMgr as quest_template, which rebuilds every quest_* table and index off to the side, validates them, and swaps, keeping the old store and reporting every error on failure; derived indexes starterByTemplateId (FIRST entry of the quest's Prep dialog -> actor_template_id), personaGoalsByObjectName, usageGoalsByTag/adjective, bountyGoalsByAdjective, waypointGoalsByZone.
- src/server/game/Quests/QuestValidator.cpp: goal names in logic, start goals and dialogs exist; a logic entry cannot both add goals and complete the quest; a Prep dialog exists for any quest that has a starter; actor_template_id exists in object_template; persona_name equals some object_template.object_name; bounty goals have at least one adjective and a tally count; every *_key resolves when LocaleStore is available.
- src/test/server/game/Quests/QuestMgrTest.cpp using an in-memory row source.
- Re-binding active quest instances and refreshing NPCs after a swap stay in 10.15.

**Data sources**

- Authored SQL only (the client has no QuestTemplate data)
- object_template from QST-3; LocaleStore from QST-1

**Database tables**

- quest_template
- quest_start_goal
- quest_goal
- quest_goal_adjective
- quest_goal_client_tag
- quest_goal_logic
- quest_goal_logic_member
- quest_dialog
- quest_dialog_entry
- quest_dialog_madlib

**Acceptance**

- [ ] Unit test: a fixture quest whose Prep dialog has entries [template 38232, template 0] indexes as offered by 38232 only.
- [ ] Unit test: the validator rejects (a) goalsToAdd naming a missing goal, (b) a bounty goal with no adjectives or tally_count=0, (c) a persona_name matching no object name, (d) a logic row with both completeQuest and goalsToAdd.
- [ ] Gameserver start log reports 'Loaded N quests, M goals, K validation errors' and refuses to register invalid quests (they are skipped, not crashed on).
- [ ] Unit test: `.reload quest_template` with an added quest swaps it in and indexes its starter. A reload that introduces a validation error keeps the old snapshot serving and reports every error.

**Risks**

- Normalized tables vs JSON columns for dialogs, requirements and results depends on the pending MySQL/MariaDB decision (JSON support differs).
- The name-hash algorithm for quest_template.name_id and goal name_id is unverified (retail capture shows QuestNameID=120047678). It must match the client's string hash, likely owned by OBJ.

## 7.04 Requirement engine v1 (QST-6)

**Goal:** One evaluator for quests, spawns, triggers, equip.

**Size:** M. **Depends on:** 7.03, 5.05, 4.15

**Acceptance**

- [ ] Each type incl. ROP_OR, apply_not, nesting
- [ ] ReqHasEntry(Q1 'Complete') gates the quest until Q1 completes
- [ ] Unknown types evaluate false and log once
- [ ] `.reload requirement` applies an edit to the next evaluation; a bad reload keeps the old lists

### Detailed spec from QST-6: Requirement engine v1

Quest availability, goal activation, dialog entries, spawns and triggers can all be gated by one data-driven requirement evaluator.

**Deliverables**

- data/sql db_world: requirement_list (id, operator AND|OR, apply_not, parent) and requirement (list_id, type, apply_not, quest_name, goal_name, required_status, entry_name, is_quest_registry, numeric_value, operator_type, magic_school, zone, gender).
- src/server/game/Conditions/RequirementMgr.h/.cpp plus one evaluator per type: ReqHasQuest, ReqHasGoal (m_requiredStatus), ReqHasEntry (quest registry vs character registry), ReqEntryValue, ReqGlobalRegistryValue, ReqMagicLevel (ReqNumeric operators), ReqSchoolOfFocus, ReqIsSchool, ReqInZone, ReqIsGender, ReqHasBadge, nested RequirementList.
- RequirementMgr holds requirement lists in a 4.15 reloadable store registered as requirement. `.reload requirement` builds and validates the lists off to the side and swaps them, and a failure keeps the old lists and reports every error.
- ScriptMgr hook ConditionScript for script-defined requirement types.
- src/test/server/game/Conditions/RequirementTest.cpp

**Data sources**

- Type dump: 70 Requirement subclasses (ReqHasGoal: m_questName, m_goalName, m_requiredStatus; ReqHasEntry: m_entryName, m_displayName, m_isQuestRegistry, m_questName; ReqGlobalRegistryValue; ReqMagicLevel)
- Zone triggers.xml / spawnData.xml use ReqHasGoal, ReqHasEntry, ReqHasQuest, ReqIsGender, ReqGlobalRegistryValue

**Database tables**

- requirement_list
- requirement

**Acceptance**

- [ ] Unit tests for each type against a fake character context, including ROP_OR lists, apply_not, and nested lists.
- [ ] Unit test: a quest with ReqHasEntry(m_questName=Q1, entry 'Complete', is_quest_registry) is unavailable until Q1 is completed.
- [ ] Unknown requirement types from extracted trigger or spawn data evaluate to false and log once per type.
- [ ] Unit test: `.reload requirement` applies an edited requirement to the next evaluation, and a reload with a malformed list keeps the old lists serving.

**Risks**

- GoalStatusRequirement enum values (e.g. 'Incomplete' seen in WC_Ravenwood triggers.xml) need confirming from the dump's enum options.

## 7.05 Quest authoring toolchain (QST-22)

**Goal:** Key-only quest SQL validated against the install.

**Size:** M. **Depends on:** 7.03, 4.08, 3.13

**Acceptance**

- [ ] refs over WC_Ravenwood lists WC-STRM-C03-003 and KillColossus of WC-BAL-C02-001; WC_Hub lists WC-MAIN-C01-003_Goal0
- [ ] validate exits nonzero on a misspelled persona and suggests closest object_name

### Detailed spec from QST-22: Quest authoring toolchain and reference index

A contributor can write a new quest as a dated SQL update that uses only keys and ids, validate it against their own client install, and play it within minutes.

**Deliverables**

- src/tools/questtool: 'validate <sql or db>' runs QuestValidator plus LocaleStore key checks, template-id existence and persona and adjective matching, and prints each resolved key's text locally for review. 'scaffold --title-key QuestTitle_xxxxx --giver <templateId> --lang-stem WizQstNNNNNN' writes a skeleton data/sql/updates/pending_db_world/YYYY_MM_DD_NN.sql with a Prep dialog entry bound to the giver and a persona goal stub. 'refs' builds a local, uncommitted index of quest and goal names referenced by client zone data (ReqHasGoal/ReqHasQuest in triggers.xml and spawnData.xml, GoalComplete_* events), so authored names match what zone triggers expect.
- doc/QuestAuthoring.md: table reference, key-only rule (never paste client text), Prep-first-entry binding rule, persona = object_name rule, bounty adjectives and tally rule, goal-logic patterns, reload loop.
- apps/codestyle check: reject SQL in data/sql that contains non-key display text in *_key columns.

**Data sources**

- Zone WADs triggers.xml / spawnData.xml / volumes.xml (3377 zones)
- Root.wad Locale/*, ObjectData/*

**Database tables**

- quest_template
- quest_goal
- quest_dialog_entry

**Acceptance**

- [ ] questtool refs over WizardCity-WC_Ravenwood.wad lists quest WC-STRM-C03-003 and goal KillColossus of WC-BAL-C02-001. Over WizardCity-WC_Hub.wad it lists WC-MAIN-C01-003 / WC-MAIN-C01-003_Goal0.
- [ ] questtool validate exits nonzero on a fixture with a misspelled persona name and prints the closest object_name.
- [ ] A contributor following doc/QuestAuthoring.md scaffolds, edits, validates, runs .reload quest_template and accepts the quest in the client, with no server restart.

**Risks**

- The line between an identifier (key, template id, internal quest name) and extracted data needs a maintainer ruling before any quest SQL is committed.

## 7.06 Character quest persistence and registry (QST-7)

**Goal:** Quest state survives logout.

**Size:** M. **Depends on:** 7.03, 3.08

**Acceptance**

- [ ] accept, 2/5, save, reload keeps quest_gid, goal_gid, count
- [ ] CompleteQuest sets registry (Q,'Complete')=1
- [ ] Orphan goals pruned with warning

### Detailed spec from QST-7: Character quest persistence and quest registry

Active quests, goal progress, completion history, registry entries and hidden-quest flags survive logout, and quest and goal GIDs stay stable across sessions.

**Deliverables**

- data/sql db_characters: character_quest (guid, quest_gid, quest_name, accepted_at), character_quest_goal (quest_gid, goal_gid, goal_name, status ACTIVE|COMPLETE, count), character_quest_registry (guid, quest_name, entry, value), character_registry (guid, entry, value), character_quest_hidden.
- src/server/game/Quests/QuestLog.h/.cpp: per-player log with Add, StartGoal, IncrementGoal, CompleteGoal, CompleteQuest (removes it and stamps the registry entry 'Complete'), Remove, IsGoalActive/IsGoalCompleted. Dirty-save through the character save path.
- GID allocation for quest and goal instances through the shared GID service.

**Database tables**

- character_quest
- character_quest_goal
- character_quest_registry
- character_registry
- character_quest_hidden

**Acceptance**

- [ ] Unit test: accept -> progress 2/5 -> save -> reload gives the same quest_gid, goal_gid and count.
- [ ] Unit test: CompleteQuest removes the active row and sets registry (Q, 'Complete') = 1, and HasCompletedQuest(Q) is true.
- [ ] Unit test: an orphan goal row whose quest template was removed is pruned on load with a warning.

**Risks**

- Whether retail quest GIDs must stay constant across sessions is unverified. Safest to persist them.

## 7.07 NPC service menu (QST-8)

**Goal:** Interaction prompt and routing.

**Size:** M. **Depends on:** 5.02, 6.13, 7.02, 7.01, 4.16

**Client messages:** MSG_INTERACTNPC, MSG_SENDNPCOPTIONS, MSG_LEAVESERVICERANGE, MSG_INTERACTOBJECT, MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION

**Acceptance**

- [ ] Providers with 1 and 2 options give flat indices 0..2 routed correctly
- [ ] One SENDNPCOPTIONS/LEAVESERVICERANGE per crossing
- [ ] Changing Npc.InteractRadiusDefault applies from the next move without a restart
- [ ] Real client: WC-RAV-NPC06 prompt shows localized name and portrait; click logs the right index

### Detailed spec from QST-8: NPC service menu: range, options and interaction routing

Walking near any NPC makes the client show its interaction prompt with the NPC's name, and clicking it routes to the right server-side service.

**Deliverables**

- src/server/game/Npc/NpcServiceProvider.h: interface with GetServiceOptions(player), OnServiceInteraction(player, index), NpcIcon/NameKey/TextKey override, WizBang, priority, interaction radius.
- src/server/game/Npc/NpcServiceMemento.cpp: on movement, entering the radius (Npc.InteractRadiusDefault, a live setting defaulting to 300, unless the provider overrides it) sends MSG_SENDNPCOPTIONS (MobileID = NPC GLOBAL id, Options = ServiceMementoBase with m_npcNameKey 'NPCFormats_Name', m_npcTextKey 'GUI_NPCInteractText', m_npcIcon from template m_sIcon, and m_personaMadlibs as an NPC block holding NAME = display key); leaving sends MSG_LEAVESERVICERANGE. The flat option index maps to the owning provider.
- src/server/game/Handlers/QuestHandler.cpp: HandleInteractNPC (MSG_INTERACTNPC: GlobalID, ServiceName, ServiceIndex, Reinteract; an empty ServiceName means closing a shop) and HandleInteractOption (GAME MSG_INTERACTOBJECT / MSG_INTERACTOPTION for interactables). Registered in the dispatch table as in-world.
- ScriptMgr NpcScript hook (OnGossip-like GetServiceOptions and OnServiceSelect) with AddSC_ loader; scripts/Custom/npc_test_greeter.cpp as a sample.

**Client messages:** MSG_INTERACTNPC, MSG_SENDNPCOPTIONS, MSG_LEAVESERVICERANGE, MSG_INTERACTOBJECT, MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION

**Data sources**

- QuestMessages.xml (service 52), GameMessages.xml (service 5)
- object_template (QST-3), zone_object (QST-4)

**Database tables**

- object_template
- zone_object

**Acceptance**

- [ ] Unit test: two providers with 1 and 2 options produce flat indices 0..2, and index 2 routes to the second provider.
- [ ] Unit test: range enter and exit send exactly one SENDNPCOPTIONS and one LEAVESERVICERANGE per crossing, not one per move packet.
- [ ] Unit test: changing Npc.InteractRadiusDefault applies from the next movement update without a restart.
- [ ] Client: walk up to WC-RAV-NPC06 in Ravenwood (spawned from zone_object) with the sample NpcScript attached. The interaction prompt appears with the NPC's localized name and portrait, and disappears on walking away. Clicking it logs HandleInteractNPC with the right service index.

**Risks**

- The reference server says MSG_SENDNPCOPTIONS.MobileID must be the object's global id, not its mobile id. Unverified against retail; test both.
- Semantics of Reinteract values (0 vs 2) are only known from reference behavior.

## 7.08 Wizbang indicators (QST-9)

**Goal:** '!' and '?' update live.

**Size:** S. **Depends on:** 7.07, 7.04, 7.06, 4.16

**Client messages:** MSG_WIZBANG

**Acceptance**

- [ ] CompleteQuestGoal > StartQuest > None
- [ ] Real client: '!' over the giver disappears within 1 s after accept

### Detailed spec from QST-9: Wizbang indicators

NPCs show the yellow '!' when they have a quest the player can take and the '?' when the player can complete a goal with them, updating live as quest state changes.

**Deliverables**

- src/server/game/Npc/WizBang.h: enum from the reference (None=0, CompleteQuestGoal=432425611, StartQuest=660791182, UnfinishedQuest=747193091, Training, Shopping, ...) and a priority order.
- NpcServiceMemento: compute the per-player top-priority wizbang from providers that currently offer options; send GAME MSG_WIZBANG (WizBangID, GameObjectID = global id) to players in render range on change and every Npc.WizbangResendInterval, a live setting defaulting to 1 s (retail re-sends; a single send can be lost while the scene loads).
- Event-driven invalidation: quest accept, goal complete and quest complete mark nearby NPC wizbangs dirty for that player.

**Client messages:** MSG_WIZBANG

**Data sources**

- GameMessages.xml MSG_WIZBANG

**Acceptance**

- [ ] Unit test: StartQuest outranks None; CompleteQuestGoal outranks StartQuest when both apply.
- [ ] Client: log in next to the QST-10 test quest giver. A '!' floats over its head. After accepting, it disappears within 1 s without re-zoning.

**Risks**

- The wizbang id values are unverified hashes taken from the reference enum. Confirm by trying them against the client.

## 7.09 Quest offer (QST-10)

**Goal:** Retail offer window from the Prep dialog's first actor.

**Size:** M. **Depends on:** 7.08

**Client messages:** MSG_INTERACTAVAILABLEQUEST, MSG_ACTORDIALOG, MSG_QUESTOFFER, MSG_DECLINEQUEST

**Acceptance**

- [ ] AcceptQuest for a never-offered, out-of-range or declined quest rejected
- [ ] Non-first Prep entry offers nothing
- [ ] Real client: Prep lines, then offer window with title, level, 'Talk To <NPC>' goal, rewards; Decline keeps '!'

### Detailed spec from QST-10: Quest offer from an NPC

Clicking a quest giver opens the retail quest offer window (Prep dialog, title, level, goals, rewards) for the first available quest, bound by the Prep dialog's first actor template id.

**Deliverables**

- src/server/game/Quests/QuestOfferProvider.cpp (NpcServiceProvider): attach when starterByTemplateId has the NPC's template id. Available = not active, not completed (unless repeatable), requirements pass; ordering by title (Persona m_maintainQuestOrder is future work). Option PrepEntry{m_displayKey = title key, m_iconKey 'Prep', m_serviceName 'QuestOfferService'}.
- On select, send in this order: WIZARD MSG_INTERACTAVAILABLEQUEST (MobileID, QuestName); MSG_ACTORDIALOG (CompletionType 'QuestInfo', ActorDialog = Prep dialog, RangeCheck, IsYesNo); QUEST MSG_QUESTOFFER (MobileID, QuestName, QuestTitle, Level, Rewards = LootInfoList preview, GoalData = GoalCompilation of start goals, Mainline).
- Per-session offered-quest cache keyed by quest name plus NPC GID. MSG_DECLINEQUEST removes the entry; leaving service range expires it.

**Client messages:** MSG_INTERACTAVAILABLEQUEST, MSG_ACTORDIALOG, MSG_QUESTOFFER, MSG_DECLINEQUEST

**Data sources**

- QuestMessages.xml, WizardMessages.xml
- Locale QuestTitle.lang, WizQst*.lang, WizardQuestGoals.lang

**Database tables**

- quest_template
- quest_dialog
- quest_dialog_entry

**Acceptance**

- [ ] Unit test: the offer cache rejects AcceptQuest for a quest never offered, for an NPC out of range, or after a decline (logs suspicious behavior; no state change).
- [ ] Unit test: an NPC whose template is referenced only by a NON-first Prep entry offers nothing.
- [ ] Client: click the test NPC. The dialog box plays the Prep lines with the NPC portrait, then the quest offer window shows the localized title, level, 'Talk To <NPC>' goal and the reward preview, with Accept and Decline buttons. Decline closes it and the '!' stays.

**Risks**

- The offer message order comes from the reference server, not a retail capture.
- The purpose of MSG_ACCEPTQUESTBOGUS and MSG_NPCINFO in retail offers is unknown.

## 7.10 Accept quest and quest book sync (QST-11)

**Goal:** Quest book and helper filled; resume on relog.

**Size:** M. **Depends on:** 7.09

**Client messages:** MSG_ACCEPTQUEST, MSG_SENDQUEST, MSG_SENDGOAL, MSG_REMOVEQUEST, MSG_REMOVEGOAL

**Acceptance**

- [ ] SENDQUEST encoding matches QuestMessages.xml field order
- [ ] Real client: 'To Ravenwood!' with 'Talk To ...' and 'Wizard City|Ravenwood'; helper points to zone
- [ ] Relog keeps progress; .quest remove drops it

### Detailed spec from QST-11: Accept quest and quest book sync

Accepting adds the quest to the client's quest book and quest helper with correct goal text, and quests re-appear intact after relogging.

**Deliverables**

- QuestHandler::HandleAcceptQuest (MSG_ACCEPTQUEST: MobileID, QuestName): validate against the offer cache, then create the instance (QST-7).
- QuestSync.cpp: MSG_SENDQUEST (QuestID, QuestNameID, QuestLevel, QuestTitle key, New=1, QuestMadlibs, Rewards, AssociatedWorlds from goal destination zone roots, NoQuestHelper, Mainline, ReadyToTurnIn, PetOnlyQuest, ActivityType, empty GoalData), then MSG_SENDGOAL per start goal whose goal requirements pass (SendType 1, GoalStatus 0 = active, GoalType, GoalTitle key, GoalLocation key, GoalDestinationZone, UseTally/GoalCount/GoalTotal/TallyText, GoalMadlibs, ClientTags, PatronIcon = Prep first entry picture).
- Login resume: after world entry, send every active quest with New=0 and its active goals with SendType 0.
- scripts/Commands/cs_quest.cpp: .quest add <name>, .quest remove <name> (sends MSG_REMOVEQUEST), .quest list, .quest goal complete <quest> <goal>.

**Client messages:** MSG_ACCEPTQUEST, MSG_SENDQUEST, MSG_SENDGOAL, MSG_REMOVEQUEST, MSG_REMOVEGOAL

**Data sources**

- Reference capture in a local session capture (reference-server traffic, not retail): SENDQUEST QuestTitle=QuestTitle_00001718, QuestNameID=120047678; SENDGOAL GoalTitle=WizardQuestGoals_TalkNPC, GoalLocation=ZoneLocName_1451497

**Database tables**

- character_quest
- character_quest_goal

**Acceptance**

- [ ] Unit test: the SENDQUEST encoding of a fixture quest has field order and types matching QuestMessages.xml (GID, UINT, UINT, INT, STR, ...).
- [ ] Client: after Accept, the quest book shows 'To Ravenwood!' and its goal 'Talk To ...' with location 'Wizard City|Ravenwood', and the quest helper arrow points to the destination zone.
- [ ] Client: log out and back in. The same quest and goal show with the same progress. .quest remove drops it from the book immediately.

**Risks**

- GoalStatus and SendType meanings (0 resume, 1 start, 2 progress) are inferred from the reference server.
- The client may need SENDQUEST before any zone objects stream on login. Coordinate with WLD attach ordering.

## 7.11 Goal logic engine and completion (QST-12)

**Goal:** GoalCompleteLogic chains goals and completes quests.

**Size:** M. **Depends on:** 7.10

**Client messages:** MSG_COMPLETEGOAL, MSG_COMPLETEQUEST, MSG_REMOVEQUEST, MSG_SENDGOAL

**Acceptance**

- [ ] Linear, AND, OR(requiredORCount=2), failing school requirement skipped
- [ ] Last goal emits COMPLETEGOAL, COMPLETEQUEST, REMOVEQUEST in order
- [ ] Real client: .quest goal complete shows popups and next goal

### Detailed spec from QST-12: Goal logic engine and quest completion

Completing a goal advances the quest through its GoalCompleteLogic chain, activating next goals or completing the quest with correct client notifications.

**Deliverables**

- src/server/game/Quests/GoalLogic.h/.cpp: pure function Evaluate(template, instance) -> {goalsToStart[], completeQuest}. AND prerequisites all complete; OR count >= requiredORCount; skip goals already active or complete; completeQuest takes precedence; with no logic match and no active goals left, the quest completes.
- QuestEngine.cpp: CompleteGoal sends MSG_COMPLETEGOAL (QuestID, GoalID, CompleteText), runs goal complete results (QST-15 hook), evaluates the logic, then either StartGoal (checks goal requirements, sends MSG_SENDGOAL SendType 1, runs activate results) or CompleteQuest.
- CompleteQuest: MSG_COMPLETEQUEST (QuestID, CompleteText) then MSG_REMOVEQUEST, registry stamp, end results, QuestScript::OnQuestComplete, wizbang refresh.
- QuestScript hooks: OnAccept, OnGoalActivate, OnGoalComplete, OnQuestComplete, with AddSC_ loader.

**Client messages:** MSG_COMPLETEGOAL, MSG_COMPLETEQUEST, MSG_REMOVEQUEST, MSG_SENDGOAL

**Data sources**

- Type dump GoalCompleteLogic 0xafb2341

**Database tables**

- quest_goal_logic
- quest_goal_logic_member

**Acceptance**

- [ ] Unit tests on GoalLogic: linear A->B->complete; parallel AND(A,B)->C; OR(A,B,C) with requiredORCount=2; a logic row both adding and completing is rejected at load (QST-5); a goal with a failing school requirement is skipped.
- [ ] Unit test: completing the last goal emits COMPLETEGOAL, COMPLETEQUEST, REMOVEQUEST in that order.
- [ ] Client: .quest goal complete on a 2-goal chain. A goal-complete popup appears, then the second goal shows in the quest helper. Completing the second removes the quest from the book with the quest-complete banner.

**Risks**

- Retail behavior when multiple logic rows fire at once is unverified.

## 7.12 Persona goals (QST-13)

**Goal:** Talk-to completes the goal and chains the next offer.

**Size:** M. **Depends on:** 7.11

**Client messages:** MSG_COMPLETEPERSONA, MSG_INTERACTCOMPLETEGOAL, MSG_PERSONAINFO, MSG_ACTORDIALOG, MSG_SENDNPCOPTIONS

**Acceptance**

- [ ] m_personaName 'WC-RAV-NPC06' binds to template 38232, not a display-name twin
- [ ] COMPLETEPERSONA for inactive goal, wrong NPC or out of range rejected
- [ ] Real client: '?' NPC plays completion lines and opens the follow-up offer

### Detailed spec from QST-13: Persona (talk to NPC) goals

The NPC named by a persona goal shows '?', offers the goal in its menu, and talking to it completes the goal and chains straight into that NPC's next quest offer.

**Deliverables**

- src/server/game/Quests/PersonaGoalProvider.cpp (NpcServiceProvider): attach when personaGoalsByObjectName contains the NPC's object_template.object_name (matched against GoalTemplate m_personaName, NOT the display name). Option GoalEntry{m_questID, m_goalID, m_goalTitle, m_questTitle, m_displayKey = quest title, m_iconKey 'Complete', m_serviceName 'QuestPersonaGoalService'}. WizBang CompleteQuestGoal.
- On select: play the goal's Completion dialog (MSG_ACTORDIALOG CompletionType 'Completion' with QuestID and GoalID), complete the goal (QST-12), and if the quest completes, re-send options with Reinteract=2 and offer the next available quest from the same NPC after the dialog.
- Handle client MSG_COMPLETEPERSONA (MobileID, QuestID, GoalID): validate the goal is active, of persona type, the NPC's object name matches and the NPC is in range, then complete it. Handle MSG_INTERACTCOMPLETEGOAL for the dialog request.

**Client messages:** MSG_COMPLETEPERSONA, MSG_INTERACTCOMPLETEGOAL, MSG_PERSONAINFO, MSG_ACTORDIALOG, MSG_SENDNPCOPTIONS

**Data sources**

- Type dump PersonaGoalTemplate 0x19af84b6 (m_personaName, m_usePatron)
- object_template.object_name

**Database tables**

- quest_goal

**Acceptance**

- [ ] Unit test: a persona goal with m_personaName 'WC-RAV-NPC06' binds to template 38232 (object_name WC-RAV-NPC06), not to an NPC that merely shares the display name.
- [ ] Unit test: MSG_COMPLETEPERSONA for an inactive goal, a wrong NPC or out of range is rejected with no state change.
- [ ] Client: with a 'Talk To' goal active, the target NPC shows '?'. Clicking it plays the completion lines, the goal completes, and if that NPC gives the follow-up quest its offer window opens right after the dialog closes.

**Risks**

- MSG_PERSONAINFO (GoalHyperlink) is not used by the reference server. Its role in retail is unknown.
- m_usePatron semantics (goal targets whoever gave the quest) are unimplemented in the reference.

## 7.13 Actor dialog lifecycle and dialog review (QST-14)

**Goal:** Dialogs at the right moments; events drive follow-ups.

**Size:** M. **Depends on:** 7.12

**Client messages:** MSG_ACTORDIALOG, MSG_COMPLETEDIALOG, MSG_INTERACTUNDERWAYQUEST, MSG_REQUESTQUESTDIALOG, MSG_QUESTDIALOG, MSG_ENCOUNTERDIALOG

**Acceptance**

- [ ] Failing-requirement entry is omitted
- [ ] EntryEvent fires OnDialogEvent once
- [ ] Real client: Start dialog on accept; quest-book review; underway reminder line

### Detailed spec from QST-14: Actor dialog lifecycle and quest-book dialog review

Quest start, goal prep, underway, completion and quest-complete dialogs play at the right moments, the client's dialog-closed events drive follow-up actions, and players can re-read dialog from the quest book.

**Deliverables**

- src/server/game/Dialogs/DialogMgr.cpp: send by tag: quest 'Start' -> CompletionType 'QuestStart'; goal 'Prep' on activate -> 'QuestStart' with ids; goal 'Completion' -> 'Completion'; quest 'Complete' -> 'QuestComplete'. Filter entries by their own RequirementList (meetsRequirements).
- Per-player pending-dialog state. HandleCompleteDialog (MSG_COMPLETEDIALOG: MobileID, CompletionType, EntryEvent) fires dialogEvents, runs deferred results, and releases chained offers.
- HandleInteractUnderwayQuest (MSG_INTERACTUNDERWAYQUEST) sends the Underway dialog. HandleRequestQuestDialog (MSG_REQUESTQUESTDIALOG by QuestNameID) replies MSG_QUESTDIALOG (QuestNameID, ActorDialog).
- Party broadcast stub for MSG_ENCOUNTERDIALOG (gated off until party support).

**Client messages:** MSG_ACTORDIALOG, MSG_COMPLETEDIALOG, MSG_INTERACTUNDERWAYQUEST, MSG_REQUESTQUESTDIALOG, MSG_QUESTDIALOG, MSG_ENCOUNTERDIALOG

**Data sources**

- WizardMessages.xml; Root.wad Locale WizQst*.lang and *-ActorDialog.lang for dialog keys

**Database tables**

- quest_dialog
- quest_dialog_entry
- quest_dialog_madlib
- requirement_list

**Acceptance**

- [ ] Unit test: a Prep dialog whose second entry has a failing requirement sends only the first entry.
- [ ] Unit test: an EntryEvent in MSG_COMPLETEDIALOG fires QuestScript::OnDialogEvent exactly once.
- [ ] Client: accepting plays the Start dialog. Opening the quest book and pressing the dialog-review button shows the quest's dialog text. Talking to a giver about an underway quest shows its reminder line instead of the offer.

**Risks**

- Neither MSG_COMPLETEDIALOG nor MSG_INTERACTUNDERWAYQUEST is handled by the reference server. Their CompletionType strings and expected replies need sniffing retail behavior or client RE.
- Cinematic camera fields in ActorDialogEntry may need zone camera names that exist only in zone data.

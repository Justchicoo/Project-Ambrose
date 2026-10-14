<!-- Project Ambrose by Imjustchico: Roadmap phase 10, Play the Wizard City opening quests. -->

# Phase 10: Play the Wizard City opening quests

**Done when:** A fresh wizard plays the first Wizard City chain from committed key-only SQL, covering offer, waypoint, persona, usage and bounty goals, rewards and gates opening. They also buy from vendors and use the Spiral Door.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 10.01 | Results engine v1 (QST-15) | M | 7.11, 7.04, 6.14, 9.04 |
| 10.02 | Waypoint goals (QST-16) | S | 10.01 |
| 10.03 | Object states (WLD-19 part 1) | M | 6.16, 10.01 |
| 10.04 | Requirement-gated visibility (WLD-19 part 2) | M | 10.03 |
| 10.05 | Usage and scavenge goals (QST-17) | M | 10.04, 7.07 |
| 10.06 | Bounty goals (QST-18) | M | 10.01, 9.11 |
| 10.07 | LootMgr core (QST-19 + CMB-20 merged) | M | 8.09, 2.07, 4.15, 4.16 |
| 10.08 | Quest rewards (QST-19) | M | 10.07, 10.01, 8.02, 8.05 |
| 10.09 | Duel rewards and drop tables (CMB-19 + CMB-20) | M | 10.07, 9.11, 10.06 |
| 10.10 | Gold vendors (EXT-1 + QST-20 + WIZ-24 gold) | M | 7.07, 8.09, 8.01, 4.15, 4.16 |
| 10.11 | Potion shop and wisps (EXT-2 + WIZ-24 potions) | S | 10.10 |
| 10.12 | Treasure card vendor (EXT-6 + WIZ-24 TC) | S | 10.10, 8.12 |
| 10.13 | Teleporters, Spiral Door, Go Home (WLD-16) | M | 6.14, 7.04, 9.03, 4.15, 4.16 |
| 10.14 | Path-walking NPCs (WLD-17) | M | 6.16, 6.12, 4.15 |
| 10.15 | Content hot reload (QST-21) | M | 7.12, 10.08, 10.10, 10.09, 10.11, 10.12, 10.13, 10.14, 8.13, 4.15 |
| 10.16 | Wizard City chain authoring and automated test (QST-23 part 1) | M | 7.05, 7.13, 10.02, 10.05, 10.06, 10.08 |
| 10.17 | Wizard City chain real-client playthrough (QST-23 part 2) | M | 10.16, 10.03 |
| 10.18 | Quest helper extras (QST-24) | M | 7.11, 7.06 |
| 10.19 | Path actions (WLD-17 part 2) | M | 10.14 |
| 10.20 | Zone tokens | M | 10.01, 7.04 |
| 10.21 | Interactive world objects | M | 10.01, 10.03, 10.04, 10.07, 13.04 |
| 10.22 | Lua script host | L | 10.01, 6.16 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Oversized.** 10.16 authoring 3-5 quests plus the automated chain test with a golden sequence (M). Fine only if 7.05 tooling is solid; otherwise split.

## 10.01 Results engine v1 (QST-15)

**Goal:** Data-driven results and quest event bus.

**Size:** M. **Depends on:** 7.11, 7.04, 6.14, 9.04

**Client messages:** MSG_NPCSPEECH, MSG_PLAYCINEMATIC

**Acceptance**

- [ ] Result list runs in order; gated result skipped
- [x] Completing WC-MAIN-C01-003_Goal0 publishes 'GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0' (`QuestLogTest.ACompletedGoalPublishesItsEventOnceAndAWizardsDynamodsComeBackFromItsRows`: the goal hands out that event once, which the wizard's session posts into its zone's triggers, and `ZoneTriggerResultTest.AnAddDynaModResultReadsAsTheModItMakesAndAFiredTriggerHandsItBack` fires OpenGateToUW on it)
- [ ] ResTeleport moves the player

### Detailed spec from QST-15: Results engine v1: registry, events and flow results

Quest start and end, goal activate and complete, tally and trigger results can change registry state, post events, add or complete quests, show text and teleport, all from data.

**Deliverables**

- data/sql db_world: result_list (id) and result (list_id, order, type, requirement_list_id, string/int/float args).
- src/server/game/Results/ResultMgr.h/.cpp: server-owned result types (not client ObjectProperty classes; most are absent from the client dump). ResModifyEntry, ResIncrementEntry, ResRemoveEntry, ResTimeStampEntry, ResPostEvent, ResPostQuestEvent, ResAddQuest (grant without offer), ResCompleteQuestGoal, ResDisplayText, ResActorDialog, ResPlaySound, ResTeleport (delegates to WLD), ResSpawn/ResDespawn (delegates to WLD), ResWait.
- Event bus: QuestEvent names such as 'GoalComplete_<quest>_<goal>' (the format seen in WC_Hub triggers.xml) published for WLD triggers to consume.

**Client messages:** MSG_NPCSPEECH, MSG_PLAYCINEMATIC

**Data sources**

- Zone triggers.xml event names and ResTeleport usage (WizardCity-WC_Hub.wad, WizardCity-WC_Ravenwood.wad)

**Database tables**

- result_list
- result

**Acceptance**

- [ ] Unit test: the result list [ModifyEntry X=1, CompleteQuestGoal Q/G] runs in order and a gated result is skipped when its requirement fails.
- [x] Unit test: completing WC-MAIN-C01-003_Goal0 in a fixture publishes 'GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0'. (`QuestLogTest.ACompletedGoalPublishesItsEventOnceAndAWizardsDynamodsComeBackFromItsRows`)
- [ ] Client: a quest whose goal completion runs ResTeleport moves the player to the destination zone and location.

**Risks**

- Result argument schemas are designed by us from observed behavior and must be kept stable once content exists.
- Overlap with WLD's trigger and volume system over who owns the event bus.

## 10.02 Waypoint goals (QST-16)

**Goal:** Zone entry/exit/proximity goals.

**Size:** S. **Depends on:** 10.01

**Client messages:** MSG_SENDGOAL, MSG_COMPLETEGOAL

**Acceptance**

- [ ] Entering WC_Ravenwood completes entry goal only
- [ ] Real client: walking through the Ravenwood gate completes 'Go to Ravenwood'

### Detailed spec from QST-16: Waypoint goals: zone entry, exit and proximity

'Go to X' goals complete when the player enters or leaves the named zone or walks into the tagged volume.

**Deliverables**

- WaypointGoalTracker.cpp: on attach and zone transfer, complete active waypoint goals with m_zoneEntry && zoneTag == new zone, or m_zoneExit && zoneTag == previous zone, before other attach logic runs.
- Proximity: subscribe to WLD volume-enter events and complete goals whose proximity_tag matches the volume name.
- Destination-zone suppression: omit GoalDestinationZone in MSG_SENDGOAL when the player is already in that zone, except on resume.

**Client messages:** MSG_SENDGOAL, MSG_COMPLETEGOAL

**Data sources**

- Zone volumes.xml (named volumes, e.g. 'Ravenwood POI' in WC_Hub)

**Database tables**

- quest_goal

**Acceptance**

- [ ] Unit test: entering WizardCity/WC_Ravenwood completes an entry goal tagged with that zone and leaves an exit goal for the same zone untouched until transfer out.
- [ ] Client: accept a 'Go to Ravenwood' fixture quest in the Commons, walk through the Ravenwood gate, and the goal-complete popup appears on arrival with the next goal shown.

**Risks**

- Proximity tags in retail content may name volumes that are not in the client volumes.xml.

## 10.03 Object states (WLD-19 part 1)

**Goal:** ENTERSTATE/LEAVESTATE and per-player Dynamods: persisted world changes such as gates and cages opening for one wizard.

**Size:** M. **Depends on:** 6.16, 10.01

**Client messages:** MSG_ENTERSTATE, MSG_LEAVESTATE, MSG_DYNAMODBEHAVIOR_UPDATEMODS

**Acceptance**

- [x] Real client: after 'OpenGateToUW' the Unicorn Way gate is 'IdleOpen' for that player only (client driver `dynamod-gate.json`: the game master's gate opens and the companion's stays closed)

### Detailed spec from WLD-19: Object states and per-player Dynamods

Objects show the right animation state, and the world changes a wizard has earned show for that wizard alone, through the client's Dynamod system. Two unverified leads describe it: contrib/findings/protocol/dynamod-update-mods.json (what the client does with a mod) and contrib/findings/world/dynamod-trigger-results.json (where mods come from, with counts). Re-derive both before building on them.

**Deliverables**

- GameObject state: m_startState on spawn, else the state set's start state; map-wide MSG_ENTERSTATE and MSG_LEAVESTATE for shared changes.
- Dynamods: each character keeps a list of mods, one per (zone, client tag), holding the tag, zone name, state, remove flag, index and originator. A mod is added or changed by the ResAddDynaMod trigger result (m_dynaModRemove true hides the tagged object instead), taken away by tag in every zone by the ResRemoveDynaMod quest result, which the server defines since the client gives it no layout, and by a quest or encounter template's QuestDynaModInfo list, which is dropped when the quest ends unless m_existBeyondQuest is set. The index is the server's handle for the mod and is unique within the character's list.
- Applying a mod is the server's job: the client never changes an object's state or visibility from a mod. For each object in the wizard's view whose m_zoneTag matches a mod for the current zone, the server sends that wizard alone MSG_ENTERSTATE with the mod's state (or omits or removes the object for a hide), on zone entry and the moment the mod changes. A mod whose state set has a transition state (IdleClosed to IdleOpen through Open) plays the transition when it changes live and lands on the final state on zone entry.
- MSG_DYNAMODBEHAVIOR_UPDATEMODS on the wizard's player object, which alone carries DynaModBehavior: UpdateAll with the whole list at login, then Add and Remove as mods change, so the client arms and clears its own spawners named by each tag. MSG_DYNAMODDUMP is debug only and is not sent.
- Tags that name no object in the zone (WC_Hub's PetParkGate and PetParkCollision) are kept and applied to nothing, and reported once when content loads.

**Client messages:** MSG_ENTERSTATE, MSG_LEAVESTATE, MSG_DYNAMODBEHAVIOR_UPDATEMODS, MSG_NEWOBJECT, MSG_REMOVEOBJECT

**Data sources**

- zone_object.zoneTag and startState
- ResAddDynaMod results in each zone archive's triggers.xml
- QuestDynaModInfo on quest, encounter and tutorial quest templates. The install ships none of these templates, so they come from the quest data the server authors.
- StateData/ in Root.wad (object state sets)

**Database tables**

- characters.character_dynamod

**Acceptance**

- [x] Real client: in WC_Hub the Unicorn Way gate shows closed for a fresh character, and after the quest goal named in 'OpenGateToUW' it plays Open and rests 'IdleOpen' for that player only; player B without the quest still sees it closed (client driver `dynamod-gate.json`: the gate swings open, the shot 0.3 seconds after the goal differing from both the closed and the raised gate, and the companion still sees it closed)
- [x] Real client: the gate is still 'IdleOpen' for that player after a relog and a zone change (client driver `dynamod-gate.json`: open after the client restarts and after `.tele zone` to Ravenwood and back)
- [ ] Unit: a character's mods load and save; ResAddDynaMod adds, changes and removes by tag; a quest's mods leave with the quest unless m_existBeyondQuest
- [x] Unit: MSG_DYNAMODBEHAVIOR_UPDATEMODS carries the whole list at login and one Add or Remove per change (`DynaModMessagesTest.TheFullSetReadsBackAsADynaModListOfEachMod` and `DynaModMessagesTest.AReplacedModDropsTheOldByIndexAndAddsTheNewWhileOthersTellNothing`: the full list reads back as a DynaModList, a replaced mod drops the old by index and adds the new, and a removal or another zone's mod sends nothing)

**Risks**

- One ResAddDynaMod bool is still unnamed; the quest files name the other two, m_useQuestAsOriginator and m_bShowsObject, whose hashes prove the names, and what m_bShowsObject changes when it is 0 is unverified: the server shows every tagged object a non-removal mod names.
- Behavior of IgnoreIfCurrentStateIsOff and the Data (EmoteStateOverrideInfo) field is unverified.

## 10.04 Requirement-gated visibility (WLD-19 part 2)

**Goal:** Quest-gated objects per player.

**Size:** M. **Depends on:** 10.03

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT

**Acceptance**

- [x] Only changed objects resend (`SightRequirementsTest.OnlyObjectsWhoseAnswerChangedAreShownOrTakenAway`: a wizard who takes up a quest is sent only the object placed for its holders as new and only the object placed for those without it as removed, an object with no spawn requirements is left alone, and dropping the quest brings the first back as added)
- [ ] Real client: quest-gated NPC appears without relog

### Detailed spec from WLD-19: Object states and per-player visibility

Objects show the right animation state and quest-gated objects appear only to players who qualify.

**Deliverables**

- GameObject state: m_startState on spawn; Map-wide MSG_ENTERSTATE and MSG_LEAVESTATE; per-player state override sent only to that player
- Per-player visibility filter in VisibilitySet: zone_object.spawnRequirements evaluated through the QST requirement engine, re-evaluated on quest or registry change events
- Per-player object state persisted in characters DB where trigger results set it (e.g. a gate shown 'IdleOpen' after a quest), sent to that player on zone enter

**Client messages:** MSG_ENTERSTATE, MSG_LEAVESTATE, MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT

**Data sources**

- zone_object.startState, spawnRequirements
- StateData/ in Root.wad (object state sets)

**Database tables**

- characters.character_object_state

**Acceptance**

- [ ] Real client: in WC_Hub the Unicorn Way gate shows closed for a fresh character, and after the quest goal named in 'OpenGateToUW' it shows 'IdleOpen' for that player only; player B without the quest still sees it closed
- [ ] Real client: a quest-gated NPC appears for a player the moment their quest step completes, without a relog
- [x] Unit: requirement re-evaluation sends NEWOBJECT/ADDOBJECT or REMOVEOBJECT only for objects whose result changed (`SightRequirementsTest.OnlyObjectsWhoseAnswerChangedAreShownOrTakenAway`: a wizard who takes up a quest is sent only the object placed for its holders as new and only the object placed for those without it as removed, an object with no spawn requirements is left alone, and dropping the quest brings the first back as added)

**Risks**

- Behavior of IgnoreIfCurrentStateIsOff and the Data (EmoteStateOverrideInfo) field is unverified

## 10.05 Usage and scavenge goals (QST-17)

**Goal:** Clickable quest objects.

**Size:** M. **Depends on:** 10.04, 7.07

**Client messages:** MSG_SENDNPCOPTIONS, MSG_INTERACTOPTION, MSG_INTERACTOBJECT, MSG_LEAVESERVICERANGE, MSG_SENDGOAL

**Acceptance**

- [ ] Adjective 'Cattail' matches
- [ ] Real client: 1/3, 2/3, complete; used objects vanish for that player

### Detailed spec from QST-17: Usage and scavenge goals on interactable objects

Quest objects in the world (glowing items, chests) become clickable only while the player has a matching goal, and clicking advances or completes it.

**Deliverables**

- UsageGoalProvider.cpp (NpcServiceProvider for templates with WizardSelectBehavior): match active USAGE or SCAVENGE goals by goal client_tag == object_name or item adjective in the object's adjective list. Offer InteractableOption{m_serviceName 'Interact'}; menu text key 'GUI_ChestInteract' (reference guess).
- On use: increment the tally and send MSG_SENDGOAL SendType 2 with the new GoalCount, or complete the goal. When tally_count > 1, despawn the object for that player (MSG_LEAVESERVICERANGE plus per-player hide via WLD).
- Validation that the player is in range and holds the goal.

**Client messages:** MSG_SENDNPCOPTIONS, MSG_INTERACTOPTION, MSG_INTERACTOBJECT, MSG_LEAVESERVICERANGE, MSG_SENDGOAL

**Data sources**

- object_template adjectives and behaviors (QST-3); spawnData SpawnPoint_* collectibles

**Database tables**

- quest_goal_adjective
- quest_goal_client_tag

**Acceptance**

- [ ] Unit test: an object whose adjective list contains 'Cattail' matches a scavenge goal with item adjective 'Cattail', and one without it does not.
- [ ] Client: with 'Collect 3 X' active, X objects show an interact prompt; with no goal they do not. Each click updates the helper to 1/3, 2/3, then completes it, and each used object vanishes.

**Risks**

- Per-player object visibility needs WLD support. Without it, a despawn affects everyone.

## 10.06 Bounty goals (QST-18)

**Goal:** Kills advance tallies.

**Size:** M. **Depends on:** 10.01, 9.11

**Client messages:** MSG_SENDGOAL, MSG_COMPLETEGOAL

**Acceptance**

- [ ] [LostSoul,LostSoul,Rat] gives +2
- [ ] BOUNTYCOLLECT 0 never, 1.0 always
- [ ] '.quest killcredit' twice completes 'Defeat 2'

### Detailed spec from QST-18: Bounty and bounty-collect goals

Defeating mobs whose adjectives match a bounty goal advances its tally ('Defeat 3 Lost Souls 1/3'), and bounty-collect goals roll their drop chance.

**Deliverables**

- BountyGoalTracker.cpp: on combat victory {defeated mob template ids and adjective lists, participants}, for each active BOUNTY or BOUNTYCOLLECT goal increment by the number of matching defeated mobs (BOUNTYCOLLECT only on a percentChance roll). Send MSG_SENDGOAL SendType 2 or complete.
- The adjective source is object_template_adjective of the defeated mob template.
- GM command in scripts/Commands/cs_quest.cpp: .quest killcredit <templateId> [count] to simulate kills before combat exists.
- Validator rule (QST-5): a bounty goal must have at least one adjective that matches at least one extracted mob template, and tally_count >= 1.

**Client messages:** MSG_SENDGOAL, MSG_COMPLETEGOAL

**Data sources**

- object_template_adjective from ObjectData m_adjectiveList

**Database tables**

- quest_goal_adjective
- object_template_adjective

**Acceptance**

- [ ] Unit test: goal adjectives [LostSoul], defeated mobs [LostSoul, LostSoul, Rat] give +2, and the goal completes at the tally count.
- [ ] Unit test: BOUNTYCOLLECT with percentChance 0 never increments; with 1.0 it always does.
- [ ] Client: .quest killcredit twice on a 'Defeat 2' goal. The helper shows 1/2, then the goal-complete popup.

**Risks**

- Combat is another domain; the victory event contract (CMB) must carry mob adjectives.
- Party kill credit (MSG_GROUPQUESTCREDIT) is not part of this milestone; it needs groups (12.04) and is planned, not yet scheduled.

## 10.07 LootMgr core (QST-19 + CMB-20 merged)

**Goal:** Weighted grouped loot tables.

**Size:** M. **Depends on:** 8.09, 2.07, 4.15, 4.16

**Acceptance**

- [x] 100000 seeded rolls of a 10% entry land in 9.5-10.5% (`LootMgrTest.ATenPercentEntryLandsNearTenPercentAndTheSameSeedRollsTheSame`: a 10% item entry is given within 9,500 to 10,500 times in 100,000 rolls, and the same seed gives the same count again)
- [x] School-gated spell entry only for that school (`LootMgrTest.ASchoolGatedSpellGoesOnlyToThatSchool`: a spell entry whose requirement list is ReqSchoolOfFocus Fire goes to a Fire wizard and not to an Ice one, who still gets the table's gold)
- [x] With Rate.Drop.Item at 2, the same 10% item entry lands in 19-21% without a restart (`LootMgrTest.RateDropItemOfTwoDoublesAnItemEntryWithoutAReloadAndNeverScalesAGuaranteedOne`: after a live set of Rate.Drop.Item to 2 the entry lands within 19,000 to 21,000 of 100,000 rolls with no reload, a 10% spell entry stays near 10%, and at 0 the item never drops while a guaranteed entry still always does)

### Detailed spec from QST-19: Loot tables and quest rewards

Quest completion grants gold, XP, items and spells from data-driven loot tables, the offer window previews them, and the client shows reward popups.

**Deliverables**

- data/sql db_world: loot_table, loot_entry (type GOLD|XP|ITEM|SPELL|REAGENT|TREASURE_CARD, template_id, min/max, chance, group, requirement_list_id), creature_loot (object template -> loot_table; the template's m_lootTable name is kept as a hint only).
- Each rolled entry's chance is multiplied by the live setting Rate.Drop.<kind> for its type (Rate.Drop.Item, Rate.Drop.Spell, Rate.Drop.Reagent, Rate.Drop.TreasureCard), bounded in its declaration, and a scaled chance never exceeds 1.0. Guaranteed entries are not scaled.
- src/server/game/Loot/LootMgr.h/.cpp (sLootMgr): roller with grouped entries, deterministic RNG seam for tests.
- Result types ResLoot/ResDropTable(table, maxRolls), ResAddGold, ResAddMagicXP, ResAddSpell/ResLearnSpell with per-school requirement; reward preview builder for MSG_QUESTOFFER and MSG_SENDQUEST Rewards.
- Send WIZARD MSG_LOOT (GlobalID, LootList) for granted loot and MSG_QUESTREWARDS (QuestID, LootList) for spell rewards; MSG_ITEMDROP on a full inventory.

**Client messages:** MSG_LOOT, MSG_QUESTREWARDS, MSG_ITEMDROP, MSG_QUESTOFFER, MSG_SENDQUEST

**Data sources**

- Type dump LootInfoList and LootInfo subclasses (GoldLootInfo, MagicXPLootInfo, ItemLootInfo, AddSpellLootInfo)
- ObjectData WizGameObjectTemplate.m_lootTable (name only)

**Database tables**

- loot_table
- loot_entry
- creature_loot

**Acceptance**

- [x] Unit test with a seeded RNG: table rolls are reproducible, and a school-gated spell entry is only granted to that school. (`LootMgrTest.ATenPercentEntryLandsNearTenPercentAndTheSameSeedRollsTheSame` and `LootMgrTest.ASchoolGatedSpellGoesOnlyToThatSchool`)
- [ ] Client: completing a quest with 100 gold, 50 XP and one item shows the reward popup with the gold, XP bar gain and item icon, and the backpack contains the item. A Fire wizard gets the spell cinematic popup for the fire spell only.

**Risks**

- Drop table data is not in the client, so all loot must be authored, and balancing is a content task.
- The preview must not re-roll randomly on every offer. Decide whether it shows guaranteed entries only.

### Detailed spec from CMB-20: Drop tables and loot window

Defeated creatures roll authored drop tables and players receive items, reagents and treasure cards in the loot popup.

**Deliverables**

- src/server/game/Loot/LootMgr (sLootMgr): world loot tables keyed by the loot-table names referenced in mob templates (e.g. 'LT-Drop-BronzeGear-04'), weighted and guaranteed entries, nested tables, with drop chances scaled by the Rate.Drop.<kind> live settings
- LootInfoList construction (GoldLootInfo, ItemLootInfo, TreasureCardLootInfo, ReagentLootInfo, ...) and MSG_LOOT
- cs_loot.cpp: .loot reload, .loot test <table>
- loot_table and creature_loot are one 4.15 reload target: `.loot reload` and `.reload loot_table` build the tables off to the side, validate them, and swap them; a failure keeps the old tables and reports every error

**Client messages:** MSG_LOOT

**Data sources**

- ObjectData mob templates: loot table name strings

**Database tables**

- world: loot_table
- world: loot_table_entry
- world: creature_loot

**Acceptance**

- [x] Unit test: 100000 seeded rolls of a table with a 10% entry land within 9.5–10.5% (`LootMgrTest.ATenPercentEntryLandsNearTenPercentAndTheSameSeedRollsTheSame`)
- [x] Unit test: after `.settings set Rate.Drop.Item 2`, 100000 seeded rolls of the same 10% item entry land within 19–21% without a restart (`LootMgrTest.RateDropItemOfTwoDoublesAnItemEntryWithoutAReloadAndNeverScalesAGuaranteedOne`)
- [ ] Real client: after victory the loot popup lists the rolled items with icons, and the items are in the backpack after relog (characters DB)

**Risks**

- Drop contents are server-side and must be authored clean-room; only table names are visible in client templates

## 10.08 Quest rewards (QST-19)

**Goal:** Gold, XP, items, spells on completion.

**Size:** M. **Depends on:** 10.07, 10.01, 8.02, 8.05

**Client messages:** MSG_LOOT, MSG_QUESTREWARDS, MSG_ITEMDROP, MSG_QUESTOFFER, MSG_SENDQUEST

**Acceptance**

- [ ] Real client: reward popup with 100 gold, 50 XP and item; Fire wizard gets only the fire spell popup
- [x] Changing Rate.Gold.Quest or Rate.XP.Quest applies to the next quest reward without a restart [QuestRewardsTest.AQuestsGoldAndExperienceFollowTheLiveQuestRatesWithoutARestart: 125 gold and 50 experience become 250 and 75 after setting Rate.Gold.Quest 2 and Rate.XP.Quest 1.5 live]

### Detailed spec from QST-19: Loot tables and quest rewards

Quest completion grants gold, XP, items and spells from data-driven loot tables, the offer window previews them, and the client shows reward popups.

**Deliverables**

- data/sql db_world: loot_table, loot_entry (type GOLD|XP|ITEM|SPELL|REAGENT|TREASURE_CARD, template_id, min/max, chance, group, requirement_list_id), creature_loot (object template -> loot_table; the template's m_lootTable name is kept as a hint only).
- src/server/game/Loot/LootMgr.h/.cpp (sLootMgr): roller with grouped entries, deterministic RNG seam for tests.
- Result types ResLoot/ResDropTable(table, maxRolls), ResAddGold, ResAddMagicXP, ResAddSpell/ResLearnSpell with per-school requirement; reward preview builder for MSG_QUESTOFFER and MSG_SENDQUEST Rewards.
- Quest gold and XP grants are multiplied by the live settings Rate.Gold.Quest and Rate.XP.Quest, and the reward preview shows the scaled amounts.
- Send WIZARD MSG_LOOT (GlobalID, LootList) for granted loot and MSG_QUESTREWARDS (QuestID, LootList) for spell rewards; MSG_ITEMDROP on a full inventory.

**Client messages:** MSG_LOOT, MSG_QUESTREWARDS, MSG_ITEMDROP, MSG_QUESTOFFER, MSG_SENDQUEST

**Data sources**

- Type dump LootInfoList and LootInfo subclasses (GoldLootInfo, MagicXPLootInfo, ItemLootInfo, AddSpellLootInfo)
- ObjectData WizGameObjectTemplate.m_lootTable (name only)

**Database tables**

- loot_table
- loot_entry
- creature_loot

**Acceptance**

- [ ] Unit test with a seeded RNG: table rolls are reproducible, and a school-gated spell entry is only granted to that school.
- [ ] Client: completing a quest with 100 gold, 50 XP and one item shows the reward popup with the gold, XP bar gain and item icon, and the backpack contains the item. A Fire wizard gets the spell cinematic popup for the fire spell only.
- [ ] Client: after `.settings set Rate.Gold.Quest 2`, completing the same quest shows 200 gold in the reward popup without a restart.

**Risks**

- Drop table data is not in the client, so all loot must be authored, and balancing is a content task.
- The preview must not re-roll randomly on every offer. Decide whether it shows guaranteed entries only.

## 10.09 Duel rewards and drop tables (CMB-19 + CMB-20)

**Goal:** XP, gold, loot window, quest credit.

**Size:** M. **Depends on:** 10.07, 9.11, 10.06

**Client messages:** MSG_UPDATEXP, MSG_LEVELUP, MSG_LOOT

**Acceptance**

- [ ] Level-3 Normal mob grants XP row to winners, nothing to fleers
- [ ] Changing Rate.XP.Kill or Rate.Gold.Kill applies to the next victory without a restart
- [ ] Real client: XP bar, gold, loot popup items persist; quest tracker increments

### Detailed spec from CMB-19: Rewards: XP, gold, quest credit

Winning a duel grants experience and gold, levels players up, and credits quest kill goals.

**Deliverables**

- src/server/game/Combat/DuelRewards: XP per creature (world table keyed by level/rank/title) and gold rolls, multiplied by the live settings Rate.XP.Kill and Rate.Gold.Kill
- `.reload creature_combat_reward` as a 4.15 target: builds the table off to the side, validates it, and swaps it; a failure keeps the old rows and reports every error
- Level-up integration through WIZ (MSG_UPDATEXP, MSG_LEVELUP)
- QuestScript/PlayerScript hook firing per defeated creature template for every living or defeated player in the duel

**Client messages:** MSG_UPDATEXP, MSG_LEVELUP, MSG_LOOT

**Data sources**

- NPCBehaviorTemplate m_nLevel, m_mobTitle

**Database tables**

- world: creature_combat_reward (xp, gold min/max)

**Acceptance**

- [ ] Unit test: a defeated level-3 Normal mob grants the XP row value to each winning player, and nothing to fled players
- [ ] Unit test: after `.settings set Rate.XP.Kill 2`, the next defeated level-3 Normal mob grants twice the XP row value without a restart
- [ ] Real client: after victory the XP bar fills by the granted amount, gold rises, and a quest 'defeat 3 X' tracker increments per kill
- [ ] Real client: crossing an XP threshold shows the level-up effect

**Risks**

- Retail XP and gold formulas are not in client data; values must be authored or observed

### Detailed spec from CMB-20: Drop tables and loot window

Defeated creatures roll authored drop tables and players receive items, reagents and treasure cards in the loot popup.

**Deliverables**

- src/server/game/Loot/LootMgr (sLootMgr): world loot tables keyed by the loot-table names referenced in mob templates (e.g. 'LT-Drop-BronzeGear-04'), weighted and guaranteed entries, nested tables
- LootInfoList construction (GoldLootInfo, ItemLootInfo, TreasureCardLootInfo, ReagentLootInfo, ...) and MSG_LOOT
- cs_loot.cpp: .loot reload, .loot test <table>

**Client messages:** MSG_LOOT

**Data sources**

- ObjectData mob templates: loot table name strings

**Database tables**

- world: loot_table
- world: loot_table_entry
- world: creature_loot

**Acceptance**

- [ ] Unit test: 100000 seeded rolls of a table with a 10% entry land within 9.5–10.5%
- [ ] Real client: after victory the loot popup lists the rolled items with icons, and the items are in the backpack after relog (characters DB)

**Risks**

- Drop contents are server-side and must be authored clean-room; only table names are visible in client templates

## 10.10 Gold vendors (EXT-1 + QST-20 + WIZ-24 gold)

**Goal:** Shop window buy/sell.

**Size:** M. **Depends on:** 7.07, 8.09, 8.01, 4.15, 4.16

**Client messages:** MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION, MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_DONESHOPPING, MSG_UPDATEGOLD, MSG_REMOVEDSHOPPER, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUEST, MSG_SELLMODIFIER

**Acceptance**

- [ ] Insufficient gold, item not listed, out of range rejected with no change; equipped/locked sell refused
- [ ] Changing Shop.SellValuePercent applies to the next sale; a failed `.reload npc_vendor` keeps the old stock
- [ ] Real client: shop lists DB items; buy deducts gold and adds item; closing frees movement
- [ ] Real client: a vendor shows its shop icon over its head and its prompt before anyone talks to it; an NPC with neither a shop nor a quest shows neither

### Detailed spec from EXT-1: Service-option framework and gold equipment vendors

Talking to a vendor NPC opens the native shop window, and the player can buy and sell items for gold.

**Deliverables**

- src/server/game/Shops/ShopMgr (sShopMgr) loading vendor stock from world DB as the 4.15 reload target npc_vendor: a reload builds the stock off to the side, validates it, and swaps it, a failure keeps the old stock and reports every error, and an open shop window keeps its list until reopened
- src/server/game/Handlers/ShopHandler.cpp (buy, sell, done)
- WizShopOffering / EquipmentShopOption serialization in the service-option list sent on interact
- MSG_WIZBANG Shopping plus MSG_ENTERSTATE 'Shop' (freezes movement) and MSG_LEAVESTATE on done
- data/sql/base/db_world: npc_vendor(npc_template_id, item_template_id, currency_type, sort)
- scripts/Commands/cs_vendor.cpp (add/remove/reload)
- src/test/server/game/Shops: price and sell-value calculation, illegal-item rejection

**Client messages:** MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION, MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_DONESHOPPING, MSG_UPDATEGOLD, MSG_REMOVEDSHOPPER

**Data sources**

- ObjectData item templates (WizItemTemplate price fields) via template extractor
- Vendor stock lists are NOT in the client. The world DB authors them (GM command or hand-written custom SQL), and committed stock is never copied from another project's data. An opt-in importer that reads another project's vendor data from a copy the user has, into that user's local world database only, is planned, not yet scheduled

**Database tables**

- world.npc_vendor
- characters.character_inventory
- characters.character_currency

**Acceptance**

- [ ] Unit: buying with insufficient gold is rejected; sell value matches formula; item not in vendor list is rejected and logged
- [ ] Unit: setting Shop.SellValuePercent to a new value with `.settings set` changes the next sell price to match, without a restart
- [ ] Client: clicking a vendor NPC in Wizard City shows the equipment shop window listing the DB items with correct prices
- [ ] Client: buying an item plays the confirm, deducts gold in the HUD, and the item appears in the backpack; selling reverses it; closing the window lets the wizard move again
- [ ] Client: a vendor NPC in Wizard City shows its shop icon (MSG_WIZBANG Shopping) over its head from the moment it is in view, before any interaction, and its interact prompt on walking up, as in the live game, where every shop NPC carries an icon so players know what it is for; an NPC whose providers offer no shop and no available quest shows no icon and no prompt

**Risks**

- Service option index to handler mapping may be shared with trainers and quest givers, so coordinate with QST/WLD interact design
- MSG_SHOPBUYREQUEST carries texture/decal/petName fields that dye/pet purchases rely on

### Detailed spec from QST-20: NPC vendors

Shopkeeper NPCs show a shop option and open the client shop window with an item list from world.npc_vendor.

**Deliverables**

- data/sql db_world: npc_vendor (object template id, item template id, price override, currency, requirement_list_id, sort).
- src/server/game/Npc/VendorProvider.cpp (NpcServiceProvider, WizBang Shopping): on select send WIZARD MSG_SHOPLIST (GlobalID, Data, Credits). MSG_DONESHOPPING and MSG_INTERACTNPC with an empty ServiceName close the shop state.
- Buy and sell handlers MSG_SHOPBUYREQUEST -> MSG_SHOPBUYCONFIRM and MSG_SHOPSELLREQUEST -> MSG_SHOPSELLCONFIRM, delegating money and inventory to WIZ with server-side price and range checks.

**Client messages:** MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_DONESHOPPING

**Data sources**

- WizardMessages.xml; the MSG_SHOPLIST Data blob class must be identified from the type dump (unverified)

**Database tables**

- npc_vendor

**Acceptance**

- [ ] Unit test: a buy fails with insufficient gold, an item not in the vendor list, or an NPC out of range, with no gold or inventory change.
- [ ] Client: click a shopkeeper and pick Shop. The shop window lists the configured items with prices. Buying one deducts gold and adds the item; selling returns gold.

**Risks**

- The MSG_SHOPLIST.Data blob class is not identified yet.
- May belong to an economy or items domain. Keep QST limited to the provider and table if it does.

### Detailed spec from WIZ-24: Gold vendors, potion and treasure-card shops, quick sell

Players can buy items, potions and treasure cards from NPC vendors with gold and sell items back.

**Deliverables**

- world.npc_vendor (npc template, item template, currency, price override), world.npc_treasure_vendor, authored SQL
- SHOPLIST sender; SHOPBUYREQUEST -> SHOPBUYCONFIRM; SHOPSELLREQUEST -> SHOPSELLCONFIRM; REQUESTQUICKSELL and WIZARD2 QUICKSELLREQUEST (QuickSellItemList blob); POTIONSHOPOPEN/POTIONBUYREQUEST/POTIONBUYCONFIRM; TREASURESHOPLIST/TREASUREBUY/TREASUREBUYCONFIRM; DONESHOPPING ends shop state
- Price rules from item_template.m_baseCost and PriceModifiers.xml; the sell value is the live setting Shop.SellValuePercent of the base cost, applied from the next sale

**Client messages:** MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUESTBANK, MSG_POTIONSHOPOPEN, MSG_POTIONBUYREQUEST, MSG_POTIONBUYCONFIRM, MSG_TREASURESHOPLIST, MSG_TREASUREBUY, MSG_TREASUREBUYCONFIRM, MSG_DONESHOPPING, WIZARD2 MSG_QUICKSELLREQUEST, WIZARD2 MSG_SELLMODIFIER

**Data sources**

- Root.wad PriceModifiers.xml (root PriceModifiers)
- world.item_template m_baseCost

**Database tables**

- world.npc_vendor
- world.npc_treasure_vendor

**Acceptance**

- [ ] Unit test: buying without enough gold returns Failure and does not change gold or backpack
- [ ] Unit test: selling an equipped or locked item is refused
- [ ] Real client: at a vendor NPC, buying a hat drops the gold counter and puts the hat in the backpack. Selling it back raises gold. Buying a potion at the potion shop refills the potion bottle.

**Risks**

- Vendor inventories are server-side and must be authored clean-room.
- The SHOPLIST Data blob format is unverified. This milestone may overlap EXT (economy); confirm ownership.

## 10.11 Potion shop and wisps (EXT-2 + WIZ-24 potions)

**Goal:** Buy potions, pick up wisps.

**Size:** S. **Depends on:** 10.10

**Client messages:** MSG_POTIONSHOPOPEN, MSG_POTIONBUYREQUEST, MSG_POTIONBUYCONFIRM, MSG_USEPOTION, MSG_UPDATEPOTIONS, MSG_UPDATEHEALTH, MSG_UPDATEMANA, MSG_ENTERSTATE, MSG_DELETEOBJECT

**Acceptance**

- [ ] Cannot exceed max potions
- [ ] Changing Potion.RefillCostPerLevel applies to the next purchase; a failed `.reload wisp_template` keeps the old rows
- [ ] Real client: bottles fill; red wisp heals, disappears and respawns

### Detailed spec from EXT-2: Potions, potion shop and wisps

Players buy and drink health/mana potions and pick up health/mana wisps in the world.

**Deliverables**

- game/Potions/PotionMgr with the refill cost formula's factors as live settings Potion.RefillCostBase and Potion.RefillCostPerLevel, defaults in gameserver.conf.dist, applied from the next purchase
- PotionShopOption service option and handler
- game/Entities wisp pickup behavior (proximity trigger, heal percent from world DB, despawn/respawn timer from the row's respawn seconds scaled by Rate.Respawn)
- world.wisp_template table (template id, stat, percent, respawn seconds)
- `.reload wisp_template` as a 4.15 target: builds the table off to the side, validates it, and swaps it; the next pickup and respawn use the new rows, and a failure keeps the old rows and reports every error

**Client messages:** MSG_POTIONSHOPOPEN, MSG_POTIONBUYREQUEST, MSG_POTIONBUYCONFIRM, MSG_USEPOTION, MSG_UPDATEPOTIONS, MSG_UPDATEHEALTH, MSG_UPDATEMANA, MSG_ENTERSTATE, MSG_DELETEOBJECT

**Data sources**

- ObjectData wisp GameObjectTemplates (names contain 'Wisp')
- Wisp heal percents are not verified as present in client data; if absent, author in world DB

**Database tables**

- world.wisp_template
- characters.character_potions

**Acceptance**

- [ ] Unit: potion purchase cost scales with level; cannot exceed max potions
- [ ] Unit: `.settings set Potion.RefillCostPerLevel` changes the next purchase's cost without a restart
- [ ] Client: Mystical Mixtures window opens from the potion NPC, buying fills potion bottles on the HUD
- [ ] Client: clicking the potion button raises the health globe; walking over a red wisp while damaged heals, plays the pickup effect, and the wisp disappears then respawns

**Risks**

- Max potion count may be a MaxPotionLootInfo loot effect rather than a constant

### Detailed spec from WIZ-24: Gold vendors, potion and treasure-card shops, quick sell

Players can buy items, potions and treasure cards from NPC vendors with gold and sell items back.

**Deliverables**

- world.npc_vendor (npc template, item template, currency, price override), world.npc_treasure_vendor, authored SQL
- SHOPLIST sender; SHOPBUYREQUEST -> SHOPBUYCONFIRM; SHOPSELLREQUEST -> SHOPSELLCONFIRM; REQUESTQUICKSELL and WIZARD2 QUICKSELLREQUEST (QuickSellItemList blob); POTIONSHOPOPEN/POTIONBUYREQUEST/POTIONBUYCONFIRM; TREASURESHOPLIST/TREASUREBUY/TREASUREBUYCONFIRM; DONESHOPPING ends shop state
- Price rules from item_template.m_baseCost and PriceModifiers.xml; the sell value is the live setting Shop.SellValuePercent of the base cost, applied from the next sale

**Client messages:** MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUESTBANK, MSG_POTIONSHOPOPEN, MSG_POTIONBUYREQUEST, MSG_POTIONBUYCONFIRM, MSG_TREASURESHOPLIST, MSG_TREASUREBUY, MSG_TREASUREBUYCONFIRM, MSG_DONESHOPPING, WIZARD2 MSG_QUICKSELLREQUEST, WIZARD2 MSG_SELLMODIFIER

**Data sources**

- Root.wad PriceModifiers.xml (root PriceModifiers)
- world.item_template m_baseCost

**Database tables**

- world.npc_vendor
- world.npc_treasure_vendor

**Acceptance**

- [ ] Unit test: buying without enough gold returns Failure and does not change gold or backpack
- [ ] Unit test: selling an equipped or locked item is refused
- [ ] Real client: at a vendor NPC, buying a hat drops the gold counter and puts the hat in the backpack. Selling it back raises gold. Buying a potion at the potion shop refills the potion bottle.

**Risks**

- Vendor inventories are server-side and must be authored clean-room.
- The SHOPLIST Data blob format is unverified. This milestone may overlap EXT (economy); confirm ownership.

## 10.12 Treasure card vendor (EXT-6 + WIZ-24 TC)

**Goal:** Buy TCs.

**Size:** S. **Depends on:** 10.10, 8.12

**Client messages:** MSG_TREASURESHOPLIST, MSG_TREASUREBUY, MSG_TREASUREBUYCONFIRM

**Acceptance**

- [ ] Buying N adds N and deducts N x price
- [ ] Real client: cards show in treasure tab and drag into sideboard

### Detailed spec from EXT-6: Treasure card vendor

Players buy treasure cards and see them in their spellbook's treasure tab.

**Deliverables**

- TreasureShopOption service option, TreasureShop handler
- Treasure card inventory per character (book and deck)
- `.reload npc_treasure_vendor` as a 4.15 target: builds the stock off to the side, validates it, and swaps it; a failure keeps the old stock and reports every error

**Client messages:** MSG_TREASURESHOPLIST, MSG_TREASUREBUY, MSG_TREASUREBUYCONFIRM, MSG_ADDTREASURESPELLTOBOOK, MSG_REMOVETREASURESPELLFROMBOOK, MSG_ADDTREASURESPELLTODECK, MSG_REMOVETREASURESPELLFROMDECK, MSG_REMOVETREASURESPELLFROMVAULT

**Data sources**

- Spells/ templates, TreasureShopModifiers
- Card stock per vendor authored in world DB

**Database tables**

- world.npc_treasure_vendor
- characters.character_treasure_cards

**Acceptance**

- [ ] Unit: buying quantity N adds N cards and deducts N x price
- [ ] Client: buying cards at the TC vendor makes them appear in the spellbook treasure section and they can be dragged into a deck's sideboard

### Detailed spec from WIZ-24: Gold vendors, potion and treasure-card shops, quick sell

Players can buy items, potions and treasure cards from NPC vendors with gold and sell items back.

**Deliverables**

- world.npc_vendor (npc template, item template, currency, price override), world.npc_treasure_vendor, authored SQL
- SHOPLIST sender; SHOPBUYREQUEST -> SHOPBUYCONFIRM; SHOPSELLREQUEST -> SHOPSELLCONFIRM; REQUESTQUICKSELL and WIZARD2 QUICKSELLREQUEST (QuickSellItemList blob); POTIONSHOPOPEN/POTIONBUYREQUEST/POTIONBUYCONFIRM; TREASURESHOPLIST/TREASUREBUY/TREASUREBUYCONFIRM; DONESHOPPING ends shop state
- Price rules from item_template.m_baseCost and PriceModifiers.xml; the sell value is the live setting Shop.SellValuePercent of the base cost, applied from the next sale

**Client messages:** MSG_SHOPLIST, MSG_SHOPBUYREQUEST, MSG_SHOPBUYCONFIRM, MSG_SHOPSELLREQUEST, MSG_SHOPSELLCONFIRM, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUESTBANK, MSG_POTIONSHOPOPEN, MSG_POTIONBUYREQUEST, MSG_POTIONBUYCONFIRM, MSG_TREASURESHOPLIST, MSG_TREASUREBUY, MSG_TREASUREBUYCONFIRM, MSG_DONESHOPPING, WIZARD2 MSG_QUICKSELLREQUEST, WIZARD2 MSG_SELLMODIFIER

**Data sources**

- Root.wad PriceModifiers.xml (root PriceModifiers)
- world.item_template m_baseCost

**Database tables**

- world.npc_vendor
- world.npc_treasure_vendor

**Acceptance**

- [ ] Unit test: buying without enough gold returns Failure and does not change gold or backpack
- [ ] Unit test: selling an equipped or locked item is refused
- [ ] Real client: at a vendor NPC, buying a hat drops the gold counter and puts the hat in the backpack. Selling it back raises gold. Buying a potion at the potion shop refills the potion bottle.

**Risks**

- Vendor inventories are server-side and must be authored clean-room.
- The SHOPLIST Data blob format is unverified. This milestone may overlap EXT (economy); confirm ownership.

## 10.13 Teleporters, Spiral Door, Go Home (WLD-16)

**Goal:** World list and hub travel.

**Size:** M. **Depends on:** 6.14, 7.04, 9.03, 4.15, 4.16

**Client messages:** MSG_INTERACTOBJECT, MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION, MSG_LEAVESERVICERANGE, MSG_WIZBANG, MSG_ENTERSTATE, MSG_ADDEFFECT, MSG_WORLDTELEPORTLIST, MSG_WORLDTELEPORTREQUEST, MSG_GOHOME

**Acceptance**

- [ ] Unmet teleporter requirement does nothing and notifies
- [ ] Changing GoHome.CooldownSeconds applies to the next press; a failed `.reload world_hub` keeps the old rows
- [ ] Real client: Spiral Door lists unlocked worlds; Krokotopia loads; Go Home loads WC_Hub, second press within GoHome.CooldownSeconds (default 30) refused

### Detailed spec from WLD-16: Interactive teleporters and the Spiral Door

Clicking teleporter objects (Spiral Door, go-home, marked world teleporters) opens the right UI and moves the player.

**Deliverables**

- Interact dispatch for objects whose template has teleport behaviors (TeleportProximityBehaviorTemplate: m_radius, m_locationName, m_requirementList), via NpcScript/GameObjectScript
- Spiral Door: MSG_WORLDTELEPORTLIST (WIZARD 12) with a WorldTeleportOptions blob built from world.world_hub rows gated by the character's unlocked worlds; MSG_WORLDTELEPORTREQUEST transfers to the world's arrival zone and location; an empty World clears the wizbang
- MSG_GOHOME (12): teleport effects (MSG_ENTERSTATE 'Teleport', MSG_ADDEFFECT RecallHome/CantGoHome) then transfer to the current world's hub from world.world_hub; a press within the live setting GoHome.CooldownSeconds (default 30) of the last one is refused
- world.world_hub table: world name, hub zone, hub location, universe teleport zone and location
- `.reload world_hub` as a 4.15 target: builds the table off to the side, validates it, and swaps it; the next world list and Go Home use the new rows, and a failure keeps the old rows and reports every error

**Client messages:** MSG_INTERACTOBJECT, MSG_SENDINTERACTOPTIONS, MSG_INTERACTOPTION, MSG_LEAVESERVICERANGE, MSG_WIZBANG, MSG_ENTERSTATE, MSG_ADDEFFECT, MSG_WORLDTELEPORTLIST, MSG_WORLDTELEPORTREQUEST, MSG_GOHOME

**Data sources**

- ObjectData templates with TeleportProximityBehaviorTemplate
- Type dump: WorldTeleportOptions, NamedEffect

**Database tables**

- world.world_hub
- characters.character_world_unlock (or QST-owned registry)

**Acceptance**

- [ ] Real client: clicking the Spiral Door in Ravenwood opens the world list with only unlocked worlds; choosing Krokotopia loads its arrival zone
- [ ] Real client: the Go Home button in a WC street plays the teleport effect, then loads WC_Hub; a second press within GoHome.CooldownSeconds (default 30) is refused
- [ ] Unit: a teleporter with unmet requirements does nothing and sends a notify text
- [ ] Unit: after `.settings set GoHome.CooldownSeconds 5`, a press 6 s after the last one is accepted without a restart

**Risks**

- How the Spiral Door is identified is unverified: the reference hard-codes template 84113. Prefer a behavior-based match.
- Ownership overlap with the NPC interaction and service-menu domain. Only teleport-type options belong here.
- GoHome.CooldownSeconds's default of 30 has no source: no client file or observation of the live game backs it, so the real Go Home cooldown is unknown until a player who knows says or a run shows it.

## 10.14 Path-walking NPCs (WLD-17)

**Goal:** Server-driven patrols.

**Size:** M. **Depends on:** 6.16, 6.12, 4.15

**Client messages:** MSG_SERVERMOVE, MSG_MOVESTATE, MSG_SERVERTELEPORT

**Acceptance**

- [x] 2-node path at speed s takes distance/s +-1 tick (PathMovementGeneratorTest.TwoNodePathCoversDistanceAtConfiguredSpeedWithinOneTick)
- [x] Empty Map sends nothing (PathWalkersTest.AnInstanceWithNoWizardHoldsItsWalkersStillAndSendsNothing)
- [x] `.reload zone_path` reroutes walking NPCs live; a failed reload keeps the old paths (PathWalkersTest.AReloadedPathCarriesAWalkerOnFromItsNextNodeAndAGonePathStopsIt, ZonePathDatabaseTest.AReloadServesAMovedNodeAndABrokenRowKeepsTheOldPaths)
- [x] Real client: patrol identical on two clients (path-patrol.json run 20261009-232525 on r806919: a game master and a companion on Unicorn Way see the Lost Souls on Path Ghost02 at the same places in each shot pair and moving between shots, facing the way they walk after ce80e1f, and the maintainer, watching, saw them walk without jumping; the game log shows 24 spawners placing 97 objects and no path or behavior error)

### Detailed spec from WLD-17: Path-walking NPCs

NPCs with path data walk their routes on the server, and every client sees the same motion.

**Deliverables**

- extractor zones: decode pathData.xml (PathManager::PathTemplateList, 0x3B6A23E8) and pathNodeData.bin into world.zone_path and world.zone_path_node
- sSpawnerMgr places each zone_spawner_entry on its path's node by path_id, start_node_type, start_node and unique_loc; until then an entry with a path and no place of its own places nothing, so every shipped wisp and reagent spawner waits on this
- src/server/game/Movement/PathMovementGenerator.h/.cpp: node-to-node travel at PathMovementBehaviorTemplate m_movementSpeed * m_movementScale, with loop, ping-pong and wait handling per path data
- Relays MSG_SERVERMOVE plus MSG_MOVESTATE to viewers only, paused when no player is in the Map
- `.reload zone_path` as a 4.15 target: builds zone_path and zone_path_node off to the side, validates them, and swaps them; an NPC on a changed path continues from its next node on the new path, and a failure keeps the old paths and reports every error
- src/test/server/game/Movement/PathMovementGeneratorTest.cpp

**Client messages:** MSG_SERVERMOVE, MSG_MOVESTATE, MSG_SERVERTELEPORT

**Data sources**

- <zone>.wad/pathData.xml (BINd)
- <zone>.wad/pathNodeData.bin
- ObjectData PathMovementBehaviorTemplate (m_movementSpeed, m_movementScale)

**Database tables**

- world.zone_path
- world.zone_path_node

**Acceptance**

- [x] Unit: an NPC on a 2-node path with speed s covers the distance in distance/s seconds (+-1 tick) (PathMovementGeneratorTest.TwoNodePathCoversDistanceAtConfiguredSpeedWithinOneTick: speed 5 times scale 2 over 50 units arrives in 250 ticks of 20 ms)
- [x] Unit: a Map with no players sends no movement packets (PathWalkersTest.AnInstanceWithNoWizardHoldsItsWalkersStillAndSendsNothing: twenty one-second passes with no wizard send no move and leave the walker on its first node)
- [x] Unit: `.reload zone_path` with a moved node sends a walking NPC to the new node position from its next leg without a restart, and a reload with a broken node keeps the old path (PathWalkersTest.AReloadedPathCarriesAWalkerOnFromItsNextNodeAndAGonePathStopsIt moves node 20 and the walker heads to its new place from the next leg; ZonePathDatabaseTest.AReloadServesAMovedNodeAndABrokenRowKeepsTheOldPaths reloads a moved node from the world database and keeps generation 1 when a node is not a number)
- [x] Real client: a patrolling NPC in a Wizard City street walks the same route on clients A and B at the same moment and does not pop on arrival (path-patrol.json run 20261009-232525 on r806919: a game master and a companion on Unicorn Way see the Lost Souls on Path Ghost02 at the same places in each shot pair and moving between shots, facing the way they walk after ce80e1f, and the maintainer, watching, saw them walk without jumping; the game log shows 24 spawners placing 97 objects and no path or behavior error)

**Risks**

- pathNodeData.bin's format (starts without BINd) is only partly understood
- Path-to-object linking (SpawnObjectInfo.m_pathID) comes through spawnData, so WLD-18 may need to land first for some NPCs

## 10.15 Content hot reload (QST-21)

**Goal:** Reload quests, loot, vendors and every other content store live.

**Size:** M. **Depends on:** 7.12, 10.08, 10.10, 10.09, 10.11, 10.12, 10.13, 10.14, 8.13, 4.15

**Client messages:** MSG_SENDQUEST, MSG_REMOVEGOAL, MSG_SENDNPCOPTIONS, MSG_WIZBANG

**Acceptance**

- [ ] Renamed dialog key used next time; instance intact
- [ ] Invalid reload refused; old snapshot served
- [ ] `.reload all` reports a result and generation for every registered content target
- [ ] Real client: '.reload quest_template' shows new title; new quest '!' within 1 s

### Detailed spec from QST-21: Hot reload of content and GM quest tooling

Quest, dialog, requirement, result, loot, vendor and every other content edit applies live without restarting, and in-progress players keep working.

**Deliverables**

- 4.15 reload targets for quest_template (all quest_* tables as one snapshot; the swap comes from 7.03 and this milestone adds the re-binding below), requirement, result, loot_table, npc_vendor and object_template, reached through `.reload <target>`, the console `reload`, and `.reload all`. Each needs an admin security level.
- Every content manager is a 4.15 target, registered by the milestone that owns it: zone_template, zone_location, zone_object, zone_trigger, zone_teleport, zone_spawner, zone_path, world_hub, game_tele, templates, quickchat, character_name, character_create_school, playercreateinfo, player_level_stats, item_template, npc_trainer_spell, npc_treasure_vendor, wisp_template, creature_combat_reward, and the spell and sigil stores. This milestone checks that none is missing. The 17.12 admin API exposes all of them when it lands.
- The managers build a new immutable store off-thread, validate it, and swap the shared_ptr atomically on the world thread. Failed validation keeps the old store and reports every error to the GM or console that asked.
- Post-swap: re-bind active quest instances by quest_name and goal_name. Goals removed from the template are dropped with MSG_REMOVEGOAL; quests removed are left dormant, not deleted. Rebuild starter and persona indexes, mark every NPC wizbang dirty, re-send options to players in range.
- cs_quest additions: .quest reset <name> (clears registry), .quest info <name> (validation state, starter NPC, persona targets).

**Client messages:** MSG_SENDQUEST, MSG_REMOVEGOAL, MSG_SENDNPCOPTIONS, MSG_WIZBANG

**Database tables**

- quest_template
- quest_goal
- quest_dialog
- requirement
- result
- loot_table
- npc_vendor

**Acceptance**

- [ ] Unit test: while a player holds goal G, reload a template that renames the dialog key. The next dialog uses the new key and the instance stays intact.
- [ ] Unit test: a reload that introduces a validation error is refused and the old snapshot is still served.
- [ ] Unit test: `.reload all` lists every content target named above with its result and generation, and a broken row in one target leaves the others swapped and that one serving its old store.
- [ ] Client: change a quest's title key in SQL, run .reload quest_template, reopen the quest book. The new title shows without relogging. Adding a new quest whose Prep actor is a nearby NPC makes a '!' appear within 1 s.

**Risks**

- Concurrency: any cached template pointers held by sessions must be snapshot-scoped (shared_ptr) to avoid use-after-swap.

## 10.16 Wizard City chain authoring and automated test (QST-23 part 1)

**Goal:** 3-5 key-only quests from QuestTitle_00001718.

**Size:** M. **Depends on:** 7.05, 7.13, 10.02, 10.05, 10.06, 10.08

**Client messages:** MSG_INTERACTNPC, MSG_QUESTOFFER, MSG_ACCEPTQUEST, MSG_SENDQUEST, MSG_SENDGOAL, MSG_COMPLETEDIALOG, MSG_COMPLETEGOAL, MSG_COMPLETEQUEST, MSG_REMOVEQUEST, MSG_LOOT, MSG_WIZBANG

**Acceptance**

- [ ] Automated chain test reaches registry 'Complete' for all; message sequence matches golden list

### Detailed spec from QST-23: Wizard City opening quest chain vertical slice

A new wizard can play the first real Wizard City quests end to end, covering offer, persona, waypoint, usage and bounty goals, rewards and chaining, from committed key-only SQL.

**Deliverables**

- data/sql/updates/db_world: 3-5 authored quests starting with QuestTitle_00001718 ('To Ravenwood!'), bound to real NPC templates extracted from WC_Hub and WC_Ravenwood, with names matching trigger references (WC-MAIN-C01-*) so existing zone triggers such as the Commons gate 'OpenGateToUW' react.
- src/test/server/game/Quests/WizardCityChainTest.cpp: scripted session test that drives accept -> goals -> complete through handlers with fake messages.
- doc/testing/wizard-city-chain.md: manual client test script.

**Client messages:** MSG_INTERACTNPC, MSG_SENDNPCOPTIONS, MSG_QUESTOFFER, MSG_ACCEPTQUEST, MSG_SENDQUEST, MSG_SENDGOAL, MSG_ACTORDIALOG, MSG_COMPLETEDIALOG, MSG_COMPLETEGOAL, MSG_COMPLETEQUEST, MSG_REMOVEQUEST, MSG_LOOT, MSG_WIZBANG

**Data sources**

- Locale QuestTitle.lang, WizQst*.lang, WC-ActorDialog.lang
- WizardCity-WC_Hub.wad and WizardCity-WC_Ravenwood.wad triggers.xml

**Database tables**

- quest_template
- quest_goal
- quest_goal_logic
- quest_dialog
- quest_dialog_entry
- loot_table

**Acceptance**

- [ ] The automated chain test passes. All quests reach registry 'Complete'; the message sequence matches a stored golden list.
- [ ] Client: fresh character, the first giver shows '!', dialog plays, quest accepted, helper points to Ravenwood; walking there completes the waypoint; the persona NPC '?' completes the talk goal; the next quest is offered immediately; the reward popup shows gold and XP; the Unicorn Way gate trigger opens once its goal is done.

**Risks**

- Retail goal structure, dialog-key-to-quest mapping and rewards for the committed chain must be reconstructed from observed behavior, not from any other project's data, and may not match retail exactly. 7.05 imports a CSR package the user kept from an older client into that user's local world database, never committed; an importer for another project's quest data is planned, not yet scheduled.
- Real zone triggers may reference goals we do not author, leaving gates closed.

## 10.17 Wizard City chain real-client playthrough (QST-23 part 2)

**Goal:** End-to-end play.

**Size:** M. **Depends on:** 10.16, 10.03

**Acceptance**

- [ ] Fresh character: '!', dialog, accept, helper to Ravenwood, waypoint, persona '?', next offer, reward popup, Unicorn Way gate trigger opens

### Detailed spec from QST-23: Wizard City opening quest chain vertical slice

A new wizard can play the first real Wizard City quests end to end, covering offer, persona, waypoint, usage and bounty goals, rewards and chaining, from committed key-only SQL.

**Deliverables**

- data/sql/updates/db_world: 3-5 authored quests starting with QuestTitle_00001718 ('To Ravenwood!'), bound to real NPC templates extracted from WC_Hub and WC_Ravenwood, with names matching trigger references (WC-MAIN-C01-*) so existing zone triggers such as the Commons gate 'OpenGateToUW' react.
- src/test/server/game/Quests/WizardCityChainTest.cpp: scripted session test that drives accept -> goals -> complete through handlers with fake messages.
- doc/testing/wizard-city-chain.md: manual client test script.

**Client messages:** MSG_INTERACTNPC, MSG_SENDNPCOPTIONS, MSG_QUESTOFFER, MSG_ACCEPTQUEST, MSG_SENDQUEST, MSG_SENDGOAL, MSG_ACTORDIALOG, MSG_COMPLETEDIALOG, MSG_COMPLETEGOAL, MSG_COMPLETEQUEST, MSG_REMOVEQUEST, MSG_LOOT, MSG_WIZBANG

**Data sources**

- Locale QuestTitle.lang, WizQst*.lang, WC-ActorDialog.lang
- WizardCity-WC_Hub.wad and WizardCity-WC_Ravenwood.wad triggers.xml

**Database tables**

- quest_template
- quest_goal
- quest_goal_logic
- quest_dialog
- quest_dialog_entry
- loot_table

**Acceptance**

- [ ] The automated chain test passes. All quests reach registry 'Complete'; the message sequence matches a stored golden list.
- [ ] Client: fresh character, the first giver shows '!', dialog plays, quest accepted, helper points to Ravenwood; walking there completes the waypoint; the persona NPC '?' completes the talk goal; the next quest is offered immediately; the reward popup shows gold and XP; the Unicorn Way gate trigger opens once its goal is done.

**Risks**

- Retail goal structure, dialog-key-to-quest mapping and rewards for the committed chain must be reconstructed from observed behavior, not from any other project's data, and may not match retail exactly. 7.05 imports a CSR package the user kept from an older client into that user's local world database, never committed; an importer for another project's quest data is planned, not yet scheduled.
- Real zone triggers may reference goals we do not author, leaving gates closed.

## 10.18 Quest helper extras (QST-24)

**Goal:** Turn-in markers, hidden quests, map, finder, next world.

**Size:** M. **Depends on:** 7.11, 7.06

**Client messages:** MSG_QUESTREADYTOTURNIN, MSG_UPDATEUSERHIDDENQUESTS, MSG_REQUESTACTIVEMAPQUESTS, MSG_QUESTFINDEROPTION, MSG_REQUESTNEXTCLOSESTQUEST, MSG_ADDQUESTFINDER, MSG_GETNEXTWORLD, MSG_CANACQUIREWORLDELIXIR

**Acceptance**

- [ ] Hidden set round-trips
- [ ] Real client: hidden quest stays hidden; map markers; dungeon auto-grant quest completes entry waypoint

### Detailed spec from QST-24: Quest helper extras: ready-to-turn-in, hidden quests, map, finder, next world

The quest book's secondary features work: turn-in markers, hiding quests, active quests on the map, the quest finder and the 'next world' hint.

**Deliverables**

- MSG_QUESTREADYTOTURNIN when only a final persona turn-in goal remains; SENDQUEST.ReadyToTurnIn on resume.
- HandleUpdateUserHiddenQuests (MSG_UPDATEUSERHIDDENQUESTS Data blob -> character_quest_hidden); resume honors it.
- HandleRequestActiveMapQuests (MSG_REQUESTACTIVEMAPQUESTS) with an ActiveMapQuestsCommon reply; MSG_QUESTFINDEROPTION flag; MSG_REQUESTNEXTCLOSESTQUEST -> MSG_ADDQUESTFINDER with an NPCDataList of QuestFinderNPCData (zone, location, first and last name keys) built from zone_object.
- MSG_GETNEXTWORLD from a mainline-progress table; auto-grant of zone-entry dungeon quests (quest requirement ReqInZone) on attach.

**Client messages:** MSG_QUESTREADYTOTURNIN, MSG_UPDATEUSERHIDDENQUESTS, MSG_REQUESTACTIVEMAPQUESTS, MSG_QUESTFINDEROPTION, MSG_REQUESTNEXTCLOSESTQUEST, MSG_ADDQUESTFINDER, MSG_GETNEXTWORLD, MSG_CANACQUIREWORLDELIXIR

**Data sources**

- Type dump ActiveMapQuestsCommon, NPCDataList, QuestFinderNPCData, HiddenQuestsBehavior
- Locale OrderedWorlds.lang

**Database tables**

- character_quest_hidden
- zone_object

**Acceptance**

- [ ] Unit test: the hidden-quest set persists and round-trips.
- [ ] Client: hide a quest in the book, relog, and it stays hidden. The world map shows markers for active quest NPCs in the current zone. Entering a dungeon with an auto-grant quest adds it and completes its entry waypoint in the same attach.

**Risks**

- Blob formats for the hidden-quests Data and active-map replies are unverified.

## 10.19 Path actions (WLD-17 part 2)

**Goal:** Walkers pause, emote, despawn, turn, change path and post zone events at their nodes.

**Size:** M. **Depends on:** 10.14

**Client messages:** MSG_ENTERSTATE, MSG_SERVERTELEPORT, MSG_MOVESTATE, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [x] Every path action class the server runs is read from the template (PathActionsTest.ReadsEveryActionClassTheServerRunsAndSkipsTheRest, PathActionsTest.AWalkingTemplateCarriesItsActionsAndItsChanceAndTimeToPause)
- [x] Each action runs on its node and chance (PathWalkersTest.APauseHoldsAWalkerAtItsNodeAndThenItWalksOn and the eight tests after it)
- [x] Wizards in sight are sent the snap and the emote, and a zone event fires each wizard's triggers (GameSessionObjectChangesTest.ASnappedWalkerItsStatesAndItsZoneEventsReachTheWizardWhoSeesIt, GameSessionObjectChangesTest.AZoneEventWaitsForAWizardStandingInTheWorld)
- [ ] Real client: a walker with an emote action plays it at its node and walks on

### Detailed spec: Path actions

PathBehaviorTemplate carries m_actionList, m_pauseChance and m_timeToPause, which 10.14 left unread. Of the 7,385 r806919 templates the catalog lists with a PathBehavior, 106 carry actions, 167 in all, which `client template --holding <class>` counts class by class: PathActionZoneEvent (46), PathActionDespawn (41), PauseAction (29, m_fMinPauseTime and m_fMaxPauseTime), PathActionEmote (18, m_sEmoteName, m_fMinTime, m_fMaxTime), TeleportToNode (15, m_nDestinationNodeID, m_bContinuePathing), PathActionChangeDirection (14), PathActionChangePath (3, m_nDestinationPathID, m_nDestinationNodeID) and PathActionEnterState (1, with an empty m_sStateName). No template ships PathActionStop. Every action names the path (m_nPathID) and node (m_nNodeID) it fires on, a priority (m_nPriority, 100 on every shipped action) and a percent chance (m_nChance). 377 of the 7,375 PathBehaviorTemplates give a nonzero m_pauseChance and m_timeToPause.

**Deliverables**

- `client template --holding <class> [text]` prints every template that holds an object of a class, with how many.
- src/server/game/Zones/PathActions.h/.cpp: reads the action list and finds the actions keyed to a node, highest priority first. PathActionEnterState is not run: its one shipped action names no state.
- PathMovementGenerator stops on each node reached when asked and hands back the time left over, and can hold, reverse and jump to a node.
- PathWalkers runs a node's actions as a walker reaches it, each on its chance: pause and the template's chance to pause hold it; an emote holds it in the Emoting state of NPCMobileStates with the EmoteStateOverrideInfo that names the animation, refused for an animation the install's list lacks, and returns it to Idle as it walks on; a zone event goes to the instance; a despawn hands the object back to its spawner, which brings it back on its respawn time; a turn reverses it; a path change carries it onto another path; a jump snaps it to a node and stands it there unless m_bContinuePathing.
- GameSession sends a snapped walker with MSG_SERVERTELEPORT and a walker's state with MSG_ENTERSTATE to the wizards who see it, and posts a walker's zone event into each in-world wizard's own triggers through ZoneTriggerMgr, without walking it through a door.

**Data sources**

- ObjectData PathBehaviorTemplate m_actionList, m_pauseChance, m_timeToPause
- StateData/NPCMobileStates.xml: the Primary category's Idle and Emoting (an EmoteGameState that returns to Idle)

**Acceptance**

- [x] Unit: each action class the server runs is read with its path, node, priority, chance and own fields, PathActionEnterState is skipped, and a walking template carries its actions and pause chance (PathActionsTest.ReadsEveryActionClassTheServerRunsAndSkipsTheRest, PathActionsTest.AWalkingTemplateCarriesItsActionsAndItsChanceAndTimeToPause)
- [x] Unit: a pause and the template's chance to pause hold a walker at the node, an emote enters Emoting with its data and returns to Idle, an emote the client cannot play is skipped, a zone event is posted only when its chance is rolled, a despawn is asked for, a turn walks it back, a path change carries it onto another path, and a jump snaps it to a node and stands it there or walks it on (PathWalkersTest.APauseHoldsAWalkerAtItsNodeAndThenItWalksOn, TheTemplatesChanceToPauseHoldsAWalkerOnAnyNode, AnEmotePlaysInEmotingAndEndsInIdleAsTheWalkerWalksOn, AnEmoteTheClientCannotPlayIsSkipped, AZoneEventGoesToTheInstanceOnlyWhenItsChanceIsRolled, ADespawnIsAskedForAndTheWalkerStops, ATurnSendsAWalkerBackTheWayItCame, APathChangeCarriesAWalkerOntoAnotherPath, AJumpSnapsAWalkerToAnotherNodeAndStandsItThereWhenItIsNotToWalkOn, AJumpThatWalksOnCarriesOnFromTheNodeItSnappedTo; PathMovementGeneratorTest.StopsOnEachNodeItReachesAndHandsBackTheTimeLeftOver, HoldsTurnsBackAndJumpsToAnotherNode)
- [x] Unit: a wizard is sent MSG_SERVERTELEPORT, MSG_MOVESTATE and MSG_ENTERSTATE only for walkers it sees, and a walker's zone event fires the wizard's trigger and shows its notify text once the wizard stands in the world (GameSessionObjectChangesTest.ASnappedWalkerItsStatesAndItsZoneEventsReachTheWizardWhoSeesIt, GameSessionObjectChangesTest.AZoneEventWaitsForAWizardStandingInTheWorld)
- [ ] Real client: a walker with a PathActionEmote, such as KT-Shopkepper-Ambient-Walker, plays its emote at its node, holds for the emote's time and walks on, seen the same on two clients

**Risks**

- An action's m_nNodeID is matched to the node's m_id, as a spawn's specific start node is; the real-client check confirms it.
- Priority is taken as order only; every shipped action has priority 100.

## 10.20 Zone tokens

**Goal:** Zones and wizards keep the zone tokens their zone data defines, triggers change and test them, and the client's objective tracker shows them.

**Size:** M. **Depends on:** 10.01, 7.04

**Client messages:** MSG_AddZoneToken, MSG_ModifyZoneToken, MSG_RemoveZoneToken

**Acceptance**

- [ ] Token definitions read from zone data; a container that does not read is refused whole
- [ ] Trigger results change a token and ReqZoneToken gates on it
- [ ] The client is sent each token as it is added, changed and removed
- [ ] A wizard's own tokens are saved with its character
- [ ] Real client: the objective tracker shows a token and its count change

### Detailed spec: Zone tokens

ZoneTokenBoolean, ZoneTokenCount and ZoneTokenTimer sit in a placement's token container, each with a storage that keeps it per wizard or per zone instance, and fire events on set, change, empty, full or expiry. Trigger results ResZoneTokenEnable, ResZoneTokenDisable and ResZoneTokenModify change them, ReqZoneToken tests them, and the client's AddZoneToken, ModifyZoneToken, RemoveZoneToken and ResetZoneToken handlers draw the zone objective tracker a dungeon or event shows.

**Deliverables**

- Token definitions read from zone_object.token_container, with kind, storage and bounds, refusing what the server cannot keep.
- Token state per wizard and per zone instance, moved by the three trigger results through the results runner, and ReqZoneToken in the requirement engine.
- The token view a wizard is sent, adding, moving and removing tokens as their state changes and dropping them all when the zone changes.
- A wizard's own tokens saved with its character and loaded when it enters the zone.

**Data sources**

- Zone placements' token containers and the triggers that name their tokens, which `client` counts per zone

**Acceptance**

- [ ] Unit: a token definition reads its kind, storage and bounds and a container is refused whole when one token does not read
- [ ] Unit: token states move as the client's own tokens do, and ReqZoneToken passes only while the wizard holds the token
- [ ] Unit: a wizard is sent MSG_AddZoneToken, MSG_ModifyZoneToken and MSG_RemoveZoneToken as states change, and its tokens are dropped when its zone changes
- [ ] Unit: a wizard's own tokens save with its character and come back after a relog
- [ ] Real client: entering a zone whose triggers count a token shows it in the objective tracker, and the count changes as the trigger fires

**Risks**

- ZoneTokenTimer and the token hook lists are filled by no shipped zone in r806919, so they are read but have no real-client check here.

## 10.21 Interactive world objects

**Goal:** Ritual objects, hot-and-cold objects, proximity and area behaviors, timed despawns and effect timers work for each wizard.

**Size:** M. **Depends on:** 10.01, 10.03, 10.04, 10.07, 13.04

**Client messages:** MSG_CASTRITUAL, MSG_CANTRIPHOTCOLD, MSG_ENTERSTATE

**Acceptance**

- [ ] A ritual cantrip cast at a ritual object gives its loot and state to a wizard who meets its requirements
- [ ] The server sends hot-and-cold the distance band
- [ ] Area, proximity, timed despawn and effect timer behaviors run
- [ ] Real client: a ritual object is used and changes state for that wizard only

### Detailed spec: Interactive world objects

RitualObjectBehavior has 82 placements in 10 zones, each with requirements, results, loot, states and a required cantrip or school. A ritual starts when the wizard casts a ritual cantrip at the object, which the client sends as MSG_CASTRITUAL, so casting cantrips (13.04) comes first. Every shipped ritual's result list is null, so a ritual's effect is its loot and its state change. HotColdObjectBehavior has the server send MSG_CANTRIPHOTCOLD, BeneficialProximityBehavior applies an effect near the object, AreaBehavior turns on behind m_enableReqs, DespawnTimedBehavior removes an object after its time, GameEffectTimerDisplayBehavior (43 placements in 9 zones) shows a timer, and ZoneTickEffectTemplate is the environmental damage a zone ticks.

**Deliverables**

- Each behavior read from its template with the fields the server runs.
- A ritual cast with MSG_CASTRITUAL: its requirements through the requirement engine, its loot through LootMgr, its state change for that wizard through its Dynamods, and its result list through the results runner should a zone fill one.
- MSG_CANTRIPHOTCOLD sent by the server as a wizard nears or leaves a hot-and-cold object.
- Hot-and-cold, proximity, area, timed despawn, effect timer and zone tick behaviors run in the zone's update.

**Data sources**

- ObjectData templates holding these behaviors, which `client template --holding <class>` lists
- Zone placements of those templates

**Acceptance**

- [ ] Unit: every behavior class here is read with the fields the server runs
- [ ] Unit: a MSG_CASTRITUAL from a wizard who meets a ritual object's requirements and casts its cantrip or school gives its loot and state; one who does not is refused and nothing changes
- [ ] Unit: the server sends MSG_CANTRIPHOTCOLD with the band of the wizard's distance to the object as it moves
- [ ] Unit: an area behavior turns on only for wizards meeting m_enableReqs, a proximity effect applies inside its range only, a timed despawn removes its object on time, and a zone tick effect damages on its period
- [ ] Real client: casting a ritual cantrip at a ritual object plays its state change and gives its loot to that wizard, while a second client sees it unchanged

**Risks**

- Rituals wait on 13.04's cantrip casting; the other behaviors here do not.

## 10.22 Lua script host

**Goal:** The server runs the install's own Lua scripts for objects, quests and results.

**Size:** L. **Depends on:** 10.01, 6.16

**Client messages:** MSG_ENTERSTATE, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [ ] Scripts load from the install in a sandbox
- [ ] Every script API function the scripts call is listed, and the host binds them or names the missing ones
- [ ] Object, quest and result scripts run
- [ ] Real client: an object with a server script runs it

### Detailed spec: Lua script host

Root.wad ships 158 Scripts/*.lua: a Server.lua, Client.lua and Messages.lua for each of the ten kiosk minigames, 32 tutorial scripts, and a script API. ScriptBehaviorTemplate (m_scriptFilename, m_clientScriptFilename) sits on objects in 11 zones, quests carry m_onStartQuestScript and m_onEndQuestScript, ResAsyncScript and ResSyncScript are quest results, and the client has a ScriptDebugger service of 20 messages. A host that runs these scripts gives phase 13's minigames and phase 14's tutorial their server logic instead of a rewrite of each.

**Deliverables**

- `client` lists every global function the install's scripts call, and which script calls it.
- A Lua host that reads scripts through the archive reader, with no file, process or network access, one state per running script, and a limit on the instructions each call may run.
- The script API the server-side scripts call, bound to the game's systems, with a call to a function not bound yet logged once by name.
- ScriptBehaviorTemplate objects start their server script when they spawn; m_onStartQuestScript and m_onEndQuestScript run as a quest starts and ends; ResAsyncScript and ResSyncScript run through the results runner.

**Data sources**

- Root.wad Scripts/*.lua
- ObjectData templates holding ScriptBehaviorTemplate, quests' script fields

**Acceptance**

- [ ] Unit: a script loads from a fixture archive, cannot open files or start processes, and a call that runs past its limit is stopped and logged
- [ ] Unit: `client` lists each function the install's scripts call; the host binds them or logs the first call to one it lacks by name
- [ ] Unit: a ScriptBehavior object starts its script on spawn, a quest's start and end scripts run, and ResAsyncScript and ResSyncScript run in order with the rest of a result list
- [ ] Real client: an object with a ScriptBehavior in one of the 11 zones that place one does what its server script says, seen on the client

**Risks**

- The Lua version the scripts are written for decides the library; it is settled under Decisions in doc/ARCHITECTURE.md when this milestone starts.

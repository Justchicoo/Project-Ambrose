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

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Oversized.** 10.16 authoring 3-5 quests plus the automated chain test with a golden sequence (M). Fine only if 7.05 tooling is solid; otherwise split.

## 10.01 Results engine v1 (QST-15)

**Goal:** Data-driven results and quest event bus.

**Size:** M. **Depends on:** 7.11, 7.04, 6.14, 9.04

**Client messages:** MSG_NPCSPEECH, MSG_PLAYCINEMATIC

**Acceptance**

- [ ] Result list runs in order; gated result skipped
- [ ] Completing WC-MAIN-C01-003_Goal0 publishes 'GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0'
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
- [ ] Unit test: completing WC-MAIN-C01-003_Goal0 in a fixture publishes 'GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0'.
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

**Goal:** ENTERSTATE/LEAVESTATE and per-player persisted overrides.

**Size:** M. **Depends on:** 6.16, 10.01

**Client messages:** MSG_ENTERSTATE, MSG_LEAVESTATE

**Acceptance**

- [ ] Real client: after 'OpenGateToUW' the Unicorn Way gate is 'IdleOpen' for that player only

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
- [ ] Unit: requirement re-evaluation sends NEWOBJECT/ADDOBJECT or REMOVEOBJECT only for objects whose result changed

**Risks**

- Behavior of IgnoreIfCurrentStateIsOff and the Data (EmoteStateOverrideInfo) field is unverified

## 10.04 Requirement-gated visibility (WLD-19 part 2)

**Goal:** Quest-gated objects per player.

**Size:** M. **Depends on:** 10.03

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT

**Acceptance**

- [ ] Only changed objects resend
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
- [ ] Unit: requirement re-evaluation sends NEWOBJECT/ADDOBJECT or REMOVEOBJECT only for objects whose result changed

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
- Party kill credit (MSG_GROUPQUESTCREDIT) is not covered.

## 10.07 LootMgr core (QST-19 + CMB-20 merged)

**Goal:** Weighted grouped loot tables.

**Size:** M. **Depends on:** 8.09, 2.07, 4.15, 4.16

**Acceptance**

- [ ] 100000 seeded rolls of a 10% entry land in 9.5-10.5%
- [ ] School-gated spell entry only for that school
- [ ] With Rate.Drop.Item at 2, the same 10% item entry lands in 19-21% without a restart

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

- [ ] Unit test with a seeded RNG: table rolls are reproducible, and a school-gated spell entry is only granted to that school.
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

- [ ] Unit test: 100000 seeded rolls of a table with a 10% entry land within 9.5–10.5%
- [ ] Unit test: after `.settings set Rate.Drop.Item 2`, 100000 seeded rolls of the same 10% item entry land within 19–21% without a restart
- [ ] Real client: after victory the loot popup lists the rolled items with icons, and the items are in the backpack after relog (characters DB)

**Risks**

- Drop contents are server-side and must be authored clean-room; only table names are visible in client templates

## 10.08 Quest rewards (QST-19)

**Goal:** Gold, XP, items, spells on completion.

**Size:** M. **Depends on:** 10.07, 10.01, 8.02, 8.05

**Client messages:** MSG_LOOT, MSG_QUESTREWARDS, MSG_ITEMDROP, MSG_QUESTOFFER, MSG_SENDQUEST

**Acceptance**

- [ ] Real client: reward popup with 100 gold, 50 XP and item; Fire wizard gets only the fire spell popup
- [ ] Changing Rate.Gold.Quest or Rate.XP.Quest applies to the next quest reward without a restart

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
- Vendor stock lists are NOT in the client. The world DB authors them (GM command or hand-written custom SQL), and they are never copied from another project's data

**Database tables**

- world.npc_vendor
- characters.character_inventory
- characters.character_currency

**Acceptance**

- [ ] Unit: buying with insufficient gold is rejected; sell value matches formula; item not in vendor list is rejected and logged
- [ ] Unit: setting Shop.SellValuePercent to a new value with `.settings set` changes the next sell price to match, without a restart
- [ ] Client: clicking a vendor NPC in Wizard City shows the equipment shop window listing the DB items with correct prices
- [ ] Client: buying an item plays the confirm, deducts gold in the HUD, and the item appears in the backpack; selling reverses it; closing the window lets the wizard move again

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

## 10.14 Path-walking NPCs (WLD-17)

**Goal:** Server-driven patrols.

**Size:** M. **Depends on:** 6.16, 6.12, 4.15

**Client messages:** MSG_SERVERMOVE, MSG_MOVESTATE, MSG_SERVERTELEPORT

**Acceptance**

- [ ] 2-node path at speed s takes distance/s +-1 tick
- [ ] Empty Map sends nothing
- [ ] `.reload zone_path` reroutes walking NPCs live; a failed reload keeps the old paths
- [ ] Real client: patrol identical on two clients

### Detailed spec from WLD-17: Path-walking NPCs

NPCs with path data walk their routes on the server, and every client sees the same motion.

**Deliverables**

- zone_extractor: decode pathData.xml (PathManager::PathTemplateList, 0x3B6A23E8) and pathNodeData.bin into world.zone_path and world.zone_path_node
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

- [ ] Unit: an NPC on a 2-node path with speed s covers the distance in distance/s seconds (+-1 tick)
- [ ] Unit: a Map with no players sends no movement packets
- [ ] Unit: `.reload zone_path` with a moved node sends a walking NPC to the new node position from its next leg without a restart, and a reload with a broken node keeps the old path
- [ ] Real client: a patrolling NPC in a Wizard City street walks the same route on clients A and B at the same moment and does not pop on arrival

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

- Retail goal structure, dialog-key-to-quest mapping and rewards must be reconstructed from observed behavior without importing any other project's data, and may not match retail exactly.
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

- Retail goal structure, dialog-key-to-quest mapping and rewards must be reconstructed from observed behavior without importing any other project's data, and may not match retail exactly.
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

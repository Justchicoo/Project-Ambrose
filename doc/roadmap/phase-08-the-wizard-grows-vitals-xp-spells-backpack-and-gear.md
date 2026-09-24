<!-- Project Ambrose by Imjustchico: Roadmap phase 8, The wizard grows: vitals, XP, spells, backpack and gear. -->

# Phase 8: The wizard grows: vitals, XP, spells, backpack and gear

**Done when:** GM commands grant gold, XP, spells and items that show live. Potions work. Equipping a hat changes the model for nearby players. Decks and treasure cards can be built, and trainers teach spells.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 8.01 | Live vitals, gold, potions (WIZ-6) | M | 5.05, 6.04, 4.16 |
| 8.02 | Experience and level-up (WIZ-7) | M | 8.01, 4.16 |
| 8.03 | School and secondary school (WIZ-8) | M | 8.02, 7.07 |
| 8.04 | SpellMgr: spell templates (CMB-2 spells + WIZ-9 extractor) | M | 5.01, 3.11, 4.15 |
| 8.05 | Spellbook (WIZ-9) | M | 8.04, 5.05 |
| 8.06 | Item template extractor, core rows (WIZ-10 part 1) | M | 7.01, 4.15 |
| 8.07 | Item requirements, effects and set bonuses (WIZ-10 part 2) | M | 8.06, 6.10 |
| 8.08 | Backpack: item instances, add/remove/trash (WIZ-11 part 1) | M | 8.06, 6.04, 4.16 |
| 8.09 | Backpack: stacks, locks, overflow (WIZ-11 part 2) | M | 8.08 |
| 8.10 | Equip and unequip (WIZ-12) | M | 8.09, 8.07, 7.04, 6.01 |
| 8.11 | Decks (WIZ-14) | M | 8.05, 8.10 |
| 8.12 | Treasure cards (WIZ-15) | M | 8.11 |
| 8.13 | Spell training at trainers (WIZ-16) | M | 8.05, 8.02, 7.07, 4.15, 4.16 |
| 8.14 | Versionable BINd encoder, byte-exact (OBJ-7) | M | 3.11 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Oversized.** 8.04 SpellMgr decoding all 18173 Spells entries (M). Same corpus-triage problem.

## 8.01 Live vitals, gold, potions (WIZ-6)

**Goal:** HUD updates immediately.

**Size:** M. **Depends on:** 5.05, 6.04, 4.16

**Client messages:** MSG_UPDATEHEALTH, MSG_UPDATEMANA, MSG_UPDATEGOLD, MSG_UPDATEPOWERPIP, MSG_UPDATESHADOWPIPRATING, MSG_UPDATEPOTIONS, MSG_USEPOTION, MSG_ELIXIRSTATECHANGE, MSG_UPDATEMAXSHADOWPIPS, MSG_UPDATEPIPCONVERSION, MSG_UPDATEARCHMASTERY

**Acceptance**

- [ ] ModifyGold clamps at pouch
- [ ] USEPOTION with 0 charges changes nothing
- [ ] Changing Potion.RestoreFraction applies to the next potion without a restart
- [ ] Real client: '.character gold 500' updates live; heal animates; potion restores and empties by one

### Detailed spec from WIZ-6: Live vitals, gold and potions

Health, mana, gold, power-pip and potion changes on the server show up immediately in the client HUD, and players can drink potions.

**Deliverables**

- src/server/game/Entities/Player/Player: SetHealth, SetMana, ModifyGold (clamped to the gold pouch), SetPowerPip, SetShadowPipRating, each sending its update message and marking stats dirty
- USEPOTION handler: needs potion_charge >= 1, restores health and mana by Potion.RestoreFraction, a live setting, sends UPDATEPOTIONS, UPDATEHEALTH and UPDATEMANA
- Potion refill timer on Potion.RefillInterval, a live setting that applies from the next refill
- ELIXIRSTATECHANGE sender stub, used later by EXT elixirs
- PlayerScript hooks OnGoldChanged and OnHealthChanged
- cs_character additions: .character mana, .character potion

**Client messages:** MSG_UPDATEHEALTH, MSG_UPDATEMANA, MSG_UPDATEGOLD, MSG_UPDATEPOWERPIP, MSG_UPDATESHADOWPIPRATING, MSG_UPDATEPOTIONS, MSG_USEPOTION, MSG_ELIXIRSTATECHANGE, WIZARD2 MSG_UPDATEMAXSHADOWPIPS, WIZARD2 MSG_UPDATEPIPCONVERSION, WIZARD3 MSG_UPDATEARCHMASTERY

**Data sources**

- world.player_level_stats

**Database tables**

- characters.character_stats

**Acceptance**

- [ ] Unit test: ModifyGold above m_baseGoldPouch clamps and reports the overflow
- [ ] Unit test: USEPOTION with 0 charges changes nothing
- [ ] Unit test: after Potion.RestoreFraction changes, the next potion restores the new fraction without a restart
- [ ] Real client: '.character gold 500' makes the backpack gold counter update without relogging. Damaging then '.character heal' makes the health globe animate up (DisplayDiff=1 floats the number). Clicking a filled potion restores health and mana, and the potion bottle empties by one.

**Risks**

- The real potion restore amounts and refill rate are server-side values not in the client files. They are setting defaults and need a source.

## 8.02 Experience and level-up (WIZ-7)

**Goal:** XP bar, level-up effect, new base stats.

**Size:** M. **Depends on:** 8.01, 4.16

**Client messages:** MSG_UPDATEXP, MSG_LEVELUP, MSG_UPDATETRAINING, MSG_UPDATEOVERFLOWXP, MSG_LOCKLEVEL, MSG_PARTYLEVELUP

**Acceptance**

- [ ] 2.5 levels from level 1 ends at 3 with remainder and training points
- [ ] Max level goes to overflow_xp; locked level does not change
- [ ] `.settings set Rate.XP.Quest 2` doubles the next quest XP grant without a restart
- [ ] Real client: level-up effect seen by a second client

### Detailed spec from WIZ-7: Experience and level-up

Gaining XP fills the XP bar, crossing a threshold levels the wizard up with a full heal and new base stats, and other players see the level-up effect.

**Deliverables**

- src/server/game/Entities/Player/PlayerLevel: GiveXP(amount, source) with bonus percentage (m_xpPercentIncrease), multi-level carry-over, a cap at m_maxSchoolLevel, overflow XP past the cap, and the level lock flag
- Every XP grant passes through Rate.XP.<source> (for example Rate.XP.Quest, Rate.XP.Kill), a live setting multiplier read at grant time
- A public API for QST and CMB rewards to call
- LEVELUP broadcast to the zone plus UPDATEXP, UPDATETRAINING, UPDATEHEALTH, UPDATEMANA, UPDATEPOWERPIP to self, and PET MSG_PETENERGYMAX coordinated with PET
- LOCKLEVEL handler (WIZARD3)
- PlayerScript OnLevelChanged and OnGiveXP hooks
- cs_character .character xp / .character level (replacing the WIZ-5 stubs)
- src/test/server/game/PlayerLevelTest.cpp

**Client messages:** MSG_UPDATEXP, MSG_LEVELUP, MSG_UPDATETRAINING, WIZARD3 MSG_UPDATEOVERFLOWXP, WIZARD3 MSG_LOCKLEVEL, GAME MSG_PARTYLEVELUP

**Data sources**

- world.player_level_stats (m_xpToLevel, m_trainingPoints)

**Database tables**

- characters.character_stats

**Acceptance**

- [ ] Unit test: GiveXP enough for 2.5 levels from level 1 ends at level 3 with the right remainder and training points gained for both levels
- [ ] Unit test: at max level the XP goes to overflow_xp and no LEVELUP is sent
- [ ] Unit test: a locked level accrues no level changes
- [ ] Unit test: after `.settings set Rate.XP.Quest 2`, GiveXP(100, Quest) grants 200 without a restart
- [ ] Real client: '.character xp <amount>' fills the XP bar. Crossing the threshold plays the level-up effect and sound, the level number on the character sheet goes up, and health and mana refill to the new maximums. A second client nearby sees the level-up effect on the first.

**Risks**

- The LEVELUP Data field format is unknown. The reference sends the literal '0000000000'.
- Whether UPDATEXP XP is the delta or the new total is unverified. The reference sends the delta plus OldXP.

## 8.03 School and secondary school (WIZ-8)

**Goal:** Set school and choose secondary focus.

**Size:** M. **Depends on:** 8.02, 7.07

**Client messages:** MSG_UPDATESCHOOL, MSG_CHOOSEFOCUS, MSG_NOTIFYSCHOOLFOCUS, MSG_REGISTRAR, MSG_UPDATEGENDER

**Acceptance**

- [ ] Secondary below min level rejected
- [ ] SchoolHash equals KiStringHash of name
- [ ] Real client: '.character school Ice' changes the sheet icon

### Detailed spec from WIZ-8: School, secondary school and registrar

A wizard can have its primary school set and change or pick a secondary school of focus, and the UI updates to match.

**Deliverables**

- src/server/game/Entities/Player/PlayerSchool: SetPrimarySchool (GM only), ChooseSecondarySchool with requirement checks (magic_school_template.min_level)
- CHOOSEFOCUS client handler -> NOTIFYSCHOOLFOCUS broadcast, UPDATESCHOOL to self
- REGISTRAR sender when an NPC with a registrar service is interacted with (interaction plumbing from QST/WLD)
- UPDATEGENDER for GM gender change
- cs_character .character school, .character gender

**Client messages:** MSG_UPDATESCHOOL, MSG_CHOOSEFOCUS, MSG_NOTIFYSCHOOLFOCUS, MSG_REGISTRAR, MSG_UPDATEGENDER

**Data sources**

- world.magic_school_template
- Root.wad NPCServices.xml (root class hash 0x32a408e1, not in the r806919 dump; decoding needed for registrar and trainer service data)

**Database tables**

- characters.character_stats

**Acceptance**

- [ ] Unit test: choosing a secondary school below its min level is rejected
- [ ] Unit test: UPDATESCHOOL SchoolHash matches the hash of the school name string
- [ ] Real client: '.character school Ice' makes the character sheet school icon and deck school colors change after the update. Choosing a secondary school at the registrar NPC shows the new focus on the character sheet.

**Risks**

- The UPDATESCHOOL Data and REGISTRAR Data blob formats are not reverse-engineered.
- NPCServices.xml uses a class the type dump does not contain. The registry must be taught that class before the file can be read.

## 8.04 SpellMgr: spell templates (CMB-2 spells + WIZ-9 extractor)

**Goal:** All SpellTemplates indexed by id and name.

**Size:** M. **Depends on:** 5.01, 3.11, 4.15

**Acceptance**

- [ ] 18173 Spells entries decode, or failures listed by class
- [ ] 'Fire Cat - Amulet' has kDamage, Fire, kEnemySingle
- [ ] Name-hash and id lookups agree; '.spell info Fire Cat' prints school, rank, accuracy, effects
- [ ] `.spell reload` with a decode failure keeps the old templates and lists every failure

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

### Detailed spec from WIZ-9: Spell template extractor and spellbook

A wizard owns a persistent set of known spells that show up in the in-game spellbook, and spells can be granted or removed live.

**Deliverables**

- src/tools/extractor/SpellExtractor: Root.wad Spells/**.xml (18173 entries) -> world.spell_template (template id, name, name hash, school, pip cost, treasure flag, display key)
- src/server/game/Spells/SpellMgr (sSpellMgr): lookup by template id and by name hash
- data/sql/updates/db_characters: character_spell
- ClientSpellbookBehavior (m_spellIDList) built into the player object
- SPELLLIST, ADDSPELLTOBOOK and REMOVESPELLFROMBOOK senders; a LearnSpell API for quests and results
- cs_learn.cpp: .learn <spell>, .unlearn <spell>

**Client messages:** MSG_SPELLLIST, MSG_ADDSPELLTOBOOK, MSG_REMOVESPELLFROMBOOK

**Data sources**

- Root.wad Spells/ (6375 top-level, TreasureCards 1756, Tiered Spells 2579, Cantrips 162, etc.)

**Database tables**

- world.spell_template
- characters.character_spell

**Acceptance**

- [ ] Unit test: the name-hash lookup returns the same template as the id lookup for a fixture spell
- [ ] Unit test: learning a known spell twice does not duplicate it
- [ ] Real client: '.learn Fire Cat' makes Fire Cat appear on the Fire page of the spellbook without relogging, and it is still there after relogging. '.unlearn' removes it.

**Risks**

- Whether ADDSPELLTOBOOK SpellID is the template id or the spell-name hash is unverified. The reference passes the template id there but name hashes in the treasure and exclusion messages.
- The SPELLLIST Data format is not reverse-engineered.

## 8.05 Spellbook (WIZ-9)

**Goal:** Persistent known spells shown live.

**Size:** M. **Depends on:** 8.04, 5.05

**Client messages:** MSG_SPELLLIST, MSG_ADDSPELLTOBOOK, MSG_REMOVESPELLFROMBOOK

**Acceptance**

- [ ] Learning twice does not duplicate
- [ ] Real client: '.learn Fire Cat' appears on the Fire page and survives relog; '.unlearn' removes

### Detailed spec from WIZ-9: Spell template extractor and spellbook

A wizard owns a persistent set of known spells that show up in the in-game spellbook, and spells can be granted or removed live.

**Deliverables**

- src/tools/extractor/SpellExtractor: Root.wad Spells/**.xml (18173 entries) -> world.spell_template (template id, name, name hash, school, pip cost, treasure flag, display key)
- src/server/game/Spells/SpellMgr (sSpellMgr): lookup by template id and by name hash
- data/sql/updates/db_characters: character_spell
- ClientSpellbookBehavior (m_spellIDList) built into the player object
- SPELLLIST, ADDSPELLTOBOOK and REMOVESPELLFROMBOOK senders; a LearnSpell API for quests and results
- cs_learn.cpp: .learn <spell>, .unlearn <spell>

**Client messages:** MSG_SPELLLIST, MSG_ADDSPELLTOBOOK, MSG_REMOVESPELLFROMBOOK

**Data sources**

- Root.wad Spells/ (6375 top-level, TreasureCards 1756, Tiered Spells 2579, Cantrips 162, etc.)

**Database tables**

- world.spell_template
- characters.character_spell

**Acceptance**

- [ ] Unit test: the name-hash lookup returns the same template as the id lookup for a fixture spell
- [ ] Unit test: learning a known spell twice does not duplicate it
- [ ] Real client: '.learn Fire Cat' makes Fire Cat appear on the Fire page of the spellbook without relogging, and it is still there after relogging. '.unlearn' removes it.

**Risks**

- Whether ADDSPELLTOBOOK SpellID is the template id or the spell-name hash is unverified. The reference passes the template id there but name hashes in the treasure and exclusion messages.
- The SPELLLIST Data format is not reverse-engineered.

## 8.06 Item template extractor, core rows (WIZ-10 part 1)

**Goal:** item_template for WizItemTemplate roots.

**Size:** M. **Depends on:** 7.01, 4.15

**Acceptance**

- [ ] Synthetic fixture maps to one row
- [ ] ~76,679 rows on the install; load time and memory logged
- [ ] `.reload item_template` applies an edited row live; a failing reload keeps the old store

### Detailed spec from WIZ-10: Item template extractor

Every equippable and backpack item in the user's client is available to the server as a typed template with requirements, equip effects, school, level and cost.

**Deliverables**

- src/tools/extractor/ItemExtractor: Root.wad ObjectData/**.xml where the root class is WizItemTemplate (76,679 files) -> world.item_template (m_templateID, m_objectName, m_displayName, m_nObjectType, m_adjectiveList, m_school, m_baseCost, m_rank, m_itemLimit, m_itemSetBonusTemplateID, m_numPrimaryColors, m_numSecondaryColors), child tables for equip requirements and equip effects (serialized GameEffectInfo, typed columns where the class is known), and the behaviors blob
- ItemSetBonusTemplate (42 files) -> world.item_set_bonus
- src/server/game/Items/ItemMgr (sItemMgr) on a 4.15 reloadable store: `.reload item_template` builds and validates the templates off to the side and swaps them, and a failure keeps the old store and reports every error
- src/test/tools/ItemExtractorTest.cpp

**Data sources**

- Root.wad ObjectData/**.xml (WizItemTemplate 76679, ItemSetBonusTemplate 42, also ReagentItemTemplate 867 and PetSnackItemTemplate 478 kept for other domains)

**Database tables**

- world.item_template
- world.item_template_requirement
- world.item_template_effect
- world.item_set_bonus

**Acceptance**

- [ ] Unit test: a synthetic WizItemTemplate BINd fixture maps to one item_template row with its requirements and effects
- [ ] On the user's install: item_template has around 76,679 rows and every requirement or effect class is present in the type registry (unknown hashes fail the import, never skipped silently)
- [ ] sItemMgr load time and memory are logged at gameserver startup
- [ ] `.reload item_template` applies an edited row without a restart, and a reload that meets an unknown class hash keeps the old store and reports it

**Risks**

- Data volume: a full scan of Root.wad took minutes in exploration, so incremental import is needed.
- The Requirement and Result class variety is large. Shared requirement evaluation belongs to QST/FND, not WIZ.

## 8.07 Item requirements, effects and set bonuses (WIZ-10 part 2)

**Goal:** Child tables plus item_set_bonus.

**Size:** M. **Depends on:** 8.06, 6.10

**Acceptance**

- [ ] Every requirement/effect class is known; unknown hashes fail the import
- [ ] 42 ItemSetBonusTemplate rows
- [ ] A reload with an unknown requirement or effect hash keeps the old store

### Detailed spec from WIZ-10: Item template extractor

Every equippable and backpack item in the user's client is available to the server as a typed template with requirements, equip effects, school, level and cost.

**Deliverables**

- src/tools/extractor/ItemExtractor: Root.wad ObjectData/**.xml where the root class is WizItemTemplate (76,679 files) -> world.item_template (m_templateID, m_objectName, m_displayName, m_nObjectType, m_adjectiveList, m_school, m_baseCost, m_rank, m_itemLimit, m_itemSetBonusTemplateID, m_numPrimaryColors, m_numSecondaryColors), child tables for equip requirements and equip effects (serialized GameEffectInfo, typed columns where the class is known), and the behaviors blob
- ItemSetBonusTemplate (42 files) -> world.item_set_bonus
- src/server/game/Items/ItemMgr (sItemMgr) on a 4.15 reloadable store: `.reload item_template` builds and validates the templates off to the side and swaps them, and a failure keeps the old store and reports every error
- src/test/tools/ItemExtractorTest.cpp

**Data sources**

- Root.wad ObjectData/**.xml (WizItemTemplate 76679, ItemSetBonusTemplate 42, also ReagentItemTemplate 867 and PetSnackItemTemplate 478 kept for other domains)

**Database tables**

- world.item_template
- world.item_template_requirement
- world.item_template_effect
- world.item_set_bonus

**Acceptance**

- [ ] Unit test: a synthetic WizItemTemplate BINd fixture maps to one item_template row with its requirements and effects
- [ ] On the user's install: item_template has around 76,679 rows and every requirement or effect class is present in the type registry (unknown hashes fail the import, never skipped silently)
- [ ] sItemMgr load time and memory are logged at gameserver startup
- [ ] `.reload item_template` applies an edited row without a restart, and a reload that meets an unknown class hash keeps the old store and reports it

**Risks**

- Data volume: a full scan of Root.wad took minutes in exploration, so incremental import is needed.
- The Requirement and Result class variety is large. Shared requirement evaluation belongs to QST/FND, not WIZ.

## 8.08 Backpack: item instances, add/remove/trash (WIZ-11 part 1)

**Goal:** Persistent backpack shown in the client.

**Size:** M. **Depends on:** 8.06, 6.04, 4.16

**Client messages:** MSG_INVENTORYBEHAVIOR_ADDITEM, MSG_INVENTORYBEHAVIOR_REMOVEITEM, MSG_WIZINVENTORYCLIENTADD, MSG_WIZINVENTORYCLIENTREMOVE, MSG_TRASHINVENTORYITEM, MSG_LOOT

**Acceptance**

- [ ] Trashing an item not owned rejected
- [ ] Real client: '.additem <hat>' shows the icon and tooltip; trash persists across relog

### Detailed spec from WIZ-11: Backpack inventory

A wizard has a persistent backpack whose items show in the client, and items can be granted, trashed and locked.

**Deliverables**

- data/sql/updates/db_characters: item_instance (guid, owner, template, quantity, color layers, locked, flags) and character_inventory
- src/server/game/Items/Inventory: capacity from ClientWizInventoryBehavior m_numItemsAllowed plus Inventory.ExtraSlots, a live setting that applies from the next add, stacking by m_itemLimit, a global item GID allocator
- ClientWizInventoryBehavior (m_itemList) in the player object
- GAME INVENTORYBEHAVIOR_ADDITEM/REMOVEITEM and WIZARD WIZINVENTORYCLIENTADD/REMOVE senders, TRASHINVENTORYITEM handler, ITEMDROP when the backpack is full, REQUESTTOGGLELOCKITEM and ITEMLOCK handlers, UPDATEQUANTITY and SPLITQUANTITY for stackables, UPDATEEXTRAINVENTORY
- LOOT display message sender, used by CMB and QST
- cs_item.cpp: .additem <template> [count], .removeitem

**Client messages:** MSG_WIZINVENTORYCLIENTADD, MSG_WIZINVENTORYCLIENTREMOVE, MSG_ITEMDROP, MSG_ITEMLOCK, MSG_REQUESTTOGGLELOCKITEM, MSG_LOOT, MSG_RENTALUPDATE, MSG_SETRENTALTIMER, GAME MSG_INVENTORYBEHAVIOR_ADDITEM, GAME MSG_INVENTORYBEHAVIOR_REMOVEITEM, GAME MSG_TRASHINVENTORYITEM, GAME MSG_UPDATEQUANTITY, GAME MSG_SPLITQUANTITY, GAME MSG_COMBINEINVENTORYITEMS, WIZARD2 MSG_UPDATEEXTRAINVENTORY, WIZARD2 MSG_REMOVEITEMLOCKS

**Data sources**

- world.item_template

**Database tables**

- characters.item_instance
- characters.character_inventory

**Acceptance**

- [ ] Unit test: adding to a full backpack sends ITEMDROP and does not persist the item
- [ ] Unit test: raising Inventory.ExtraSlots lets the next add to a full backpack succeed without a restart
- [ ] Unit test: trashing an item not owned is rejected and logged
- [ ] Real client: '.additem <hat template>' makes a new hat icon appear in the backpack with the right name tooltip. Trashing it removes it and it stays gone after relogging. Locking an item shows the lock icon and hides the trash option.

**Risks**

- When the client uses GAME INVENTORYBEHAVIOR_* versus WIZARD WIZINVENTORYCLIENT* is unverified. The reference only sends the GAME variants.
- SerializedItem needs the 4-byte SerializerBinary wrapper (sniffer README). An unwrapped blob makes the client misallocate memory.

## 8.09 Backpack: stacks, locks, overflow (WIZ-11 part 2)

**Goal:** Stackables, locks, full-backpack drops.

**Size:** M. **Depends on:** 8.08

**Client messages:** MSG_ITEMDROP, MSG_ITEMLOCK, MSG_REQUESTTOGGLELOCKITEM, MSG_RENTALUPDATE, MSG_SETRENTALTIMER, MSG_UPDATEQUANTITY, MSG_SPLITQUANTITY, MSG_COMBINEINVENTORYITEMS, MSG_UPDATEEXTRAINVENTORY, MSG_REMOVEITEMLOCKS

**Acceptance**

- [ ] Full backpack sends ITEMDROP and does not persist
- [ ] Raising Inventory.ExtraSlots makes room on the next add without a restart
- [ ] Locked item shows lock icon and hides trash

### Detailed spec from WIZ-11: Backpack inventory

A wizard has a persistent backpack whose items show in the client, and items can be granted, trashed and locked.

**Deliverables**

- data/sql/updates/db_characters: item_instance (guid, owner, template, quantity, color layers, locked, flags) and character_inventory
- src/server/game/Items/Inventory: capacity from ClientWizInventoryBehavior m_numItemsAllowed plus Inventory.ExtraSlots, a live setting that applies from the next add, stacking by m_itemLimit, a global item GID allocator
- ClientWizInventoryBehavior (m_itemList) in the player object
- GAME INVENTORYBEHAVIOR_ADDITEM/REMOVEITEM and WIZARD WIZINVENTORYCLIENTADD/REMOVE senders, TRASHINVENTORYITEM handler, ITEMDROP when the backpack is full, REQUESTTOGGLELOCKITEM and ITEMLOCK handlers, UPDATEQUANTITY and SPLITQUANTITY for stackables, UPDATEEXTRAINVENTORY
- LOOT display message sender, used by CMB and QST
- cs_item.cpp: .additem <template> [count], .removeitem

**Client messages:** MSG_WIZINVENTORYCLIENTADD, MSG_WIZINVENTORYCLIENTREMOVE, MSG_ITEMDROP, MSG_ITEMLOCK, MSG_REQUESTTOGGLELOCKITEM, MSG_LOOT, MSG_RENTALUPDATE, MSG_SETRENTALTIMER, GAME MSG_INVENTORYBEHAVIOR_ADDITEM, GAME MSG_INVENTORYBEHAVIOR_REMOVEITEM, GAME MSG_TRASHINVENTORYITEM, GAME MSG_UPDATEQUANTITY, GAME MSG_SPLITQUANTITY, GAME MSG_COMBINEINVENTORYITEMS, WIZARD2 MSG_UPDATEEXTRAINVENTORY, WIZARD2 MSG_REMOVEITEMLOCKS

**Data sources**

- world.item_template

**Database tables**

- characters.item_instance
- characters.character_inventory

**Acceptance**

- [ ] Unit test: adding to a full backpack sends ITEMDROP and does not persist the item
- [ ] Unit test: raising Inventory.ExtraSlots lets the next add to a full backpack succeed without a restart
- [ ] Unit test: trashing an item not owned is rejected and logged
- [ ] Real client: '.additem <hat template>' makes a new hat icon appear in the backpack with the right name tooltip. Trashing it removes it and it stays gone after relogging. Locking an item shows the lock icon and hides the trash option.

**Risks**

- When the client uses GAME INVENTORYBEHAVIOR_* versus WIZARD WIZINVENTORYCLIENT* is unverified. The reference only sends the GAME variants.
- SerializedItem needs the 4-byte SerializerBinary wrapper (sniffer README). An unwrapped blob makes the client misallocate memory.

## 8.10 Equip and unequip (WIZ-12)

**Goal:** Paper-doll and public appearance.

**Size:** M. **Depends on:** 8.09, 8.07, 7.04, 6.01

**Client messages:** MSG_EQUIPITEM, MSG_EQUIPMENTBEHAVIOR_EQUIPITEM, MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM, MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM, MSG_EQUIPMENTBEHAVIOR_PUBLICUNEQUIPITEM

**Acceptance**

- [ ] Fire-only robe on an Ice wizard refused
- [ ] Occupied slot swaps back to the backpack
- [ ] Real client: hat changes model; second client sees it; persists

### Detailed spec from WIZ-12: Equip and unequip gear

Players can drag items between backpack and equipment slots, and other players see the appearance change.

**Deliverables**

- data/sql/updates/db_characters: character_equipment (slot, item guid)
- src/server/game/Items/Equipment: slot list (Hat, Robe, Boots, Wand, Athame, Amulet, Ring, Deck, Pet, Mount, etc., from the player EquipmentBehaviorTemplate and EquipSlot adjectives AND/OR/NOT), equip requirement checks (level, school) using the shared requirement evaluator, swap when the slot is in use
- ClientWizEquipmentBehavior (m_itemList, m_slotList, m_publicItemList) in the player object
- GAME EQUIPITEM handler and confirmation; EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM/PUBLICUNEQUIPITEM broadcast
- PlayerScript OnEquip and OnUnequip hooks

**Client messages:** GAME MSG_EQUIPITEM, GAME MSG_EQUIPMENTBEHAVIOR_EQUIPITEM, GAME MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM, GAME MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM, GAME MSG_EQUIPMENTBEHAVIOR_PUBLICUNEQUIPITEM

**Data sources**

- world.item_template (+ requirements)
- The player object template's EquipmentBehaviorTemplate (source file in ObjectData unverified)

**Database tables**

- characters.character_equipment

**Acceptance**

- [ ] Unit test: equipping a Fire-only robe on an Ice wizard is refused and the item stays in the backpack
- [ ] Unit test: equipping into an occupied slot moves the old item back to the backpack
- [ ] Real client: dragging a hat onto the hat slot changes the paper-doll and the 3D model. A second nearby client sees the new hat. It is still equipped after relogging.

**Risks**

- Where the player slot definitions live in Root.wad is unverified. ObjectData/BasicMobileEquipment.xml is the only EquipmentTemplate root found.
- Public item serialization (a reduced item) needs its own flag mask.

## 8.11 Decks (WIZ-14)

**Goal:** Build the equipped deck.

**Size:** M. **Depends on:** 8.05, 8.10

**Client messages:** MSG_ADDSPELLTODECK, MSG_REMOVESPELLFROMDECK, MSG_CLEAR_EQUIPPED_DECK, MSG_SETDECKNAME, MSG_UPDATEITEMSPELLEXCLUSIONLIST, MSG_SetDeckArchmastery

**Acceptance**

- [ ] 5th copy with max 4 returns Success=0
- [ ] Removing a spell not in deck returns Success=0
- [ ] Real client: drag Fire Cat into deck persists; item card exclusion persists

### Detailed spec from WIZ-14: Decks: add and remove spells

Players can build their equipped deck from their spellbook, within copy and size limits, and the deck persists.

**Deliverables**

- Deck item instances carry DeckBehavior (m_spellList SpellData{template, enchantment, quantity}, m_serializedExclusionList, m_archmasterySchool)
- Limits from DeckBehaviorTemplate on the deck item template (max size, max copies, per-school copy limits)
- ADDSPELLTODECK and REMOVESPELLFROMDECK handlers that reply with Success; CLEAR_EQUIPPED_DECK; SETDECKNAME (the deck name is stored as a pattern, DeckName/DeckNameExtension uints)
- WIZARD2 UPDATEITEMSPELLEXCLUSIONLIST handler, resent on login for each excluded item card
- data/sql/updates/db_characters: item_deck_spell, item_deck_exclusion
- src/test/server/game/DeckTest.cpp

**Client messages:** MSG_ADDSPELLTODECK, MSG_REMOVESPELLFROMDECK, MSG_CLEAR_EQUIPPED_DECK, MSG_SETDECKNAME, WIZARD2 MSG_UPDATEITEMSPELLEXCLUSIONLIST, WIZARD3 MSG_SetDeckArchmastery

**Data sources**

- world.item_template (DeckBehaviorTemplate in behaviors)
- world.spell_template

**Database tables**

- characters.item_deck_spell
- characters.item_deck_exclusion

**Acceptance**

- [ ] Unit test: adding a 5th copy of a spell to a deck with max copies 4 returns Success=0
- [ ] Unit test: removing a spell not in the deck returns Success=0 and changes nothing
- [ ] Real client: in the deck screen, dragging Fire Cat from the spellbook into the deck adds the card. Relogging keeps it. Crossing out an item card greys it out and the state survives relog.

**Risks**

- The SpellID encoding in ADDSPELLTODECK (template id versus name hash) needs checking against a real client before handlers are written.
- Spell fusion (WIZARD3 ADDSPELLFUSIONTODECK) is not part of this milestone; it is planned, not yet scheduled.

## 8.12 Treasure cards (WIZ-15)

**Goal:** TC book and sideboard.

**Size:** M. **Depends on:** 8.11

**Client messages:** MSG_ADDTREASURESPELLTOBOOK, MSG_ADDTREASURESPELLTODECK, MSG_REMOVETREASURESPELLFROMBOOK, MSG_REMOVETREASURESPELLFROMDECK, MSG_REMOVETREASURESPELLFROMVAULT

**Acceptance**

- [ ] Moving quantity 1 book->deck leaves 0/1
- [ ] Unknown spell hash replies Success=0
- [ ] Real client: '.addtc Tower Shield 3' then 2 into sideboard persists

### Detailed spec from WIZ-15: Treasure cards

Treasure cards are owned in quantity, shown in the treasure-card page, and movable between the book and the sideboard of the deck.

**Deliverables**

- data/sql/updates/db_characters: character_treasure_card (template, enchantment, quantity)
- ClientTreasureBookBehavior (m_spellList) in the player object
- ADDTREASURESPELLTOBOOK/REMOVETREASURESPELLFROMBOOK senders and handlers; ADDTREASURESPELLTODECK/REMOVETREASURESPELLFROMDECK (Destroy flag); REMOVETREASURESPELLFROMVAULT
- Login resend of sideboard treasure cards (one ADDTREASURESPELLTODECK per copy, as the reference does)
- cs_learn .addtc <spell> [count]

**Client messages:** MSG_ADDTREASURESPELLTOBOOK, MSG_ADDTREASURESPELLTODECK, MSG_REMOVETREASURESPELLFROMBOOK, MSG_REMOVETREASURESPELLFROMDECK, MSG_REMOVETREASURESPELLFROMVAULT

**Data sources**

- world.spell_template (Spells/TreasureCards)

**Database tables**

- characters.character_treasure_card

**Acceptance**

- [ ] Unit test: moving a TC with quantity 1 from book to deck leaves 0 in the book and 1 in the deck
- [ ] Unit test: an unknown spell-name hash replies Success=0
- [ ] Real client: '.addtc Tower Shield 3' shows 3 Tower Shield treasure cards in the treasure tab. Dragging 2 into the deck sideboard shows 1 left in the book and 2 in the sideboard, and they survive relog.

**Risks**

- The SpellID in these messages is a string hash of the spell name (as the reference resolves it). The hash function must match the client's.

## 8.13 Spell training at trainers (WIZ-16)

**Goal:** Spend training points at professors.

**Size:** M. **Depends on:** 8.05, 8.02, 7.07, 4.15, 4.16

**Client messages:** MSG_TRAIN, MSG_SPELLTRAINCOMPLETE, MSG_UPDATETRAINING, MSG_ADDSPELLTOBOOK, MSG_RESPECCONFIRM

**Acceptance**

- [ ] Not enough points changes nothing
- [ ] Out-of-bounds TrainingIndex rejected
- [ ] `.reload npc_trainer_spell` updates a trainer's list on the next interaction; Training.OffSchoolCost applies live
- [ ] Real client: professor teaches a spell and points drop

### Detailed spec from WIZ-16: Spell training at trainer NPCs

Players can spend training points at a school trainer to learn spells, with the training UI updating live.

**Deliverables**

- world.npc_trainer_spell (npc template id, index, spell template id, requirements) authored as SQL, since trainer lists are not in the verified client data
- Trainer lists load into a 4.15 reloadable store. `.reload npc_trainer_spell` builds and validates the lists off to the side and swaps them, and a failure keeps the old lists and reports every error.
- src/server/game/Handlers/TrainHandler.cpp: TRAIN handler (validates MobileID in range, the NPC has the trainer service, TrainingIndex in bounds, requirements met, point cost: same school free, off-school costs Training.OffSchoolCost, a live setting defaulting to 1 per the reference, gold cost if any)
- Replies ADDSPELLTOBOOK, UPDATETRAINING, SPELLTRAINCOMPLETE(DisplayText locale key)
- RESPECCONFIRM stub (crowns respec is EXT)
- ScriptMgr hook OnTrainSpell

**Client messages:** MSG_TRAIN, MSG_SPELLTRAINCOMPLETE, MSG_UPDATETRAINING, MSG_ADDSPELLTOBOOK, MSG_RESPECCONFIRM

**Data sources**

- Root.wad NPCServices.xml (unknown class 0x32a408e1, may hold trainer service data)
- Root.wad Locale/WizTraining*.lang for display keys

**Database tables**

- world.npc_trainer_spell

**Acceptance**

- [ ] Unit test: training without enough points changes nothing and sends no ADDSPELLTOBOOK
- [ ] Unit test: a TrainingIndex out of bounds is rejected
- [ ] Unit test: `.reload npc_trainer_spell` with an added row offers the new spell on the next interaction without a restart, and changing Training.OffSchoolCost changes the next off-school cost
- [ ] Real client: talking to a school professor and choosing an untrained spell plays the train sequence, the spell appears in the spellbook, the training point count on the character sheet drops by the cost, and the spell no longer shows as trainable.

**Risks**

- Trainer spell lists and training point costs may exist only on the server. The reference loaded them from its own data set, which is never committed here, so committed lists need clean-room authoring or client-side reverse engineering (NPCServices.xml). An opt-in importer that reads such a data set from a copy the user has, into that user's local world database only, is planned, not yet scheduled.

## 8.14 Versionable BINd encoder, byte-exact (OBJ-7)

**Goal:** Write BINd/versionable blobs for tools.

**Size:** M. **Depends on:** 3.11

**Acceptance**

- [x] Synthetic encode/decode equal. `VersionableRoundTripTest.SyntheticVersionableObjectRoundTrips` encodes a synthetic object and decodes it back equal
- [x] Hat template and a 2000-file sample re-encode byte-identically (inflated compare for the manifest). `VersionableRoundTripTest.HatTemplateAndManifestAreByteExact` and `TwoThousandDecodedFilesAreByteExact`, run against the pinned install. Both fail when the write order is reversed, so they compare bytes rather than assert nothing

### Detailed spec from OBJ-7: Versionable BINd encoder with byte-exact round trip

The server and tools can write BINd files and versionable blobs that the client accepts, proven by re-encoding client files byte for byte.

**Deliverables**

- ObjectSerializer Encode path for versionable mode: back-patched object and property bit sizes, properties in id order, flags header, optional compression
- BindFile::Write
- src/test/server/shared/ObjectProperty/VersionableRoundTripTest.cpp

**Acceptance**

- [ ] Unit test: for synthetic objects, encode then decode gives equal objects
- [ ] Client-gated test: decode then re-encode is byte-identical for the hat template, TemplateManifest.xml (compared after inflate, since zlib output may differ) and a random 2000-file sample; any non-identical file is listed with the first differing bit

**Risks**

- Re-compressed zlib bytes will not match KI's compressor, so compare inflated payloads
- Properties the client omitted (defaults) may be absent from files. If a byte-exact match needs 'present-property' tracking, PropertyObject needs a presence bitmap

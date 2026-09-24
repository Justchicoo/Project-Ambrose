<!-- Project Ambrose by Imjustchico: Roadmap phase 11, Real combat. -->

# Phase 11: Real combat

**Done when:** Two players and several mobs fight with gear stats, blades, traps, shields, heals, DoTs, AoE, stuns and authored creature decks. Health bars never desync.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 11.01 | Multi-participant duels (CMB-10) | M | 9.12, 4.15, 4.16 |
| 11.02 | StatCalculator and equip effects (WIZ-13 part 1) | M | 8.10, 9.03, 5.04 |
| 11.03 | Set bonuses and rating conversions (WIZ-13 part 2) | M | 11.02, 4.15 |
| 11.04 | Combat stats and pip rules (CMB-11) | M | 11.01, 11.03, 4.15 |
| 11.05 | DamageCalc and limit curves (CMB-12 part 1) | M | 11.04, 4.16 |
| 11.06 | Crit, block and SETST parity (CMB-12 part 2) | M | 11.05 |
| 11.07 | Blades and traps (CMB-13 part 1) | M | 11.06 |
| 11.08 | Shields, absorbs, max damage (CMB-13 part 2) | M | 11.07 |
| 11.09 | Heals, over-time, accuracy, dispels (CMB-14) | M | 11.08 |
| 11.10 | Team targets and AoE (CMB-15 part 1) | M | 11.09 |
| 11.11 | Compound, conditional, X-pip spells (CMB-15 part 2) | M | 11.10, 7.04 |
| 11.12 | Stun and pip manipulation (CMB-16 part 1) | M | 11.11 |
| 11.13 | Removal, push, steal, swap, auras (CMB-16 part 2) | M | 11.12 |
| 11.14 | Creature deck tables and loader (CMB-18 part 1) | M | 9.10, 2.07, 4.15, 4.16 |
| 11.15 | Weighted creature AI (CMB-18 part 2) | M | 11.14, 11.11 |
| 11.16 | Treasure cards, item cards, enchantments in combat (CMB-21) | M | 11.08, 8.12 |
| 11.17 | Minions (CMB-22) | M | 11.15 |
| 11.18 | Timers, AFK, pause, spectators (CMB-23) | M | 11.01, 4.16 |
| 11.19 | Polymorph (CMB-17 part 1) | M | 11.13 |
| 11.20 | Mind control, confusion, taunt, cloaked effects (CMB-17 part 2) | M | 11.19 |
| 11.21 | Duel modifiers and battlefield effects (CMB-24 part 1) | M | 11.20, 11.15 |
| 11.22 | Combat triggers and sigil spells (CMB-24 part 2) | M | 11.21 |
| 11.23 | Shadow pips and backlash (CMB-25 part 1) | M | 11.13, 4.16 |
| 11.24 | Shadow pacts and shadow creatures (CMB-25 part 2) | M | 11.23 |
| 11.25 | Archmastery school pips and tiered spells (CMB-25 part 3) | M | 11.23 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Oversized.** 11.05 DamageCalc with '30+ golden cases each with source noted' (M). The roadmap's own risk list says no local source for the formulas exists, so this is an RE project, not an M implementation.

## 11.01 Multi-participant duels (CMB-10)

**Goal:** 4v4, join-in-progress, turn order.

**Size:** M. **Depends on:** 9.12, 4.15, 4.16

**Client messages:** MSG_COMBATADD, MSG_COMBATUPFIRST, MSG_COMBATMOVESELECTION, MSG_COMBATLOADED, MSG_UPDATECOMBATPARTICIPANT

**Acceptance**

- [ ] 2v3 executes team A ascending then B; round-2 joiner acts round 3
- [ ] Real client: second wizard added next round; third client zoning in sees the fight
- [ ] `.reload zone_combat_rules` changes the next duel's pull-in cap without a restart

### Detailed spec from CMB-10: Multi-participant duels: join in progress, teams and turn order

Up to 4 players and 4 creatures share a duel, with correct late joins, creature pull-ins and team turn order.

**Deliverables**

- Join queue: participants entering the engage radius mid-round are added at the next round start (MSG_COMBATADD), never mid-execution
- Creature pull-in rules (base creature slots per zone plus one per extra player), exposed as world DB columns per zone or sigil and reloaded live with `.reload zone_combat_rules`; the fallbacks for zones without a row are the live settings Combat.PullIn.BaseSlots and Combat.PullIn.PerExtraPlayer, applied from the next duel
- First team roll and slot-order execution; teammate move selection broadcast only to the same team
- MSG_COMBATLOADED sent to a client that enters the zone while a duel is active, so it renders the fight

**Client messages:** MSG_COMBATADD, MSG_COMBATUPFIRST, MSG_COMBATMOVESELECTION, MSG_COMBATLOADED, MSG_UPDATECOMBATPARTICIPANT

**Data sources**

- Sigils/*.xml sub-circles

**Database tables**

- world: zone_combat_rules (creature pull-in caps)

**Acceptance**

- [ ] Sim test: a 2v3 duel executes team A slots in ascending order, then team B; a creature added in round 2 acts from round 3
- [ ] Real client (2 clients): a second wizard runs into an active fight, is added at the next round, and both players see each other's card choice icons
- [ ] Real client: a third client zoning in mid-fight sees the ring, participants and health correctly (MSG_COMBATLOADED)
- [ ] Editing a zone_combat_rules row, then `.reload zone_combat_rules`, changes the pull-in cap of the next duel in that zone without a restart; a row that fails validation keeps the old rules and reports the error

**Risks**

- The retail extra-creature pull-in rule is not in client data; the reference hardcodes 1/2/3 base, so the defaults are live settings that can be tuned without a restart

## 11.02 StatCalculator and equip effects (WIZ-13 part 1)

**Goal:** Gear changes stats.

**Size:** M. **Depends on:** 8.10, 9.03, 5.04

**Client messages:** MSG_ADDEFFECT, MSG_REMOVEEFFECT, MSG_UPDATEHEALTH, MSG_UPDATEMANA

**Acceptance**

- [ ] +100 HP hat raises max by 100; clamps on unequip
- [ ] Real client: +5% Fire and +40 HP shows on Stats tab and globe

### Detailed spec from WIZ-13: Equipment stat effects and set bonuses

Equipped gear changes the wizard's maximum health, damage, resist, crit, block, pips and other stats the way the client expects, and those changes show on the character sheet.

**Deliverables**

- src/server/game/Entities/Player/StatCalculator: base stats from player_level_stats, plus each equipped item's m_equipEffects (WizStatisticEffect fields: m_hitPointBonus, m_manaBonus, m_damageBonusPercent, m_damageReducePercent, m_accuracyBonusPercent, m_criticalHitRating, m_blockRating, m_powerPipBonusPercent, m_pipConversionRating, m_archmastery, per-school fields via effect category), plus ItemSetBonusTemplate tiers by equipped count
- GAME ADDEFFECT/REMOVEEFFECT sent on equip, unequip and login; UPDATEHEALTH/UPDATEMANA with the new maximums
- Rating-to-percent conversions from stat_effect_config (critical, block, pip conversion) for derived WizGameStats fields; stat_effect_config and item_set_bonus reload live with `.reload stat_effect_config` and `.reload item_set_bonus`, keep the old tables on failure, and recompute online players' stats after a successful swap
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

## 11.03 Set bonuses and rating conversions (WIZ-13 part 2)

**Goal:** Set tiers and rating-to-percent.

**Size:** M. **Depends on:** 11.02, 4.15

**Acceptance**

- [ ] 3-piece bonus only with 3 pieces
- [ ] Sheet numbers match server values
- [ ] `.reload stat_effect_config` updates an online player's sheet without a restart

### Detailed spec from WIZ-13: Equipment stat effects and set bonuses

Equipped gear changes the wizard's maximum health, damage, resist, crit, block, pips and other stats the way the client expects, and those changes show on the character sheet.

**Deliverables**

- src/server/game/Entities/Player/StatCalculator: base stats from player_level_stats, plus each equipped item's m_equipEffects (WizStatisticEffect fields: m_hitPointBonus, m_manaBonus, m_damageBonusPercent, m_damageReducePercent, m_accuracyBonusPercent, m_criticalHitRating, m_blockRating, m_powerPipBonusPercent, m_pipConversionRating, m_archmastery, per-school fields via effect category), plus ItemSetBonusTemplate tiers by equipped count
- GAME ADDEFFECT/REMOVEEFFECT sent on equip, unequip and login; UPDATEHEALTH/UPDATEMANA with the new maximums
- Rating-to-percent conversions from stat_effect_config (critical, block, pip conversion) for derived WizGameStats fields; stat_effect_config and item_set_bonus reload live with `.reload stat_effect_config` and `.reload item_set_bonus`, keep the old tables on failure, and recompute online players' stats after a successful swap
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

## 11.04 Combat stats and pip rules (CMB-11)

**Goal:** Starting pips, power pips, pip payment.

**Size:** M. **Depends on:** 11.01, 11.03, 4.15

**Client messages:** MSG_COMBATSTATS, MSG_COMBATPIPS

**Acceptance**

- [ ] Rank-4 Fire spell with 1 power + 2 pips succeeds for Fire, fails for Ice
- [ ] 100% power pip chance always; 0% never
- [ ] Real client: stats window matches; power pips gold
- [ ] `.reload creature_combat_stats` changes the next duel's creature stats without a restart

### Detailed spec from CMB-11: Combat stats, starting pips, power pips and pip payment rules

Participants enter combat with real stats derived from level, school and gear, and pips behave like retail.

**Deliverables**

- src/server/game/Combat/CombatStats: builds WizGameStats for players (WIZ) and creatures (NPCBehaviorTemplate plus world DB overrides); creature_combat_stats reloads live with `.reload creature_combat_stats`, keeps the old overrides on failure, and applies from the next duel
- Starting pips and power pips from stats (m_startingPips/m_startingPowerPips), power pip chance m_powerPipBase + m_powerPipBonusPercentAll
- Pip payment: power pips count double only for on-school or Balance-equivalent spells, off-school power pips count as 1, school pips from SpellRank fields reserved for shadow/archmastery later
- Accuracy with m_accBonusPercent/m_accReducePercent per school and All

**Client messages:** MSG_COMBATSTATS, MSG_COMBATPIPS

**Data sources**

- WizGameStats schema in the type dump
- NPCBehaviorTemplate in ObjectData

**Database tables**

- world: creature_combat_stats (overrides: resist, boost, accuracy per school)

**Acceptance**

- [ ] Unit test table: SpellRank (rank 4, school Fire) paid by a Fire wizard with 1 power pip + 2 pips succeeds; paid by an Ice wizard it fails
- [ ] Unit test: 100% power pip chance gives power pips every round; 0% never does
- [ ] Real client: the combat stats window numbers match MSG_COMBATSTATS; power pips show as gold pips and a 4-pip on-school spell becomes castable with 2 power pips
- [ ] Editing a creature's resist in creature_combat_stats, then `.reload creature_combat_stats`, changes the damage it takes in the next duel without a restart

**Risks**

- Exact pip-payment and power-pip rules must come from client RE or observation; the reference does not implement off-school rules

## 11.05 DamageCalc and limit curves (CMB-12 part 1)

**Goal:** Pure damage/resist/pierce functions.

**Size:** M. **Depends on:** 11.04, 4.16

**Acceptance**

- [ ] 30+ golden cases each with source noted

### Detailed spec from CMB-12: Damage, resist, pierce and critical math

Outgoing and incoming damage use stat percentages, flat values, pierce, the damage/resist limit curves, and crit/block.

**Deliverables**

- src/server/game/Combat/DamageCalc.{h,cpp}: pure functions over (caster stats, target stats, Duel scalar/limit constants) with no duel state
- Limit curves using Duel m_damageLimit/m_dK0/m_dN0 and m_resistLimit/m_rK0/m_rN0 from CombatSigilTemplate PvE/PvP fields, with optional server overrides as the live settings Combat.Limit.PvE.* and Combat.Limit.PvP.*, applied from the next duel
- Critical and block ratings to chance, crit multiplier, and roll outcomes recorded in CombatAction (m_criticalHitRoll, m_CritHitList TargetCritHit)
- MSG_SETST/MSG_SETST2 sent to clients so their displayed numbers use the same stat-rebalance settings
- The stat-rebalance values are live settings (Combat.StatRebalance.*); a change applies from the next duel and re-sends MSG_SETST/MSG_SETST2 to every connected client

**Client messages:** MSG_SETST, MSG_SETST2, MSG_COMBATACTIONS, MSG_COMBATHEALTH

**Data sources**

- CombatSigilTemplate scalar and limit fields
- Client executable RE of its CombatResolver (read-only study)

**Acceptance**

- [ ] Golden unit tests: 30+ (stats, spell, sigil constants) → expected damage cases, each with its source noted (client RE function address or recorded observation)
- [ ] Real client: a Fire Cat crit shows the critical effect and the health drop the client shows matches server HP after MSG_COMBATHEALTH with no correction jump
- [ ] Real client: the stats window damage% and resist% shown after MSG_SETST match the server's computed effective values
- [ ] Changing a Combat.StatRebalance.* setting re-sends MSG_SETST to connected clients and changes the next duel's damage without a restart

**Risks**

- Formulas are not available in any local source (the reference has no limit curve or crit code). This needs client binary RE or controlled observation, and a mismatch shows up as visible health desync

## 11.06 Crit, block and SETST parity (CMB-12 part 2)

**Goal:** Rolls recorded; client numbers match.

**Size:** M. **Depends on:** 11.05

**Client messages:** MSG_SETST, MSG_SETST2, MSG_COMBATACTIONS, MSG_COMBATHEALTH

**Acceptance**

- [ ] Real client: Fire Cat crit with no correction jump
- [ ] Stats window damage%/resist% after MSG_SETST match
- [ ] Combat.StatRebalance.* change re-sends MSG_SETST and applies from the next duel without a restart

### Detailed spec from CMB-12: Damage, resist, pierce and critical math

Outgoing and incoming damage use stat percentages, flat values, pierce, the damage/resist limit curves, and crit/block.

**Deliverables**

- src/server/game/Combat/DamageCalc.{h,cpp}: pure functions over (caster stats, target stats, Duel scalar/limit constants) with no duel state
- Limit curves using Duel m_damageLimit/m_dK0/m_dN0 and m_resistLimit/m_rK0/m_rN0 from CombatSigilTemplate PvE/PvP fields, with optional server overrides as the live settings Combat.Limit.PvE.* and Combat.Limit.PvP.*, applied from the next duel
- Critical and block ratings to chance, crit multiplier, and roll outcomes recorded in CombatAction (m_criticalHitRoll, m_CritHitList TargetCritHit)
- MSG_SETST/MSG_SETST2 sent to clients so their displayed numbers use the same stat-rebalance settings
- The stat-rebalance values are live settings (Combat.StatRebalance.*); a change applies from the next duel and re-sends MSG_SETST/MSG_SETST2 to every connected client

**Client messages:** MSG_SETST, MSG_SETST2, MSG_COMBATACTIONS, MSG_COMBATHEALTH

**Data sources**

- CombatSigilTemplate scalar and limit fields
- Client executable RE of its CombatResolver (read-only study)

**Acceptance**

- [ ] Golden unit tests: 30+ (stats, spell, sigil constants) → expected damage cases, each with its source noted (client RE function address or recorded observation)
- [ ] Real client: a Fire Cat crit shows the critical effect and the health drop the client shows matches server HP after MSG_COMBATHEALTH with no correction jump
- [ ] Real client: the stats window damage% and resist% shown after MSG_SETST match the server's computed effective values
- [ ] Changing a Combat.StatRebalance.* setting re-sends MSG_SETST to connected clients and changes the next duel's damage without a restart

**Risks**

- Formulas are not available in any local source (the reference has no limit curve or crit code). This needs client binary RE or controlled observation, and a mismatch shows up as visible health desync

## 11.07 Blades and traps (CMB-13 part 1)

**Goal:** Outgoing/incoming modifiers.

**Size:** M. **Depends on:** 11.06

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATADD, MSG_COMBATHEALTH

**Acceptance**

- [ ] Fire Blade consumed by Fire Cat, not by Ice
- [ ] Identical blades do not stack; different ones do
- [ ] Real client: blade flies into the spell; trap consumed

### Detailed spec from CMB-13: Hanging effects I: charms and wards (blades, traps, shields, weaknesses)

Blades, traps, shields and weaknesses are placed, displayed, stack and are consumed correctly by damage.

**Deliverables**

- src/server/game/Combat/HangingEffects.{h,cpp}: per-participant m_hangingEffects and m_publicHangingEffects, disposition kBeneficial/kHarmful
- kModifyOutgoingDamage/Flat, kModifyIncomingDamage/Flat, kModifyOutgoingArmorPiercing, kModifyIncomingArmorPiercing, kAbsorbDamage, kMaximumIncomingDamage
- Consumption rules: school match, one per distinct spell template, order of application
- CombatAction results for added and removed hanging effects

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATADD, MSG_COMBATHEALTH

**Data sources**

- Spells/ with effect types 9-44 of kSpellEffects

**Acceptance**

- [ ] Unit tests: Fire Blade (+X% Fire) then Fire Cat consumes the blade and deals boosted damage; an Ice Blade is not consumed by a Fire spell
- [ ] Unit tests: two identical blades do not stack; different blade spells do
- [ ] Unit test: a Tower Shield absorbs the percentage and is removed; an absorb shield soaks the flat amount
- [ ] Real client: the blade icon appears over the caster, then flies into the spell on cast and disappears; the trap icon on the target is consumed, with health drops matching the server

**Risks**

- Exact stacking and ordering rules are not in data; need observation or RE

## 11.08 Shields, absorbs, max damage (CMB-13 part 2)

**Goal:** Defensive wards.

**Size:** M. **Depends on:** 11.07

**Acceptance**

- [ ] Tower Shield absorbs percentage and is removed; absorb soaks flat amount

### Detailed spec from CMB-13: Hanging effects I: charms and wards (blades, traps, shields, weaknesses)

Blades, traps, shields and weaknesses are placed, displayed, stack and are consumed correctly by damage.

**Deliverables**

- src/server/game/Combat/HangingEffects.{h,cpp}: per-participant m_hangingEffects and m_publicHangingEffects, disposition kBeneficial/kHarmful
- kModifyOutgoingDamage/Flat, kModifyIncomingDamage/Flat, kModifyOutgoingArmorPiercing, kModifyIncomingArmorPiercing, kAbsorbDamage, kMaximumIncomingDamage
- Consumption rules: school match, one per distinct spell template, order of application
- CombatAction results for added and removed hanging effects

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATADD, MSG_COMBATHEALTH

**Data sources**

- Spells/ with effect types 9-44 of kSpellEffects

**Acceptance**

- [ ] Unit tests: Fire Blade (+X% Fire) then Fire Cat consumes the blade and deals boosted damage; an Ice Blade is not consumed by a Fire spell
- [ ] Unit tests: two identical blades do not stack; different blade spells do
- [ ] Unit test: a Tower Shield absorbs the percentage and is removed; an absorb shield soaks the flat amount
- [ ] Real client: the blade icon appears over the caster, then flies into the spell on cast and disappears; the trap icon on the target is consumed, with health drops matching the server

**Risks**

- Exact stacking and ordering rules are not in data; need observation or RE

## 11.09 Heals, over-time, accuracy, dispels (CMB-14)

**Goal:** Sustain and denial effects.

**Size:** M. **Depends on:** 11.08

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATHEALTH

**Acceptance**

- [ ] 3-round DoT ticks exactly 3 times at the victim's slot
- [ ] Fire dispel fizzles next Fire only
- [ ] Real client: DoT tick numbers match; heals never exceed max

### Detailed spec from CMB-14: Hanging effects II: heals, over-time, accuracy and dispels

Heals, damage-over-time, heal-over-time, accuracy buffs and dispels work.

**Deliverables**

- kHeal, kHealPercent, kSetHealPercent, kStealHealth, kModifyIncomingHeal/Outgoing heal (flat and percent)
- kDamageOverTime and kHealOverTime ticks at the participant's turn, kReduceOverTime, kDetonateOverTime, kModifyOverTimeDuration
- kModifyAccuracy (consumed on next cast of that school), kDispel (next cast of that school fizzles)

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATHEALTH

**Data sources**

- Spells/ heal, DoT, HoT and accuracy spells

**Acceptance**

- [ ] Sim test: a 3-round DoT ticks exactly 3 times at the affected participant's action slot and then leaves the hanging list
- [ ] Sim test: a dispel on Fire makes the next Fire cast fizzle and is consumed; an Ice cast is unaffected
- [ ] Real client: the DoT tick animation plays at the start of the target's turn with the matching number; heals raise the health globe and never exceed max HP

**Risks**

- Whether DoT ticks happen at round start or at the victim's action slot (the reference ticks at the caster's slot) must be verified against client playback

## 11.10 Team targets and AoE (CMB-15 part 1)

**Goal:** kEnemyTeam and variants.

**Size:** M. **Depends on:** 11.09

**Client messages:** MSG_COMBATMOVE, MSG_COMBATACTIONS

**Acceptance**

- [ ] Meteor Strike hits all living enemies once in slot order
- [ ] Real client: AoE numbers match server HP

### Detailed spec from CMB-15: Targeting and compound effects: AoE, effect lists, conditionals, X-pip

Spells with team targets, multiple and variable effects, conditional branches and X-pip costs resolve correctly.

**Deliverables**

- kEffectTarget handling: kEnemyTeam, kEnemyTeamAllAtOnce, kFriendlyTeam(AllAtOnce), kSelf, kFriendlySingle, kFriendlySingleNotMe, kMultiTargetEnemy/Friendly, kAtLeastOneEnemy
- EffectListSpellEffect, VariableSpellEffect, RandomPerTargetSpellEffect (m_randomSpellEffectPerTargetRolls), TargetCountSpellEffect, CountBasedSpellEffect, ConditionalSpellEffect with combat requirements (ReqCombatHealth, ReqHangingCharm/Ward/Aura, ReqPipCount, ReqMinion...)
- X-pip spells (SpellRank m_xPipSpell, CombatAction m_xPipCost)

**Client messages:** MSG_COMBATMOVE, MSG_COMBATACTIONS

**Data sources**

- Spells/ compound effect classes

**Acceptance**

- [ ] Sim test: Meteor Strike hits all living enemies once each; a dead enemy is skipped; the target list order matches slot order
- [ ] Sim test: a conditional spell takes its branch when the target HP requirement is met and the fallback otherwise
- [ ] Real client: an AoE cinematic shows numbers over each enemy that match server HP; an X-pip spell spends all pips and the damage scales

**Risks**

- Combat-specific Req* classes need evaluators over duel state that the shared requirement system (other domain) may not anticipate

## 11.11 Compound, conditional, X-pip spells (CMB-15 part 2)

**Goal:** Effect lists and combat Req* classes.

**Size:** M. **Depends on:** 11.10, 7.04

**Acceptance**

- [ ] Conditional takes its branch when the HP requirement is met, else fallback
- [ ] X-pip spends all pips and scales

### Detailed spec from CMB-15: Targeting and compound effects: AoE, effect lists, conditionals, X-pip

Spells with team targets, multiple and variable effects, conditional branches and X-pip costs resolve correctly.

**Deliverables**

- kEffectTarget handling: kEnemyTeam, kEnemyTeamAllAtOnce, kFriendlyTeam(AllAtOnce), kSelf, kFriendlySingle, kFriendlySingleNotMe, kMultiTargetEnemy/Friendly, kAtLeastOneEnemy
- EffectListSpellEffect, VariableSpellEffect, RandomPerTargetSpellEffect (m_randomSpellEffectPerTargetRolls), TargetCountSpellEffect, CountBasedSpellEffect, ConditionalSpellEffect with combat requirements (ReqCombatHealth, ReqHangingCharm/Ward/Aura, ReqPipCount, ReqMinion...)
- X-pip spells (SpellRank m_xPipSpell, CombatAction m_xPipCost)

**Client messages:** MSG_COMBATMOVE, MSG_COMBATACTIONS

**Data sources**

- Spells/ compound effect classes

**Acceptance**

- [ ] Sim test: Meteor Strike hits all living enemies once each; a dead enemy is skipped; the target list order matches slot order
- [ ] Sim test: a conditional spell takes its branch when the target HP requirement is met and the fallback otherwise
- [ ] Real client: an AoE cinematic shows numbers over each enemy that match server HP; an X-pip spell spends all pips and the damage scales

**Risks**

- Combat-specific Req* classes need evaluators over duel state that the shared requirement system (other domain) may not anticipate

## 11.12 Stun and pip manipulation (CMB-16 part 1)

**Goal:** Stun, stun block, pip effects.

**Size:** M. **Depends on:** 11.11

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATPIPS

**Acceptance**

- [ ] Stunned creature passes, then gains stun blocks per rule
- [ ] Real client: stun rings and pip steal display

### Detailed spec from CMB-16: Control effects I: stun, pips, removal, push/steal/swap

Stuns, pip manipulation and charm/ward/over-time removal and transfer work.

**Deliverables**

- kStun with kStunBlock/kStunResist (m_stunResistRoll), kRemoveStunBlock
- kModifyPips, kModifyPowerPips, kModifyPowerPipChance, kModifyPipRoundRate, kSuspendPips/kResumePips, kPipConversion/kPowerPipConversion
- kRemoveCharm/Ward/OverTime/Aura, kPushCharm/Ward/OverTime, kStealCharm/Ward/OverTime, kSwapAll/Charm/Ward/OverTime (and the Converted variants)
- Auras (m_auraEffects, m_nAuraTurnLength) and global effects (bubbles) with replacement rules

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATPIPS

**Data sources**

- Spells/

**Acceptance**

- [ ] Sim test: a stunned creature passes next round, then gets stun blocks equal to the retail rule
- [ ] Sim test: Enfeeble removes the target's newest blade only
- [ ] Real client: stun rings appear over the target, it skips its turn, and stun-block shields show afterwards; a pip steal reduces the target's pip display

**Risks**

- Stun block count and removal order rules are unverified

## 11.13 Removal, push, steal, swap, auras (CMB-16 part 2)

**Goal:** Charm/ward manipulation.

**Size:** M. **Depends on:** 11.12

**Acceptance**

- [ ] Enfeeble removes only the newest blade

### Detailed spec from CMB-16: Control effects I: stun, pips, removal, push/steal/swap

Stuns, pip manipulation and charm/ward/over-time removal and transfer work.

**Deliverables**

- kStun with kStunBlock/kStunResist (m_stunResistRoll), kRemoveStunBlock
- kModifyPips, kModifyPowerPips, kModifyPowerPipChance, kModifyPipRoundRate, kSuspendPips/kResumePips, kPipConversion/kPowerPipConversion
- kRemoveCharm/Ward/OverTime/Aura, kPushCharm/Ward/OverTime, kStealCharm/Ward/OverTime, kSwapAll/Charm/Ward/OverTime (and the Converted variants)
- Auras (m_auraEffects, m_nAuraTurnLength) and global effects (bubbles) with replacement rules

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATPIPS

**Data sources**

- Spells/

**Acceptance**

- [ ] Sim test: a stunned creature passes next round, then gets stun blocks equal to the retail rule
- [ ] Sim test: Enfeeble removes the target's newest blade only
- [ ] Real client: stun rings appear over the target, it skips its turn, and stun-block shields show afterwards; a pip steal reduces the target's pip display

**Risks**

- Stun block count and removal order rules are unverified

## 11.14 Creature deck tables and loader (CMB-18 part 1)

**Goal:** Authored mob decks with reload.

**Size:** M. **Depends on:** 9.10, 2.07, 4.15, 4.16

**Acceptance**

- [ ] '.creature deck reload' changes the next duel's cards without restart
- [ ] A deck reload that references a missing spell keeps the old decks and reports the error

### Detailed spec from CMB-18: Creature AI and creature decks

Mobs fight with authored decks and personalities (smart, selfish, aggressive) instead of a single attack.

**Deliverables**

- src/server/game/AI/CreatureCombatAI: hand from creature deck; choice weighted by NPCBehaviorTemplate m_fIntelligence, m_fSelfishFactor, m_nAggressiveFactor; heal threshold; buff-before-hit; hate table targeting (kModifyHate, damage/heal threat)
- sCreatureDeckMgr loading world.creature_deck, world.creature_deck_spell and world.creature_combat_ai with a reload command on the 4.15 framework: the new decks are built and validated off to the side, swapped atomically, and a failed reload keeps the old decks and reports every error
- ScriptMgr NpcScript/CombatScript hook for boss scripts to override move choice (scripts/<World>/)
- Combat.AI.* tunables as live settings with defaults in conf/dist, applied from the next AI decision
- tools extractor: optional seed rows from each mob's MobDeckBehavior basic spell

**Client messages:** MSG_COMBATACTIONS

**Data sources**

- ObjectData mob templates (NPCBehaviorTemplate, MobDeckBehavior)
- ObjectData/Decks/ (3972 names only, apparently no spell lists)

**Database tables**

- world: creature_deck
- world: creature_deck_spell
- world: creature_combat_ai

**Acceptance**

- [ ] Sim test: with intelligence 1.0 and a blade plus attack in hand at enough pips, the AI blades first then attacks; with intelligence 0 it attacks immediately
- [ ] Sim test: a creature below the heal threshold with a heal in hand heals with the configured probability
- [ ] GM .creature deck reload changes a mob's next-duel cards without restart
- [ ] Changing a Combat.AI.* setting with `.settings set` changes the next AI decision without a restart
- [ ] Real client: a mob uses varied cards from its authored deck across rounds and targets the player who dealt the most damage

**Risks**

- Real mob deck contents are not in the client (sampled ObjectData/Decks entries hold only name/adjectives), so decks must be authored clean-room. Content volume is large
- Committed decks never come from a community spell database or other projects' deck data. An opt-in importer that reads a community spell database or another project's deck data from a copy the user has, into that user's local world database only and never committed, is planned, not yet scheduled

## 11.15 Weighted creature AI (CMB-18 part 2)

**Goal:** Intelligence, selfish, aggressive factors, hate.

**Size:** M. **Depends on:** 11.14, 11.11

**Client messages:** MSG_COMBATACTIONS

**Acceptance**

- [ ] Intelligence 1.0 blades then attacks; 0 attacks immediately
- [ ] Heal threshold heals with configured probability
- [ ] Real client: varied cards; targets top damage dealer
- [ ] Combat.AI.* change applies from the next AI decision without a restart

### Detailed spec from CMB-18: Creature AI and creature decks

Mobs fight with authored decks and personalities (smart, selfish, aggressive) instead of a single attack.

**Deliverables**

- src/server/game/AI/CreatureCombatAI: hand from creature deck; choice weighted by NPCBehaviorTemplate m_fIntelligence, m_fSelfishFactor, m_nAggressiveFactor; heal threshold; buff-before-hit; hate table targeting (kModifyHate, damage/heal threat)
- sCreatureDeckMgr loading world.creature_deck, world.creature_deck_spell and world.creature_combat_ai with a reload command on the 4.15 framework: the new decks are built and validated off to the side, swapped atomically, and a failed reload keeps the old decks and reports every error
- ScriptMgr NpcScript/CombatScript hook for boss scripts to override move choice (scripts/<World>/)
- Combat.AI.* tunables as live settings with defaults in conf/dist, applied from the next AI decision
- tools extractor: optional seed rows from each mob's MobDeckBehavior basic spell

**Client messages:** MSG_COMBATACTIONS

**Data sources**

- ObjectData mob templates (NPCBehaviorTemplate, MobDeckBehavior)
- ObjectData/Decks/ (3972 names only, apparently no spell lists)

**Database tables**

- world: creature_deck
- world: creature_deck_spell
- world: creature_combat_ai

**Acceptance**

- [ ] Sim test: with intelligence 1.0 and a blade plus attack in hand at enough pips, the AI blades first then attacks; with intelligence 0 it attacks immediately
- [ ] Sim test: a creature below the heal threshold with a heal in hand heals with the configured probability
- [ ] GM .creature deck reload changes a mob's next-duel cards without restart
- [ ] Changing a Combat.AI.* setting with `.settings set` changes the next AI decision without a restart
- [ ] Real client: a mob uses varied cards from its authored deck across rounds and targets the player who dealt the most damage

**Risks**

- Real mob deck contents are not in the client (sampled ObjectData/Decks entries hold only name/adjectives), so decks must be authored clean-room. Content volume is large
- Committed decks never come from a community spell database or other projects' deck data. An opt-in importer that reads a community spell database or another project's deck data from a copy the user has, into that user's local world database only and never committed, is planned, not yet scheduled

## 11.16 Treasure cards, item cards, enchantments in combat (CMB-21)

**Goal:** Draw and enchant.

**Size:** M. **Depends on:** 11.08, 8.12

**Client messages:** MSG_COMBATDRAW, MSG_COMBATHAND, MSG_COMBATMOVE, MSG_REMOVETREASURESPELLFROMDECK, MSG_REMOVETREASURESPELLFROMVAULT

**Acceptance**

- [ ] Enchanted Fire Cat deals base+X; both cards leave hand
- [ ] Real client: draw fills slot; TC count lower after duel; Strong enchant art

### Detailed spec from CMB-21: Treasure cards, item cards and enchantments

Players can draw from their treasure card vault, cast consumable cards, use gear item cards, and enchant cards.

**Deliverables**

- Sideboard/vault in PlayDeck; MSG_COMBATDRAW draws a random TC into an open slot
- TC consumption persisted, with MSG_REMOVETREASURESPELLFROMDECK / MSG_REMOVETREASURESPELLFROMVAULT
- Item cards from equipment (Spell m_itemCard) added to the deck
- Enchant move: kModifyCardDamage, kModifyCardAccuracy, kModifyCardRank, kModifyCardArmorPiercing, kModifyCardHeal etc. setting Spell.m_enchantment; DuelModifier flags m_noTreasureCards and m_noEnchantedTreasureCards enforced

**Client messages:** MSG_COMBATDRAW, MSG_COMBATHAND, MSG_COMBATMOVE, MSG_REMOVETREASURESPELLFROMDECK, MSG_REMOVETREASURESPELLFROMVAULT

**Data sources**

- Spells/ treasure and enchantment spells

**Database tables**

- characters: character_treasure_card (WIZ)

**Acceptance**

- [ ] Sim test: casting an enchanted Fire Cat (+X damage) deals base+X; the enchantment card and the base card both leave the hand
- [ ] Real client: clicking the draw button after a discard puts a TC in the open slot; after casting it the TC count in the deck window is lower after the duel
- [ ] Real client: dragging a Strong enchantment onto Fire Cat shows the enchanted card art

**Risks**

- The encoding of an enchant move in MSG_COMBATMOVE (which fields carry the target card) is unverified

## 11.17 Minions (CMB-22)

**Goal:** Summon and dismiss.

**Size:** M. **Depends on:** 11.15

**Client messages:** MSG_COMBATADD, MSG_COMBATREMOVE, MSG_DISMISS_SUMMON, MSG_COMBATACTIONS

**Acceptance**

- [ ] Full team cannot summon; solo summon acts next round
- [ ] Real client: minion spawns and dismisses

### Detailed spec from CMB-22: Minions and henchmen

Summon spells add minions to the caster's team and players can dismiss them.

**Deliverables**

- kSummonCreature/kSpawnCreature: spawn a creature into a free same-team slot as a minion (m_isMinion, m_minionSubCircle, m_minionStartingHealth)
- Minion AI through CreatureCombatAI; minions excluded from win/loss counts and pull-in caps
- MSG_DISMISS_SUMMON handling; henchmen (ResSummonHenchman/ReqCanSummonHenchman) gated by Duel m_noHenchmen

**Client messages:** MSG_COMBATADD, MSG_COMBATREMOVE, MSG_DISMISS_SUMMON, MSG_COMBATACTIONS

**Data sources**

- Spells/ minion summons
- ObjectData minion templates
- ObjectData/Decks/MinionDeck-*

**Acceptance**

- [ ] Sim test: a player with 3 teammates cannot summon (no slot); a solo player summons into slot 5 or later and the minion acts the following round
- [ ] Real client: casting a minion spell spawns the minion on a player circle mid-cinematic; right-click dismiss removes it

**Risks**

- Minion deck contents may also be server-side

## 11.18 Timers, AFK, pause, spectators (CMB-23)

**Goal:** Duel timers and combat AFK.

**Size:** M. **Depends on:** 11.01, 4.16

**Client messages:** MSG_SETDUELTIMER, MSG_UPDATEDUELTIMER, MSG_SETSTATUS, MSG_COMBATAFK, MSG_COMBATPAUSED, MSG_COMBATPHASEFORSPECTATORS, MSG_SETPLANNINGPHASETIMER

**Acceptance**

- [ ] Idle player flagged after N rounds; any move clears
- [ ] Combat.AFK.FlagRounds change applies from the next round without a restart
- [ ] Real client: '.duel pause' freezes countdown; bystander panel updates

### Detailed spec from CMB-23: Timers, AFK, pause and spectators

Duel and turn timers, combat AFK auto-pass, pause, and spectator views behave correctly.

**Deliverables**

- Duel timer for timed duels: MSG_SETDUELTIMER, MSG_UPDATEDUELTIMER, MSG_SETSTATUS (Duel m_matchTimer, m_bonusTime, m_passPenalty, m_yellowTime/m_redTime, m_minTurnTime)
- Combat AFK: after N rounds without a move the participant is flagged (MSG_COMBATAFK) and auto-passes; a client MSG_COMBATAFK clears it; after M rounds they are removed. N and M are the live settings Combat.AFK.FlagRounds and Combat.AFK.RemoveRounds, applied from the next round
- MSG_COMBATPAUSED for GM pause; MSG_COMBATPHASEFORSPECTATORS for nearby non-participants

**Client messages:** MSG_SETDUELTIMER, MSG_UPDATEDUELTIMER, MSG_SETSTATUS, MSG_COMBATAFK, MSG_COMBATPAUSED, MSG_COMBATPHASEFORSPECTATORS, MSG_SETPLANNINGPHASETIMER

**Acceptance**

- [ ] Sim test: an idle player gets the AFK flag after the configured rounds; any move clears it
- [ ] Real client: the AFK warning appears after idling and disappears on clicking; a GM .duel pause freezes the countdown for all participants
- [ ] Real client: a bystander sees the phase and names panel update while watching
- [ ] `.settings set Combat.AFK.FlagRounds 1` flags an idle participant after the next round of an ongoing duel without a restart

**Risks**

- The exact AFK thresholds are unknown, so they are live settings that can be tuned without a restart

## 11.19 Polymorph (CMB-17 part 1)

**Goal:** Polymorph decks and restore.

**Size:** M. **Depends on:** 11.13

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATHAND

**Acceptance**

- [ ] Unpolymorph restores exact deck and graveyard
- [ ] Real client: model and hand change

### Detailed spec from CMB-17: Control effects II: polymorph, mind control, confusion, reshuffle, cloaked effects

Advanced state-changing effects work, including polymorph decks and revealed cloaked traps.

**Deliverables**

- kPolymorph/kUnPolymorph with saved hand, deck and stats (m_pSavedHand, m_pSavedPlayDeck, m_pSavedGameStats) using Decks/Polymorph Decks
- kMindControl, kConfusion/kConfusionBlock, kReshuffle, kModifyHate, kTaunt/kPacify, kUntargetable/kForceTargetable, kIntercept, kDelayCast
- Cloaked charms and wards (kCloakedCharm, kCloakedWard, kRevealCloak) with MSG_COMBATREVEALHANGING

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATHAND, MSG_COMBATREVEALHANGING

**Data sources**

- Decks/Polymorph Decks (100)
- Spells/

**Acceptance**

- [ ] Sim test: polymorph swaps the deck to the polymorph deck; unpolymorph restores the exact previous deck and graveyard
- [ ] Real client: casting a polymorph turns the wizard model into the creature with its hand; a cloaked trap shows as a hidden icon to enemies and reveals with the reveal animation when triggered

**Risks**

- Polymorph deck contents in client Decks/ need decoding; they may be partial

## 11.20 Mind control, confusion, taunt, cloaked effects (CMB-17 part 2)

**Goal:** Advanced control effects.

**Size:** M. **Depends on:** 11.19

**Client messages:** MSG_COMBATREVEALHANGING

**Acceptance**

- [ ] Real client: cloaked trap hidden to enemies, revealed on trigger

### Detailed spec from CMB-17: Control effects II: polymorph, mind control, confusion, reshuffle, cloaked effects

Advanced state-changing effects work, including polymorph decks and revealed cloaked traps.

**Deliverables**

- kPolymorph/kUnPolymorph with saved hand, deck and stats (m_pSavedHand, m_pSavedPlayDeck, m_pSavedGameStats) using Decks/Polymorph Decks
- kMindControl, kConfusion/kConfusionBlock, kReshuffle, kModifyHate, kTaunt/kPacify, kUntargetable/kForceTargetable, kIntercept, kDelayCast
- Cloaked charms and wards (kCloakedCharm, kCloakedWard, kRevealCloak) with MSG_COMBATREVEALHANGING

**Client messages:** MSG_COMBATACTIONS, MSG_COMBATHAND, MSG_COMBATREVEALHANGING

**Data sources**

- Decks/Polymorph Decks (100)
- Spells/

**Acceptance**

- [ ] Sim test: polymorph swaps the deck to the polymorph deck; unpolymorph restores the exact previous deck and graveyard
- [ ] Real client: casting a polymorph turns the wizard model into the creature with its hand; a cloaked trap shows as a hidden icon to enemies and reveals with the reveal animation when triggered

**Risks**

- Polymorph deck contents in client Decks/ need decoding; they may be partial

## 11.21 Duel modifiers and battlefield effects (CMB-24 part 1)

**Goal:** Sigil rules at duel start.

**Size:** M. **Depends on:** 11.20, 11.15

**Client messages:** MSG_DUEL, MSG_COMBATACTIONS

**Acceptance**

- [ ] Fire bubble battlefield effect active from round 1

### Detailed spec from CMB-24: Duel modifiers, battlefield effects, combat triggers and sigil spells

Boss fights and special sigils apply their rules, battlefield effects, cheats and sigil-cast spells.

**Deliverables**

- DuelModifier from DuelModifierTemplate and sigil m_battlefieldEffects applied at duel start
- Combat triggers (kAddCombatTriggerList/kRemoveCombatTriggerList, CombatParticipant m_combatTriggerIDs) evaluated by ScriptMgr CombatScript boss scripts in scripts/<World>/
- MSG_SIGILSPELL sigil actions (Duel m_sigilActions)
- Initiative switch modes (Duel m_initiativeSwitchMode/m_initiativeSwitchRounds)

**Client messages:** MSG_SIGILSPELL, MSG_COMBATACTIONS, MSG_DUEL

**Data sources**

- Spells/*CombatTrigger-Spells folders
- Sigils/*Boss.xml

**Database tables**

- world: creature_combat_trigger (script binding, reloaded live with `.reload creature_combat_trigger` and applied from the next duel)

**Acceptance**

- [ ] Sim test: a sigil with a Fire bubble battlefield effect shows it as the global effect from round 1
- [ ] Real client: a scripted boss 'cheat' (e.g. casts a shield when hit by Fire) fires on the triggering action with its text and cinematic; the sigil spell icon appears in the HUD before it fires

**Risks**

- Boss trigger conditions are server-side logic and must be authored per boss as scripts

## 11.22 Combat triggers and sigil spells (CMB-24 part 2)

**Goal:** Boss cheats via CombatScript.

**Size:** M. **Depends on:** 11.21

**Client messages:** MSG_SIGILSPELL

**Acceptance**

- [ ] Real client: scripted boss shield cheat fires with text and cinematic; sigil spell icon shows first

### Detailed spec from CMB-24: Duel modifiers, battlefield effects, combat triggers and sigil spells

Boss fights and special sigils apply their rules, battlefield effects, cheats and sigil-cast spells.

**Deliverables**

- DuelModifier from DuelModifierTemplate and sigil m_battlefieldEffects applied at duel start
- Combat triggers (kAddCombatTriggerList/kRemoveCombatTriggerList, CombatParticipant m_combatTriggerIDs) evaluated by ScriptMgr CombatScript boss scripts in scripts/<World>/
- MSG_SIGILSPELL sigil actions (Duel m_sigilActions)
- Initiative switch modes (Duel m_initiativeSwitchMode/m_initiativeSwitchRounds)

**Client messages:** MSG_SIGILSPELL, MSG_COMBATACTIONS, MSG_DUEL

**Data sources**

- Spells/*CombatTrigger-Spells folders
- Sigils/*Boss.xml

**Database tables**

- world: creature_combat_trigger (script binding, reloaded live with `.reload creature_combat_trigger` and applied from the next duel)

**Acceptance**

- [ ] Sim test: a sigil with a Fire bubble battlefield effect shows it as the global effect from round 1
- [ ] Real client: a scripted boss 'cheat' (e.g. casts a shield when hit by Fire) fires on the triggering action with its text and cinematic; the sigil spell icon appears in the HUD before it fires

**Risks**

- Boss trigger conditions are server-side logic and must be authored per boss as scripts

## 11.23 Shadow pips and backlash (CMB-25 part 1)

**Goal:** Shadow pip rules.

**Size:** M. **Depends on:** 11.13, 4.16

**Client messages:** MSG_COMBATPIPS, MSG_COMBATACTIONS

**Acceptance**

- [ ] Shadow pip at threshold; backlash equals m_initialBacklash
- [ ] Combat.ShadowPip.RoundThreshold change applies from the next duel without a restart

### Detailed spec from CMB-25: Shadow magic, archmastery, backlash and school pips

Late-game pip systems (shadow pips, archmastery, school pips) and shadow creatures work.

**Deliverables**

- Shadow pip rules (ShadowPipRule, Duel shadow threshold fields, m_shadowPipRatingFactor), with the server defaults as live settings Combat.ShadowPip.* applied from the next duel
- ShadowSpellEffect, backlash (kBacklashDamage, kModifyBacklash), shadow creature levels, ShadowPactSpellEffect with MSG_COMBATMOVE ShadowPactTarget
- Archmastery school pips (ParticipantPipData m_arch/m_archPoints, PipCount school fields, kModifySchoolPips)
- Tiered spells (MSG_COMBATMOVE SelectedTieredSpellID)

**Client messages:** MSG_COMBATPIPS, MSG_COMBATMOVE, MSG_COMBATACTIONS

**Data sources**

- Spells/ shadow and school-pip spells

**Acceptance**

- [ ] Sim test: shadow pip gained at the configured round threshold (live setting Combat.ShadowPip.RoundThreshold), and a shadow spell adds backlash equal to its m_initialBacklash
- [ ] Real client: a shadow pip appears in the pip ring, casting a shadow-enhanced spell plays the shadow cinematic, and school pips render in their colors

**Risks**

- Very large and poorly documented; split further before starting

## 11.24 Shadow pacts and shadow creatures (CMB-25 part 2)

**Goal:** ShadowPactTarget and creature levels.

**Size:** M. **Depends on:** 11.23

**Client messages:** MSG_COMBATMOVE

**Acceptance**

- [ ] Real client: shadow-enhanced cinematic plays

### Detailed spec from CMB-25: Shadow magic, archmastery, backlash and school pips

Late-game pip systems (shadow pips, archmastery, school pips) and shadow creatures work.

**Deliverables**

- Shadow pip rules (ShadowPipRule, Duel shadow threshold fields, m_shadowPipRatingFactor), with the server defaults as live settings Combat.ShadowPip.* applied from the next duel
- ShadowSpellEffect, backlash (kBacklashDamage, kModifyBacklash), shadow creature levels, ShadowPactSpellEffect with MSG_COMBATMOVE ShadowPactTarget
- Archmastery school pips (ParticipantPipData m_arch/m_archPoints, PipCount school fields, kModifySchoolPips)
- Tiered spells (MSG_COMBATMOVE SelectedTieredSpellID)

**Client messages:** MSG_COMBATPIPS, MSG_COMBATMOVE, MSG_COMBATACTIONS

**Data sources**

- Spells/ shadow and school-pip spells

**Acceptance**

- [ ] Sim test: shadow pip gained at the configured round threshold (live setting Combat.ShadowPip.RoundThreshold), and a shadow spell adds backlash equal to its m_initialBacklash
- [ ] Real client: a shadow pip appears in the pip ring, casting a shadow-enhanced spell plays the shadow cinematic, and school pips render in their colors

**Risks**

- Very large and poorly documented; split further before starting

## 11.25 Archmastery school pips and tiered spells (CMB-25 part 3)

**Goal:** School pips and SelectedTieredSpellID.

**Size:** M. **Depends on:** 11.23

**Acceptance**

- [ ] Real client: school pips render in their colors

### Detailed spec from CMB-25: Shadow magic, archmastery, backlash and school pips

Late-game pip systems (shadow pips, archmastery, school pips) and shadow creatures work.

**Deliverables**

- Shadow pip rules (ShadowPipRule, Duel shadow threshold fields, m_shadowPipRatingFactor), with the server defaults as live settings Combat.ShadowPip.* applied from the next duel
- ShadowSpellEffect, backlash (kBacklashDamage, kModifyBacklash), shadow creature levels, ShadowPactSpellEffect with MSG_COMBATMOVE ShadowPactTarget
- Archmastery school pips (ParticipantPipData m_arch/m_archPoints, PipCount school fields, kModifySchoolPips)
- Tiered spells (MSG_COMBATMOVE SelectedTieredSpellID)

**Client messages:** MSG_COMBATPIPS, MSG_COMBATMOVE, MSG_COMBATACTIONS

**Data sources**

- Spells/ shadow and school-pip spells

**Acceptance**

- [ ] Sim test: shadow pip gained at the configured round threshold (live setting Combat.ShadowPip.RoundThreshold), and a shadow spell adds backlash equal to its m_initialBacklash
- [ ] Real client: a shadow pip appears in the pip ring, casting a shadow-enhanced spell plays the shadow cinematic, and school pips render in their colors

**Risks**

- Very large and poorly documented; split further before starting

<!-- Project Ambrose by Imjustchico: Roadmap phase 13, Pets, cantrips, crafting and minigames. -->

# Phase 13: Pets, cantrips, crafting and minigames

**Done when:** An equipped pet follows the wizard, eats snacks, levels up and casts in duels. Cantrips cast. Reagents are harvested and crafted. Kiosk minigames pay rewards, and the Crown Shop browses and sells.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 13.01 | Pet core (EXT-10) | M | 8.10, 6.16, 4.16 |
| 13.02 | Pet snacks, level, talents, rename (EXT-11) | M | 13.01 |
| 13.03 | Pets in combat (CMB-26 + EXT-22) | M | 13.02, 11.11 |
| 13.04 | Cantrips casting and XP (EXT-12) | M | 13.01, 8.05 |
| 13.05 | Cantrips advanced (EXT-13) | M | 13.04, 9.10 |
| 13.06 | Reagents and recipes (EXT-14) | M | 8.09, 7.07, 9.04, 4.15, 4.16 |
| 13.07 | Crafting (EXT-15) | M | 13.06 |
| 13.08 | Minigame kiosk and client-process framework (EXT-16 part 1) | M | 12.18, 1.16 |
| 13.09 | Concentration end to end (EXT-16 part 2) | M | 13.08 |
| 13.10 | Remaining simple minigames (EXT-17) | S | 13.09, 4.15 |
| 13.11 | Sorcery Stones state machine (EXT-18 part 1) | M | 13.09 |
| 13.12 | Sorcery Stones handlers and client (EXT-18 part 2) | M | 13.11 |
| 13.13 | Pet games: dance (EXT-19 part 1) | M | 13.02, 13.08 |
| 13.14 | Pet games: maze and jump (EXT-19 part 2) | M | 13.13 |
| 13.15 | Hatch session (EXT-20 part 1) | M | 13.02, 12.10, 4.16 |
| 13.16 | Egg incubation and inheritance (EXT-20 part 2) | M | 13.15, 13.07 |
| 13.17 | Pet morphing (EXT-21) | S | 13.16 |
| 13.18 | Crown shop list (EXT-8 part 1) | M | 12.15, 7.01, 4.15 |
| 13.19 | Crown shop segments (EXT-8 part 2) | M | 13.18 |
| 13.20 | Crown purchases, gifts, access passes (EXT-9) | M | 13.19, 6.14, 4.16 |
| 13.21 | Paid loot rolls (EXT-44) | S | 12.15, 10.09 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Correction.** 13.10 lists MSG_MG9_REWARDS, which does not exist. Service 54 (Messages/CatchAKeyMessages.xml) defines MSG_MG3_CONNECT/MOVED/REWARDS, the same names as service 44 (ShockALockMessages.xml).

## 13.01 Pet core (EXT-10)

**Goal:** Pet spawn/follow and energy.

**Size:** M. **Depends on:** 8.10, 6.16, 4.16

**Client messages:** MSG_PETUPDATEBEHAVIOR, MSG_PETENERGYTICK, MSG_PETENERGYMAX, MSG_LEASH, MSG_LEASHOFFSET, MSG_EQUIPMENTBEHAVIOR_EQUIPITEM, MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM, MSG_NEWOBJECT, MSG_DELETEOBJECT

**Acceptance**

- [ ] Energy never exceeds max; unequip despawns
- [ ] Real client: pet follows through doors; others see it
- [ ] Changing Pet.EnergyRegenInterval applies from the next energy tick without a restart

### Detailed spec from EXT-10: Pet core: equip, spawn/follow, energy

An equipped pet follows the wizard, and the energy bar ticks and caps correctly.

**Deliverables**

- game/Pets/PetMgr and Pet object on character (ServerPetOwnerBehavior state)
- Pet spawn/despawn on equip/unequip and zone change, leash offset
- Energy regen timer and max from level table, with the regen interval as the live setting Pet.EnergyRegenInterval

**Client messages:** MSG_PETUPDATEBEHAVIOR, MSG_PETENERGYTICK, MSG_PETENERGYMAX, MSG_LEASH, MSG_LEASHOFFSET, MSG_EQUIPMENTBEHAVIOR_EQUIPITEM, MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM, MSG_NEWOBJECT, MSG_DELETEOBJECT

**Data sources**

- ObjectData/Pets (1480) templates
- Energy per level: MagicLevels data in the client (exact file unverified)

**Database tables**

- characters.character_pet (item guid, level, xp, talents, stats)

**Acceptance**

- [ ] Unit: energy tick never exceeds max; unequip despawns pet object
- [ ] Client: equipping a pet spawns it beside the wizard, it follows while running and through zone doors, and other players see it; the energy bar refills over time

## 13.02 Pet snacks, level, talents, rename (EXT-11)

**Goal:** Pets grow.

**Size:** M. **Depends on:** 13.01

**Client messages:** MSG_PETSNACKADD, MSG_PETSNACKREMOVE, MSG_PETSNACKREMOVEREQUEST, MSG_PETSNACKUPDATE, MSG_GETSNACKLIST, MSG_SNACKLIST, MSG_FEEDINVENTORYITEM, MSG_PETLEVELUP, MSG_SHAREDBANKDELETEREAGENTORPETSNACK, MSG_SHAREDBANKDELETEREAGENTORPETSNACKCONFIRM

**Acceptance**

- [ ] Talent roll only from pool, no duplicates
- [ ] Real client: feeding shows XP; level-up names the talent
- [ ] `.settings set Rate.XP.Pet 2` doubles the next snack's pet XP without a restart

### Detailed spec from EXT-11: Pet snacks, XP, level up, talent manifestation, rename

Feeding snacks grows a pet through levels and reveals talents from its pool.

**Deliverables**

- Snack inventory, feed XP calculation with PetFeedingRewardConfig likes/dislikes, with every pet XP grant passing through Rate.XP.Pet
- Level-up with talent roll from talent pool, pet stat rules, with manifestation odds as the live setting Pet.TalentManifestChance
- Pet rename via name-part picker

**Client messages:** MSG_PETSNACKADD, MSG_PETSNACKREMOVE, MSG_PETSNACKREMOVEREQUEST, MSG_PETSNACKUPDATE, MSG_GETSNACKLIST, MSG_SNACKLIST, MSG_FEEDINVENTORYITEM, MSG_PETLEVELUP, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM, MSG_SHAREDBANKDELETEREAGENTORPETSNACK, MSG_SHAREDBANKDELETEREAGENTORPETSNACKCONFIRM

**Data sources**

- Root.wad PetFeedingRewardConfig.xml
- ObjectData/PetSnacks (478)
- TalentData/ (740) talent templates and pet talent pools
- PetTomePetTotals.xml (pet tome)

**Database tables**

- characters.character_pet_snack
- characters.character_pet_talent

**Acceptance**

- [ ] Unit: level thresholds match data; talent roll only picks from the pet's pool and never duplicates
- [ ] Client: feeding a snack shows the XP gain and snack consumed; reaching a new level plays the level-up popup naming the new talent, which is visible in the pet stats page after relog

**Risks**

- Talent manifestation odds are not in client data; behavior must be studied and the result becomes the default of Pet.TalentManifestChance

## 13.03 Pets in combat (CMB-26 + EXT-22)

**Goal:** May-cast and stat talents.

**Size:** M. **Depends on:** 13.02, 11.11

**Client messages:** MSG_SHOWPETCARD, MSG_PETWILLCAST, MSG_COMPANIONEFFECTS, MSG_COMBATACTIONS

**Acceptance**

- [ ] 100% act chance with requirement met casts
- [ ] Real client: pet card shows and pet casts

### Detailed spec from CMB-26: Pets in combat

Pet may-cast talents and pet cards fire in duels.

**Deliverables**

- Pet may-cast selection per round (WizGameStats m_petActChance), MSG_SHOWPETCARD, MSG_PETWILLCAST handling
- CombatAction m_petCast/m_petCasted/m_petCastTarget

**Client messages:** MSG_SHOWPETCARD, MSG_PETWILLCAST, MSG_COMBATACTIONS

**Data sources**

- Spells/ pet may-casts

**Acceptance**

- [ ] Sim test: at 100% act chance with a qualifying requirement the pet card is offered and cast
- [ ] Real client: the pet card appears in the HUD, and the pet leaps out and casts its spell

**Risks**

- Depends on PET domain data models

### Detailed spec from EXT-22: Pet talents in combat

Pet may-cast talents and stat talents take effect in duels.

**Deliverables**

- Pet stat talents folded into wizard stats
- May-cast trigger evaluation per round from PetTalentsTriggeredSpells.xml

**Client messages:** MSG_SHOWPETCARD, MSG_PETWILLCAST, MSG_COMPANIONEFFECTS

**Data sources**

- Root.wad PetTalentsTriggeredSpells.xml
- TalentData/

**Acceptance**

- [ ] Unit: trigger fires only under its condition (e.g. low health) and within cast chance
- [ ] Client: a pet with a heal may-cast shows the pet card and casts on the wizard when triggered; stat talents change the character stats page

## 13.04 Cantrips casting and XP (EXT-12)

**Goal:** Emote/effect/teleport cantrips.

**Size:** M. **Depends on:** 13.01, 8.05

**Client messages:** MSG_CANTRIPSSPELLCAST, MSG_CANTRIPSRESPONSE, MSG_CANTRIPSRESPONSEERROR, MSG_CASTEFFECT, MSG_UPDATECANTRIPXP, MSG_CANTRIPLEVELUP, MSG_CANTRIPENDLOOP, MSG_CANTRIPAFFECTEDPLAYER

**Acceptance**

- [ ] Insufficient energy errors
- [ ] Real client: dance cantrip seen by others; teleport cantrip moves

### Detailed spec from EXT-12: Cantrips casting, XP and level-up

Players learn cantrips and cast emote/effect/teleport cantrips for energy, gaining cantrip XP.

**Deliverables**

- game/Cantrips/CantripMgr (CantripsSpellTemplate effect dispatch)
- Handlers for spell cast and response/errors, energy cost
- Cantrip XP/level from CantripXPConfig.xml, with every cantrip XP grant passing through Rate.XP.Cantrip

**Client messages:** MSG_CANTRIPSSPELLCAST, MSG_CANTRIPSRESPONSE, MSG_CANTRIPSRESPONSEERROR, MSG_CASTEFFECT, MSG_UPDATECANTRIPXP, MSG_CANTRIPLEVELUP, MSG_CANTRIPENDLOOP, MSG_CANTRIPAFFECTEDPLAYER

**Data sources**

- Root.wad CantripXPConfig.xml
- Spells/ CantripsSpellTemplate entries
- CantripsItemTemplate in ObjectData

**Database tables**

- characters.character_cantrip (known, level, xp)

**Acceptance**

- [ ] Unit: cast with insufficient energy returns error; XP crosses threshold emits level up
- [ ] Client: casting a dance cantrip plays the animation seen by nearby players and drains energy; a teleport cantrip moves the wizard; cantrip XP bar advances

## 13.05 Cantrips advanced (EXT-13)

**Goal:** Invisibility, rituals, beneficial objects.

**Size:** M. **Depends on:** 13.04, 9.10

**Client messages:** MSG_CASTRITUAL, MSG_UPDATERITUALOBJECT, MSG_CANCELINVISIBLITY, MSG_CONTROLVISIBLITY, MSG_ENTERBENEFICIALOBJECT, MSG_EXITBENEFICIALOBJECT, MSG_BENEFICIALLUCKUPDATE, MSG_CANTRIPSIGILSPELL, MSG_CANTRIPNOAGGRO, MSG_CANTRIPTUTORIALEFFECT

**Acceptance**

- [ ] Invisible player excluded from aggro
- [ ] Real client: hidden to others, walk past mobs

### Detailed spec from EXT-13: Cantrips advanced: invisibility, rituals, beneficial objects, detector

Invisibility, ritual objects and luck/beneficial objects behave correctly for all viewers.

**Deliverables**

- Visibility control per viewer in object streaming
- Ritual object state and beneficial-object zones

**Client messages:** MSG_CASTRITUAL, MSG_UPDATERITUALOBJECT, MSG_CANCELINVISIBLITY, MSG_CONTROLVISIBLITY, MSG_ENTERBENEFICIALOBJECT, MSG_EXITBENEFICIALOBJECT, MSG_BENEFICIALLUCKUPDATE, MSG_CANTRIPSIGILSPELL, MSG_CANTRIPNOAGGRO, MSG_CANTRIPTUTORIALEFFECT

**Data sources**

- CantripsMajorInvisibilityEffect, CantripDetectorBehaviorTemplate in ObjectData

**Acceptance**

- [ ] Unit: invisible player is excluded from mob aggro checks; moving cancels invisibility per template rule
- [ ] Client: casting invisibility makes the wizard translucent to self and hidden to others and lets them walk past mobs; starting combat or the cancel request makes them visible again

## 13.06 Reagents and recipes (EXT-14)

**Goal:** Harvest and hold reagents.

**Size:** M. **Depends on:** 8.09, 7.07, 9.04, 4.15, 4.16

**Client messages:** MSG_REAGENTADD, MSG_REAGENTREMOVE, MSG_REAGENTREMOVEREQUEST, MSG_REAGENTUPDATE, MSG_RECIPEADD, MSG_RECIPEREMOVE

**Acceptance**

- [ ] Stack cap enforced
- [ ] Real client: harvest node despawns; reagent tab count; recipe bought
- [ ] Changing Reagent.RespawnSeconds or Rate.Respawn affects the next harvested node's respawn without a restart

### Detailed spec from EXT-14: Reagents and recipes

Players harvest world reagents, hold reagents and recipes, and see them in the crafting UI.

**Deliverables**

- Reagent stack storage (separate from backpack), recipe list
- Harvestable reagent spawn behavior with respawn timers from the live setting Reagent.RespawnSeconds, scaled by Rate.Respawn
- `.reload reagent_spawn` builds the spawn store off to the side, validates it, swaps it, spawns and despawns the difference in live zones, and keeps the old store on failure
- Recipe vendor (RecipeShopOption) via EXT-1 framework

**Client messages:** MSG_REAGENTADD, MSG_REAGENTREMOVE, MSG_REAGENTREMOVEREQUEST, MSG_REAGENTUPDATE, MSG_RECIPEADD, MSG_RECIPEREMOVE

**Data sources**

- ObjectData/Reagents (828)
- RecipeTemplate objects in ObjectData
- Reagent spawn points: zone data if present, otherwise world DB (unverified)

**Database tables**

- characters.character_reagent
- characters.character_recipe
- world.reagent_spawn

**Acceptance**

- [ ] Unit: reagent stack cap enforced; remove request for more than held rejected
- [ ] Client: clicking a glowing reagent node plays harvest, the node despawns, and the reagent count appears in the reagents tab; buying a recipe adds it to the recipe book

## 13.07 Crafting (EXT-15)

**Goal:** Craft at stations.

**Size:** M. **Depends on:** 13.06

**Client messages:** MSG_CRAFTINGSLOTCOUNT, MSG_CRAFTINGSLOTADD, MSG_CRAFTINGSLOTREMOVE, MSG_CLEARALLCRAFTINGSLOTS, MSG_USERECIPE, MSG_ALCHEMYSTATION

**Acceptance**

- [ ] Missing ingredients consume nothing; success atomic
- [ ] Real client: craft consumes and grants

### Detailed spec from EXT-15: Crafting stations and crafting slots

Players craft items at a crafting station using reagents and recipe slots.

**Deliverables**

- Crafting slot pool per character with add/remove/clear
- UseRecipe transaction: consume reagents plus item ingredients and grant result

**Client messages:** MSG_CRAFTINGSLOTCOUNT, MSG_CRAFTINGSLOTADD, MSG_CRAFTINGSLOTREMOVE, MSG_CLEARALLCRAFTINGSLOTS, MSG_USERECIPE, MSG_ALCHEMYSTATION

**Data sources**

- RecipeTemplate ingredient lists, CraftingSlot/CraftingSlotLootInfo, ObjectData/CraftedEquipment

**Database tables**

- characters.character_crafting_slot

**Acceptance**

- [ ] Unit: crafting without ingredients fails with the right Error code and consumes nothing; success is atomic
- [ ] Client: at a crafting station the recipe shows green when ingredients are held; crafting consumes them, shows the new item, and increases quantity for stackables

**Risks**

- Crafting cooldown timers per recipe unverified

## 13.08 Minigame kiosk and client-process framework (EXT-16 part 1)

**Goal:** MSG_MESSAGE_PROCESS routing.

**Size:** M. **Depends on:** 12.18, 1.16

**Client messages:** MSG_MINIGAMEKIOSK, MSG_MINIGAMESELECT, MSG_ENTERMINIGAME, MSG_LEAVEMINIGAME, MSG_START_CLIENT_PROCESS, MSG_MESSAGE_PROCESS, MSG_KILL_CLIENT_PROCESS, MSG_CLIENT_PROCESS_TERMINATED

**Acceptance**

- [ ] MSG_MESSAGE_PROCESS payload re-framed and decoded to MSG_MG1_REWARDS

### Detailed spec from EXT-16: Minigame kiosk and client-process framework (Concentration end to end)

A player can pick a minigame at a kiosk, play it, and receive mana/gold rewards.

**Deliverables**

- game/Minigames/MinigameMgr loading MinigameConfig.xml (MinigameInfo: name, zone, reward tables) as a 4.15 reload target, so `.reload minigame` swaps it and keeps the old config on failure
- Minigame gold rewards pass through Rate.Gold.Minigame
- ClientProcess session component: job ids, MSG_MESSAGE_PROCESS inner-message decode and routing to services 25/40-47/54
- Minigame handler for service 42 (Concentration): connect, moved, rewards with score sanity bounds
- Leaderboard (ScoreTrackingList) storage

**Client messages:** MSG_MINIGAMEKIOSK, MSG_MINIGAMESELECT, MSG_ENTERMINIGAME, MSG_LEAVEMINIGAME, MSG_START_CLIENT_PROCESS, MSG_MESSAGE_PROCESS, MSG_KILL_CLIENT_PROCESS, MSG_CLIENT_PROCESS_TERMINATED, MSG_MINIGAMEREWARDS, MSG_MINIGAMETIMERSTART, MSG_MINIGAMETIMEREND, MSG_MG1_CONNECT, MSG_MG1_MOVED, MSG_MG1_REWARDS

**Data sources**

- Root.wad MinigameConfig.xml (BINd)
- Root.wad Scripts/concentration/Client.lua (runs on the client; the server only names it)
- Minigame phantom zones (ThePhantomZoneWorld/*PhantomZone) in ZoneData

**Database tables**

- characters.minigame_score

**Acceptance**

- [ ] Unit: MSG_MESSAGE_PROCESS payload re-framed and decoded to MSG_MG1_REWARDS; score above configured max is clamped/rejected
- [ ] Client: clicking the minigame sign and choosing Concentration moves the wizard into the private phantom zone; the card game plays; on finish the reward screen shows mana/gold which update the HUD, and the player returns to the kiosk

**Risks**

- The inner-message framing inside MSG_MESSAGE_PROCESS is only known from the C# reference and needs checking against the client

## 13.09 Concentration end to end (EXT-16 part 2)

**Goal:** First minigame with rewards.

**Size:** M. **Depends on:** 13.08

**Client messages:** MSG_MINIGAMEREWARDS, MSG_MINIGAMETIMERSTART, MSG_MINIGAMETIMEREND, MSG_MG1_CONNECT, MSG_MG1_MOVED, MSG_MG1_REWARDS

**Acceptance**

- [ ] Score above max clamped/rejected
- [ ] Real client: kiosk to phantom zone, play, rewards update HUD, return

### Detailed spec from EXT-16: Minigame kiosk and client-process framework (Concentration end to end)

A player can pick a minigame at a kiosk, play it, and receive mana/gold rewards.

**Deliverables**

- game/Minigames/MinigameMgr loading MinigameConfig.xml (MinigameInfo: name, zone, reward tables) as a 4.15 reload target, so `.reload minigame` swaps it and keeps the old config on failure
- Minigame gold rewards pass through Rate.Gold.Minigame
- ClientProcess session component: job ids, MSG_MESSAGE_PROCESS inner-message decode and routing to services 25/40-47/54
- Minigame handler for service 42 (Concentration): connect, moved, rewards with score sanity bounds
- Leaderboard (ScoreTrackingList) storage

**Client messages:** MSG_MINIGAMEKIOSK, MSG_MINIGAMESELECT, MSG_ENTERMINIGAME, MSG_LEAVEMINIGAME, MSG_START_CLIENT_PROCESS, MSG_MESSAGE_PROCESS, MSG_KILL_CLIENT_PROCESS, MSG_CLIENT_PROCESS_TERMINATED, MSG_MINIGAMEREWARDS, MSG_MINIGAMETIMERSTART, MSG_MINIGAMETIMEREND, MSG_MG1_CONNECT, MSG_MG1_MOVED, MSG_MG1_REWARDS

**Data sources**

- Root.wad MinigameConfig.xml (BINd)
- Root.wad Scripts/concentration/Client.lua (runs on the client; the server only names it)
- Minigame phantom zones (ThePhantomZoneWorld/*PhantomZone) in ZoneData

**Database tables**

- characters.minigame_score

**Acceptance**

- [ ] Unit: MSG_MESSAGE_PROCESS payload re-framed and decoded to MSG_MG1_REWARDS; score above configured max is clamped/rejected
- [ ] Client: clicking the minigame sign and choosing Concentration moves the wizard into the private phantom zone; the card game plays; on finish the reward screen shows mana/gold which update the HUD, and the player returns to the kiosk

**Risks**

- The inner-message framing inside MSG_MESSAGE_PROCESS is only known from the C# reference and needs checking against the client

## 13.10 Remaining simple minigames (EXT-17)

**Goal:** Services 40,41,43,44,45,46,47,54.

**Size:** S. **Depends on:** 13.09, 4.15

**Client messages:** MSG_SKULLRIDERS_REWARDS, MSG_DOODLEDOUG_REWARDS, MSG_MG2_REWARDS, MSG_MG3_REWARDS, MSG_MG4_REWARDS, MSG_MG5_REWARDS, MSG_MG6_REWARDS, MSG_MG9_REWARDS

**Acceptance**

- [ ] Each service decodes REWARDS score/gameName
- [ ] Real client: each launches and shows rewards
- [ ] `.reload minigame_reward_bounds` applies an edited bound to the next reward, and a failed reload keeps the old bounds

### Detailed spec from EXT-17: Remaining simple minigames

All 3-message minigames (Skull Riders, Doodle Doug, Hot Shots, Shock-a-Lock, Choo Choo Zoo, Potion Motion, Dueling Diego, Catch-a-Key) are playable with rewards.

**Deliverables**

- Data-driven handlers for services 40,41,43,44,45,46,47,54 reusing EXT-16
- Per-game score bounds in world DB, reloaded live with `.reload minigame_reward_bounds` (validate, swap, keep the old bounds on failure)

**Client messages:** MSG_SKULLRIDERS_CONNECT, MSG_SKULLRIDERS_MOVED, MSG_SKULLRIDERS_REWARDS, MSG_DOODLEDOUG_CONNECT, MSG_DOODLEDOUG_MOVED, MSG_DOODLEDOUG_REWARDS, MSG_MG2_CONNECT, MSG_MG2_MOVED, MSG_MG2_REWARDS, MSG_MG3_CONNECT, MSG_MG3_MOVED, MSG_MG3_REWARDS, MSG_MG4_CONNECT, MSG_MG4_MOVED, MSG_MG4_REWARDS, MSG_MG5_CONNECT, MSG_MG5_MOVED, MSG_MG5_REWARDS, MSG_MG6_CONNECT, MSG_MG6_MOVED, MSG_MG6_REWARDS, MSG_MG9_CONNECT, MSG_MG9_MOVED, MSG_MG9_REWARDS

**Data sources**

- MinigameConfig.xml
- Scripts/<Game>/Client.lua names per game

**Database tables**

- world.minigame_reward_bounds

**Acceptance**

- [ ] Unit: each service id decodes its REWARDS score/gameName
- [ ] Client: each kiosk entry launches its game, and finishing shows the reward screen with a leaderboard

## 13.11 Sorcery Stones state machine (EXT-18 part 1)

**Goal:** Server-authoritative Soblocks logic.

**Size:** M. **Depends on:** 13.09

**Acceptance**

- [ ] Seeded row generator deterministic; level-up at thresholds

### Detailed spec from EXT-18: Sorcery Stones (Soblocks, service 25)

The server-authoritative Sorcery Stones minigame works, including rows, levels, attacks and win/loss.

**Deliverables**

- game/Minigames/Soblocks C++ game state machine (row generation, levels, attacks, countdown)
- Level thresholds and countdown length as the live settings Soblocks.LevelThresholds and Soblocks.CountdownSeconds, applied from the next game
- Handlers for the 23 service-25 messages

**Client messages:** MSG_SOBLOCKS_STARTSWAP, MSG_SOBLOCKS_PAUSE, MSG_SOBLOCKS_PAUSEON, MSG_SOBLOCKS_PAUSEOFF, MSG_SOBLOCKS_ADVANCEON, MSG_SOBLOCKS_ADVANCEOFF, MSG_SOBLOCKS_ENDGAME, MSG_SOBLOCKS_RESETGAME, MSG_SOBLOCKS_SELECTGAME, MSG_SOBLOCKS_SENDROW, MSG_SOBLOCKS_ROWINFO, MSG_SOBLOCKS_REQUESTROW, MSG_SOBLOCKS_LEVELUP, MSG_SOBLOCKS_READY, MSG_SOBLOCKS_SETLEVEL, MSG_SOBLOCKS_LOSS, MSG_SOBLOCKS_WIN, MSG_SOBLOCKS_COUNTDOWN, MSG_SOBLOCKS_INFO, MSG_SOBLOCKS_ROCKDROP, MSG_SOBLOCKS_FREEZEBLOCKS, MSG_SOBLOCKS_TIMEDDROP, MSG_SOBLOCKS_ATTACK

**Data sources**

- Root.wad Scripts/Soblocks/{Server,ServerCore,Shared,Config,GameData}.lua (behavior study only)

**Acceptance**

- [ ] Unit: row generator deterministic under a seed; level-up at configured thresholds
- [ ] Client: Sorcery Stones starts after countdown, rows rise, clearing blocks scores, and topping out shows the loss screen then rewards

**Risks**

- Scripts/*/Server.lua are the original server logic shipped in the client. Running them needs an embedded Lua runtime, a new dependency the maintainer has to approve, which would let the scripts reload live; reimplementing in C++ is clean-room but costs more, and its tunables become live settings instead

## 13.12 Sorcery Stones handlers and client (EXT-18 part 2)

**Goal:** 23 service-25 messages.

**Size:** M. **Depends on:** 13.11

**Client messages:** MSG_SOBLOCKS_STARTSWAP, MSG_SOBLOCKS_SENDROW, MSG_SOBLOCKS_REQUESTROW, MSG_SOBLOCKS_LEVELUP, MSG_SOBLOCKS_COUNTDOWN, MSG_SOBLOCKS_LOSS, MSG_SOBLOCKS_WIN, MSG_SOBLOCKS_ATTACK

**Acceptance**

- [ ] Real client: countdown, rows rise, topping out shows loss then rewards

### Detailed spec from EXT-18: Sorcery Stones (Soblocks, service 25)

The server-authoritative Sorcery Stones minigame works, including rows, levels, attacks and win/loss.

**Deliverables**

- game/Minigames/Soblocks C++ game state machine (row generation, levels, attacks, countdown)
- Level thresholds and countdown length as the live settings Soblocks.LevelThresholds and Soblocks.CountdownSeconds, applied from the next game
- Handlers for the 23 service-25 messages

**Client messages:** MSG_SOBLOCKS_STARTSWAP, MSG_SOBLOCKS_PAUSE, MSG_SOBLOCKS_PAUSEON, MSG_SOBLOCKS_PAUSEOFF, MSG_SOBLOCKS_ADVANCEON, MSG_SOBLOCKS_ADVANCEOFF, MSG_SOBLOCKS_ENDGAME, MSG_SOBLOCKS_RESETGAME, MSG_SOBLOCKS_SELECTGAME, MSG_SOBLOCKS_SENDROW, MSG_SOBLOCKS_ROWINFO, MSG_SOBLOCKS_REQUESTROW, MSG_SOBLOCKS_LEVELUP, MSG_SOBLOCKS_READY, MSG_SOBLOCKS_SETLEVEL, MSG_SOBLOCKS_LOSS, MSG_SOBLOCKS_WIN, MSG_SOBLOCKS_COUNTDOWN, MSG_SOBLOCKS_INFO, MSG_SOBLOCKS_ROCKDROP, MSG_SOBLOCKS_FREEZEBLOCKS, MSG_SOBLOCKS_TIMEDDROP, MSG_SOBLOCKS_ATTACK

**Data sources**

- Root.wad Scripts/Soblocks/{Server,ServerCore,Shared,Config,GameData}.lua (behavior study only)

**Acceptance**

- [ ] Unit: row generator deterministic under a seed; level-up at configured thresholds
- [ ] Client: Sorcery Stones starts after countdown, rows rise, clearing blocks scores, and topping out shows the loss screen then rewards

**Risks**

- Scripts/*/Server.lua are the original server logic shipped in the client. Running them needs an embedded Lua runtime, a new dependency the maintainer has to approve, which would let the scripts reload live; reimplementing in C++ is clean-room but costs more, and its tunables become live settings instead

## 13.13 Pet games: dance (EXT-19 part 1)

**Goal:** Pet XP from games.

**Size:** M. **Depends on:** 13.02, 13.08

**Client messages:** MSG_PETGAMEKIOSK, MSG_PETGAMEJOIN, MSG_PETGAMEJOINRSP, MSG_PETGAMEINIT, MSG_PETGAMEREADY, MSG_PETGAMESTART, MSG_PETGAMEDANCE, MSG_PETGAMEENDING, MSG_PETGAMEEND, MSG_PETGAMEINDIVIDUALRESULTS, MSG_PETGAMESNACKFEEDSUCCESS, MSG_PETGAMESNACKFEEDFAILED

**Acceptance**

- [ ] No energy rejected; too many dance moves rejected
- [ ] Real client: dance, feed snack, pet XP, energy drops

### Detailed spec from EXT-19: Pet games (pet pavilion single-player)

Players play pet games to earn pet XP and snacks, spending energy.

**Deliverables**

- game/Pets/PetGameMgr from PetGames.xml (game, track, energy cost, rewards)
- Session join/init/ready/start/end flow, result validation, snack reward drop
- Pet XP award path into EXT-11

**Client messages:** MSG_PETGAMEKIOSK, MSG_PETGAMEJOIN, MSG_PETGAMEJOINRSP, MSG_PETGAMEINIT, MSG_PETGAMEREADY, MSG_PETGAMESTART, MSG_PETGAMEDATA, MSG_PETGAMEJUMP, MSG_PETGAMEMAZE, MSG_PETGAMEDANCE, MSG_PETGAMEDROPBONUS, MSG_PETGAMEDROPOBJECT, MSG_PETGAMEENDING, MSG_PETGAMEEND, MSG_PETGAMEINDIVIDUALRESULTS, MSG_PETGAMESNACKFEEDSUCCESS, MSG_PETGAMESNACKFEEDFAILED

**Data sources**

- Root.wad PetGames.xml (PetGameConfig)

**Database tables**

- characters.pet_game_stats

**Acceptance**

- [ ] Unit: join without energy rejected; dance game result with more moves than the sequence rejected
- [ ] Client: at the pet pavilion kiosk choosing Dance Game plays it, then the feed-snack screen shows; feeding grants pet XP and the energy bar drops

**Risks**

- Split into dance game first then maze/jump games if one stretch proves too big

## 13.14 Pet games: maze and jump (EXT-19 part 2)

**Goal:** Other pet games.

**Size:** M. **Depends on:** 13.13

**Client messages:** MSG_PETGAMEDATA, MSG_PETGAMEJUMP, MSG_PETGAMEMAZE, MSG_PETGAMEDROPBONUS, MSG_PETGAMEDROPOBJECT

**Acceptance**

- [ ] Real client: maze and jump games award pet XP

### Detailed spec from EXT-19: Pet games (pet pavilion single-player)

Players play pet games to earn pet XP and snacks, spending energy.

**Deliverables**

- game/Pets/PetGameMgr from PetGames.xml (game, track, energy cost, rewards)
- Session join/init/ready/start/end flow, result validation, snack reward drop
- Pet XP award path into EXT-11

**Client messages:** MSG_PETGAMEKIOSK, MSG_PETGAMEJOIN, MSG_PETGAMEJOINRSP, MSG_PETGAMEINIT, MSG_PETGAMEREADY, MSG_PETGAMESTART, MSG_PETGAMEDATA, MSG_PETGAMEJUMP, MSG_PETGAMEMAZE, MSG_PETGAMEDANCE, MSG_PETGAMEDROPBONUS, MSG_PETGAMEDROPOBJECT, MSG_PETGAMEENDING, MSG_PETGAMEEND, MSG_PETGAMEINDIVIDUALRESULTS, MSG_PETGAMESNACKFEEDSUCCESS, MSG_PETGAMESNACKFEEDFAILED

**Data sources**

- Root.wad PetGames.xml (PetGameConfig)

**Database tables**

- characters.pet_game_stats

**Acceptance**

- [ ] Unit: join without energy rejected; dance game result with more moves than the sequence rejected
- [ ] Client: at the pet pavilion kiosk choosing Dance Game plays it, then the feed-snack screen shows; feeding grants pet XP and the energy bar drops

**Risks**

- Split into dance game first then maze/jump games if one stretch proves too big

## 13.15 Hatch session (EXT-20 part 1)

**Goal:** Two-player hatch window.

**Size:** M. **Depends on:** 13.02, 12.10, 4.16

**Client messages:** MSG_PETHATCHCREATE, MSG_PETHATCHREQUEST, MSG_PETHATCHJOINSTATUS, MSG_PETHATCHREADYSTATUS, MSG_PETHATCHRESULT, MSG_LENTPET

**Acceptance**

- [ ] Real client: both ready; egg appears with timer

### Detailed spec from EXT-20: Hatching and hatchmaking

Two players (or a kiosk) hatch pets into an egg that incubates and hatches with inherited talents.

**Deliverables**

- Hatch session (create/request/join/ready/result) with gold cost from the live setting Hatch.GoldCost
- Egg item in crafting slot with incubation timer and instant hatch for gold, from the live settings Hatch.IncubationSeconds and Hatch.InstantGoldCost
- Offspring talent inheritance rules, with odds as the live settings Hatch.InheritChance.Manifested and Hatch.InheritChance.Pool
- Hatchmaking kiosk listing (HatchmakingCrownsPets)

**Client messages:** MSG_PETHATCHCREATE, MSG_PETHATCHREQUEST, MSG_PETHATCHJOINSTATUS, MSG_PETHATCHREADYSTATUS, MSG_PETHATCHRESULT, MSG_PETHATCHED, MSG_HATCHEGGNOW, MSG_LENTPET

**Data sources**

- Root.wad HatchmakingCrownsPets.xml
- Pet template talent pools (TalentData)

**Database tables**

- characters.character_egg
- characters.hatchmaking_listing

**Acceptance**

- [ ] Unit: offspring talents are drawn only from the parents' manifested plus pool talents; egg timer survives restart
- [ ] Client: two wizards open the hatch window, both press ready, the egg appears in the backpack with a timer, and paying gold via hatch-now plays the hatch cinematic and adds the baby pet

**Risks**

- Inheritance odds are not in the client, so they must be studied from behavior, and the result becomes the defaults of Hatch.InheritChance.*
- Depends on a trade/ready-window pattern owned by social systems (prefix guessed)

## 13.16 Egg incubation and inheritance (EXT-20 part 2)

**Goal:** Hatch-now and talents.

**Size:** M. **Depends on:** 13.15, 13.07

**Client messages:** MSG_PETHATCHED, MSG_HATCHEGGNOW

**Acceptance**

- [ ] Offspring talents only from parents' manifested plus pool; timer survives restart
- [ ] Real client: hatch-now plays cinematic and adds baby pet
- [ ] Changing Hatch.IncubationSeconds applies to the next egg without a restart, and eggs already incubating keep their stored end time

### Detailed spec from EXT-20: Hatching and hatchmaking

Two players (or a kiosk) hatch pets into an egg that incubates and hatches with inherited talents.

**Deliverables**

- Hatch session (create/request/join/ready/result) with gold cost from the live setting Hatch.GoldCost
- Egg item in crafting slot with incubation timer and instant hatch for gold, from the live settings Hatch.IncubationSeconds and Hatch.InstantGoldCost
- Offspring talent inheritance rules, with odds as the live settings Hatch.InheritChance.Manifested and Hatch.InheritChance.Pool
- Hatchmaking kiosk listing (HatchmakingCrownsPets)

**Client messages:** MSG_PETHATCHCREATE, MSG_PETHATCHREQUEST, MSG_PETHATCHJOINSTATUS, MSG_PETHATCHREADYSTATUS, MSG_PETHATCHRESULT, MSG_PETHATCHED, MSG_HATCHEGGNOW, MSG_LENTPET

**Data sources**

- Root.wad HatchmakingCrownsPets.xml
- Pet template talent pools (TalentData)

**Database tables**

- characters.character_egg
- characters.hatchmaking_listing

**Acceptance**

- [ ] Unit: offspring talents are drawn only from the parents' manifested plus pool talents; egg timer survives restart
- [ ] Client: two wizards open the hatch window, both press ready, the egg appears in the backpack with a timer, and paying gold via hatch-now plays the hatch cinematic and adds the baby pet

**Risks**

- Inheritance odds are not in the client, so they must be studied from behavior, and the result becomes the defaults of Hatch.InheritChance.*
- Depends on a trade/ready-window pattern owned by social systems (prefix guessed)

## 13.17 Pet morphing (EXT-21)

**Goal:** Morph pairs.

**Size:** S. **Depends on:** 13.16

**Client messages:** MSG_PETMORPHSET, MSG_PETMORPHCANAFFORD, MSG_PETMORPHREADY, MSG_PETEGGMORPHED, MSG_PETMORPHINGSLOT

**Acceptance**

- [ ] Invalid pair returns cannot-afford/invalid
- [ ] Real client: countdown and morph egg

### Detailed spec from EXT-21: Pet morphing

Pets can be morphed into new forms per PetMorphing rules.

**Deliverables**

- Morph recipe lookup, cost check, morph slot timer from the live setting PetMorph.SlotSeconds

**Client messages:** MSG_PETMORPHSET, MSG_PETMORPHCANAFFORD, MSG_PETMORPHREADY, MSG_PETEGGMORPHED, MSG_PETMORPHINGSLOT

**Data sources**

- Root.wad PetMorphing.xml (BasePetMorphManager)

**Database tables**

- characters.character_egg (morph flags)

**Acceptance**

- [ ] Unit: invalid pet pair returns cannot-afford/invalid
- [ ] Client: the morph window accepts a valid pair, shows the morph slot countdown, and produces the morph egg

## 13.18 Crown shop list (EXT-8 part 1)

**Goal:** PCS list and catalog extractor.

**Size:** M. **Depends on:** 12.15, 7.01, 4.15

**Client messages:** MSG_PCS_LIST_REQUEST, MSG_PCS_LIST_RESPONSE, MSG_PCS_PATCH, MSG_SHOWCASEDSTOREITEMINFO

**Acceptance**

- [ ] Real client: Crown Shop opens with tabs

### Detailed spec from EXT-8: Crown shop catalog browse (PCS)

The in-game Crown Shop window opens and shows categorized items with prices.

**Deliverables**

- game/CrownShop/CrownShopMgr loading catalog from world DB; `.reload crown_shop` builds the catalog off to the side, validates it, swaps it, keeps the old one on failure, and bumps the catalog version so clients refetch
- PCS segment/list serializer
- tools extractor: builds world.crown_shop rows from ObjectData/CrownItems in the user's install

**Client messages:** MSG_PCS_LIST_REQUEST, MSG_PCS_LIST_RESPONSE, MSG_PCS_SEGDATA_REQUEST, MSG_PCS_SEGDATA_RESPONSE, MSG_PCS_CACHESEGREQSSUMMARY_REQUEST, MSG_PCS_PATCH, MSG_SHOWCASEDSTOREITEMINFO

**Data sources**

- ObjectData/CrownItems (29965 entries)
- Catalog tabs/categories layout not verified to exist in client data

**Database tables**

- world.crown_shop_item
- world.crown_shop_category

**Acceptance**

- [ ] Unit: segment request returns only items in the requested segment; cache summary reflects catalog version
- [ ] Client: opening the Crown Shop shows tabs and item tiles with crown prices, and search/filter works without client errors

**Risks**

- PCS wire format is not handled in the C# reference and has no sniffer capture; fully unverified RE work
- Size could force a split into list vs segdata

## 13.19 Crown shop segments (EXT-8 part 2)

**Goal:** PCS segdata and cache summary.

**Size:** M. **Depends on:** 13.18

**Client messages:** MSG_PCS_SEGDATA_REQUEST, MSG_PCS_SEGDATA_RESPONSE, MSG_PCS_CACHESEGREQSSUMMARY_REQUEST

**Acceptance**

- [ ] Segment request returns only its items; summary reflects catalog version
- [ ] Real client: item tiles, prices, search without errors
- [ ] `.reload crown_shop` after a price edit bumps the catalog version, and the next segment request shows the new price without a restart

### Detailed spec from EXT-8: Crown shop catalog browse (PCS)

The in-game Crown Shop window opens and shows categorized items with prices.

**Deliverables**

- game/CrownShop/CrownShopMgr loading catalog from world DB; `.reload crown_shop` builds the catalog off to the side, validates it, swaps it, keeps the old one on failure, and bumps the catalog version so clients refetch
- PCS segment/list serializer
- tools extractor: builds world.crown_shop rows from ObjectData/CrownItems in the user's install

**Client messages:** MSG_PCS_LIST_REQUEST, MSG_PCS_LIST_RESPONSE, MSG_PCS_SEGDATA_REQUEST, MSG_PCS_SEGDATA_RESPONSE, MSG_PCS_CACHESEGREQSSUMMARY_REQUEST, MSG_PCS_PATCH, MSG_SHOWCASEDSTOREITEMINFO

**Data sources**

- ObjectData/CrownItems (29965 entries)
- Catalog tabs/categories layout not verified to exist in client data

**Database tables**

- world.crown_shop_item
- world.crown_shop_category

**Acceptance**

- [ ] Unit: segment request returns only items in the requested segment; cache summary reflects catalog version
- [ ] Client: opening the Crown Shop shows tabs and item tiles with crown prices, and search/filter works without client errors

**Risks**

- PCS wire format is not handled in the C# reference and has no sniffer capture; fully unverified RE work
- Size could force a split into list vs segdata

## 13.20 Crown purchases, gifts, access passes (EXT-9)

**Goal:** Buy with crowns.

**Size:** M. **Depends on:** 13.19, 6.14, 4.16

**Client messages:** MSG_PCS_PRICE_LOCK_REQUEST, MSG_PCS_PRICE_LOCK_RESPONSE, MSG_PCS_PURCHASE_REQUEST, MSG_PCS_PURCHASE_RESPONSE, MSG_PCS_UPDATEUSERWISHLIST, MSG_ACCESSPASSINFOREQUEST, MSG_ACCESSPASSOFFER, MSG_ACCESSPASSBUYREQUEST, MSG_ACCESSPASSBUYCONFIRM, MSG_ACCESSPASSREJECTED, MSG_ACCESSPASSDECLINED, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_NOTIFY_GIFT, MSG_REQUEST_GIFTS, MSG_RECEIVE_GIFTS, MSG_REDEEM_GIFT, MSG_GIFT_REDEEMED, MSG_DELETE_GIFT

**Acceptance**

- [ ] Expired price lock rejected; gift to unknown name fails
- [ ] Real client: mount purchase; locked area offer then transfer
- [ ] Changing CrownShop.PriceLockSeconds applies to the next price lock without a restart

### Detailed spec from EXT-9: Crown shop purchase, gifting, wishlist, access passes

Players buy crown shop items, including area access passes and gifts to others.

**Deliverables**

- Price lock plus purchase transaction, with the lock timeout as the live setting CrownShop.PriceLockSeconds
- Gift and wishlist persistence
- Access pass gating hook for zone transfer (WLD)

**Client messages:** MSG_PCS_PRICE_LOCK_REQUEST, MSG_PCS_PRICE_LOCK_RESPONSE, MSG_PCS_PURCHASE_REQUEST, MSG_PCS_PURCHASE_RESPONSE, MSG_PCS_UPDATEUSERWISHLIST, MSG_ACCESSPASSINFOREQUEST, MSG_ACCESSPASSOFFER, MSG_ACCESSPASSBUYREQUEST, MSG_ACCESSPASSBUYCONFIRM, MSG_ACCESSPASSREJECTED, MSG_ACCESSPASSDECLINED, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_NOTIFY_GIFT, MSG_REQUEST_GIFTS, MSG_RECEIVE_GIFTS, MSG_REDEEM_GIFT, MSG_GIFT_REDEEMED, MSG_DELETE_GIFT

**Data sources**

- Root.wad AccessPass.xml
- ObjectData/CrownItems

**Database tables**

- login.account_access_pass
- characters.character_gifts
- characters.character_wishlist

**Acceptance**

- [ ] Unit: purchase after price lock expiry is rejected; gift to unknown name fails
- [ ] Client: buying a mount deducts crowns and the mount appears in the backpack; walking into a locked area without a pass shows the access pass offer, and after buying it the transfer proceeds

**Risks**

- Price lock timeout semantics unverified; the observed value becomes the default of CrownShop.PriceLockSeconds

## 13.21 Paid loot rolls (EXT-44)

**Goal:** Crowns extra roll after bosses.

**Size:** S. **Depends on:** 12.15, 10.09

**Client messages:** MSG_PAID_LOOT_ROLL_PROMPT, MSG_PAID_LOOT_ROLL_RESPONSE, MSG_PAID_LOOT_ROLL_RESULT, MSG_PAID_LOOT_CROWNS_BALANCE, MSG_PAID_LOOT_ROLL_ERROR

**Acceptance**

- [ ] Real client: prompt, crowns deducted, extra item

### Detailed spec from EXT-44: Paid loot rolls

After boss fights the paid extra loot roll prompt works with crowns.

**Deliverables**

- Paid roll prompt, crowns debit, roll result from drop table

**Client messages:** MSG_PAID_LOOT_ROLL_PROMPT, MSG_PAID_LOOT_ROLL_RESPONSE, MSG_PAID_LOOT_ROLL_RESULT, MSG_PAID_LOOT_CROWNS_BALANCE, MSG_PAID_LOOT_ROLL_ERROR

**Data sources**

- Drop table data (source per CMB loot milestone)

**Database tables**

- login.crown_transactions

**Acceptance**

- [ ] Client: after defeating a boss the roll prompt appears; accepting deducts crowns and shows the extra item

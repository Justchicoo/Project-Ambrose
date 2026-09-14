<!-- Project Ambrose by Imjustchico: Roadmap phase 15, Housing, gardening and fishing. -->

# Phase 15: Housing, gardening and fishing

**Done when:** Players go home, decorate, store items in attic and vaults, grow gardens, fish ponds, publish castle tours and build castle magic.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 15.01 | Own and enter a house (EXT-25) | M | 12.18, 8.09 |
| 15.02 | Furniture placement and persistence (EXT-26 part 1) | M | 15.01, 8.14, 4.16 |
| 15.03 | Visitor patches, pickup-all, blobs (EXT-26 part 2) | M | 15.02 |
| 15.04 | Attic (EXT-27 part 1) | M | 15.03 |
| 15.05 | Vaults (EXT-27 part 2) | M | 15.04 |
| 15.06 | Decor surfaces, tint, signs (EXT-28 part 1) | M | 15.03 |
| 15.07 | Teleporters, music, mannequins, housing pets (EXT-28 part 2) | M | 15.06, 13.01 |
| 15.08 | Island space and trains (EXT-29) | M | 15.03, 12.15 |
| 15.09 | Garden plant lifecycle (EXT-30 part 1) | M | 15.03 |
| 15.10 | Garden harvest and XP (EXT-30 part 2) | M | 15.09, 10.07 |
| 15.11 | Gardening spells (EXT-31) | M | 15.10, 13.01 |
| 15.12 | Fish populations and casting (EXT-32 part 1) | M | 6.16, 8.09, 13.01, 4.16 |
| 15.13 | Catching and fishing XP (EXT-32 part 2) | M | 15.12 |
| 15.14 | Fishing spells, aquariums, selling, tournaments (EXT-33) | M | 15.13, 15.03 |
| 15.15 | Castle tours publish and visit (EXT-40 part 1) | M | 15.07, 12.01 |
| 15.16 | Castle tours ratings and hall of fame (EXT-40 part 2) | M | 15.15 |
| 15.17 | Castle magic RE spike and persistence (EXT-41 part 1) | M | 15.07, 13.04 |
| 15.18 | Castle magic runtime (EXT-41 part 2) | M | 15.17 |
| 15.19 | Monster magic (EXT-42 part 1) | M | 15.18, 11.15 |
| 15.20 | Housing games (EXT-42 part 2) | M | 15.18 |
| 15.21 | Interactive music (EXT-42 part 3) | M | 15.07 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 15.01 lists MSG_GOHOME for entering a house. MSG_GOHOME (WizardMessages 12:66) is described as 'teleported to the world home (Hub)', which 10.13 already uses correctly. House and dorm travel go through MSG_REQUESTHOUSETELEPORT and MSG_GotoDormConfirm (53).
- **Oversized.** 15.17 castle magic RE spike and persistence (M). Two different kinds of work in one milestone.
- **Correction.** 15.01 uses MSG_GOHOME for house entry. Per WizardMessages.xml 12:66 it teleports to the world Hub.

## 15.01 Own and enter a house (EXT-25)

**Goal:** Private house instances.

**Size:** M. **Depends on:** 12.18, 8.09

**Client messages:** MSG_GOHOME, MSG_REQUESTHOUSETELEPORT, MSG_REQUESTHOUSINGZONE, MSG_REQUESTHOUSINGZONETELEPORT, MSG_REQUESTDEEDZONE, MSG_LEAVEHOUSINGLOT, MSG_HOUSINGZONEPLAYER, MSG_HOUSINGZONEPLAYERLIST, MSG_SETINVISIBLETOFRIENDS, MSG_REQUESTHOUSEOWNERCHARACTERID, MSG_TELEPORTTOSTART, MSG_PREVIEW_ISLAND

**Acceptance**

- [ ] Same template owners get separate instances
- [ ] Real client: Go Home loads house; friend joins same instance

### Detailed spec from EXT-25: Housing: own and enter a house

A player who owns a house deed can teleport home and walk around a private house zone.

**Deliverables**

- game/Housing/HousingMgr: deed item to house zone mapping, house instance keyed by owner character
- Go-home and leave-lot flows, visitor list, invisible-to-friends flag

**Client messages:** MSG_GOHOME, MSG_REQUESTHOUSETELEPORT, MSG_REQUESTHOUSINGZONE, MSG_REQUESTHOUSINGZONETELEPORT, MSG_REQUESTDEEDZONE, MSG_LEAVEHOUSINGLOT, MSG_HOUSINGZONEPLAYER, MSG_HOUSINGZONEPLAYERLIST, MSG_SETINVISIBLETOFRIENDS, MSG_REQUESTHOUSEOWNERCHARACTERID, MSG_TELEPORTTOSTART, MSG_PREVIEW_ISLAND

**Data sources**

- ObjectData/Housing (8828) deed and house templates
- House zones in ZoneData

**Database tables**

- characters.character_house

**Acceptance**

- [ ] Unit: two owners of the same house template get separate instances
- [ ] Client: pressing the Go Home button loads the owned house, the player walks inside and out to the lot, and a friend teleporting to the owner joins the same instance

## 15.02 Furniture placement and persistence (EXT-26 part 1)

**Goal:** Place, move, pick up.

**Size:** M. **Depends on:** 15.01, 8.14, 4.16

**Client messages:** MSG_PLACEHOUSINGOBJECT, MSG_UPDATEHOUSINGOBJECT, MSG_PLACEOBJECT, MSG_PICKUPOBJECT, MSG_UPDATEMAXIMUMHOUSINGITEMS, MSG_HOUSINGOBJECTNOPICKUP

**Acceptance**

- [ ] Over limit and non-owner rejected
- [ ] Real client: chair placed, moved, persists across restart
- [ ] Changing Housing.MaxItems updates the limit shown to an owner inside their house without a restart

### Detailed spec from EXT-26: Housing: place, move and pick up furniture

Players decorate: placed items persist and are seen by visitors.

**Deliverables**

- Housing object persistence (template, position, yaw, state), item-count limit from the live setting Housing.MaxItems, re-sent with MSG_UPDATEMAXIMUMHOUSINGITEMS when it changes
- Patch broadcast to everyone in the house
- Pickup-all

**Client messages:** MSG_PLACEHOUSINGOBJECT, MSG_UPDATEHOUSINGOBJECT, MSG_PLACEOBJECT, MSG_PICKUPOBJECT, MSG_PATCHADDHOUSINGOBJECT, MSG_PATCHDELETEHOUSINGOBJECT, MSG_PATCHUPDATEHOUSINGOBJECT, MSG_PATCHHOUSE, MSG_PICKUPALL, MSG_UPDATEMAXIMUMHOUSINGITEMS, MSG_HOUSINGOBJECTNOPICKUP, MSG_HIDEHOUSINGOBJECT, MSG_HOUSINGOBJECTPROXIMITY, MSG_SENDHOUSINGOBJECTSTATES, MSG_REQUEST_BLOBS, MSG_SEND_BLOB, MSG_SETHOUSECUSTOMIZATION

**Data sources**

- ObjectData/Housing item templates

**Database tables**

- characters.house_object

**Acceptance**

- [ ] Unit: placement over item limit rejected; non-owner place request rejected
- [ ] Client: dragging a chair from the backpack into the house places it; moving and rotating persist across relog and server restart; a visitor sees the same layout; Pick Up All returns every item to the backpack

**Risks**

- MSG_REQUEST_BLOBS/MSG_SEND_BLOB usage is unverified

## 15.03 Visitor patches, pickup-all, blobs (EXT-26 part 2)

**Goal:** Everyone sees layout.

**Size:** M. **Depends on:** 15.02

**Client messages:** MSG_PATCHADDHOUSINGOBJECT, MSG_PATCHDELETEHOUSINGOBJECT, MSG_PATCHUPDATEHOUSINGOBJECT, MSG_PATCHHOUSE, MSG_PICKUPALL, MSG_HIDEHOUSINGOBJECT, MSG_HOUSINGOBJECTPROXIMITY, MSG_SENDHOUSINGOBJECTSTATES, MSG_REQUEST_BLOBS, MSG_SEND_BLOB, MSG_SETHOUSECUSTOMIZATION

**Acceptance**

- [ ] Real client: visitor sees same layout; Pick Up All returns every item

### Detailed spec from EXT-26: Housing: place, move and pick up furniture

Players decorate: placed items persist and are seen by visitors.

**Deliverables**

- Housing object persistence (template, position, yaw, state), item-count limit from the live setting Housing.MaxItems, re-sent with MSG_UPDATEMAXIMUMHOUSINGITEMS when it changes
- Patch broadcast to everyone in the house
- Pickup-all

**Client messages:** MSG_PLACEHOUSINGOBJECT, MSG_UPDATEHOUSINGOBJECT, MSG_PLACEOBJECT, MSG_PICKUPOBJECT, MSG_PATCHADDHOUSINGOBJECT, MSG_PATCHDELETEHOUSINGOBJECT, MSG_PATCHUPDATEHOUSINGOBJECT, MSG_PATCHHOUSE, MSG_PICKUPALL, MSG_UPDATEMAXIMUMHOUSINGITEMS, MSG_HOUSINGOBJECTNOPICKUP, MSG_HIDEHOUSINGOBJECT, MSG_HOUSINGOBJECTPROXIMITY, MSG_SENDHOUSINGOBJECTSTATES, MSG_REQUEST_BLOBS, MSG_SEND_BLOB, MSG_SETHOUSECUSTOMIZATION

**Data sources**

- ObjectData/Housing item templates

**Database tables**

- characters.house_object

**Acceptance**

- [ ] Unit: placement over item limit rejected; non-owner place request rejected
- [ ] Client: dragging a chair from the backpack into the house places it; moving and rotating persist across relog and server restart; a visitor sees the same layout; Pick Up All returns every item to the backpack

**Risks**

- MSG_REQUEST_BLOBS/MSG_SEND_BLOB usage is unverified

## 15.04 Attic (EXT-27 part 1)

**Goal:** Attic storage.

**Size:** M. **Depends on:** 15.03

**Client messages:** MSG_REQUESTATTIC, MSG_SETATTICID, MSG_MOVETOATTIC, MSG_MOVEFROMATTIC, MSG_PATCHADDATTIC, MSG_PATCHDELETEATTIC, MSG_DELETEFROMATTIC, MSG_UPDATEATTICCOUNT, MSG_AUDITATTICRESULTS, MSG_PETTOMESCANATTIC

**Acceptance**

- [ ] Capacity enforced
- [ ] Real client: attic lists items; move to house places
- [ ] Changing Housing.AtticCapacity applies to the next move to the attic without a restart

### Detailed spec from EXT-27: Housing: attic and vaults

Players store housing items in the attic and use gear, jewel, TC and seed vaults.

**Deliverables**

- Attic storage with counts and audit, capacity from the live setting Housing.AtticCapacity
- Gear vault, jewel vault, treasure card vault and poster, gardening shed stores

**Client messages:** MSG_REQUESTATTIC, MSG_SETATTICID, MSG_MOVETOATTIC, MSG_MOVEFROMATTIC, MSG_PATCHADDATTIC, MSG_PATCHDELETEATTIC, MSG_DELETEFROMATTIC, MSG_UPDATEATTICCOUNT, MSG_AUDITATTICRESULTS, MSG_PETTOMESCANATTIC, MSG_MOVEGEARTOGEARVAULT, MSG_PATCHHOUSINGGEARVAULT, MSG_MOVEGEARFROMGEARVAULT, MSG_MOVEJEWELTOJEWELVAULT, MSG_PATCHHOUSINGJEWELVAULT, MSG_MOVEJEWELFROMJEWELVAULT, MSG_MOVETCTOTCVAULT, MSG_PATCHTREASURECARDVAULT, MSG_MOVETCFROMTCVAULT, MSG_ADDTOTREASURECARDPOSTER, MSG_PATCHTREASURECARDPOSTER, MSG_MOVESEEDTOGARDENINGSHED, MSG_PATCHHOUSINGGARDENINGSHED, MSG_MOVESEEDFROMGARDENINGSHED

**Data sources**

- GardeningShedBehavior, HousingObjectJewelWandContainer templates

**Database tables**

- characters.house_attic
- characters.house_vault

**Acceptance**

- [ ] Unit: attic capacity enforced; vault accepts only matching item kinds
- [ ] Client: the attic tab lists stored items and moving one to the house places it; putting gear into a placed gear vault removes it from the backpack and it can be withdrawn

## 15.05 Vaults (EXT-27 part 2)

**Goal:** Gear, jewel, TC, seed vaults.

**Size:** M. **Depends on:** 15.04

**Client messages:** MSG_MOVEGEARTOGEARVAULT, MSG_PATCHHOUSINGGEARVAULT, MSG_MOVEGEARFROMGEARVAULT, MSG_MOVEJEWELTOJEWELVAULT, MSG_PATCHHOUSINGJEWELVAULT, MSG_MOVEJEWELFROMJEWELVAULT, MSG_MOVETCTOTCVAULT, MSG_PATCHTREASURECARDVAULT, MSG_MOVETCFROMTCVAULT, MSG_ADDTOTREASURECARDPOSTER, MSG_PATCHTREASURECARDPOSTER, MSG_MOVESEEDTOGARDENINGSHED, MSG_PATCHHOUSINGGARDENINGSHED, MSG_MOVESEEDFROMGARDENINGSHED

**Acceptance**

- [ ] Vault accepts only matching kinds
- [ ] Real client: gear vault deposit/withdraw

### Detailed spec from EXT-27: Housing: attic and vaults

Players store housing items in the attic and use gear, jewel, TC and seed vaults.

**Deliverables**

- Attic storage with counts and audit, capacity from the live setting Housing.AtticCapacity
- Gear vault, jewel vault, treasure card vault and poster, gardening shed stores

**Client messages:** MSG_REQUESTATTIC, MSG_SETATTICID, MSG_MOVETOATTIC, MSG_MOVEFROMATTIC, MSG_PATCHADDATTIC, MSG_PATCHDELETEATTIC, MSG_DELETEFROMATTIC, MSG_UPDATEATTICCOUNT, MSG_AUDITATTICRESULTS, MSG_PETTOMESCANATTIC, MSG_MOVEGEARTOGEARVAULT, MSG_PATCHHOUSINGGEARVAULT, MSG_MOVEGEARFROMGEARVAULT, MSG_MOVEJEWELTOJEWELVAULT, MSG_PATCHHOUSINGJEWELVAULT, MSG_MOVEJEWELFROMJEWELVAULT, MSG_MOVETCTOTCVAULT, MSG_PATCHTREASURECARDVAULT, MSG_MOVETCFROMTCVAULT, MSG_ADDTOTREASURECARDPOSTER, MSG_PATCHTREASURECARDPOSTER, MSG_MOVESEEDTOGARDENINGSHED, MSG_PATCHHOUSINGGARDENINGSHED, MSG_MOVESEEDFROMGARDENINGSHED

**Data sources**

- GardeningShedBehavior, HousingObjectJewelWandContainer templates

**Database tables**

- characters.house_attic
- characters.house_vault

**Acceptance**

- [ ] Unit: attic capacity enforced; vault accepts only matching item kinds
- [ ] Client: the attic tab lists stored items and moving one to the house places it; putting gear into a placed gear vault removes it from the backpack and it can be withdrawn

## 15.06 Decor surfaces, tint, signs (EXT-28 part 1)

**Goal:** Wallpaper, flooring, palette, signs.

**Size:** M. **Depends on:** 15.03

**Client messages:** MSG_SENDHOUSINGTEXTUREINFO, MSG_PATCHTEXTUREREMAP, MSG_PICKUPHOUSINGTEXTURE, MSG_PATCHREMOVETEXTUREREMAP, MSG_SETTILEWALLPAPER, MSG_PATCHUPEXTENDEDTILE, MSG_SETHOUSINGPALETTE, MSG_PATCHTINTHOUSINGOBJECT, MSG_SETHOUSESIGN, MSG_PATCHHOUSINGSIGNOBJECT

**Acceptance**

- [ ] Real client: wallpaper for owner and visitors; sign text

### Detailed spec from EXT-28: Housing: decor surfaces, signs, teleporters, music, mannequins, housing pets

Wallpaper/flooring/tint, signs, house teleporters, music players, mannequins and roaming housing pets work.

**Deliverables**

- Texture remap and tile wallpaper persistence, palette tint
- Sign text from HousingSignText, teleporter linking to other houses
- Music player selection, mannequin equipment, housing pet roam

**Client messages:** MSG_SENDHOUSINGTEXTUREINFO, MSG_PATCHTEXTUREREMAP, MSG_PICKUPHOUSINGTEXTURE, MSG_PATCHREMOVETEXTUREREMAP, MSG_SETTILEWALLPAPER, MSG_PATCHUPEXTENDEDTILE, MSG_SETHOUSINGPALETTE, MSG_PATCHTINTHOUSINGOBJECT, MSG_SETHOUSESIGN, MSG_PATCHHOUSINGSIGNOBJECT, MSG_SETHOUSETELEPORTER, MSG_REQUESTTELEPORTERHOUSINGZONE, MSG_REQUESTTELEPORTERHOUSINGCONFIRM, MSG_PATCHHOUSINGTELEPORTEROBJECT, MSG_TELEPORTEDTODELETEDLOT, MSG_EQUIPHOUSEMUSIC, MSG_SELECTHOUSEMUSIC, MSG_PATCHHOUSINGMUSICOBJECT, MSG_REMOVEHOUSEMUSIC, MSG_SELECTMUSICSTYLE, MSG_EQUIPMANNEQUINITEM, MSG_PATCHEQUIPHOUSINGOBJECT, MSG_DELETEEQUIPPEDHOUSINGITEM, MSG_UPDATEHOUSINGPET, MSG_BLOCKPETSPAWN, MSG_CHANGEBREADCRUMBREQUEST, MSG_PATCHHOUSINGBREADCRUMB

**Data sources**

- Root.wad HousingSignText.xml, HousingNames.xml
- HousingMusicPlayerBehaviorTemplate, HousingTeleporterTargeting templates

**Database tables**

- characters.house_texture
- characters.house_object (extra state json or typed columns)

**Acceptance**

- [ ] Unit: teleporter to a deleted house triggers the deleted-lot path
- [ ] Client: applying wallpaper changes the walls for owner and visitors; a sign shows chosen text; a teleporter set to a friend's house sends the player there; a music player plays the selected track

**Risks**

- Split along decor vs teleporter/music if one stretch proves too big

## 15.07 Teleporters, music, mannequins, housing pets (EXT-28 part 2)

**Goal:** Interactive decor.

**Size:** M. **Depends on:** 15.06, 13.01

**Client messages:** MSG_SETHOUSETELEPORTER, MSG_REQUESTTELEPORTERHOUSINGZONE, MSG_REQUESTTELEPORTERHOUSINGCONFIRM, MSG_PATCHHOUSINGTELEPORTEROBJECT, MSG_TELEPORTEDTODELETEDLOT, MSG_EQUIPHOUSEMUSIC, MSG_SELECTHOUSEMUSIC, MSG_PATCHHOUSINGMUSICOBJECT, MSG_REMOVEHOUSEMUSIC, MSG_SELECTMUSICSTYLE, MSG_EQUIPMANNEQUINITEM, MSG_PATCHEQUIPHOUSINGOBJECT, MSG_DELETEEQUIPPEDHOUSINGITEM, MSG_UPDATEHOUSINGPET, MSG_BLOCKPETSPAWN, MSG_CHANGEBREADCRUMBREQUEST, MSG_PATCHHOUSINGBREADCRUMB

**Acceptance**

- [ ] Teleporter to deleted house triggers deleted-lot path
- [ ] Real client: teleporter to friend's house; music plays

### Detailed spec from EXT-28: Housing: decor surfaces, signs, teleporters, music, mannequins, housing pets

Wallpaper/flooring/tint, signs, house teleporters, music players, mannequins and roaming housing pets work.

**Deliverables**

- Texture remap and tile wallpaper persistence, palette tint
- Sign text from HousingSignText, teleporter linking to other houses
- Music player selection, mannequin equipment, housing pet roam

**Client messages:** MSG_SENDHOUSINGTEXTUREINFO, MSG_PATCHTEXTUREREMAP, MSG_PICKUPHOUSINGTEXTURE, MSG_PATCHREMOVETEXTUREREMAP, MSG_SETTILEWALLPAPER, MSG_PATCHUPEXTENDEDTILE, MSG_SETHOUSINGPALETTE, MSG_PATCHTINTHOUSINGOBJECT, MSG_SETHOUSESIGN, MSG_PATCHHOUSINGSIGNOBJECT, MSG_SETHOUSETELEPORTER, MSG_REQUESTTELEPORTERHOUSINGZONE, MSG_REQUESTTELEPORTERHOUSINGCONFIRM, MSG_PATCHHOUSINGTELEPORTEROBJECT, MSG_TELEPORTEDTODELETEDLOT, MSG_EQUIPHOUSEMUSIC, MSG_SELECTHOUSEMUSIC, MSG_PATCHHOUSINGMUSICOBJECT, MSG_REMOVEHOUSEMUSIC, MSG_SELECTMUSICSTYLE, MSG_EQUIPMANNEQUINITEM, MSG_PATCHEQUIPHOUSINGOBJECT, MSG_DELETEEQUIPPEDHOUSINGITEM, MSG_UPDATEHOUSINGPET, MSG_BLOCKPETSPAWN, MSG_CHANGEBREADCRUMBREQUEST, MSG_PATCHHOUSINGBREADCRUMB

**Data sources**

- Root.wad HousingSignText.xml, HousingNames.xml
- HousingMusicPlayerBehaviorTemplate, HousingTeleporterTargeting templates

**Database tables**

- characters.house_texture
- characters.house_object (extra state json or typed columns)

**Acceptance**

- [ ] Unit: teleporter to a deleted house triggers the deleted-lot path
- [ ] Client: applying wallpaper changes the walls for owner and visitors; a sign shows chosen text; a teleporter set to a friend's house sends the player there; a music player plays the selected track

**Risks**

- Split along decor vs teleporter/music if one stretch proves too big

## 15.08 Island space and trains (EXT-29)

**Goal:** Extra space and trains.

**Size:** M. **Depends on:** 15.03, 12.15

**Client messages:** MSG_ISLANDSPACESHOPOPEN, MSG_ISLANDSPACEBUYREQUEST, MSG_BUYISLANDSPACECONFIRM, MSG_UPDATEISLANDSPACE, MSG_REQUESTISLANDSWITCH, MSG_EMPTYLOTCHECK, MSG_REQUESTSENDAWAY, MSG_UPDATETRAIN, MSG_SETNEXTTRAIN, MSG_PATCHHOUSINGTRAIN, MSG_STARTTRAIN, MSG_PLAYTRAINSOUND

**Acceptance**

- [ ] Real client: item cap rises; train runs for visitors

### Detailed spec from EXT-29: Housing: island space and trains

Players buy extra island space and run placed trains.

**Deliverables**

- Island space purchase and plot switch
- Train track state and schedule

**Client messages:** MSG_ISLANDSPACESHOPOPEN, MSG_ISLANDSPACEBUYREQUEST, MSG_BUYISLANDSPACECONFIRM, MSG_UPDATEISLANDSPACE, MSG_REQUESTISLANDSWITCH, MSG_EMPTYLOTCHECK, MSG_REQUESTSENDAWAY, MSG_UPDATETRAIN, MSG_SETNEXTTRAIN, MSG_PATCHHOUSINGTRAIN, MSG_STARTTRAIN, MSG_PLAYTRAINSOUND

**Data sources**

- ObjectData/Housing island and train templates

**Database tables**

- characters.character_house (island_space)

**Acceptance**

- [ ] Client: buying island space raises the item cap shown; placing a train set and starting it makes the train run the track, seen by visitors

## 15.09 Garden plant lifecycle (EXT-30 part 1)

**Goal:** Growth including offline time.

**Size:** M. **Depends on:** 15.03

**Client messages:** MSG_GARDENINGCOMMAND, MSG_GARDENINGCOMMANDRESPONSE, MSG_PATCHGARDENING

**Acceptance**

- [ ] Simulated clock advances stages; unmet needs stall
- [ ] Real client: seedling reaches next stage
- [ ] Changing Rate.Gardening.Growth applies to growing plants from the next growth tick without a restart

### Detailed spec from EXT-30: Gardening: plant lifecycle and harvest

Players plant seeds, plants grow over real time with needs, and harvesting yields rewards and gardening XP.

**Deliverables**

- game/Gardening/GardenMgr: plant state machine, growth ticks including offline time, needs and pests, with growth time scaled by the live setting Rate.Gardening.Growth
- Harvest loot, second spring, gardening XP/level, with every gardening XP grant passing through Rate.XP.Gardening

**Client messages:** MSG_GARDENINGCOMMAND, MSG_GARDENINGCOMMANDRESPONSE, MSG_PATCHGARDENING, MSG_GARDENINGHARVESTPLANT, MSG_GARDENINGHARVESTPLANTSECONDSPRING, MSG_UPDATEGARDENINGXP, MSG_GARDENLEVELUP, MSG_GARDENINGCSRRESULTS

**Data sources**

- Root.wad GardeningXPConfig.xml
- GardeningBehaviorTemplate, GardenPlant, GardenData templates (seed templates in ObjectData)

**Database tables**

- characters.garden_plant

**Acceptance**

- [ ] Unit: growth stage advances by elapsed time with simulated clock; unmet needs stall growth
- [ ] Client: planting a seed in the house yard places a seedling; after its growth time scaled by Rate.Gardening.Growth it shows the next stage; harvesting at Elder drops rewards into the backpack and the gardening XP bar rises

## 15.10 Garden harvest and XP (EXT-30 part 2)

**Goal:** Rewards and gardening levels.

**Size:** M. **Depends on:** 15.09, 10.07

**Client messages:** MSG_GARDENINGHARVESTPLANT, MSG_GARDENINGHARVESTPLANTSECONDSPRING, MSG_UPDATEGARDENINGXP, MSG_GARDENLEVELUP, MSG_GARDENINGCSRRESULTS

**Acceptance**

- [ ] Real client: Elder harvest drops rewards; XP bar rises
- [ ] `.settings set Rate.XP.Gardening 2` doubles the next harvest's gardening XP without a restart

### Detailed spec from EXT-30: Gardening: plant lifecycle and harvest

Players plant seeds, plants grow over real time with needs, and harvesting yields rewards and gardening XP.

**Deliverables**

- game/Gardening/GardenMgr: plant state machine, growth ticks including offline time, needs and pests, with growth time scaled by the live setting Rate.Gardening.Growth
- Harvest loot, second spring, gardening XP/level, with every gardening XP grant passing through Rate.XP.Gardening

**Client messages:** MSG_GARDENINGCOMMAND, MSG_GARDENINGCOMMANDRESPONSE, MSG_PATCHGARDENING, MSG_GARDENINGHARVESTPLANT, MSG_GARDENINGHARVESTPLANTSECONDSPRING, MSG_UPDATEGARDENINGXP, MSG_GARDENLEVELUP, MSG_GARDENINGCSRRESULTS

**Data sources**

- Root.wad GardeningXPConfig.xml
- GardeningBehaviorTemplate, GardenPlant, GardenData templates (seed templates in ObjectData)

**Database tables**

- characters.garden_plant

**Acceptance**

- [ ] Unit: growth stage advances by elapsed time with simulated clock; unmet needs stall growth
- [ ] Client: planting a seed in the house yard places a seedling; after its growth time scaled by Rate.Gardening.Growth it shows the next stage; harvesting at Elder drops rewards into the backpack and the gardening XP bar rises

## 15.11 Gardening spells (EXT-31)

**Goal:** Energy spells on plants.

**Size:** M. **Depends on:** 15.10, 13.01

**Client messages:** MSG_GARDENINGCASTSPELL, MSG_GARDENINGPELLFIZZLE, MSG_GARDENINGSPELLINSPECT

**Acceptance**

- [ ] Spell on plant without matching need fizzles
- [ ] Real client: water clears need icon and drains energy

### Detailed spec from EXT-31: Gardening spells

Gardening spells (water, pest removal, area buffs) cast with energy on plants.

**Deliverables**

- GardenSpellTemplate dispatch, area targeting, energy use, fizzle rules

**Client messages:** MSG_GARDENINGCASTSPELL, MSG_GARDENINGPELLFIZZLE, MSG_GARDENINGSPELLINSPECT

**Data sources**

- Spells/ GardenSpellTemplate entries

**Acceptance**

- [ ] Unit: spell on a plant without the matching need fizzles and costs nothing (or as behavior shows)
- [ ] Client: casting a water spell over plants plays the effect, clears their water need icon, and drains energy

## 15.12 Fish populations and casting (EXT-32 part 1)

**Goal:** Per-instance fish and cast state.

**Size:** M. **Depends on:** 6.16, 8.09, 13.01, 4.16

**Client messages:** MSG_SETINSTANCEFISH, MSG_ADDINSTANCEFISH, MSG_SETFISHINGPLAYERS, MSG_BEGINFISHINGCAST, MSG_SHOWFISHINGCAST, MSG_ENDFISHINGCAST

**Acceptance**

- [ ] Real client: bobber shows for self and others
- [ ] Changing Fishing.RespawnSeconds or Rate.Respawn affects the next fish respawn in a loaded pond without a restart

### Detailed spec from EXT-32: Fishing: fish spawns and catching

Players fish in world ponds, catch fish, and earn fishing XP.

**Deliverables**

- game/Fishing/FishingMgr: per-instance fish population from pond data, respawn from the live setting Fishing.RespawnSeconds, scaled by Rate.Respawn
- Cast/catch/escape state machine with server-side success roll
- Fishing XP/level from FishingXPConfig.xml, with every fishing XP grant passing through Rate.XP.Fishing

**Client messages:** MSG_SETINSTANCEFISH, MSG_ADDINSTANCEFISH, MSG_SETFISHINGPLAYERS, MSG_BEGINFISHINGCAST, MSG_SHOWFISHINGCAST, MSG_ENDFISHINGCAST, MSG_CATCHFISH, MSG_REQUESTCATCHSUCCESS, MSG_CATCHSUCCESS, MSG_MISSFISH, MSG_FISHESCAPED, MSG_DISPLAYCATCHFISH, MSG_DISPLAYCAUGHTFISH

**Data sources**

- Root.wad FishingXPConfig.xml
- ObjectData/Fish (257), FishingBehaviorTemplate, FishingInfo
- Pond fish lists in zone data (location unverified)

**Database tables**

- characters.character_fish
- characters.character_fishing

**Acceptance**

- [ ] Unit: catching a fish removes it from the instance population for all anglers
- [ ] Client: casting at a pond shows the bobber, a bite prompts reeling, the caught fish window appears and the fish goes to the fish basket; other anglers see that fish disappear

## 15.13 Catching and fishing XP (EXT-32 part 2)

**Goal:** Server-side catch roll.

**Size:** M. **Depends on:** 15.12

**Client messages:** MSG_CATCHFISH, MSG_REQUESTCATCHSUCCESS, MSG_CATCHSUCCESS, MSG_MISSFISH, MSG_FISHESCAPED, MSG_DISPLAYCATCHFISH, MSG_DISPLAYCAUGHTFISH

**Acceptance**

- [ ] Caught fish removed for all anglers
- [ ] Real client: caught window, basket, others see fish vanish
- [ ] `.settings set Rate.XP.Fishing 2` doubles the next catch's fishing XP without a restart

### Detailed spec from EXT-32: Fishing: fish spawns and catching

Players fish in world ponds, catch fish, and earn fishing XP.

**Deliverables**

- game/Fishing/FishingMgr: per-instance fish population from pond data, respawn from the live setting Fishing.RespawnSeconds, scaled by Rate.Respawn
- Cast/catch/escape state machine with server-side success roll
- Fishing XP/level from FishingXPConfig.xml, with every fishing XP grant passing through Rate.XP.Fishing

**Client messages:** MSG_SETINSTANCEFISH, MSG_ADDINSTANCEFISH, MSG_SETFISHINGPLAYERS, MSG_BEGINFISHINGCAST, MSG_SHOWFISHINGCAST, MSG_ENDFISHINGCAST, MSG_CATCHFISH, MSG_REQUESTCATCHSUCCESS, MSG_CATCHSUCCESS, MSG_MISSFISH, MSG_FISHESCAPED, MSG_DISPLAYCATCHFISH, MSG_DISPLAYCAUGHTFISH

**Data sources**

- Root.wad FishingXPConfig.xml
- ObjectData/Fish (257), FishingBehaviorTemplate, FishingInfo
- Pond fish lists in zone data (location unverified)

**Database tables**

- characters.character_fish
- characters.character_fishing

**Acceptance**

- [ ] Unit: catching a fish removes it from the instance population for all anglers
- [ ] Client: casting at a pond shows the bobber, a bite prompts reeling, the caught fish window appears and the fish goes to the fish basket; other anglers see that fish disappear

## 15.14 Fishing spells, aquariums, selling, tournaments (EXT-33)

**Goal:** Fishing extras.

**Size:** M. **Depends on:** 15.13, 15.03

**Client messages:** MSG_FISHINGSPELLCAST, MSG_ADDFISHTOAQUARIUM, MSG_PATCHAQUARIUM, MSG_REMOVEFISHFROMAQUARIUM, MSG_PLACEHOUSINGFISH, MSG_NOFISHSPACE, MSG_SELLFISHOPEN, MSG_SELLFISHREQUEST, MSG_REQUESTALLFISH, MSG_REQUESTFISHHISTORY, MSG_DELETEFISH, MSG_CATCHOFTHEDAYOPEN, MSG_FISHTOURNAMENTOPEN, MSG_ENTERTOURNAMENTFISH, MSG_ENTERTOURNAMENTFISHRESULT, MSG_FISHTOURNAMENTLEADERBOARDOPEN, MSG_FISHTOURNAMENTLEADERBOARDREQUEST, MSG_FISHINGCSRRESULTS

**Acceptance**

- [ ] Real client: aquarium fish swims for visitors; selling grants gold; tournament leaderboard

### Detailed spec from EXT-33: Fishing spells, aquariums, selling and tournaments

Fishing spells, aquarium placement, fish selling and fish tournaments work.

**Deliverables**

- Fishing spell effects (reveal, chest)
- Aquarium storage and patching in houses
- Fish sell shop, catch of the day, fish tournament entry and leaderboard, with sell gold passing through Rate.Gold.Fishing

**Client messages:** MSG_FISHINGSPELLCAST, MSG_ADDFISHTOAQUARIUM, MSG_PATCHAQUARIUM, MSG_REMOVEFISHFROMAQUARIUM, MSG_PLACEHOUSINGFISH, MSG_NOFISHSPACE, MSG_SELLFISHOPEN, MSG_SELLFISHREQUEST, MSG_REQUESTALLFISH, MSG_REQUESTFISHHISTORY, MSG_DELETEFISH, MSG_CATCHOFTHEDAYOPEN, MSG_FISHTOURNAMENTOPEN, MSG_ENTERTOURNAMENTFISH, MSG_ENTERTOURNAMENTFISHRESULT, MSG_FISHTOURNAMENTLEADERBOARDOPEN, MSG_FISHTOURNAMENTLEADERBOARDREQUEST, MSG_FISHINGCSRRESULTS

**Data sources**

- AquariumBehaviorTemplate, FishSellingOption, FishTournamentOption, SortedCaughtFish

**Database tables**

- characters.house_aquarium
- characters.fish_tournament_entry

**Acceptance**

- [ ] Client: placing a fish in a placed aquarium shows it swimming for visitors; selling fish at the fish vendor grants gold; entering a tournament fish lists it on the leaderboard

## 15.15 Castle tours publish and visit (EXT-40 part 1)

**Goal:** List and visit houses.

**Size:** M. **Depends on:** 15.07, 12.01

**Client messages:** MSG_CASTLETOURSREQUEST, MSG_CASTLETOURSADD, MSG_CASTLETOURSPREADD, MSG_CASTLETOURSADDRESULT, MSG_CASTLETOURSREMOVE, MSG_CASTLETOURSREMOVERESULT, MSG_CASTLETOURSREQUESTMYCASTLES, MSG_CASTLETOURSREQUESTMYCASTLEDATA, MSG_CASTLETOURSVISITCASTLE, MSG_CASTLETOURSTELEPORTPLAYER, MSG_CASTLETOURSTELEPORTREJECTED, MSG_CASTLETOURSVISITIGNORED, MSG_CASTLETOURSREQUESTFRIENDS, MSG_REPORTHOUSE

**Acceptance**

- [ ] Real client: owner publishes; another player visits via the tours book

### Detailed spec from EXT-40: Castle tours

Players publish houses to castle tours, visit, rate, favorite and see leaderboards/hall of fame.

**Deliverables**

- game/Housing/CastleToursMgr (listing, ratings, favorites, hall of fame, bans)

**Client messages:** MSG_CASTLETOURSREQUEST, MSG_CASTLETOURSADD, MSG_CASTLETOURSPREADD, MSG_CASTLETOURSADDRESULT, MSG_CASTLETOURSREMOVE, MSG_CASTLETOURSREMOVERESULT, MSG_CASTLETOURSREQUESTMYCASTLES, MSG_CASTLETOURSREQUESTMYCASTLEDATA, MSG_CASTLETOURSVISITCASTLE, MSG_CASTLETOURSTELEPORTPLAYER, MSG_CASTLETOURSTELEPORTREJECTED, MSG_CASTLETOURSVISITIGNORED, MSG_CASTLETOURSENABLERATINGDISPLAY, MSG_CASTLETOURSRATINGDISPLAY, MSG_CASTLETOURSSENDRATING, MSG_CASTLETOURSPOSTRATEHOUSE, MSG_CASTLETOURSREQUESTLEADERBOARD, MSG_CASTLETOURSREQUESTNEXTLEADERBOARD, MSG_CASTLETOURSLEADERBOARDRESPONSE, MSG_CASTLETOURSREQUESTFRIENDS, MSG_CASTLETOURSADDFAVORITE, MSG_CASTLETOURSREMOVEFAVORITE, MSG_CASTLETOURSFAVORITEINFO, MSG_CASTLETOURSFAVORITEINFO2, MSG_CASTLETOURSHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERESPONSE, MSG_CASTLETOURSREMOVEHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERATINGS, MSG_CASTLETOURSCSRRESULTS, MSG_CASTLETOURSDELETECHARACTER, MSG_CASTLETOURSPLAYERBANNED, MSG_REPORTHOUSE

**Data sources**

- HousingHallOfFameBehavior templates

**Database tables**

- characters.castle_tour_listing
- characters.castle_tour_rating
- characters.castle_tour_favorite

**Acceptance**

- [ ] Client: owner adds a house to tours; another player opens the tours book, visits it, rates it, and the rating appears on the leaderboard

## 15.16 Castle tours ratings and hall of fame (EXT-40 part 2)

**Goal:** Ratings and leaderboards.

**Size:** M. **Depends on:** 15.15

**Client messages:** MSG_CASTLETOURSENABLERATINGDISPLAY, MSG_CASTLETOURSRATINGDISPLAY, MSG_CASTLETOURSSENDRATING, MSG_CASTLETOURSPOSTRATEHOUSE, MSG_CASTLETOURSREQUESTLEADERBOARD, MSG_CASTLETOURSREQUESTNEXTLEADERBOARD, MSG_CASTLETOURSLEADERBOARDRESPONSE, MSG_CASTLETOURSADDFAVORITE, MSG_CASTLETOURSREMOVEFAVORITE, MSG_CASTLETOURSFAVORITEINFO, MSG_CASTLETOURSFAVORITEINFO2, MSG_CASTLETOURSHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERESPONSE, MSG_CASTLETOURSREMOVEHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERATINGS, MSG_CASTLETOURSCSRRESULTS, MSG_CASTLETOURSDELETECHARACTER, MSG_CASTLETOURSPLAYERBANNED

**Acceptance**

- [ ] Real client: rating appears on leaderboard

### Detailed spec from EXT-40: Castle tours

Players publish houses to castle tours, visit, rate, favorite and see leaderboards/hall of fame.

**Deliverables**

- game/Housing/CastleToursMgr (listing, ratings, favorites, hall of fame, bans)

**Client messages:** MSG_CASTLETOURSREQUEST, MSG_CASTLETOURSADD, MSG_CASTLETOURSPREADD, MSG_CASTLETOURSADDRESULT, MSG_CASTLETOURSREMOVE, MSG_CASTLETOURSREMOVERESULT, MSG_CASTLETOURSREQUESTMYCASTLES, MSG_CASTLETOURSREQUESTMYCASTLEDATA, MSG_CASTLETOURSVISITCASTLE, MSG_CASTLETOURSTELEPORTPLAYER, MSG_CASTLETOURSTELEPORTREJECTED, MSG_CASTLETOURSVISITIGNORED, MSG_CASTLETOURSENABLERATINGDISPLAY, MSG_CASTLETOURSRATINGDISPLAY, MSG_CASTLETOURSSENDRATING, MSG_CASTLETOURSPOSTRATEHOUSE, MSG_CASTLETOURSREQUESTLEADERBOARD, MSG_CASTLETOURSREQUESTNEXTLEADERBOARD, MSG_CASTLETOURSLEADERBOARDRESPONSE, MSG_CASTLETOURSREQUESTFRIENDS, MSG_CASTLETOURSADDFAVORITE, MSG_CASTLETOURSREMOVEFAVORITE, MSG_CASTLETOURSFAVORITEINFO, MSG_CASTLETOURSFAVORITEINFO2, MSG_CASTLETOURSHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERESPONSE, MSG_CASTLETOURSREMOVEHALLOFFAME, MSG_CASTLETOURSHALLOFFAMERATINGS, MSG_CASTLETOURSCSRRESULTS, MSG_CASTLETOURSDELETECHARACTER, MSG_CASTLETOURSPLAYERBANNED, MSG_REPORTHOUSE

**Data sources**

- HousingHallOfFameBehavior templates

**Database tables**

- characters.castle_tour_listing
- characters.castle_tour_rating
- characters.castle_tour_favorite

**Acceptance**

- [ ] Client: owner adds a house to tours; another player opens the tours book, visits it, rates it, and the rating appears on the leaderboard

## 15.17 Castle magic RE spike and persistence (EXT-41 part 1)

**Goal:** Action model decoded and stored.

**Size:** M. **Depends on:** 15.07, 13.04

**Client messages:** MSG_CASTLEMAGICUPDATESTATE, MSG_CASTLEMAGICCHANGEACTION, MSG_PATCHCASTLEMAGIC, MSG_CASTLEMAGICCURRENTSTATE, MSG_CASTLEMAGICCLEAR

**Acceptance**

- [ ] doc/ action model; tagged setup survives restart

### Detailed spec from EXT-41: Castle magic

Castle magic spells and orbs let players script house interactions that persist and run for visitors.

**Deliverables**

- Castle magic action graph persistence and runtime per house instance

**Client messages:** MSG_CASTLEMAGICUPDATESTATE, MSG_CASTLEMAGICCHANGEACTION, MSG_PATCHCASTLEMAGIC, MSG_CASTLEMAGICCURRENTSTATE, MSG_CASTLEMAGICCLEAR, MSG_CASTLEMAGICORB, MSG_CASTLEMAGICTUTORIAL, MSG_CASTLEMAGICREQUESTUSE, MSG_CASTLEMAGICREQUESTALLOWMOUNTS, MSG_CASTLEMAGICREQUESTPLAYERTELEPORT, MSG_CASTLEMAGICREQUESTRANDOM, MSG_CASTLEMAGICREQUESTALLOWPVP, MSG_CASTLEMAGICREQUESTPVPSTATE, MSG_CASTLEMAGICCANTRIP, MSG_REQUESTTRANSITION, MSG_REQUESTRESTORESTATE

**Data sources**

- CastleMagic* classes (48 in type dump)

**Database tables**

- characters.house_castle_magic

**Acceptance**

- [ ] Client: tagging a door with a castle magic spell and a trigger orb makes the door open when a visitor steps on the orb, and the setup survives restart

**Risks**

- Large and poorly documented; RE of the action model required before splitting

## 15.18 Castle magic runtime (EXT-41 part 2)

**Goal:** Orb triggers run for visitors.

**Size:** M. **Depends on:** 15.17

**Client messages:** MSG_CASTLEMAGICORB, MSG_CASTLEMAGICTUTORIAL, MSG_CASTLEMAGICREQUESTUSE, MSG_CASTLEMAGICREQUESTALLOWMOUNTS, MSG_CASTLEMAGICREQUESTPLAYERTELEPORT, MSG_CASTLEMAGICREQUESTRANDOM, MSG_CASTLEMAGICREQUESTALLOWPVP, MSG_CASTLEMAGICREQUESTPVPSTATE, MSG_CASTLEMAGICCANTRIP, MSG_REQUESTTRANSITION, MSG_REQUESTRESTORESTATE

**Acceptance**

- [ ] Real client: door opens when a visitor steps on the orb

### Detailed spec from EXT-41: Castle magic

Castle magic spells and orbs let players script house interactions that persist and run for visitors.

**Deliverables**

- Castle magic action graph persistence and runtime per house instance

**Client messages:** MSG_CASTLEMAGICUPDATESTATE, MSG_CASTLEMAGICCHANGEACTION, MSG_PATCHCASTLEMAGIC, MSG_CASTLEMAGICCURRENTSTATE, MSG_CASTLEMAGICCLEAR, MSG_CASTLEMAGICORB, MSG_CASTLEMAGICTUTORIAL, MSG_CASTLEMAGICREQUESTUSE, MSG_CASTLEMAGICREQUESTALLOWMOUNTS, MSG_CASTLEMAGICREQUESTPLAYERTELEPORT, MSG_CASTLEMAGICREQUESTRANDOM, MSG_CASTLEMAGICREQUESTALLOWPVP, MSG_CASTLEMAGICREQUESTPVPSTATE, MSG_CASTLEMAGICCANTRIP, MSG_REQUESTTRANSITION, MSG_REQUESTRESTORESTATE

**Data sources**

- CastleMagic* classes (48 in type dump)

**Database tables**

- characters.house_castle_magic

**Acceptance**

- [ ] Client: tagging a door with a castle magic spell and a trigger orb makes the door open when a visitor steps on the orb, and the setup survives restart

**Risks**

- Large and poorly documented; RE of the action model required before splitting

## 15.19 Monster magic (EXT-42 part 1)

**Goal:** Monster arenas.

**Size:** M. **Depends on:** 15.18, 11.15

**Client messages:** MSG_MONSTERMAGICADDMONSTER, MSG_PATCHMONSTERARENA, MSG_MONSTERMAGICERASEMONSTER

**Acceptance**

- [ ] Real client: arena monster is duelable

### Detailed spec from EXT-42: Monster magic, housing games, interactive music (coarse)

Monster arenas, housing mini-games and interactive music instruments work in houses.

**Deliverables**

- Monster arena spawns, housing game state machines, interactive music sessions

**Client messages:** MSG_MONSTERMAGICADDMONSTER, MSG_PATCHMONSTERARENA, MSG_MONSTERMAGICERASEMONSTER, MSG_STARTHOUSINGGAME, MSG_SETHOUSINGGAMESTATE, MSG_HOUSINGGAMEUSEPOWERUP, MSG_HOUSINGGAMEREQUESTPOWERUPLOCATIONS, MSG_HOUSINGGAMESTATUSUPDATE, MSG_CASTLEGAMESREQUEST, MSG_CASTLEGAMESREQUESTGAMEDATA, MSG_CASTLEGAMESTELEPORTREJECTED, MSG_REQUESTINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC2, MSG_PLAYINTERACTIVEMUSICLOOP, MSG_STOPINTERACTIVEMUSICLOOP, MSG_SETMUSICLOOP, MSG_PATCHMUSICLOOP, MSG_DELETEMUSICLOOP, MSG_PLAYINTERACTIVEMUSICLOOPING, MSG_PLAYINTERACTIVEMUSICLOOPING2, MSG_INTERACTIVEMUSICINUSE, MSG_INTERACTIVEMUSICINUSE2, MSG_INTERACTIVEMUSICERROR

**Data sources**

- HousingGameManager, HousingGameKhanDanceTurnData and related types

**Database tables**

- characters.house_monster_arena

**Acceptance**

- [ ] Client: placing a monster in an arena spawns a duelable mob; playing an instrument is heard by other visitors

**Risks**

- Needs to be split into three milestones once the earlier housing work is done

## 15.20 Housing games (EXT-42 part 2)

**Goal:** Castle games.

**Size:** M. **Depends on:** 15.18

**Client messages:** MSG_STARTHOUSINGGAME, MSG_SETHOUSINGGAMESTATE, MSG_HOUSINGGAMEUSEPOWERUP, MSG_HOUSINGGAMEREQUESTPOWERUPLOCATIONS, MSG_HOUSINGGAMESTATUSUPDATE, MSG_CASTLEGAMESREQUEST, MSG_CASTLEGAMESREQUESTGAMEDATA, MSG_CASTLEGAMESTELEPORTREJECTED

**Acceptance**

- [ ] Real client: housing game starts and reports status

### Detailed spec from EXT-42: Monster magic, housing games, interactive music (coarse)

Monster arenas, housing mini-games and interactive music instruments work in houses.

**Deliverables**

- Monster arena spawns, housing game state machines, interactive music sessions

**Client messages:** MSG_MONSTERMAGICADDMONSTER, MSG_PATCHMONSTERARENA, MSG_MONSTERMAGICERASEMONSTER, MSG_STARTHOUSINGGAME, MSG_SETHOUSINGGAMESTATE, MSG_HOUSINGGAMEUSEPOWERUP, MSG_HOUSINGGAMEREQUESTPOWERUPLOCATIONS, MSG_HOUSINGGAMESTATUSUPDATE, MSG_CASTLEGAMESREQUEST, MSG_CASTLEGAMESREQUESTGAMEDATA, MSG_CASTLEGAMESTELEPORTREJECTED, MSG_REQUESTINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC2, MSG_PLAYINTERACTIVEMUSICLOOP, MSG_STOPINTERACTIVEMUSICLOOP, MSG_SETMUSICLOOP, MSG_PATCHMUSICLOOP, MSG_DELETEMUSICLOOP, MSG_PLAYINTERACTIVEMUSICLOOPING, MSG_PLAYINTERACTIVEMUSICLOOPING2, MSG_INTERACTIVEMUSICINUSE, MSG_INTERACTIVEMUSICINUSE2, MSG_INTERACTIVEMUSICERROR

**Data sources**

- HousingGameManager, HousingGameKhanDanceTurnData and related types

**Database tables**

- characters.house_monster_arena

**Acceptance**

- [ ] Client: placing a monster in an arena spawns a duelable mob; playing an instrument is heard by other visitors

**Risks**

- Needs to be split into three milestones once the earlier housing work is done

## 15.21 Interactive music (EXT-42 part 3)

**Goal:** Instruments heard by visitors.

**Size:** M. **Depends on:** 15.07

**Client messages:** MSG_REQUESTINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC2, MSG_PLAYINTERACTIVEMUSICLOOP, MSG_STOPINTERACTIVEMUSICLOOP, MSG_SETMUSICLOOP, MSG_PATCHMUSICLOOP, MSG_DELETEMUSICLOOP, MSG_PLAYINTERACTIVEMUSICLOOPING, MSG_PLAYINTERACTIVEMUSICLOOPING2, MSG_INTERACTIVEMUSICINUSE, MSG_INTERACTIVEMUSICINUSE2, MSG_INTERACTIVEMUSICERROR

**Acceptance**

- [ ] Real client: instrument heard by other visitors

### Detailed spec from EXT-42: Monster magic, housing games, interactive music (coarse)

Monster arenas, housing mini-games and interactive music instruments work in houses.

**Deliverables**

- Monster arena spawns, housing game state machines, interactive music sessions

**Client messages:** MSG_MONSTERMAGICADDMONSTER, MSG_PATCHMONSTERARENA, MSG_MONSTERMAGICERASEMONSTER, MSG_STARTHOUSINGGAME, MSG_SETHOUSINGGAMESTATE, MSG_HOUSINGGAMEUSEPOWERUP, MSG_HOUSINGGAMEREQUESTPOWERUPLOCATIONS, MSG_HOUSINGGAMESTATUSUPDATE, MSG_CASTLEGAMESREQUEST, MSG_CASTLEGAMESREQUESTGAMEDATA, MSG_CASTLEGAMESTELEPORTREJECTED, MSG_REQUESTINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC, MSG_PLAYINTERACTIVEMUSIC2, MSG_PLAYINTERACTIVEMUSICLOOP, MSG_STOPINTERACTIVEMUSICLOOP, MSG_SETMUSICLOOP, MSG_PATCHMUSICLOOP, MSG_DELETEMUSICLOOP, MSG_PLAYINTERACTIVEMUSICLOOPING, MSG_PLAYINTERACTIVEMUSICLOOPING2, MSG_INTERACTIVEMUSICINUSE, MSG_INTERACTIVEMUSICINUSE2, MSG_INTERACTIVEMUSICERROR

**Data sources**

- HousingGameManager, HousingGameKhanDanceTurnData and related types

**Database tables**

- characters.house_monster_arena

**Acceptance**

- [ ] Client: placing a monster in an arena spawns a duelable mob; playing an instrument is heard by other visitors

**Risks**

- Needs to be split into three milestones once the earlier housing work is done

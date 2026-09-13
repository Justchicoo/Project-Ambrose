<!-- Project Ambrose by Imjustchico: Roadmap phase 14, Dungeons, tutorial and PvP. -->

# Phase 14: Dungeons, tutorial and PvP

**Done when:** Groups enter sigil dungeons with countdowns. New wizards play the scripted tutorial, and players queue for ranked PvP, tournaments, pet derby and daily assignments.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 14.01 | Dungeon sigils and zone timers (EXT-23) | M | 12.18, 12.05, 10.01 |
| 14.02 | Tutorial manager (EXT-24) | M | 10.17, 3.16, 8.10 |
| 14.03 | Scripted tutorial duel (CMB-28) | M | 14.02, 11.09 |
| 14.04 | PvP scalars and restrictions (CMB-27 part 1) | M | 11.06, 11.18 |
| 14.05 | Alternating turns (CMB-27 part 2) | M | 14.04 |
| 14.06 | PvP queue and brackets (EXT-34 part 1) | M | 14.05, 14.01 |
| 14.07 | Ready check and arena match (EXT-34 part 2) | M | 14.06 |
| 14.08 | Ratings, tickets, leaderboards, daily PvP (EXT-35) | M | 14.07, 10.10 |
| 14.09 | Tournament scheduler and brackets (EXT-36 part 1) | M | 14.08 |
| 14.10 | Tournament credits and rewards (EXT-36 part 2) | M | 14.09 |
| 14.11 | Derby race simulation (EXT-37 part 1) | M | 13.13, 14.07 |
| 14.12 | Derby laps and results (EXT-37 part 2) | M | 14.11 |
| 14.13 | Derby abilities and cheers (EXT-38) | M | 14.12 |
| 14.14 | Battlegrounds RE spike (EXT-39 part 1) | M | 14.08, 11.19 |
| 14.15 | Battleground sigil, polymorph, POI scoring (EXT-39 part 2) | L | 14.14 |
| 14.16 | Daily assignments (EXT-43) | M | 10.08 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 14.02/14.03 (tutorial) sit after 10.17, yet every new character's first session is the tutorial. Until then, 3.16 must place new characters directly in WC_Hub and skip the tutorial flags. That should be an explicit interim decision, not only a question in decisions_needed.
- **Oversized.** 14.15 battleground match (L). Explicitly large. With the corrected facts (dedicated BG messages exist) it should be split into queue, polymorph select, POI scoring and end/rewards.
- **Correction.** 14.14 'Battlegrounds RE spike: Find where BG state travels (no dedicated messages)' is wrong. WizardMessages2.xml (svc 53) has dozens of BG messages: MSG_BATTLEGROUNDQUEUEPLAYER, MSG_BATTLEGROUNDQUEUEUPDATE, MSG_BGQueueStatus, MSG_BGPOIUpdate, MSG_BGPlayerSync, MSG_BGPlayerStatsUpdate, MSG_SetBGPolymorphLevel, MSG_BGSELECTPOLYMORPHREQUEST, MSG_BATTLEGROUNDUPDATEPOINTS, MSG_BATTLEGROUNDEND, MSG_REQUESTBATTLEGROUNDSLOOT, among others. Root.wad also has Sigils/BGPolymorphSigil.xml and Sigils/BattlegroundSigil.xml. 14.15's message list (MATCHMAKERUPDATE etc.) is the wrong set.
- **Correction.** 14.06 lists MSG_ARENAERROR. The actual name is MSG_ARENA_ERROR.

## 14.01 Dungeon sigils and zone timers (EXT-23)

**Goal:** Countdown into private instance.

**Size:** M. **Depends on:** 12.18, 12.05, 10.01

**Client messages:** MSG_LEAVESIGILTIMERWAITING, MSG_DISPOSABLEDUNGEONNOOWNER, MSG_ADDZONETIMER, MSG_UPDATEZONETIMER, MSG_REMOVEZONETIMER, MSG_UPDATEZONECOUNTER, MSG_TELEPORT_TO_GAME_ZONE, MSG_ZONEEVENTTIMEREXPIRED, MSG_POSTZONEEVENTFROMCLIENT

**Acceptance**

- [ ] Leavers excluded; groups get different ids
- [ ] Real client: countdown, private instance, skeleton key consumed

### Detailed spec from EXT-23: Dungeon sigils and private instances

Players (solo or team) enter a dungeon through its sigil, with a countdown and private instance.

**Deliverables**

- game/Instances/InstanceMgr: sigil gather timer, party snapshot, instance id allocation, owner loss handling
- Zone timers and counters for dungeon mechanics
- world.instance_template (zone, max players, requirement)

**Client messages:** MSG_LEAVESIGILTIMERWAITING, MSG_DISPOSABLEDUNGEONNOOWNER, MSG_ADDZONETIMER, MSG_UPDATEZONETIMER, MSG_REMOVEZONETIMER, MSG_UPDATEZONECOUNTER, MSG_TELEPORT_TO_GAME_ZONE, MSG_ZONEEVENTTIMEREXPIRED, MSG_POSTZONEEVENTFROMCLIENT

**Data sources**

- ZoneData sigil/trigger objects (SigilZoneInfo)
- ObjectData/OneShotDungeons, DisposableDungeon, SkeletonKeys

**Database tables**

- world.instance_template
- characters.instance_lockout

**Acceptance**

- [ ] Unit: members who leave the circle before countdown ends are excluded; two groups get different instance ids
- [ ] Client: standing on a dungeon sigil shows the countdown, the group teleports into an instance other players cannot see, and a skeleton-key door consumes its key

## 14.02 Tutorial manager (EXT-24)

**Goal:** Server tutorial commands and tip persistence.

**Size:** M. **Depends on:** 10.17, 3.16, 8.10

**Client messages:** MSG_TUTORIALS, MSG_SERVERTUTORIALCOMMAND, MSG_CLIENTTUTORIALEVENT, MSG_TUTORIALEVENT, MSG_DISMISSTUTORIALTIP, MSG_SHOWGUI

**Acceptance**

- [ ] Unknown tutorial action rejected
- [ ] Real client: dismissed tips stay dismissed after relog

### Detailed spec from EXT-24: Tutorial (intro and later tutorials)

A new wizard plays the intro tutorial and later tutorials, driven by the client Lua with server commands.

**Deliverables**

- game/Tutorial/TutorialMgr handling server tutorial commands (add/remove quest, complete goal, post event, refill health/mana, grant starter kit)
- Tutorial tip dismissal persistence

**Client messages:** MSG_TUTORIALS, MSG_SERVERTUTORIALCOMMAND, MSG_CLIENTTUTORIALEVENT, MSG_TUTORIALEVENT, MSG_DISMISSTUTORIALTIP, MSG_SHOWGUI

**Data sources**

- Root.wad Tutorials/*.xml (11)
- Scripts/Tutorials/*.lua (client-run)
- TutorialTips/*.ttip (564)

**Database tables**

- characters.character_tutorial
- characters.character_tutorial_tip

**Acceptance**

- [ ] Unit: each tutorial command action maps to its effect and rejects unknown actions
- [ ] Client: a freshly created wizard spawns into the intro, follows the prompts through the tutorial duel, gets the starter wand/deck, and arrives in Wizard City; dismissed tips stay dismissed after relog

**Risks**

- New characters hit this first, so it may need to move earlier into the QST/LOG roadmap even though it sits here

## 14.03 Scripted tutorial duel (CMB-28)

**Goal:** Tutorial mode, no timer.

**Size:** M. **Depends on:** 14.02, 11.09

**Client messages:** MSG_TUTORIALEVENT, MSG_COMBATHAND, MSG_SETPLANNINGPHASETIMER

**Acceptance**

- [ ] Real client: new character's first duel shows tips in order, hidden countdown, scripted enemy moves, scripted victory; starter kit granted and arrives in Wizard City

### Detailed spec from CMB-28: Scripted tutorial duel

The new-player tutorial duel plays its scripted rounds with tutorial mode and no timer.

**Deliverables**

- Duel m_tutorialMode and m_disableTimer set by a zone script (scripts/WizardCity/) through a CombatScript hook
- Scripted creature moves and hand grants per round, MSG_TUTORIALEVENT triggers

**Client messages:** MSG_TUTORIALEVENT, MSG_COMBATHAND, MSG_SETPLANNINGPHASETIMER

**Data sources**

- Tutorial zone data and quest data (QST)

**Acceptance**

- [ ] Real client: a new character's first duel shows the tutorial tips in order, the countdown is hidden, and the scripted enemy moves happen on the right rounds until the scripted victory

**Risks**

- Tutorial round goals are server-side and must be derived by observation

## 14.04 PvP scalars and restrictions (CMB-27 part 1)

**Goal:** PvP sigils and PvE-only flags.

**Size:** M. **Depends on:** 11.06, 11.18

**Client messages:** MSG_ALLOWLEAVEPVP, MSG_UPDATECOMBATPARTICIPANT, MSG_COMBATMATCHRESULT

**Acceptance**

- [ ] Real client: PvE-only card greyed out in a practice duel

### Detailed spec from CMB-27: Player-vs-player duel rules

Practice and ranked PvP duels use PvP scalars, alternating turns and PvP-only restrictions.

**Deliverables**

- PvPCombatSigilTemplate usage, Duel m_bPVP with PvP scalar and limit fields
- AlternateTurnsCombatRule (MSG_SHOWCOMBATUI AltTurn/AltTurnTeam), Duel m_executionOrder kDEO_Alternating
- PvP/PvE-only spell flags (SpellTemplate m_PvP/m_PvE, DuelModifier ignore flags), MSG_ALLOWLEAVEPVP, MSG_UPDATECOMBATPARTICIPANT HidePVPEnemyChat
- Match-making queues stay in the PvP domain (MSG_PVP* in WizardMessages.xml)

**Client messages:** MSG_SHOWCOMBATUI, MSG_ALLOWLEAVEPVP, MSG_UPDATECOMBATPARTICIPANT, MSG_COMBATMATCHRESULT

**Data sources**

- Sigils/*PvPSigil*.xml
- PvP/

**Acceptance**

- [ ] Sim test: in alternating mode only the active team's moves are accepted each turn
- [ ] Real client (2 clients): a practice PvP duel at an arena sigil alternates turns with the turn indicator switching, and a PvE-only card is greyed out

**Risks**

- Ranked rating and rewards belong to another domain; the boundary needs agreement

## 14.05 Alternating turns (CMB-27 part 2)

**Goal:** kDEO_Alternating.

**Size:** M. **Depends on:** 14.04

**Client messages:** MSG_SHOWCOMBATUI

**Acceptance**

- [ ] Only the active team's moves accepted
- [ ] Real client: turn indicator alternates

### Detailed spec from CMB-27: Player-vs-player duel rules

Practice and ranked PvP duels use PvP scalars, alternating turns and PvP-only restrictions.

**Deliverables**

- PvPCombatSigilTemplate usage, Duel m_bPVP with PvP scalar and limit fields
- AlternateTurnsCombatRule (MSG_SHOWCOMBATUI AltTurn/AltTurnTeam), Duel m_executionOrder kDEO_Alternating
- PvP/PvE-only spell flags (SpellTemplate m_PvP/m_PvE, DuelModifier ignore flags), MSG_ALLOWLEAVEPVP, MSG_UPDATECOMBATPARTICIPANT HidePVPEnemyChat
- Match-making queues stay in the PvP domain (MSG_PVP* in WizardMessages.xml)

**Client messages:** MSG_SHOWCOMBATUI, MSG_ALLOWLEAVEPVP, MSG_UPDATECOMBATPARTICIPANT, MSG_COMBATMATCHRESULT

**Data sources**

- Sigils/*PvPSigil*.xml
- PvP/

**Acceptance**

- [ ] Sim test: in alternating mode only the active team's moves are accepted each turn
- [ ] Real client (2 clients): a practice PvP duel at an arena sigil alternates turns with the turn indicator switching, and a PvE-only card is greyed out

**Risks**

- Ranked rating and rewards belong to another domain; the boundary needs agreement

## 14.06 PvP queue and brackets (EXT-34 part 1)

**Goal:** Matchmaking.

**Size:** M. **Depends on:** 14.05, 14.01

**Client messages:** MSG_REQUESTPVPKIOSK, MSG_PREPVPKIOSK, MSG_REQUESTPVPACTOR, MSG_PVPMATCHREQUEST, MSG_PVPINTENT, MSG_PVPCONFIRM, MSG_PVPQUEUE, MSG_EXPANDPVPSEARCH, MSG_ARENAERROR, MSG_MATCHMAKERUPDATE

**Acceptance**

- [ ] Two same-bracket players matched

### Detailed spec from EXT-34: PvP kiosk and ranked 1v1 matchmaking

Players queue at the arena, get matched, and fight a ranked duel that records a result.

**Deliverables**

- game/PvP/MatchmakingMgr (queue, bracket by level, search expansion, ready check)
- Match lifecycle into a PvP arena instance and result recording

**Client messages:** MSG_REQUESTPVPKIOSK, MSG_PREPVPKIOSK, MSG_REQUESTPVPACTOR, MSG_PVPMATCHREQUEST, MSG_PVPINTENT, MSG_PVPCONFIRM, MSG_PVPQUEUE, MSG_EXPANDPVPSEARCH, MSG_ARENAERROR, MSG_MATCHMAKERUPDATE, MSG_MATCHINVITE, MSG_PLAYERREADYACK, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_ALLOWLEAVEPVP, MSG_SETDUELTIMER

**Data sources**

- Root.wad PvP/Leagues/*.xml, PvP/Seasons/*.xml
- Sigils PvPCombatSigilTemplate
- DuelModifiers/ (37)

**Database tables**

- characters.pvp_match

**Acceptance**

- [ ] Unit: two queued players in the same bracket are matched; a declined ready check requeues the other
- [ ] Client: two wizards click Ranked 1v1 at the arena kiosk, both get the match popup, accept, are teleported to the arena sigil and duel; the loser/winner result screen shows

## 14.07 Ready check and arena match (EXT-34 part 2)

**Goal:** Invite to arena duel.

**Size:** M. **Depends on:** 14.06

**Client messages:** MSG_MATCHINVITE, MSG_PLAYERREADYACK, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_SETDUELTIMER

**Acceptance**

- [ ] Declined ready check requeues the other
- [ ] Real client: both accept, teleport to arena, duel, result screen

### Detailed spec from EXT-34: PvP kiosk and ranked 1v1 matchmaking

Players queue at the arena, get matched, and fight a ranked duel that records a result.

**Deliverables**

- game/PvP/MatchmakingMgr (queue, bracket by level, search expansion, ready check)
- Match lifecycle into a PvP arena instance and result recording

**Client messages:** MSG_REQUESTPVPKIOSK, MSG_PREPVPKIOSK, MSG_REQUESTPVPACTOR, MSG_PVPMATCHREQUEST, MSG_PVPINTENT, MSG_PVPCONFIRM, MSG_PVPQUEUE, MSG_EXPANDPVPSEARCH, MSG_ARENAERROR, MSG_MATCHMAKERUPDATE, MSG_MATCHINVITE, MSG_PLAYERREADYACK, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_ALLOWLEAVEPVP, MSG_SETDUELTIMER

**Data sources**

- Root.wad PvP/Leagues/*.xml, PvP/Seasons/*.xml
- Sigils PvPCombatSigilTemplate
- DuelModifiers/ (37)

**Database tables**

- characters.pvp_match

**Acceptance**

- [ ] Unit: two queued players in the same bracket are matched; a declined ready check requeues the other
- [ ] Client: two wizards click Ranked 1v1 at the arena kiosk, both get the match popup, accept, are teleported to the arena sigil and duel; the loser/winner result screen shows

## 14.08 Ratings, tickets, leaderboards, daily PvP (EXT-35)

**Goal:** Rank progression.

**Size:** M. **Depends on:** 14.07, 10.10

**Client messages:** MSG_MATCHAWARD, MSG_UPDATEARENAPOINTS, MSG_PVPUPDATEINFO, MSG_PVPUPDATEREQUEST, MSG_UPDATESHADOWPIPRATING, MSG_GETLADDER, MSG_LADDER, MSG_GET_RANKINGS, MSG_RANKING, MSG_PRELEADERBOARD, MSG_LEADERBOARDREQUEST, MSG_LEADERBOARDRESPONSE, MSG_LEADERBOARDFRIENDREQUEST, MSG_DailyPvPUpdate, MSG_DAILYPVPOPEN

**Acceptance**

- [ ] Symmetric bounded rating delta
- [ ] Real client: rating, rank, tickets, leaderboard update

### Detailed spec from EXT-35: PvP ratings, arena tickets, rank, leaderboards, daily PvP

Match results update rating, rank and arena tickets, and players can view leaderboards.

**Deliverables**

- Rating calculation from PvPRatingsConfig/AdvPvPRankConfig
- Arena ticket currency and arena vendor shop type (EXT-1 currency_type=PvP)
- Ladder/leaderboard queries; daily PvP

**Client messages:** MSG_MATCHAWARD, MSG_UPDATEARENAPOINTS, MSG_PVPUPDATEINFO, MSG_PVPUPDATEREQUEST, MSG_UPDATESHADOWPIPRATING, MSG_GETLADDER, MSG_LADDER, MSG_GET_RANKINGS, MSG_RANKING, MSG_PRELEADERBOARD, MSG_LEADERBOARDREQUEST, MSG_LEADERBOARDRESPONSE, MSG_LEADERBOARDFRIENDREQUEST, MSG_DailyPvPUpdate, MSG_DAILYPVPOPEN

**Data sources**

- Root.wad PvPRatingsConfig.xml, PvP/AdvPvPRankConfig.xml, DailyQuestData/DailyPvPData.xml

**Database tables**

- characters.pvp_rating
- characters.character_currency (arena_tickets)

**Acceptance**

- [ ] Unit: rating delta symmetric for equal ratings and bounded; rank thresholds match config
- [ ] Client: after a ranked win the character's rating and rank title change on the PvP page, arena tickets increase, and the leaderboard lists the player

## 14.09 Tournament scheduler and brackets (EXT-36 part 1)

**Goal:** Scheduled brackets.

**Size:** M. **Depends on:** 14.08

**Client messages:** MSG_TOURNAMENTUPDATE, MSG_BRACKETREPORT, MSG_SUBOPTIMAL_BRACKET_RESPONSE

**Acceptance**

- [ ] N players give correct rounds and byes

### Detailed spec from EXT-36: PvP tournaments

Scheduled tournaments with brackets, credits and rewards run end to end.

**Deliverables**

- Tournament scheduler from Tournaments/*.xml and PvPTournamentConfig.xml
- Bracket pairing, rounds, credit consumption, rewards

**Client messages:** MSG_TOURNAMENTUPDATE, MSG_PVPCONFIRMTOURNEY, MSG_PVPCONSUMEPVPTOURNEYCURRENCY, MSG_FREETOURNEYCREDITINFO, MSG_BRACKETREPORT, MSG_NEWTOURNEYREWARDS, MSG_SUBOPTIMAL_BRACKET_RESPONSE

**Data sources**

- Root.wad Tournaments/PvP*.xml, PvPTournamentConfig.xml, TourneyNames.xml

**Database tables**

- characters.tournament
- characters.tournament_entry

**Acceptance**

- [ ] Unit: bracket of N players produces correct rounds and byes
- [ ] Client: the kiosk lists an upcoming tournament, joining consumes a credit, matches pop in sequence, and the final standings grant rewards

**Risks**

- Wire name MSG_PVMSG_PVPREGISTERFAILEDPISSUETOURNEYCREDIT is malformed in WizardMessages.xml and its intent is unverified

## 14.10 Tournament credits and rewards (EXT-36 part 2)

**Goal:** Join and reward.

**Size:** M. **Depends on:** 14.09

**Client messages:** MSG_PVPCONFIRMTOURNEY, MSG_PVPCONSUMEPVPTOURNEYCURRENCY, MSG_FREETOURNEYCREDITINFO, MSG_NEWTOURNEYREWARDS

**Acceptance**

- [ ] Real client: joining consumes credit; final standings grant rewards

### Detailed spec from EXT-36: PvP tournaments

Scheduled tournaments with brackets, credits and rewards run end to end.

**Deliverables**

- Tournament scheduler from Tournaments/*.xml and PvPTournamentConfig.xml
- Bracket pairing, rounds, credit consumption, rewards

**Client messages:** MSG_TOURNAMENTUPDATE, MSG_PVPCONFIRMTOURNEY, MSG_PVPCONSUMEPVPTOURNEYCURRENCY, MSG_FREETOURNEYCREDITINFO, MSG_BRACKETREPORT, MSG_NEWTOURNEYREWARDS, MSG_SUBOPTIMAL_BRACKET_RESPONSE

**Data sources**

- Root.wad Tournaments/PvP*.xml, PvPTournamentConfig.xml, TourneyNames.xml

**Database tables**

- characters.tournament
- characters.tournament_entry

**Acceptance**

- [ ] Unit: bracket of N players produces correct rounds and byes
- [ ] Client: the kiosk lists an upcoming tournament, joining consumes a credit, matches pop in sequence, and the final standings grant rewards

**Risks**

- Wire name MSG_PVMSG_PVPREGISTERFAILEDPISSUETOURNEYCREDIT is malformed in WizardMessages.xml and its intent is unverified

## 14.11 Derby race simulation (EXT-37 part 1)

**Goal:** Lanes, speed, morale.

**Size:** M. **Depends on:** 13.13, 14.07

**Client messages:** MSG_PETDERBYSTART, MSG_DERBYSYNC, MSG_DERBYLOCATION, MSG_PETDERBYSWITCHLANE, MSG_PETDERBYSWITCHLANEFAIL, MSG_PETDERBYJUMPDUCK, MSG_PETDERBYSPEED, MSG_PETDERBYMORALE

**Acceptance**

- [ ] Fixed seed and inputs give same finish order

### Detailed spec from EXT-37: Pet derby: matchmaking and race simulation

Pets race on derby tracks with lanes, jumps, speed and morale, and results are recorded.

**Deliverables**

- game/Pets/Derby race simulation (server-authoritative positions, lap tracking, photo finish)
- Derby queue via tournament/match configs

**Client messages:** MSG_PETDERBYSTART, MSG_DERBYSYNC, MSG_DERBYLOCATION, MSG_PETDERBYSWITCHLANE, MSG_PETDERBYSWITCHLANEFAIL, MSG_PETDERBYJUMPDUCK, MSG_PETDERBYSPEED, MSG_PETDERBYMORALE, MSG_PETDERBYLAP, MSG_PETDERBYPHOTOFINISH, MSG_PETDERBYPLAYERLEFT, MSG_PETGAMEDERBYRESULTS, MSG_DERBYPETENERGYINFO

**Data sources**

- Root.wad PetDerbyTracks.xml, Tournaments/Pet*.xml, Matches/Pet*.xml

**Database tables**

- characters.pet_derby_result

**Acceptance**

- [ ] Unit: simulation with fixed seed and inputs yields the same finishing order
- [ ] Client: four players queue for a derby, pets appear in lanes, switching lanes and jumping obstacles changes speed, and the results screen shows placements

## 14.12 Derby laps and results (EXT-37 part 2)

**Goal:** Finish and record.

**Size:** M. **Depends on:** 14.11

**Client messages:** MSG_PETDERBYLAP, MSG_PETDERBYPHOTOFINISH, MSG_PETDERBYPLAYERLEFT, MSG_PETGAMEDERBYRESULTS, MSG_DERBYPETENERGYINFO

**Acceptance**

- [ ] Real client: four players race; results screen placements

### Detailed spec from EXT-37: Pet derby: matchmaking and race simulation

Pets race on derby tracks with lanes, jumps, speed and morale, and results are recorded.

**Deliverables**

- game/Pets/Derby race simulation (server-authoritative positions, lap tracking, photo finish)
- Derby queue via tournament/match configs

**Client messages:** MSG_PETDERBYSTART, MSG_DERBYSYNC, MSG_DERBYLOCATION, MSG_PETDERBYSWITCHLANE, MSG_PETDERBYSWITCHLANEFAIL, MSG_PETDERBYJUMPDUCK, MSG_PETDERBYSPEED, MSG_PETDERBYMORALE, MSG_PETDERBYLAP, MSG_PETDERBYPHOTOFINISH, MSG_PETDERBYPLAYERLEFT, MSG_PETGAMEDERBYRESULTS, MSG_DERBYPETENERGYINFO

**Data sources**

- Root.wad PetDerbyTracks.xml, Tournaments/Pet*.xml, Matches/Pet*.xml

**Database tables**

- characters.pet_derby_result

**Acceptance**

- [ ] Unit: simulation with fixed seed and inputs yields the same finishing order
- [ ] Client: four players queue for a derby, pets appear in lanes, switching lanes and jumping obstacles changes speed, and the results screen shows placements

## 14.13 Derby abilities and cheers (EXT-38)

**Goal:** Talents and track effects.

**Size:** M. **Depends on:** 14.12

**Client messages:** MSG_PETDERBYUSETALENT, MSG_PETDERBYCHEER, MSG_PETDERBYSLOW, MSG_PETDERBYBUFF, MSG_PETDERBYMODIFYSTAT, MSG_DERBYEFFECTAPPLY, MSG_DERBYEFFECTREMOVE, MSG_DERBYSTATMOD, MSG_DERBYEFFECTSLISTUPDATE, MSG_CHEERCOSTMAPUPDATE

**Acceptance**

- [ ] Real client: talent icon and speed change; cheer costs shown morale

### Detailed spec from EXT-38: Pet derby: abilities, cheers and effects

Derby talents, cheers, buffs and track effects apply during races.

**Deliverables**

- Derby effect system (apply/remove/stat mod/immunity) and cheer cost map

**Client messages:** MSG_PETDERBYUSETALENT, MSG_PETDERBYCHEER, MSG_PETDERBYSLOW, MSG_PETDERBYBUFF, MSG_PETDERBYMODIFYSTAT, MSG_DERBYEFFECTAPPLY, MSG_DERBYEFFECTREMOVE, MSG_DERBYSTATMOD, MSG_DERBYEFFECTSLISTUPDATE, MSG_CHEERCOSTMAPUPDATE

**Data sources**

- Derby* effect classes (162 types in the dump) and derby talents in TalentData

**Acceptance**

- [ ] Client: using a derby talent applies its effect icon and changes speed for the target; cheering costs the shown morale

## 14.14 Battlegrounds RE spike (EXT-39 part 1)

**Goal:** Find where BG state travels (no dedicated messages).

**Size:** M. **Depends on:** 14.08, 11.19

**Acceptance**

- [ ] doc/ records the carrying behaviors/blobs and a split plan

### Detailed spec from EXT-39: Battlegrounds (coarse)

Team battleground matches run with polymorphs, points of interest and team scoring.

**Deliverables**

- game/PvP/BattlegroundMgr from Battlegrounds/*.xml (BattlegroundTemplate)
- Team data, POI capture, BG sigil timers, polymorph assignment, player stats

**Client messages:** MSG_MATCHMAKERUPDATE, MSG_MATCHINVITE, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_UPDATEZONETIMER

**Data sources**

- Root.wad Battlegrounds/ (84), BattlegroundPolymorphs.xml, Sigils/BattlegroundSigil.xml, Sigils/BGPolymorphSigil.xml
- ObjectData/Battlegrounds (930)

**Database tables**

- characters.battleground_stats

**Acceptance**

- [ ] Client: players join a battleground, are polymorphed at the BG sigil, capturing a POI updates the team score UI, and the match ends with rewards

**Risks**

- No dedicated battleground messages exist in the XML; state is probably carried in ObjectProperty behavior blobs, which is unverified and needs RE
- Must be split into several milestones once RE is done

## 14.15 Battleground sigil, polymorph, POI scoring (EXT-39 part 2)

**Goal:** Team BG match.

**Size:** L. **Depends on:** 14.14

**Client messages:** MSG_MATCHMAKERUPDATE, MSG_MATCHINVITE, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_UPDATEZONETIMER

**Acceptance**

- [ ] Real client: polymorphed at BG sigil; POI capture updates score; match ends with rewards

### Detailed spec from EXT-39: Battlegrounds (coarse)

Team battleground matches run with polymorphs, points of interest and team scoring.

**Deliverables**

- game/PvP/BattlegroundMgr from Battlegrounds/*.xml (BattlegroundTemplate)
- Team data, POI capture, BG sigil timers, polymorph assignment, player stats

**Client messages:** MSG_MATCHMAKERUPDATE, MSG_MATCHINVITE, MSG_MATCHREADY, MSG_MATCHRESULT, MSG_UPDATEZONETIMER

**Data sources**

- Root.wad Battlegrounds/ (84), BattlegroundPolymorphs.xml, Sigils/BattlegroundSigil.xml, Sigils/BGPolymorphSigil.xml
- ObjectData/Battlegrounds (930)

**Database tables**

- characters.battleground_stats

**Acceptance**

- [ ] Client: players join a battleground, are polymorphed at the BG sigil, capturing a POI updates the team score UI, and the match ends with rewards

**Risks**

- No dedicated battleground messages exist in the XML; state is probably carried in ObjectProperty behavior blobs, which is unverified and needs RE
- Must be split into several milestones once RE is done

## 14.16 Daily assignments (EXT-43)

**Goal:** Daily rolls and rewards.

**Size:** M. **Depends on:** 10.08

**Client messages:** MSG_DailyQuestUpdate, MSG_DAILYQUESTOPEN, MSG_DAILYQUESTCOMPLETED, MSG_DAILYQUESTEXPLORE, MSG_DAILYQUESTCSRDATA

**Acceptance**

- [ ] Roll stable within a day, changes after reset
- [ ] Real client: task list; completion grants reward

### Detailed spec from EXT-43: Daily assignments

Daily quests (assignments) roll, track, and grant daily rewards.

**Deliverables**

- Daily roll at reset time, progress tracking, reward tiers

**Client messages:** MSG_DailyQuestUpdate, MSG_DAILYQUESTOPEN, MSG_DAILYQUESTCOMPLETED, MSG_DAILYQUESTEXPLORE, MSG_DAILYQUESTCSRDATA

**Data sources**

- Root.wad DailyQuestData/DailyQuestData.xml, DailyQuestRewards.xml

**Database tables**

- characters.character_daily_quest

**Acceptance**

- [ ] Unit: roll is stable within a day and changes after reset
- [ ] Client: the daily assignment window lists today's tasks; completing one marks it done and grants its reward

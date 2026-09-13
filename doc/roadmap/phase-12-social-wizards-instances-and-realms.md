<!-- Project Ambrose by Imjustchico: Roadmap phase 12, Social wizards, instances and realms. -->

# Phase 12: Social wizards, instances and realms

**Done when:** Players friend, whisper, group, trade, bank and dye. They wear titles, save outfits, pick instances and switch realms from the in-game picker.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 12.01 | Friends (WIZ-17 part 1) | M | 6.03, 5.05 |
| 12.02 | Ignore list (WIZ-17 part 2) | S | 12.01 |
| 12.03 | Whispers and inspect (WIZ-18) | M | 12.02, 11.03 |
| 12.04 | Groups: invite, join, leave (WIZ-19 part 1) | M | 12.01, 8.02 |
| 12.05 | Groups: updates, member zones, channel chat, leader (WIZ-19 part 2) | M | 12.04 |
| 12.06 | Privacy, friendly player, teleport to friend (WIZ-20) | M | 12.01, 6.07 |
| 12.07 | Chat moderation (WIZ-21) | M | 6.04 |
| 12.08 | Badges and titles (WIZ-22) | M | 6.04, 10.01 |
| 12.09 | Bank and shared bank (WIZ-23) | M | 8.09 |
| 12.10 | Player trade (WIZ-25) | M | 8.09, 12.06 |
| 12.11 | Dye shop (EXT-3 + WIZ-26 dye) | S | 10.10, 8.10 |
| 12.12 | Stitching (EXT-4 + WIZ-26 stitch) | S | 12.11 |
| 12.13 | Equipment sets (WIZ-26 part) | M | 8.10 |
| 12.14 | Custom emotes and pet rename (WIZ-26 part) | M | 6.03, 5.05 |
| 12.15 | Crowns balance and crown services (EXT-7) | S | 10.10, 2.13 |
| 12.16 | Bazaar (EXT-5) | M | 10.10, 12.09 |
| 12.17 | Public instances by capacity (WLD-22 part 1) | M | 6.14 |
| 12.18 | Private dungeon instances (WLD-22 part 2) | M | 12.17, 12.05 |
| 12.19 | Realm and instance picker (LOG-16 part 1 + WLD-22 part 3) | M | 12.17, 4.03 |
| 12.20 | Realm transfer (LOG-16 part 2) | M | 12.19, 6.07 |
| 12.21 | Admission queue (LOG-15) | M | 4.05, 2.15 |
| 12.22 | Same-connection zone transfer spike (WLD-21) | S | 6.07 |
| 12.23 | MoveBehavior/Physics observation and speed validation (WLD-23) | S | 5.03, 11.03 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 12.03 (whispers) depends on 11.03 (set bonuses) only so the inspect sheet shows stats, which blocks basic whispers behind the whole combat-stats phase. Split inspect out.
- **Ordering.** 12.23 speed validation acceptance says 'mounts never corrected', but mounts (MSG_RIDEOBJECT, MSG_SETSTOREDMOUNT, MSG_RIDABLEUPDATE in WizardMessages2) are not implemented in any earlier milestone.
- **Oversized.** 12.23 MoveBehavior/Physics observation plus speed validation across services 15/16 (S). Under-sized.
- **Correction.** 12.19/12.20 group MSG_CURRENTREALM with GAME realm messages, but it lives in GameMessages2.xml (svc 55). 12.03's MSG_REQUESTRADIALFRIENDQUICKCHATEXT is in WizardMessages2 (53). The names are valid, but these service placements are unacknowledged.

## 12.01 Friends (WIZ-17 part 1)

**Goal:** Add, accept, deny, remove, presence.

**Size:** M. **Depends on:** 6.03, 5.05

**Client messages:** MSG_BUDDYREQUESTLIST, MSG_BUDDYENTRY, MSG_BUDDYLISTCOMPLETE, MSG_BUDDYREQUESTADD, MSG_BUDDYREQUESTACCEPT, MSG_BUDDYREQUESTDENY, MSG_BUDDYREQUESTDROP, MSG_BUDDYDROP, MSG_BUDDYSTATUSUPDATE, MSG_BESTFRIEND, MSG_REQUESTMAXFRIENDS

**Acceptance**

- [ ] Accepting an unsent request fails with CHATERROR
- [ ] Real client: add/accept shows both online with zone; logout shows offline

### Detailed spec from WIZ-17: Friends and ignore lists

Players can add, accept, deny and remove friends and ignored players, see online status and location, and the lists survive relog.

**Deliverables**

- data/sql/updates/db_characters: character_friend (owner, friend, best_friend_symbol, date), character_ignore, character_friend_request
- src/server/game/Social/SocialMgr (sSocialMgr): realm-wide online presence (single gameserver process now, designed so a chat server can be split out later), friend cap from config
- GAME handlers: BUDDYREQUESTLIST -> BUDDYENTRY* + BUDDYLISTCOMPLETE; BUDDYREQUESTADD (forwarded to the target), BUDDYREQUESTACCEPT, BUDDYREQUESTDENY, BUDDYREQUESTDROP -> BUDDYDROP both sides; BUDDYSTATUSUPDATE on login, logout and zone change; BESTFRIEND; IGNOREADD, IGNOREDROP, IGNORELIST (ListData blob of IgnoreEntryData); CHATERROR; REQUESTMAXFRIENDS
- src/test/server/game/SocialMgrTest.cpp

**Client messages:** GAME MSG_BUDDYREQUESTLIST, GAME MSG_BUDDYENTRY, GAME MSG_BUDDYLISTCOMPLETE, GAME MSG_BUDDYREQUESTADD, GAME MSG_BUDDYREQUESTACCEPT, GAME MSG_BUDDYREQUESTDENY, GAME MSG_BUDDYREQUESTDROP, GAME MSG_BUDDYDROP, GAME MSG_BUDDYSTATUSUPDATE, GAME MSG_BESTFRIEND, GAME MSG_IGNOREADD, GAME MSG_IGNOREDROP, GAME MSG_IGNORELIST, GAME MSG_CHATERROR, GAME MSG_REQUESTMAXFRIENDS

**Database tables**

- characters.character_friend
- characters.character_ignore
- characters.character_friend_request

**Acceptance**

- [ ] Unit test: accepting a request that was never sent fails with CHATERROR
- [ ] Unit test: ignoring a player removes them from friends and suppresses their radial chat to the owner
- [ ] Two real clients: A clicks B and chooses Add Friend. B gets the friend request popup and accepts. Both friends lists show each other online with zone name. B logs out and A's list shows B offline within one status update. A ignores B and stops seeing B's chat bubbles.

**Risks**

- The Status, FriendInfo and Permissions bit meanings in BUDDYENTRY are unverified.
- True-friend chat codes (REQUESTCHATCODE/SENDCHATCODE/USECHATCODE) are left out to keep this milestone small.

## 12.02 Ignore list (WIZ-17 part 2)

**Goal:** Ignore suppresses chat.

**Size:** S. **Depends on:** 12.01

**Client messages:** MSG_IGNOREADD, MSG_IGNOREDROP, MSG_IGNORELIST, MSG_CHATERROR

**Acceptance**

- [ ] Ignoring removes friendship and suppresses radial chat
- [ ] Real client: A stops seeing B's bubbles

### Detailed spec from WIZ-17: Friends and ignore lists

Players can add, accept, deny and remove friends and ignored players, see online status and location, and the lists survive relog.

**Deliverables**

- data/sql/updates/db_characters: character_friend (owner, friend, best_friend_symbol, date), character_ignore, character_friend_request
- src/server/game/Social/SocialMgr (sSocialMgr): realm-wide online presence (single gameserver process now, designed so a chat server can be split out later), friend cap from config
- GAME handlers: BUDDYREQUESTLIST -> BUDDYENTRY* + BUDDYLISTCOMPLETE; BUDDYREQUESTADD (forwarded to the target), BUDDYREQUESTACCEPT, BUDDYREQUESTDENY, BUDDYREQUESTDROP -> BUDDYDROP both sides; BUDDYSTATUSUPDATE on login, logout and zone change; BESTFRIEND; IGNOREADD, IGNOREDROP, IGNORELIST (ListData blob of IgnoreEntryData); CHATERROR; REQUESTMAXFRIENDS
- src/test/server/game/SocialMgrTest.cpp

**Client messages:** GAME MSG_BUDDYREQUESTLIST, GAME MSG_BUDDYENTRY, GAME MSG_BUDDYLISTCOMPLETE, GAME MSG_BUDDYREQUESTADD, GAME MSG_BUDDYREQUESTACCEPT, GAME MSG_BUDDYREQUESTDENY, GAME MSG_BUDDYREQUESTDROP, GAME MSG_BUDDYDROP, GAME MSG_BUDDYSTATUSUPDATE, GAME MSG_BESTFRIEND, GAME MSG_IGNOREADD, GAME MSG_IGNOREDROP, GAME MSG_IGNORELIST, GAME MSG_CHATERROR, GAME MSG_REQUESTMAXFRIENDS

**Database tables**

- characters.character_friend
- characters.character_ignore
- characters.character_friend_request

**Acceptance**

- [ ] Unit test: accepting a request that was never sent fails with CHATERROR
- [ ] Unit test: ignoring a player removes them from friends and suppresses their radial chat to the owner
- [ ] Two real clients: A clicks B and chooses Add Friend. B gets the friend request popup and accepts. Both friends lists show each other online with zone name. B logs out and A's list shows B offline within one status update. A ignores B and stops seeing B's chat bubbles.

**Risks**

- The Status, FriendInfo and Permissions bit meanings in BUDDYENTRY are unverified.
- True-friend chat codes (REQUESTCHATCODE/SENDCHATCODE/USECHATCODE) are left out to keep this milestone small.

## 12.03 Whispers and inspect (WIZ-18)

**Goal:** Private messages and character sheet.

**Size:** M. **Depends on:** 12.02, 11.03

**Client messages:** MSG_REQUESTDIRECTEDCHAT, MSG_DIRECTEDCHAT, MSG_DIRECTEDCHATFAIL, MSG_REQUESTDIRECTEDQUICKCHAT, MSG_DIRECTEDQUICKCHAT, MSG_REQUESTDIRECTEDQUICKCHATEXT, MSG_DIRECTEDQUICKCHATEXT, MSG_BUDDYSTATS, MSG_REQUESTRADIALFRIENDQUICKCHATEXT

**Acceptance**

- [ ] Whisper to an ignoring player gives DIRECTEDCHATFAIL
- [ ] Real client: cross-zone whisper; portrait opens sheet with level, school, gear

### Detailed spec from WIZ-18: Whispers and player inspect

Players can send private messages to friends and open another player's character sheet (stats, gear, level).

**Deliverables**

- GAME REQUESTDIRECTEDCHAT -> DIRECTEDCHAT to the target (blocked by ignore, offline or permissions -> DIRECTEDCHATFAIL); REQUESTDIRECTEDQUICKCHAT/EXT -> DIRECTEDQUICKCHAT/EXT; WIZARD2 REQUESTRADIALFRIENDQUICKCHATEXT
- GAME BUDDYSTATS request/response with StatBlock (WizGameStats), CharBlock, EquipBlock (public items), EffectBlock, PetStatBlock, CRC fields so the client can skip unchanged blocks
- Reuse of SocialMgr presence for cross-zone delivery

**Client messages:** GAME MSG_REQUESTDIRECTEDCHAT, GAME MSG_DIRECTEDCHAT, GAME MSG_DIRECTEDCHATFAIL, GAME MSG_REQUESTDIRECTEDQUICKCHAT, GAME MSG_DIRECTEDQUICKCHAT, GAME MSG_REQUESTDIRECTEDQUICKCHATEXT, GAME MSG_DIRECTEDQUICKCHATEXT, GAME MSG_BUDDYSTATS, WIZARD2 MSG_REQUESTRADIALFRIENDQUICKCHATEXT

**Acceptance**

- [ ] Unit test: a whisper to an ignoring player yields DIRECTEDCHATFAIL and no delivery
- [ ] Unit test: BUDDYSTATS blocks round-trip through the ObjectProperty codec and CRCs match
- [ ] Two real clients in different zones: A whispers B from the friends list and B sees the message in the chat log marked as private. A clicks B's portrait and opens the character sheet, which shows B's level, school and equipped items.

**Risks**

- The CRC algorithm for BUDDYSTATS blocks is unverified.
- The reference only reads BuddyID from BUDDYSTATS (for GM selection). The full response layout is not yet observed.

## 12.04 Groups: invite, join, leave (WIZ-19 part 1)

**Goal:** Parties of four.

**Size:** M. **Depends on:** 12.01, 8.02

**Client messages:** MSG_PARTYREQUESTINVITE, MSG_PARTYREQUESTJOIN, MSG_PARTYREQUESTACCEPT, MSG_PARTYREQUESTDECLINE, MSG_PARTYJOINNOTIFICATION, MSG_PARTYLEAVE, MSG_PARTYLEAVENOTIFICATION, MSG_PARTYDISBAND, MSG_PARTYREQUESTTIMEOUT, MSG_PARTYJOINFAILED, MSG_PARTYREQUESTRESPONSE

**Acceptance**

- [ ] 5th invite gives PARTYJOINFAILED; leader leaving promotes next
- [ ] Real client: invite popup, portraits, leave clears

### Detailed spec from WIZ-19: Groups (parties)

Players can invite each other into a group of up to four, see member info and leave or disband, and group state follows zone changes.

**Deliverables**

- src/server/game/Groups/GroupMgr (sGroupMgr): party id, leader, members, invite timeout (config)
- GAME PARTYREQUESTINVITE -> PARTYREQUESTJOIN to the target; PARTYREQUESTACCEPT/DECLINE; PARTYJOINNOTIFICATION, PARTYUPDATE (school, level, zone, leader), PARTYLEAVE -> PARTYLEAVENOTIFICATION, PARTYDISBAND, PARTYREQUESTTIMEOUT, PARTYJOINFAILED, PARTYREQUESTRESPONSE errors; PARTYLEVELUPUPDATE from the WIZ-7 hook; PARTYREQUESTMEMBERZONES -> PARTYSUBMITMEMBERZONES
- WIZARD3 CHANGEGROUPLEADER; WIZARD2 group quick chat routed to members
- Group chat channel via GAME CHANNELCHAT

**Client messages:** GAME MSG_PARTYREQUESTINVITE, GAME MSG_PARTYREQUESTJOIN, GAME MSG_PARTYREQUESTACCEPT, GAME MSG_PARTYREQUESTDECLINE, GAME MSG_PARTYJOINNOTIFICATION, GAME MSG_PARTYUPDATE, GAME MSG_PARTYLEAVE, GAME MSG_PARTYLEAVENOTIFICATION, GAME MSG_PARTYDISBAND, GAME MSG_PARTYREQUESTTIMEOUT, GAME MSG_PARTYJOINFAILED, GAME MSG_PARTYREQUESTRESPONSE, GAME MSG_PARTYLEVELUPUPDATE, GAME MSG_PARTYREQUESTMEMBERZONES, GAME MSG_PARTYSUBMITMEMBERZONES, GAME MSG_CHANNELCHAT, WIZARD3 MSG_CHANGEGROUPLEADER

**Acceptance**

- [ ] Unit test: a 5th invite into a full party returns PARTYJOINFAILED
- [ ] Unit test: the leader leaving promotes the next member
- [ ] Real clients A, B: A invites B, B sees the invite popup and accepts. Both see each other's portrait in the group panel with level and school. B levels up and A's panel updates. B leaves and A's panel clears.

**Risks**

- ErrorCode values for PARTYREQUESTRESPONSE and PARTYJOINFAILED are unknown and must be found by watching the client's error text.
- Multi-player mount party messages are deferred.

## 12.05 Groups: updates, member zones, channel chat, leader (WIZ-19 part 2)

**Goal:** Live party panel.

**Size:** M. **Depends on:** 12.04

**Client messages:** MSG_PARTYUPDATE, MSG_PARTYLEVELUPUPDATE, MSG_PARTYREQUESTMEMBERZONES, MSG_PARTYSUBMITMEMBERZONES, MSG_CHANNELCHAT, MSG_CHANGEGROUPLEADER

**Acceptance**

- [ ] Real client: B levels up and A's panel updates

### Detailed spec from WIZ-19: Groups (parties)

Players can invite each other into a group of up to four, see member info and leave or disband, and group state follows zone changes.

**Deliverables**

- src/server/game/Groups/GroupMgr (sGroupMgr): party id, leader, members, invite timeout (config)
- GAME PARTYREQUESTINVITE -> PARTYREQUESTJOIN to the target; PARTYREQUESTACCEPT/DECLINE; PARTYJOINNOTIFICATION, PARTYUPDATE (school, level, zone, leader), PARTYLEAVE -> PARTYLEAVENOTIFICATION, PARTYDISBAND, PARTYREQUESTTIMEOUT, PARTYJOINFAILED, PARTYREQUESTRESPONSE errors; PARTYLEVELUPUPDATE from the WIZ-7 hook; PARTYREQUESTMEMBERZONES -> PARTYSUBMITMEMBERZONES
- WIZARD3 CHANGEGROUPLEADER; WIZARD2 group quick chat routed to members
- Group chat channel via GAME CHANNELCHAT

**Client messages:** GAME MSG_PARTYREQUESTINVITE, GAME MSG_PARTYREQUESTJOIN, GAME MSG_PARTYREQUESTACCEPT, GAME MSG_PARTYREQUESTDECLINE, GAME MSG_PARTYJOINNOTIFICATION, GAME MSG_PARTYUPDATE, GAME MSG_PARTYLEAVE, GAME MSG_PARTYLEAVENOTIFICATION, GAME MSG_PARTYDISBAND, GAME MSG_PARTYREQUESTTIMEOUT, GAME MSG_PARTYJOINFAILED, GAME MSG_PARTYREQUESTRESPONSE, GAME MSG_PARTYLEVELUPUPDATE, GAME MSG_PARTYREQUESTMEMBERZONES, GAME MSG_PARTYSUBMITMEMBERZONES, GAME MSG_CHANNELCHAT, WIZARD3 MSG_CHANGEGROUPLEADER

**Acceptance**

- [ ] Unit test: a 5th invite into a full party returns PARTYJOINFAILED
- [ ] Unit test: the leader leaving promotes the next member
- [ ] Real clients A, B: A invites B, B sees the invite popup and accepts. Both see each other's portrait in the group panel with level and school. B levels up and A's panel updates. B leaves and A's panel clears.

**Risks**

- ErrorCode values for PARTYREQUESTRESPONSE and PARTYJOINFAILED are unknown and must be found by watching the client's error text.
- Multi-player mount party messages are deferred.

## 12.06 Privacy, friendly player, teleport to friend (WIZ-20)

**Goal:** Enforced privacy toggles.

**Size:** M. **Depends on:** 12.01, 6.07

**Client messages:** MSG_REQUESTPRIVACYOPTIONS, MSG_RESPONSEPRIVACYOPTIONS, MSG_UPDATEPRIVACYOPTIONS, MSG_SETFRIENDLYPLAYER, MSG_UPDATEFRIENDLYWORLD, MSG_REQUESTFRIENDLYPLAYERS, MSG_REQUESTFRIENDLYPLAYERQUEST, MSG_GOTOFRIENDLYPLAYER, MSG_GOTOPLAYER, MSG_GOTOPLAYERRESP

**Acceptance**

- [ ] GOTOPLAYER with AllowFriendTeleport=0 errors, no transfer
- [ ] Real client: toggle persists; teleport refused or lands next to friend

### Detailed spec from WIZ-20: Privacy options, friendly-player and teleport to friend

Players can set privacy toggles (friend requests, teleports, trade, hatch, party invites) and teleport to a friend, with the server enforcing those toggles.

**Deliverables**

- data/sql/updates/db_characters: character_privacy
- WIZARD REQUESTPRIVACYOPTIONS -> RESPONSEPRIVACYOPTIONS; UPDATEPRIVACYOPTIONS persisted and enforced in SocialMgr and GroupMgr
- SETFRIENDLYPLAYER, UPDATEFRIENDLYWORLD, REQUESTFRIENDLYPLAYERS/REQUESTFRIENDLYPLAYERQUEST minimal responses
- GAME GOTOPLAYER -> GOTOPLAYERRESP and WIZARD GOTOFRIENDLYPLAYER, handing the move to the WLD zone-transfer API
- Refuse or allow per AllowFriendTeleport

**Client messages:** MSG_REQUESTPRIVACYOPTIONS, MSG_RESPONSEPRIVACYOPTIONS, MSG_UPDATEPRIVACYOPTIONS, MSG_SETFRIENDLYPLAYER, MSG_UPDATEFRIENDLYWORLD, MSG_REQUESTFRIENDLYPLAYERS, MSG_REQUESTFRIENDLYPLAYERQUEST, MSG_GOTOFRIENDLYPLAYER, GAME MSG_GOTOPLAYER, GAME MSG_GOTOPLAYERRESP

**Database tables**

- characters.character_privacy

**Acceptance**

- [ ] Unit test: GOTOPLAYER to a friend with AllowFriendTeleport=0 returns an error and no transfer
- [ ] Real client: in Options, turning off 'allow friend teleport' and relogging keeps the toggle. A friend who clicks teleport gets a refusal message. With it on, the friend's client loads into your zone next to you.

**Risks**

- The REQUESTFRIENDLYPLAYERS Data blob format is unknown.

## 12.07 Chat moderation (WIZ-21)

**Goal:** Filter, permissions, mute.

**Size:** M. **Depends on:** 6.04

**Client messages:** MSG_CHATFILTERBLACK, MSG_CHATFILTERWHITE, MSG_MUTE, MSG_NOTMUTED

**Acceptance**

- [ ] Blacklisted word flagged; whitelisted phrase passes
- [ ] Muted REQUESTRADIALCHAT dropped with notice
- [ ] Real client: '.mute <name> 5m' works

### Detailed spec from WIZ-21: Chat moderation: filter, permissions and mute

Accounts get open or filtered chat, filtered words are handled the way the client expects, and GMs can mute players.

**Deliverables**

- login.account chat_mode (open/filtered/closed) -> ClientWizPlayerNameBehavior.m_chatPermissions
- src/server/game/Chat/ChatFilter: loads ChatFilter/WhiteListBase.txt, WhiteListPhrasesBase.txt, BlackListBase.txt, ExceptionListBase.txt and CharacterReplacementMap.txt from the user's install at startup (UTF-16), and sets the RADIALCHAT Filter byte
- CHATFILTERBLACK/CHATFILTERWHITE senders for runtime additions
- GAME MUTE, NOTMUTED; login.account_muted (until, reason, by)
- src/server/scripts/Commands/cs_mute.cpp (.mute, .unmute), cs_ban stub owned with LOG

**Client messages:** MSG_CHATFILTERBLACK, MSG_CHATFILTERWHITE, GAME MSG_MUTE, GAME MSG_NOTMUTED

**Data sources**

- Root.wad ChatFilter/*.txt (UTF-16; WhiteListBase.txt 2.4 MB)

**Database tables**

- login.account_muted

**Acceptance**

- [ ] Unit test: a blacklisted word from a fixture list is flagged and a whitelisted phrase passes
- [ ] Unit test: a muted account's REQUESTRADIALCHAT is dropped with a notice
- [ ] Real client: after '.mute <name> 5m', the muted player gets the mute notice and nobody sees their chat until it expires. A filtered-chat account sees another player's off-whitelist message as filtered text.

**Risks**

- Whether the client or the server applies the whitelist, and how the Filter byte values map, is unverified.

## 12.08 Badges and titles (WIZ-22)

**Goal:** Earn and select titles.

**Size:** M. **Depends on:** 6.04, 10.01

**Client messages:** MSG_BADGES, MSG_SELECT_BADGE, MSG_NEWTITLE, MSG_REQUESTNEWBADGE, MSG_REQUESTPLAYERBADGE

**Acceptance**

- [ ] No duplicates; progress clamps; LastSegment only on final
- [ ] Real client: '.badge add' shows under filter; title seen by second client

### Detailed spec from WIZ-22: Badges and titles

Wizards earn badges, can pick one as the title above their head, and the badge book shows progress.

**Deliverables**

- world.badge_template (name, title key, info key, filter, requirements, registry name/value, auto-add, overcount) authored as SQL, because BadgeTemplate files do not appear as roots in Root.wad
- BadgeFilterDescriptions.xml extractor -> world.badge_filter
- data/sql/updates/db_characters: character_badge (badge, progress, complete), character_stats.selected_badge
- GAME BADGES (segmented, with the BadgeInfo/BadgeFilterInfo blobs), SELECT_BADGE handler that updates ClientWizPlayerNameBehavior.m_badgeTitle and broadcasts NEWTITLE
- WIZARD3 REQUESTNEWBADGE / REQUESTPLAYERBADGE minimal handlers; the leaderboard and reward-loot badge messages deferred
- cs_badge.cpp: .badge add/remove

**Client messages:** GAME MSG_BADGES, GAME MSG_SELECT_BADGE, MSG_NEWTITLE, WIZARD3 MSG_REQUESTNEWBADGE, WIZARD3 MSG_REQUESTPLAYERBADGE

**Data sources**

- Root.wad BadgeFilterDescriptions.xml (root BadgeFilterDescriptionList)
- Root.wad Locale/*.lang badge title keys

**Database tables**

- world.badge_template
- world.badge_filter
- characters.character_badge

**Acceptance**

- [ ] Unit test: granting a badge twice does not duplicate it; progress clamps at max
- [ ] Unit test: the BADGES segmented sequence sets LastSegment on the final message only
- [ ] Real client: '.badge add <badge>' makes the badge appear in the Badges tab under the right filter. Selecting it changes the title under the wizard's name, and a second client sees the new title.

**Risks**

- The badge definitions list is server-side and must be authored clean-room. Where the names and requirements come from is unresolved.
- The BADGES segment and blob layout needs capture confirmation. The private sniffer holds decoded BadgeInfoList samples for study only.

## 12.09 Bank and shared bank (WIZ-23)

**Goal:** Move items to storage.

**Size:** M. **Depends on:** 8.09

**Client messages:** MSG_OPENBANK, MSG_MOVEINVTOBANK, MSG_INVTOBANKCONFIRM, MSG_MOVEBANKTOINV, MSG_BANKTOINVCONFIRM, MSG_MOVEBANKTOBANK, MSG_BANKTOBANKCONFIRM, MSG_BANKDELETE, MSG_BANKDELETECONFIRM, MSG_STORAGECLIENTADD, MSG_STORAGECLIENTREMOVE, MSG_ITEMOVERFLOWTOBANK, MSG_BANKCOUNT, MSG_UPDATEBANKLIMIT, MSG_QUICKSELLREQUESTBANK

**Acceptance**

- [ ] Unowned move returns Failure=1
- [ ] Shared bank visible to second character
- [ ] Real client: hat to bank persists

### Detailed spec from WIZ-23: Bank and shared bank

Players can open the bank, move items between backpack, bank and shared bank, and delete stored items.

**Deliverables**

- data/sql/updates/db_characters: character_bank; data/sql/updates/db_login or db_characters: account_shared_bank (decide which database with the maintainer)
- OPENBANK sender (house or bank object interaction), MOVEINVTOBANK -> INVTOBANKCONFIRM, MOVEBANKTOINV -> BANKTOINVCONFIRM, MOVEBANKTOBANK -> BANKTOBANKCONFIRM, BANKDELETE -> BANKDELETECONFIRM, STORAGECLIENTADD/REMOVE, ITEMOVERFLOWTOBANK, SHAREDBANKDELETEREAGENTORPETSNACK(+CONFIRM)
- WIZARD2 BANKCOUNT, UPDATEBANKLIMIT
- Enforce ClientRequestID echo and capacity

**Client messages:** MSG_OPENBANK, MSG_MOVEINVTOBANK, MSG_INVTOBANKCONFIRM, MSG_MOVEBANKTOINV, MSG_BANKTOINVCONFIRM, MSG_MOVEBANKTOBANK, MSG_BANKTOBANKCONFIRM, MSG_BANKDELETE, MSG_BANKDELETECONFIRM, MSG_STORAGECLIENTADD, MSG_STORAGECLIENTREMOVE, MSG_ITEMOVERFLOWTOBANK, MSG_SHAREDBANKDELETEREAGENTORPETSNACK, MSG_SHAREDBANKDELETEREAGENTORPETSNACKCONFIRM, WIZARD2 MSG_BANKCOUNT, WIZARD2 MSG_UPDATEBANKLIMIT

**Database tables**

- characters.character_bank
- account_shared_bank

**Acceptance**

- [ ] Unit test: moving an item the player does not own returns Failure=1 and changes nothing
- [ ] Unit test: shared bank items are visible from a second character on the same account
- [ ] Real client: opening the bank and dragging a hat from backpack to bank moves the icon across. Relog keeps it there. A second character on the account sees shared-bank items.

**Risks**

- Bank capacity values are server-side and need a source. The shared bank crosses databases (account versus character).

## 12.10 Player trade (WIZ-25)

**Goal:** Safe two-sided trade.

**Size:** M. **Depends on:** 8.09, 12.06

**Client messages:** MSG_TRADE_CREATE, MSG_TRADE_REQUEST, MSG_TRADE_JOIN_STATUS, MSG_TRADE_CHANGE_ITEM, MSG_TRADE_CHANGE_MONEY, MSG_TRADE_READY_STATUS, MSG_TRADE_RESULT

**Acceptance**

- [ ] Change after ready clears readiness; swap fails whole on capacity
- [ ] Real client: swap works; disconnect mid-trade changes nothing

### Detailed spec from WIZ-25: Player trade

Two players can trade items and gold safely with both sides confirming.

**Deliverables**

- src/server/game/Trade/TradeSession: TRADE_CREATE -> TRADE_REQUEST to the target; TRADE_JOIN_STATUS; TRADE_CHANGE_ITEM and TRADE_CHANGE_MONEY (either change resets ready); TRADE_READY_STATUS; atomic swap in one DB transaction; TRADE_RESULT
- Respect AllowTradeRequest privacy, no-trade item flags, and capacity checks

**Client messages:** MSG_TRADE_CREATE, MSG_TRADE_REQUEST, MSG_TRADE_JOIN_STATUS, MSG_TRADE_CHANGE_ITEM, MSG_TRADE_CHANGE_MONEY, MSG_TRADE_READY_STATUS, MSG_TRADE_RESULT

**Database tables**

- characters.item_instance (ownership move)

**Acceptance**

- [ ] Unit test: a change after both are ready clears readiness
- [ ] Unit test: the swap fails whole if the receiver's backpack cannot hold the items
- [ ] Two real clients: A opens trade with B, both add an item and gold, both confirm, and the items and gold swap in both backpacks. A crash or disconnect mid-trade leaves both inventories unchanged.

**Risks**

- The Status and Action enum values are unknown. In retail, trade is limited to treasure cards/items by template (ItemTemplate/ItemEnchant fields), so which item kinds can be traded needs checking.

## 12.11 Dye shop (EXT-3 + WIZ-26 dye)

**Goal:** Recolor gear.

**Size:** S. **Depends on:** 10.10, 8.10

**Client messages:** MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM

**Acceptance**

- [ ] Unowned item rejected
- [ ] Real client: new colors persist and are seen by others

### Detailed spec from EXT-3: Dye shop

Players recolor equipment at the dye NPC.

**Deliverables**

- DyeShopOption service option, game/Handlers/DyeHandler.cpp
- Persist texture/decal/decal2 per item instance
- Dye cost rule in world DB

**Client messages:** MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM

**Data sources**

- DyeShopModifiers and dye template data in ObjectData

**Database tables**

- characters.character_inventory (texture, decal, decal2 columns)

**Acceptance**

- [ ] Unit: dye request on item not owned is rejected
- [ ] Client: dye window previews colors; confirming charges gold and the wizard model shows the new colors, which survive relog and are seen by other players

### Detailed spec from WIZ-26: Equipment sets, custom emotes and dye/stitch services

Quality-of-life wardrobe features work: saved equipment sets, unlocked custom emotes, dyeing and stitching gear.

**Deliverables**

- WIZARD2 CREATENEWEQUIPMENTSET/EQUIPMENTSETCREATED/CREATESETFAILED, DELETEEQUIPMENTSET, EQUIPSET, MOVEEQUIPMENTSET, UPDATEEQUIPMENTSETS, DELETESETSWITHITEM (EquipmentSetList in ClientWizEquipmentBehavior.m_equipmentSets)
- WIZARD2 UPDATECUSTOMEMOTES from purchased emote bitfields (WizGameStats m_purchasedCustomEmotes1-3) and CustomEmoteBehaviorTemplate data; WIZARD3 PII radial emote play request/response
- DYESHOPOPEN/DYEREQUEST/DYECONFIRM with gold cost; SEAMSTRESSOPEN/STITCHITEMS/STITCHITEMSCONFIRM and UNSTITCHOPEN/UNSTITCHITEMS (gold path only; the crowns path belongs to EXT)
- PETRENAMEREQUEST/PETRENAMECONFIRM (coordinate with PET)

**Client messages:** WIZARD2 MSG_CREATENEWEQUIPMENTSET, WIZARD2 MSG_EQUIPMENTSETCREATED, WIZARD2 MSG_CREATESETFAILED, WIZARD2 MSG_DELETEEQUIPMENTSET, WIZARD2 MSG_EQUIPSET, WIZARD2 MSG_MOVEEQUIPMENTSET, WIZARD2 MSG_UPDATEEQUIPMENTSETS, WIZARD2 MSG_DELETESETSWITHITEM, WIZARD2 MSG_UPDATECUSTOMEMOTES, WIZARD3 MSG_REQUESTPIIRADIALMENUPLAYEMOTE, WIZARD3 MSG_PIIRADIALMENUPLAYEMOTE, MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM, MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM

**Data sources**

- world.item_template (m_numPrimaryColors/m_numSecondaryColors/m_numPatterns)
- CustomEmoteBehaviorTemplate data inside ObjectData item behaviors

**Database tables**

- characters.character_equipment_set
- characters.item_instance (dye layers)

**Acceptance**

- [ ] Unit test: equipping a saved set with a missing item equips the rest and reports the missing item
- [ ] Real client: saving the current outfit as a set, changing gear, then clicking the set re-equips it. Dyeing a robe changes its colors on the model for all nearby players. An unlocked custom emote appears in the emote menu and plays for others.

**Risks**

- This milestone is broad and may need to be split into three (sets, emotes, dye/stitch) when scheduled.
- Where the custom emote template list lives is unverified.

## 12.12 Stitching (EXT-4 + WIZ-26 stitch)

**Goal:** Stats of one item, look of another.

**Size:** S. **Depends on:** 12.11

**Client messages:** MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS

**Acceptance**

- [ ] Different slot types rejected
- [ ] Real client: tooltip stats vs worn look; others see the look

### Detailed spec from EXT-4: Stitching (seamstress) and unstitch

Players can take the stats of one item and the appearance of another, and later separate them.

**Deliverables**

- game/Items stitched-appearance field on item instance
- Handler for stitch/unstitch with cost and item consumption rules

**Client messages:** MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS

**Data sources**

- ObjectData item templates (StatsID/DisplayID template compatibility flags)

**Database tables**

- characters.character_inventory (display_template_id)

**Acceptance**

- [ ] Unit: stitching items of different slot types is rejected
- [ ] Client: after stitching, the tooltip shows the stat item's stats while the character wears the display item's look, and other clients see the display look

**Risks**

- Unstitch rules (which item returns) unverified

### Detailed spec from WIZ-26: Equipment sets, custom emotes and dye/stitch services

Quality-of-life wardrobe features work: saved equipment sets, unlocked custom emotes, dyeing and stitching gear.

**Deliverables**

- WIZARD2 CREATENEWEQUIPMENTSET/EQUIPMENTSETCREATED/CREATESETFAILED, DELETEEQUIPMENTSET, EQUIPSET, MOVEEQUIPMENTSET, UPDATEEQUIPMENTSETS, DELETESETSWITHITEM (EquipmentSetList in ClientWizEquipmentBehavior.m_equipmentSets)
- WIZARD2 UPDATECUSTOMEMOTES from purchased emote bitfields (WizGameStats m_purchasedCustomEmotes1-3) and CustomEmoteBehaviorTemplate data; WIZARD3 PII radial emote play request/response
- DYESHOPOPEN/DYEREQUEST/DYECONFIRM with gold cost; SEAMSTRESSOPEN/STITCHITEMS/STITCHITEMSCONFIRM and UNSTITCHOPEN/UNSTITCHITEMS (gold path only; the crowns path belongs to EXT)
- PETRENAMEREQUEST/PETRENAMECONFIRM (coordinate with PET)

**Client messages:** WIZARD2 MSG_CREATENEWEQUIPMENTSET, WIZARD2 MSG_EQUIPMENTSETCREATED, WIZARD2 MSG_CREATESETFAILED, WIZARD2 MSG_DELETEEQUIPMENTSET, WIZARD2 MSG_EQUIPSET, WIZARD2 MSG_MOVEEQUIPMENTSET, WIZARD2 MSG_UPDATEEQUIPMENTSETS, WIZARD2 MSG_DELETESETSWITHITEM, WIZARD2 MSG_UPDATECUSTOMEMOTES, WIZARD3 MSG_REQUESTPIIRADIALMENUPLAYEMOTE, WIZARD3 MSG_PIIRADIALMENUPLAYEMOTE, MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM, MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM

**Data sources**

- world.item_template (m_numPrimaryColors/m_numSecondaryColors/m_numPatterns)
- CustomEmoteBehaviorTemplate data inside ObjectData item behaviors

**Database tables**

- characters.character_equipment_set
- characters.item_instance (dye layers)

**Acceptance**

- [ ] Unit test: equipping a saved set with a missing item equips the rest and reports the missing item
- [ ] Real client: saving the current outfit as a set, changing gear, then clicking the set re-equips it. Dyeing a robe changes its colors on the model for all nearby players. An unlocked custom emote appears in the emote menu and plays for others.

**Risks**

- This milestone is broad and may need to be split into three (sets, emotes, dye/stitch) when scheduled.
- Where the custom emote template list lives is unverified.

## 12.13 Equipment sets (WIZ-26 part)

**Goal:** Saved outfits.

**Size:** M. **Depends on:** 8.10

**Client messages:** MSG_CREATENEWEQUIPMENTSET, MSG_EQUIPMENTSETCREATED, MSG_CREATESETFAILED, MSG_DELETEEQUIPMENTSET, MSG_EQUIPSET, MSG_MOVEEQUIPMENTSET, MSG_UPDATEEQUIPMENTSETS, MSG_DELETESETSWITHITEM

**Acceptance**

- [ ] Set with a missing item equips the rest and reports it
- [ ] Real client: clicking a set re-equips it

### Detailed spec from WIZ-26: Equipment sets, custom emotes and dye/stitch services

Quality-of-life wardrobe features work: saved equipment sets, unlocked custom emotes, dyeing and stitching gear.

**Deliverables**

- WIZARD2 CREATENEWEQUIPMENTSET/EQUIPMENTSETCREATED/CREATESETFAILED, DELETEEQUIPMENTSET, EQUIPSET, MOVEEQUIPMENTSET, UPDATEEQUIPMENTSETS, DELETESETSWITHITEM (EquipmentSetList in ClientWizEquipmentBehavior.m_equipmentSets)
- WIZARD2 UPDATECUSTOMEMOTES from purchased emote bitfields (WizGameStats m_purchasedCustomEmotes1-3) and CustomEmoteBehaviorTemplate data; WIZARD3 PII radial emote play request/response
- DYESHOPOPEN/DYEREQUEST/DYECONFIRM with gold cost; SEAMSTRESSOPEN/STITCHITEMS/STITCHITEMSCONFIRM and UNSTITCHOPEN/UNSTITCHITEMS (gold path only; the crowns path belongs to EXT)
- PETRENAMEREQUEST/PETRENAMECONFIRM (coordinate with PET)

**Client messages:** WIZARD2 MSG_CREATENEWEQUIPMENTSET, WIZARD2 MSG_EQUIPMENTSETCREATED, WIZARD2 MSG_CREATESETFAILED, WIZARD2 MSG_DELETEEQUIPMENTSET, WIZARD2 MSG_EQUIPSET, WIZARD2 MSG_MOVEEQUIPMENTSET, WIZARD2 MSG_UPDATEEQUIPMENTSETS, WIZARD2 MSG_DELETESETSWITHITEM, WIZARD2 MSG_UPDATECUSTOMEMOTES, WIZARD3 MSG_REQUESTPIIRADIALMENUPLAYEMOTE, WIZARD3 MSG_PIIRADIALMENUPLAYEMOTE, MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM, MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM

**Data sources**

- world.item_template (m_numPrimaryColors/m_numSecondaryColors/m_numPatterns)
- CustomEmoteBehaviorTemplate data inside ObjectData item behaviors

**Database tables**

- characters.character_equipment_set
- characters.item_instance (dye layers)

**Acceptance**

- [ ] Unit test: equipping a saved set with a missing item equips the rest and reports the missing item
- [ ] Real client: saving the current outfit as a set, changing gear, then clicking the set re-equips it. Dyeing a robe changes its colors on the model for all nearby players. An unlocked custom emote appears in the emote menu and plays for others.

**Risks**

- This milestone is broad and may need to be split into three (sets, emotes, dye/stitch) when scheduled.
- Where the custom emote template list lives is unverified.

## 12.14 Custom emotes and pet rename (WIZ-26 part)

**Goal:** Unlocked emotes.

**Size:** M. **Depends on:** 6.03, 5.05

**Client messages:** MSG_UPDATECUSTOMEMOTES, MSG_REQUESTPIIRADIALMENUPLAYEMOTE, MSG_PIIRADIALMENUPLAYEMOTE, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM

**Acceptance**

- [ ] Real client: unlocked emote appears and plays for others

### Detailed spec from WIZ-26: Equipment sets, custom emotes and dye/stitch services

Quality-of-life wardrobe features work: saved equipment sets, unlocked custom emotes, dyeing and stitching gear.

**Deliverables**

- WIZARD2 CREATENEWEQUIPMENTSET/EQUIPMENTSETCREATED/CREATESETFAILED, DELETEEQUIPMENTSET, EQUIPSET, MOVEEQUIPMENTSET, UPDATEEQUIPMENTSETS, DELETESETSWITHITEM (EquipmentSetList in ClientWizEquipmentBehavior.m_equipmentSets)
- WIZARD2 UPDATECUSTOMEMOTES from purchased emote bitfields (WizGameStats m_purchasedCustomEmotes1-3) and CustomEmoteBehaviorTemplate data; WIZARD3 PII radial emote play request/response
- DYESHOPOPEN/DYEREQUEST/DYECONFIRM with gold cost; SEAMSTRESSOPEN/STITCHITEMS/STITCHITEMSCONFIRM and UNSTITCHOPEN/UNSTITCHITEMS (gold path only; the crowns path belongs to EXT)
- PETRENAMEREQUEST/PETRENAMECONFIRM (coordinate with PET)

**Client messages:** WIZARD2 MSG_CREATENEWEQUIPMENTSET, WIZARD2 MSG_EQUIPMENTSETCREATED, WIZARD2 MSG_CREATESETFAILED, WIZARD2 MSG_DELETEEQUIPMENTSET, WIZARD2 MSG_EQUIPSET, WIZARD2 MSG_MOVEEQUIPMENTSET, WIZARD2 MSG_UPDATEEQUIPMENTSETS, WIZARD2 MSG_DELETESETSWITHITEM, WIZARD2 MSG_UPDATECUSTOMEMOTES, WIZARD3 MSG_REQUESTPIIRADIALMENUPLAYEMOTE, WIZARD3 MSG_PIIRADIALMENUPLAYEMOTE, MSG_DYESHOPOPEN, MSG_DYEREQUEST, MSG_DYECONFIRM, MSG_SEAMSTRESSOPEN, MSG_STITCHITEMS, MSG_STITCHITEMSCONFIRM, MSG_UNSTITCHOPEN, MSG_UNSTITCHITEMS, MSG_PETRENAMEREQUEST, MSG_PETRENAMECONFIRM

**Data sources**

- world.item_template (m_numPrimaryColors/m_numSecondaryColors/m_numPatterns)
- CustomEmoteBehaviorTemplate data inside ObjectData item behaviors

**Database tables**

- characters.character_equipment_set
- characters.item_instance (dye layers)

**Acceptance**

- [ ] Unit test: equipping a saved set with a missing item equips the rest and reports the missing item
- [ ] Real client: saving the current outfit as a set, changing gear, then clicking the set re-equips it. Dyeing a robe changes its colors on the model for all nearby players. An unlocked custom emote appears in the emote menu and plays for others.

**Risks**

- This milestone is broad and may need to be split into three (sets, emotes, dye/stitch) when scheduled.
- Where the custom emote template list lives is unverified.

## 12.15 Crowns balance and crown services (EXT-7)

**Goal:** Crowns ledger, energy, respec.

**Size:** S. **Depends on:** 10.10, 2.13

**Client messages:** MSG_CROWNBALANCE, MSG_CROWNSERVICESOPEN, MSG_CROWNSBUYREQUEST, MSG_CROWNSBUYCONFIRM, MSG_ENERGYSHOPOPEN, MSG_ENERGYBUYREQUEST, MSG_BUYENERGYCONFIRM, MSG_RESPECCONFIRM, MSG_RENTALUPDATE, MSG_SETRENTALTIMER

**Acceptance**

- [ ] Debit atomic, never negative under concurrency
- [ ] Real client: HUD crowns; energy refill; respec

### Detailed spec from EXT-7: Crown balance, crown services NPC, energy and respec

The HUD shows a crowns balance and crown services (energy refill, respec, crown purchases at NPCs) work.

**Deliverables**

- login DB account crowns ledger with transactional debit
- game/Handlers/CrownServicesHandler.cpp
- GM command cs_crowns (grant/show)

**Client messages:** MSG_CROWNBALANCE, MSG_CROWNSERVICESOPEN, MSG_CROWNSBUYREQUEST, MSG_CROWNSBUYCONFIRM, MSG_ENERGYSHOPOPEN, MSG_ENERGYBUYREQUEST, MSG_BUYENERGYCONFIRM, MSG_RESPECCONFIRM, MSG_RENTALUPDATE, MSG_SETRENTALTIMER

**Data sources**

- ObjectData/CrownItems templates

**Database tables**

- login.account_crowns
- login.crown_transactions

**Acceptance**

- [ ] Unit: debit is atomic and never goes negative under two concurrent purchases
- [ ] Client: HUD crowns counter shows the granted amount; buying an energy refill at the crown NPC refills energy and lowers crowns; respec resets training points

**Risks**

- Real-money purchase of crowns is out of scope; crowns only granted by GM or config

## 12.16 Bazaar (EXT-5)

**Goal:** Shared stock auction house.

**Size:** M. **Depends on:** 10.10, 12.09

**Client messages:** MSG_AUCTIONHOUSEREQUEST, MSG_AUCTIONHOUSECONTENTS, MSG_AUCTIONRESPONSE, MSG_AUCTIONHOUSEMOREACKNOWLEDGEMENT, MSG_AUCTIONHOUSEUPDATE, MSG_AUCTIONREQUESTBANK, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUESTBANK

**Acceptance**

- [ ] Selling raises stock; price follows modifiers
- [ ] Real client: item sold by A bought by B

### Detailed spec from EXT-5: Bazaar (auction house)

Players sell items to and buy them back from a shared, stock-based Bazaar.

**Deliverables**

- game/AuctionHouse/AuctionHouseMgr (sAuctionHouseMgr) with shared stock, price modifiers and category paging
- AuctionHouseEntry blob writer
- game/Handlers/AuctionHouseHandler.cpp dispatching on Command byte
- Periodic stock decay/restock timer

**Client messages:** MSG_AUCTIONHOUSEREQUEST, MSG_AUCTIONHOUSECONTENTS, MSG_AUCTIONRESPONSE, MSG_AUCTIONHOUSEMOREACKNOWLEDGEMENT, MSG_AUCTIONHOUSEUPDATE, MSG_AUCTIONREQUESTBANK, MSG_REQUESTQUICKSELL, MSG_QUICKSELLREQUESTBANK

**Data sources**

- Root.wad AuctionHouseConfig.xml (AuctionHouseConfig, AuctionPriceMods, AuctionTemplateIDList)

**Database tables**

- characters.auction_stock (realm-wide)

**Acceptance**

- [ ] Unit: selling increases stock and buy price follows AuctionPriceMods; buying last unit empties category
- [ ] Client: Bazaar NPC opens categories with paging; an item sold by player A shows up for player B and can be bought; quick-sell from bank works

**Risks**

- Command byte values and paging key semantics need confirming against the client window behavior

## 12.17 Public instances by capacity (WLD-22 part 1)

**Goal:** Soft/hard limits, '.instance list/go'.

**Size:** M. **Depends on:** 6.14

**Acceptance**

- [ ] Soft limit 2: 3rd gets a new instance, 4th joins the less full

### Detailed spec from WLD-22: Instances: capacity spill, private dungeons, instance switching

Busy zones split across instances, dungeons get private per-group instances, and players can list and switch instances.

**Deliverables**

- MapMgr: a public instance is picked with m_nSoftLimit as the join preference and m_nHardLimit as the refusal point; private instances keyed by owner or group id with Zone.PrivateInstanceTimeout
- MSG_REALM_INFO_QUERY response with RealmInfoList and InstanceInfoList blobs; MSG_TRANSFER_INSTANCE (ZoneID) switches instance through the transfer flow; MSG_CURRENTREALM (GAME2 55)
- '.instance list' and '.instance go <id>' in cs_zone.cpp
- ResTeleport flag for private-instance destinations, used by dungeon entrances

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_INSTANCE, MSG_TRANSFER_REALMS, MSG_CURRENTREALM, MSG_SERVERTRANSFER

**Data sources**

- zone_template soft/hard limit and zone adjectives
- Type dump: RealmInfoList, RealmInfo, InstanceInfoList, InstanceInfo

**Database tables**

- world.zone_template

**Acceptance**

- [ ] Unit: with soft limit 2, a 3rd player gets a new instance and a 4th joins the less-full one
- [ ] Real client: the realm/instance picker lists the instances of the current zone with populations; choosing another reloads there
- [ ] Real client: two ungrouped players entering the same dungeon door get separate instances; leaving and re-entering within the timeout returns to the same instance

**Risks**

- How groups and parties key private instances depends on the social/party domain (EXT)
- MSG_TRANSFER_REALMS needs multi-realm support from LOG. Answer with an error until then.

## 12.18 Private dungeon instances (WLD-22 part 2)

**Goal:** Per owner/group instances.

**Size:** M. **Depends on:** 12.17, 12.05

**Acceptance**

- [ ] Real client: ungrouped players get separate dungeon instances; re-entry within timeout returns to same

### Detailed spec from WLD-22: Instances: capacity spill, private dungeons, instance switching

Busy zones split across instances, dungeons get private per-group instances, and players can list and switch instances.

**Deliverables**

- MapMgr: a public instance is picked with m_nSoftLimit as the join preference and m_nHardLimit as the refusal point; private instances keyed by owner or group id with Zone.PrivateInstanceTimeout
- MSG_REALM_INFO_QUERY response with RealmInfoList and InstanceInfoList blobs; MSG_TRANSFER_INSTANCE (ZoneID) switches instance through the transfer flow; MSG_CURRENTREALM (GAME2 55)
- '.instance list' and '.instance go <id>' in cs_zone.cpp
- ResTeleport flag for private-instance destinations, used by dungeon entrances

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_INSTANCE, MSG_TRANSFER_REALMS, MSG_CURRENTREALM, MSG_SERVERTRANSFER

**Data sources**

- zone_template soft/hard limit and zone adjectives
- Type dump: RealmInfoList, RealmInfo, InstanceInfoList, InstanceInfo

**Database tables**

- world.zone_template

**Acceptance**

- [ ] Unit: with soft limit 2, a 3rd player gets a new instance and a 4th joins the less-full one
- [ ] Real client: the realm/instance picker lists the instances of the current zone with populations; choosing another reloads there
- [ ] Real client: two ungrouped players entering the same dungeon door get separate instances; leaving and re-entering within the timeout returns to the same instance

**Risks**

- How groups and parties key private instances depends on the social/party domain (EXT)
- MSG_TRANSFER_REALMS needs multi-realm support from LOG. Answer with an error until then.

## 12.19 Realm and instance picker (LOG-16 part 1 + WLD-22 part 3)

**Goal:** MSG_REALM_INFO_QUERY with RealmInfoList and InstanceInfoList.

**Size:** M. **Depends on:** 12.17, 4.03

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_INSTANCE, MSG_CURRENTREALM

**Acceptance**

- [ ] RealmInfoList with 2 realms serializes with class hash 713899438; empty InstanceInfoList non-empty
- [ ] Real client: picker lists instances with populations; choosing another reloads

### Detailed spec from LOG-16: In-game realm list and realm transfer

A player can open the in-game realm picker, see every online realm with its population, and switch realms without logging out.

**Deliverables**

- src/server/game/Handlers/RealmHandler.cpp: HandleRealmInfoQuery replies MSG_REALM_INFO_QUERY{RealmInfoList=serialized RealmInfoList of RealmInfo{m_realmName, m_displayName, m_realmPopulation}, CurrentRealm, InstanceInfoList=serialized empty InstanceInfoList (must be a valid object, not an empty string), CurrentZone}
- HandleTransferRealms: validate the target realm is online and not full; create a login_key for the target realm; send MSG_SERVERTRANSFER{IP, TCPPort, UDPPort, Key(INT), UserID, CharID, ZoneName, ZoneID, Location, Slot, SessionID, SessionSlot, TargetPlayerID, Fallback*=current realm, TransitionID}; on error reply MSG_TRANSFER_REALMS{Error}
- The gameserver sets MSG_LOGINCOMPLETE.RealmName to its own realmlist name (field owned by WLD)

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_REALMS, MSG_SERVERTRANSFER, MSG_ATTACH, MSG_LOGINCOMPLETE

**Data sources**

- Type dump RealmInfoList (713899438), RealmInfo (1889885615), InstanceInfoList
- Root.wad Locale/en-US/RealmNames.lang

**Database tables**

- realmlist
- login_key

**Acceptance**

- [ ] Unit: a RealmInfoList with 2 realms serializes with class hash 713899438 and decodes back to the same values; the empty InstanceInfoList serializes to a non-empty blob
- [ ] Real client with 2 gameservers running: the realm window lists both with population bars; picking the other realm triggers a loading transition and the log shows the character attached on the second gameserver in the same zone

**Risks**

- MSG_SERVERTRANSFER.Key is an INT, while the login key is a string; the reference sets Key=0 and pre-registers the session on the target realm. That transfer-key mechanism needs its own design (for example a key table indexed by a 32-bit token).
- Whether the client needs WIZARD2 MSG_CURRENTREALM (the reference handles one in service 55) is unverified.
- It depends on WLD's zone-transfer machinery being in place.

### Detailed spec from WLD-22: Instances: capacity spill, private dungeons, instance switching

Busy zones split across instances, dungeons get private per-group instances, and players can list and switch instances.

**Deliverables**

- MapMgr: a public instance is picked with m_nSoftLimit as the join preference and m_nHardLimit as the refusal point; private instances keyed by owner or group id with Zone.PrivateInstanceTimeout
- MSG_REALM_INFO_QUERY response with RealmInfoList and InstanceInfoList blobs; MSG_TRANSFER_INSTANCE (ZoneID) switches instance through the transfer flow; MSG_CURRENTREALM (GAME2 55)
- '.instance list' and '.instance go <id>' in cs_zone.cpp
- ResTeleport flag for private-instance destinations, used by dungeon entrances

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_INSTANCE, MSG_TRANSFER_REALMS, MSG_CURRENTREALM, MSG_SERVERTRANSFER

**Data sources**

- zone_template soft/hard limit and zone adjectives
- Type dump: RealmInfoList, RealmInfo, InstanceInfoList, InstanceInfo

**Database tables**

- world.zone_template

**Acceptance**

- [ ] Unit: with soft limit 2, a 3rd player gets a new instance and a 4th joins the less-full one
- [ ] Real client: the realm/instance picker lists the instances of the current zone with populations; choosing another reloads there
- [ ] Real client: two ungrouped players entering the same dungeon door get separate instances; leaving and re-entering within the timeout returns to the same instance

**Risks**

- How groups and parties key private instances depends on the social/party domain (EXT)
- MSG_TRANSFER_REALMS needs multi-realm support from LOG. Answer with an error until then.

## 12.20 Realm transfer (LOG-16 part 2)

**Goal:** Switch realms without logout.

**Size:** M. **Depends on:** 12.19, 6.07

**Client messages:** MSG_TRANSFER_REALMS, MSG_SERVERTRANSFER, MSG_ATTACH, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] Real client with 2 gameservers: switching attaches on the second in the same zone

### Detailed spec from LOG-16: In-game realm list and realm transfer

A player can open the in-game realm picker, see every online realm with its population, and switch realms without logging out.

**Deliverables**

- src/server/game/Handlers/RealmHandler.cpp: HandleRealmInfoQuery replies MSG_REALM_INFO_QUERY{RealmInfoList=serialized RealmInfoList of RealmInfo{m_realmName, m_displayName, m_realmPopulation}, CurrentRealm, InstanceInfoList=serialized empty InstanceInfoList (must be a valid object, not an empty string), CurrentZone}
- HandleTransferRealms: validate the target realm is online and not full; create a login_key for the target realm; send MSG_SERVERTRANSFER{IP, TCPPort, UDPPort, Key(INT), UserID, CharID, ZoneName, ZoneID, Location, Slot, SessionID, SessionSlot, TargetPlayerID, Fallback*=current realm, TransitionID}; on error reply MSG_TRANSFER_REALMS{Error}
- The gameserver sets MSG_LOGINCOMPLETE.RealmName to its own realmlist name (field owned by WLD)

**Client messages:** MSG_REALM_INFO_QUERY, MSG_TRANSFER_REALMS, MSG_SERVERTRANSFER, MSG_ATTACH, MSG_LOGINCOMPLETE

**Data sources**

- Type dump RealmInfoList (713899438), RealmInfo (1889885615), InstanceInfoList
- Root.wad Locale/en-US/RealmNames.lang

**Database tables**

- realmlist
- login_key

**Acceptance**

- [ ] Unit: a RealmInfoList with 2 realms serializes with class hash 713899438 and decodes back to the same values; the empty InstanceInfoList serializes to a non-empty blob
- [ ] Real client with 2 gameservers running: the realm window lists both with population bars; picking the other realm triggers a loading transition and the log shows the character attached on the second gameserver in the same zone

**Risks**

- MSG_SERVERTRANSFER.Key is an INT, while the login key is a string; the reference sets Key=0 and pre-registers the session on the target realm. That transfer-key mechanism needs its own design (for example a key table indexed by a 32-bit token).
- Whether the client needs WIZARD2 MSG_CURRENTREALM (the reference handles one in service 55) is unverified.
- It depends on WLD's zone-transfer machinery being in place.

## 12.21 Admission queue (LOG-15)

**Goal:** Queue for full realms.

**Size:** M. **Depends on:** 4.05, 2.15

**Client messages:** MSG_CHARACTERSELECTED, MSG_USER_ADMIT_IND

**Acceptance**

- [ ] player_limit=1: second queued at 1 and released on logout
- [ ] Real client: queue position then auto-enter

### Detailed spec from LOG-15: Admission queue for full realms

When the chosen realm is at player_limit, the client waits in a visible queue and is sent in automatically when a slot opens.

**Deliverables**

- src/server/apps/loginserver/Realms/AdmissionQueue.{h,cpp}: a FIFO per realm; SelectCharacter on a full realm replies MSG_CHARACTERSELECTED{PrepPhase=1, Slot=<position>} (or USER_ADMIT_IND with PositionInQueue) and caches the final CHARACTERSELECTED, released when population drops
- Periodic position updates; GM accounts (security_level >= config) bypass the queue

**Client messages:** MSG_CHARACTERSELECTED, MSG_USER_ADMIT_IND

**Database tables**

- realmlist
- realm_online_character

**Acceptance**

- [ ] Unit: with player_limit=1 and one player online, a second select is queued at position 1 and receives the cached CHARACTERSELECTED when the first player's realm_online_character row is removed
- [ ] Real client: with the limit set to 1, the second client shows a queue position and enters the world automatically when the first client logs out

**Risks**

- The whole queue UI contract (PrepPhase and Slot semantics, whether ADMIT_IND Status=0 means queued or refused) is unverified; this is the reference's behavior only. Low priority.

## 12.22 Same-connection zone transfer spike (WLD-21)

**Goal:** Test MSG_ZONETRANSFER.

**Size:** S. **Depends on:** 6.07

**Client messages:** MSG_ZONETRANSFER, MSG_UPDATEZONECOUNTER, MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] inprocess mode WC_Hub -> Ravenwood with no reconnect, or stall recorded and default stays reconnect

### Detailed spec from WLD-21: Same-connection zone transfer spike (MSG_ZONETRANSFER)

Find out whether the retail client can change zones on the same connection, and adopt it if so, because it is faster.

**Deliverables**

- Conf switch Zone.TransferMode = reconnect|inprocess
- In-process path: after MSG_ZONETRANSFERACK send MSG_ZONETRANSFER (ZoneName, ZoneID, DynamicZoneID, DynamicServerProcID, ZoneCounter+1, TransitionID), then the new zone's LOGINCOMPLETE-equivalent flow; MSG_UPDATEZONECOUNTER for intra-cluster moves
- A written result in doc/ of which messages the client expected, whatever the outcome

**Client messages:** MSG_ZONETRANSFER, MSG_UPDATEZONECOUNTER, MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] Real client in inprocess mode: transfer WC_Hub -> Ravenwood with the loading screen and no socket reconnect (verified in server connection log); MSG_CLIENTMOVE after arrival carries the new ZoneCounter
- [ ] If the client stalls, the spike records the observed state and the default stays 'reconnect'

**Risks**

- Completely unverified: the behavior reference never sends MSG_ZONETRANSFER. What the client needs after it (a new LOGINCOMPLETE or not) is unknown.

## 12.23 MoveBehavior/Physics observation and speed validation (WLD-23)

**Goal:** Settle services 15/16; reject impossible moves.

**Size:** S. **Depends on:** 5.03, 11.03

**Client messages:** MSG_MB_MOVE, MSG_MB_MOVE_T, MSG_MB_TELEPORT, MSG_MB_MOVESTATE, MSG_PHYSICS_STATE, MSG_PHYSICS_FORCE, MSG_PHYSICS_FORCE_AT_POS, MSG_PHYSICS_TORQUE, MSG_PHYSICS_GRAB, MSG_PHYSICS_RELEASE, MSG_MOVECORRECTION, MSG_CLIENTMOVE

**Acceptance**

- [ ] Counter summary recorded in doc/
- [ ] 10x distance move yields one MSG_MOVECORRECTION
- [ ] Normal running and mounts never corrected

### Detailed spec from WLD-23: MoveBehavior/Physics observation and move validation

Settle whether services 15 and 16 are ever used by the retail client in normal world play, and reject impossible player moves.

**Deliverables**

- Log-only handlers with counters for MSG_MB_MOVE, MSG_MB_MOVE_T, MSG_PHYSICS_STATE, MSG_PHYSICS_FORCE, MSG_PHYSICS_FORCE_AT_POS, MSG_PHYSICS_TORQUE, MSG_PHYSICS_GRAB, MSG_PHYSICS_RELEASE (server-bound directions only)
- Speed check in HandleClientMove from speed stats (WIZ) plus a tolerance; on violation send MSG_MOVECORRECTION (float location and direction) and keep the last valid position
- conf/dist/gameserver.conf.dist: Movement.ValidateSpeed, Movement.SpeedTolerance

**Client messages:** MSG_MB_MOVE, MSG_MB_MOVE_T, MSG_MB_TELEPORT, MSG_MB_MOVESTATE, MSG_PHYSICS_STATE, MSG_PHYSICS_FORCE, MSG_PHYSICS_FORCE_AT_POS, MSG_PHYSICS_TORQUE, MSG_PHYSICS_GRAB, MSG_PHYSICS_RELEASE, MSG_MOVECORRECTION, MSG_CLIENTMOVE

**Data sources**

- Messages/MoveBehaviorMessages.xml, Messages/PhysicsBehaviorMessages.xml

**Acceptance**

- [ ] After a normal play session (walk, mount, zone, jump), the counter summary for services 15 and 16 is recorded in doc/
- [ ] Unit: a move 10x the allowed distance per tick is rejected and yields one MSG_MOVECORRECTION
- [ ] Real client: a crafted teleport-hack packet snaps the wizard back to the last valid spot; normal running and mounts never trigger a correction

**Risks**

- Mount speed multipliers and m_speedMultiplier come from WIZ; false positives would rubber-band honest players, so ship with validation off by default
- Server-side collision (collision.bcd, zone.nav) is out of scope; only speed is checked

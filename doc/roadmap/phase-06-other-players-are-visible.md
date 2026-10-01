<!-- Project Ambrose by Imjustchico: Roadmap phase 6, Other players are visible. -->

# Phase 6: Other players are visible

**Done when:** Two clients in WC_Hub see each other walk, jump, chat and emote, and one sees the other vanish on logout. Walking through the Ravenwood gate transfers zones. GMs teleport, kick and ban.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 6.01 | Whole-zone player broadcast (WLD-10) | M | 5.02, 5.03, 5.05, 4.16 |
| 6.02 | PLAYERWIZBANG broadcast (WIZ-1 remainder) | S | 6.01 |
| 6.03 | Say chat, quick chat, core emotes (WIZ-4) | M | 6.01, 4.02, 4.15, 4.16 |
| 6.04 | GM commands in chat (WIZ-5 remainder) | M | 6.03, 5.01, 4.16 |
| 6.05 | GM account, ban, character commands (LOG-17) | M | 6.04, 3.17, 4.15 |
| 6.06 | Same-zone teleport and cs_tele (WLD-12) | S | 6.01, 4.02, 4.15 |
| 6.07 | Cross-zone transfer via MSG_SERVERTRANSFER (WLD-13) | M | 6.06, 4.13 |
| 6.08 | Logout, link-dead, AFK, shutdown (WLD-20) | M | 6.01, 4.16 |
| 6.09 | Schema probe for classes missing from the dump (OBJ-11) | M | 3.11 |
| 6.10 | Supplemental server-side schemas (OBJ-12) | S | 6.09, 3.03, 4.15 |
| 6.11 | Trigger/volume schemas and zone WAD sweep (WLD-3 part 1 + OBJ-18) | M | 6.10 |
| 6.12 | Volume/trigger extraction to world rows (WLD-3 part 2) | M | 6.11, 4.08 |
| 6.13 | Volumes and walk-in trigger events (WLD-14) | M | 6.12, 5.03, 4.01, 4.15 |
| 6.14 | Zone doors table and walk-in transfers (WLD-15) | M | 6.07, 6.13, 4.15 |
| 6.15 | AOI grid and visibility sets, unit level (WLD-11 part 1) | M | 6.01, 4.16 |
| 6.16 | AOI in the real client (WLD-11 part 2) | M | 6.15 |
| 6.17 | Network hardening (NET-11) | M | 2.09, 4.16 |
| 6.18 | Packet log, diagnostics, network hooks (NET-12) | S | 2.09, 4.02, 4.16 |

## 6.01 Whole-zone player broadcast (WLD-10)

**Goal:** Players see each other appear, move and leave.

**Size:** M. **Depends on:** 5.02, 5.03, 5.05, 4.16

**Client messages:** MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE, MSG_CLIENTMOVESTATE, MSG_JUMP, MSG_CLIENTMOVE

**Acceptance**

- [x] Two clients see each other with correct name and gear
- [x] B sees A's smooth run and idle within ~0.5 s; jumps relay
- [x] A logs out and vanishes on B at once; quick relog shows no ghost
- [x] Changing Zone.MoveFlushInterval applies from the next flush

### Detailed spec from WLD-10: Players see each other (whole-zone broadcast)

Two real clients in the same zone instance see each other appear, walk, jump, animate and disappear.

**Deliverables**

- src/server/game/World/PlayerMeetings and World::Update (built in place of the Map::AddPlayer and Map::RemovePlayer this spec first named, because a session may not reach the world that owns it and Map holds no sessions): a session marks that its wizard arrived, with its public WizClientObject encoded once under the Public mask, and when it left; each tick the world takes the departures first, sending MSG_REMOVEOBJECT to every wizard still in the instance, then shows each newcomer and the wizards already there to each other with MSG_NEWOBJECT, each followed by the other's last move and its state when it is moving, two newcomers of one tick meeting once and a wizard never shown to or taken from a session holding the same wizard
- src/server/game/Entities/Player/MovementRelay and World::FlushMovement: at each Zone.MoveFlushInterval flush every wizard's new move goes to the others in its instance as MSG_SERVERMOVE under its mobile id, as its client packed it, and its movement state as MSG_MOVESTATE under its global id when that changed
- Idle detection: a wizard whose client sends no new move for Zone.MoveIdleIntervals flushes (default 2) is shown standing, MSG_MOVESTATE NewState=0 once; this is the relay's own view, kept apart from the state the client said, so its next move or state shows the client's own again
- Zone.MoveFlushInterval and Zone.MoveIdleIntervals are live settings applied from the next flush
- Jumps: the client handles no MSG_JUMP from the server. A jump is the player object's Jumping state, which the world tells the other wizards in the instance with MSG_ENTERSTATE at its next tick, and the jumper's own client too when its MSG_JUMP did not set ExcludeOriginator; src/server/game/Entities/Player/PlayerStates names the state

**Client messages:** MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE, MSG_CLIENTMOVESTATE, MSG_JUMP, MSG_CLIENTMOVE

**Acceptance**

- [x] Two real clients A and B log into WC_Hub: each sees the other's wizard with correct name and gear. Earned on 2026-09-28 by the client driver's two-wizards.json on r806919 (run 20260928-203426), which starts a second client, the companion, on an account and a wizard of its own: the main wizard backs away from the Commons' Start, the companion enters there, and the game server logs that the two were shown to each other. The main client's shot shows Adrian AshBloom under his name in the yellow hat, robe and boots his row gives him, and the companion's, once it has turned around, shows Adam AngleBane in blue. `PlayerMeetingsTest` holds who is shown to whom, including two wizards arriving in one tick meeting once.
- [x] A walks in a circle: B sees smooth motion with the run animation, and A stops animating within about half a second of stopping. The same run holds W and D together on the companion for 2.5 seconds, one full circle at the 2.47 rad/s turn an earlier run's saved facings measured, while the main client is filmed about every 0.4 seconds: the frames show the companion running along the circle in the run pose, still running 0.3 seconds after the keys are let go and standing 0.7 seconds after, a span that includes the capture's own delay and the client's interpolation. The client's MSG_MoveState handler, read in Ghidra, queues the state with the moves: 1 makes WizardNetworkMovementBehavior carry the wizard on along its last heading until a newer move arrives and 0 stops it at the last move, so a stopped wizard would slide on unless the idle state goes out, which `MovementRelayTest` holds.
- [x] A jumps: B sees the jump. The client has no handler for an MSG_JUMP from the server: the tool finds none registered, and GamebryoClient::HandleEmoteJump plays the Jump animation on the jumper's own object after sending MSG_JUMP with ExcludeOriginator set. Another client plays it when told the jumper's object entered Jumping, the state the player's state set, StateData/PlayerMobileStates.xml, keeps in its Jump category with the Jump animation and a return to NotJumping after two seconds; MSG_ENTERSTATE names a state by the client's string hash, which a retail capture confirms, its NotShopping arriving as 1685237158 (`PlayerStatesClientTest`). The run logs the companion's jump shown to one other wizard, and in run 20260928-202543, on the same server build, the main client's frame 0.4 seconds into the jump shows the companion crouched mid-jump and the next one standing again.
- [x] A logs out: A's model vanishes on B's screen at once. The companion's client quits: the game server logs the companion taken away from the one wizard still in the instance, and the main client's shot a second later shows no Adrian AshBloom.
- [x] A logs back in quickly and gets a new mobile id: B sees one A, not a ghost. The run raises Zone.MobileIdReleaseDelay to a minute so a returning wizard cannot take its old id back, and the scenario rejects the old one: the companion returns with mobile id 49154 where it had 49153, is shown to the main wizard again, and the main client's shot shows one Adrian AshBloom where he stood.
- [x] Unit: changing Zone.MoveFlushInterval or Zone.MoveIdleIntervals applies from the next flush without a restart. `MovementRelayTest.TheFlushClockTakesAChangedIntervalFromTheNextFlush` and `TheIdleCountAppliesFromTheNextFlush`; the world reads both settings at each tick.

**Risks**

- The capture shows MSG_SERVERMOVE and MSG_MOVESTATE in equal counts (3141 each), each MSG_MOVESTATE there with NewState 0 for a creature; the client keeps the last state it was sent, so the server sends a player's only when it changes
- A client asks with MSG_RIDERSLIST for the riders of another wizard once it sees one; the answer loads a RidableBehavior's rider list, and no milestone answers it yet, so two-wizards.json expects it unanswered
- Public versus authority property masks for other players' objects come from WIZ. A wrong mask leaks private data or crashes the viewer.

## 6.02 PLAYERWIZBANG broadcast (WIZ-1 remainder)

**Goal:** Spellbook wizbang seen by others.

**Size:** S. **Depends on:** 6.01

**Client messages:** MSG_PLAYERWIZBANG, MSG_WIZBANG

**Acceptance**

- [x] Real client: a wizbang the server relays for a wizard draws over it for the others and clears, and opening and closing the spellbook relays SpellbookWizbang's id and then 0, which r806919 draws nothing for since its WizBangs.xml holds no spellbook marker (see Resolved in doc/ROADMAP.md) [client driver run 20260930-215125 with StartQuest's hash in place of SpellbookWizbang's: the quest-start marker over the companion in 20-main-sees-spellbook-wizbang.png, none in 16-main-sees-companion.png or 24-main-sees-spellbook-wizbang-cleared.png; run 20260930-200510 relayed 1288150110 and then 0 to both wizards]

### Detailed spec from WIZ-1: Wizard service dispatch skeleton and login chatter

A character entering the world gets no errors or unknown-message spam from the WIZARD requests the client sends right after login, and every WIZARD message is either handled or deliberately listed as not yet handled.

**Deliverables**

- src/server/game/Handlers/WizardHandler.cpp: register the handlers in the session dispatch table (state: in world)
- src/server/game/Handlers/WizardHandler.cpp: minimal replies. GETTIMEDACCESSPASSES -> empty TIMEDACCESSPASSES; GETSUBSCRIBERONLYITEMS -> empty SUBSCRIBERONLYITEMS; CROWNBALANCE -> TotalCrowns=0; DONESHOPPING, LOGCLIENTRESOLUTION, LOGPATCHCLIENTPATCHTIME and QUESTFINDEROPTION accepted and logged at debug
- PLAYERWIZBANG handler: broadcast GAME MSG_WIZBANG to the zone (StateName SpellbookWizbang -> a non-zero WizBangID; any other state -> 0)
- src/server/shared/Messages: an explicit list of unhandled WIZARD/WIZARD2/WIZARD3 messages that logs once per session, not per packet
- src/test/server/game/WizardDispatchTest.cpp

**Client messages:** MSG_LOGCLIENTRESOLUTION, MSG_LOGPATCHCLIENTPATCHTIME, MSG_GETTIMEDACCESSPASSES, MSG_TIMEDACCESSPASSES, MSG_GETSUBSCRIBERONLYITEMS, MSG_SUBSCRIBERONLYITEMS, MSG_CROWNBALANCE, MSG_DONESHOPPING, MSG_PLAYERWIZBANG, MSG_QUESTFINDEROPTION, MSG_SHOWCLIENTMESSAGEBOX, MSG_SHOWGUI

**Data sources**

- Root.wad WizardMessages.xml, WizardMessages2.xml, WizardMessages3.xml (read at build/run time by the message generator, not committed)

**Acceptance**

- [x] Unit test: every name from all three Wizard XML files maps to exactly one entry, either a handler or the unhandled list; MSG_PETHATCHREADYSTATUS maps to one order (`GameMessageTableClientTest.EveryWorldMessageHasExactlyOneRuleAndTheEntryChatterIsHandled`)
- [x] Unit test: WIZARD message order is the 1-based index in the de-duplicated alphabetical list, with UPDATEMANA=233 and ADDSPELLTOBOOK=10 as fixtures (`GameMessageTableClientTest.WizardOrdersAreThePlacesOfTheirTagsSortedWithoutRepeats`)
- [x] Real client: log in to a zone. The server log shows GETTIMEDACCESSPASSES, GETSUBSCRIBERONLYITEMS, LOGCLIENTRESOLUTION and DONESHOPPING handled with no warnings. Opening and closing the spellbook relays the wizbang to other clients in range and clears it; a relayed marker draws over the wizard and clears, and the spellbook's own draws nothing on r806919, which has no book marker (see Resolved in doc/ROADMAP.md) [client driver runs 20260930-200510 and 20260930-215125, as in the check above]

**Client investigation (r806919):** `client messages` reports WIZARD `MSG_PLAYERWIZBANG` carries `StateName` as STR, and GAME `MSG_WIZBANG` carries `GameObjectID` as GID and `WizBangID` as UINT; its description says the message puts an icon above an object. Decompiling `GameClient::MSG_WizBang` at `0x141708770` confirms it resolves `GameObjectID` and passes `WizBangID` unchanged to the object's virtual slot at `+0x1D0`. `ShowWizBangOf` sends the same world GUID used for the already-shown wizard's movement relays; the two-client run 20260930-200510 showed and moved that companion, so current evidence does not support changing `GameObjectID`. `WizardGUIManager::GetSpellbookPrefsWindow` at `0x140dd4df0` sends `StateName` `SpellbookWizbang`. `WizardGraphicalClient::Initialize` at `0x141329290` registers `SetWizbang` to `0x141361bc0`; that action maps selectors 0-3 to 0 and the client hashes of `StartQuest`, `UnfinishedQuest`, and `CompleteQuestGoal`, then calls the same virtual slot. The client's `WizBangs.xml`, loaded through `WizBangTemplateManager::Initialize` at `0x1417be3e0`, has no `SpellbookWizbang` template. Slot `+0x1D0` is SetWizBang at `0x141755700`, shared by eight object classes' tables: it looks the id up through `0x1417be550` in the template list of the WizBangTemplateManager singleton at `0x1433a9620`, matching each template's id field after its name and model, and keeps the marker it finds. Client driver run 20260930-215125 sent StartQuest's hash, 660791182, in place of SpellbookWizbang's, and the quest-start marker drew over the companion and cleared, so ids are the string hashes of template names, and `client wizbangs`, taught for this, lists the 33 r806919 holds and says SpellbookWizbang draws nothing. The server keeps relaying SpellbookWizbang's own hash. Run 20260930-200510 completed the full scenario without `Setup.SchemaProbeTimeout=1`, passed all driver checks and captured 41 screenshots; its server log recorded ID 1288150110 and then 0 for both wizards, while the two main-client screenshots named above showed no marker over the companion. The standard `client decompile` invocation fails on Ghidra 12.1.4 because its headless launch does not start PyGhidra; the decompilation above used PyGhidra against the local analyzed program.

**Risks**

- The duplicate PETHATCHREADYSTATUS element changes every order after position 122. Only UPDATEMANA was checked against a live capture, so check more high-order messages (for example UPDATEGOLD=231) against a real client.
- The real response formats for TIMEDACCESSPASSES and SUBSCRIBERONLYITEMS are unknown. An empty Data string is untested.

## 6.03 Say chat, quick chat, core emotes (WIZ-4)

**Goal:** Chat bubbles and emotes in range.

**Size:** M. **Depends on:** 6.01, 4.02, 4.15, 4.16

**Client messages:** MSG_REQUESTRADIALCHAT, MSG_RADIALCHAT, MSG_REQUESTRADIALQUICKCHAT, MSG_RADIALQUICKCHAT, MSG_REQUESTRADIALQUICKCHATEXT, MSG_RADIALQUICKCHATEXT, MSG_CORE_EMOTE

**Acceptance**

- [x] Command-prefixed messages are never broadcast
- [x] B sees A's 'hello' bubble, quick chat and wave
- [x] Prefix and range changes apply to the next message; a failed `.reload quickchat` keeps the old IDs

### Detailed spec from WIZ-4: Say chat, quick chat and core emotes

Players in the same zone can see each other's typed chat bubbles, quick-chat lines and emote animations.

**Deliverables**

- src/server/game/Handlers/ChatHandler.cpp: REQUESTRADIALCHAT -> RADIALCHAT to players in range, REQUESTRADIALQUICKCHAT -> RADIALQUICKCHAT, REQUESTRADIALQUICKCHATEXT -> RADIALQUICKCHATEXT
- CORE_EMOTE handler: echo to range, respecting ExcludeOriginator
- src/server/game/Chat/ChatMgr: range filter on Chat.SayRange, blocks senders on the listener's ignore list (the list itself comes in WIZ-17), and routes text starting with GM.CommandPrefix (default '.') to the command system instead of broadcast. Both are live settings applied to the next message.
- `.reload quickchat` rebuilds the quick-chat ID set from QuickChat.xml off to the side, validates it, and swaps it; a failure keeps the old set and reports every error
- src/test/server/game/ChatHandlerTest.cpp

Built as src/server/game/Chat/ChatMgr, ChatText and QuickChatMgr, src/server/game/Entities/AnimationListMgr, src/server/shared/Characters/PackedName, src/server/game/Server/SpeechMessages, src/server/game/World/SpeechRelay and src/server/game/Handlers/ChatHandler.cpp, with the test in src/test/server/game/Handlers/ChatHandlerTest.cpp. A command line is never shown; 6.04 hands it to CommandMgr. The listener's ignore list arrives with 12.02, whose checks cover the suppression. The radial menu's emotes stay with 12.14. The wire formats are settled under Chat and emotes in doc/ARCHITECTURE.md.

**Client messages:** GAME MSG_REQUESTRADIALCHAT, GAME MSG_RADIALCHAT, GAME MSG_REQUESTRADIALQUICKCHAT, GAME MSG_RADIALQUICKCHAT, GAME MSG_REQUESTRADIALQUICKCHATEXT, GAME MSG_RADIALQUICKCHATEXT, GAME MSG_CORE_EMOTE

**Data sources**

- Root.wad QuickChat.xml (root QuickChatEntry, m_chatID) for validating quick-chat IDs

**Acceptance**

- [x] Unit test: a message starting with the command prefix is never broadcast. `ChatMgrTest.ATypedLineIsShownUnlessItIsACommandOrDoesNotRead` judges `.help` a command under the prefix `.`, and the handler keeps nothing it judges a command. In the client driver's say-and-emote.json run 20260929-173917 on r806919, the main wizard typed `.help`. The game server logged "typed a command, which is never shown to anyone", and the companion's shot shows no bubble.
- [x] Unit test: RADIALCHAT SourceName is the wizard's name blob and SourceID the player's GameObject GID. `SpeechMessagesTest.EachMessageNamesTheSpeakerAndCarriesWhatItDid` covers SourceName, SourceID and Filter. `PackedNameTest` holds the blob to the layout the client's own `WizardNameCodec::PackName` and `UnpackName` give, read in Ghidra, with the locale always the reader's own. In the same run the companion's chat log names the speaker Adam AngleBane, as its nameplate does. An earlier run, which sent the stored locale, showed the German table's Adrian AscheAst.
- [x] Unit test: changing GM.CommandPrefix or Chat.SayRange applies to the next message without a restart, and `.reload quickchat` with an unreadable QuickChat.xml keeps the old ID set and reports the error. The handler judges each line against the prefix CommandMgr holds when the line arrives, and the world reads Chat.SayRange at each tick. `ChatMgrTest` shows a changed prefix deciding the next line, `SpeechRelayTest` shows a range deciding who hears, and `QuickChatMgrTest.AFailedReloadOfQuickChatKeepsTheOldPhrasesAndReportsTheError` keeps the serving phrases through a reload of an unreadable file. In the same run, `settings set Chat.SayRange 1` on the game server console made the next line reach 0 wizards. `settings set GM.CommandPrefix !` made the next `.help` a line shown to the companion and `!help` a command. Both took effect with nothing restarted.
- [x] Two real clients in one zone: A types 'hello'. B sees a speech bubble over A and a line in the chat log. A picks a quick-chat phrase and B sees it. A clicks the wave emote and B sees A wave. Run 20260929-173917, with two-wizards.json passing on the same build as run 20260929-174313:
  - Typed hello is a menu chat phrase, so the client sent it as quick chat phrase 270, Hello, which plays the Wave emote. The companion's shot shows Hello over the main wizard with its arm raised, and `[Adam AngleBane] ** Hello` in its chat log.
  - A line no phrase matches, `hello from the Commons`, went out as typed chat. The companion shows it as a bubble and a chat log line, and the talking emote the line plays was shown to it too.
  - From the quick chat menu, the main wizard picked Yes, which the companion shows.
  - It then chose the Hello/Goodbye category and picked Hi, the phrase marked with the waving hand. The companion was filmed every 0.4 seconds and shows the main wizard waving from 1.6 to 2.8 seconds after the press.
  - The emote wheel, which opens from the menu's Emotes entry, belongs to 12.14 with the radial menu's own messages.

**Risks**

- The capture shows REQUESTRADIALCHAT Message bytes as UTF-16 even though the XML types the field STR. The codec must treat it as a length-prefixed byte string and pass the payload through untouched; the exact encoding is unverified. Resolved in 6.03: a 16-bit count of UTF-16 units, then the units, as the client writes a wide string. The server passes the bytes on unchanged once they read that way.
- The meaning of the Filter byte (the reference sends 2 for typed chat and 0 for quick chat) is unverified. Resolved in 6.03: it is the chat level the speaker's own client shows its own line at, 2 with open chat, otherwise 1 with menu chat, otherwise 0. The client uses the same rule for quick chat.
- In the capture the client sends CORE_EMOTE Name=Chat together with each typed line. It must not be treated as a real emote request. Resolved in 6.03: it is the talking gesture the speaker's own client plays with every line. Others see it as the Emoting state with the Chat animation, and it adds no line to anyone's chat.

## 6.04 GM commands in chat (WIZ-5 remainder)

**Goal:** Chat prefix routes to CommandMgr; cs_gm/cs_character/cs_lookup.

**Size:** M. **Depends on:** 6.03, 5.01, 4.16

**Client messages:** MSG_COMMAND, MSG_COMMANDRESULT, MSG_SERVERMESSAGE, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [x] GM '.help' lists commands with no bubble for others
- [x] Player account text is chat or refused per config
- [x] GM.LogCommands changes apply from the next command

### Detailed spec from WIZ-5: Account security levels and GM command framework

An account's security level decides which chat-prefixed GM commands it may run, and the first cs_ command groups give testers a harness for every later milestone.

**Deliverables**

- data/sql/updates/db_login: account_access (account_id, realm_id, security_level) with levels PLAYER=0, MODERATOR=1, GAMEMASTER=2, ADMINISTRATOR=3, CONSOLE=4 (AzerothCore precedent)
- src/server/game/Chat/CommandMgr: CommandScript tables, argument parsing, security check against the command's default level or its command_security override, per-command help
- src/server/scripts/Commands/cs_gm.cpp (.gm on/off, .gm visible), cs_character.cpp (.character level, .character gold, .character xp, .character heal), cs_lookup.cpp (.lookup item / spell by name)
- Replies via SYSTEM MSG_SERVERMESSAGE or GAME MSG_CLIENTNOTIFYTEXT
- Set LOGINCOMPLETE IsCSR and Permissions from the security level (coordinate with NET/LOG)
- gameserver.conf.dist: GM.CommandPrefix, GM.LogCommands, live settings applied from the next command
- src/test/server/game/CommandMgrTest.cpp

Built on 4.02's CommandMgr, account levels and command_security. GameSession::TakeCommandLine in src/server/game/Handlers/ChatHandler.cpp runs a command line for an account above player level. It sends the reply back as EXTENDEDBASE MSG_SERVERMESSAGE and hides the talking emote the client sends with the line. ChatMgr::FateOf decides what happens to a player's line under the live setting GM.PlayerCommandsAsChat. cs_misc.cpp gained the in-game `help`. The settled rules are under Commands in chat in doc/ARCHITECTURE.md.

MSG_CLIENTNOTIFYTEXT is not a reply channel: its text is a locale key. The client sends MSG_COMMAND only from its customer service windows, and MSG_COMMANDRESULT fires a script event for them.

Four deliverables wait on other work:
- `.gm on`, `off` and `visible` stay unavailable until visibility sets arrive in 6.15 and 6.16, because hiding a game master means leaving it out of them.
- `.character heal`, like the level, gold and experience commands, waits for the milestone that sends a wizard's changed stats to its client.
- A spell is looked up by name with `spell info <text>`, which already lists every spell whose name holds the text. A second command would repeat that search. Looking up an item waits for item templates.
- LOGINCOMPLETE `IsCSR` has followed the security level since 4.x, and an account's own `Permissions` come with 6.05.

**Client messages:** GAME MSG_COMMAND, GAME MSG_COMMANDRESULT, SYSTEM MSG_SERVERMESSAGE, GAME MSG_CLIENTNOTIFYTEXT

**Database tables**

- login.account_access
- world.command_security
- characters.gm_command_log (optional)

**Acceptance**

- [x] Unit test: a PLAYER-level account running a GAMEMASTER command gets 'no such command' and nothing executes. `CommandMgrTest.AnAccountBelowACommandsLevelIsToldThereIsNoSuchCommand`, from 4.02. In chat, `ChatMgrTest.AStaffAccountRunsACommandLineAndAPlayersIsSaidOrRefused` shows that a player-level account runs no command line at all.
- [x] Unit test: the command table parses '.character gold 500' into (character, gold, [500]). `CommandMgrTest.ACommandIsTheDeepestNameThatMatchesAndTheRestAreArguments` and `TheClientsPrefixIsTakenOffBeforeTheWordsAreRead`, from 4.02.
- [x] Unit test: turning GM.LogCommands off stops logging from the next command without a restart. `CommandMgrTest.TurningTheCommandLogOffStopsItFromTheNextCommand` logs one command, drops the next once logging is off, and logs again once it is back on. The game server applies a GM.LogCommands change to CommandMgr as the change arrives.
- [x] Real client, GM account: typing '.help' shows the command list in the chat window, and nearby players see no bubble. The same text from a player account shows up as normal chat or is refused, depending on config. The client driver's gm-commands-in-chat.json run 20260929-194726 on r806919 made the main account a game master on the login server's console before two wizards entered the Commons.
  - The game master's `.help` was run and logged. Its client's chat window shows the command list as Server Message lines, and the companion, a player, shows no bubble over it. No talking emote from the game master was passed on.
  - The companion's own `.help` was said as a line, which the game master's shot shows over the companion.
  - After `settings set GM.PlayerCommandsAsChat false` on the game server's console, the companion's next `.help` was refused, and its chat window says so.
  - With `GM.LogCommands` off, `.help server` still answered but was not logged. Once the setting was reset, the next command, `.help kick`, was logged again.
  - `ChatHandlerTest.AGameMastersCommandRunsAndItsRepliesComeBackAsServerMessages` holds the reply path over loopback, one MSG_SERVERMESSAGE for a two-line reply.

**Risks**

- Whether the stock client ever sends GAME MSG_COMMAND itself is unverified. The capture only shows '.mod ...' arriving as REQUESTRADIALCHAT. Resolved in 6.04: the client sends MSG_COMMAND only from its customer service CharacterEditWindow, so a typed command always arrives as MSG_REQUESTRADIALCHAT.
- The bit meanings of LOGINCOMPLETE Permissions are unverified. The reference sends a constant 207 (0b11001111).

## 6.05 GM account, ban, character commands (LOG-17)

**Goal:** Manage accounts without SQL.

**Size:** M. **Depends on:** 6.04, 3.17, 4.15

**Client messages:** MSG_FORCE_DISCONNECT, MSG_SERVERMESSAGE

**Acceptance**

- [x] Lower security levels refused (GmAccountCommandTest.EachCommandRefusesAnAccountBelowItsLevel)
- [x] A command_security override applies after `.reload command_security` without a restart (GmAccountCommandDatabaseTest.RaisingBanAccountAndReloadingRefusesAGameMasterWithNothingRestarted)
- [x] `ban account test 1h spam` disconnects and blocks next login; unban restores (GmAccountCommandDatabaseTest.ABanBlocksTheAccountAndUnbanLiftsIt; client driver run 20261001-184159, gm-ban-and-restore.json: the game master's `.ban account clientdriver2 1h spam` kicked the companion's session with This account is banned: spam, though the kicked client did not visibly show the disconnect, which the ban TimeStamp hang the Server thread is fixing is suspected of, and its next login was refused with MSG_USER_AUTHEN_RSP Error=AccountBanned and the client showed the suspension notice, and after `.unban account` it was let back in)
- [x] `character deleted restore <guid>` brings the wizard back (GmAccountCommandDatabaseTest.ADeletedWizardIsRestoredToTheAccountItWasDeletedFrom; client driver run 20261001-184159, gm-ban-and-restore.json: after its owner's delete the companion's list held 0 characters, and after `.character deleted restore 2` and a relog it held 1 and the select screen showed the wizard)

### Detailed spec from LOG-17: GM account and character commands

Operators can manage accounts, bans, security levels and deleted characters from the gameserver console or in-game chat, without editing SQL by hand.

**Deliverables**

- src/server/scripts/Commands/cs_account.cpp: account create, delete, set password (revokes account_session), set gmlevel, lock, unlock, onlinelist
- src/server/scripts/Commands/cs_ban.cpp: ban account, ban ip, ban machine, unban, baninfo
- src/server/scripts/Commands/cs_character.cpp: character deleted list, character deleted restore, character rename flag (sets should_rename)
- Security levels mapped to account.security_level; command tables declare their default required level, and a world.command_security row overrides it live through `.reload command_security`
- An account's own permissions: account.permissions, NULL keeping `LoginComplete.Permissions`, set with `account set permissions`. They go in MSG_LOGINCOMPLETE and in the name behavior's `m_chatPermissions`, whose chat bits choose the mark other clients draw beside the wizard's name (settled in 6.03), so an account's chat level shows where its players see it. Added on 2026-09-29 at the maintainer's direction.

**Client messages:** MSG_FORCE_DISCONNECT, MSG_SERVERMESSAGE

**Database tables**

- account
- account_banned
- ip_banned
- machine_banned
- account_session
- characters

**Acceptance**

- [x] Unit: each command's permission check refuses a lower security level (GmAccountCommandTest.EachCommandRefusesAnAccountBelowItsLevel)
- [x] Unit: a world.command_security row raising '.ban account' to ADMINISTRATOR, then `.reload command_security`, refuses a GAMEMASTER account without a restart (GmAccountCommandDatabaseTest.RaisingBanAccountAndReloadingRefusesAGameMasterWithNothingRestarted)
- [x] Real client: `ban account test 1h spam` from a GM character disconnects the target, whose next login attempt is refused; `unban` lets them back in (client driver run 20261001-184159, gm-ban-and-restore.json: typed in game chat, `.ban account clientdriver2 1h spam` kicked the companion with disconnect type 87620544, This account is banned: spam, though its client still showed it standing in the world 2 s later, which the ban TimeStamp hang the Server thread is fixing is suspected of; its next login was refused with Error=AccountBanned; `.unban account clientdriver2` let it log in again)
- [x] Real client: `character deleted restore <guid>` makes a deleted wizard reappear on its owner's select screen after a relog (client driver run 20261001-184159, gm-ban-and-restore.json: the deleted wizard was gone from the companion's list, and after `.character deleted restore 2` and a relog the select screen showed it again)
- [x] Unit: an account's own permissions reach MSG_LOGINCOMPLETE and the name behavior, and an account without them gets `LoginComplete.Permissions` (GmAccountCommandTest.AnAccountsOwnPermissionsTakeThePlaceOfTheSetting chooses the account's own over the setting, and PlayerObjectBuilderTest.TheWizardsObjectCarriesItsHeaderIdsPlaceAndBehaviorsInTemplateOrder puts what it is given in the name behavior's m_chatPermissions of the object MSG_LOGINCOMPLETE carries)
- [x] Real client: an account set to menu chat only shows the filtered balloon beside its wizard's name on another client, and one with open chat shows no mark (client driver run 20261001-184159, gm-ban-and-restore.json: the companion's account, set to 0x23 on the console, showed the filtered balloon beside its wizard's name on the game master's client; client driver run 20261001-184643, gm-teleport.json: the game master's account with the default open chat showed no mark beside its name on the companion's client)

**Risks**

- It depends on the command framework milestone of whichever domain owns ScriptMgr and CommandScript (guessed prefix CMD, which is not in the listed prefixes; possibly FND or WLD).

## 6.06 Same-zone teleport and cs_tele (WLD-12)

**Goal:** GM teleport seen by onlookers.

**Size:** S. **Depends on:** 6.01, 4.02, 4.15

**Client messages:** MSG_SERVERTELEPORT, MSG_COMMAND, MSG_COMMANDRESULT

**Acceptance**

- [ ] '.tele Start' snaps without loading; B sees it
- [ ] '.gps' matches minimap
- [ ] Out-of-range '.go xyz' refused; players cannot '.tele'
- [ ] A point from '.tele add' works at once without a reload

### Detailed spec from WLD-12: Same-zone teleport and GM teleport commands

A GM can instantly move themselves or another player to a named location or coordinates in the current zone, and onlookers see it.

**Deliverables**

- src/server/game/Entities/Player::TeleportWithinMap: update position and send MSG_SERVERTELEPORT (packed location, MobileID) to self and viewers
- src/server/scripts/Commands/cs_tele.cpp: '.tele <location name>', '.go xyz <x> <y> <z> [yaw]', '.gps' (zone, x, y, z, yaw, dynamic zone id), with security levels
- world.game_tele table (named GM teleport points: name, zone, x, y, z, yaw) and '.tele add/del', which update the live list and the table at once and are journaled as a pending SQL update
- `.reload game_tele` swaps in hand-edited rows; a failure keeps the old list and reports every error

**Client messages:** MSG_SERVERTELEPORT, MSG_COMMAND, MSG_COMMANDRESULT

**Data sources**

- world.zone_location

**Database tables**

- world.game_tele

**Acceptance**

- [ ] Real client: '.tele Start' in WC_Hub snaps the wizard to the Start fountain with no loading screen, and B sees A pop to the new spot
- [ ] Real client: '.gps' prints coordinates matching the minimap position
- [ ] Unit: '.go xyz' outside the packable range is refused with a message
- [ ] A player-level account cannot run '.tele'
- [ ] Real client: a point added with '.tele add' works immediately without a reload or restart

**Risks**

- Direction handling in MSG_SERVERTELEPORT carries the same unverified byte scale as WLD-1

## 6.07 Cross-zone transfer via MSG_SERVERTRANSFER (WLD-13)

**Goal:** Retail loading-screen zone change.

**Size:** M. **Depends on:** 6.06, 4.13

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_ZONETRANSFERNACK, MSG_SERVERTRANSFER, MSG_RETRYTELEPORT, MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Acceptance**

- [ ] '.tele zone WizardCity/WC_Ravenwood' loads Ravenwood 'Start'; B in WC_Hub sees A disappear
- [ ] Bad zone path gives an error and the player stays
- [ ] Double request ignored; NACK clears; transfer key works once

### Detailed spec from WLD-13: Cross-zone transfer via reconnect (MSG_SERVERTRANSFER)

Players can move between zones with the retail loading-screen flow, triggered here by a GM command.

**Deliverables**

- src/server/game/Handlers/ZoneHandler.cpp: Player::TransferToZone sends MSG_ZONETRANSFERREQUEST (SendAck=1), waits for MSG_ZONETRANSFERACK or MSG_ZONETRANSFERNACK, removes the player from the Map, saves position, issues a one-time attach key, sends MSG_SERVERTRANSFER (IP, ports, Key, UserID, CharID, ZoneName, Location, TransitionID, Fallback* set to the current zone)
- Transfer-queued guard (no double transfers), MSG_RETRYTELEPORT re-sends the last transfer, and fallback so a MSG_ATTACHFAILED or timeout returns the player to the Fallback zone
- '.tele zone <zone path> [location]' in cs_tele.cpp
- TransitionID chosen from ZoneTPTrans.xml (loading-screen art) when present, else 1

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_ZONETRANSFERNACK, MSG_SERVERTRANSFER, MSG_RETRYTELEPORT, MSG_ATTACH, MSG_ATTACHFAILED, MSG_LOGINCOMPLETE

**Data sources**

- <zone>.wad/ZoneTPTrans.xml (in 171 zones, e.g. 'Krokotopia World Transition' with a loadscreen jpg)

**Database tables**

- characters.characters
- login.account_session

**Acceptance**

- [ ] Real client: '.tele zone WizardCity/WC_Ravenwood' shows the loading screen and the player arrives at Ravenwood 'Start'; B in WC_Hub sees A disappear
- [ ] Real client: transferring to a zone path that does not exist gives an error message and the player stays put
- [ ] Unit: a second transfer request while one is queued is ignored; a NACK clears the queue
- [ ] Unit: the attach key issued for the transfer works exactly once

**Risks**

- This is the path the behavior reference uses for every zone change and it is known to work; the cheaper same-connection MSG_ZONETRANSFER is WLD-21
- Key is INT in MSG_SERVERTRANSFER but STR in MSG_CHARACTERSELECTED. How the client carries it into MSG_ATTACH.LoginKey is unverified.

## 6.08 Logout, link-dead, AFK, shutdown (WLD-20)

**Goal:** Clean world exit in all cases.

**Size:** M. **Depends on:** 6.01, 4.16

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK, MSG_NOT_AFK, MSG_SERVERSHUTDOWN, MSG_REMOVEOBJECT, MSG_ATTACH

**Acceptance**

- [x] Quit removes A from B at once; relog same spot (two-wizards-menu-logout.json, client-driver run 20260929-170915: the companion was saved at (-28, 64, -24), taken out of the main wizard's instance at once, and a fresh login came back at exactly that spot)
- [x] Killed process: B sees A stand then vanish; reattach within window resumes (two-wizards-link-dead-expiry.json, run 20260929-160700: the resumed wizard kept mobile id 49153 with no ghost, and after a second kill it was removed when its 300-second link-dead time ran out)
- [x] Server stop shows notice; second attach kicks the first (game-server-shutdown-and-return.json, run 20260929-163330, showed the maintenance notice and brought the wizard back at its saved spot; GameSessionLifecycleTest.ReplacementAttachTakesOverTheExistingCharacterPlacement)
- [x] Changing Player.AfkTime applies from the next AFK timer (GameSessionLifecycleTest.NotAfkResetsTheWarningAndLiveAfkTimeControlsDisconnect)

### Detailed spec from WLD-20: Logout, disconnect, link-dead and server shutdown

Players leave the world cleanly on logout, crash or server stop, their position is saved, and others see them go.

**Deliverables**

- Handlers: MSG_QUERY_LOGOUT (reply per IsInstance), MSG_CLIENT_DISCONNECT (save and remove immediately)
- Socket loss without logout: keep the player in the Map as link-dead for Player.LinkDeadTime and send MSG_ZOMBIE_PLAYER (GlobalID, Remaining) to viewers; a reattach in that window resumes the session (MSG_ATTACH Reattach=1)
- AFK: MSG_DISCONNECT_AFK warning, then disconnect after Player.AfkTime; MSG_NOT_AFK resets the timer
- World shutdown: MSG_SERVERSHUTDOWN to all sessions, save all positions, then close
- conf/dist/gameserver.conf.dist: Player.LinkDeadTime, Player.AfkWarnTime, Player.AfkTime, live settings applied from the next timer

**Client messages:** MSG_QUERY_LOGOUT, MSG_CLIENT_DISCONNECT, MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK, MSG_NOT_AFK, MSG_SERVERSHUTDOWN, MSG_REMOVEOBJECT, MSG_ATTACH

**Database tables**

- characters.characters

**Acceptance**

- [x] Real client: Quit from the menu on client A removes A from B's view at once, and A relogs at the same spot (two-wizards-menu-logout.json, run 20260929-170915: wizard 2 was saved and removed at once, then entered again at exactly (-28, 64, -24); the scenario ends the client once the server confirms the logout, because the client otherwise retries its spent zone ticket)
- [x] Kill A's process: B sees A stand still, then vanish after the link-dead time; relogging within it resumes without a duplicate (two-wizards-link-dead-expiry.json, run 20260929-160700)
- [x] Server stop: connected clients get the shutdown notice and a relog after restart puts them where they were (game-server-shutdown-and-return.json, run 20260929-163330: the client showed the maintenance notice, the server saved (1684, -752, 0), and the login after the restart entered there)
- [x] Unit: two sessions for one character: the second attach kicks the first cleanly (GameSessionLifecycleTest.ReplacementAttachTakesOverTheExistingCharacterPlacement)
- [x] Unit: changing Player.AfkTime or Player.LinkDeadTime applies from the next timer without a restart (GameSessionLifecycleTest.LinkDeadDeadlineUsesTheLiveSettingOnTheNextWorldTimer and GameSessionLifecycleTest.NotAfkResetsTheWarningAndLiveAfkTimeControlsDisconnect)

**Risks**

- The semantics of Reattach and Retry in MSG_ATTACH are unverified
- The payload of MSG_SERVERSHUTDOWN Message (UINT) is unknown

## 6.09 Schema probe for classes missing from the dump (OBJ-11)

**Goal:** Name and type unknown hashes via the hash oracle.

**Size:** M. **Depends on:** 3.11

**Acceptance**

- [x] Lists 1451865413 (7094), 520243970 (6875), 1120896859 (5567), 829470368 (5473). `schemaprobe` against the pinned install lists all four with exactly those instance counts, each under m_behaviors
- [x] 520243970 gets m_behaviorName, m_npcProximity, m_questList, m_personaName. `BindFileClientTest.EachPropertyOfAnUnknownClassIsListedAndTheOracleNamesTheNpcBehaviors` on the pinned install: the sweep lists the five properties every one of the 6875 NPC behaviors holds, and the property oracle, from the dump's own 641 types and 4758 names, names four of them m_behaviorName:std::string, m_npcProximity:float, m_questList:std::string and m_personaName:std::string
- [x] Full Root.wad under 5 minutes. 173088 entries read and 134635 of 134640 BINd files decoded in 32.5 seconds, 361 MiB peak

### Detailed spec from OBJ-11: Schema probe tool for classes missing from the dump

Unknown class and property hashes found in client data can be named and typed by us using the hash formulas as an oracle.

**Deliverables**

- src/tools/schemaprobe: sweeps Root.wad and zone WADs, collects unknown class hashes with instance counts, paths, parent property, and each property's hash and bit-size distribution
- Oracle matching: tests each unknown property hash against every (known property name x known type string) pair from the registry, plus a user-supplied candidate list; tests candidate class names against the class hash
- Output: a report and a draft schema in our own format for OBJ-12, each unknown class with every property its objects hold, their bit sizes and the oracle's names, and a draft of the properties the oracle names one way only; headerless versionable objects such as a zone's gamedata.bin are swept too, and the oracle lives in src/server/shared/ObjectProperty/PropertyOracle; classes are named from the client program's strings through client-image's ProgramStrings::ClassNames

**Acceptance**

- [x] Client-gated run lists at least the top unknowns found in my sweep: 1451865413 (7094 instances), 520243970 (6875), 1120896859 (5567), 829470368 (5473), all under ObjectData m_behaviors, plus Result/Requirement subclasses under ResultList.m_results and RequirementList.m_requirements. `schemaprobe` against the pinned install on 2026-09-26 lists 104 unknown classes with every property each holds, the four with exactly those instance counts under m_behaviors, and 57 result and requirement classes under m_results and m_requirements; `BindFileClientTest` holds the NPC behavior's count and one of each kind
- [x] For 520243970, the oracle names m_behaviorName:std::string, m_npcProximity:float, m_questList:std::string and m_personaName:std::string (I reproduced this in Python). `BindFileClientTest.EachPropertyOfAnUnknownClassIsListedAndTheOracleNamesTheNpcBehaviors` on the pinned install: the sweep lists the five properties every one of the 6875 NPC behaviors holds, and the property oracle, from the dump's own 641 types and 4758 names, names four of them m_behaviorName:std::string, m_npcProximity:float, m_questList:std::string and m_personaName:std::string
- [x] Report runtime on the full Root.wad stays under 5 minutes. 32.5 seconds in an optimized build, as the short list records, and 2 minutes 14 seconds in a Debug build with every unknown class's properties listed and named

**Risks**

- Settled: class names come from the client program's own strings rather than brute force. Read as class names, as they stand or with class or struct before them, they name 101 of the 110 unknown classes Root.wad and the Commons' and Ravenwood's archives hold, such as class QuestingBehaviorTemplate for 520243970; the other nine stay hash-named and decode all the same. `SchemaProbeTest.cmake` holds WC_Hub's class MinigameSigilInfo

## 6.10 Supplemental server-side schemas (OBJ-12)

**Goal:** Server-owned classes decode like dump classes.

**Size:** S. **Depends on:** 6.09, 3.03, 4.15

**Acceptance**

- [x] A supplemental class extends a dump base and decodes (TypeRegistryTest.ASupplementClassJoinsTheLoadedDumpInANewGeneration, which round-trips a synthetic versionable object of the supplemental class and checks its inherited base relationship)
- [x] A name/type not hashing to its declared hash is rejected (TypeRegistryTest.ASupplementThatDoesNotHashIsRefusedAndTheCatalogKeepsServing, which rejects the bad property hash and leaves the active catalog unchanged)
- [x] A failed `.reload server_class_schema` keeps the old registry (ObjectSchemaMgrTest.AServerClassThatDoesNotHashFailsItsReloadWithTheCatalogUntouched, run against a database)

### Detailed spec from OBJ-12: Supplemental server-side class schemas

Classes that exist in client data or server logic but not in the client dump decode and encode like any other class.

**Deliverables**

- A supplemental schema format (JSON or conf, our own) of classes with name or hash-only id, bases, and properties (name or hash, type, flags, container), merged into sTypeRegistry after the dump. Every named entry is verified by the hash formula at load
- Location: the world database's server_class, server_class_base and server_class_property tables, settled on 2026-09-25 and recorded in doc/ARCHITECTURE.md; contains no client bytes. The tables, their loader and the registry's rebuild were built early for 4.11, whose player object needs BasicMobileBehavior, and this milestone's checks stay open until 6.09 is done
- Registry reports which classes came from the dump and which from the supplement
- The classes an install's data holds that its dump does not describe are server classes marked `install` in `server_class.source`, found from the install and never committed: `schemaprobe --server-classes` proposes each from the sweep through src/server/shared/ObjectProperty/ServerClassProposer, names it and its properties through the property oracle, reads its base from the list it sits in, and src/server/shared/ClientData/ServerClassExtractor keeps it only once a re-sweep decodes every object of it cleanly, trying a failing new property as a list first. The install's plain-XML object files are swept too through src/server/shared/ObjectProperty/XmlSweep, and a class only they hold, one only the server reads, takes the server's own types from the values they give it by the rule doc/ARCHITECTURE.md records. src/server/shared/ClientData/ServerClassCache keeps the result per client revision in the Ambrose data folder under the build lock the type dump uses, recording the way the classes were found so a file from an earlier way is built again, the game server writes it to the world database through src/server/database/Extraction/ServerClassScript when none marked install is there and replaces them when they differ from the file, and `extractor classes` does the same by hand. An enum property's options live in `server_class_property_option`
- `.reload server_class_schema` merges the edited supplement into a new registry generation off to the side, verifies every hash, and swaps it; a failure keeps the old registry and reports every error

**Acceptance**

- [x] Unit test: a supplemental class extends a dump base class and decodes a synthetic versionable blob (TypeRegistryTest.ASupplementClassJoinsTheLoadedDumpInANewGeneration; `TypeRegistryTest.ASupplementClassDecodesInAListOfTheDumpClassItExtends` writes a supplement class derived from a dump class into a list of that dump class as a BINd file and reads it back as itself with its values, where a catalog without the supplement reports the same bytes as an unknown class, and `ServerClassExtractorTest` does the same through the whole extraction, over an archive the test writes)
- [x] Unit test: a supplemental entry whose name and type do not hash to its declared property hash is rejected (TypeRegistryTest.ASupplementThatDoesNotHashIsRefusedAndTheCatalogKeepsServing; `ObjectSchemaMgrTest.AServerClassThatDoesNotHashFailsItsReloadWithTheCatalogUntouched` does the same through a reload, and `ASupplementPropertyHashesItsTypeAsWrittenAsTheDumpsOwnDo` holds the fix the install's classes forced: the check hashed a pointer or shared pointer type with the pointer stripped, where the dump and the client hash the type as written, so every class holding such a property was refused)
- [x] Unit test: `.reload server_class_schema` with an added class decodes it without a restart, and a supplement with a bad hash keeps the old registry and reports the error. `ObjectSchemaMgrTest.AServerClassThatDoesNotHashFailsItsReloadWithTheCatalogUntouched`, run against a database: the bad hash fails the reload with the error named and the serving catalog kept, then an added class joins the next generation and an object of it is written as a BINd file and read back as itself with its value. A BINd file is written under the Save mask, which its property carries; the round trip #26 proposed read 0 back because it encoded under the default transmit mask, which that property, like the client's own m_behaviorTemplateNameID, does not carry. `InstallClassesReplaceTheInstallClassesBeforeThemWithTheirEnumOptionsAndLeaveTheAuthoredOnes` holds the rows an install writes, read back and compared with the file they came from.
- [x] Client-gated sweep: the unknown-class count from OBJ-6 drops for every class added; the sweep result is recorded. On r806919, `extractor classes` ran schemaprobe over all 3595 of the install's archives in 5 minutes 54 seconds of a Debug build and kept 61 classes after four rounds. In the BINd files and headerless objects the unknown classes went from 133 to 87 and the objects read from 50430 to 51030 as 50 were named and the objects inside them became readable, while the 10 files that did not decode before still did not and none joined them. In the 3475 plain-XML object files, 11 classes the dump does not describe held 6837 objects before and none after, with no file refused: the portals.xml of 3377 zone archives, 86 light lists, two sky documents, Chatter.xml, Colors.xml and CharacterCreationConfig.xml, classes only the server reads, which take the server's own types by the rule doc/ARCHITECTURE.md records; measured on the XML properties of the dump's own classes, that rule gave every scalar type and container the dump gives them. A second sweep of Root.wad with the class file loaded through `--supplement` shows the same: 104 unknown classes in its BINd files before and 62 after, the 46 of the 50 that Root.wad holds gone, the other 4 living in zones' gamedata.xml, and 4 revealed in the m_assignedPlayers lists of NPC behaviors that could not be read before, and in its 8 XML object files 6 unknown classes holding 35 objects before and none after. The 50 binary classes are 22 behavior templates such as PotionShopBehaviorTemplate and WizTrainingBehaviorTemplate, 21 Result classes such as ResAddGold and 7 Requirement classes such as ReqHasAdultPet, each derived from the class the list it sits in holds, the one base the data proves. The rest are refused with the reason, most for a property whose type and name no source names, among them the four sigil-info classes F-65 counts in Wizard City, which share one such property, 62872365: `client name` shows the widest search names a random hash 0.8% of the time and names none of the sigil classes' own properties, so those wait for a source that holds them. The game server's first start wrote the 61 to a new world database, and with the behavior_client_class rows 2026_09_28_01 adds for every behavior a placed template names, followed to the factory the client program registers or shown to be a name it holds no string of, the client entered the Commons with all 123 objects (run 20260929-134442).

## 6.11 Trigger/volume schemas and zone WAD sweep (WLD-3 part 1 + OBJ-18)

**Goal:** TriggerList 0x06DAAC43, Trigger 0x068C265B, TriggerVolumeList 0x1B6EF770, TriggerVolume 0x1B7B55F6 and TriggerGroupList authored and hash-checked.

**Size:** M. **Depends on:** 6.10

**Acceptance**

- [x] Every authored class and property hash recomputes (these classes confirmed absent from the dump) (`ObjectSchemaMgrTest.EveryAuthoredClassAndPropertyHashesAsItsNameSays`, run against a database, over the rows data/sql/updates/db_world/2026_09_30_00.sql adds)
- [x] Sweep of all zone WADs: zero crashes, every file kind has a root class (`schemaprobe --all-wads`: 3589 archives, 2479 file kinds, every one with a root class and none failing)
- [x] Aquila-AQ_Z00_Hub.wad triggers.xml and volumes.xml decode with no unknown classes (`schemaprobe --wad Aquila-AQ_Z00_Hub.wad` with the install's class file: its only unknown classes are two sigil classes in gamedata.bin)

The roadmap's working names were guesses. The client program holds no string for any of the three roots, so their names come from a search of name forms, each confirmed by the list property hashing with the element type it holds: TriggerList 0x06DAAC43 through m_allTriggers, a SharedPointer to Trigger; TriggerVolumeList 0x1B6EF770 through m_allVolumes, a pointer to TriggerVolume; and TriggerGroupList through m_allTriggerGroups. Trigger 0x068C265B and TriggerVolume 0x1B7B55F6 match the roadmap's hashes. WizardCity-WC_Hub's leftover trig_backup_saveme.notxml spells Trigger, TriggerObjectInfo and most of their properties, and ArchiveText::PropertyNames now reads .notxml and class names written as `class.X`, as candidates checked by hash. Of the rest, 21 property names no dump, text or program string holds come from a two-word search, and m_requiredQuest from a three-word one, each name fitting the values the files hold. A field no name fits yet stays typed under its hash, such as Trigger's #780900737, and is read and stored, never skipped. Every class, with its evidence, is in data/sql/updates/db_world/2026_09_30_00.sql.

The full sweep found one more class inside triggers.xml: StateTrigger, 17 entries in 10 zones, a Trigger that also holds a state it requires.

Seven files outside the zones fail to decode, all for a root class the dump does not list: HighScoreConfig.xml, MonsterMagicWorldLoot.xml, NPCServices.xml, WhirlyBurlyConfig.xml and WizBangPriority.xml in Root.wad, and Combat/CombatAIData.xml and CombatAIDataBruteForce.xml in _Shared-WorldData.wad. None is a zone file kind. The name-form search names five of the roots: HighScoreConfig, MonsterMagicWorldLootList, NPCServices, WhirlyBurlyConfig and WizBangPriorityManager. CombatAIData's root, 561859630, is not named. They wait for the milestones that use them: minigames, loot, NPC services and combat.

### Detailed spec from WLD-3: Zone extractor part 2: volumes and triggers (server-only classes)

Walk-in volumes and event triggers for every zone are decoded into typed world rows, even though their classes are missing from the client type dump.

**Deliverables**

- src/server/shared/ObjectProperty: hand-authored schemas registered for TriggerList (0x06DAAC43), Trigger (0x068C265B), TriggerVolumeList (0x1B6EF770), TriggerVolume (0x1B7B55F6) and the Result/Requirement subclasses seen in triggers.xml, each name checked by recomputing its property-name hash
- extractor zones: BINd reader (magic 'BINd', uint32 flags=7, then class hash), decodes triggers.xml, volumes.xml and trigger_groups.xml
- data/sql/updates/db_world: zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Data sources**

- <zone>.wad/triggers.xml, volumes.xml, trigger_groups.xml (BINd, flags 7)
- <zone>.wad/trig_backup_saveme.notxml is a leftover plain-XML test file (WC_Hub has one). It hints at field names but is not a data source.

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [x] Unit: each hand-authored class hash equals the string hash of its name, and each property id equals the hash of its property name. `ObjectSchemaMgrTest.EveryAuthoredClassAndPropertyHashesAsItsNameSays` reads every authored row back from a world database and checks each class hash against its name and each property hash against its type and name, or against the hash an unnamed property's name carries (`TypeRegistryTest.AnUnnamedSupplementPropertyIsCheckedByTheHashItsNameCarries`).
- [x] WC_Hub volumes decode to rows including 'Ravenwood POI' (Sphere, enter event 'Enter_Ravenwood POI', exit 'Exit_Ravenwood POI', type STATIC_CLIENT_SERVER) (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Ravenwood POI is a Sphere with enter event Enter_Ravenwood POI, exit event Exit_Ravenwood POI and loading type 0, STATIC_CLIENT_SERVER; the rows written to a world database by `extractor zones` hold the same)
- [x] WC_Hub triggers decode to rows including 'Trigger POI Ravenwood' (fire event 'Enter_Ravenwood POI') and 'TeleportToShoppingDistrict' with a teleport result whose destination is empty (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Trigger POI Ravenwood fires on Enter_Ravenwood POI, and TeleportToShoppingDistrict's one result is a class ResTeleport, which has no properties, so it holds no destination)
- [x] Extractor summary counts zones whose trigger or volume files fail to decode, target 0 across all 3356. `extractor zones` over r806919 with a world database holding the authored and install classes: 3356 zones, 13,390 zone_volume rows, 19,644 zone_trigger rows (17 of them StateTrigger), 63,907 events and 40,022 results, and 0 zones whose volume or trigger files do not decode. 25,903 of the results are of Res classes nothing describes yet. They keep their place and class hash, so they can be read once their classes are named. Before StateTrigger was authored, the summary counted 10 zones.

**Risks**

- These classes are server-side and absent from the client dump, so schemas must be reverse-engineered from the binary alone. Verify every name against its hash, and leave unknown fields as typed but unnamed, never skipped.
- ResTeleport has no destination zone or location in client data (confirmed: the dump defines the class with no properties). Destinations come from WLD-14.

### Detailed spec from OBJ-18: Zone WAD object files decode sweep

Per-zone BINd object files (spawns, triggers, volumes, paths) decode reliably and are ready for the world domain to consume.

**Deliverables**

- An extension of bindecode/schemaprobe that sweeps all 3589 zone and sound WADs for BINd .xml entries (spawnData.xml, clientSpawnData.xml, triggers.xml, trigger_groups.xml, volumes.xml, pathData.xml, FishingInfo.xml, Compass.xml) and reports root classes, unknown classes and decode failures per file name
- A documented list of the root classes per file kind for WLD to build on (e.g. triggers and volumes roots, which the reference study shows are server-side classes not in the client dump)

**Acceptance**

- [x] Client-gated sweep over all zone WADs completes with zero crashes; every file kind has a known root class or an OBJ-12 supplemental entry (`schemaprobe --all-wads`: 3589 archives, 2479 file kinds, every one with a root class and none failing)
- [x] The Aquila-AQ_Z00_Hub.wad triggers.xml and volumes.xml decode with no unknown classes after supplements (`schemaprobe --wad Aquila-AQ_Z00_Hub.wad` with the install's class file: its only unknown classes are two sigil classes in gamedata.bin)

**Risks**

- Trigger, volume and zone-router classes appear to be server-only. Naming them may take several OBJ-11/OBJ-12 iterations

## 6.12 Volume/trigger extraction to world rows (WLD-3 part 2)

**Goal:** zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result.

**Size:** M. **Depends on:** 6.11, 4.08

**Acceptance**

- [x] WC_Hub 'Ravenwood POI' sphere with Enter_/Exit_Ravenwood POI, STATIC_CLIENT_SERVER (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Ravenwood POI is a Sphere with enter event Enter_Ravenwood POI, exit event Exit_Ravenwood POI and loading type 0, STATIC_CLIENT_SERVER; the rows written to a world database by `extractor zones` hold the same)
- [x] 'Trigger POI Ravenwood' and 'TeleportToShoppingDistrict' with an empty teleport destination (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Trigger POI Ravenwood fires on Enter_Ravenwood POI, and TeleportToShoppingDistrict's one result is a class ResTeleport, which has no properties, so it holds no destination)
- [x] Failures across 3356 zones counted, target 0 (`extractor zones` over r806919 with a world database holding the authored and install classes: 3356 zones, 13,390 zone_volume rows, 19,644 zone_trigger rows (17 of them StateTrigger), 63,907 events and 40,022 results, and 0 zones whose volume or trigger files do not decode.)

Delivered with 6.11 in one landing: the same four tables, written by `extractor zones` and the game server's first start from the classes 6.11 authored, data/sql/updates/db_world/2026_09_30_01.sql.

### Detailed spec from WLD-3: Zone extractor part 2: volumes and triggers (server-only classes)

Walk-in volumes and event triggers for every zone are decoded into typed world rows, even though their classes are missing from the client type dump.

**Deliverables**

- src/server/shared/ObjectProperty: hand-authored schemas registered for TriggerList (0x06DAAC43), Trigger (0x068C265B), TriggerVolumeList (0x1B6EF770), TriggerVolume (0x1B7B55F6) and the Result/Requirement subclasses seen in triggers.xml, each name checked by recomputing its property-name hash
- extractor zones: BINd reader (magic 'BINd', uint32 flags=7, then class hash), decodes triggers.xml, volumes.xml and trigger_groups.xml
- data/sql/updates/db_world: zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Data sources**

- <zone>.wad/triggers.xml, volumes.xml, trigger_groups.xml (BINd, flags 7)
- <zone>.wad/trig_backup_saveme.notxml is a leftover plain-XML test file (WC_Hub has one). It hints at field names but is not a data source.

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [x] Unit: each hand-authored class hash equals the string hash of its name, and each property id equals the hash of its property name. `ObjectSchemaMgrTest.EveryAuthoredClassAndPropertyHashesAsItsNameSays` reads every authored row back from a world database and checks each class hash against its name and each property hash against its type and name, or against the hash an unnamed property's name carries (`TypeRegistryTest.AnUnnamedSupplementPropertyIsCheckedByTheHashItsNameCarries`).
- [x] WC_Hub volumes decode to rows including 'Ravenwood POI' (Sphere, enter event 'Enter_Ravenwood POI', exit 'Exit_Ravenwood POI', type STATIC_CLIENT_SERVER) (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Ravenwood POI is a Sphere with enter event Enter_Ravenwood POI, exit event Exit_Ravenwood POI and loading type 0, STATIC_CLIENT_SERVER; the rows written to a world database by `extractor zones` hold the same)
- [x] WC_Hub triggers decode to rows including 'Trigger POI Ravenwood' (fire event 'Enter_Ravenwood POI') and 'TeleportToShoppingDistrict' with a teleport result whose destination is empty (`ZoneExtractorClientTriggerTest.TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses`, client-gated, reads WC_Hub's volumes.xml and triggers.xml through the authored classes a new world database holds: Trigger POI Ravenwood fires on Enter_Ravenwood POI, and TeleportToShoppingDistrict's one result is a class ResTeleport, which has no properties, so it holds no destination)
- [x] Extractor summary counts zones whose trigger or volume files fail to decode, target 0 across all 3356. `extractor zones` over r806919 with a world database holding the authored and install classes: 3356 zones, 13,390 zone_volume rows, 19,644 zone_trigger rows (17 of them StateTrigger), 63,907 events and 40,022 results, and 0 zones whose volume or trigger files do not decode.

**Risks**

- These classes are server-side and absent from the client dump, so schemas must be reverse-engineered from the binary alone. Verify every name against its hash, and leave unknown fields as typed but unnamed, never skipped.
- ResTeleport has no destination zone or location in client data (confirmed: the dump defines the class with no properties). Destinations come from WLD-14.

## 6.13 Volumes and walk-in trigger events (WLD-14)

**Goal:** Enter/exit fires triggers and ZoneScript hooks.

**Size:** M. **Depends on:** 6.12, 5.03, 4.01, 4.15

**Client messages:** MSG_POSTZONEEVENTFROMCLIENT, MSG_CLIENTNOTIFYTEXT

**Acceptance**

- [ ] Containment for each primitive incl. boundary
- [ ] Cooldown fires once per player
- [ ] Walking into Ravenwood POI logs 'Enter_Ravenwood POI' and fires 'Trigger POI Ravenwood'; spawning inside fires nothing
- [ ] Triggers with requirements fail closed until 7.04
- [ ] `.reload zone_trigger` applies an edited trigger; a failed reload keeps the old set

### Detailed spec from WLD-14: Volumes and walk-in trigger events

Walking into a zone volume fires its enter and exit events into the zone's triggers and the script system.

**Deliverables**

- src/server/game/Zones/ZoneVolume.h/.cpp: sphere/box/cylinder containment from zone_volume (primitiveType, radius, length, ...); a player spawned inside counts as inside without firing enter
- src/server/game/Zones/ZoneTriggerMgr: event bus per Map; a trigger with a matching fireEvent passes cooldown and triggerMax, then its results run through a Result dispatcher interface (results implemented by owning domains)
- The built-in 'EnterZone' event fires on player enter (the reference posts 'EnterZone'; the data uses both 'StartZone' and 'EnterZone')
- ZoneScript hooks: OnVolumeEnter, OnVolumeExit, OnTriggerFired; scripts/World/ loader
- MSG_POSTZONEEVENTFROMCLIENT posts client-sent events, allow-listed per zone
- `.reload zone_trigger` rebuilds volumes, triggers, events, results, cooldowns and the client-event allow-list off to the side, validates them, and swaps them into live Maps; a failure keeps the old set and reports every error. A player inside a volume that still exists stays inside without firing enter.

**Client messages:** MSG_POSTZONEEVENTFROMCLIENT, MSG_CLIENTNOTIFYTEXT

**Data sources**

- world.zone_volume, zone_trigger, zone_trigger_event, zone_trigger_result

**Database tables**

- world.zone_volume
- world.zone_trigger
- world.zone_trigger_event
- world.zone_trigger_result

**Acceptance**

- [ ] Unit: containment tests for each primitive type, including boundary and hysteresis
- [ ] Unit: a trigger with a cooldown fires once per player per cooldown
- [ ] Real client: walking into Ravenwood's POI sphere in WC_Hub logs 'Enter_Ravenwood POI' and fires 'Trigger POI Ravenwood'; with the POI-text result wired, the zone-entry text shows
- [ ] Real client: logging in while standing inside a volume fires no enter event
- [ ] Unit: editing a zone_trigger row, then `.reload zone_trigger`, changes what fires on the next enter without a restart; a row that fails validation keeps the old triggers and reports the error

**Risks**

- Which message shows POI text (e.g. WizardPOI_00000001 keys) is unverified
- Requirement evaluation on triggers depends on QST's requirement engine. Until it exists, triggers with requirements must fail closed, not fire.

## 6.14 Zone doors table and walk-in transfers (WLD-15)

**Goal:** Walk through exits to the right arrival point.

**Size:** M. **Depends on:** 6.07, 6.13, 4.15

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_SERVERTRANSFER, MSG_SERVERTELEPORT, MSG_ENTERSTATE

**Acceptance**

- [ ] Real client: WC_Hub Ravenwood gate lands in Ravenwood; back lands at 'Target location(WC_Hub Ravenwood)'
- [ ] 'Teleport location (WC_Hub WC_Headmistress_House Entrance)' works
- [ ] Two triggers on one event give one transfer; '.zone teleports' flags missing destinations
- [ ] `.reload zone_teleport` sends the next walk-through to an edited destination

### Detailed spec from WLD-15: Zone doors: teleport destination table and walk-in transfers

Walking through a zone exit (e.g. WC_Hub -> Ravenwood) transfers the player to the correct zone and arrival point.

**Deliverables**

- world.zone_teleport table: zone, trigger_name -> dest_zone, dest_location, transition_id, same_zone flag
- `.reload zone_teleport` rebuilds the destination map off to the side, validates every destination zone and location, and swaps it; a failure keeps the old rows and reports every error
- ResTeleport result handler: a same-zone destination uses WLD-12; otherwise WLD-13. When paired triggers share an event, only the first teleport in data order runs.
- src/tools/extractor zones --propose-teleports: suggests pairs by matching 'Target location (<SrcZone> <DstZone> Exit)'-style location names and 'TeleportTo<X>' trigger names across zones, and writes a review CSV. The opt-in --apply-proposals also writes the suggestions into the user's local world database as journaled edits exportable as a pending SQL update; proposals reach the repository only as rows a human has reviewed
- data/sql/updates/db_world: hand-reviewed zone_teleport rows for the Wizard City starting area (WC_Hub <-> Ravenwood, Shopping District, Unicorn Way, Golem Court, Library)

**Client messages:** MSG_ZONETRANSFERREQUEST, MSG_ZONETRANSFERACK, MSG_SERVERTRANSFER, MSG_SERVERTELEPORT, MSG_ENTERSTATE

**Data sources**

- world.zone_location names (e.g. WC_Hub 'Target location (WC_Hub Street1 Exit)')
- world.zone_trigger names and events (e.g. 'TeleportToShoppingDistrict', 'TeleportToRavenwoodTrigger')

**Database tables**

- world.zone_teleport
- world.zone_trigger_result

**Acceptance**

- [ ] Real client: in WC_Hub, walking through the Ravenwood gate shows the loading screen and lands in Ravenwood facing away from the gate; walking back lands at 'Target location(WC_Hub Ravenwood)' in WC_Hub
- [ ] Real client: walking into 'Teleport location (WC_Hub WC_Headmistress_House Entrance)' works
- [ ] Unit: two triggers on one event with teleport results produce exactly one transfer
- [ ] '.zone teleports <zone>' lists each teleport trigger and flags any with no destination row
- [ ] Real client: editing a zone_teleport destination, then `.reload zone_teleport`, sends the next walk-through to the new destination without a restart; a row naming a missing zone keeps the old rows

**Risks**

- Destination data is not in the client: authoring rows for ~3356 zones is a large manual content job. Committed rows come only from a name-matching heuristic plus human review, which keeps them clean-room. An opt-in importer that reads another project's teleport data from a copy the user has, into that user's local world database only, is planned, not yet scheduled; imported rows are never committed or redistributed.
- Decided on 2026-09-16 at the maintainer's direction: a destination table that names zones, locations and triggers by their client identifiers may be committed as hand-reviewed zone_teleport rows, like key-only quest SQL. No client file or client text is copied into them.

## 6.15 AOI grid and visibility sets, unit level (WLD-11 part 1)

**Goal:** Per-player known sets with hysteresis.

**Size:** M. **Depends on:** 6.01, 4.16

**Client messages:** MSG_ADDOBJECT

**Acceptance**

- [x] Boundary crossing within hysteresis sends nothing (`VisibilitySetTest.CrossingTheBoundaryBackAndForthInsideTheHysteresisBandSendsNothing`: with a distance of 100 and a band of 20, an object shown at 90 moves to 101, 99, 119, 100, 115 and 120 with no change and leaves only past 120, and one outside stays away until it comes within 100)
- [x] Re-entry sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT (`VisibilitySetTest.ReEntryAfterExitIsAnAddNotASecondNewObject`: the first showing is New, and after it leaves view it comes back as Added, never New again unless the wizard's client forgot it)
- [x] Changing Visibility.Distance applies on the next visibility update (`VisibilitySetTest.LoweringTheDistanceTakesAwayWhatIsNowOutOfRangeOnTheNextUpdate`: lowering the distance from 300 to 100 removes the objects at 150 and 250 on the next update and keeps the exempt one at 1000; Visibility.Distance and Visibility.Hysteresis are live settings read at each update)

### Detailed spec from WLD-11: Area of interest: grid visibility

In big or busy zones each client gets only objects and players within range, with correct spawn and despawn as people move.

**Deliverables**

- src/server/game/Zones/Grid.h/.cpp: cell grid per Map with neighbor queries
- src/server/game/Zones/VisibilitySet: per-player known-object set that sends MSG_ADDOBJECT on re-entry and MSG_REMOVEOBJECT on exit; MSG_NEWOBJECT only the first time an object is made known
- Objects whose template has m_exemptFromAOI are always visible
- Move relays go only to players who can see the mover
- conf/dist/gameserver.conf.dist: Visibility.Distance (default: zone m_farClip), Visibility.Hysteresis, live settings; a change re-evaluates every visibility set on the next update
- src/test/server/game/Zones/GridTest.cpp, VisibilitySetTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Data sources**

- zone_template.farClip
- GameObjectTemplate.m_exemptFromAOI

**Acceptance**

- [x] Unit: an object crossing the range boundary back and forth within the hysteresis band generates no messages (`VisibilitySetTest.CrossingTheBoundaryBackAndForthInsideTheHysteresisBandSendsNothing`: with a distance of 100 and a band of 20, an object shown at 90 moves to 101, 99, 119, 100, 115 and 120 with no change and leaves only past 120, and one outside stays away until it comes within 100)
- [x] Unit: re-entry after exit sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT (`VisibilitySetTest.ReEntryAfterExitIsAnAddNotASecondNewObject`: the first showing is New, and after it leaves view it comes back as Added, never New again unless the wizard's client forgot it)
- [x] Unit: lowering Visibility.Distance removes objects now out of range on the next update without a restart (`VisibilitySetTest.LoweringTheDistanceTakesAwayWhatIsNowOutOfRangeOnTheNextUpdate`: lowering the distance from 300 to 100 removes the objects at 150 and 250 on the next update and keeps the exempt one at 1000; Visibility.Distance and Visibility.Hysteresis are live settings read at each update)

The two real-client checks this milestone's detailed spec once listed, B walking away from A and coming back in the right place and a far-off exempt landmark staying visible, are 6.16's, which lists them as its own acceptance; they moved there on 2026-09-30, because 6.15 builds the grid and the visibility sets at unit level and 6.16 wires them into the real client, so 6.15 is done with every check it can earn itself.

**Risks**

- The reference notes the client ignores MSG_ADDOBJECT for an object it never got via MSG_NEWOBJECT, and needs about 250ms between NEWOBJECT and REMOVEOBJECT to register it. Both are observed behavior, not specified.
- The reference culls only objects whose animation template fades in or out. The retail rule for which objects are AOI-culled is unknown.

## 6.16 AOI in the real client (WLD-11 part 2)

**Goal:** Busy zones stream only nearby objects.

**Size:** M. **Depends on:** 6.15

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Acceptance**

- [ ] B walks away and reappears in the right place for A
- [ ] Far exempt landmark stays visible

### Detailed spec from WLD-11: Area of interest: grid visibility

In big or busy zones each client gets only objects and players within range, with correct spawn and despawn as people move.

**Deliverables**

- src/server/game/Zones/Grid.h/.cpp: cell grid per Map with neighbor queries
- src/server/game/Zones/VisibilitySet: per-player known-object set that sends MSG_ADDOBJECT on re-entry and MSG_REMOVEOBJECT on exit; MSG_NEWOBJECT only the first time an object is made known
- Objects whose template has m_exemptFromAOI are always visible
- Move relays go only to players who can see the mover
- conf/dist/gameserver.conf.dist: Visibility.Distance (default: zone m_farClip), Visibility.Hysteresis, live settings; a change re-evaluates every visibility set on the next update
- src/test/server/game/Zones/GridTest.cpp, VisibilitySetTest.cpp

**Client messages:** MSG_NEWOBJECT, MSG_ADDOBJECT, MSG_REMOVEOBJECT, MSG_SERVERMOVE, MSG_MOVESTATE

**Data sources**

- zone_template.farClip
- GameObjectTemplate.m_exemptFromAOI

**Acceptance**

- [ ] Unit: an object crossing the range boundary back and forth within the hysteresis band generates no messages
- [ ] Unit: re-entry after exit sends MSG_ADDOBJECT, not a second MSG_NEWOBJECT
- [ ] Real client: in a large zone, B walks away from A: A sees B vanish at range and reappear when B returns, in the right place
- [ ] Real client: a far-off exempt landmark stays visible

**Risks**

- The reference notes the client ignores MSG_ADDOBJECT for an object it never got via MSG_NEWOBJECT, and needs about 250ms between NEWOBJECT and REMOVEOBJECT to register it. Both are observed behavior, not specified.
- The reference culls only objects whose animation template fades in or out. The retail rule for which objects are AOI-culled is unknown.

## 6.17 Network hardening (NET-11)

**Goal:** Abusive traffic disconnected predictably.

**Size:** M. **Depends on:** 2.09, 4.16

**Acceptance**

- [ ] 10-minute fuzz: no crash, no allocation above MaxFrameSize
- [x] 10k frames/s flooder disconnected within 1 s (SessionBaseTest.FrameFloodDisconnectsWithoutStallingAnotherSession: 10,000-frame stream closed within 1 s; survivor handled within 500 ms)
- [x] A never-reading client disconnected at the high-water mark (OutboundMessagesTest.AClientThatStopsReadingIsClosedAtTheSendQueueLimit)
- [x] Changing Network.RateLimit.PerSecond applies to connected sessions from the next frame (SessionBaseTest.LoweringTheFrameRateAppliesToTheNextInboundFrame)

### Detailed spec from NET-11: Network hardening

Malformed, oversized or abusive traffic can't crash or stall a server and is disconnected predictably.

**Deliverables**

- Per-session token bucket on frames per second (Network.RateLimit.Burst=150, PerSecond=50, as a reference server's defaults suggest) with a violation counter and disconnect
- Per-IP connection cap and accept-rate cap in SocketMgr (Network.MaxConnectionsPerIP, Network.AcceptRatePerSecond)
- Strict DML checks: dmlLen must match bytes consumed by Decode; STR/WSTR length above the remaining frame gives a reject; control opcode not in {0,3,4,5} gives a strike
- Send-queue high-water mark (Network.SendQueueHighWater): a slow reader is disconnected instead of growing memory without limit
- The rate limits, caps and high-water mark are live settings with bounds, applied from the next frame or connection
- src/test/fuzz/FrameFuzz.cpp and MessageDecodeFuzz.cpp (libFuzzer target where the compiler supports it; otherwise a randomized gtest)

**Data sources**

- None

**Acceptance**

- [ ] Fuzz run of 10 minutes on the frame and decode paths has no crash, no ASan report and no allocation above MaxFrameSize
- [x] A test client flooding 10k frames/s is disconnected within 1s, and other sessions show no latency spike above a threshold in the integration test (SessionBaseTest.FrameFloodDisconnectsWithoutStallingAnotherSession: 10,000-frame stream closed within 1 s; survivor handled within 500 ms)
- [x] A test client that never reads is disconnected when its send queue passes the high-water mark (OutboundMessagesTest.AClientThatStopsReadingIsClosedAtTheSendQueueLimit)
- [x] Integration test: lowering Network.RateLimit.PerSecond while a client is connected throttles that client from the next frame without a restart (SessionBaseTest.LoweringTheFrameRateAppliesToTheNextInboundFrame)

**Risks**

- Rate limits set too low could drop legitimate bursts (MSG_CLIENTMOVE during movement, zone loads). Tune with real-client traces

## 6.18 Packet log, diagnostics, network hooks (NET-12)

**Goal:** Named packet logging and CanPacketReceive hooks.

**Size:** S. **Depends on:** 2.09, 4.02, 4.16

**Client messages:** MSG_USER_AUTHEN_V3, MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK

**Acceptance**

- [ ] Log shows 'C->S LOGIN MSG_USER_AUTHEN_V3 (7:27)' with credentials redacted; suppressed messages absent
- [x] A module blocks one message with no core edits (`ChatHandlerTest.AServerScriptHoldsBackTheOneMessageItRefusesWithNoEditToTheCore`: a ServerScript defined only in the test refuses MSG_REQUESTRADIALQUICKCHAT, and over loopback a real game session queues the other three chat messages and never sees that one, counting it neither unhandled nor a strike; with the script unloaded all four arrive)
- [ ] '.network sessions' returns live count
- [ ] '.network packetlog' toggles and filters logging live

Built on 2026-09-30 by the maintainer's track session: `PacketLog` and `NetworkHooks` in src/server/shared/Network, the ServerScript kind in ScriptMgr bridged to them, and the `.network` command group in cs_network.cpp, with `PacketLogTest` and `NetworkHooksTest` proving the line format, the redaction, the filter and suppression and a live settings change from the next message. The login log line and the two commands stay unticked until a real client shows them.

### Detailed spec from NET-12: Packet logging, diagnostics and network hooks

Developers can see every message by name with fields, and scripts or modules can observe connections, without touching core code.

**Deliverables**

- src/server/shared/Network/PacketLog.h/.cpp: optional (Network.PacketLog.Enable, .Filter, .Suppress defaults MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK, keepalives) text log with direction, session id, service:order, name and DynamicMessage fields. The PacketLog options are live settings applied from the next message.
- ScriptMgr ServerScript hooks: OnNetworkStart, OnSocketOpen, OnSocketClose, CanPacketReceive(session, service, order), CanPacketSend
- GM command group src/server/scripts/Commands/cs_network.cpp: 'network sessions' (count, per-app), 'network session <id>' (state, RTT, queued bytes, strikes), 'network packetlog on|off|filter <names>' (sets the PacketLog settings live)

**Client messages:** MSG_USER_AUTHEN_V3, MSG_CLIENTMOVE, MSG_SERVERMOVE, MSG_NEWOBJECT, MSG_REMOVEOBJECT, MSG_LOGIN_NOT_AFK

**Data sources**

- Generated registry

**Acceptance**

- [ ] With PacketLog enabled and a real client at login, the log shows 'C->S LOGIN MSG_USER_AUTHEN_V3 (7:27)' with decoded fields, and suppressed messages are absent
- [x] A test module registering CanPacketReceive returning false for one message blocks it with no core edits (`ChatHandlerTest.AServerScriptHoldsBackTheOneMessageItRefusesWithNoEditToTheCore`: a ServerScript defined only in the test refuses MSG_REQUESTRADIALQUICKCHAT, and over loopback a real game session queues the other three chat messages and never sees that one, counting it neither unhandled nor a strike; with the script unloaded all four arrive)
- [ ] '.network sessions' in game chat returns the live count
- [ ] '.network packetlog filter MSG_CLIENTMOVE' on a running server changes what is logged from the next message without a restart

**Risks**

- Logging credentials: the MSG_USER_AUTHEN_V3 payload must be redacted in the log

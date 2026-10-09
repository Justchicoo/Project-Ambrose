<!-- Project Ambrose by Imjustchico: Every live setting, written from the declarations in src/server/shared/Settings. -->
# Live settings

Every setting here can be changed while its app runs with `.settings set <key> <value> [reason]` in game or `settings set` on the app's console, and returned to its config value with `settings reset`. A change is checked against the type and bounds below, persisted in the `settings` table of the database the app owns (`characters` for the game server, `login` for the login server, and the panel store for the supervisor), and written to `setting_audit` with who made it and why. A setting also set by an `AMBROSE_` environment variable or a command-line override is locked and cannot be changed live. The layers are described in [README.md](README.md).

Applies says when a change takes hold: live at once, or from the next connection or operation that reads it.

Access says who may see and change a setting over the admin API and the panel. A secret's value is shown masked, in `setting_audit` too, unless the caller asks for it with the right to see secrets, and every such reveal is audited. The admin and panel tokens and the password in each database connection string are secrets too, although only config holds them, and are masked the same way wherever they are shown, a configuration file read through the panel included. A restricted setting is one whose wrong value stops the app or locks players out, so changing it takes its own right besides the right to change settings.

## Accounts

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Account.AllowPlainVerifiers` | bool | true | none | next connection or operation | loginserver | restricted | Whether an account whose verifier is still unencrypted may log in while a verifier key is active. |
| `Account.PasswordMinLength` | unsigned | 4 characters | from 1 to 128 characters | next connection or operation | loginserver | normal | The fewest characters a new or changed password may have. |
| `Account.UsernameMinLength` | unsigned | 3 characters | from 1 to 32 characters | next connection or operation | loginserver | normal | The shortest username a new account may use. |
| `Account.VerifierActiveKey` | unsigned | 0 | from 0 to 255 | next connection or operation | gameserver, loginserver | restricted | The key id that seals new and changed verifiers, which Account.VerifierKeys must list; 0 stores them unencrypted and is refused while keys are listed. |
| `Account.VerifierKeys` | string | empty | at most 65535 bytes | next connection or operation | gameserver, loginserver | secret, restricted | The AES-256 keys that seal stored password verifiers, written id:hex with ids 1 to 255 and 64 hex digits each, separated by commas; keep every key that still seals a stored verifier. |

## Characters

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Character.AllowChosenNames` | bool | false | none | live | loginserver | normal | Whether any account may name a wizard freely rather than from the client's name tables. |
| `Character.DeleteMode` | string | soft | at most 8 bytes | live | loginserver | normal | What deleting a wizard from character select does, read on each delete: soft keeps its row, with the time and the account it was taken from, so a game master can restore it; hard removes it and everything it holds. |
| `Character.KeepDeletedDays` | unsigned | 0 days | from 0 to 36500 days | live | loginserver | normal | How long a soft-deleted wizard is kept before the next delete removes it for good, read on each delete; 0 keeps them all. |
| `Character.MaxPerAccount` | unsigned | 6 | from 0 to 250 | live | loginserver | normal | How many wizards an account may hold. |

## Chat

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Chat.SayRange` | float | 0 world units | from 0 to 100000 world units | live | gameserver | normal | How far a wizard's typed chat, quick chat and emotes reach the other wizards in its instance, read at each tick; 0 reaches the whole instance. |

## Commands

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `GM.CommandPrefix` | string | . | at most 8 bytes | live | gameserver | normal | What a chat line starts with to be read as a command. |
| `GM.LogCommands` | bool | true | none | live | gameserver | normal | Whether every command run is written to the log. |
| `GM.PlayerCommandsAsChat` | bool | true | none | live | gameserver | normal | Whether a player's chat line that starts with the command prefix is said as an ordinary line; when off it is refused and the player told so. An account above player level runs such a line as a command. |

## Diagnostics

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `LoginComplete.SaveDataTo` | string | empty | at most 1024 bytes | live | gameserver | normal | A folder the game server writes each MSG_LOGINCOMPLETE Data it sends into, as the enveloped bytes the client receives, for reading with client core; empty writes nothing. |

## Files

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Files.ListMaxEntries` | unsigned | 100000 entries | from 1000 to 10000000 entries | live | supervisor | normal | The most entries a folder listing reads before it stops and says the folder held more. |
| `Files.MinFreeBytes` | unsigned | 1073741824 bytes | from 0 to 1125899906842624 bytes | live | supervisor | normal | The least free space a volume must keep after any write the panel makes; the larger of this and Files.MinFreePercent holds. |
| `Files.MinFreePercent` | unsigned | 5 % | from 0 to 90 % | live | supervisor | normal | The least free space a volume must keep after any write the panel makes, as a share of the volume; the larger of this and Files.MinFreeBytes holds. |
| `Files.ReadMaxBytes` | unsigned | 4194304 bytes | from 65536 to 67108864 bytes | live | supervisor | normal | The most of a file one read hands the panel; a file this size or smaller also carries its content hash, and a configuration file larger than this is not shown. |

## Inventory

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Inventory.ExtraSlots` | unsigned | 0 items | from 0 to 10000 items | live | gameserver | normal | How many items every wizard's backpack holds beyond Inventory.Slots, read at each add, so raising it lets the next add to a full backpack succeed; the client is told the new total when the wizard next enters the world. |
| `Inventory.Slots` | unsigned | 100 items | from 0 to 10000 items | next connection or operation | gameserver | normal | How many items every wizard's backpack holds, read when the wizard enters the world and sent to its client as the m_numItemsAllowed of its inventory behavior, since neither the player template nor any other file of the install gives one. |

## Locale

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Locale.Default` | string | en-US | at most 16 bytes | live | gameserver, loginserver | normal | The locale names and texts are read in when a client names none. |

## Login

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Login.AfkTimeout` | unsigned | 360 s | from 0 to 86400 s | next connection or operation | loginserver | normal | How long a client may idle before choosing a wizard before it is closed; 0 never closes it. |
| `Login.AfkWarning` | integer | 1 | from -128 to 127 | next connection or operation | loginserver | normal | The Warning byte MSG_DISCONNECT_LOGIN_AFK carries. |
| `Login.AllowedRevision` | string | empty | at most 1024 bytes | live | loginserver | normal | The client revisions let in while Login.EnforceRevision is on, separated by commas. |
| `Login.DuplicateLoginPolicy` | unsigned | 1 | from 0 to 1 | live | loginserver | normal | What a login to an account already logged in does: 0 refuses it, 1 closes the earlier session. |
| `Login.EnforceRevision` | bool | false | none | live | loginserver | normal | Whether a client whose revision Login.AllowedRevision does not list is refused. |
| `Login.KeyTTL` | unsigned | 60 s | from 5 to 2592000 s | next connection or operation | loginserver | normal | How long the key a client carries to a game server stays good for. |
| `Login.LockoutSeconds` | unsigned | 900 s | from 1 to 2592000 s | live | loginserver | normal | How long a locked-out address is refused, and how long a failure is remembered. |
| `Login.MaxAuthAttempts` | unsigned | 5 | from 0 to 1000 | live | loginserver | normal | Wrong passwords from one address before it is locked out; 0 never locks it out. |
| `Login.Name` | string | Ambrose | at most 64 bytes | live | loginserver | normal | The login server's name, sent in MSG_STARTCHARACTERLIST. |
| `Login.SessionKeyLifetime` | unsigned | 108000 s | from 60 to 2592000 s | next connection or operation | loginserver | normal | How long the session key a successful login issues stays valid. |
| `Login.ShutdownGrace` | unsigned | 5 s | from 0 to 60 s | live | loginserver | normal | How long a stopping login server waits for its shutdown notices to be written. |

## Network

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Attach.Timeout` | unsigned | 30 s | from 1 to 3600 s | next connection or operation | gameserver | normal | How long a new game connection may go without MSG_ATTACH before it is closed. |
| `Network.AcceptRatePerSecond` | unsigned | 50 | from 1 to 100000 | live | gameserver, loginserver | normal | How many new client connections one IP address may establish per second. |
| `Network.DroppedMessageBurst` | unsigned | 64 | from 1 to 100000 | next connection or operation | gameserver, loginserver | normal | How many messages a connection may send that are dropped unread before a drop counts as a strike. |
| `Network.DroppedMessagesPerSecond` | unsigned | 16 | from 1 to 100000 | next connection or operation | gameserver, loginserver | normal | How fast that allowance of dropped messages refills, per second. |
| `Network.HandoffGrace` | unsigned | 30 s | from 1 to 3600 s | next connection or operation | loginserver | normal | How long a client sent to a game server may keep its login connection open. |
| `Network.KeepAliveInterval` | unsigned | 60 s | from 0 to 3600 s | next connection or operation | gameserver, loginserver | normal | How often an idle connection is asked whether it is still there; 0 never asks. |
| `Network.KeepAliveTimeout` | unsigned | 15 s | from 1 to 3600 s | next connection or operation | gameserver, loginserver | normal | How long a keepalive may go unanswered before the connection is closed. |
| `Network.MaxConnectionsPerIP` | unsigned | 100 | from 1 to 100000 | live | gameserver, loginserver | normal | How many simultaneous client connections one IP address may hold. |
| `Network.MaxStrikes` | unsigned | 10 | from 1 to 1000 | next connection or operation | gameserver, loginserver | normal | How many refused or malformed messages a connection may send before it is closed. |
| `Network.PacketLog.Enable` | bool | false | none | live | gameserver, loginserver | normal | Whether every DML message a session sends or receives is written to the network.packets log by name with its fields, read at each message; a message carrying credentials is written without its field values. |
| `Network.PacketLog.Filter` | string | empty | at most 2048 bytes | live | gameserver, loginserver | normal | The message tags the packet log keeps, separated by commas or spaces, read at each message; empty keeps every message not suppressed. |
| `Network.PacketLog.Suppress` | string | MSG_CLIENTMOVE,MSG_SERVERMOVE,MSG_NEWOBJECT,MSG_REMOVEOBJECT,MSG_LOGIN_NOT_AFK | at most 2048 bytes | live | gameserver, loginserver | normal | The message tags the packet log leaves out, separated by commas or spaces, read at each message; keepalives are control frames and are never written. |
| `Network.PingBurst` | unsigned | 16 | from 1 to 100000 | next connection or operation | gameserver, loginserver | normal | How many pings a connection may send at once before a ping counts as a strike. |
| `Network.PingsPerSecond` | unsigned | 4 | from 1 to 100000 | next connection or operation | gameserver, loginserver | normal | How fast that allowance of pings refills, per second. |
| `Network.RateLimit.Burst` | unsigned | 150 | from 1 to 100000 | live | gameserver, loginserver | normal | How many inbound frames a session may receive in a burst before frames count against its per-second rate. |
| `Network.RateLimit.PerSecond` | unsigned | 50 | from 1 to 100000 | live | gameserver, loginserver | normal | How fast a session's inbound frame allowance refills, per second. |
| `Network.SendQueueHighWater` | unsigned | 16777216 bytes | from 1048576 to 1073741824 bytes | live | gameserver, loginserver | normal | How many bytes one connection may have waiting to be sent before it is closed; applies to existing connections immediately. |
| `Network.SessionAcceptTimeout` | unsigned | 15 s | from 1 to 3600 s | next connection or operation | gameserver, loginserver | normal | How long a new connection may take to finish its handshake. |

## Player

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Player.AfkTime` | unsigned | 1800 s | from 0 to 86400 s | live | gameserver | normal | How long an in-world wizard may be idle before its session is disconnected; 0 disables the AFK timer. |
| `Player.AfkWarnTime` | unsigned | 900 s | from 0 to 86400 s | live | gameserver | normal | How long an in-world wizard may be idle before the client receives MSG_DISCONNECT_AFK. |
| `Player.LinkDeadTime` | unsigned | 60 s | from 0 to 86400 s | live | gameserver | normal | How long a disconnected wizard remains visible and may reattach before being removed from the world. |
| `Potion.RefillInterval` | unsigned | 300 s | from 0 to 86400 s | live | gameserver | normal | How long after a potion is used before one charge refills; 0 disables later refills, and changes apply after the next charge refills. |
| `Potion.RestoreFraction` | float | 1 fraction | from 0 to 1 fraction | live | gameserver | normal | The share of maximum health and mana each potion restores, read whenever a wizard uses a potion. |

## Rates

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Rate.Drop.Item` | float | 1 times | from 0 to 100 times | live | gameserver | normal | Multiplies the chance of each item a defeated creature may drop. |
| `Rate.Gold.Kill` | float | 1 times | from 0 to 100 times | live | gameserver | normal | Multiplies the gold a defeated creature drops. |
| `Rate.Gold.Quest` | float | 1 times | from 0 to 100 times | live | gameserver | normal | Multiplies the gold a quest gives. |
| `Rate.Respawn` | float | 1 times | from 0.1 to 100 times | live | gameserver | normal | Multiplies how long a defeated creature takes to return. |
| `Rate.XP.Kill` | float | 1 times | from 0 to 100 times | live | gameserver | normal | Multiplies the experience a defeated creature gives. |
| `Rate.XP.Quest` | float | 1 times | from 0 to 100 times | live | gameserver | normal | Multiplies the experience a quest gives. |

## Realms

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `PublicAddress` | string | empty | at most 255 bytes | next connection or operation | gameserver | normal | The address players reach this game server at, used when Realm.Address is empty. |
| `Realm.Address` | string | empty | at most 255 bytes | next connection or operation | gameserver | normal | The address the login server sends players to for this realm; empty uses PublicAddress, then BindIP. |
| `Realm.DefaultRealm` | string | empty | at most 64 bytes | live | loginserver | normal | The realm a player is sent to when their client names none; a name no realm online has falls through to the least-full realm. |
| `Realm.HeartbeatInterval` | unsigned | 30 s | from 1 to 3600 s | live | gameserver, loginserver | normal | How often a game server tells the login server it is up, and the beat the login server counts missed heartbeats by. |
| `Realm.Name` | string | Ambrose | at most 64 bytes | next connection or operation | gameserver | normal | The realm's name, announced to the login server with each heartbeat and sent in MSG_LOGINCOMPLETE. |
| `Realm.OfflineAfterIntervals` | unsigned | 3 | from 1 to 1000 | live | loginserver | normal | How many heartbeats a realm may miss before no player is sent to it. |
| `Realm.RefreshInterval` | unsigned | 10 s | from 1 to 3600 s | live | loginserver | normal | How often the login server rereads the realmlist table. |

## Social

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Social.MaxFriends` | unsigned | 100 | from 0 to 10000 | live | gameserver | normal | How many friends one wizard may have; a change applies to the next friend request and max-friends reply. |

## World

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `LoginComplete.CSRSecurityLevel` | unsigned | 2 | from 0 to 4 | next connection or operation | gameserver | normal | The account security level from which MSG_LOGINCOMPLETE opens the client's game master tools. |
| `LoginComplete.Permissions` | unsigned | 47 | from 0 to 4294967295 | next connection or operation | gameserver | normal | The permission bits MSG_LOGINCOMPLETE gives a wizard: 0x1 and 0x4 chat level, 0x2 and 0x8 show chat, 0x20 gifting, 0x40 test features, 0x400 paying, 0x1000 earning crowns. |
| `LoginComplete.TestServer` | bool | false | none | next connection or operation | gameserver | normal | Whether MSG_LOGINCOMPLETE tells the client it is on a test server. |
| `Templates.CacheSize` | unsigned | 256 MiB | from 1 to 65536 MiB | live | gameserver | normal | How much memory the object templates decoded from the install may hold before the least recently used is dropped; a smaller budget drops them at once. |
| `World.Heartbeat` | unsigned | 60 s | from 0 to 86400 s | live | gameserver | normal | How often the world logs that it is still ticking; 0 turns the line off. |
| `World.UpdateInterval` | unsigned | 50 ms | from 1 to 10000 ms | live | gameserver | normal | How long the world waits between ticks. |

## Zones

| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |
|---|---|---|---|---|---|---|---|
| `Visibility.Distance` | float | 0 world units | from 0 to 100000 world units | live | gameserver | normal | How near an object must come to a wizard to be shown to it, read at each visibility update; 0 takes the zone's own far clip, and a zone with none shows everything. |
| `Visibility.Hysteresis` | float | 20 world units | from 0 to 10000 world units | live | gameserver | normal | How far past the visibility distance an object already shown may go before it is taken away, read at each visibility update, so one standing at the edge is not shown and taken away over and over. |
| `Zone.DoorsIgnoreRequirements` | bool | false | none | live | gameserver | normal | Whether a door, a trigger holding a ResTeleport, fires for a wizard who does not meet its requirements, which otherwise fail closed until requirements are checked; for exploring a test server. |
| `Zone.MobileIdReleaseDelay` | unsigned | 2000 ms | from 0 to 60000 ms | next connection or operation | gameserver | normal | How long a mobile id rests after its wizard leaves before another wizard may take it. |
| `Zone.MoveFlushInterval` | unsigned | 250 ms | from 50 to 5000 ms | next connection or operation | gameserver | normal | How often the moves and movement states of the wizards in an instance are sent to the others in it, read at each flush. |
| `Zone.MoveIdleIntervals` | unsigned | 2 | from 1 to 100 | next connection or operation | gameserver | normal | How many flushes a wizard said to be moving may pass without a new move before the others are told it is standing, read at each flush. |
| `Zone.UnloadDelay` | unsigned | 60 s | from 0 to 86400 s | next connection or operation | gameserver | normal | How long an empty zone instance stays loaded, read when its last wizard leaves. |

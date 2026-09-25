<!-- Project Ambrose by Imjustchico: Every live setting, written from the declarations in src/server/shared/Settings. -->
# Live settings

Every setting here can be changed while its app runs with `.settings set <key> <value> [reason]` in game or `settings set` on the app's console, and returned to its config value with `settings reset`. A change is checked against the type and bounds below, persisted in the `settings` table of the database the app owns (`characters` for the game server, `login` for the login server), and written to `setting_audit` with who made it and why. A setting also set by an `AMBROSE_` environment variable or a command-line override is locked and cannot be changed live. The layers are described in [README.md](README.md).

Applies says when a change takes hold: live at once, or from the next connection or operation that reads it.

## Characters

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Character.AllowChosenNames` | bool | false | none | live | loginserver | Whether any account may name a wizard freely rather than from the client's name tables. |
| `Character.MaxPerAccount` | unsigned | 6 | from 0 to 250 | live | loginserver | How many wizards an account may hold. |

## Commands

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `GM.CommandPrefix` | string | . | at most 8 bytes | live | gameserver | What a chat line starts with to be read as a command. |
| `GM.LogCommands` | bool | true | none | live | gameserver | Whether every command run is written to the log. |

## Locale

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Locale.Default` | string | en-US | at most 16 bytes | live | gameserver, loginserver | The locale names and texts are read in when a client names none. |

## Login

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Login.AfkTimeout` | unsigned | 360 s | from 0 to 86400 s | next connection or operation | loginserver | How long a client may idle before choosing a wizard before it is closed; 0 never closes it. |
| `Login.AfkWarning` | integer | 1 | from -128 to 127 | next connection or operation | loginserver | The Warning byte MSG_DISCONNECT_LOGIN_AFK carries. |
| `Login.AllowedRevision` | string | empty | at most 1024 bytes | live | loginserver | The client revisions let in while Login.EnforceRevision is on, separated by commas. |
| `Login.DuplicateLoginPolicy` | unsigned | 1 | from 0 to 1 | live | loginserver | What a login to an account already logged in does: 0 refuses it, 1 closes the earlier session. |
| `Login.EnforceRevision` | bool | false | none | live | loginserver | Whether a client whose revision Login.AllowedRevision does not list is refused. |
| `Login.KeyTTL` | unsigned | 60 s | from 5 to 2592000 s | next connection or operation | loginserver | How long the key a client carries to a game server stays good for. |
| `Login.LockoutSeconds` | unsigned | 900 s | from 1 to 2592000 s | live | loginserver | How long a locked-out address is refused, and how long a failure is remembered. |
| `Login.MaxAuthAttempts` | unsigned | 5 | from 0 to 1000 | live | loginserver | Wrong passwords from one address before it is locked out; 0 never locks it out. |
| `Login.Name` | string | Ambrose | at most 64 bytes | live | loginserver | The login server's name, sent in MSG_STARTCHARACTERLIST. |
| `Login.SessionKeyLifetime` | unsigned | 108000 s | from 60 to 2592000 s | next connection or operation | loginserver | How long the session key a successful login issues stays valid. |
| `Login.ShutdownGrace` | unsigned | 5 s | from 0 to 60 s | live | loginserver | How long a stopping login server waits for its shutdown notices to be written. |

## Network

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Attach.Timeout` | unsigned | 30 s | from 1 to 3600 s | next connection or operation | gameserver | How long a new game connection may go without MSG_ATTACH before it is closed. |
| `Network.DroppedMessageBurst` | unsigned | 64 | from 1 to 100000 | next connection or operation | gameserver, loginserver | How many messages a connection may send that are dropped unread before a drop counts as a strike. |
| `Network.DroppedMessagesPerSecond` | unsigned | 16 | from 1 to 100000 | next connection or operation | gameserver, loginserver | How fast that allowance of dropped messages refills, per second. |
| `Network.HandoffGrace` | unsigned | 30 s | from 1 to 3600 s | next connection or operation | loginserver | How long a client sent to a game server may keep its login connection open. |
| `Network.KeepAliveInterval` | unsigned | 60 s | from 0 to 3600 s | next connection or operation | gameserver, loginserver | How often an idle connection is asked whether it is still there; 0 never asks. |
| `Network.KeepAliveTimeout` | unsigned | 15 s | from 1 to 3600 s | next connection or operation | gameserver, loginserver | How long a keepalive may go unanswered before the connection is closed. |
| `Network.MaxStrikes` | unsigned | 10 | from 1 to 1000 | next connection or operation | gameserver, loginserver | How many refused or malformed messages a connection may send before it is closed. |
| `Network.PingBurst` | unsigned | 16 | from 1 to 100000 | next connection or operation | gameserver, loginserver | How many pings a connection may send at once before a ping counts as a strike. |
| `Network.PingsPerSecond` | unsigned | 4 | from 1 to 100000 | next connection or operation | gameserver, loginserver | How fast that allowance of pings refills, per second. |
| `Network.SessionAcceptTimeout` | unsigned | 15 s | from 1 to 3600 s | next connection or operation | gameserver, loginserver | How long a new connection may take to finish its handshake. |

## Rates

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Rate.Drop.Item` | float | 1 times | from 0 to 100 times | live | gameserver | Multiplies the chance of each item a defeated creature may drop. |
| `Rate.Gold.Kill` | float | 1 times | from 0 to 100 times | live | gameserver | Multiplies the gold a defeated creature drops. |
| `Rate.Gold.Quest` | float | 1 times | from 0 to 100 times | live | gameserver | Multiplies the gold a quest gives. |
| `Rate.Respawn` | float | 1 times | from 0.1 to 100 times | live | gameserver | Multiplies how long a defeated creature takes to return. |
| `Rate.XP.Kill` | float | 1 times | from 0 to 100 times | live | gameserver | Multiplies the experience a defeated creature gives. |
| `Rate.XP.Quest` | float | 1 times | from 0 to 100 times | live | gameserver | Multiplies the experience a quest gives. |

## Realms

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `PublicAddress` | string | empty | at most 255 bytes | next connection or operation | gameserver | The address players reach this game server at, used when Realm.Address is empty. |
| `Realm.Address` | string | empty | at most 255 bytes | next connection or operation | gameserver | The address the login server sends players to for this realm; empty uses PublicAddress, then BindIP. |
| `Realm.DefaultRealm` | string | empty | at most 64 bytes | live | loginserver | The realm a player is sent to when their client names none; a name no realm online has falls through to the least-full realm. |
| `Realm.HeartbeatInterval` | unsigned | 30 s | from 1 to 3600 s | live | gameserver, loginserver | How often a game server tells the login server it is up, and the beat the login server counts missed heartbeats by. |
| `Realm.Name` | string | Ambrose | at most 64 bytes | next connection or operation | gameserver | The realm's name, announced to the login server with each heartbeat and sent in MSG_LOGINCOMPLETE. |
| `Realm.OfflineAfterIntervals` | unsigned | 3 | from 1 to 1000 | live | loginserver | How many heartbeats a realm may miss before no player is sent to it. |
| `Realm.RefreshInterval` | unsigned | 10 s | from 1 to 3600 s | live | loginserver | How often the login server rereads the realmlist table. |

## World

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `LoginComplete.CSRSecurityLevel` | unsigned | 2 | from 0 to 4 | next connection or operation | gameserver | The account security level from which MSG_LOGINCOMPLETE opens the client's game master tools. |
| `LoginComplete.Permissions` | unsigned | 47 | from 0 to 4294967295 | next connection or operation | gameserver | The permission bits MSG_LOGINCOMPLETE gives a wizard: 0x1 and 0x4 chat level, 0x2 and 0x8 show chat, 0x20 gifting, 0x40 test features, 0x400 paying, 0x1000 earning crowns. |
| `LoginComplete.TestServer` | bool | false | none | next connection or operation | gameserver | Whether MSG_LOGINCOMPLETE tells the client it is on a test server. |
| `World.Heartbeat` | unsigned | 60 s | from 0 to 86400 s | live | gameserver | How often the world logs that it is still ticking; 0 turns the line off. |
| `World.UpdateInterval` | unsigned | 50 ms | from 1 to 10000 ms | live | gameserver | How long the world waits between ticks. |

## Zones

| Key | Type | Default | Bounds | Applies | Apps | What it does |
|---|---|---|---|---|---|---|
| `Zone.MobileIdReleaseDelay` | unsigned | 2000 ms | from 0 to 60000 ms | next connection or operation | gameserver | How long a mobile id rests after its wizard leaves before another wizard may take it. |
| `Zone.UnloadDelay` | unsigned | 60 s | from 0 to 86400 s | next connection or operation | gameserver | How long an empty zone instance stays loaded, read when its last wizard leaves. |

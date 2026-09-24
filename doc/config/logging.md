<!-- Project Ambrose by Imjustchico: Logging levels, appenders, loggers, routing, reload behavior, and troubleshooting. -->
# Logging

Every app logs through named loggers. Code writes `LOG_INFO("server.gameserver", "Realm {} online", name)`, and the `Appender.*` and `Logger.*` options decide where each line goes. Format strings are checked at compile time, so a wrong argument count or type does not build.

## Levels

| Number | Name | Macro |
|---|---|---|
| 0 | disabled | |
| 1 | trace | `LOG_TRACE` |
| 2 | debug | `LOG_DEBUG` |
| 3 | info | `LOG_INFO` |
| 4 | warn | `LOG_WARN` |
| 5 | error | `LOG_ERROR` |
| 6 | fatal | `LOG_FATAL` |

Levels may be written as the number or the name in any case; `warning` also means 4. A level lets through its own messages and everything more severe. `LOG_FATAL` flushes every appender before it returns and does not stop the app.

A disabled message costs a few atomic reads. Its arguments are not evaluated and nothing is allocated.

## Appenders

`Appender.<Name> = <Type>,<Level>,<Flags>[,fields...]`

Names start with a letter and use letters, digits and `_`. Fields are separated by commas; wrap a field in double quotes to keep commas or spaces inside it.

| Type | Name | Fields after Flags |
|---|---|---|
| 1 | Console | `[Colors]` |
| 2 | File | `FileName[,Mode[,MaxFileSize[,MaxBackups[,FlushIntervalMs]]]]` |
| 3 | Stream | `[Backlog]` |
| 4 | DB | `[RealmId]`; rows go to the login database's `logs` table (`logged_at` in Unix milliseconds, `realm_id`, `category`, `level` as the number above, `message` up to 65535 bytes of repaired UTF-8) in batched transactions once the app opens that database. It never receives `sql` categories, keeps up to 10000 rows while the database is busy or closed, drops the oldest beyond that, and reports drops under `server.logging` |
| 5-99 | | Reserved for core |
| 100-255 | | Modules |

### Flags

Add the values together, in decimal or `0x` hex.

| Value | Meaning | Applies to |
|---|---|---|
| 0x01 | Prefix `YYYY-MM-DD_HH:MM:SS.mmm` | Console, File |
| 0x02 | Prefix the level, padded to five characters | Console, File |
| 0x04 | Prefix `[category]` | Console, File |
| 0x08 | Add `_YYYY-MM-DD_HH-MM-SS` to the file name | File |
| 0x10 | With mode `w`, rename a non-empty existing file to its timestamped name first | File |
| 0x20 | Prefix `T<thread id>` after the level | Console, File |

The prefix order is always time, level, thread, category. Flags 7 give `2026-09-13_15:53:28.123 INFO  [server.gameserver] Realm online`.

### Console

Colors are six codes separated by spaces, in the order fatal, error, warn, info, debug, trace. The default is `1 9 3 13 7 7`, which is doc/DESIGN.md's terminal line in the sixteen terminal colors: red for errors, gold for warnings, teal for a healthy line and grey for everything that is only detail. The timestamp, thread and category of a line are always grey, whatever the level's color is, so the words stand out from what marks them.

| Code | Color | Code | Color |
|---|---|---|---|
| 0 | black | 8 | yellow |
| 1 | red | 9 | light red |
| 2 | green | 10 | light green |
| 3 | brown | 11 | light blue |
| 4 | blue | 12 | light magenta |
| 5 | magenta | 13 | light cyan |
| 6 | cyan | 14 | white |
| 7 | grey | 15 | terminal default |

Grey, code 7, is the quiet color: it is written as the terminal's dim grey, SGR 90, and as the console's dark grey attribute where virtual terminal sequences are not available, so it reads as quieter than the message it marks on a light background as well as a dark one.

`Console.Colors` chooses when colors are used: 0 never, 1 only when standard output is a terminal, 2 always. With 1, a non-empty `NO_COLOR` environment variable turns colors off and `CLICOLOR_FORCE=1` turns them on. A change applies from the next line written, so a configuration reload needs no restart. Redirected output such as `gameserver > out.log` gets plain text, with no escape sequence of any kind. Windows consoles get virtual terminal sequences, enabled as the process starts and falling back to console text attributes on old consoles, and the console mode is restored at shutdown.

Every console line, whether it comes from a logger or from a command's answer, is written through one writer, so the two never mix inside a line. When the app reads commands from a terminal, that writer also erases and redraws the `Ambrose> ` prompt around each line, so a line arriving while a command is half typed leaves the typed text on screen. The prompt is held to one row of the window: a command wider than the window scrolls sideways around the cursor rather than wrapping, and the whole command is written out in full when it is entered.

### File

| Field | Default | Rules |
|---|---|---|
| FileName | required | UTF-8, relative to `LogsDir` unless absolute; subfolders are allowed; `%s` is not allowed |
| Mode | `a` | `a` appends; `w` rewrites the file the first time it opens in the process, so a reload never truncates it |
| MaxFileSize | `0` | `0` never rotates; otherwise at least `1K`, with an optional `K`, `M` or `G` suffix |
| MaxBackups | `0` | 0-1000 rotated files to keep; `0` keeps all |
| FlushIntervalMs | `0` | 0-60000; `0` writes every line immediately; error and fatal lines always write immediately |

When a file would grow past MaxFileSize it is renamed to `Name_YYYY-MM-DD_HH-MM-SS[_N].ext` and a new file starts. The oldest backups beyond MaxBackups are deleted. If the rename fails, for example because another program holds the file open, a line saying so is written and logging continues in the same file. If writing fails, lines are counted and the file is reopened every 5 seconds; the first line after recovery reports how many were lost.

Examples: `Appender.World = 2,2,7,World.log,a,64M,10,1000` and `Appender.Session = 2,3,15,Session.log,w`.

Two File appenders may not write to the same file.

### Stream

The Stream appender keeps the last `Backlog` records (0-100000, default 1000) in memory and hands every record to live subscribers such as the admin API's log stream. A slow subscriber loses its oldest records and is told how many; it never slows logging down. Only one Stream appender is allowed.

### Pending appenders

An appender whose type is not registered yet, such as `Appender.DB = 4,2,0` before the database pools open, is not an error. It keeps up to `Log.PendingBuffer` lines and replays them in order when its type registers. The app logs a warning for appenders still pending at startup.

## Loggers

`Logger.<name> = <Level>,<Appender>[ <Appender>...]`

Names are `root` or dot-separated segments such as `sql.updates`. Appender names are separated by spaces or commas. `Logger.root` is required.

A message goes to the logger with the longest matching name at a dot boundary. `sql.sql.x` uses `Logger.sql.sql` if it exists, then `Logger.sql`, then `Logger.root`; `sqlx` uses `Logger.root`. The message is written when both the logger's level and the appender's level let it through.

| Example | Effect |
|---|---|
| `Logger.sql.sql = warn,Console` | Query logging shows only warnings and worse |
| `Logger.server.gameserver = 2,Console DB` | Game server debug lines go to the console and the database |
| `Logger.network = 0,Console` | Silences every `network.*` category |

## Categories

| Category | Used for |
|---|---|
| `server.<app>` | App lifecycle and each app's own events. The login server logs each admitted login, each kick for a second login, each client dropped for idling and the shutdown notice with how many notices were still unwritten when the wait ended at Info, each failed login with its error at Info within a budget shared by every session and at Debug beyond it, address lockouts at Warn, and each decoded authentication request and each request refused while its address is locked out at Debug |
| `server.config` | Configuration warnings |
| `server.logging` | Logging problems such as dropped lines |
| `network`, `network.opcode` | Sockets and client messages by protocol and name: handled messages at Debug, messages not handled yet at Info, and messages dropped for their session status or refused with a strike, protocol errors, and connections closed at the send queue limit at Warn. A session's dropped and refused messages are logged only within `Network.DroppedMessageBurst`, so a flood from one client cannot fill the logs |
| `network.session` | Session offers, accepts and kicks at Info; sessions closed for too many strikes, a full inbound queue, a failed handler, a protocol error, a missing SessionAccept or silence after a keepalive at Warn; strikes, keepalives in both directions and each closed session at Debug; and messages that could not be sent at Error |
| `sql.sql`, `sql.updates`, `sql.driver` | Database queries, updates and connections |
| `accounts` | Accounts created, passwords, security levels, locks and bans, and verifiers that do not open |
| `characters` | Characters created, deleted and restored at Info, and a commit that reported a failure while the character was stored at Warn |
| `commands.console` | Console lines accepted, refused during shutdown, dropped when the queue is full, and the input closing. Arguments of commands marked sensitive, such as `account create`, are never written |

## Async mode

With `Log.Async.Enable = 1`, lines are written on a dedicated thread. `Log.Async.QueueSize` bounds the queue; when it is full, `Log.Async.QueueFull = 0` makes the logging thread wait and `1` drops the line and later logs how many were dropped. Shutdown and normal process exit write every queued line first. A crash loses queued lines, so the shipped default is synchronous.

## Environment and reload

Options resolve through the configuration layers, so `AMBROSE_LOGGER_SQL_SQL=4,Console` changes an existing `Logger.sql.sql` line. An environment variable cannot add a new `Appender.*` or `Logger.*` key, because only keys present in a file or a command-line override are discovered.

The reload triggers, added in milestones 4.15 and 17.12, are listed in [README.md](README.md). No logging option needs a restart.

Reloading configuration applies all logging options at once, including `LogsDir` and `Log.Utc` for appenders whose own lines did not change. If any option or route is invalid, nothing changes. The one exception is a file that cannot be opened: the reload fails, but other files named in the same reload may already have been created. Files are kept open and never truncated by a reload, Stream subscribers stay connected, and pending buffers are kept unless `Log.PendingBuffer` changes.

Async mode cannot be switched from inside an appender running on the logging thread; that reload fails with an error.

## Message text

Console and file output is always LF-terminated, valid UTF-8. A message with several lines gets the full prefix on every line, so text from players cannot forge a log line. Control characters are shown as `\xHH` (for example `\x1B`), C1 controls as `\u0080` to `\u009F`, and invalid UTF-8 bytes become U+FFFD. Stream and database records keep the original text.

## Rules for appender authors

- Register a type with `sLog.RegisterAppender<T>()`, where `T::GetTypeInfo()` returns the type id, a factory, and any categories the type must never receive.
- Never read configuration inside `Write`; read it in the factory.
- A log call made from inside `Write` is delivered to the other appenders after the current message, never back to the same appender. Appenders that must not receive such messages override `AcceptsNestedMessages`.
- Never block inside `Write` on work that itself logs.
- Unregister a type with `sLog.UnregisterAppenderType` before destroying anything its appenders use.

## Troubleshooting

| Message | Fix |
|---|---|
| `level 'X' is out of range` | Use 0-6 or a level name |
| `type 'X' is not valid` | Use 1-255 or Console, File, Stream, DB |
| `flags 'X' are not valid` | Combine only the values in the flags table |
| `appender names cannot contain '.'` | Rename the appender |
| `references appender 'X', which is not defined` | Define `Appender.X` or fix the spelling |
| `Logger.root is required` | Add `Logger.root = 3,Console` |
| `writes to the same file as Appender.X` | Give each File appender its own file |
| `only one Stream appender is allowed` | Remove the extra Stream appender |
| `cannot open <path>` | Check the path and permissions, or that no folder has that name |
| `routes to appender 'X', whose type never accepts category 'Y'` | Route that category to a different appender |
| `appender 'X' uses a type no layer in this app registers` | Remove the appender or run the app that provides the type |

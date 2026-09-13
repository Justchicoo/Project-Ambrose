<!-- Project Ambrose by Imjustchico: Every option in gameserver.conf.dist with its type, default, and meaning. -->
# gameserver options

See doc/config/README.md for the file format, layers, and environment variable names, and doc/config/logging.md for the full logging grammar.

| Option | Type | Default | Environment variable | Meaning |
|---|---|---|---|---|
| `LogsDir` | string | `logs` | `AMBROSE_LOGS_DIR` | Folder for log files, relative to the working directory unless absolute; created when a File appender exists |
| `WorldServerPort` | uint16 | `12000` | `AMBROSE_WORLD_SERVER_PORT` | TCP port the realm listens on for game clients |
| `Log.Async.Enable` | bool | `0` | `AMBROSE_LOG_ASYNC_ENABLE` | Write log lines on a dedicated thread; shutdown and exit drain every queued line |
| `Log.Async.QueueSize` | uint32 | `65536` | `AMBROSE_LOG_ASYNC_QUEUE_SIZE` | Queued lines before the full-queue policy applies (1024-16777216) |
| `Log.Async.QueueFull` | uint8 | `0` | `AMBROSE_LOG_ASYNC_QUEUE_FULL` | 0 waits for room, 1 drops the line and logs the drop count |
| `Log.Utc` | bool | `0` | `AMBROSE_LOG_UTC` | Timestamps and file names in UTC instead of local time |
| `Log.PendingBuffer` | uint32 | `1000` | `AMBROSE_LOG_PENDING_BUFFER` | Lines kept for an appender whose type registers later, such as DB |
| `Console.Colors` | uint8 | `1` | `AMBROSE_CONSOLE_COLORS` | 0 never, 1 when stdout is a terminal, 2 always; `NO_COLOR` disables 1 |
| `Appender.Console` | appender | `1,3,3,"1 9 3 6 5 8"` | `AMBROSE_APPENDER_CONSOLE` | Colored console output at Info with time and level |
| `Appender.Server` | appender | `2,2,7,Server.log,w` | `AMBROSE_APPENDER_SERVER` | Server.log in LogsDir, rewritten each start, with time, level and category |
| `Appender.Errors` | appender | `2,4,7,Errors.log,a,16M,10` | `AMBROSE_APPENDER_ERRORS` | Warnings and worse, appended across restarts, rotated at 16 MiB keeping 10 backups |
| `Appender.Stream` | appender | `3,2,0,1000` | `AMBROSE_APPENDER_STREAM` | Live records for the admin API with a 1000-record backlog |
| `Logger.root` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_ROOT` | Required fallback for every category |
| `Logger.server` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_SERVER` | App lifecycle, config and logging messages |
| `Logger.sql` | logger | `4,Console Server Errors Stream` | `AMBROSE_LOGGER_SQL` | Database messages, warnings and worse |
| `Logger.network` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_NETWORK` | Sockets and message traffic |

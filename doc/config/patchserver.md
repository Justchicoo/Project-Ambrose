<!-- Project Ambrose by Imjustchico: Every option in patchserver.conf.dist with its type, default, and meaning. -->
# patchserver options

See doc/config/README.md for the file format, layers, environment variable names, and reload triggers, and doc/config/logging.md for the full logging grammar.

The Applies column says when a changed value takes effect after a configuration reload or a live edit. No option in this file needs a restart. The socket layer applies the network options through `SocketMgr::ApplySettings`; the reload triggers that call it arrive in milestone 4.15.

| Option | Type | Default | Environment variable | Applies | Meaning |
|---|---|---|---|---|---|
| `LogsDir` | string | `logs` | `AMBROSE_LOGS_DIR` | Live | Folder for log files, relative to the working directory unless absolute; created when a File appender exists |
| `BindIP` | string | `0.0.0.0` | `AMBROSE_BIND_IP` | Rebinds live like the port option | Local address the listener binds; 0.0.0.0 listens on every IPv4 address |
| `PatchServerPort` | uint16 | `12500` | `AMBROSE_PATCH_SERVER_PORT` | Rebinds live; the new listener opens before the old one closes, and a failed bind keeps the old one | TCP port the patch server listens on for clients |
| `Network.Threads` | uint32 | `1` | `AMBROSE_NETWORK_THREADS` | Live; new threads start at once, and removed threads stop taking sockets and exit when their last connection closes | Network threads that read and write sockets (1-256) |
| `Network.MaxFrameSize` | uint64 | `4194304` | `AMBROSE_NETWORK_MAX_FRAME_SIZE` | Next connection | Largest frame in bytes a client may send, checked before the frame is buffered (17 to 1 GiB) |
| `Network.MaxDmlMessages` | uint32 | `1024` | `AMBROSE_NETWORK_MAX_DML_MESSAGES` | Next connection | Most DML messages one frame may chain (at least 1) |
| `Network.MaxSendQueueBytes` | uint64 | `16777216` | `AMBROSE_NETWORK_MAX_SEND_QUEUE_BYTES` | Next connection | Most bytes a connection may have waiting to be sent (1 MiB to 1 GiB). A frame that would pass the limit is not queued and the connection is closed, so a client that stops reading cannot grow server memory without bound |
| `Network.LongFrameLength` | string | `BodyOnly` | `AMBROSE_NETWORK_LONG_FRAME_LENGTH` | Next connection | What a long frame's 32-bit length counts: `BodyOnly` or `HeaderAndBody`, until doc/CAPTURE.md settles it |
| `Network.OutKBuff` | int32 | `-1` | `AMBROSE_NETWORK_OUT_K_BUFF` | Next connection | Socket send buffer in bytes; -1 keeps the operating system default |
| `Network.TcpNoDelay` | bool | `1` | `AMBROSE_NETWORK_TCP_NO_DELAY` | Next connection | Disable Nagle's algorithm so small frames go out at once |
| `Log.Async.Enable` | bool | `0` | `AMBROSE_LOG_ASYNC_ENABLE` | Live | Write log lines on a dedicated thread; shutdown and exit drain every queued line |
| `Log.Async.QueueSize` | uint32 | `65536` | `AMBROSE_LOG_ASYNC_QUEUE_SIZE` | Live | Queued lines before the full-queue policy applies (1024-16777216) |
| `Log.Async.QueueFull` | uint8 | `0` | `AMBROSE_LOG_ASYNC_QUEUE_FULL` | Live | 0 waits for room, 1 drops the line and logs the drop count |
| `Log.Utc` | bool | `0` | `AMBROSE_LOG_UTC` | Live | Timestamps and file names in UTC instead of local time |
| `Log.PendingBuffer` | uint32 | `1000` | `AMBROSE_LOG_PENDING_BUFFER` | Live | Lines kept for an appender whose type registers later, such as DB |
| `Console.Colors` | uint8 | `1` | `AMBROSE_CONSOLE_COLORS` | Live, from the next line written | 0 never, 1 when stdout is a terminal, 2 always; `NO_COLOR` disables 1 |
| `Console.Enable` | bool | `1` | `AMBROSE_CONSOLE_ENABLE` | At startup, and live once 4.15's reload triggers restart the console reader | Read commands such as `help`, `status` and `shutdown` from standard input; 0 starts no input thread. On a terminal the app draws an `Ambrose> ` prompt with line editing, history and tab completion of command names; a redirected input is read line by line instead. A closed or redirected-from-nothing input leaves the server running, and a terminal owned by another foreground job is not read |
| `Appender.Console` | appender | `1,3,3,"1 9 3 13 7 7"` | `AMBROSE_APPENDER_CONSOLE` | Live | Colored console output at Info with time and level |
| `Appender.Server` | appender | `2,2,7,Patch.log,w` | `AMBROSE_APPENDER_SERVER` | Live | Patch.log in LogsDir, rewritten each start, with time, level and category |
| `Appender.Errors` | appender | `2,4,7,PatchErrors.log,a,16M,10` | `AMBROSE_APPENDER_ERRORS` | Live | Warnings and worse, appended across restarts, rotated at 16 MiB keeping 10 backups |
| `Appender.Stream` | appender | `3,2,0,1000` | `AMBROSE_APPENDER_STREAM` | Live | Live records for the admin API with a 1000-record backlog |
| `Logger.root` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_ROOT` | Live | Required fallback for every category |
| `Logger.server` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_SERVER` | Live | App lifecycle, config and logging messages |
| `Logger.sql` | logger | `4,Console Server Errors Stream` | `AMBROSE_LOGGER_SQL` | Live | Database messages, warnings and worse |
| `Logger.network` | logger | `3,Console Server Errors Stream` | `AMBROSE_LOGGER_NETWORK` | Live | Sockets and message traffic |

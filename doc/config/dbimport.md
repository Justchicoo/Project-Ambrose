<!-- Project Ambrose by Imjustchico: Every option in dbimport.conf.dist with its type, default, and meaning. -->
# dbimport options

See doc/config/README.md for the file format, layers and environment variable names, doc/config/logging.md for the logging grammar, and doc/config/loginserver.md for the database connection string form.

dbimport creates and updates the databases `Updates.EnableDatabases` selects, opens each pool once to prove it works, then exits 0, or 1 on the first failure. It reads its configuration once, so no option applies live.

| Option | Type | Default | Environment variable | Meaning |
|---|---|---|---|---|
| `LogsDir` | string | `logs` | `AMBROSE_LOGS_DIR` | Folder for log files, relative to the working directory unless absolute |
| `LoginDatabaseInfo` | string | `127.0.0.1;3306;ambrose;ambrose;ambrose_login` | `AMBROSE_LOGIN_DATABASE_INFO` | Login database; empty skips it |
| `LoginDatabase.WorkerThreads` | uint32 | `1` | `AMBROSE_LOGIN_DATABASE_WORKER_THREADS` | Async connections opened to prove the login pool works (0-64) |
| `LoginDatabase.SynchThreads` | uint32 | `1` | `AMBROSE_LOGIN_DATABASE_SYNCH_THREADS` | Blocking connections opened to prove the login pool works (1-64) |
| `CharacterDatabaseInfo` | string | `127.0.0.1;3306;ambrose;ambrose;ambrose_characters` | `AMBROSE_CHARACTER_DATABASE_INFO` | Characters database; empty skips it |
| `CharacterDatabase.WorkerThreads` | uint32 | `1` | `AMBROSE_CHARACTER_DATABASE_WORKER_THREADS` | Async connections for the characters pool (0-64) |
| `CharacterDatabase.SynchThreads` | uint32 | `1` | `AMBROSE_CHARACTER_DATABASE_SYNCH_THREADS` | Blocking connections for the characters pool (1-64) |
| `WorldDatabaseInfo` | string | `127.0.0.1;3306;ambrose;ambrose;ambrose_world` | `AMBROSE_WORLD_DATABASE_INFO` | World database; empty skips it |
| `WorldDatabase.WorkerThreads` | uint32 | `1` | `AMBROSE_WORLD_DATABASE_WORKER_THREADS` | Async connections for the world pool (0-64) |
| `WorldDatabase.SynchThreads` | uint32 | `1` | `AMBROSE_WORLD_DATABASE_SYNCH_THREADS` | Blocking connections for the world pool (1-64) |
| `MaxPingTime` | uint32 | `30` | `AMBROSE_MAX_PING_TIME` | Minutes an idle connection waits before it pings |
| `Updates.EnableDatabases` | uint32 | `7` | `AMBROSE_UPDATES_ENABLE_DATABASES` | Bitmask of databases to create and update: 1 login, 2 characters, 4 world; a missing key means 0, none |
| `Updates.AutoSetup` | bool | `1` | `AMBROSE_UPDATES_AUTO_SETUP` | Create a missing database with utf8mb4; an empty database always gets its base imported |
| `Updates.SourcePath` | string | empty | `AMBROSE_UPDATES_SOURCE_PATH` | The Project Ambrose folder that holds `data/sql`; empty uses the folder dbimport was built from, then `share/ambrose` next to an installed `bin` |
| `Updates.Redundancy` | bool | `0` | `AMBROSE_UPDATES_REDUNDANCY` | At each updater run | Re-apply an already recorded update whose non-empty hash changed; when disabled, the updater reports the edited file as an error |
| `Updates.AllowRehash` | bool | `0` | `AMBROSE_UPDATES_ALLOW_REHASH` | At each updater run | Fill an empty recorded hash from the file on disk; it never overwrites a non-empty hash |
| `Updates.CleanDeadRefMaxCount` | int32 | `3` | `AMBROSE_UPDATES_CLEAN_DEAD_REF_MAX_COUNT` | At each updater run | Delete missing applied-file rows when their count is at most this value; `0` keeps them and `-1` allows any count, while exceeding a positive limit errors without deleting |
| `Updates.AllowPending` | bool | `0` | `AMBROSE_UPDATES_ALLOW_PENDING` | At each updater run | In development only, include `pending_db_<name>` files named `rev_<unix-timestamp>_<slug>.sql` and record them as `PENDING` |
| `Log.Async.Enable` | bool | `0` | `AMBROSE_LOG_ASYNC_ENABLE` | Write log lines on a dedicated thread |
| `Log.Utc` | bool | `0` | `AMBROSE_LOG_UTC` | Timestamps and file names in UTC |
| `Console.Colors` | uint8 | `1` | `AMBROSE_CONSOLE_COLORS` | 0 never, 1 when stdout is a terminal, 2 always |
| `Appender.Console` | appender | `1,3,3,"1 9 3 13 7 7"` | `AMBROSE_APPENDER_CONSOLE` | Colored console output at Info with time and level |
| `Appender.DBImport` | appender | `2,2,7,DBImport.log,w` | `AMBROSE_APPENDER_DBIMPORT` | DBImport.log in LogsDir, rewritten each run |
| `Logger.root` | logger | `3,Console DBImport` | `AMBROSE_LOGGER_ROOT` | Everything at Info |
| `Logger.sql` | logger | `4,Console DBImport` | `AMBROSE_LOGGER_SQL` | Database warnings and worse |
| `Logger.sql.driver` | logger | `3,Console DBImport` | `AMBROSE_LOGGER_SQL_DRIVER` | Pools opening and closing |
| `Logger.sql.updates` | logger | `3,Console DBImport` | `AMBROSE_LOGGER_SQL_UPDATES` | Database creation, base imports and applied updates |

<!-- Project Ambrose by Imjustchico: A practical guide to reading Ambrose startup and runtime logs. -->

# Reading Ambrose logs

This guide explains how to configure, read, and troubleshoot the logs written by Ambrose applications. It was checked against the logging reference in `doc/config/logging.md`, the application configuration references, and the current contributor-tool validation scripts on 2026-09-18.

## Start with a file log

Each application logs to the console and can also write a file log. The file appender is configured in the application's `.conf` file:

```ini
LogsDir = logs
Appender.Server = 2,2,7,Server.log,a
Logger.root = 3,Server
```

The appender fields are:

```text
Appender.<name> = <type>,<level>,<flags>,<file-name>,<mode>,<max-size>,<backups>,<flush-ms>
```

For a file appender:

- type `2` means file;
- the level is the minimum level written;
- flag `7` adds the timestamp, level, and category;
- mode `a` appends to the current file;
- `LogsDir` is the base directory for relative file names.

The file is created below the application's working directory unless `LogsDir` is absolute. A file appender with a zero flush interval writes each line immediately. Keep that setting while diagnosing startup or connection failures.

## Read a healthy startup

A healthy startup should show these stages in order:

1. Configuration is loaded and any invalid values are reported.
2. The configured loggers and appenders become active.
3. The application opens the databases it needs.
4. The application binds its listener.
5. The application reports that it is ready to accept work.

The exact wording varies by application and configuration. Use the category in brackets, rather than the message text, to decide which subsystem owns a line.

Common categories include:

| Category | What it covers |
| --- | --- |
| `server.<app>` | Application lifecycle, accepted work, shutdown, and application-level failures |
| `server.config` | Configuration warnings and rejected values |
| `server.logging` | Appender failures and dropped log records |
| `network` | Socket lifecycle and session behavior |
| `network.session` | One session's own life: attach, keepalive, idle and close |
| `server.admin` | The admin API listener, its authentication and its refusals |
| `server.threading` | Worker threads starting, stopping and failing |
| `server.loading` | Data loaded at startup and what it cost |
| `sql.updates` | Database update files applied, skipped or refused |
| `sql.driver` | Connection pool and driver-level failures |
| `network.opcode` | Client message decoding, dispatch, refused messages, and protocol violations |
| `sql` | Database connection and update activity |
| `sql.sql` | Query-level diagnostics when enabled |

## Read the console line

A console attached to a terminal lays each record out in fixed columns, so the eye scans down rather than across:

```text
21:12:54.502 INFO  [server.loginserver] Session 3 authenticated as example (id 1)
21:12:54.884 WARN  [network.opcode   ] Session 3 sent MSG_CREATECHARACTER, which the login server does not handle yet
```

The time is `HH:MM:SS.mmm`, the level word is padded to five characters, and the category sits in a column of its own, `Console.CategoryWidth` wide and 18 by default, so every message begins at the same column whatever the category. A category longer than the column is shortened in the middle, with the whole name still written to every other appender, so never read a shortened name as the real one.

Color marks the parts that carry meaning and nothing else. The level word alone takes the level's color, the message body stays body text at every level, the timestamp is faint and brightens on the first line of each new second, and the category is quiet. A debug or trace record is dimmed whole, which is how a detail line reads as a detail before you have read a word of it. A line is never tinted end to end by its level, so color inside a message means something specific rather than repeating what the level word already says.

Redirected output is deliberately different and deliberately unchanged: the full date, the category unpadded, no colors and no escape bytes at all. A log piped to a file or another tool is the same bytes it has always been, so anything that parses it keeps working.

`Console.Timestamp` takes `short`, `full` or `off`, `Console.CategoryWidth` takes 0 to write the category inline and unpadded, and `Console.RepeatCategory = 0` blanks the column when a line repeats the category above it. `Console.Colors` on the console appender still takes six codes in the order fatal, error, warn, info, debug, trace, and also takes named pairs such as `warn=14 body=15 category=8`, where a name is a level or one of body, value, category, timestamp, mark and punctuation.

## Understand severity

Ambrose levels are numeric and ordered:

| Level | Name | Meaning |
| ---: | --- | --- |
| 0 | disabled | No records |
| 1 | trace | Very detailed diagnostics |
| 2 | debug | Developer diagnostics |
| 3 | info | Normal operation |
| 4 | warn | An operation continued with a problem |
| 5 | error | An operation failed |
| 6 | fatal | A serious failure was recorded; the application does not necessarily exit |

A logger and an appender both filter a record. A debug line appears only when both the matching logger and destination appender allow level 2.

For a temporary diagnostic increase, lower the relevant logger threshold instead of enabling trace globally:

```ini
Logger.network = 2,Server
Logger.network.opcode = 2,Server
```

Restore the normal level after reproducing the issue. Very verbose protocol logging can expose session details and produce large files.

## Read message and session lines

Message dispatch lines use the `network` or `network.opcode` categories. The useful distinctions are:

- a message that is not handled yet is logged at info;
- a message refused because the app never accepts it or because it is server-only is logged with a strike;
- an unknown message includes its service and order pair;
- a malformed message body is reported as a dropped or decode-failed message;
- a message received in the wrong session status is reported separately from an unhandled message.

The message watcher in `contrib/tools/ambrose-message-watcher` can summarize refused or unhandled lines from a saved log:

```powershell
.\ambrose-message-watcher.exe logs\Server.log
```

Use follow mode while reproducing an issue:

```powershell
.\ambrose-message-watcher.exe logs\Server.log --follow
```

The watcher is an investigation aid. Confirm any conclusion against the complete log because per-session dropped-message logging is budgeted, and a session can stop emitting individual lines after the budget is exhausted.

## Diagnose common startup failures

### No log file appears

Check that:

1. `Logger.root` names an existing appender.
2. The appender type is `2`.
3. The file name is present and does not point at a directory.
4. The process can create `LogsDir`.
5. Another process is not holding a rotated file open.

The console remains the first place to look when the file appender itself cannot initialize.

### The file is empty or missing recent lines

Check the appender's flush interval and level. Error and fatal records flush immediately, but normal records follow the configured interval. A `server.logging` warning indicates that an appender could not write and that records may have been lost.

### The application stops during startup

Find the first error or fatal line, not the last line printed. Database connection errors, missing configuration, invalid client-data paths, and listener bind failures can prevent readiness. If the message names a setting, fix that setting before raising logger verbosity.

### The server is ready but the client is stuck

Capture the lines from the first connection through the stuck screen. Compare:

- `network` session and keepalive activity;
- `network.opcode` message dispatch;
- application lines under `server.loginserver` or `server.gameserver`;
- any warning or error between the last expected message and the timeout.

Do not treat a quiet log as proof that no traffic occurred: a logger may be filtering debug records, or a per-session budget may have suppressed later dropped-message lines.

## Preserve useful evidence

When reporting a log problem, include:

- application name and revision;
- operating system and build configuration;
- the relevant `.conf` logger and appender settings;
- the first error and the lines immediately before it;
- whether the process was started from a terminal or through another tool;
- the exact reproduction steps.

Remove credentials, database connection strings, access tokens, account names, and client-derived data before sharing a log. Keep the original file private and share a short, redacted excerpt.

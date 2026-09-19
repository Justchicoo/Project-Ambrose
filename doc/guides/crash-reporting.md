<!-- Project Ambrose by Imjustchico: A safe workflow for collecting and reading Ambrose crash evidence. -->

# Reading and reporting a crash

This guide explains what to collect when an Ambrose process exits unexpectedly, how to read the evidence in order, and what to send for investigation. It was checked against the current logging guide, application configuration references, and the repository's clean-room restrictions on 2026-09-19.

## First preserve the scene

Before restarting the process or deleting its work folder, record:

- which application stopped (`loginserver`, `gameserver`, `patchserver`, or a tool);
- the exact command line, without passwords or tokens;
- the commit or build identifier;
- the operating system and compiler configuration;
- whether the process was started from a terminal, a service, or a test preset;
- the local time and timezone;
- the last action that preceded the failure; and
- whether the failure reproduces on a clean work folder.

Do not run KingsIsle's launcher or patcher to reproduce an Ambrose crash. Do not contact KingsIsle servers. Reproduce only against your own Ambrose server and your own installation when a client is required.

## Collect the application log

Use the application's file appender so the evidence survives a terminal closing. A diagnostic configuration can use:

```ini
LogsDir = logs
Appender.Server = 2,2,7,Server.log,a
Logger.root = 3,Server
```

Use the app-specific `.conf.dist` as the starting point and change only the settings needed for the reproduction. Do not commit the local `.conf`, log files, database connection strings, or generated client data.

Copy the log before restarting. Preserve the complete interval from process start through the failure, not only the last screenful. The first error or fatal record is usually more useful than the final line printed after dependent work has stopped.

When reading the file, group records by category:

- `server.<app>` shows application lifecycle and application-level failures.
- `server.config` shows rejected or invalid configuration.
- `server.logging` shows appender failures or dropped records.
- `network` and `network.session` show socket and session lifecycle.
- `network.opcode` shows message decoding, dispatch, refusals, and protocol violations.
- `server.threading` shows worker-thread lifecycle and failures.
- `sql` and `sql.driver` show database connection and update activity.

Use the complete log rather than assuming silence means success. In particular, refused-message logging is budgeted per session; after the budget is exhausted, the absence of more `network.opcode` lines does not prove that traffic stopped.

## Separate a clean exit from a crash

An expected shutdown has an application stop message and a normal process exit. A crash commonly has one or more of:

- an error or fatal record immediately before the process disappears;
- a debugger or operating-system exception report;
- a process exit code that is not the documented success or validation failure;
- a missing graceful-shutdown line; or
- a repeatable failure at the same operation.

Do not call every non-zero exit a crash. A missing configuration, invalid setting, failed database connection, occupied listener, or failed `--check` is a startup failure and should be reported with its command output and exit code.

## Capture a debugger dump

If the process is still running under a debugger, break on the first-chance exception and save the call stack, thread list, and exception type. If it has already stopped, use the operating system's supported user-dump facility or the debugger's dump command for that process. Keep the original dump private until it has been checked for paths, command lines, usernames, tokens, passwords, client data, and database contents.

The useful parts of a dump are:

- the exception or signal;
- the faulting module and instruction;
- the active thread's symbolized stack;
- other threads blocked at the time;
- loaded-module versions;
- the process exit code; and
- the build identifier used to produce the binary.

Send a minidump or a symbolized stack only when the maintainer asks for it and the dump has been redacted or transferred through the project's agreed private channel. A dump can contain arbitrary process memory, including credentials and client-derived data; it is not safe to commit it or attach it to a public issue by default.

If symbols are unavailable, send the raw addresses and the exact binary and commit information privately. Do not guess function names from an address. A stack trace without matching symbols can point at the wrong source line.

## Make the report reproducible

Include a short reproduction such as:

```text
Application: loginserver
Build: <commit or local build id>
Platform: Windows 11, MSVC Debug
Started with: loginserver.exe --check --config <redacted path>
Observed: exits during startup after the database pool opens
Reproduces: 3 of 3 runs in a clean work folder
Exit code: <code>
Evidence: first error line and private dump filename
```

Replace placeholders with observed values. Redact:

- passwords and database connection strings;
- admin tokens and session keys;
- account names and personal paths;
- client archives, assets, dumps, captures, and encoded payloads; and
- any unrelated user's data.

For a client-facing failure, report the screen and the last known Ambrose log category rather than attaching a capture or client file. Captures and screenshots may contain credentials or client-derived content and must stay private unless the maintainer has approved their handling.

## What to send

The smallest useful report normally contains:

1. the redacted reproduction command;
2. the commit or build identifier;
3. operating system and build configuration;
4. the complete relevant log interval;
5. the exit code or exception type;
6. a symbolized stack or debugger summary, if available;
7. whether the failure reproduces; and
8. what changed between a working and failing run.

Send only the evidence needed to reproduce the failure. Keep the original log and dump privately so they can be rechecked if redaction removed context.

## Cheapest disproof

Before reporting a crash, run the same command once with the shipped configuration in a new work folder and compare the exit result. If the clean run succeeds, the problem may be local configuration, data, database state, or a leftover process rather than a code crash. If the failure disappears after removing a local configuration file, preserve that fact and report the configuration difference instead of calling it a crash.

## Verification and limitations

This guide documents the evidence Ambrose's current logs and process boundaries can provide. It does not claim that every platform automatically creates a dump, that every binary has symbols installed, or that a dump can be made safe by removing one field. Confirm the operating system's dump mechanism and the private transfer method before collecting sensitive evidence.

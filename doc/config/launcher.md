<!-- Project Ambrose by Imjustchico: Every option of the launcher, in launcher.conf.dist and on its command line, with the folder the client runs from and what the launcher never touches. -->
# launcher options

The launcher starts your own Wizard101 client against an Ambrose login server. It never runs KingsIsle's launcher or patcher, never writes inside the install, and starts the client from a folder of its own whose configuration reaches nothing outside your machine.

See doc/config/README.md for the file format, the layers and the environment variable names. The launcher reads `launcher.conf` in the working folder, or else beside the program, unless `--config` names another file; with no such file it uses the defaults below. It starts one client and exits, so every value applies to the next run and nothing reloads. `launcher --help` prints the options.

## Settings

| Option | Type | Default | Environment variable | Meaning |
|---|---|---|---|---|
| `ClientDir` | string | empty | `AMBROSE_CLIENT_DIR` | The install to start: the folder that holds `Bin` and `Data`. Empty means the newest install found on this machine, as the servers do it. `AMBROSE_SETUP_MODE=off` or `ask` changes whether that search chooses on its own, as doc/config/gameserver.md describes |
| `LoginHost` | string | `127.0.0.1` | `AMBROSE_LOGIN_HOST` | Login server the client connects to, passed as `-L <host> <port>` |
| `LoginPort` | uint16 | `12000` | `AMBROSE_LOGIN_PORT` | Login server port, 1 to 65535 |
| `Locale` | string | `en-US` | `AMBROSE_LOCALE` | Client locale, passed as `-A`, such as `en-US` or `de` |
| `Window` | string | `1280x720` | `AMBROSE_WINDOW` | Window size, written into the run folder's configuration as `Resolution`, from 320x320 to 16384x16384. The retail client ignores a resolution on its command line, so the size comes only from that configuration |
| `Fullscreen` | uint8 | `0` | `AMBROSE_FULLSCREEN` | `IsFullscreen` in that configuration: 0 windowed, 1 fullscreen, 2 the client's third mode, whose meaning is unconfirmed |
| `WindowX` | int32 | empty | `AMBROSE_WINDOW_X` | `WindowedX`, the window's position across the screen. Empty keeps the install's own value |
| `WindowY` | int32 | empty | `AMBROSE_WINDOW_Y` | `WindowedY`, the window's position down the screen. Empty keeps the install's own value |
| `RunDir` | string | empty | `AMBROSE_RUN_DIR` | The folder the client runs from. Empty means `client/<revision>` in the Ambrose data folder, which is `%LOCALAPPDATA%/ProjectAmbrose` on Windows. A relative path is taken from the working folder |
| `Patch` | bool | `0` | `AMBROSE_PATCH` | Must stay 0. The launcher always starts the client with `-P 0` and refuses to start when this is set, because only KingsIsle's launcher patches a retail install and Ambrose never runs it. Milestone 16.13 adds a player launcher that patches a copy of its own from an Ambrose patch server |

## Command line

| Argument | Meaning |
|---|---|
| `--config <file>` | Read this settings file instead of `launcher.conf` |
| `--client <dir>` | `ClientDir` for this run |
| `--host <host>`, `--port <port>` | `LoginHost` and `LoginPort` for this run |
| `--locale <name>` | `Locale` for this run |
| `--window <width>x<height>` | `Window` for this run |
| `--fullscreen` | `Fullscreen = 1` for this run |
| `--run-dir <dir>` | `RunDir` for this run |
| `--user <id> <key> [name]` | Passed on as the client's own `-U ..<id> <key> [name]`, which makes the client send MSG_USER_VALIDATE instead of showing its login window. The `..` prefix is added when the id does not have it. A server answers that message from milestone 5.06 |
| `--character <name>` | Passed on as the client's own `-C <name>`, which creates or selects that character. A server creates a wizard from milestone 3.16 |
| `--dry-run` | Print the install, the run folder and the exact command, and start and write nothing |
| `--wait` | Wait for the client and exit with its own code. The client runs in a job object that ends it if the launcher is killed, and Ctrl+C ends it too |
| `--tail` | Print the client's own log lines while it runs. It waits for the client as `--wait` does, because the launcher has to stay running to read the log |
| `--help` | Print the usage and exit 0 |

An option overrides the settings file. Values the environment sets override the file too, as doc/config/README.md's layers describe, so `AMBROSE_CLIENT_DIR` is enough to choose the install without a settings file.

Without `--wait` or `--tail` the client is started detached, so closing the launcher leaves the game running.

## The folder the client runs from

The retail client reads `config.xml`, `preferences.xml`, `revision.dat` and `data.dat` by relative name from its working folder, and nothing else it needs is relative to it, so the launcher gives it a folder of Ambrose's own and passes the install's data folder with `-D`. The folder holds:

| File | What it is |
|---|---|
| `config.xml` | The install's own `Bin/config.xml`, or `defaultconfig.xml` from its `Root.wad` when the install has none, with `IsFullscreen`, `Resolution`, `WindowedX` and `WindowedY` set to the window asked for and `SilentMetricsURL` emptied, because the client fetches that address while it starts. Everything else, `VersionInfo` included, stays as the install has it |
| `preferences.xml` | The install's own `Bin/preferences.xml` with the same window keys, because the client's preferences override its configuration |
| `revision.dat`, `data.dat` | Copies of the install's own, the other files the client opens by relative name |
| `launcher.stamp` | The revision and window options the files were written for. The folder is written again when either changes, and left alone when neither did, so the client's own saved settings survive between runs |
| `WizardClient.log` | The client's own log, from `-G`. It is removed before each run, so `--tail` and the 3.24 driver read only the run in front of them |

The client writes its own files there as it runs, such as `state.dat`. Nothing in the folder is committed.

## What the launcher never touches

- The install. It is read for the client program, the configuration templates, the revision files and its `Root.wad`, and nothing in it is ever written.
- KingsIsle's launcher and patcher. The launcher always passes `-L`, because the retail client starts `..\Wizard101.exe` when it sees none of its own options, and it always passes `-P 0`. `-ST` is never passed.
- Anything outside this machine. The generated configuration leaves `SilentMetricsURL` empty, and the client is pointed only at the login server named here.

## Exit status

0 when the client started, or the client's own code with `--wait`. 1 when a refusal names its cause: no install found, a folder that holds no install, a missing client program, patching asked for, a missing or unusable login host or port, a value that makes no sense, a settings file that cannot be read, a run folder that cannot be named or written, or a client that cannot be started. 2 on bad usage.

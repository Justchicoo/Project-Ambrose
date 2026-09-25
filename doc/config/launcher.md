<!-- Project Ambrose by Imjustchico: Every option of the launcher, in launcher.conf.dist and on its command line, with the folder the client runs from, which machines can start a client, and what the launcher never touches. -->
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
| `WindowX` | int32 | empty | `AMBROSE_WINDOW_X` | `WindowedX`, the window's position across the screen. Empty keeps the value the run folder's own configuration already has |
| `WindowY` | int32 | empty | `AMBROSE_WINDOW_Y` | `WindowedY`, the window's position down the screen. Empty keeps the value the run folder's own configuration already has |
| `RunDir` | string | empty | `AMBROSE_RUN_DIR` | The folder the client runs from. Empty means `client/<revision>` in the Ambrose data folder, which is `%LOCALAPPDATA%/ProjectAmbrose` on Windows. A relative path is taken from the working folder. It may not be the install or a folder inside it, because nothing in the install is ever written |
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
| `--window-ui` | Open the launcher as a window instead of printing to the terminal, as "The window" below describes. A machine with no web view says so once and runs as the console launcher |
| `--dry-run` | Print the install, the run folder and the exact command, and start and write nothing |
| `--wait` | Wait for the client and exit with its own code. The client runs in a job object that ends it if the launcher is killed, and Ctrl+C ends it too |
| `--tail` | Print the client's own log lines while it runs. It waits for the client as `--wait` does, because the launcher has to stay running to read the log |
| `--help` | Print the usage and exit 0 |

An option overrides the settings file. Values the environment sets override the file too, as doc/config/README.md's layers describe, so `AMBROSE_CLIENT_DIR` is enough to choose the install without a settings file. A setting left blank in the file counts as unset and the default above applies, as an empty environment variable does. No value may begin with `-`, because the client's own option parser would read it as one of its options.

Without `--wait` or `--tail` the client is started detached, so closing the launcher leaves the game running.

## The window

`launcher --window-ui` opens the launcher as a window instead of printing to the terminal. Every option above still applies and still means the same thing, because the window asks the same launcher the terminal asks: it sends the values it holds, the launcher builds the plan, and the window shows what came back. Nothing is decided in the window, so the window and the terminal cannot disagree about what would be run.

- **The window is the operating system's own web view**, WebView2 on Windows. A machine that has none says so in one line and runs as the console launcher instead, which is why `--window-ui` never turns a working launcher into one that will not start.
- **The page is read from the program's own folder**, `launcher-ui` beside the executable, through a folder the view is given directly. No listener is opened and no port is taken, so the only traffic a run makes is the client's own to the login server. The fonts ship with the page, so it is the same window with the network unplugged.
- **A password is hidden before the window is told anything.** The command the window shows carries `********` where a key would be, while the client is still started with the real one, because a screen anybody can see can be photographed.
- **Where the window was left is remembered** in `launcher-window.json` in the Ambrose data folder: its size, its corner and whether it was maximised. A place no screen holds any more is dropped and the window opens where it would have on a machine that had never run it, and the window will not be made smaller than 820 by 560, which is what its own screens need to hold their words.
- **The window has no account field yet.** The client's own login window is what asks, and automatic login from the launcher waits on milestone 5.06, which answers MSG_USER_VALIDATE. A field that looks as though it signs somebody in and does not is worse than no field.

## The folder the client runs from

The retail client reads `config.xml`, `preferences.xml`, `revision.dat` and `data.dat` by relative name from its working folder, and nothing else it needs is relative to it, so the launcher gives it a folder of Ambrose's own and passes the install's data folder with `-D`. The folder holds:

| File | What it is |
|---|---|
| `config.xml` | The `config.xml` the folder already holds, or else the install's own `Bin/config.xml`, or `defaultconfig.xml` from its `Root.wad` under its own root element when the install has none, with `IsFullscreen`, `Resolution`, `WindowedX` and `WindowedY` set to the window asked for and `SilentMetricsURL` emptied wherever it appears, because the client fetches that address while it starts. Every other byte, `VersionInfo`, the declaration, the indentation and the line endings included, is left exactly as the template has it: the values are spliced into the text, and the file is never parsed and written again, because the client ignores a file that has been rewritten that way and falls back to its built-in defaults, which are fullscreen at a resolution of its own choosing and a metrics address at KingsIsle. It is written every run, because the client saves its own over it as it exits, so the window asked for and the empty address hold on every run and the client's other saved settings survive |
| `preferences.xml` | The same, from the `preferences.xml` the folder holds or else the install's own `Bin/preferences.xml`, with the same window keys and `SilentMetricsURL` emptied where it has one, because the client's preferences override its configuration |
| `revision.dat`, `data.dat` | Copies of the install's own, the other files the client opens by relative name |
| `launcher.stamp` | The install and revision those two copies were made from. They are copied again when either changes, and the two files above are then seeded from the install again instead of from the folder |
| `WizardClient.log` | The client's own log, from `-G`. It is removed before each run, so `--tail` and the 3.24 driver read only the run in front of them |

The client writes its own files there as it runs, such as `state.dat`. Nothing in the folder is committed.

## What the launcher never touches

- The install. It is read for the client program, the configuration templates, the revision files and its `Root.wad`, and nothing in it is ever written. A `RunDir` that is the install or lies inside it is refused, by the path given and by the path its links lead to.
- KingsIsle's launcher and patcher. The launcher always passes `-L`, because the retail client starts `..\Wizard101.exe` when it sees none of its own options, and it always passes `-P 0`. `-ST` is never passed.
- Anything outside this machine. The generated configuration leaves `SilentMetricsURL` empty in both files and is written again on every run, and the client is pointed only at the login server named here.

## Which machines can start a client

The client is a Windows program, so the launcher starts it on Windows. On any other machine it finds installs in Wine, Proton, Lutris and Steam prefixes as the servers do, prints what it would run and refuses to start it, naming that as the reason and writing nothing; `--dry-run` there prints the command for you to run through Wine yourself.

## Exit status

0 when the client started, or the client's own code with `--wait`. 1 when a refusal names its cause: no install found, a folder that holds no install, a missing client program, patching asked for, a missing or unusable login host or port, a value that makes no sense or begins with `-`, a settings file that cannot be read, a run folder that cannot be named, lies inside the install or cannot be written, a machine that cannot start a Windows program, or a client that cannot be started. 2 on bad usage.

<!-- Project Ambrose by Imjustchico: How to run your own Wizard101 client against Ambrose without contacting KingsIsle patch servers, and check its first handshake with the login server. -->

# Running the client without patching

Every development milestone runs the retail client against a local Ambrose login server with patching explicitly turned off. No patch server is needed for that path, and the client does not contact KingsIsle for patching.

## Launch flags

The client's own usage text (in `Bin/WizardGraphicalClient.exe` of the 1.610 install) lists the flags:

| Flag | Meaning |
|---|---|
| `-L <host> <port>` | Login server to connect to |
| `-P <0\|1>` | Patching enabled (0 turns it off) |
| `-A <locale>` | Client locale, such as `en-US` |
| `-D <dir>` | Data root folder, `..\Data\GameData\` by default, which needs its trailing separator |
| `-G <file>` | The client's own log file |
| `-U ..<id> <key> [name]` | Log in without the login window, by sending MSG_USER_VALIDATE |
| `-C <name>` | Create or select that character |
| `-PT` | Patch client patch time |

Always include `-P 0` during development. The launcher does this for every
run and refuses a configuration that asks for patching. The behavior of the
retail client when `-P` is omitted has not been established against the pinned
install, so omitting it is not a supported development path and may contact
the host named by that install's `PatchConfig.xml`.

## The launcher

`launcher`, built from `src/tools/launcher`, starts the client:

1. Copy `launcher.conf.dist`, which the build puts beside the program, to `launcher.conf` and set `ClientDir` to the folder that holds `Bin` and `Data`. Every setting is optional: with no file at all the launcher uses the newest install it finds and a local login server on port 12000.
2. Run `launcher`. Add `--dry-run` to print the run folder and the exact command without starting anything, `--port 12001` or `--window 1600x900` to override a setting for one run, `--wait` to keep the launcher in front of the client, and `--tail` to watch the client's own log.

The launcher never runs KingsIsle's launcher or patcher and never writes inside the install, and it refuses a run folder that is the install or lies inside it. It always passes `-L`, `-P 0`, `-A`, `-D` and `-G`, and it starts the client from `client/<revision>` in the Ambrose data folder, where it writes `config.xml` and `preferences.xml` every run, from the files that folder already holds or else the install's own, with the window asked for and `SilentMetricsURL` emptied, plus copies of `revision.dat` and `data.dat`. doc/config/launcher.md documents every option, that folder and what is never touched. The 3.24 driver starts the client through it, and a player launcher that patches a copy of the install from an Ambrose patchserver is planned for milestone 16.13.

## First handshake with the login server

1. Build Ambrose, then copy `loginserver.conf.dist` next to `loginserver.exe` as `loginserver.conf`.
2. Set `ClientDir` in it to your install, so client messages are logged by name. If you leave it empty, the server uses the install with the newest revision on this machine without asking. With `TypeDumpPath` left empty, it builds that install's type dump with typeextract when the revision has no current dump, which can take a minute or two. It uses an install it found, and saves it to `conf.d/client-data.conf`, only once it has that dump. To choose the install and dump on a terminal instead, set `Setup.Mode = ask`.
3. Start the server. Keepalives, handled client messages and decoded authentication requests log at Debug, so they appear in `Login.log` in `LogsDir` but not on the console. To see them on the console too, start it as `loginserver --set "Appender.Console=1,2,3"`.
4. Create an account from the server console with `account create <name> <password>`.
5. Start the client with `launcher`, or `launcher --port <port>` when the server does not listen on 12000, and log in with it.

A working session logs lines like these, in this order. The session id, port, timing and size vary.

```
Session 1 offered to 127.0.0.1:50512
Session 1 accepted by 127.0.0.1:50512 after 12 ms
LOGIN MSG_USER_AUTHEN_V3 (7:27) from session 1, 213 bytes
Session 1 sent MSG_USER_AUTHEN_V3: version W.1.610.x, revision r806919.Wizard_1_610, data revision ..., locale enUS, machine ..., patch client ..., Steam patcher 0, console type 0, ...-byte Rec1
Session 1 from 127.0.0.1 authenticated as <name> (id 1) on machine ...: sent MSG_USER_AUTHEN_RSP Error=0 and MSG_USER_ADMIT_IND Status=1
```

Client strings in the decoded request are escaped and cut to 64 bytes, so a client cannot write its own log lines.

The client should then show the empty character select screen. A wrong password logs `failed to authenticate as <name>: the password is wrong; sent MSG_USER_AUTHEN_RSP Error=AuthenFailed`, and the client should show its invalid-login dialog and let you try again. After `Login.MaxAuthAttempts` wrong passwords the session closes and your address is refused for `Login.LockoutSeconds`.

To check the idle drop without waiting six minutes, start the server with `--set Login.AfkTimeout=30` and leave the client on the login or character select screen. After 30 seconds without input it should show its AFK disconnect message rather than a generic connection-lost error. To check the shutdown notice, type `shutdown` in the server console while the client is connected; the client should show a server-shutdown notice. The server stops accepting clients first and waits up to `Login.ShutdownGrace` for the notice to be written.

Leave the client idle at the login stage for 5 minutes. Keepalive lines should appear in both directions (`keepalive from the client` and `keepalive sent to the client`, then `answered by the client`), and no `Closing session` line should appear.

## Rules

- Never run the retail launcher or patcher against the install you develop with. It updates files, changes the client revision, and breaks the pinned 1.610 message and type data. A separate copy may be patched: the retail patcher contacts KingsIsle's servers, which is your own choice on your own account and machine, and the patched copy may no longer match Ambrose's pinned message and type data.
- An Ambrose patchserver serves executables only when its operator turns that on, and each one must match a manifest signed with the operator's own key. The client checks only CRC-32 of what it downloads, so patch from a server only if you trust its operator.
- Modified client executables are never distributed. Binary patches or a hook DLL of Ambrose's own code are applied by you to your own copy locally, at your own risk, and never to the install you develop with.
- Always pass `-L <host> <port>`. Checked in the r806919 program on 2026-09-17: with none of `-L`, `-U`, `-X`, `-T`, `-R`, `-R2`, `-CS` or `-IgnoreMissingParams`, the client starts `..\Wizard101.exe`, KingsIsle's own launcher, which is the one thing never to run against the install you develop with. The Ambrose launcher and the 3.24 driver always pass it.
- The client fetches its configuration's `SilentMetricsURL`, a KingsIsle address, while it starts. A run that must reach nothing outside the machine starts the client from a working directory of its own whose `config.xml` leaves that value empty, which the launcher writes and the 3.24 driver relies on.
- The window comes only from the configuration in that working directory: `IsFullscreen`, `Resolution`, `WindowedX` and `WindowedY` in `config.xml` and `preferences.xml`. The retail build ignores a resolution on the command line, and a client that is maximized, double-clicked on its title bar or sent Alt+Enter switches itself to fullscreen.
- The client reads that configuration only while it is still the file the client itself would write. Checked on r806919 on 2026-09-17: a `config.xml` parsed and written again by an XML writer, differing from the install's own only in its declaration, its indentation and its line endings, was ignored, and the client used its built-in defaults instead, logging `VidSettingsAuto ... picked UserRes [1600, 900]`, a fullscreen renderer and KingsIsle's own metrics address although the file asked for a 1024x768 window and an empty address. The same values spliced into the install's own bytes as text were all honoured, and the file the client saved as it exited was byte for byte the one it had been given. So a tool that changes these files changes only the values it needs and leaves every other byte alone, which is what the launcher does.
- `-U ..<user id> <key>` makes the client send MSG_USER_VALIDATE instead of showing its login window, and `-C <name>` makes it create a character of that name by itself. Both are useful for automated runs once 5.06 and 3.16 land, and neither changes the install; the launcher passes them on with `--user` and `--character`.
- The install stays yours. Ambrose reads it at runtime and never copies its files into the repository.

## Verification

These checks need the maintainer's own client, so they are done by hand and recorded here with the client revision and date.

| Check | How | Result |
|---|---|---|
| `-P 0` makes no patch connection | Listen on 127.0.0.1:12500 with any TCP listener, start the client with `-L 127.0.0.1 12000 -P 0`, and confirm the 12500 listener records no connection while the client connects to 12000 | Not yet recorded |
| Default without `-P` | Start the client with only `-L 127.0.0.1 12000` and note whether it contacts the patch host from `Bin/PatchConfig.xml` (this 1.610 install has no `PatchConfig.xml`) | Not yet recorded |
| No patch error dialog | With `-P 0`, the login screen appears with no "Patch failed" or GUI_PatchingFailed dialog | Not yet recorded |
| The launcher starts the client | Run `launcher --window 1280x720` against a local login server and confirm the client reaches the login screen in a window of that size, that nothing inside the install changed, and that a capture shows no connection leaving the machine | Recorded in part on 2026-09-17 on r806919, with no login server listening: two runs through the built launcher, `--window 1024x768` and then `--window 1280x720` in the same run folder, each logged `Attempt to create renderer with width=<the size asked for>, flags=40`, the windowed flag, with no `VidSettingsAuto` line choosing another resolution, `Metric Url: ?message=...` with no host and no line naming wizard101.com; the client ran its main loop for about 40 seconds and exited with code 0 on a posted `WM_SYSCOMMAND SC_CLOSE`; a snapshot of all 3363 files in the install before and after, with SHA-256 of the 119 files under `Bin`, showed no change; `psutil` polling the client and its children 189 times a run saw one socket, a `SYN_SENT` to port 12000 at 172.31.64.1, which is this machine's own address for its hostname, and nothing else. The login screen itself was not photographed and no login server answered, so the login-screen half of this check and the 3.24 driver's capture are still to be recorded |
| `--wait` and the job object | Run `launcher --wait`, close the client, and confirm the launcher exits with the client's code; run it again and kill the launcher, and confirm the client ends with it | Not yet recorded |

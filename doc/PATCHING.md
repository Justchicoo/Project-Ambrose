<!-- Project Ambrose by Imjustchico: How to run your own Wizard101 client against Ambrose without contacting KingsIsle patch servers, and check its first handshake with the login server. -->

# Running the client without patching

Every development milestone runs the retail client against a local Ambrose login server with patching turned off. No patch server is needed, and the client never contacts KingsIsle.

## Launch flags

The client's own usage text (in `Bin/WizardGraphicalClient.exe` of the 1.610 install) lists the flags:

| Flag | Meaning |
|---|---|
| `-L <host> <port>` | Login server to connect to |
| `-P <0\|1>` | Patching enabled (0 turns it off) |
| `-A <locale>` | Client locale, such as `en-US` |
| `-PT` | Patch client patch time |

The development command is:

```
Bin\WizardGraphicalClient.exe -L 127.0.0.1 12000 -P 0
```

Run it from the install's `Bin` folder, because the client resolves its data relative to its working directory.

## The launcher

`apps/launcher/run-client.ps1` (or `run-client.bat`) starts the client this way:

1. Copy `conf/dist/launcher.conf.dist` to `conf/launcher.conf`, which git ignores.
2. Set `ClientDir` to the folder that contains `Bin`. `LoginHost`, `LoginPort`, and `Locale` are optional.
3. Run `apps\launcher\run-client.bat`. Add `-WhatIf` to print the command without starting the client, or `-LoginPort 12001` to override a value.

This development launcher always starts `WizardGraphicalClient.exe` with `-P 0`. A player launcher that patches a copy of the install from an Ambrose patchserver is planned for milestone 16.13.

## First handshake with the login server

1. Build Ambrose, then copy `loginserver.conf.dist` next to `loginserver.exe` as `loginserver.conf`.
2. Set `ClientDir` in it to your install, so client messages are logged by name.
3. Start the server. Keepalives, handled client messages and decoded authentication requests log at Debug, so they appear in `Login.log` in `LogsDir` but not on the console. To see them on the console too, start it as `loginserver --set "Appender.Console=1,2,3"`.
4. Create an account from the server console with `account create <name> <password>`.
5. Start the client with the launcher and log in with it.

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
- The install stays yours. Ambrose reads it at runtime and never copies its files into the repository.

## Verification

These checks need the maintainer's own client, so they are done by hand and recorded here with the client revision and date.

| Check | How | Result |
|---|---|---|
| `-P 0` makes no patch connection | Listen on 127.0.0.1:12500 with any TCP listener, start the client with `-L 127.0.0.1 12000 -P 0`, and confirm the 12500 listener records no connection while the client connects to 12000 | Not yet recorded |
| Default without `-P` | Start the client with only `-L 127.0.0.1 12000` and note whether it contacts the patch host from `Bin/PatchConfig.xml` (this 1.610 install has no `PatchConfig.xml`) | Not yet recorded |
| No patch error dialog | With `-P 0`, the login screen appears with no "Patch failed" or GUI_PatchingFailed dialog | Not yet recorded |

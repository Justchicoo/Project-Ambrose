<!-- Project Ambrose by Imjustchico: A cautious Wine and Proton workflow for running an operator's own client against a local Ambrose server. -->

# Running the client with Wine or Proton

This guide describes the boundary between Ambrose's launcher and a
user-managed Wine or Proton runtime. The launcher can discover an install in a
Wine, Proton, Lutris, or Steam prefix and can print the command it would use,
but the current launcher refuses to start a Windows client on a non-Windows
host. The operator must run the printed command through their own compatibility
runtime.

This is not a claim that every client revision works under Wine or Proton.
Graphics, audio, input, prefix layout, and anti-cheat behavior vary by
runtime. The guide's result is **works**, **fails**, or **unable to run** for
the exact host, prefix, client revision, and server tested.

## Safety boundary

Use only an installation and account you own or are authorized to test. Keep
the development client copy separate from any copy that has been patched by
the retail launcher. Never run KingsIsle's launcher or patcher against the
development copy, and never use a public patch service as part of this
workflow.

The client installation remains outside the repository. The run directory is
also outside the repository and outside the installation. The launcher copies
the relative files it needs into that run directory, passes `-P 0`, empties
`SilentMetricsURL`, and points the client at the configured local login
server. Do not commit the run directory, client logs, screenshots, captures,
WADs, type dumps, protocol XML, or generated client data.

For a traffic experiment, follow
[safe-session-capture.md](safe-session-capture.md) first. Use a disposable
account, loopback only, and a private capture directory. Record metadata, not
payload bytes.

## Host prerequisites

Prepare:

- a Linux distribution supported by the chosen Wine or Proton package;
- a 64-bit Wine build or a Proton installation visible to the runtime;
- working graphics drivers and Vulkan support when the selected runtime needs
  them;
- the Ambrose loginserver built for the host and bound to loopback;
- a disposable database account and local Ambrose account; and
- an operator-owned Windows client installation with the pinned revision.

The server is built and checked on Linux as described in
[linux.md](linux.md). Set its `BindIP` to `127.0.0.1` for a local test and
choose a free `LoginServerPort`. Confirm readiness with `loginserver --check`
before starting a compatibility-runtime session.

Do not assume that a prefix's `C:` drive is the same path seen by the Linux
server. The client and server communicate through the host loopback address,
while the client path is interpreted inside Wine or Proton.

## Discover and inspect the launcher command

Build the launcher and inspect its options:

```bash
cmake --build --preset linux-gcc-debug --target launcher
./build/linux-gcc/bin/Debug/launcher --help
```

If the launcher can discover the installation, use `--dry-run` first:

```bash
./build/linux-gcc/bin/Debug/launcher \
  --client "$HOME/.local/share/your-private-client" \
  --host 127.0.0.1 \
  --port 12000 \
  --run-dir "$HOME/.local/share/ambrose-client-runs/r806919" \
  --window 1280x720 \
  --dry-run
```

The output names the run directory and prints the command with `-L`, `-P 0`,
`-A`, `-D`, and `-G`. Review it before running anything. If the launcher
refuses because the host cannot start a Windows program, that is expected on
Linux and is not a protocol failure.

Use an absolute `--run-dir` outside both the install and checkout. If the
launcher reports that the directory is inside the installation, choose a
different path rather than disabling the guard.

## Run with Wine

Run the exact command printed by `--dry-run`, replacing the Windows program
path with the prefix's path as Wine sees it. A typical operator-owned prefix
looks like this:

```bash
export WINEPREFIX="$HOME/.local/share/wineprefixes/ambrose"
winecfg
wine 'C:\AmbroseClientRun\WizardGraphicalClient.exe' \
  -L 127.0.0.1 12000 \
  -P 0 \
  -A en-US \
  -D 'Z:\private\Wizard101\Data\GameData\' \
  -G 'C:\AmbroseClientRun\WizardClient.log'
```

The exact paths and arguments come from the launcher output; do not copy the
example paths into a real run. If the client cannot resolve the data root,
check the prefix mapping and trailing separator without moving the install
into the repository.

Keep the first run in the foreground. Save only the exit code and a sanitized
summary of the client and server logs. A log can contain account identifiers,
paths, or protocol details; keep the raw file private and delete it after the
useful metadata is recorded.

## Run with Proton

Proton is normally launched by Steam, so it does not provide one universal
command line. Use the Proton version selected by the operator and a private
prefix. A generic direct invocation is illustrative only:

```bash
STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam" \
STEAM_COMPAT_DATA_PATH="$HOME/.local/share/compatdata/ambrose-private" \
  "$HOME/.local/share/Steam/steamapps/common/Proton - Experimental/proton" \
  run 'Z:\private\AmbroseClientRun\WizardGraphicalClient.exe' \
  -L 127.0.0.1 12000 -P 0 -A en-US \
  -D 'Z:\private\Wizard101\Data\GameData\' \
  -G 'Z:\private\AmbroseClientRun\WizardClient.log'
```

Use the actual Proton executable and prefix paths on the host. Do not add the
client as a public Steam shortcut or point a shared prefix at a real account.
If Steam owns the prefix, stop Steam's game process normally before removing
the private prefix.

## First-session checks

Run the checks in this order:

1. `loginserver --check` exits successfully.
2. The server binds only to loopback and reports its ready line.
3. The launcher `--dry-run` command names the intended revision and local
   endpoint.
4. Wine or Proton starts the client without starting a retail launcher.
5. The login screen appears, or the runtime reports a clear graphics/input
   failure.
6. A disposable account can authenticate, if the runtime reaches the login
   screen.
7. Character select appears, if the current server and client support that
   stage.
8. The client exits cleanly when the server sends its shutdown notice.

A failure before the first server connection is a runtime or path issue, not
evidence about Ambrose protocol behavior. A server authentication failure
after a successful handshake should be investigated from the server log and
the account setup, not by enabling the retail patcher.

## Troubleshooting

### The launcher refuses to start

On Linux this is expected: the launcher prints that the machine cannot start a
Windows program. Re-run with `--dry-run` and execute the printed command
through Wine or Proton. Do not edit the launcher to bypass the platform guard.

### The client starts the retail launcher

Stop the process. Check that the command contains `-L` and `-P 0`, and that
the run directory contains the launcher-generated configuration. Never fix
this by running the retail launcher against the development installation.

### The client cannot connect

Confirm the server is listening on `127.0.0.1`, the port matches `-L`, and the
Wine or Proton prefix can reach the host loopback. Check the first server
error and the client log privately. Do not expose the login port on all
interfaces as a troubleshooting shortcut.

### Graphics or input fail

Record the runtime name and version, GPU driver, display session, prefix
location, and the first sanitized error. Try a fresh disposable prefix before
changing the client copy. A compatibility workaround that modifies client
files is not a repository contribution.

### The run needs a capture

Stop and apply the safe-session-capture checklist. Capture only loopback
traffic for the disposable account, keep the raw file private, and report
frame metadata rather than bytes.

## Verification record

For a useful Wine or Proton report, record:

- host distribution and kernel;
- Wine or Proton version and prefix type;
- graphics driver and display session;
- client revision read from the operator's private `revision.dat`;
- Ambrose build preset and server port;
- whether the launcher dry-run command was reviewed;
- the first successful stage, or the exact stage and error where it stopped;
- whether the client contacted only loopback; and
- what could not be tested.

The project currently has no CI leg that starts the retail client under Wine or
Proton. A guide-only validation can check links, style, forbidden files, and
diff hygiene; it cannot claim that a specific runtime supports the client.

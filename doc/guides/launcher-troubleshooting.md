<!-- Project Ambrose by Imjustchico: Troubleshooting the launcher configuration, fullscreen behavior, and refused-login browser side effect. -->

# C-68: Troubleshooting the Ambrose launcher

This guide covers three launcher problems that are easy to misdiagnose:

1. the client silently ignoring a configuration file;
2. a window unexpectedly becoming fullscreen; and
3. the client opening a KingsIsle page in the system browser after a refused
   login.

The launcher starts the user's own Wizard101 client from an Ambrose-owned run
folder. It does not run KingsIsle's launcher or patcher, and it never writes
inside the retail install. Keep the install, captures, type dumps, logs, and
credentials private and outside the repository.

## Start with a dry run

Before starting a client, ask the launcher to print the selected install, run
folder, and complete command:

```powershell
.\launcher.exe --client C:\Games\Wizard101 `
  --host 127.0.0.1 --port 12000 `
  --window 1280x720 --dry-run
```

The command should contain all of these client arguments:

```text
-L 127.0.0.1 12000 -P 0 -A en-US -D <install>\Data\GameData -G <run-folder>\WizardClient.log
```

`-L` prevents the retail client from trying to start the KingsIsle launcher.
`-P 0` prevents patching. `-D` points at the install's data directory while
the working directory remains the separate run folder. `--dry-run` writes
nothing and starts no client, so it is safe to use while correcting settings.

If the launcher reports that no install was found, pass `--client` explicitly
or set `AMBROSE_CLIENT_DIR`. If it reports that the run folder is inside the
install, choose a directory outside the install with `--run-dir`. Do not solve
either error by placing Ambrose files in `Bin`.

## The configuration the client silently ignores

### Edit the run folder, not the retail install

The launcher writes these files in its run folder:

```text
<run-folder>\config.xml
<run-folder>\preferences.xml
<run-folder>\revision.dat
<run-folder>\data.dat
```

The run folder defaults to `client\<revision>` below the Ambrose data directory
on Windows. Use `--run-dir` or `RunDir` when a different location is needed.
The retail install's `Bin\config.xml` and `Bin\preferences.xml` are templates;
the launcher reads them but never modifies them.

The configuration source order is:

1. the existing run-folder file when the install and revision stamp still
   match;
2. the install's `Bin\config.xml` or `Bin\preferences.xml`; and
3. for `config.xml` only, `defaultconfig.xml` in the install's
   `Data\GameData\Root.wad` when `Bin\config.xml` is absent.

The launcher generates a fresh run-folder file every run. It preserves the
template's declaration, indentation, attribute order, line endings, and
unrelated settings. It changes only the values it owns:

| File | Keys controlled by the launcher | Purpose |
| --- | --- | --- |
| `config.xml` | `VideoSettings.IsFullscreen`, `VideoSettings.Resolution`, optional `WindowedX`, optional `WindowedY`, and every `SilentMetricsURL` | Window mode, size, position, and local-only startup |
| `preferences.xml` | The same window keys when present, and every `SilentMetricsURL` when present | Preferences can override the main configuration |

`SilentMetricsURL` is emptied in both generated files so the client does not
fetch its retail metrics address during startup. Other values, including
`VersionInfo`, UI scale, sound settings, and game settings, remain the
template's values.

### Why an apparently correct XML edit does nothing

The pinned client does not reliably accept an XML document that has been
parsed and written again by a generic XML library. A rewritten file can differ
only in its declaration spacing, indentation, or line endings and still be
ignored; the client then falls back to built-in defaults. That fallback can
look like a fullscreen or wrong-resolution bug, and it can restore a retail
metrics address.

Do not repair this by hand-formatting the generated XML. Use the launcher,
which splices the owned values into the template's original bytes:

```powershell
.\launcher.exe --window 1280x720 --run-dir C:\Ambrose\client-test --dry-run
```

For a persistent setting, put it in `launcher.conf` or use the documented
environment variable:

```text
Window = 1280x720
Fullscreen = 0
WindowX = 100
WindowY = 80
RunDir = C:\Ambrose\client-test
```

The equivalent environment variables are `AMBROSE_WINDOW`,
`AMBROSE_FULLSCREEN`, `AMBROSE_WINDOW_X`, `AMBROSE_WINDOW_Y`, and
`AMBROSE_RUN_DIR`. Command-line values override the configuration file and
environment layers.

To inspect the actual generated file, run the launcher once without
`--dry-run`, then inspect the run folder. Do not inspect or edit the retail
install's files as a workaround:

```powershell
Get-Content C:\Ambrose\client-test\config.xml
Get-Content C:\Ambrose\client-test\preferences.xml
```

If a setting appears correct but the client still ignores it, check these
items in order:

1. confirm the client is running from the run folder printed by the launcher;
2. confirm both `config.xml` and `preferences.xml` contain the intended
   window values, because preferences can override configuration;
3. confirm `SilentMetricsURL` is empty in every occurrence;
4. confirm the run folder is not stale or inside the install; and
5. remove only the run folder's `launcher.stamp` if the install's
   `revision.dat` or `data.dat` needs to be copied again.

Never copy the generated run folder into the repository. The client may write
`state.dat`, logs, and other runtime files there.

## Fullscreen traps

### Use the configuration, not a resolution argument

The retail client ignores a resolution supplied as a command-line option. Set
the window through the launcher:

```powershell
.\launcher.exe --window 1280x720
```

`--window` writes `Resolution` in both generated files. A valid size is from
`320x320` through `16384x16384`. `--fullscreen` sets `IsFullscreen = 1`;
without it, the default is windowed (`IsFullscreen = 0`). The configuration
also accepts `Fullscreen = 2`, but the meaning of that third client mode is
not confirmed; do not describe it as borderless or rely on it without a local
measurement.

`WindowX` and `WindowY` set the windowed position. They do not keep a window
windowed after a fullscreen-triggering action.

### Actions that switch the client to fullscreen

The pinned client switches itself to fullscreen when its window is:

- maximized;
- double-clicked on the title bar; or
- sent `Alt+Enter`.

Do not use those actions when measuring a windowed launch. The client-driver
also avoids minimizing, maximizing, and `Alt+Enter`, because each can change
the renderer mode. If a window unexpectedly fills the screen:

1. close it without recording the result as a launcher configuration failure;
2. remove the maximized state and start again with `--window`;
3. do not double-click the title bar or press `Alt+Enter`; and
4. inspect `IsFullscreen` in both generated XML files before repeating.

The launcher cannot prevent the client's own window manager shortcuts from
changing the mode after startup. It can only supply the initial configuration.
Use a fixed window size and position for repeatable screenshots or client-driver
reference crops. The interface scale remains the install's own setting, so
changing it invalidates measurements made at another scale.

### A practical windowed launch check

Use a disposable run folder and no automatic login:

```powershell
.\launcher.exe --client C:\Games\Wizard101 `
  --host 127.0.0.1 --port 12000 `
  --window 1280x720 --run-dir C:\Ambrose\window-test --wait
```

Confirm that:

- the launcher printed the expected `-L`, `-P 0`, `-D`, and `-G` arguments;
- `config.xml` and `preferences.xml` both contain `IsFullscreen = 0` and
  `Resolution = 1280x720`;
- the client window was not maximized or sent `Alt+Enter`; and
- no file under the retail install changed.

Record the client revision, launcher revision, window size, interface scale,
and the exact action that changed the mode if fullscreen still occurs.

## The page opened after a refused login

### This is a client state, not a server redirect

When the client receives a non-success login response, its log contains a line
like:

```text
LOGIN RESPONSE: Error=<non-zero>
```

The client shows its refused-login dialog. On the pinned revision, asking that
client to quit while the refusal state is still active opens a KingsIsle page
in the machine's default browser. The page is outside the client and may
contact a remote service. Pressing the dialog away does not clear the state,
and waiting for the login window to return does not clear it either. The state
lasts until the client has been admitted by a later successful login.

Do not treat the browser page as evidence that the Ambrose server redirected
the client. It is client behavior after a refused login.

### Safe response

After a refused login:

1. keep the capture, logs, and account private;
2. do not ask the client to quit normally while the refusal state is active;
3. end the client process and its children directly;
4. close the disposable account or rotate its password; and
5. remove the private run folder, capture, and diagnostic files.

The client-driver implements this rule with `references.json`. It watches the
client log for a non-zero `LOGIN RESPONSE` and force-ends the client during
teardown unless a later line says that the LoginServer admitted the user. The
`refused-login-quit` scenario deliberately expects a failure at a line that
will never be written, so the teardown rule is exercised without asking the
refused client to quit:

```powershell
python apps\clientdriver\drive.py run `
  --scenario refused-login-quit `
  --capture-dir C:\Temp\ambrose-capture
```

Use the driver's preflight first:

```powershell
python apps\clientdriver\drive.py check
```

Exit code `77` means the machine cannot run the client driver prerequisites.
That is a prerequisite limitation, not a protocol result. The scenario's
report and capture remain under the driver's private run directory; neither
belongs in a commit.

### Distinguish refusal from a successful login

A successful authentication is followed by server log evidence equivalent to:

```text
sent MSG_USER_AUTHEN_RSP Error=0
sent MSG_USER_ADMIT_IND Status=1
```

Only after the client has been admitted is a normal quit safe under this
specific rule. A character-list request or a returned login window alone is
not enough to prove that the refusal state was cleared.

If a browser opens unexpectedly, stop the run, preserve only safe metadata
(revision, direction, message names, and timestamps), rotate any credential
that may have been used, and do not upload the browser URL, capture, cookies,
or screenshots containing private information.

## Checklist

Before reporting a launcher issue, record:

- client revision and launcher revision;
- the exact command or configuration layers used;
- the run-folder path, not the retail install path;
- the generated `IsFullscreen`, `Resolution`, `WindowedX`, and `WindowedY`;
- whether `preferences.xml` overrode `config.xml`;
- whether the title bar, maximize button, or `Alt+Enter` was used;
- the login response and whether a later admission occurred; and
- whether the client was force-ended before teardown.

Never include client archives, type dumps, packet bytes, credentials, browser
URLs, or generated run folders in an issue or contribution.

## Verification and limits

This guide was checked against `doc/config/launcher.md`, the launcher run-folder
implementation and tests, `doc/PATCHING.md`, the client-driver references, and
the `refused-login-quit` scenario on 2026-09-22. It describes observed
behavior for the pinned client revision and must be rechecked when the client
revision, launcher configuration writer, or client-driver teardown changes.

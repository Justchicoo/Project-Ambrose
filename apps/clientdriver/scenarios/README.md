<!-- Project Ambrose by Imjustchico: A catalog of client-driver scenarios and the repeatable checks each one performs. -->

# Client-driver scenario catalog

The client driver loads one JSON file at a time from this directory. The files
listed below are the repeatable checks that cover the C-03 scenario set. They
share the disposable-account, local-server, and private-capture requirements
described in [the safe-session capture guide](../../../doc/guides/safe-session-capture.md).

## C-03 coverage

| Check | Scenario | What it verifies |
| --- | --- | --- |
| Idle timeout | [`c39-idle-timeout.json`](./c39-idle-timeout.json) | A bounded `Login.AfkTimeout` closes an idle admitted session and records the server-side AFK disconnect. |
| Ban enforcement | [`c39-ban-enforcement.json`](./c39-ban-enforcement.json) | A console-applied account ban closes the admitted session and produces a refused-login response on the next attempt. |
| Reconnect after refusal | [`c39-reconnect-after-refusal.json`](./c39-reconnect-after-refusal.json) | A wrong password, invalid-login dialog, reconnect action, and successful retry occur in that order. |
| Shutdown notice | [`c39-shutdown-notice.json`](./c39-shutdown-notice.json) | A graceful login-server shutdown emits both the server notice and the client maintenance notice. |

The files retain their original C-39 scenario names because they are also
individual regression checks. This catalog groups them under C-03 without
changing their executable behavior or asserting that a run passed without a
client and local server.

## Listing and running

List the scenarios without starting a client or server:

```powershell
python apps\clientdriver\drive.py scenarios
```

Run one check with a user-owned client, locally built binaries, and a run
directory outside the repository:

```powershell
python apps\clientdriver\drive.py run `
  --scenario c39-idle-timeout.json `
  --binaries C:\Path\To\Ambrose\bin `
  --client C:\Path\To\Your\Client `
  --runs C:\Temp\ambrose-clientdriver
```

Replace the scenario name for the other rows. The run directory contains the
private report, logs, and (unless `--no-capture` is explicitly selected) the
private `login.pcapng` and `.tshark.txt` files. Do not copy those artifacts
into this directory or into the repository.

## Boundaries

- These scenarios require the driver preflight to succeed; an unavailable
  client, capture adapter, or required helper is a prerequisite limitation,
  not a failed protocol assertion.
- The scenario runner creates its disposable account and scratch databases
  according to the selected server options. Do not provide a real account or
  a production database.
- A scenario's recorded logs and capture remain private. Public reports may
  cite only non-sensitive metadata such as the scenario name, result, frame
  count, and failure step.

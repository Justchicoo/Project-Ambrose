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

## World entry

| Check | Scenario | What it verifies |
| --- | --- | --- |
| Standing in Ravenwood | [`enter-world.json`](./enter-world.json) | A wizard saved in WizardCity/WC_Ravenwood is listed, played and handed its object in MSG_LOGINCOMPLETE, the client loads the zone and says so, the entry chatter is answered, and the session stays up through a keepalive. |
| Announcing and kicking | [`announce-and-kick.json`](./announce-and-kick.json) | The Commons entry, then `server announce` on the game server console, whose text the client shows as a notification and logs as a server message, and `kick` with the wizard's character id, which sends the reason CSR and opens the client's dialog for a disconnect by an administrator. |
| Walking in the Commons | [`enter-the-commons.json`](./enter-the-commons.json) | The same entry for a wizard saved in WizardCity/WC_Hub with no position, placed at the zone's Start, with the fields MSG_LOGINCOMPLETE carried recorded, and a held W that moves the view far more than the same time idle, which is the player controlling the wizard. |
| A wizard's stats | [`wizard-stats.json`](./wizard-stats.json) | A level 5 Fire wizard seeded with 900 experience, 1234 gold, 300 health and 10 mana enters Ravenwood; the game server logs the stats it built from the database and the level tables, and the run shoots the HUD, then the backpack and the character stats opened with the keys the client's own InputBindings.xml gives them. |

All four require the game server as well as the login server; the driver starts one of its own.

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

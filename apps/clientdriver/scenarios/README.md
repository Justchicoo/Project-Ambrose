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

## Login and character select

| Check | Scenario | What it verifies |
| --- | --- | --- |
| Banned before sign-in | [`c77-banned-login.json`](./c77-banned-login.json) | 2.14's banned account: `account ban` on the login server console before the account first signs in, the server's refusal with AccountBanned, the client choosing GUI_AccountBannedTime and a shot of its dialog, and neither the client's admission nor character select ever logged. |
| AFK on character select | [`c78-afk-character-select.json`](./c78-afk-character-select.json) | 2.15's AFK check: a seeded wizard left on character select past a 150-second `Login.AfkTimeout`, the server's MSG_DISCONNECT_LOGIN_AFK, and the client choosing GUI_ConnectionAFK with no lost-connection message, with shots before the timeout and of the dialog. |
| Idle at the login prompt | [`c79-idle-login-keepalive.json`](./c79-idle-login-keepalive.json) | 1.22's idle check: after a wrong password, the client held at the login retry prompt with `Login.AfkTimeout` raised to 600, through six keepalive cycles each way at a 60-second `Network.KeepAliveInterval`, over five minutes, with the session never closed and the account never authenticated. |

C-78 also requires the game server, which the driver starts; C-77 and C-79 run
against the login server alone.

## World entry

| Check | Scenario | What it verifies |
| --- | --- | --- |
| Standing in Ravenwood | [`enter-world.json`](./enter-world.json) | A wizard saved in WizardCity/WC_Ravenwood is listed, played and handed its object in MSG_LOGINCOMPLETE, the client loads the zone and says so, the entry chatter is answered, and the session stays up through a keepalive. |
| Announcing and kicking | [`announce-and-kick.json`](./announce-and-kick.json) | The Commons entry, then `server announce` on the game server console, whose text the client shows as a notification and logs as a server message, and `kick` with the wizard's character id, which sends the reason CSR and opens the client's dialog for a disconnect by an administrator. |
| Walking in the Commons | [`enter-the-commons.json`](./enter-the-commons.json) | The same entry for a wizard saved in WizardCity/WC_Hub with no position, placed at the zone's Start, with the fields MSG_LOGINCOMPLETE carried recorded, and a held W that moves the view far more than the same time idle, which is the player controlling the wizard. |
| Walking away and back | [`walk-and-return.json`](./walk-and-return.json) | The Commons entry, a walk with W, then the client quits and is started again under the same guard; the game server writes where the wizard stood as it leaves, and the second login must put the wizard back at exactly that place, which differs from the zone's Start it first stood at. |
| A wizard's stats | [`wizard-stats.json`](./wizard-stats.json) | A level 5 Fire wizard seeded with 900 experience, 1234 gold, 300 health and 10 mana enters Ravenwood; the game server logs the stats it built from the database and the level tables, and the run shoots the HUD, then the backpack and the character stats opened with the keys the client's own InputBindings.xml gives them. |
| Learning a spell | [`learn-a-spell.json`](./learn-a-spell.json) | A Fire wizard with an empty spellbook enters Ravenwood; `learn "Fire Cat" <wizard>` on the game server console sends MSG_ADDSPELLTOBOOK and a second learn is refused, the spellbook opened with P is shot on its Fire page before and after, the client is quit and started again and the wizard must enter knowing Fire Cat, and `unlearn` sends MSG_REMOVESPELLFROMBOOK before a last shot. The Spell Deck shows no card for a wizard without a deck, so the Fire page stays empty until 8.10 and 8.11 give it one. |
| Two wizards | [`two-wizards.json`](./two-wizards.json) | A second client, the companion, enters the Commons on an account and wizard of its own after the main wizard has backed away from the Start; the companion opens and closes its spellbook while the main client records the icon appearing and clearing, then turns to face the main wizard, runs a circle with W and D held together, stops and jumps while the main client is filmed every quarter second, then quits and comes back, and must be taken away from the main wizard at once and come back with a new mobile id as one wizard, not a ghost. |

All seven require the game server as well as the login server; the driver starts one of its own. A step names the client it drives with `"client": "companion"`, `hold_key` holds a list of keys together, and a held key may `watch` the other client, which is filmed every `watch_every` seconds while the key is held and for `watch_after` seconds after.

## Patching

| Check | Scenario | What it verifies |
| --- | --- | --- |
| No patch connection with -P 0 | [`patch-off.json`](./patch-off.json) | The launcher's client, started with `-P 0` from a run folder holding the install's own `PatchConfig.xml` pointed at a live local listener, shows the login window and logs in while that listener and one on 12500 see no connection and the guard sees nothing off the machine, with no patcher line in the client's log. |
| The default without -P | [`patch-default.json`](./patch-default.json) | The client started from the launcher's own prepared command with only `-P 0` taken out, so it follows its own default, with the install's own `PatchConfig.xml` in its folder pointed at a local listener; the client connecting to that listener shows it contacts the patch host its configuration names by default, while the guard keeps every connection on this machine and the install check proves nothing on disk changed. |

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

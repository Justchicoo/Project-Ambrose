<!-- Project Ambrose by Imjustchico: Every option of the panel program, in panel.conf.dist and on its command line, with where it keeps its list and what it never holds. -->
# panel program options

The panel program lists Ambrose panels and opens each in a window of its own, loaded from that panel's own address. This computer heads the list, without being added, whenever a supervisor on this machine answers on its admin API with the token in the default file under the Ambrose data folder, and it opens through a one-time local link that supervisor hands out. Other panels are added by address or paired from the line a supervisor's console prints.

See doc/config/README.md for the file format and the layers. The program reads `panel.conf` beside it; with no such file it uses the defaults below. `panel --help` prints the options.

## Settings

| Option | Type | Default | Environment variable | Meaning |
|---|---|---|---|---|
| `ThisComputer.AdminPort` | integer | `12020` | `AMBROSE_THIS_COMPUTER_ADMIN_PORT` | The loopback port of this machine's supervisor's admin API, which the program asks for its health and for local links. Match it to `Admin.Port` in supervisor.conf |

## Command line

| Option | Meaning |
|---|---|
| `--help` | Print the options and exit |
| `--version` | Print the program's version and the revision it was built from, and exit |

## What it keeps and what it never holds

- Its list, `panels.sqlite3`, its window's place, its log and a web view profile per panel live in `PanelApp` in the Ambrose data folder. Forgetting a panel deletes its profile with everything in it.
- It never asks for, sees or stores a panel password. A panel is signed in to on its own sign-in page, inside its own window, and the session it opens stays in that panel's profile.
- A panel with a self-signed certificate is added only once the operator confirms the fingerprint it serves matches the one its supervisor printed, or from a pairing line, which carries the fingerprint. A panel whose certificate then changes is refused before anything is sent to it, naming both fingerprints.
- Plain HTTP beyond this computer is refused unless the operator takes the opt-in for that entry, which then carries a warning that the password and the session cross the network unencrypted.
- The list's probe reads only each panel's public session route, and only while the list is shown. With no remote entries, the program opens no connection off this machine.
- On a machine with no web view, a chosen panel opens in the default browser, This computer through its local link, and the program says once why.

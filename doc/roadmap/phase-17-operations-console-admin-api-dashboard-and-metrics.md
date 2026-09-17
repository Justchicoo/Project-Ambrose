<!-- Project Ambrose by Imjustchico: Roadmap phase 17, Operations: console, admin API, dashboard and metrics. -->

# Phase 17: Operations: console, admin API, dashboard and metrics

**Done when:** From a browser on a desktop or a phone, an operator sees every server's health and player counts, follows live logs, runs audited commands, edits game settings and reloads content live, restarts a crashed server, and reviews performance history in Grafana. Like a game server hosting panel, it signs each operator in with their own account and permissions, runs scheduled restarts and backups, restores a backup, updates with one click and rolls back, edits files, and manages servers on several machines from one place, and a player on a desktop starts everything from one icon. The servers stay headless, so they run the same on a desktop, a Linux VPS, in Docker, or under an existing Pterodactyl panel. This track runs in parallel: 17.01 can land right after 1.20, and the rest any time after phase 2, except 17.12 and 17.13, which follow 4.16, and the milestones whose dependencies name later phases.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 17.01 | Server console: colored logs and a command prompt | M | 1.10, 1.20 |
| 17.02 | Admin API listener and authentication | M | 1.09, 1.12, 1.19, 1.20 |
| 17.03 | Status API | S | 17.02, 1.22 |
| 17.04 | Live log stream | S | 17.02, 1.10 |
| 17.05 | Remote command console with audit log | M | 17.01, 17.02, 4.02 |
| 17.06 | Dashboard app and overview page | M | 17.03 |
| 17.07 | Dashboard log viewer and command console pages | M | 17.04, 17.05, 17.06 |
| 17.08 | Process control, config, and database pages | M | 17.06, 2.06 |
| 17.09 | Metrics registry and Prometheus endpoint | S | 17.02 |
| 17.10 | Grafana dashboards and operations guide | S | 17.09 |
| 17.11 | Terminal dashboard mode | S | 17.03, 17.04 |
| 17.12 | Settings and reload admin API | S | 17.02, 17.05, 4.16 |
| 17.13 | Dashboard settings editor and reload page | M | 17.06, 17.12 |
| 17.14 | Panel users, roles, sub-users and two-factor sign-in | M | 17.08, 2.13, 1.12 |
| 17.15 | Schedules with in-game countdowns | M | 17.05, 17.08, 17.14, 2.15 |
| 17.16 | Backups and restore | M | 17.08, 17.14, 2.06 |
| 17.17 | One-click updates and rollback | M | 17.16, 3.23 |
| 17.18 | File manager | M | 17.14 |
| 17.19 | Built-in resource graphs and alerts | M | 17.03, 17.06, 17.09 |
| 17.20 | Client data and revisions page | S | 17.06, 3.23 |
| 17.21 | Accounts, bans, characters and online players pages | M | 17.05, 17.14, 3.17 |
| 17.22 | Nodes: one panel for servers on several machines | M | 17.14, 17.16 |
| 17.23 | Operating system services, Docker image and Pterodactyl egg | M | 17.08 |
| 17.24 | Desktop control app | M | 17.06, 17.08, 3.22, 1.21 |

## Review notes for this phase

- **Origin.** Added on 2026-09-13 at the maintainer's request for a modern, intuitive way to run the servers. It also covers the roadmap review's missing work item for remote administration and health endpoints.
- **Decisions.** Settled on 2026-09-13 under Decisions, Operations in doc/ARCHITECTURE.md: Crow for 17.02, TypeScript and Svelte with Vite for 17.06, an Ambrose supervisor for 17.08, and localhost-only access unless TLS and a token are configured. On 2026-09-16 at the maintainer's direction, plain HTTP beyond localhost became an opt-in setting, off by default (see 17.02). 17.11 picks FTXUI, which vcpkg provides.
- **Live settings.** 17.12 and 17.13 were added on 2026-09-14 for the Live reload and live settings rule in doc/ARCHITECTURE.md. Config editing moved from 17.08 to 17.13, so edits apply live through the settings API instead of writing the `.conf` file and asking for a restart.
- **Hosting panel parity.** 17.14-17.24 were added on 2026-09-17 at the maintainer's request to manage everything in one place the way game server hosting panels such as Pterodactyl do. The supervisor from 17.08 becomes the panel's single entry point, much as a Pterodactyl node daemon serves its panel: operators sign in to it once, and it relays each app's admin API. Work on this phase starts after 3.23 and runs alongside the gameplay phases. The choices are recorded under Decisions, Operations in doc/ARCHITECTURE.md: Argon2id from libsodium for panel passwords, TOTP for two-factor sign-in, zstd for backup archives, and the supervisor's own SQLite file for panel users, schedules and backup records, so the panel works before any game database exists.

## 17.01 Server console: colored logs and a command prompt

**Goal:** Every app's console is readable at a glance and accepts commands without log output corrupting what the operator is typing.

**Size:** M. **Depends on:** 1.10, 1.20

**Deliverables**

- Console log appender in `src/common/Logging/` that colors each level when output is a terminal, enables virtual terminal sequences on Windows at startup, and writes plain text when output is redirected
- Console input runner in `src/server/shared/Console/` with a `Ambrose>` prompt, line editing, history, and tab completion of command names, redrawing the prompt after each log line. It builds on the reader thread, command thread, `ConsoleCommandTable`, `help`, `shutdown` and `Console.Enable` that 2.13 added, and moves command replies onto the same writer as the log lines so the two cannot interleave
- Built-in commands available before CommandMgr exists: `help`, `status` (revision, uptime, sessions once 1.22 lands), `shutdown [seconds]`, and `reload config` once 4.15 lands; 4.02 later registers them in CommandMgr
- `Console.Enable` and `Console.Colors` options in each app's `.conf.dist`, documented in `doc/config/<app>.md`; `Console.Colors` applies from the next line after a config reload

**Acceptance**

- [ ] In a terminal, error lines render red and warning lines yellow on Windows and Linux
- [ ] `gameserver > out.log` writes no escape sequences to the file
- [ ] `status` prints the revision and uptime, the up arrow recalls the previous command, and `shutdown` exits 0
- [ ] Log lines arriving while a command is half typed do not corrupt the typed text
- [ ] With `Console.Enable = 0` the app runs with no input thread and still shuts down on Ctrl+C

## 17.02 Admin API listener and authentication

**Goal:** Each app can expose a local, authenticated HTTP and WebSocket API that every later operations feature builds on.

**Size:** M. **Depends on:** 1.09, 1.12, 1.19, 1.20

**Deliverables**

- `src/server/shared/Admin/AdminServer` serving HTTP/1.1 and WebSocket on the project's Asio layer, off unless `Admin.Enable = 1`, bound to 127.0.0.1 by default
- Bearer token authentication with a constant-time comparison; the token comes from config or is generated on first start into a file readable only by the current user, and it rotates live on a config reload
- A per-address TokenBucket that limits failed authentication attempts
- `GET /api/health` returning app name, realm, revision, uptime in seconds, and lifecycle state as JSON
- Startup refuses a non-loopback bind address unless a TLS certificate and key are configured, or the operator sets the opt-in `Admin.AllowPlainHttpRemote = 1`, which is off by default. With that setting the token is still required and startup logs a warning naming the option, but the token, commands and logs cross the network unencrypted, so anyone on the path can read them and reuse the token. On a config reload an unsafe non-loopback bind is refused with an error naming the option and the old binding stays, while a safe bind change rebinds live and a failed bind keeps the old listener. This hooks into 4.15 when it lands

**Acceptance**

- [ ] `GET /api/health` without a token returns 401, and with the token returns 200 and the running revision
- [ ] Twenty wrong tokens within one second from one address produce 429 responses
- [ ] Setting `Admin.BindIP = 0.0.0.0` without TLS logs an error naming the option and exits 1 at startup, and on a config reload is refused while the old binding keeps serving
- [ ] With `Admin.AllowPlainHttpRemote = 1` and no TLS, a non-loopback bind starts, still requires the token, and logs a warning naming the option
- [ ] Rotating the token and reloading config makes the old token return 401 and the new one 200 without a restart
- [ ] Routing, authentication, and rate limiting are unit tested without opening sockets

## 17.03 Status API

**Goal:** Live numbers about each app are available to dashboards without the admin layer knowing every subsystem.

**Size:** S. **Depends on:** 17.02, 1.22

**Deliverables**

- A stats registry in `src/common/` that subsystems publish named values into
- `GET /api/status` returning uptime, process memory, thread count, connected sessions, and for the gameserver the average and maximum tick time over the last 60 seconds
- Later milestones add fields as their systems exist: players online (4.01), realms (4.03), database pool usage (2.01), pending SQL updates (2.06), the settings generation (4.16), and the last reload result per target (4.15)

**Acceptance**

- [ ] Two fake clients from the 1.22 test harness show `sessions: 2`, and disconnecting both shows 0 within one second
- [ ] Reported memory is within 10 percent of the operating system's own figure
- [ ] A schema test confirms fields are only ever added, never renamed or removed

## 17.04 Live log stream

**Goal:** Dashboards can follow a server's logs live with filtering, without slowing the server down.

**Size:** S. **Depends on:** 17.02, 1.10

**Deliverables**

- WebSocket `/api/logs` streaming structured records with time, level, category, and message
- Per-subscriber filters for minimum level and categories
- A backlog of the last 1000 records sent to each new subscriber
- A bounded queue per subscriber that drops the oldest records and reports how many were dropped, so logging never blocks

**Acceptance**

- [ ] A subscriber filtered to warnings receives only warnings and errors
- [ ] A subscriber that stops reading does not change logging latency by more than 10 percent across 100000 log lines
- [ ] A reconnecting subscriber receives the backlog

## 17.05 Remote command console with audit log

**Goal:** Operators can run server commands from the dashboard, and every remote command leaves a record.

**Size:** M. **Depends on:** 17.01, 17.02, 4.02

**Deliverables**

- `POST /api/command` running a command through CommandMgr at console security level and returning its output lines
- An audit log of every remote command with time, remote address, command, and result, written to a file and to a db_login audit table once one exists; it also records every setting change and reload made remotely, alongside the setting_audit rows from 4.16
- A confirmation flag required for destructive commands such as shutdown and account deletion

**Acceptance**

- [ ] Creating an account through the API succeeds and appears in the audit log
- [ ] `shutdown` without the confirmation flag is refused
- [ ] A failing command returns its error text with `success: false`

## 17.06 Dashboard app and overview page

**Goal:** A modern web dashboard shows the health of every server at a glance, on desktop or phone.

**Size:** M. **Depends on:** 17.03

**Deliverables**

- `apps/dashboard/`, built to static files that the admin API can serve at `/`
- A server list the operator fills with URLs and tokens, stored in the browser
- Overview cards per app showing health, uptime, revision, sessions, players, and tick time, with automatic reconnection and a visible stale state
- A responsive layout that works at phone width, with light and dark themes
- Dashboard build and tests added to CI

**Acceptance**

- [ ] With loginserver and gameserver running, both show online within 2 seconds, and stopping one marks it offline within 5 seconds
- [ ] The overview is usable at 400 pixels wide with no horizontal scrolling
- [ ] The built dashboard served by the admin API loads with no browser console errors

## 17.07 Dashboard log viewer and command console pages

**Goal:** Operators read logs and run commands from the dashboard as comfortably as at the server's own console.

**Size:** M. **Depends on:** 17.04, 17.05, 17.06

**Deliverables**

- A log viewer with level and category filters, text search, pause and resume, copy, and a tab per server
- A command console page with history and output that notes commands are audited

**Acceptance**

- [ ] Filtering to errors hides lower levels immediately, and pausing stops scrolling while counting new lines
- [ ] Running `status` shows the same output as the local console

## 17.08 Process control, config, and database pages

**Goal:** Operators start, stop, and restart servers, review configuration, and see database update state from the dashboard.

**Size:** M. **Depends on:** 17.06, 2.06

**Deliverables**

- A supervisor that starts, stops, and restarts loginserver, gameserver, and patchserver, restarts crashed apps with backoff, remembers exit codes, and exposes an admin endpoint
- A config page comparing each app's effective settings with its `.conf.dist` defaults and showing each value's source layer. Edits go through the settings API, apply live, and are audited; the editing part is built in 17.13. Only options documented as restart-required show that, with the reason
- A database page listing applied and pending update files per database from the updater, plus, once 4.15 lands, the journal of live world-database edits, exportable as pending SQL updates

**Acceptance**

- [ ] The restart button restarts gameserver while loginserver sessions stay connected
- [ ] Killing gameserver from outside makes the supervisor restart it and the dashboard shows one crash
- [ ] The config page shows each value's effective value, default, and source layer, and marks only options documented as restart-required, with the reason

## 17.09 Metrics registry and Prometheus endpoint

**Goal:** Performance data is exported in a standard format so it can be graphed over time.

**Size:** S. **Depends on:** 17.02

**Deliverables**

- `src/common/Metric/` with counters, gauges, and histograms, including tick time, message handling time per service, database query time, and reload counts, failures, and duration per target
- `GET /metrics` in the Prometheus text exposition format, protected by the admin token or an address allow list

**Acceptance**

- [ ] `promtool check metrics` accepts the endpoint's output
- [ ] Handling 1000 fake messages increases that service's counter by exactly 1000
- [ ] A microbenchmark shows a metric update costs under 50 nanoseconds on average

## 17.10 Grafana dashboards and operations guide

**Goal:** Operators get ready-made graphs of realm health and performance history.

**Size:** S. **Depends on:** 17.09

**Deliverables**

- `apps/grafana/` with a Docker Compose file for Prometheus and Grafana, a provisioned data source, and dashboards for realm overview, performance, and database health
- `doc/OPERATIONS.md` covering the console, dashboard, metrics stack, safe remote access through a TLS reverse proxy, and the list of restart-required cases with the reason for each: a binary upgrade, adding or removing a compiled module, schema updates the new binary needs, a client revision or type dump change when live objects can't hold the old registry, and client-side WAD changes, which need a client re-patch

**Acceptance**

- [ ] `docker compose up` next to a running gameserver shows live graphs within 30 seconds
- [ ] Every provisioned dashboard loads with no missing panel errors

## 17.11 Terminal dashboard mode

**Goal:** Operators working over SSH get live panels inside the terminal itself.

**Size:** S. **Depends on:** 17.03, 17.04

**Deliverables**

- A `--tui` option that draws full-screen panels for status, sessions, logs, and a command line when output is a terminal, using FTXUI from vcpkg

**Acceptance**

- [ ] Resizing the terminal lays the panels out again, and `q` or Ctrl+C exits with code 0
- [ ] Without a terminal, `--tui` logs a warning and falls back to the normal console

## 17.12 Settings and reload admin API

**Goal:** Operators read and change live settings and trigger reloads over the admin API, with every change validated and audited.

**Size:** S. **Depends on:** 17.02, 17.05, 4.16

**Deliverables**

- `GET /api/settings` returning each setting's schema, default, effective value, source layer, apply mode, and lock
- `PUT /api/settings/{key}` taking a value and a reason, returning 422 with every validation error, and refusing a key locked by an environment variable or command-line override with an error naming the layer
- `POST /api/settings/batch` applying all entries or none
- `GET /api/settings/{key}/history` returning audit rows with old and new values, who, and why
- `POST /api/reload/{target}` and `GET /api/reload` returning each target's generation, last result, and errors
- WebSocket `/api/events` streaming setting changes and reload results

**Acceptance**

- [ ] An out-of-bounds PUT returns 422 and changes nothing
- [ ] A PUT to a key set by an environment variable is refused and names the locking layer
- [ ] A batch with one bad entry applies none
- [ ] A change appears on a second dashboard within one second
- [ ] A reload of a broken message definition returns every error while the old generation keeps serving

## 17.13 Dashboard settings editor and reload page

**Goal:** Operators change any live setting and reload any store from a browser on a desktop or a phone, without restarting a server.

**Size:** M. **Depends on:** 17.06, 17.12

**Deliverables**

- A settings page by category with search, typed and bounded inputs, the default and source layer shown, a required reason, a review step before apply, and per-key history with revert; it is where the 17.08 config page's values are edited
- A reload page showing each target's generation, last result, and errors, with a button per target
- A restart-required list with the reason for each case

**Acceptance**

- [ ] Changing Rate.Drop.Item from a phone applies to the running gameserver, and history shows who and why
- [ ] Editing a config value applies without a restart and survives one
- [ ] Invalid values can't be submitted, and server refusals show their message
- [ ] Revert restores the old value and writes a new audit row

## 17.14 Panel users, roles, sub-users and two-factor sign-in

**Goal:** Every operator signs in to the panel with their own account and sees and does only what their role and grants allow.

**Size:** M. **Depends on:** 17.08, 2.13, 1.12

**Deliverables**

- Panel users stored in the supervisor's SQLite file, with passwords hashed by Argon2id from libsodium and optional TOTP two-factor sign-in with one-time recovery codes
- On first start the supervisor creates an owner account and writes a one-time sign-in link to its console and log, usable only from the same machine, so no default password ever exists
- Roles owner, admin, operator and viewer, plus per-server sub-user grants of single permissions such as `console.read`, `console.write`, `power.restart`, `files.write`, `backups.restore`, `schedules.edit`, `settings.edit` and `accounts.ban`, enforced on every route and WebSocket
- Signed, expiring session cookies with CSRF protection, a sign-in rate limit per address and per user, a sessions list with sign-out everywhere, and scoped personal API keys for automation
- The supervisor relays each app's admin API behind the signed-in user's permissions, so the browser holds no per-app tokens; the per-app tokens from 17.02 remain for scripts
- A panel users page for creating users, assigning roles and grants, resetting two-factor, and reading each user's audit trail; every action records the panel user in the 17.05 audit log
- Optional linking of a panel user to a game account from 2.13, so a game master's panel role can follow their security level

**Acceptance**

- [ ] A fresh supervisor has no default password and prints a one-time owner link that works once, only from localhost
- [ ] A viewer can read logs but gets 403 on a command, a restart, a file write and a restore, and the dashboard hides those controls
- [ ] A sub-user granted only `power.restart` on gameserver can restart gameserver and nothing else
- [ ] With two-factor on, a correct password without a valid TOTP code is refused, and each recovery code works once
- [ ] Twenty failed sign-ins for one user within a minute are rate limited, and the correct password works again once the window passes
- [ ] Revoking a user ends their open sessions and WebSockets within one second

## 17.15 Schedules with in-game countdowns

**Goal:** Operators schedule restarts, backups, commands and announcements, and players get warned in game before a restart.

**Size:** M. **Depends on:** 17.05, 17.08, 17.14, 2.15

**Deliverables**

- Schedules with cron expressions or one-time dates in a chosen time zone, each running an ordered list of tasks with offsets: announce, run a command, restart, stop, start, back up, and update once 17.17 lands
- Restart countdowns that warn connected players at set intervals, through the 2.15 shutdown notice on the loginserver, and on the gameserver through the zone broadcast from 6.01 and GM messages from 6.04 once those exist
- Options to skip a run when no players are online, to wait for a quiet moment up to a limit, and to retry a failed task
- A schedules page with next run times, run now, pause, and a history of every run with each task's result and output, all audited

**Acceptance**

- [ ] A schedule set to restart the loginserver at a set time with a 5 minute countdown warns connected clients at 5, 1 and 0 minutes and restarts at that time
- [ ] A daylight saving change neither skips nor doubles a schedule's run in its time zone
- [ ] A schedule marked to skip when empty does not run while a player is online and does run once no one is
- [ ] Restarting the supervisor keeps schedules and runs a missed one-time task once if it is still within its grace window

## 17.16 Backups and restore

**Goal:** Operators back up everything a realm needs and restore it with one click, with every backup verified.

**Size:** M. **Depends on:** 17.08, 17.14, 2.06

**Deliverables**

- Backups of every Ambrose database as consistent logical dumps taken inside one transaction, plus the config, data and type dump folders, packed into one zstd-compressed archive with a SHA-256 manifest
- Manual backups, scheduled backups through 17.15, retention by count and age, and pinned backups that retention never removes
- Local storage by default and opt-in S3-compatible remote storage, with uploads resumed after a failure
- Restore of the whole archive or of chosen databases or folders: the supervisor stops the affected apps, takes a safety backup, verifies every checksum before changing anything, restores, runs the updater, and starts the apps again
- A backups page with size, duration, contents, download, restore, lock and delete, all permission checked and audited

**Acceptance**

- [ ] A backup taken while accounts are created and deleted through the console restores to a state where every foreign key and row count matches the moment the dump began
- [ ] A backup archive with one flipped byte is refused at restore with the failing file named, and nothing is changed
- [ ] Restoring a backup from before a dated SQL update brings the database back and then applies the update again
- [ ] Retention set to keep 3 removes the oldest unpinned backup when a fourth completes
- [ ] A restore interrupted by killing the supervisor leaves the safety backup in place, and the next start reports the incomplete restore

## 17.17 One-click updates and rollback

**Goal:** Operators move to a newer Ambrose build with one click, and a failed update rolls back by itself.

**Size:** M. **Depends on:** 17.16, 3.23

**Deliverables**

- Update sources: a release channel whose packages are checked against published SHA-256 sums and signatures, and a build channel that pulls a git branch and builds it with the project's presets
- Versioned install folders kept side by side, so an update installs next to the running build and the switch is a pointer change
- An update run that takes a backup through 17.16, installs, runs `--check` on every app, switches, restarts through the supervisor, and waits for health; a failed check or health wait switches back, and restores the backup when the update had applied database changes
- The last few builds kept for manual rollback, with each build's version, commit and changelog shown
- When 3.23 reports a newer KingsIsle client revision, the panel shows it next to the Ambrose update and can rebuild client data before the restart

**Acceptance**

- [ ] Updating to a newer build keeps accounts and characters and shows the new version after the restart
- [ ] A build whose gameserver fails `--check` is never switched in, and the running build keeps serving
- [ ] A build that starts but fails its health wait is rolled back to the previous build within the configured time, and the database matches the backup
- [ ] A release package with a wrong checksum or signature is refused before any file is written to the install folder

## 17.18 File manager

**Goal:** Operators browse, edit, upload and download the server's own files from the panel without shell access.

**Size:** M. **Depends on:** 17.14

**Deliverables**

- A file browser limited to the Ambrose install, config, logs, data and backup folders, refusing path traversal, symbolic links that leave those roots, and device files
- A text editor with syntax highlighting for `.conf`, SQL, JSON, XML and Lua, which validates `.conf` files against their settings schema before saving and keeps the previous version
- Upload with size limits, download of files and folders as archives, extraction of uploaded archives within the roots, rename, move, copy, delete to a recoverable trash, and search by name
- Following a growing log file live, with every write permission checked and audited with the file's hash before and after

**Acceptance**

- [ ] Requests for `../`, an absolute path, an encoded traversal or a symbolic link leaving the roots are refused with 403 and audited
- [ ] Saving a `.conf` file with an out-of-range value is refused with the setting and bound named, and the file is unchanged
- [ ] Two operators saving the same file at once get a conflict for the second save instead of a silent overwrite
- [ ] A viewer can read and download but cannot upload, edit or delete

## 17.19 Built-in resource graphs and alerts

**Goal:** Operators see CPU, memory, network, disk, players and tick time over time in the panel itself, and are told when something goes wrong.

**Size:** M. **Depends on:** 17.03, 17.06, 17.09

**Deliverables**

- The supervisor samples every app's CPU, memory, threads, open handles, network in and out, and disk use every few seconds, keeping a day at full detail in memory and 30 days downsampled on disk, with no Prometheus or Grafana needed
- Graphs per app and per realm for those values plus sessions, players online and tick time, with ranges from 5 minutes to 30 days
- Alert rules for a crash, a crash loop, high tick time, high memory, low disk space, a failed backup, a failed schedule and a failed update, sent to webhooks such as Discord and to email, with repeat limits
- An alerts page with each alert's history and acknowledgement

**Acceptance**

- [ ] With gameserver under a synthetic load, the panel's CPU graph is within 10 percent of the operating system's own figure
- [ ] Killing gameserver three times within a minute sends one crash-loop alert, not three separate ones
- [ ] A 30 day graph loads in under one second with a month of samples
- [ ] Sampling costs under 1 percent of one core

## 17.20 Client data and revisions page

**Goal:** Operators see which client install and type data each server uses, and rebuild it from the panel.

**Size:** S. **Depends on:** 17.06, 3.23

**Deliverables**

- A page listing the client installs found, the revision each server uses, the type dump in use with its revision, executable hash, extractor version and build time, and the message definitions, name tables and creation config loaded
- The revision following state from 3.23: the newest revision seen, whether data for it is built, and any build in progress with its live output
- Buttons to rebuild client data and to switch a server to a different install or revision, each checked, audited, and applied live where 3.23 supports it

**Acceptance**

- [ ] After a client revision change, the page shows the new revision within a minute and the rebuild's live output while it runs
- [ ] A failed rebuild shows the extractor's error, and the servers keep using the previous data

## 17.21 Accounts, bans, characters and online players pages

**Goal:** Game masters manage accounts, bans, characters and online players from the panel.

**Size:** M. **Depends on:** 17.05, 17.14, 3.17

**Deliverables**

- An accounts page with search, create, password reset, lock and unlock, security level, email and last sign-in address, backed by AccountMgr from 2.13
- A bans page for account and address bans with duration, reason, who and when, and unban with a reason, matching the console commands
- A characters page per account listing characters with level, school and location, with rename, restore of a deleted character and delete as 3.17 and later phases support them
- An online players page with realm, zone and session time, and kick, mute and teleport actions from 6.05, 6.06 and 12.07 once those exist
- Every action goes through CommandMgr with the panel user's permissions and is audited

**Acceptance**

- [ ] Banning an account from the panel while its player is on character select disconnects that client with the ban message
- [ ] An operator without `accounts.ban` cannot ban, and the attempt is audited
- [ ] Creating an account from the panel lets a real client sign in with it

## 17.22 Nodes: one panel for servers on several machines

**Goal:** One panel manages loginservers, gameservers and patchservers running on several machines.

**Size:** M. **Depends on:** 17.14, 17.16

**Deliverables**

- A node mode for the supervisor on each extra machine, joined to the panel with a one-time join token that becomes mutually authenticated TLS with pinned certificates
- A nodes page with each node's health, resources, apps and port allocations, and the ability to place an app or realm on a node and move it with a backup and restore
- Console, logs, files, backups, schedules, graphs and power actions work the same for apps on any node, relayed by the panel under the signed-in user's permissions
- A node that loses the panel keeps its apps running and resyncs when the link returns

**Acceptance**

- [ ] A gameserver on a second machine shows in the panel, and restarting it from the panel works
- [ ] Cutting the network between panel and node leaves the node's apps serving players, and the panel shows the node offline, then online again within 10 seconds of the link returning
- [ ] A node presenting a certificate other than the pinned one is refused

## 17.23 Operating system services, Docker image and Pterodactyl egg

**Goal:** Ambrose runs as a background service, in Docker, or under an existing Pterodactyl panel with no manual setup.

**Size:** M. **Depends on:** 17.08

**Deliverables**

- `supervisor --install-service` and `--uninstall-service`, which register the supervisor as a Windows service or a systemd unit running as a dedicated user and starting at boot
- A multi-stage Dockerfile for a small runtime image, and a Docker Compose file with MariaDB, the supervisor and the apps, with volumes for config, data, logs and backups, and health checks
- A Pterodactyl egg that installs and starts Ambrose, exposes its settings as startup variables, and maps the console and stop command, for hosts that already run Pterodactyl
- Packaging docs in doc/OPERATIONS.md, with client data built on first start through 3.22 from a mounted client install

**Acceptance**

- [ ] After `--install-service` and a reboot, the servers are running and the panel is reachable, on Windows and on Linux
- [ ] `docker compose up` on a clean machine with a client install mounted reaches a ready loginserver with no other steps
- [ ] Importing the egg into a Pterodactyl panel creates a server that installs, starts, shows its console and stops cleanly

## 17.24 Desktop control app

**Goal:** A player hosting on their own computer starts the database, servers, panel and client from one icon.

**Size:** M. **Depends on:** 17.06, 17.08, 3.22, 1.21

**Deliverables**

- `apps/desktop/`, an installer and tray app for Windows and Linux desktops that installs Ambrose, starts and stops the supervisor, opens the panel, and launches the client through the 1.21 launcher once the loginserver is ready
- Database setup with no steps: it uses a MariaDB or MySQL server it finds, with credentials the user gives, or else installs a private MariaDB into the Ambrose data folder, checked against its published checksum and bound to localhost with generated credentials
- First start runs 3.22's automatic setup and shows its progress, and the tray shows server status, players online and update notices from 17.17

**Acceptance**

- [ ] On a clean Windows machine with Wizard101 installed, installing the app and clicking Play reaches the login screen against the local server with no other steps
- [ ] Quitting from the tray closes the client connection cleanly and stops the servers and the private database, and the next start keeps accounts and characters
- [ ] A private MariaDB download with a wrong checksum is refused, and the app says what failed

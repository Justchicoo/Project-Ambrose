<!-- Project Ambrose by Imjustchico: Roadmap phase 17, Operations: console, admin API, dashboard and metrics. -->

# Phase 17: Operations: console, admin API, dashboard and metrics

**Done when:** From a browser on a desktop or a phone, an operator sees every server's health and player counts, follows live logs, runs audited commands, edits game settings and reloads content live, restarts a crashed server, and reviews performance history in Grafana. The servers stay headless, so they run the same on a desktop, a Linux VPS, or in Docker. This track runs in parallel: 17.01 can land right after 1.20, and the rest any time after phase 2, except 17.12 and 17.13, which follow 4.16.

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

## Review notes for this phase

- **Origin.** Added on 2026-09-13 at the maintainer's request for a modern, intuitive way to run the servers. It also covers the roadmap review's missing work item for remote administration and health endpoints.
- **Decisions.** Settled on 2026-09-13 under Decisions, Operations in doc/ARCHITECTURE.md: Crow for 17.02, TypeScript and Svelte with Vite for 17.06, an Ambrose supervisor for 17.08, and localhost-only access unless TLS and a token are configured. On 2026-09-16 at the maintainer's direction, plain HTTP beyond localhost became an opt-in setting, off by default (see 17.02). 17.11 picks FTXUI, which vcpkg provides.
- **Live settings.** 17.12 and 17.13 were added on 2026-09-14 for the Live reload and live settings rule in doc/ARCHITECTURE.md. Config editing moved from 17.08 to 17.13, so edits apply live through the settings API instead of writing the `.conf` file and asking for a restart.

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

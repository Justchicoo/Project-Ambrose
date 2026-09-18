<!-- Project Ambrose by Imjustchico: Roadmap phase 17, Operations: console, admin API, dashboard and metrics. -->

# Phase 17: Operations: console, admin API, dashboard and metrics

**Done when:** From a browser on a desktop or a phone, an operator sees every server's health and player counts, follows live logs, runs audited commands, edits game settings and reloads content live, restarts a crashed server, and reviews performance history in Grafana. Like a game server hosting panel, it signs each operator in on the panel's own listener with their own account, two-factor sign-in and permissions, records every action in an activity log, runs scheduled restarts and backups, restores a backup, updates with one click and rolls back, edits files, and manages servers on several machines from one place. Built for Wizard101, it also manages realms and zones, players online, accounts, registration and password recovery, bans, characters, reports and mutes, world database edits, announcements and timed events, patch revisions, an installation-wide maintenance mode that still lets game masters in, and a player on a desktop starts everything from one icon. The servers stay headless, so they run the same on a desktop, a Linux VPS, in Docker, or under an existing Pterodactyl panel. Whether that listener follows the admin API's remote-access rule, with TLS and its certificate handling, is listed under Decisions needed in doc/ROADMAP.md, so no part of this definition of done rests on it. This track runs in parallel: 17.01 after 1.20, 17.12 and 17.13 after 4.16, and the rest once their dependencies land, which for 17.14 and the panel milestones above it means after 3.23. doc/PANEL.md describes the panel these milestones build.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 17.01 | Server console: colored logs and a command prompt | S | 1.10, 1.20 |
| 17.02 | Admin API listener and authentication | M | 1.09, 1.12, 1.19, 1.20 |
| 17.03 | Status API | M | 17.02, 1.22 |
| 17.04 | Live log stream | M | 17.02, 1.10 |
| 17.05 | Remote command console with audit log | M | 17.01, 17.02, 4.02 |
| 17.06 | Dashboard app and overview page | L | 17.03, 17.73 |
| 17.07 | Dashboard log viewer and command console pages | S | 17.04, 17.05, 17.06 |
| 17.08 | Process control, config, and database pages | M | 17.06, 2.06 |
| 17.09 | Metrics registry and Prometheus endpoint | S | 17.02 |
| 17.10 | Grafana dashboards and operations guide | S | 17.09 |
| 17.11 | Terminal dashboard mode | S | 17.03, 17.04, 17.73 |
| 17.12 | Settings and reload admin API | M | 17.02, 17.04, 17.05, 4.16 |
| 17.13 | Dashboard settings editor and reload page | M | 17.06, 17.12 |
| 17.14 | Panel listener, TLS, sessions and the audit store | L | 17.08, 1.12, 1.19 |
| 17.15 | Schedules with in-game countdowns | L | 17.05, 17.27, 17.48, 2.15, 6.01, 6.04 |
| 17.16 | Backups: consistent dumps and verified archives | L | 17.08, 17.27, 17.48, 2.06, 5.03 |
| 17.17 | One-click updates and rollback | M | 17.16, 17.51, 17.52, 3.23 |
| 17.18 | File roots, the path jail and browsing | L | 17.12, 17.48 |
| 17.19 | Built-in resource graphs | M | 17.03, 17.06, 17.09 |
| 17.20 | Client data and revisions page | S | 17.06, 3.23 |
| 17.21 | Accounts, bans, characters and online players pages | M | 17.05, 17.48, 3.17, 6.05 |
| 17.22 | Nodes: one panel for servers on several machines | L | 17.26, 17.28, 17.29, 17.32, 17.49 |
| 17.23 | Operating system services, Docker image and Pterodactyl egg | M | 17.08 |
| 17.24 | Desktop control app | L | 17.06, 17.08, 17.46, 1.21, 3.22 |
| 17.25 | Activity log pages | M | 17.05, 17.49 |
| 17.26 | Panel event socket: envelope, tickets and generated types | M | 17.04, 17.12, 17.48 |
| 17.27 | App states, operation locks and power targets | M | 17.08, 17.26 |
| 17.28 | Launch and startup settings | M | 17.08, 17.13, 17.48 |
| 17.29 | Port allocations and listen addresses | M | 17.28 |
| 17.30 | Database hosts and credential rotation | M | 17.08, 17.47, 17.48, 2.08, 4.16 |
| 17.31 | Realms and zones pages | M | 17.06, 17.26, 17.48, 4.03, 4.09 |
| 17.32 | Realm maintenance mode | S | 17.15, 17.31, 4.05, 6.01, 6.05 |
| 17.33 | Announcements and timed game events | M | 17.12, 17.15, 6.01, 6.04 |
| 17.34 | World database edits page | M | 17.13, 17.25, 4.15, 5.01 |
| 17.35 | Panel settings: general, mail and security | M | 17.14, 17.48 |
| 17.36 | Personal API keys | M | 17.48, 17.25 |
| 17.37 | Invites and the grant editor | M | 17.48, 17.25 |
| 17.38 | Account page: profile, security, sessions and game account link | M | 17.47, 17.35 |
| 17.39 | File manager archives, search and log follow | M | 17.18, 17.54, 17.26 |
| 17.40 | Opt-in SFTP access | M | 17.54, 17.38 |
| 17.41 | Opt-in remote file pull | M | 17.54 |
| 17.42 | Moving an app or realm between nodes | M | 17.22, 17.27, 17.51 |
| 17.43 | S3-compatible backup storage and catalog repair | M | 17.16 |
| 17.44 | Player-aware schedule conditions and rolling restarts | S | 17.15, 17.31 |
| 17.45 | Opt-in security keys and passkeys | M | 17.38 |
| 17.46 | Panel users, password policy and sign-in | M | 17.14, 2.13 |
| 17.47 | Two-factor sign-in, recovery codes and required enrollment | M | 17.46 |
| 17.48 | Permission catalog, roles and grants | M | 17.05, 17.46 |
| 17.49 | Panel audit scope, app relay and command history | M | 17.05, 17.48 |
| 17.50 | Panel users, roles and grants pages | M | 17.48, 17.49 |
| 17.51 | Backup restore with staging swap and rollback | L | 17.16, 17.27, 2.06 |
| 17.52 | Backup retention, pins and audited downloads | S | 17.16, 17.47 |
| 17.53 | File editor with validated saves and version history | M | 17.18 |
| 17.54 | File uploads, downloads and batch operations | M | 17.18 |
| 17.55 | Trash, purge, new files and folders, and permission toggles | M | 17.54 |
| 17.56 | Operator-defined file roots | S | 17.18 |
| 17.57 | Socket subscriptions and live permission filtering | M | 17.26, 17.48 |
| 17.58 | Socket limits, fan-out and one live page pipeline | M | 17.57, 17.07, 17.13 |
| 17.59 | Pre-start checks and restart confirmation | M | 17.27, 3.22 |
| 17.60 | Exit classification, backoff and crash records | M | 17.27, 17.28 |
| 17.61 | Reconciling stale online and session rows after a crash | M | 17.60, 4.07, 2.13 |
| 17.62 | Player account registration and password recovery | M | 17.35, 2.13 |
| 17.63 | Moderation queue, chat search and mute history | M | 17.21, 17.25, 12.07 |
| 17.64 | Installation maintenance mode | M | 17.12, 17.14, 2.14 |
| 17.65 | Patch server operations: manifest, revisions and signing key | M | 17.14, 17.47, 17.48, 16.03, 16.08 |
| 17.66 | Dashboard localization | S | 17.06, 17.35, 17.38 |
| 17.67 | Alerts, notifications and acknowledgement | M | 17.15, 17.16, 17.19, 17.35, 17.60 |
| 17.68 | Schedules page and run history | M | 17.15, 17.26, 17.44 |
| 17.69 | Game operations analytics | M | 17.19, 17.21, 2.08 |
| 17.70 | Public status and scheduled downtime page | S | 17.14, 17.19, 17.64 |
| 17.71 | Admission queue control | S | 17.19, 17.31, 12.21 |
| 17.72 | Opt-in backup archive encryption | S | 17.43, 17.47, 17.51, 17.52 |
| 17.73 | Design system: tokens, components and the gallery | L | 1.02, 1.03 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Origin.** Added on 2026-09-13 at the maintainer's request for a modern, intuitive way to run the servers. It also covers the roadmap review's missing work item for remote administration and health endpoints.
- **Decisions.** Settled on 2026-09-13 under Decisions, Operations in doc/ARCHITECTURE.md: Crow for 17.02, TypeScript and Svelte with Vite for 17.06, an Ambrose supervisor for 17.08, and localhost-only access unless TLS and a token are configured. On 2026-09-16 at the maintainer's direction, plain HTTP beyond localhost became an opt-in setting, off by default (see 17.02). 17.11 picks FTXUI, which vcpkg provides.
- **Live settings.** 17.12 and 17.13 were added on 2026-09-14 for the Live reload and live settings rule in doc/ARCHITECTURE.md. Config editing moved from 17.08 to 17.13, so edits apply live through the settings API instead of writing the `.conf` file and asking for a restart.
- **Hosting panel parity.** 17.14-17.24 were added on 2026-09-17 at the maintainer's request to manage everything in one place the way game server hosting panels such as Pterodactyl do. The supervisor from 17.08 becomes the panel's single entry point, much as a Pterodactyl node daemon serves its panel: operators sign in to it once, and it relays each app's admin API. The panel milestones, 17.14 and everything above it, start after 3.23 and run alongside the gameplay phases. The choices are recorded under Decisions, Operations in doc/ARCHITECTURE.md: Argon2id from libsodium for panel passwords, TOTP for two-factor sign-in, zstd for backup archives, and the supervisor's own SQLite file for panel users, schedules and backup records, so the panel works before any game database exists.
- **Pterodactyl source.** Settled on 2026-09-17 at the maintainer's direction: Ambrose owns its panel, tailored to Wizard101, and uses the MIT-licensed Pterodactyl panel and Wings source only as a reference for behavior, with no code copied and no Wings. The Ambrose panel is not a fork: the Pterodactyl panel is PHP 8.2 with Laravel, React, Redis, a web server and a queue worker on Linux, and Wings is Go and runs only on Linux with Docker, not on Windows. A fork would break the desktop run that sets itself up with no steps, and the TypeScript with Svelte dashboard and C++ supervisor settled under Decisions, Operations. Its generic model of a container with a console also cannot reach typed Ambrose features such as live settings, reloads, accounts and client data. Instead, this phase studies its source as the reference for features and behavior: the permission names and sub-users, schedules and task chains, backups, the file manager and the console socket. Operators who already run Pterodactyl use the 17.23 egg. A maintained fork stays an opt-in idea, planned, not yet scheduled.
- **Built first.** Settled on 2026-09-18 at the maintainer's direction: the panel's foundation comes before the rest of the game, so later systems are built into it rather than fitted to it. The order is 17.01, 17.02, 17.03, 17.04, 17.73 and 17.06 first, which need nothing that is not already built, with 17.73 before 17.06 because the panel and the launcher window are both built from it; then 4.01 and 4.02, which the game needs next anyway and which 17.05 waits on; then 17.05, 17.08, 17.09, 17.11, 17.14 and 17.46-17.50. Everything else in this phase arrives with the system it shows, under the rule in doc/ROADMAP.md that every subsystem ships with its panel surface, so the pages for realms, players, settings, world edits and client data are built by the milestones that build those systems.
- **Order.** Ids are allocation order, not build order. Within this phase the dependency graph gives the build order, so a dependency may name a higher id: 17.15 and 17.16 wait for 17.27 and 17.46-17.48, 17.22 waits for 17.26, 17.28, 17.29, 17.32 and 17.49, 17.31 waits for 17.26, and 17.24 waits for 17.46. 17.01-17.24 keep the ids they were published with, and everything added later takes an id from 17.25 up. A check never rests on a milestone outside its own dependency closure: where one did, the dependency was added or the check was narrowed to what exists at that point.
- **Splits.** 17.14 became six milestones, 17.16 three, 17.18 five, 17.26 three and 17.27 four. Each keeps its id for its first part and the rest take ids from 17.46 up (17.46-17.50 from 17.14, 17.51 and 17.52 from 17.16, 17.53-17.56 from 17.18, 17.57 and 17.58 from 17.26, 17.59-17.61 from 17.27). Three published titles narrowed to what their milestone now holds: 17.16, 17.18 and 17.19, whose alerts moved to 17.67 so graphs no longer wait for mail settings. Two titles changed because their milestone grew instead: 17.14 now names the audit store, which the panel needs from its first milestone, and 17.50 the roles page.
- **Sizes.** A size is read off the milestone's own content, so a label can be checked against the text: S is at most 4 deliverables and at most 5 acceptance checks, L is 8 or more deliverables or 8 or more acceptance checks, and M is everything between. Two milestones carry L on judgment instead of count, 17.24 because it installs and runs on two desktop operating systems and 17.51 because it is one operation from end to end, and the Oversized note names both with the rest. Recounting moved sizes that were already published, without touching any id: 17.01 and 17.07 to S, 17.03, 17.04 and 17.12 to M, and 17.06, 17.14, 17.15 and 17.16 to L. Of the milestones added later, 17.50 moved to M with its roles page and 17.64 to M on its count.
- **Oversized.** The large milestones are the nine carrying L: 17.06 dashboard app and overview page, 17.14 panel listener and audit store, 17.15 schedules, 17.16 backups, 17.18 file roots and the path jail, 17.22 nodes, 17.24 desktop control app, 17.51 backup restore and 17.73 the design system. No other milestone in this phase is large. Split 17.73 along these lines if a focused stretch cannot finish it: the token pipeline with its generator and gates, and the component set with its gallery and tests. Split any of them again if a focused stretch cannot finish it, along these lines: 17.14 into the listener with its TLS and the sessions, limiter and audit store; 17.15 into the engine with its triggers and the tasks with their completion and countdowns; 17.16 into the dumps with their snapshot record and the archive with its verification; 17.06 into the app shell with its route table and the overview cards; 17.18 into the jail with its roots and the listing and reading page; 17.22 into the join with its heartbeat and the nodes page with placement.
- **Gated checks.** A check that needs the maintainer's own machine, a second machine, a security key, a desktop SFTP client, a Pterodactyl install or a retail client session is marked `Dev-gated:` with what it needs, the form phase 16 already uses; a check an environment variable turns on is marked `Env-gated` with that variable, as the Tests section of doc/ARCHITECTURE.md describes. doc/ROADMAP.md's Where we are paragraph lists the phase 17 checks that wait for the maintainer.
- **Proposals.** doc/PANEL.md proposes the choices this phase rests on, and doc/ROADMAP.md lists every one of them under Decisions needed with the milestones it blocks. All twenty-one: the scope tree with its default role bundles; whether the admin API's remote-access rule extends to the panel's own listener; the command security level cap on `console.write`; the keyring for the supervisor's sealed secrets; whether the panel's ciphers and keyed hashes stay inside the settled Botan stack or libsodium widens; the event socket protocol with its close codes; backup archive encryption as a default and whether dumps are structured rows rather than SQL text; the S3 client; the time zone data source and whether the image ships tzdata; the login-screen countdown notice; whether a node's schedules run on the node or centrally; the file editor and archive libraries with the default archive format; whether SFTP access and remote file pull are built at all, and the SSH library; the WebAuthn implementation; the QR renderer; the trash and version store locations; database credential rotation per server type; the realm and installation maintenance bypass levels; whether the panel may export world edits into a pending SQL tree; whether the panel offers player registration; and where the operator's patch signing key lives. Nothing here settles any of them, each milestone's text names the ones it rests on as proposals, and no check assumes one. Until the scope tree is settled, grants are the per-app sub-user grants already settled under Decisions, Operations, and every acceptance check here stays at app scope.
- **Docs.** The supervisor is a fourth executable and the panel's host. The commit that adds this file also adds it to doc/ARCHITECTURE.md's Processes table, its repository layout block and its Operations paragraph, so nothing is left to do there. What stays open is recording the panel listener's own bind and TLS rule under Decisions, Operations, which waits on the matching entry under Decisions needed in doc/ROADMAP.md; until it is recorded, 17.14's text says that rule is a proposal.
- **Security first.** Trusted proxies and the client-address rule land with the panel listener in 17.14, and required two-factor with 17.47, not behind 17.19 and 17.35. Sign-in throttles, rate limits and audit addresses are wrong without them.
- **One of each.** One stream layer with backlog, sequence numbers and resume (17.04), which 17.12 and 17.26 reuse; one app list (17.06); one browser socket (17.26); one audit store (17.14) with one scope that writes into it (17.49); one stored command history (17.49). No milestone builds a second copy of a subsystem. The one piece of rework is transport: the 17.06 overview and the live pages of 17.07 and 17.13 ship on the per-app streams of 17.04 and 17.12, and 17.58 moves them onto the panel socket so the dashboard ends with one socket client and one reconnect policy. What is replaced there is the transport, not the pages.
- **Client-derived data.** Backup archives and file roots carry the type dump and extracted data built from the operator's own install, and opt-in patch components can carry client files. 17.16, 17.18, 17.43 and 17.65 follow the bring-your-own-files rule under Decisions, Experimental features: the client install root is never downloadable through the panel, and off-machine storage is the operator's own bucket with the archive's contents stated.
- **Correction.** 17.15's deliverable said to skip a run when no players are online, while its acceptance check described a schedule that runs only when no one is online. Both are useful and opposite, so 17.44 offers both conditions by name, `skip_if_empty` and `only_when_empty`, and 17.15's checks name neither.
- **Risk.** 2.15's real-client check showed that the client displays MSG_LOGINSERVERSHUTDOWN as the server going down, and the login server closes the connection as it sends it. A login-screen warning at 5 and 1 minutes therefore needs a notice that does not disconnect, found by capture or client reverse engineering. Until one is found, 17.15 sends only the final notice on the login server and earlier warnings go to players in the world.

## 17.01 Server console: colored logs and a command prompt

**Goal:** Every app's console is readable at a glance and accepts commands without log output corrupting what the operator is typing.

**Size:** S. **Depends on:** 1.10, 1.20

**Deliverables**

- Console log appender in `src/common/Logging/` that colors each level when output is a terminal, enables virtual terminal sequences on Windows at startup, and writes plain text when output is redirected
- Console input runner in `src/server/shared/Console/` with a `Ambrose>` prompt, line editing, history, and tab completion of command names, redrawing the prompt after each log line. It builds on the reader thread, command thread, `ConsoleCommandTable`, `help`, `shutdown` and `Console.Enable` that 2.13 added, and moves command replies onto the same writer as the log lines so the two cannot interleave
- Built-in commands available before CommandMgr exists: `help`, `status` (revision, uptime, sessions once 1.22 lands), `shutdown [seconds]`, and `reload config` once 4.15 lands; 4.02 later registers them in CommandMgr
- `Console.Enable` and `Console.Colors` options in each app's `.conf.dist`, documented in `doc/config/<app>.md`; `Console.Colors` applies from the next line after a config reload

**Acceptance**

- [ ] Dev-gated: in a terminal, error lines render red and warning lines yellow on Windows and Linux. Needs the maintainer's own Windows console and Linux terminal, so it is run by hand and recorded
- [x] `gameserver > out.log` writes no escape sequences to the file (verified; a redirected `gameserver --check` wrote 1950 bytes of INFO and ERROR lines holding no 0x1B byte)
- [x] `status` prints the revision and uptime, the up arrow recalls the previous command, and `shutdown` exits 0 (ServerAppTest.StatusAndADelayedShutdownAnswerOnTheConsole, ConsoleLineEditorTest.HistoryRecallsPreviousLinesAndTheDraft, ConsoleKeyDecoderTest.EscapeSequencesBecomeNavigationKeys, and the loginserver AppSmoke test for the exit code)
- [x] Log lines arriving while a command is half typed do not corrupt the typed text (ConsolePromptTest.ALogLineErasesAndRedrawsTheHalfTypedCommand and ConsolePromptTest.ALineWiderThanTheWindowScrollsSidewaysInsteadOfWrapping, byte for byte on a fake console; the prompt is held to one row so a line wider than the window scrolls sideways instead of wrapping)
- [x] With `Console.Enable = 0` the app runs with no input thread and still shuts down on Ctrl+C (ServerAppTest.ConsoleEnableZeroStartsNoReader and ServerAppTest.InterruptSignalShutsDownGracefully)

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

**Size:** M. **Depends on:** 17.02, 1.22

**Deliverables**

- A stats registry in `src/common/` that subsystems publish named values into
- `GET /api/status` returning uptime, process memory, thread count, connected sessions, and for the gameserver the average and maximum tick time over the last 60 seconds
- Later milestones add fields as their systems exist: players online (4.01), sessions at character select (4.05), realms (4.03), zones loaded with players per zone (4.09), database pool usage (2.01), pending SQL updates (2.06), the settings generation (4.16), and the last reload result per target (4.15)
- `GET /api/capabilities` listing the reload targets, schedule actions, announcement channels and problem codes the running build supports, built from the same registries the build uses, so the panel offers only what the build can do and follows a newer build after an update
- `GET /api/apps`, the one app list every later page reads: an app answers with itself, its role, realm, address and port, and the supervisor answers the same shape in 17.14 with the apps the signed-in user may see, so nothing stores a second list
- Structured problem records in the status response, each with a code, a message and a subject, for conditions such as a missing install, a stale type dump, an unreachable database, a pending schema update or a revision `Login.AllowedRevision` does not list

**Acceptance**

- [ ] Two fake clients from the 1.22 test harness show `sessions: 2`, and disconnecting both shows 0 within one second
- [ ] Reported memory is within 10 percent of the operating system's own figure
- [ ] A schema test confirms fields are only ever added, never renamed or removed
- [ ] With the type dump removed, the status response carries the stale or missing type dump problem, and it clears once the dump is rebuilt
- [ ] `GET /api/capabilities` lists every reload target and problem code the build registers, and a test fails when a registry gains an entry the response leaves out
- [ ] `GET /api/apps` from a single app returns exactly that app, with no field the supervisor's answer lacks

## 17.04 Live log stream

**Goal:** Dashboards can follow a server's logs live with filtering, without slowing the server down.

**Size:** M. **Depends on:** 17.02, 1.10

**Deliverables**

- One stream layer in `src/server/shared/Admin/` with sequence numbers, a bounded backlog, resume after a sequence number, a dropped marker naming missed ranges, and a bounded per-subscriber queue; `/api/logs` is its first user, 17.12's `/api/events` and 17.26's panel socket reuse it rather than building their own
- WebSocket `/api/logs` streaming structured records with time, level, category, message and a sequence number
- Per-subscriber filters for minimum level and categories
- A backlog of the last 1000 records sent to each new subscriber, and resume after a sequence number, which sends the records since then or, when they have left the backlog, a dropped marker naming the missed range
- A bounded queue per subscriber that drops the oldest records and reports how many were dropped, with their sequence range, so logging never blocks
- Arguments of sensitive commands and values of secret settings never appear in streamed records, as they never appear in log files

**Acceptance**

- [ ] A subscriber filtered to warnings receives only warnings and errors
- [ ] A subscriber that stops reading does not change logging latency by more than 10 percent across 100000 log lines
- [ ] A reconnecting subscriber receives the backlog
- [ ] A subscriber resuming after sequence number N receives exactly the records after N, or a dropped marker with the missed range when those records left the backlog
- [ ] A command marked sensitive and a secret setting change stream with their values redacted, and a search of the streamed records finds neither value
- [ ] The stream layer's sequence, backlog, resume and drop behavior is unit tested once, without opening sockets

## 17.05 Remote command console with audit log

**Goal:** Operators can run server commands from the dashboard, and every remote command leaves a record.

**Size:** M. **Depends on:** 17.01, 17.02, 4.02

**Deliverables**

- `POST /api/command` running a command through CommandMgr at console security level, or at a lower level the caller passes, and returning its output lines with a request id
- A cap of 4096 bytes on a command payload and a refusal of unknown keys in its body, so a newer client cannot smuggle a field an older server ignores
- An audit log of every remote command with time, remote address, the acting panel user when the supervisor relays it, command, security level, and result, written to a file and to a db_login audit table once one exists; it also records every setting change and reload made remotely, alongside the setting_audit rows from 4.16
- Refused commands, including those above the caller's level and those missing their confirmation, are recorded with the reason
- A confirmation flag required for destructive commands such as shutdown and account deletion
- Arguments of commands marked sensitive, such as passwords, are replaced by a redaction marker in the audit log and in the response echo

**Acceptance**

- [ ] Creating an account through the API succeeds and appears in the audit log
- [ ] `shutdown` without the confirmation flag is refused
- [ ] A failing command returns its error text with `success: false`
- [ ] A command above the passed security level answers as an unknown command, runs nothing, and writes a refused audit row
- [ ] `account create test secret` leaves no `secret` in the audit file, the audit table or the response
- [ ] A payload over 4096 bytes and a body with an unknown key are both refused with 422 and run nothing

## 17.06 Dashboard app and overview page

**Goal:** A modern web dashboard shows the health of every server at a glance, on desktop or phone.

**Size:** L. **Depends on:** 17.03, 17.73

**Deliverables**

- `apps/dashboard/`, built to static files that the admin API can serve at `/` and that the supervisor serves in 17.14 without a second build
- The app list read from `GET /api/apps` on the host that served the page, so the browser stores no server list and no per-app token: served by an app it shows that app, and served by the supervisor in 17.14 it shows the apps the signed-in user may see
- A route table in which each route names the permission it needs and whether it appears in navigation, so later pages hide what the user cannot use and a direct link shows an access-denied page
- Overview cards per app showing health, role, realm, address and port, uptime, revision, sessions, players against the realm's limit, tick time average and maximum, and badges for crash loops, restart required, pending SQL updates, client revision mismatch and open problems, with automatic reconnection and a visible stale state
- Problem records from 17.03 shown on the card with a button that opens the page that fixes them
- Actions and fields the build does not report, read from `GET /api/capabilities`, hidden rather than shown failing
- Errors shown beside the form or dialog that caused them, with each field of a 422 response marked and the response's request id shown
- A responsive layout that works at phone width, with light and dark themes that default to the system setting, and no request to any host other than the dashboard's own
- Dashboard build and tests added to CI

**Acceptance**

- [ ] With loginserver and gameserver running, both show online within 2 seconds, and stopping one marks it offline within 5 seconds
- [ ] The overview is usable at 400 pixels wide with no horizontal scrolling
- [ ] The built dashboard served by the admin API loads with no browser console errors
- [ ] With the type dump removed, the gameserver card shows the problem, and its button opens the client data page
- [ ] Loading the built dashboard makes no network request to any other host
- [ ] A capability the build does not report leaves its control out of the page, and nothing in the browser holds an app token or a stored server list
- [ ] A test over the route table fails when an entry names no permission or leaves out its navigation flag, and a direct link to a route the caller may not use shows the access-denied page
- [ ] A 422 answer marks every field it names beside the form that caused it, and the request id in the answer is shown on the page
- [ ] The dashboard's build and its tests run in CI, and a failing dashboard test fails that job

## 17.07 Dashboard log viewer and command console pages

**Goal:** Operators read logs and run commands from the dashboard as comfortably as at the server's own console.

**Size:** S. **Depends on:** 17.04, 17.05, 17.06

**Deliverables**

- A log viewer with level and category filters, text search with highlighted matches, pause and resume with a count of new lines, a jump-to-newest control, copy, and a tab per server; it resumes after its last sequence number when the connection returns instead of clearing
- A command console page with output that notes commands are audited, showing each command's result lines under the command, asking before a destructive command sends its confirmation flag, and completing command names the caller's security level allows
- Recall of the commands typed in the open page, held in memory only, so nothing here has to be replaced when 17.49 delivers the one stored history per panel user and app

**Acceptance**

- [ ] Filtering to errors hides lower levels immediately, and pausing stops scrolling while counting new lines
- [ ] Running `status` shows the same output as the local console
- [ ] Dropping the connection for 10 seconds while logging continues shows the missed lines, or a gap marker, after reconnecting, without clearing the view
- [ ] Sending `shutdown` asks for confirmation first, and cancelling sends nothing
- [ ] Reloading the page clears the recall list and stores nothing in the browser

## 17.08 Process control, config, and database pages

**Goal:** Operators start, stop, and restart servers, review configuration, and see database update state from the dashboard.

**Size:** M. **Depends on:** 17.06, 2.06

**Deliverables**

- A supervisor that starts, stops, and restarts loginserver, gameserver, and patchserver, restarts an app that exits unexpectedly, records each exit code, and exposes an admin endpoint; the tracked operation model, locks, backoff, crash classification and crash records are 17.27 and 17.60, so nothing here is built twice
- Readiness from each app's `GET /api/health` lifecycle state rather than console output, with a `ready` lifecycle line as the fallback when the admin API is off
- A graceful stop that first asks the app's admin API to shut down with its own countdown and drain, so the login server sends its 2.15 notice and the gameserver saves characters, then sends `shutdown` on standard input, then Ctrl+Break or SIGTERM to the process group, and only after a per-app stop timeout ends the process tree through its job object or process group
- The desired state of each app saved when it changes; on supervisor start, apps still running are adopted again after checking their process id, process start time and executable path, and apps meant to run are started
- Output of each app captured by the supervisor as a fallback log source, kept for the current and previous run
- A config page comparing each app's effective settings with its `.conf.dist` defaults and showing each value's source layer. Edits go through the settings API, apply live, and are audited; the editing part is built in 17.13. Only options documented as restart-required show that, with the reason
- A database page listing applied and pending update files per database from the updater, applying data-only updates live and reloading the affected stores, and showing connection pool use, plus, once 4.15 lands, the journal of live world-database edits, exportable as pending SQL updates

**Acceptance**

- [ ] The restart button restarts gameserver while loginserver sessions stay connected
- [ ] Killing gameserver from outside makes the supervisor restart it and the dashboard shows one crash
- [ ] The config page shows each value's effective value, default, and source layer, and marks only options documented as restart-required, with the reason
- [ ] Stopping loginserver from the dashboard sends its shutdown notice to a connected fake client before the process exits
- [ ] Restarting the supervisor while apps run adopts them without restarting them, and a process id reused by an unrelated program is not adopted
- [ ] With an app's admin API off, the supervisor's own capture of its output is what the page shows, and the previous run's output is still readable after a restart
- [ ] The database page lists applied and pending update files per database, applies a data-only update live and shows pool use, and an update the running binary's schema needs is listed as restart-required instead of applied

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

**Size:** S. **Depends on:** 17.03, 17.04, 17.73

**Deliverables**

- A `--tui` option that draws full-screen panels for status, sessions, logs, and a command line when output is a terminal, using FTXUI from vcpkg

**Acceptance**

- [ ] Resizing the terminal lays the panels out again, and `q` or Ctrl+C exits with code 0
- [ ] Without a terminal, `--tui` logs a warning and falls back to the normal console

## 17.12 Settings and reload admin API

**Goal:** Operators read and change live settings and trigger reloads over the admin API, with every change validated and audited.

**Size:** M. **Depends on:** 17.02, 17.04, 17.05, 4.16

**Deliverables**

- `GET /api/settings` returning each setting's schema, default, effective value, source layer, apply mode, lock, visibility (normal or secret) and edit class (normal or restricted); a secret value is masked unless the caller asks for it with the right to see it, and every reveal is audited
- `PUT /api/settings/{key}` taking a value and a reason, returning 422 with every validation error, and refusing a key locked by an environment variable or command-line override with an error naming the layer
- `POST /api/settings/batch` applying all entries or none
- `GET /api/settings/{key}/history` returning audit rows with old and new values, who, and why, with secret values masked
- `POST /api/reload/{target}` and `GET /api/reload` returning each target's generation, last result, and errors
- WebSocket `/api/events` streaming setting changes and reload results on 17.04's stream layer, so its sequence numbers, backlog, resume and drop marker are the same code

**Acceptance**

- [ ] An out-of-bounds PUT returns 422 and changes nothing
- [ ] A PUT to a key set by an environment variable is refused and names the locking layer
- [ ] A batch with one bad entry applies none
- [ ] A change appears on a second dashboard within one second
- [ ] A reload of a broken message definition returns every error while the old generation keeps serving
- [ ] Reading `Account.VerifierKeys` without asking to reveal it returns a masked value, and its history shows masked old and new values
- [ ] An events subscriber resuming after a sequence number receives exactly the missed events or a dropped marker, through the same test the 17.04 layer passes

## 17.13 Dashboard settings editor and reload page

**Goal:** Operators change any live setting and reload any store from a browser on a desktop or a phone, without restarting a server.

**Size:** M. **Depends on:** 17.06, 17.12

**Deliverables**

- A settings page by category with search, typed and bounded inputs, the default and source layer shown, a required reason, a review step before apply, and per-key history with revert; it is where the 17.08 config page's values are edited
- Secret settings shown masked with a reveal control for callers allowed to see them, restricted settings marked, and locked keys shown with the layer that locks them
- Settings presets: export a chosen category or set of keys, such as a realm's rates and timeouts, to a versioned JSON file, and import one with validation against the schema and a diff before applying; an import never removes or resets keys the file does not name
- A reload page showing each target's generation, last result, and errors, with a button per target
- A restart-required list with the reason for each case

**Acceptance**

- [ ] Changing Rate.Drop.Item from a phone applies to the running gameserver, and history shows who and why
- [ ] Editing a config value applies without a restart and survives one
- [ ] Invalid values can't be submitted, and server refusals show their message
- [ ] Revert restores the old value and writes a new audit row
- [ ] Importing a preset with one out-of-bounds value shows the error in the diff and applies nothing
- [ ] A secret setting shows masked, its reveal control returns the value only for a caller allowed to see it, and every reveal writes an audit row

## 17.14 Panel listener, TLS, sessions and the audit store

**Goal:** The panel has one hardened entry point of its own: a listener that refuses to carry sign-ins insecurely, sessions that cannot be replayed or forged, a rate limit every costly route declares itself into, and the audit tables every later milestone writes its record into.

**Size:** L. **Depends on:** 17.08, 1.12, 1.19

**Deliverables**

- `Panel` in `src/server/apps/supervisor/` serving the panel API under `/api/panel/` and the dashboard built in 17.06 at `/`, off unless `Panel.Enable = 1`, bound to 127.0.0.1 by default, on the same Crow and Asio layer as 17.02
- Proposed, extending the settled Remote access rule under Decisions, Operations in doc/ARCHITECTURE.md to the panel's own listener, which carries session cookies, passwords and two-factor codes: startup refuses a non-loopback bind unless a certificate and key are configured, or the operator sets `Panel.AllowPlainHttpRemote = 1`, off by default, which logs a warning naming the option and states that sign-in secrets then cross the network unencrypted; a reload that would leave the bind unsafe is refused with an error naming the option while the old listener keeps serving
- Certificate and key in PEM from `Panel.TlsCertificate` and `Panel.TlsKey`, checked at load for a matching key, validity dates and chain order, swapped live on reload with the old pair kept and every error reported on failure, the fingerprint printed to the console and log, and a warning as expiry approaches; `supervisor --panel-self-signed` writes a certificate for a machine-local panel and prints its fingerprint
- Signed, expiring session cookies (HttpOnly, SameSite=Strict, host-prefixed, Secure whenever TLS is on) stored as hashes with idle and absolute lifetimes, a session generation per user that 17.46 to 17.48 bump on a password, two-factor, role or grant change, and a double-submit CSRF token required on every state-changing request
- A cost-weighted rate limit on session-authenticated routes, per user and per address, where each route declares its cost as it registers and a route with no declared cost is uncosted, so the routes that cost real work are limited as they land: backup creation (17.16), archive and search jobs (17.39), activity exports (17.25), settings batches (17.12), database host tests (17.30) and remote pulls (17.41). A throttled request answers 429 with a retry hint, and one audit row records each throttled user and minute
- The panel store's audit tables, `audit_event` and `audit_subject`, with an event id for idempotent forwarding, batch id, time, event name in `namespace:path.action` form, actor type and id, address, user agent, node, result, error, reason, properties and any number of subjects per event, written in the same transaction as the change they record, so a change cannot outlive its record; a security-relevant write whose audit row cannot be written fails closed. 17.49 binds every action to these tables through its `AuditScope`, and 17.25 adds the event catalog and the activity pages, so no milestone writes a second store
- `Panel.TrustedProxies`: the client address is the rightmost hop not in that list, forwarded headers from any other peer are ignored, and every throttle, audit row and sign-in record uses that address
- HSTS under TLS, a strict Content-Security-Policy, frame-ancestors none, nosniff and same-origin referrer on every response, and a dashboard that requests nothing from another host

**Acceptance**

- [ ] `Panel.BindIP = 0.0.0.0` with no certificate logs an error naming the option and exits 1, and the same change on a reload is refused while the old listener keeps serving
- [ ] With `Panel.AllowPlainHttpRemote = 1` and no certificate, a non-loopback bind starts and logs a warning naming the option and the risk
- [ ] Replacing the certificate and reloading serves the new one with no restart, and a key that does not match the certificate leaves the old pair serving with the error reported
- [ ] A state-changing request without its CSRF token is refused, and a session cookie replayed from another origin does not authenticate it
- [ ] Two hundred requests in a minute from one session to a route registered with a cost answer 429 with a retry hint while an uncosted status read from the same session still answers, and one audit row records the throttling
- [ ] A state-changing request whose audit row cannot be written is refused with the audit failure named rather than applied unrecorded, and the record and the change it describes commit together or not at all
- [ ] With `Panel.TrustedProxies` empty, a forwarded header naming another address changes neither the throttled address nor the address in the audit row
- [ ] Every response carries the CSP, frame-ancestors, nosniff and referrer headers, and HSTS only under TLS
- [ ] The 17.06 dashboard loads from the panel listener with no browser console errors and no request to another host

## 17.15 Schedules with in-game countdowns

**Goal:** Operators schedule restarts, backups, commands and announcements, and players get warned in game before a restart.

**Size:** L. **Depends on:** 17.05, 17.27, 17.48, 2.15, 6.01, 6.04

**Deliverables**

- `ScheduleMgr` in the supervisor with no cron daemon or queue: a heap of due times on one timer, re-armed when a schedule changes or the wall clock jumps, with each run's state committed to the store before and after every task; cron parsing and next-run math in `src/common/Time/`
- Schedules with a cron expression (five fields, names, steps and the `@daily`-style macros) or a one-time date in a chosen IANA time zone, an overlap policy and a misfire grace, each running an ordered list of tasks timed after the previous task or from the scheduled time, with offsets: announce, run a command, restart, stop, start, back up, reload, set a setting, and update once 17.17 lands
- Parse errors returned with the field and position, refusal of expressions that never fire, and a preview of the next five runs with daylight saving notes
- Time zone data reloaded with the supervisor's reload, which recomputes every next run; a schedule whose zone the new data no longer holds is held with an error instead of firing at the wrong time. The data source where a platform's library lacks one is a proposal in doc/PANEL.md
- A version per schedule, saves that take `If-Match`, and a stale save answered with 409 and the current version
- Task completion that waits for the real result through 17.27's operations: a restart when the app is healthy again, a command when its output returns, a backup when it is verified; a failure policy per task of stop, continue or retry with a delay, and always-run cleanup tasks
- Saving a task, running a schedule now, changing its timing and resuming it each require the permission of every task's action, and an automatic run is skipped as permission revoked when its author no longer holds them
- Restart countdowns that warn connected players at set intervals, on the gameserver through the 6.01 zone broadcast and 6.04 GM messages, and on the loginserver through the 2.15 shutdown notice as the final warning only, as this phase's risk note explains
- Run records with trigger, who, scheduled and actual start, status, skip reason, a snapshot of the tasks as they ran, and each task's result and capped output; runs left in progress by a crash are marked interrupted and their power, backup and update steps are not repeated

**Acceptance**

- [ ] A schedule set to restart the gameserver at a set time with a 5 minute countdown warns connected players at 5, 1 and 0 minutes and restarts at that time, and one on the loginserver sends its shutdown notice at 0 minutes
- [ ] A daylight saving change neither skips nor doubles a schedule's run in its time zone, in America/New_York, Europe/London and Australia/Lord_Howe
- [ ] Two editors saving the same schedule leave the second with 409 naming the current version, and the first save intact
- [ ] Reloading the supervisor after a time zone data update recomputes the next run of a schedule in a changed zone, and a schedule in a zone the data lost is held with its error
- [ ] A schedule whose restart task finds the gameserver locked by a restore waits for the lock or records a skip with the reason, and never reports success
- [ ] Restarting the supervisor keeps schedules and runs a missed one-time task once if it is still within its grace window
- [ ] Killing the supervisor during a restart task leaves that run marked interrupted, and the restart is not repeated on the next start
- [ ] A user who holds `schedules.run` but not `power.restart` cannot run a schedule containing a restart task

## 17.16 Backups: consistent dumps and verified archives

**Goal:** Operators back up everything a realm needs, with each backup verified and recorded well enough that a restore can be checked against it.

**Size:** L. **Depends on:** 17.08, 17.27, 17.48, 2.06, 5.03

**Deliverables**

- Backups of named components: every Ambrose database as consistent logical dumps taken inside one read-only transaction per database server, the config, data and type dump folders, and the supervisor's store, packed into one zstd-compressed archive with a SHA-256 manifest
- Online gameservers flush their write-behind queue and save characters through the 5.03 persistence path before the dump begins, so nobody is kicked and no character is captured mid-save
- A snapshot record written inside each dump transaction: per table the row count and a row checksum, plus the last applied update per database, the client revision and the Ambrose version, so 17.51 can prove a restore matches the moment the dump began
- A status per backup (queued, dumping, archiving, verifying, succeeded, failed, interrupted, cancelled) with phase, progress, error and the storage location on the record; jobs left running by a crash are marked interrupted at the next start and their partial files removed
- Verification that reads the finished archive back, checks every file against the manifest and the snapshot record against the dump, and only then marks the backup succeeded
- Local storage in a folder readable only by the service user, with a free space check and a reservation before the dump starts; S3-compatible storage follows in 17.43
- The archive holds the type dump and extracted data built from the operator's own client install, so the Client-derived data review note applies: the contents list names every component, and no component carries a client file the operator did not place in a backed-up folder. Whether archives are sealed is 17.72 and a decision in doc/ROADMAP.md
- A backups page listing status, size, duration, contents and the snapshot record, permission checked and audited

**Acceptance**

- [ ] A backup taken while accounts are created and deleted through the console records a snapshot whose row counts and checksums equal those of a dump taken with the database quiet
- [ ] An archive that fails verification is marked failed, never succeeded, and its partial files are removed
- [ ] A backup of a realm with players online completes with every character saved and no player disconnected
- [ ] Killing the supervisor during a dump leaves the backup interrupted at the next start, with no partial archive left behind
- [ ] Starting a backup with less free space than its reservation is refused with the volume and figure named, before any byte is written
- [ ] A user without `backups.create` gets 403 and the attempt is audited
- [ ] The backup's contents list names every component and its byte count, and a component the archive does not hold cannot be listed

## 17.17 One-click updates and rollback

**Goal:** Operators move to a newer Ambrose build with one click, and a failed update rolls back by itself.

**Size:** M. **Depends on:** 17.16, 17.51, 17.52, 3.23

**Deliverables**

- Update sources: a release channel whose packages are checked against published SHA-256 sums and signatures, and a build channel that pulls a git branch and builds it with the project's presets; channel checks are the only outbound requests the panel makes, and only when a channel is configured
- Versioned install folders kept side by side, so an update installs next to the running build and the switch is a pointer change
- An update run in the protected updating state that takes a pre-update backup through 17.16, installs, runs `--check` on every app, switches, restarts through the supervisor, and waits for health; a failed check or health wait switches back, and restores through 17.51 only the databases whose update level changed
- Each step streamed live with its result, and the whole run recorded and audited
- The pre-update backup pinned for the rollback window through 17.52, and the last few builds kept for manual rollback, with each build's version, commit and changelog shown
- When 3.23 reports a newer KingsIsle client revision, the panel shows it next to the Ambrose update and can rebuild client data before the restart

**Acceptance**

- [ ] Updating to a newer build keeps accounts and characters and shows the new version after the restart
- [ ] A build whose gameserver fails `--check` is never switched in, and the running build keeps serving
- [ ] A build that starts but fails its health wait is rolled back to the previous build within the configured time, and the database matches the backup
- [ ] A release package with a wrong checksum or signature is refused before any file is written to the install folder
- [ ] Retention cannot remove the pre-update backup during the rollback window
- [ ] Every step of an update run appears live with its result, the finished run is recorded with who started it and its outcome, and a newer client revision reported by 3.23 shows beside the Ambrose update with its rebuild offer

## 17.18 File roots, the path jail and browsing

**Goal:** Operators browse the server's own files from the panel, and no request can reach outside the roots or read a secret the caller may not see.

**Size:** L. **Depends on:** 17.12, 17.48

**Deliverables**

- Named roots with policies: install (read-only), config, logs, data (type dumps read-only, lock files hidden), custom SQL, backups (read-only here) and the user's client installs (read-only and never downloadable); the supervisor's store, keyring, token files and TLS keys sit outside every root and are refused
- A jail that resolves each request to a root handle and checks each component: one decoding pass, no absolute paths, drive letters, UNC prefixes, backslashes, empty, `.` or `..` components, control characters, names Windows cannot hold or reserved device names on any system; on Linux `openat2` beneath the root or a component-by-component walk without following links, and on Windows handle-relative opens that refuse reparse points and compare the final path with the root; FIFOs, devices and sockets are refused before any read
- Protected path patterns per root, matched on the resolved path and applied to every operation, and secret `.conf` values shown redacted without `settings.secrets.read`
- A space guard with a minimum free space per volume and a reservation that every writer takes before streaming
- Listing and reading with paging, sorting, size, type and modification time, and each root's policy in the response, so the page disables what the root refuses instead of failing on submit
- Roots marked client-derived refuse download, archive and any share path for every caller whatever their permissions, as the Client-derived data review note requires
- Every refused traversal audited with the resolved path, while the response never shows it
- A files page per root and per app with breadcrumbs, selection, a context menu and mass actions, rendered from the root's policy in the listing response so a control the root refuses is disabled rather than failing on submit, with a bottom sheet at phone width; uploads, downloads and mass copy are 17.54 and archives and search 17.39

**Acceptance**

- [ ] Requests for `../`, an absolute path, an encoded traversal, a device name such as `CON`, or a symbolic link or junction leaving the roots are refused with 403 and audited with the resolved path, which the response does not carry
- [ ] A download of a file under the client install root is refused for an owner as well as a viewer, on every path including a share link
- [ ] Reading a `.conf` file without `settings.secrets.read` shows every secret value redacted
- [ ] A write that would leave less than the minimum free space is refused with the volume and the figure named, before any byte reaches the disk
- [ ] A FIFO, a device node and a unix socket placed inside a root are refused before they are opened
- [ ] The jail's component and link checks pass in the unit tests on Windows and on Linux
- [ ] A protected path is refused for listing, reading and every write operation, not only for writes
- [ ] The files page walks into a folder by its breadcrumbs, selects a filtered set and shows its count, and on a read-only root every write control is disabled with the root's policy named

## 17.19 Built-in resource graphs

**Goal:** Operators see CPU, memory, network, disk, players and tick time over time in the panel itself.

**Size:** M. **Depends on:** 17.03, 17.06, 17.09

**Deliverables**

- The supervisor samples every app's CPU, memory, threads, open handles, network in and out, and disk use every few seconds through the operating system, keeping a day at full detail in memory and 30 days downsampled on disk, with no Prometheus or Grafana needed; network traffic comes from each app's own counters
- Graphs per app and per realm for those values plus sessions, players online, sessions at character select and tick time, with ranges from 5 minutes to 30 days and a live view that does not reset when an app stops
- Downsampling on write, so any range reads a bounded number of points, and a stretch while an app was stopped drawn as a gap rather than as zero
- History that survives a supervisor restart, with the day's full-detail samples folded into the long series exactly once
- A benchmark that reports the sampler's cost in microseconds per app per sample, recorded with the milestone instead of asserted as a share of a core
- Alert rules, delivery and acknowledgement are 17.67, so this milestone needs no mail settings

**Acceptance**

- [ ] With gameserver under a synthetic load, the panel's CPU graph is within 10 percent of the operating system's own figure
- [ ] A 30 day graph loads in under one second with a month of samples
- [ ] Stopping an app leaves a gap in its graph, and the live view keeps updating for the other apps
- [ ] Restarting the supervisor keeps the history, and a day's samples appear in the long series once, not twice
- [ ] The benchmark reports microseconds per app per sample, and sampling 20 apps stays under one millisecond of work per round

## 17.20 Client data and revisions page

**Goal:** Operators see which client install and type data each server uses, and rebuild it from the panel.

**Size:** S. **Depends on:** 17.06, 3.23

**Deliverables**

- A page listing the client installs found, the revision each server uses, the type dump in use with its revision, executable hash, extractor version and build time, and the message definitions, name tables and creation config loaded
- The revision following state from 3.23: the newest revision seen, whether data for it is built, and any build in progress with its live output
- Buttons to rebuild client data and to switch a server to a different install or revision, each checked, audited, run as the protected setup state so power actions wait, and applied live where 3.23 supports it
- The page reads the user's own install at runtime and never serves or copies client files

**Acceptance**

- [ ] After a client revision change, the page shows the new revision within a minute and the rebuild's live output while it runs
- [ ] A failed rebuild shows the extractor's error, and the servers keep using the previous data
- [ ] A restart requested during a rebuild is refused with the rebuild named
- [ ] No response from this page carries a client file's bytes, and the install root stays outside every downloadable root

## 17.21 Accounts, bans, characters and online players pages

**Goal:** Game masters manage accounts, bans, characters and online players from the panel.

**Size:** M. **Depends on:** 17.05, 17.48, 3.17, 6.05

**Deliverables**

- An accounts page with search by username, email, address or MachineID, create, password reset, lock and unlock, security level, email and last sign-in address, backed by AccountMgr from 2.13; a password reset seals the verifier with the active key, deletes `account_session` and kicks live sessions, and a generated password is shown once and never logged
- Email, address and MachineID hidden from users without `accounts.pii.read`
- A bans page for account, address and machine bans with duration, reason, who and when, and unban with a reason, matching the 6.05 console commands
- A characters page per account listing characters with level, school and location, with rename, restore of a deleted character and delete as 3.17 and later phases support them, and an edit form whose fields are those the running build reports as editable through `GET /api/capabilities`, such as gold and level once later phases make them so, each applied through its own GM command
- An online players page with realm, zone and session time, joins and leaves arriving live, and kick, mute and teleport actions from 6.05, 12.07 and 6.06 once those exist
- Every action goes through CommandMgr with the panel user's permissions and command level, refuses to act on an account at or above the user's own security level, requires a reason for bans, locks and security level changes, and is audited, refused attempts included
- A search in the top bar over accounts, characters and the apps the caller may see, each result checked against the caller's own permissions before it is returned and each row opening its page

**Acceptance**

- [ ] Banning an account from the panel while its player is on character select disconnects that client with the ban message, through the 6.05 ban path
- [ ] An operator without `accounts.ban` cannot ban, and the attempt is audited
- [ ] Dev-gated: creating an account from the panel lets a real client sign in with it. Needs the maintainer's own retail client, as doc/PATCHING.md describes
- [ ] A panel user linked to a game master account cannot ban or reset an administrator account
- [ ] A user without `accounts.pii.read` sees no email, address or MachineID in any account response
- [ ] The top-bar search finds an account by username and a character by name, and returns nothing for an app or account the caller holds nothing on
- [ ] The character edit form offers only the fields the build reports as editable, and a field the build does not report cannot be submitted

## 17.22 Nodes: one panel for servers on several machines

**Goal:** One panel manages loginservers, gameservers and patchservers running on several machines.

**Size:** L. **Depends on:** 17.26, 17.28, 17.29, 17.32, 17.49

**Deliverables**

- A node mode for the supervisor on each extra machine, joined to the panel with a one-time join token, stored hashed and valid for minutes, that becomes mutually authenticated TLS with pinned certificates; certificates reissue with an overlap window, and removing a node revokes its pin at once
- The local supervisor registered as the first node, and locations that group nodes
- A heartbeat pushed by each node with its build, operating system, resources, app states, players, tick times, client revision and type dump revision
- A supervisor protocol version in the join and in every heartbeat: the panel refuses a node whose protocol is newer than its own and names both versions, keeps a node one version behind working read-only with a badge and a message naming what it cannot do, and a node refuses a command it does not understand instead of guessing
- A nodes page with each node's health, resources, apps, launch settings from 17.28 and port allocations from 17.29, a maintenance flag that stops new placements, badges the node and puts the realms placed on it into the 17.32 realm maintenance state, and the ability to place an app or realm on a node; moving one follows in 17.42
- A node may report on and receive commands only for apps placed on it, and config pushed to a node is validated, written and swapped, with refusals naming the layer that locks a key
- Console, logs, files, backups, schedules, graphs and power actions work the same for apps on any node, relayed by the panel under the signed-in user's permissions through 17.49; browsers only ever connect to the panel
- A node that loses the panel keeps its apps running and its schedules on time, buffers audit rows and run results, and resyncs when the link returns. Whether a node's schedules run on the node or centrally is a proposal in doc/PANEL.md

**Acceptance**

- [ ] Dev-gated: a gameserver on a second machine shows in the panel, and restarting it from the panel works. Needs the maintainer's own second machine or a virtual machine
- [ ] Dev-gated: cutting the network between panel and node leaves the node's apps serving players, and the panel shows the node offline, then online again within 10 seconds of the link returning. Needs the maintainer's own second machine or a virtual machine
- [ ] A node presenting a certificate other than the pinned one is refused
- [ ] A node asking about an app placed on another node is refused
- [ ] A node reporting a newer protocol version is refused with both versions named, and one a version behind shows read-only with its badge
- [ ] Audit rows written on a node while its link was down reach the panel once, not twice, after it returns
- [ ] Placing an app on a node writes its launch settings and reserves its allocations, and a placement onto a node in maintenance is refused
- [ ] Putting a node into maintenance puts every realm placed on it into the 17.32 maintenance state and badges those realms, and leaving maintenance clears both

## 17.23 Operating system services, Docker image and Pterodactyl egg

**Goal:** Ambrose runs as a background service, in Docker, or under an existing Pterodactyl panel with no manual setup.

**Size:** M. **Depends on:** 17.08

**Deliverables**

- `supervisor --install-service` and `--uninstall-service`, which register the supervisor as a Windows service or a systemd unit running as a dedicated user and starting at boot
- A multi-stage Dockerfile for a small runtime image, and a Docker Compose file with MariaDB, the supervisor and the apps, with volumes for config, data, logs and backups, the client install mounted read-only, and health checks
- Time zone data in the image, so the 17.15 schedule engine has a source wherever the platform library lacks one; the data source, and whether the image is where it ships, is a proposal listed under Decisions needed in doc/ROADMAP.md
- A Pterodactyl egg that installs and starts Ambrose, exposes its settings as startup variables, uses the ready lifecycle line as its startup done string and `shutdown` as its stop command so Wings does not count a stop as a crash, and maps the console, for hosts that already run Pterodactyl
- Packaging docs in doc/OPERATIONS.md, with client data built on first start through 3.22 from a mounted client install

**Acceptance**

- [ ] Dev-gated: after `--install-service` and a reboot, the servers are running and the panel is reachable, on Windows and on Linux. A reboot cannot run in CI, so it is run by hand and recorded
- [ ] `docker compose up` on a clean machine with a client install mounted reaches a ready loginserver with no other steps
- [ ] Dev-gated: importing the egg into a Pterodactyl panel creates a server that installs, starts, shows its console and stops cleanly. Needs the maintainer's own Pterodactyl install
- [ ] Dev-gated: stopping the egg's server from Pterodactyl leaves no crash message in its console. Needs the maintainer's own Pterodactyl install
- [ ] The image's time zone database resolves America/New_York, Europe/London and Australia/Lord_Howe, and an image built without it fails this check rather than starting

## 17.24 Desktop control app

**Goal:** A player hosting on their own computer starts the database, servers, panel and client from one icon.

**Size:** L. **Depends on:** 17.06, 17.08, 17.46, 1.21, 3.22

**Deliverables**

- `apps/desktop/`, an installer and tray app for Windows and Linux desktops that installs Ambrose, starts and stops the supervisor, opens the panel, and launches the client through the 1.21 launcher once the loginserver is ready
- Opening the panel from the tray through a fresh 17.46 one-time owner link bound to the local machine, so the player never types a panel password
- Database setup with no steps: it uses a MariaDB or MySQL server it finds, with credentials the user gives, or else installs a private MariaDB into the Ambrose data folder, checked against its published checksum and bound to localhost with generated credentials, and registers it as a database host once 17.30 lands
- First start runs 3.22's automatic setup and shows its progress, and the tray shows server status, players online and update notices from 17.17

**Acceptance**

- [ ] Dev-gated: on a clean Windows machine with Wizard101 installed, installing the app and clicking Play reaches the login screen against the local server with no other steps. Needs the maintainer's own machine and client
- [ ] Quitting from the tray closes the client connection cleanly and stops the servers and the private database, and the next start keeps accounts and characters
- [ ] A private MariaDB download with a wrong checksum is refused, and the app says what failed
- [ ] The tray's panel link signs in once and cannot be reused from another machine

## 17.25 Activity log pages

**Goal:** Operators see who did what, when and from where, for any user, app, realm, account or node, and nothing that happened is missing.

**Size:** M. **Depends on:** 17.05, 17.49

**Deliverables**

- An event catalog with one sentence per event name in `namespace:path.action` form, rendered with escaped values, and a unit test that fails when the code emits an event the catalog has no sentence for
- Read-only events that fail open with an error line, while the security-relevant writes keep the fail-closed rule the 17.14 tables carry, and an API key id on each event so a key's actions are as traceable as a person's
- Activity pages per panel user (events where the user acted or was the subject), per app, realm, node, game account and character, and a global page, with filters by event prefix, actor, subject, result, time range and address, cursor pagination up to 100 rows, a details view for extra properties, and new rows arriving live once 17.57 lands
- Addresses shown only to the actor and to holders of `activity.ip.read`, and CSV and JSON export for holders of `activity.export`; a CSV field that begins with an equals sign, a plus, a minus or an at sign is written so a spreadsheet cannot read it as a formula
- Retention per event class as a live setting, 365 days for security events and 90 days for high-volume events, applied hourly and recorded as one event per sweep

**Acceptance**

- [ ] Every event name the code emits renders a sentence, and a test fails when one is added without a sentence
- [ ] A refused restart appears on the app's activity page with result denied
- [ ] A user without `activity.ip.read` sees no other user's address in rows or exports
- [ ] A user's activity tab shows both their sign-ins and the restarts they ran
- [ ] Filling the store past its disk limit makes a settings change fail with an error rather than apply unaudited
- [ ] A row whose reason begins with an equals sign exports as text, and opening the CSV runs no formula
- [ ] An hourly sweep removes rows past their class retention, keeps security rows for the longer window, and records one event for the sweep

## 17.26 Panel event socket: envelope, tickets and generated types

**Goal:** Every live page in the panel runs on one typed, versioned socket instead of a socket per page.

**Size:** M. **Depends on:** 17.04, 17.12, 17.48

**Deliverables**

- WebSocket `/api/panel/events`, the only socket a browser opens, authenticated by the session cookie with an exact Origin match and a CSRF token in the first frame, and a one-time ticket from `POST /api/panel/events/ticket` for API key clients, sent in the first frame and never in the URL
- The envelope `{v, type, id, scope, seq, time, data}` with structured data, and the client and server message types listed under Event socket in doc/PANEL.md; the protocol and its close codes are a proposal there
- Sequence numbers, backlog and resume taken from 17.04's stream layer rather than written again, with status and power events never dropped and stats keeping only the newest sample
- TypeScript types generated from the schemas the C++ side serializes, a test that fields are only added, and a contract test that fails when the server can send a type the dashboard does not handle
- Full error text only for `debug.errors`, and otherwise a generic message with a correlation id that also appears in the supervisor log

**Acceptance**

- [ ] A socket opened with a foreign Origin is refused, and a ticket works once and only within 30 seconds
- [ ] A page reconnecting after 10 seconds offline receives exactly the missed records or a dropped marker, with no duplicates, through the 17.04 layer's own tests
- [ ] The contract test fails when a server event type is added without a dashboard handler
- [ ] A ticket sent in the URL rather than the first frame is refused, and the URL appears in no log
- [ ] A caller without `debug.errors` receives a correlation id that matches a line in the supervisor log, and no error text

## 17.27 App states, operation locks and power targets

**Goal:** Every start, stop, restart and kill is one tracked operation with a visible result that never overlaps with a restore, update or rebuild.

**Size:** M. **Depends on:** 17.08, 17.26

**Deliverables**

- App states offline, starting, running, stopping, crashed, backoff, crash loop and disabled, plus the protected states setup, updating, restoring and moving that refuse power actions
- A lock per app and a stack lock; a refused action answers 409 naming the holder, its action and start time, and kill may bypass a held lock after marking the app stopping
- Power targets of one app, one realm's gameservers or the whole stack, started in dependency order and stopped in reverse
- `POST /api/panel/power` answering 202 with an operation id, with each step and the result streamed as power progress and result events on the 17.26 socket and recorded in the audit log
- Disable and enable for an app, which stops it and refuses starts while disabled, with the reason and who set it on the record
- Pages for an app in a protected state show the state and its live progress in place of controls, while owners keep the console

**Acceptance**

- [ ] A restart requested while a backup restore holds the app is refused with 409 naming the restore
- [ ] Kill during a stuck stop ends the process and records a requested exit, not a crash
- [ ] Restarting the stack stops gameservers before the loginserver and starts the loginserver before gameservers
- [ ] A power request answers 202 with an operation id, and its progress and final result arrive on the socket and in the audit log
- [ ] Disabling an app stops it and refuses a start with the disable reason named, and enabling it allows the next start
- [ ] An app in the restoring state shows the state and progress instead of power controls, and an owner still reaches its console

## 17.28 Launch and startup settings

**Goal:** Operators control how each app is launched, from its build and overrides to timeouts, restart policy and resource limits, without editing service files.

**Size:** M. **Depends on:** 17.08, 17.13, 17.48

**Deliverables**

- Launch settings per app in the supervisor's store: the build it runs, command-line overrides, environment variables with secret values sealed and masked, working folder, start and stop timeouts, restart policy (always, on crash, never), backoff and crash loop limits, autostart with the supervisor and process priority
- Secret launch values sealed with AES-256-GCM under the supervisor's key, the cipher Decisions, Accounts and the console already settles for `login.account.verifier`; which library provides the panel's cipher and keyed hashes is the crypto stack proposal listed under Decisions needed in doc/ROADMAP.md, which names this milestone, and where the key lives is the keyring proposal there
- Opt-in hard limits on memory and CPU through a job object on Windows and a cgroup on Linux, off by default
- A launch page with a preview of the exact command line and environment, secret values masked, applied at the app's next start and audited with old and new values
- Keys the supervisor passes on the command line are the settled command-line override layer, not a new layer: the settings page shows them locked and names that layer, exactly as an operator's own command line does
- Validation that refuses a missing build, a working folder outside the install, and timeouts or limits outside their bounds

**Acceptance**

- [ ] Changing the gameserver's stop timeout applies at its next stop, and the audit row shows the old and new values
- [ ] An environment variable marked secret never appears unmasked in the page, the API or the audit log
- [ ] A command-line override of `Rate.Drop.Item` shows the key as locked on the settings page naming the command-line override layer, and a live edit is refused naming the same layer
- [ ] With a memory limit set, an app that exceeds it is recorded as out of memory
- [ ] A launch setting naming a build that does not exist is refused with the path named, and the app keeps its previous setting

## 17.29 Port allocations and listen addresses

**Goal:** Operators see and change every address and port each app listens on, with conflicts caught before an app fails to bind.

**Size:** M. **Depends on:** 17.28

**Deliverables**

- Allocations per node with bind address, port, protocol, public alias, the app and role using it (login listener, game listener, patch HTTP, admin API, panel, metrics) and notes, unique by node, address, port and protocol, seeded on first run from the shipped defaults
- Adding an allocation or a range of at most 1000 ports checks that each port is free by binding it, accepts IPv6, and warns about ports below 1025
- Assigning a port to an app writes its listen setting through the settings API, which rebinds live and keeps the old listener when the bind fails, and, once 4.03 lands, updates the realm list address and port from the public alias
- Deleting an allocation an app uses is refused on every path, including bulk delete
- A port pool range per node that supplies ports for new realms
- A network page per node and per app, with every change audited

**Acceptance**

- [ ] Adding a port another program holds is refused with the port named
- [ ] Moving gameserver to a free port rebinds it without a restart, and a port that fails to bind leaves the old listener serving
- [ ] Bulk-deleting a selection that includes an assigned allocation deletes nothing and names the assigned one
- [ ] An IPv6 bind address is saved and used
- [ ] A range of 1000 ports is accepted and one of 1001 is refused, with every added port proven free

## 17.30 Database hosts and credential rotation

**Goal:** Operators register database servers, give each app a least-privilege user, and rotate credentials without stopping a server.

**Size:** M. **Depends on:** 17.08, 17.47, 17.48, 2.08, 4.16

**Deliverables**

- A database host registry in the supervisor's store with host, port, administrative user, sealed password, TLS mode, node affinity and server version; a connection, version and privilege test runs before a host is saved, and a failed test saves nothing
- Generated users: one runtime user per app with only data access to its databases, and an updater user with schema rights used only by the 2.06 updater, each with a host restriction and a connection limit sized from the app's worker and synchronous threads; identifiers come from an allow list and are quoted
- Rotation with no downtime: on MySQL 8.0.14 and later, add the new password while retaining the old, change the app's connection setting live so its pool opens a new generation and swaps, then discard the old password; on MariaDB, create a second user with the same grants, swap the pool to it, then drop the old user; a failed pool swap keeps the old generation and leaves the old credentials working
- Revealing a stored password needs `database.secrets.read` and a recent 17.47 step-up check, and every test, create, rotation and reveal is audited
- The database page gains the hosts list, each app's user and grants, and a rotate button
- Rotation per server type and whether a realm ever gets its own world database are proposals in doc/PANEL.md

**Acceptance**

- [ ] Registering a host with a wrong password saves nothing and shows the server's error
- [ ] Env-gated (AMBROSE_TEST_DB): rotating the gameserver's credentials while fake clients are connected causes no failed query and no disconnect
- [ ] Env-gated (AMBROSE_TEST_DB): after rotation the old password no longer connects. Run once against MySQL 8 and once against MariaDB; CI runs only the leg its runner provides, so the other is the maintainer's own run
- [ ] The gameserver's runtime user cannot create or drop a table
- [ ] Revealing a stored password without a recent step-up check is refused, and a successful reveal writes an audit row naming the host

## 17.31 Realms and zones pages

**Goal:** Operators see every realm's population, health and zones, and change a realm's limits and flags from the panel.

**Size:** M. **Depends on:** 17.06, 17.26, 17.48, 4.03, 4.09

**Deliverables**

- A realms page listing each `realmlist` row: name with its display name read from the user's install, address and local address, port, flags, population against player limit, last heartbeat, and the gameserver app and node behind it
- Editing a realm's player limit, flags and the default realm through the realm settings, applied from the next realmlist refresh and audited with a reason
- A realm page with population over time, players at character select headed there, the zones the realm has loaded with players per zone, and public instances with their capacity once 12.17 lands
- Zone actions checked and audited: reload a zone's data through 4.15, and move or kick everyone in a zone once 6.06 and 6.05 exist
- Realm and zone changes streamed live as realm events on the 17.26 socket
- Authorization is at the scope of the realm's own gameserver app, as settled under Decisions, Operations; a realm scope of its own waits for the scope tree proposal in doc/PANEL.md

**Acceptance**

- [ ] Starting a second gameserver adds its realm to the page with a fresh heartbeat within one heartbeat interval, and stopping it marks the realm offline
- [ ] Lowering a realm's player limit from the panel applies at the next realmlist refresh without a restart, and the audit row shows who and why
- [ ] A user who holds nothing on a realm's gameserver app gets 404 for that realm, and a user holding `realms.read` on it sees only that realm
- [ ] A zone reload with broken data keeps the previous zone data serving and shows the errors
- [ ] A realm's page draws its population over time and lists the zones the realm has loaded with players per zone, and a zone that unloads leaves the list within one refresh
- [ ] Changing a realm's flags sends one realm event on the 17.26 socket to holders of `realms.read` and nothing to a user who holds nothing on that realm

## 17.32 Realm maintenance mode

**Goal:** Operators close a realm to players for maintenance while game masters can still enter to check it.

**Size:** S. **Depends on:** 17.15, 17.31, 4.05, 6.01, 6.05

**Deliverables**

- A maintenance state per realm with who, why and when, set from the realm page or a schedule task, with an optional countdown through 17.15, and badged on the overview and the realms page; a node in maintenance putting the realms placed on it into this state is 17.22, which owns node maintenance
- Entering maintenance warns in-world players through 6.01 and then removes them through the 6.05 kick path, and sets the realm list flag so the login server stops sending players there
- `Realm.MaintenanceBypassLevel`, a live setting, lets accounts at or above that security level enter a realm in maintenance; its default is a proposal in doc/PANEL.md
- Leaving maintenance clears the flag and is audited

**Acceptance**

- [ ] With a realm in maintenance, a player account selecting a character there is refused and a game master account enters
- [ ] Entering maintenance with a 1 minute countdown warns in-world players and removes them when it ends
- [ ] Lowering `Realm.MaintenanceBypassLevel` lets a moderator account in on the next attempt without a restart
- [ ] Leaving maintenance clears the realm list flag and writes an audit row with who and why
- [ ] A realm in maintenance is badged on the overview and the realms page with who set it and why, and the badge clears when maintenance ends

## 17.33 Announcements and timed game events

**Goal:** Operators message players now or on a schedule, and run timed events such as a double experience weekend that end by themselves.

**Size:** M. **Depends on:** 17.12, 17.15, 6.01, 6.04

**Deliverables**

- Announcements sent now or scheduled, to everyone, a realm or a zone, through the 6.01 zone broadcast and 6.04 GM system messages; a channel whose milestone has not landed is refused with that milestone named
- Each announcement recorded with its text, scope, channels, who, when and the number of sessions reached
- Timed events: a named group of setting changes with a start and an end, applied through the settings API with the event's name as the reason, recording the values each change replaced and restoring them at the end
- A choice per event of whether its end restores a value that someone changed during the event, shown before the event starts
- An events page with upcoming, running and past events, ending an event early, and a countdown on the overview while one runs
- Every announcement and event step audited, and events built on the 17.15 schedule engine so they survive a supervisor restart

**Acceptance**

- [ ] A double experience event that doubles an experience rate from `Rate.XP.*` for one hour restores the previous value when it ends, and history shows both changes with the event's name
- [ ] Restarting the supervisor during an event still ends it on time
- [ ] An announcement to a realm reaches in-world players on that realm and no other
- [ ] Ending an event early restores its values at once
- [ ] An announcement to a channel whose milestone has not landed is refused naming that milestone, and nothing is recorded as sent

## 17.34 World database edits page

**Goal:** Operators edit world content from the panel with typed forms, see every live edit in the journal, and export edits as SQL updates.

**Size:** M. **Depends on:** 17.13, 17.25, 4.15, 5.01

**Deliverables**

- A world edits page over the content tables the game server loads, starting with those that exist when it lands and gaining spawns, doors, vendors and quests as later phases add them
- Table forms built from a schema the game server publishes with types, bounds and references, so a foreign key is chosen from its table
- Edits sent to the game server, which applies them to the world database, records them in the 4.15 world edit journal with the panel user as author, and reloads the affected stores; a group of edits applies as one change set and reloads together
- A failed reload rolls back the database change and keeps the previous store serving, with every error shown
- A journal view with who, when, source (the panel or a GM command) and statement, and export of chosen entries as a local-only SQL file into `data/sql/custom/db_world`, which the repository sanctions for local SQL; writing into a `pending_db_<name>` tree waits for the pending naming decision that blocks 3.19
- Every edit, rollback and export audited

**Acceptance**

- [ ] Editing a template field from the panel changes it in game after the reload, and the journal names the panel user
- [ ] An edit whose reload fails leaves the database and the running store as they were and shows the errors
- [ ] Exported journal entries apply cleanly to a fresh world database
- [ ] An export writes only under `data/sql/custom/db_world`, and a request naming any other folder is refused
- [ ] A user without `world.edit` can browse rows but gets 403 on a save

## 17.35 Panel settings: general, mail and security

**Goal:** Owners configure the panel itself from the panel, with the same typed, locked and audited settings as the game servers.

**Size:** M. **Depends on:** 17.14, 17.48

**Deliverables**

- Panel settings in the supervisor's store on the 4.16 settings model, with defaults, bounds, audit rows and environment and command-line locks, applied live
- General: panel name, public URL, logo, default locale for 17.66, session idle and absolute lifetimes, and the retention defaults this phase's stores use
- Mail: SMTP host, port, TLS mode (none, STARTTLS, implicit TLS), username, a sealed password never shown back with an explicit clear control, from address and name, used by 17.62's verification mail and 17.67's alerts, and a test that sends only to the signed-in user
- Security: sign-in thresholds, relay timeouts, the default port pool range, and an opt-in captcha from a chosen provider with the operator's own keys, never shipped keys and never an echoed secret, applied only after repeated failures and failing closed with a clear error when the provider is unreachable
- The keys 17.14 and 17.47 own, `Panel.BindIP`, the TLS paths, `Panel.AllowPlainHttpRemote`, `Panel.TrustedProxies` and `Panel.TwoFactorRequired`, are shown here read-only with their layer and their owning milestone, so this page never becomes the place that first enforces them
- A settings page for each group, visible only with `panel.settings`

**Acceptance**

- [ ] The mail test reaches only the signed-in user's address, and a bad SMTP password shows the server's error
- [ ] The saved SMTP password never appears in any response, log or audit row
- [ ] With `AMBROSE_PANEL_TRUSTED_PROXIES` set, the key shows as locked and a live edit is refused naming the layer
- [ ] With the captcha on and its provider unreachable, sign-in after repeated failures is refused with a clear error rather than allowed through
- [ ] A user without `panel.settings` gets 403 on every group and sees no page in navigation

## 17.36 Personal API keys

**Goal:** Operators automate the panel with scoped, expiring keys that can never do more than their owner.

**Size:** M. **Depends on:** 17.48, 17.25

**Deliverables**

- Keys in the form `amb_` plus a 12-character public id plus a 32-byte secret in base32, with only a hash of the secret stored, compared in constant time, and the full key shown once in a dialog that cannot be dismissed from outside
- Each key's name, permission subset, scopes, allowed CIDRs for IPv4 and IPv6, expiry (90 days by default, with no expiry only when owner policy allows it), last used time and address written at most once a minute, and created and revoked times
- Rights at request time are the key's subset intersected with its owner's current permissions, so demoting the owner shrinks every key at once
- A limit of 25 keys per user by default, counted in a transaction; rotation that creates a replacement with the same scopes and an optional short grace for the old key; revocation that applies to the next request
- Requests from an address outside a key's CIDRs refused with 403 and audited with the key id; rate limits keyed by key and user; keys refused on cookie-authenticated requests and never accepted by an app's own admin API
- An API keys tab on the account page, and a page for `apikeys.manage` to review and revoke other users' keys

**Acceptance**

- [ ] A key's secret is shown once and cannot be read again from any response
- [ ] Demoting a key's owner to viewer makes the key's restart request answer 403 at once
- [ ] A key used from an address outside its CIDR list is refused and the attempt is audited with the key id
- [ ] An expired key answers 401, and a rotated key's old secret stops working when its grace ends
- [ ] A key presented to an app's own admin API is refused, and a key sent with a session cookie is refused as well

## 17.37 Invites and the grant editor

**Goal:** Owners and admins bring new operators in with one-time links and grant exactly the rights they hold, never more.

**Size:** M. **Depends on:** 17.48, 17.25

**Deliverables**

- Invites carrying a scope, a role and extra grants, stored as a SHA-256 of their token, expiring after 72 hours by default, working once, listable and revocable, rate limited per inviter and per scope
- An invite page where the invitee chooses a username and password and enrolls two-factor sign-in when policy requires it; inviting an existing panel user by username adds the grant at once and notifies them in the panel, and no response says whether a username or email exists
- A unique grant per user and scope with a version, updated through add and remove lists with `If-Match`
- The no-escalation rule: a caller adds or removes only permissions they hold at that scope, permissions they lack stay on the target untouched, a caller cannot edit or remove a user whose permissions at that scope are not a strict subset of their own, and nobody edits their own grants
- A grant editor rendered from the 17.48 permission catalog by group, with descriptions and danger badges, keys the editor cannot grant disabled with the reason, each group's select-all limited to grantable keys, and a review step
- Every invite, acceptance and grant change audited with old and new sets, and applied to open sessions within one second

**Acceptance**

- [ ] An invite link works once, and a second use or a use after expiry is refused
- [ ] An operator holding `power.restart` but not `power.kill` can grant the first and not the second, and saving a user who already holds `power.kill` leaves it in place
- [ ] An operator cannot remove an admin's grants or edit their own
- [ ] Two invites for the same user and scope sent at once create one grant
- [ ] A grant change reaches the target's open session within one second, and a stale `If-Match` answers 409

## 17.38 Account page: profile, security, sessions and game account link

**Goal:** Each operator manages their own profile, password, two-factor sign-in, sessions and game account link in one place.

**Size:** M. **Depends on:** 17.47, 17.35

**Deliverables**

- Profile: display name, email, locale and theme; changing email needs the password and a fresh second-factor check, is limited per day, and with SMTP configured confirms the new address by link and notifies the old one
- Security: password change with the current password, two-factor setup with the secret grouped for typing and a QR code rendered in the browser by a renderer bundled with the dashboard, never fetched from another host; the renderer choice is a proposal in doc/PANEL.md. A pending secret is kept apart from the active one, recovery codes are shown once with copy and download, with a count of those left, regeneration needs the password and a code, and disabling needs the password and a code
- Sessions: each session's device, address, first and last seen, with sign out per session and everywhere
- Self-service password reset by email when SMTP is configured, answering the same whether or not the account exists, rate limited per address and per user, with the token in the request body, and never signing in past two-factor sign-in
- Linking a game account with proof of ownership: that account's password, or a one-time code typed in game once 6.04 exists; the linked account's security level then caps console commands under the command level proposal in doc/PANEL.md, and unlinking is audited
- Every change audited, and a password or two-factor change ending the user's other sessions

**Acceptance**

- [ ] Changing the password ends every other session of that user and keeps the current one
- [ ] The sessions list shows a second browser's session, and signing it out ends it within one second
- [ ] Linking a game account with a wrong password is refused and audited
- [ ] After linking a game master account, a console command above game master level answers as unknown
- [ ] A reset request for an unknown email answers exactly as one for a known email
- [ ] The enrollment page renders its QR code with no request to another host, and the page's CSP would block one

## 17.39 File manager archives, search and log follow

**Goal:** Operators archive and extract files safely, find files by name, and follow growing logs from the file manager.

**Size:** M. **Depends on:** 17.18, 17.54, 17.26

**Deliverables**

- Archive creation of chosen files and folders to zip or tar.zst, named with a UTC time without colons, as a cancellable background job with progress on the 17.26 socket, leaving out protected paths, the trash, the version store and every root marked client-derived
- Download of folders and selections as an archive streamed on the fly, in the default format the maintainer chooses from the proposals in doc/PANEL.md
- Safe extraction as a background job: entry names normalized and resolved through the jail; link, device and FIFO entries refused unless a link stays inside its root; caps on each entry's declared size, total size, entry count, depth and compression ratio; modes masked to 0644 and 0755; names that collide by case or are reserved on Windows refused; extraction into staging on the same volume, then moved into place with conflicts listed or resolved as the operator chose; and a report of written, skipped and refused entries
- Name search within a root, bounded by entry count and time, with results paged and the bound reported when it stops early
- Following a growing log file live across rotation and truncation, with pause, and an app's own log offered through its structured stream instead
- Every archive, extraction and search audited, and archive jobs counted against the 17.14 rate limit

**Acceptance**

- [ ] An archive holding `../evil.conf`, an absolute path, a symbolic link out of the root and a device entry extracts none of those and names each in the report
- [ ] An archive that inflates past the total cap stops at the cap, removes its staging folder and changes nothing in the destination
- [ ] Cancelling an archive job removes its partial file
- [ ] Following a log that rotates keeps showing new lines from the new file
- [ ] A search that hits its entry or time bound says so in the response instead of reporting a complete result
- [ ] An archive request that includes a client-derived root is refused naming that root

## 17.40 Opt-in SFTP access

**Goal:** Operators who want a desktop file client can reach the same roots over SFTP, with the same permissions and without bypassing two-factor sign-in.

**Size:** M. **Depends on:** 17.54, 17.38

**Deliverables**

- An SFTP service in the supervisor, off by default behind `Panel.Sftp.Enable`, documented with its risk under Decisions, Experimental features; whether it is built at all and the SSH library are proposals in doc/PANEL.md
- An Ed25519 host key generated at first start with its fingerprint shown in the panel, modern key exchange, ciphers and MACs only, the SFTP subsystem only, and at most 6 authentication attempts
- Authentication by SSH keys registered on the account page, validated before parsing (allowed key type prefixes, at most 16384 bytes, no NUL, no DSA, RSA of at least 2048 bits, fingerprint unique per user), or by keyboard-interactive password plus TOTP for users with two-factor sign-in; rate limited per user and per address
- The same roots, jail, protected paths, space guard and `files.*` grants as HTTP, with `files.sftp` required to connect; the config root read-only over SFTP; client-derived roots not offered at all; append and resume honored; writes refused during protected states; sessions ended when the user's permissions change
- SFTP activity merged per user, root and minute into the audit log

**Acceptance**

- [ ] With the service off, nothing listens on its port
- [ ] A user with two-factor sign-in cannot connect with the password alone
- [ ] A user without `files.delete` cannot remove a file over SFTP, and the attempt is audited
- [ ] Revoking a user's `files.sftp` ends their open SFTP session within one second
- [ ] Dev-gated: a desktop SFTP client lists a root, uploads a file and resumes an interrupted upload. Needs a third-party SFTP client, so it is run by hand and recorded

## 17.41 Opt-in remote file pull

**Goal:** Operators fetch a file from a URL straight into a root, without the server being usable to reach internal addresses.

**Size:** M. **Depends on:** 17.54

**Deliverables**

- `POST /api/panel/files/pull`, off by default behind `Panel.Files.AllowRemotePull` and requiring `files.pull`, documented with its risk under Decisions, Experimental features, including that it must not be pointed at KingsIsle's servers without the terms risk stated and must not mirror client data
- The address each connection actually reaches, and each redirect hop's, checked against loopback, private, link-local, carrier-grade NAT, benchmark, multicast, unspecified and the host's own addresses, with an allow list for hosts such as a LAN mirror
- HTTPS by default, the size limit enforced while streaming so chunked responses work, an optional expected SHA-256, the file name taken as a base name only and passed through the jail and protected paths
- At most 3 concurrent pulls per node and a rate limit per user through the 17.14 limiter, progress on the event socket, cancel, and partial files removed on failure or cancel
- Every pull audited with its URL and resulting hash

**Acceptance**

- [ ] A URL resolving to 127.0.0.1, 10.0.0.1, 169.254.169.254 or ::1 is refused, including through a redirect or a DNS answer that changes between checks
- [ ] A download larger than the limit stops at the limit and leaves no partial file
- [ ] With the setting off, the route answers 404
- [ ] A pull whose expected SHA-256 does not match is discarded with nothing written into the root
- [ ] A fourth concurrent pull on one node is refused with the limit named

## 17.42 Moving an app or realm between nodes

**Goal:** Operators move an app or a realm to another machine from the panel, with players warned and nothing left half moved.

**Size:** M. **Depends on:** 17.22, 17.27, 17.51

**Deliverables**

- A move record with source and target nodes, reserved target allocations, state (pending, draining, archiving, streaming, verifying, starting, completed, failed, cancelled), who, times and error, running as the protected moving state
- Refusal of a move to the same node, to a node in maintenance, or to a node lacking the required build, supervisor protocol version or client revision
- The move flow: a countdown to players, realm maintenance from 17.32 when that milestone has landed, a final backup of config, data and logs, stop, stream to the target over the node link with SHA-256 verification and resume, start on the target, wait for health, update the realm list address and port, and clear maintenance
- Client data rebuilt on the target from that machine's own install, never shipped between machines
- Cancel from the panel and a stall timeout, each releasing the reserved allocations and restarting the app on the source; only the target node can report success
- Move progress on the event socket and every step audited

**Acceptance**

- [ ] Dev-gated: moving a gameserver to a second machine keeps its characters, and players can sign in to the realm at its new address. Needs the maintainer's own second machine or a virtual machine
- [ ] Dev-gated: cutting the link mid-stream fails the move after the stall timeout, releases the target's allocations, and brings the app back on the source. Needs the maintainer's own second machine or a virtual machine
- [ ] A move report of success from the source node is refused
- [ ] A move to a node lacking the build or one protocol version behind is refused naming what is missing
- [ ] No client file crosses the node link: the stream's manifest lists only config, data and logs, and the target rebuilds client data itself

## 17.43 S3-compatible backup storage and catalog repair

**Goal:** Operators keep backups off the machine in S3-compatible storage, uploads survive failures, and the backup list can be rebuilt from storage.

**Size:** M. **Depends on:** 17.16

**Deliverables**

- Storage targets with endpoint, region, bucket, prefix applied to object keys, path style, storage class, part size and concurrency, and credentials from config, environment or a sealed store value, never shown back; a connection test that writes, reads and deletes a probe object
- Endpoints checked against loopback, link-local, metadata and carrier-grade NAT addresses, with an allow list for a LAN server
- Multipart uploads with each part's ETag and checksum saved as it finishes, resumed after a failure or restart by reconciling with the parts the store lists, retried with backoff and jitter, aborted on cancel or delete, and a start-up sweep that aborts stale uploads under the prefix
- A manifest sidecar per archive, an optional local copy with its own retention, and restores that stream from storage with resumable ranged reads
- A scan that lists the local backup folder and the storage prefix, matches entries by uuid, imports unknown archives after verifying their manifests, and offers to delete orphans
- The upload page states plainly that the archive holds the type dump and data built from the operator's own client install, as the Client-derived data review note requires, and that the bucket must be storage the operator controls; 17.72's sealing is offered before the first upload
- The S3 client is a proposal in doc/PANEL.md and is the maintainer's decision

**Acceptance**

- [ ] Killing the supervisor midway through a multipart upload resumes it at the next start without re-sending finished parts
- [ ] Deleting the supervisor's store and running the scan lists every backup in the bucket again, verified
- [ ] A target whose endpoint resolves to a link-local address is refused
- [ ] Changing the default target does not affect restoring a backup stored on the previous one
- [ ] The first upload to a new target shows the contents statement and records the operator's acknowledgement in the audit log

## 17.44 Player-aware schedule conditions and rolling restarts

**Goal:** Schedules take players into account: they skip empty realms, wait for a quiet moment, and restart realms one at a time so the game is never fully down.

**Size:** S. **Depends on:** 17.15, 17.31

**Deliverables**

- The condition `skip_if_empty`, which skips a run when no players are online in its scope, and `only_when_empty`, which waits for a moment with no players up to a set limit and then runs or skips as chosen, both recorded with their skip reasons
- Conditions checked at run start and again before any task that sets its own guard, using players online and sessions at character select
- A rolling restart action that restarts a set of realms one at a time, waiting for each to report healthy before the next, and stops with the remaining realms untouched when one fails

**Acceptance**

- [ ] A schedule marked `only_when_empty` does not run while a player is online and does run once no one is
- [ ] A schedule marked `skip_if_empty` records a skipped run with the reason when no players are online
- [ ] A rolling restart of two realms never has both offline at the same moment, and a failed first realm leaves the second untouched
- [ ] A player who signs in while a run waits for a quiet moment leaves the guarded task skipping with its reason on the run record, and the condition is checked again before that task rather than only at run start

## 17.45 Opt-in security keys and passkeys

**Goal:** Operators sign in with a hardware security key or a passkey, as a second factor or without a password.

**Size:** M. **Depends on:** 17.38

**Deliverables**

- WebAuthn registration and authentication in the supervisor, off by default behind `Panel.WebAuthn.Enable`, available only in a secure context, which the 17.14 listener's TLS rule already makes the normal case
- `panel_webauthn_credential` rows with credential id, public key, signature counter, transports, name, and created and last used times, managed on the account page
- Security keys as a second factor alongside TOTP, and passkeys as passwordless sign-in when the owner allows it, with the same throttles, session generation and audit as other sign-ins
- A signature counter that does not advance refuses the sign-in and raises an alert, and removing the last second factor while two-factor sign-in is required is refused
- The WebAuthn implementation is a proposal in doc/PANEL.md and is the maintainer's decision

**Acceptance**

- [ ] Dev-gated: a registered security key completes sign-in as the second factor, and a removed key no longer does. Needs the maintainer's own security key, so it is run by hand and recorded
- [ ] Dev-gated: with the owner allowing it, a registered passkey signs in with no password, and the audit row records the sign-in as passwordless. Needs the maintainer's own security key or platform authenticator, so it is run by hand and recorded
- [ ] A replayed authentication response is refused and audited
- [ ] Over plain HTTP from another machine, security key options are not offered
- [ ] Removing the last second factor while `Panel.TwoFactorRequired` covers that user is refused with the reason named

## 17.46 Panel users, password policy and sign-in

**Goal:** Every operator has their own panel account, and signing in tells an attacker nothing and costs them a throttle.

**Size:** M. **Depends on:** 17.14, 2.13

**Deliverables**

- Panel users in the supervisor's SQLite store, separate from game accounts, with usernames following the game account rules from 2.13, one password policy on every path that sets a password, passwords hashed by Argon2id from libsodium as settled under Decisions, Operations, a disabled flag and a must-change flag
- Sign-in that checks the password before any second factor, spends one Argon2id verify on unknown and disabled users so timing matches, regenerates the session id on success, and never records the typed username or password on failure
- Failure throttles per user and per address over the 17.14 client address, counting only failures, with each account's address count kept separately so a success against one account does not clear another's
- On first start the supervisor creates an owner account and writes a one-time sign-in link to its console and log, valid for minutes and only from the same machine, so no default password ever exists
- Supervisor console commands `panel user create`, `panel user list`, `panel user reset-password` and `panel user disable`, with arguments kept out of logs and audit rows
- One-time password reset links an operator can issue, single use, expiring, and ending the user's other sessions when used; a password or disable change bumps the 17.14 session generation

**Acceptance**

- [ ] A fresh supervisor has no default password and prints a one-time owner link that works once, only from localhost
- [ ] Twenty failed sign-ins for one user within a minute are throttled, and the correct password works again once the window passes
- [ ] A successful sign-in to one account from an address does not reset that address's failure count against another account
- [ ] Sign-in against an unknown user, a disabled user and a wrong password answer the same message and take the same time within measurement noise
- [ ] A password change ends that user's other sessions within one second
- [ ] `panel user create` leaves the password in no console line, log file or audit row
- [ ] A password that fails the policy is refused identically from the console, the reset link and the users page

## 17.47 Two-factor sign-in, recovery codes and required enrollment

**Goal:** Two-factor sign-in cannot be replayed or skipped, recovery codes work once, and an owner can require it.

**Size:** M. **Depends on:** 17.46

**Deliverables**

- TOTP with a window of one step either side by default, refusing any time step at or below the last one accepted, on sign-in, on enabling and on step-up checks
- Ten recovery codes per user, stored as keyed hashes under a supervisor key so one can be found by lookup instead of a loop, each usable once, shown once with a count of those left; which library provides that keyed hash is the crypto stack proposal listed under Decisions needed in doc/ROADMAP.md, which names this milestone, and where the key lives is the keyring proposal there
- Enabling needs the password and a current code; disabling needs the password and a current code and ends the user's other sessions
- `Panel.TwoFactorRequired` (none, danger permission holders, owners and admins, or everyone) enforced on every route and socket from this milestone, with only the sign-in and enrollment routes exempt, and API key requests answering 403 with `two_factor_required`
- Step-up checks with a freshness window as a live setting, required by secret reveals, backup downloads, credential rotation and other danger actions, and recorded in the audit log with what they authorized

**Acceptance**

- [ ] With two-factor on, a correct password without a valid code is refused, a code already accepted once is refused the second time, and each recovery code works once
- [ ] Setting two-factor required for everyone sends a user without it to enrollment on their next request, and their API key requests answer 403 with `two_factor_required`
- [ ] A danger action attempted with a second factor older than the freshness window asks again and changes nothing
- [ ] Disabling two-factor without a current code is refused, and disabling it with one ends that user's other sessions
- [ ] A recovery code appears in no response, no log and no audit row after the dialog that issued it, and the store holds only its keyed hash

## 17.48 Permission catalog, roles and grants

**Goal:** One authorization decision covers every route and socket, and a route cannot ship without declaring what it needs.

**Size:** M. **Depends on:** 17.05, 17.46

**Deliverables**

- The permission catalog from doc/PANEL.md as one C++ table of groups, keys, descriptions, danger flags and the scopes each key may be granted at, served at `GET /api/panel/permissions`
- Roles owner, admin, operator, game master and viewer as permission bundles, plus per-app sub-user grants of single permissions such as `console.read`, `console.write`, `power.restart`, `files.write`, `backups.restore`, `schedules.edit`, `settings.edit` and `accounts.ban`, which is the arrangement settled under Decisions, Operations; the wider node, cluster and realm scopes are a proposal in doc/PANEL.md and no route or check here depends on them
- One `AuthorizationMgr` that decides every check in order: authenticate, resolve the scope with 404 when the caller holds nothing there, confirm every object in the path belongs to that scope, check the permission with 403 and an audit row for a refused danger permission, then check state with 409
- Route registration that fails when a route declares neither a permission nor that it is open to any member, and a test generated from the route registry that checks every route answers 403 without its permission and 404 for a scope the caller cannot see
- Response shaping from the catalog: secret settings masked without `settings.secrets.read`, account email, address and MachineID hidden without `accounts.pii.read`, and no verifier, session key hash, two-factor secret or key secret in any response whatever the caller holds
- The last owner can never be deleted, disabled or demoted, and nobody changes their own role or grants; a role or grant change bumps the 17.14 session generation of every affected user

**Acceptance**

- [ ] A viewer can read an app's status but gets 403 on a command through 17.05 and on a restart through 17.08, and the dashboard hides both controls
- [ ] A sub-user granted only `power.restart` on gameserver can restart gameserver and nothing else, and gets 404 for an app they hold nothing on
- [ ] A route registered without a permission fails the route registry test, and so does a route naming a key the catalog does not hold
- [ ] `GET /api/panel/permissions` lists every group, key, description, danger flag and scope the routes reference
- [ ] Demoting or deleting the last owner is refused
- [ ] A caller without `settings.secrets.read` receives the masked value, and the same route with the permission returns the value and writes an audit row
- [ ] A grant change ends nothing for other users but bumps the affected user's session generation within one second

## 17.49 Panel audit scope, app relay and command history

**Goal:** Nothing the panel does happens without a record, and the browser never holds an app's token.

**Size:** M. **Depends on:** 17.05, 17.48

**Deliverables**

- An `AuditScope` that records the panel user, address, subjects, result, reason and error of every action into the 17.14 audit tables, in the same transaction as the change it records, refused attempts included, so a change cannot outlive its record and no milestone keeps a log of its own
- The supervisor relays each app's admin API behind the signed-in user's permissions, holding the per-app tokens itself and passing the command security level the user's grants allow; the browser holds no app token, and the 17.02 tokens stay for scripts. The level cap is the command level proposal in doc/PANEL.md
- One stored command history, per panel user and app, in the supervisor's store, capped per user, with sensitive arguments redacted, which replaces the in-page recall 17.07 keeps for the open page and is the only history anything stores
- A relay timeout and an error that distinguishes an app that is stopped, one that refuses the token and one that timed out, each recorded
- Refusals recorded with their reason: above the caller's level, missing confirmation, wrong scope, protected state

**Acceptance**

- [ ] A restart run through the panel appears once in the audit log with the panel user, the client address and the app as subject
- [ ] A command refused for being above the caller's level writes a refused audit row and runs nothing
- [ ] A forced failure of the audit insert leaves the change unapplied, since both are one transaction
- [ ] No response body or browser storage holds an app token, and a request that tries to pass one is ignored by the relay
- [ ] A command run in one browser appears in that user's history in another browser, with a password argument redacted
- [ ] A relay call to a stopped app and one to an app refusing its token give different errors, both recorded

## 17.50 Panel users, roles and grants pages

**Goal:** Owners and admins manage operators from the panel: who exists, what they hold and what they did.

**Size:** M. **Depends on:** 17.48, 17.49

**Deliverables**

- A users page for creating users, assigning roles and per-app grants, disabling, resetting two-factor, issuing a one-time password reset link, and reading each user's audit trail
- A detail view per user: last sign-in time and address, open sessions, two-factor state, grants by app, and API keys once 17.36 lands
- Roles kept as rows built from the 17.48 catalog rather than bundles compiled into the binary, with a roles page where an owner adds or edits a custom role and the five shipped roles are rows like any other; a role change builds a new resolution table, swaps it in and bumps the session generation of every affected user, and the owner-only set stays owner-only whatever a role names
- Every action needs its `users.*` permission, applies the 17.37 no-escalation rule when that milestone lands, and is audited with both users named

**Acceptance**

- [ ] Creating a user from the page lets them sign in, and the page then shows their first sign-in time and address
- [ ] A user with `users.read` but not `users.update` sees the page and gets 403 on every change
- [ ] Resetting another user's two-factor ends that user's sessions and writes an audit row naming both users
- [ ] Disabling a user ends their open sessions within one second while another user's sessions stay open
- [ ] The audit trail on a user's page shows the actions they ran and the actions others ran on them
- [ ] A custom role built from the catalog grants exactly the keys it names, editing it changes what its holders may do within one second without a sign-out, and a role cannot be given a key its editor does not hold

## 17.51 Backup restore with staging swap and rollback

**Goal:** A restore either brings the installation back exactly as the dump recorded it or leaves everything as it was.

**Size:** L. **Depends on:** 17.16, 17.27, 2.06

**Deliverables**

- Restore of the whole archive or of chosen components: verify every checksum and the update level before changing anything, take a safety backup, load databases into staging schemas and run the 2.06 updater there, warn and drain players, stop only the affected apps, swap tables atomically and folders into place, raise `id_sequences` so no guid is reused, clear `account_session` and the online flags, then start the apps again
- Automatic rollback when a step after the swap fails, and a refusal when the archive's applied-update list is not a prefix of the running build's
- A persisted restoring state through 17.27 that refuses power actions, settings edits, reloads, file writes, schedule runs and updates while keeping read-only pages and live progress available
- A restore wizard confirmed by typing the installation name, with a component picker; restoring login and characters needs `backups.restore.players`
- A restore report that compares the restored databases with the 17.16 snapshot record, table by table, listing every row count and checksum that differs
- Every step streamed and audited; an interrupted restore is reported at the next start with its safety backup named, and its staging schemas are cleaned up

**Acceptance**

- [ ] A restore's report shows every table's row count and checksum equal to the 17.16 snapshot, and a table altered on purpose before the comparison is listed as different
- [ ] A backup archive with one flipped byte is refused at restore with the failing file named, and nothing is changed
- [ ] Restoring a backup from before a dated SQL update brings the database back and then applies the update again
- [ ] A backup made by a newer build whose update list the running build lacks is refused at restore
- [ ] A restore interrupted by killing the supervisor leaves the safety backup in place, and the next start reports the incomplete restore and removes the staging schemas
- [ ] A restart requested during a restore is refused with 409 naming the restore
- [ ] A new wizard created after a restore gets a guid no earlier wizard ever had, and no character is left locked by a stale online flag

## 17.52 Backup retention, pins and audited downloads

**Goal:** Old backups go away on a rule, the ones that matter cannot, and a download needs a fresh check and leaves a record.

**Size:** S. **Depends on:** 17.16, 17.47

**Deliverables**

- Retention by count and age, applied only after a new backup has been verified, with pinned backups retention never removes and system pins on the newest verified backup and on a running restore's safety backup
- Download through a one-time ticket that needs a recent 17.47 step-up check, bound to the user and the backup, expiring in minutes, refused for a backup that has not succeeded
- Ranged downloads so a large archive resumes, with the whole archive's SHA-256 shown next to the link
- Delete and pin with their permissions, a refusal for a pinned or in-use backup, and every pin, delete, ticket and download audited with the backup named

**Acceptance**

- [ ] Retention set to keep 3 removes the oldest unpinned backup when a fourth completes, and removes nothing when the fourth fails
- [ ] A download ticket works once, is refused after its expiry, and a second user holding the same link gets 403
- [ ] A download of a failed backup is refused, and a download without a recent step-up check asks for one
- [ ] A ranged download resumed after a break has the same SHA-256 as the whole archive
- [ ] Deleting a pinned backup is refused with the pin named, and every download writes an audit row

## 17.53 File editor with validated saves and version history

**Goal:** Operators edit a config or SQL file in the browser without two of them overwriting each other and without saving a file the server would refuse.

**Size:** M. **Depends on:** 17.18

**Deliverables**

- A text editor with syntax highlighting for `.conf`, SQL, JSON, XML and Lua, from the editor library the maintainer chooses from the proposals in doc/PANEL.md
- `.conf` validation against the 17.12 settings schema before saving, a warning when a key is shadowed by a live setting or locked by a layer, and an offer to run `reload config` afterwards
- Saves that require the ETag from the last read, write a temporary file and rename it over the target, and answer 409 with the current ETag and a diff when the file changed underneath
- A version store per root keeping previous versions with who and when, with view, diff and restore; its location and retention are a live setting and a proposal in doc/PANEL.md
- Every save audited with the file's hash before and after, and secret values still redacted in the editor without `settings.secrets.read`

**Acceptance**

- [ ] Saving a `.conf` file with an out-of-range value is refused with the setting and bound named, and the file is unchanged
- [ ] Two operators saving the same file at once get a conflict with a diff for the second save, instead of a silent overwrite
- [ ] A previous version restores with its content and hash unchanged, and the restore is audited
- [ ] A save that fails midway leaves the original file intact and no temporary file behind
- [ ] A viewer gets 403 on save, and the audit row of a successful save carries the hash before and after

## 17.54 File uploads, downloads and batch operations

**Goal:** Operators move files in and out of the roots, and a refused operation changes nothing.

**Size:** M. **Depends on:** 17.18

**Deliverables**

- Upload through single-use signed links with size limits enforced while streaming, no overwrite unless replace is chosen, and a reservation taken from the 17.18 space guard before the first byte
- Download of a file with Range support and as an attachment with a safe name, refused for read-only-download and client-derived roots
- Rename, move and copy within and between roots, each checked against both roots' policies and protected paths
- Batches validated completely before any change, then applied in order, stopping at the first failure and reporting what was done and what was not
- Every operation permission checked and audited with its paths, and writes audited with the hash before and after

**Acceptance**

- [ ] An upload larger than the limit is refused before the limit's worth of bytes plus one reaches the disk, and leaves no partial file
- [ ] A viewer can read and download but cannot upload, move or delete
- [ ] A batch with one refused entry changes nothing and names the refused entry
- [ ] A ranged download of a large log returns the same bytes as the whole file
- [ ] A move from the config root into the read-only install root is refused naming the target root's policy

## 17.55 Trash, purge, new files and folders, and permission toggles

**Goal:** A deleted file comes back, a purge really removes it, and operators create and protect files without a shell.

**Size:** M. **Depends on:** 17.54

**Deliverables**

- Delete to a recoverable trash per root recording who, when and the original path, restore to that path, and a retention that is a live setting
- Permanent delete from the trash behind `files.purge`, and a purge-all that reports how many entries and bytes it removed
- Create a folder, including a nested path made in one call, and create an empty file, both through the 17.18 jail and refused for protected paths
- Read-only and executable toggles behind `files.permissions`: on Linux the mode is masked to 0644 and 0755 and on Windows the read-only attribute is set, never a free octal mode, each toggle audited with the old and new state
- The trash and the version store left out of listings, archives, searches and free space figures, and both inside the root they belong to

**Acceptance**

- [ ] A deleted file restores from the trash to its original path with its content and hash unchanged
- [ ] A purge with `files.purge` removes the entry for good and reports the count and bytes; without the permission it is refused and audited
- [ ] Creating `a/b/c` in one call creates all three levels, and the same call for a protected path is refused with nothing created
- [ ] Marking a file read-only makes the next save fail with a clear error, clearing it allows the save, and both toggles are audited
- [ ] Trash entries appear in no listing, archive or search result, and a trash entry past its retention is gone after the sweep

## 17.56 Operator-defined file roots

**Goal:** An operator can add a root of their own, such as a mirror folder or a scratch area, without loosening the jail.

**Size:** S. **Depends on:** 17.18

**Deliverables**

- Extra roots defined by an owner: a name, an absolute path, a policy of list, read, write, download and archive, and a client-derived flag, stored in the supervisor's store
- A root refused when its path holds the supervisor's store, keyring, token files or TLS keys, when it overlaps an existing root, when the path is a link, or when the service user cannot open it
- Grants over an extra root use the same `files.*` keys at app scope; node-scoped roots follow 17.22, and the wider scopes wait for the scope tree proposal in doc/PANEL.md
- Adding, changing and removing a root is audited, open sessions re-resolve their roots within a second, and removing a root ends its open operations

**Acceptance**

- [ ] A root whose path contains the supervisor's store or keyring is refused with the reason named
- [ ] A root added as list and read only refuses every write and upload, and its files still list
- [ ] A root marked client-derived refuses download and archive for every caller
- [ ] Removing a root makes its paths answer 404 within a second and writes an audit row

## 17.57 Socket subscriptions and live permission filtering

**Goal:** A page sees exactly the streams its user may see, and a permission taken away stops the stream at once.

**Size:** M. **Depends on:** 17.26, 17.48

**Deliverables**

- Subscriptions per scope and stream over the 17.26 envelope, each checked against the user's current permissions on every message rather than only at subscribe time
- Re-filtering when a user's role, grants or session generation change, closing a subscription with 4403 when nothing permitted is left and the whole socket with 4401 when the session ends; revocation closes only that user's sockets
- A `session.expiring` message a set time before a session's idle or absolute lifetime ends, so a page can prompt rather than fail mid-action
- Scope resolution shared with 17.48's `AuthorizationMgr`, so a subscription to an app the caller cannot see answers not found exactly as the route does
- Every close code and its meaning documented with the socket types in doc/PANEL.md, which the protocol proposal covers

**Acceptance**

- [ ] Removing a user's `console.read` stops their log stream within one second while their status stream continues, and another user's sockets are untouched
- [ ] Ending a session closes only that user's sockets, with 4401, and the page signs in again without losing its place
- [ ] A subscription to an app the caller holds nothing on answers not found, never an empty stream
- [ ] A `session.expiring` message arrives before the idle lifetime ends, and the page's refresh keeps the socket open
- [ ] A grant added while a socket is open makes the new stream available without reconnecting

## 17.58 Socket limits, fan-out and one live page pipeline

**Goal:** One socket carries every live page under limits that cannot be used to overload the supervisor or an app.

**Size:** M. **Depends on:** 17.57, 17.07, 17.13

**Deliverables**

- Limits per connection and per user for commands, power actions, backlog and other messages, a 64 KiB frame cap, ordered handling of each connection's messages, a reply of `throttled` or `error` to every refusal, and pings every 20 seconds with idle sockets closed after 60
- The supervisor subscribes once to each app's `/api/logs` and `/api/events` and fans out to panel sockets, so ten browsers on one app cost the app one subscriber
- The 17.07 log and console pages, the 17.13 settings and reload pages and the 17.06 overview move onto this socket, and the dashboard keeps one socket client with one reconnect policy
- `console.raw`, owner-only by default, writing one line to an app's standard input when its admin API is down, audited like any command and refused while the app is in a protected state
- A per-app fan-out buffer that drops the oldest records with a marker, so one slow browser cannot slow the app or another browser

**Acceptance**

- [ ] Twenty commands sent in one burst reach the app in order, and those beyond the limit are answered with `throttled`
- [ ] A frame over 64 KiB closes the connection with its close code, and nothing is processed from it
- [ ] Ten browsers following one app's logs leave the app with one `/api/logs` subscriber
- [ ] One browser that stops reading gets a dropped marker while the others keep receiving every record
- [ ] The dashboard opens exactly one socket with every live page loaded, proven by a browser test
- [ ] `console.raw` from a non-owner is refused and audited, and a raw line during a restore is refused naming the restore

## 17.59 Pre-start checks and restart confirmation

**Goal:** A start says why it cannot work before it fails, and a restart shows the operator who is about to be interrupted.

**Size:** M. **Depends on:** 17.27, 3.22

**Deliverables**

- Pre-start checks streamed as supervisor records: the binary and its `--check`, config validity, database reachability and pending schema updates, client install and type dump (running 3.22's setup as the setup state when one is missing), free ports and free disk space
- A failed check that ends the operation with the reason in its result rather than a silent accepted response, and a check list on the app's page with each item's last result
- A restart choice of now or with a countdown, whose confirmation shows players in world, sessions at character select and active patch downloads
- A note in the confirmation when a live setting or a reload would avoid the restart, naming the setting or target
- Every check result and every confirmed restart recorded with what the operator was shown

**Acceptance**

- [ ] A start whose type dump check fails reports failure with the reason in the operation result, and the app stays offline
- [ ] A start with a missing type dump runs 3.22's setup as the setup state and then continues, with progress streamed
- [ ] A restart confirmation shows the number of players in world and sessions at character select at that moment
- [ ] A restart to apply a value that is a live setting shows the note naming that setting
- [ ] A start with a port already held is refused before the process is launched, with the port named

## 17.60 Exit classification, backoff and crash records

**Goal:** Every exit is classified, a crash loop stops restarting, and each crash leaves enough to diagnose it.

**Size:** M. **Depends on:** 17.27, 17.28

**Deliverables**

- Exit classification: requested, clean but unexpected, crash with the Windows exception or POSIX signal named, out of memory, and startup failure, taken from the exit code, the signal and the app's own last lifecycle state
- Exponential backoff from 1 second to 5 minutes, reset after 10 healthy minutes, and a crash loop after a set number of crashes in a window, which stops restarting until a manual start and badges the app
- A crash record per crash with time, exit code or signal, classification, uptime, the last 200 log lines and any minidump path, kept per app with a retention setting
- A crash history on the app's page with each record's detail, and the crash and crash-loop conditions published for 17.67's alert rules
- The supervisor's own restart of a crashed app, from 17.08, now runs through the 17.27 operation model so a crash during a protected state does not fight the operation holding the lock

**Acceptance**

- [ ] Killing gameserver from outside four times within the crash loop window stops restarts, badges the app as crash looping, and keeps four crash records with their log tails
- [ ] A clean exit that nobody requested is classified as clean but unexpected, not as a crash
- [ ] An app killed for exceeding its 17.28 memory limit is classified out of memory
- [ ] Backoff grows from 1 second towards 5 minutes and resets after 10 healthy minutes
- [ ] A crash while a restore holds the app's lock records the crash and does not restart the app until the restore releases it

## 17.61 Reconciling stale online and session rows after a crash

**Goal:** A gameserver crash never leaves a character locked out, and no pass ever clears rows that belong to a live server.

**Size:** M. **Depends on:** 17.60, 4.07, 2.13

**Deliverables**

- A reconciliation pass over the rows 4.07 writes at world entry: `characters.online`, `account.online`, `realm_online_character`, the realm's unused `login_key` rows and `account_session` rows whose realm is gone, run when an app exits without a clean shutdown and at every supervisor start
- A pass scoped to the crashed app's realm, so one realm's crash never clears another realm's players
- Fencing by run generation: the gameserver stamps its generation on the rows it writes, and a pass clears only rows from a generation older than the app's current run, so a restarted app already serving players loses nothing
- A report per pass with the rows cleared per table, the realm, and the 17.60 crash record it followed, shown on the app's page and written to the audit log; a pass that clears rows publishes the condition 17.67 alerts on, since a locked character is player-visible
- The same pass as a console command and a panel button for an app the supervisor did not start, refused while that app is running

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): killing a gameserver with a character in world leaves stale online rows, the pass runs automatically and clears them, and the same character logs in again with its saved position
- [ ] Env-gated (AMBROSE_TEST_DB): a character online on another realm keeps its rows through the pass
- [ ] A late pass against a realm whose gameserver is already serving again clears nothing, because the run generation does not match
- [ ] The pass writes one audit row naming the rows cleared per table and the crash record it followed
- [ ] Running the pass by hand for a running app is refused with the app named

## 17.62 Player account registration and password recovery

**Goal:** Players make their own accounts and recover their own passwords, without an operator typing console commands.

**Size:** M. **Depends on:** 17.35, 2.13

**Deliverables**

- `Panel.Registration.Enable`, off by default: a public sign-up page on the panel listener, outside the operator area, that creates a game account through AccountMgr from 2.13 under 2.13's username and password rules, at the lowest security level, and never a panel user
- Email verification when SMTP is configured in 17.35: the account is created unverified, a single-use link expiring in hours verifies it, and `Login.RequireVerifiedEmail`, a live setting, decides whether an unverified account may sign in to the game
- Player-driven password reset: a request that answers the same whether or not the address is known, a single-use token carried in the request body, and a new password under the same policy that seals the verifier with the active key, deletes `account_session` and kicks live sessions
- Throttles per address, per account and per email domain through the 17.14 limiter, the 17.35 captcha after repeated attempts, and a cap on mails per address per day
- Every registration, verification, reset request and reset audited with the client address, and no response that tells a stranger whether a username or email exists
- An operator page listing recent registrations with verification state, resend and block, behind `accounts.read` and `accounts.registration`
- Whether the panel offers registration at all, and with which requirements, is a proposal listed under Decisions needed in doc/ROADMAP.md

**Acceptance**

- [ ] With the setting off, the registration and reset routes answer 404
- [ ] A reset request for an unknown email answers exactly as one for a known email, and neither reveals whether it exists
- [ ] A reset token works once, is refused after its expiry, and the reset deletes `account_session` and kicks the live session
- [ ] Ten registrations from one address in a minute are throttled, and with a captcha configured the next attempt is asked for one
- [ ] Registration creates a game account at the lowest security level and no panel user
- [ ] The registrations page lists a new sign-up with its verification state, resend and block are audited, and a user holding `accounts.read` without `accounts.registration` gets 403
- [ ] Dev-gated: an account registered and verified through the panel signs in from the retail client. Needs the maintainer's own retail client, as doc/PATCHING.md describes

## 17.63 Moderation queue, chat search and mute history

**Goal:** A reported player reaches a moderator with the evidence attached, and every mute leaves a history.

**Size:** M. **Depends on:** 17.21, 17.25, 12.07

**Deliverables**

- A report queue over 12.07's moderation records: player reports and house reports with reporter, subject, category, text, realm, zone, time and state (new, claimed, actioned, dismissed), claimed by one moderator at a time with the claim visible
- Chat search for a reported player over 12.07's chat records, bounded by time range and result count, with the lines around a match, filtering by channel, and the bound reported when it stops early
- Mute history per account and character with who, why, how long, when it ends, and the kicks and bans the same moderator issued, plus a repeat count per subject
- Actions taken from a report go through 17.21's checked paths for mute, kick and ban, each linked to the report it came from and each requiring a reason
- Queue counts need `players.read`; reporter identity and chat text need the moderation permissions, and every read of chat text is audited with the subject

**Acceptance**

- [ ] A report filed in game appears in the queue with its subject, realm and zone, and claiming it blocks a second moderator with the claim shown
- [ ] A chat search for a reported player returns that player's lines with their surrounding lines and nothing from an unrelated account
- [ ] Muting from a report links the mute to that report and shows in the subject's mute history with who, why and how long
- [ ] A user with `players.read` and no moderation permission sees queue counts and no chat text, and the refusal is audited
- [ ] Dismissing a report records who and why and leaves it searchable
- [ ] Every read of chat text writes an audit row naming the subject and the range read

## 17.64 Installation maintenance mode

**Goal:** An operator closes the whole installation to players for database work while game masters can still sign in and check it.

**Size:** M. **Depends on:** 17.12, 17.14, 2.14

**Deliverables**

- `Login.Maintenance`, a live setting through 17.12, that closes the login server to players: 2.14's authentication answers with the maintenance reason the client shows, and the realm list offers them nothing
- `Login.MaintenanceBypassLevel`, a live setting, letting accounts at or above that level sign in normally while maintenance is on; its default is a proposal in doc/PANEL.md
- A panel banner and control with who, why, when it started and an optional window, audited, and the window published to 17.70's public page
- Maintenance that survives a loginserver restart while it is on, and that leaves players already in the world connected unless the operator also closes their realms through 17.32
- A schedule task that enters and leaves maintenance, so a database window can be planned through 17.15

**Acceptance**

- [ ] With maintenance on, a player account's sign-in is refused with the maintenance reason and an account above the bypass level signs in
- [ ] Turning maintenance on and off applies without a loginserver restart, and it is still on after one
- [ ] Entering maintenance leaves players already in the world connected
- [ ] Every change writes an audit row with who, why and the window
- [ ] Dev-gated: the retail client shows the maintenance reason rather than a generic failure. Needs the maintainer's own retail client, and the result is recorded with the milestone

## 17.65 Patch server operations: manifest, revisions and signing key

**Goal:** Operators publish a client revision from the panel and hold the signing key the experimental-features rule requires.

**Size:** M. **Depends on:** 17.14, 17.47, 17.48, 16.03, 16.08

**Deliverables**

- A patch page over the 16.03 manifest: the revisions the patch output holds, each with its file count, bytes and build time, which one is being served, and the last generator run with its live output
- Publishing a revision: run the 16.03 generator against a chosen install, validate the output, then swap the served manifest through 16.08's reload so downloads in flight keep the old file set, with a rollback to the previous revision
- The operator's own signing key for patch components: generated or imported, its public part and fingerprint shown, rotation with an overlap window, the private part sealed in the supervisor's keyring, never in a response, and exportable only by an owner after a 17.47 step-up check; where that key lives and how it rotates is a proposal listed under Decisions needed in doc/ROADMAP.md
- Signed components and a patchserver that refuses to serve an executable whose signature does not verify, as Decisions, Experimental features requires
- Owner-only permissions for publishing and for the key, with every publish, swap, rollback, generation and rotation audited
- No route here serves a client file for download, and nothing is published that the operator did not build or place themselves, as the Client-derived data review note requires

**Acceptance**

- [ ] Publishing a revision from a chosen install swaps the served manifest with no restart, and a download in flight completes against the old file set
- [ ] A manifest that fails validation is not served, the previous revision keeps serving, and every error is shown
- [ ] An executable component whose signature does not verify is refused by the patchserver, naming the key it expected
- [ ] Rotating the signing key keeps the previous key valid for its overlap window and refuses it afterwards
- [ ] A non-owner gets 403 on publishing and on every key route, and both attempts are audited
- [ ] No patch route returns a file from outside the patch output root

## 17.66 Dashboard localization

**Goal:** The panel's own text can be read in the operator's language, without a half-translated page showing blanks.

**Size:** S. **Depends on:** 17.06, 17.35, 17.38

**Deliverables**

- Locale catalogs for the dashboard's own strings, one JSON file per locale under `apps/dashboard/`, with the installation default from 17.35 and a per-user locale from 17.38
- A test that fails when a string is added without a key in the default catalog, and a report of the keys each locale is missing, so an incomplete locale falls back key by key instead of rendering blank
- Dates, numbers and durations formatted with the browser's own locale facilities, and the operator's time zone named beside every absolute time
- Server-sent notices stay in one language, since localized server text is planned, not yet scheduled in doc/ROADMAP.md; this milestone covers only what the dashboard renders itself

**Acceptance**

- [ ] Switching a user's locale renders the navigation and the settings page in it, and every value stays unchanged
- [ ] A string added without a key in the default catalog fails the test
- [ ] A locale missing a key falls back to the default text rather than rendering blank, and the report names that key
- [ ] Absolute times render in the operator's own time zone with the zone named

## 17.67 Alerts, notifications and acknowledgement

**Goal:** Operators are told when something is wrong, once, and can see what was acknowledged and by whom.

**Size:** M. **Depends on:** 17.15, 17.16, 17.19, 17.35, 17.60

**Deliverables**

- Alert rules with a threshold, duration and severity for a crash, a crash loop, high tick time, high memory, low disk space, a failed backup, a failed or permission-revoked schedule, stale online rows cleared after a crash, sign-in lockouts above a rate and credentials near expiry; the update, node and queue conditions register with 17.17, 17.22 and 17.71 when those land
- Delivery to webhooks such as Discord and to email through 17.35's mail settings, with repeat limits, one notice while a condition stays true, and a failed delivery recorded and retried with backoff
- A notification center in the dashboard with toasts for finished background work
- An alerts page with each alert's history and acknowledgement, showing who acknowledged it and when, and a mute per rule with a reason and an end time
- Alert delivery that carries no secret and no player personal data, only the subject and the figure that tripped the rule

**Acceptance**

- [ ] Killing gameserver three times within a minute sends one crash-loop alert, not three separate ones
- [ ] A low disk alert that stays true for an hour sends one notice, and one more only after the repeat limit passes
- [ ] A failed backup and a failed schedule each send one alert naming the record
- [ ] A webhook that answers 500 is retried with backoff and its failure is visible on the alerts page
- [ ] Acknowledging an alert records who and when and stops its repeats while the history keeps the original
- [ ] An alert body carries no secret setting value and no player email or address

## 17.68 Schedules page and run history

**Goal:** Operators read, reorder and run schedules from the browser and can see exactly what every past run did.

**Size:** M. **Depends on:** 17.15, 17.26, 17.44

**Deliverables**

- A schedules page listing each schedule with its next run in the schedule's time zone and the viewer's, its tasks in order, its state and its last result
- Run now, pause, resume and cancel, each checked by 17.15's rule that an action needs every task's permission, with the reason shown wherever the page disables a control
- Drag-and-drop task reordering that sends the schedule's version with `If-Match`, so a stale reorder answers 409 instead of shuffling another operator's edit
- A run history per schedule, paged, with trigger, who, scheduled and actual start, status, skip reason, and each task's result and capped output
- Live run progress on the 17.26 socket, with a run interrupted by a crash shown as interrupted rather than still running
- A timeline per schedule showing every task against the anchor time and each 17.44 condition's effect in plain language, so an operator sees why a run would wait or skip before saving it

**Acceptance**

- [ ] The page shows the next run in both time zones and updates within a second of a save
- [ ] A user without `power.restart` sees run-now disabled on a schedule holding a restart task with the reason named, and the route refuses it too
- [ ] Reordering tasks from a stale view answers 409 and changes nothing
- [ ] A run's progress appears live, and killing the supervisor mid-run leaves that run shown as interrupted
- [ ] A long task's output is capped on the page exactly as it is capped in the store, with the cap stated
- [ ] Pausing a schedule lets the running run finish and arms no new one, resuming recomputes the next run under the misfire policy, and cancelling a run marks the remaining tasks cancelled while its always-run tasks still run
- [ ] The timeline places each task at its offset from the anchor and names each condition's effect, and a schedule that would skip an empty realm says so before it is saved

## 17.69 Game operations analytics

**Goal:** An operator sees how the game itself is doing: who signs up, who comes back, where the money goes and where new players stop.

**Size:** M. **Depends on:** 17.19, 17.21, 2.08

**Deliverables**

- Daily aggregates in the supervisor's store, read from the game databases: accounts created, accounts verified, first sessions, players returning by day and week, characters created by school and level band, and a session length distribution
- Economy figures where their tables exist: gold and crowns held and spent, items and reagents gained per source, and vendor and bazaar turnover; a figure whose owning milestone has not landed is reported as unavailable with that milestone named, never as zero
- A quest funnel per chain of offered, accepted, completed and abandoned counts, so an operator sees where new players stop
- Aggregation as a scheduled job over a window, idempotent per day so a re-run overwrites rather than doubles, with its rows exportable as CSV for holders of `activity.export` under 17.25's formula-safe writer
- Reads that use a statement timeout and, where one is configured, a follower connection, so a report never slows a live realm

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): a day of known seeded data reports exactly those accounts created, first sessions and characters created
- [ ] Re-running the aggregation for one day leaves the same rows, not doubled ones
- [ ] A figure whose source tables do not exist reports as unavailable naming the milestone, not as zero
- [ ] An aggregation run over a seeded month does not raise the gameserver's tick time past its alert threshold
- [ ] A user without `metrics.read` gets 403 on every analytics route, and an export without `activity.export` is refused

## 17.70 Public status and scheduled downtime page

**Goal:** Players can see whether the game is up and when the next maintenance window is, without an operator answering each question.

**Size:** S. **Depends on:** 17.14, 17.19, 17.64

**Deliverables**

- A public read-only page on the panel listener at its own path, off unless `Panel.PublicStatus.Enable` is set, showing each realm's up or down state, whether the login server is accepting players, and the current or next maintenance window from 17.64
- Aggregate figures only: no player name, address, account count or app internal, with a response shaping test that proves the payload holds nothing else
- Scheduled downtime published from a 17.15 schedule or a 17.64 window, with a short operator note, and an incident note an operator can post and clear, audited
- Its own cache and rate limit, no cookie set and no session read, so the page cannot be used to load the panel or to probe a session

**Acceptance**

- [ ] With the setting off the page answers 404, and with it on it answers with no session and sets no cookie
- [ ] The payload carries no player name, address or account figure, proven by the response shaping test
- [ ] A maintenance window set in 17.64 appears on the page, and clearing it removes it
- [ ] A flood of requests is rate limited while the operator area keeps answering
- [ ] An incident note posted and cleared by an operator is audited with who and when

## 17.71 Admission queue control

**Goal:** When a realm fills, an operator can see the queue, let players in and answer where someone stands.

**Size:** S. **Depends on:** 17.19, 17.31, 12.21

**Deliverables**

- A queue view per realm over 12.21's admission queue: length, the oldest wait, the admission rate and why the realm is closed
- Controls: raise or lower the realm's player limit, which releases queued players as the limit rises, pause and resume admissions, and drop the queue with a reason, each permission checked and audited
- A position lookup for one account, showing its position and estimated wait, with the account's identity behind `accounts.pii.read`
- Queue length and oldest wait added to the 17.19 graphs and to the realm page, with a queue-above-threshold rule offered to 17.67

**Acceptance**

- [ ] With a realm at its limit and players queued, raising the limit admits exactly as many as the rise allows, in queue order
- [ ] Pausing admissions leaves queued players waiting, and resuming admits them in the same order
- [ ] Dropping the queue records who and why and tells every dropped client
- [ ] A position lookup shows the position without revealing the account's email to a user without `accounts.pii.read`
- [ ] Queue length appears in the realm's graph with the same figure the queue view shows

## 17.72 Opt-in backup archive encryption

**Goal:** An archive that leaves the machine is useless to anyone without the operator's key.

**Size:** S. **Depends on:** 17.43, 17.47, 17.51, 17.52

**Deliverables**

- `Backups.Encrypt`, off by default and documented with its risk under Decisions, Experimental features: the archive sealed with AES-256-GCM under a key in the supervisor's keyring, so an archive holding account verifiers and the config that holds `Account.VerifierKeys` is unreadable without it; which library provides that cipher is the crypto stack proposal listed under Decisions needed in doc/ROADMAP.md, which names this milestone
- A key generated once, never written into an archive or a backup record, exportable only by an owner after a 17.47 step-up check, with the plain warning that a lost key makes every sealed archive unreadable
- Restore, 17.52's downloads and 17.43's catalog scan all recognizing a sealed archive, naming the key id they need and refusing clearly when it is absent
- Whether encryption becomes the default, and whether dumps become structured rows rather than SQL text, are proposals listed under Decisions needed in doc/ROADMAP.md

**Acceptance**

- [ ] With the setting on, the archive's bytes hold neither a known account verifier nor a known config secret, while a restore with the key succeeds
- [ ] A sealed archive restored without its key is refused naming the key id, and nothing is changed
- [ ] Exporting the key needs a step-up check and writes an audit row; a viewer gets 403
- [ ] The catalog scan lists a sealed archive from storage and marks it as needing a key it does not hold

## 17.73 Design system: tokens, components and the gallery

**Goal:** Every Ambrose surface is built from one set of tokens and one set of components, so the panel, the launcher window and the terminal cannot drift from doc/DESIGN.md or from each other.

**Size:** L. **Depends on:** 1.02, 1.03

Added on 2026-09-18 at the maintainer's direction, who asked that every screen look and feel like one product. It comes before the surfaces that use it: 17.06 builds the panel from it and 3.26 builds the launcher window from it, so neither invents its own look and nothing has to be restyled afterwards. It needs no server subsystem, only the build and its tests.

**Deliverables**

- `design/tokens.json`, the one place a design value is written, and `apps/designtokens/designtokens.py` with its tests, which generates the Tailwind theme and semantic layer as CSS, typed constants and union types as TypeScript, a constexpr header for the terminal carrying each token's truecolor, 256 and 16 values, and the tables inside doc/DESIGN.md itself, so the document cannot disagree with the code
- A contrast gate in the generator that computes every documented text and ground pair and refuses to generate when one falls below 4.5:1, or below 3:1 for text at 24 px and above and for a focus indicator
- `packages/ui/`, the `@ambrose/ui` workspace package that both apps import: no build step, raw components exported through the `svelte` condition, with the generated tokens, the shared motion module and its duration constants, the typed host bridge, and the named icon set
- The component set the panel and the launcher both need, grouped as doc/COMPONENTS.md lists them: foundations, controls, containers and overlays, shell and navigation, state and meaning, and data; behaviour from the primitive library, look from the tokens, and each one carrying its states, its keyboard behaviour and its label rules
- The three fonts vendored as latin variable files with our own face rules and their licence text, so a surface loads no font from any host
- A gallery in `packages/ui/.storybook` showing every component in every state, with a tokens page printing each token's contrast on each surface and an icons page listing the icons in use, built as static files that are never served to an operator
- Tests in four projects: logic and tokens with no browser, every component and every story in both Chromium and WebKit with the accessibility gate, screenshots in a container off the blocking path, and the scaffolding for the end-to-end project the surfaces will use
- Repository checks in the existing checks job: the generated files regenerate identically, no raw colour, arbitrary value or inline style carrying a colour appears outside the generated token file, every component has a story, and a built bundle contains no absolute http or https URL
- `apps/ci/ci_npm_cache.py`, which primes one npm cache for both Windows and Linux so an offline machine installs and builds, and a CMake option that skips the front-end build with a message naming what it left out
- A front-end CI job on Linux only, path-filtered to the front-end folders and the lockfile, and doc/COMPONENTS.md with the recipe and the scaffold command an agent follows to add a component

**Acceptance**

- [ ] Changing one value in `design/tokens.json` and running the generator changes the CSS, the TypeScript, the C++ header and the doc/DESIGN.md table together, and `designtokens.py --check` fails when any generated file is edited by hand
- [ ] Lowering one text token's lightness until a documented pair falls under its ratio makes the generator refuse, naming the pair and the ratio it reached
- [ ] A C++ test reads the generated header and a component test reads the generated stylesheet, and both fail when one token's value differs from the other's
- [ ] A component that writes a hex colour, a Tailwind arbitrary value or an inline style carrying a colour fails the checks job, and the allow comment is the only way past it
- [ ] Every component in `packages/ui` has at least one story, proved by a check that fails when a new component file has none
- [ ] Every story passes the accessibility gate at WCAG 2.1 AA, and the canary story with a deliberately unlabelled control fails the run
- [ ] The same component set passes in Chromium and in WebKit, and a failure in WebKit alone fails the job
- [ ] With the network unavailable, `npm ci --offline --ignore-scripts` from the primed cache installs and both apps build, on Windows and on Linux
- [ ] The built output makes no request to any other host, proved by a check that fails on an absolute http or https URL in the bundle
- [ ] With reduced motion set, every duration constant is zero, no screen loses information, and a test asserts both
- [ ] Building with the front-end CMake option off still builds the servers and prints what it left out
- [ ] The front-end job runs on Linux only, and a failing gallery or component test fails it

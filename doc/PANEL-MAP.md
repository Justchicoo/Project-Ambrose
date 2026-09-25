<!-- Project Ambrose by Imjustchico: Map of each Pterodactyl behavior studied to the phase 17 milestone that covers it in Ambrose. -->
# Pterodactyl behavior mapped to phase 17

This file lives at doc/PANEL-MAP.md and is linked from doc/PANEL.md, under Reference and ownership, which is how it is indexed; doc/PANEL.md itself is named in doc/ROADMAP.md's phase 17 row and in the review notes inside doc/roadmap/phase-17-operations-console-admin-api-dashboard-and-metrics.md. It is the working record behind that design: the MIT-licensed Pterodactyl panel and Wings were studied for behavior, as CLAUDE.md allows, and every behavior worth keeping, changing or dropping is listed here with the milestone that carries it.

Each row names one behavior, the phase 17 milestone that covers it in Ambrose, or "not applicable" with the reason Ambrose does not need it, and a note saying what Ambrose does instead. A behavior covered by several milestones lists them all, with the first one owning the core. Rows describe behavior only: this is not a record of another project's defects, and where Ambrose chose differently the note gives the reason in its own terms. Milestone ids are allocation order, not build order, so a row may name a higher id than the one beside it; doc/roadmap/phase-17-operations-console-admin-api-dashboard-and-metrics.md gives the dependency order. doc/PANEL.md describes what Ambrose keeps, changes or drops for each area, and marks which of its choices are still proposals.

## Accounts and sign-in

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Panel users: identity, username rules, unique email and username | 17.46 | Game-account username rules; email optional |
| Creating users from the admin form, application API and CLI | 17.46, 17.50, 17.36 | Panel users page, console `panel user create`, API through personal keys |
| Account-created email with a setup link | 17.37 | One-time invite link; email optional through 17.35 |
| Admin flag as the only global role | 17.48, 17.50 | Roles owner, admin, operator, game master and viewer from the catalog, with custom roles on 17.50's roles page |
| Cannot delete yourself; cannot delete a user who owns servers | 17.48 | Last-owner protection replaces the owns-servers rule |
| Admin cannot reset another user's two-factor from the web UI | 17.50 | Reset from the users page, audited with both users named |
| CLI command to disable two-factor | 17.50 | Reset from the users page; the console covers create, list, reset-password and disable (17.46) |
| Only user creation recorded in the activity log | 17.49, 17.25 | Every admin action on a user is audited |
| Demoting an admin silently breaking their application keys | 17.36 | Key rights intersect the owner's current permissions |
| Disabled or suspended user concept (absent) | 17.46 | Disabled flag added |
| Last sign-in time and address kept only in activity rows | 17.46 | Stored on the user row as well |
| SPA sign-in with a CSRF cookie flow | 17.14 | Same-origin session cookie and CSRF header |
| Password checked before the second factor to prevent enumeration | 17.46 | Kept |
| Unknown user skipping the password hash | 17.46 | Dummy Argon2id verify for unknown and disabled users |
| Typed identifier stored in failed sign-in activity | 17.46, 17.25 | Resolved user id or keyed hash only |
| Session id regenerated at sign-in | 17.46 | Kept |
| Remember-me cookie always set | 17.14 | Dropped; absolute lifetime instead |
| Login route throttle by address | 17.46 | Per address, per user and a global delay breaker |
| Login and two-factor checkpoint sharing one bucket | 17.46, 17.47 | Second factor has its own per-challenge limit |
| Success clearing the address's failure count | 17.46 | Only failures count; a success clears nothing |
| API rate limits per user | 17.14, 17.36, 17.58 | Per key and per user |
| Email change limited per user per day | 17.38 | Kept, with confirmation to the new address and notice to the old |
| reCAPTCHA with keys shipped in the panel | 17.35 | Opt-in captcha with the operator's own keys only; shipped keys not applicable because they are shared by every install and send visitor data to a third party |
| TOTP setup with a secret shown as QR and grouped text | 17.38 | QR rendered in the browser |
| Enabling TOTP with password and code | 17.47, 17.38 | Replay check also on enable |
| TOTP window of four steps | 17.47 | One step either side by default, bounded setting |
| Replay protection by last accepted time step | 17.47 | Kept |
| Two-factor required level: none, admins, everyone | 17.47 | `Panel.TwoFactorRequired`, enforced on routes and the socket |
| Required two-factor applied to API keys | 17.47, 17.36 | Kept |
| Disabling two-factor with only the password | 17.47, 17.38 | Needs password and a code, ends other sessions, deletes secret and codes |
| Old secret and recovery tokens left after disable | 17.47 | Deleted on disable |
| Re-fetching setup rotating a scanned secret | 17.38 | Pending secret kept apart from the active one |
| Confirmation token not consumed on a failed checkpoint | 17.47 | Challenge dies after a set number of attempts |
| Recovery tokens: 10, shown once, single use | 17.47, 17.38 | Kept, as keyed hashes found by lookup, with a count left and regeneration |
| Recovery tokens stored as salted bcrypt requiring a loop | 17.47 | Keyed hash under a supervisor key, found by direct lookup: Botan's HMAC-SHA-256, as Panel operations in doc/ARCHITECTURE.md settles |
| Case-sensitive recovery codes with no grouping | 17.47 | Crockford base32, grouped, case-insensitive |
| Security keys and WebAuthn (absent) | 17.45 | Opt-in security keys and passkeys |
| Session cookie flags and encrypted cookies | 17.14 | HttpOnly, SameSite=Strict, Secure and `__Host-` under TLS |
| Secure cookies depending on an environment variable | 17.14 | Secure whenever TLS is on |
| Password change ending other browser sessions | 17.38 | Kept |
| No sessions list or sign-out everywhere | 17.38 | Sessions list with per-session and everywhere sign-out |
| Two-factor changes not touching sessions | 17.47 | Session generation bump ends other sessions |
| Revocation job to the node, retried three times | 17.14, 17.57 | Revocation in-process within one second; queue not applicable |
| Security headers without a CSP | 17.14 | Strict CSP, frame-ancestors none, nosniff, referrer policy, HSTS |
| Trusted proxies for the client address | 17.14 | `Panel.TrustedProxies` on the listener itself, so throttles and audit rows are right from the start |
| Forgot password with an identical response either way | 17.38, 17.62 | Kept for operators when SMTP is configured, and for players in 17.62 |
| Reset tokens hashed, 60-minute expiry, per-user throttle | 17.46, 17.38 | Admin-issued link always available; self-service needs SMTP |
| Reset token and email in the query string | 17.38 | Token in the request body |
| Reset endpoint without its own throttle | 17.38 | Limited per address and per user |
| Reset not bypassing two-factor sign-in | 17.38 | Kept |
| Reset activity event with an untranslated name | 17.25 | Catalog test prevents events without a sentence |
| Admin SMTP settings with encrypted password and test mail | 17.35 | Kept, with an explicit clear control |
| Mail as a queued job requiring a worker | not applicable | No queue worker; mail sends in-process |
| Account page with email, password, two-factor, API keys, SSH keys, activity | 17.38 | Plus profile, sessions and game account link |
| Email change without notice to old address or session end | 17.38 | Confirms new address and notifies the old one |
| Client API keys: identifier plus encrypted secret, 25 per user, IP allow list | 17.36 | Hashed secret, scopes, expiry, rotation |
| Admin's client key carrying full application API power | 17.36 | Keys never exceed the owner's permissions at request time |
| Key comparison not constant time | 17.36 | Constant time |
| Last-used time written on every request | 17.36 | Written at most once a minute |
| Allowed IPs split without trimming line endings | 17.36 | Trimmed, blank lines ignored |
| Application API keys with per-resource read and write columns | 17.36 | Merged into scoped personal keys; the separate deprecated type is not applicable |
| Showing a key's full secret to its owner again | 17.36 | Only a hash is stored, so the secret is shown once |
| Any admin revoking any application key, unaudited | 17.36 | `apikeys.manage`, audited |
| SSH keys on the account with validation before parsing | 17.40 | Kept for opt-in SFTP |
| SFTP sign-in with the panel password | 17.40 | Password only with a TOTP code when two-factor is on |
| SFTP throttle keyed on the node's address | 17.40 | Keyed per user and per client address |
| Activity recording builder with actor, subjects, batch and transaction | 17.49, 17.14, 17.25 | `AuditScope` over the audit tables 17.14 adds, same-transaction audit rows |
| Activity write failures swallowed in production | 17.14, 17.25 | Fail closed for security-relevant actions |
| Wings activity uploaded at least once without an idempotency key | 17.22, 17.25 | Event uuid deduplication on forwarding |
| Activity pruning after 90 days | 17.25 | Retention per event class |
| Account activity showing only events where the user is subject | 17.25 | Actor or subject |
| Server activity page with event filter and pagination | 17.25 | Per app, realm, node, account, character and global |
| Addresses shown to the actor or a global admin | 17.25 | Actor or `activity.ip.read` |
| Hide admin activity option | not applicable | Every action must stay visible in the audit log |
| Actor emails visible to anyone reading server activity | 17.48, 17.25 | Other operators' emails need `users.read` |
| Metadata dialog for extra properties | 17.25 | Details view |
| No admin-wide activity viewer | 17.25 | Global activity page |
| Gravatar avatars | not applicable | Third-party request that leaks an email hash; local initials instead |
| Machine credentials: node bearer token and token id | 17.22 | Mutual TLS with pinned certificates |
| Browser-to-node HS256 tokens signed with the node secret | not applicable | Browsers only talk to the panel on its own origin |
| Player-facing account registration and password recovery (absent) | 17.62 | Opt-in sign-up on the panel listener, email verification, player-driven reset |

## Authorization

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Static permission catalog of groups and keys served to the UI | 17.48 | Ambrose catalog with danger flags and allowed scopes |
| Catalog descriptions drifting from enforced checks | 17.48, 17.18 | One table drives checks and labels; one write permission for files |
| Unknown permission strings dropped on save | 17.48, 17.37 | Kept, with a rename mapping |
| A socket-connect permission auto-granted to every sub-user | 17.48, 17.57 | `status.read` implicit; `console.read` separate |
| Admin-only socket permissions for error text, install and transfer | 17.26 | `debug.errors`, `clientdata.read`, `nodes.read` |
| Wildcard `*` for owners and admins | 17.48 | No wildcard; explicit roles |
| Effective permissions from admin, owner or subuser row | 17.48 | Roles plus grants over a scope tree |
| No unique index on user and server for subusers | 17.37 | Unique grant key |
| Ownership moving leaving an old subuser row active | 17.37 | Ownership is a grant at a scope; changes revoke |
| Route binding by uuid, short id or prefixed id | 17.48 | Stable ids and readable prefixed ids |
| 404 for non-members, 403 for missing permission | 17.48 | Kept |
| Conflict 409 for suspended, maintenance, installing, restoring, transferring | 17.27 | Protected states: setup, updating, restoring, moving, disabled |
| A resource-belongs-to-server check that fails closed on unknown types | 17.48 | Registration fails for a parameter without a belongs-to rule |
| Request classes with no permission open to any member | 17.48 | Route registry test fails when no permission is declared |
| Per-route permission map for client API | 17.48 | Every route registers its permission and scope |
| Business rules layered on permissions (variables, images, restore state) | 17.12, 17.28, 17.51 | Setting edit classes, restore gates, launch validation |
| Response filtering: hidden variables, allocations, subusers, database passwords | 17.12, 17.48, 17.30 | Masked secrets, personal data, database passwords |
| Backup created locked only with delete permission | 17.52 | `backups.pin` is its own permission; refused with 403, not ignored |
| Subuser invite by email creating an account instantly | 17.37 | One-time invite link |
| Invite response revealing whether an account exists | 17.37 | No response reveals existence |
| Invite throttled per server | 17.37 | Per inviter and per scope |
| Duplicate invites racing without a lock | 17.37 | Unique grant key |
| AddedToServer and RemovedFromServer emails | 17.37 | In-panel notice; email opt-in |
| Subset rule on subuser create and update | 17.37 | Strict no-escalation rule with add and remove lists |
| Subuser editing themselves refused | 17.37 | Kept |
| No hierarchy between subusers | 17.37 | Cannot edit a user who is not a strict subset |
| Editor unable to resubmit permissions they lack | 17.37 | Add and remove lists leave them untouched |
| Group select-all ignoring disabled keys | 17.37 | Select-all limited to grantable keys |
| Revocation cancelling every socket on the server | 17.57 | Only the affected user's sockets close |
| Resource creation throttles per server shared by all users | 17.14, 17.58 | Per user and per scope |
| Quota counts under row locks | 17.16, 17.36 | Counts inside transactions |
| Wings 30 sockets per server, per-event limits, 4096 byte read limit | 17.58 | Per user and per connection limits, 64 KiB frames |
| Websocket token issuance with permissions inside the token | 17.26 | Live permission checks per message |
| Outbound event filtering by permission | 17.57 | Each stream has its read permission |
| Generic error text with error id unless admin | 17.26 | `debug.errors`, correlation id |
| Origin allow list including a wildcard | 17.26 | Exact origin only |
| Signed download, upload and backup links, single use, 15 minutes | 17.52, 17.54 | HMAC links and download tickets bound to user and path |
| S3 presigned download URL with no user binding | 17.43 | Presigned reads only for restores streamed by the supervisor |
| SFTP per-operation permission checks | 17.40 | Same `files.*` grants as HTTP |
| One global admin flag granting the admin area, the application API, every server and addresses | 17.48 | Owner and admin roles |
| No last-admin protection; demotion revoking nothing | 17.48 | Last-owner protection; demotion bumps the session generation |
| Admin-only listings of every server | 17.06 | Owners and admins switch between granted and all apps |
| Server ownership and owner changes revoking the old owner | 17.37 | Owner grant at a scope; changes audited and revoked |
| Server ownership as the tenancy model | not applicable | Ambrose operators are one team; grants replace ownership |
| Account keys carrying the user's full rights | 17.36 | Keys carry a subset |
| Application key bitmask ACL per resource | 17.36 | Scoped subsets of catalog permissions |
| IP allow lists with blocked attempts logged | 17.36 | Kept |
| Node configuration endpoint requiring write-level rights | 17.22 | Secret-bearing node config needs `nodes.manage` |
| Schedule tasks requiring their action's permission | 17.15 | Kept and extended to run now, timing edits and resume |
| Run now not checking task permissions by design | 17.15 | Run now checks every task's permission |
| Tasks running after their author loses rights | 17.15 | Automatic runs skipped as permission revoked |
| Node trust boundary with generic 403 and node-scoped servers | 17.22 | Node may act only on apps placed on it |
| A node secret able to mint browser tokens | not applicable | Ambrose mints no node-signed user tokens; browsers talk only to the panel |
| Front-end permission hooks gating controls | 17.06, 17.48 | Route table and hidden controls |
| Prefix match without the dot matching other groups | 17.06 | Group matches include the dot |
| Permissions refreshed only on page reload | 17.57 | `permissions.changed` event updates the UI live |
| Download menu shown without a permission check | 17.54 | Every control gated |
| Revocation on password change, deletion, subuser change | 17.46, 17.48 | Session generation bump on each |
| No revocation on admin demotion, two-factor disable or key deletion | 17.47, 17.48, 17.36 | Each ends affected sessions or key use at once |
| Denied client API requests not logged | 17.49, 17.25 | Refused attempts recorded |
| Addon lifecycle hooks | not applicable | Ambrose extends through compiled modules, not install hooks run as root |

## Console and power

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Websocket token endpoint and HS256 JWT with claims and 10-minute expiry | 17.26 | Session cookie on the panel origin; one-time ticket for API keys; the JWT itself is not applicable |
| Token refresh on expiring and expired events | 17.57 | `session.expiring` renews the session over HTTP |
| Token issuance limited to 5 per minute per server | not applicable | No tokens to fetch; per-user limits instead |
| Revocation denylist by server and user, boot-time cutoff | 17.57 | Session generation checked per message |
| Upgrade with CSP and frame headers, origin check, socket cap | 17.14, 17.26 | Headers on every response; exact origin; per-user caps |
| Suspended server closing the socket with 4409 | 17.27, 17.57 | Disabled apps refuse starts; socket close codes 4400, 4401, 4403, 4429 |
| Auth event as first message; re-auth swapping the token | 17.26 | `hello` first frame with CSRF token |
| Invalid token replacing outbound events with errors | 17.57 | Permission snapshot checked per frame; closes with 4403 |
| No ping or pong frames | 17.58 | Pings every 20 seconds, idle close after 60 |
| Reconnect that clears the terminal and replays a fixed number of lines | 17.07, 17.26 | Resume by sequence number on the one stream layer |
| Envelope of event plus string args with JSON inside strings | 17.26 | Versioned typed envelope with structured data |
| Event catalog: status, stats, console output, install, backup, transfer, daemon messages | 17.26 | Full Ambrose event list in doc/PANEL.md |
| A completion listener whose event name never matches what the server sends | 17.26 | Contract test between server events and UI handlers |
| Transfer status values the UI does not handle | 17.26, 17.42 | Typed move events with handlers |
| Silent drop of commands and power without permission | 17.57, 17.58 | Every refusal answers `error` or `throttled` |
| Error disclosure gated by admin permission with error id | 17.26 | `debug.errors` and correlation id |
| Anyone with connect permission reading all console output | 17.48, 17.57 | `console.read` separate from status |
| Per-connection flood guard and per-event buckets | 17.58 | Kept, per connection and per user |
| Inbound messages handled concurrently, reordering commands | 17.58 | One ordered queue per connection |
| UI ignoring throttled events | 17.58 | Throttled replies shown |
| Output capture from attached container stdout and stderr | 17.08 | Supervisor capture as fallback to structured logs |
| Line splitting at 64 KiB and stray carriage returns | 17.08 | `ChildProcess` line splitting at 64 KiB |
| Output throttle dropping lines at 2000 per 100 ms | 17.04 | Rate guard coalesces and reports; lines still reach the log file |
| Log fan-out that drops the oldest records after 10 ms | 17.04, 17.58 | Bounded queues with dropped markers; status never drops |
| Daemon messages as colored console prefixes | 17.26 | `source = supervisor` field |
| Backlog of 150 lines from the container log | 17.04 | 1000 records per app, current and previous run |
| Container log truncated on each start | 17.60 | Previous run kept, crash tail stored |
| No backlog while offline | 17.04, 17.60 | Backlog kept after exit |
| Commands written to container stdin with no result | 17.05, 17.07 | CommandMgr with output lines and request id |
| Commands during start silently lost | 17.07, 17.27 | Refused with a reason |
| HTTP command route with no length cap | 17.05, 17.58 | Bounded by the 64 KiB frame cap |
| Stop command typed in console treated as a stop | 17.08 | Supervisor knows its own requests; app reports stopping |
| Command history of 32 lines in localStorage | 17.49 | Per user and app in the supervisor's store; the page keeps only in-memory recall |
| Power actions start, stop, restart, kill with permission mapping | 17.27 | Kept, with `power.kill` separate |
| Power lock with kill bypass | 17.27 | Kept, refusals name the holder |
| HTTP power returning 202 with failures only logged | 17.27 | Operation id with progress and result |
| Pre-start sync, disk check, config file rewrite | 17.59 | Typed pre-start checks; config rewrite not applicable |
| Stop via command or signal, 10-minute wait then SIGKILL | 17.08 | Admin API shutdown, stdin, Ctrl+Break or SIGTERM, then tree kill after a per-app timeout |
| Egg stop string with `^SIGNAL` syntax | not applicable | Stop behavior is typed per app kind |
| Restart starting an offline server | 17.27 | Kept |
| Four states and state change events | 17.27 | Extended states and protected states |
| Ready detection by done string or regex in console output | 17.08 | Health API lifecycle state |
| No starting timeout | 17.28 | Start timeout per app |
| Crash detection with one restart per 60 seconds | 17.60 | Exit classification, exponential backoff, crash loop |
| No crash history or alert | 17.60, 17.67 | Crash records with log tails and alerts |
| Clean exit counted as a crash by default | 17.60 | Kept, configurable |
| States file written every minute and restored at boot | 17.08 | Desired state saved on change; re-adoption checks process identity |
| Disk limiter stopping a server over quota | 17.18, 17.67 | Space guard refusal plus a low-disk alert; stopping a server over a quota is not applicable, because it disconnects players |
| Live stats from Docker streaming API | 17.19 | Operating system sampling per process |
| CPU not normalized to limits | 17.19 | Percent of one core and of the machine |
| Cumulative container network counters | 17.03, 17.19 | App's own network and message counters |
| Disk usage cached walk with stale value | 17.19 | Kept, with hard links counted once |
| Stats pushed with no viewers | 17.57 | Pushed only to subscribers |
| Resource endpoint with 20-second cache and 30-second polling | 17.06, 17.58 | Socket updates within a second |
| Console commands and power actions in activity, successes only | 17.05, 17.25, 17.49 | Refused and failed attempts too |
| Wings activity batching via local SQLite and cron | 17.22 | Node-side durable buffer only; single machine writes directly |
| Command text stored with no redaction | 17.05 | Sensitive arguments redacted |
| xterm terminal with search, links, jump to bottom | 17.07 | Log viewer with search, filters, pause, jump to newest |
| Power buttons: start when offline, stop becomes kill with confirmation | 17.27 | Kept |
| Stat blocks with 80 and 90 percent colors | 17.19 | Kept |
| Graphs of 20 points reset on offline | 17.19 | 60 or more live points backed by 30 days of history |
| Egg feature modals matching console strings | 17.03, 17.06 | Structured problem records with fix-it buttons |
| Minecraft EULA, Java, Steam, GSL token features | not applicable | Not Wizard101 concerns |
| Dashboard server rows with status bar and usage | 17.06 | Cards with Wizard101 fields |
| Restart confirmation showing who is about to be interrupted (absent) | 17.59 | Players in world, sessions at character select and active patch downloads |
| Pre-start checks reported as a failed operation (absent) | 17.59 | Binary, config, database, client data, ports and disk, each with its result |

## Schedules

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Schedule record with name, cron fields, active, processing, only when online | 17.15 | Schedule with scope, trigger, zone, state, policies |
| An update clearing the in-progress flag whenever the active toggle changes | 17.15 | Runs marked interrupted at start; no processing flag |
| Cascade delete of tasks | 17.15 | Kept; delete during a run answers 409 unless cancelling |
| No run history table | 17.15, 17.68 | Run and task run records |
| Missing month field causing a server error | 17.15 | One expression string with every field error named |
| Five separate cron inputs with a generic invalid message | 17.15 | Presets or a custom expression with field positions |
| Extended cron syntax with L, W and # | 17.15 | Standard five fields, names, steps and macros; extended forms not planned |
| Single panel-wide time zone | 17.15 | IANA zone per schedule |
| MySQL session offset workaround | not applicable | UTC Unix times in SQLite |
| DST handling not defined | 17.15 | Defined gap and repeat behavior with tests |
| Next and last run shown in browser time without a zone label | 17.68 | Schedule zone and viewer zone both shown |
| Minute cron runner via crontab and artisan | 17.15 | Heap and timer in the supervisor; cron runner not applicable |
| One broken schedule not aborting the batch | 17.15 | Each run its own coroutine |
| Zero-task schedules matched every minute | 17.15 | A schedule with no tasks is never armed |
| Missed slots firing once on recovery | 17.15 | Misfire policy with grace |
| Overlapping chains firing back to back | 17.15 | Overlap policy skip or queue one |
| A stuck in-progress flag after a lost job | 17.15 | State in committed rows; interrupted at start |
| The active flag stopping a chain in flight and stamping the last run | 17.15 | Pause and separate cancel; skips recorded, never stamped as run |
| Only when online checked once before the first task | 17.15, 17.44 | Per-task `require_target_running` |
| Wings errors swallowed during the online check | 17.15 | Recorded skip with reason |
| Run now dispatching the first task inside the HTTP request | 17.15 | 202 with run id; never inside the request |
| Run now ignoring an already running chain | 17.15 | 409 unless queue one |
| Failed manual run missing from activity | 17.15, 17.25 | Every run and refusal audited |
| Task sequence insert, shift and delete | 17.15 | Unique position rewritten in one transaction |
| No reorder UI | 17.68 | Drag-and-drop and keyboard reordering |
| Duplicate sequence ids under concurrent creates | 17.15 | Unique position key |
| Time offsets 0 to 900 seconds after previous | 17.15 | After previous or from anchor, offsets up to a day |
| Offsets measured from an accepted request, not completion | 17.15, 17.27 | Completion waits for readiness or real results |
| Continue on failure only for daemon connection errors | 17.15 | Failure policy for every error type, retry option |
| Implicit three retries repeating a command | not applicable | No queue worker; retries are explicit |
| Command task writing a multi-line payload to stdin | 17.15 | One CommandMgr line, no newlines |
| Power task limited to start, stop, restart, kill payloads | 17.15, 17.27 | Kept, with restart-with-countdown macro |
| Backup task with ignore list and rotation of oldest | 17.15, 17.16, 17.52 | Typed component selection; retention after success |
| Backup throttles aborting scheduled chains | 17.16 | Backups queue behind a running job; the creation throttle is per user |
| A task runner that reloads rows and chains itself | 17.15 | Snapshot of tasks per run |
| Tasks still reaching the daemon during a transfer | 17.15, 17.27 | Protected states make tasks wait or skip |
| Failure recording limited to the last run and an in-progress flag | 17.68, 17.67 | Run history and failed schedule alerts |
| Task limit of 10 per schedule | 17.15 | Cap is a live setting |
| Schedule creation throttle 2 per minute per server | not applicable | Tenancy throttle; per-user limits cover abuse |
| Schedule permissions read, create, update, delete | 17.48, 17.15 | `schedules.read`, `edit`, `run`, `delete` |
| Deleting a task needing no action permission | 17.15 | Saving the task list needs every task's permission |
| Schedule UI with cheatsheet, cron rows, task rows, run now button | 17.68 | Preview, timeline, run history, run page |
| An in-progress state that is never cleared | 17.15, 17.68 | Live schedule events |
| The daemon's own in-process scheduler with an overlap guard | 17.15 | Same guard pattern in `ScheduleMgr` |
| Announcements before restarts (absent) | 17.15, 17.33 | In-game countdowns and announcements |
| Skip when empty or wait for quiet (absent) | 17.44 | `skip_if_empty` and `only_when_empty` |
| Rolling restarts (absent) | 17.44 | One realm at a time |
| Time zone data reloaded with the process (absent) | 17.15 | A reload recomputes every next run; a schedule whose zone the data lost is held with an error |
| Optimistic concurrency on a schedule save (absent) | 17.15, 17.68 | A version with `If-Match`; a stale save or reorder answers 409 |

## Backups and restore

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Backup row with state inferred from two columns | 17.16 | Status enum with phase and progress |
| Storage adapter looked up from the current default at completion | 17.16, 17.43 | Storage target stored per backup |
| Deleting a server orphaning archives and objects | 17.43 | Catalog scan finds and deletes orphans |
| Transfers leaving backups on the old node | 17.42 | Moves back up on the source and restore on the target |
| Per-server backup limit where 0 disables backups | not applicable | A hosting quota; retention, disk budget and permissions replace it |
| Time throttle counting deleted backups | 17.16 | Kept, per user |
| Row lock before counting against the limit | 17.16 | Counts inside transactions |
| Rotation deleting the oldest before the new backup exists | 17.52 | Retention only after the new backup is verified |
| Start request with name, ignore list and lock flag | 17.16 | Name, components, patterns, pin |
| Lock flag silently ignored without delete permission | 17.52 | Refused with 403 without `backups.pin` |
| Wings allowing concurrent backups of one server | 17.16 | One backup or restore at a time, others queue |
| A root ignore file | not applicable | Typed components with per-component patterns |
| tar.gz with tunable compression levels | not applicable | zstd is settled |
| Write rate limit for archives | 17.16 | Kept, plus I/O priority and tick-time backoff |
| Live files copied while the server runs | 17.16 | Consistent database snapshots and a character flush |
| Empty directories lost from archives | 17.16 | Recorded |
| A SHA-1 checksum recorded per archive | 17.16 | SHA-256 manifest and per-entry checksums |
| Completion callback from Wings to the panel | 17.16, 17.22 | In-process on one machine; node outbox in node mode |
| Failed callback deleting the archive and leaving the row in progress | 17.16 | Interrupted jobs marked at start; node reports retried from a durable outbox |
| Failed backup re-reportable as successful | 17.16 | Each state transition accepted once per uuid |
| Realtime backup completed events filtered by backup.read | 17.57 | `backup.*` events for `backups.read` |
| UI marking every completion successful | 17.26 | Typed payload with true status |
| S3 multipart upload with presigned part URLs | 17.43 | Resumable, per-part checksums; presigned parts only in node mode |
| All part URLs sharing one expiry | 17.43 | Short-lived batches in node mode |
| No resume after a part failure | 17.43 | Resume by reconciling listed parts |
| Configured prefix never applied to object keys | 17.43 | Prefix applied |
| Orphaned multipart uploads never aborted | 17.43 | Start-up sweep aborts stale uploads |
| Prune job marking backups older than six hours failed | 17.16 | Interrupted jobs marked at start, watchdog per phase |
| Backup list with count against limit | 17.16 | Storage usage against free space |
| Download link JWT from Wings, single use, 15 minutes | 17.52 | Hashed ticket with step-up, Range support |
| Download of an in-progress backup serving a partial file | 17.52 | Refused unless succeeded |
| S3 presigned download link valid 5 minutes | 17.43 | Presigned GET for downloads from a target |
| Locked and succeeded backups not deletable; failed ones always deletable | 17.52 | Kept as pins |
| Lock and delete sharing one permission | 17.52 | `backups.pin` separate |
| Deleting an in-progress backup while Wings writes | 17.16 | Delete cancels the running job first |
| Restore requiring status null and a successful completed backup | 17.51 | Kept, plus update level and verifier key gates |
| Truncate before locating or downloading the archive | 17.51 | Verify everything first; staging and atomic swap |
| Checksum never verified on restore | 17.51 | Every SHA-256 checked before any change |
| No safety backup or rollback | 17.51 | Safety backup and automatic rollback |
| Server not started again after restore | 17.51 | Apps started and health checked |
| Restoring state blocking every route with 409 | 17.51, 17.27 | Read-only pages and live progress stay available |
| Restore completed event sent even on failure | 17.51, 17.26 | `restore.failed` event |
| Event names that never render, and a start event never emitted | 17.25 | Catalog test ties emitted names to sentences |
| Wings boot reset clearing restoring state without auditing | 17.51 | Incomplete restore reported at next start |
| SSRF guard for S3 restore download URLs | 17.43 | Endpoint address checks on storage targets |
| Panel-supplied restore download URL | not applicable | Objects resolve from admin-configured targets only |
| No body read timeout on restore downloads | 17.43 | Resumable ranged reads with timeouts |
| Scheduled backup tasks with rotation override | 17.15, 17.52 | Retention after success |
| Five backup permissions with danger text | 17.48 | `backups.*` with danger flags, plus `restore.players` and `pin` |
| Backup UI: list, create modal, context menu, restore dialog | 17.16, 17.51 | Restore wizard with typed confirmation |
| Trash button on failed backups not permission-gated | 17.52 | Every control gated |
| A restore checked against the dump's own record (absent) | 17.16, 17.51 | Per-table row counts and checksums taken inside the dump transaction, compared by the restore report |
| Archive encryption (absent) | 17.72 | `Backups.Encrypt`, on by default, as Panel operations in doc/ARCHITECTURE.md settles |

## File management

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Jail rooted at the server data folder with dirfd and openat2 | 17.18 | Named roots and a jail for Linux and Windows |
| openat fallback relying on /proc | 17.18 | Component-by-component walk without following links |
| Absolute paths joined under the root | 17.18 | Absolute paths refused |
| Refusing to delete or rename the root | 17.18 | Kept |
| Errors hiding resolved host paths | 17.18 | Kept; resolved path only in the audit row |
| Symlink write, copy and mkdir escapes refused | 17.18 | Kept |
| Chmod following symlinks before Wings 1.12.2 | 17.55 | fchmod on a descriptor opened without following links |
| Symlinked directories listed as files | 17.18 | Typed entries with link targets checked |
| Archiver reading link targets relative to the working directory | 17.39 | Links stored only when they resolve inside the root |
| Quota with atomic usage and per-write reservation | 17.18 | Space guard per volume with a reservation ledger |
| Usage unknown until the first walk allowing writes | 17.18 | Free-space check from the volume |
| Hard links counted once in usage | 17.18 | Kept |
| Compress checking quota after writing the archive | 17.39 | Reservation while streaming |
| Egg file denylist matched on unresolved paths | 17.18 | Protected paths matched on resolved paths for every operation |
| Denylist skipped for SFTP, delete, read and compress | 17.18, 17.40 | Applied to all operations |
| Denylist replaced by egg inheritance | not applicable | No eggs |
| Listing with MIME sniffing of every file | 17.18 | Extension plus first-bytes check on open |
| One failing entry failing the whole listing | 17.18 | Error row per entry |
| 250-entry UI limit and duplicate name filtering | 17.18 | Server-side paging, sorting and filtering |
| Read contents with 4 MiB editor cap checked after download | 17.18, 17.53 | Cap checked from size before streaming |
| FIFO refused after open | 17.18 | Non-blocking open and type check before any read |
| Write truncating in place with last save winning | 17.53 | Temporary file, rename, ETag conflicts, version store |
| Write needing create permission while UI checks update | 17.48, 17.54 | One `files.write` permission |
| Batch rename applied concurrently with partial results | 17.54 | Validated first, applied in order, result per pair |
| Missing rename source silently skipped | 17.54 | Reported per pair |
| Copy limited to one file in the same folder | 17.54 | Recursive copy to a destination |
| Copy naming race without exclusive create | 17.54 | Exclusive create |
| Permanent delete | 17.55 | Trash with restore, and purge behind `files.purge` |
| Create folder with nested names | 17.55 | Kept, with name rules and nested paths made in one call |
| Compress to tar.gz named with a timestamp | 17.39 | zip or tar.zst named without colons, background job |
| Two archives in one second colliding | 17.39 | Exclusive create |
| Decompress with declared-size pre-check and per-entry caps | 17.39 | Kept, plus total, count, depth and ratio caps |
| Extraction overwriting files and leaving partial results | 17.39 | Staging with conflicts listed and a report |
| Denylisted archive entries dropped silently | 17.39 | Reported as skipped |
| Setuid bits from archives passed through | 17.39 | Modes masked |
| Decompress needing create while archive text claims it | 17.48, 17.39 | `files.archive` covers both |
| Chmod with free octal modes, unaudited | 17.55 | Read-only and executable toggles behind `files.permissions`, audited, never a free octal mode |
| Chmod failing on kernels without the newer syscall | 17.55 | fchmod on a descriptor opened without following links |
| Remote pull with connected-address SSRF checks | 17.41 | Opt-in, with more ranges blocked |
| Remote pull missing 0.0.0.0/8, CGNAT and own addresses | 17.41 | Blocked |
| Remote pull refusing redirects and chunked responses | 17.41 | Redirects checked per hop; size enforced while streaming |
| Remote pull leaving partial files | 17.41 | Partial files removed |
| Remote pull as the way to install server jars and mods | not applicable | Updates come from the 17.17 release and build channels |
| Signed download JWT with file path claim | 17.54 | HMAC link bound to path and file identity |
| File swapped at the path after issue being served | 17.54 | File id and modification time in the link |
| Download link logged at issue instead of use | 17.54, 17.25 | Logged when served |
| Signed upload JWT with directory not bound | 17.54 | Folder bound in the link |
| Upload size checked only after the body is received | 17.54 | Enforced while streaming |
| Uploads silently overwriting existing files | 17.54 | Replace must be chosen |
| Folder uploads unsupported | 17.54 | Supported |
| Token single use through an in-memory unique id cache | 17.54 | Nonce remembered until expiry |
| Revocation denylist and boot-time cutoff for tokens | 17.54 | Issue time checked against start and user revocation; grant checked again at use |
| SFTP server with modern algorithms and ed25519 host key | 17.40 | Opt-in, off by default |
| SFTP username in `user.serverid` form | 17.40 | Panel username; node suffix only in node mode |
| SFTP read-only mode switch | 17.40 | Config root read-only over SFTP |
| SFTP password auth bypassing two-factor | 17.40 | Password only with a TOTP code |
| SFTP public key validation before parsing | 17.40 | Kept |
| SFTP per-operation permission checks before side effects | 17.40 | Kept with `files.*` grants |
| SFTP write always truncating, breaking resume | 17.40 | Append and resume honored |
| SFTP rename refusing overwrite; rmdir recursive | 17.40 | Standard behavior per operation where safe |
| SFTP writes stopped during protected states | 17.40 | Kept |
| SFTP activity merged per minute | 17.40, 17.25 | Kept |
| File permission keys and route gating | 17.48, 17.18 | `files.*` keys with roots as a grant dimension |
| File activity events for read, write, rename, delete and more | 17.18, 17.25 | Plus chmod and download served |
| File manager UI: breadcrumbs, selection, context menu, mass actions | 17.18, 17.54 | Files page kept, with a bottom sheet on phones; mass copy and download are 17.54 |
| Mass select-all acting on unseen files | 17.18 | Count shown for the filtered set |
| Drag-and-drop upload with per-file progress and cancel | 17.54 | Kept, with overwrite prompt |
| One failed upload cancelling the rest | 17.54 | Per-file results |
| CodeMirror 5 editor with language modes and Ctrl+S | 17.53 | CodeMirror 6 proposed; Ambrose file modes |
| New-file drafts kept in session storage | 17.53 | Drafts for new and edited files |
| No dirty-state guard when leaving the editor | 17.53 | Unsaved-changes guard |
| A root ignore file banner in the editor | not applicable | No root ignore file |
| Web-hosting and Minecraft editor modes (PHP, Vue, Pug, Sass and others) | not applicable | Ambrose files are conf, SQL, JSON, XML, Lua, logs and scripts |
| Following a growing log (absent) | 17.39 | Log follow across rotation |
| Name search (absent) | 17.39 | Bounded search |
| Version history for an edited file (absent) | 17.53 | Per-root version store with view, diff and restore |
| Operator-defined roots beyond the shipped ones (absent) | 17.56 | Owner-defined, refused when they overlap a root or hold the supervisor's keys |

## Admin area and platform

| Pterodactyl feature | Ambrose milestone | Notes |
|---|---|---|
| Nodes with location, FQDN, scheme, capacity, overallocation, maintenance | 17.22 | Node record with Wizard101 capacity |
| Overallocation of -1 meaning different things in UI and placement | 17.22 | One formula for page and placement |
| Node settings saved even when Wings is unreachable, with a warning | 17.22 | Pending until acknowledged |
| Node token stored encrypted with the app key | 17.22 | Pinned certificates; join token stored hashed |
| Reset daemon master key checkbox | 17.22 | Certificate reissue with overlap |
| Delete node refused while servers exist | 17.22 | Refused while apps are placed |
| Maintenance mode blocking non-admin access | 17.22, 17.32 | Node maintenance can cascade to realm maintenance |
| Configuration YAML shown with the plaintext token | not applicable | No bearer secret to show; join command shown once |
| Auto-deploy with a long-lived application key | 17.22 | Single-use, short-lived join token |
| `wings configure` fetching configuration | 17.22 | `supervisor --join` |
| Config push validating, writing then swapping, refusing env overrides | 17.22 | Kept |
| Token indirection through environment or file | 17.35 | Environment and command-line locks on settings |
| Browser pinging every node with its token in page HTML | 17.22 | Heartbeats pushed over the node link |
| System information proxy with OS, kernel, CPU | 17.22 | Heartbeat carries system information |
| Boot reconciliation fetching servers and restoring states | 17.08, 17.22 | Re-adoption with identity checks; node resync |
| Servers stuck installing reset to normal on boot | 17.20, 17.27 | Interrupted setup reported, not cleared silently |
| CDN latest version check | 17.17 | Release channel check, only when configured; CDN check not applicable |
| Default-on telemetry | not applicable | Ambrose sends no telemetry |
| Locations as node groups | 17.22 | Kept |
| Allocations: IP, CIDR, port ranges up to 1000, aliases, notes | 17.29 | Kept, with IPv6 and free-port checks |
| Ports at or below 1024 refused | 17.29 | Allowed with a privileges warning |
| Mass delete not rechecking assignment | 17.29 | Refused on every path |
| Allocation alias changes not pushed | 17.29 | Assignment writes listen settings live |
| Client-created allocations from a port range | not applicable | Hosting customer feature; the node port pool supplies new realms |
| Primary allocation and additional allocations per server | 17.29 | Role per allocation |
| Automatic deployment choosing viable nodes and allocations | 17.22 | Placement on nodes |
| Dedicated IP deployment option | not applicable | No per-customer IP placement |
| Nests grouping eggs | not applicable | App kinds are fixed: loginserver, gameserver, patchserver |
| Eggs: startup command, images, stop, done strings, config file parsers | 17.28 | Typed launch settings per app; the egg model is not applicable |
| Egg import and export with inheritance | 17.13 | Settings presets with validation and diff |
| Update via import deleting variables missing from the file | 17.13 | Imports never remove unnamed keys |
| Egg features for console-string modals | 17.03, 17.06 | Structured problem records |
| Egg for running Ambrose under Pterodactyl | 17.23 | Kept |
| Egg variables with rules, viewable and editable flags | 17.12, 17.13 | Typed settings with visibility and edit class |
| Laravel rule strings as an admin-editable DSL | not applicable | Compiled typed schemas |
| Reserved variable names | 17.12 | Locked layers |
| Hidden variables never serialized; startup preview with hidden values | 17.12, 17.28 | Masked secrets; command-line preview with masking |
| Install log dumping every environment variable | 17.20, 17.59 | Setup logs redact secrets |
| Install scripts in install containers | not applicable | Setup is typed C++ (3.22 setup, typeextract, updater) |
| Install lifecycle with a lock, live output and status | 17.20, 17.27 | Protected setup state with live output |
| A failed install state recoverable only by deleting the server | 17.20 | Rerun setup from the client data page |
| Reinstall blocked when scripts are skipped | 17.20 | Rerun setup refused when `Setup.Mode = off` |
| Docker images per egg with admin override | 17.28 | Build selection per app from curated builds |
| Local-only `~` images | 17.17 | Local builds verified by hash |
| Mounts with node allowlist | 17.56 | Operator-defined roots with the same jail and policy fields |
| Mount allowlist prefix test without separator boundary | 17.56 | Component-boundary comparison |
| Database hosts tested after insert inside a transaction | 17.30 | Tested before anything is saved |
| Per-server databases created by customers with limits | not applicable | Fixed login, characters and world databases |
| Generated database users with broad grants | 17.30 | Least-privilege runtime and updater users |
| Password rotation dropping and recreating the user | 17.30 | Dual passwords or a second user with pool swap |
| Password reveal behind its own permission | 17.30 | `database.secrets.read` plus a step-up check |
| Server creation with owner, node, allocation, limits, egg, image | 17.22, 17.28, 17.29 | Placement, launch settings, allocations |
| Server identity by numeric id, uuid, short id, prefixed id, external id | 17.22 | Stable ids and readable prefixes |
| Server config delivered to Wings by pull | 17.22 | Validated config push to nodes |
| Resource limits memory, swap, disk, io, cpu, threads, OOM | 17.28, 17.67 | Opt-in hard limits, with soft thresholds as 17.67 alert rules over 17.19's samples |
| Swap, io weight and OOM killer toggles | not applicable | Container runtime settings |
| Feature limits for databases, allocations, backups | not applicable | Hosting quotas; realm player limit (17.31), retention (17.16) and schedule caps (17.15) cover Ambrose needs |
| Server state machine and conflict screens | 17.27 | Protected states and state pages |
| Suspension stopping the server and blocking access | 17.27, 17.32, 17.64 | Disable an app; realm maintenance |
| Server deletion safe and forced | 17.22 | Removing an app placement from a node |
| Details changes with owner revocation | 17.37 | Grant changes revoke at once |
| Server transfers between nodes | 17.42 | Moves with cancel and stall timeout |
| Transfers stuck forever when a daemon dies | 17.42 | Stall timeout fails the move and releases allocations |
| Settings key-value table overriding environment config | 17.35 | Typed settings with locks |
| Settings saves restarting queue workers | not applicable | Settings apply live |
| General settings: name, two-factor level, language | 17.35 | Kept |
| Mail settings with `!e` clearing the password | 17.35 | Explicit clear control |
| Advanced settings: reCAPTCHA, HTTP timeouts, allocation range | 17.35, 17.29 | Opt-in captcha, relay timeouts, port pool |
| Admin area gated by one global admin flag | 17.48 | Roles and grants |
| Application API ACL per resource | 17.36 | Scoped personal keys |
| Admin Blade UI and React client as two stacks | 17.06 | One Svelte app |
| Route definitions with permission per route | 17.06 | Kept |
| Admin sidebar grouped by section | 17.06 | Operate, Game, Platform, Panel groups |
| Dashboard server rows polling resources every 30 seconds | 17.06, 17.58 | Socket updates |
| Show all servers toggle in localStorage | 17.06 | Per-user toggle |
| Maintenance badge, with polling skipped while it is set | 17.06 | Maintenance badge |
| Flash messages keyed per component | 17.06 | Errors beside the form that caused them |
| SweetAlert confirmations | 17.06 | Typed-name confirmation for destructive actions |
| Email notifications for install and server membership | 17.67, 17.37 | Notification center and opt-in email |
| Request id on daemon errors | 17.06 | Request id on every error |
| Dark-only theme with custom palette | 17.06 | Light and dark themes |
| i18n locale files loaded by the SPA | 17.66 | Locale catalogs for the dashboard's own strings, with a per-user locale; server strings stay in one language |
| Gravatar in the admin header | not applicable | Third-party request |

## Ambrose features with no Pterodactyl counterpart

A hosting panel manages a container with a console. These milestones exist because Ambrose manages a Wizard101 installation, so nothing in the studied source maps to them.

| Ambrose feature | Milestone | Why it has no counterpart |
|---|---|---|
| Server console with colored logs, a prompt and built-in commands | 17.01 | Pterodactyl reads a container's output; an Ambrose app owns its own console |
| Each app's own admin API and token | 17.02 | The panel relays typed app APIs instead of attaching to standard input |
| Prometheus metrics and provisioned Grafana dashboards | 17.09, 17.10 | Graphs of game figures such as tick time and players per zone |
| Terminal dashboard mode | 17.11 | For an operator on a shell with no browser |
| Live settings and reload pages | 17.12, 17.13 | Typed, bounded game settings that apply without a restart |
| Game accounts, bans, characters and online players | 17.21 | A hosting panel has no player accounts to manage |
| Desktop control app that starts everything from one icon | 17.24 | A player hosting for themselves, not a hosting customer |
| Realms and zones, with population and zone actions | 17.31 | Wizard101 realms and loaded zones |
| Realm maintenance with a bypass level | 17.32 | Closing one realm to players while game masters check it |
| Announcements and timed game events | 17.33 | In-game messages and reverting rate changes |
| World database edits with a journal and SQL export | 17.34 | Typed content editing over the game server's own schema |
| Reconciling stale online and session rows after a crash | 17.61 | A crashed gameserver otherwise leaves a character locked out |
| Player registration and password recovery | 17.62 | Players, not operators, and never a panel user |
| Moderation queue, chat search and mute history | 17.63 | Player reports and chat records |
| Installation-wide maintenance mode with a bypass level | 17.64 | Closing the login server to players for database work |
| Patch server operations and the operator's signing key | 17.65 | Publishing a client revision and signing what the patchserver serves |
| Game operations analytics | 17.69 | Sign-ups, retention, economy and quest funnels |
| Public status and scheduled downtime page | 17.70 | Players asking whether the game is up |
| Admission queue control | 17.71 | Releasing queued players when a realm's limit rises |
| Client-derived roots that refuse download and archive | 17.18, 17.43, 17.65 | The bring-your-own-files rule, which a generic panel has no reason to carry |
| Supervisor protocol version in the node join and heartbeat | 17.22 | Panel and node are one project's builds, so a mismatch is refusable rather than fatal |

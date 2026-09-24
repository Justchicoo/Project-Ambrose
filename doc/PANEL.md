<!-- Project Ambrose by Imjustchico: Design of the Ambrose panel: its host, security model, pages, APIs and data. -->
# The Ambrose panel

The Ambrose panel is the web control center for running Wizard101 servers built on Project Ambrose. It gives operators the depth of a game server hosting panel such as Pterodactyl: their own accounts and permissions, a live console, power control, schedules, backups, a file manager, graphs and alerts, and several machines under one panel. It is built for Ambrose's own apps and data, so it also manages what a generic hosting panel cannot reach: realms and zones, players online, game accounts, bans and characters, live settings and reloads, client data and revisions, world database edits, announcements and events, patch revisions, player account registration and recovery, reports and mutes, an installation-wide maintenance mode, and the stale rows a gameserver crash leaves behind.

Phase 17 of doc/ROADMAP.md plans the work, milestone by milestone. This document describes the design those milestones build. Choices already settled live under Decisions in doc/ARCHITECTURE.md, chiefly Operations, Stack, Live reload and live settings, and Experimental features. Nothing is settled by being written here: where this design proposes a choice, the text says so, and the choice is listed under Decisions needed in doc/ROADMAP.md, which is where decisions that block milestones live until the maintainer settles them and doc/ARCHITECTURE.md records them.

## Reference and ownership

Pterodactyl's panel (PHP with Laravel and React) and its node daemon Wings (Go) are MIT-licensed. Ambrose studies their source for behavior, data models, flows, security measures and user experience, and copies no code and no long text. Ambrose is not a fork and does not run Wings, for the reasons recorded in the phase 17 review notes: the panel must run natively on Windows and Linux with no setup steps, and its model of a container with a console cannot reach typed Ambrose features. Each section below closes with what Ambrose keeps from Pterodactyl, what it changes, and what it drops, with the reason for the choice Ambrose made. The references studied were the panel's 1.0-develop line, its develop line and Wings; where the two panel lines differ, Ambrose follows the more hardened behavior. doc/PANEL-MAP.md maps each studied feature to the milestone that covers it.

## Architecture

### Processes

| Process | Role in the panel |
|---|---|
| supervisor | Starts, stops, restarts and watches every app on its machine. Hosts the panel: serves the built Svelte files, the panel API under `/api/panel/`, and the event socket. Holds the panel's SQLite store. In node mode it is the agent for a remote panel |
| loginserver, gameserver, patchserver | Each serves its own admin API from 17.02, bound to localhost with a per-app token. The supervisor relays these APIs; browsers never talk to them |
| typeextract and other tools | Run by the supervisor through `ChildProcess` as jobs whose output streams to the panel |

The supervisor is a fourth executable in `src/server/apps/`, beside loginserver, gameserver and patchserver, and it is both the panel backend and the node daemon. On a single machine nothing sits between the browser and the process that owns the apps, so permissions are checked live on every request and every socket message, with no token handed to the browser for another host.

### Request path

1. The browser loads the panel from the supervisor's own origin and signs in (17.14).
2. Every API call and socket message carries the session cookie and a CSRF token. The supervisor's `AuthorizationMgr` resolves the caller's permissions at the target scope before any work.
3. Panel-owned work (users, schedules, backups, files, nodes, audit) runs in the supervisor on a bounded worker pool, never on the HTTP I/O threads.
4. App work (commands, settings, reloads, accounts, players, world edits) is relayed to the app's admin API with the per-app token the supervisor holds (17.49). The supervisor passes the panel user's identity and the command security level their grants allow, and the app runs the action through CommandMgr at that level. A panel grant never lifts a command above its allowed level.
5. Results, progress and live data return on the event socket (17.26).

### Layering and code placement

- `src/server/apps/supervisor/` holds the supervisor executable, its process control, the panel HTTP routes and the event socket.
- Game-agnostic pieces go in `src/common/`: cron expressions and next-run math, the token bucket, the TOTP and recovery code primitives, and the cross-platform file jail.
- The panel front end lives in `apps/dashboard/`, TypeScript and Svelte built by Vite, as settled. Its TypeScript types for API and socket payloads are generated from the same schemas the C++ side serializes, so the two cannot drift.
- Nothing from the game client enters the panel's build or its store. Client data shown in the panel is read at runtime from the user's own install.

### Listener, TLS and the bind rule

The panel listener (17.14) is the one entry point that carries session cookies, passwords and two-factor codes, so it is held to the admin API's rule and then some. Extending the Remote access rule under Decisions, Operations to the panel's own listener was settled on 2026-09-22 and built in 17.14; what it comes to is:

- `Panel.Enable` is off by default and `Panel.BindIP` is 127.0.0.1. A non-loopback bind is refused at startup unless `Panel.CertificateFile` and `Panel.PrivateKeyFile` name a certificate and key, the same names the admin API uses under its own prefix, and a reload that would leave the bind unsafe is refused with an error naming the option while the old listener keeps serving.
- `Panel.AllowPlainHttpRemote`, off by default and documented with its risk, is the only way to serve the panel beyond localhost without TLS. It logs a warning naming the option and states plainly that sign-in secrets then cross the network unencrypted.
- The certificate and key are PEM files, checked at load for a matching key, validity dates and chain order, swapped live on a reload with the old pair kept and every error reported when the new pair fails, with the fingerprint printed to the console and the log and a warning as expiry approaches. `supervisor --panel-self-signed` writes a certificate for a machine-local panel and prints its fingerprint.
- HSTS is sent under TLS only, beside the strict Content-Security-Policy and the other response headers below.
- `Panel.TrustedProxies` decides the client address: it is the rightmost hop not in that list, forwarded headers from any other peer are ignored, and every throttle, audit row and sign-in record uses that address. It belongs to the listener, not to a later settings page, because sign-in throttles and audit addresses are wrong without it.
- Session-authenticated routes carry a cost-weighted rate limit per user and per address, where each route declares its cost as it registers, so the routes that cost real work are limited as they land: backup creation, archive and name-search jobs, activity exports, settings batches, database host tests and remote pulls. A throttled request answers 429 with a retry hint, and one audit row records each throttled user and minute.
- Security keys and passkeys (17.45) need a secure context, which localhost is and a TLS panel is; over plain HTTP from another machine they are not offered.

### Store

The supervisor keeps one SQLite file in WAL mode in the Ambrose data folder. It holds everything the panel needs before any game database exists:

- panel users, roles, grants, invites, sessions, API keys, recovery codes and security keys;
- the audit log;
- schedules, tasks and run history;
- backup records, storage targets, upload state and restore jobs;
- nodes, locations, port allocations and app placements;
- launch settings, database host records and panel settings;
- alert rules, alert history and downsampled graph history.

Times are UTC Unix milliseconds in `INTEGER` columns. What is settled about cryptography is narrower than what the panel needs: Botan 3 is the project's library, covering SHA-2, Twofish and the random number generator under Decisions, Stack; Botan's Argon2id hashes panel passwords under Decisions, Operations, libsodium having been dropped on 2026-09-22 because its port needs autotools a clean Linux machine lacks; and AES-256-GCM is the cipher Decisions, Accounts and the console already uses for `login.account.verifier`. What this design proposes is that the panel adds no third library, so its sealing and its keyed hashes come from Botan while libsodium stays at Argon2id. Whether Botan's entry grows to name a cipher and a keyed hash, or libsodium widens instead, is a proposal listed under Decisions needed in doc/ROADMAP.md, and no milestone treats it as settled. Under that proposal, secrets the supervisor must use again, such as TOTP secrets, SMTP and S3 credentials and database host passwords, are sealed with AES-256-GCM under a supervisor key, with a key id on each row so keys rotate the way `Account.VerifierKeys` does; whether that key lives in a keyring file separate from the store, and where it lives on each platform, is another proposal in doc/ROADMAP.md. Secrets that are only checked, such as passwords, API key secrets, session ids, invite tokens and recovery codes, are stored only as hashes, keyed where the value is random enough to be found by direct lookup. Schema changes to the store use the same dated update files as the game databases, applied by the supervisor at start.

The store is included in backups as the `panel` component (see Backups and restore) and is never restored implicitly.

### Live behavior

The panel follows the Live reload and live settings rule. Every panel option, such as session lifetimes, sign-in thresholds, retention, alert repeat limits, trusted proxies and SMTP settings, is a typed setting with a default and bounds, changed from the panel, audited, applied from the next operation, and locked by a higher layer with that layer named. The layer order is the settled one under Configuration in doc/ARCHITECTURE.md: `<app>.conf.dist`, `conf.d/*.conf.dist`, `<app>.conf`, `conf.d/*.conf`, persisted live settings, `AMBROSE_` environment variables, then command-line overrides. The panel invents no layer of its own, and the keys the supervisor passes to an app on its command line are that last layer. Structures such as the role table, file roots, node list and port pool are rebuilt off to the side and swapped atomically, keeping the old structure and reporting every error when a rebuild fails. The supervisor restarts only for a binary upgrade.

### From Pterodactyl

- **Keeps:** one entry point for operators; a daemon that owns processes and reports state; a relational store for panel data; a scheduled job runner; a node trust boundary checked on every call.
- **Changes:** the panel and the daemon are one C++ process instead of a PHP web app plus a Go daemon; the store is an embedded SQLite file instead of MySQL plus Redis; there is no queue worker, since jobs run in-process and persist their state at each step.
- **Drops:** Laravel, Redis, the queue worker, Blade and AdminLTE admin pages, the separate React client, Docker as the app runtime, and the panel-to-daemon HTTP hop on one machine. Each would break the setup-free desktop run or duplicate what the supervisor already does.

## Identity, sign-in and sessions

### Panel users

Panel users are operators, separate from game accounts in `login.account`. A panel user can be linked to a game account (see Account page).

- `panel_user`: id, uuid, username (the game-account username rules: up to 32 ASCII letters, digits, `_`, `-` and `.`, unique regardless of case), display name, optional email (unique when set), password hash, disabled flag, `password_changed_at`, `session_generation`, TOTP state, created, last sign-in time and address, locale, theme and linked game account id.
- Passwords use libsodium `crypto_pwhash_str` (Argon2id), as settled under Decisions, Operations, with its limits as bounded settings, and are rehashed at the next sign-in when the limits change.
- One password policy applies on every path, self-service, admin, console and API: at least 12 bytes by default, at most 128 bytes to match the game password limit.
- A disabled user cannot sign in, and disabling ends their sessions. Deleting is separate and cascades to grants, keys and sessions.
- The last owner can never be deleted, disabled or demoted, and nobody can raise their own role or edit their own grants.
- Every admin action on a user is audited: create, update, role or grant change, disable, delete, two-factor reset and password reset link.

The first owner is created from a one-time sign-in link the supervisor prints on first start, usable once and only from the same machine (17.46). There is never a default password. The supervisor console also has `panel user create`, `panel user list`, `panel user reset-password` and `panel user disable`, run at console level, audited with the actor `console`, with their arguments kept out of logs as the console rules require. Resetting another user's two-factor sign-in is a users page action (17.50).

### Sign-in

1. `POST /api/panel/auth/sign-in` takes a username and password.
2. The password is checked before any second factor, so the second step never tells a stranger that an account exists. An unknown or disabled user still costs one Argon2id verify against a dummy hash, so its timing matches a wrong password, as the login server does for ClientKey1.
3. With no second factor, the session id is regenerated and the response is complete. With a second factor, the response carries a challenge bound to a short pre-authentication cookie, valid for 5 minutes and for at most 5 attempts, after which the challenge dies.
4. Failures return one generic message. The audit row stores the resolved user id when there is one, or else a keyed hash of the normalized username, never the typed text, so a password pasted into the username field never reaches the log.

Throttling reuses the 17.02 token bucket at three levels: per address (an IPv6 client by its /64), per target user, and a small global breaker that only adds delay and never locks everyone out. Only failures count, and a success does not clear the counters, so signing in to one account cannot reset guesses against another. The second-factor step has its own per-challenge limit and does not share the password bucket. Client addresses behind a reverse proxy come only from the `Panel.TrustedProxies` setting, which the listener owns from 17.14, so no throttle or audit row ever trusts a forwarded header from an untrusted peer. There is no captcha by default (see Panel settings). Sign-in and its throttles are 17.46 and the second factor 17.47, so both are correct before any other page is built.

### Two-factor sign-in

- TOTP per RFC 6238 (SHA-1, 6 digits, 30 seconds) with a window of plus or minus one step by default, bounded as a setting.
- The last accepted time step is stored, and any step at or below it is refused, on sign-in, on enabling and on every step-up check, so a code cannot be replayed.
- Setup keeps a pending secret apart from the active one; enabling needs the password and a valid code. Fetching setup again does not silently replace a pending secret that is already scanned.
- Disabling needs the password and a current code or recovery code, deletes the secret and the recovery codes, and ends every other session.
- Recovery codes: 10 codes of 10 Crockford base32 characters, grouped for typing and read case-insensitively. They are random, so each is stored as a keyed hash under a supervisor key, which allows a direct lookup instead of a loop over every stored code; which library provides that keyed hash is the crypto proposal described under Store. A used code records its time; the account page shows how many remain and can regenerate them with the password and a code.
- `Panel.TwoFactorRequired` is none, holders of a danger permission and owners and admins, or everyone. When two-factor is required and missing, every API route and the socket answer 403 with the code `two_factor_required`, except the sign-in, account security and enrollment routes, and the panel sends the user to enrollment. Enforcement lands with two-factor sign-in itself in 17.47; 17.35's settings page only shows the key with the layer that locks it.
- Security keys and passkeys through WebAuthn are an opt-in second factor or passwordless sign-in (17.45). They need a secure context, which localhost is and a TLS panel is, so the listener's bind rule already makes that the normal case. The WebAuthn implementation is a proposal in doc/ROADMAP.md.

### Sessions

- `panel_session`: SHA-256 of a 256-bit cookie secret, user id, created, last seen, idle expiry, absolute expiry, address, user agent, CSRF secret, time of the last second-factor check, the user's session generation, revoked time.
- The cookie is HttpOnly and SameSite=Strict, Secure whenever TLS is on, and uses the `__Host-` prefix under TLS. Idle (12 hours) and absolute (7 days) lifetimes are settings; "keep me signed in" picks a longer absolute lifetime and there is no separate remember cookie that outlives a password change.
- Every state-changing cookie request carries the CSRF token in a header. API keys are never accepted with a cookie, and cookies are ignored when a key is present.
- Step-up: downloading a backup, revealing a secret setting or database password, restoring, rotating credentials and managing owners need a password or second-factor check within the last few minutes.
- A password change, a two-factor change, a role or grant reduction, a disable or a delete bumps the user's session generation. Every session and socket of that user that no longer matches is closed within one second, and only that user's sockets close.
- Every response carries a strict Content-Security-Policy, `frame-ancestors 'none'`, `X-Content-Type-Options: nosniff`, `Referrer-Policy: same-origin`, and HSTS under TLS, all from the listener in 17.14. The panel makes no request to third-party hosts.

### Password reset and email

Email is optional. The baseline, which always works, is an admin action that issues a one-time reset link for the admin to hand over, and the console `panel user reset-password`. Reset tokens are stored hashed, single use, valid for 30 to 60 minutes as a setting, carried in the POST body rather than the query string, and audited. A reset ends every session and does not bypass two-factor sign-in. When SMTP is configured (17.35), self-service "forgot password" sends the link by email and answers the same way whether or not the account exists, rate limited per address and per user. Players are a different surface: their registration and recovery are described under Player accounts (17.62) and share no route, throttle or session with panel sign-in.

### From Pterodactyl

- **Keeps:** password checked before the second factor; session id regenerated at sign-in; generic failure messages; TOTP with recovery codes shown once in a dialog that cannot be dismissed from outside; replay protection by last accepted time step; a required two-factor level with enrollment routes exempt; password changes ending other sessions; email change limits per user; a console path to recover access.
- **Changes:** Argon2id instead of bcrypt; a disabled flag instead of delete-or-change-password; throttles keyed per address and per user with the second factor in its own bucket, since a bucket shared by both steps, or one not keyed to its target, lets one client lock others out; replay protection also on enable; a narrower TOTP window than 4 steps; disabling two-factor needs a code and ends sessions; recovery codes are keyed hashes found by lookup, countable and regenerable; failed sign-ins never store the typed identifier; a sessions list with sign-out per session and everywhere; last-owner protection; every admin action audited, where Pterodactyl records only user creation.
- **Drops:** reCAPTCHA with keys shipped in the panel, which needs Google and sends visitor data there; Gravatar avatars, which leak an email hash; required first and last names and billing external ids; remember-me cookies that outlive a session; mail as a hard dependency for account setup; SFTP sign-in with the panel password, which bypasses two-factor sign-in.

## Permissions

### Scopes

What is settled, under Decisions, Operations in doc/ARCHITECTURE.md, is roles plus per-app grants in the style of sub-users. Until the maintainer settles more, every grant, route and acceptance check in phase 17 stays at that app scope.

A wider scope tree is a proposal listed under Decisions needed in doc/ROADMAP.md, because it decides how realms, clusters and nodes are addressed at all. If it lands, a grant applies at a scope and to everything below it, from widest to narrowest:

| Scope | Holds |
|---|---|
| panel | Everything: nodes, panel users, API keys, updates, panel settings |
| node | One machine and the apps placed on it |
| cluster | One login database with its loginserver and patchserver, and the game accounts, bans and patch content in it |
| realm | One gameserver with its characters in play, online players, zones, world edits and realm settings |
| app | One app instance: its power, console, logs, files, graphs, launch settings and reloads |

Characters are stored in the shared `characters` database, as Decisions, Character list records, so a character grant is checked at realm scope for play actions and at cluster scope for account-wide actions such as restoring a deleted wizard. At the settled app scope both are checked on the realm's own gameserver app.

### Roles

Roles are named permission bundles resolved on the server, and they are edited live: a change builds a new resolution table, swaps it in and bumps the session generation of every affected user. 17.50 keeps them as rows built from the catalog rather than bundles compiled into the binary, with a roles page where an owner adds or edits a custom role, so the five below are rows like any other. The bundles below and the owner-only set are proposed together with the scope tree in doc/ROADMAP.md.

| Role | Holds |
|---|---|
| owner | Every permission, including the owner-only set |
| admin | Every permission except the owner-only set |
| operator | Day-to-day operation: console, power, settings, reloads, schedules, backups without restoring player data, files outside secrets, players, accounts without delete or security level, announcements |
| game master | Players, account reads, bans, characters, announcements, activity about those subjects, and console at game master level |
| viewer | Status, logs, graphs, settings reads, activity reads, and file listing and reading outside secrets |

Custom roles are allowed, built from the catalog on 17.50's roles page, and no custom role may carry a key from the owner-only set, which stays with the owner role. That set is: managing owners and admins, node join tokens and certificates, update channels, `Account.VerifierKeys` and other secret keys, the listener keys (`Admin.BindIP` and `Panel.BindIP`, the TLS paths and both `AllowPlainHttpRemote` settings), the supervisor's own keys, defining file roots (17.56), the backup archive key (17.72), and publishing a patch revision with the patch signing key (17.65).

A user's effective permissions at a scope are the union of their role and every grant at that scope and its ancestors. An API key's permissions are intersected with that set on every request.

### Permission catalog

The catalog is one C++ table of groups, keys, descriptions, a structured danger flag, the scope types each key may be granted at, and default role membership. The panel serves it at `GET /api/panel/permissions`, the Svelte UI renders the grant editor from it, and the route registry checks every route against it when routes register. Renaming a key goes through a mapping that drops unknown names rather than granting something wider. The catalog is built in 17.48, and its Scopes column below belongs to the scope tree proposal; today every key is granted at app scope.

| Group | Keys | Scopes |
|---|---|---|
| status | `status.read` (implicit for any member of a scope) | all |
| console | `console.read`, `console.write`, `console.raw` (danger, owner-only by default: write to an app's standard input when its admin API is down) | app, realm, node, panel |
| power | `power.start`, `power.stop`, `power.restart`, `power.kill` (danger: can lose unsaved character state) | app, realm, node, panel |
| launch | `launch.read`, `launch.edit` | app, node, panel |
| network | `network.read`, `network.edit` (port allocations and listen addresses) | app, node, panel |
| settings | `settings.read`, `settings.edit`, `settings.edit.restricted` (danger), `settings.secrets.read` (danger) | app, realm, cluster, panel |
| reload | `reload.read`, `reload.run` | app, realm, cluster, panel |
| files | `files.list`, `files.read`, `files.download`, `files.write`, `files.upload`, `files.delete`, `files.purge`, `files.archive`, `files.permissions`, `files.roots` (danger, owner-only: defining a root), `files.pull` (opt-in), `files.sftp` (opt-in) | app, node, panel |
| backups | `backups.read`, `backups.create`, `backups.download` (danger: archives hold account verifiers and config secrets), `backups.restore` (danger), `backups.restore.players` (danger: can undo bans and password changes), `backups.delete`, `backups.pin`, `backups.settings`, `backups.key` (danger, owner-only: exporting the archive key) | node, panel |
| schedules | `schedules.read`, `schedules.edit`, `schedules.run`, `schedules.delete` | app, realm, node, panel |
| updates | `updates.read`, `updates.apply` (danger), `updates.rollback` | node, panel |
| clientdata | `clientdata.read`, `clientdata.rebuild`, `clientdata.switch` | app, node, panel |
| patch | `patch.read`, `patch.publish` (danger, owner-only), `patch.key` (danger, owner-only: the operator's signing key) | node, panel |
| database | `database.read`, `database.hosts` (danger), `database.rotate`, `database.secrets.read` (danger) | cluster, realm, node, panel |
| world | `world.read`, `world.edit`, `world.export` | realm, panel |
| realms | `realms.read`, `realms.edit`, `realms.maintenance`, `realms.queue` (admission queue controls) | realm, cluster, panel |
| accounts | `accounts.read`, `accounts.pii.read` (email, last address, MachineID), `accounts.create`, `accounts.edit`, `accounts.password`, `accounts.security` (danger), `accounts.ban`, `accounts.unban`, `accounts.delete` (danger), `accounts.registration` (the sign-up page's operator controls) | cluster, panel |
| characters | `characters.read`, `characters.rename`, `characters.restore`, `characters.edit`, `characters.delete` (danger) | realm, cluster, panel |
| players | `players.read`, `players.kick`, `players.mute`, `players.teleport` | realm, cluster, panel |
| moderation | `reports.read`, `reports.claim`, `reports.action`, `chat.read` (danger: reads player chat text, audited per read) | realm, cluster, panel |
| announcements | `announcements.send`, `events.manage` | realm, cluster, panel |
| metrics | `metrics.read` (status figures, graphs and the analytics pages), `alerts.read`, `alerts.manage` | all |
| activity | `activity.read`, `activity.ip.read`, `activity.export` | all |
| users | `users.read`, `users.invite`, `users.update`, `users.delete` | all |
| apikeys | `apikeys.manage` (other users' keys; every user manages their own) | panel |
| nodes | `nodes.read`, `nodes.manage` (danger), `nodes.move` | node, panel |
| panel | `panel.settings` (danger), `panel.maintenance` (danger: closes the installation to players), `panel.status` (posting on the public status page) | panel |
| debug | `debug.errors` (full error text instead of a correlation id) | panel |

### Enforcement

One `AuthorizationMgr` in the supervisor (17.48) decides every check, in this order:

1. Authenticate the session or API key.
2. Resolve the scope the route names. When the caller holds nothing at that scope, or the object does not exist, answer 404, so a stranger cannot learn that an app or realm exists.
3. Confirm that every object in the path belongs to that scope: a backup to its node, a task to its schedule and the schedule to its scope, a file to its root, a character to its realm, an account to its cluster. A parameter type without a belongs-to rule fails the route's registration.
4. Check the permission. A missing permission answers 403, and a refused danger permission writes an audit row.
5. Check business rules and state, answering 409 while the target is restoring, updating, moving or running setup.

Every route registers its permission and scope parameter. A route that declares neither, and does not explicitly declare itself open to any member, fails a test generated from the route registry. The same test checks that every route answers 403 without its permission and 404 for a scope the caller cannot see.

Response shaping follows the catalog. Secret settings are masked without `settings.secrets.read`. Account email, address and MachineID are hidden without `accounts.pii.read`. Audit addresses show only to the actor and to holders of `activity.ip.read`. Other operators' emails show only with `users.read`. No response ever carries a verifier, a session key hash, a TOTP secret or an API key secret, whatever the caller holds.

### Game-specific rules

- **Command level.** A `console.write` grant carries a maximum CommandMgr security level: game master (2) by default for operators and game masters, and console (4) only for owners and admins. When the panel user is linked to a game account, the lower of the grant's level and the account's level applies. Commands above the cap answer as unknown, as 4.02 does. Typed panel actions such as ban, kick and password reset check both the typed permission and the level of the command they run. This is a proposal listed under Decisions needed in doc/ROADMAP.md, and it ties to the open decision on how account security levels map to LOGINCOMPLETE IsCSR and Permissions.
- **No escalation over game accounts.** A panel user with `accounts.security` may set a level only below their own effective level, and cannot ban, lock, reset, rename or delete an account whose level is at or above their own, or one linked to a panel user who outranks them.
- **Settings classes.** Each setting in the 4.16 schema has a visibility, normal or secret, and an edit class, normal or restricted. `settings.edit` covers normal settings. Restricted settings (`Admin.*`, the `Login.*` lockout and attempt limits, `Account.*` keys, `Console.Enable`, patchserver serving) need `settings.edit.restricted`. Keys locked by the environment or command line are refused for everyone, with the layer named.
- **Reload targets.** `reload.run` is checked per target, and a message definition or type dump reload also needs `clientdata.rebuild`.
- **Schedules.** Saving a task, running a schedule now, changing its timing and resuming it each need the permission of every task's action at its target, so schedule rights never become console or power rights.

### Invites and the grant editor

Ambrose runs with no mail server by default, so invites are one-time links (17.37):

- An invite carries a scope, a role and extra grants, is stored as a SHA-256 of its token, expires after 72 hours by default, works once, and can be listed and revoked.
- The invitee chooses a username and password and enrolls two-factor sign-in when policy requires it. Inviting an existing panel user by username adds the grant at once and notifies them in the panel. No response says whether a username or email exists.
- `panel_grant` has a unique key on user, scope type and scope id, so two concurrent invites cannot create two grants, and a version for optimistic updates.

The editor enforces a strict no-escalation rule. A caller may add or remove only permissions they hold at that scope, and permissions they lack stay untouched on the target. A caller cannot edit or remove a user whose effective set at that scope is not a strict subset of their own, and nobody edits themselves. Updates are add and remove lists with `If-Match` on the grant version. Every change is audited with the old and new sets and takes effect on open sessions within one second.

The UI renders the catalog by group with descriptions and danger badges. It disables what the editor cannot grant, with a note that only permissions you hold can be granted, limits each group's select-all to the keys the editor can grant, and shows a review step before saving.

### API keys

Personal API keys (17.36) are the single key type for automation:

- The format is `amb_` plus a 12-character public id plus a 32-byte secret in base32. Only a hash of the secret is stored, it is compared in constant time, and the full key is shown once.
- Fields: name, permission subset, scopes, allowed CIDRs (IPv4 and IPv6, trimmed, blank lines ignored), expiry (90 days by default, and no expiry only when owner policy allows it), last used time and address (written at most once a minute), and created and revoked times.
- At request time a key's rights are its subset intersected with its owner's current permissions, so demoting the owner shrinks every key at once.
- A user holds 25 keys by default, counted inside a transaction. Rotating creates a replacement with the same scopes and an optional short grace for the old key. Requests from a blocked address are audited with the key id. Rate limits are keyed by key and user.
- Keys never work on an app's own admin API. The per-app tokens from 17.02 and the node certificates from 17.22 remain the machine credentials.

### From Pterodactyl

- **Keeps:** a catalog of grouped permissions with descriptions served to the UI; per-scope grants in the style of sub-users; 404 for strangers and 403 for members without a permission; a central check that every object in the path belongs to the scope; grant editing limited to permissions the editor holds, with self-editing refused; scheduled tasks requiring their action's permission; secret fields gated by their own permission, as database passwords are; revocation on permission changes; IP allow lists and one-time display for keys.
- **Changes:** a scope tree instead of one server scope; roles and custom roles instead of one global admin flag; no wildcard permission, so a new permission is never granted implicitly; a strict hierarchy, so a lower operator cannot remove a higher one; add and remove lists instead of replacing the whole set, which fixes an editor being unable to save a user holding permissions the editor lacks; a unique grant key that closes the duplicate invite race; route registration that fails when no permission is declared, so a route can never ship open by omission; running a schedule now also needs its tasks' permissions; keys are hashed, scoped, expiring, and shrink with their owner; revocation closes only the affected user's sockets; invites are links, not instant accounts with a reset email.
- **Drops:** the separate, deprecated application key type and its per-resource read and write columns; showing a key again to its owner; server ownership as the access model; hiding admin activity from members, since every action must stay visible in the audit log; per-server throttles shared by every user of that server.

## Activity and audit log

### Store

All panel activity lives in the supervisor's store: the `audit_event` and `audit_subject` tables come with the listener in 17.14, so the first milestone that must record something has somewhere to record it, the `AuditScope` that writes into them is 17.49, and the pages are 17.25. App-local rows, such as `setting_audit` from 4.16, the 17.05 command audit and in-game GM command logs, stay in their own databases and are linked by id.

- `audit_event`: id, `event_uuid` for idempotent forwarding from nodes, `batch_uuid`, time, event name, actor type (`panel_user`, `api_key`, `console`, `schedule`, `node`, `system`, `game_gm`), actor id, API key id, address, user agent, node id, result (`ok`, `denied`, `failed`) with error text, reason, and properties as JSON.
- `audit_subject`: event id, subject type (`panel_user`, `app`, `realm`, `node`, `game_account`, `character`, `backup`, `schedule`, `setting_key`, `file`, `client_revision`, `reload_target`, `database_host`, `allocation`) and subject id. One event may have several subjects.

### Recording

- An `AuditScope` bound to each request sets the default actor, subjects, API key and batch.
- A change the panel makes itself writes its audit row in the same SQLite transaction as the change, so the row exists exactly when the change committed. A relayed app action writes a requested row and then its result.
- When writing the audit row fails, security-relevant actions fail closed (settings, bans, grants, restores, credential rotation) and read-only events fail open with an error line.
- Refused and failed attempts are recorded, not only successes.
- Passwords, keys, TOTP codes and the arguments of sensitive console commands are never recorded. File writes record the path and the SHA-256 before and after.
- Event names follow `namespace:path.action`. Every name the code emits has a sentence in the catalog the UI renders, checked by a unit test that fails when an emitted event has no sentence.

Namespaces: `auth` (sign-in, lockout, second factor, recovery code, sign-out, session revoke, API key blocked), `user` (password, email, two-factor, recovery codes, API keys), `panel` (users, roles, grants, invites, settings), `app` (console command, power, crash, launch settings), `settings` (change, batch, revert), `reload`, `realm` (edit, maintenance), `account`, `ban`, `character`, `player` (kick, mute, teleport), `announce`, `event`, `world` (edit, export), `backup`, `restore`, `schedule`, `file`, `update`, `node`, `network`, `database`, `clientdata`.

### Pages

- An activity tab per panel user showing events where the user is actor or subject, per app, realm, node, game account and character, and a global page for `activity.read` at panel scope.
- Filters by event prefix, actor, subject, result, time range and address; cursor pagination up to 100 rows; live rows from the event socket once 17.57 lands; CSV and JSON export with `activity.export`, where a field beginning with an equals sign, a plus, a minus or an at sign is written so a spreadsheet cannot read it as a formula.
- Each row shows the actor, a rendered sentence with escaped values, whether an API key or a schedule acted, the address when visible, relative time with the absolute time on hover, and a details view for extra properties.
- Retention is a setting per event class: 365 days for security events and 90 days for high-volume events such as file reads and console commands, with pinned rows kept.
- Nodes buffer events locally while the panel link is down and forward them in batches, deduplicated by `event_uuid`. High-volume file events may be merged per actor, subject and minute.
- A tamper-evident hash chain, where each row stores the hash of the one before it with a verify command, is planned, not yet scheduled, as an opt-in.

### From Pterodactyl

- **Keeps:** actor and many subjects per event; the log-with-the-change transaction; batches; namespaced event names rendered through a translation catalog with escaped values; address visibility limited to the actor and admins; a details view; pruning by age; nodes batching activity to the panel.
- **Changes:** admin actions, grant changes, key creation and revocation and permission changes are all recorded, so an admin action is as visible as a member's; refused attempts are recorded; a user's activity includes what they did, not only what was done to them; forwarding is idempotent by event id instead of at-least-once with duplicates; a test ties every emitted event to its sentence, which prevents events that render as raw keys; security-relevant writes fail closed.
- **Drops:** storing the typed sign-in identifier; the option to hide admin activity from members; the unused legacy audit table.

## Event socket

The panel has one WebSocket, `/api/panel/events`, for console output, status, stats, progress of long operations, audit rows and alerts. Its envelope, tickets and generated types are 17.26, its subscriptions and live permission filtering 17.57, and its limits, supervisor fan-out and the move of the existing live pages onto it 17.58. The protocol itself, with its message types and close codes, is a proposal listed under Decisions needed in doc/ROADMAP.md. Each app keeps its own `/api/logs` and `/api/events` sockets (17.04, 17.12), which only the supervisor subscribes to; both sit on the one stream layer with sequence numbers, a bounded backlog and resume that 17.04 builds, so no milestone writes a second copy of it.

### Connection

- The upgrade is authenticated by the session cookie and requires an exact `Origin` match with the panel's configured origin; a wildcard origin is never allowed. Scripts using an API key first `POST /api/panel/events/ticket`, which returns a random ticket valid for 30 seconds and one use, bound to the key, the client address and the scopes, sent in the first frame, never in the URL.
- The first client frame is `hello` with the protocol version and the CSRF token. The server answers `ready` or closes.
- Each socket holds its user, session and permission generation. Every inbound message and every outbound stream class is checked against the current permissions, so a change applies to the next frame, not after a token expires.
- The server sends `ping` every 20 seconds and closes a socket silent for 60 seconds. `session.expiring` arrives 60 seconds before idle or absolute expiry so the page can renew over HTTP.

Close codes:

| Code | Meaning |
|---|---|
| 4400 | Malformed frame or unsupported protocol version |
| 4401 | Session ended: sign-out, expiry, or a revoked session |
| 4403 | Access lost: the user's permissions no longer allow any subscribed stream |
| 4429 | Closed for exceeding limits repeatedly |

### Envelope

Every frame in both directions is one JSON object:

```json
{"v": 1, "type": "log", "id": "c42", "scope": {"app": "gameserver-1"}, "seq": 90211, "time": 1789650000123, "data": {}}
```

- `v` is the protocol version. Fields are only ever added; a test fails when a field is renamed or removed, as 17.03 does for the status API.
- `type` names the event. `id` is a client request id that the answer echoes. `scope` names the app, realm, node or panel. `seq` is a sequence number per stream, used to resume. `time` is Unix milliseconds. `data` is structured JSON, never JSON encoded inside a string.
- The TypeScript types are generated from the schemas the C++ side serializes. A contract test fails when the server can send a type the client has no handler for.

### Client to server

| Type | Data | Needs |
|---|---|---|
| `hello` | protocol version, CSRF token or ticket | signed in |
| `subscribe` | scopes, streams (`logs`, `status`, `stats`, `players`, `setup`, `backups`, `updates`, `settings`, `reloads`, `schedules`, `nodes`, `alerts`, `audit`, `files`), log minimum level and categories | each stream's read permission |
| `unsubscribe` | scopes, streams | none |
| `resume` | scope, stream, last `seq` seen | the stream's permission |
| `backlog` | scope, count, before `seq`, run (`current` or `previous`) | `console.read` |
| `command` | app, line, confirm flag | `console.write` at the command's level |
| `power` | target (app, realm or stack), action (`start`, `stop`, `restart`, `kill`), countdown seconds, reason | `power.<action>` |
| `stats.now` | scope | `metrics.read` |
| `ping` | none | none |

### Server to client

| Type | Data | Sent to holders of |
|---|---|---|
| `ready` | protocol version, server time, permission snapshot per scope, apps and realms | the socket's user |
| `status` | app, state, since, process id, exit code, exit reason, crash count, next restart time | `status.read` |
| `stats` | scope, sample (CPU, memory, handles, threads, network, disk, sessions, players, tick average and maximum) | `metrics.read` |
| `log` | app, `seq`, time, level, category, source (`app`, `stdout`, `stderr`, `supervisor`), message | `console.read`; Account and Login categories also need `activity.ip.read` |
| `dropped` | stream, count, first and last `seq` missed | the stream's holders |
| `command.result` | request id, success, output lines, audit id | the sender |
| `power.accepted`, `power.progress`, `power.result` | request id, operation id, step, outcome, reason, lock holder | `status.read` in scope; the sender always |
| `players` | realm, online count, at character select, per zone counts, joins and leaves | `players.read` |
| `setup.output`, `setup.state` | job id, line or state for automatic setup, typeextract and client data rebuilds | `clientdata.read` |
| `backup.progress`, `backup.completed`, `backup.failed` | backup uuid, phase, bytes, status, error | `backups.read` |
| `restore.progress`, `restore.completed`, `restore.failed` | restore id, phase, status, error | `backups.read` |
| `update.progress`, `update.result` | update id, step, build, outcome | `updates.read` |
| `setting.changed` | scope, key, old and new value (masked when secret), who, why, generation | `settings.read` |
| `reload.result` | scope, target, generation, result, errors | `reload.read` |
| `schedule.run.started`, `schedule.run.waiting`, `schedule.countdown.tick`, `schedule.task.started`, `schedule.task.output`, `schedule.task.finished`, `schedule.run.finished`, `schedule.changed` | schedule id, run id, task position, status, output chunk, next run | `schedules.read` |
| `realm.changed` | realm, flags, player limit, maintenance state | `realms.read` |
| `announce.sent` | scope, message id, channels | `players.read` |
| `file.job` | job id, kind, progress, result | `files.list` |
| `node.status`, `node.move` | node, state, heartbeat, move progress | `nodes.read` |
| `alert` | rule, severity, subject, state | `alerts.read` |
| `audit` | a rendered audit row | `activity.read` in the row's scope |
| `permissions.changed` | new permission snapshot | the affected user |
| `throttled` | limited message type, retry after | the sender |
| `error` | request id, code, message, correlation id | the sender |
| `session.expiring` | seconds left | the socket's user |
| `pong` | none | the sender |

Error text is full only for holders of `debug.errors`. Everyone else gets a generic message with a correlation id that also appears in the supervisor log and the audit row.

Later milestones add streams the same way, each with its read permission and its close codes documented here: alerts and notifications in 17.67, schedule run progress in 17.68, and a realm's queue length and oldest wait inside the stats sample in 17.71.

### Limits

Limits are per connection and per user (17.58), live settings applied from the next message:

- commands: a burst of 10, then 2 a second;
- power: a burst of 3, then 1 every 5 seconds;
- backlog and resume: a burst of 2, then 1 every 5 seconds;
- other messages: 20 a second;
- frames up to 64 KiB, sockets per user and per scope capped.

Each connection handles its inbound messages in order on one queue, so two commands never reach an app out of order. Every refusal answers `throttled` or `error`; nothing is dropped silently.

### Output pipeline

- The primary log source is each app's structured `/api/logs` stream. The supervisor subscribes to it once per app and fans out to every panel socket, so ten browsers following one app cost that app one subscriber (17.58). The fallback is the supervisor's own capture of standard output and error, split into lines up to 64 KiB, which covers output before the admin API is up, the tail after a crash, and apps with `Admin.Enable = 0`. Records already seen on the structured stream are not repeated.
- The supervisor keeps a ring of the last 1000 records per app for the current run and the previous run, including after the app exits, and saves the last 200 lines with each crash record.
- Each subscriber has a bounded queue that drops its oldest records and sends `dropped` with the missed range. Status and power events never drop; stats keep only the newest sample.
- A per-app rate guard coalesces floods into one record saying how many lines were suppressed. Suppressed lines still reach the log file, and the app is never stopped for output volume.
- On reconnect a page sends `resume` with its last `seq` and receives the gap or a `dropped` marker, instead of clearing the screen and replaying a fixed number of lines.

### From Pterodactyl

- **Keeps:** one socket per page for console, status and stats; permission checks on every inbound and outbound event; admin-only streams; error text gated by permission with an error id matching the server log; per-event token buckets and a message size cap; a warning before credentials expire; an Origin check against cross-site socket hijacking; a backlog on connect.
- **Changes:** permissions are evaluated live per message instead of frozen for 10 minutes in a signed token; the socket authenticates with the session cookie on the panel's own origin instead of a token sent to another host; revocation closes only the affected user's sockets; structured typed payloads replace JSON inside strings; sequence numbers and resume replace clear-and-replay; limits are per user as well as per connection, and messages are handled in order; every refusal answers; keepalive pings detect half-open sockets; generated types and a contract test prevent event name mismatches between server and UI.
- **Drops:** HS256 tokens signed with the node secret, the token refresh cycle, the per-server token fetch limit shared by all users, the boot-time token cutoff, the 30-socket cap shared by a whole server, install output and transfer log events in their Docker form, and a wildcard allowed origin.

## Console

The console page (17.07, moved onto the event socket by 17.58) shows each app's log in a tab with level and category filters, text search, pause with a count of new lines, copy, a jump-to-newest control, and a view of the previous run. The command box sits under it when the user holds `console.write`, and notes that commands are audited.

- Commands go through CommandMgr on the app's admin API (17.05), return their output lines as `command.result` tied to the request, and show inline under the command.
- Destructive commands (shutdown, account delete, ban, realm close) need the confirm flag, and the page asks before sending one.
- A command sent while the app is starting or stopped is refused with the reason, not dropped. A command payload is capped at 4096 bytes and a body with an unknown key is refused, so a newer page cannot smuggle a field an older app ignores (17.05).
- History is kept per user and app in the supervisor's store (17.49), so it follows the operator across devices and the browser keeps nothing beyond the open page's recall. Tab completion offers only the commands the user may run at their level.
- `console.raw`, owner-only by default, writes a line to an app's standard input when its admin API is down (17.58). It is audited like any command and refused while the app is in a protected state.

### From Pterodactyl

- **Keeps:** a terminal-style log view with search, a jump-to-newest helper, a command box shown only with the command permission, arrow-key history, and a banner while reconnecting.
- **Changes:** commands run through CommandMgr with a result instead of raw standard input with no reply; history is stored per user on the server instead of in one browser; level and category filters and pause; the previous run's output stays readable after a crash.
- **Drops:** feature pop-ups that match English substrings in console output, replaced by structured problem records (see Power and supervision).

## Power and supervision

### States

Each app has one state: `offline`, `starting`, `running`, `stopping`, `crashed`, `backoff`, `crash_loop` or `disabled`. Operations that must not overlap with power actions hold the app in a protected state: `setup` (3.22 setup, typeextract, a 17.20 client data rebuild), `updating` (17.17), `restoring` (17.51) or `moving` (17.42). The supervisor persists each app's desired state when it changes.

- `starting` becomes `running` only when `GET /api/health` reports lifecycle ready: listener bound, databases open, client data loaded, and for a gameserver its realm registered with the login server once 4.03 lands. A `ready` lifecycle line on standard output is the fallback when the admin API is off. A start timeout marks a failed start.
- A stop the supervisor requested is known from its own request, and the app reports `stopping` before exiting, so no output matching is needed to tell a requested exit from a crash.
- An admin can disable an app, which stops it and refuses every start until it is enabled again (17.27).

### Operations and locks

Power actions (17.27) target one app, one realm (its gameservers), or the whole stack.

- One lock per app and one stack lock. A power action that finds the lock held, or the app in a protected state, is refused with 409 naming the holder, its action and its start time. `kill` may bypass a held lock, and first marks the app stopping.
- The HTTP route returns 202 with an operation id. `power.progress` and `power.result` report each step on the socket, and the result lands in the audit log, so a failed start is never hidden behind an accepted response.
- The stack starts in dependency order: the private database when 17.24 runs one, then the loginserver and patchserver, then gameservers. It stops in reverse, gameservers first.
- Pre-start checks (17.59) stream as supervisor records: the binary exists and `--check` passes, config parses, databases answer with no pending schema update the binary needs, the client install and type dump are present (otherwise 3.22 setup runs as the protected `setup` state), ports are free, and disk space is enough.
- A graceful stop tries, in order: the admin API's shutdown with a countdown, `POST /api/shutdown` with `seconds` from 0 to 86400, which `cancel` withdraws, and which on the login server sends the 2.15 shutdown notice within `Login.ShutdownGrace` and on the gameserver saves characters and drains; `shutdown` on standard input when the admin API is unreachable; Ctrl+Break to the child's process group on Windows or SIGTERM on POSIX; and, after a per-app stop timeout above the app's own grace, ending the process tree through its job object or process group.
- A restart offers now or with a countdown (17.59). Its confirmation shows the effect on players from the status API: players in world on that realm, sessions at character select, active patch downloads.
- When a live setting or reload would make a restart unnecessary, the page says so, using the restart-required list from 17.10.

### Crashes

- Every exit is classified (17.60): requested, clean but unexpected (exit 0 without a request, a crash by default and configurable), crash (a nonzero code, a Windows exception code shown by name such as ACCESS_VIOLATION, or a POSIX signal name), out of memory (a job memory limit on Windows, cgroup memory events on Linux), and startup failure (exited before ready).
- Restart uses exponential backoff from 1 second up to 5 minutes, and the counter resets after 10 healthy minutes. K crashes within a window make a crash loop: restarting stops, one alert goes out (17.67), and a manual start is needed. A startup failure caused by config or client data is retried once.
- Each crash is stored with time, exit code or signal, uptime, the last 200 log lines and any minidump path. The overview counts crashes per app.
- On supervisor start, apps still running are re-adopted after checking their process id, process start time and executable path, so a reused process id is never mistaken for an app, and their logs reattach. Apps whose desired state is running and are not running are started.
- Backoff, windows, timeouts and exit classification are live settings.

### Error reports

Errors are reported from the code that raised them, not matched out of console text (17.106). Every log record carries the repository-relative source file, line and function of the call that wrote it and the message's format template, and the live log stream carries both. Each app groups what it logs at error level and above by category, location and template with a count, first and last time and build revision, and the supervisor keeps those groups in its store across restarts.

- The Error reports page lists the groups by app, category, location and time, and each opens its file and line, template, count, the times and the log lines from before its latest occurrence.
- Create report writes a versioned file with the product version, commit and branch, the operating system, each app's revision and the chosen groups. Templates hold no player data, so they are always in the file; rendered messages and surrounding log lines join only when the operator ticks them after a preview of exactly what the file will hold.
- No secret, account verifier, client file or player identity is in a report unless the operator included rendered text after that preview. The page downloads the file and links to the repository's issue form for sending it; nothing is sent from the machine on its own, and every report is audited.

### Stale state after a crash

A gameserver that dies leaves rows behind that a player feels. 4.07 writes `characters.online`, `account.online`, `realm_online_character` and a realm's `login_key` rows at world entry, and until they are cleared the character stays locked out, which is the crash-safety item doc/ROADMAP.md's review list names. The supervisor is what notices the crash, so it owns the repair (17.61):

- A reconciliation pass runs when an app exits without a clean shutdown and at every supervisor start. It is also a console command and a panel button for an app the supervisor did not start, refused while that app is running.
- The pass is scoped to the crashed app's realm, so one realm's crash never clears another realm's players, and it also clears `account_session` rows whose realm is gone.
- It is fenced by run generation: the gameserver stamps its generation on the rows it writes, and a pass clears only rows from a generation older than the app's current run, so a restarted app already serving players loses nothing.
- Each pass reports the rows cleared per table with the realm and the crash record it followed, on the app's page and in the audit log, and publishes the condition 17.67 alerts on, because a locked character is player-visible.

### Problem records

Apps report structured problems with a code and a fix-it action instead of the panel matching console text: install not found, type dump stale, database unreachable, pending schema update, port in use, revision not allowed by `Login.AllowedRevision`, disk low. The overview and the app page show them with a button that opens the matching page: client data, rerun setup, apply updates, network, or the settings key.

### Launch settings

Each app has launch settings in the store (17.28), edited with `launch.edit` and applied at its next start: the build it runs (see Updates), extra command-line overrides, which are the settled command-line override layer and so lock their keys, shown as locked on the settings page with that layer named, environment variables whose secret values are sealed with AES-256-GCM under the supervisor's key, as Store describes, and masked everywhere they are shown, the working folder, start timeout, stop timeout, restart policy (always, on crash, never), backoff and crash-loop limits, autostart with the supervisor, process priority, and opt-in hard limits on memory and CPU through a job object or a cgroup. Every change shows a preview of the exact command line with secret values masked. Launch settings never change a setting the app itself owns; game options stay in the settings page.

### Port allocations

The network page (17.29) lists allocations per node: bind address, port, protocol (TCP, UDP, HTTP), public alias players connect to, the app and role using it (login listener, game listener, patch HTTP, admin API, metrics), and notes. Allocations are seeded on first run from the shipped defaults (login 12000, game 12333, patch 12500, admin API). Adding one checks that the port is actually free by binding it. IPv6 works, ports below 1025 are allowed with a warning about privileges, a range is capped at 1000 ports, and a unique key on node, address, port and protocol prevents duplicates. Assigning a port to an app writes the app's listen setting through the settings API, which rebinds live and keeps the old listener when the bind fails, and updates the realm list address once 4.03 lands. Deleting an assigned allocation is refused on every path. A port pool per node supplies ports for new realms.

### From Pterodactyl

- **Keeps:** the four core states; a per-app power lock that kill may bypass; protected states that refuse power actions during install, restore and transfer; mapping each action to its own permission; a crash counted only when no stop was requested; a limit on how often crashes restart; restoring states when the daemon starts without stopping apps that kept running; a pre-start step that checks config and disk; allocations with aliases and notes, seeded and assigned per server, never deleted while assigned.
- **Changes:** readiness comes from the typed health API instead of matching a done string in console output; a 202 carries an operation id with progress and a result instead of hiding failures; refusals name the lock holder; the graceful stop runs the app's own shutdown with player notices before any signal; backoff, crash loops and crash history with log tails replace one restart per 60 seconds with no record; desired state is saved on change instead of once a minute; process identity is verified when re-adopting; allocations support IPv6 and check the port is free; deleting an assigned allocation is refused on every path, a bulk delete included.
- **Drops:** Docker container recreation on each start, the egg stop string and signal names, egg config file rewriting before start, stopping a server that exceeds a disk quota (a low-disk alert replaces it), billing suspension (a disabled app and realm maintenance replace it), and client-created allocations.

## Schedules

### Engine

Schedules (17.15) run in the supervisor with no cron daemon and no queue. `ScheduleMgr` keeps a min-heap of due times and one steady timer for the earliest; it re-arms when a schedule changes and when a wall-clock check every 30 seconds finds a jump of two minutes or more. Each run is a coroutine whose state moves through store rows committed before and after every task, so a crash always leaves a truthful record. A schedule with no tasks is never armed. Cron parsing and next-run math live in `src/common/Time/` with unit tests that open no sockets.

### Data

- `schedule`: id, name, description, scope, trigger (cron, one time, manual only), cron expression, one-time instant, IANA time zone, state (active, paused, archived), overlap policy (skip, or queue one), misfire policy (skip, or run once within grace) with grace seconds, conditions, next run, run-as user, created and updated by, version.
- `schedule_task`: schedule id, position (unique within the schedule, rewritten in one transaction by a reorder), action, target app, parameters validated against the action's schema, timing (after the previous task, or from the anchor time), offset seconds (negative only from the anchor), completion (accepted, or ready), timeout, failure policy (stop, continue, retry N times with a delay), always-run flag for cleanup steps.
- `schedule_run`: trigger (cron, catch-up, manual, API), who, anchor time, start and finish, status (`pending`, `waiting_condition`, `countdown`, `running`, `succeeded`, `partial`, `failed`, `skipped`, `cancelled`, `interrupted`), skip reason (`paused`, `overlap`, `missed_beyond_grace`, `target_not_running`, `players_online`, `quiet_timeout`, `permission_revoked`, `target_missing`), and a snapshot of the tasks as they ran.
- `schedule_task_run`: run id, position, attempt, due time, start and finish, status, error code and text, capped output, and links to the audit row, backup, setting audit row, reload generation or update.

A schedule carries a version, saves take `If-Match`, and a stale save answers 409 with the current version, so two operators editing one schedule cannot silently overwrite each other. A reorder sends the same version.

### Timing

- One expression string with the five cron fields, lists, ranges, steps, month and weekday names, 0 or 7 for Sunday, and the `@hourly`, `@daily`, `@weekly`, `@monthly` and `@yearly` macros. When both day fields are restricted a day matches either, and the editor says so. Parsing returns every error with its field and position as a 422, and an expression that can never fire is refused.
- Next runs are computed in the schedule's own time zone through C++20 time zones. A fixed-time run whose local time falls in a spring-forward gap runs once at the first instant after the gap; in a fall-back repeat it runs once at the earlier occurrence; a wildcard or stepped hour follows real elapsed time. Next run is computed from the later of now and the last anchor, so a backward clock step never fires a slot twice.
- Time zone data reloads with the supervisor's reload, which recomputes every next run. A schedule whose zone the new data no longer holds is held with an error instead of firing at the wrong local time. Where a platform's C++20 library ships no data, the source is a proposal in doc/ROADMAP.md.
- On start or after a clock jump, a missed run within its grace runs once as catch-up; otherwise a skipped run is recorded and the next slot is used. Runs left in progress by a crash are marked interrupted with an alert, and power, backup and update steps are never repeated automatically.
- With the `from_anchor` timing, "warn at 10, 5 and 1 minutes and restart at 04:00" fires the restart exactly at 04:00, and the run starts at the earliest offset.

### Tasks and completion

A task completes when its work is really done: a power task when the app reports healthy or has exited cleanly within the timeout (`ready`), or when the supervisor started the operation (`accepted`); a command when CommandMgr returns its lines; a backup when the archive is verified; an update when its health wait passes; a reload when the new generation or its errors come back. The failure policy covers every error type. Editing a schedule during a run changes only the next run.

Actions offered are those the target's running build advertises through its capability list and the user may perform: announce, restart with countdown (a macro that expands into timed announcements and a restart, with a cancel message), power, command (one CommandMgr line, no newlines, at most 4096 bytes, destructive commands confirmed at save time, commands with sensitive arguments refused), backup, update, reload, set a setting with a reason, set a realm flag, client data rebuild or apply, database data update, wait until a condition holds, notify through alert channels, export the world edit journal, and rolling restart of realms one at a time (17.44).

### Conditions

- `require_target_running` per task; crashed or backoff targets skip with the reason recorded.
- A target in a protected state makes the task wait for its lock up to the task's lock wait, or skip.
- Player-aware conditions (17.44): skip when no players are online, and run only when empty, waiting for a quiet moment up to a limit and then running or skipping as chosen.
- Conditions are checked at run start and again before each task that sets its own guard. Every skip is a recorded run with its reason.

### Run now, pause, cancel

Run now returns 202 with a run id and never runs a task inside the HTTP request. It refuses with 409 while a run is active unless the overlap policy queues one, and needs `schedules.run` plus every task's action permission. Pause lets the current run finish; cancel is separate, marks remaining tasks cancelled, still runs always-run tasks, and sends a countdown's cancel announcement. Resume recomputes the next run and applies the misfire policy. At automatic run time the engine checks that the run-as user still holds the tasks' permissions; if not, the run is skipped as `permission_revoked`, an alert fires, and owners and admins can adopt the schedule.

### Countdown notices

On the gameserver, countdown announcements go through the zone broadcast from 6.01 and GM system messages from 6.04. On the loginserver, MSG_LOGINSERVERSHUTDOWN closes the connection as it is shown, as 2.15 confirmed with the retail client, so it can serve only as the final notice. Earlier login-screen warnings need a notice message that does not disconnect, found by capture or client reverse engineering, which is a proposal in doc/ROADMAP.md. Until one is found, 17.15 sends only the final notice on the login server and earlier warnings go to players in the world.

### Page

The schedules page (17.68) lists name, targets, a plain-language trigger ("every day at 04:00 America/Chicago"), next run in the schedule's zone and the viewer's, last run status linked to the run, state, and quick actions hidden without permission. The editor has trigger presets or a custom expression with field errors and a preview of the next five runs with daylight saving notes, a time zone picker that warns when it differs from the node's, conditions, tasks with drag-and-drop and keyboard reordering, typed parameter editors, a timeline of every task relative to the anchor, failure policies, and a review step with a diff. Run history shows trigger, who, nominal and actual start, duration and status; the run page shows each task live with its output, errors and links, with cancel and run again.

### From Pterodactyl

- **Keeps:** an ordered task chain with per-task offsets; continue on failure; run now; only when online; each task's action requiring its own permission; ownership checks that answer 404 for another target's ids; a cron cheatsheet in the editor.
- **Changes:** a time zone per schedule with defined daylight saving behavior instead of one panel-wide zone; run history with task results and output instead of only last and next run; interrupted runs are marked at start, so no run is left showing as in progress; skipped runs are recorded, never stamped as run; an overlap policy instead of back-to-back catch-up; completion waits for real results instead of an accepted response; retry on any error type, not only connection errors; run now needs every task's permission, so schedule rights never become console or power rights; an automatic run checks that its author still holds those rights; offsets up to a day; reordering in the UI; every field error named.
- **Drops:** the minute cron runner, Redis queue jobs and their implicit retries, the 900-second offset cap and per-server creation throttles, the MySQL time zone offset workaround, and multi-line command payloads that become several console lines.

## Backups and restore

### What a backup holds

A backup (17.16) covers one installation, the supervisor or one node, because the login and characters databases are shared by every realm. It lists named components: `db:login`, `db:characters`, `db:world`, a database per module when a module adds one, `config` (each app's `.conf` and `conf.d`), `data` (the Ambrose data folder), `types` (type dumps), `panel` (the supervisor store), and optionally `logs`, `audit` and `patch` (off by default, since they can be rebuilt from the user's install). Each component has include and exclude patterns. Restore groups the panel enforces: player data (login and characters together), content (world with its live edit journal), config, client data, and the panel store, which is never restored implicitly.

The `types` component, and the extracted data inside `data`, are built from the operator's own client install, so the bring-your-own-files rule under Decisions, Experimental features applies to every archive: the contents list names each component and its byte count, no component carries a client file the operator did not place in a backed-up folder, and off-machine storage (17.43) is a bucket the operator controls, never a public host. An archive also holds `login.account.verifier` rows and the config that holds `Account.VerifierKeys`, so it is as sensitive as a password file: file permissions, a step-up download ticket and `Backups.Encrypt` (17.72), off by default, which seals the archive with AES-256-GCM under a key in the supervisor's keyring, as Store describes. Whether sealing becomes the default rather than opt-in, and whether dumps become structured rows through prepared inserts rather than SQL text, are proposals listed under Decisions needed in doc/ROADMAP.md.

### Records

- `backup`: uuid, name, kind (manual, scheduled, safety, pre-update, pre-move, imported), schedule id, created by, node, components and patterns, storage target and object key stored per row, status (`queued`, `preparing`, `dumping`, `archiving`, `uploading`, `verifying`, `succeeded`, `failed`, `interrupted`, `cancelled`, `deleting`), phase, progress bytes, error, pin with who and why, system pin, archive and manifest SHA-256, compressed and raw bytes, file count, row counts per table in the manifest, the last applied update per database, client revision, client program and type dump SHA-256, Ambrose version and commit, settings generation per app, start, finish, duration, last verify time and result, soft delete time.
- `storage_target`, `backup_upload` and `backup_upload_part` for resumable S3-compatible uploads (17.43), and `restore_job` with its components, folder mode, safety backup, status, phase, who, apps stopped, error and times.

### Taking a backup

1. Preflight: estimated free space from the last backup of the same components, databases reachable, nothing else running. One backup or restore runs at a time per installation, and others queue.
2. Ask each gameserver to flush its write-behind queue and save online characters, and record the saved generation, so no player is kicked.
3. Dump each database server from one read-only `REPEATABLE READ` transaction with a consistent snapshot, streaming rows in primary-key order, with the `updates` table included. The same transaction writes a snapshot record of each table's row count and row checksum, so a restore can be checked against the moment the dump began (17.16, compared table by table in 17.51). Databases on different servers record separate snapshot times, and the page warns. Non-InnoDB tables are reported.
4. Walk folders without following links out of their roots, skipping sockets and devices, recording empty folders, and reading again any file whose size or modification time changed during the copy.
5. Write a tar stream through multithreaded zstd to a `.partial` file with the manifest first and a trailer of every entry's SHA-256, flush, and rename into place. Files are readable only by the service user. A write-rate limit and background I/O priority apply, and the job slows itself when gameserver tick time rises.
6. Verify, mark succeeded, then apply retention, so retention removes an old backup only after the new one is good.

Retention, pins and audited downloads are 17.52. Retention keeps a count and an age, and pinned backups are exempt. Failed and interrupted backups lose their pins and can always be deleted. System pins protect the newest verified backup, a running restore's safety backup, and a pre-update backup during 17.17's rollback window. A creation throttle per user counts deleted backups too.

### Restore

Restore is 17.51. `POST /api/panel/backups/{uuid}/restore` takes components, folder mode (replace or merge), countdown seconds, a reason, and the installation name typed back.

Gates: the backup succeeded; nothing else runs; the manifest format is supported; the backup's applied updates are a prefix of the running build's list, so a backup from a newer build is refused; a warning when the client revision differs; and every verifier key id in restored login rows is still listed in `Account.VerifierKeys`. Restoring player data needs `backups.restore.players`.

Sequence:

1. Save the restore job and enter the `restoring` state, persisted so it survives a crash.
2. Verify every SHA-256 and the manifest before changing anything, naming a failing file.
3. Take a pinned safety backup of the affected components.
4. Load databases into staging schemas, compare row counts with the manifest, and run the updater on staging.
5. Warn players with the countdown, then save and drain them.
6. Stop only the affected apps, gracefully and then by force after a timeout.
7. Swap each database with one atomic `RENAME TABLE` statement, keeping the old tables under a restore suffix, and swap or merge folders.
8. Fix up: raise `id_sequences` to the larger of restored and current high-water marks so no guid is reused; clear `account_session` and the online flags, the same rows 17.61 repairs after a crash; bump the settings generation; rerun client data extraction when the install's revision differs from the backup's; keep host-specific keys such as `ClientDir`, `TypeDumpPath`, bind addresses, TLS paths and admin tokens unless the operator opts in.
9. Start the apps, wait for health, finish the job, and drop the kept tables after a grace period. The report compares every restored table with the 17.16 snapshot record and lists each row count and checksum that differs.

A failure before the swap changes nothing; a failure after it rolls back from the kept tables, falling back to the safety backup. While restoring, power actions, settings edits, reloads, file writes, schedule runs (deferred and recorded), other backups and updates are refused, while read-only pages and live restore progress stay available. A content-only restore of the world database can swap live and reload every world manager through 4.15, keeping players online.

### Downloads and catalog

Downloads (17.52) use `POST /api/panel/backups/{uuid}/download`, which needs a recent step-up check and returns a one-time ticket stored hashed and bound to user, session and backup, valid for minutes and revoked with the session. The download serves Range requests, refuses backups that have not succeeded, can serve a single component, and is audited with the address. A scan action lists the local folder and the storage prefix, matches entries by uuid, imports unknown archives after verifying their manifests, and offers to delete orphans (17.43).

### Page

The backups page lists name, kind, status with the current phase and progress, component chips, sizes, duration, storage location and upload state, who, client revision and Ambrose version, pin with reason, and when retention will remove it, with storage usage against free space. Its restore wizard shows the update level difference, client revision mismatch, players online with the countdown choice, the forced safety backup, and the typed confirmation.

### From Pterodactyl

- **Keeps:** locking, as pinning, with locked backups exempt from rotation and failed ones always deletable; a throttle that counts deleted backups; a checksum and size recorded per backup; single-use, short-lived download links; restore refused unless the backup succeeded and completed; the node owning a backup being the only one to report it; S3-compatible storage with credentials kept by the panel and a check of the storage endpoint's address.
- **Changes:** consistent database dumps with a flush of online characters instead of copying live files; a status enum instead of inferring state from two columns; the storage target stored per backup instead of looked up from the current default; SHA-256 instead of SHA-1; every checksum verified before a restore changes anything; staging and atomic swaps with automatic rollback, so nothing is removed before the archive is verified; a safety backup, a player countdown and a restart after restore; retention after success instead of deleting the oldest before the new backup exists; resumable uploads; interrupted jobs marked at start instead of a six-hour prune; a catalog scan for orphans; download links with Range support; pin as its own permission.
- **Drops:** per-server backup limits as a hosting quota, a root ignore file, tar.gz, Wings callbacks and daemon tokens on one machine, presigned upload URLs handed to a node on one machine, and panel-supplied restore download URLs.

## File manager

### Roots

The file manager works on named roots instead of one container home. The roots, the jail and browsing are 17.18, the editor and version history 17.53, uploads, downloads and batches 17.54, trash, purge, creation and permission toggles 17.55, operator-defined roots 17.56, and archives, name search and log follow 17.39. Each root has an id, a path and a policy: whether it is writable, whether its files may be downloaded or archived, and whether it is client-derived.

| Root | Policy |
|---|---|
| install | The running build's folder. Read-only; changes go through updates |
| config | Each app's `.conf.dist`, `.conf` and `conf.d`. Writable, with schema validation |
| logs | Read, follow, truncate, rotate and trash |
| data | The Ambrose data folder. Type dumps and their lock files are read-only, lock files are hidden, and the `types` folder is client-derived |
| sql-custom | `data/sql/custom/db_<name>`, writable, where exported world edits appear |
| backups | Read-only here; download, delete and restore go through the backups page |
| client | The user's own Wizard101 installs found by `ClientLocator`. Listing and metadata only, and client-derived |
| patch-output | The patchserver's generated output, built from that install. Read-only and client-derived; the patchserver itself serves it to clients under 16.05 |

The supervisor store, its keys, admin token files, TLS keys, any SFTP host key and a private MariaDB data folder are outside every root and refused by the protected path policy. Roots reload live and are rebuilt atomically.

A root marked client-derived refuses download, archive, extraction targets, share links, SFTP and remote pull for every caller whatever their permissions, an owner included, and the refusal names the root. It refuses an editor read and a preview as well, since both hand a file's bytes to a browser, so no panel response ever carries a client file's bytes: what the panel shows about such a root is the listing and the metadata the client data page reads at runtime (17.20). That is the bring-your-own-files rule under Decisions, Experimental features: Ambrose hosts, mirrors and redistributes nothing it does not own, and a remote panel must not become a way to pull WADs off the operator's machine.

An owner can define extra roots of their own (17.56) with the same policy fields, including the client-derived flag. A root is refused when its path holds the supervisor's store, its keys, token files or TLS keys, when it overlaps an existing root, when the path is a link, or when the service user cannot open it. Grants over an extra root use the same `files.*` keys at app scope; node-scoped roots follow 17.22, and wider scopes wait for the scope tree proposal.

### Jail

Every request resolves to a root handle and relative components, and every action works through handles, never by resolving a path string twice.

- Common checks: percent-decoding happens once; NUL and control characters, absolute paths, drive letters, UNC and `\\?\` prefixes, backslashes, empty, `.` and `..` components, over-long components and excessive depth are refused. Names are normalized to NFC. Names Windows cannot hold are refused on every system, so roots move between a desktop and Docker: `< > : " | ? *`, a trailing dot or space, and reserved device names such as CON, PRN, AUX, NUL, COM0-9 and LPT0-9 with any extension. Refusing `:` also blocks NTFS alternate data streams.
- Linux: the root opens with `O_PATH | O_DIRECTORY`; each lookup uses `openat2` with `RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS`, or walks component by component with `O_NOFOLLOW` where `openat2` is missing. Files open non-blocking and must be regular files or folders before any read, so FIFOs, devices and sockets are refused. Chmod uses `fchmod` on a descriptor opened without following links. Moves use `renameat2` with `RENAME_NOREPLACE`.
- Windows: the root opens as a handle; each component opens relative to its parent with reparse points not followed, and any junction, symbolic link or mount point is refused unless policy allows one that resolves inside a root. The final handle's path is compared with the root's final path, which also collapses 8.3 short names. Share modes let running apps keep their files open, and a sharing violation reports which app holds the file.
- A regular file with more than one hard link is refused for writes and deletes unless policy allows it.
- A refused traversal answers 403 and writes an audit row with the resolved path; the response never shows the host path.

### Protected paths

A built-in policy plus operator patterns per root, in gitignore syntax, is matched against the canonical resolved path and applies to every operation: listing hides, and reading, downloading, archiving, extraction targets, SFTP and remote pull refuse, naming the rule. Built-in entries cover secrets, generated artifacts and lock files. A `.conf` line holding a secret key is shown redacted without `settings.secrets.read`.

### Space guard

Instead of per-server quotas, the panel keeps the disk that the databases, logs and backups share from filling: a minimum free space per volume (bytes and percent), optional caps per root, a reservation ledger per volume that every writer reserves against before streaming and settles on close, and usage scans that count hard links once, follow no links, and serve a stale value while refreshing. A refusal for space raises the low-disk alert.

### Operations

- **List:** paged, sorted and filtered on the server, with a name search bounded by entry count and time. Editability is decided by extension plus a NUL and UTF-8 check of the first bytes when a file opens, not by sniffing every file in a folder. An entry that cannot be read shows as an error row instead of failing the listing.
- **Read and edit:** a size cap for the editor (4 MiB by default) checked from the file's size before streaming; larger or binary files open in a read-only viewer. Every read carries an ETag of the content hash and file id, and keeps byte order marks and CRLF line endings.
- **Save:** requires `If-Match`; a mismatch answers 409 with the current ETag and the editor shows a diff. The save writes a temporary file in the same folder, flushes and renames it over the target, keeps the previous version in a per-root version store with count and age limits, and audits both hashes.
- **Config files:** saving a `.conf` file parses it against the 4.16 schema first and refuses an out-of-range value naming the setting and bound. The editor warns when a key is shadowed by a live setting or locked by the environment or command line, then offers `reload config` and shows its result.
- **Upload:** signed single-use links with the size limit enforced while streaming to a temporary file; no overwrite unless replace is chosen, which needs write rights; folder uploads.
- **Download:** refused outright for a client-derived root and for a root whose policy forbids downloads; otherwise Range support, attachment disposition with an RFC 5987 file name and `nosniff`; inline previews as plain text under a sandboxing CSP, so uploaded HTML or SVG never runs in the panel's origin. Folders and selections download as an archive streamed on the fly, with protected paths left out.
- **Rename, move, copy:** batches validated first and applied in order with a result per pair; no overwrite by default; a folder cannot move into itself; moves across volumes become copy then trash with a reservation; copies are recursive, follow no links, and create names exclusively.
- **Create (17.55):** a folder, including a nested path made in one call, and an empty file, both through the jail and refused for a protected path.
- **Delete (17.55):** to a trash folder per root on the same volume with a manifest of original path, user, time and hash, restorable and with retention by age and size. A permanent delete from the trash needs `files.purge`, and a purge-all reports how many entries and bytes it removed. The trash and the version store are left out of listings, archives, searches and free space figures. Roots, active log files (truncate or rotate instead), the running build and backup archives cannot be deleted here.
- **Archive and extract (17.39):** zip and tar.zst output named with a UTC time without colons, as a cancellable background job. Extraction normalizes entry names and resolves each through the jail; refuses link, device and FIFO entries unless a link stays in its root; caps each entry at its declared size, the total, the entry count, the depth and the compression ratio; masks modes to 0644 and 0755; extracts into staging on the same volume and moves into place with conflicts listed or resolved as chosen; refuses names that collide by case or are reserved on Windows; and reports written, skipped and refused entries.
- **Permissions (17.55):** read-only on or off, behind `files.permissions`. On Linux the mode is masked to 0644 and 0755 and on Windows the read-only attribute is set, never a free octal mode; never setuid or setgid; each toggle audited with the old and new state.
- **Log follow (17.39):** tails a log file across rotation and truncation with pause and backpressure; an app's own log prefers its structured stream.
- **Remote pull (17.41):** opt-in and off by default, needs `files.pull`. Every connection and redirect hop checks the connected address against loopback, private, link-local, CGNAT, benchmark, multicast, unspecified and the host's own addresses, with an allow list; HTTPS by default; the size limit is enforced while streaming; an optional expected SHA-256; at most 3 concurrent per node and a rate per user; partial files are removed on failure. Its documentation states it must not be pointed at KingsIsle's servers without the terms risk stated and must not mirror client data.
- **SFTP (17.40):** opt-in and off by default. Ed25519 host key with its fingerprint shown in the panel, modern algorithms, the SFTP subsystem only, authentication by SSH keys registered on the account page or by a scoped app password with a TOTP code, the same jail, protected paths, space guard and grants as HTTP, the config root read-only over SFTP, append and resume honored, writes refused during protected states, sessions ended on revocation, and activity merged per user, root and minute.

Signed links for browser-native downloads and uploads are an HMAC over scope, user, node, root, path or folder, maximum bytes, file id and modification time for downloads, issue time, expiry (15 minutes), and a nonce remembered until expiry. They are refused when issued before the supervisor started or before the user's revocation time, and the user's current grant is checked again when the link is used.

### Editor

The editor (17.53) uses the library the maintainer chooses; CodeMirror 6 is the proposal in doc/ROADMAP.md, as are the archive library for extraction, the default archive format for folder downloads, and the trash and version store locations with their retention defaults. It offers Ctrl+S, an unsaved-changes guard, drafts per node, root and path in session storage, the conflict diff, a version history with restore, and an apply-live button. Modes are detected by file name first, then extension, with a manual override: an Ambrose `.conf` mode with key completion, type, bound and default hovers and unknown-key warnings from the settings schema; SQL in the MySQL and MariaDB dialect; JSON with a tree viewer for large type dumps; XML; Lua; logs with level colors and follow; INI, YAML, Dockerfile, shell, PowerShell, batch, Markdown, CSV and plain text.

### From Pterodactyl

- **Keeps:** handle-relative resolution that checks and acts on the same descriptor; never following the last link; refusing to delete or rename the root; hiding host paths in errors; per-write space reservation; refusing FIFOs; single-use short-lived signed links for uploads and downloads; zip-slip and zip-bomb defenses with declared-size checks and per-entry caps; empty folders kept on extraction; copy naming; a SSRF check on the connected address for remote pull; a per-server download concurrency cap; SSH key validation before parsing; activity per file operation.
- **Changes:** a jail that also works on Windows; named roots with policies; protected paths matched on the resolved path, so one rule covers every operation, SFTP and remote pull included; atomic saves with conflict detection and version history instead of truncate-and-write where the last save wins; delete to trash; size limits enforced while streaming; one write permission so the API and UI cannot disagree about overwrite; chmod audited and made safe on every kernel; batches validated before applying; no 250-row cap; folder upload; archive download of folders; SFTP authentication that cannot bypass two-factor sign-in; remote pull opt-in with the missing address ranges blocked and partial files removed.
- **Drops:** container homes and per-server disk quotas, per-egg file deny lists, free-form octal chmod, chown to a container user, content sniffing of every listed file, a root ignore file with its own editor banner, and web-hosting editor modes that Ambrose files never use. Client-derived roots also drop download, archive and share links entirely, which a generic panel has no reason to do.

## Graphs and alerts

### Sampling and history

The supervisor samples every app (17.19) through the operating system, with no Prometheus or Grafana needed: on Windows, process times, private bytes and working set, handle and thread counts; on Linux, the `/proc` stat, status, descriptor and I/O files. CPU is reported both as a percent of one core and of the machine. Network traffic and message counts come from each app's own counters, since per-process network use is not portable. Disk is the size of each root and the free space of each volume. Wizard101 figures come from the stats registry: sessions, players online per realm and zone, sessions at character select, tick time average and maximum, database pool use, pending SQL updates, settings generation, last reload result, login attempts and lockouts, patch bytes served and active downloads, and the client revision in use.

Samples push once a second only to subscribed sockets. History keeps a day at full detail in memory and 30 days downsampled in the store. An app that stops sends one zeroed sample with its last exit.

### Graphs

Graphs per app, realm and node show CPU, memory, network, disk, sessions, players and tick time over ranges from 5 minutes to 30 days, with a live view of at least 60 points that does not reset when an app stops. Stat blocks turn gold above 80 percent and ember above 90 percent of a limit.

### Alerts

- Rules (17.67): crash, crash loop, high tick time, high memory, low disk, backup failed, schedule failed or skipped for lost permissions, update failed, restore incomplete, stale online rows cleared after a crash, node offline, admission queue above a threshold, certificate or API key expiring, and sign-in lockouts above a rate. A condition whose milestone has not landed registers when it does.
- Each rule has a threshold, a duration, a severity, channels and a repeat limit, so a rule firing every minute sends one notice. A crash loop sends one alert, not one per crash.
- Channels: the panel's notification center, webhooks such as Discord, and email through SMTP (configured on the panel settings page, 17.35).
- The alerts page shows each alert's history with acknowledgement and who acknowledged it, a mute per rule with a reason and an end time, and a failed delivery retried with backoff and visible on the page. Toasts report async results such as a reload or backup finishing.
- An alert body carries no secret setting value and no player email or address, only the subject and the figure that tripped the rule.
- Graphs and history are 17.19 and need no mail settings; rules, delivery and acknowledgement are 17.67, which is where the SMTP dependency belongs.

### Game operations analytics

Host resources say whether the machine is healthy; they say nothing about the game. 17.69 keeps daily aggregates in the supervisor's store, read from the game databases with a statement timeout and, where one is configured, a follower connection, so a report never slows a live realm.

- Accounts created and verified, first sessions, players returning by day and week, characters created by school and level band, and a session length distribution.
- Economy figures where their tables exist: gold and crowns held and spent, items and reagents gained per source, and vendor and bazaar turnover. A figure whose owning milestone has not landed reports as unavailable naming that milestone, never as zero.
- A quest funnel per chain of offered, accepted, completed and abandoned counts, so an operator sees where new players stop.
- Aggregation is a scheduled job over a window, idempotent per day so a re-run overwrites rather than doubles, and its rows export as CSV through the formula-safe writer in 17.25. Reading the pages needs `metrics.read`, exporting needs `activity.export`.

### Public status and scheduled downtime

Players ask whether the game is up, and an operator should not have to answer each time. 17.70 serves a public read-only page on the panel listener at its own path, off unless `Panel.PublicStatus.Enable` is set.

- It shows each realm's up or down state, whether the login server is accepting players, and the current or next maintenance window from 17.64 or a 17.15 schedule, with a short operator note and an incident note an operator can post and clear, audited with who and when.
- It carries aggregate figures only. A response shaping test proves the payload holds no player name, address, account figure or app internal.
- It has its own cache and rate limit, sets no cookie and reads no session, so it can neither be used to load the panel nor to probe a session.

### From Pterodactyl

- **Keeps:** CPU, memory, disk and network per app; stat blocks with warning colors near limits; a final zeroed sample when an app stops; disk scans that serve a stale value while refreshing; one live socket for stats on the console page.
- **Changes:** stored history up to 30 days instead of 20 points that vanish when a server stops; Wizard101 figures such as players, tick time and pool use; alert rules and notices, which Pterodactyl does not have; the overview uses the socket instead of polling every card every 30 seconds behind a 20-second cache.
- **Drops:** Docker stats arithmetic and container network counters.

## Nodes

### Model and joining

A node (17.22) is a machine running the supervisor in node mode. On first start the supervisor registers itself as node 1 in a location named local, so a single machine needs no setup.

- `node`: id, uuid, name, description, location, address and admin port, maintenance flag, eligible for placement, capacity (player slots, cores, memory, disk), pinned certificate fingerprint, supervisor build and commit, operating system, architecture, CPU threads, memory, disk, client install revision and type dump revision, last heartbeat, state.
- `location`: short code (unique) and long name, used to group nodes. Deleting one that still holds nodes is refused.
- Joining uses a one-time token, stored hashed, bound to one node record and valid for minutes, shown once in a command such as `supervisor --join <panel-url> <token>`. It becomes mutually authenticated TLS with a pinned certificate. Credential rotation reissues the certificate with an overlap window, and removing a node revokes its pin at once.
- Every join and heartbeat carries the supervisor protocol version. The panel refuses a node whose protocol is newer than its own and names both versions, keeps a node one version behind working read-only with a badge and a message naming what it cannot do, and a node refuses a command it does not understand rather than guessing.
- A node may report on and receive commands only for apps placed on it; the panel checks this on every call.
- Config pushed to a node is validated, written, then swapped, and the node answers applied or refused, naming any environment or command-line lock that refused a key. The panel saves its record even while the node is offline and shows it as pending until acknowledged.
- Maintenance drains placement, blocks non-owner actions, badges the node, and can put the node's realms into maintenance.

### Heartbeat

Each node pushes a heartbeat over its link at a set interval with its build, uptime, per-app state and exit codes, CPU, memory, handles, disk, sessions, players, tick times, settings generation, last reload result, client revision and type dump revision. Missing a set number of heartbeats marks it offline; it shows online within 10 seconds of the link returning. A node that loses the panel keeps its apps running, keeps its own schedules on time, buffers audit rows and run results, and resyncs when the link returns. Whether a node's schedules run on the node from replicated definitions or centrally on the panel is a proposal in doc/ROADMAP.md. The browser only ever talks to the panel, which relays console, logs, files, backups, schedules, graphs and power actions under the signed-in user's permissions.

### Moves

Moving an app or realm to another node (17.42) has a record with source, target, reserved target allocations, state (`pending`, `draining`, `archiving`, `streaming`, `verifying`, `starting`, `completed`, `failed`, `cancelled`), who, times and error. The flow: a countdown to players, realm maintenance, a final backup of config, data and logs, stop, stream to the target over the node link with SHA-256 verification and resume, start on the target, wait for health, update the realm list address and port, and clear maintenance. Client data is rebuilt on the target from that machine's own install, never shipped. The panel can cancel a move, a stall timeout fails it and releases reserved allocations, only the target can report success, and a move to the same node or to a node lacking the required build, supervisor protocol version or client revision is refused.

### From Pterodactyl

- **Keeps:** nodes with locations, capacity and a maintenance flag; a daemon that re-syncs with the panel when it starts; a node that can act only on its own servers; reading a node's secret configuration requiring write-level rights; config pushes that validate, write and then swap, refusing values that conflict with local overrides; moves with reserved target allocations, a scoped short-lived credential, checksum verification and success reported only by the target.
- **Changes:** mutual TLS with pinned certificates and single-use join tokens instead of a long-lived bearer token and an admin API key for auto-deploy; heartbeats pushed over the node link, so no page ever carries a node credential to a browser; node schedules keep running without the panel; moves can be cancelled and time out instead of leaving a server stuck transferring; capacity in Wizard101 terms with one overallocation formula used by both the page and placement.
- **Drops:** Wings and its YAML configuration, Let's Encrypt paths, the behind-proxy flag, CDN version checks and default-on telemetry, and moving whole container folders.

## Wizard101 pages

These pages have no counterpart in a generic hosting panel. Each relays to the apps' admin APIs and CommandMgr, so the panel and the in-game GM commands change the same state the same way.

### Status API

Every app answers three GET routes on its admin API (17.03), each behind the same token as `GET /api/health`, and every dashboard page is built from them rather than from anything the panel stores itself.

- `GET /api/status` is the live picture of one app: `schema`, `app`, `role`, `realm`, `revision`, `state`, `uptime` in seconds, `memory.resident_bytes` and `threads` as the operating system reports them, `sessions` (null until the app publishes one), `tick` with `average_ms`, `max_ms`, `samples` and `window_seconds` over the last sixty seconds for an app that ticks and null for one that does not, `stats` holding every value a subsystem has published into the process's stats registry under its own name and JSON type, and `problems`, each a `code`, a `message` and a `subject`. Later milestones add fields as their systems exist: players online and the settings generation from 4.01 and 4.16, sessions at character select from 4.05, realms from 4.03, zones with players per zone from 4.09, database pool usage from 2.01, pending SQL updates from 2.06 and the last reload result per target from 4.15.
- `GET /api/apps` is the one app list every page reads: an app answers with itself as `name`, `role`, `realm`, `address`, `port` and `revision`, and the supervisor (17.14) answers the same shape with the apps the signed-in user may see, so nothing stores a second list.
- `GET /api/capabilities` says what this build can do, from the registries the build itself fills: `reload_targets`, `schedule_actions`, `announcement_channels` and `problem_codes`, each code with the description an operator reads beside it. The panel offers only what is listed and follows a newer build after an update without a change of its own.

A field, once written, is never renamed or removed; it may only be added, and `schema` counts up when a shape changes in a way a reader has to know about. A test holds every version-one field of all three routes, and a build that loses one fails it. A problem's `code` is always one the build registered, so the panel can name every code it may ever see before it sees one.

### Live log stream

Every app streams its log over one WebSocket, `/api/logs` (17.04), behind the same token as the GET routes, and the panel's log pages, the terminal dashboard and the supervisor's fan-out all read it rather than a log file. It sits on the one stream layer in `src/server/shared/Admin/`: sequence numbers from the log itself, the backlog the `Appender.Stream` line sizes (1000 records unless changed), resume after a sequence number, a dropped marker for any range a reader missed, and a bounded queue per subscriber, which 17.12's `/api/events` and 17.26's panel socket reuse rather than building their own.

- The first text frame a client sends is its subscribe request, a JSON object with three optional fields: `level` (the lowest level to receive, `trace` to `fatal`), `categories` (a list of category names, each matched with its children, so `commands` covers `commands.console`), and `after` (the last sequence number the client already has). An empty object subscribes to everything and receives the whole backlog. A request the server cannot read is answered with a `problem` message and a close.
- The server answers with a `hello` carrying `latest` and `oldest`, the sequence numbers the backlog spans, and `backlog`, how many records it holds. Then come the backlog records the filter passes, or, with `after`, only the records after it, and from there every new record as it is logged.
- A `record` is `sequence`, `time` (UTC, the log file's own format), `epoch_ms`, `level`, `category` and `message`. A `dropped` marker is `from`, `to` and `count`: the range a reader will not see, either because a resume asked for records the backlog had already let go, or because that subscriber's queue filled and the oldest records were dropped so logging never waited. The queue drops rather than blocks, and the log thread's cost is the same with a stalled subscriber as with none.
- Arguments of console commands marked sensitive and values of secret settings (the admin token and every database connection string's password) never enter a record, as they never enter a log file: the console logs a sensitive command as its name with `(arguments hidden)`, a setting change is logged through the redaction layer, and every message is scrubbed once more as it is encoded for the stream.

### Overview

The overview (17.06) shows one card per app: a status bar color, name, role (login, game, patch), realm, address and port, uptime, build, sessions, players against the realm's player limit, tick time average and maximum, and usage against limits. Badges show disabled, maintenance, setting up, restoring, updating, moving, crash loop, restart required, update available, pending SQL updates, client revision mismatch and open problems. Cards update from the event socket and show a visible stale state after missed updates. Owners and admins can switch between the apps they are granted and all apps, remembered per user.

### Realms and zones

The realms page (17.31) lists every row of `realmlist` from 4.03: name (its RealmNames.lang key with the display name read from the user's install), address and local address, port, flags (offline, recommended, full, test), population against player limit, last heartbeat, the gameserver app behind it and its node.

- Editing the player limit, flags and default realm goes through the realm settings and applies from the next realmlist refresh, which releases queued players when the limit rises (12.21). 17.71 shows and manages that queue: its length, the oldest wait, the admission rate and why the realm is closed, with raise and lower, pause and resume, a drop with a reason, and a position lookup for one account whose identity needs `accounts.pii.read`.
- A realm page shows population over time, players at character select headed there, the zones the realm has loaded from `sZoneMgr` (4.09) with players and instances per zone, and, once 12.17 lands, public instances with their capacity.
- Zone actions, each audited and checked: reload a zone's data through 4.15, and teleport or kick everyone in a zone once 6.06 and 6.05 exist.

Realm maintenance (17.32) closes a realm to players while game masters at or above `Realm.MaintenanceBypassLevel`, whose default is a proposal in doc/ROADMAP.md, can still enter. Entering maintenance can run a countdown, kicks connected players with a notice, sets the realm list flag so the login server stops sending players there, and records who and why. A node in maintenance can put its realms into maintenance.

### Players online

The online players page (17.21) lists each player with realm, zone, character name and level, account (with `accounts.read`), session time, address and MachineID (with `accounts.pii.read`), and whether they are at character select or in world. Actions come as their milestones land: kick (6.05), mute (12.07), teleport (6.06), message the player, and open the account or character. Joins and leaves arrive live over the `players` stream.

### Accounts, bans and characters

- **Accounts (17.21):** search by username, email, address or MachineID; create; reset password; lock and unlock; set security level; see email, join date, last sign-in time, address and machine; and the account's activity. A password reset writes the verifier through AccountMgr, seals it with `Account.VerifierActiveKey`, deletes `account_session`, and kicks live sessions under `Login.DuplicateLoginPolicy`. The operator types the new password or has one generated and shown once; it is never logged.
- **Bans:** account, address and machine bans with duration, reason, who and when, and unban with a reason, matching the 6.05 console commands. Bans apply live and disconnect a banned client.
- **Characters (17.21):** a list per account with name, level, school, location and deleted state; rename, restore a deleted wizard and delete as 3.17 and later phases support them; and an edit form offering the fields the running build reports as editable through `GET /api/capabilities`, such as gold and level once later phases make them so, each applied through its own GM command.
- Every action requires its typed permission, runs through CommandMgr at the level the user's grants and linked game account allow, follows the no-escalation rule over security levels, requires a reason for bans, locks and security changes, and is audited, refused attempts included.

### Player accounts

Operators should not have to type console commands to make a player an account, and a player should be able to recover their own password. `Panel.Registration.Enable`, off by default, opens a public sign-up page on the panel listener outside the operator area (17.62).

- Sign-up creates a game account through AccountMgr under 2.13's username and password rules, at the lowest security level, and never a panel user.
- With SMTP configured in 17.35 the account is created unverified, a single-use link expiring in hours verifies it, and `Login.RequireVerifiedEmail`, a live setting, decides whether an unverified account may sign in to the game.
- Player-driven reset answers the same whether or not the address is known, carries its single-use token in the request body, and a new password under the same policy seals the verifier with the active key, deletes `account_session` and kicks live sessions.
- Throttles run per address, per account and per email domain through the listener's limiter, the 17.35 captcha applies after repeated attempts, and mails per address per day are capped. Every registration, verification, reset request and reset is audited with the client address, and no response tells a stranger whether a username or email exists.
- An operator page lists recent registrations with their verification state, resend and block, behind `accounts.read` and `accounts.registration`.

Whether the panel offers player registration at all, and with which requirements, is a proposal listed under Decisions needed in doc/ROADMAP.md.

### Moderation

Kick, mute and ban are actions; moderation is a queue with the evidence attached (17.63).

- The report queue sits over 12.07's moderation records: player reports and house reports with reporter, subject, category, text, realm, zone, time and state (new, claimed, actioned, dismissed), claimed by one moderator at a time with the claim visible.
- Chat search for a reported player runs over 12.07's chat records, bounded by time range and result count, with the lines around each match, filtering by channel, and the bound reported when it stops early.
- Mute history per account and character shows who, why, how long, when it ends, the kicks and bans the same moderator issued, and a repeat count per subject.
- Actions taken from a report go through the checked paths in 17.21 for mute, kick and ban, stay linked to the report they came from, and each need a reason.
- Queue counts need `players.read`. Reporter identity and chat text need the moderation keys, and every read of chat text is audited with the subject and the range read.

### Live settings and reloads

The read half comes first, with 17.08: every app answers `GET /api/settings` with each key it has loaded, its effective value, the shipped `.conf.dist` default, the layer, file and line each comes from, secrets masked the way the log stream masks them, and the reason for each option the app documents as taking effect only at its next start, which the config page shows as restart-required. The settings page (17.13) shows every setting from 17.12 by category with search, typed and bounded inputs, default, effective value, source layer, apply mode and lock. Edits need a reason and pass a review step, and each key has history with revert. Secret settings are masked without `settings.secrets.read`, and restricted settings need `settings.edit.restricted`. Settings presets, such as a realm's rates and timeouts, export to a versioned JSON file and import with validation against the schema and a diff before applying; an import never removes keys the file does not mention. The reload page shows each target's generation, last result and errors with a button per target, and the restart-required list with the reason for each case.

### Client data and revisions

The client data page (17.20) lists the installs `ClientLocator` found and the revision each app uses, the type dump in use with its revision, program SHA-256, extractor version and build time, and the message definitions, name tables and creation config loaded. It shows 3.23's revision following: the newest revision seen, whether its data is built, and a build in progress with live output over the `setup` stream. Rebuild and switch buttons run as the protected `setup` state, are audited and apply live where 3.23 supports it; a failed rebuild keeps the previous data serving and shows the extractor's error. Nothing is served or copied from the install; the page reads it at runtime.

### World database edits

The world edits page (17.34) is a typed editor over the content tables the game server loads, starting with those that exist when it lands: spawns, templates, doors, vendors and quests as later phases add them.

- Rows are browsed with search and filters, and each table's form comes from a schema the game server publishes with types, bounds and references, so a foreign key is picked from its table.
- An edit is sent to the game server, which applies it to the world database, records it in the 4.15 world edit journal with the panel user as author, and reloads the affected store, reporting the reload result. A group of edits applies as one change set and reloads together.
- The page shows the journal with who, when, source (the panel or a GM command such as `.npc add`) and statement, and exports selected entries as a local-only SQL file into `data/sql/custom/db_world`, the tree doc/ARCHITECTURE.md sanctions for local SQL, the way `.journal export` does. A request naming any other folder is refused. `pending_db_<name>/` belongs to open pull requests, and its naming is an open decision that blocks 3.19, so a running panel never writes there.
- A failed reload rolls back the database change and keeps the previous store serving.

### Database management

The database page (17.08) lists applied and pending update files per database from the updater, applies data-only updates live and reloads the affected stores, and shows connection pool use. Each app answers for its own databases:

- `GET /api/database`: each database's state, its address without the password, whether `Updates.EnableDatabases` includes it, pool use (async and sync connections, those running a statement, sync leases and callers waiting for one, queued work and reconnects) and the stores that read it. It opens no connection, so a page can poll it.
- `GET /api/database/updates`: the applied files with hash, state, time and duration, marking one whose file changed or went missing, and the pending files in the order the updater applies them. A pending file is data-only when every statement only changes rows, reads without writing a file, steps a transaction, locks tables or sets a session value; anything else, an executable comment included, can change the schema, so that file is restart-required with the statement and line that make it so, and every file after it is restart-required too, naming the file it waits behind.
- `POST /api/database/apply` with the database's name applies its pending data-only files in order, each inside one transaction when it holds only row changes and reads, so a failing file leaves nothing behind, and stops before the first file that can change the schema. It then reloads every store that reads that database, keeping the loaded one when a reload fails, and answers with the files applied, where it stopped, any failure, and each store's result. It is refused with 409 while the updater runs, when the database is not open, or when `Updates.EnableDatabases` leaves it out.
- `POST /api/database/reload` reloads those stores alone, so the supervisor can bring the other apps that share the database up to date after one of them applied an update.

Database hosts (17.30) extend it:

- A registry of database servers with host, port, administrative user, sealed password, TLS mode, node affinity and server version, auto-registering the private MariaDB from 17.24. A connection, version and privilege test runs before a host is saved, and a failure saves nothing.
- The panel creates least-privilege users: one runtime user per app with only data access to its databases, and a separate updater user with schema rights used only by the 2.06 updater, with host restrictions and a connection limit sized from the app's worker and synchronous threads. Identifiers come from an allow list and are quoted, never taken from free text.
- Credential rotation causes no downtime. On MySQL 8.0.14 and later, the new password is added while the old is retained, the app's connection setting is changed through the live settings path so its pool opens a new generation and swaps (keeping the old generation on failure, as Decisions, Database pools settles), and then the old password is discarded. On MariaDB, which has no dual passwords, a second user with the same grants is created, the pool swaps to it, and the old user is dropped. Revealing a password needs `database.secrets.read` and a step-up check, and every test, create, rotation and reveal is audited. Rotation per server type, and whether a realm ever gets its own world database, are proposals in doc/ROADMAP.md.
- The group has no export key of its own. A single database's contents leave the machine one way only: as a backup component taken by 17.16 and fetched through `backups.download` with the recent step-up check and the audit row 17.52 requires, so there is one audited path instead of two.

### Announcements and events

Announcements (17.33) send a message now or on a schedule:

- Channels: the gameserver's zone broadcast (6.01) and GM system messages (6.04) to a realm, a zone or everyone, and the final login-screen shutdown notice as a schedule step. Channels whose milestone has not landed are refused with the missing milestone named.
- Each announcement records its text, scope, channels, who, when and the sessions reached.

Timed game events are scheduled groups of setting changes with an automatic revert, such as a double experience weekend: set `Rate.XP.*` on Friday and restore the prior values on Sunday. An event records the values it replaced, reverts even if the settings were changed meanwhile only when the operator chose that, shows its countdown on the overview, and can end early. Events are built on the schedule engine and the settings API, so every change carries the event's name as its reason.

### Installation maintenance

Realm maintenance (17.32) closes one realm. Closing the whole installation for database work, while game masters can still sign in and check it, is `Login.Maintenance` (17.64), a live setting through 17.12.

- 2.14's authentication answers with the maintenance reason the client shows, and the realm list offers players nothing. Accounts at or above `Login.MaintenanceBypassLevel` sign in normally; its default, like the realm bypass level, is a proposal in doc/ROADMAP.md.
- The panel shows a banner and the control with who, why, when it started and an optional window, audited, and publishes the window to the public status page.
- Maintenance survives a loginserver restart while it is on and leaves players already in the world connected, unless the operator also closes their realms through 17.32.
- A schedule task enters and leaves maintenance, so a database window is planned like any other job.

### Patch server operations

The patchserver has a page of its own (17.65) over the 16.03 manifest: the revisions the patch output holds with their file counts, bytes and build times, which one is being served, and the last generator run with its live output.

- Publishing a revision runs the generator against a chosen install, validates the output, then swaps the served manifest through 16.08's reload so downloads in flight keep the old file set, with a rollback to the previous revision.
- The operator's own signing key lives here: generated or imported, its public part and fingerprint shown, rotated with an overlap window, its private part sealed with the supervisor's key, never in a response, and exportable only by an owner after a step-up check. Where that key lives and how it rotates is a proposal in doc/ROADMAP.md.
- That key is what Decisions, Experimental features requires before the patchserver serves an executable: a component whose signature does not verify is refused, naming the key it expected.
- Publishing and the key routes are owner-only, and every publish, swap, rollback, generation and rotation is audited. No route here returns a file from outside the patch output root, nothing is published that the operator did not build or place themselves, and the patch output root is client-derived, so the panel neither downloads nor archives it.

### Updates

The updates page (17.17) shows the release channel and build channel, each build's version, commit and changelog, the running build, the last few builds kept for rollback, and the update run's live steps: backup, install beside the running build, `--check` on every app, switch, restart and health wait, with an automatic switch back and restore on failure. A newer KingsIsle client revision from 3.23 shows next to the Ambrose update with a rebuild button. Channel checks are the only outbound request the panel makes, and only when a channel is configured.

### From Pterodactyl

- **Keeps:** startup variables' split between viewable and editable, as setting visibility and edit classes; a value an admin locks, as environment and command-line locks; export and import of shareable definitions, as settings presets; database hosts tested before saving, generated database users with host limits, and password reveal behind its own permission; a server list whose cards show status and usage; install output shown live, as client data builds.
- **Changes:** every game option is a typed, bounded live setting instead of an environment variable in a startup template; credential rotation with no window where the user is missing, instead of dropping and recreating it; least-privilege runtime users instead of broad grants; a host saved only after its test passes, instead of inserting then testing; imports never silently delete what the file omits.
- **Drops:** eggs, nests, install scripts and Docker images as the service model; client-created per-server databases and database limits; the startup command template with its placeholders; Minecraft- and Steam-specific console feature pop-ups.

## Account page

The account page (17.38) has Profile (display name, email, locale, theme), Security (password change with the current password; two-factor setup, disable and recovery codes; security keys once 17.45 lands; the sessions list with device, address, first and last seen, sign out per session and everywhere), API keys (17.36), SSH keys when 17.40 is enabled, Activity, and the game account link.

- Changing email needs the password and a fresh second-factor check, is limited per day per user, and, when SMTP is configured, confirms the new address by link and notifies the old one.
- Linking a game account needs proof of ownership: that account's password, or a one-time code typed in game once 6.04 exists. The linked account's security level then caps console commands, and role changes that follow the level apply on the next request.
- Two-factor enrollment renders its QR code in the browser from a renderer bundled with the dashboard, never fetched from another host, and the page's CSP would block one; which renderer is a proposal in doc/ROADMAP.md.
- Show-once dialogs for keys and recovery codes cannot be dismissed from outside and offer copy and download as text. Buttons stay disabled until the form is valid, with the reason shown, and errors show inline beside the form that caused them.

## Panel settings

Panel settings (17.35) use the same typed schema, locks and audit as game settings and apply live.

- **General:** panel name, public URL, logo, the default locale 17.66 renders in, session idle and absolute lifetimes, and the retention defaults this phase's stores use.
- **Mail:** SMTP host, port, TLS mode (none, STARTTLS, implicit TLS), username, a sealed password that is never shown with an explicit clear control, from address and name, and a test that sends only to the signed-in user. 17.62's verification mail and 17.67's alerts use it.
- **Security:** sign-in thresholds, relay timeouts, the port pool range, and an opt-in captcha from a chosen provider with the operator's own keys, never shipped keys and never an echoed secret, applied only after repeated failures and failing closed with a clear error when the provider is unreachable.
- **Owned elsewhere:** `Panel.BindIP`, the TLS paths, `Panel.AllowPlainHttpRemote`, `Panel.TrustedProxies` and `Panel.TwoFactorRequired` show here read-only with their layer and their owning milestone (17.14 and 17.47), so this page never becomes the place that first enforces them.
- **Environment only:** keys set by the environment or command line show as locked, so an operator can make any part of the settings read-only in the UI.

### From Pterodactyl

- **Keeps:** general, mail and security settings pages; a mail test sent only to the signed-in admin; an encrypted mail password never shown back; a required two-factor level; settings that can be locked to the environment.
- **Changes:** live application with no worker restart; typed bounds and an audit row per change; an explicit clear control instead of a magic string; captcha opt-in with no shipped keys.
- **Drops:** the key-value settings table with string sentinels, CDN version checks and telemetry.

## Interface

### Structure

One Svelte app serves every page, with routes declared in a table that names each route's permission and whether it shows in navigation:

- `/sign-in`, `/sign-in/verify`, `/first-run`, `/invite/:token`, `/reset/:token`
- Public, each off by default: `/register` and `/account-recovery` (17.62), `/status` (17.70)
- `/` overview
- `/realms`, `/realms/:id/{overview, zones, players, settings, reloads, schedules, maintenance, activity}`
- `/apps/:id/{console, logs, settings, reloads, launch, network, files, backups, schedules, graphs, database, activity}`
- `/players`, `/accounts`, `/accounts/:id/{characters, bans, sessions, activity}`, `/bans`, `/moderation`, `/moderation/:reportId`
- `/world`, `/announcements`, `/events`, `/client-data`, `/patch`, `/alerts`, `/analytics`, `/updates`, `/schedules`, `/backups`, `/files`, `/activity`, `/maintenance`
- `/admin/{nodes, nodes/:id, locations, database-hosts, users, roles, invites, api-keys, settings/general, settings/mail, settings/security}`
- `/account/{profile, security, api-keys, ssh-keys, activity}`

A top bar holds a realm and app switcher, a global search across accounts, characters and the apps the caller may see whose every result is checked against their own permissions (17.21), the notification center, a theme toggle and the account menu. The side navigation groups Operate (overview, realms, apps, schedules, backups, alerts, analytics, maintenance), Game (players, accounts, bans, moderation, world, announcements, events, client data), Platform (nodes, locations, database hosts, files, patch, updates) and Panel (users, roles, invites, API keys, settings, activity). Tabs and controls the user lacks are hidden, a direct link without permission shows an access-denied page, and pages for a target in a protected state show that state and its live progress while owners keep the console.

### Conventions

- Light and dark themes with a system default and a per-user choice, from CSS custom properties with accessible contrast; every page usable at 400 pixels wide.
- Errors render beside the form or dialog that caused them, keyed per form; 422 responses mark each field; every error carries a request id that also appears in the supervisor log.
- Destructive actions (delete an app, restore, move, rotate credentials, reset a node's credentials, delete a user) confirm by typing the target's name.
- No external requests: no avatars from third-party hosts, fonts and the enrollment QR renderer are bundled, and no KingsIsle logos, art or fonts are bundled or served.
- The dashboard's own strings come from locale catalogs, one JSON file per locale under `apps/dashboard/`, with the installation default from 17.35 and a per-user locale (17.66). A test fails when a string is added without a key in the default catalog, an incomplete locale falls back key by key rather than rendering blank, and dates, numbers and durations use the browser's own locale facilities with the operator's time zone named beside every absolute time. Server-sent notices stay in one language, since localized server text is planned, not yet scheduled in doc/ROADMAP.md.

### From Pterodactyl

- **Keeps:** a route table where each route declares its permission; navigation that shows only permitted pages and an access-denied page for direct links; keyed flash messages rendered where the error happened; full-page state screens during install, restore and transfer; per-user persisted view toggles; request ids for correlating errors with logs.
- **Changes:** one app instead of separate admin and client front ends; light and dark themes instead of dark only; permissions update live instead of on reload; the overview streams instead of polling each card.
- **Drops:** AdminLTE, jQuery and SweetAlert, Gravatar, and the separate admin area behind one admin flag.

## Packaging

- The supervisor installs itself as a Windows service or systemd unit running as a dedicated user (17.23). When it runs as root on Linux, created files are given to the service user.
- The Docker image and Compose file run the whole stack, with volumes for config, data, logs and backups and the user's client install mounted read-only.
- The Pterodactyl egg (17.23) is for operators who already run Pterodactyl. It uses Wings' contract: the ready lifecycle line as the startup done string, `shutdown` as the stop command so Wings does not count the exit as a crash, and plain log lines when output is redirected.
- The desktop app (17.24) installs Ambrose, starts the supervisor, the private database and the client, and opens the panel through a fresh one-time owner link bound to the local machine (17.46), so a player hosting on their own computer never types a panel password.

## Open decisions

Nothing here is settled by being written in this document. Every choice this design proposes is listed under Decisions needed in doc/ROADMAP.md, which is where decisions that block milestones live: the maintainer settles each one, doc/ARCHITECTURE.md records it under Decisions, and the roadmap entry says which milestones it blocks. Each section above marks its own proposals where they matter and points here, so this document keeps no second list of them to fall out of date, and no milestone may treat a choice as settled because it appears in this file.

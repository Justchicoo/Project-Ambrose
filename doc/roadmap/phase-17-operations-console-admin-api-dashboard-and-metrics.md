<!-- Project Ambrose by Imjustchico: Roadmap phase 17, Operations: console, admin API, dashboard and metrics. -->

# Phase 17: Operations: console, admin API, dashboard and metrics

**Done when:** From a browser on a desktop or a phone, an operator sees every server's health and player counts, follows live logs, runs audited commands, edits game settings and reloads content live, restarts a crashed server, and reviews performance history in Grafana. Like a game server hosting panel, it signs each operator in on the panel's own listener with their own account, two-factor sign-in and permissions, records every action in an activity log, runs scheduled restarts and backups, restores a backup, updates with one click and rolls back, edits files, and manages servers on several machines from one place. Built for Wizard101, it also manages realms and zones, players online, accounts, registration and password recovery, bans, characters, reports and mutes, world database edits, announcements and timed events, patch revisions, an installation-wide maintenance mode that still lets game masters in, and a player on a desktop starts everything from one icon. The servers stay headless, so they run the same on a desktop, a Linux VPS, in Docker, or under an existing Pterodactyl panel. That listener follows the admin API's remote-access rule under its own `Panel.` option names, with TLS and its certificate handling, settled on 2026-09-22 and recorded under Decisions, Operations in doc/ARCHITECTURE.md. This track runs in parallel: 17.01 after 1.20, 17.12 and 17.13 after 4.16, and the rest once their dependencies land, which for 17.14 and the panel milestones above it means after 3.23. doc/PANEL.md describes the panel these milestones build.

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
| 17.20 | Client data and revisions page | M | 17.06, 3.23 |
| 17.21 | Accounts and bans pages | M | 17.05, 17.48, 17.49, 2.13 |
| 17.22 | Nodes: one panel for servers on several machines | L | 17.26, 17.28, 17.29, 17.32, 17.49 |
| 17.23 | Operating system services, Docker image and Pterodactyl egg | M | 17.08 |
| 17.24 | Desktop control app: hosting a game on this computer | L | 17.08, 17.181, 3.22, 3.25 |
| 17.25 | Activity log pages | M | 17.05, 17.49 |
| 17.26 | Panel event socket: envelope, tickets and generated types | M | 17.04, 17.12, 17.48 |
| 17.27 | App states, operation locks and power targets | M | 17.08, 17.26 |
| 17.28 | Launch and startup settings | M | 17.08, 17.13, 17.48 |
| 17.29 | Port allocations and listen addresses | M | 17.28 |
| 17.30 | Database hosts and credential rotation | M | 17.08, 17.47, 17.48, 2.08, 4.16 |
| 17.31 | Realms and zones pages | M | 17.06, 17.26, 17.48, 17.175, 4.03, 4.09 |
| 17.32 | Realm maintenance mode | S | 17.15, 17.31, 4.05, 6.01, 6.05 |
| 17.33 | Announcements and timed game events | M | 17.12, 17.15, 6.01, 6.04 |
| 17.34 | World database edits page | M | 17.13, 17.25, 17.173, 4.15 |
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
| 17.63 | Moderation queue, chat search and mute history | M | 17.21, 17.25, 17.177, 12.07 |
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
| 17.74 | Console log line: columns, parts and per-part color | S | 17.01 |
| 17.75 | Console color conventions and the color depth ladder | S | 17.74 |
| 17.76 | Value runs inside a log message | M | 17.04, 17.74 |
| 17.77 | Console wrapping, hanging indent and the record cap | S | 17.01, 17.74 |
| 17.78 | Event overlays on every graph | M | 17.19, 17.25, 17.49 |
| 17.79 | Correlate a window | S | 17.19, 17.78 |
| 17.80 | Log search over history with facets | M | 17.04, 17.07, 17.14, 17.76 |
| 17.81 | Synthetic login probe and end-to-end health checks | M | 17.15, 17.19, 17.67, 3.24 |
| 17.82 | Installation health checks with fixes | M | 17.03, 17.06, 17.16, 17.47 |
| 17.83 | Crash dumps, symbolization and grouping | M | 17.18, 17.60 |
| 17.84 | Support bundle | S | 17.08, 17.18, 17.60 |
| 17.85 | Alert grouping, inhibition and silences | M | 17.22, 17.67 |
| 17.86 | Notification routing, preferences and delivery test | M | 17.38, 17.48, 17.67 |
| 17.87 | Event-triggered automation | M | 17.15, 17.26, 17.60, 17.67 |
| 17.88 | Command palette and keyboard-first operation | S | 17.06, 17.21, 17.48, 17.73 |
| 17.89 | Kill switches | S | 17.12, 17.13, 17.27 |
| 17.90 | Slow query and database health page | M | 17.08, 17.09, 17.30 |
| 17.91 | Tick breakdown and on-demand profiles | M | 17.09, 17.19 |
| 17.92 | Per-session network quality and the player inspector | M | 17.19, 17.80, 17.175, 17.177 |
| 17.93 | Tamper-evident audit chain and audit streaming | S | 17.14, 17.49 |
| 17.94 | Uptime history and incident timeline | S | 17.70, 17.81 |
| 17.95 | Character point-in-time restore and undelete | M | 17.16, 17.51, 17.177, 3.17 |
| 17.96 | Operations calendar | S | 17.15, 17.32, 17.33, 17.64, 17.68 |
| 17.97 | Daily operations digest | S | 17.67, 17.69, 17.86 |
| 17.98 | Capacity forecasts and the weekly load heatmap | S | 17.19, 17.44, 17.69 |
| 17.99 | Declarative installation file with diff and apply | M | 17.12, 17.28, 17.29, 17.36, 17.48 |
| 17.100 | ambrosectl: one command line over the panel API | M | 17.36, 17.48, 17.99 |
| 17.101 | Content packs: install, version and uninstall | M | 17.18, 17.34, 17.65 |
| 17.102 | Outbound event webhooks | S | 17.26, 17.67 |
| 17.103 | Item and currency ledger with anomaly rules | M | 17.25, 17.69, 8.08 |
| 17.104 | Compensation grants and mass mail | M | 17.21, 17.33, 17.69, 8.08 |
| 17.105 | Releases: the panel program and the launcher as downloadable builds | M | 17.14, 17.24, 17.183 |
| 17.106 | Error reports: source locations, grouping and a report file | L | 17.04, 17.08, 17.14 |
| 17.107 | A value in a log line is a place you can go | M | 17.07, 17.76, 17.106 |
| 17.108 | A panel nobody has to click past a warning to use | M | 17.14, 17.181 |
| 17.109 | Plugins: what one is, and the tab that installs it | M | 17.101, 17.48, 17.18 |
| 17.110 | A panel tool runs without being trusted | L | 17.109 |
| 17.111 | Plugins ship with the panel, from this repository | L | 17.109, 17.110, 17.105 |
| 17.112 | Tomes: templates for realms, apps and companion services | L | 17.27, 17.28, 17.29, 17.30, 17.99 |
| 17.113 | Hang detection and watchdogs at every layer | L | 17.22, 17.23, 17.24, 17.27, 17.60, 17.67, 17.83 |
| 17.114 | Host awareness: sleep, OS updates, shutdown and the clock | L | 17.15, 17.22, 17.23, 17.24, 17.60, 17.78, 17.82, 17.94, 17.98 |
| 17.115 | Runbooks on alerts, findings and crash groups | L | 17.53, 17.67, 17.82, 17.83, 17.86, 17.88 |
| 17.116 | Restore drills and update rehearsals | L | 17.16, 17.17, 17.29, 17.43, 17.51, 17.67, 17.81, 17.82 |
| 17.117 | Panel store durability: checks, snapshots and freeing space | M | 17.14, 17.16, 17.18, 17.19, 17.43, 17.72, 17.80 |
| 17.118 | Drift watch: config, installed files and schema | L | 17.08, 17.12, 17.17, 17.22, 17.67, 17.99, 17.105, 2.06 |
| 17.119 | Drive and host health: SMART, latency, pressure and limits | L | 17.19, 17.22, 17.67, 17.82 |
| 17.120 | Release health, bake window and canary realm | M | 17.17, 17.19, 17.44, 17.52, 17.60, 17.81, 17.83, 17.106 |
| 17.121 | Request tracing and the service map | L | 17.04, 17.49, 17.80, 17.81, 17.90, 17.91, 17.92 |
| 17.122 | Service level objectives and error budgets | M | 17.17, 17.19, 17.44, 17.67, 17.81, 17.86, 17.94, 17.96 |
| 17.123 | Reachability from the internet: router, firewall and an outside probe | M | 17.22, 17.23, 17.24, 17.29, 17.81 |
| 17.124 | Backup copies to disks, shares and SFTP, with a 3-2-1 check | M | 17.16, 17.40, 17.43, 17.52, 17.72, 17.82 |
| 17.125 | Moving the whole installation to a new machine | M | 17.16, 17.22, 17.24, 17.46, 17.51, 17.72, 17.99, 3.22 |
| 17.126 | Point-in-time recovery from binary logs | L | 17.16, 17.24, 17.30, 17.43, 17.51, 17.67, 17.72, 17.78 |
| 17.127 | Dynamic DNS and DNS-01 certificates | M | 17.14, 17.35, 17.108 |
| 17.128 | Reverse proxy configs and the forwarded-header check | S | 17.14, 17.26, 17.35 |
| 17.129 | Staging copies and pull request previews | L | 17.16, 17.17, 17.29, 17.30, 17.34, 17.51, 17.99, 17.116, 3.24 |
| 17.130 | Usage accounting: transfer, energy and cost | M | 17.19, 17.22, 17.25, 17.65, 17.67 |
| 17.131 | Host security audit | M | 17.14, 17.23, 17.29, 17.30, 17.82, 17.123 |
| 17.132 | New sign-in notices and stolen-session signals | M | 17.14, 17.35, 17.36, 17.38, 17.46, 17.47, 17.86 |
| 17.133 | Sign-in with OIDC, Discord or GitHub | L | 17.35, 17.38, 17.46, 17.47, 17.48, 17.50, 17.86 |
| 17.134 | Access reviews and the who-can explorer | M | 17.15, 17.25, 17.36, 17.37, 17.48, 17.50, 17.86 |
| 17.135 | Compromise response: canary credentials and lockdown | L | 17.16, 17.22, 17.25, 17.36, 17.37, 17.47, 17.52, 17.67, 17.84, 17.87, 17.99, 17.140 |
| 17.136 | Expiring grants, just-in-time elevation and break-glass | M | 17.37, 17.47, 17.48, 17.50, 17.57, 17.86 |
| 17.137 | Installed components and security advisories | M | 17.17, 17.67, 17.82, 17.97, 17.105 |
| 17.138 | Network access rules and the address ban list | M | 17.14, 17.36, 17.40, 17.46, 17.47, 17.48, 17.132, 6.05 |
| 17.139 | Player privacy requests: export, erasure and retention | L | 17.21, 17.25, 17.47, 17.51, 17.52, 17.62, 17.63, 17.80, 17.95 |
| 17.140 | Secrets inventory and key rotation | M | 17.02, 17.22, 17.28, 17.30, 17.36, 17.47, 17.65, 17.67, 17.72 |
| 17.141 | Two-person approval and change requests | L | 17.25, 17.37, 17.47, 17.48, 17.49, 17.53, 17.86 |
| 17.142 | Discord bot: commands, alert buttons and live status | L | 17.26, 17.36, 17.38, 17.48, 17.49, 17.67, 17.85, 17.86 |
| 17.143 | Crash and error groups linked to issues and fix releases | M | 17.17, 17.83, 17.105, 17.106 |
| 17.144 | Feature flags with targeting and staged rollout | M | 17.12, 17.67, 17.81, 17.83, 17.106, 4.16 |
| 17.145 | MCP server for AI assistants | M | 17.36, 17.48, 17.80, 17.84, 17.100, 17.106 |
| 17.146 | OpenAPI description and API explorer | M | 17.26, 17.36, 17.48, 17.100, 17.105 |
| 17.147 | Sandboxed operator scripts | L | 17.15, 17.28, 17.48, 17.53, 17.87, 17.110 |
| 17.148 | Installation file synced from Git | M | 17.15, 17.17, 17.36, 17.67, 17.99, 17.149 |
| 17.149 | Inbound webhooks that start a task chain | M | 17.14, 17.15, 17.87 |
| 17.150 | Getting-started checklist and page tours | M | 3.22, 17.15, 17.16, 17.21, 17.37, 17.47, 17.82, 17.86, 17.108 |
| 17.151 | Built-in help, offline docs and what's new | M | 17.06, 17.13, 17.37, 17.88, 17.105 |
| 17.152 | Custom boards and a paired wall display | L | 17.06, 17.14, 17.19, 17.48, 17.58, 17.73, 17.99 |
| 17.153 | High-contrast and color-vision themes, and display preferences | M | 17.38, 17.66, 17.73 |
| 17.154 | Installable panel app, OS notifications and opt-in Web Push | M | 17.06, 17.67, 17.86, 17.108 |
| 17.155 | Saved views, stars, recents and wider search | M | 17.06, 17.21, 17.25, 17.80, 17.88 |
| 17.156 | Undo from the activity record, and bulk actions | L | 17.13, 17.25, 17.37, 17.48, 17.49, 17.53, 17.54, 17.55, 17.68, 17.88 |
| 17.157 | Printable operations reports | M | 17.15, 17.16, 17.17, 17.18, 17.25, 17.35, 17.67, 17.94, 17.97, 17.98 |
| 17.158 | Incident workspace and postmortems | L | 17.67, 17.70, 17.78, 17.79, 17.86, 17.94, 17.102, 17.159, 17.164 |
| 17.159 | Notes, pinned warnings and graph annotations | M | 17.13, 17.25, 17.48, 17.78, 17.79, 17.151 |
| 17.160 | Operator presence and one socket across tabs | M | 17.26, 17.57, 17.58 |
| 17.161 | Change freeze windows | M | 17.15, 17.27, 17.47, 17.48, 17.96 |
| 17.162 | Comment threads, mentions and an inbox | M | 17.26, 17.57, 17.67, 17.86, 17.159 |
| 17.163 | On-call rotations, escalation and shift handoff | L | 17.25, 17.67, 17.85, 17.86, 17.96, 17.97 |
| 17.164 | Staff task board and recurring chores | M | 17.15, 17.48, 17.82, 17.86, 17.96, 17.97, 17.159 |
| 17.165 | Type registry browser and the decoded data routes | M | 17.06, 17.14, 17.48, 17.73, 3.03, 3.07 |
| 17.166 | Archive browser: every archive and entry, decoded where a decoder exists | S | 17.165, 3.11, 3.13 |
| 17.167 | Locale text browser and a live locale reload | S | 17.165, 3.13, 4.15 |
| 17.168 | Template explorer with cross-links and where each is used | S | 17.166, 17.167, 5.01, 5.02 |
| 17.169 | Protocol browser: message definitions and what each server does with them | S | 17.165, 1.15, 2.09, 4.01 |
| 17.170 | Client scans from the panel: census, decode sweep, schema probe and program scans | M | 17.166, 17.169, 3.11, 6.09 |
| 17.171 | Client program reader, opt-in | M | 17.170 |
| 17.172 | Progression and character creation data | S | 17.165, 17.19, 3.14, 5.04 |
| 17.173 | World tables browser and the world schema the servers publish | M | 17.168, 17.08, 4.09, 4.15, 5.04 |
| 17.174 | Zone catalog: templates, named places and placements on a plan | S | 17.173, 4.09, 5.02 |
| 17.175 | Live world: zone instances, spawned objects and wizards in the world | M | 17.165, 17.49, 4.10, 5.02, 5.03, 5.05 |
| 17.176 | Spells and sigils pages | M | 17.167, 17.168, 17.175, 8.04, 8.05, 9.02 |
| 17.177 | Characters and online players pages | S | 17.21, 17.175, 3.17, 6.05 |
| 17.178 | Client driver runs from the panel, opt-in | M | 17.06, 17.14, 17.48, 3.24 |
| 17.179 | One desktop shell for the launcher and the panel program | L | 1.04, 3.25, 17.14, 17.73 |
| 17.180 | Sign-in links for a desktop program | M | 17.05, 17.14, 17.46 |
| 17.181 | The panel program: its own window and a list of panels | L | 17.06, 17.179, 17.180 |
| 17.182 | The panel program in the tray | M | 17.24 |
| 17.183 | Installing the panel program: installer, uninstaller and portable archive | M | 17.24 |
| 17.184 | Signed updates for the desktop programs | M | 17.105, 17.183 |
| 17.185 | The Ambrose service from the panel program | S | 17.23, 17.24 |
| 17.186 | Reaching a panel through SSH | M | 17.180, 17.181 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Origin.** Added on 2026-09-13 at the maintainer's request for a modern, intuitive way to run the servers. It also covers the roadmap review's missing work item for remote administration and health endpoints.
- **Decisions.** Settled on 2026-09-13 under Decisions, Operations in doc/ARCHITECTURE.md: Crow for 17.02, TypeScript and Svelte with Vite for 17.06, an Ambrose supervisor for 17.08, and localhost-only access unless TLS and a token are configured. On 2026-09-16 at the maintainer's direction, plain HTTP beyond localhost became an opt-in setting, off by default (see 17.02). 17.11 picks FTXUI, which vcpkg provides.
- **Live settings.** 17.12 and 17.13 were added on 2026-09-14 for the Live reload and live settings rule in doc/ARCHITECTURE.md. Config editing moved from 17.08 to 17.13, so edits apply live through the settings API instead of writing the `.conf` file and asking for a restart.
- **Hosting panel parity.** 17.14-17.24 were added on 2026-09-17 at the maintainer's request to manage everything in one place the way game server hosting panels such as Pterodactyl do. The supervisor from 17.08 becomes the panel's single entry point, much as a Pterodactyl node daemon serves its panel: operators sign in to it once, and it relays each app's admin API. The panel milestones, 17.14 and everything above it, start after 3.23 and run alongside the gameplay phases. The choices are recorded under Decisions, Operations in doc/ARCHITECTURE.md: Argon2id for panel passwords, from Botan since 2026-09-22, TOTP for two-factor sign-in, zstd for backup archives, and the supervisor's own SQLite file for panel users, schedules and backup records, so the panel works before any game database exists.
- **Pterodactyl source.** Settled on 2026-09-17 at the maintainer's direction: Ambrose owns its panel, tailored to Wizard101, and uses the MIT-licensed Pterodactyl panel and Wings source only as a reference for behavior, with no code copied and no Wings. The Ambrose panel is not a fork: the Pterodactyl panel is PHP 8.2 with Laravel, React, Redis, a web server and a queue worker on Linux, and Wings is Go and runs only on Linux with Docker, not on Windows. A fork would break the desktop run that sets itself up with no steps, and the TypeScript with Svelte dashboard and C++ supervisor settled under Decisions, Operations. Its generic model of a container with a console also cannot reach typed Ambrose features such as live settings, reloads, accounts and client data. Instead, this phase studies its source as the reference for features and behavior: the permission names and sub-users, schedules and task chains, backups, the file manager and the console socket. Operators who already run Pterodactyl use the 17.23 egg. A maintained fork stays an opt-in idea, planned, not yet scheduled.
- **Built first.** Settled on 2026-09-18 at the maintainer's direction: the panel's foundation comes before the rest of the game, so later systems are built into it rather than fitted to it. The order is 17.01, 17.74, 17.02, 17.03, 17.04, 17.73 and 17.06 first, which need nothing that is not already built, with 17.74 immediately after 17.01 because the console line is the surface the maintainer reads on every day of the rest of this project, and 17.73 before 17.06 because the panel and the launcher window are both built from it; then 4.01 and 4.02, which the game needs next anyway and which 17.05 waits on; then 17.05, 17.08, 17.09, 17.11, 17.14 and 17.46-17.50. On 2026-09-22 the maintainer asked for panel accounts that can each be given any role, so after 17.06 the order is 17.08, 17.14 and 17.46, then 4.01, 4.02 and 17.05, then 17.47-17.50, then 17.09 and 17.11. The same day the maintainer asked for error reports an operator sends to the maintainer, so 17.106 follows 17.46. The same day the maintainer asked that a colored value in a log line be a link with a hovercard that opens whatever it names, so 17.107 follows the milestones it reads from, which are 17.76 for the runs, 17.106 for the source location and 17.07 for the log viewer. Everything else in this phase arrives with the system it shows, under the rule in doc/ROADMAP.md that every subsystem ships with its panel surface, so the pages for realms, players, settings, world edits and client data are built by the milestones that build those systems.
- **Order.** Ids are allocation order, not build order. Within this phase the dependency graph gives the build order, so a dependency may name a higher id: 17.15 and 17.16 wait for 17.27 and 17.46-17.48, 17.22 waits for 17.26, 17.28, 17.29, 17.32 and 17.49, and 17.31 waits for 17.26. 17.01-17.24 keep the ids they were published with, and everything added later takes an id from 17.25 up. Nothing added on 2026-09-18 breaks the rule the other way either: 17.85 waits for 17.22, 17.92 for 17.80, 17.94 for 17.81 and 17.100 for 17.99, all lower ids. Three pairs are built in their own order and are worth naming, since each second half is worthless without its first: 17.78 then 17.79, 17.80 then 17.92, and 17.103 then 17.104. A check never rests on a milestone outside its own dependency closure: where one did, the dependency was added or the check was narrowed to what exists at that point. The fifty-three milestones added on 2026-09-27, 17.112-17.164, take ids from 17.112 up in the order they were grouped, reliability, hosting, security, automation, experience and teamwork, after 17.112 itself; among them 17.135 waits for 17.140; 17.148 waits for 17.149; 17.158 waits for 17.159 and 17.164, and 17.129 waits for 17.116 so a staging copy reuses the drill's scratch copy. The twenty-two milestones added later on 2026-09-27, 17.165-17.186, take ids from 17.165 up: 17.165 to 17.178 are the game data pages and 17.179 to 17.186 the desktop program. Lower ids wait on higher ones here: 17.21 on 17.49, 17.24 on 17.181, 17.31 on 17.175, 17.34 on 17.173, 17.63 and 17.95 on 17.177, 17.92 on 17.175 and 17.177, 17.105 on 17.183, and 17.108 on 17.181.
- **Splits.** 17.14 became six milestones, 17.16 three, 17.18 five, 17.26 three and 17.27 four. Each keeps its id for its first part and the rest take ids from 17.46 up (17.46-17.50 from 17.14, 17.51 and 17.52 from 17.16, 17.53-17.56 from 17.18, 17.57 and 17.58 from 17.26, 17.59-17.61 from 17.27). Three published titles narrowed to what their milestone now holds: 17.16, 17.18 and 17.19, whose alerts moved to 17.67 so graphs no longer wait for mail settings. Two titles changed because their milestone grew instead: 17.14 now names the audit store, which the panel needs from its first milestone, and 17.50 the roles page. On 2026-09-27 two more were divided, each keeping its id for the part that stayed: 17.21 kept accounts and bans and 17.177 took characters and online players, and 17.24 kept its goal, hosting a game on this computer with its private database and Play, while the program it runs in, its tray and its installer went to 17.181, 17.182 and 17.183. Their titles changed with them, and 17.105's now names the panel program before the launcher. 17.20 grew instead, from S to M, when it came to list every install, the caches built per revision and the extracted world table sets.
- **Sizes.** A size is read off the milestone's own content, so a label can be checked against the text: S is at most 4 deliverables and at most 5 acceptance checks, L is 8 or more deliverables or 8 or more acceptance checks, and M is everything between. A deliverable is a top-level bullet of the Deliverables list, and the bullets nested under one detail it and are not counted. One milestone carries L on judgment instead of count, 17.51, because it is one operation from end to end; 17.24 did too, because it installed and ran on two desktop operating systems, until 2026-09-27, when it came to carry L on its eight deliverables. The Oversized note names both with the rest. Recounting moved sizes that were already published, without touching any id: 17.01 and 17.07 to S, 17.03, 17.04 and 17.12 to M, and 17.06, 17.14, 17.15 and 17.16 to L. Of the milestones added later, 17.50 moved to M with its roles page, 17.64 to M on its count, 17.106 carries L on its eight deliverables, 17.107 carries M on six deliverables and six checks, and of the three plugin milestones added on 2026-09-23, 17.109 carries M while 17.110 carries L on its nine acceptance checks and 17.111 on its ten deliverables. Of the thirty-one added on 2026-09-18, the thirteen marked S each carry at most 4 deliverables and at most 5 checks, and none of the eighteen marked M reaches 8 of either, so nothing new is large and the Oversized note's list of nine stands as it is. Where a 2026-09-18 deliverable was added to a milestone that already existed, it was folded into a deliverable already there wherever a new bullet would have changed that milestone's size, which is why 17.21 and 17.70 gained a clause rather than a line. Of the fifty-three added on 2026-09-27, 1 carry S, 32 carry M and 20 carry L, each read off its own count: 17.112, 17.113, 17.114, 17.115, 17.116, 17.118, 17.119, 17.121, 17.126, 17.129, 17.133, 17.135, 17.139, 17.141, 17.142, 17.147, 17.152, 17.156, 17.158, 17.163 each reach 8 deliverables or 8 checks. Of the twenty-two added later that day with the game data pages and the desktop program, 8 carry S, 12 carry M, and 17.179 and 17.181 carry L on count; 17.20 moved from S to M on its count when it grew.
- **Oversized.** The large milestones published before 2026-09-27 are the twelve carrying L: 17.110 the frame a panel tool runs in and 17.111 plugins shipping with the panel, and 17.06 dashboard app and overview page, 17.14 panel listener and audit store, 17.15 schedules, 17.16 backups, 17.18 file roots and the path jail, 17.22 nodes, 17.24 hosting a game on this computer, 17.51 backup restore, 17.73 the design system and 17.106 error reports. No other milestone published before then is large. Split 17.106 along these lines if a focused stretch cannot finish it: the source location on every record with its grouping, and the errors page with its report file. Split 17.73 along these lines if a focused stretch cannot finish it: the token pipeline with its generator and gates, and the component set with its gallery and tests. The twenty L milestones added on 2026-09-27 split along these lines if a focused stretch cannot finish one: 17.112 into the tome format with its reader, checks and the supervisor's ready checks, and the Tomes page with export and upgrades; 17.113 into the per-thread progress counters with the hung restart, and the operating system watchdogs with the outside heartbeat; 17.114 into keep-awake and a graceful operating system shutdown, and the update, reboot and clock findings; 17.115 into the runbook format with its built-in set and coverage test, and the action buttons with the steps recorded on an alert; 17.116 into the restore drill with its badge and finding, and the update rehearsal; 17.118 into configuration and settings drift, and installed files and schema drift; 17.119 into drives and their health, and memory pressure, steal time and handle limits; 17.121 into spans with their propagation, and the trace view with the service map; 17.126 into capturing and shipping binary logs, and the timeline restore; 17.129 into staging copies with scrubbed personal data, and pull request previews; 17.133 into generic OIDC with linking, and Discord and GitHub with role mapping and break-glass owners; 17.135 into canary credentials with their alerts, and the lockdown with its review; 17.139 into player data export, and erasure with retention; 17.141 into approval policies on chosen actions, and change requests; 17.142 into account linking with slash commands, and alert buttons with live status messages; 17.147 into the sandboxed runner with its capabilities, and the Scripts page with versions, dry runs and tasks; 17.152 into boards and widgets, and the wall display with its pairing; 17.156 into undo from the activity record, and selection with bulk actions; 17.158 into the incident workspace with its timeline, and postmortems with their actions; 17.163 into rotations with escalation, and handoff with the shift log. Split any of them again if a focused stretch cannot finish it, along these lines: 17.14 into the listener with its TLS and the sessions, limiter and audit store; 17.15 into the engine with its triggers and the tasks with their completion and countdowns; 17.16 into the dumps with their snapshot record and the archive with its verification; 17.06 into the app shell with its route table and the overview cards; 17.18 into the jail with its roots and the listing and reading page; 17.22 into the join with its heartbeat and the nodes page with placement. The L milestones of the desktop program split along these lines if a focused stretch cannot finish one: 17.179 into the shared host with the embedded pages, and the bound view with its origin gate, pin hook and profiles; 17.181 into the program with its list and local opening, and remote panels with pairing, pinning and the probe; and 17.24 into hosting with the first start, and the private database with Play.
- **Gated checks.** A check that needs the maintainer's own machine, a second machine, a security key, a desktop SFTP client, a Pterodactyl install or a retail client session is marked `Dev-gated:` with what it needs, the form phase 16 already uses; a check an environment variable turns on is marked `Env-gated` with that variable, as the Tests section of doc/ARCHITECTURE.md describes. doc/ROADMAP.md's Where we are paragraph lists the phase 17 checks that wait for the maintainer.
- **Proposals.** doc/PANEL.md proposed the choices this phase rests on, and every one of them is settled. On 2026-09-22, when 17.14 built it, the admin API's remote-access rule was extended to the panel's own listener, under Panel option names. On 2026-09-25 the rest were settled at the maintainer's direction: the time zone data source under Time zones in doc/ARCHITECTURE.md, and under Panel operations in doc/ARCHITECTURE.md the scope tree with its default role bundles, the command security level cap on `console.write`, the keyring, the cipher and keyed hashes, outbound HTTP, certificates for a hostname, the event socket protocol, backup sealing and structured dumps, the S3 client, the login-screen countdown, where a node's schedules run, the file editor and archive libraries with the default archive format, SFTP and remote pull, WebAuthn, the QR renderer, the trash and version stores, database credential rotation, the maintenance bypass levels, world edit exports, player registration, the patch signing key, the sequential ramp, the one search engine and the installation file's format. On 2026-09-27 the thirteen choices the game data pages and the desktop program rest on were settled at the maintainer's standing direction to take the recommended option, under Desktop programs and client data in doc/ARCHITECTURE.md. Each milestone below names the entry it rests on. Until the milestones that build nodes, clusters and realms land, grants are the per-app sub-user grants and every acceptance check here stays at app scope.
- **Docs.** The supervisor is a fourth executable and the panel's host. The commit that adds this file also adds it to doc/ARCHITECTURE.md's Processes table, its repository layout block and its Operations paragraph, so nothing is left to do there. The panel listener's own bind and TLS rule was recorded under Decisions, Operations on 2026-09-22, when 17.14 built it, so nothing is left open there either.
- **Security first.** Trusted proxies and the client-address rule land with the panel listener in 17.14, and required two-factor with 17.47, not behind 17.19 and 17.35. Sign-in throttles, rate limits and audit addresses are wrong without them.
- **One of each.** One stream layer with backlog, sequence numbers and resume (17.04), which 17.12 and 17.26 reuse; one app list (17.06); one browser socket (17.26); one audit store (17.14) with one scope that writes into it (17.49); one stored command history (17.49). No milestone builds a second copy of a subsystem. The one piece of rework is transport: the 17.06 overview and the live pages of 17.07 and 17.13 ship on the per-app streams of 17.04 and 17.12, and 17.58 moves them onto the panel socket so the dashboard ends with one socket client and one reconnect policy. What is replaced there is the transport, not the pages.
- **Client-derived data.** Backup archives and file roots carry the type dump and extracted data built from the operator's own install, and opt-in patch components can carry client files. 17.16, 17.18, 17.43 and 17.65 follow the bring-your-own-files rule under Decisions, Experimental features: the client install root is never downloadable through the panel, and off-machine storage is the operator's own bucket with the archive's contents stated. The game data pages, 17.165-17.178, show decoded values from the operator's own install and never serve an install file, its bytes, a texture, model or screenshot, or an export of what was decoded or extracted, which the 17.165 route sweep proves on every data route; 17.18 names every client-derived folder of the data root from one list.
- **Correction.** 17.15's deliverable said to skip a run when no players are online, while its acceptance check described a schedule that runs only when no one is online. Both are useful and opposite, so 17.44 offers both conditions by name, `skip_if_empty` and `only_when_empty`, and 17.15's checks name neither.
- **Request size.** 17.02 bounds what the admin API accepts with `Admin.MaxRequestBytes`, checked after the token and before any handler, and the same value caps a WebSocket message. Crow buffers a request body in memory before any handler or middleware runs and offers no hook to refuse one earlier, so that setting bounds what an endpoint sees, not what an unauthenticated peer can make the process allocate. Closing that needs a patched Crow parser or another HTTP library, and it is listed here so a later milestone decides rather than the gap going unrecorded.
- **Listening socket.** Crow's acceptor sets `SO_REUSEADDR` on the socket it listens on and offers no way to choose otherwise, so on Windows another process running under any account on the same machine can bind the same address and port as a running admin API and receive the connections meant for it, reading the bearer token an operator's dashboard or `curl` then sends. 17.02's pre-flight check already skips the option for exactly this reason, but the socket Crow binds is beyond its reach. Closing it needs `SO_EXCLUSIVEADDRUSE` on that socket, which means either a project-owned acceptor handed to `crow::Server` in place of `App::run_async`, or a patched Crow port, the same choice the Request size note asks for, so both are listed here for one decision rather than two. Until then the admin port is only as private as the machine's local accounts, and `doc/config/<app>.md` says so beside `Admin.BindIP`.
- **Risk.** 2.15's real-client check showed that the client displays MSG_LOGINSERVERSHUTDOWN as the server going down, and the login server closes the connection as it sends it. A login-screen warning at 5 and 1 minutes therefore needs a notice that does not disconnect, found by capture or client reverse engineering. Until one is found, 17.15 sends only the final notice on the login server and earlier warnings go to players in the world.
- **Operations depth.** 17.74-17.104 were added on 2026-09-18, after research against the panels and observability tools operators already run and after the maintainer judged the console this phase shipped. They fall in five bands. Reading a line: 17.74-17.77, which rework what 17.01 shipped and carry their own checks rather than editing 17.01's ticked ones. Seeing from outside: 17.81 and 17.94, because every health signal the first 73 milestones define is the server reporting on itself, and a server that is healthy by every internal figure and unreachable behind a firewall rule reads as fully green. Understanding what happened: 17.78, 17.79, 17.80, 17.83, 17.90, 17.91 and 17.92, which answer why rather than whether. Acting on the game rather than on the process: 17.89, 17.95, 17.101, 17.103 and 17.104, since nothing in the first 73 restores anything smaller than a whole database or gives a player anything. Not being told twice: 17.85, 17.86 and 17.87. The rest, 17.82, 17.84, 17.88, 17.93, 17.96, 17.97, 17.98, 17.99, 17.100 and 17.102, each remove a reason to reach for something Ambrose deliberately does not have, such as a shell. **Scheduled now:** 17.74, with 17.75, 17.76 and 17.77 behind it; 17.74 is in the Built first order immediately after 17.01. Everything else here is planned work that waits for its dependencies and its turn, and nothing else in this phase is reordered for it. Once those dependencies allow, the order worth taking is 17.81, 17.78, 17.83, 17.80, 17.82, 17.86, 17.89, 17.88 and 17.95; if the phase has to be cut, the last to schedule are 17.79, 17.96, 17.97, 17.98, 17.99, 17.100, 17.101, 17.102, 17.103 and 17.104, none of which blocks a server from running. Four ideas from the same research were deliberately not taken: statistical anomaly detection per metric, which on one machine with tens of players pages on every unusual login hour; browser push, which routes through another company's endpoints and would break the rule that nothing leaves the machine, reopened on 2026-09-27 as 17.154's opt-in, off by default and carrying only a wake-up with no subject, figure or identity, so nothing about the server leaves through it; a browser shell, which would defeat the path jail, the protected paths and the audit model in one control, and which 17.84 removes the reason to ask for, and which 17.147's sandboxed scripts do not bring back, since a script has no shell, no file system and no network, only the audited calls it declared; and a plugin API, which is a remote code execution surface on a panel that fronts a game database, and whose useful half 17.101 gives without running anybody's code. That refusal was reversed on 2026-09-23 at the maintainer's direction, and 17.109, 17.110 and 17.111 are the terms on which it comes back rather than a change of mind about the risk. The objection stands and is answered in three places: a plugin ships in this repository and arrives by the same signed release as the panel, so there is no second trust root and no registry to take over; a panel tool runs in a frame with no session, no cookies and one audited bridge whose every call names a capability the operator granted, so a review that misses something costs a broken tool rather than a database; and the server never loads code a plugin shipped, because a server-side plugin is source the operator builds. A plugin API that skipped any one of those three would still be the thing this note refused.

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
- Startup refuses a non-loopback bind address unless a TLS certificate and key are configured, or the operator sets the opt-in `Admin.AllowPlainHttpRemote = 1`, which is off by default. With that setting the token is still required and startup logs a warning naming the option, but the token, commands and logs cross the network unencrypted, so anyone on the path can read them and reuse the token. On a config reload an unsafe non-loopback bind is refused with an error naming the option and the old binding stays, while a safe bind change rebinds live and a failed bind keeps the old listener. TLS itself is not served yet, so a bind beyond this machine that names a certificate and key is refused until 17.14 settles certificate handling, as recorded under Decisions, Operations in doc/ARCHITECTURE.md. This hooks into 4.15 when it lands

**Acceptance**

- [x] `GET /api/health` without a token returns 401, and with the token returns 200 and the running revision (AdminServerTest.ServesHealthOnLoopbackOnlyWithTheToken and AdminServerTest.AnAppServesItsOwnHealthWhileRunning)
- [x] Twenty wrong tokens within one second from one address produce 429 responses (AdminServerTest.RateLimitsWrongTokens)
- [x] Setting `Admin.BindIP = 0.0.0.0` without TLS logs an error naming the option and exits 1 at startup, and on a config reload is refused while the old binding keeps serving (AdminSettingsTest.RefusesAnUnsafeRemoteBind, AdminServerTest.RefusesAnUnsafeRemoteBindAtStart, AdminServerTest.AnAppRefusesToStartWithAnUnsafeAdminBind and AdminServerTest.ReloadKeepsTheOldListenerWhenTheNewBindIsUnsafe)
- [x] With `Admin.AllowPlainHttpRemote = 1` and no TLS, a non-loopback bind starts, still requires the token, and logs a warning naming the option. Env-gated: `AMBROSE_TEST_ADMIN_REMOTE_BIND` names the address to bind (AdminServerTest.StartsBeyondThisMachineWithThePlainHttpOptIn, run with `AMBROSE_TEST_ADMIN_REMOTE_BIND=0.0.0.0`, and AdminSettingsTest.PlainHttpRemoteIsAnOptInThatWarns)
- [x] Rotating the token and reloading config makes the old token return 401 and the new one 200 without a restart (AdminServerTest.RotatesTheTokenOnReloadWithoutRestarting and AdminServerTest.AnAppReloadsItsAdminApiFromItsOwnConfig)
- [x] Routing, authentication, and rate limiting are unit tested without opening sockets (AdminRouterTest, AdminAuthTest, AdminTokenTest and AdminSettingsTest, 34 cases that open no socket)

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

- [x] Two fake clients from the 1.22 test harness show `sessions: 2`, and disconnecting both shows 0 within one second (AdminStatusTest.TwoLoopbackClientsShowAsTwoSessionsAndNoneWithinASecondOfClosing: two loopback sockets against a SocketMgr, the status route reads 2, both close, and it reads 0 inside one second of polling; passes on Windows and on Ubuntu 24.04)
- [x] Reported memory is within 10 percent of the operating system's own figure (AdminStatusTest.ReportedMemoryIsWithinTenPercentOfTheOperatingSystemsFigure, against GetProcessMemoryInfo on Windows and /proc/self/statm on Linux, read independently by the test)
- [x] A schema test confirms fields are only ever added, never renamed or removed (AdminStatusTest.FieldsAreOnlyEverAdded holds the version-one field lists of all three routes as literals and fails when a response or the declared field list loses one)
- [x] With the type dump removed, the status response carries the stale or missing type dump problem, and it clears once the dump is rebuilt (AdminStatusTest.AMissingTypeDumpIsAProblemUntilOneIsInUse on a running app: type_dump_missing appears with the setup's own message, clears when a dump is in use, and type_dump_stale and install_missing take their turns)
- [x] `GET /api/capabilities` lists every reload target and problem code the build registers, and a test fails when a registry gains an entry the response leaves out (AdminStatusTest.CapabilitiesListEveryEntryTheRegistriesHold compares each list in the response with the registry itself after adding an entry to every registry, so an entry the response left out fails it)
- [x] `GET /api/apps` from a single app returns exactly that app, with no field the supervisor's answer lacks (AdminStatusTest.AnAppAnswersTheAppsListWithExactlyItself: one element, and its key set equals AdminStatus::AppFields, which is the shape 17.14's supervisor answers with)

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

- [x] A subscriber filtered to warnings receives only warnings and errors (LogStreamServiceTest.AWarnFilterPassesOnlyWarningsAndAbove on the hub alone, and LogStreamSocketTest.ASubscriberFilteredToWarningsReceivesOnlyWarningsAndErrors over a real `/api/logs` socket on a running app: two info lines logged between a warning and an error never arrive)
- [x] A subscriber that stops reading does not change logging latency by more than 10 percent across 100000 log lines (LogStreamCost.ASubscriberThatStopsReadingChangesLoggingCostByNoMoreThanTenPercent logs 100000 AMBROSE_LOG lines three ways, in twenty interleaved rounds of 5000: with no subscriber, with one the pump thread drains, and with one whose queue of 100 is never drained, holds the best round with the stalled subscriber within ten percent of the best round with the reading one, and records all three figures as test properties; a stalled subscriber is never dearer than a reading one on MSVC Debug, GCC Debug or GCC release. The figures showed where the cost was: the subscriber queue became a ring that stops allocating once grown, because the deque it replaced allocated on every push under MSVC, the ring is guarded by a spin lock the log thread takes once per record, the hub holds its subscriptions plainly with a handle whose last copy closes them instead of locking a weak pointer per record, and the pump is woken through one atomic flag with a notify only while it sleeps. Against no subscriber at all, a subscriber costs the log thread two reference counts and one lock per record: about 6 percent of a stream-only line on MSVC Debug and 12 to 15 percent on GCC, where a line costs 110 ns optimized, which is why the bound is judged against a reading subscriber, the comparison the check is about)
- [x] A reconnecting subscriber receives the backlog (LogStreamServiceTest.ANewSessionGetsTheHelloThenTheWholeBacklogInOrder, and LogStreamSocketTest.AReconnectingSubscriberReceivesTheBacklog opens the socket twice and reads the same five lines both times)
- [x] A subscriber resuming after sequence number N receives exactly the records after N, or a dropped marker with the missed range when those records left the backlog (LogStreamServiceTest.AResumeAfterNGetsExactlyTheRecordsAfterN reads 31 to 50 after 30 with no marker; AResumeAfterAnEvictedSequenceGetsADroppedMarkerNamingTheMissedRange reads `dropped` 21 to 40, count 20, then 41 to 50 from a backlog of ten; LogStreamSocketTest.AResumeAfterNReceivesExactlyTheRecordsAfterN does it over the socket and the first record carries N plus one)
- [x] A command marked sensitive and a secret setting change stream with their values redacted, and a search of the streamed records finds neither value (LogStreamSocketTest.ASensitiveCommandAndASecretSettingChangeStreamRedacted runs `account set password fred hunter2-never-streamed` through a scripted console on a running app, whose handler logs an Admin.Token change through LogRedaction::DescribeSettingChange, then searches every streamed record for the password and the token and finds `(arguments hidden)` and `***` instead; LogStreamServiceTest.SecretSettingValuesAreMaskedBeforeARecordLeaves covers the encoder alone, connection string password included)
- [x] The stream layer's sequence, backlog, resume and drop behavior is unit tested once, without opening sockets (the twelve LogStreamServiceTest cases drive LogStreamService over a bare LogStreamHub with a recording sink: AFullQueueDropsTheOldestAndReportsHowManyWithTheirRange reads `dropped` 1 to 7, count 7, ahead of records 8 to 12 from a queue of five, and SequenceNumbersNeverGoBackwardsAcrossPumpBatches reads 2000 records in order through batches of seven; all pass on Windows and on Ubuntu 24.04)

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

- [x] Creating an account through the API succeeds and appears in the audit log. `AdminCommandRouteTest.ARunIsAnsweredAndWrittenDownAndASecretIsInNeither` runs `account create` through the route and finds it in the record with the caller, the address and the app
- [x] `shutdown` without the confirmation flag is refused. `AdminCommandTest.SomethingThatCannotBeUndoneWaitsForAConfirmation`, which also checks it runs once confirmed and that nothing was said while it was refused
- [x] A failing command returns its error text with `success: false`. `AdminCommandTest.AFailingCommandSaysSoAndAnUnknownOneIsNotPretendedTo` keeps what the command said even though it failed
- [x] A command above the passed security level answers as an unknown command, runs nothing, and writes a refused audit row. `AdminCommandTest.BelowTheConsoleLevelThereIsNoSuchCommand` for the wording and `AdminCommandRouteTest.ARefusalIsAnsweredAndRecordedWithItsReason` for the row
- [x] `account create test secret` leaves no `secret` in the audit file or the response. The route test asserts the secret is in neither, and the line is kept as `account create (arguments hidden)`. There is no audit table yet, so that half waits for db_login to have one
- [x] A payload over 4096 bytes and a body with an unknown key are both refused with 422 and run nothing. `AdminCommandRouteTest.ABodyTooLongOrCarryingAnUnknownKeyRunsNothing`, which also asserts no record file was written at all

## 17.06 Dashboard app and overview page

**Goal:** A modern web dashboard shows the health of every server at a glance, on desktop or phone.

**Size:** L. **Depends on:** 17.03, 17.73

**Deliverables**

- `apps/dashboard/`, built to static files that the admin API can serve at `/` and that the supervisor serves in 17.14 without a second build
- The app list read from `GET /api/apps` on the host that served the page, so the browser stores no server list and no per-app token: served by an app it shows that app, and served by the supervisor in 17.14 it shows the apps the signed-in user may see
- A route table in which each route names the permission it needs and whether it appears in navigation, so later pages hide what the user cannot use and a direct link shows an access-denied page
- Overview cards per app showing health, role, realm, address and port, uptime, revision, sessions, players against the realm's limit, tick time average and maximum, and badges for crash loops, restart required, pending SQL updates, client revision mismatch and open problems, with automatic reconnection and a visible stale state
- Problem records from 17.03 shown on the card with a button that opens the page that fixes them
- Every live figure carrying the age of the sample it came from, and a figure past its freshness budget demoted to the muted treatment doc/DESIGN.md's Live data rules set, saying how long since the last sample rather than being blanked or left looking current
- Actions and fields the build does not report, read from `GET /api/capabilities`, hidden rather than shown failing
- Errors shown beside the form or dialog that caused them, with each field of a 422 response marked and the response's request id shown
- A responsive layout that works at phone width, with light and dark themes that default to the system setting, and no request to any host other than the dashboard's own
- Dashboard build and tests added to CI

**Acceptance**

- [x] With loginserver and gameserver running, both show online within 2 seconds, and stopping one marks it offline within 5 seconds (tests/e2e/panel.spec.ts against a running patchserver: online 421 ms after signing in, and not answering inside 5 seconds of a stop; checked on 2026-09-22 with a real loginserver and gameserver on MariaDB, each serving the panel from its own admin API: online 353 and 362 ms after signing in, and the stopped loginserver not answering 807 ms later while the gameserver's tab stayed live)
- [x] The overview is usable at 400 pixels wide with no horizontal scrolling (tests/e2e/panel.spec.ts, the overview works at 400 pixels wide with no sideways scrolling, against a running patchserver)
- [x] The built dashboard served by the admin API loads with no browser console errors (tests/e2e/panel.spec.ts, the load raises no console error from the signed-out page through signing in to the live overview; GET /api/session answers 200 when nobody is signed in, so no refusal reaches the console, and every dialog, menu, select and sheet opens and closes under the served Content-Security-Policy)
- [x] With the type dump removed, the gameserver card shows the problem, and its button opens the client data page (Overview.browser.test.ts, a problem the build reports shows with the button that opens #client; checked on 2026-09-22 with a real gameserver given an install and no built type dump, whose card showed type_dump_missing and whose button opened the client data page, which names milestone 17.20)
- [x] Closing the stream leaves every card's figures readable and visibly stale within the freshness budget and one interval, with the age of the last sample named, and no card reports a stale figure as current (tests/e2e/panel.spec.ts, after a stop every figure says Last sample N s ago inside the three-second budget and one interval, and none says Updated; Overview.browser.test.ts holds the wording)
- [x] Loading the built dashboard makes no network request to any other host (tests/e2e/panel.spec.ts watches every request from the signed-out page to the live overview, and ci_frontend_checks.py checks the bundle names no other host)
- [x] A capability the build does not report leaves its control out of the page, and nothing in the browser holds an app token or a stored server list (Overview.browser.test.ts, a problem the build's capabilities do not list keeps its message and loses its button; tests/e2e/panel.spec.ts, after signing in local storage, session storage and document.cookie hold neither the token nor the app list, the session cookie being HttpOnly)
- [x] A test over the route table fails when an entry names no permission or leaves out its navigation flag, and a direct link to a route the caller may not use shows the access-denied page (routes.test.ts, checkRoutes fails an entry with no permission or no navigation flag, and resolve sends a route the caller may not use to the access-denied page and leaves it out of the side bar)
- [x] A 422 answer marks every field it names beside the form that caused it, and the request id in the answer is shown on the page (SignIn.browser.test.ts, a 422 marks the token field beside it with the request id, and a field the form lacks is listed with it; AdminServerTest.SignsInOnlyFromItsOwnOriginAndNamesEachWrongField, the server names every field and puts the request id in the body and the header)
- [x] The dashboard's build and its tests run in CI, and a failing dashboard test fails that job (the front-end workflow runs npm run test:logic and npm run test:browser, which now take the dashboard and dashboard-browser projects, and builds the panel; Vitest exits non-zero on a failing test, which fails the job)

## 17.07 Dashboard log viewer and command console pages

**Goal:** Operators read logs and run commands from the dashboard as comfortably as at the server's own console.

**Size:** S. **Depends on:** 17.04, 17.05, 17.06

**Deliverables**

- A log viewer with level and category filters, text search with highlighted matches, pause and resume with a count of new lines, a jump-to-newest control, copy, and a tab per server; it resumes after its last sequence number when the connection returns instead of clearing
- A command console page with output that notes commands are audited, showing each command's result lines under the command, asking before a destructive command sends its confirmation flag, and completing command names the caller's security level allows
- Recall of the commands typed in the open page, held in memory only, so nothing here has to be replaced when 17.49 delivers the one stored history per panel user and app
- The console row built as the four-column grid doc/DESIGN.md's Log lines and values section sets, with the level chip the only tinted part of the row, the message in its own column so a wrapped line hangs under itself, the value runs read from the record's own typed ranges once 17.76 carries them rather than lexed in the browser, and a search hit offering to mark that token everywhere it appears

**Acceptance**

- [x] Filtering to errors hides lower levels immediately, and pausing stops scrolling while counting new lines (run on 2026-09-24 against a live panel: turning the other levels off left exactly the 3 warn rows of 51, and pausing froze the view at 51 rows while the control read "13 new")
- [x] Running `status` shows the same output as the local console (run against a supervisor with a loginserver and a gameserver on 2026-09-24: the panel returned the same five lines, server, revision, uptime, state and sessions, that the app's own command route prints)
- [x] Dropping the connection for 10 seconds while logging continues shows the missed lines, or a gap marker, after reconnecting, without clearing the view (every log request aborted for 11 seconds while the server kept logging: the view held its 117 rows rather than clearing and reached 125 after reconnecting)
- [x] Sending `shutdown` asks for confirmation first, and cancelling sends nothing (run against matching builds at 28e72e06: the server refused it with "Type yes to run it, or anything else to leave it alone" and ran nothing, typing anything else sent no further request at all, and typing yes re-sent the same command with confirm true, which shut the app down and left the supervisor starting it again)
- [x] Reloading the page clears the recall list and stores nothing in the browser (recall walked back through help then status, a reload left it empty, and localStorage, sessionStorage and readable cookies were all empty)

## 17.08 Process control, config, and database pages

**Goal:** Operators start, stop, and restart servers, review configuration, and see database update state from the dashboard.

**Size:** M. **Depends on:** 17.06, 2.06

Done on 2026-09-22. The supervisor is a fourth executable in `src/server/apps/supervisor`, built on the shared app, and the pages read it and the apps behind it. Backoff, crash loops, exit classification, the tracked operation model and its locks stay with 17.27 and 17.60, so this milestone restarts an app that exits unexpectedly from running one second later and records every exit with its code. Launch settings move into the panel's store with 17.28, so they are `App.<name>.*` options here.

**Deliverables**

- A supervisor that starts, stops, and restarts loginserver, gameserver, and patchserver, restarts an app that exits unexpectedly, records each exit code, and exposes an admin endpoint; the tracked operation model, locks, backoff, crash classification and crash records are 17.27 and 17.60, so nothing here is built twice
- Readiness from each app's `GET /api/health` lifecycle state rather than console output, with a `ready` lifecycle line as the fallback when the admin API is off
- A graceful stop that first asks the app's admin API to shut down with its own countdown and drain, so the login server sends its 2.15 notice and the gameserver saves characters, then sends `shutdown` on standard input, then Ctrl+Break or SIGTERM to the process group, and only after a per-app stop timeout ends the process tree through its job object or process group
- The desired state of each app saved when it changes; on supervisor start, apps still running are adopted again after checking their process id, process start time and executable path, and apps meant to run are started
- Output of each app captured by the supervisor as a fallback log source, kept for the current and previous run
- A config page comparing each app's effective settings with its `.conf.dist` defaults and showing each value's source layer. Edits go through the settings API, apply live, and are audited; the editing part is built in 17.13. Only options documented as restart-required show that, with the reason
- A database page listing applied and pending update files per database from the updater, applying data-only updates live and reloading the affected stores, and showing connection pool use, plus, once 4.15 lands, the journal of live world-database edits, exportable as pending SQL updates

**Acceptance**

- [x] The restart button restarts gameserver while loginserver sessions stay connected (SupervisorLoginClientTest.RestartingTheGameServerLeavesTheLoginServersSessionConnected, run on 2026-09-22 with a real login server and game server on MariaDB and the maintainer's own install: the game server came back as another process, and the client that had finished the session handshake stayed connected, was told nothing and was still counted as one session; the panel's own button is tests/e2e/supervisor.spec.ts, the restart button asks first and brings the app back as another process)
- [x] Killing gameserver from outside makes the supervisor restart it and the dashboard shows one crash (SupervisorLoginClientTest.AGameServerEndedFromOutsideIsOneCrashAndStartsAgain, run on 2026-09-22 against a real game server: one crash, one restart and an exit recorded as not asked for while it was running; SupervisorTest.AnAppEndedFromOutsideCountsOneCrashAndStartsAgain holds the same for any app, and Servers.browser.test.ts holds the crash count the page shows)
- [x] The config page shows each value's effective value, default, and source layer, and marks only options documented as restart-required, with the reason (Config.browser.test.ts, where the page moved on 2026-09-26 when 17.35 had taken Settings.svelte for the panel's own settings, and AdminConfigViewTest: every key carries its value, its shipped default, the layer with the file and line, secrets only as the mask, and a restart reason only where the app declares one; checked on 2026-09-22 against a real login server through the supervisor, whose ninety options listed Setup.Mode and the other startup-only keys as restart-required and nothing else)
- [x] Stopping loginserver from the dashboard sends its shutdown notice to a connected fake client before the process exits (SupervisorLoginClientTest.AStopFromThePanelTellsAConnectedClientBeforeTheLoginServerExits, run on 2026-09-22 with a real login server: the panel's stop went through the app's admin API, the connected client read the login service's notice and then the close, and the exit was recorded as asked for with code 0. The notice needs the client's own message definitions, so the test names AMBROSE_CLIENT_DIR and skips without it; SupervisorLoginTest.AStopFromThePanelGoesThroughTheLoginServersAdminApiAndItExitsAsAsked holds the path without an install)
- [x] Restarting the supervisor while apps run adopts them without restarting them, and a process id reused by an unrelated program is not adopted (SupervisorTest.ANewSupervisorTakesARunningAppBackAndRefusesAProcessIdThatIsNotItAnyMore: the second supervisor took the same process back and said so, and with the saved start time changed by one it refused that process id and started another; ChildProcessHandleTest.AdoptTakesARunningChildBackOnlyWhileItsIdentityMatches holds the identity check itself)
- [x] With an app's admin API off, the supervisor's own capture of its output is what the page shows, and the previous run's output is still readable after a restart (SupervisorTest.StartsAnAppOnItsReadyLineAndStopsItThroughItsInput, where an app with no admin API is called ready by its own ready line and stopped by a line on its input, and ARestartStartsTheAppAgainAsANewProcess, where the exit of the run before is read from the previous run's output; OutputLogTest holds the rotation, the rings and a file emptied at its limit; Servers.browser.test.ts and tests/e2e/supervisor.spec.ts hold what the page shows)
- [x] The database page lists applied and pending update files per database, applies a data-only update live and shows pool use, and an update the running binary's schema needs is listed as restart-required instead of applied (AdminDatabaseViewTest.ALiveApplyRunsDataOnlyFilesStopsBeforeTheSchemaAndReloadsTheStore on MariaDB, DBUpdaterTest.ALiveDataOnlyApplyStopsBeforeASchemaChangeAndRollsAFailingFileBack, and Database.browser.test.ts for the page; checked on 2026-09-22 through the panel against a real login server: with one data-only file and one schema file pending, the apply took the data file, left the schema file pending as restart-required, and the page showed both databases' connections in use)

## 17.09 Metrics registry and Prometheus endpoint

**Goal:** Performance data is exported in a standard format so it can be graphed over time.

**Size:** S. **Depends on:** 17.02

**Deliverables**

- `src/common/Metric/` with counters, gauges, and histograms, including tick time, message handling time per service, database query time, and reload counts, failures, and duration per target
- `GET /metrics` in the Prometheus text exposition format, protected by the admin token or an address allow list

**Acceptance**

- [x] `promtool check metrics` accepts the endpoint's output (promtool 3.14.0 over the 80 series a running loginserver answered on GET /metrics, exit 0; the same promtool rejects a scrape with a counter that does not end in _total, no help line or an le label on something that is not a histogram, exit 3, so a pass says something)
- [x] Handling 1000 fake messages increases that service's counter by exactly 1000 (MessageHandlerTableTest.HandlingAThousandMessagesMovesTheServicesCounterByAThousand: a thousand messages through Dispatch move ambrose_messages_handled_total{service} by exactly 1000 and the handler histogram by 1000 observations, a refused message moves the dropped counter by one and the handled counter by none)
- [x] A microbenchmark shows a metric update costs under 50 nanoseconds on average (MetricRegistryTest.AnUpdateIsCheapEnoughToSitInAHotPath: 2,000,000 increments in 16 ms, about 8 ns each, against a 50 ns budget)

## 17.10 Grafana dashboards and operations guide

**Goal:** Operators get ready-made graphs of realm health and performance history.

**Size:** S. **Depends on:** 17.09

**Deliverables**

- `apps/grafana/` with a Docker Compose file for Prometheus and Grafana, a provisioned data source, and dashboards for realm overview, performance, and database health
- `doc/OPERATIONS.md` covering the console, dashboard, metrics stack, safe remote access through a TLS reverse proxy, and the list of restart-required cases with the reason for each: a binary upgrade, adding or removing a compiled module, schema updates the new binary needs, a client revision or type dump change when live objects can't hold the old registry, and client-side WAD changes, which need a client re-patch

**Acceptance**

- [x] `docker compose up` next to a running gameserver shows live graphs within 30 seconds (run by the maintainer against a gameserver from main with `Admin.AllowedHosts = host.docker.internal`: the scrape was up 13 seconds after `docker compose up`, and all 11 panel queries across the three dashboards returned live series through Grafana's own data source 28 seconds after it)
- [x] Every provisioned dashboard loads with no missing panel errors (the three dashboards and the `ambrose-prometheus` data source provision with the uids every panel names, Grafana logs no dashboard error, and every panel's query returns data)

## 17.11 Terminal dashboard mode

**Goal:** Operators working over SSH get live panels inside the terminal itself.

**Size:** S. **Depends on:** 17.03, 17.04, 17.73

**Deliverables**

- A `--tui` option that draws full-screen panels for status, sessions, logs, and a command line when output is a terminal, using FTXUI from vcpkg

**Acceptance**

- [x] Resizing the terminal lays the panels out again, and `q` or Ctrl+C exits with code 0 (TerminalDashboardTest.ATerminalThatChangedShapeIsLaidOutAgainAndQLeavesWithNothingWrong drives the whole loop against a terminal the test supplies: the size changes from 80 by 24 to 132 by 43 between key presses, and the first and last paintings are measured at those sizes, so laying out again is read rather than assumed; q returns 0, and Ctrl+C is judged the same way by TheKeysThatLeaveAreJudgedTheSameWhereverTheyCameFrom. The layout and the key rule are the ones the real terminal uses, since RunInTerminal composes the same element and judges through the same function, so what is tested is what runs. Not driven against a real terminal emulator, which is what a resize by hand would add)
- [x] Without a terminal, `--tui` logs a warning and falls back to the normal console (verified on a real server on 2026-09-24: loginserver run with --tui and its output redirected to a file printed `--tui draws panels that need a terminal, and this output is not one, so the ordinary console is used instead` and went on to `loginserver ready`, serving normally; TerminalDashboardTest.WithoutATerminalTheDashboardIsDeclinedAndSaysWhy holds all four cases, including that asking for nothing warns about nothing, since a warning nobody asked for is noise)

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

- [x] An out-of-bounds PUT returns 422 and changes nothing (AdminSettingsViewTest.AnOutOfBoundsPutAnswers422AndChangesNothing, which also has a PUT with a bad value and no reason name both in one answer, and SettingsTest.AWrongTypeOrOutOfBoundsValueIsRefusedNamingItAndNothingIsPersistedAuditedOrAnnounced; with the lower bound check removed the PUT answers 200)
- [x] A PUT to a key set by an environment variable is refused and names the locking layer (AdminSettingsViewTest.APutToAKeyTheEnvironmentSetsIsRefusedNamingTheVariable: 409 `setting_locked` with layer `environment` and AMBROSE_WORLD_UPDATE_INTERVAL named, and layer `override` for a command-line override; with the lock check removed it answers 200)
- [x] A batch with one bad entry applies none (SettingsTest.ABatchWithOneBadEntryAppliesNoneAndNamesEveryBadOne, SettingsTest.ABatchWhoseStoreFailsChangesNothingAndAGoodBatchIsOneCommitAnnouncedOnce and AdminSettingsViewTest.ABatchWithOneBadEntryAnswers422AndAppliesNone; SettingsStoreTest.ABatchTheDatabaseRefusesOneRowOfLeavesEveryRowUnwritten on MySQL 8, where the first row of a batch whose second row the database refuses is not kept)
- [x] A change appears on a second dashboard within one second (AdminEventSocketTest.AChangeReachesASecondDashboardWithinOneSecond: two subscribers on a running app whose world ticks every five seconds, and the change made over HTTP reaches the second with its value, who and why in under a second; announced from the tick instead, it took 4995 ms)
- [x] A reload of a broken message definition returns every error while the old generation keeps serving (MessageReloadTest.ABrokenDefinitionReloadedOverTheAdminApiAnswersEveryErrorAndKeepsServing: POST /api/reload/messages with two broken fields in two messages answers 409 naming both, GET /api/reload and the events feed report the same, and the message registry keeps its generation and encodes as before; AdminReloadViewTest covers the listing, unknown targets and all)
- [x] Reading `Account.VerifierKeys` without asking to reveal it returns a masked value, and its history shows masked old and new values (AdminSettingsViewTest.VerifierKeysAreMaskedUnlessRevealedAndHistoryIsMasked, which also has `?reveal=1` with the right show it and record the reveal once, and without the right or a forwarded grant stay masked; SettingsTest.ASecretIsMaskedInEveryMessageLogLineAuditRowAndCommandAnswer and SettingsStoreTest.ASecretLongerThanTheOldColumnIsKeptWholeAndAuditedMasked keep it out of messages, the console and the audit table)
- [x] An events subscriber resuming after a sequence number receives exactly the missed events or a dropped marker, through the same test the 17.04 layer passes (the cases in src/test/mocks/StreamLayerCases.h run for both feeds: AdminEventStreamTest.AResumeAfterNGetsExactlyTheEventsAfterN and AdminEventStreamTest.AResumeAfterAnEvictedSequenceGetsADroppedMarkerNamingTheMissedRange beside LogStreamServiceTest's cases of the same names, and AdminEventSocketTest.AResumeAfterNReceivesExactlyTheEventsAfterN over a real socket)

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

- [x] Changing Rate.Drop.Item from a phone applies to the running gameserver, and history shows who and why (tests/e2e/config.spec.ts, run on 2026-09-27 with AMBROSE_E2E_PORT_BASE=13600 and AMBROSE_E2E_GAMESERVER_CONF against a real gameserver under the supervisor's panel listener, on MySQL 8, in a Pixel 7 viewport: with a reason given, 250 still could not be reviewed; 2.5 was applied and the gameserver's own admin API then reported Rate.Drop.Item 2.5 set live; the newest row of the history sheet named the operator, the change from the config value and the reason, and the gameserver's history named the operator and the source panel; the page never scrolled sideways)
- [x] Editing a config value applies without a restart and survives one (the same run: the gameserver was the same process before the change and after its admin API reported it set live, and after the panel's restart brought it back as a new process it still held 2.5 set live; the run resets the setting before and after and checks each reset, so it can be repeated against the same database)
- [x] Invalid values can't be submitted, and server refusals show their message (Config.browser.test.ts "keeps a value out of bounds or of the wrong type from being sent and shows the server's refusal": the review step stays shut while the value is out of bounds, of the wrong type or unchanged, and a 422 from the app is shown with its message and field; settings.test.ts holds each type's bounds the same way the app does)
- [x] Revert restores the old value and writes a new audit row (Config.browser.test.ts "reverts to the value a change in the history replaced, with a reason naming it": the history sheet's revert sends the replaced value through the same review step as a change of its own, with a reason naming the change it reverts; AdminSettingsViewTest.ARelayedChangeIsAttributedToTheUserTheSupervisorNames holds that a second change of the same key, which is all a revert sends, writes a history row of its own over the in-memory store)
- [x] Importing a preset with one out-of-bounds value shows the error in the diff and applies nothing (Config.browser.test.ts "shows an out-of-bounds preset value in the diff and applies nothing" and "shows the app's own refusal from the dry run in the diff": every entry is checked against the app's schema, the rest by the app in a dry run, and while any is refused there is no reason to give and no way to apply; "applies a preset only once the app's dry run of it passed, sending only what changes" holds that a dry run that could not be made keeps it shut until it is checked again, and that the one batch then carries only the entry that changes; AdminSettingsViewTest.ADryRunSaysWhatABatchWouldChangeOrRefuseAndChangesNothing holds that the dry run writes nothing)
- [x] A secret setting shows masked, its reveal control returns the value only for a caller allowed to see it, and every reveal writes an audit row (Config.browser.test.ts "offers a secret's reveal only to a caller allowed to see secrets, for that one key"; AdminSettingsViewTest.ARevealNamingOneKeyShowsAndRecordsOnlyThatOne and VerifierKeysAreMaskedUnlessRevealedAndHistoryIsMasked hold that the app shows it only with the right and records each reveal in its activity record, and PanelTest.RelayedSettingsChangesReloadsAndRevealsAreRecordedWithoutTheirValues that the panel writes an audit row naming only the keys shown)

## 17.14 Panel listener, TLS, sessions and the audit store

**Goal:** The panel has one hardened entry point of its own: a listener that refuses to carry sign-ins insecurely, sessions that cannot be replayed or forged, a rate limit every costly route declares itself into, and the audit tables every later milestone writes its record into.

**Size:** L. **Depends on:** 17.08, 1.12, 1.19

**Deliverables**

- `Panel` in `src/server/apps/supervisor/` serving the panel API under `/api/panel/` and the dashboard built in 17.06 at `/`, off unless `Panel.Enable = 1`, bound to 127.0.0.1 by default, on the same Crow and Asio layer as 17.02
- Proposed, extending the settled Remote access rule under Decisions, Operations in doc/ARCHITECTURE.md to the panel's own listener, which carries session cookies, passwords and two-factor codes: startup refuses a non-loopback bind unless a certificate and key are configured, or the operator sets `Panel.AllowPlainHttpRemote = 1`, off by default, which logs a warning naming the option and states that sign-in secrets then cross the network unencrypted; a reload that would leave the bind unsafe is refused with an error naming the option while the old listener keeps serving
- Certificate and key in PEM from `Panel.CertificateFile` and `Panel.PrivateKeyFile`, named as the admin API's are so one listener reads both, checked at load for a matching key, validity dates and chain order, swapped live on reload with the old pair kept and every error reported on failure, the fingerprint printed to the console and log, and a warning as expiry approaches; `supervisor --panel-self-signed` writes a certificate for a machine-local panel and prints its fingerprint
- Signed, expiring session cookies (HttpOnly, SameSite=Strict, host-prefixed, Secure whenever TLS is on) stored as hashes with idle and absolute lifetimes, and a double-submit CSRF token required on every state-changing request. The session generation per user waits for 17.46, which is where users first exist to bump it for; `panel_session` already holds the column
- A cost-weighted rate limit on session-authenticated routes, per user and per address, where each route declares its cost as it registers and a route with no declared cost is uncosted, so the routes that cost real work are limited as they land: backup creation (17.16), archive and search jobs (17.39), activity exports (17.25), settings batches (17.12), database host tests (17.30) and remote pulls (17.41). A throttled request answers 429 with a retry hint, and one audit row records each throttled user and minute
- The panel store's audit tables, `audit_event` and `audit_subject`, with an event id for idempotent forwarding, batch id, time, event name in `namespace:path.action` form, actor type and id, address, user agent, node, result, error, reason, properties and any number of subjects per event, written in the same transaction as the change they record, so a change cannot outlive its record; a security-relevant write whose audit row cannot be written fails closed. 17.49 binds every action to these tables through its `AuditScope`, and 17.25 adds the event catalog and the activity pages, so no milestone writes a second store
- `Panel.TrustedProxies`: the client address is the rightmost hop not in that list, forwarded headers from any other peer are ignored, and every throttle, audit row and sign-in record uses that address
- HSTS under TLS, a strict Content-Security-Policy, frame-ancestors none, nosniff and same-origin referrer on every response, and a dashboard that requests nothing from another host

**Acceptance**

- [x] `Panel.BindIP = 0.0.0.0` with no certificate logs an error naming the option and exits 1, and the same change on a reload is refused while the old listener keeps serving. The supervisor exited 1 with the error naming `Panel.BindIP`, `Panel.CertificateFile` and `Panel.AllowPlainHttpRemote`, and `PanelTest.ARefusedReloadLeavesTheOldListenerServing` keeps the old port answering
- [x] With `Panel.AllowPlainHttpRemote = 1` and no certificate, a non-loopback bind starts and logs a warning naming the option and the risk. `PanelTest.StartsBeyondThisMachineWithThePlainHttpOptIn`, run with `AMBROSE_TEST_ADMIN_REMOTE_BIND=0.0.0.0` so a bind off this machine is never opened unasked
- [x] Replacing the certificate and reloading serves the new one with no restart, and a key that does not match the certificate leaves the old pair serving with the error reported. `PanelTest.AReloadSwapsTheCertificateWithNoRestart` and `AdminServerTest.AReloadSwapsTheCertificateAndKeepsTheOldOneWhenTheNewPairIsWrong`, both reading the fingerprint the client was actually served
- [x] A state-changing request without its CSRF token is refused, and a session cookie replayed from another origin does not authenticate it. Built in 17.06 and kept here: the panel listener is the same listener, so `AdminServerTest.SignsABrowserInAndServesItByCookie` refuses a cookie's unsafe request with no CSRF token, with no origin, and with another origin, and `AcceptsASocketUpgradeByCookieOnlyFromItsOwnOrigin` covers an upgrade
- [x] Two hundred requests in a minute from one session to a route registered with a cost answer 429 with a retry hint while an uncosted status read from the same session still answers, and one audit row records the throttling. `PanelTest.HoldsBackACostlyRouteAndRecordsItOnceAMinute` sends two hundred: 120 answered, 80 refused with `Retry-After`, the uncosted route still answered, one row in `audit_event`
- [x] A state-changing request whose audit row cannot be written is refused with the audit failure named rather than applied unrecorded, and the record and the change it describes commit together or not at all. `PanelTest.AChangeWhoseRecordCannotBeWrittenIsNotApplied`: the record is written first inside the transaction, so neither half can outlive the other
- [x] With `Panel.TrustedProxies` empty, a forwarded header naming another address changes neither the throttled address nor the address in the audit row. `PanelTest.AForwardedHeaderChangesNothingWithNoTrustedProxies` reads the address back out of `audit_event`, and `TrustedProxiesTest` covers the list, the ranges and an untrusted peer
- [x] Every response carries the CSP, frame-ancestors, nosniff and referrer headers, and HSTS only under TLS. The end-to-end run checks the page and a 401 from the panel listener, and `AdminServerTest.ServesOverTlsWithHstsAndTheCertificateItWasGiven` and `ServesPlainHttpWithoutHsts` cover both sides of HSTS
- [x] The 17.06 dashboard loads from the panel listener with no browser console errors and no request to another host. `tests/e2e/panel-listener.spec.ts` signs in and reaches the overview against a supervisor serving its own panel listener

## 17.15 Schedules with in-game countdowns

**Goal:** Operators schedule restarts, backups, commands and announcements, and players get warned in game before a restart.

**Size:** L. **Depends on:** 17.05, 17.27, 17.48, 2.15, 6.01, 6.04

**Deliverables**

- `ScheduleMgr` in the supervisor with no cron daemon or queue: a heap of due times on one timer, re-armed when a schedule changes or the wall clock jumps, with each run's state committed to the store before and after every task; cron parsing and next-run math in `src/common/Time/`
- Schedules with a cron expression (five fields, names, steps and the `@daily`-style macros) or a one-time date in a chosen IANA time zone, an overlap policy and a misfire grace, each running an ordered list of tasks timed after the previous task or from the scheduled time, with offsets: announce, run a command, restart, stop, start, back up, reload, set a setting, and update once 17.17 lands
- Parse errors returned with the field and position, refusal of expressions that never fire, and a preview of the next five runs with daylight saving notes, computed by a call to the server against the same code that will fire the schedule, never by a second implementation in the browser
- Time zone data reloaded with the supervisor's reload, which recomputes every next run; a schedule whose zone the new data no longer holds is held with an error instead of firing at the wrong time. The data is the platform's own database, as Time zones in doc/ARCHITECTURE.md settles, and the page names its version and whether it came from the system or the standard library's built-in copy
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
- A snapshot record written inside each dump transaction: per table the row count and a row checksum, plus the last applied update per database, the client revision and the Ambrose version, so 17.51 can prove a restore matches the moment the dump began, and a per-character read path over the archive, so one character's rows can be read out without restoring it, which 17.95 needs and which is nearly free while the format is being written
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

Changed on 2026-09-27: the data root is client-derived by default. Only what the supervisor itself keeps there is left out, so a folder any tool writes later is refused for download from the day it appears, rather than waiting for somebody to add it to a list. The launcher's run folders are covered this way, because they hold copies of the install's own files.

**Deliverables**

- Named roots with policies:
  - Install (read-only), config, logs, data (type dumps read-only, lock files hidden), custom SQL, backups (read-only here), and the user's client installs (read-only and never downloadable).
  - Within data, everything is client-derived except what the supervisor itself keeps there, `supervisor/` and `launcher-window.json`, with `admin/`, `panel/`, the keyring and lock files hidden. That covers, without naming them one by one:
    - the type dumps and their fast copies;
    - the client tool's caches of messages, handlers, behaviors and functions;
    - decompiled output and its Ghidra project;
    - the launcher's `client/<revision>` run folders, which hold copies of the install's own `revision.dat` and `data.dat`;
    - the client driver's runs, reference crops and zone caches;
    - 17.170's scan results;
    - any folder a later milestone adds.
  - The supervisor's store, keyring, token files and TLS keys sit outside every root and are refused.
- A jail that resolves each request to a root handle and checks each component:
  - One decoding pass.
  - No absolute paths, drive letters, UNC prefixes, backslashes, empty, `.` or `..` components, control characters, names Windows cannot hold, or reserved device names, on any system.
  - On Linux, `openat2` beneath the root, or a component-by-component walk without following links.
  - On Windows, handle-relative opens that refuse reparse points and compare the final path with the root.
  - FIFOs, devices and sockets are refused before any read.
- Protected path patterns per root, matched on the resolved path and applied to every operation, and secret `.conf` values shown redacted without `settings.secrets.read`
- A space guard with a minimum free space per volume, and a reservation that every writer takes before streaming
- Listing and reading with paging, sorting, size, type and modification time, and each root's policy in the response, so the page disables what the root refuses instead of failing on submit
- Roots marked client-derived refuse download, archive and any share path for every caller whatever their permissions, as the Client-derived data review note requires
- Every refused traversal audited with the resolved path, while the response never shows it
- A files page per root and per app:
  - Breadcrumbs, selection, a context menu and mass actions, with a bottom sheet at phone width.
  - Rendered from the root's policy in the listing response, so a control the root refuses is disabled rather than failing on submit.
  - Uploads, downloads and mass copy are 17.54, and archives and search 17.39.

**Acceptance**

- [x] Requests for `../`, an absolute path, an encoded traversal, a device name such as `CON`, or a symbolic link or junction leaving the roots are refused with 403 and audited with the resolved path, which the response does not carry (route tests on Windows and Linux) (FilesServiceTest.TraversalAbsoluteEncodedDeviceAndEscapingLinkRequestsAre403AndAuditedWithTheResolvedPath covers `../x`, `/etc/passwd`, `C:/Windows`, `%2e%2e%2fx`, `CON`, `nul.txt` and a link as the path or as a folder on it, each 403 with no host path in the body and one audit row naming the resolved path; it and FileJailTest.RefusesALinkedFolderLeavingTheRoot fail with the jail's link and outside checks taken out; run on Windows and on Linux under WSL, and again through tests/e2e/files.spec.ts against a real supervisor)
- [ ] A download of a file under the client install root, or anywhere in the data root outside what the supervisor itself keeps, is refused for an owner as well as a viewer, on every path including a share link (route tests over each path), and a folder a test creates in the data root under a new name is refused the same way
- [x] Reading a `.conf` file without `settings.secrets.read` shows every secret value redacted (route test) (FilesServiceTest.AConfFileReadWithoutTheSecretsRightShowsEverySecretRedacted: a viewer asking to reveal and an owner not asking both see Admin.Token, Panel.Token, the database password and the verifier keys masked with the keys named, copies such as `supervisor.conf.bak` included, and only an owner who asks sees them, with the reveal audited)
- [x] A write that would leave less than the minimum free space is refused with the volume and the figure named, before any byte reaches the disk (unit test with a fake volume) (SpaceGuardTest.AWriteThatWouldLeaveLessThanTheMinimumIsRefusedWithTheVolumeAndFigureBeforeAnyByte names the volume, the free space, the minimum and the size asked for, and no file exists afterwards)
- [x] A FIFO, a device node and a unix socket placed inside a root are refused before they are opened (unit test on Linux) (FileJailTest.RefusesAFifoADeviceNodeAndASocketBeforeOpeningThem, run on Linux under WSL, kernel 6.6, where each is refused from its O_PATH descriptor's fstat before any open; on Windows the same test refuses a device path and an AF_UNIX socket)
- [x] The jail's component and link checks pass in the unit tests on Windows and on Linux (JailPathTest, FileJailTest, PathRulesTest and FolderPageTest pass on Windows and on Linux under WSL: 50 on Windows with the file-symbolic-link case skipped only because an unprivileged Windows account cannot make one, and 91 with the panel suites on Linux, where only the 8.3 short-name case skips because Linux keeps none)
- [x] A protected path is refused for listing, reading and every write operation, not only for writes (route tests) (FilesServiceTest and PanelFileRulesTest: an owner's pattern and the built-in ones refuse list, read and each write operation alike, and PathRulesTest holds the gitignore matching, a pattern of hundreds of `**/` segments included)
- [x] The files page walks into a folder by its breadcrumbs, selects a filtered set and shows its count, and on a read-only root every write control is disabled with the root's policy named (browser test) (apps/dashboard/src/pages/Files.browser.test.ts, and tests/e2e/files.spec.ts against a real supervisor)

## 17.19 Built-in resource graphs

**Goal:** Operators see CPU, memory, network, disk, players and tick time over time in the panel itself.

**Size:** M. **Depends on:** 17.03, 17.06, 17.09

**Deliverables**

- The supervisor samples every app's CPU, memory, threads, open handles, network in and out, and disk use every few seconds through the operating system, keeping a day at full detail in memory and 30 days downsampled on disk, with no Prometheus or Grafana needed; network traffic comes from each app's own counters
- Graphs per app and per realm for those values plus sessions, players online, sessions at character select and tick time, with ranges from 5 minutes to 30 days and a live view that does not reset when an app stops
- Downsampling on write, so any range reads a bounded number of points, and a stretch while an app was stopped drawn as a gap rather than as zero
- History that survives a supervisor restart, with the day's full-detail samples folded into the long series exactly once
- A benchmark that reports the sampler's cost in microseconds per app per sample, recorded with the milestone instead of asserted as a share of a core
- The sample shape left open for the rows 17.91 and 17.92 add, the tick breakdown per subsystem and the per-session network quality, so those milestones add series to this sampler rather than building a second one
- Alert rules, delivery and acknowledgement are 17.67, so this milestone needs no mail settings

**Acceptance**

- [x] With gameserver under a synthetic load, the panel's CPU graph is within 10 percent of the operating system's own figure (ResourceSamplerTest.TheSeriesTheGraphDrawsMatchesAProcessUnderLoad: a process is told to hold half of one core and holds it by measuring itself, the sampler reads it through the same ProcessInfo the supervisor uses and writes the same series the panel draws, and that series reads 52.4 percent where the process measured 50 on its own clock, which is two independent measurements of one thing rather than a number compared against itself; ProcessInfoTest.TheShareReportedMatchesWhatTheProcessActuallyHeld holds the reader to the same band on its own, five runs landing between 46 and 53 against 49 to 50 held. The load is a synthetic process rather than a gameserver under players, which is what a real client session would add)
- [x] A 30 day graph loads in under one second with a month of samples (TimeSeriesTest.AMonthOfSamplesReadsBackWellInsideASecond: 43201 minute samples written and the whole month read back as 709 points in 2.6 ms against the second an operator would notice)
- [x] Stopping an app leaves a gap in its graph, and the live view keeps updating for the other apps (ResourceSamplerTest.AStoppedAppLeavesAGapWhileTheOthersKeepBeingWritten: two apps sampled together, one stops, its graph ends in a point that is absent rather than zero while the other goes on being written and holds more readings; TimeSeriesTest.ABucketNothingWasWrittenIntoIsAbsentRatherThanZero holds the store to the same rule, and Graphs.browser.test.ts shows the panel saying a series holds three readings of four rather than drawing it as complete)
- [x] Restarting the supervisor keeps the history, and a day's samples appear in the long series once, not twice (SeriesStoreTest.AHistoryComesBackAfterARestart and ReadingTheSameFileTwiceLeavesTheHistoryExactlyOnce: the second reads the same file twice and compares every point's sample count, which would double if a day were folded in again, and it asserts the history is not empty first so it cannot pass by comparing two nothings)
- [x] The benchmark reports microseconds per app per sample, and sampling 20 apps stays under one millisecond of work per round (ResourceSamplerTest.ARoundIsCheapEnoughToSitOnATimerBesideTwentyApps prints the figure: 21.0 microseconds per app per sample and 420 microseconds for a round of twenty on Windows, 12.8 and 256 on Linux, both Debug builds)

## 17.20 Client data and revisions page

**Goal:** Operators see which client install and type data each server uses, what has been built and extracted from it, and rebuild it from the panel.

**Size:** M. **Depends on:** 17.06, 3.23

Widened on 2026-09-27 at the maintainer's direction, who asked that everything the client work can read be seen from the panel. The page now also lists every install found, the caches built per revision and the world table sets extracted from the install. Browsing what those hold is 17.165 onward; this page remains the one that says what exists and whether it is current.

**Deliverables**

- A page listing:
  - The client installs `ClientLocator` finds, each with its revision, whether it holds the client program, and how many archives it has.
  - The revision each server uses.
  - The type dump in use, with its revision, executable hash, extractor version and build time, and whether its fast copy is current.
  - The message definitions, name tables and creation config loaded.
- The caches built per revision in the Ambrose data folder, by kind: type dumps and fast copies, message definitions, handlers, behaviors, functions, decompiled output, the launcher's run folders, and 17.170's scan results once it lands. Each is listed with its revision, size and build time, as facts and never as files.
- The world table sets extracted from the install (names, levels and stats, zones), each with its row count and the revision it was extracted from. Each set records that revision, as 3.23 makes the name tables do, and a set from another revision than the install's is marked.
- The revision following state from 3.23: the newest revision seen, whether data for it is built, and any build in progress with its live output
- Buttons to rebuild client data and to switch a server to a different install or revision:
  - Each is checked against `clientdata.rebuild` or `clientdata.switch`, and audited.
  - Each runs as the protected setup state, so power actions wait, and is applied live where 3.23 supports it.
- The page reads the user's own install at run time and never serves or copies client files

**Acceptance**

- [ ] After a client revision change, the page shows the new revision within a minute and the rebuild's live output while it runs (integration test with a fixture install whose revision.dat changes)
- [ ] A failed rebuild shows the extractor's error, and the servers keep using the previous data (integration test with a fake extractor that fails)
- [ ] A restart requested during a rebuild is refused with the rebuild named (route test)
- [ ] No response from this page carries a client file's bytes, and the install root and every client-derived folder stay outside every downloadable root (route test over a fixture install whose files hold marker runs)
- [ ] Every install `ClientLocator` finds is listed with the revision and archive count the locator reports, and the one each server uses is marked (route test over a fixture tree)
- [ ] An extracted set from another revision is marked, and once 3.23 has extracted it again the page shows the install's revision and the new row count (database integration test)

## 17.21 Accounts and bans pages

**Goal:** Game masters manage accounts and bans from the panel.

**Size:** M. **Depends on:** 17.05, 17.48, 17.49, 2.13

Split on 2026-09-27 at the maintainer's direction, who asked to manage everything from the panel. Accounts and bans need only AccountMgr from 2.13 and the command levels 17.49 passes to the apps, so they keep this id. Characters and online players moved to 17.177, which waits for 3.17 and 6.05.

**Deliverables**

- An accounts page backed by AccountMgr from 2.13:
  - Search by username, email, address or MachineID.
  - Create, password reset, lock and unlock, and security level.
  - Each account's email and last sign-in address.
  - A password reset seals the verifier with the active key, deletes `account_session` and kicks live sessions. A generated password is shown once and never logged.
- Email, address and MachineID hidden from users without `accounts.pii.read`
- A bans page for account, address and machine bans, each with its duration, reason, who and when, and unban with a reason:
  - AccountMgr gains address and machine bans. They write the `ip_banned` and `machine_banned` rows the login server's sign-in check already reads and nothing writes today, with login console commands for them.
  - 6.05's `ban ip` and `ban machine` call the same functions rather than writing the rows a second way.
  - 17.138's address rules are a separate list of its own.
- Each account's characters listed read-only, with level, school, location and whether each is deleted, under `characters.read`. Renaming, restoring, deleting and editing them are 17.177.
- Every action goes through CommandMgr at the panel user's command level. It refuses to act on an account at or above that level, requires a reason for bans, locks and security level changes, and is audited, refused attempts included.
- A search in the top bar over accounts and the apps the caller may see. Each result is checked against the caller's own permissions before it is returned, and each row opens its page. 17.177 adds characters to it.

**Acceptance**

- [ ] An operator without `accounts.ban` cannot ban, and the attempt is audited (route test)
- [ ] A panel user whose grants carry the game master command level, as the operator and game master roles do, cannot ban or reset an administrator account, and the attempt is audited (route test)
- [ ] A user without `accounts.pii.read` sees no email, address or MachineID in any account response (route test over every account route)
- [ ] An account ban, an address ban and a machine ban from the panel each write the row the sign-in check reads, with who, why and until, and the login server then refuses that account, address and machine; an unban records its reason (database integration test)
- [ ] A password reset from the panel stores the new verifier sealed with the active key, removes the account's `account_session` rows, and shows the generated password once and in no log line or audit row (database integration test)
- [ ] The top-bar search finds an account by username, and returns nothing for an app or account the caller holds nothing on (browser test)
- [ ] Dev-gated: an account created from the panel signs a real client in. Needs the maintainer's own retail client, as doc/PATCHING.md describes

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
- A node that loses the panel keeps its apps running and its schedules on time, buffers audit rows and run results, and resyncs when the link returns. A schedule whose targets are all on one node runs on that node, and one whose targets span nodes runs on the panel, as Panel operations in doc/ARCHITECTURE.md settles

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
- The distribution's tzdata package in the image's runtime stage, as Time zones in doc/ARCHITECTURE.md settles, so the 17.15 schedule engine reads current rules rather than the standard library's built-in copy
- A Pterodactyl egg that installs and starts Ambrose, exposes its settings as startup variables, uses the ready lifecycle line as its startup done string and `shutdown` as its stop command so Wings does not count a stop as a crash, and maps the console, for hosts that already run Pterodactyl
- Packaging docs in doc/OPERATIONS.md, with client data built on first start through 3.22 from a mounted client install

**Acceptance**

- [ ] Dev-gated: after `--install-service` and a reboot, the servers are running and the panel is reachable, on Windows and on Linux. A reboot cannot run in CI, so it is run by hand and recorded
- [x] `docker compose up` on a clean machine with a client install mounted reaches a ready loginserver with no other steps (run in review with a client install mounted read-only: the database turns healthy, the supervisor logs `supervisor ready`, then `loginserver is ready: it printed its ready line` 18 seconds later, once the image's typeextract has built the type dump, then `gameserver is ready`, and the container turns healthy when the login port accepts a connection)
- [ ] Dev-gated: importing the egg into a Pterodactyl panel creates a server that installs, starts, shows its console and stops cleanly. Needs the maintainer's own Pterodactyl install
- [ ] Dev-gated: stopping the egg's server from Pterodactyl leaves no crash message in its console. Needs the maintainer's own Pterodactyl install
- [x] The image resolves America/New_York, Europe/London and Australia/Lord_Howe from the system time zone database rather than the standard library's built-in copy, and an image built without tzdata fails this check rather than starting (the build prints `time-zone check: libstdc++ resolved all required zones from system tzdata 2026c`, and the same image built without tzdata stops at `time-zone check: /usr/share/zoneinfo/tzdata.zi is missing or has no version header`)

## 17.24 Desktop control app: hosting a game on this computer

**Goal:** A player hosting on their own computer starts the database, servers, panel and client from one icon.

**Size:** L. **Depends on:** 17.08, 17.181, 3.22, 3.25

Changed on 2026-09-27 at the maintainer's direction, who asked for the panel as its own installable program.

- **What this milestone keeps.** The program this app runs in is 17.181, over the shell of 17.179 and the local link of 17.180, and its tray and installer are 17.182 and 17.183. This milestone keeps what its goal names: hosting the stack on this computer, its database and Play.
- **The client.** It starts through 3.25's launcher, as Client launcher in doc/ARCHITECTURE.md already says, rather than through 1.21, whose scripts 3.25 replaced.
- **The private database.** It rests on the private database decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md: MariaDB bundled in the package's optional server component.
- **Size.** It now carries L on its count of deliverables rather than on judgment.

**Deliverables**

- The server programs are the ones beside the program, as a build tree and 17.183's server component lay them out, or the ones `Host.Supervisor` in `panel.conf` names. With neither, the program runs connect-only: it offers no hosting, says which package carries the server programs, and opens panels on other machines as before.
- A first host asks nothing:
  - It makes a server home in the Ambrose data folder, holding the configuration, `conf.d` and the logs.
  - It copies the running build's `.conf.dist` files into the home at each start, and makes each `.conf` from its `.conf.dist` when it is missing, never over an edited one, the way the `conf` step in apps/installer does.
  - It writes one file of the program's own in `conf.d`, which turns the panel on at 127.0.0.1 on a free port (12080 when free) and is remembered.
  - When the server programs are present, the first open hosts without asking unless `Host.Automatic = 0`.
- Database setup with no steps:
  - The program uses a MariaDB or MySQL server it finds on this machine, with credentials the user gives.
  - Otherwise it runs a private MariaDB from the package's server component, as the private database decision sets. The private database is checked against its published checksum before any of it runs, bound to localhost with generated credentials written to the program's own `conf.d` file (readable only by the user), started before the supervisor and stopped after it.
  - The database is registered as a database host once 17.30 lands.
- Start and stop:
  - The supervisor is started detached through `ChildProcess` under the operator's own account, so closing the window leaves it running, and the next open finds it again through its admin API on loopback.
  - Stop asks for the supervisor's own graceful shutdown over that API and waits for its stop timeout. A supervisor that does not stop is reported, and only then does the program offer to end it.
  - A second copy of the program never starts a second supervisor.
- The first start shown as it happens:
  - Until the panel answers, the program's own screen shows each app's start step with its numbers from the supervisor's status. That includes 3.22's install discovery, the type dump build, the name, level and zone extraction, and any problem record with its message and fix.
  - Once the panel answers, the program opens it through a local link.
- Play, on the program's This computer screen, starts the client through 3.25's `launcher` against the local login server once the supervisor reports the login server ready, and says what it waits for until then. On a machine that is not Windows, Play names the launcher's own refusal.
- Quit with stop chosen closes the client connection cleanly and stops the servers and the private database, and the next start keeps accounts and characters
- Tests and docs:
  - Unit tests over the configuration step, connect-only mode, start, find and stop against a fake supervisor, and Play against the launcher's `--dry-run`.
  - An integration test that hosts a real supervisor from `panel-core`, reads its apps' start steps, stops it gracefully and hosts it again on the same data.
  - doc/config/panel.md and doc/OPERATIONS.md describe hosting from the program.

**Acceptance**

- [ ] Dev-gated: on a clean Windows machine with Wizard101 installed, installing the program and clicking Play reaches the login screen against the local server with no other steps. Needs the maintainer's own machine and client
- [ ] With the server programs beside it and nothing configured, the first open starts a supervisor with its panel on loopback and opens that panel signed in, asking nothing (integration test with fixture apps)
- [ ] Writing the configuration twice never overwrites an edited `.conf`, and a taken panel port moves to the next free one and is remembered (unit tests)
- [ ] Closing the program leaves the supervisor running, opening it again finds that supervisor and starts no second one, and Stop ends it gracefully within its stop timeout (integration test)
- [ ] With no server programs beside it and none configured, the program offers no hosting, says why, and still opens panels on other machines (unit test)
- [ ] A start that meets an unreachable database shows that problem's message and fix on the program's own screen (integration test with the database pointed at a closed port)
- [ ] Play waits until the supervisor reports the login server ready, then starts the launcher with the local login server's address and port (unit test with a fake supervisor and the launcher's `--dry-run`)
- [ ] A private MariaDB whose checksum is wrong is refused before any of it runs, and the program says what failed (unit test)
- [ ] Dev-gated: with a wizard in the world, Quit with stop chosen saves and disconnects the client cleanly and stops the servers and the private database, and the next start keeps the account and the wizard. Needs the maintainer's own machine and client

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
- The envelope `{v, type, id, scope, seq, time, data}` with structured data, and the client and server message types listed under Event socket in doc/PANEL.md; the protocol and its close codes are settled under Panel operations in doc/ARCHITECTURE.md
- Sequence numbers, backlog and resume taken from 17.04's stream layer rather than written again, with status and power events never dropped and stats keeping only the newest sample
- TypeScript types generated from the schemas the C++ side serializes, a test that fields are only added, and a contract test that fails when the server can send a type the dashboard does not handle
- Full error text only for `debug.errors`, and otherwise a generic message with a correlation id that also appears in the supervisor log

**Acceptance**

- [x] A socket opened with a foreign Origin is refused, and a ticket works once and only within 30 seconds (PanelEventSocketTest.AForeignOriginIsRefusedAtTheUpgradeWhileTheSameOriginSignsIn answers a foreign origin and `*` with 403 cross_origin at the upgrade while the panel's own origin reaches ready, and fails when the origin check is taken out; PanelEventTicketsTest.ATicketIsGoodForThirtySecondsAndOneUseFromItsOwnAddress holds a ticket good at 29.999 s and gone at 30 s, once, from the address it was minted for; PanelEventSocketTest.ATicketSignsASocketInOnceAndASecondUseClosesWith4401 does it over a real listener)
- [x] A page reconnecting after 10 seconds offline receives exactly the missed records or a dropped marker, with no duplicates, through the 17.04 layer's own tests (PanelEventStreamTest runs the shared StreamLayerCases against the panel feed, and PanelEventSocketTest.AReconnectingPageGetsExactlyTheMissedStatusRecordsOrADroppedMarker resumes from 3 to receive exactly 4 to 7, and after a trimmed backlog one dropped frame for 8 to 25, count 18, then 26 to 30; events.test.ts covers the page resuming after its last sequence and skipping what it already has)
- [x] The contract test fails when a server event type is added without a dashboard handler (apps/dashboard/src/lib/events.test.ts "every type the server can send has a dashboard handler" names a missing handler, the handler map is typed over every sent type so svelte-check refuses a gap, and PanelEventCatalogTest.TheDashboardTypesAreWhatTheCatalogRenders holds apps/dashboard/src/lib/protocol.ts to the C++ catalog)
- [x] A ticket sent in the URL rather than the first frame is refused, and the URL appears in no log (PanelEventSocketTest.ATicketInTheUrlIsRefusedBurnedAndNeverLogged: `?ticket=` answers 400 credentials_in_url, the ticket is burned so its later hello closes with 4401, and no captured log line carries it; AdminServerTest.ARouteThatAdmitsItsOwnUpgradesNeedsNoTokenMayRefuseAndLogsNoQuery holds the listener to the same)
- [x] A caller without `debug.errors` receives a correlation id that matches a line in the supervisor log, and no error text (PanelEventSocketTest.AnInternalFailureShowsItsTextOnlyToDebugErrorsAndItsCorrelationIsLogged: a viewer gets the generic message with a correlation id that a server.panel line carries with the full text, and only an owner holding debug.errors sees the text; the signed-in panel opens exactly one socket in tests/e2e/panel-listener.spec.ts, run against a real supervisor)

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
- Protected hours: a setting naming the hours when a restart, an update or a migration is refused, checked here where every power request already passes and again in the schedule path, with an owner override that needs a reason and writes an audit row

**Acceptance**

- [ ] A restart requested while a backup restore holds the app is refused with 409 naming the restore
- [ ] A restart inside protected hours is refused naming the window, an owner's override with a reason goes through and is audited, and a scheduled restart inside the window is refused the same way
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
- Secret launch values sealed with AES-256-GCM under the supervisor's key, the cipher Decisions, Accounts and the console already settles for `login.account.verifier`; Botan provides the cipher and the keyed hashes and the key lives in the keyring, both as Panel operations in doc/ARCHITECTURE.md settles
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
- Rotation per server type as Panel operations in doc/ARCHITECTURE.md settles, and a realm may be given its own world database, opt-in

**Acceptance**

- [ ] Registering a host with a wrong password saves nothing and shows the server's error
- [ ] Env-gated (AMBROSE_TEST_DB): rotating the gameserver's credentials while fake clients are connected causes no failed query and no disconnect
- [ ] Env-gated (AMBROSE_TEST_DB): after rotation the old password no longer connects. Run once against MySQL 8 and once against MariaDB; CI runs only the leg its runner provides, so the other is the maintainer's own run
- [ ] The gameserver's runtime user cannot create or drop a table
- [ ] Revealing a stored password without a recent step-up check is refused, and a successful reveal writes an audit row naming the host

## 17.31 Realms and zones pages

**Goal:** Operators see every realm's population, health and zones, and change a realm's limits and flags from the panel.

**Size:** M. **Depends on:** 17.06, 17.26, 17.48, 17.175, 4.03, 4.09

Changed on 2026-09-27: the zone instances a realm has loaded are read from 17.175's live world routes, so this page shows them per realm without a second instance listing.

**Deliverables**

- A realms page listing each `realmlist` row: its name with the display name read from the user's install, address and local address, port, flags, population against the player limit, last heartbeat, and the gameserver app and node behind it
- Editing a realm's player limit, flags and the default realm through the realm settings, applied from the next realmlist refresh and audited with a reason
- A realm page showing:
  - Population over time, and the players at character select headed there.
  - The zones the realm has loaded, with players per zone, read from the 17.175 instance routes and opening its instance pages.
  - Public instances with their capacity, once 12.17 lands.
- Zone actions, each checked and audited: reload a zone's data through 4.15, and move or kick everyone in a zone once 6.06 and 6.05 exist
- Realm and zone changes streamed live as realm events on the 17.26 socket
- Authorization is at the scope of the realm's own gameserver app, as settled under Decisions, Operations. A realm scope of its own follows the scope tree settled under Panel operations in doc/ARCHITECTURE.md, once the realm scope is built.

**Acceptance**

- [ ] Starting a second gameserver adds its realm to the page with a fresh heartbeat within one heartbeat interval, and stopping it marks the realm offline (integration test)
- [ ] Lowering a realm's player limit from the panel applies at the next realmlist refresh without a restart, and the audit row shows who and why (integration test)
- [ ] A user who holds nothing on a realm's gameserver app gets 404 for that realm, and a user holding `realms.read` on it sees only that realm (route test)
- [ ] A zone reload with broken data keeps the previous zone data serving and shows the errors (route test)
- [ ] A realm's page draws its population over time and lists the zones the realm has loaded with players per zone, and a zone that unloads leaves the list within one refresh (integration test)
- [ ] Changing a realm's flags sends one realm event on the 17.26 socket to holders of `realms.read`, and nothing to a user who holds nothing on that realm (socket test)

## 17.32 Realm maintenance mode

**Goal:** Operators close a realm to players for maintenance while game masters can still enter to check it.

**Size:** S. **Depends on:** 17.15, 17.31, 4.05, 6.01, 6.05

**Deliverables**

- A maintenance state per realm with who, why and when, set from the realm page or a schedule task, with an optional countdown through 17.15, and badged on the overview and the realms page; a node in maintenance putting the realms placed on it into this state is 17.22, which owns node maintenance
- Entering maintenance warns in-world players through 6.01 and then removes them through the 6.05 kick path, and sets the realm list flag so the login server stops sending players there
- `Realm.MaintenanceBypassLevel`, a live setting, lets accounts at or above that security level enter a realm in maintenance; its default is game master (2), as Panel operations in doc/ARCHITECTURE.md settles
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

**Size:** M. **Depends on:** 17.13, 17.25, 17.173, 4.15

Changed on 2026-09-27. The tables browser, and the world schema its forms are built from, are 17.173's, so this milestone adds editing, the journal and export on top of them rather than a second schema. The export follows the settled rules: it writes only into `data/sql/custom/db_world`, as World edit exports under Panel operations settles, and it never exports rows extracted from the install, as World threads, zone data and extracted tables settles. Templates are read from the install at run time and are never in the world database, as Object templates settles, so the first check now edits a placement.

**Deliverables**

- A world edits page over the content tables the game server loads, built on the 17.173 tables browser. It starts with the tables that exist when it lands, and gains spawns, doors, vendors and quests as later phases add them.
- Table forms built from the world schema 17.173 publishes, with its types, bounds and references, so a foreign key is chosen from its table
- Edits sent to the game server, which applies them to the world database, records them in the 4.15 world edit journal with the panel user as author, and reloads the affected stores. A group of edits applies as one change set and reloads together.
- A failed reload rolls back the database change and keeps the previous store serving, with every error shown
- A journal view and its export:
  - The view shows who, when, source (the panel or a GM command) and statement.
  - Chosen entries export as a local-only SQL file into `data/sql/custom/db_world` and never into a pending folder.
  - An entry that changed a table the schema marks as extracted from the install is refused for export, with the table named.
- Every edit, rollback and export audited

**Acceptance**

- [ ] Editing a `zone_object` row's position from the panel moves that object in game after the reload, and the journal names the panel user (integration test with a test client in the zone)
- [ ] An edit whose reload fails leaves the database and the running store as they were and shows the errors (database integration test)
- [ ] Exported journal entries of an authored table such as `playercreateinfo` apply cleanly to a fresh world database (database integration test)
- [ ] An export writes only under `data/sql/custom/db_world`, and a request naming any other folder is refused (route test)
- [ ] A user without `world.edit` can browse rows but gets 403 on a save (route test)
- [ ] Exporting an entry that changed an extracted table, such as `zone_object`, is refused with the table named, and nothing is written (route test)

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
- [x] The saved SMTP password never appears in any response, log or audit row (PanelSettingsTest.KeepsSavedSecretsOutOfAnswersLogsAndAuditRows)
- [x] With `AMBROSE_PANEL_TRUSTED_PROXIES` set, the key shows as locked and a live edit is refused naming the layer (PanelSettingsTest.KeepsTrustedProxiesLockedToTheEnvironmentLayer)
- [ ] With the captcha on and its provider unreachable, sign-in after repeated failures is refused with a clear error rather than allowed through
- [x] A user without `panel.settings` gets 403 on every group and sees no page in navigation (PanelSettingsTest.RequiresPanelSettingsForEveryGroupAndBothMethods, which an operator holding settings.read but not panel.settings fails on every group for both GET and PATCH, and routes.test.ts, which keeps the page out of navigation without panel.settings)

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
- Security: password change with the current password, two-factor setup with the secret grouped for typing and a QR code rendered in the browser by a renderer bundled with the dashboard, never fetched from another host; the renderer is qrcode-generator, as Panel operations in doc/ARCHITECTURE.md settles. A pending secret is kept apart from the active one, recovery codes are shown once with copy and download, with a count of those left, regeneration needs the password and a code, and disabling needs the password and a code
- Sessions: each session's device, address, first and last seen, with sign out per session and everywhere
- Self-service password reset by email when SMTP is configured, answering the same whether or not the account exists, rate limited per address and per user, with the token in the request body, and never signing in past two-factor sign-in
- Linking a game account with proof of ownership: that account's password, or a one-time code typed in game once 6.04 exists; the linked account's security level then caps console commands as Panel operations in doc/ARCHITECTURE.md settles, and unlinking is audited
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
- Download of folders and selections as an archive streamed on the fly, zip by default with tar.zst beside it, as Panel operations in doc/ARCHITECTURE.md settles
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

- An SFTP service in the supervisor, off by default behind `Panel.Sftp.Enable`, documented with its risk under Decisions, Experimental features; it is built, opt-in, on libssh, as Panel operations in doc/ARCHITECTURE.md settles
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
- The S3 client is Signature Version 4 over Botan and the outbound HTTP client, as Panel operations in doc/ARCHITECTURE.md settles

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
- WebAuthn is verified by the panel itself over Botan, with its own CBOR reader, as Panel operations in doc/ARCHITECTURE.md settles

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

- Panel users in the supervisor's SQLite store, separate from game accounts, with usernames following the game account rules from 2.13, one password policy on every path that sets a password, passwords hashed by Argon2id from Botan as settled under Decisions, Operations, a disabled flag and a must-change flag
- Sign-in that checks the password before any second factor, spends one Argon2id verify on unknown and disabled users so timing matches, regenerates the session id on success, and never records the typed username or password on failure
- Failure throttles per user and per address over the 17.14 client address, counting only failures, with each account's address count kept separately so a success against one account does not clear another's
- On first start the supervisor creates an owner account and writes a one-time sign-in link to its console and log, valid for minutes and only from the same machine, so no default password ever exists
- Supervisor console commands `panel user create`, `panel user list`, `panel user reset-password` and `panel user disable`, with arguments kept out of logs and audit rows
- One-time password reset links an operator can issue, single use, expiring, and ending the user's other sessions when used; a password or disable change bumps the 17.14 session generation

**Acceptance**

- [x] A fresh supervisor has no default password and prints a one-time owner link that works once, only from localhost. `PanelSignInTest.AFreshPanelHasNoPasswordAndItsLinkMakesTheOwnerOnce`: the account does not exist until the link is claimed, a second claim answers 410, and a claim from anywhere but loopback answers 403. Checked by hand against a running supervisor, which printed the link and made the maintainer's own owner account from a browser
- [x] Twenty failed sign-ins for one user within a minute are throttled, and the correct password works again once the window passes. `PanelSignInTest.HoldsBackAGuesserUntilTheWindowPasses` sends twenty over HTTP and gets 429 with `Retry-After` on the twenty-first even with the right password, and `AFoldedNameIsTheSameGuessingAndTheWindowEndsOnItsOwn` moves a clock through the window
- [x] A successful sign-in to one account from an address does not reset that address's failure count against another account. `PanelSignInTest.ASuccessForOneAccountDoesNotForgiveGuessesAtAnother`: the count is kept per account and per account-and-address together, never per address alone
- [x] Sign-in against an unknown user, a disabled user and a wrong password answer the same message and take the same time within measurement noise. `PanelUsersTest.AnswersTheSameAndTakesTheSameTimeForEveryRefusal` times five rounds of each and holds the slowest median inside three times the quickest; an unknown name spends a verify against a decoy hash made at start, and a disabled account is checked after its password is
- [x] A password change ends that user's other sessions within one second. `PanelSignInTest.APasswordChangeEndsTheSessionsThatUserHad`: a session carries the generation its user had when it opened, and the one statement that holds a session compares it, so the next request after the change is refused
- [x] `panel user create` leaves the password in no console line, log file or audit row. It takes no password at all: it makes the account with an unguessable one nobody is told and prints a one-time link the operator sets their own from, and `panel user reset-password` does the same
- [x] A password that fails the policy is refused identically from the console, the reset link and the users page. There is one `PanelPasswordPolicy::Check`, called by `Create` and `SetPassword`, which every path goes through; `PanelUsersTest.HoldsAPasswordToOnePolicyOnEveryPathThatSetsOne` covers both, and the claim and reset routes answer 422 with the same sentence

## 17.47 Two-factor sign-in, recovery codes and required enrollment

**Goal:** Two-factor sign-in cannot be replayed or skipped, recovery codes work once, and an owner can require it.

**Size:** M. **Depends on:** 17.46

**Deliverables**

- TOTP with a window of one step either side by default, refusing any time step at or below the last one accepted, on sign-in, on enabling and on step-up checks
- Ten recovery codes per user, stored as keyed hashes under a supervisor key so one can be found by lookup instead of a loop, each usable once, shown once with a count of those left; the keyed hash is Botan's HMAC-SHA-256 and the key lives in the keyring, as Panel operations in doc/ARCHITECTURE.md settles
- Enabling needs the password and a current code; disabling needs the password and a current code and ends the user's other sessions
- `Panel.TwoFactorRequired` (none, danger permission holders, owners and admins, or everyone) enforced on every route and socket from this milestone, with only the sign-in and enrollment routes exempt, and API key requests answering 403 with `two_factor_required`
- Step-up checks with a freshness window as a live setting, required by secret reveals, backup downloads, credential rotation and other danger actions, and recorded in the audit log with what they authorized

**Acceptance**

- [x] With two-factor on, a correct password without a valid code is refused, a code already accepted once is refused the second time, and each recovery code works once (PanelTwoFactorTest.APasswordWithoutAValidCodeOpensNoSession, ACodeAcceptedOnceIsRefusedTheSecondTime on sign-in, on enabling and on a step-up check, and EachRecoveryCodeWorksOnce; PanelStepUpTest.AStepUpCodeCannotBeOneAlreadyAccepted and TotpTest.AcceptsOneStepEitherSideAndNothingAtOrBelowTheLastAccepted fail with the step check taken out of the code match and the store's update; tests/e2e/two-factor.spec.ts signs in with a recovery code once and is refused the second time against a real supervisor)
- [ ] Setting two-factor required for everyone sends a user without it to enrollment on their next request, and their API key requests answer 403 with `two_factor_required`
- [x] A danger action attempted with a second factor older than the freshness window asks again and changes nothing (PanelStepUpTest.ADangerActionPastTheFreshnessWindowAsksAgainAndChangesNothing answers 403 step_up_required with Panel.Name unchanged and no settings.changed row; AFreshCheckAuthorizesTheActionAndTheAuditRowSaysWhat, ASecretRevealAsksForAFreshCheckEveryTime and PanelRoutesTest.TheRelayAsksForAFreshCheckBeforeARevealAKillOrARestrictedChange cover the relay)
- [x] Disabling two-factor without a current code is refused, and disabling it with one ends that user's other sessions (PanelTwoFactorTest.DisablingWithoutACurrentCodeIsRefusedAndWithOneEndsTheOtherSessions: the password alone answers 422 and a wrong or replayed code 403, while a correct one ends the other session and deletes the secret and codes; DisablingIsRefusedWhileTheRequirementCoversTheUser)
- [x] A recovery code appears in no response, no log and no audit row after the dialog that issued it, and the store holds only its keyed hash (PanelTwoFactorTest.ARecoveryCodeIsShownOnceAndTheStoreHoldsOnlyItsKeyedHash scans every later answer, audit_event and audit_subject, the captured log, the store file, its write-ahead log and the keyring, and finds only the keyring's HMAC-SHA-256 of each code; tests/e2e/two-factor.spec.ts checks the page and the browser's storage after the dialog closes)

## 17.48 Permission catalog, roles and grants

**Goal:** One authorization decision covers every route and socket, and a route cannot ship without declaring what it needs.

**Size:** M. **Depends on:** 17.05, 17.46

**Deliverables**

- The permission catalog from doc/PANEL.md as one C++ table of groups, keys, descriptions, danger flags and the scopes each key may be granted at, served at `GET /api/panel/permissions`
- Roles owner, admin, operator, game master and viewer as permission bundles, plus per-app sub-user grants of single permissions such as `console.read`, `console.write`, `power.restart`, `files.write`, `backups.restore`, `schedules.edit`, `settings.edit` and `accounts.ban`, which is the arrangement settled under Decisions, Operations; the wider node, cluster and realm scopes are settled under Panel operations in doc/ARCHITECTURE.md, and no route or check here depends on them until their milestones build them
- One `AuthorizationMgr` that decides every check in order: authenticate, resolve the scope with 404 when the caller holds nothing there, confirm every object in the path belongs to that scope, check the permission with 403 and an audit row for a refused danger permission, then check state with 409
- Route registration that fails when a route declares neither a permission nor that it is open to any member, and a test generated from the route registry that checks every route answers 403 without its permission and 404 for a scope the caller cannot see
- Response shaping from the catalog: secret settings masked without `settings.secrets.read`, account email, address and MachineID hidden without `accounts.pii.read`, and no verifier, session key hash, two-factor secret or key secret in any response whatever the caller holds
- The last owner can never be deleted, disabled or demoted, and nobody changes their own role or grants; a role or grant change bumps the 17.14 session generation of every affected user

**Acceptance**

- [x] A viewer can read an app's status but gets 403 on a command through 17.05 and on a restart through 17.08, and the dashboard hides both controls
- [x] A sub-user granted only `power.restart` on gameserver can restart gameserver and nothing else, and gets 404 for an app they hold nothing on
- [x] A route registered without a permission fails the route registry test, and so does a route naming a key the catalog does not hold
- [x] `GET /api/panel/permissions` lists every group, key, description, danger flag and scope the routes reference
- [x] Demoting or deleting the last owner is refused
- [x] A caller without `settings.secrets.read` receives the masked value, and the same route with the permission returns the value and writes an audit row
- [x] A grant change ends nothing for other users but bumps the affected user's session generation within one second

## 17.49 Panel audit scope, app relay and command history

**Goal:** Nothing the panel does happens without a record, and the browser never holds an app's token.

**Size:** M. **Depends on:** 17.05, 17.48

**Deliverables**

- An `AuditScope` that records the panel user, address, subjects, result, reason and error of every action into the 17.14 audit tables, in the same transaction as the change it records, refused attempts included, so a change cannot outlive its record and no milestone keeps a log of its own
- A chain hash column on the audit row from the day the store is written, carrying the hash of the row and of the row before it, so 17.93 has a chain to verify and stream rather than a migration to run over a store already full
- The supervisor relays each app's admin API behind the signed-in user's permissions, holding the per-app tokens itself and passing the command security level the user's grants allow; the browser holds no app token, and the 17.02 tokens stay for scripts. The level cap is settled under Panel operations in doc/ARCHITECTURE.md
- One stored command history, per panel user and app, in the supervisor's store, capped per user, with sensitive arguments redacted, which replaces the in-page recall 17.07 keeps for the open page and is the only history anything stores
- A relay timeout and an error that distinguishes an app that is stopped, one that refuses the token and one that timed out, each recorded
- Refusals recorded with their reason: above the caller's level, missing confirmation, wrong scope, protected state

**Acceptance**

- [x] A restart run through the panel appears once in the audit log with the panel user, the client address and the app as subject (PanelTest.APanelRestartIsRecordedOnceWithItsUserAddressAppAndChainHash)
- [x] A command refused for being above the caller's level writes a refused audit row and runs nothing (PanelTest.ARefusedCommandIsAuditedWithoutRunningIt; AdminCommandRouteTest.ACommandAboveTheRunnerLevelIsRefusedWithoutExecution)
- [x] A forced failure of the audit insert leaves the change unapplied, since both are one transaction (PanelTest.AChangeWhoseRecordCannotBeWrittenIsNotApplied)
- [x] No response body or browser storage holds an app token, and a request that tries to pass one is ignored by the relay (SupervisorTest.TheRelayRemovesClientCredentialsAndCapsCommandLevels; Console.browser.test.ts, where the history stays on the server and browser storage stays empty)
- [x] A command run in one browser appears in that user's history in another browser, with a password argument redacted (PanelTest.CommandHistoryIsStoredPerUserAndReturnsOnlyTheRedactedCommand; Console.browser.test.ts, which loads the shared history after the page is reopened)
- [x] A relay call to a stopped app and one to an app refusing its token give different errors, both recorded (SupervisorTest.StoppedAppAndInvalidTokenRelayFailuresAreDistinctAndAudited)

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

- A text editor with syntax highlighting for `.conf`, SQL, JSON, XML and Lua, CodeMirror 6, as Panel operations in doc/ARCHITECTURE.md settles
- `.conf` validation against the 17.12 settings schema before saving, a warning when a key is shadowed by a live setting or locked by a layer, and an offer to run `reload config` afterwards
- Saves that require the ETag from the last read, write a temporary file and rename it over the target, and answer 409 with the current ETag and a diff when the file changed underneath, the diff shown in the editor's own merge view; a settings, grant or schedule review shows a structured difference instead, never a text difference of formatted JSON, which invents noise from key order and hides which secret is which
- A version store per root keeping previous versions with who and when, with view, diff and restore; its location and retention defaults are settled under Panel operations in doc/ARCHITECTURE.md, and the retention is a live setting
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
- Grants over an extra root use the same `files.*` keys at app scope; node-scoped roots follow 17.22, and the wider scopes follow the scope tree settled under Panel operations in doc/ARCHITECTURE.md as they are built
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
- Every close code and its meaning documented with the socket types in doc/PANEL.md, which the protocol settled under Panel operations in doc/ARCHITECTURE.md covers

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
- A restart choice of now or with a countdown, whose confirmation shows players in world, sessions at character select and active patch downloads, and says so when the time asked for falls inside 17.27's protected hours, with the override and its reason in the same place
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
- A crash record per crash with time, exit code or signal, classification, uptime, the last 200 log lines and the dump path, which 17.83 is what writes, symbolizes and groups; until it lands the record says no dump was written rather than carrying a path to nothing
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
- Registration is offered, off by default, on the terms Panel operations in doc/ARCHITECTURE.md settles

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

**Size:** M. **Depends on:** 17.21, 17.25, 17.177, 12.07

Changed on 2026-09-27: mute and kick moved with the online players page to 17.177 when 17.21 was split, so actions taken from a report use 17.177's paths for those and 17.21's for bans.

**Deliverables**

- A report queue over 12.07's moderation records: player reports and house reports with reporter, subject, category, text, realm, zone, time and state (new, claimed, actioned, dismissed), claimed by one moderator at a time with the claim visible
- Chat search for a reported player over 12.07's chat records, bounded by time range and result count, with the lines around a match, filtering by channel, and the bound reported when it stops early
- Mute history per account and character: who, why, how long and when it ends, plus the kicks and bans the same moderator issued and a repeat count per subject
- Actions taken from a report go through the checked paths of 17.177 for mute and kick and of 17.21 for bans, each linked to the report it came from and each requiring a reason
- A live chat tab beside the search:
  - It runs on the same socket, under the same channel filter and permission, with its own rate limit.
  - An operator speaks as an operator and never as a player.
  - It exists because an incident is watched as it happens, and the page and the socket already exist.
- Queue counts need `players.read`. Reporter identity and chat text need the moderation permissions, and every read of chat text is audited with the subject.

**Acceptance**

- [ ] A report filed in game appears in the queue with its subject, realm and zone, and claiming it blocks a second moderator with the claim shown (integration test with a test client filing a report)
- [ ] A chat search for a reported player returns that player's lines with their surrounding lines, and nothing from an unrelated account (database integration test)
- [ ] Muting from a report links the mute to that report and shows in the subject's mute history with who, why and how long (integration test)
- [ ] A user with `players.read` and no moderation permission sees queue counts and no chat text, and the refusal is audited (route test)
- [ ] Dismissing a report records who and why and leaves it searchable (route test)
- [ ] Every read of chat text writes an audit row naming the subject and the range read (route test)

## 17.64 Installation maintenance mode

**Goal:** An operator closes the whole installation to players for database work while game masters can still sign in and check it.

**Size:** M. **Depends on:** 17.12, 17.14, 2.14

**Deliverables**

- `Login.Maintenance`, a live setting through 17.12, that closes the login server to players: 2.14's authentication answers with the maintenance reason the client shows, and the realm list offers them nothing
- `Login.MaintenanceBypassLevel`, a live setting, letting accounts at or above that level sign in normally while maintenance is on; its default is game master (2), as Panel operations in doc/ARCHITECTURE.md settles
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
- The operator's own signing key for patch components: generated or imported, its public part and fingerprint shown, rotation with an overlap window, the private part sealed in the supervisor's keyring, never in a response, and exportable only by an owner after a 17.47 step-up check; the key is Ed25519 and rotates with signatures from both keys through the window, as Panel operations in doc/ARCHITECTURE.md settles
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
- The rule and the fired-alert record carrying a group key, a parent rule that suppresses them, a composite condition and a maintenance-window suppression flag from the day they are written, even though 17.85 is what uses them, because two fields retrofitted into a table of fired alerts is the painful version; a composite condition is also what stops the commonest false page on a small installation, high memory while a backup runs
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

- A public read-only page on the panel listener at its own path, off unless `Panel.PublicStatus.Enable` is set, showing each realm's up or down state, whether the login server is accepting players, and the current or next maintenance window from 17.64; once 17.94 lands the page takes its uptime figures from there rather than computing a second set of its own
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

- `Backups.Encrypt`, on by default: the archive sealed with Botan's AES-256-GCM under a key in the supervisor's keyring, so an archive holding account verifiers and the config that holds `Account.VerifierKeys` is unreadable without it, and an owner asked to export that key until they have, as Panel operations in doc/ARCHITECTURE.md settles
- A key generated once, never written into an archive or a backup record, exportable only by an owner after a 17.47 step-up check, with the plain warning that a lost key makes every sealed archive unreadable
- Restore, 17.52's downloads and 17.43's catalog scan all recognizing a sealed archive, naming the key id they need and refusing clearly when it is absent
- Databases dumped as structured rows and restored through prepared inserts, never as SQL text, as Panel operations in doc/ARCHITECTURE.md settles

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
- A contrast gate in the generator that computes every documented text and ground pair and refuses to generate when one falls below 4.5:1, or below 3:1 for text at 24 px and above and for a focus indicator, and a second, non-text pair set at 3:1 covering every control edge against every surface it may sit on, because a gate that passes while an empty input is a 1.06:1 rectangle is worse than no gate, since it gets cited as proof
- `packages/ui/`, the `@ambrose/ui` workspace package that both apps import: no build step, raw components exported through the `svelte` condition, with the generated tokens, the shared motion module and its duration constants, the typed host bridge, and the named icon set
- The component set the panel and the launcher both need, grouped as doc/COMPONENTS.md lists them: foundations, controls, containers and overlays, shell and navigation, state and meaning, and data; behaviour from the primitive library, look from the tokens, and each one carrying its states, its keyboard behaviour and its label rules, with every component that renders a collection shipping four stories, loading, empty, zero results and error, since Ambrose has an unusual number of legitimately empty surfaces on its first day
- The three fonts vendored as latin variable files with our own face rules and their licence text, so a surface loads no font from any host
- A gallery in `packages/ui/.storybook` showing every component in every state, with a tokens page printing each token's contrast on each surface and an icons page listing the icons in use, built as static files that are never served to an operator
- Tests in four projects: logic and tokens with no browser, every component and every story in both Chromium and WebKit with the accessibility gate, screenshots in a container off the blocking path, and the scaffolding for the end-to-end project the surfaces will use
- Repository checks in the existing checks job: the generated files regenerate identically, no raw colour, arbitrary value or inline style carrying a colour appears outside the generated token file, every component has a story, and a built bundle contains no absolute http or https URL
- `apps/ci/ci_npm_cache.py`, which primes one npm cache for both Windows and Linux so an offline machine installs and builds, and a CMake option that skips the front-end build with a message naming what it left out
- A front-end CI job on Linux only, path-filtered to the front-end folders and the lockfile, and doc/COMPONENTS.md with the recipe and the scaffold command an agent follows to add a component

**Acceptance**

- [x] Changing one value in `design/tokens.json` and running the generator changes the CSS, the TypeScript, the C++ header and the doc/DESIGN.md table together, and `designtokens.py --check` fails when any generated file is edited by hand (verified on 2026-09-18: adding `edge-control` and the three value tokens rewrote packages/ui/src/tokens/tokens.css, tokens.ts, src/common/Design/Tokens.h and the two doc/DESIGN.md tables in one run, and `--check` had reported doc/DESIGN.md stale before it)
- [x] Lowering one text token's lightness until a documented pair falls under its ratio makes the generator refuse, naming the pair and the ratio it reached (verified: the first light ramp written for the value tokens was refused with `light: semantic.color.value-name (#3869AA) on semantic.color.surface-sunken (#EADFC4) reaches 4.21:1, under the 4.5:1 needed for a value highlighted inside a message, on every surface`, and no file was written)
- [x] A C++ test reads the generated header and a component test reads the generated stylesheet, and both fail when one token's value differs from the other's (verified both ways on 2026-09-18: changing `edge-control` to `#546AA6` in tokens.css alone fails DesignTokensTest.TheHeaderAndTheStylesheetHoldTheSameValues, and changing it in either generated file alone fails the component tests, which now read every block of the stylesheet and every array of the header rather than looking for the value anywhere in the file)
- [x] A component that writes a hex colour, a Tailwind arbitrary value or an inline style carrying a colour fails the checks job, and the allow comment is the only way past it (verified both ways: a `#123456` added to Badge.svelte failed the check by file and line, adding that line to apps/ci/ci_frontend_allow.json made it pass, and removing the entry failed it again, so the allow file is the only way past and every exception is a line a reviewer can read)
- [x] Every component in `packages/ui` has at least one story, proved by a check that fails when a new component file has none (verified: `npm run checks` passes on the tree, and a component added without one failed with `ProbeNoStory.svelte: has no story`)
- [x] Every story passes the accessibility gate at WCAG 2.2 AA, and the canary story with a deliberately unlabelled control fails the run (verified: 142 tests across 63 files pass in one run, and `npm run test:canary` exits 1 on `AnIconWithNoLabel` with `aria-label attribute does not exist or is empty`)
- [x] The three criteria the automated gate cannot see each have their own test and each fails when broken: a focused row under sticky chrome, a target under its floor for the pointer in use, and an authentication field that refuses a pasted value (verified by breaking each on 2026-09-24. Removing `scroll-padding-block` from base.css fails the first with `expected 1 to be greater than or equal to 40.5`, the focused row sitting at the scrollport top under a 40px bar; shrinking one target to 18px fails the second with `expected 18 to be greater than or equal to 24`; and an `onpaste` that calls preventDefault on TextField fails the third. The first test did not fail when broken before this: its fixture used a Tailwind `top-0` class that is not generated for that file, so the chrome had `position: sticky` with `top: auto` and never stuck, and every assertion after it passed for the wrong reason. It now sets the offset itself, asserts the chrome is stuck before measuring against it, and focuses a row that starts just above the fold so the browser must scroll it to the edge and the scroll padding is what decides. The third test also asserted a value it had assigned itself, which proved nothing, and now asks only whether anything cancelled the paste. Carried onto the real sign-in page as SignIn.browser.test.ts 'lets a password manager fill and paste into every field it asks a secret in', which found the admin token field carrying `autocomplete="off"`, against the settled rule that no authentication field blocks autofill; it now names its purpose, and putting the breach back fails the test by field name)
- [x] Every collection component ships its loading, empty, zero-results and error stories, proved by a check that fails when one is missing (verified: DataTable and LogList take a status and show each state through the new CollectionState, their stories carry the four tags, and removing the `state:error` tag from DataTable's stories fails the check by name; packages/ui/collections.json classifies every list-shaped component, and a new one that is classified in neither list fails, which is how the check stays honest as components are added)
- [x] Lowering an edge token until a control edge falls under 3:1 against any surface it may sit on makes the generator refuse, naming the pair (verified: `edge-control` at `#3A4A72` was refused naming both failing pairs, 2.08:1 on sunken and 2.25:1 on chrome, against the 3.0:1 the rule sets)
- [x] The same component set passes in Chromium and in WebKit, and a failure in WebKit alone fails the job (verified: `npm run test` runs the logic, chromium and webkit projects together and passes 142 tests, and webkit alone runs 60 of them in 30 files, so the project is really exercised rather than skipped)
- [x] With the network unavailable, `npm ci --offline --ignore-scripts` from the primed cache installs and both apps build, on Windows and on Linux (verified on both, from one cache primed by `python apps/ci/ci_npm_cache.py`, 451 packages: on Windows `npm ci --offline --ignore-scripts --cache .npm-cache` added 350 packages in 7s and both apps built; the same cache over /mnt on Ubuntu 24.04 added 356, the six more being the Linux platform binaries a Windows install skips, and both apps built there too. The cache is 1.1 GB rather than the 30 to 45 MB doc/UI-STACK.md estimates, which is worth correcting there)
- [x] The built output makes no request to any other host, proved by a check that fails on an absolute http or https URL in the bundle (verified: `npm run build` then `python apps/ci/ci_frontend_checks.py` scans the bundle and reports no problem, and the end-to-end test `the built panel serves itself and asks no other host for anything` passes)
- [x] With reduced motion set, every duration constant is zero and the two things that carry information through motion render their still form, the live dot as a filled dot with the word Live and its sample's age, and the indeterminate indicator as words saying an operation is running (verified: motion.test.ts holds the durations to zero under both the media query and the setting, and two stories assert the still forms in the browser with `data-motion="off"`, the new LiveDot showing a filled dot, the word Live and the age of its sample at an animation duration of 0s, and the indeterminate ProgressBar still reading Running)
- [x] Building with the front-end CMake option off still builds the servers and prints what it left out (verified: configuring without `-DFRONTEND=ON` printed the three lines naming the panel, the launcher window's page and how to turn them on, and the servers and unit tests built from that configure)
- [x] The front-end job runs on Linux only, and a failing gallery or component test fails it (verified after a correction: the job is .github/workflows/front-end.yml, which 17.73 already shipped and which a second job briefly duplicated by mistake. It runs on ubuntu-latest alone and fails on the gallery, on a component test in either browser, on the end-to-end pass or on the canary passing. It was failing on main because the browser system libraries were installed only when the browser cache missed, and a cache holds the binaries but not the apt packages, so WebKit could not load libevent and no test ran at all; the libraries are now installed on every run)

## 17.74 Console log line: columns, parts and per-part color

**Goal:** An operator reads a console line by scanning columns, and color marks only the parts that carry meaning.

**Size:** S. **Depends on:** 17.01

Added on 2026-09-18 at the maintainer's direction. 17.01 shipped a console where the level's color runs the length of the message, so an INFO line is teal from end to end and a WARN line gold from end to end, and the category is not shown at all. This milestone is the rework, with its own checks: 17.01's ticked checks stay as they are and none of them changes meaning here. The layout and the tokens it uses are in doc/DESIGN.md's Log lines and values section, so the format survives outside this text.

**Deliverables**

- The record's parts widened to timestamp, level, thread, category, body, value, punctuation and padding, with each part taking a named role instead of each level coloring a whole line: the level word in the level's color, the body in body text, the timestamp faint and the category quiet, and the whole record dimmed at debug and trace
- A fixed console layout when output is a terminal: a short `HH:MM:SS.mmm` time, the five-character padded level word the file already writes, and the category padded to `Console.CategoryWidth` (18 by default, 0 for inline and unpadded), so the message begins at the same column on every line, with the timestamp brightening on the first line of each new second
- `Console.Colors` keeping its six-code form as a shorthand for the six level roles and gaining a named form for the body, value, category, timestamp and punctuation roles, with `Console.Timestamp`, `Console.CategoryWidth` and `Console.RepeatCategory`, all applying from the next line written, documented in doc/config/logging.md
- Each app's `.conf.dist` console appender turning on the category flag, which is off today, so the column exists at all

**Acceptance**

- [x] At every level the message body renders in the body color and only the level word carries the level's color, asserted on a fake console (AppenderConsoleTest.EveryLevelLeavesTheBodyInTheBodyColor, and the per-level expectations in TerminalGetsAnsiColorsPerLevel, which now read `ESC[91mERROR ESC[0mbroken` rather than the whole line in red)
- [x] Across a run of mixed categories every message starts at the same column, and a category longer than the width is shortened in the middle while the full value still reaches the file and the log stream (AppenderConsoleTest.EveryMessageStartsAtTheSameColumn over three categories of different lengths, and ALongCategoryIsShortenedInTheMiddleAndTheRecordKeepsItWhole, where the console shows `..` and the captured record still carries server.database.connection.pool)
- [x] With output redirected the bytes are identical to the bytes before this milestone, full date and unpadded category included, and hold no escape sequence (AppenderConsoleTest.RedirectedOutputKeepsTheFullDateAndTheUnpaddedCategory: the full date, `[server.login]` unpadded, and no escape byte)
- [x] A six-code `Console.Colors` line written before this milestone still parses and still colors the six levels as it did (AppenderConsoleTest.CustomColorStringApplies, unchanged in what it configures, plus NamedColorsSetTheRolesAndTheLevels and AnUnknownColorNameIsAnError for the new form)
- [ ] Dev-gated: on the maintainer's own Windows console and Linux terminal, a screenful of a real login server start reads as columns with only the level words and the categories marked. Needs a terminal, so it is run by hand and recorded

## 17.75 Console color conventions and the color depth ladder

**Goal:** Ambrose honors the color conventions other command-line programs honor, and sends the color the terminal can actually show.

**Size:** S. **Depends on:** 17.74

**Deliverables**

- One precedence, documented as a table in doc/config/logging.md: `--color=never|auto|always`, then `Console.Colors`, then `NO_COLOR` present and not empty, then `CLICOLOR_FORCE` or `FORCE_COLOR` set and not `0`, then `CLICOLOR` set to `0`, then whether standard output is a terminal and `TERM` is not `dumb`
- `FORCE_COLOR`'s levels 0, 1, 2 and 3 also choosing the depth, and `TERM=dumb` turning color off on Windows as it already does on POSIX
- A depth detected once at start and again on a configuration reload, pinned by `Console.ColorDepth = auto|16|256|truecolor`: truecolor when `COLORTERM` reports it or the environment names a terminal that carries it, 256 on a Windows console host with virtual terminal sequences because it rounds a truecolor value to its own table, 256 when `TERM` ends in `256color`, and 16 otherwise
- The truecolor, 256 and 16 value of every role read from the one header 17.73's generator writes, so the terminal cannot drift from the panel

**Acceptance**

- [ ] `CLICOLOR_FORCE` set to `true`, to `yes` and to `2` each force color on, `FORCE_COLOR=0` turns it off, and `CLICOLOR=0` turns it off on a terminal, each with a unit test against a fake environment
- [ ] `NO_COLOR` turns color off while `Console.Colors = 2` still turns it on, which is the convention that a per-instance setting overrides the variable
- [ ] `TERM=dumb` writes no escape sequence on Windows and on Linux
- [ ] At 16 colors the bytes are identical to the bytes this milestone started with, so nothing already shipped changes appearance
- [ ] A test compares the generated header's value for a role with the panel stylesheet's value for the same role and fails when one is changed alone

## 17.76 Value runs inside a log message

**Goal:** The parts of a message that name a thing are marked by the call that wrote them, not guessed at afterwards by pattern matching in a hot path.

**Size:** M. **Depends on:** 17.04, 17.74

**Deliverables**

- The formatted write path splitting its format string on replacement fields and formatting field by field, recording the byte range of every substituted argument on the record, with no change at any call site and no new macro
- A silent fallback to the single formatting call whenever the walk meets something it does not handle, counted in the logging statistics so the fallback is visible rather than invisible, and the walk performed only while a consumer is attached
- One class filter shared by every path: quoted text, a path or a file and line, an address or connection target, a hex digest, a number with a unit, and a dotted, underscored or all-capital identifier, with a bare count never a value and at most eight marked runs a line
- Each marked run drawn in the value ramp doc/DESIGN.md sets, by its class, never in the line's level color, so a warning line is not gold from its level word to its values
- The same classifier over already-rendered text for lines written as text and for output captured from a child process, working over a view without allocating, behind `Console.Highlight`
- The ranges travelling on the record to the log stream as typed data, so the panel's console colors the same parts from data instead of re-reading text in a browser

**Acceptance**

- [ ] A line whose arguments are an address and a thread count marks the address and nothing else, and the bare count stays body text
- [ ] A golden-file test over a corpus of real log lines reproduces the expected byte ranges exactly and fails when one range moves
- [ ] The three formatting edge cases, doubled braces, a named argument and a dynamic width, each produce the same text as before this milestone, and any failure falls back rather than changing the line
- [ ] A line written by a logging call and the same line captured from a child process's output mark the same runs
- [ ] A line holding twenty numbers carries no more than eight marked runs and none of them is a bare count
- [ ] A warning line draws its level word in the waiting color and each value run in its own class's color from the value ramp, so no line is one color from end to end
- [ ] A log stream subscriber reconstructs exactly the runs the console drew

## 17.77 Console wrapping, hanging indent and the record cap

**Goal:** A long line stays readable on a narrow terminal, and a stack trace does not scroll the operator's prompt away.

**Size:** S. **Depends on:** 17.01, 17.74

**Deliverables**

- The terminal width function moved from `src/server/shared/Console/` into `src/common/`, so the logging layer may measure display width without breaking the layering doc/ARCHITECTURE.md fixes, with the console prompt and its tests following the move
- Wrapping inside the console appender only and on a terminal only: wrap at a word boundary, hang the continuation under the message column, never split a token that holds no space so a path or a digest stays searchable, and stop wrapping when the message column would fall below 24
- `Console.MaxRecordLines`, 20 by default, after which a record ends with a line naming how many more are in the file, while the file, the stream and the database keep every line
- The window width read once per record under the console writer's lock the prompt already takes, with `Console.Wrap = 0` giving the terminal's own soft wrap back

**Acceptance**

- [ ] At 80 columns a message wraps at a space and hangs under the message column, and a 74-character path holding no space overflows whole rather than being split
- [ ] A name in a wide script and a combining mark wrap at the right column, measured by the width function rather than by bytes
- [ ] The file's bytes for the same record are unchanged while the console wraps, and no inserted newline or padding byte reaches a file, the log stream or the database
- [ ] A log line arriving while a command is half typed leaves the typed text alone when the line wraps to several rows, asserted byte for byte on a fake console
- [ ] A 60-line record prints 20 lines and a tail naming the file, and the file holds all 60

## 17.78 Event overlays on every graph

**Goal:** A graph shows what happened as well as what changed, so an operator answers what was done at the moment a figure moved without opening another page.

**Size:** M. **Depends on:** 17.19, 17.25, 17.49

**Deliverables**

- An events-in-range query over the audit store, crash records, schedule runs, backup outcomes, settings generations, reloads, restarts and maintenance windows, each returned with its time, kind, subject, actor and the page that explains it
- Markers on every time series in the panel, as real markup positioned against the chart's scales rather than pixels drawn into the canvas, so each one can be hovered, focused and read, colored by meaning rather than by kind
- A filter choosing which kinds are drawn, remembered per user, and a rule that groups markers closer together than a few pixels into one naming how many
- The same events listed under the graph for the visible range, so the information exists without a pointer
- Permission filtering inside the query, so a marker never reveals an event its viewer may not see, and no marker carries a secret value or a player's identity

**Acceptance**

- [ ] A settings change, a backup, a crash and a schedule run inside the visible range each appear at the right time with the audit row behind them
- [ ] A marker is reachable by keyboard and reads its kind, time and subject to a screen reader
- [ ] A viewer without the permission covering a kind sees neither its marker nor its row, and the graph still draws
- [ ] Twenty events inside one pixel column become one marker naming how many
- [ ] Turning a kind off removes it and the choice survives reopening the page
- [ ] The marker colors come from the token file, proved by the check that forbids a raw color

## 17.79 Correlate a window

**Goal:** An operator selects the minutes where something went wrong and gets a short list of what else moved with it.

**Size:** S. **Depends on:** 17.19, 17.78

**Deliverables**

- A window selection on any graph returning every other series ranked by how far its mean moved inside the window against the stretch before it, with a variance guard so a quiet noisy series cannot rank first
- The ranking computed in the supervisor over the day it already keeps at full detail, with a minimum window and a cap on how many series are scored, so the answer is bounded work
- The result beside the graph as a list of series with their before and after figures and the size of the change, each opening its own graph, alongside the same window's events from 17.78
- The words that this ranks and never concludes, on the page as well as in the documentation

**Acceptance**

- [ ] A run where one series steps and the rest are flat ranks that series first, and a flat noisy series does not rank above it
- [ ] A window shorter than the minimum is refused with the minimum named
- [ ] The ranking answers inside its stated budget with the full set of series a busy installation carries
- [ ] Selecting a window lists the audited events inside it as well as the ranked series
- [ ] A viewer without the metrics permission cannot run it

## 17.80 Log search over history with facets

**Goal:** An operator finds what a session did at eight o'clock yesterday, not only what is on the screen now.

**Size:** M. **Depends on:** 17.04, 17.07, 17.14, 17.76

**Deliverables**

- Full-text search over the logs the supervisor keeps, with a time range, a query and facet counts for app, level, category, realm, zone, request id, session id and account
- A volume histogram above the results drawn with the same chart wrapper the graphs use, so a burst is visible before it is read
- The context around a hit, the lines either side from the same run, and links to the player, the account and the app the line came from
- A retention cap by days and by bytes enforced by the same sweep the activity log uses, and a refusal to write rather than to fill the volume when the space guard says no
- One search engine serving this, the activity log's search and the chat search, so there is one and not three; SQLite's FTS5 in an index file of its own, as Panel operations in doc/ARCHITECTURE.md settles
- Every identity field behind the permission that covers it, and export through the formula-safe writer

**Acceptance**

- [ ] A line written an hour ago is found by its session id inside its range, and the facet counts agree with the rows listed
- [ ] Asking for the context around a hit returns the lines either side from the same run
- [ ] The retention cap removes the oldest rows once either limit is passed, and the page says what is kept
- [ ] With the volume near its reserve the writer refuses and says so rather than filling the disk
- [ ] A user without the account permission sees the line and not the account it names
- [ ] The histogram's buckets and the result count agree for the same query

## 17.81 Synthetic login probe and end-to-end health checks

**Goal:** Ambrose knows whether a player can actually log in, measured from outside, not only that its own processes are running.

**Size:** M. **Depends on:** 17.15, 17.19, 17.67, 3.24

**Deliverables**

- A scheduled probe running the client driver's own scenario against the installation's public address: connect, handshake, authenticate a probe account, fetch the realm list, select a character, enter the world, move and log out
- Each step timed and recorded as its own series, so a slow handshake and a slow world entry are different lines
- A probe account marked as one, left out of the analytics, refused a second concurrent use, with its credentials in the supervisor's keyring
- Certificate and patch manifest checks on the same schedule: expiry, reachability and time to first byte
- Every step offered to the alert rules as a condition, and the result on the overview as the one figure that says the game is reachable
- A probe that cannot start because the driver or the account is missing reporting as unknown, never as down

**Acceptance**

- [ ] With the login server stopped the probe reports the connect step failing and the overview says the game is unreachable, and starting it again clears both
- [ ] With the login server running but its port unreachable the probe fails while every internal figure stays healthy, which is the case this milestone exists for
- [ ] Each step's time is a series on the graphs carrying the same figure the probe's own record shows
- [ ] The probe account cannot be used by a person and does not appear in the analytics
- [ ] A certificate inside its expiry window raises its rule once, not once per run
- [ ] Removing the probe account makes the probe report unknown rather than failing

## 17.82 Installation health checks with fixes

**Goal:** One page says whether this installation is set up correctly, with a link to the page that fixes each thing that is not.

**Size:** M. **Depends on:** 17.03, 17.06, 17.16, 17.47

**Deliverables**

- A registry of checks, each with a code, a severity, a sentence naming what is wrong and what it costs, and the route that fixes it, built on the problem codes the status API already publishes
- The checks a self-hosted installation actually gets wrong: plain HTTP beyond loopback, no verified backup inside a window, two-factor not required, a first-run owner grant still held, disk headroom under the reservation, a client revision the servers did not load, a stale type dump, pending database updates, and a patch signing key missing or near expiry
- A page listing findings by severity with their fixes and never a score, because a score invites gaming rather than fixing
- Each check registrable as an alert rule, so the page and the alerts cannot disagree
- A check whose subject has not been built reporting as unknown and naming its milestone, never as healthy

**Acceptance**

- [ ] Turning on plain HTTP beyond loopback shows it as wrong with the setting named and a link that opens it, and turning it off clears the finding
- [ ] With no backup inside the window the page says so with the date of the last one, and a completed backup clears it
- [ ] A check registered here and as an alert rule fires from the same figure, proved by a test that trips both
- [ ] A finding whose subject is not built reads as unknown naming its milestone
- [ ] A viewer sees only the findings whose subjects they may see

## 17.83 Crash dumps, symbolization and grouping

**Goal:** A crash at three in the morning leaves a stack, and the second crash of the same fault is one row with a count.

**Size:** M. **Depends on:** 17.18, 17.60

**Deliverables**

- A crash writer in each app producing a dump at the moment of a crash, through the operating system's own writer on each platform, `MiniDumpWriteDump` on Windows and a small writer of our own on Linux, into a root the file manager already governs, with a size cap and a retention count
- A fingerprint from the top frames, so crashes group into one record with a count, a first-seen build and a last-seen time
- Symbolization out of band against the build's own symbols, with the build recorded so a dump from an older build is still readable
- A crash page listing groups, each opening the stack, the log lines the crash record already keeps, and the app, realm and revision it happened on
- The dump path the crash record already carries actually written, read and offered for download behind a step-up check, since a dump can hold memory
- Settled on 2026-09-18: a crash reporting library such as Crashpad is not a dependency here, because its build cost does not fit the continuous integration budget; it stays an option to revisit, and the grouping, the page and the checks below do not depend on which writer produced the dump

**Acceptance**

- [ ] A deliberate crash in a test build writes a dump and the page shows its stack with function names
- [ ] The same crash three times is one group with a count of three, and a different stack is a different group
- [ ] A dump from a previous build symbolizes against that build's symbols
- [ ] Downloading a dump needs a step-up check and writes an audit row
- [ ] The retention count removes the oldest dumps and the space guard refuses a write rather than filling the volume
- [ ] With dumps turned off the crash record still carries its classification and its log lines

## 17.84 Support bundle

**Goal:** A maintainer can debug someone else's installation from one file, without a screen share and without a shell.

**Size:** S. **Depends on:** 17.08, 17.18, 17.60

**Deliverables**

- One archive holding versions and build commit, the effective configuration with every secret redacted by the rule file reads already use, the last lines of each app's log, crash records, system and volume figures, the installation health findings, and schedule and backup outcomes
- A manifest inside the archive naming exactly what it holds and what it deliberately leaves out, so an operator knows what they are sending
- Nothing from the client install and no player identity, proved by a response shaping test, in the archive format backups already use and through the same space guard
- Its creation audited and behind its own permission

**Acceptance**

- [ ] The bundle holds every section its manifest names and nothing else
- [ ] A known configuration secret and a known account verifier appear nowhere in its bytes
- [ ] No file from the client install root is included, proved by the shaping test
- [ ] Creating a bundle is audited with who and when, and a viewer is refused
- [ ] With the volume near its reserve the bundle is refused rather than written

## 17.85 Alert grouping, inhibition and silences

**Goal:** A node going down sends one notice naming what it took with it, not fourteen.

**Size:** M. **Depends on:** 17.22, 17.67

**Deliverables**

- Grouping in the delivery path, so alerts about the same subject arriving within a short wait become one notice naming each of them
- Inhibition, so an alert whose parent is firing is suppressed and counted rather than sent, with the parent naming what it suppressed
- Silences by matcher rather than only per rule: everything about a node, a realm or an app, for a window, with a reason and who set it, audited and expiring on its own
- Suppression by maintenance window, so a planned restart does not page the operator about their own restart
- The full history kept, so a suppressed alert is visible on the page even though it was never delivered

**Acceptance**

- [ ] A node going offline with two apps, two realms and a failed backup on it sends one notice naming the suppressed alerts and their count
- [ ] A silence by matcher mutes every alert about its subject, expires on its own, and records its reason and who set it
- [ ] An alert inside a maintenance window is suppressed and one naming the window is sent instead
- [ ] Alerts arriving inside the group wait become one notice, and one arriving after it is its own notice
- [ ] Every suppressed alert appears in the history, so nothing is lost, only undelivered
- [ ] A rule with no parent and no group behaves exactly as it did before this milestone

## 17.86 Notification routing, preferences and delivery test

**Goal:** Each operator gets the notices that concern them, in a channel they read, and can prove the channel works before they need it.

**Size:** M. **Depends on:** 17.38, 17.48, 17.67

**Deliverables**

- Routing by severity and by scope, so an operator receives only alerts about the apps, realms and nodes their grants cover
- Per-user channels and preferences: the notification center, email, a templated webhook and a self-hosted push endpoint, with quiet hours and a minimum severity per channel
- A test send per channel reporting its delivery result, with failures retried with backoff and shown on the page
- One templated webhook body covering the services that accept an HTTP post, documented with its fields, and no third-party push service, because that would send an operator's notices off the machine
- The payload rule enforced in one place: no secret, no player email or address, only the subject and the figure that tripped the rule

**Acceptance**

- [ ] An operator whose grants cover one app receives that app's alerts and not another app's
- [ ] A test send reports success and failure honestly, and a failed delivery is retried and shown
- [ ] Quiet hours hold a notice until they end while a critical alert still goes out
- [ ] A webhook body carries no secret and no player identity, proved by a shaping test
- [ ] Changing a preference applies to the next alert with no restart
- [ ] Removing a user's grant stops their notices about that subject from the next alert

## 17.87 Event-triggered automation

**Goal:** The panel acts on what happens, not only on the clock.

**Size:** M. **Depends on:** 17.15, 17.26, 17.60, 17.67

**Deliverables**

- Triggers on the events the panel already publishes: a crash, a crash loop, an exit with its classification, a failed backup, a failed reload, a queue above a threshold, a realm offline, a node offline and a client revision change
- A trigger running the same task chain schedules already own, under the same rule, so arming one needs every permission its tasks need
- Guards: a cooldown per trigger, a cap on runs per hour, and a refusal to arm a trigger whose own action can raise the event that fires it without a cooldown
- A run history per trigger showing what fired it, what ran and what happened, in the record schedules already keep
- Triggers loading live with an atomic swap, keeping the previous set when a new one fails to load

**Acceptance**

- [ ] A crash fires its trigger once and the cooldown holds a second crash inside the window
- [ ] A trigger whose task the arming user may not run is refused, naming the permission
- [ ] A trigger that restarts an app which then crashes does not loop, stopped by the cap with a record saying so
- [ ] A trigger's run appears in the history with its cause, its tasks and its outcome
- [ ] An invalid trigger set leaves the previous set running and reports the error
- [ ] Disabling a trigger stops it firing without removing its history

## 17.88 Command palette and keyboard-first operation

**Goal:** An operator reaches any page, object or action they are allowed without knowing where it lives.

**Size:** S. **Depends on:** 17.06, 17.21, 17.48, 17.73

**Deliverables**

- One palette over three sources, the route table, the objects the global search returns and the actions each page registers, every one filtered by permission on the server, because a palette that offers a command and then refuses it is a permissions leak
- A shortcut registry that is both what binds the keys and what renders the shortcut sheet, so the documented shortcut and the real one cannot disagree, with no binding firing while focus is in a field or the editor
- The sheet opened with one key, grouped by scope and filtered to what the current page and the caller's permissions allow
- Every palette string in the locale catalogs, and every action confirmed exactly the way it is confirmed on its own page

**Acceptance**

- [ ] An action the caller may not run never appears, proved for a user whose grants were narrowed while the page was open
- [ ] Every route in the route table is reachable from the palette, proved by a check that fails when a route is added without a name
- [ ] A shortcut listed in the sheet performs the action it names, proved from the registry rather than from a fixture
- [ ] No shortcut fires while focus is inside an input, a textarea or the editor
- [ ] The palette is operable by keyboard alone from open to action and passes the accessibility gate

## 17.89 Kill switches

**Goal:** During an incident an operator turns one thing off in five seconds, and cannot forget it is off.

**Size:** S. **Depends on:** 17.12, 17.13, 17.27

**Deliverables**

- A short curated page of named switches over settings that already exist, such as registration, trading, the bazaar, character creation, patch downloads and new logins, each with one sentence saying what it breaks
- A confirmation naming that effect, a reason the operator must give, an audit row, and who engaged it shown beside the switch
- A banner on the overview and on the public status page for every engaged switch, and a line in the digest while any is engaged
- An engaged switch surviving a restart and reported by the status API, so nothing silently comes back on

**Acceptance**

- [ ] Engaging a switch takes effect live with no restart, and writes the same setting the settings page shows
- [ ] A switch cannot be engaged without a reason, and the reason and operator are audited
- [ ] The overview and the public status page both show an engaged switch
- [ ] A restart leaves an engaged switch engaged
- [ ] A user without the permission the underlying setting needs cannot engage it

## 17.90 Slow query and database health page

**Goal:** When the pool saturates, the panel says which statement did it.

**Size:** M. **Depends on:** 17.08, 17.09, 17.30

**Deliverables**

- Timing on every statement at the layer the server already owns, kept as a top list by total time and by longest single run, per database and per generation
- The call site recorded with each statement, so a slow query names the code that issued it
- A page showing the top statements, the pool's waiters and longest wait, lock and deadlock counts where the driver reports them, and the pending update list the database page already shows
- Bound parameters redacted by default, because a statement's parameters can carry a player's name or address, with the full text behind its own permission
- Slow-statement and pool-saturation conditions offered to the alert rules, and the figures added to the graphs

**Acceptance**

- [ ] A deliberately slow statement appears in the top list with its total time, its longest run and its call site
- [ ] Parameters are redacted for a user without the permission and present for one with it
- [ ] The top list resets per generation and cannot grow without bound
- [ ] The pool figures on the page are the figures the status API reports
- [ ] Timing costs nothing measurable with the page closed, proved by a benchmark in the test suite

## 17.91 Tick breakdown and on-demand profiles

**Goal:** A slow tick says where the time went, not only that it was slow.

**Size:** M. **Depends on:** 17.09, 17.19

**Deliverables**

- Accumulators per subsystem inside the world tick, network drain, movement, scripting and database waits to begin with and each later system as it lands, published through the metrics registry the way tick time already is
- The breakdown drawn under the tick graph, with the subsystem that grew named in words as well as by color
- A capture button recording a bounded profile for a stated number of seconds and offering it as a Chrome trace event file, as Panel operations in doc/ARCHITECTURE.md settles, off by default, never leaving a listener open
- A budget per subsystem, so the breakdown says which part went past its share rather than only which is largest
- The breakdown offered to the alert rules, so a tick alert names a subsystem

**Acceptance**

- [x] A test that makes one subsystem slow shows that subsystem growing, and the parts sum to the tick time inside a stated tolerance (WorldTest.TickBreakdownNamesTheSlowSubsystemAndAddsToTheTick: an injected 15 ms queue delay identifies network_drain, and the subsystem nanoseconds sum to the tick histogram within 0.5 ms)
- [ ] The accumulators cost nothing measurable while nothing subscribes, proved by a benchmark
- [x] A capture runs for the requested seconds, produces a file, and leaves nothing listening afterwards (WorldTest.TickProfileIsBoundedTimedAndReadableAsAChromeTrace and WorldTest.TickProfileStopsAtItsEventBound; Overview.browser.test.ts downloads the Chrome trace; npm run test:browser -- --project dashboard-browser apps/dashboard/src/pages/Overview.browser.test.ts passed 6/6; capture uses the existing admin API and opens no listener)
- [ ] A tick alert names the subsystem that grew
- [x] A subsystem whose milestone has not landed reads as unavailable naming it, never as zero (WorldTest.TickBreakdownNamesTheSlowSubsystemAndAddsToTheTick reads database_waits and combat as unavailable with their reasons and movement, which 6.01 landed, as timed; Overview.browser.test.ts names each unavailable subsystem rather than showing 0; browser suite passed 6/6)

## 17.92 Per-session network quality and the player inspector

**Goal:** When a player says the game is laggy, the panel answers with that session's own figures.

**Size:** M. **Depends on:** 17.19, 17.80, 17.175, 17.177

Changed on 2026-09-27: the player inspector is the 17.175 wizard page grown, and the online players list is 17.177's, so this milestone adds to both rather than building a second player page.

**Deliverables**

- Counters per session in the game and login servers: round-trip time from the protocol's own exchanges, loss where the transport reports it, jitter, and bytes and messages a second, kept as a rolling window on the session
- The figures shown as a small chart per row in the slot 17.177's online players list reserves, and in full on the wizard's page, with the session's history for as long as the session has lasted
- The 17.175 wizard page becomes the player inspector, adding, each behind the permission that covers it:
  - The session's recent log lines, joined by correlation id.
  - Its recent actions from the audit log.
  - Its open reports.
- Session quality added to the graphs per realm as a distribution, so a realm-wide problem is visible without opening any player
- Every identity field behind the permission that covers it, so a game master sees the quality without the account

**Acceptance**

- [ ] A session with a deliberately delayed client shows a higher round-trip time than a healthy one, measured from the server's own exchanges (integration test with two test clients, one behind an added delay)
- [ ] The figures on the row and on the player page are the same figures at the same moment (browser test)
- [ ] The log lines shown for a session are that session's, joined by correlation id (integration test)
- [ ] A user without the identity permission sees the quality figures and not the account (route test)
- [ ] A realm-wide slowdown is visible in the distribution without opening a player (integration test that delays every test client on a realm)
- [ ] The counters add less than one percent to the handling time `ambrose_message_handle_seconds` measures per message (benchmark)

## 17.93 Tamper-evident audit chain and audit streaming

**Goal:** An audit row that was changed or removed can be shown to have been.

**Size:** S. **Depends on:** 17.14, 17.49

**Deliverables**

- A hash over each audit row and the hash before it, written in the same transaction as the row on the column 17.49 already carries, so a partial edit breaks the chain
- A verify command and a panel page that walk the chain and name the first row that does not agree
- An optional append-only copy of every audit row to an outside collector, with a queue and backoff, so a local deletion does not remove the only copy
- The limit written where an operator reads it: a chain on the same machine proves nothing against someone who can rewrite the whole chain, and the copy off the machine is what does the work

**Acceptance**

- [ ] Changing one audit row makes verify report that row and every row after it
- [ ] Deleting a row is reported the same way
- [ ] A row that cannot be chained is not written, because the hash and the row commit together
- [ ] With the collector unreachable the queue holds rows and drains when it returns, and the page says how many wait
- [ ] Verifying a large audit store completes inside its stated budget

## 17.94 Uptime history and incident timeline

**Goal:** A player and an operator can both see whether the game was up yesterday, and what happened when it was not.

**Size:** S. **Depends on:** 17.70, 17.81

**Deliverables**

- Uptime percentages per realm and for the login path over a day, a month and a year, computed from the probe's own results rather than from the servers' opinion of themselves, and served as the one source 17.70's public page reads
- A timeline of incidents with start, end, a one-sentence cause and the maintenance windows that were planned, with an operator's note where one was posted
- The aggregate-only rule the public page already carries, with its response shaping test extended to the new fields
- A sparkline per realm on the public page, and the same figures inside the panel for the operator

**Acceptance**

- [ ] A stretch where the probe failed appears as downtime with the right start and end, and the percentages follow
- [ ] A planned maintenance window is shown as planned and not as an incident
- [ ] The payload carries no player name, address or account figure, proved by the shaping test
- [ ] The percentages on the public page and inside the panel are the same figures

## 17.95 Character point-in-time restore and undelete

**Goal:** One player's loss is repaired without rolling the whole database back on everybody else.

**Size:** M. **Depends on:** 17.16, 17.51, 17.177, 3.17

Changed on 2026-09-27: the characters page moved from 17.21 to 17.177, so this milestone waits for 17.177.

**Deliverables**

- Reading one character's rows out of a backup archive through 17.16's per-character read path, without restoring the archive, listing what that snapshot holds for that character
- A difference between the snapshot and the live rows, per table, shown before anything is written
- Applying a chosen subset inside one transaction, with an audit row naming the operator, the reason, the snapshot and every table touched
- Undelete of a character through the delete path phase 3 already owns, inside the window that delete keeps
- The duplication guard that makes this safe:
  - A restore invalidates the source rows in the same transaction.
  - It refuses to run twice on the same snapshot and target without an explicit override.
  - It writes everything it creates into the ledger.
- A dry run as the default, and the whole operation behind its own permission with a step-up check

**Acceptance**

- [ ] Items restored from a snapshot exist exactly once, with the source rows invalidated in the same transaction (database integration test)
- [ ] Running the same restore twice is refused without an override, and the override is audited naming who gave it (route test)
- [ ] The difference shown before the write is what the write actually changes (database integration test)
- [ ] A restore aimed at a character who is in the world is refused, naming why (integration test)
- [ ] An undelete inside the window returns the character, and outside it is refused naming the window (database integration test)
- [ ] A dry run changes nothing and produces the same report the real run does (database integration test)
- [ ] A user without the permission, or without the step-up check, is refused and the attempt is audited (route test)

## 17.96 Operations calendar

**Goal:** One view of everything planned, so a double-experience weekend is not booked over a migration.

**Size:** S. **Depends on:** 17.15, 17.32, 17.33, 17.64, 17.68

**Deliverables**

- One calendar over the four sources of planned events, schedules, timed game events, installation maintenance and realm maintenance, each carrying its own meaning color
- A week and a month view in the operator's own time zone, naming the node's zone where it differs
- A conflict warning where two planned things overlap in a way that matters, such as an event running across a restart
- Each entry opening the page that owns it and nothing edited here, so each thing is still edited in one place

**Acceptance**

- [ ] An event from each of the four sources appears at the right time in the operator's zone
- [ ] An event spanning a planned restart is marked as a conflict
- [ ] Changing a schedule moves its entry without reopening the page
- [ ] An entry the viewer may not see is not shown
- [ ] A schedule's next run is the same time here and on the schedules page

## 17.97 Daily operations digest

**Goal:** An operator reads one message a day instead of watching.

**Size:** S. **Depends on:** 17.67, 17.69, 17.86

**Deliverables**

- One daily message: peak and average players, new accounts, crashes by group, alerts raised and acknowledged, backup and schedule outcomes, disk and capacity headroom, and anything that failed
- Sent through the routing and channels alerts already use, at a time and in a zone each operator chooses
- The payload rules alerts carry, and a link into the panel for each line rather than the detail itself
- A figure whose milestone has not landed named as unavailable rather than reported as zero

**Acceptance**

- [ ] The digest's figures are the figures the pages they come from show for the same day
- [ ] A day with nothing to report still sends, saying so, because silence is indistinguishable from a broken digest
- [ ] Each operator receives it at their own time in their own zone
- [ ] A figure whose milestone has not landed is named as unavailable

## 17.98 Capacity forecasts and the weekly load heatmap

**Goal:** An operator is told that a volume fills in six days, and when it is safe to restart.

**Size:** S. **Depends on:** 17.19, 17.44, 17.69

**Deliverables**

- A fit over the stored history for each volume and each growing store, reported as the date it runs out, refused when the history is too short to say
- A rule offered to the alerts for a resource forecast to run out inside a window, so the notice comes before the disk is full rather than when it is
- A grid of median players by hour and weekday as real markup, so it is selectable, printable and readable, taking the sequential ramp settled under Panel operations in doc/ARCHITECTURE.md, with the quietest hours named in words as well
- Those quietest hours offered to the schedule editor as the anchor for a restart, which is what makes the player-aware conditions chosen rather than guessed

**Acceptance**

- [ ] A volume filling at a steady rate is forecast inside a stated tolerance, and one with too little history says so rather than guessing
- [ ] The forecast rule fires before the low-disk rule does on the same data
- [ ] The grid's medians are the stored history's medians for the same hours
- [ ] The grid is readable by a screen reader and prints
- [ ] The schedule editor offers the quietest hour and the operator may ignore it

## 17.99 Declarative installation file with diff and apply

**Goal:** An installation can be written down, compared with what is running, and rebuilt from the file.

**Size:** M. **Depends on:** 17.12, 17.28, 17.29, 17.36, 17.48

**Deliverables**

- One file describing apps, launch settings, ports and addresses, realms, schedules, alert rules, notification channels, file roots, and panel roles and grants, in the types the settings schema already generates; its format is TOML 1.0 through toml++, as Panel operations in doc/ARCHITECTURE.md settles
- An export writing the running installation into that file, with every secret a reference into the keyring and never a value
- A difference between the file and what is running, shown before anything is written, in the review shape the settings editor already uses
- An apply that is idempotent, refuses anything the caller's permissions do not cover, and stops at the first refusal with nothing half applied
- Validation against the same schema the settings API serves, so a file written for another build is refused with its fields named

**Acceptance**

- [ ] Exporting an installation and applying that file back to it changes nothing
- [ ] Applying a file with one changed value changes that value and nothing else, and the difference said so first
- [ ] No secret value appears in the exported file, proved by a shaping test
- [ ] A caller lacking a permission the file needs is refused naming it, with nothing applied
- [ ] A file naming a field the running build does not know is refused naming the field
- [ ] Applying the same file twice leaves the same installation and writes one audit record per real change

## 17.100 ambrosectl: one command line over the panel API

**Goal:** Everything the panel does can be done from a terminal, by a person or by a script.

**Size:** M. **Depends on:** 17.36, 17.48, 17.99

**Deliverables**

- One program speaking the panel's own API with a personal API key, covering apps and power, settings, schedules, backups, files, users and grants, and the declarative file's export, difference and apply
- Human output by default and machine output on request, with exit codes that mean something, so a script can act on a failure
- The same permission answers the panel gets, the same audit rows, and no path that skips a confirmation the panel requires
- Built and shipped for Windows and Linux with the rest of the release, its command documentation generated from the same route table the panel reads
- A refusal that names the permission or the reason, and never a stack trace

**Acceptance**

- [ ] Every route in the route table is reachable from the command line, proved by a check that fails when a route is added without a command
- [ ] A key without a permission is refused naming it, and the refusal is audited exactly as the panel's is
- [ ] Machine output is stable across a release, proved by a schema test
- [ ] A destructive command needs the same confirmation the panel needs, and refuses to run unattended without an explicit flag, which is audited
- [ ] The generated command documentation matches the routes, proved by a check
- [ ] The program runs on Windows and on Linux from the release artifacts

## 17.101 Content packs: install, version and uninstall

**Goal:** Authored content from outside the project is installed, listed and removed on a running server instead of being merged into it.

**Size:** M. **Depends on:** 17.18, 17.34, 17.65

**Deliverables**

- A pack format: a manifest naming the pack, its version, what it touches, its checksums and its licence, with its dated database updates and its data files
- An install running inside the same journal world edits already keep, so everything a pack did is recorded and reversible
- An uninstall reversing what the install did, refused when a later pack or an operator's own edit depends on it, naming what depends
- A packs page listing what is installed, its version, where it came from and what it touches, with an update to a newer version through the same journal
- Signature verification against the operator's own trusted keys, reusing the signing work the patch server needs, with an unsigned pack allowed only on an explicit acknowledgement
- The rule that a pack never carries a file from the game client, checked on install and stated in the format

**Acceptance**

- [ ] A pack installs, appears in the list with its version, and its rows are present in the world database
- [ ] Uninstalling it removes exactly what it added and nothing an operator changed afterwards
- [ ] A pack that depends on another is refused when its dependency is absent, naming it
- [ ] An unsigned pack is refused unless the operator acknowledges it, and the acknowledgement is audited
- [ ] A pack carrying a file that came from a client install is refused on install
- [ ] Installing the same pack twice is refused, and an update to a newer version records both versions
- [ ] A failed install leaves the database as it was

## 17.102 Outbound event webhooks

**Goal:** A community can build things around a server, from events the server already publishes.

**Size:** S. **Depends on:** 17.26, 17.67

**Deliverables**

- Subscriptions with an event filter and a shared secret, delivering signed messages for the events worth broadcasting, such as the server coming back up, maintenance starting, a realm opening or closing and a revision changing
- The retry with backoff, the delivery record and the page the alert channels already use
- The payload rules alerts carry, with player names off by default, since naming a player needs a consent model that does not exist yet
- A rate limit per subscription and a subscription disabled after a stated run of failures, with the operator told

**Acceptance**

- [ ] A subscribed event arrives signed and the signature verifies with the shared secret
- [ ] An unsubscribed event does not arrive
- [ ] The payload carries no secret, no player email or address, and no player name while names are off
- [ ] A failing endpoint is retried with backoff and disabled after the stated run, with the operator told
- [ ] A subscription is audited when created, changed and removed

## 17.103 Item and currency ledger with anomaly rules

**Goal:** Where an item came from is a question the server can answer.

**Size:** M. **Depends on:** 17.25, 17.69, 8.08

**Deliverables**

- A row for every item and currency mutation carrying its source, reason, actor, character and a correlation id, written by the systems that mutate them rather than by the panel
- A page following one item or one account's currency through its own history, and a query by source
- Rules over it: an account gaining more than a stated amount an hour, an item whose live count exceeds what the ledger accounts for, and one account that is the sink of many trades, each reporting and never acting on its own
- A retention cap by days and by bytes with the sweep and the space guard the log history already uses
- Every identity field behind its permission, and exports through the formula-safe writer

**Acceptance**

- [ ] Every path that creates, moves or destroys an item writes a ledger row, proved by a test that fails when a new path does not
- [ ] Following one item shows each step in order with its source and actor
- [ ] A deliberate excess gain raises its rule, and the rule takes no action on its own
- [ ] The count check finds a discrepancy planted in the test data
- [ ] The retention cap removes the oldest rows and the page says what is kept
- [ ] A user without the identity permission sees the movement and not the account

## 17.104 Compensation grants and mass mail

**Goal:** After a bad night the operator can give the affected players something, safely and on the record, instead of typing SQL at a live game database.

**Size:** M. **Depends on:** 17.21, 17.33, 17.69, 8.08

**Deliverables**

- A grant taking a cohort, a thing to give and a reason, where the cohort is a query the analytics already build, such as everyone who was in a realm during a window
- A preview with the exact recipient count and a sample of who, before anything is sent, with a dry run as the default
- Caps and approval: a value cap per grant in the settings, a second operator's approval above a threshold, and an audit row naming both operators, the cohort, the reason and the exact count
- Every unit created written into the ledger, so a compensation is as traceable as a drop
- Delivery to the character directly where the game supports it, and by mail once the milestone that owns mail lands, naming that milestone until then
- A grant that cannot run twice on the same cohort and reason without an explicit override, which is audited

**Acceptance**

- [ ] The preview's count is exactly the number of accounts the grant then affects
- [ ] A dry run changes nothing and produces the report the real run produces
- [ ] A grant above the value cap is refused, and one above the approval threshold waits for a second operator
- [ ] Every unit granted appears in the ledger with the grant as its source
- [ ] Running the same grant twice is refused without an override, and the override names both operators
- [ ] A user without the permission is refused and the attempt is audited
- [ ] While the mail milestone has not landed the page says so and offers only what exists

## 17.105 Releases: the panel program and the launcher as downloadable builds

**Goal:** Somebody who does not build from source downloads the panel program, and the launcher once 3.27 makes it an app of its own, from the repository's releases page. They install them and run a server or play with the game as it stands, and each release is rebuilt by CI rather than by hand.

**Size:** M. **Depends on:** 17.14, 17.24, 17.183

Added on 2026-09-22 at the maintainer's direction: the panel and the launcher are the two programs a player or an operator touches, and both have to be handed to people who will never open a compiler. A release is a tag, and the tag is what builds it, so the page never carries a build a human assembled.

Changed on 2026-09-27 at the maintainer's direction, who asked for the panel as its own installable program:

- The panel no longer waits for the launcher's app in 3.27. The workflow publishes the panel program of 17.181 with its hosting (17.24), packaged by 17.183.
- 3.27 adds the launcher to the workflow, and 17.184 adds the signed update manifest.
- Operating system code signing follows the code signing decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md: the project applies for SignPath Foundation's free signing for open-source projects, and until then ships unsigned builds with SHA-256 sums and the signed manifest.

**Deliverables**

- A release workflow that runs on a version tag:
  - It builds the panel program with its server component for Windows and Linux with the release presets, and runs the same checks the pull request job runs.
  - It packages the program through 17.183's pipeline and publishes each package to a GitHub release under the tag, with a SHA-256 beside each file.
  - 3.27 adds the launcher and 17.184 the signed manifest to the same workflow.
- Each package standalone:
  - It carries the program's own screens, the supervisor with the dashboard compiled in (17.179), the apps and the tools they run, so nothing is fetched at first run from anywhere but the user's own installation.
  - It carries the install manifest 17.183 writes, which 17.118 reads.
  - No package carries a client file, a type dump or extracted data, and the workflow checks each against its content list.
- A version read from the tag, reported by `--version` on the panel program, the supervisor and every app, and shown on the panel's about page. An operator can say which release they run, and the panel can tell them a newer one exists without checking on its own.
- Release notes generated from the merged milestones and merged contributor items since the previous tag, in the roadmap's own words, with a hand-written line at the top for what a player will notice
- A pre-release mark until the game reaches the milestone the maintainer names as the first playable one, so nobody mistakes a build for a finished game, and the README's front page linking the latest release beside the Discord and the Reddit
- Executables signed for the operating system through SignPath Foundation's free signing for open-source projects once it is granted, as the code signing decision settles. Until then the release page says the build is unsigned and how to check its SHA-256 and, once 17.184 adds it, the signed manifest.

**Acceptance**

- [ ] Pushing a tag produces a release carrying the panel program for Windows and Linux, each with a SHA-256, and a tag that fails a check produces no release (the workflow's own run on a test tag)
- [ ] The workflow fails a package that carries a file from a client install, a type dump or extracted data (the workflow run over a fixture package that carries one)
- [ ] A smoke job installs the downloaded Windows and Linux packages on fresh runners with Node, Python and every compiler removed from the path, starts the packaged supervisor and reaches its panel's overview
- [ ] `--version` on the panel program, the supervisor and each app, and the panel's about page, report the tag that built them (the smoke job)
- [ ] The release notes name every milestone and contributor item merged since the previous tag and nothing else (unit test over the generator with a fixture history)
- [ ] Dev-gated: the maintainer downloads a release on a machine that has never built Ambrose, installs it, reaches the overview and records the run. Needs a machine that has never built Ambrose

## 17.106 Error reports: source locations, grouping and a report file

**Goal:** Somebody running their own server opens one page, sees every error their apps logged with the source file and line it came from, and downloads a report file to send to the maintainer, who reads the exact build and line and fixes it without a screen share.

**Size:** L. **Depends on:** 17.04, 17.08, 17.14

Added on 2026-09-22 at the maintainer's direction: errors an operator meets should reach the maintainer as a file that names where in the code each one was raised, sent from the panel rather than pasted out of a log.

**Deliverables**

- Every log record carries the source file, line and function of the call that wrote it, and the message's format template, captured by the logging macros at compile time so a record costs no extra formatting; the file is the path inside the repository, never the build machine's own folder
- The live log stream (17.04) adds `source` and `template` to each record, as the rule that fields are only ever added allows, and the console and file lines keep the four columns 17.74 settled
- Each app groups the records it writes at error level and above by app, category, source location and template, keeping a count, the first and last time, the build revision and the last message with secrets masked by the rule the log stream uses, bounded in number with the least recently seen group dropped first, and answers them on `GET /api/errors`
- The supervisor (17.08) gathers every app's groups and its own, keeps them in its store (17.14) across app and supervisor restarts, and marks a group new again when it returns after the operator cleared it
- An Error reports page listing the groups by app, category, location and time, each opening its source file and line, template, count, first and last seen, and the log lines from before its latest occurrence, with a Create report action
- A report file in a versioned format holding the product version, commit and branch, the operating system, each app's revision, and the chosen groups with their locations, templates, counts and times; the rendered messages and the log lines around each occurrence are included only when the operator ticks them after seeing exactly what the file will hold
- Nothing from the client install, no secret, no account verifier and no player identity in a report unless the operator includes rendered text after that preview, proved by the same kind of response shaping test 17.84 uses; the file downloads from the page, which links to the repository's issue form for sending it, and nothing leaves the machine on its own
- Creating a report is audited with who and when

**Acceptance**

- [x] An error logged in a test build appears on the page with its repository-relative file, line and function, and the build revision it came from (SLogSourceTest.ARecordCarriesTheFileLineAndFunctionOfItsCall; AdminStatusTest.TheErrorsRouteReportsAGroupPerPlaceAnErrorWasRaised; Errors.browser.test.ts)
- [x] The same error logged three times is one group with a count of three, a different line is a different group, and a group survives a restart of the app and of the supervisor (LogErrorStoreTest.OneLineRaisedManyTimesIsOneGroupWithACount; LogErrorStoreTest.TheSameWordsFromDifferentPlacesAreDifferentGroups; PanelErrorsTest.ACountThatWentBackwardsIsAnAppThatRestarted; PanelErrorsTest.TheContextBeforeAnErrorSurvivesClosingAndReopeningThePanelStore)
- [x] A report built from groups of two apps names both apps' revisions and each group's location, and a known secret, a known account verifier and a known account name appear nowhere in its bytes while rendered text is left out (PanelTest.ErrorReportsPreviewPrivacyAndAuditExactlyTheSelectedGroups)
- [x] Ticking rendered messages shows them in the preview and puts exactly those lines in the file (Errors.browser.test.ts; PanelTest.ErrorReportsPreviewPrivacyAndAuditExactlyTheSelectedGroups, where the created report equals the preview with rendered text ticked)
- [x] The console and file log lines are byte for byte what they were before, and the stream's version-one fields are all still there (LogMessageTest.PrefixSegmentsFollowFlagsInFixedOrder; AppenderConsoleTest.RedirectedOutputKeepsTheFullDateAndTheUnpaddedCategory; SyncAndAsync/SLogFileTest.WritesServerLogInLogsDirWithPrefix/0 and /1; LogStreamServiceTest.ARecordCarriesWhereItWasWrittenAndWhatItWasWrittenFrom)
- [x] Creating a report writes an audit row (PanelTest.ErrorReportsPreviewPrivacyAndAuditExactlyTheSelectedGroups)

## 17.107 A value in a log line is a place you can go

**Goal:** Anywhere the panel shows a log line, the parts that name a thing are live. Hovering one says what it is and what is known about it right now, and clicking one opens the thing itself: the source file and line that wrote the record, the setting that holds the value, the app that binds the address, the file at the path, the commit at the revision. An operator meeting an error moves from the line to the cause instead of going to look for it.

**Size:** M. **Depends on:** 17.07, 17.76, 17.106

Added on 2026-09-22 at the maintainer's direction: a colored run should be a link carrying a hovercard, on every log surface the panel has, so an error or a warning is one click from wherever the issue is.

**Deliverables**

- One resolver shared by every log surface, turning a marked run and the record it sits in into what the run is, what its card should say and where a click goes, with a run it does not recognize keeping its color and staying plain text rather than becoming a link that leads nowhere
- A card opened by hover and by keyboard focus and closed by Escape, naming the run's class and what the panel already knows about that value now, which is the setting's value and where it was read, the app's state, the database's pending updates, without a request of its own
- A source location opening the code: the repository-relative file and line 17.106 puts on the record go to the repository at the revision the app was built from, and to the editor on the machine when the panel is open on that machine, with the operator's choice between them remembered
- A value opening the panel's own page for it: a setting to its option on the settings page, an app or a category to the log viewer filtered to it, a path to the file page inside 17.18's roots, a revision to its commit, an address to the app that binds it
- Every surface using it, which is the captured output of 17.08, the log viewer and console transcript of 17.07, the error reports page of 17.106 and every later page that draws a record, so a line offers the same links wherever it is read
- Nothing revealed by a link that the page would not reveal: no token in a link, an external link opened with no referrer, and a path outside the file roots left unlinked

**Acceptance**

- [ ] Hovering an address on a captured line opens a card naming it as an address and the app that binds it, the card opens on keyboard focus as well, and Escape closes it
- [ ] Clicking the source location of an error opens the repository at that file and line at the revision the app was built from
- [ ] With the panel open on the machine running it, the same click opens the editor at that file and line, and the choice between the two is remembered
- [ ] Clicking a setting's name opens the settings page at that option, and clicking a category opens the log viewer filtered to it
- [ ] A run the resolver does not recognize is drawn in its color and is not clickable, and a path outside the file roots is not a link
- [ ] The same line read on the captured output, the log viewer and the error reports page offers the same links


## 17.108 A panel nobody has to click past a warning to use

**Goal:** Somebody who installs Ambrose reaches their panel without a browser telling them it is unsafe, whichever of the three situations they are in, and without being taught what a certificate is. What they are asked for is the situation, not the cryptography.

**Size:** M. **Depends on:** 17.14, 17.181

Added on 2026-09-22 at the maintainer's direction, from running the panel. 17.14 gives the panel TLS and writes it a certificate that signed itself, which is correct and which every browser calls not secure. Every operator therefore meets a warning on their first visit, and the connections a page makes behind itself can be refused outright. Deciding what a certificate should be is settled; getting a trusted one into an operator's hands is not, and that is what this milestone is.

Changed on 2026-09-27: the trusting moves from 17.24's desktop app to the panel program of 17.181, so this milestone no longer waits for hosting. The program's own window pins a panel's certificate itself and needs no store entry; the store entry is for the operator's browsers.

**Deliverables**

- Loopback served over plain HTTP by default, because a browser already treats `http://127.0.0.1` as a secure context, so the commonest case, a panel on the machine it manages, has no warning and no certificate at all. TLS stays required the moment the bind leaves loopback, which is the rule 17.14 settled, and the option that chooses is one the operator reads as a situation rather than a protocol.
- A name the operator owns, answered by ACME:
  - The operator gives the hostname, and the panel obtains and renews its certificate with no further action.
  - It renews early enough to survive a failed attempt, and keeps serving the old certificate until the new one is in hand.
  - It says on the panel what the state of it is.
- No name and not loopback, which is a private network: the certificate that signed itself stays, and the operator is walked through trusting it once, with the fingerprint shown on the page, in the log and in the panel program, and the same fingerprint checked after
- The trusting is done by the panel program of 17.181, which is already on the operator's machine and can ask properly:
  - It names the store it will write to and asks once.
  - It writes only the panel's own certificate, to the current user rather than the machine, and can undo it.
  - It prints the command for anyone who would rather run it themselves.
- The panel saying which of the three it is on its settings page, with what to do next when the answer is the warning, so nobody has to find this out from a log line
- Nothing weakened to make this easy: no trust added without being asked, no key written where another user can read it, no certificate accepted because it merely matches a name, and a fingerprint checked before trust is granted rather than after

**Acceptance**

- [ ] A first start bound to loopback serves the panel with no certificate and no browser warning, and the same configuration bound beyond loopback refuses to start until TLS is configured (end-to-end run against a supervisor started with each bind)
- [ ] Given a hostname that resolves to the machine, the panel obtains a certificate, serves it, and renews it in a test that moves the clock, keeping the old one serving until the new one is in hand (integration test against a local ACME test server)
- [ ] A failed renewal keeps the old certificate serving, says so on the panel, and retries rather than falling back to one that signed itself (the same integration test with the server refusing the order)
- [ ] The panel program names the store and asks once, and after the operator agrees the browser reaches the panel with no warning; refusing leaves the store untouched (unit tests with the store behind a fake, and Dev-gated on the maintainer's Windows and Linux desktops for a real browser)
- [ ] The fingerprint on the page, in the log and in the panel program are the same string, and it is the certificate actually served (integration test reading each)
- [ ] Undoing the trust removes exactly the one certificate it added and nothing else (unit test with the store behind a fake)

## 17.109 Plugins: what one is, and the tab that installs it

**Goal:** A plugin is a named, versioned bundle that adds a tool to the panel or content to the game, installed, listed, enabled, disabled and removed from a Plugins tab. It is the content pack of 17.101 with a wider idea of what a pack may carry, on the same manifest, the same checksums and the same journal, so nothing about how it lands is new.

**Size:** M. **Depends on:** 17.101, 17.48, 17.18

Added on 2026-09-23 at the maintainer's direction: people should be able to contribute plugins that modify the game or add tools such as a quest editor, and an operator should install them from the panel once they have passed this project's checks.

**Deliverables**

- The format, extending 17.101's manifest rather than replacing it: name, version, kind, what it touches, checksums, licence, the Ambrose versions it says it works with, and the capabilities it asks for, each written for an operator to read rather than for a machine to parse
- What a plugin shows a person before they install it, carried in the bundle and not fetched from anywhere: an icon, up to a few screenshots, a one-line summary, a longer description of what it does, and a page saying how it is used, all in the repository beside the plugin so the tab works with no network at all
- Three kinds, named apart in the format because what each may do differs completely: a panel tool, which is a page and runs in the browser; content, which is data and dated database updates and is exactly a 17.101 pack; and a server module, which is source the operator builds, never a binary the panel loads
- Install, enable, disable and uninstall through the journal world edits already keep, so everything a plugin did is recorded and reversible, with disable stopping a plugin while leaving what it wrote in place and uninstall reversing it under 17.101's rules
- The Plugins tab: what is installed, its version, its kind, where it came from, what it touches, which capabilities were granted and when, with enable, disable, update and remove
- A plugin never carries a file from the game client, checked on install the way a pack is, and stated in the format
- Installing, enabling, granting to and removing a plugin are permissions of their own under 17.48, each audited with who, what and when

**Acceptance**

- [ ] A panel tool, a content plugin and a server module each install from a file, appear in the tab with their kind and version, and are refused when their manifest names an Ambrose version this build is not
- [ ] Disabling a plugin stops it without removing what it wrote, and enabling it again reinstalls nothing
- [ ] Uninstalling reverses exactly what installing did, and is refused, naming what depends, when something else depends on it
- [ ] A plugin carrying a file that came from a client install is refused, and so is one whose checksums do not match its bundle
- [ ] A server module plugin never loads into a running server: installing it stages source for the operator to build, and the tab says so rather than offering to run it
- [ ] An operator without the grant cannot install, enable or remove, and each of those actions writes an audit row naming the plugin
- [ ] A plugin with no icon, summary, description or usage page is refused by the format, and every image it carries is served from the panel rather than fetched from anywhere

## 17.110 A panel tool runs without being trusted

**Goal:** A tool somebody else wrote runs inside the panel without being able to do anything the operator did not grant it. This is the milestone that answers why a plugin API was refused here until now. The panel fronts a game database, so the answer cannot be that plugins are reviewed and therefore safe. It has to be that a plugin cannot reach what it was not handed, whether it is honest or not, and that a review that misses something costs an operator a broken tool rather than their database.

**Size:** L. **Depends on:** 17.109

**Deliverables**

- A panel tool runs in a frame of its own with no ambient authority: an origin of its own, no panel session and no cookies, a content security policy that refuses any origin its manifest did not declare, and no path to the admin API except the one below
- One bridge out, and only one: a typed message channel where every call names a capability the manifest asked for and the operator granted, checked in the panel and again at the server, so a tool that forges a message is refused by the server that receives it rather than by the page that sent it
- A capability list that is small, specific and readable, each entry a sentence an operator can judge, such as reading quests or writing quests. Nothing in it means call any admin route, and a capability amounting to that is not added
- Every call a plugin makes is audited as the plugin rather than as the operator, so the activity log says which tool did a thing and under whose grant it did it
- A capability can be withdrawn while the panel is running: the frame is told, later calls are refused, and a tool that handles refusal keeps working with less rather than breaking
- The rule that the server never runs code a plugin shipped, enforced rather than promised: nothing in the install path can load a library, and a server module is source the operator builds through the module discovery the build already has

**Acceptance**

- [ ] A tool granted nothing reaches no route, cannot read the panel session, and its frame holds no cookie belonging to the panel
- [ ] A tool granted a read capability is refused when it calls the matching write, at the server and not only in the panel
- [ ] A forged bridge message naming a capability the operator never granted is refused by the server and audited as a refusal
- [ ] Every accepted call appears in the activity log naming the plugin, the capability and the operator whose grant it used
- [ ] Withdrawing a capability from a running panel takes effect on the next call without a reload, and the tool is told rather than left to guess
- [ ] A plugin bundle carrying a shared library or an executable is refused on install, whatever its manifest says
- [ ] The frame's content security policy refuses a request to an origin the manifest did not declare, and the refusal is visible to the operator
- [ ] A tool that tries to send what it read to an origin it did not declare is stopped by the browser rather than by the review, proved with a tool written to do exactly that
- [ ] What a granted tool did is recoverable: its writes go through the journal 17.101 keeps, so an operator who finds a plugin misbehaved can see every change it made and undo them

## 17.111 Plugins ship with the panel, from this repository

**Goal:** A plugin that is good enough lives in this repository, and an operator gets it by updating the panel. There is no separate store to host, sign or keep alive: the release an operator already trusts carries the plugins, and the review that let a plugin in is the review this project already does on everything else.

**Size:** L. **Depends on:** 17.109, 17.110, 17.105

Settled on 2026-09-23 at the maintainer's direction, and it is the reason a plugin store is safe to have here at all. The objection to a plugin API was that it is a remote code execution surface on a panel that fronts a game database. Shipping plugins in the repository answers most of it before 17.110 answers the rest: a plugin arrives by the same signed release as the panel itself, from a pull request a human reviewed, so there is no second trust root, no index to sign, no service to pay for, and nothing for an attacker to take over by taking over a registry.

**Deliverables**

- Plugins live in the repository under a folder of their own, one directory per plugin with its manifest, and the contributor track carries them the way it carries milestones, so submitting one is a pull request and the review is the ordinary one
- The checks a plugin passes run in CI on every pull request and can be run locally before submitting: the manifest is well formed, every capability is declared and justified in words, no file came from a client install, no origin is contacted that the manifest did not declare, a licence is present, and nothing in the bundle is a library or an executable
- Every plugin is readable source. Minified, packed, obfuscated or generated-without-its-source files are refused, because a bundle nobody can read cannot be reviewed and a review is the main thing standing between an operator and a bad plugin
- Scans, from both sides and compared, aimed at malicious code, trojans and anything that tries to reach further than it said it would: an author submits the report of a public multi-engine scan together with the digest of exactly what they scanned, and CI runs the project's own scans over the bundle in the pull request. A scan whose digest does not match the bundle in the repository counts for nothing and is refused as though it were absent
- Every dependency vendored and inventoried, because a trojan in a plugin arrives through a library far more often than through the author's own hand: third-party code is committed in the bundle rather than fetched, listed in the manifest with its version and digest, and nothing is installed from a package index at build or run time. A bundle that fetches code when it runs is refused
- What the scans catch and what they do not, written where a contributor reads it: an engine built for binaries is strong against a known trojan and weak against a hand-written one in readable script, which will pass every engine clean. That weakness is the reason for the rest, not an argument against scanning. What actually stops a trojan here is that a bundle holds no binary, that its source and its dependencies can be read, that a human reads them, that the frame it runs in has no session and no route it was not granted, and that it cannot reach an origin it did not declare. A clean scan never substitutes for any of those, and the tab never shows a scan result to an operator as an assurance
- A release (17.105) carries the plugins that were in the repository when it was tagged, so updating the panel is what delivers new ones, and the tab says which release a plugin arrived in
- The tab has two sources and tells them apart: the plugins that shipped with this panel, which an operator installs and enables without fetching anything, and a plugin installed from a file by an operator who does not want to wait for a release
- The tab reads as a place worth browsing rather than a table of rows: each plugin a card with its icon, name, summary and kind, opening to its screenshots, its description, how it is used, its licence, its author, its version and the capabilities it will ask for, in the look doc/DESIGN.md sets and working the same on a phone
- Nothing is installed or enabled on the operator's behalf. A new plugin arriving in an update appears in the tab as available, never as running, and updating the panel never changes which plugins are enabled
- A plugin withdrawn from the repository stops shipping in later releases and is marked withdrawn in the tab with its reason, and an operator running it keeps it until they remove it

**Acceptance**

- [ ] A plugin added to the repository appears in the tab of a panel built from that commit, marked as shipped with it and not enabled
- [ ] Updating a panel to a release with new plugins leaves every enabled plugin exactly as it was, and adds the new ones as available only
- [ ] The submission checks refuse, by name, a plugin with an undeclared capability, one carrying a client-derived file, one with no licence, one carrying an executable or a library, and one whose script is minified or obfuscated
- [ ] A submission whose author scan digest does not match the bundle is refused, and so is one with no scan at all
- [ ] A bundle that fetches code when it is built or run is refused, and one whose manifest omits a dependency it carries is refused naming the file
- [ ] The tab shows a plugin's icon, screenshots, summary, description and usage page, and its capabilities in the operator's words, before anything is installed
- [ ] An author can run those checks locally and get the same answer CI gives
- [ ] A plugin installed from a file is told apart in the tab from one that shipped, and survives a panel update
- [ ] A withdrawn plugin shows as withdrawn with its reason, and nothing is removed from the operator's machine

## 17.112 Tomes: templates for realms, apps and companion services

**Goal:** An operator adds a realm, another game server or a program the community runs beside the game by picking a tome, answering a few typed questions and reviewing what it will create, instead of writing configuration by hand. A tome is one readable file that can be shared, and nothing in it is hidden behind inheritance, a second placeholder language or a shell script.

**Size:** L. **Depends on:** 17.27, 17.28, 17.29, 17.30, 17.99

Settled on 2026-09-27 at the maintainer's direction: Ambrose's panel is its own product, and this is its own answer to the need other hosting panels meet with server templates, written to be simpler to read and safer to apply. A tome describes what should exist; 17.99's installation file describes what does exist. Applying a tome produces a fragment of that installation file and goes through 17.99's difference and apply, so there is one writer and one review for both.

**Deliverables**

- The tome format: TOML 1.0 through toml++, the format 17.99 already settles, with a `[tome]` table naming it, its version, a one-line summary, its author, its licence and the lowest Ambrose release it needs, then the questions it asks and the things it creates. Its schema is generated from the same types the settings schema uses and published beside it, so an editor can check a tome as it is written
- Questions (`[[ask]]`) are typed, never rule strings: a bool, an integer or unsigned integer with bounds, a number, a line of text with a byte limit, a choice from a list, a port name, a file root path or a secret. A question may instead name a declared setting (`setting = "Rate.Drop.Item"`) and then takes that setting's type, bounds, unit, description and default from the registry, so nothing is typed twice and a tome can never offer a value the setting would refuse. A secret answer is sealed into the keyring at once and the tome and every export hold only its reference
- Ambrose's own apps are created by kind (`kind = "gameserver"`, `"loginserver"` or `"patchserver"`), and the tome says nothing about how to start, check or stop them: the supervisor already knows, from 17.27, 17.28, each app's health and start steps and its admin API shutdown. The tome gives only what differs, such as the realm an app belongs to, settings values or a 17.13 preset, and the port names it needs
- Companion services (`kind = "service"`) for programs a community runs beside the game, such as a bot or a website: the program and its arguments as a list, never one shell string; its working folder inside a file root; its environment; how the supervisor knows it is ready, an HTTP answer, an open TCP port or, as a last resort the page marks as fragile, a line in its output; and how it is stopped, an HTTP request, a signal or a line on its input, with the 17.27 stop timeout and kill after it. The supervisor gains the HTTP and TCP ready checks it needs for these, and they are the only new supervision it gains
- One placeholder form only, `{{ask.name}}` for an answer and `{{port.name}}` for an allocated port, usable in a companion's arguments, environment and in a configuration file the tome renders for it. Every placeholder is checked against the tome's own questions and ports when the tome is read, and a rendered file is shown as a difference before it is written. There is no inheritance between tomes and no reference to another tome's values: a tome that wants another's apps lists them itself
- Ports are asked for by name and role (`game`, `admin`, `http`) from 17.29's pool, never as numbers, and databases by name on a 17.30 host, created with their updates applied by dbimport
- Install steps are typed and few, never a script: every Ambrose app runs the build already installed, and putting one on another machine stays with 17.22 and 17.42; the steps create the databases, place a file downloaded from a URL whose SHA-256 the tome pins into the companion's folder, and let 3.22's automatic setup extract client data as it already does. A tome carrying anything else is refused on reading, naming it; running operator code belongs to 17.147's sandbox, not here
- A Tomes page: the library of tomes that shipped with this panel and tomes imported from a file, each as a card with its summary, what it creates and what it will ask; creating from one opens a form generated from its questions, checked as it is filled, then a review listing every app, port, database, setting and file it would create, with nothing written until it is applied. Every creation is audited with the tome's name, version and digest
- Export as a tome: any realm, app or companion service already running can be written out as a tome, with installation-specific values such as secrets, paths and hostnames turned into questions rather than carried, so a working setup can be shared without leaking it
- Every app made from a tome remembers the tome, version and answers that made it. When a newer version of that tome arrives, the page shows a three-way difference between the old tome, the new tome and what the operator has changed since, and an operator's own change is never overwritten without asking which to keep
- Shipped tomes live in the repository and arrive with a release, the way 17.111 ships plugins: at least "A realm" (a login server, a patch server and one game server with their databases and ports) and "Another game server for a realm". Imported tomes are kept in the panel store with their text and digest, not as loose files
- doc/TOMES.md written for someone who has never hosted a server: what a tome is, a complete short example, every field, and the reasons it has no scripts, no inheritance and one placeholder form

**Acceptance**

- [ ] Creating from the shipped "Another game server for a realm" tome on a running installation asks only for its name and realm, reviews the app, its ports and its database before writing anything, and the new game server reaches ready (end-to-end run against a real supervisor)
- [ ] A question naming `Rate.Drop.Item` shows the setting's own bounds, unit and description, and an answer out of bounds is refused naming the bounds (unit test)
- [ ] A tome carrying a shell script, a download without a pinned digest, a placeholder that names no question or port, an inheritance key, or a field the running build does not know is refused on reading, naming each (unit test)
- [ ] A companion service with an HTTP ready check shows as starting until the check answers and is marked failed with the reason when it never answers within its start timeout, and its stop sends the declared request, then kills it after the stop timeout (unit test with a stub program)
- [ ] Exporting a running realm as a tome and creating from it on a clean installation gives the same apps, port roles and settings, and no secret value appears in the exported file (unit test and end-to-end run)
- [ ] Upgrading a tome whose new version changes a setting the operator also changed shows both values and keeps the operator's until they choose (browser test)
- [ ] An app made from a tome shows the tome's name and version on its page, and the audit record of its creation names the tome's digest (browser test)

## 17.113 Hang detection and watchdogs at every layer

**Goal:** An app that is still running but no longer doing anything is noticed, recorded and restarted like a crash, and a frozen supervisor or a machine gone silent is noticed by something outside it.

**Size:** L. **Depends on:** 17.22, 17.23, 17.24, 17.27, 17.60, 17.67, 17.83

**Deliverables**

- A progress counter per critical loop in every app, the world tick, each network thread, each database worker and the admin API's own loop, advanced once per turn at the cost of one relaxed atomic increment and published on `GET /api/health` and in the metrics registry; a loop with nothing to do still turns on its wait timeout, so an empty realm or an idle queue never reads as stuck
- A stall limit per loop, a live setting, read by the supervisor on every health poll: a counter that has not moved for its limit while the process lives and its health endpoint still answers puts the app in a new `hung` state in 17.27's model, and the app card names the loop and how long it has been still. 4.03's realm heartbeat already drops a stalled realm from the realm list, but nothing restarts or records it; this does
- No hang declared while an app is starting, stopping or in a protected state, or across a gap in the supervisor's own polling, so a restore's drain, a long save at shutdown or a machine waking from sleep is never taken for a deadlock
- A dump of every thread of the hung process before it is ended, through the operating system's own writer from outside the process on Windows and through 17.83's writer, woken by a signal, on Linux, with a time limit after which the process is ended anyway and the record says no dump was written
- The hung app restarted through the normal 17.27 restart operation with 17.60's backoff and crash loop limits, and a crash record classified as hung, a new 17.60 class, naming the loop, its last counter value and the seconds it was still; 17.83 groups it by the stuck thread's top frames, so the same deadlock is one group with a count
- A watchdog inside the supervisor over its own loops, the event loop, the relay, the sampler and the schedule engine, which ends the supervisor with a failure exit when one of them stops; the supervisor started after it adopts the apps still running, as 17.08 already does, so its own recovery never drops a player
- The operating system watching the supervisor: the systemd unit `--install-service` writes becomes `Type=notify` with `WatchdogSec`, sending `READY=1` once ready and `WATCHDOG=1` only while the supervisor's loops move, where today it writes `Type=simple` with `Restart=on-failure`, which restarts a supervisor that exits and not one that freezes; the Windows service gains failure actions that restart it after a failure exit, where today it registers none; in Docker the same failure exit lets the Compose restart policy bring the container back, and the 17.24 desktop app starts again a supervisor it launched that exits this way
- An opt-in outbound heartbeat, off by default: a URL the operator gives, such as a push monitor on their own Uptime Kuma or Healthchecks, called on an interval through the outbound HTTP client only while the supervisor and every app meant to run are healthy, so a machine that loses power, sleeps or freezes is noticed by what stops arriving; with nodes, a second node can be the witness instead, sending one notice through the webhook or mail channel chosen for it when the panel's link has been silent past a limit
- The hung condition, a supervisor restarted by its watchdog and a witness's missing heartbeat published for 17.67's alert rules, and every hang, dump and watchdog restart audited

**Acceptance**

- [ ] A test app whose world loop blocks on a lock while its health endpoint keeps answering is marked hung naming that loop inside its stall limit, a dump holding every thread is written, the app restarts through a 17.27 operation, and its crash record is classified hung
- [ ] An app left idle with no players and an empty database queue for several stall limits is never marked hung
- [ ] The same deadlock hit three times is one crash group with a count of three, and a hang in a different loop is a different group
- [ ] A loop that stalls while its app holds the restoring state, or while it is stopping, is neither dumped nor restarted
- [ ] Stalling the supervisor's own event loop in a test build makes its watchdog end it with the failure exit, and the supervisor started after it adopts the running apps without restarting them
- [ ] With the outbound heartbeat set to a local test endpoint, pings arrive while everything is healthy and stop while an app is hung, and with the setting off no request leaves the machine
- [ ] Dev-gated: a supervisor frozen by a test command is restarted by the operating system under the systemd unit and under the Windows service, while its apps keep serving players. Needs a real service install on each system, which CI cannot make, so it is run by hand and recorded

## 17.114 Host awareness: sleep, OS updates, shutdown and the clock

**Goal:** Ambrose on a home PC keeps the machine awake while it is needed, says plainly when the operating system slept or restarted under it, stops cleanly when the machine shuts down, and warns when a clock is wrong.

**Size:** L. **Depends on:** 17.15, 17.22, 17.23, 17.24, 17.60, 17.78, 17.82, 17.94, 17.98

**Deliverables**

- A named keep-awake request held while players are in the world or a backup, restore, update or move runs, and released when none is: a power request with a reason string on Windows, which `powercfg /requests` lists, and a logind sleep inhibitor lock naming why on Linux. It never keeps the display on, is on by default only when the 17.24 desktop app starts the stack and off by default for a service install, is a live setting in both, and is never taken in Docker
- Sleep and resume read from the operating system, its power notifications on Windows and logind's `PrepareForSleep` on Linux, and recorded as host events, so the dropped sessions and the gap in every graph after a resume are explained rather than read as crashes; 17.15 already re-arms schedules after a wall-clock jump, and a resume counts as one
- The end of the previous boot read at every supervisor start and classified as a clean restart, a restart for operating system updates or an unexpected power loss, from Windows events Kernel-Power 41, EventLog 6008 and User32 1074, which names the process that asked for the restart, and from the journal's record of the previous boot on Linux; each appears as a 17.78 graph marker, a 17.94 incident timeline entry and a sentence such as "The PC restarted for updates at 03:12"
- Pending restarts as 17.82 findings: the reboot-required flags of Windows Update and component servicing, pending file renames, `/run/reboot-required`, and apps still mapping libraries an upgrade deleted on Linux, with a finding when the operating system's own restart window, Windows' active hours among it, falls outside the quietest hours 17.98 names
- A graceful stop when the operating system shuts down: the Windows service accepts preshutdown with a timeout long enough for 17.08's stop sequence, the 17.24 desktop app holds a desktop shutdown with a reason the operating system shows while the stop runs, and on Linux a logind shutdown delay inhibitor does the same, so players are warned and characters saved; every exit that follows is recorded as a host shutdown, a new 17.60 class, never as a crash. Today the service accepts only a plain shutdown, and a desktop run outside the service has no shutdown hook at all
- A clock section per node showing whether the operating system's time sync is working and when it last succeeded, and each node's offset from the panel measured over the 17.22 heartbeat, with findings when an offset or a stopped sync threatens what assumes a correct clock: two-factor codes, certificate renewal, S3 request signing, which a store refuses beyond 15 minutes of skew, schedules, and the order of audit rows across nodes
- An opt-in reboot host task for 17.15 schedules, off by default and owner-only under `host.reboot`, that counts down to players, stops the stack cleanly, asks the operating system to restart with a reason it records, and completes its run after the boot once every app meant to run is healthy, or fails naming the app that is not; it is not offered in Docker, where a container cannot restart its host
- A host page per node gathering these events, and the unexpected power loss, restart pending inside busy hours, time sync stopped and clock offset conditions published for 17.67's alert rules

**Acceptance**

- [ ] With the power interface replaced by a test double, a keep-awake request is held while a player is in the world or a backup runs, released when both end, and never taken when the supervisor runs in its Docker image
- [ ] Recorded event fixtures of a power loss and of an update restart are classified apart, and each appears as a graph marker and an incident timeline entry with its time and sentence
- [ ] A simulated operating system shutdown during a test run saves every character in the world before the apps exit, and each exit is recorded as a host shutdown rather than a crash
- [ ] A reboot-required flag produces its finding, and a restart window that overlaps the busiest hours in 17.98's grid produces a finding naming both
- [ ] A node whose clock is set 20 minutes off the panel's shows the offset and a finding naming what that offset breaks, two-factor codes among them, and an offset of one second shows none
- [ ] A user without `host.reboot` cannot create the reboot task, and the task is not offered in Docker
- [ ] Dev-gated: on a Windows desktop with players in the world, `powercfg /requests` lists Ambrose's request with its reason and the machine stays awake past its sleep timeout, and the reboot task restarts the machine and completes its run once the apps are healthy. Needs a real desktop that may sleep and restart, which CI is not

## 17.115 Runbooks on alerts, findings and crash groups

**Goal:** Every alert, problem, finding and crash group opens a short procedure that says what it means, how to confirm it and what to do next, with the actions as buttons, so a newer game master can handle a crash loop at three in the morning without paging the owner.

**Size:** L. **Depends on:** 17.53, 17.67, 17.82, 17.83, 17.86, 17.88

**Deliverables**

- A runbook format: a Markdown document with a title, what the condition means, how to confirm it and numbered steps, where a step is text, an action, a live figure or a link to a page opened on the right subject and time window
- Actions that are the ones pages already register for 17.88's palette, such as opening the graphs for the alert's window, restarting gameserver or muting a rule for 30 minutes, filled in with the subject and window of whatever opened the runbook, confirmed exactly as on their own page and checked at the server; a step never holds a shell command, an action the reader may not run shows disabled naming the permission, and one whose milestone has not landed shows as unavailable naming it
- Live-figure blocks naming a series or a status field, rendered with its current value under the same permission filtering as the page it comes from
- A runbook opened from every 17.67 rule, every problem code 17.03 publishes and 17.82 turns into a finding, and every 17.83 crash group by its class or fingerprint, reached from the alert, the finding, the crash page and every notification
- Built-in runbooks kept in the repository beside the rule or check that raises them, shipped with the release and read-only in the panel, and a coverage test that fails when a rule, a problem code or a crash class lands without one
- Operators' own runbooks, and local notes on built-in ones, kept in the panel store with every version, who and when, a difference between versions and a restore, edited in 17.53's editor under `runbooks.edit`; a local note survives an update that replaces the built-in runbook beneath it
- A step record: while an alert is open, each step an operator opens and each action run from it is written to that alert's history with who, when and the result, and audited, so the next person sees what was already tried
- Markdown rendered with no raw HTML, no script and no image from another host, under the panel's content security policy, because a runbook is text one operator wrote and others open
- A starter set written to the format: a crash loop, a disk nearly full, a saturated database pool, restoring from a backup, and rotating a leaked database credential or API key

**Acceptance**

- [ ] The coverage test fails when an alert rule, a problem code or a crash class is added in a fixture without a runbook, and passes for every built-in one
- [ ] A crash-loop alert opens its runbook with the app and window filled in, and its restart step asks for the same confirmation the app's page asks for and is refused at the server for a user without `power.restart`
- [ ] Running an action from a runbook while its alert is open writes the step, who ran it and its result to the alert's history and to the audit log
- [ ] An operator's runbook saved twice keeps both versions with who and when, and the earlier one restores; a local note on a built-in runbook survives replacing the built-in with a newer copy
- [ ] A runbook holding raw HTML, a script or an image from another host renders none of them, proved by a sanitizer test and the page's content security policy
- [ ] A live-figure block shows the value its own page shows at the same moment, and nothing to a viewer without that figure's permission
- [ ] Every email and webhook notification carries its runbook's link, proved by a shaping test

## 17.116 Restore drills and update rehearsals

**Goal:** Each backup carries proof that it restores, boots and lets a player sign in, and a waiting update says how long its schema changes take on real data before anyone schedules it.

**Size:** L. **Depends on:** 17.16, 17.17, 17.29, 17.43, 17.51, 17.67, 17.81, 17.82

**Deliverables**

- A drill, on a 17.15 schedule or from a button under `backups.drill`: the newest verified backup or a chosen one, fetched back from its 17.43 target when no local copy is kept, verified and loaded by 17.51's staging loader into scratch schemas named `drill_` with the run's id, on the same database host, refused when a name collides and never touching a live schema
- The 2.06 updater run on the scratch schemas, then a throwaway loginserver and gameserver from the running build started on loopback-only ports from the 17.29 pool, with the copy's config but mail, webhooks, S3, the public status page and registration off, and nothing published to the live realm list
- The proof: the drill waits for health, runs the 17.81 probe's sign-in and world entry against the copy, creating the probe account on the copy when the backup predates it, then stops the throwaway servers and drops every scratch schema and folder
- Each phase timed, fetch, load, update, boot and sign-in, and the result kept on the backup's record as a badge such as "restore proven on 2026-09-27 in 6 m 12 s", or as the step that failed with its error; a drill whose sign-in cannot run because the client driver or the probe account is missing records sign-in as unknown, and the badge never says proven
- No successful drill inside a window, a live setting, as a 17.82 finding and a 17.67 condition from the same figure, and a failed drill raising its own alert naming the backup and the step
- An update rehearsal: when 17.17 has a build waiting, the same copy first runs that build's `--check` and its schema updates, and the update page shows the time each update file took, which `ALTER TABLE` statements had to copy the whole table after `ALGORITHM=INSTANT` and then `ALGORITHM=INPLACE, LOCK=NONE` were refused, the rows touched and the disk growth, and so the downtime the real update should take
- Drills throttled like backups: one at a time per database host, skipped with the reason when free space is below the copy's size plus the reservation, refused while the live installation is restoring, updating or moving, and cancelled and cleaned up when one of those starts
- An interrupted drill recorded as interrupted at the next supervisor start, with its scratch schemas, folders and ports removed then, so nothing named `drill_` outlives its run
- Drill results on the backups page and on each backup's record, and every drill audited with who started it or which schedule did

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): a drill of a verified backup loads it into `drill_` schemas, starts a loginserver and gameserver on pool ports, reaches health, and leaves no drill schema, folder or listening port behind, and the backup's record carries each phase's time
- [ ] Env-gated (AMBROSE_TEST_DB): every live table's row count and checksum are the same before and after a drill
- [ ] A drill whose sign-in cannot run because the client driver is missing records sign-in as unknown, and the backup is not badged as proven
- [ ] Killing the supervisor during a drill leaves it recorded as interrupted at the next start, which removes its scratch schemas
- [ ] With no successful drill inside the window the finding and its alert fire from the same figure, and a successful drill clears both
- [ ] Env-gated (AMBROSE_TEST_DB): rehearsing a waiting build lists each update file's time and names an `ALTER TABLE` on a seeded table that needed a full copy, and the live database's applied update list is unchanged
- [ ] A drill requested while the live installation is restoring is refused naming the restore, and a running drill is cancelled and cleaned up when an update starts

## 17.117 Panel store durability: checks, snapshots and freeing space

**Goal:** The one file holding panel users, two-factor secrets, grants, the audit chain, schedules and the backup catalog survives a power cut, and a filling disk frees space by a stated rule before the panel loses the controls needed to fix it.

**Size:** M. **Depends on:** 17.14, 17.16, 17.18, 17.19, 17.43, 17.72, 17.80

**Deliverables**

- Checks of the panel store, the 17.80 search index and the 17.19 graph history, a quick check at every supervisor start and a full integrity check weekly, each result a problem record in the status API; `PanelStore` opens the store in WAL mode with `synchronous = NORMAL`, and nothing checks it today
- Online snapshots of the store every few minutes, a live setting, through SQLite's online backup interface while writes continue, into a folder on another volume when one exists and another folder when not, each checked before it counts and kept by number, and optionally shipped sealed by 17.72 to the 17.43 target; the keyring stays a separate file, as Panel operations in doc/ARCHITECTURE.md settles, so a snapshot alone never opens its secrets
- Recovery at start: a store that fails its check or will not open is moved aside with its time in its name, the newest good snapshot restored and every panel session ended, and the panel opens with a banner naming the snapshot's time, the minutes lost and that any revocation made inside them must be repeated, kept until an owner acknowledges it; the search index and graph history are rebuilt or started empty rather than holding start back. 17.16's panel component stays what an operator restores on purpose, and this is only the store recovering itself
- Space freed before any writer refuses: when a volume nears its 17.18 reserve, the supervisor frees space in a stated order, expired trash, file versions beyond their retention floor, crash dumps beyond their count, the oldest days of the search index, then local copies of backups already verified off the machine, never a pinned backup or the only copy; each class is registered by the milestone that owns it and one whose milestone has not landed is skipped, and each pass stops when headroom returns and writes one audit row with the bytes freed per class
- A small reservation on the store's volume held back for the store's own security writes and the audit rows they carry, so disabling a user, revoking a key or changing a grant still commits when everything else refuses; 17.14's rule that a change whose audit row cannot be written is refused still holds for every other write
- A durability setting for ordinary writes, `NORMAL` or `FULL`, shown with its cost measured on this volume before it is saved, while writes to users, two-factor secrets, grants, keys and sessions always run with `synchronous = FULL`

**Acceptance**

- [ ] A store with bytes overwritten inside a page fails the start check, is moved aside and replaced by the newest snapshot, every session is ended, and the banner names the snapshot's time
- [ ] A test that cuts the store off at a random write, discarding everything after its last sync the way a power cut does, leaves a store that opens and passes the integrity check in every one of a hundred runs, by itself or through the recovery
- [ ] A snapshot taken during a stream of writes passes its own check, and lands on the other volume when one is configured
- [ ] With a volume filled to its reserve, the freeing pass removes classes in the stated order and stops when headroom returns, a settings change then succeeds with its audit row, and a local backup with no verified off-machine copy is untouched
- [ ] With the volume full, disabling a panel user still commits with its audit row from the held reservation
- [ ] Writes to users, grants and keys run with `synchronous = FULL` whatever the setting says, proved by a test reading the pragma inside those transactions, and the setting shows its measured cost before it is saved

## 17.118 Drift watch: config, installed files and schema

**Goal:** One page lists everything that no longer matches what runs or what was installed, so an edit made weeks ago is found before the restart that exposes it.

**Size:** L. **Depends on:** 17.08, 17.12, 17.17, 17.22, 17.67, 17.99, 17.105, 2.06

**Deliverables**

- A drift page listing each mismatch by kind with when it was first seen, what it affects, its fix and an alert condition, so the page and 17.67's rules cannot disagree
- Config drift: a `.conf` or `conf.d` file changed outside the panel since the app loaded it, shown key by key as what the next reload or restart would change, read through 17.12's schema and layers, with buttons to reload now when every changed key is live or to put back the text the app loaded, which the supervisor keeps for this
- Installed file drift: every file of the running build checked against an install manifest holding each file's SHA-256, which the 17.105 release workflow writes into every package and 17.17 records at install for a build from source, listing each file changed, missing or added; on Windows a missing file is shown beside the antivirus detection event that names it when one exists, since quarantine is the usual cause, and the fix is reinstalling the build through 17.17
- Schema drift: the 2.06 updater records a fingerprint of each database's schema after every update it applies, tables, columns with type, nullability and default, indexes, keys and triggers, and the watch compares the live schema with it and names each table, column or index changed by hand, with the pending update file that would fail on it; 17.08's database page already marks an applied update file that changed, and this looks at the schema itself
- Node drift: settings that differ between nodes serving the same role, such as the gameservers of one realm, ignoring the host-specific keys a restore also keeps, such as bind addresses, `ClientDir`, TLS paths and admin tokens
- Watches through `ReadDirectoryChangesW` on Windows and inotify on Linux over the config and install folders, with a periodic full rescan, a live setting, because both can drop events, and a cap on hashing per scan
- Accepting a finding with a reason under a permission of its own, which hides it until the thing changes again, and every acceptance, revert and reload audited
- The line with 17.99 kept: its difference against a file the operator wrote stays on demand there, and a scheduled comparison with a Git-synced installation file is 17.148's; this watch compares only against what was loaded, installed and recorded

**Acceptance**

- [ ] Editing a gameserver `.conf` value on disk by hand shows on the drift page inside the watch's delay with the key's current and next values, and reloading clears it
- [ ] A change made while the watcher is stopped in a test is still found by the next rescan
- [ ] Deleting one file of the installed build and changing another lists both with their expected and actual hashes, and a clean build lists nothing
- [ ] Env-gated (AMBROSE_TEST_DB): a column added by hand to a table in the characters database is named against the recorded fingerprint, and applying a dated update file records a new fingerprint with no drift
- [ ] Two gameservers of one realm with different values of one rate setting are listed, and different bind addresses are not
- [ ] Putting back the loaded text of a hand-edited `.conf` file restores it byte for byte and writes an audit row naming the file
- [ ] A finding raises its alert once, and accepting it with a reason hides it until the file changes again

## 17.119 Drive and host health: SMART, latency, pressure and limits

**Goal:** A failing drive, a slow volume, memory pressure, stolen CPU on a VPS or a descriptor leak shows on the panel before it takes the game database or the tick with it.

**Size:** L. **Depends on:** 17.19, 17.22, 17.67, 17.82

**Deliverables**

- A host page per node listing each physical drive with its model, health, temperature, wear, power-on hours, reallocated and pending sectors and the operating system's own disk error events, and which Ambrose stores sit on it: each local database's data folder, the backups, the panel store and the logs
- SMART data from smartmontools' `smartctl --json` when the operator installs it, found on the path or named in a setting and run with a time limit, and otherwise what the operating system gives without it, the storage reliability counters on Windows and the NVMe health figures on Linux; a figure no source can give, or one the service account may not read, reads unknown with the reason, never healthy
- Read and write latency and queue depth per volume, from the physical disk counters on Windows and `diskstats` on Linux
- Memory pressure per node, the pressure stall figures on Linux, and commit charge against its limit with hard faults a second on Windows
- CPU steal time on a virtual machine, with a note on the tick graph when steal rises with tick time, since a slow tick on a cheap VPS is often the neighbour rather than Ambrose
- Each app's open descriptors against its limit on Linux and its handles on Windows, with the growth rate since it started, so a leak shows before the limit rather than when a socket fails to open
- Every figure a 17.82 finding and a 17.67 condition: a drive failing or its pending sectors rising, a temperature, a latency or a pressure held above its threshold, steal held high, and descriptors near their limit
- The rows added to 17.19's sampler and carried in 17.22's heartbeat rather than a second sampler, with their cost reported by 17.19's benchmark; the finding for backups kept on the database's own disk belongs to 17.124's copy targets, so it is not built twice

**Acceptance**

- [ ] With smartctl absent, every SMART figure reads unknown with its reason and none reads healthy
- [ ] Given recorded `smartctl --json` output whose pending sector count rises, the drive shows as failing, the finding names the stores on it, and its alert fires once
- [ ] A test app leaking descriptors shows its growth rate and raises the near-limit finding before the limit is reached
- [ ] Pressure and steal figures parsed from recorded `/proc` files equal the values in them
- [ ] 17.19's benchmark reports the sampler's cost with the new rows, and a round of twenty apps stays inside its budget
- [ ] A viewer without `nodes.read` sees none of a node's host figures
- [ ] Dev-gated: on a machine with smartmontools installed, each physical drive shows the health, temperature and wear smartctl itself reports. Needs real drives with SMART, which hosted runners do not expose

## 17.120 Release health, bake window and canary realm

**Goal:** An update that starts but behaves worse than the build before it is caught by comparison and rolled back, and an operator with several realms can try a build on one of them first.

**Size:** M. **Depends on:** 17.17, 17.19, 17.44, 17.52, 17.60, 17.81, 17.83, 17.106

**Deliverables**

- A bake window after 17.17's health wait passes, its length a live setting, comparing the new build with the previous one over the same hours: crashes an hour, crash groups and error groups first seen on this build from 17.83 and 17.106, tick p99, sign-in success, the memory slope and the 17.81 probe's step times
- The baseline taken from 17.19's history and from the build each crash, error group and probe run already records, and a baseline too short to judge said so rather than passing or failing the bake
- A threshold per figure, a live setting, and a regression shown with both values and the size of the change, offering a rollback through 17.17's own path while 17.52 still pins the pre-update backup; rolling back by itself on a regression is opt-in and off by default, and the bake window can never be set longer than that pin
- A canary realm: with several realms, an update can go to one chosen realm first through 17.44's rolling restart, the rest following once its bake passes; it is offered only for a build whose release declares every schema update additive, since realms share the login and characters databases and the old build must keep running beside them, and the update page says why when it is not offered
- The bake's result recorded on the update run as passed, regressed with its figures, or rolled back, shown in the update history, audited, and published for 17.67's alert rules

**Acceptance**

- [ ] A test build that crashes twice an hour during the bake, against a baseline with no crashes, is marked regressed with both figures, and a rollback is offered while the pre-update backup is still pinned
- [ ] With the automatic rollback on, the same regression rolls back through 17.17's path and the run records why, and with it off nothing changes until the operator acts
- [ ] An error group first seen on the new build is listed as new, and one also seen on the previous build is not
- [ ] A baseline with too little history says so, and the bake neither passes nor fails
- [ ] A canary update of one realm leaves the other realm on the previous build until the bake passes, and a build with a schema update not declared additive is not offered as a canary, with the reason shown
- [ ] A bake window longer than the pre-update backup's pin is refused naming the pin

## 17.121 Request tracing and the service map

**Goal:** A slow sign-in, world entry or panel action opens as one timed waterfall across every process it crossed, and a live map shows what depends on what.

**Size:** L. **Depends on:** 17.04, 17.49, 17.80, 17.81, 17.90, 17.91, 17.92

**Deliverables**

- Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md: spans in the OpenTelemetry data model, W3C Trace Context's `traceparent` header on every HTTP hop the panel and apps make, the trace id carried on the login key row across the loginserver's hand-off to the gameserver, and no tracing library linked, as the Tick profiles row under Panel operations in doc/ARCHITECTURE.md settled on no profiler library
- Spans in the apps: each loginserver handler, each database statement with its call site, emitted by 17.90's timing layer rather than a second timer, the hand-off, the gameserver's world entry and object streaming, and in the supervisor each panel request and its relay into an app, each span with its parent, start, duration, status and a small set of attributes where a player appears only as an id
- Tail sampling in the supervisor: a trace's spans held for a bounded time and kept when it errored or ran slower than its route's threshold, and otherwise kept at a small random rate, with the store bounded by days and bytes through the sweep 17.80 uses and under the space guard
- A waterfall view of one trace as a timed tree across processes, opened from a log line's request id, a slow statement on 17.90's page, a probe run and 17.92's player inspector, and listing the log lines its spans wrote
- The trace id added to each record on the live log stream, as the rule that fields are only ever added allows, so 17.80's search finds every line of a trace and the correlation id 17.26 gives an error leads to it
- A service map of the apps, the databases and the outbound dependencies, mail, S3, webhooks, certificate issuance and update channels, with each edge's rate, error share and latency, counted from every span before sampling so the rates are true
- An opt-in exporter, off by default, sending kept spans over OTLP through the outbound HTTP client to the operator's own collector, such as Jaeger, Grafana Tempo or SigNoz, with a queue and backoff; with it off nothing leaves the machine
- A span site costing one relaxed flag check while tracing is off, and the world tick itself left to 17.91's breakdown rather than traced per tick

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): a sign-in on a test installation produces one trace from the loginserver's handler through its statements and the hand-off to the gameserver's world entry, with every parent right and every child inside its parent's time
- [ ] With a seeded sampler, a request slower than its threshold and one that errored are both kept, and fast ones are kept at the configured rate
- [ ] A log line's request id opens the trace it belongs to, and the waterfall lists that line
- [ ] The map's rate between gameserver and its database over a window equals the statement count 17.90 reports for the same window
- [ ] With the exporter pointed at a local OTLP test receiver the kept spans arrive, and with it off no request leaves the machine
- [ ] A span site costs nothing measurable with tracing off, proved by a benchmark in the test suite
- [ ] A viewer without the account permission sees a span's timing and not the account it names

## 17.122 Service level objectives and error budgets

**Goal:** An operator sets a target such as 99.5 percent of sign-ins succeeding over 28 days, sees how much room is left, and is paged when players are being hurt at a rate that matters rather than for one slow tick.

**Size:** M. **Depends on:** 17.17, 17.19, 17.44, 17.67, 17.81, 17.86, 17.94, 17.96

**Deliverables**

- Objectives over series the server already keeps: sign-in success counting only failures the server caused, so a wrong password never counts; world entries under a time; ticks inside their budget; and the 17.81 probe's availability, each with a target, a rolling window and its counting rule written out in words
- An objectives page with each one's current level, the budget left and a burn-down over the window, with planned maintenance from 17.96's sources left out of the count and drawn as left out; 17.94's uptime figures stay as they are, and this adds targets, budgets and player traffic to them
- Fast and slow burn-rate alerts over two windows each, in the form the Google SRE Workbook describes, registered with 17.67 and routed through 17.86, with the plain threshold rule for the same signal switched to burn rate when the objective is created, so the operator is not paged twice for one thing
- A spent budget named in the confirmation of an update on 17.17's page, of a 17.44 rolling restart and of any schedule marked risky, which may ask for a reason but never blocks
- Nothing computed and no condition registered until an objective exists, and suggested objectives offered from the installation's own history, changing nothing until saved

**Acceptance**

- [ ] A synthetic month of sign-in results with a known failure count gives the level and budget a hand computation gives, and wrong-password failures change neither
- [ ] A failure rate burning the budget 14.4 times too fast fires the fast alert inside its short window, and a slow steady drip fires the slow alert and not the fast one
- [ ] A planned maintenance window is left out of the level and drawn as left out
- [ ] With a budget spent, the update and rolling restart confirmations both say so, and neither is blocked
- [ ] With no objective, no burn-rate condition exists and nothing is computed
- [ ] A suggested objective is offered from the history and changes nothing until it is saved

## 17.123 Reachability from the internet: router, firewall and an outside probe

**Goal:** An operator hosting at home is told in plain words whether players outside the network can reach login, game and patch, and if not which step fails, with the router and firewall changes offered for exactly the ports players need.

**Size:** M. **Depends on:** 17.22, 17.23, 17.24, 17.29, 17.81

**Deliverables**

- A reachability page giving each player-facing allocation, login, game and patch, one verdict in plain words, reachable from outside, reachable only on this network or not reachable, with the step that fails: no router mapping, the firewall, carrier-grade NAT, a second router, or something beyond the router
- The router's external address read through UPnP IGD, NAT-PMP or PCP, with miniupnpc and libnatpmp, both BSD-licensed and cross-platform, and carrier-grade NAT reported when that address is in 100.64.0.0/10 or a private range, saying plainly that port forwarding cannot work there
- Opt-in router forwards, off by default, for the player-facing allocations 17.29 holds and never the admin API, panel, metrics or database ports, leased and renewed while the app runs, removed when it stops or its allocation changes, discovered only on the local interface, and audited
- Firewall rules scoped to port, protocol, profile and executable, made on Windows by the installer or the 17.24 desktop app, which can ask for elevation where the service account cannot, and on Linux applied through ufw or firewalld when either is found and the supervisor may, or printed for the operator; in Docker the page names the ports 17.23's Compose file publishes instead
- The 17.81 probe run from outside: from a second node, or from a helper's machine running the supervisor in a probe-only mode, joined with a token that can run the probe against the public address and report its result and nothing else; a probe from the same machine through its own public address is labelled as proving nothing about outside reach, since NAT loopback can succeed where the outside fails
- Every change the page makes to a router or a firewall audited, and doc/guides/internet-safety.md's hand-written firewall steps pointing to the page

**Acceptance**

- [ ] Against a test UPnP gateway, turning forwards on maps exactly the login, game and patch allocations, renews them and removes them when the app stops, and a request to map the panel or admin port is refused
- [ ] A gateway reporting 100.64.1.2 as its external address is reported as carrier-grade NAT with the sentence that forwarding cannot work
- [ ] With forwards off, the test gateway receives no discovery or mapping request
- [ ] A probe-only token runs the probe and reports its result, and is refused on every other route
- [ ] A probe through the machine's own public address is labelled as not proving outside reach
- [ ] Dev-gated: on a home network with a UPnP router, turning forwards on lets a machine on another connection reach the login screen, and turning them off closes it again. Needs a real router and a second internet connection
- [ ] Dev-gated: on Windows, the firewall rule made for the login allocation names its port, profile and executable, and removing the allocation removes the rule. Needs an elevated desktop run on a real machine

## 17.124 Backup copies to disks, shares and SFTP, with a 3-2-1 check

**Goal:** A hoster without an S3 bucket keeps a verified copy of every backup on a second disk, a USB drive, a NAS or an SFTP server they already own, and the panel says whether any copy would survive losing this machine's drive.

**Size:** M. **Depends on:** 17.16, 17.40, 17.43, 17.52, 17.72, 17.82

**Deliverables**

- Copy targets beside 17.43's S3 targets: a folder on another volume; a removable drive recognised by its volume serial on Windows or its filesystem UUID on Linux, never written when another drive takes its letter or mount point; an SMB or NFS share with its credentials sealed in the keyring, since the Windows service runs as `NT AUTHORITY\LocalService`, which reaches no share without them; and an SFTP server through libssh's client side, the library 17.40 already brings, with its host key pinned
- Each verified backup copied to each target by that target's rule, read back against the manifest's SHA-256 and recorded per backup with its location, its last check and a retention of its own under 17.52's pins; copies stay sealed by 17.72, and the first copy to a target shows the contents statement 17.43 shows and records the acknowledgement
- A removable drive catching up with every copy it missed when it is attached again, and a drive not seen for a set time raising a finding
- Copies checked again on a schedule by reading them back, and 17.51's restore streaming from any copy when the local archive is gone
- The backups page showing per backup how many copies exist, on how many kinds of media and how many off the machine, where only S3, SFTP and a share on another host count as off the machine
- 17.82 findings, each also an alert condition: every copy on the same physical disk as the database, no off-machine copy newer than 7 days, a removable drive not seen inside its window, and a copy that failed its check

**Acceptance**

- [ ] A backup copied to a folder on another volume is read back against its manifest and recorded with its location and check time, and a copy with one flipped byte fails its check and is marked bad
- [ ] A removable drive target receives the copies it missed when it is attached again, and a different drive at the same letter or mount point is not written
- [ ] An SFTP target whose host key changed is refused with both fingerprints named
- [ ] With every copy on the database's physical disk the finding says so, and adding an off-machine copy clears it
- [ ] Env-gated (AMBROSE_TEST_DB): with the local archive deleted, a restore streams from a copy target and succeeds
- [ ] No share or SFTP credential appears in any response, log or audit row
- [ ] Dev-gated: an SMB share on another machine receives a verified copy while the supervisor runs as its Windows service. Needs a real share on a second machine or a NAS

## 17.125 Moving the whole installation to a new machine

**Goal:** A hoster moving from an old PC to a new one, or from a desktop to a VPS, brings the whole installation across with panel users and their keys, and a dead machine is recovered the same way from a copy and the exported keys.

**Size:** M. **Depends on:** 17.16, 17.22, 17.24, 17.46, 17.51, 17.72, 17.99, 3.22

**Deliverables**

- Move out on the old machine, owner-only after a step-up check: a transfer package holding a fresh verified backup with its panel component, the 17.99 installation file, and the keyring's keys wrapped under a passphrase the operator types, the key derived by Argon2id and the keys sealed with AES-256-GCM through Botan, as the panel's other secrets are
- The package written to a file, or streamed to the new machine after a one-time pairing code over 17.22's join handshake, which never makes the new machine a node
- On the new machine, the desktop app's first run and the panel's `/first-run` page offering to bring an installation from another machine before any empty database is created: the databases restored through 17.51 and the panel store with them, the one place a store is restored on purpose, so users keep their passwords, two-factor, security keys and API keys, and every session the store carried ended on arrival
- Client data rebuilt from the new machine's own install through 3.22, never carried in the package
- A walk through what differs, the client path, folders, ports, database host, service registration and certificates, keeping the new machine's host-specific keys as the restore sequence in doc/PANEL.md already does unless the operator takes the old values
- The old installation marked moved, refusing to start apps without an owner override that needs a reason, so two machines never serve the same accounts
- A report comparing every table's row count with the snapshot record, and the same path recovering a dead machine from any archive copied off it plus the keys the owner exported; with only the backup key, panel users come back with two-factor and API keys reset, and the page says so before anything is restored

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): moving a test installation into a fresh data folder keeps accounts, characters, panel users with their two-factor secrets, and API keys, and the report shows every row count equal to the snapshot
- [ ] A package opened with the wrong passphrase is refused and nothing is written
- [ ] A pairing code works once and expires, and the receiving machine never appears on the nodes page
- [ ] After a move, the old installation refuses to start an app without an override, and the override is audited with its reason
- [ ] The package's manifest lists no client file, and the new machine builds its own type dump
- [ ] Env-gated (AMBROSE_TEST_DB): recovering from a sealed archive and the exported keys restores the installation with no package, and with only the backup key it says first that two-factor and API keys will be reset
- [ ] Dev-gated: moving from a Windows desktop to a Linux machine brings players back on the same accounts, and the owner signs in with the same two-factor. Needs two machines

## 17.126 Point-in-time recovery from binary logs

**Goal:** A bad GM command, a duplication exploit or a corrupting bug costs seconds of play rather than everything since last night's backup, because the operator can restore to the second before it.

**Size:** L. **Depends on:** 17.16, 17.24, 17.30, 17.43, 17.51, 17.67, 17.72, 17.78

**Deliverables**

- Opt-in per database host: on 17.24's private MariaDB the supervisor turns on row-based binary logging with full row images, and on a host registered through 17.30 it checks the same settings and names what is missing rather than changing that server
- A replication user among 17.30's generated users with only the rights to read the binary log, and the supervisor reading finished and in-progress binary logs through the replication interface of the MariaDB connector the servers already load, into the backup store, sealed as 17.72 seals archives and copied to every 17.43 target
- Each 17.16 snapshot recording its binary log position inside the dump transaction, so a base archive and the logs after it join exactly
- The recovery window on the backups page, such as "any second in the last 7 days", from the oldest base archive with an unbroken run of logs to the newest event read, with any gap named and the window ending at it; a gap, such as the server purging a log before it was read, raises a 17.67 alert
- A timeline in 17.51's restore wizard with 17.78's events and the activity log's rows as anchors, so the operator picks a moment such as the second before a world edit rather than typing one
- Restoring to a moment: the base archive loaded into staging by 17.51, the decoded row events replayed up to that second through prepared statements, so no SQL text is ever parsed, as the Backup archives row under Panel operations in doc/ARCHITECTURE.md requires, then the swap and the report 17.51 already does
- A schema change inside the replayed range matched to its 2.06 update file and applied through the updater, and one with no update file stopping the replay before it with the event named
- Binary logs kept as long as a retained base archive needs them and no longer, their size on the backups page, and their writes under the space guard

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): rows written after a backup come back when restoring to a moment after them, and a row written one second after the chosen moment is absent
- [ ] Env-gated (AMBROSE_TEST_DB): a world edit recorded in the activity log is undone by restoring to the second before it, picked from the timeline, while an account created before it remains
- [ ] The replay applies row events only through prepared statements, proved by a test that fails if the replay path reaches the plain text query call
- [ ] A schema change with no matching update file stops the replay before it and names the event
- [ ] A log purged on the server before it was read leaves a gap that the window shows and the alert names
- [ ] A stored binary log holds no known account verifier in its bytes, and a restore with the key succeeds
- [ ] Env-gated (AMBROSE_TEST_DB): turning recovery on for a registered host without binary logging names the missing settings and changes nothing on that server

## 17.127 Dynamic DNS and DNS-01 certificates

**Goal:** A home hoster whose address changes, or whose ISP blocks port 80, keeps a working hostname and gets a trusted certificate, and a panel reachable only on a LAN or a VPN gets one too.

**Size:** M. **Depends on:** 17.14, 17.35, 17.108

**Deliverables**

- Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md, beside the Certificates for a hostname row: the ACME DNS-01 challenge beside HTTP-01, through the DNS providers below, with HTTP-01 on port 80 staying the default
- A hostname and a DNS provider with a free API, DuckDNS, deSEC, dynv6 or Cloudflare, or RFC 2136 dynamic updates signed with TSIG for the operator's own BIND, Knot or PowerDNS, with the token sealed in the keyring and never shown back; each provider a small adapter of our own over the outbound HTTP client, and nothing contacted until one is chosen
- A and AAAA records kept pointed at the current external address, read from the router when 17.123 has found one or from an address service the operator names, which is the off-by-default setting for discovering a public address that Decisions, Experimental features in doc/ARCHITECTURE.md lists, updated only when the address changes and checked afterwards against the zone's authoritative servers rather than a cache
- DNS-01 issuance through 17.108's ACME client: the challenge record written, waited on until every authoritative server answers it, the order completed and the record removed, so a certificate is issued with port 80 closed and a panel bound only to a LAN or VPN address gets a trusted one with no self-signed walkthrough
- Checks before any order is placed, so a certificate authority's rate limit is never spent on an order that cannot succeed: the name resolves over IPv4 and IPv6 to the address the panel serves, a CAA record allows the authority, and the token can write the zone, each failure named
- A DNS card on the panel settings page with each record, its value, its last update and check, the challenge in use and the next renewal, and every update and issuance audited

**Acceptance**

- [ ] Against a local DNS server accepting RFC 2136 updates, a changed external address updates the A record once, and an unchanged one sends nothing
- [ ] Against a local ACME test server and that DNS server, a DNS-01 order issues a certificate with port 80 closed, and the challenge record is gone afterwards
- [ ] A CAA record naming another authority stops issuance before an order is placed, naming the record
- [ ] A stale answer from a caching resolver does not count as an update being live; only the authoritative servers' answer does
- [ ] The provider token appears in no response, log or audit row
- [ ] Dev-gated: an installation behind a router that blocks inbound port 80, with a DuckDNS or deSEC name, obtains a trusted certificate and keeps its record current across an address change. Needs an account with an outside provider and a real home connection

## 17.128 Reverse proxy configs and the forwarded-header check

**Goal:** An operator who already runs a reverse proxy gets a correct config for it, and the panel says whether the proxy's forwarded headers are trusted, so throttles and audit rows record the real client rather than the proxy.

**Size:** S. **Depends on:** 17.14, 17.26, 17.35

**Deliverables**

- A behind-a-proxy page that asks for the proxy, Caddy, nginx, Apache httpd, HAProxy, Traefik or IIS with Application Request Routing, and the public hostname, and writes a ready config for the panel listener and optionally the patchserver from templates of our own: the event socket's upgrade passed, read timeouts longer than the socket's 60-second silent close, buffering off for the socket, a body limit matching the listener's, HSTS set in one place only, and the admin API and `/metrics` never proxied
- After the operator has reviewed it, the page setting `Panel.TrustedProxies` to the proxy's address and the public URL 17.35 keeps, through the settings path with its audit row, refused where a layer locks the key
- A connection check reporting the request the panel actually received: the peer, every forwarded header, whether the peer is trusted, the client address, scheme and host that result, compared with the public URL, and whether a socket upgrade got through, each mismatch explained with its fix
- A problem record when forwarded headers arrive from a peer not in `Panel.TrustedProxies`, since every throttle and audit row then records the proxy's address, and doc/guides/reverse-proxy-tls.md pointing to the page for the choices it leaves to the operator

**Acceptance**

- [ ] The nginx, Caddy, HAProxy and Apache configs the page writes pass each product's own configuration check in a test, and a template test proves all six leave the admin API and `/metrics` unproxied
- [ ] Behind a test proxy missing from the trusted list, the check says the peer is untrusted and that audit rows carry the proxy's address, and once the page has set `Panel.TrustedProxies` the check and the next audit row show the real client address
- [ ] The event socket stays open for five minutes through the generated nginx config, proved by an end-to-end run
- [ ] A host or scheme that differs from the public URL is shown with both values named
- [ ] Setting the trusted proxies from the page is audited with the old and new values, and refused for a user without `panel.settings`

## 17.129 Staging copies and pull request previews

**Goal:** An operator or a helper tries a new build, pack, plugin or setting on a realistic copy of the installation that holds no real player data and cannot mail real players, and a pull request can be previewed against that copy before it lands.

**Size:** L. **Depends on:** 17.16, 17.17, 17.29, 17.30, 17.34, 17.51, 17.99, 17.116, 3.24

**Deliverables**

- A staging copy made from a chosen backup or a fresh one, loaded by the scratch-copy path 17.116 builds into schemas prefixed `stg_` and the copy's name, with the config carried across through 17.99's installation file and a second loginserver and gameserver started on 17.29 pool ports with a chosen build: the running one, a newer release, or a branch or pull request built through 17.17's build channel
- The copy badged STAGING on every card, page and audit row, with its apps at a scope of their own, so a grant can give a helper the staging copy and nothing live
- Scrubbing before the first start from a classification registry that names every column of every Ambrose database as personal, secret or plain: emails rewritten to `.invalid` addresses, addresses and machine ids dropped, every verifier reset to one staging password, security levels capped below game master, chat and report text emptied and sessions cleared; a test fails when a column exists with no class
- Mail, webhooks, bots, S3 and copy targets, the public status page, registration, certificate issuance, update channels and the patch signing key forced off in staging by the supervisor rather than by config the copy carries, and shown as forced off on the page
- Refresh from a newer backup keeping the chosen build, destroy with one click, and a lifetime after which the copy is removed with a notice first, each dropping every `stg_` schema, folder and port it held
- World edits made in staging reaching live only as a 17.34 journal change set, exported from staging and applied on live's world edits page with its own review, and nothing else flowing back
- A previews tab that builds an open pull request into a staging copy, runs the 3.24 client driver's scenarios against it and posts a commit status with the operator's own token, sealed in the keyring and unused until given; building a pull request from a fork runs its author's code, so it needs `staging.preview.fork` and a confirmation naming the author and commit
- `staging.create` and `staging.destroy` as permissions of their own, every create, refresh, destroy and preview audited, and staging schemas left out of 17.16's backups unless chosen

**Acceptance**

- [ ] Env-gated (AMBROSE_TEST_DB): a staging copy of a seeded installation starts on pool ports with STAGING on its cards and audit rows, and an account signs in to it with the staging password
- [ ] Env-gated (AMBROSE_TEST_DB): no seeded email, address, machine id or chat line survives in the copy, proved by a test that searches every copied row for each seeded value
- [ ] The classification test fails when a column is added to any Ambrose database without a class
- [ ] With mail and webhooks configured on live, an action in staging that would send either sends nothing, and the page shows both forced off
- [ ] A world edit made in staging reaches live only when its change set is applied on live's world edits page, and destroying the copy leaves no `stg_` schema, folder or listening port
- [ ] A preview of a fork's pull request is refused without `staging.preview.fork`, and with it asks for a confirmation naming the author and commit
- [ ] Dev-gated: a preview of a real pull request runs the client driver's scenarios against its staging copy and posts a commit status. Needs the maintainer's own retail client and a GitHub token

## 17.130 Usage accounting: transfer, energy and cost

**Goal:** An operator paying for a server, or watching a home data cap, sees what each app and node used this month, when the transfer allowance runs out, and roughly what it all costs.

**Size:** M. **Depends on:** 17.19, 17.22, 17.25, 17.65, 17.67

**Deliverables**

- Monthly totals per app, realm and node kept for 13 months: core-hours, memory GB-hours, disk growth per root and per database, and network in and out, with the patchserver's downloads counted apart; totals come from the counters 17.19 samples, never from downsampled averages, so a restart or a coarse range neither doubles nor loses any
- A transfer allowance per node with its reset day, the day it is forecast to run out, and a 17.67 rule that warns before it does, with the patch downloads' share shown beside it, since patch downloads by new players are what most often use one up
- Energy measured from RAPL on Linux where its counters are readable, and otherwise estimated from CPU time and a wattage the operator enters, always labelled as an estimate
- Prices the operator types, per node a month, per gigabyte of transfer and per kilowatt-hour, producing a monthly cost report per node and app and a cost per player-hour; no pricing service is ever called
- A usage page under `usage.read`, with export as CSV through 17.25's formula-safe writer and as JSON
- Every node's figures carried in 17.22's heartbeat, so the panel adds up an installation across machines

**Acceptance**

- [ ] A synthetic month of samples gives core-hours, memory GB-hours and network totals equal to a hand computation, and restarting the supervisor mid-month neither doubles nor loses any
- [ ] Patch downloads are counted apart from game traffic, and the two add up to the node's total
- [ ] An allowance forecast to run out inside the warning window raises its alert once, naming the day
- [ ] Without readable RAPL counters energy is labelled an estimate from the entered wattage, and with recorded counters it equals their difference across a counter wrap
- [ ] A CSV cell beginning with an equals sign exports as text
- [ ] A user without `usage.read` gets 403 on every usage route

## 17.131 Host security audit

**Goal:** The installation health page says where the machine around Ambrose leaves it open, such as a keyring any local user can read, a database that accepts remote root or an admin port reachable from outside, and each finding carries the exact fix.

**Size:** M. **Depends on:** 17.14, 17.23, 17.29, 17.30, 17.82, 17.123

**Deliverables**

- A Security group in 17.82's check registry, each check with its code, severity, sentence and fix route, and each fix either the page that makes it or a one-click fix that shows exactly what it will change, asks for confirmation and a step-up check, and is audited with the state before and after; the security checks 17.82 already runs, plain HTTP beyond loopback, two-factor not required and a first-run owner grant still held, stay where they are
- File access: the keyring, the panel store, every admin token file and every TLS private key held to the rule Panel operations in doc/ARCHITECTURE.md settles for the keyring, owned by the service user with mode 0600 in a 0700 folder on Linux and an access list naming only the service account, SYSTEM and Administrators on Windows, with a fix that resets a file to that rule and nothing wider
- Run-as: the supervisor or an app running as root, or as an elevated Administrator outside a service, routed to 17.23's service install under a dedicated user; a container running as root or with the Docker socket mounted, found from inside the container; and the hardening an installed unit or service still leaves unset, such as ProtectHome, a bounded capability set or a restricted service SID, reported with the line to add rather than rewritten by a second installer
- Database posture on every host 17.30 registers, read through its administrative connection: remote root, anonymous users, the test schema, accounts with empty passwords, a runtime user whose grants have drifted beyond the least privilege 17.30 generated, and a server bound beyond loopback while every app using it is on the same machine; each statement-level finding offered as a one-click fix naming the statement it runs, and the bind shown with the configuration line to change, since it needs the database server restarted
- Exposure: every admin API, SFTP and `/metrics` listener in 17.29's allocations bound beyond loopback, the operating system firewall leaving the panel or an admin port open to the network, read and closed through the firewall layer 17.123 builds without touching a rule it did not create, and the TLS versions the panel and admin listeners actually offer, with anything below TLS 1.2 named
- Information findings that never raise an alert on their own: whether the volumes holding the data folder and the backups are encrypted at rest, and whether operating system security updates are pending, as the platform's own update service reports it
- A check that cannot read its subject, such as a database host whose administrative user may not list accounts or a firewall the service account may not query, reporting as unknown with the reason, never as passed, under 17.82's rule

**Acceptance**

- [ ] A keyring file made readable by every local user is reported with its path and the rule it breaks, and the fix restores the settled owner and mode, or access list, and clears the finding, proved by unit tests on Linux and on Windows
- [ ] Env-gated (AMBROSE_TEST_DB): a database host with an anonymous user, the test schema and a root account that accepts remote connections shows three findings, and each fix removes exactly its subject and writes an audit row naming the statement, run once against MySQL 8 and once against MariaDB
- [ ] An admin API bound to 0.0.0.0 shows its finding with the allocation named, and a firewall reader reporting that port open adds the firewall finding with its fix, proved by a unit test over a fake firewall layer
- [ ] Dev-gated: on Windows and on Linux with ufw, the fix closes an open admin port while a player-facing allocation stays open and a rule the operator wrote stays untouched. Needs a machine whose firewall the run may change, so it is run by hand and recorded
- [ ] A listener that accepts TLS 1.1 is reported naming the versions a handshake against the running listener was actually served, proved by a unit test that connects with each version
- [ ] Running the supervisor as root, or as an elevated Administrator outside a service, shows the finding with the route to the service install, and the Docker image started with the socket mounted shows its own finding
- [ ] A check whose subject cannot be read reads as unknown with the reason, a viewer sees no fix control, and a fix without a recent step-up check changes nothing and asks for one

## 17.132 New sign-in notices and stolen-session signals

**Goal:** An operator hears within seconds when their panel account is used from a device or network it has not seen, and one link in that notice shuts the intruder out; owners are told when a session looks stolen.

**Size:** M. **Depends on:** 17.14, 17.35, 17.36, 17.38, 17.46, 17.47, 17.86

**Deliverables**

- Known devices and networks per panel user: a signed, HttpOnly device cookie holding a random id stored only as its hash, the browser family from the user agent, and the network as the /24 of an IPv4 or the /48 of an IPv6 client address taken from 17.14, listed on the 17.38 account page with first and last seen and a control to forget one
- An address database reader for an MMDB file the operator places in the data folder, such as DB-IP Lite, which needs no account, answering country and ASN, reloaded live with the old file kept when a new one will not open, and harmless when absent: every rule here then works on device and network alone and says so, and 17.138 reads this reader rather than a second one. Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md: the reader is libmaxminddb from vcpkg, under Apache-2.0, which OSS-Fuzz fuzzes continuously, rather than a reader of the project's own, because an input from outside the machine is better met by a parser that years of fuzzing have hardened
- A sign-in from a device or network the user has not used before notifies that user through their 17.86 channels, the notification centre always and email or their webhook where they set one, naming the time, browser, network and, with an address database, the country, and carrying a single-use "This wasn't me" link
- "This wasn't me" works without signing in, is stored only as a hash, expires after a set number of days and works once: it ends every session and socket of that user, suspends their API keys, sets 17.46's must-change flag, requires a fresh second-factor check at the next sign-in and notifies every owner. A suspended API key is refused until its owner reinstates it, a state built by whichever of this milestone and 17.135 lands first
- Deterministic signals raised to owners as 17.67 conditions through 17.86's routing, never a statistical score: a correct password followed by repeated wrong second-factor codes, a session cookie presented from a network or browser family other than the one it was issued to, one user active from two countries at once, and an API key's first use from a network it has not used; an owner chooses each rule's action, notify only, require a step-up check on the session's next danger action, or end the session
- For users without two-factor sign-in, an optional hold, off by default and needing SMTP from 17.35, that keeps a sign-in from a new device on a new network pending until a link mailed to the user confirms it
- Every notice, confirmation, "This wasn't me" use and signal audited under the `auth` namespace with the user as subject, and no notice or alert body carrying the full client address, only the network and country, under the payload rule 17.86 enforces

**Acceptance**

- [ ] A sign-in from a new browser on a new network notifies the user once with the browser and network, and the same browser signing in again from that network sends nothing, proved by an end-to-end run against a supervisor
- [ ] Opened without a session, the "This wasn't me" link ends that user's other sessions within one second, answers their API keys with 401, and makes the next sign-in set a new password and pass a second factor, and a second use of the link is refused
- [ ] A session cookie replayed from another network raises the owner signal, and with the rule set to end the session the next request from either address is refused, proved by a unit test with `Panel.TrustedProxies` set
- [ ] Three wrong second-factor codes after a correct password raise one owner alert naming the user, not three
- [ ] With an MMDB file present, sign-ins by one user from two countries inside the rule's window raise the two-countries signal; with the file removed that rule reports as unavailable and the device and network rules still fire
- [ ] An API key's first request from a new network notifies its owner and raises the owner signal, and a later request from the same network does neither
- [ ] No notice or alert body holds a full client address, proved by a shaping test

## 17.133 Sign-in with OIDC, Discord or GitHub

**Goal:** Operators sign in to the panel with an identity their staff already share, losing a Discord role or a team membership takes panel access away within minutes, and flagged local owners keep a way in when the provider fails.

**Size:** L. **Depends on:** 17.35, 17.38, 17.46, 17.47, 17.48, 17.50, 17.86

**Deliverables**

- A Sign-in providers tab in 17.35's panel settings, behind `panel.settings` and empty by default, holding any number of providers: generic OpenID Connect by issuer URL, which covers Authentik, Keycloak, Google and Microsoft, and Discord and GitHub over their OAuth 2.0 flows; each client secret sealed in the keyring and never shown back, and each callback address built from the panel's public URL
- The authorization code flow with PKCE, with state and nonce bound to a short pre-authentication cookie; for OpenID Connect the discovery document and signing keys fetched through the outbound HTTP client Panel operations in doc/ARCHITECTURE.md settles and cached, the ID token's signature checked with Botan for RS256, ES256 and EdDSA, and its issuer, audience, expiry and nonce checked within a bounded clock skew; a provider that cannot be reached or answers wrongly fails that sign-in with the reason and leaves password sign-in untouched
- The sign-in page showing a Continue with button for each enabled provider beside the password form, and exactly the password form when none is enabled, so the zero-step desktop run and every installation that never opens this tab are unchanged
- Linking: an operator links an identity from the 17.38 account page after entering their password and a current second factor, sees each linked identity with its last use, and unlinks it the same way; just-in-time provisioning, which creates a panel user for an unknown identity a mapping rule admits, is a per-provider setting off by default, and an identity that neither a link nor provisioning admits is refused with the one generic sign-in message
- Mapping rules per provider, evaluated in order, from a claim, a group, a Discord guild and role or a GitHub organization and team to a role at a scope or a set of grants, such as a guild's Moderator role giving game master on one realm once realm scope exists and on its gameserver app until then; grants a rule made are marked as managed by that provider and shown read-only with the rule named wherever grants are edited, and no rule can grant past 17.48's owner-only and no-escalation rules
- Membership rechecked on a set interval with the provider's refresh token sealed in the keyring: a lost role, team or group removes what its rule granted and bumps the user's session generation, so their sessions and sockets close within one second; while a provider cannot be reached the last known mapping holds for a grace period an owner sets, and then that provider's sessions end
- A role on 17.50's roles page can be marked provider only, so its holders are refused at the password form, and an owner can flag named local owners as break-glass accounts that keep password sign-in, each of whose password sign-ins notifies every other owner at once through 17.86
- Second factor per provider: an owner either accepts an OpenID Connect provider's own multi-factor claim from `amr` or `acr`, or requires the local 17.47 second factor after the provider as well, which is the default and the only choice for Discord and GitHub, whose tokens carry no such claim
- Every link, unlink, provider sign-in, refusal, mapping change, recheck result and break-glass sign-in audited under the `auth` namespace with the provider named, and no token, code or client secret in any log, response or audit row

**Acceptance**

- [ ] Against an OpenID Connect provider the test serves itself, a linked operator signs in, and an ID token with a wrong audience, an expired one, a replayed nonce and one signed by a key outside the provider's set are each refused with the reason audited, proved by unit tests
- [ ] With no provider enabled, the sign-in page and every sign-in route answer exactly as they did before this milestone, proved by the 17.46 and 17.47 sign-in tests passing unchanged and a browser test of the page
- [ ] A mapping rule gives a provisioned user the mapped role, and removing the group at the test provider makes the next recheck remove that role and close the user's sockets within one second
- [ ] A holder of a provider-only role is refused at the password form, and a flagged break-glass owner signs in by password while every other owner is notified
- [ ] Linking an identity without a current second factor is refused, and an identity neither linked nor admitted by provisioning gets the message an unknown user gets
- [ ] With the test provider unreachable, sessions it issued last through the grace period and end after it, and break-glass owners are untouched
- [ ] Dev-gated: an operator signs in with Discord, and removing their Moderator role in a real guild removes their panel access within one recheck interval; the same run is made with a GitHub team. Needs a Discord guild, a GitHub organization and the maintainer's own application credentials, so it is run by hand and recorded

## 17.134 Access reviews and the who-can explorer

**Goal:** An owner answers who could have done this in seconds during an incident, sees who still holds rights they no longer use, and runs a periodic review that trims grants on the record.

**Size:** M. **Depends on:** 17.15, 17.25, 17.36, 17.37, 17.48, 17.50, 17.86

**Deliverables**

- Last use per permission: `AuthorizationMgr` records, for each user and API key, when each permission was last exercised at each scope, written at most once a minute as 17.36 writes a key's last use, and kept in the panel store rather than read back out of audit rows, so the figure outlives the activity log's retention
- A who-can explorer: pick a permission and a scope, such as `backups.restore` on one app, and see every user and API key holding it with its source, their role, a direct grant or a grant inherited from a wider scope, and when each last used it; or pick a user and see their effective permissions with the source of each; both answered by the same resolution 17.48 uses to decide a request, so the explorer and the decision cannot disagree
- A Dormant tab listing users not signed in for a set number of days, API keys unused for a set number of days, and danger permissions a holder has not exercised in 90 days, each with a one-click reduce or revoke that goes through 17.37's no-escalation rule
- Optional automatic disabling of dormant users and keys, off by default, which warns the holder through their 17.86 channels a set number of days before it acts and never touches the last owner
- Access reviews an owner schedules through 17.15, such as quarterly: each grant, role assignment and API key at the chosen scope becomes a line that a named reviewer marks keep, reduce or revoke by a deadline, reviewers are notified through 17.86, reduce and revoke apply through 17.37's grant editing path, and a line left unanswered at the deadline takes the review's chosen default, keep and flag or revoke
- The explorer and the Dormant tab behind `users.read` and every change behind `users.update`, each change a review or the Dormant tab makes audited with the review named, and a finished review exportable as CSV and JSON through 17.25's formula-safe writer for holders of `activity.export`

**Acceptance**

- [ ] The explorer lists every user and key holding `backups.restore` on an app with its source, a grant inherited from panel scope included, and a test that compares its answer with 17.48's decision for every user in a seeded installation fails when they differ
- [ ] Using a permission updates its last use at most once a minute, and the figure is still there after an activity sweep has removed the audit rows
- [ ] A user idle past the threshold appears on the Dormant tab, and revoking a grant there bumps their session generation and writes an audit row
- [ ] With automatic disabling on, a dormant user is warned first and disabled only when the notice period ends, and the last owner is never disabled whatever their idle time, proved with a clock the test moves
- [ ] A scheduled review opens on time with a line per grant, a revoke marked there removes the grant through the grant editing path, and a line unanswered at the deadline takes the review's default
- [ ] The exported review lists every line with its reviewer, decision and time, and a reason beginning with an equals sign exports as text
- [ ] A user without `users.read` gets 403 on the explorer, the Dormant tab and every review

## 17.135 Compromise response: canary credentials and lockdown

**Goal:** A leaked backup, support bundle or installation file names itself the moment someone tries what is inside it, and one control stops a hijacked or leaked panel from doing more harm while an owner works out what happened.

**Size:** L. **Depends on:** 17.16, 17.22, 17.25, 17.36, 17.37, 17.47, 17.52, 17.67, 17.84, 17.87, 17.99, 17.140

**Deliverables**

- Canary credentials, each unique to one artifact and recorded in the panel store with the artifact it went into: a game account row written into the login database's stream in each backup archive, which exists in no live database; an API key string in the `amb_` form written into each support bundle's effective configuration in place of one redacted value, and into each exported installation file as a comment line that apply ignores; and an admin token written into each backup's config component in a token file no app reads. The operator is told when each artifact is made that it carries one
- Detection wherever a canary can be tried: the login server holds the canary account names as keyed hashes the supervisor pushes to it, never in a table a backup carries, and reports any sign-in attempt naming one, successful or not; the panel reports any request presenting a canary key's public id; and each app's admin API holds the canary tokens' hashes and answers one with 401 like any wrong token while reporting it. A canary never grants anything
- A tripped canary raising a critical 17.67 alert naming the artifact, the address that tried it and every audited download or export of that artifact with who made it, read from 17.52's downloads and the 17.84 and 17.99 audit rows
- A restore through 17.51 leaving the canary row out, and 17.16's snapshot record counting each table without it, so verification, the restore report and a restored installation are exactly what they were
- A Lock down control for owners, also `panel lockdown on` at the supervisor console and a task 17.87's triggers can run, which in one step ends every panel session and socket except the initiator's, suspends every API key, closes invites and, where they are on, SFTP and remote pull, pauses every schedule and trigger whose tasks change anything, rotates every app's admin token at once with no overlap through 17.140's rotation path, revokes every outstanding node join token, restricts sign-in to owners and requires each owner to pass their second factor again. A suspended API key is refused until reinstated, a state built by whichever of this milestone and 17.132 lands first
- Lockdown kept in the panel store so a supervisor restart comes back locked down, shown as a banner on every page and reported by the status API naming who locked the panel down, when and why, and every lockdown step audited
- A review screen grouping every audit row since a time the owner chooses by actor, with sign-ins, grant changes, key use, restores, settings changes and file writes marked, read from 17.25's store rather than a copy
- Unlocking as a staged checklist an owner walks through, confirming the review, rotating what the review points at, reinstating API keys one by one or together, reopening invites and remote access, resuming schedules and triggers and lifting the owner-only restriction, each stage audited and the panel staying locked down until the last

**Acceptance**

- [ ] Signing in to the login server with the canary account of one backup, with any password, raises one critical alert naming that backup and each audited download of it, proved by an end-to-end run against a login server
- [ ] The canary key string from a support bundle is refused at the panel with 401 and raises the alert naming that bundle, and a backup's canary admin token presented to an app's admin API does the same
- [ ] Restoring a backup leaves no canary account in the login database, and the restore report matches the snapshot record table by table
- [ ] Locking down ends every other session and socket within one second, answers every API key with 401, refuses a non-owner's sign-in and pauses a mutating schedule, while the initiator's session keeps working
- [ ] After a lockdown every app refuses its previous admin token, and the panel still relays to each app with the new one
- [ ] A supervisor restarted during a lockdown comes back locked down with its banner, and the lockdown lifts only when every unlock stage is done, each writing an audit row
- [ ] A user who is not an owner can neither lock down nor unlock, and `panel lockdown on` at the console works with no panel session

## 17.136 Expiring grants, just-in-time elevation and break-glass

**Goal:** A helper game master holds rights only for the event weekend, a volunteer who needs restore rights at three in the morning asks and gets them for two hours, and an emergency can skip approval only in plain sight.

**Size:** M. **Depends on:** 17.37, 17.47, 17.48, 17.50, 17.57, 17.86

**Deliverables**

- An end date on grants and role assignments, set in 17.37's grant editor and on 17.50's users page and carried on the versioned grant row; a sweep ends each at its time, bumps the user's session generation and lets 17.57 re-filter their sockets, and while one is running the user sees a banner with the time left and a control to ask for more
- Request access: the access-denied page, a 403 the dashboard shows and a control shown disabled for a missing permission each offer to ask for the permission and scope they name, with a duration up to an owner's maximum and a reason; the request reaches the users who may grant it under 17.37's no-escalation rule through their 17.86 channels, one of them approves or refuses it in one click, and approval creates the grant with its end date, never a permanent one
- Eligible roles: an owner marks a user eligible for a role at a scope, and the user activates it for up to a set number of hours with a reason and a step-up check, with or without approval as the eligibility says
- Break-glass: users an owner designates may take a named emergency role for a bounded time with a mandatory reason and a step-up check and no approver; while it is active every operator's page carries a red banner naming who and why, every owner is notified at once, and it ends by itself like any other expiring grant
- Nobody approves their own request, a request for more than an approver holds cannot be approved by them, and 17.48's last-owner and owner-only rules hold for a time-boxed grant exactly as for a permanent one
- A requests page with open, approved, refused and expired requests and the active elevations with their time left, and every request, approval, refusal, activation, expiry and break-glass audited with both users named

**Acceptance**

- [ ] A grant with an end date stops working at that time without a sign-out, the user's socket loses the stream it covered within one second, and the audit log shows the expiry, proved with a clock the test moves
- [ ] A game master refused `backups.restore` asks for it for two hours, an admin approves it in one click, and the restore route answers for those two hours and refuses after them
- [ ] A user cannot approve their own request, and an approver who does not hold the requested permission is not offered the request
- [ ] Activating an eligible role without a recent step-up check is refused and changes nothing
- [ ] Breaking glass without a reason is refused; with one, every signed-in operator's page shows the banner within one second and every owner is notified
- [ ] The time-left banner shows the end time the grant row holds, and asking for more reaches the approvers

## 17.137 Installed components and security advisories

**Goal:** When the next OpenSSL or libssh advisory lands, the panel says whether this build is affected and which Ambrose release fixes it, from an inventory of everything the installation runs.

**Size:** M. **Depends on:** 17.17, 17.67, 17.82, 17.97, 17.105

**Deliverables**

- A CycloneDX software bill of materials for every release, built in 17.105's release job from the SPDX files vcpkg writes for each port and from the npm lockfile, covering the supervisor, each app, the dashboard and the private MariaDB 17.24 installs, published beside the release's artefacts with its SHA-256 and carried inside the installed build
- A Components page listing each library with its version, licence and the programs that load it, beside what the supervisor detects at run time: each registered database server's version, the OpenSSL actually loaded, the operating system build and, in Docker, the base image
- One advisory matcher in `src/common/` over OSV records, fed from three sources: a feed signed with the release key and fetched by the channel check 17.17 already makes, adding no request of its own; opt-in queries to OSV.dev, off by default, that send only component names and versions; and an OSV export an operator drops into the data folder for a machine with no network
- Each finding showing its advisory id, severity, affected and fixed versions, whether a newer Ambrose release carries the fix, with a link to 17.17's update page, and, where the signed feed carries one, the project's note on whether the affected code is reachable in the way Ambrose uses the library, never guessed by the panel
- Suppression of a finding with a reason and an expiry, audited, after which it returns; and findings at or above a set severity published as a 17.82 finding and a 17.67 condition, and as a line in 17.97's digest while any is open
- The same matcher run in the release job over the release's own bill of materials, and on a pull request that changes `vcpkg.json` in the Linux leg such a change already builds, failing on an unsuppressed advisory at or above a set severity

**Acceptance**

- [ ] A release's bill of materials lists every vcpkg port and npm package the build carries with its version and licence, and a check fails when a port in `vcpkg.json` is missing from it
- [ ] Given an OSV export naming the loaded OpenSSL's version as affected, the Components page shows the finding with its fixed version and names the newer release whose bill carries the fix, proved by a unit test over the matcher and a browser test of the page
- [ ] With OSV.dev queries off, the supervisor makes no outbound request beyond 17.17's channel check, proved by a test that records every outbound request
- [ ] A feed whose signature does not verify is refused, nothing is replaced, and the previous feed's findings stay
- [ ] A suppressed finding is hidden until its expiry and returns after it, and the suppression is audited with its reason
- [ ] A finding at the alert severity raises its 17.67 condition once and appears on 17.82's page, and clearing it clears both

## 17.138 Network access rules and the address ban list

**Goal:** An owner keeps the owner surface of the panel reachable only from where it should be while moderators still sign in from anywhere, a stolen session cookie is useless from another network, and repeated guessers are banned in plain view.

**Size:** M. **Depends on:** 17.14, 17.36, 17.40, 17.46, 17.47, 17.48, 17.132, 6.05

**Deliverables**

- A Network access page holding allow and deny rule sets panel-wide, per role and per user, each rule a CIDR range, a country or an ASN, such as owners only from one provider's range or no sign-in from hosting ASNs; country and ASN come from the address reader 17.132 builds, and with no address database only CIDR rules can be saved
- Enforcement at sign-in and again on every request and socket upgrade against the 17.14 client address, compiled into a prefix table swapped atomically when rules change so a check costs one lookup, and optionally at SFTP sign-in; a refused request answers 403 and is audited with the rule named; 17.36's CIDR lists on API keys and 17.09's allow list on `/metrics` stay as they are, and a key must pass both its own list and these rules
- Report-only mode for any rule set, which audits every request it would have refused without refusing it, so a rule is watched before it is enforced
- Lock-out protection: saving a rule that would refuse the saver's own current address names that address and asks for a step-up check, an owner on loopback passes whatever the rules say, and `panel access off` at the supervisor console turns enforcement off until it is turned on again, audited with the console as actor
- An address ban list, fail2ban style, of addresses banned automatically after repeated sign-in failures across several accounts, beside 17.46's throttles rather than replacing them, each entry with its reason, first and last hit, hit count and expiry, and an unban that takes a reason
- An opt-in rule set for players, pushed to the login server as a live reload target, which refuses a matching address before authentication the way a 6.05 address ban does, with the reason the client shows
- Every rule change, ban, unban and enforcement toggle audited with the rule set before and after

**Acceptance**

- [ ] A rule allowing owners only from one range refuses an owner's request from outside it with 403 and names the rule in the audit row, while a moderator outside the range still signs in
- [ ] A session cookie presented from an address a rule refuses is refused on its next request and its socket closes, proved by a unit test over the 17.14 client address
- [ ] In report-only mode nothing is refused and each would-be refusal writes an audit row
- [ ] Saving a rule that excludes the saver's own address without a recent step-up check changes nothing, an owner on loopback passes every rule, and `panel access off` at the console lets a refused owner back in
- [ ] Failed sign-ins against several accounts from one address put it on the ban list with its count and expiry, the ban lifts at its expiry, and an unban lifts it at once with an audit row
- [ ] With no MMDB file a country rule cannot be saved and CIDR rules still apply; with one, a country rule refuses an address the file places in that country
- [ ] A player rule set refuses a matching address at the login server before authentication and admits every other, proved by an end-to-end run with a fake client

## 17.139 Player privacy requests: export, erasure and retention

**Goal:** When a player asks what the server holds about them, or asks to be forgotten, the operator answers completely from one tab, later restores included, without hand-editing SQL.

**Size:** L. **Depends on:** 17.21, 17.25, 17.47, 17.51, 17.52, 17.62, 17.63, 17.80, 17.95

**Deliverables**

- A Privacy tab on each game account behind a new `accounts.privacy` permission, marked danger and grantable at cluster and panel scope, with every action on it needing a 17.47 step-up check
- Export: one archive of everything held about the account, the account row without its verifier, the email, sign-in history with addresses and MachineIDs, characters, chat the player wrote, reports by and about them with other players' identities removed, bans and mutes, registrations and verifications, and the audit rows naming the account as subject, written as JSON with an HTML index that opens offline, built as a background job and fetched once through a one-time ticket the way 17.52's downloads are, and audited
- Erasure that deletes or pseudonymises in one transaction per database: the username replaced by a tombstone, the email, addresses and MachineIDs removed, chat text blanked while moderation records and item and currency ledger rows keep their shape with the identity replaced, and the search index rows 17.80 holds for that text removed, with a report of the rows touched per table
- An opt-in keyed hash of each MachineID, an HMAC under a keyring key, kept for an erased account that carries a machine ban, so the ban still holds without the MachineID being kept
- A forget list of erased accounts in the panel store, reapplied automatically after any 17.51 restore or 17.95 character restore before the restored apps start, so a restore never brings erased data back; what cannot be rewritten in place, sealed backup archives and log files, is listed on the request with the date its retention removes it
- Retention as live settings for sign-in addresses, MachineIDs and chat text, swept daily by the sweep 17.25 runs, with one audit event per sweep counting what it removed
- A request log with who asked, when, for what, its due date, its state and its answer, a reminder before the due date, and every export, erasure and change audited with the account as subject
- Opt-in self-service on 17.62's public pages, off by default and needing SMTP: a player asks for an export or an erasure and confirms it by a link mailed to the account's address, and an erasure waits a cooling-off period an owner sets, during which the player signing in cancels it

**Acceptance**

- [ ] An export of a seeded account holds every section its index lists, no verifier, and no other player's name or address, proved by a shaping test
- [ ] Erasing an account removes its email, addresses and MachineIDs from every table and from the search index and blanks its chat text, while its mutes, bans and ledger rows remain with the identity replaced
- [ ] With the keyed hash on, a machine-banned account stays banned after erasure: a new account from the same MachineID is refused, proved by a unit test over the ban check
- [ ] Restoring a backup taken before an erasure leaves the account erased when the apps start, and the restore report names the forget list as applied
- [ ] The retention sweep removes addresses older than the setting, keeps newer ones, and writes one audit event with the count
- [ ] A user without `accounts.privacy`, or without a recent step-up check, is refused and the attempt is audited
- [ ] With self-service on, an erasure a player asks for waits for the mailed confirmation and the cooling-off period, and the player signing in during it cancels it

## 17.140 Secrets inventory and key rotation

**Goal:** An owner sees every credential the installation holds, how old each is and what uses it, and rotates any of them, keyring keys included, without taking anything down.

**Size:** M. **Depends on:** 17.02, 17.22, 17.28, 17.30, 17.36, 17.47, 17.65, 17.67, 17.72

**Deliverables**

- An owner-only Secrets page listing every secret without its value: each app's admin token, the keyring's keys by purpose and id, the backup key, the patch signing key, database host and runtime passwords, the SMTP and S3 secrets, node certificates, TLS private keys and the ACME account key, `Account.VerifierKeys`, the SFTP host key, the probe account and every API key; each with when it was created and last rotated and by whom, its age and expiry, what uses it, and how many sealed rows still carry an older key id. A secret whose milestone has not landed is absent rather than shown empty
- A maximum age per kind as a live setting, raising a 17.67 condition and a reminder to the secret's owner when it passes, beside the dated expiry alerts 17.67 already sends
- A Rotate button on each row that calls the path that secret already has, 17.30 for database passwords, 17.36 for API keys, 17.65 for the patch key, 17.22 for node certificates and the settings path for `Account.VerifierKeys`, so no secret gains a second rotation path
- App admin token rotation from the panel, new here: the supervisor gives the app a new token through its admin API, the app accepts the old and the new for an overlap window, the supervisor's relay moves to the new one, and the old stops working when the window ends, where 17.02 rotates a token only by a config edit and a reload
- Keyring key rotation, new here: a new key id becomes the one that seals, a background job re-seals every sealed row under it with progress shown and resumes after a restart, and the old key is retired only when no row, and for the backup key no retained archive, still needs it
- "I think this leaked" on each row, which rotates at once with no overlap, ends every session and socket that depended on that secret, and records the reason
- Every rotation, retirement and leak response needing a step-up check and audited, and no value in any response, log line or audit row

**Acceptance**

- [ ] The page lists every kind of secret the build holds, and a test fails when a sealed column or a token file is added without an inventory entry
- [ ] Rotating an app's admin token from the panel keeps the relay answering throughout, the old token answers 401 once the overlap window ends, and the app never restarts
- [ ] A keyring key rotation re-seals every row under the new key id, resumes where it stopped after the supervisor is killed halfway, and retires the old key only when the count of rows under it reaches zero
- [ ] The backup key is not retired while a retained archive is sealed under it, and the page names the archives that hold it back
- [ ] "I think this leaked" on an API key revokes it at once with no grace, and on an admin token ends every admin API session opened with it, with no overlap window
- [ ] A secret past its maximum age raises one condition and one reminder, and rotating it clears both
- [ ] No response, log line or audit row from any route here holds a secret value, proved by a shaping test, and a user who is not an owner gets 403 on every route

## 17.141 Two-person approval and change requests

**Goal:** The actions that can wipe a server, restore over live data or promote an outsider need a second operator to agree, and any change can be proposed, reviewed and then applied exactly as it was reviewed.

**Size:** L. **Depends on:** 17.25, 17.37, 17.47, 17.48, 17.49, 17.53, 17.86

**Deliverables**

- Change requests: a mutating route that declares itself submittable can be sent as a request instead of applied, storing the exact payload, its route, the structured difference 17.53's review renders, a reason and the version of what it changes; settings batches and grant and role edits declare it here, and world edit change sets, schedule saves, installation-file applies, restores and pack or plugin installs declare it as they land
- A Changes page listing open, approved, applied, refused, expired and stale requests, each with its difference, author, reason and the reviewer's note, live on the 17.26 socket; discussion on a request comes with 17.162's threads
- Review: another holder of the permission the change needs at its scope approves or refuses it with a step-up check, an author never approves their own, and approval is refused when the reviewer's own rights do not cover the whole change
- Apply runs the stored payload through its normal route under the same `AuthorizationMgr` and `AuditScope` as a direct change, so a request carries no authority of its own, with one audit row naming the author, the approver and the request; a request whose base version has moved is stale and cannot be applied until it is rebased, and an approved request not applied expires after a set time
- An approval policy per permission, off by default, naming the actions that must go through a request, such as restoring player databases, purging files, granting the owner role, revealing a secret, exporting the backup key and engaging a kill switch; a covered action sent directly is refused with 409 naming the policy, and turning a rule on is refused while fewer than two users could approve at that scope
- A veto timer for an installation with one owner: a covered action waits a set time while every owner is notified, then applies unless cancelled, with the wait and any cancel audited
- Notices through 17.86 to the users who can review a request, and to its author when it is decided
- 17.104's second-operator approval for compensation above a threshold carried onto this engine as one policy when both have landed, so the panel keeps one approval path; asking for rights one does not hold stays 17.136's access request, which asks for access rather than proposing a change

**Acceptance**

- [ ] With the policy on for settings batches, a batch sent as a request changes nothing until a second operator approves it, and applying it changes exactly what the difference showed, with one audit row naming both operators
- [ ] An author cannot approve their own request, and an approval without a recent step-up check is refused
- [ ] A request whose settings changed underneath it is marked stale and cannot be applied, and neither can one past its expiry
- [ ] Turning a policy on at a scope where only one user holds the permission is refused with the reason named, and on a single-owner installation the veto timer applies the change after its wait unless it is cancelled
- [ ] A reviewer whose rights cover only part of a grant change cannot approve it
- [ ] A covered action sent directly while its policy is on answers 409 naming the policy, from the panel and from an API key alike
- [ ] With every policy off, every route answers exactly as before this milestone, proved by the route registry test

## 17.142 Discord bot: commands, alert buttons and live status

**Goal:** A community run from Discord operates its server from there: operators check status, restart a crashed realm and acknowledge alerts from a phone with the same permissions and audit trail as the panel, and nothing opens a port.

**Size:** L. **Depends on:** 17.26, 17.36, 17.38, 17.48, 17.49, 17.67, 17.85, 17.86

**Deliverables**

- An opt-in bot, off by default: an owner pastes the token of a Discord application they created, sealed in the keyring and never shown back, and picks the guild and channels; the bot connects outward over Discord's gateway, so it works behind NAT with no open port, and reconnects and resumes with backoff
- The gateway client is D++ from vcpkg, under Apache-2.0, rather than a client of the project's own over the admin API's WebSocket layer. Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md
- Linking: each operator links their Discord user to their panel user once, with a one-time code the 17.38 account page shows and a command in Discord; an unlinked Discord user is refused everything, and unlinking on either side applies to the next command
- A chat-neutral command layer that runs each command as the linked panel user through 17.49's relay and `AuthorizationMgr`, audited with the source `discord`, offering `/status`, `/players`, `/restart`, `/announce` and `/maintenance` as the build's capabilities and the user's permissions allow and answering a refusal with the permission it lacked; a Matrix adapter can reuse it without touching the commands
- Confirmation: a destructive command asks with a button and then a modal in which the operator types the target's name, and an action that needs a step-up check never runs from chat and is answered with a link to it in the panel
- Danger permissions disabled in chat for everyone until an owner allows each by name, whatever the linked user holds
- Alerts posted to a chosen channel with Acknowledge, Silence 1h and Open buttons, which write the same acknowledgement and silence rows as 17.67 and 17.85 under the pressing user's permissions
- One status message per realm, pinned and edited in place with its state, players and last restart rather than a new message per change, within Discord's own rate limits
- The bot's state on the panel, connected or not with its last error, the linked users and the commands run, and every link, unlink, command and button press audited

**Acceptance**

- [ ] With the bot off, the supervisor opens no connection to Discord and the zero-step desktop run is unchanged, proved by a test that records outbound connections
- [ ] Driven through a test adapter, `/restart` from a linked user holding `power.restart` restarts the app after the typed confirmation, and from one without it answers with the permission named and runs nothing, each audited with the source `discord`
- [ ] A command from an unlinked Discord user is refused, and a one-time link code works once and expires
- [ ] A danger permission not allowed in chat is refused there for an owner who holds it, and a step-up action answers only with a panel link
- [ ] Pressing Acknowledge on a posted alert writes the same acknowledgement row the alerts page writes, naming the linked user
- [ ] Dev-gated: in a real guild, the bot answers `/status`, restarts a realm after confirmation, and edits its pinned status message in place when the realm comes back. Needs a Discord guild and the maintainer's own application token, so it is run by hand and recorded

## 17.143 Crash and error groups linked to issues and fix releases

**Goal:** An operator who meets a bug learns in one click whether the project already knows about it and whether an update fixes it, and the maintainer gets one issue per fault instead of a pile of duplicates.

**Size:** M. **Depends on:** 17.17, 17.83, 17.105, 17.106

**Deliverables**

- A stable fingerprint string for every 17.106 error group and 17.83 crash group, derived from the grouping key and carrying no path from the operator's machine, no message text and no identity, so the same fault gives the same string on every installation
- A Track button on each group, working only once the operator turns issue lookups on, off by default, which searches the repository's issue tracker for the fingerprint through the outbound HTTP client, with no token for a public repository, and shows each match with its state; the repository defaults to the one the release came from and can be changed for a fork
- With no match, a prefilled new-issue form opened in the browser carrying the fingerprint, build, source location and the fields of 17.106's report the operator chose, sending nothing until the operator submits it there; with the operator's own token sealed in the keyring, the panel can create the issue itself after showing exactly what it will send
- A tracked group showing "tracked in #N" with the issue's state, refreshed on 17.17's channel check interval, and once the issue is closed, the first release tag that contains the fix with "fixed in vX, you run vY" and a link to 17.17's update page
- A group seen again on a build that already contains its fix marked as a regression on the group and on its page
- Every lookup, link and created issue audited, and no request to any host while issue lookups are off

**Acceptance**

- [ ] The same fault logged on two installations with different install paths gives the same fingerprint, and the fingerprint holds no path, message text or account name, proved by a unit test
- [ ] With issue lookups off, the error and crash pages make no request to any other host, proved by a test that records outbound requests
- [ ] Against a recorded issue search, a group whose fingerprint is in an open issue shows "tracked in" with its number and state, and one with no match offers the prefilled form holding the fingerprint and build
- [ ] Creating an issue with a token shows the exact body first, sends that body and nothing more, and writes an audit row
- [ ] Given a closed issue and a list of tags, the panel names the first tag that contains the fix, and a group seen on a later build is marked as a regression, proved by a unit test over recorded answers
- [ ] Dev-gated: against the project's public repository, a Track lookup finds a real issue by its fingerprint. Needs network access to the live issue tracker, so it is run by hand and recorded

## 17.144 Feature flags with targeting and staged rollout

**Goal:** A subsystem that is new in a build reaches game masters first, then a few percent of players, then everyone, and steps back by itself if errors or crashes appear, instead of the only choices being a whole release or a kill switch.

**Size:** M. **Depends on:** 17.12, 17.67, 17.81, 17.83, 17.106, 4.16

**Deliverables**

- Flags declared in server code the way 4.16 declares settings, each with a name, one sentence, its owning milestone and a default of off, in the same registry, so the build reports them through `GET /api/capabilities` and a flag the build does not declare cannot be set
- A rule per flag: off, on, accounts at game master level and above, a list of accounts, named realms, or a sticky percentage of accounts chosen by a hash of the account id and the flag's name, so the same player stays in or out as the percentage grows; rules validate, persist, audit and reach the apps through 17.12's settings API, applying live
- Evaluation in the app as one lookup in a snapshot swapped atomically when a rule changes, with no allocation and no lock on the path that asks
- A Flags page listing every flag with its rule, who changed it and when, and the share of online players it covers now, each change needing a reason
- Rollout plans, such as 5 percent for a day, then 25, then everyone: a plan advances only while no new 17.106 error group or 17.83 crash group appears on a build carrying the flag, no 17.81 probe fails and no 17.67 alert fires in its scope, and otherwise pauses and steps back to its previous stage by itself, recording why
- A stale report: flags fully on for a set time named on the page, and a repository check in the checks job listing flags whose owning milestone has every acceptance check ticked, so a finished flag is removed rather than kept

**Acceptance**

- [ ] A flag at 10 percent covers the same accounts on every evaluation and across a restart, and raising it to 25 percent keeps every account the 10 percent covered, proved by a unit test over a seeded set of accounts
- [ ] A game master rule is on for a game master account and off for a player account, and a named realm rule is on only on that realm
- [ ] Changing a rule applies to the running app without a restart and writes an audit row with the reason
- [ ] A rollout plan whose next stage is due does not advance while a new crash group is open, steps back to its previous stage, and records that crash group as the reason
- [ ] Evaluating a flag allocates nothing and stays inside a stated budget in a benchmark in the test suite
- [ ] Setting a flag the build does not declare is refused naming it, and the repository check lists a flag whose milestone is complete

## 17.145 MCP server for AI assistants

**Goal:** An operator gives an AI assistant a least-privilege, audited way into their server, so it reads error groups and logs directly instead of the operator pasting them, and acts only as far as its key and a confirmation allow.

**Size:** M. **Depends on:** 17.36, 17.48, 17.80, 17.84, 17.100, 17.106

**Deliverables**

- `ambrosectl mcp`, a Model Context Protocol server over standard input and output that the assistant's client starts, authenticated with a personal API key from 17.36, opening no listener and holding no credential but that key
- Read tools by default: app status and problem records, log search over 17.80, error and crash groups, settings with secret values masked, and activity and schedule history; error report files from 17.106 and support bundle manifests from 17.84 offered as resources; each answered only as far as the key's rights reach
- Tool schemas generated from the route table 17.100 generates its commands from, so a tool exists only for a route the build has, and a test fails when a route marked for assistants has no tool
- Write tools only when the server is started with the flag that allows them and the key holds the permission, each carrying read-only or destructive annotations; a destructive call first returns what it would change and a single-use confirmation token valid for a minute, and only a second call carrying that token acts, so the person approving tool calls in their client sees the effect before it happens
- No action that needs a step-up check exposed as a tool, whatever the key holds
- Every call audited as the API key with the client's name from its initialize request beside it, refusals included, so the activity log tells an assistant's actions from a script's

**Acceptance**

- [ ] A client listing tools with a viewer's key sees only read tools, and a write reached through any tool is refused with 403 naming the permission, proved by an end-to-end run with a scripted client
- [ ] A destructive call without its confirmation token changes nothing and returns what it would change, and the token works once and expires after a minute
- [ ] No step-up action appears in the tool list for an owner's key
- [ ] Every call writes an audit row naming the key and the client, and so does every refused call
- [ ] A secret setting read through the settings tool is masked for a key without `settings.secrets.read`
- [ ] The server opens no listening socket while it runs, proved by a test that lists the process's sockets

## 17.146 OpenAPI description and API explorer

**Goal:** Anyone building a bot, a site or a tool around their server reads one stable, documented contract for the panel API, tries it from the panel, and takes a generated client for their language from the release.

**Size:** M. **Depends on:** 17.26, 17.36, 17.48, 17.100, 17.105

**Deliverables**

- `GET /api/panel/openapi.json`, an OpenAPI 3.1 description generated from the route registry and the C++ schemas 17.26 turns into TypeScript, with each operation's permission key, scope, danger flag, confirmation flag, rate-limit cost and error shapes, served to a signed-in user or an API key and listing only the operations the caller holds
- An API page in the dashboard built from `@ambrose/ui` components rather than a third-party API viewer, so it follows doc/DESIGN.md and the panel's strict Content-Security-Policy like every other page: operations grouped by permission group, each with its schema and errors, a try-it form for read routes that runs with the caller's own session, and copyable curl, `ambrosectl` and Python snippets
- A unit test holding the committed description of the previous release, which fails when an operation or a field in it is removed or renamed, so fields are only ever added, as 17.03 and 17.26 already hold for their own shapes
- Python and TypeScript clients generated from the description in 17.105's release job and published beside the release's artefacts with their SHA-256, never shipped inside the panel; the generator is openapi-generator, under Apache-2.0, run only in that job. Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md
- Danger routes marked in the description and on the page, and try-it offered only for read routes, so nothing the explorer does changes the installation

**Acceptance**

- [ ] The served description validates as OpenAPI 3.1, and a test fails when a route in the registry is missing from it or lacks its permission
- [ ] A viewer's description and API page list no route they cannot call
- [ ] Removing a field from a response schema fails the compatibility test, and adding one passes it
- [ ] The API page makes no request to another host and passes the accessibility gate, proved by a browser test
- [ ] A try-it call answers with the caller's own permissions, and no write route offers try-it
- [ ] A release carries the generated Python and TypeScript clients, and the Python client reads an app's status from a running supervisor with an API key

## 17.147 Sandboxed operator scripts

**Goal:** An operator writes the rule a fixed task cannot express, such as restarting only the realms over their tick budget that hold fewer than five players, one at a time, and runs it as a button or a scheduled task under the panel's own permissions, with no outside machine holding a key.

**Size:** L. **Depends on:** 17.15, 17.28, 17.48, 17.53, 17.87, 17.110

**Deliverables**

- Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md: sandboxed Lua scripts on the terms below, opt-in and off by default through `Panel.Scripts.Enable`. It is not the browser shell the Operations depth note refused: a script has no shell, no file system and no network, only the audited `ambrose.*` calls it declared
- A Scripts page, owner-only by default through new `scripts.edit` and `scripts.run` permissions, where a short Lua script is written in 17.53's editor with typed parameters and declared capabilities, such as reading status, restarting an app, announcing or taking a backup, each a sentence an owner can judge
- Each run in a worker process separate from the supervisor, on the Lua runtime Decisions, Experimental features settles, added through vcpkg by whichever of 13.11 and this milestone lands first, with the `io`, `os`, `package` and `debug` libraries and every loader removed, and caps on memory, instructions and wall time enforced under 17.28's job object on Windows or cgroup on Linux
- One API, `ambrose.*`, generated from the route table, where each call is checked against the script's declared capabilities intersected with its author's permissions at the moment it runs, so demoting the author shrinks every script at once, as it shrinks their API keys
- Every call audited as the script, with its version and the author whose rights it used, the way 17.110 audits a panel tool's calls
- A dry run, the default for a new version, that answers read calls and records each write call it would make without making it, shown as a list before the first real run
- Running as a button with its parameters, or as a task in a 17.15 schedule, a 17.87 trigger or, once 17.149 lands, an inbound hook, under the rule that arming a task needs every capability the script declares
- Every saved version kept with who saved it and when, a difference between versions, and a run history with each run's parameters, calls, capped output and result

**Acceptance**

- [ ] A script calling `os.execute`, `io.open` or `require` fails with the name undefined, and one that loops forever stops at its instruction cap with the cap named in its run record
- [ ] A script whose worker passes its memory cap is ended by the job object or cgroup and recorded, and the supervisor keeps serving
- [ ] A script that declares only reading status is refused when it calls restart, and a script whose author has lost `power.restart` is refused the restart it made before
- [ ] A dry run changes nothing and lists each write call the real run then makes
- [ ] A script scheduled through 17.15 runs at its time, and every call it made appears in the activity log naming the script, its version and its author
- [ ] A user without `scripts.edit` cannot save a script, and saving a new version keeps the previous one restorable with its difference
- [ ] Arming a trigger that runs a script is refused when the arming user lacks a capability the script declares

## 17.148 Installation file synced from Git

**Goal:** An operator keeps the installation file in a Git repository, reviews changes there, and the panel pulls and applies them, even on a home machine that nothing on the internet can reach.

**Size:** M. **Depends on:** 17.15, 17.17, 17.36, 17.67, 17.99, 17.149

**Deliverables**

- A sync source an owner sets on the installation file page: a repository, a branch, a folder and the file's path inside it, a poll interval, and manual or auto mode, with a read-only token for HTTPS or a deploy key for SSH sealed in the keyring and never shown back. The panel fetches through the one Git client the supervisor carries, libgit2 from vcpkg, which 17.17's build channel brings and whose choice is settled under Panel operations in doc/ARCHITECTURE.md, and only ever pulls, so it needs no inbound connection and works behind NAT
- A sync status of Synced, Out of sync, Drifted or Unknown, computed with 17.99's difference between the file at the branch head, the file at the last applied commit and an export of what is running, and shown with the commit, its author, message and time; a repository that cannot be reached reads Unknown with the time of the last successful fetch, never Synced. Drift here means the running installation differs from the file in Git; files and schemas changed outside the panel are 17.118's
- Manual mode offering Apply on the pending commit, run through 17.99's apply under the caller's own permissions after the difference is shown; auto mode applying each commit that validates as a 17.36 personal API key the owner creates for it, so its rights are the key's subset intersected with the owner's current permissions, and every audit row the apply writes carries the commit hash and the key id
- A commit that fails validation or a permission check held rather than applied, with the failing field or permission and the commit named on the page, the installation left at the last applied commit, and a held-commit condition offered to 17.67's alert rules
- Changes made in the panel since the last applied commit listed as drift, key by key with who made each from the audit log, and a Propose control that pushes a branch and opens a pull request through the Git host's API when the owner gave a separate write token, or otherwise downloads the change as a patch against the synced commit
- A sync task on the 17.15 engine, so a schedule polls at its own interval and a 17.149 inbound hook starts a sync when the host reports a push; every source change, fetch, apply, hold and proposal is audited, under permission keys of their own in the 17.48 catalog

**Acceptance**

- [ ] With a local repository as the source, a commit that changes one value reads Out of sync with a difference naming that value, and Apply changes that value and nothing else, proved by an end-to-end run with the panel bound to loopback and no inbound connection
- [ ] In auto mode a commit naming a field the build does not know is held with the field and the commit named, and the running installation is unchanged, proved by a unit test
- [ ] Every audit row written by an auto apply carries the commit hash and the key id, and after the owner loses a permission the next commit needing it is held naming that permission, proved by a unit test
- [ ] A setting changed in the panel after a sync reads Drifted with that key and who changed it, and with no write token Propose downloads a patch that applies cleanly to the synced commit, proved by a unit test that applies it with git
- [ ] With the repository unreachable the status reads Unknown with the time of the last successful fetch, never Synced, proved by a unit test
- [ ] Neither token nor the deploy key appears in any response, log line, audit row or exported file, proved by a response shaping test
- [ ] Dev-gated: Propose opens a pull request on an outside Git host with the owner's own write token. Needs an account on an outside Git host, so it is run by hand and recorded

## 17.149 Inbound webhooks that start a task chain

**Goal:** The operator's own CI, Git host or community bot can start one known, safe action on the server without holding a general API key.

**Size:** M. **Depends on:** 17.14, 17.15, 17.87

**Deliverables**

- Hooks on the automation page, each bound to exactly one task chain on the 17.15 engine, such as a backup then an update from the build channel, a world data reload or an announcement, with a URL carrying an unguessable id and a secret shown once in a dialog that cannot be dismissed from outside and then kept sealed in the keyring
- A verifier chosen per hook: a GitHub signature, a Gitea or Forgejo signature, or a timestamped HMAC documented for any sender, with a replay window that refuses a timestamp outside it and a delivery id already seen inside it
- Deliveries posted to the panel listener at a path of their own that reads no cookie and no session, with a rate limit and a size cap per hook inside the listener's request bound, checked in the order size, signature in constant time, replay window, then filters such as the ref equalling `refs/heads/main`; every refusal answers the same way, runs nothing, and is recorded with its reason. A hook is reachable only where the panel listener already is, and nothing is opened for it
- A passing delivery running its chain as a 17.87 trigger of its own kind, under that milestone's cooldown, hourly cap and loop guard and in the run record schedules keep, with rights that are the hook's permission set intersected with its creator's permissions at the moment it runs, so a run whose creator lost a permission a task needs is skipped as permission revoked
- Payload values reaching tasks only as typed variables the hook declares, each read by a JSON pointer and checked against a pattern, such as a commit hash of forty hex digits, and passed as a task's parameter, never spliced into command text
- A deliveries tab listing each request with its time, source address, verdict, reason and the run it started, kept for a retention that is a live setting, with redeliver running a stored delivery once more as the operator's own audited action through the same signature and filter checks
- Creating, changing, rotating the secret of and deleting a hook each audited, under hook permissions of their own in the 17.48 catalog, and creating one needing every permission its chain's tasks need, as 17.15 requires of a schedule

**Acceptance**

- [ ] A delivery with a valid GitHub signature for `refs/heads/main` runs the bound chain once, and the same delivery sent again inside the window is refused as a replay, proved by a unit test over a recorded delivery
- [ ] A wrong signature, a timestamp outside the window, a body over the cap and a ref the filter refuses each run nothing, answer alike and appear on the deliveries tab with their reasons, proved by a unit test
- [ ] After the creator loses a permission one of the chain's tasks needs, the next delivery is skipped as permission revoked naming it, and the skip is audited, proved by a unit test
- [ ] A variable whose value fails its pattern runs nothing, and a value carrying shell metacharacters that passes a loose pattern reaches its task as one parameter and never as command text, proved by a unit test
- [ ] The secret is shown once, no response returns it again, and the store holds it only sealed, proved by a response shaping test
- [ ] A flood of deliveries to one hook is rate limited while the rest of the panel keeps answering, and 17.87's cooldown holds a second valid delivery inside it, proved by a unit test
- [ ] Redelivering from the tab runs the stored delivery once more as the operator's audited action and links the new run, proved by a browser test

## 17.150 Getting-started checklist and page tours

**Goal:** A new owner learns what the first start already did for them and which few decisions are theirs, and the panel ticks each one off as it sees it done.

**Size:** M. **Depends on:** 3.22, 17.15, 17.16, 17.21, 17.37, 17.47, 17.82, 17.86, 17.108

**Deliverables**

- A first-start record: 3.22's automatic setup publishes what it did as structured facts on each app's status, which install it chose and why, its revision, whether the type dump was built or reused and where, how many name tables it extracted, and the file each choice was saved to, and the supervisor keeps the first start's facts in its store so they outlive the log files
- A Getting started card on the overview for owners, shown after the `/first-run` claim until an owner dismisses it, which is audited and can be undone from the account menu, opening with that record in plain sentences before listing what is left to decide; it adds no step to the first run, and nothing waits on it
- The decisions left to the owner, each linking to the page that makes it: turn on two-factor sign-in (17.47), invite a teammate or record that you work alone (17.37), take and verify a first backup (17.16), schedule a nightly one (17.15), add an alert channel and send a test (17.86), decide how the panel is reached (17.108), and create a game account and sign a client in with it (17.21)
- Each step ticked from observed state rather than from a click, by reading a check in 17.82's registry and adding the checks the registry lacks, so the card and the health page read one figure and cannot disagree; a step can be skipped with a reason that is audited and can be reopened, and a step the viewer cannot do names who can
- Short tours on the busier pages, offered once per user on a first visit and remembered in the store rather than the browser, operable by keyboard with Escape ending them, following reduced motion, built on the design system's own popover with no tour library, and replayable from the page and from the help drawer once 17.151 lands
- A test that renders each page with a tour and fails when a step's anchor is no longer on the page, and one that fails when a checklist step links to a route the route table does not hold

**Acceptance**

- [ ] On a fresh installation the card names the install, the revision, the type dump's path and the count of name tables 3.22 reported, and still does after the log files are removed, proved by an end-to-end run against a supervisor with a synthetic install
- [ ] Taking and verifying a backup ticks its step, and the health page reads the same state from the same check, proved by a unit test that drives both from one check
- [ ] Skipping a step records the reason and who skipped it in the audit log, and a skipped step can be reopened, proved by a browser test
- [ ] A step whose subject is not built reads as unavailable naming its milestone, never as done, proved by a browser test
- [ ] A tour runs from its first step to its last by keyboard alone, Escape ends it, and it passes the accessibility gate, proved by a browser test
- [ ] Removing an element a tour step points at fails the anchor test, and so does a step linking to a route the table lacks, proved by running both tests against a broken fixture

## 17.151 Built-in help, offline docs and what's new

**Goal:** Every page explains itself from documentation built into the panel, which matches the running build and works with no network, and every operator learns what changed after an update.

**Size:** M. **Depends on:** 17.06, 17.13, 17.37, 17.88, 17.105

**Deliverables**

- The operator documentation built into the dashboard at build time as static pages: the option rows in doc/config, the guides in doc/guides and one page of help per route kept beside those guides, so the help a build shows was written for that build and needs no network; the route table gains the help section each route opens
- One Markdown renderer for the panel, markdown-it under MIT with its output passed through DOMPurify. Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md, since doc/UI-STACK.md named none: used at build time for the documentation and in the browser for the operator-written text later milestones show, with raw HTML off, links limited to http, https and the panel's own routes, and images only from the panel
- A help drawer on every page opened by F1 or by ?, registered in 17.88's shortcut registry so the sheet lists it and it never fires inside a field, showing the current route's section and keeping links between help pages inside the drawer
- Help in context: each setting on the 17.13 settings page opens its option's row, each problem code from 17.03 and each error a page shows opens its explanation, and each permission in 17.37's grant editor opens what it allows, beside the catalog's one-line description
- Search over the documentation from an index the build step writes, run in the browser with no search library and offered as a source in 17.88's palette
- A What's new sheet each operator sees once after the version they run changes, holding the release notes 17.105 generates for that version as built into the dashboard, with each operator's dismissal kept in the store, and a New mark for two weeks on a page whose route names the version that added it
- External links written as plain text unless their address is on the allow list the bundle check reads, and opened with no referrer, so the built dashboard still names no host it was not allowed to; a check fails when a route has no help section, when a help link names a section that does not exist, or when a problem code or a permission has no explanation

**Acceptance**

- [ ] With the network blocked, F1 and ? open the current page's help, and its text is the text built into that build, proved by an end-to-end run
- [ ] Adding a route without a help section fails the check, and so does a help link to a section that does not exist, proved by the checks job
- [ ] The help control beside a setting opens that option's row, and the one beside a problem code opens its explanation, proved by a browser test
- [ ] A word from the body of a guide finds it from the palette and opens it in the drawer, proved by a browser test
- [ ] After the version changes, each operator sees What's new once, a second sign-in does not show it again, and another operator still sees it, proved by a unit test over the store
- [ ] A help page holding raw HTML, a script link and a remote image renders them as text or drops them, and the built bundle passes the check that it names no other host, proved by a browser test and the checks job
- [ ] A page added in the running version carries its New mark until two weeks after that version, proved by a unit test with a fixed clock

## 17.152 Custom boards and a paired wall display

**Goal:** A team builds the views it watches from widgets the panel already renders, shares them by role, and puts one on a television that can see nothing else.

**Size:** L. **Depends on:** 17.06, 17.14, 17.19, 17.48, 17.58, 17.73, 17.99

**Deliverables**

- A Boards page where a user creates, names and removes boards, each a grid of widgets kept in the supervisor's store, beside 17.06's overview, which stays one fixed layout for every installation
- A widget catalog over data the panel already serves, each type registering its data source and the permission it needs: an app card, any 17.19 series, a stat tile with thresholds, a filtered alert list, the next schedule runs, backup freshness, health findings, players per realm, the top error groups and a filtered log tail; a type whose milestone has not landed is offered as unavailable naming it
- Placing and resizing widgets by drag and by keyboard, where a focused widget is picked up with Enter, moved or resized with the arrow keys and put down or cancelled with Escape, snapping to the grid, stacking in reading order at phone width, and built from the design system's components with no layout library
- Each widget's data checked on the server against the viewer's permissions, a widget the viewer may not see showing the denied empty that names the permission rather than an empty one, and live widgets subscribed on the one socket only while visible, each figure carrying the age of its sample
- A board kept personal or shared with a role, edited only by holders of the board permission at that scope, and chosen by a user as their home in place of the overview
- Export and import of shared boards through 17.99's installation file, validated against the board schema, refusing an unknown widget type or field by name, and holding layout and filters but never data or an identity
- A wall display mode showing chosen boards full screen in large type, rotating through them at a set interval with no animation under reduced motion, with the connection state and each figure's age shown large enough to read from across a room, so a stale figure is never mistaken for a current one
- Pairing a television's browser once: opening the display path shows a short code, and an owner or a holder of the display permission confirms that code on the Displays page and chooses its boards; a code works once, expires in minutes and is throttled against guessing
- A display session of its own kind, since a 17.36 key is refused on a cookie request and a person's session would carry their rights: read-only, limited to its boards' widget data, listed with its last address and time, expiring, revocable at once and audited, and never sent an identity, with player names, account names, addresses and operator names left out of every widget it reads

**Acceptance**

- [ ] A widget over data the viewer may not see shows the denied state naming the permission, and its data never reaches the browser, proved by a browser test that reads the socket frames
- [ ] A board is built, moved and resized by keyboard alone and passes the accessibility gate, proved by a browser test
- [ ] A shared board exported with the installation file and applied to a fresh installation gives the same layout, and a file naming an unknown widget type is refused naming it, proved by a unit test
- [ ] A paired display shows only its boards, is refused on every other route and socket subscription, and loses its boards within one second of being revoked, proved by an end-to-end run
- [ ] No response or socket frame sent to a display session carries a player name, account name, address or operator name, proved by a response shaping test
- [ ] A pairing code works once, is refused after its expiry, and repeated wrong codes are throttled, proved by a unit test
- [ ] With the stream closed, every figure on the wall display turns visibly stale with its sample's age, proved by an end-to-end run

## 17.153 High-contrast and color-vision themes, and display preferences

**Goal:** An operator who needs more contrast, or who cannot tell some hues apart, gets a panel built for them from the same tokens, and every operator chooses how times are shown.

**Size:** M. **Depends on:** 17.38, 17.66, 17.73

**Deliverables**

- High-contrast dark and light modes in `design/tokens.json` as remaps of the semantic tier, with text at 7:1 or better on every surface it sits on, control edges at 4.5:1 and a wider focus ring, chosen automatically under `prefers-contrast: more` when a user's choice is the system's, while forced-colors mode stays supported as doc/DESIGN.md describes
- Color-vision modes over either theme, one for red-green deficiency, protanopia and deuteranopia, and one for tritanopia, that remap the state accents, the value ramp and the series ramp so no two meanings collide, removing the collisions doc/DESIGN.md accepts today for the value ramp, green and salmon under deuteranopia and blue and green under tritanopia
- `apps/designtokens/designtokens.py` generating the modes with gates of their own: a 7:1 text gate and a 4.5:1 control edge gate for high contrast, and for each color-vision mode the simulation the series ramp gate already runs, extended to hold every pair of state accents, value kinds and series slots a set distance apart, refusing to write a file when one falls short and naming the pair, the mode and the distance; doc/DESIGN.md gains the generated tables for each mode
- Dash patterns and point shapes per series slot from the tokens, which a user can turn on so a multi-series chart reads without hue, with every chart's table view unchanged
- A Display tab on 17.38's account page holding the theme, the contrast and color-vision modes, the density and the time display: the browser's zone, UTC or the node's zone, 12 or 24 hours, and relative or absolute time first, stored with the user's profile so the choice follows them to another browser rather than living in one browser's storage as the theme does today, and applied through the one formatter with the zone named as 17.66 requires
- The end-to-end suite running the accessibility gate over every real page in every theme and mode, not only over the component stories, and the screenshot job covering each mode

**Acceptance**

- [ ] Lowering one high-contrast text token until a pair falls under 7:1 makes the generator refuse naming the pair, proved by the generator's own tests
- [ ] Moving two state accents together in a color-vision mode until they collide under its simulation makes the generator refuse naming both and the mode, proved by the generator's own tests
- [ ] With `prefers-contrast: more` emulated and the choice left on the system, the panel renders high contrast, and an explicit choice wins over it, proved by a browser test
- [ ] Every real page passes the accessibility gate in every theme and mode, proved by the end-to-end run
- [ ] Choosing UTC shows every absolute time in UTC with the zone named, and the choice holds when the same user signs in from another browser, proved by an end-to-end run
- [ ] With patterns on, every series in a multi-series chart carries a different dash or point shape and its table view is unchanged, proved by a browser test on the design system's chart component

## 17.154 Installable panel app, OS notifications and opt-in Web Push

**Goal:** An operator installs the panel as an app of its own and hears about an alert routed to them while its window is in the background, and a phone can be woken for one only if the owner opts in.

**Size:** M. **Depends on:** 17.06, 17.67, 17.86, 17.108

**Deliverables**

- A web app manifest and icons from the design system in the dashboard's build, with shortcuts to Overview, Alerts and Console, so the panel installs from a Chromium browser on Windows, Linux or Android into a window of its own wherever it is served in a secure context, which 17.108 gives on loopback and with a trusted certificate
- A service worker caching only the content-hashed static build and the shell page, never an API answer and never socket data, so the rule that the browser holds no server data stands; a new build replaces it with a prompt to reload, and the Content-Security-Policy gains only a same-origin worker and manifest source
- An unreachable screen the cached shell shows when the supervisor does not answer, saying the panel has been unreachable since a named time and retrying with the attempt and a countdown, in the connection states doc/DESIGN.md sets, holding no figure from before
- A this-browser channel in 17.86's per-user channels, raising an OS notification for each alert routed to that operator while the app is open or in the background, asked for on an explicit click and never on load, carrying only what 17.86's payload rule allows and opening the alert when clicked
- The count of unacknowledged alerts routed to that operator on the app's taskbar or dock badge where the browser supports badging, cleared as they are acknowledged
- Settled on 2026-09-27 at the maintainer's direction to take the recommended option, and recorded under Panel operations in doc/ARCHITECTURE.md, reopening the browser push the Operations depth note declined as an opt-in: Web Push as its own opt-in, `Panel.WebPush.Enable`, off by default, with VAPID keys (RFC 8292) the supervisor generates into the keyring, messages encrypted to RFC 8291 through Botan and sent by the settled outbound HTTP client, and a payload that is only a wake-up with no subject, figure or identity, after which the device fetches the details from the panel itself; each subscribed device is listed on the account page with revoke, and the settings page says which company runs a device's push endpoint and that it learns only that a message was sent and when
- Everything except Web Push working with no outside service, and a panel that is never installed working exactly as before

**Acceptance**

- [ ] The built panel meets Chromium's installability criteria when served on loopback, with a valid manifest and a service worker controlling the page, proved by an end-to-end run
- [ ] After signing in and opening every page, no cache the service worker holds contains an API answer or socket data, proved by an end-to-end run that lists every cache entry
- [ ] With the supervisor stopped, the installed app opens to the unreachable screen naming the time and retrying, and shows no figure from before, proved by an end-to-end run
- [ ] An alert routed to the operator raises one OS notification and sets the badge to the unacknowledged count, and acknowledging it clears the badge, proved by a browser test with the notification and badging interfaces stubbed
- [ ] With Web Push off no request goes to any push endpoint, and with it on a message decrypts, with the subscription's own keys, to a wake-up carrying no subject, figure, name or address, proved by a unit test that also passes the RFC 8291 test vectors
- [ ] Dev-gated: an installed panel on an Android phone is woken while closed and shows the alert's details only after fetching them from the panel. Needs a phone and an outside push endpoint, so it is run by hand and recorded

## 17.155 Saved views, stars, recents and wider search

**Goal:** An operator returns to the same filtered view in one click, sends a teammate a link that opens it under the teammate's own rights, and finds any panel object by name.

**Size:** M. **Depends on:** 17.06, 17.21, 17.25, 17.80, 17.88

**Deliverables**

- Every list page keeping its filters, sort, columns and time range in its address, so a copied link opens the same view under the recipient's own rights: activity, logs and log search, backups, alerts, schedules and runs, files, crash and error groups, users and registrations, each list page adopting it as it lands, with a test over the route table's list pages that fails when one does not bring its state back from its address
- Filter values that could name a person, such as an account, a username, an email or an address, kept on the server behind an opaque id rather than written into the address, the id resolving only for a caller allowed to see what it stands for
- Views saved by name, personal or shared with a role, and made a page's default for a user or for a role, kept in the supervisor's store; a shared view opens under each viewer's own rights, and a view naming a filter its page no longer has says so rather than opening wrong
- Stars on any app, realm, node, schedule, backup, setting key, alert rule or file, each kind as its milestone lands, gathered in a Starred section of the side bar, with a starred object the user can no longer see leaving the list rather than showing as a broken link
- A Recent list of the objects and pages each user opened, kept in the store so it follows them between the desktop and a phone, and clearable by its owner
- One search registry that 17.21's top-bar search and 17.88's palette both read, which reach accounts, apps, routes and page actions, and characters once 17.177 lands, extended to settings, schedules, backups, alert rules, crash and error groups, panel users and saved views, each kind registering its own provider as its milestone lands and every result filtered by the caller's permissions on the server; shared views are audited when created, shared and removed, while stars and recents stay a user's own

**Acceptance**

- [ ] A filtered, sorted activity page's address opened by another user shows the same filters and sort with only the rows that user may see, proved by an end-to-end run
- [ ] No address the panel writes carries an account name, username, email or address, proved by a browser test over every list page with such a filter, and an opaque id resolves to nothing for a caller who may not see it, proved by a unit test
- [ ] A view shared with a role appears for a member of that role and for nobody else, and made the page's default it opens that page with its filters, proved by a browser test
- [ ] A star added in one browser appears in the same user's side bar in another, and a starred app the user loses access to leaves the list, proved by an end-to-end run
- [ ] The palette finds a saved view and a panel user by name, and never one the caller may not see, proved by a browser test for a user whose grants narrowed while the page was open
- [ ] A list page that does not bring its state back from its address fails the route table test

## 17.156 Undo from the activity record, and bulk actions

**Goal:** A wrong change made late at night is undone in one step from where it was made or from its activity row, and routine work across many rows is one checked action instead of dozens.

**Size:** L. **Depends on:** 17.13, 17.25, 17.37, 17.48, 17.49, 17.53, 17.54, 17.55, 17.68, 17.88

**Deliverables**

- An inverse in 17.25's event catalog for every change that has one, registered by the milestone that owns the change and resting on the before value its audit row already keeps: a setting change (17.13), a grant change (17.37), a file save through 17.53's versions, a file delete through 17.55's trash and a schedule edit (17.68), with a role change, an app disable, a silence and a backup pin joining as 17.50, 17.27, 17.85 and 17.52 land; a test fails when an event marked reversible has no inverse
- Undo as a new action rather than an erasure: it runs through the same route and permission check the change itself needs, is audited as its own event naming the one it undoes, marks that event undone with who and when, and cannot run twice
- A refusal whenever the subject changed after the event, compared by its version or entity tag, showing the change that came since and changing nothing, so an undo never overwrites a later change
- Undo offered on the activity row and inline where the action was taken, as a line under the control rather than in a toast, since doc/DESIGN.md keeps a toast from being the only record, for a window that is a live setting
- Ctrl+Z undoing the operator's own last change on the current page after a confirmation naming it, bound through 17.88's shortcut registry so it never fires while focus is in a field, and never reaching into the file editor, which keeps its own undo
- A My recent changes panel listing today's changes by the operator and, for each, whether it can still be undone and why not
- Row selection on every table, a page at a time or every row matching the filter across pages with the count named, surviving paging and cleared when the filter changes
- A bulk bar offering only the actions the caller holds on every selected row, computed on the server; the server preflights each item's permission, state and version and shows a verdict per item before anything changes, applies under one audit batch through 17.49's scope with a result per item, keeps 17.54's rule that a file batch is validated whole before any change, and asks once for the confirmation the single action asks for
- A bulk action undone as one from its batch row, each item's inverse checked on its own, with the items that changed since listed and left as they are

**Acceptance**

- [ ] Undoing a setting change restores the old value through the settings API, writes its own audit row naming the original, and marks the original undone, proved by a unit test
- [ ] Undoing a setting that was changed again since is refused with both values shown and changes nothing, proved by a unit test
- [ ] Undoing a file delete restores it from the trash, and undoing a save restores the previous version with its hash, proved by a unit test
- [ ] A caller who no longer holds the permission the inverse needs is offered no Undo and the route refuses it, proved by a unit test and a browser test
- [ ] Selecting every schedule matching a filter across three pages and pausing them shows a verdict per schedule, applies under one audit batch with a result for each, and undoing the batch resumes exactly those it paused, proved by an end-to-end run
- [ ] The bulk bar offers an action only when the caller holds it on every selected row, proved by a browser test over a selection that mixes apps the caller may and may not restart
- [ ] Ctrl+Z inside a field does nothing to the panel, and outside one asks before undoing the operator's own last change on the page, proved by a browser test

## 17.157 Printable operations reports

**Goal:** An owner answers how last month went with one document built from the panel's own figures, printed or saved as a PDF by the browser, or written and mailed on a schedule.

**Size:** M. **Depends on:** 17.15, 17.16, 17.17, 17.18, 17.25, 17.35, 17.67, 17.94, 17.97, 17.98

**Deliverables**

- A Reports page building a document for a week, a month or a chosen range in a chosen zone, over the whole installation, a realm or a node, with its figures read by the same queries the pages they come from and the 17.97 digest use, so the three cannot disagree
- Sections for availability per realm and for the login path with the incidents and their notes from 17.94, crashes by group once 17.83 lands, backups taken, verified and restored, updates applied and rolled back from 17.17, a summary of who changed what by operator and by area from the audit store, alerts with their time to acknowledge, capacity headroom and forecasts from 17.98, and open findings once 17.82 lands; a section whose milestone has not landed says unavailable naming it, never zero
- A layout for paper: a print stylesheet with page breaks between sections, table rows that never split and headers that repeat, a header and footer naming the installation, the period and the page, charts printed as markup with their table view, and meaning carried by words so a grey printout loses nothing
- Saving as a PDF through the browser's own print dialog, with no PDF library in the dashboard or the supervisor
- A report task on the 17.15 engine writing the same document as one self-contained HTML file, with no script and its stylesheet and fonts inline, into a reports root under 17.18's jail with a retention of its own, and mailing it through 17.35's mail settings to chosen panel users
- A report carrying only the sections its viewer may see, a scheduled one mailed only to users who hold every permission its sections need, and no report carrying a secret value or a player's name, email or address; creating and scheduling reports audited under permission keys of their own
- A print stylesheet on every other page, hiding navigation and controls and saying when a long list printed only the rows it had loaded

**Acceptance**

- [ ] A month's report shows the same availability, backup and alert figures their pages show for that month, proved by a unit test building both from one seeded month
- [ ] Printing the report to PDF in Chromium gives pages whose header and footer name the installation, the period and the page, and no table row is split across two pages, proved by an end-to-end run
- [ ] A section whose milestone has not landed prints as unavailable naming it, never as zero, proved by a browser test
- [ ] A scheduled report writes one HTML file that holds no script and requests nothing from any host, and mails it to its recipients, proved by an end-to-end run with a local mail catcher
- [ ] No report carries a secret value or a player's name, email or address, proved by a response shaping test
- [ ] A user without `activity.read` gets a report without the audit summary, and a scheduled report cannot name a recipient who lacks a permission one of its sections needs, proved by a unit test
- [ ] The overview and a list page print without the side bar or controls, proved by a browser test under print media

## 17.158 Incident workspace and postmortems

**Goal:** When the game falls over, the team runs the incident in one place that gathers what happened as it happens, tells players once, and ends in a written postmortem whose actions are tracked.

**Size:** L. **Depends on:** 17.67, 17.70, 17.78, 17.79, 17.86, 17.94, 17.102, 17.159, 17.164

**Deliverables**

- Declare incident from an alert, from a crash group once 17.83 lands, or from any page's header and the palette, with a title, a severity from a list the owner sets with one sentence each, and a scope of apps, realms and nodes; incidents kept in the supervisor's store with the states declared, investigating, identified, monitoring and resolved
- Commander and communications roles held by panel users who can see the scope, the declaring operator commanding until a hand-over, each hand-over on the timeline and each holder told through their 17.86 channels
- A live timeline drawn from 17.78's events-in-range query over the incident's scope from its start, so audit rows, alerts, crashes, power operations and settings changes, kill switches among them, arrive as they happen on the one socket, merged with hand-typed entries written as 17.159 notes, each entry filtered by the viewer's permissions, and 17.79's correlation offered over the incident's window
- One status box whose update is previewed exactly as it will leave and then posted to 17.70's public page as its incident note and to 17.102's subscribers for incident events, each post audited and held to the payload rules alerts carry
- A declared incident and one 17.94 detects from the probe over the same window joined into one, so the uptime figures and the public page count it once and 17.94's note becomes the incident's
- Resolving opens a postmortem prefilled with the timeline and the impact, the downtime 17.94 recorded, the sessions that dropped, and the times to acknowledge and to resolve, with sections for the cause, what went well, what went badly and the action items
- Action items created as tasks on the 17.164 board, each linked back to the incident with an owner and a due date
- The postmortem kept with its history as draft, reviewed or published to staff, and exported as Markdown holding the timeline, the impact and the action items with their task links
- An incidents list with severity, duration, time to acknowledge and time to resolve and filters over them, with every declaration, role change, status post and postmortem edit audited under permission keys of their own

**Acceptance**

- [ ] Declaring an incident from an alert opens it with that alert, and a settings change and a restart made in its scope afterwards appear on its timeline without a reload, proved by an end-to-end run
- [ ] A timeline entry from an audit row the viewer may not see is left out for that viewer while the rest renders, proved by a unit test
- [ ] One status update reaches the public page and a subscribed webhook signed, matching the preview, with no player name or address in either, proved by a unit test and a response shaping test
- [ ] A probe-detected incident and a declared one over the same window are one incident, and the uptime figures do not count it twice, proved by a unit test
- [ ] Resolving opens a postmortem prefilled with the timeline and the downtime 17.94 recorded for that window, proved by a browser test
- [ ] Each action item becomes a task on the board linked to the incident, and the Markdown export holds the timeline, the impact and the action items with their task links, proved by a unit test
- [ ] The incidents list shows time to acknowledge and time to resolve computed from the timeline of a seeded incident, proved by a unit test

## 17.159 Notes, pinned warnings and graph annotations

**Goal:** The reason a setting has an odd value, or why a realm lives on one node, is written on the thing itself where the next operator will see it, and events outside the server are marked on the graphs they explain.

**Size:** M. **Depends on:** 17.13, 17.25, 17.48, 17.78, 17.79, 17.151

**Deliverables**

- A Notes panel on every object the panel shows, each kind joining as its milestone lands: an app, realm, node, setting key, schedule, backup, alert rule, allocation, database host, file root and panel user, addressed through one object reference of kind and id that later milestones reuse; the notes field 17.29 gives an allocation moves into this panel once both are built, so a note has one place to live
- Each note short Markdown under a size cap, rendered by 17.151's renderer with raw HTML and remote images off, carrying its author, time, edit history and an optional expiry after which it is archived rather than deleted
- Reading a note needing the read permission of the object it sits on, checked on every read like the object's own data, and writing, pinning and deleting another operator's note needing note permissions of their own in the 17.48 catalog
- A note flagged as a pinned warning shown as a banner wherever that object is edited, beside the control it concerns, with its author and time, until it is unpinned or expires, starting with the 17.13 settings editor and joined by each page that edits a kind as it lands
- Annotations drawn on any graph by dragging across a range, or by entering its start and end from the keyboard, with a short text and a scope of an app, a realm or the installation; 17.78 draws them as markers of their own kind beside the automatic ones under the same filter, and 17.79 lists them for the window it correlates
- Every note and annotation created, edited, pinned, unpinned and deleted audited, with its previous text kept in its history

**Acceptance**

- [ ] A note on a setting key is shown to a user holding `settings.read` and refused to one without it, on the page and on the API, proved by a unit test
- [ ] A pinned warning on `Rate.Drop.Item` shows as a banner beside that key in the settings editor and disappears when it is unpinned or its expiry passes, proved by a browser test with a fixed clock
- [ ] An annotation dragged across a range appears as a marker on every graph showing that window, is reachable by keyboard, and is listed by 17.79 for that window, proved by a browser test
- [ ] A note holding raw HTML, a script link and a remote image shows them as text or drops them, and the page requests nothing from another host, proved by a browser test
- [ ] Editing a note keeps the previous text in its history, and every change writes an audit row, proved by a unit test

## 17.160 Operator presence and one socket across tabs

**Goal:** Operators see who else is looking at or editing the same thing before they collide, and a browser with many panel tabs holds one connection instead of one per tab.

**Size:** M. **Depends on:** 17.26, 17.57, 17.58

**Deliverables**

- Presence messages on the 17.26 socket, added as new client and server types under the rule that types and fields are only added and covered by its contract test: a page reports the object it is viewing and, while an editor holds unsaved changes, that it is editing
- Presence held only in the supervisor's memory, never in the store or the audit log, expiring with the socket or after an idle period, and filtered through 17.57 so a viewer learns another operator is on an object only when the viewer may see that object, and which page someone is on only with `users.read`
- The header showing the initials of anyone else on the same app, schedule, file or settings key, the console showing who else is typing to that app, and an editor opened on something another operator has unsaved changes in saying who and since when before work starts, while the 409 on a stale save stays the protection it already is
- An Operators online list of who is signed in and, where the viewer may see it, on which page, with a per-user setting that hides one's own presence from everyone else
- One socket per browser rather than one per tab: tabs elect a leader through the Web Locks API, the leader holds the one 17.26 socket and shares it with the other tabs over a BroadcastChannel, and when the leader closes another tab takes over and resumes by sequence number; a browser lacking either interface falls back to a socket per tab under 17.58's per-user caps, which count every page load today
- Sign-out, theme and locale applied in every tab of the browser at once, and a page open in two tabs warning in each about the other

**Acceptance**

- [ ] Two operators on one app each see the other's initials within one second, and a third who holds nothing on that app sees neither, proved by a unit test and a browser test
- [ ] Opening the settings editor on a key another operator holds unsaved changes in names them and since when, and a stale save still answers 409, proved by an end-to-end run
- [ ] Presence never reaches the store or the audit log, and clears within one idle period after the other operator closes the page, proved by a unit test
- [ ] Twelve tabs of one browser open exactly one socket, and closing the leader hands it to another tab with no missed or repeated records, proved by a browser test counting sockets and sequence numbers
- [ ] Signing out in one tab signs every tab out, and a theme chosen in one applies in all, proved by a browser test
- [ ] A user who hides their presence appears in no other operator's header or online list, proved by a unit test

## 17.161 Change freeze windows

**Goal:** For a launch weekend or an event, the team freezes the kinds of change that could hurt it, and the freeze holds on every path until it ends.

**Size:** M. **Depends on:** 17.15, 17.27, 17.47, 17.48, 17.96

**Deliverables**

- A freeze an owner declares with a name, a start and end in a chosen zone, a reason, a scope of the installation, a realm or an app, and the change classes it blocks, chosen from settings edits, world edits, pack and plugin installs, updates, patch publishes, installation file applies, grant changes and restarts, each class joining as its milestone lands; declaring, changing and ending one early audited, under a permission of its own
- Each mutating route and socket message registered with the change class it belongs to, and a route registry test that fails when a mutating route names none
- Enforcement at 17.48's state step, so a request in a frozen class answers 409 naming the freeze, its reason and its end whatever it came through, the page, an API key, `ambrosectl` or a socket message, checked beside 17.27's protected hours, which stay the daily window they are
- Banners naming the freeze on the controls it blocks, disabled with the reason named, and the freeze shown on the overview while it runs
- Exceptions only through an owner override with a reason and a fresh 17.47 step-up check, audited naming the freeze, or through an approved change request once 17.141 lands
- Scheduled tasks in a frozen class held until the freeze ends and then run within their misfire grace or recorded as skipped for the freeze, unless an owner marked the task exempt, while the supervisor's own restart of a crashed app is never held, because it is not a change
- Freezes on the 17.96 calendar in a meaning color of their own, with a schedule run or a timed event planned inside one marked as a conflict

**Acceptance**

- [ ] With settings edits frozen, a settings change through the page and through a direct API request each answer 409 naming the freeze while reads still answer, and every mutating route and socket message the registry holds is covered, proved by a unit test driven from the route registry
- [ ] A mutating route registered without a change class fails the route registry test
- [ ] An owner override with a reason and a fresh step-up check goes through inside a freeze and is audited naming the freeze and the reason, and one without either is refused, proved by a unit test
- [ ] A scheduled restart inside a restart freeze is held and runs when the freeze ends within its grace, and one marked exempt runs on time, proved by a unit test with a fake clock
- [ ] The supervisor still restarts a crashed app inside a restart freeze, proved by a unit test
- [ ] A freeze appears on the calendar, and a schedule run inside it is marked as a conflict, proved by a browser test

## 17.162 Comment threads, mentions and an inbox

**Goal:** Staff discuss an alert, a crash or a backup on the object itself, under the panel's permissions, and each operator has one inbox of what needs them.

**Size:** M. **Depends on:** 17.26, 17.57, 17.67, 17.86, 17.159

**Deliverables**

- A discussion thread on alerts, crash and error groups, error reports, schedule runs, backups and audit rows, each kind joining as its milestone lands, and on change requests and incidents once 17.141 and 17.158 land, attached through 17.159's object references so a thread is readable exactly when its object is, and written in Markdown through the same renderer
- New comments arriving live as an event on the one 17.26 socket, filtered by 17.57 per message, so a thread never reaches a viewer who lost access mid-conversation
- Typing @ suggesting only panel users who can see that object, computed on the server, and a mention of someone who cannot see it refused with the reason rather than sent
- A mention delivered through the mentioned user's own 17.86 channels and preferences, carrying who mentioned them on which object and a link, never the comment's text, so nothing written in a thread leaves the panel
- An Inbox page listing mentions, replies in threads the user follows and, as their milestones land, assignments and review requests, each read, unread or done, with the unread count in the header and an item leaving the inbox when its object leaves the user's sight
- Following and unfollowing a thread, edits kept with their history, deletion by the author or a holder of the moderation permission leaving a tombstone naming who deleted it, and every comment, edit and deletion audited under permission keys of their own

**Acceptance**

- [ ] A comment on an alert appears in another operator's open thread within one second with no reload, proved by an end-to-end run
- [ ] Typing @ on a backup suggests only users who can read that backup, and a mention of a user who cannot is refused naming why, proved by a unit test
- [ ] A mention reaches the user through their chosen channel with a link and without the comment's text, proved by a shaping test on the delivered payload
- [ ] The inbox lists a new mention as unread, marking it done removes it from the count, and it leaves the inbox when the user loses access to the object, proved by a unit test
- [ ] A user without read on an object gets 404 for its thread on the route and not found on the socket, proved by a unit test
- [ ] An edited comment keeps its history and a deleted one leaves a tombstone naming who deleted it, proved by a unit test

## 17.163 On-call rotations, escalation and shift handoff

**Goal:** A volunteer team always knows who is watching, an alert nobody answers moves on to someone who will, and each shift hands over what is still open in writing.

**Size:** L. **Depends on:** 17.25, 17.67, 17.85, 17.86, 17.96, 17.97

**Deliverables**

- Rotations an owner defines per scope, the installation, a node, a realm or an app: participants in order, a daily or weekly length, a handoff time in a named zone and layers such as primary and secondary, kept in the store; nothing about on call shows until a rotation exists, and a lone owner is always on call
- Overrides an owner sets and swaps one operator requests from another for a range, taking effect only when accepted, each audited and shown on the 17.96 calendar
- Who is on call for the current page's scope shown in the top bar
- Routing through the rotation once one covers an alert's scope: the alert goes first to whoever is on call through their 17.86 channels rather than to everyone whose grants cover it, which stays the rule where no rotation exists, and 17.85's grouping, inhibition and silences apply before anyone is paged
- Escalation policies moving an alert unacknowledged within a set number of minutes to the next layer and then to every owner, stopped by 17.67's acknowledgement, each step recorded on the alert's history, a critical step never held by quiet hours, and every timer committed to the store so a supervisor restart resumes it without repeating a step
- A handoff report at each handoff for the outgoing and incoming operators: open and unacknowledged alerts, active silences and kill switches, maintenance in effect, running incidents, pending change requests and the schedules due in the next shift, each section following the reader's permissions and one whose milestone has not landed named as unavailable
- A shift log in which the outgoing operator writes a note the incoming one must acknowledge, an unacknowledged handoff escalated to the owners after a set time, and the log kept and searchable
- A Since you were last here card at each operator's sign-in, gathering from the audit log and 17.97's figure sources the alerts raised and resolved, the changes others made in their scope, and the incidents and handoff notes since their last session
- Permissions of their own for managing rotations and requesting swaps in the 17.48 catalog, with every rotation, override, swap, escalation step and acknowledgement audited

**Acceptance**

- [ ] With a rotation on an app, an alert on that app goes only to the operator on call and not to every holder of the grant, and with no rotation it goes to every holder as 17.86 routes it, proved by a unit test
- [ ] An alert unacknowledged past its layer's wait moves to the next layer and then to every owner, each step on its history, and acknowledging it stops the escalation, proved by a unit test with a fake clock
- [ ] Killing the supervisor mid-escalation resumes the timer on the next start and sends no step twice, proved by a unit test
- [ ] An accepted swap changes who is on call for exactly its range and shows on the calendar, and an unaccepted request changes nothing, proved by a unit test
- [ ] The handoff report for a seeded state lists its unacknowledged alerts, active silences and next-shift schedules, and names a section whose milestone has not landed as unavailable, proved by a unit test
- [ ] A handoff note left unacknowledged past its time is escalated to the owners, and acknowledging it records who and when, proved by a unit test
- [ ] With no rotation defined the top bar shows nothing about on call and routing is unchanged, and a lone owner reads as on call, proved by a browser test

## 17.164 Staff task board and recurring chores

**Goal:** Work that is not an alert, such as a restore drill, a grant review or a certificate renewal, is written down, assigned and dated beside the objects it concerns, and recurring chores no longer depend on someone remembering them.

**Size:** M. **Depends on:** 17.15, 17.48, 17.82, 17.86, 17.96, 17.97, 17.159

**Deliverables**

- A Tasks page with a board of To do, In progress, Waiting and Done and a list view, each task carrying a title, a Markdown description through 17.159's renderer, assignees among panel users, a due date and links to panel objects through 17.159's references, such as a crash group, an app, a setting key, a backup, an incident or a node, each link shown only to a viewer who may see its object
- Moving cards by drag and by keyboard, each move sending the task's version with `If-Match` so a move from a stale view answers 409 instead of undoing another operator's
- Tasks created by hand, from a health finding's Create task control carrying its code and fix route, from a crash or error group as those land, and from postmortem action items once 17.158 lands; a task from a finding shows the finding's current state and offers to close itself once the finding clears
- A create-task step on the 17.15 engine, so recurring templates such as a monthly restore drill, a quarterly grant review or a certificate renewal make a task with its assignee and due offset on a schedule, and a template with an open task from its last run does not make a second one
- Due tasks on the 17.96 calendar, overdue ones in the 17.97 digest and in the handoff report once 17.163 lands, and assignment notices through each assignee's 17.86 channels
- Task permissions of their own in the 17.48 catalog for reading, writing, assigning and managing templates, with every change audited

**Acceptance**

- [ ] A task created from a health finding carries its code and fix link, and says so when the finding clears, proved by a unit test
- [ ] A schedule with a create-task step makes one task per run and no second one while the first is open, proved by a unit test
- [ ] A task due tomorrow appears on the calendar on its date, and an overdue one appears in the next digest, proved by a unit test
- [ ] Moving a card from a stale view answers 409 and changes nothing, and a card moves between columns by keyboard alone and passes the accessibility gate, proved by a browser test
- [ ] A task linked to a backup shows that link only to users who can read the backup, and a user without the task read permission gets 403, proved by a unit test
- [ ] Assigning a task notifies the assignee through their channels, and every change writes an audit row, proved by a unit test

## 17.165 Type registry browser and the decoded data routes

**Goal:** An operator opens any class a running server's type registry holds, with its bases, derived classes, properties, flags and enum options, and every game data page after this one is built on the same guarded routes, the same byte-free writer and the same object view.

**Size:** M. **Depends on:** 17.06, 17.14, 17.48, 17.73, 3.03, 3.07

Added on 2026-09-27 at the maintainer's direction, who asked that the panel hold everything the client work can read and everything the servers hold. Every page built on these routes shows decoded values, read at run time from the installation's own client install, to an operator who holds the permission. None of them serves, downloads or exports the install's bytes, as Operations in doc/ARCHITECTURE.md and this phase's Client-derived data note require. The routes, the writer and the object view are built once here, under One of each. The registry already knows which classes came from the `server_class` tables, so this milestone also gives 6.10's supplement its panel surface without 6.10 waiting for the panel. The session-only mark rests on the decoded client data and API keys decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- `AdminDataView` in src/server/shared/Admin: the `/api/data/<kind>/` route family an app registers for each kind of decoded data it holds.
  - Pages of at most 200 rows with an opaque cursor, a search and filters.
  - Every answer stamped with the install's revision, the type dump's program SHA-256 and the generation of the store it read.
  - Every answer sent with `Cache-Control: no-store` and never with `Content-Disposition`.
  - No export, download or bulk route in the family.
- One writer every data answer is built through. It writes numbers, text, booleans, enum option names, class and property names, nested objects and containers. It cannot carry a byte buffer: a property that holds raw bytes is written only as its length and CRC-32.
- Budgets:
  - Data requests run on the admin API's threads and never on the world thread, as World threads, zone data and extracted tables settles.
  - At most two decodes run at once per app, each held to a time budget and a byte budget that are live settings. A request past either answers 503 naming it.
  - The relay gives the family a cost, so the 17.14 limiter holds each user to its rate and answers 429.
- The relay learns the family:
  - `Supervisor::PermissionFor` maps each `/api/data/` prefix to its permission.
  - `/api/capabilities` is relayed, so the panel learns which kinds each app serves and at which generation.
  - The catalog gains `clientdata.browse`, "Browse the game data decoded from the client install", at app, node and panel scope. It is held by the operator, game master and viewer roles as well as the owner and admin, and marked session-only so no API key can hold it.
- Type routes on every app that loads a type registry:
  - Classes a page at a time, searched by name or hash and filtered by kind and property flag.
  - A class with its bases in order, its derived classes, its properties (id, name, type, container, flags by name, offset and hash) and its enum options, including `__DEFAULT` and `__BASECLASS`.
  - Whether the class came from the type dump or from `server_class`, with that row's evidence.
  - The typed views bound to it, from one table the typed views register in.
- `ObjectTree` and `RecordLink` in packages/ui, with their stories:
  - `ObjectTree`: a typed property tree that expands nested objects, pages long containers on demand, and names flags and enum options.
  - `RecordLink`: a link that opens another record's page by its address.
- A Types page under a Game data heading in the Game group, for the app chosen among those `/api/capabilities` lists:
  - Search, the class page, and links between a class, its bases, its derived classes and its property types.
  - The revision stamp on every view.
  - Each data kind joins 17.155's search registry when that lands.

**Acceptance**

- [ ] A class's properties on the page, with their names, types, flags and hashes, equal what `client types <class>` prints from the same dump, proved by a route test over a fixture dump and a Client-gated test on the installed revision
- [ ] The data route sweep, which every later data milestone extends with its own routes, runs every registered `/api/data/` route against a fixture install whose byte-buffer properties, plain text entries and program code hold 64-byte marker runs. No answer holds a 32-byte run of any marker raw, in hex or in base64; every answer carries the install revision, the program SHA-256, the store generation and `Cache-Control: no-store`; and none carries `Content-Disposition`
- [ ] Through the relay, a user without `clientdata.browse` gets 403 on every data route, a user who holds nothing on the app gets 404, and a data route registered without a permission fails the route registry test
- [ ] A page asked for more than 200 rows, a request past its time or byte budget, and a user past the rate limit are each refused with the limit named (route tests)
- [ ] With twenty concurrent data requests, the 99th percentile of `ambrose_world_tick_seconds` over a minute stays within 10 percent of the same minute without them, recorded by a benchmark
- [ ] A class added through `server_class` shows as the server's with its evidence, and is gone after its rows are removed and `server_class_schema` reloads (database integration test)
- [ ] The Types page finds a class by name and by hash and walks to a base and back, and its component and accessibility tests pass in Chromium and WebKit

## 17.166 Archive browser: every archive and entry, decoded where a decoder exists

**Goal:** An operator walks the install's archives and opens any entry as the typed object it holds, and the panel never hands out the entry itself.

**Size:** S. **Depends on:** 17.165, 3.11, 3.13

Added on 2026-09-27 at the maintainer's direction. It brings the browsing half of doc/TOOLS.md's wadview into the panel, so there is one client data browser rather than a second local web app. Nothing the panel serves carries a file from the install, as Operations in doc/ARCHITECTURE.md settles, so no view renders an entry's texture, model, sound, layout or text.

**Deliverables**

- Archive routes on the game server:
  - The install's archives, each with its entry count, stored size and unpacked size.
  - An archive's entries a page at a time, filtered by path prefix, extension and kind (BINd, headerless object, locale table, plain text or other). Each entry shows its offset, size, compressed size, compressed flag and CRC, and whether the CRC matches the client's own variant.
  - A path search over one archive or all of them.
- An entry opens as what it holds, through `KiwadArchive` and the decoders the suite has:
  - A BINd file or a headerless versionable object opens through `ObjectSerializer` into the 17.165 tree.
  - A `.lang` file opens as its keys and text.
  - Any other kind shows its facts and the decoder it waits for, never its text or its bytes.
- Decode problems shown on the entry as the versionable decoder reports them: an unknown root class by its hash, and nested objects of classes the registry lacks, each with its properties' hashes and bit sizes
- An Archives page with a tree of archives and folders, search by path, and the entry view, whose address other pages link to

**Acceptance**

- [ ] Client-gated: Root.wad shows as many entries as `client wad --list` prints for the same install, and a sampled entry's size, compressed size and CRC equal the header `KiwadArchive` reads for it
- [ ] Client-gated: a BINd entry opens with the classes and values `bindecode` prints for it, and a zone's headerless gamedata.bin opens as WizZoneData
- [ ] An entry whose root class the registry lacks shows its class hash and each property's hash and bit size, and no bytes (fixture test)
- [ ] Client-gated: LoginMessages.xml shows its size, its kind and the decoder it waits for, and no answer carries its text; the 17.165 sweep covers every archive route
- [ ] Client-gated: a path search over every archive answers its first page within the request budget

## 17.167 Locale text browser and a live locale reload

**Goal:** An operator finds any text the client shows, in every locale the install has, and sees which keys a locale lacks.

**Size:** S. **Depends on:** 17.165, 3.13, 4.15

Added on 2026-09-27 at the maintainer's direction. The locale store is not a reload target today, which the Live reload and live settings rule in doc/ARCHITECTURE.md does not allow, so this milestone makes it one.

**Deliverables**

- Locale routes on the game server over `sLocaleStore`:
  - The install's locales, each with its file and entry counts.
  - A locale's files, and a file's keys and text a page at a time.
  - Search by key and by text.
  - One key's text in every locale, side by side.
- The comparison `localetool check` makes:
  - Keys the default locale has and another lacks.
  - Keys repeated within a locale, with both texts.
  - Files a locale does not carry.
- `locale` becomes a reload target:
  - The default locale is built again from Root.wad off to the side and swapped in whole.
  - The other locales are dropped, so they are built again on first use.
  - A failure keeps the tables serving and names every error.
- A Locale page, which every locale key the panel shows opens through `RecordLink`

**Acceptance**

- [ ] Client-gated: en-US shows the file and entry counts `localetool locales` reports for the same install
- [ ] Client-gated: searching a phrase finds its key, and the key's page shows its text in every locale that has it and names each locale that does not
- [ ] Client-gated: the missing-keys view for pl against en-US lists the keys `localetool check` lists
- [ ] A `locale` reload from an archive that cannot be read keeps the tables serving and reports the error, and a good reload shows a new generation (unit test)
- [ ] The 17.165 sweep covers the locale routes, and none answers more than 200 entries

## 17.168 Template explorer with cross-links and where each is used

**Goal:** Any template in the install is found by id, name or class and shown with its real property names, linked to the text, zones and other records it names.

**Size:** S. **Depends on:** 17.166, 17.167, 5.01, 5.02

Added on 2026-09-27 at the maintainer's direction. Templates stay decoded from the install at run time and are never stored, as Object templates in doc/ARCHITECTURE.md settles. This page reads `sObjectTemplateMgr` and keeps nothing.

**Deliverables**

- Template routes on the game server over `sObjectTemplateMgr`:
  - The manifest a page at a time, searched by id, name, class and archive.
  - A template with its class, name and archive entry; its behaviors in the order the store builds them, with empty slots kept; its adjectives; and its decoded object.
  - The store's figures from `GetCacheStats` (templates held, bytes against `Templates.CacheSize`, hits, misses and evictions), with its generation.
- Cross-links from one table declared beside the typed views, which names what a field means: a template id, a zone path, a locale key, a spell or a school. A text value is also linked when it is exactly a key in the locale store in use. No number is linked on a guess.
- Where a template is used: the `zone_object` placements that name it, each with its zone and position, shown to a holder of `world.read`
- A Templates page with search, the template view and its links, whose archive entry opens in 17.166

**Acceptance**

- [ ] Client-gated: template 1 opens as Player Object with its 39 behaviors in the order the store builds them
- [ ] Client-gated: a template found by name and the same template found by id are one record, and the manifest's total equals the count `client template --list` prints
- [ ] Client-gated: the Ravenwood NPC template 38232 lists the placements that use it, each with its zone and position
- [ ] A field the link table names opens the linked record, a text that is not a locale key is not linked, and a number the table does not name is not linked (unit test)
- [ ] Decoding a template moves the cache figures, and a `templates` reload shows the new generation within one refresh (route test)

## 17.169 Protocol browser: message definitions and what each server does with them

**Goal:** An operator or developer sees every message the client can send or receive, its fields, and whether each server handles it.

**Size:** S. **Depends on:** 17.165, 1.15, 2.09, 4.01

Added on 2026-09-27 at the maintainer's direction. The panel shows only a count of message definitions today.

**Deliverables**

- Message routes on every app over its `MessageRegistry`:
  - Protocols with their service id, version and record counts.
  - Messages a page at a time, searched by name, tag and service, each with its id, fields, DML types and description.
  - The load warnings.
  - The duplicates merged by service and tag.
- The app's `MessageHandlerTable` beside the definitions: each message handled, pending or refused as its rule says, with the session statuses it is accepted in and whether it is processed in place or queued
- Per-message counts kept in memory since the app started: received, handled, dropped and not handled. They show on the message's page and are kept out of the Prometheus register, which they would swell by about fifteen hundred series per app.
- A Protocol page with a summary per service of messages defined, handled, pending and refused, and a page per message

**Acceptance**

- [ ] Client-gated: the page lists as many protocols, records and ids as `client messages` prints for the same install (29, 1448 and 1446 on r806919), with the same count of fields per DML type
- [ ] A message the login server handles shows as handled with its statuses, and one its table refuses shows as refused (route test)
- [ ] A message the server does not handle, sent by a test client, raises its not-handled count by one on the page (integration test)
- [ ] Each service's summary equals the counts in the apps' handler tables (unit test)

## 17.170 Client scans from the panel: census, decode sweep, schema probe and program scans

**Goal:** The tool suite's install-wide reports run from the panel with live progress and show as tables, so an operator sees what the install holds, and what Ambrose cannot decode yet, without a terminal.

**Size:** M. **Depends on:** 17.166, 17.169, 3.11, 6.09

Added on 2026-09-27 at the maintainer's direction. No server holds these results. The app runs the tools that make them the way `TypeDumpCache` runs typeextract, from src/server/shared/ClientData, because the app knows its install and type dump and the supervisor does not. Where a tool cannot yet report in a form a program reads, the tool learns to, and the panel never parses its text.

**Deliverables**

- `ClientScans` beside `TypeDumpCache` starts a program from src/tools against the app's install through `ChildProcess`:
  - It passes an exact argument list, applies a timeout and a lowered priority, and runs one scan per install at a time across every app on the machine.
  - A cancel ends the tool and everything it started.
  - Each output line goes on a stream the page follows through the relay.
  - Starting a scan needs `clientdata.scan`, a new key at app scope held by the owner, admin and operator roles, and every start is audited.
- The tools learn `--format json` where they lack it:
  - `client census` reports the install's archives, entries and unpacked size, and each kind of file with its count and whether Ambrose decodes it.
  - `bindecode --sweep` reports its results as JSON.
  - schemaprobe's report is already JSON, and the `client handlers`, `behaviors` and `functions` answers are already JSON caches per revision, which are read rather than asked again.
- Results kept per revision in the Ambrose data folder beside the tools' caches, in a folder on 17.18's client-derived list, and never served as files
- The results shown as decoded tables:
  - The census.
  - The sweep's clean files, unknown root classes, nested unknown classes and re-encode mismatches, each opening its entry in 17.166.
  - The schema probe's unknown classes by hash, with the name the program's strings give, each property's hash, bit size and proven name, and the draft schema.
  - The handler registrations, joined to 17.169's messages.
  - The behavior classes set against `behavior_client_class`, with each difference named.
  - The functions whose log lines name them.
- A Scans page listing each scan with when it last ran, on which revision, how long it took and its result, with a button to run it

**Acceptance**

- [ ] Client-gated: a census gives the archive and entry counts that opening every archive of the install with `KiwadArchive` gives, and lists cinematics, state machines, Lua scripts, GUI layouts and SWF screens as kinds with no decoder yet
- [ ] Client-gated: the schema probe over Root.wad lists by hash the unknown classes `schemaprobe` prints, with the same names
- [ ] Client-gated: the behavior scan lists as many registrations as `client behaviors --list`, and marks every `behavior_client_class` row whose class differs from the one the program registers
- [ ] Cancelling a scan ends the tool and every process it started within a second, and a second scan of the same install is refused while one runs, from the same app or another (unit test with a fake tool)
- [ ] The 17.165 sweep covers the scan routes, and none answers with a result file
- [ ] A user without `clientdata.scan` cannot start a scan, and the refusal is audited (route test)

## 17.171 Client program reader, opt-in

**Goal:** A developer reads the client program's own code from the panel instead of a terminal: its strings, cross-references, disassembly and vtables, and, with their own copy of Ghidra, its decompiled functions.

**Size:** M. **Depends on:** 17.170

Added on 2026-09-27 at the maintainer's direction. It is an experimental feature under Decisions, Experimental features, off by default. It reads only the operator's own client program from disk and shows decoded instructions, names and text, never the program's bytes. That it belongs in the panel at all rests on the developer pages decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- `ClientData.ProgramReader`, a live setting off by default, and `clientdata.program`, a danger key marked opt-in as `files.pull` is, so the owner holds it and an administrator only when granted. While the setting is off, every program route answers 404
- Program routes through 17.170's runner, over `--format json` output the `client` tool learns for `strings`, `xrefs`, `disasm`, `vtable` and `functions`:
  - Strings searched by text, with their addresses.
  - The cross-references to an address.
  - A function's disassembly, annotated with the names the code index knows.
  - A class's vtable and the RTTI class names.
  - A function decompiled through the operator's own Ghidra when `client decompile` finds one, served from its cache.
- No view of the program's bytes: there is no hex view, instructions show only as mnemonics and operands, and the program's code sections are among the 17.165 sweep's markers
- Every program request audited with who made it, the address or query, and the program SHA-256 it read
- A Program page with search and an address view that walks cross-references and calls, opened from 17.170's handler registrations and function lists

**Acceptance**

- [ ] With the setting off, every program route answers 404 whatever the caller holds (route test)
- [ ] Client-gated: the disassembly of a handler address from the handler scan equals what `client disasm` prints for it, and a string search finds the addresses `client strings` finds
- [ ] Without Ghidra installed, a decompile request answers that Ghidra is missing and how to install it, and every other program route still answers (unit test)
- [ ] The 17.165 sweep over the program routes finds no 32-byte run of the program's code, raw, in hex or in base64
- [ ] Each program request writes one audit row naming its address or query (route test)

## 17.172 Progression and character creation data

**Goal:** An operator sees the level curves, schools, stat bands, name tables and creation data the servers loaded, as charts and tables rather than rows.

**Size:** S. **Depends on:** 17.165, 17.19, 3.14, 5.04

Added on 2026-09-27 at the maintainer's direction. These are world tables the servers extracted from the install, so they are shown under `world.read`, and changing one is a world edit through 17.34.

**Deliverables**

- Routes on the game server under `world.read`, over `sPlayerLevelMgr` and the stat effect set:
  - Per school and level: experience, health, mana, gold cap, pip chance, training points, crafting slots, pet energy, pip conversion values, shadow pip rating, archmastery and level name.
  - The magic schools with their badges.
  - The XP config and its encounter factors.
  - The mob rank levels.
  - The stat effect settings, with the crit, block and pip conversion bands.
- Routes on the login server over `sCharacterNameMgr` and `sCharacterCreateStore`:
  - The name parts per table and locale.
  - The disallowed names.
  - The creation schools and the `playercreateinfo` starting state.
  - A name check through `CharacterNameMgr::Check` that says whether a first, middle and last choice is allowed, and why not when it is refused.
- A Progression page:
  - A chart per value across levels, one line per school, drawn with uPlot with the table view every chart carries, as Charts and live data in doc/ARCHITECTURE.md settles.
  - The bands as tables.
  - Each chart and table naming the world table it came from.
- A Character creation page with the name tables, the disallowed list, the schools and the starting state

**Acceptance**

- [ ] Client-gated, after extraction: each school's curves cover levels 0 to 180 with the values `player_level_stats` holds
- [ ] Changing a `player_level_stats` value and reloading `player_level_stats` redraws that chart with the new value (database integration test)
- [ ] The name check refuses a disallowed name with the reason and accepts an allowed one (unit test)
- [ ] The name part counts per locale equal the rows the name store loaded (route test)

## 17.173 World tables browser and the world schema the servers publish

**Goal:** Every world table the servers read can be browsed from the panel, showing where each row came from and what each column points to, and the schema 17.34's edit forms are built from exists.

**Size:** M. **Depends on:** 17.168, 17.08, 4.09, 4.15, 5.04

Added on 2026-09-27 at the maintainer's direction. Tables filled from the install stay local and are never exported, as World threads, zone data and extracted tables settles, so every view carries that mark. `command_security` stays read at start until 6.05 makes it a reload target, and the page says so. Where the schema is declared rests on the world schema decision, and the session-only rule on the decoded client data and API keys decision, both settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- A world schema each store publishes when it registers its reload target:
  - The tables it reads.
  - Each table's source: extracted from the install, authored in the repository, or imported with the import's source. Where a table's source column mixes them, each row gives its own source.
  - Each table's key.
  - Each column's type as the database reports it.
  - The reference a column carries: another table's row, a template, a locale key, a class or a client blob.
- World table routes on the game and login servers, under `world.read`:
  - The tables, each with its row count, source and generation.
  - Rows a page at a time, sorted and filtered.
  - One row, with each reference opening its record.
- Client blobs and extracted tables:
  - A client blob held in a column, such as a placement's spawn requirements, is shown decoded through the registry, or as its length and CRC-32 when it does not decode, and never as bytes.
  - An extracted table offers no export, and its routes are marked session-only in the route registry.
- Tables no store reads, such as `character_create_option` and `zone_teleport`, listed as read by nothing
- A World tables page in the Game group

**Acceptance**

- [ ] Every table a store reads appears with its reload target and source, and a store that registers a table without a source or a key fails its unit test
- [ ] A placement's spawn requirements show decoded as a requirement list, and a blob that does not decode shows its length and CRC-32 and no bytes (Client-gated test and fixture test)
- [ ] A template reference opens its 17.168 template and a locale key reference opens its 17.167 key (route test)
- [ ] Through the relay, a user without `world.read` gets 403 on every world table route, and every extracted table's route is marked session-only (route test and route registry test)
- [ ] The 17.165 sweep covers the world table routes, and none answers more than 200 rows or offers an export

## 17.174 Zone catalog: templates, named places and placements on a plan

**Goal:** An operator sees every zone the install has, with its template, its named places and its placements laid out on a plan, and which of those the server spawns.

**Size:** S. **Depends on:** 17.173, 4.09, 5.02

Added on 2026-09-27 at the maintainer's direction. This is the static catalog; the instances a realm has loaded are 17.175 and 17.31. The plan is drawn from decoded positions, with no client art.

**Deliverables**

- Zone routes on the game server over `sZoneMgr`, under `world.read`:
  - Zones a page at a time, searched by path and display name.
  - A zone's template: its display name through 17.167, far clip, healing per minute, soft and hard limits, and no mounts.
  - Its named places, with position and direction.
  - Its placements, each with class, template, object id, position, orientation, scale, zone tag, start state, override name, flags, loading type and whether it is critical.
- Which placements the server spawns, those whose loading type is DYNAMIC_SERVER, and which the client builds itself. The zone extraction records the entries it leaves out, per zone and class and with the reason, in a world table of its own, such as the sigils that wait for 6.10, and the page shows them.
- A plan of the zone drawn from the decoded positions:
  - Named places and placements as marks on the zone's own coordinates.
  - Filters by class and loading type, with each mark opening its placement.
  - The placements table as the plan's table view.
- A Zones page in the Game group. Each zone opens its archive in 17.166, each placement opens its template in 17.168, and a zone path anywhere in the panel opens its zone.

**Acceptance**

- [ ] Client-gated: the Commons shows 31 named places and 177 placements, 123 of them spawned by the server and 1 critical, and its 6 MinigameSigilInfo entries as left out with the reason
- [ ] Client-gated: Ravenwood shows 93 placements, and its 4 sigil entries as left out by class, with the reason
- [ ] The zone count equals the rows `zone_template` holds (route test)
- [ ] A `zone_object` reload shows the new generation, and the plan redraws within one refresh (route test)

## 17.175 Live world: zone instances, spawned objects and wizards in the world

**Goal:** An operator sees what a game server is running right now: each zone instance, the objects spawned in it, and every wizard in the world with their state.

**Size:** M. **Depends on:** 17.165, 17.49, 4.10, 5.02, 5.03, 5.05

Added on 2026-09-27 at the maintainer's direction. The realms page of 17.31, the online players page of 17.177 and the player inspector of 17.92 build on these routes, so there is no second listing of instances or sessions. 17.49 is here because a kick runs at the caller's command level, which 17.49 passes to the app.

**Deliverables**

- A world snapshot the world thread builds on request, at most once a second, and hands to the admin API, so no route reads an instance or a session off the world thread, as World threads, zone data and extracted tables settles. It holds the tick count and times, the sessions, the instances and the scripts loaded.
- Instance routes under `realms.read`:
  - Each instance with its dynamic zone id, zone path, public or private, players, objects, mobile ids held and cooling, unload time and the generations it was built from.
  - An instance's objects, each with global id, perm id, mobile id, template, class, position and critical flag.
  - Each object as the server sent it, decoded through the registry and never as its bytes.
- Wizard routes under `players.read`. Each session shows:
  - Its account, character, security level, state, zone and instance, position and yaw.
  - Its school, level, experience, health, mana, gold and training points.
  - Its count of messages not handled.
  - Its address, only to a holder of `accounts.pii.read`.
- Kick from a wizard's row through the existing `kick` command at the caller's command level, which needs `players.kick` and a reason and is audited
- The login server's `/api/players` names every wizard, those named by name indices included, through the name store
- A Live world page in the Game group, with the instances, an instance page with its objects and players, and a wizard page. Each refreshes through the relay every two seconds until 17.58 moves it onto the panel socket.

**Acceptance**

- [ ] A wizard appears on the page within one refresh with its zone, instance and position, and leaves it when it detaches (integration test that attaches a wizard)
- [ ] An instance's objects are the placements the server spawned for it, and one opens decoded through the registry with no bytes in the answer (integration test)
- [ ] With the page open and refreshing, the 99th percentile of `ambrose_world_tick_seconds` over a minute stays within 10 percent of the same minute without it (benchmark)
- [ ] Kicking from the page closes that session with the reason, and a user without `players.kick` gets 403 and the attempt is audited (integration test)
- [ ] `/api/players` names a wizard whose name is made of name indices (unit test)
- [ ] A user without `players.read` sees no wizard, and a user without `accounts.pii.read` sees no address (route test)

## 17.176 Spells and sigils pages

**Goal:** An operator looks up any spell or sigil as the server holds it, with its school, pips, accuracy, effects, tiers and text, and teaches a spell to a wizard in the world or takes one away.

**Size:** M. **Depends on:** 17.167, 17.168, 17.175, 8.04, 8.05, 9.02

Added on 2026-09-27 at the maintainer's direction. It brings the read-only half of doc/TOOLS.md's Spell and deck inspector into the panel, and decks join this page in the milestone that loads them. 8.04, 8.05 and 9.02 have only real-client checks left, which wait for 6.04. Until then, 17.168 already shows every spell's template decoded.

**Deliverables**

- Spell routes on the game server over `sSpellMgr`, under `clientdata.browse`:
  - Spells a page at a time, filtered by school, secondary and required school, type, level restriction, PvP and PvE, treasure card, tiered, retired and cantrip, and searched by name and id.
  - A spell with its pips per school, accuracy, training cost, source and flags.
  - Its effect tree, with each type, target and disposition named from the type dump's enums, and each conditional element marked with its requirements.
- Tiered groups from `TieredSpellsGroupInfo.xml`, each with its members, and each tiered spell's group and retired flag
- Sigil routes over `sSigilMgr`:
  - Each sigil's circles with their slot, angle and radius, drawn and also given as a table.
  - Its engage radius, battlefield effects, shadow thresholds, and PvP and PvE scalars and limits.
- The spells that failed the last `spells` reload, each with how it failed
- A wizard's known spells on the 17.175 wizard page, each opening its spell. A spell is learned or unlearned for a wizard in the world through the existing `learn` and `unlearn` commands at the caller's command level, which needs `characters.edit` and a reason and is audited.
- Spells and Sigils pages. The name and description open 17.167, and the template opens 17.168.

**Acceptance**

- [ ] Client-gated: Fire Cat shows school Fire, rank 1, accuracy 75%, type Damage, and its random effect with five kDamage Fire amounts on kEnemySingle, the lines `.spell info Fire Cat` gives on the game server's console
- [ ] Client-gated: tiered group 4 lists Fire Cat among its members
- [ ] Client-gated: a sigil's circles and limits on the page equal what `.sigil info` gives for it
- [ ] Learning a spell from the panel adds it to that wizard's book and to `character_spell`, and a user without `characters.edit` gets 403 and the attempt is audited (integration test)
- [ ] The spell and effect counts equal the set the server loaded, and a `spells` reload that fails shows its errors while the old set keeps serving (route test)

## 17.177 Characters and online players pages

**Goal:** Game masters rename, restore, delete and edit characters, and act on players online, from the panel.

**Size:** S. **Depends on:** 17.21, 17.175, 3.17, 6.05

Split from 17.21 on 2026-09-27 at the maintainer's direction, under this phase's Splits note. This part waits for character deletion in 3.17 and the GM ban and character commands in 6.05, so the accounts and bans of 17.21 are not held back by it.

**Deliverables**

- On each account's character list from 17.21:
  - Rename, restore of a deleted character, and delete, as 3.17 and later phases support them.
  - An edit form offering the fields the running build reports as editable through `GET /api/capabilities`, such as gold and level once later phases make them so, each applied through its own GM command.
- The 17.175 wizard rows become the online players page:
  - Joins and leaves.
  - A ban that disconnects the player through the 6.05 ban path.
  - Mute and teleport from 12.07 and 6.06, once those exist.
  - A slot in each row reserved for the per-session quality chart 17.92 fills, so that milestone adds a column rather than reworking a finished table.
- The top-bar search from 17.21 gains characters by name, each result checked against the caller's own permissions
- Every action goes through CommandMgr at the panel user's command level, refuses to act on an account at or above that level, requires a reason, and is audited, refused attempts included

**Acceptance**

- [ ] Banning an account from the panel while its player is on character select disconnects that client with the ban message, through the 6.05 ban path (integration test with a test client)
- [ ] Restoring a deleted character from the panel returns it to the account's list, and a user without `characters.restore` gets 403 and the attempt is audited (database integration test)
- [ ] The top-bar search finds a character by name, and returns nothing for a character on an app the caller holds nothing on (browser test)
- [ ] The character edit form offers only the fields the build reports as editable, and a field the build does not report cannot be submitted (browser test and route test)

## 17.178 Client driver runs from the panel, opt-in

**Goal:** The maintainer starts a client driver scenario from the panel and reads its report there, so the real-client checks that hold milestones back are run and read in one place.

**Size:** M. **Depends on:** 17.06, 17.14, 17.48, 3.24

Added on 2026-09-27 at the maintainer's direction. It is an experimental feature, off by default, resting on the developer pages decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md. The driver starts the operator's own retail client on the machine the supervisor runs on, exactly as 3.24's driver does from a terminal. It is repository tooling in Python, so it runs only from a checkout.

**Deliverables**

- The settings and the key:
  - `ClientDriver.Enable`, off by default.
  - `ClientDriver.Folder`, which names a checkout's apps/clientdriver.
  - `clientdata.drive`, a danger key marked opt-in, so only the owner holds it by default.
  - While the driver is off or its folder is not named, every driver route answers 404.
- Driver routes in the supervisor:
  - The scenarios, with their steps.
  - The runs under the data folder's `clientdriver/runs`, each with scenario, start, duration, verdict and each check's result.
  - A run's report, with the messages the server did not handle named by service and tag, and the warnings from either side.
- Starting a scenario runs the driver through `ChildProcess`:
  - Only when the supervisor runs in the signed-in user's desktop session and a client install is present.
  - One run at a time, with its progress on a stream the page follows.
  - A stop that ends the driver and the client it started.
  - A refusal that says why a run cannot start.
  - Each start and stop is audited.
- Screens the driver captures are judged on the machine and shown only as verdicts and scores, never as images. The driver's runs, reference crops and zone caches are on 17.18's client-derived list.
- A Client driver page in the Game group

**Acceptance**

- [ ] With the driver off, every driver route answers 404 whatever the caller holds (route test)
- [ ] A finished run's page shows the verdict and the messages not handled that its report.json holds (unit test over a fixture run folder)
- [ ] A second run is refused while one is going, and stopping a run ends the driver and the client it started within five seconds (integration test with a fake driver)
- [ ] A supervisor running as a service lists the runs and refuses to start one, naming why (unit test)
- [ ] No driver route serves a screenshot or a crop (route test)
- [ ] Dev-gated: on the maintainer's machine, the enter-world.json scenario started from the panel reaches the world and its report shows on the page. Needs the maintainer's own client

## 17.179 One desktop shell for the launcher and the panel program

**Goal:** The launcher and the panel program open their windows through one host, so every safeguard lands once: pages compiled into the program, a remote panel held to its own origin with no way to reach the program, certificates accepted only by pin, and nothing written beside the executable.

**Size:** L. **Depends on:** 1.04, 3.25, 17.14, 17.73

Added on 2026-09-27 at the maintainer's direction, who asked for the panel as its own installable program. Nothing here is a new choice:

- The window is the operating system's own web view opened by a C++ program, as the Look and Front-end layout rows under Decisions, Operations and doc/UI-STACK.md settle.
- Built pages are compiled into the program, as Serving the built files from C++ in doc/UI-STACK.md settles.

The window host 3.26 began inside `launcher-core` moves to where a second program can use it, under One of each, and 3.26 keeps its checks, earned on this shell. It fixes four places where the host as built falls short of doc/UI-STACK.md:

- WebView2 is created with no user data folder, so it writes beside the executable.
- The Windows page is served on `http://launcher.ambrose`, which a web view does not treat as a secure context.
- The WebKitGTK scheme is registered as CORS-enabled but not as secure.
- The pages are copied beside the programs rather than compiled in.

**Deliverables**

- `src/tools/shell/`, a `desktop-shell` library holding the window host moved out of `launcher-core` with its behaviour unchanged: the WebView2 window on Windows, the WebKitGTK window elsewhere, the remembered place and the message loop. `launcher-core` links it, and the launcher's options, window and tests behave as before.
- `ambrose_embed_page`, a CMake step that turns a built Vite folder into one generated translation unit, never committed, holding each file's path, media type, entity tag and bytes:
  - The shell serves it from memory on the program's own `https` origin: through a web resource handler on WebView2, and through a custom scheme registered as both secure and CORS-enabled on WebKitGTK.
  - Hashed assets get an immutable cache header and `index.html` gets `no-cache`.
  - Any other path outside the API prefix gets `index.html`, and the API prefix never falls back.
  - The launcher's page moves from the `launcher-ui` folder beside it into its binary, and a build without the front-end option embeds a page that says so.
- The supervisor serves the dashboard compiled into its own binary when `Panel.DashboardDir` and `Admin.DashboardDir` are empty, with the headers `AdminFiles` sends today. A named folder still wins, so a developer can point at a fresh build.
- A view bound to one remote origin, `http://127.0.0.1:<port>` or `https://<host>:<port>`:
  - A navigation to any other origin, a new window or a `target=_blank` link opens in the system browser instead.
  - A download is saved only through a save dialog the program shows.
- The host channel serves only the program's own origin:
  - A remote-bound view has web messages turned off and no message handler.
  - A message from any other origin is dropped, and its origin logged once.
  - `hostKind()` in packages/ui picks a native host only on the program's own origin.
- A certificate hook:
  - When the web view meets a certificate it cannot verify, the shell hands its SHA-256 fingerprint to the program, which accepts it only when it equals the pin held for that host and port.
  - Every other certificate error is refused with the fingerprint named.
  - A publicly trusted certificate needs no pin.
- Web view data kept in a folder of each program's own under the Ambrose data folder, never beside the executable. Each remote origin gets a profile of its own, whose cookies and storage no other profile sees, and a profile can be deleted with everything in it.
- Tests and docs:
  - Unit tests over the embedded manifest, the origin gate, the navigation rule, the pin decision and the remembered place.
  - A smoke test, skipped where no web view exists, that opens the launcher's page from memory off screen and closes it.
  - The launcher's existing tests, unchanged.
  - doc/ARCHITECTURE.md's repository layout and Client launcher entry, doc/UI-STACK.md's serving section and doc/TOOLS.md name the shell and the embedding step.

**Acceptance**

- [ ] Every Launcher test passes unchanged, and `launcher --window-ui` from a build tree with no `launcher-ui` folder opens its page from memory (the Launcher tests and the shell smoke test)
- [ ] Under both web views the page reports `window.isSecureContext` as true and can write local storage (the smoke test on Windows, and on Linux under a virtual display)
- [ ] A window run from a folder the user cannot write opens, and the run writes nothing beside the executable (a smoke test that lists the program's folder before and after)
- [ ] The embedded page answers `index.html` for an unknown path and 404 for an unknown path under the API prefix, gives each file its media type and entity tag, and puts an immutable cache header only on hashed assets (unit tests over the generated manifest)
- [ ] A supervisor with both dashboard options empty serves the dashboard from its own binary with the headers it sent before, and one naming a folder serves that folder (`tests/e2e/panel-listener.spec.ts` against the compiled copy, and a supervisor test for the folder)
- [ ] A page at a remote origin that posts to the host channel gets no answer, the program logs that origin once, and `hostKind()` answers `http` on any origin but the program's own (a smoke test with a page served from a loopback listener, and a unit test in packages/ui)
- [ ] A view bound to an origin sends a navigation to another origin, and a new-window request, to the system browser, and stays where it was (unit tests over the navigation rule, and the smoke test)
- [ ] A self-signed certificate is accepted only when its fingerprint equals the pin, a second certificate on the same host and port is refused with both fingerprints named, and with no pin every certificate error is refused (unit tests over the pin decision, and a smoke test against a loopback listener serving two `supervisor --panel-self-signed` certificates in turn)
- [ ] Two remote profiles share no cookie, and deleting one leaves no file of it (the smoke test)

## 17.180 Sign-in links for a desktop program

**Goal:** A program on the panel's own machine opens the panel signed in without anyone typing a password, and an operator pairs a desktop with a panel on another machine from that machine's console, the pairing carrying the certificate to trust so it is never guessed.

**Size:** M. **Depends on:** 17.05, 17.14, 17.46

Added on 2026-09-27 at the maintainer's direction. 17.24 promised that a player hosting at home opens the panel through a one-time link bound to the machine. But 17.46's owner link only makes the first owner, once, and its password links set a password, so nothing signs an existing owner in. Holding the supervisor's admin token already allows everything the console allows, `panel user create` among it, so a link minted with that token grants nothing the token did not. The pairing link is a new way to open a session, so this milestone rests on the desktop sign-in links decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- `panel_link` in the supervisor's store, from a dated update in `data/sql/panel/`:
  - It holds each link's kind (owner claim, password, local or pairing), user, issuer, expiry and time of use, with the token kept only as its hash.
  - 17.46's owner claim and password links move into it from memory, so there is one store of single-use links and each survives a supervisor restart.
  - 17.152's display pairing codes use it when that lands.
- `POST /api/panel-links` on the supervisor's admin API, behind its bearer token:
  - A local link lasts 60 seconds and is honoured only from a loopback peer on the panel listener.
  - A pairing link lasts 10 minutes and is honoured from any address. It is refused for a listener serving plain HTTP beyond loopback, and for a name `Panel.AllowedHosts` would refuse.
  - On a panel with no user yet, a local link makes the owner as `panel user create` does: under the name given, or `owner`, with an unguessable password nobody is told. A desktop's first run therefore asks nothing.
- Command-line and console access:
  - `supervisor --panel-link [username]` and `supervisor --panel-pair <username> --address <host[:port]>` read the admin token as the supervisor does and ask the running supervisor.
  - The console commands `panel user link` and `panel user pair` do the same from an attached console.
  - A pairing prints one line holding the address, the port, the fingerprint of the certificate the listener serves, and the token.
- `POST /api/panel/link` on the panel listener, and a `#link` page beside the claim and password pages, trade a link for a session through the path sign-in takes:
  - The session id is regenerated and the sign-in is recorded.
  - A wrong or spent token counts against the 17.46 throttles, and a disabled user is refused.
  - No second factor is skipped; 17.47 adds its step here when it lands.
- Auditing:
  - `panel:link.issued` is audited with the kind, user, issuer and expiry, and `panel:session.opened` names the link it came from.
  - The token reaches only the caller that asked for it: no log file, audit row or command history holds it.
- doc/PANEL.md's Panel users and Sign-in sections, doc/config/supervisor.md and the console's help describe both links

**Acceptance**

- [ ] A local link signs in once and only from loopback: a second use answers 410, a use from a peer that is not loopback answers 403, and a use after 60 seconds answers 410 (PanelLinkTest over HTTP, with a clock moved through the window)
- [ ] Without the admin token the route answers 401. With it, on a panel with no users, it makes the owner under the name given and returns a link that signs them in, and no password for that owner appears anywhere (PanelLinkTest)
- [ ] An owner claim link printed before a supervisor restart claims once after it, and never twice (PanelLinkTest)
- [ ] Against a running supervisor, `supervisor --panel-link` prints a link that opens a session, and `--panel-pair` prints the fingerprint of the certificate the listener actually serves (an AppSmoke run that reads the fingerprint back through `AdminClient`)
- [ ] Env-gated on `AMBROSE_TEST_ADMIN_REMOTE_BIND`: a pairing token signs in once over TLS from an address that is not loopback, and a pairing asked of a listener serving plain HTTP beyond loopback is refused with the reason
- [ ] Twenty wrong or spent tokens from one address within a minute answer 429, as failed sign-ins do, and a disabled user's link is refused (PanelLinkTest)
- [ ] After a link is issued and used, the token is in no log file, audit row or command history, and the store holds only its hash (a test that searches each for the token)

## 17.181 The panel program: its own window and a list of panels

**Goal:** Anyone running Ambrose opens one program, picks a panel from its list and manages it in the program's own window, whether the panel is on this computer or on a server elsewhere. Every connection is checked, and the program holds no panel password.

**Size:** L. **Depends on:** 17.06, 17.179, 17.180

Added on 2026-09-27 at the maintainer's direction, who asked for the panel as its own program, opened from an executable, so people can manage their servers on their own machine or live on another.

- **Why a program of its own.** It is not a mode of the launcher, because the launcher is for players and must work where no server is (3.27). It shares the launcher's shell through 17.179.
- **No copy of the dashboard.** The program carries none for the panels it opens: each panel is shown from its own origin, so a panel and its pages are always the same version. Every page a later milestone adds, the game data pages among them, appears in the program with no change to it.
- **Relation to 17.154.** 17.154 lets a browser install one panel as a web app. This is the native program, with its list of panels, and later hosting (17.24), a tray (17.182) and packages (17.183).
- **Passkeys.** Chromium refuses WebAuthn on a page it loaded past a certificate error, so passkeys (17.45) inside the program need the trusted certificate 17.108 gives.

It rests on the panel program decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- `src/tools/panel`, the `panel` executable over a `panel-core` library that links the 17.179 shell and `shared`:
  - Its own screens live in `apps/panelui`, built from packages/ui and compiled in through `ambrose_embed_page`.
  - `panel.conf.dist` sits beside it, documented in doc/config/panel.md.
  - Its data lives in `PanelApp` in the Ambrose data folder.
- The server list, in the program's own SQLite file:
  - Each entry holds a name, an address, a certificate mode (loopback, publicly trusted, pinned to a SHA-256 fingerprint, or plain HTTP the operator opted into), the username last used and when it was last opened.
  - Entries are added, renamed, edited and forgotten, and forgetting one deletes its profile.
  - A supervisor on this machine is listed as This computer without being added, when its admin API answers on loopback and its token is in the default file under the Ambrose data folder. One whose panel listener is off is listed with the setting that turns it on.
- A panel on another machine is added in one of three ways:
  - From a 17.180 pairing line, which carries the address and the pin together.
  - From an address whose certificate a public authority vouches for.
  - From an address with a self-signed certificate. The program shows the fingerprint grouped for comparison with the one the supervisor printed, and pins it only once the operator confirms they match.

  Plain HTTP beyond loopback is refused unless the operator takes the per-entry opt-in. The opt-in says the password and the session then cross the network unencrypted, as `Panel.AllowPlainHttpRemote` does.
- Each panel opens in a window of the program's own, through a 17.179 view bound to its origin, with its own profile and no channel to the program:
  - This computer opens through a 17.180 local link.
  - A paired panel opens through its pairing link once, then through its own sign-in page whenever its session ends.
  - Any other panel opens through its own sign-in page, as does one that answers 404 to the link route.
  - The program never asks for, sees or stores a panel password.
- A pinned panel whose certificate changes is refused before any request is sent. The program shows the old and new fingerprints, with the choice to confirm the new one against the supervisor's log or to pair again.
- The list shows which panels answer, through a probe per entry:
  - The probe goes through libcurl over OpenSSL, as the Outbound HTTP row settles.
  - It checks the pin before sending anything, and reads only the panel's public `GET /api/panel/session`.
  - It names each state: answering, unreachable, certificate changed, or not an Ambrose panel.
  - Probes run only while the list is shown.
- `panel --version` and `--help`, and an about screen showing the program's version and revision, the web view runtime's version, and the revision of This computer's supervisor from its health
- On a machine with no web view, the chosen panel opens in the default browser, This computer through its local link, and the program says once why
- Tests and docs:
  - Unit tests over the server list, the certificate modes, the pairing parser, the plain HTTP refusal and the probe states against fake answers.
  - An integration test that runs a supervisor on loopback with a self-signed certificate and proves the probe accepts the pin and refuses a swapped certificate.
  - A smoke test, skipped with no web view, that opens This computer through a local link and reaches the overview.
  - The front-end job builds and tests `apps/panelui`.
  - doc/DESIGN.md's Surfaces, doc/TOOLS.md and the repository layout in doc/ARCHITECTURE.md name the program.

**Acceptance**

- [ ] With a supervisor serving its panel on loopback, `panel` lists This computer without being told, and opening it reaches the overview signed in as the owner with nothing typed (the smoke test)
- [ ] A panel over TLS with a self-signed certificate opens once its fingerprint is confirmed or paired, and after the certificate on that port is replaced the program sends the panel no request and names both fingerprints (the integration test with two `--panel-self-signed` certificates)
- [ ] Dev-gated: from the maintainer's desktop, a panel on a second machine or virtual machine is paired from its console and opened in the program's window, signed in once by the pairing line and afterwards through its own sign-in page. Needs a second machine or a virtual machine
- [ ] A plain HTTP address beyond loopback is refused without the opt-in and accepted with it, and the entry then carries its warning (unit tests)
- [ ] After a sign-in to a remote panel, the program's data folder, SQLite file, log and configuration hold no panel password, and forgetting the panel leaves no file of its profile (a test that searches the folder for the typed password)
- [ ] A panel page that posts to the host channel gets no answer, and a link on it to another site opens in the system browser (the smoke test against the dashboard)
- [ ] With no remote entries, the program opens no connection off this machine (the probe's fake transport records none)
- [ ] `panel --version` and the about screen report the version and revision the build carries, and the about screen shows the local supervisor's revision (a unit test and the smoke test)
- [ ] On a machine with no web view, choosing This computer opens the default browser at a local link that signs in once (a test with a fake browser opener)

## 17.182 The panel program in the tray

**Goal:** While a server runs on this computer, the panel program stays out of the way in the tray, its icon says whether the server is healthy and how many players are on, and it opens, starts and stops the server.

**Size:** M. **Depends on:** 17.24

Added on 2026-09-27 at the maintainer's direction; the tray moves here from 17.24. It rests on the tray decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md. Alerts reach an operator through 17.154's channel inside the panel's window, so the tray raises no notification of its own.

**Deliverables**

- A tray icon in the identity doc/DESIGN.md sets, whose state follows this computer's supervisor: stopped, starting with its step, running with the players online, or a problem. Its menu holds Open panel, Servers, Start or Stop this computer, Play and Quit, and shows a line when 17.17 reports an update.
- Closing the window keeps the program in the tray while this computer's server runs. Quit asks whether to stop the server, stops it through 17.24's graceful path when told to, and leaves it serving otherwise.
- Start at sign-in, opt-in and off by default, through the per-user Run key on Windows and an autostart desktop entry elsewhere, removed when turned off
- A desktop with no tray host, such as GNOME without an extension, keeps the window in the taskbar and says so once
- Tests: unit tests over the state and menu for each status with the tray behind a fake, and a smoke test on each operating system, skipped where there is no tray host

**Acceptance**

- [ ] The icon follows the supervisor within one status interval: stopped, starting with its step, running with the player count, and a problem (unit tests over the fake tray, and an integration test with a fake supervisor)
- [ ] Closing the window keeps the server running and the icon present. Quit with stop chosen stops the supervisor gracefully and exits, and Quit without it leaves the supervisor serving (integration test)
- [ ] Turning start at sign-in on writes exactly one entry, and turning it off removes it (unit tests against a fake registry and folder)
- [ ] On a desktop with no tray host, the window stays in the taskbar and the program says once why (a Linux smoke test with no StatusNotifier host)
- [ ] Dev-gated: on the maintainer's Windows and Linux desktops, the icon and its menu appear and follow a restart of the stack. Needs the maintainer's desktops

## 17.183 Installing the panel program: installer, uninstaller and portable archive

**Goal:** The panel program installs like any desktop program on Windows and Linux, with an icon, shortcuts, a version and a clean uninstall, from packages CI builds at no cost.

**Size:** M. **Depends on:** 17.24

Added on 2026-09-27 at the maintainer's direction; the installer moves here from 17.24. It rests on the installer format decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md. The launcher's own package comes from this pipeline once 3.27 lands, so the two programs install and uninstall one way.

**Deliverables**

- One packaging pipeline in `apps/packaging/desktop/`, run from the release presets:
  - On Windows: a per-user installer into the user's own programs folder that needs no administrator, with Start menu and desktop shortcuts, an icon and version resource drawn from the tokens, and an entry in the system's installed apps; plus a portable zip.
  - On Linux: an archive whose program installs its desktop entry and icon for the current user on first run, and a Debian package that declares the web view as a dependency.
- The program and the server component are each laid into a version folder of their own, beside a small starter that runs the current program. This is the layout 17.184 switches with one pointer change.
- An optional server component, chosen at install and on by default:
  - It holds the supervisor, the apps, their `.conf.dist` files, the tools they and the panel run (typeextract, extractor, client, bindecode, schemaprobe and localetool), and the private database on the terms the private database decision sets.
  - Without it, the program installs connect-only.
- An install manifest in every package, listing each file with its size and SHA-256, which 17.118 checks for drift and 17.184 signs. No package holds a file from a client install, a type dump or extracted data, and each is checked against its content list.
- Uninstalling:
  - It removes the program, its shortcuts, its desktop entry and its start-at-sign-in entry, and stops a supervisor the program started.
  - It removes the program's own data and the hosted server's data only when the operator ticks each.
  - It never touches the game install.
- On Windows the installer checks for the WebView2 runtime. When it is missing, the installer says so and offers Microsoft's installer only with the operator's consent. Without the runtime, the program opens panels in the default browser.
- Tests and docs: a packaging test in CI that builds each package on its own platform and checks its content list and manifest, unit tests over the uninstall plan, and doc/INSTALL.md describing how the program is installed

**Acceptance**

- [ ] The packaging test builds the Windows installer and zip and the Linux archive and Debian package in CI, each holding exactly its content list: no file from a client install, no type dump and no extracted data
- [ ] A package built without the server component installs a program that runs connect-only (packaging test)
- [ ] Each package's install manifest lists every file it carries with the SHA-256 of its bytes, and a file changed after packaging fails the check (packaging test)
- [ ] The uninstall plan removes the program, its shortcuts and its entries, keeps the program's data and the server's data unless each is ticked, and touches nothing the install did not write (unit tests over the plan, and the portable package installed into and removed from a temporary folder)
- [ ] Dev-gated: on a clean Windows machine the installer needs no administrator and adds both shortcuts with the icon and version, the program opens from them, and it uninstalls cleanly. Needs a clean Windows machine
- [ ] Dev-gated: on a clean Ubuntu desktop the archive's program installs its desktop entry on first run and opens from the application menu, and the Debian package installs with its web view dependency. Needs a clean Ubuntu desktop

## 17.184 Signed updates for the desktop programs

**Goal:** The panel program and the launcher update themselves from a signed release: they stage the new version beside the old, switch in one step, and roll back when the new version will not start. The server's own programs are left to 17.17.

**Size:** M. **Depends on:** 17.105, 17.183

Added on 2026-09-27 at the maintainer's direction. 3.27 asks for a launcher that updates itself, and nothing planned how the panel program updates itself, so this is one updater for both. Downloads go through libcurl, as the Outbound HTTP row settles. The manifest is signed with the project release key that the release signing key decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md, sets on the patch signing key's terms: Ed25519 through Botan, with overlapping rotation.

**Deliverables**

- A shared updater in `src/tools/shell`, with a channel named in the program's configuration and off by default:
  - The manifest is 17.183's install manifest with the version and the lowest version it updates from, signed with the release key.
  - The public keys are compiled in, and a configuration may add one more for an operator's own channel.
  - A rotated key is accepted across its overlap window.
- Staging and switching:
  - Each file is downloaded into a new version folder beside the running one and checked against the manifest before anything switches.
  - The switch is one pointer change of 17.183's starter.
  - A new version that does not report itself started within a set time is replaced by the previous one. The failed version is kept aside, and both are logged.
  - 17.17 wraps this same staging and switch with its backup, check and health wait when it lands, rather than building a second updater.
- An update touches only the program's own version folders: never the game install, the Ambrose data folder, or the server component, whose builds 17.17 switches. A server build that arrives in a package is staged for 17.17, and until 17.17 lands the program says the server programs stay at their version.
- Each program's about screen shows the running version and the channel. An update is offered, never forced, and a program with no channel configured makes no connection off the machine.
- 17.105's release workflow publishes the signed manifest beside each package
- Tests and docs: unit tests over the manifest checks against a local fixture channel, a rollback test, an interrupted-switch test, and doc/config/panel.md describing the channel

**Acceptance**

- [ ] A manifest with a wrong signature, a file with a wrong hash or size, a missing file, or a version older than the running one is refused before anything switches, and the running version keeps working (unit tests against the fixture channel)
- [ ] A new version that exits at start is replaced by the previous one, and the failure and the rollback are both logged (rollback test)
- [ ] A switch interrupted at any of its steps leaves one complete version that starts (interrupted-switch test)
- [ ] An update writes nothing inside the game install, the Ambrose data folder or the server component's folders (a test that lists each before and after)
- [ ] With no channel configured, a whole run opens no connection off the machine (the fake transport records none)

## 17.185 The Ambrose service from the panel program

**Goal:** On a machine where Ambrose runs as a service, the panel program shows and controls that service instead of starting a second server, and opens its panel.

**Size:** S. **Depends on:** 17.23, 17.24

Added on 2026-09-27 at the maintainer's direction. 17.23's service runs as a dedicated user whose admin token and keyring the operator's own account cannot read. The program therefore cannot treat it as its own process, and must never start a second supervisor beside it. It rests on the service control decision, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- The service `--install-service` registers, `AmbroseSupervisor` on Windows and `ambrose.service` elsewhere, is detected and listed as This computer (service) with its state, and the program starts no supervisor of its own beside it
- Start and Stop go through the operating system's own elevation, the administrator prompt on Windows and polkit elsewhere, with the command shown for anyone who would rather run it
- The service's panel opens through its own sign-in page, as the service control decision sets, and through a 17.180 local link only when the operator's account can already read the service's admin token. Opening it never asks for elevation.
- Unit tests over detection, the commands and the refusal to start a second supervisor, with the service manager behind a fake

**Acceptance**

- [ ] With a service registered, the program lists it with its state and refuses to start a supervisor of its own, naming the service (unit tests with a fake service manager)
- [ ] Start and Stop go through the elevation prompt, and declining it changes nothing (unit tests)
- [ ] Opening the service's panel never asks for elevation (unit tests)
- [ ] Dev-gated: after `--install-service` on the maintainer's Windows and Linux machines, the program shows the service, stops and starts it, and opens its panel. Needs the maintainer's machines

## 17.186 Reaching a panel through SSH

**Goal:** An operator manages a VPS whose panel stays bound to that machine's own loopback, never exposed to the internet, through an SSH connection the panel program opens with a pinned host key.

**Size:** M. **Depends on:** 17.180, 17.181

Added on 2026-09-27 at the maintainer's direction, as an opt-in beside TLS and pairing. Through a forward, the panel sees the program as a loopback peer, so 17.180's local links work and nothing about the panel listens beyond the machine. It rests on the SSH client and credential store decisions, settled on 2026-09-27 and recorded under Desktop programs and client data in doc/ARCHITECTURE.md.

**Deliverables**

- An SSH entry in the server list:
  - It holds the host, port and user, and the host key's SHA-256 fingerprint, pinned only after the operator confirms it or given in advance.
  - Sign-in uses the operator's SSH agent or a key file.
  - A key file's passphrase is kept in the operating system's credential store only when asked, and never in a file, through a credential store wrapper in `src/tools/shell`. Whichever of this milestone and 3.27 lands first builds that wrapper, and the other uses it.
- A forward from a loopback port the program picks to the panel's address on the remote machine:
  - It opens with the panel's window and closes with it.
  - The view is bound to that loopback origin, with a profile of the entry's own.
  - Any host key but the pinned one is refused before authentication.
- Sign-in needs no password when the SSH user may run `supervisor --panel-link` on the remote machine; otherwise the panel's own sign-in page is used
- libssh in `vcpkg.json` and in THIRD-PARTY-NOTICES.md, loaded as a shared library, as its LGPL-2.1 licence asks
- Tests: unit tests over the entry and the host key pin, and an integration test against a local SSH server that forwards to a supervisor on that server's loopback and opens its panel, Env-gated on `AMBROSE_TEST_SSH`

**Acceptance**

- [ ] Env-gated on `AMBROSE_TEST_SSH`: a host key other than the pinned one is refused before authentication, naming both fingerprints
- [ ] Env-gated on `AMBROSE_TEST_SSH`: through the forward, the program opens a panel bound only to the remote machine's loopback, and signs in with no password when the SSH user may run the link command
- [ ] A saved passphrase lives only in the credential store, and the program's data folder holds none (unit tests behind a fake store, and a search of the folder)
- [ ] Closing the panel's window closes the forward and leaves no port open (the integration test)
- [ ] Dev-gated: from the maintainer's desktop, a panel on a VPS or virtual machine bound to 127.0.0.1 is managed through SSH. Needs a second machine or a virtual machine

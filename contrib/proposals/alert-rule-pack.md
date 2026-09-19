<!-- Project Ambrose by Imjustchico: C-40 evidence-gated alert rule pack proposal. -->

# C-40: Evidence-gated alert rule pack

## Purpose

This proposal gives milestone 17.67 a reviewable shape for alert rules without pretending that an unmeasured threshold is a safe default. A rule is not promoted to an enabled default until an operator records the workload, hardware, observed baseline, and reason for the threshold. Until then, the rule remains disabled or explicitly labelled `candidate`.

The pack targets the conditions named by phase 17.67: crash loops, high tick time, high memory, low disk, failed backups, failed schedules, stale rows cleared after a crash, and sign-in lockouts. It does not add code, change a phase file, or claim that any of these conditions is implemented today.

## Candidate pack

| Rule id | Figure | Candidate threshold | Duration | Repeat limit | Severity | Why this is a candidate |
|---|---|---:|---:|---:|---|---|
| `crash-loop` | gameserver exits | 3 exits | 60 s | 1 per 15 min | critical | Matches the phase-17.67 acceptance example and avoids one alert per restart. |
| `tick-time-high` | maximum tick time | 100 ms | 5 min | 1 per 30 min | warning | A round number for review only; it must be replaced by a measured healthy baseline. |
| `memory-high` | private bytes | 85% of configured limit | 10 min | 1 per 60 min | warning | Leaves room below a hard limit and avoids a one-sample page during a backup. |
| `disk-low` | free bytes on a data volume | 15% free | 15 min | 1 per 6 h | critical | Gives an operator time to stop writes; the absolute free-space guard remains authoritative. |
| `backup-failed` | backup job result | any failed terminal result | 0 s | 1 per job | critical | A failed backup is actionable regardless of duration or machine size. |
| `schedule-failed` | schedule run result | failed or permission-revoked | 0 s | 1 per run | warning | Names the schedule record instead of producing a generic scheduler alert. |
| `stale-rows-cleared` | rows removed after crash recovery | at least 1 row | 0 s | 1 per recovery pass | warning | Makes cleanup visible without paging for a clean recovery. |
| `sign-in-lockouts` | lockouts | 5 in 10 min | 0 s | 1 per 30 min | warning | Detects a burst while avoiding a notice for one mistyped password. |

The `tick-time-high` and `memory-high` values are intentionally not production defaults. The pack must not enable them merely because the numbers are convenient.

## Evidence record required for promotion

For every threshold that is promoted, keep a private measurement record with:

1. the Ambrose revision and build configuration;
2. operating system, CPU, memory, storage type, and database placement;
3. the number of realms, accounts, connected clients, and active scheduled jobs;
4. workload duration and the command or scenario used;
5. p50, p95, p99, maximum, and sample count for the figure;
6. the proposed threshold and the margin above the healthy p99;
7. the false-positive observation period; and
8. the operator who reviewed the result and the date.

Only the summary belongs in a future public proposal or test fixture. Logs, addresses, credentials, captures, dumps, and client-derived data stay private and are never committed.

## Promotion and disproof rules

- A candidate is **unverified** until the evidence record exists. Unverified rules are disabled by default.
- A threshold is **promoted** only after two representative runs and one deliberately stressed run show that it catches the intended condition without firing during the healthy runs.
- A threshold is **refuted** if it fires during either healthy run, misses the stressed condition, or depends on a figure the running build does not publish.
- A long-lived condition sends one notification while true. A repeat is allowed only after its repeat limit and must retain the original group key.
- A crash loop groups exits by app and window; it does not send one notice per exit.
- A rule body contains the subject and figure only. It never includes a secret setting value, player email, address, or credential.

## Evidence and current limitation

The rule names, grouping behavior, repeat limits, delivery behavior, and privacy boundary come from `doc/PANEL.md` and phase 17.67. The three-second crash-loop example is also stated by that phase's acceptance checks. The numeric CPU-independent candidates above are proposals, not observations: this checkout had no built server or measured workload when this contribution was prepared, so they must not be treated as verified operating values. The first real server run that contradicts a candidate should replace it or mark it refuted rather than silently widening the threshold.

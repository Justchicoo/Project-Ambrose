<!-- Project Ambrose by Imjustchico: C-19 proposal for an operator incident workspace in the panel. -->

# C-19: Operator incident workspace

## Summary

Add an **Incident workspace** to the panel overview. It is a bounded,
permission-aware view for answering the first operational question:

> What changed, what is affected, and what can I safely do next?

The workspace is not a second log viewer and it is not an automatic
diagnosis engine. It joins the existing health status, live log, metrics,
audit, and power surfaces into one time-bounded investigation view. It must
remain useful when an app is stopped, a database is unavailable, or the
operator has read access but not control access.

This proposal fits the existing supervisor architecture: the browser talks
only to the supervisor, app data is relayed through the app admin APIs, and
every route and event is checked against the caller's permission and scope.

## Page layout

### 1. Scope and time window

The header contains:

- a required scope: node, app, realm, or whole panel;
- a time window: last 15 minutes, 1 hour, 6 hours, or a custom UTC range;
- the current data freshness timestamp;
- a refresh control that does not reset the selected window; and
- a clear statement when one source is unavailable or permission-filtered.

The default is the smallest useful scope and the last 15 minutes. The page
must never silently widen scope or time range.

### 2. Current impact

The first card shows facts, not guesses:

- apps and realms currently down, starting, stopping, or degraded;
- player count and connection count where the caller has the relevant
  `players.read` and status permissions;
- the oldest active alert in the selected scope;
- the last successful health sample; and
- whether a maintenance mode is active.

Every number links to the source page or event stream that produced it. An
unknown value is shown as **Unavailable**, with the reason, rather than zero.

### 3. Timeline

The central timeline merges events in UTC order. Each row has:

- event time and source;
- severity and a short localized sentence;
- app, realm, or node scope;
- correlation id when one exists;
- whether it is an alert, health transition, audit action, power event, or
  log event; and
- a link to the detail permitted for the caller.

Equal timestamps retain source order and a stable event id so a refresh cannot
shuffle the investigation. Repeated health samples are summarized, but state
transitions and every audit event remain visible.

### 4. Safe next actions

Actions are shown only when the required permission and scope are present:

- open live logs;
- acknowledge or silence an alert;
- enter or leave maintenance mode;
- reload configuration;
- restart one app; and
- open the audit entry for an earlier action.

Destructive power actions require the existing confirmation, reason, and
lock-holder flow. The workspace must not offer a kill action as a shortcut.
If an action is unavailable, the disabled control names the missing
permission or prerequisite without exposing hidden resource names.

### 5. Evidence bundle

Offer a **Copy evidence summary** action that places metadata on the
clipboard, never raw logs or payloads. The summary includes scope, UTC
window, revision, event ids, timestamps, statuses, metric names and
correlation ids. It excludes passwords, tokens, cookies, client files,
packet bytes, account identifiers, and full filesystem paths.

The action is a local browser copy operation. It does not upload evidence,
create a public link, or persist a new server-side artifact.

## API and event shape

Add a read-only incident query under the supervisor:

```text
GET /api/panel/incidents?scope=<id>&from=<utc>&to=<utc>&cursor=<opaque>
```

The response is versioned and paginated:

```json
{
  "v": 1,
  "scope": {"kind": "app", "id": "loginserver"},
  "from": "2026-09-19T12:00:00Z",
  "to": "2026-09-19T12:15:00Z",
  "fresh_at": "2026-09-19T12:15:02Z",
  "events": [
    {
      "id": "event-id",
      "at": "2026-09-19T12:04:11.502Z",
      "kind": "health.transition",
      "severity": "warn",
      "scope": {"kind": "app", "id": "loginserver"},
      "summary": "Login server health changed to degraded",
      "correlation_id": "request-id"
    }
  ],
  "next_cursor": null,
  "sources": {
    "health": "ok",
    "logs": "permission_denied",
    "audit": "ok",
    "metrics": "stale"
  }
}
```

The server owns event ordering, redaction, permission filtering, and cursor
validation. The browser must not merge untrusted timestamps or decide which
events are in scope. Event summaries are localized catalog keys plus typed
values; the API does not send arbitrary log text to a viewer without the
existing error-detail permission.

Live updates use the existing event socket with `incident.append`,
`incident.refresh_required`, and `incident.source_state` events. A dropped
socket event triggers a bounded refresh from the last cursor; it does not
pretend that the timeline is complete.

## Permissions and audit

Introduce no broad `admin` shortcut. The page requires `status.read` for its
current-impact card and `activity.read` for audit rows. Log details require
`logs.read`, metrics require `metrics.read`, and each action keeps its
existing permission and scope. A caller can therefore see a degraded status
without seeing account names, command arguments, or log bodies.

Opening the workspace is not an audit event. Copying an evidence summary,
acknowledging an alert, changing maintenance mode, reloading settings, and
every power action are audited with the caller, scope, reason where required,
result, and correlation id. Failed and refused actions are audited too.

## Failure and stale-data behavior

- A stopped app remains visible with its last known state and a freshness
  age.
- An unavailable app API marks only that source unavailable; it does not
  erase events from the supervisor or other apps.
- A database timeout shows an explicit partial result and a retry action.
- A clock disagreement uses supervisor receipt time for ordering and keeps
  the source timestamp as a separate field.
- A cursor that has expired returns a typed `cursor_expired` response so the
  client can restart the selected window.
- An event that cannot be redacted safely is omitted and increments a
  supervisor diagnostic counter; it is never sent as an unfiltered fallback.
- A missing permission produces a source state of `permission_denied`, not an
  empty list that looks like no activity occurred.

## Acceptance checks

1. A viewer with only `status.read` can open the page, see current impact and
   freshness, and cannot see logs, audit details, player identifiers, or
   action controls.
2. An operator can select one app and a UTC window, refresh it, and receive
   stable event ordering with a valid opaque cursor.
3. A stopped app, unavailable app API, stale metrics source, and denied audit
   source each render their own explicit state without clearing other sources.
4. A power action requires its existing permission, scope, confirmation,
   reason, lock, and audit behavior; the incident page adds no bypass.
5. Copying evidence contains only approved metadata and is never sent to the
   supervisor.
6. A dropped event socket message causes a bounded refresh and no duplicate
   event rows.
7. API and socket payloads pass schema compatibility checks, redaction tests,
   pagination tests, permission tests, and localization checks.
8. The page remains usable with no database connection and with the browser
   disconnected from the event socket.

## Deliberate non-goals

This page does not automatically restart an app, infer a root cause, expose
raw packet captures, display client-derived files, provide a global log
download, or replace the dedicated player, database, backup, and settings
pages. Those choices keep the first response safe and preserve the existing
permission boundaries.

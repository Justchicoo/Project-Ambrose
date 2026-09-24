<!-- Project Ambrose by Imjustchico: Documents the source log-category and logger-route audit tool. -->

# Ambrose log-category audit

This dependency-free Python tool compares production logging categories with
the category table in `doc/config/logging.md` and the `Logger.*` routes in
every shipped `.conf.dist` file. It reports:

- source categories not covered by the logging guide;
- source categories that have no shipped logger route;
- documented concrete categories that are not used by production source; and
- shipped logger prefixes that do not match a production category.

The audit skips `src/test/`, because test-only categories exercise the logging
format checks and are not production categories an operator must route.

## Run

From the repository root:

```powershell
python contrib\tools\log-category-audit\audit.py
```

Use JSON for CI or a follow-up note:

```powershell
python contrib\tools\log-category-audit\audit.py --format json
```

Use `--root` to inspect another checkout. The command exits `0` when no
finding is present and `1` when the audit reports a finding.

## Matching rules

The tool reads literal categories passed to `LOG_TRACE`, `LOG_DEBUG`,
`LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, and `LOG_FATAL`. It also resolves
translation-unit constants declared as `constexpr` or `const` character
strings, including shared `LogFilter` constants. Dynamic categories are not
guessable by a static audit and must be reviewed separately.

The logging guide's `server.<app>` row matches every `server.` category.
Logger names are prefixes at dot boundaries, and `Logger.root` routes every
category. A category therefore has a route when at least one shipped
`.conf.dist` contains a matching logger or root route.

## Verification

The tool reads repository source, documentation, and `.conf.dist` files only.
It does not read a client installation, captures, type dumps, credentials, or
generated client data.

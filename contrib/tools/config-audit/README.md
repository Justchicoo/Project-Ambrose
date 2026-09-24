<!-- Project Ambrose by Imjustchico: Audits shipped configuration defaults against the documented configuration tables. -->

# Ambrose configuration audit

This dependency-free Python tool compares every shipped `.conf.dist` file
with the matching option tables in `doc/config/`. It reports:

- options documented for an app but absent from its `.conf.dist`;
- options present in a `.conf.dist` but undocumented for that app; and
- defaults that differ between the `.conf.dist` and its documentation.

The audit reads repository files only and writes no files.

## Run

From the repository root:

```powershell
python contrib\tools\config-audit\audit.py
```

To audit another checkout:

```powershell
python contrib\tools\config-audit\audit.py --root C:\src\Project-Ambrose
```

Use JSON for CI or a follow-up note:

```powershell
python contrib\tools\config-audit\audit.py --format json
```

The command exits `0` when no drift is found and `1` when it finds any
missing, undocumented, or mismatched option. Malformed `.conf.dist` lines or
documentation rows are errors and exit `2`.

## Matching rules

The app name comes from the `.conf.dist` filename. For example,
`loginserver.conf.dist` is compared with `doc/config/loginserver.md`.
Documentation rows use the first table cell as the option key and the third
cell as the documented default. A documentation key containing a placeholder,
such as `App.<name>.Config`, matches one concrete segment in the shipped key.
The comparison preserves quoted configuration values, so a database
connection string is compared as its complete value rather than split at
semicolons.

The tool intentionally audits only shipped `.conf.dist` files and option
tables. It does not infer options from prose, environment-variable tables,
command-line switches, logging-category tables, or generated build output.

## Verification

The tool was run against the repository on 2026-09-22. It is intended to be
rerun after changing a `.conf.dist` file or its corresponding configuration
guide. No client installation, capture, type dump, credential, or generated
client data is required.

The audit reads only the shipped files under `src/`. A build folder, a worktree or any other copy of a `.conf.dist` holds whatever the commit it came from said, so counting those reports drift that the source does not have.

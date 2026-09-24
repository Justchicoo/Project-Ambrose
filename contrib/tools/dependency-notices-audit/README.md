<!-- Project Ambrose by Imjustchico: Documents the dependency and licence-notice audit tool. -->

# Ambrose dependency notices audit

This dependency-free Python tool compares direct dependencies in `vcpkg.json`
and the npm workspace manifests with `THIRD-PARTY-NOTICES.md`. It reports:

- a manifest dependency missing from the notices tables;
- a licence differing between the npm lockfile and its notice row; and
- a notice row with no matching direct manifest dependency.

The root npm workspace and the dashboard, launcher, and shared UI manifests
are checked. Internal `@ambrose/*` workspace packages are not third-party
dependencies and are excluded. npm licence values come from the corresponding
direct package entry in `package-lock.json`; vcpkg's manifest does not carry
licence fields, so those entries are checked by name and require the notice
row to state the licence and obligations.

## Run

From the repository root:

```powershell
python contrib\tools\dependency-notices-audit\audit.py
```

Use JSON for CI or a follow-up note:

```powershell
python contrib\tools\dependency-notices-audit\audit.py --format json
```

The command exits `0` when no findings are present and `1` when it reports a
finding. It reads manifests, the lockfile, and the notices document only.
It does not resolve packages, contact registries, or inspect client files.

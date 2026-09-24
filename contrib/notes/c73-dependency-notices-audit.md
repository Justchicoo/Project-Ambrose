<!-- Project Ambrose by Imjustchico: Records the C-73 dependency-notices audit result. -->

# C-73: Dependency and licence-notice audit

The C-73 checker was run from the repository root on 2026-09-22:

```powershell
python contrib\tools\dependency-notices-audit\audit.py --format json
```

It compared direct dependencies from `vcpkg.json`, the root npm workspace,
the dashboard, the launcher UI, and the shared UI package with
`THIRD-PARTY-NOTICES.md`. npm licence values were read from the matching
direct entries in `package-lock.json`; vcpkg dependencies were matched by
their documented aliases because the vcpkg manifest does not carry licence
metadata.

The JSON run is the audit baseline for this revision. It reports every
missing notice, licence mismatch, and notice row without a direct manifest
dependency. It found 9 missing notice rows: `crow`, `openssl`, `sqlite3`,
`gtest`, `@internationalized/date`, `clsx`, `tailwind-merge`,
`tailwind-variants`, and `tw-animate-css`. No licence mismatches or
unmatched notice rows remained after accounting for grouped packages and
transitive build tools explicitly described by the notices document. The
checker and its matching rules are documented in the
[tool README](../tools/dependency-notices-audit/README.md).

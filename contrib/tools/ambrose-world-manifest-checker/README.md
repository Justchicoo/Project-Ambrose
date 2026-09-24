<!-- Project Ambrose by Imjustchico: Usage and safety boundary for canonical world-manifest comparison. -->

# Ambrose world manifest checker

This dependency-free C++20 tool compares two operator-generated, canonical
tab-separated manifests: one exported from the Ambrose world database and one
derived from the operator's own client installation. It reports rows missing
from either side and rows whose values disagree. It does not parse client
archives, copy client data, connect to a database, or modify either input.

## Canonical manifest format

Each non-empty line has exactly four tab-separated fields:

```text
table<TAB>key<TAB>field<TAB>value
```

`table`, `key`, and `field` are ASCII identifiers. `value` is UTF-8 text with
tabs and line breaks rejected. The complete comparison key is
`table/key/field`; duplicate keys are errors rather than silently selecting
one value. Exporters must normalize ordering before writing, but the checker
also sorts rows so input order does not affect the result.

This deliberately small interchange format keeps the checker independent of
the future world schema and of client archive formats. An exporter may read a
private database or client installation at runtime, but the resulting manifest
must remain outside the repository.

## How to build

```powershell
cd contrib\tools\ambrose-world-manifest-checker
cmake -S . -B build
cmake --build build --config Debug --target ambrose-world-manifest-checker
```

## How to run

```powershell
.\build\Debug\ambrose-world-manifest-checker.exe `
  --world C:\Temp\ambrose-world.tsv `
  --client C:\Temp\ambrose-client.tsv
```

The report prints counts and disagreement keys, never values. Exit `0` means
the manifests match, `1` means differences were found, and `2` means an
input or format error. Inputs are bounded to 100,000 rows and 1 MiB per line.

## Clean-room and capture boundary

Read only an operator-owned world export and an operator-owned client export
at runtime. Keep both files outside Git and delete or rotate any associated
credentials after use. Do not use packet captures, account data, archives,
assets, type dumps, screenshots, or extracted client text as repository
fixtures. This tool prints metadata about disagreements only.

## Verification

Run `--self-test` for matching, missing-row, differing-value, duplicate-key,
and malformed-line cases. The self-test uses synthetic manifests in memory.

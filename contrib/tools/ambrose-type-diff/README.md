<!-- Project Ambrose by Imjustchico: A standalone C++ tool that compares two format-v2 type dumps without storing client data. -->

# Ambrose Type Diff

This tool compares two format-v2 type dumps produced from user-owned installations and reports what changed between them.

## How to build

```powershell
cd contrib\tools\ambrose-type-diff
cmake -S . -B build
cmake --build build --config Debug --target ambrose-type-diff
```

## How to run

```powershell
.\build\Debug\ambrose-type-diff.exe old-types.json new-types.json
```

The tool reads both dumps at runtime and prints only metadata and structural differences. It never modifies either file and never writes a dump, client asset, or extracted data to the repository.

The report includes revision and executable metadata, added or removed classes, changed class hashes, and added, removed, or changed property fields. It identifies a property by its class key and property name.

## Verification

The focused validation uses two small synthetic format-v2 dumps containing a changed revision, one added class, one removed class, one changed class hash, and property additions, removals, and field changes. Run it against dumps from two of your own installations before using the report for revision research.

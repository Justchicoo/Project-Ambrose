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

The report includes revision and executable metadata, added or removed classes, changed class hashes, and added, removed, or changed property fields. It identifies a property by its class key and property name, and names the field that changed with both of its values, so an offset moving does not read the same as a type changing.

A dump whose `version` is not 2 is refused by name rather than compared, because two dumps written to different formats cannot be compared field by field and a silent empty report is the worst way to say so. Metadata lines are printed only when they differ, so comparing a dump with itself prints nothing at all.

## Verification

The focused validation uses two small synthetic format-v2 dumps containing a changed revision, one added class, one removed class, one changed class hash, and property additions, removals, and field changes. Run it against dumps from two of your own installations before using the report for revision research.

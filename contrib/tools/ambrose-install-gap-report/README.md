<!-- Project Ambrose by Imjustchico: Usage and limits for the operator-owned install gap reporter. -->

# Ambrose install gap report

This dependency-free C++20 tool compares the class names in an operator-owned
format-v2 type dump with exact textual references in a selected Ambrose source
tree. It reports classes with no source reference and source files with no
known type-name reference as candidates for review. It does not claim that a
textual reference is a reader implementation, and it does not inspect or
copy client bytes.

## How to build

```powershell
cd contrib\tools\ambrose-install-gap-report
cmake -S . -B build
cmake --build build --config Debug --target ambrose-install-gap-report
```

## How to run

```powershell
.\build\Debug\ambrose-install-gap-report.exe `
  --dump C:\Temp\ambrose-data\type-dump.json `
  --source-root C:\Github\Project-Ambrose\src
```

The report prints only the dump revision, class names, reference counts, and
relative source paths. It never prints type-dump properties, client paths,
source contents, or extracted data. Source files are read only at runtime and
are not modified.

The dump must have `version: 2` and an object-valued `classes` member. The
source scan is bounded to 10,000 files and 4 MiB per file, and considers C/C++
source extensions only. A class name is counted when it occurs as a complete
identifier in source text. A missing reference is a review candidate, not
proof that a reader is absent; generated code, registration tables, aliases,
reflection, and indirect dispatch can produce false positives and negatives.

## Safety and clean-room boundary

Read a type dump produced from your own installation at runtime. Do not place
the dump, client files, captures, archives, extracted assets, or credentials in
the repository. The tool writes no output file and never runs the client,
launcher, patcher, a shell, or a network request.

## Verification

Run `--self-test` for the parser and identifier-boundary checks. For a real
report, use a private copy of your own type dump and a source tree, then review
the candidates before using them as engineering evidence.

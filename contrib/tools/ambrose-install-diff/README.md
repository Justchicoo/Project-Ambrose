<!-- Project Ambrose by Imjustchico: A standalone C++ tool that compares archive, zone, and locale files between two client revisions. -->

# Ambrose Install Diff

This tool compares two user-provided installation folders and reports which archive, zone, and locale files were added, removed, or changed.

## How to build

```powershell
cd contrib\tools\ambrose-install-diff
cmake -S . -B build
cmake --build build --config Debug --target ambrose-install-diff
```

## How to run

```powershell
.\build\Debug\ambrose-install-diff.exe C:\path\to\old-install C:\path\to\new-install
```

The tool reads both folders at runtime and writes only path names and sizes to standard output. It uses an in-memory hash to detect same-size changes, never modifies either installation, and never writes client data to the repository.

Archive files are recognized by `.wad` and `.kiwad` extensions. Locale files are files below a `Locale` directory or files with a `.lang` extension. Zone files are files below a `Zone` or `Zones` directory or files with `.zone`, `.nif`, or `.dds` extensions. Other files are ignored.

The hash is a deterministic 64-bit FNV-1a digest calculated while reading each file. It is a change detector, not a cryptographic identity; a changed file with an unchanged size and digest could therefore evade detection.

## Output

The report has one section for each supported category:

```text
[archives]
added Data/New.wad
removed Data/Old.wad
changed Data/Root.wad 1234 -> 1250
```

## Verification

The focused validation uses two temporary synthetic folders containing representative archive, zone, locale, ignored, added, removed, and changed files. Run the executable against two of your own installations before using the report for revision research.

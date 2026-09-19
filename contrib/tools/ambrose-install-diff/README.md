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

The tool reads both folders at runtime and writes only path names and sizes to standard output. It never modifies either installation, and never writes client data to the repository.

It reads as little as it can. A file is classified first and only then considered, so nothing outside the three categories is opened at all, and a digest is taken only when the same path exists in both installations at the same size, because a size that differs already proves a change. On a real installation that is the difference between reading tens of gigabytes and reading almost nothing.

Archive files are recognized by `.wad` and `.kiwad` extensions. Locale files are files below a `Locale` directory or files with a `.lang` extension. Zone files are files below a `Zone` or `Zones` directory or files with `.zone`, `.nif`, or `.dds` extensions. Other files are ignored.

The hash is a deterministic 64-bit FNV-1a digest calculated while reading each file. It is a change detector, not a cryptographic identity; a changed file with an unchanged size and digest could therefore evade detection.

A file that cannot be opened, because it is locked or unreadable, is named under `[unread]` at the end and the tool exits 3. The rest of the report still stands; an installation in use usually holds at least one such file, and losing the whole report to it would be worse than saying which file was missed.

## Output

The report has one section for each supported category:

```text
[archives]
added Data/New.wad 1250
changed Data/Root.wad 1234 -> 1250
changed Data/GameData.wad 8192 -> 8192 same size, contents differ
removed Data/Old.wad 900
```

## Verification

The focused validation uses two temporary synthetic folders containing representative archive, zone, locale, ignored, added, removed, and changed files. Run the executable against two of your own installations before using the report for revision research.

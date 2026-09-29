<!-- Project Ambrose by Imjustchico: Usage and output shape of the zone catalog exporter. -->

# Ambrose zone catalog

This dependency-free Python tool calls the built `extractor` against your own
Wizard101 install and prints the zone catalog carried in
[`contrib/findings/world/zone_catalog.json`](../../findings/world/zone_catalog.json).
Each zone has its path, KI string hash, first path component as `world`, and
the GameData WAD containing `gamedata.bin` whose stem the existing
`ZoneExtractor` validates against that path. This is the zone-data archive,
not a list of every asset WAD the zone may reference.

The tool writes nothing into the client install. Its output contains the same
zone names and ids as the finding, not client assets or extracted files.

## Run

```powershell
.\.venv\Scripts\python.exe contrib\tools\ambrose-zone-catalog\catalog.py `
  --extractor build\windows-msvc-x64\bin\Debug\extractor.exe `
  --client "C:\Program Files (x86)\Steam\steamapps\common\Wizard101" `
  --type-dump "C:\Users\<you>\AppData\Local\ProjectAmbrose\types\r806919.Wizard_1_610.json" `
  --json > C:\Temp\ambrose-zone-catalog.json
```

The JSON object has a `zones` array sorted by path. Each entry contains
`path`, `id`, `world`, and `archive`. Without `--json`, the tool still prints
canonical tab-separated manifest rows for use by existing manifest tools.
The extractor's zone/archive validation runs before output is printed.
`--self-test` verifies both output shapes and duplicate-path rejection without
reading an install.

The exporter does not write to the client install or contact any server.

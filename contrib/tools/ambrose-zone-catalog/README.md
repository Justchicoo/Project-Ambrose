<!-- Project Ambrose by Imjustchico: Usage and privacy boundary for the private zone catalog exporter. -->

# Ambrose zone catalog

This dependency-free Python tool calls the built `extractor` against your own
Wizard101 install and prints a private tab-separated catalog. Each zone is
identified by the KI string hash of its `m_zoneName`; `world` is the first
namespace in that zone path, and `archive` is the GameData WAD containing
`gamedata.bin` whose stem the existing `ZoneExtractor` validates against that
path. This is the zone-data archive, not a list of every asset WAD the zone
may reference.

The catalog is generated at runtime and is not committed. Keep redirected
output outside the repository: it contains paths from the client install.
`WizZoneData` does not have an `m_world` field, and `WorldHubZones.xml` only
maps hub worlds, so the namespace is the exact field this tool can report; it
does not claim an explicit world association where the client data has none.

## Run

```powershell
.\.venv\Scripts\python.exe contrib\tools\ambrose-zone-catalog\catalog.py `
  --extractor build\windows-msvc-x64\bin\Debug\extractor.exe `
  --client "C:\Program Files (x86)\Steam\steamapps\common\Wizard101" `
  --type-dump "C:\Users\<you>\AppData\Local\ProjectAmbrose\types\r806919.Wizard_1_610.json" `
  > C:\Temp\ambrose-client-zones.tsv
```

Each output line has the canonical manifest shape
`table<TAB>id<TAB>field<TAB>value`, with one `world` and one `archive` field
for each zone ID. The extractor's zone/archive validation runs before any rows
are printed. `--self-test` verifies the hash, TSV mapping and duplicate-path
rejection without reading an install.

## Verify the private output

```powershell
.\contrib\tools\ambrose-world-manifest-checker\build\Debug\ambrose-world-manifest-checker.exe `
  --world C:\Temp\ambrose-world.tsv `
  --client C:\Temp\ambrose-client-zones.tsv
```

The world manifest must also remain outside Git. This exporter does not write
to the client install or contact any server. Build the checker by following
[`ambrose-world-manifest-checker/README.md`](../ambrose-world-manifest-checker/README.md).

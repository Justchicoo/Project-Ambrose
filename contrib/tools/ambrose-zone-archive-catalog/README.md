<!-- Project Ambrose by Imjustchico: Prints a private table connecting each extracted zone to its world archive. -->

`zone_archive_catalog.py` runs the existing `extractor` against the user's own
installation and converts its private SQL output into tab-separated rows:

```text
zone<TAB>zone_id<TAB>world<TAB>zone_path<TAB>archive
```

The archive is the `Data/GameData/<zone path with slashes replaced by
hyphens>.wad` file containing that zone's `gamedata.bin`. The output is
intentionally written to standard output so it can be redirected outside the
repository. No client-derived table belongs in a checkout.

Run its self-test without a client:

```powershell
python contrib/tools/ambrose-zone-archive-catalog/zone_archive_catalog.py --self-test
```

Run it against a matching local install and type dump:

```powershell
python contrib/tools/ambrose-zone-archive-catalog/zone_archive_catalog.py `
  --extractor build/windows-msvc-x64/bin/Debug/extractor.exe `
  --client "C:\Program Files (x86)\Steam\steamapps\common\Wizard101" `
  --type-dump "$env:LOCALAPPDATA\ProjectAmbrose\types\r806919.Wizard_1_610.json" `
  > "$env:TEMP\ambrose-zone-archive-catalog.tsv"
```

The extractor's SQL remains in a temporary directory and is removed when the
command exits.

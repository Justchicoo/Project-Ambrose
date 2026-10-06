<!-- Project Ambrose by Imjustchico: Records the verified scope and current blocker in the F-58 trigger-class census. -->

# F-58 trigger class census investigation

## Status

This is an investigation note, not the F-58 census. The available sweep
confirmed the archive and root-file totals below, but it did not produce the
required count of every class instance nested in those files.

## Source and method

The source was my own `r806919.Wizard_1_610` client install and its matching
type dump. I ran the built `schemaprobe` across all GameData WADs with five
workers:

```powershell
.\build\windows-msvc-x64\bin\Debug\schemaprobe.exe `
  --client "<my client install>" `
  --type-dump "<matching type dump>" `
  --all-wads --threads 5 `
  --output "<private temporary report>"
```

The report and class-cache experiments remained in the local temporary
directory and are not included here.

## Observations

- The sweep covered 3,599 WAD archives, with 550,749 entries and 177,013 BINd
  files.
- It found 3,387 `triggers.xml` files, 3,387 `trigger_groups.xml` files, and
  3,387 `volumes.xml` files, each in 3,387 archives.
- Their BINd root hashes were `114994243` (`TriggerList`),
  `1021044609` (`TriggerGroupList`), and `460257136` (`TriggerVolumeList`).
  These root class names do not appear in the matching type dump; the local
  class cache does name them.
- Without a class supplement, the sweep could not decode those unknown roots,
  so it did not observe the objects nested beneath them.
- `schemaprobe`'s unknown-class report is not a complete class census: its
  `unknown_classes` collection counts classes missing from the active type
  catalog, while its `file_kinds` summary reports the root class and file
  totals. The zone extractor already reads volumes and triggers when the
  relevant server classes are supplied, but its printed summary is aggregate
  rows rather than counts of every nested class.

## Conclusion and next step

F-58 is still open. The confirmed root-file counts do not answer which classes
occur inside the three files, how many instances of each occur, or which of
those nested classes are absent from the type dump. A useful next tool
capability would be an occurrence-count report for both known and unknown
classes while decoding these files, retaining the class hash when a class has
no name in the dump.

This conclusion would be disproved by a repeatable run of the existing tools
that emits a complete per-class inventory and instance counts for all three
file kinds, including classes already present in the type dump and classes
whose hashes remain unnamed.

## Evidence boundary

No client files, decoded object contents, credentials, or scan reports are
part of this note. The reported numbers are archive, file, and class-hash
metadata only.

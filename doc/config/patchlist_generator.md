<!-- Project Ambrose by Imjustchico: Configuration reference for the install scanner manifest generator. -->
# Patchlist generator

The optional `patchlist_generator.conf.dist` file documents the install scanner's
rule format. Each non-empty, non-comment line is:

```text
SrcFileName|TarFileName|Package|FileType
```

For example:

```text
Data/GameData/GUI-WorldData.wad|Data/GameData/GUI-WorldData.wad|GUI-WorldData|5
```

The generator assigns file type `3` to WAD files and `1` to other files by
default. A rule may override this with type `5` for a WAD or type `4` for a
compressed-download variant. The package column may move a file between
`Base`, `PatchClient`, and the named WAD package.

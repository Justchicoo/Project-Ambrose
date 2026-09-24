<!-- Project Ambrose by Imjustchico: Usage and safety boundary for the SVG zone-object map renderer. -->

# Ambrose zone map renderer

This dependency-free C++20 tool renders a private, operator-generated zone
object manifest as an SVG map. It is a review aid for positions, headings,
layers, and object categories; it does not decode a WAD, copy a minimap, or
embed client assets.

## Manifest format

Each non-empty line has seven tab-separated fields:

```text
zone<TAB>id<TAB>kind<TAB>x<TAB>y<TAB>yaw_degrees<TAB>layer
```

`zone`, `id`, `kind`, and `layer` are identifiers. `x`, `y`, and `yaw_degrees`
are finite decimal numbers. The renderer filters rows to one requested zone,
normalizes coordinates into a fixed-size SVG view, draws an arrow for heading,
and uses a stable color derived from the kind identifier. It prints counts and
writes only the explicitly requested SVG output.

## How to build

```powershell
cd contrib\tools\ambrose-zone-map-renderer
cmake -S . -B build
cmake --build build --config Debug --target ambrose-zone-map-renderer
```

## How to run

```powershell
.\build\Debug\ambrose-zone-map-renderer.exe `
  --manifest C:\Temp\ambrose-zone\objects.tsv `
  --zone WizardCity/WC_Hub `
  --output C:\Temp\ambrose-zone\map.svg
```

The input is bounded to 100,000 rows and 1 MiB per line. Output dimensions
are fixed at 1600x1000, and coordinates are padded to keep edge objects
visible. Duplicate `(zone,id)` keys and malformed numbers are errors.
`--self-test` exercises parsing and SVG escaping using synthetic in-memory
rows.

## Clean-room boundary

The manifest may be produced from an operator's own installation at runtime,
but it must remain outside Git. Do not commit WADs, minimaps, textures,
extracted strings, captures, screenshots, credentials, or generated client
files. This tool reads only the manifest, never writes to the client install,
and emits no network traffic.

## Verification

Run `--self-test`, then render a private manifest to a temporary directory.
Review the SVG locally and delete it with the source manifest after recording
only non-sensitive metadata.

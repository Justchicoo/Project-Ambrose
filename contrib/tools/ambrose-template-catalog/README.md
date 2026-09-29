<!-- Project Ambrose by Imjustchico: Usage and privacy boundary for the ObjectData template cataloger. -->

# Ambrose template catalog

This Windows C++20 tool reuses Project Ambrose's `ObjectTemplateMgr` and
type-registry libraries to decode every `ObjectData/` entry in the client's
`TemplateManifest.xml`. It writes a private JSON report with the template
count for every decoded class, up to the top 100 classes with their direct
bases and ancestors, and the direct bases and ancestors shared by that ranked
set. When fewer than 100 classes occur, the report includes all of them.

The report contains metadata only: class names, counts and bases, which are
facts about the data, and a finding records the ones it needs, as
`contrib/findings/data/objectdata_template_catalog.json` does. The report file
itself is a run's output, so the tool refuses output paths inside the
repository or client install, and refuses to produce a partial report: a
missing archive, entry, decode failure, or unknown template class is an error.
It does not contact any server.

## Build

Use the same configured Windows build tree that contains Project Ambrose's
Debug static libraries and vcpkg dependencies:

```powershell
cmake -S contrib\tools\ambrose-template-catalog -B $env:TEMP\ambrose-template-catalog `
  -DAMBROSE_ROOT="$PWD" `
  -DAMBROSE_BUILD_DIR="$PWD\build\windows-msvc-x64"
cmake --build $env:TEMP\ambrose-template-catalog --config Debug
```

## Run

```powershell
& "$env:TEMP\ambrose-template-catalog\Debug\ambrose-template-catalog.exe" `
  "C:\Program Files (x86)\Steam\steamapps\common\Wizard101" `
  "C:\Users\<you>\AppData\Local\ProjectAmbrose\types\r806919.Wizard_1_610.json" `
  --output C:\Temp\ambrose-objectdata-templates.json
```

The executable requires the private output path and refuses to place it inside
the repository or client install. It writes the complete JSON report there
and progress or errors to the console. A nonzero exit means no complete
catalog was produced.

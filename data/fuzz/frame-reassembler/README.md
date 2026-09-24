<!-- Project Ambrose by Imjustchico: Synthetic frame-reassembler seed corpus and its expected parser outcomes. -->

# Frame reassembler fuzz seeds

These seeds are hand-written byte strings for the project's `0xF00D` frame
parser. They are synthetic inputs, not captures or client-derived files. Each
seed is deliberately small and stresses a different length, opcode, control
flag, or boundary condition from `FrameLayout` and `FrameReassembler`.

The repository currently builds the `objectproperty_fuzzer`; the frame
reassembler has deterministic unit coverage but no libFuzzer target yet. This
corpus is therefore kept as parser-ready input for that target without adding
a new target or changing build files.

The manifest records the expected parser result. Validate the corpus from the
repository root:

```powershell
python data\fuzz\frame-reassembler\validate.py
```

The validator checks every seed's provenance-free binary shape and confirms the
expected result against the frame-length rules implemented in `src/server/shared/Network/Frame.cpp`.

A long frame's declared length is the body alone, which is what `FrameLimits::LongLength` defaults to (`LongFrameLength::BodyOnly`). Under the other setting, `HeaderAndBody`, the same bytes give different answers, so a seed's expected result is only meaningful beside that setting.

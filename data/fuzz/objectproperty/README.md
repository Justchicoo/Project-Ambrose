<!-- Project Ambrose by Imjustchico: Safe handling and reduction instructions for ObjectProperty fuzz seeds. -->

# ObjectProperty fuzz seeds

This directory is the seed-corpus location for the `objectproperty_fuzzer`
target. The target writes the project's deterministic synthetic seeds into the
first non-option directory passed on its command line. Those seeds exercise
the decoder without reading a client installation or a packet capture.

## Generate the project-owned baseline

Build the fuzz targets with the repository's `linux-clang-fuzz` or equivalent
fuzzer-enabled preset, then run the target with a disposable output directory:

```powershell
objectproperty_fuzzer C:\Temp\ambrose-objectproperty-seeds -runs=1
```

The generated files contain a mode byte followed by the encoded test object.
Keep this output outside the repository while checking the target and its
limits. A maintainer may copy reviewed, deterministic seeds into this
directory after confirming that they contain no client-derived bytes.

## Reducing a private-capture input

Capture-derived seeds are optional additions, not replacements for the
project-owned baseline. Reduce a private input to the smallest byte sequence
that still reproduces the decoder behavior, prepend the mode byte expected by
the fuzzer, and run it through the target before review. Keep the original
capture, packet metadata, and reduction workspace outside the repository.

Before proposing a seed:

1. Confirm the input came from a disposable account and an Ambrose loopback
   session, or from a contributor-owned generator.
2. Remove credentials, session keys, addresses, timestamps, strings, and
   client-derived fields that are not required to reproduce the decoder path.
3. Verify that the final bytes are not a whole packet, stream, pcapng block,
   protocol XML fragment, type dump, archive, or configuration file.
4. Record only the seed filename, decoder target, reproduction result, and
   private-artifact hash in the contribution discussion.
5. Delete or securely retain the private source material according to the
   safe-session capture guide; never commit it as a fixture.

Do not add a capture-derived seed when the provenance or reduction boundary
cannot be explained. A synthetic seed from the project's own generator is
preferable to an unexplained byte sequence.

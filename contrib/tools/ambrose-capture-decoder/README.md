<!-- Project Ambrose by Imjustchico: Usage and safety boundary for the metadata-only pcapng capture decoder. -->

# Ambrose capture decoder

This dependency-free C++20 tool reads a private pcapng capture of an
operator's own loopback session and prints Ambrose frame metadata. It parses
the pcapng section/interface/enhanced-packet blocks, extracts IPv4/IPv6 TCP
payloads from Ethernet or raw-IP packets, and identifies complete Ambrose
frames in capture order.

## How to build

```powershell
cd contrib\tools\ambrose-capture-decoder
cmake -S . -B build
cmake --build build --config Debug --target ambrose-capture-decoder
```

## How to run

```powershell
.\build\Debug\ambrose-capture-decoder.exe `
  --capture C:\Temp\ambrose-capture\login.pcapng `
  --port 12100
```

The report contains packet and frame numbers, direction, frame kind, control
opcode or DML service/order, declared length, and body length. It never prints
timestamps, addresses, ports, payload bytes, strings, credentials, or fields
decoded from client data. The tool reads the capture but never modifies it.

The decoder supports little-endian pcapng captures with section headers,
interface descriptions, and enhanced packet blocks. It accepts Ethernet
IPv4/IPv6 and raw-IP loopback link types, handles TCP payloads in capture
order, and keeps at most 4 MiB per direction. Frames split across packets are
reported once their bytes are available; out-of-order or missing TCP segments
are reported as incomplete metadata rather than guessed.

## Safety boundary

Use only a private capture made from your own client and your own loopback
Ambrose server, as required by `doc/guides/safe-session-capture.md`. Keep the
pcapng and diagnostics outside the repository and delete them after recording
metadata. Do not pass a capture from another user or a public service. This
tool does not sanitize a capture; it only avoids printing payload contents.

## Verification

Run `--self-test` for synthetic pcapng block, TCP extraction, frame-header,
and truncation checks. The self-test creates no capture file.

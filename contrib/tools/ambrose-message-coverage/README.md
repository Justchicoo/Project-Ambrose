<!-- Project Ambrose by Imjustchico: C-24 message coverage tool usage and output contract. -->

# C-24: Message coverage

This standalone C++20 tool reads an Ambrose server log and reports the message
identifiers that were observed as received, sent, refused, or unhandled. It
does not need a running server, a client installation, a protocol XML file, or
a packet capture. It reads only the log path supplied by the operator.

## Build and run

```powershell
cd contrib\tools\ambrose-message-coverage
cmake -S . -B build
cmake --build build --config Debug --target ambrose-message-coverage
.\build\Debug\ambrose-message-coverage.exe logs\Server.log --pretty
```

The default output is JSON. `--pretty` indents it for a report. The result
contains counts and sorted message arrays for `received`, `sent`, `refused`,
and `unhandled`. A message may occur in more than one category. Lines that do
not contain the `network.opcode` category or a recognized message form are
ignored. Exit code `0` means the log was read, `1` means the log could not be
opened, and `2` means invalid arguments.

The parser recognizes names in the forms used by current Ambrose logs:
`received MSG_NAME`, `sent MSG_NAME`, `sending MSG_NAME`, and the refused or
unhandled forms documented by the logging guide, including `Unknown message`.
Unknown service/order pairs are retained as identifiers such as `1:2`.

Coverage is a lower bound. Per-session dropped-message logging is budgeted,
and a quiet log does not prove that no messages occurred. The tool reports
what the supplied log proves; it does not infer unseen message IDs or claim
protocol completeness.

## Self-test

```powershell
.\build\Debug\ambrose-message-coverage.exe --self-test
```

The self-test parses an in-memory synthetic log and checks all four categories
without writing a capture or storing client-derived data.

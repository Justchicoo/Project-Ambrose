<!-- Project Ambrose by Imjustchico: Usage and safety boundary for the machine-checkable finding validator. -->

# Ambrose Finding Check

This dependency-free C++20 tool validates the optional `machine_check` block
described in `contrib/proposals/machine-checkable-findings.md`. It checks the
block's schema, declared input names, placeholders, result contracts, and
repository-relative entrypoint without executing anything.

## How to build

```powershell
cd contrib\tools\ambrose-finding-check
cmake -S . -B build
cmake --build build --config Debug --target ambrose-finding-check
```

## How to run

```powershell
.\build\Debug\ambrose-finding-check.exe contrib\findings\protocol\example.json
```

The command exits `0` when every supplied finding is structurally valid, `1`
when a finding is invalid, and `2` for invalid command arguments or unreadable
files. A finding without `machine_check` is valid because the block is
optional. The report names fields and paths, but never prints input values,
client bytes, capture bytes, or command output.

The validator accepts schema `1` and kind `command`. Entrypoints must be
regular files below the finding's own directory. Arguments are an argv array;
the only interpolation recognized is `${name}`, and every placeholder must
refer to exactly one declared input. Declared inputs are metadata only: this
tool does not open, fetch, write, or execute them.

## Safety boundary

This tool is a dry structural check. It never starts a client, server,
launcher, patcher, shell, network request, or command from a finding. A future
runner must separately enforce the capture and client-install rules in
`doc/guides/safe-session-capture.md` and the proposal before execution.

## Verification

Use synthetic JSON fixtures for a valid block, an unknown schema, a missing
placeholder input, a traversal entrypoint, and a missing entrypoint. Do not use
captures, client installations, type dumps, credentials, or generated client
files as fixtures.

<!-- Project Ambrose by Imjustchico: C-21 machine-checkable finding verifier usage and result contract. -->

# C-21: Finding verifier

This dependency-free Python tool reads one finding JSON file and evaluates its
optional machine-checkable `checks` block. It reports `pass`, `fail`, or
`unable` without turning a missing client installation, server, or capture
into a false result.

## Check format

The base finding fields remain those documented in
`contrib/findings/README.md`. A finding may add:

```json
"checks": [
  {"name": "type dump exists", "kind": "file_exists", "path": "dump.json"},
  {
    "name": "local helper passes",
    "kind": "command",
    "argv": ["python", "tools/check.py"],
    "cwd": ".",
    "timeout_seconds": 10
  }
]
```

`file_exists` checks a path below the selected root. `command` runs an argv
array without a shell, only when `--run-commands` is supplied. Command
executables are restricted to `python`, `python3`, `cmake`, and
`ambrose-finding-check`; this prevents a finding from silently launching an
arbitrary shell or remote client. Commands run with the selected root as their
working directory unless `cwd` stays below that root. A non-zero exit is
`fail`; a missing executable, invalid path, timeout, or omitted command mode is
`unable`.

Run a finding without executing commands:

```powershell
python contrib\tools\ambrose-finding-verifier\verify.py `
  --finding contrib\findings\protocol\my-finding.json --root .
```

Allow declared local commands explicitly:

```powershell
python contrib\tools\ambrose-finding-verifier\verify.py `
  --finding contrib\findings\protocol\my-finding.json --root . --run-commands --pretty
```

Exit codes are `0` for `pass`, `1` for `fail`, `2` for invalid input, and
`77` for `unable`. The output contains no command stdout, payloads, capture
bytes, or credentials; it reports bounded status details only. A finding with
no `checks` is `unable`, because prose alone is not a machine verification.

## Self-test

```powershell
python contrib\tools\ambrose-finding-verifier\verify.py --self-test
```

The self-test uses temporary operator-authored files and a local Python
command, then removes them before exiting. It does not need a client,
database, network service, or capture.

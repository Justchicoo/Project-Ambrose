<!-- Project Ambrose by Imjustchico: Proposal for a safe machine-checkable block inside contributor findings. -->

# Proposal: machine-checkable findings

## Item

C-22: define the shape of a machine-checkable finding before the verifier in C-21 is built.

## Problem

The contributor findings format records a claim, evidence, repeat steps, and a disproof condition. That is enough for a person to review, but it leaves a verifier with no stable way to know what to run, what input it may read, or when a result is inconclusive.

A check must not turn an unverified finding into a fact. It must report whether the check passed, disproved the claim, or could not run with the available installation and server. It must also stay inside the project's clean-room rules.

## Proposed format

Add an optional `machine_check` object to a finding. Existing findings remain valid without it.

```json
{
  "machine_check": {
    "schema": 1,
    "kind": "command",
    "entrypoint": "checks/character-info.py",
    "arguments": [
      "--client",
      "${client}",
      "--capture",
      "${capture}"
    ],
    "inputs": [
      {
        "name": "client",
        "kind": "client_install",
        "required": true
      },
      {
        "name": "capture",
        "kind": "owned_capture",
        "required": true
      }
    ],
    "success": {
      "exit_code": 0,
      "result": "pass"
    },
    "failure": {
      "exit_code": 1,
      "result": "fail"
    },
    "unavailable": {
      "exit_code": 77,
      "result": "unable"
    }
  }
}
```

The block would have these rules:

- `schema` is a positive integer. A verifier refuses a schema it does not know.
- `kind` is initially `command`; later kinds need a new proposal or an explicit schema revision.
- `entrypoint` is a repository-relative path below the finding's own folder. It may not escape that folder.
- `arguments` is an argv array, not a shell string. `${name}` references an input by name; no other interpolation is performed.
- `inputs` declares every external value before it can be used. The verifier supplies paths or values at runtime and never reads a path not declared here.
- `client_install` and `owned_capture` inputs are local-only. The verifier must never fetch them, write inside them, or include their bytes in output.
- A check prints human-readable evidence to standard output and a short machine result to standard error in the form `result=pass`, `result=fail`, or `result=unable`.
- Exit code `0` means the claim's success condition was observed, `1` means the claim was disproved, and `77` means the check could not run because a declared prerequisite was unavailable.
- Any other exit code is an error in the check itself and is reported separately from a failed claim.
- A check may emit facts such as field names, offsets, sizes, counts, hashes, and frame numbers, but never client bytes, extracted assets, or capture payloads.

## Verifier behavior

C-21 should validate the block before execution:

1. Parse the finding with the existing findings validator.
2. Refuse an unknown `machine_check.schema` or `kind`.
3. Resolve the entrypoint relative to the finding directory and reject traversal.
4. Confirm that every placeholder has exactly one declared input.
5. Confirm that every required input is available and that its declared kind is allowed.
6. Run the entrypoint without a shell, with a bounded timeout and captured output.
7. Classify the result from the exit code and the required result marker.
8. Print `pass`, `fail`, `unable`, or `error` with the finding path and a redacted evidence summary.

The verifier should default to a dry validation that checks the block and reports unavailable inputs. An explicit run flag should be required before it starts a client or server process. This prevents reviewing a finding from unexpectedly launching local programs.

## Safety and scope

The first implementation should not support network URLs, shell pipelines, arbitrary environment-variable expansion, or commands supplied by a finding outside its own folder. It should run only tools already present in the repository or an explicitly selected local executable, and it should inherit the repository's rule that a user's client and captures remain outside version control.

The check result is evidence for a maintainer, not an automatic change from `claimed` to `verified`. A maintainer still records verification against the milestone and the exact evidence that justified it.

## Cheapest disproof

Before implementing C-21, create a fixture with one passing command, one command that exits 1, one missing input, one unknown schema, and one path-traversal entrypoint. If the proposed fields cannot distinguish those five cases without inspecting client bytes or launching an undeclared command, this proposal is wrong and should be revised.

## Dependencies and cost

This proposal depends on the existing findings shape and `ci_findings.py`. It does not require a client, a capture, a database, or a running server. C-21 would add the verifier and its fixtures later; no phase file or core build file needs to change for either contribution.

## Acceptance

The proposal is ready for maintainer review when a verifier author can implement the five fixture cases above without inventing another required field, and when a reviewer can explain from the block alone what the check may read, what it may execute, and why the result is pass, fail, unable, or error.

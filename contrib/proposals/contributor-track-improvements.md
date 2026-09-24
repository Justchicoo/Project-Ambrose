<!-- Project Ambrose by Imjustchico: C-18 proposal for clearer contributor-track prerequisites and acceptance evidence. -->

# C-18: Contributor-track improvements

## Summary

The contributor track already defines safe paths, clean-room limits, required
headers, and the need to say how work was checked. It does not yet give every
open item a consistent way to state what a contributor needs before starting
or what evidence a reviewer should expect at the end.

Add a small **item contract** to each open C item when the track is next
edited. This proposal does not change the allowed paths, the roadmap, or any
settled architecture decision. It makes the existing expectations machine-
and reviewer-friendly.

## Proposed item contract

Each C-item row or its linked detail should name:

| Field | Meaning |
| --- | --- |
| `deliverable` | The one file, folder, scenario, or report the item produces |
| `runtime_inputs` | What the contributor may read locally, such as a server log, private capture, or own installation |
| `forbidden_inputs` | Data that must remain private or must never be committed |
| `prerequisites` | Required build, server, database, client, operating system, or network conditions |
| `offline_path` | Whether a synthetic fixture or source-backed check can validate the work without those prerequisites |
| `acceptance` | Observable checks the pull request must report |
| `limitations` | What a passing check does not prove |

The values should be short prose or a bounded list. They are guidance for
contributors, not a second roadmap and not a promise that a maintainer will
run a contributor's private environment.

## Example

The C-23 capture replayer could be described as:

```text
deliverable: contrib/tools/ambrose-capture-replayer/
runtime_inputs: operator-authored manifest derived from a private local capture
forbidden_inputs: pcap/pcapng files, raw payloads, credentials, client files
prerequisites: Python 3.13; loopback endpoint for the focused test
offline_path: synthetic loopback echo fixture
acceptance: self-test passes; mismatch reports contain lengths and hashes only;
  contributor-path, codestyle and forbidden-file checks pass
limitations: a passing replay proves only the supplied vectors against the
  selected endpoint; it does not prove protocol completeness
```

This makes the safe-session-capture guide actionable without encouraging a
contributor to attach the capture itself.

## Selection guidance

Add a short prerequisite grouping above the open-item table:

- **No runtime dependency:** proposals, source-backed notes, synthetic tools,
  and documentation;
- **Own server or logs:** log watchers, coverage tools, and quality reports;
- **Own installation:** installation readers, type/data comparison, and
  client-derived checks that read only at runtime;
- **Private capture:** protocol findings and replay or decode work; and
- **Build or database:** work that needs a configured Ambrose build or a
  disposable database.

An item may belong to more than one group. The table should still be the
source of truth for scope and allowed paths.

## Acceptance evidence

Every pull request should include one compact verification block:

```text
Item: C-xx
Changed paths: ...
Focused check: command and result
Repository checks: paths, findings, codestyle, forbidden files, diff
Runtime prerequisites: available, skipped, or unable; explain why
Known limitation: what the result does not establish
```

When a prerequisite is unavailable, the contributor should not substitute a
success-shaped mock for the missing evidence. A synthetic check may validate
the tool's parsing or safety behavior, but it must be labeled synthetic and
the runtime limitation must remain visible.

## Maintenance rules

1. When an item is merged, keep its contract in the merged table or move it
   to a short archive section so later contributors do not repeat settled
   work.
2. If a prerequisite becomes obsolete because a phase lands, update the
   item contract rather than adding an informal exception in a pull request.
3. If two items depend on the same future shape, name the dependency and keep
   both items open until the maintainer decides whether they should merge.
4. Do not put private paths, account names, hostnames, credentials, capture
   hashes that identify a secret, or client revision files into the contract.
5. Keep acceptance commands reproducible from the repository root and use
   exit codes or explicit output rather than screenshots.

## Why this is worth adding

Clear prerequisites reduce wasted setup attempts, especially for work that
needs a retail client, a database, a second machine, or a private capture.
Explicit limitations prevent a synthetic test from being reported as a
protocol or gameplay result. A stable verification block also lets a
maintainer compare otherwise different contributions without requiring every
reviewer to reconstruct the contributor's environment.

The proposal is successful if a new contributor can select an open item,
determine whether it is feasible on their machine, identify the safe evidence
boundary, and report a reviewer-visible acceptance result without reading a
phase file or guessing at unstated prerequisites.

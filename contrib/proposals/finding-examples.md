<!-- Project Ambrose by Imjustchico: Proposal for two worked finding examples, one verified and one refuted. -->

# Proposal: worked finding examples

## Item

C-52: add a proposal for what a good finding looks like, with one verified example and one refuted example.

## Problem

The findings guide defines the required fields and explains that a merged finding starts as `claimed`, then changes to `verified` or `refuted` only after Ambrose re-derives it. It does not yet show both completed outcomes side by side. The examples must show all fields the checker requires for each status: `verified_by`, `verified_on` and `verified_how`, or `refuted_by`, `refuted_on` and `refuted_how`.

## Proposed change

Add two clearly labelled teaching examples to `contrib/findings/README.md`. Keep them in the guide rather than `contrib/findings/`, because they are not claims about Wizard101 and must not be mistaken for findings the project can cite.

Base the examples on existing synthetic frame tests so they are repeatable without a client, installation, capture, database, network connection or external source. The `revision` below is a schema-shaped synthetic placeholder, not a real client revision and not a value to copy into a real finding.

### Verified example

This example demonstrates a narrow claim and an observed passing check. It describes only the synthetic input in the named Ambrose test, not retail-client behavior.

```json
{
  "subject": "Synthetic frame control flag",
  "area": "protocol",
  "claim": "The synthetic frame input with control flag 2 is rejected as BadControlFlag.",
  "revision": "r0.Synthetic_FrameTest",
  "method": "experiment",
  "how_to_repeat": [
    "Build the unit test executable using the repository's documented CMake preset.",
    "Run `unit_tests --gtest_filter=FrameTest.ProtocolErrorsStopTheStream`.",
    "Check that the input with control flag 2 produces FrameError::BadControlFlag."
  ],
  "evidence": [
    "FrameTest.ProtocolErrorsStopTheStream passed.",
    "Its assertion expects the synthetic input with control flag 2 to produce FrameError::BadControlFlag."
  ],
  "disproof": "The focused test fails, or the same synthetic input is accepted or produces a different frame error.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-27",
  "status": "verified",
  "verified_by": "FrameTest.ProtocolErrorsStopTheStream",
  "verified_on": "2026-09-27",
  "verified_how": "The focused unit test passed and asserted BadControlFlag for the specified synthetic input."
}
```

### Refuted example

This example shows a plausible overgeneralization and the concrete counterexample that disproves it. It describes only the synthetic inputs in the named Ambrose test, not the retail protocol.

```json
{
  "subject": "Synthetic frame message count",
  "area": "protocol",
  "claim": "Every synthetic DML frame contains exactly one DML message.",
  "revision": "r0.Synthetic_FrameTest",
  "method": "experiment",
  "how_to_repeat": [
    "Build the unit test executable using the repository's documented CMake preset.",
    "Run `unit_tests --gtest_filter=FrameTest.ChainedDmlMessagesSplitInOrder`.",
    "Check the assertions for the decoded message counts and order."
  ],
  "evidence": [
    "FrameTest.ChainedDmlMessagesSplitInOrder passed.",
    "The test decodes a frame containing three DML messages and separately asserts that another frame contains two."
  ],
  "disproof": "A successful decode of any synthetic DML frame containing more than one message disproves the one-message claim.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-27",
  "status": "refuted",
  "refuted_by": "FrameTest.ChainedDmlMessagesSplitInOrder",
  "refuted_on": "2026-09-27",
  "refuted_how": "The test successfully decoded frames containing three and two messages, so the claim that every frame contains exactly one is false."
}
```

## Rules for the examples

- Keep both examples visibly labelled as synthetic teaching material and do not cite them as evidence about Wizard101.
- Preserve the status-specific fields required by `apps/ci/ci_findings.py`.
- A real finding must replace the synthetic revision with the revision actually observed and report only evidence someone actually collected.
- Name the check and the result it asserts; a test name alone is not enough evidence.
- Do not add captures, client files, protocol bytes or output copied from a client.

## Cheapest disproof

Run the two named unit tests. If either test is absent, fails, or no longer asserts the behavior described, the corresponding example is unsupported and must be updated before it is added to the guide. Validate both JSON objects with `python apps/ci/ci_findings.py --paths <example paths>`; if the checker rejects either object, it does not match the findings format.

## Dependencies and cost

This documentation-only proposal belongs in `contrib/proposals/`. Its implementation would edit only `contrib/findings/README.md`, which the contributor track allows, and would leave the finding schema and checker unchanged. Neither the proposal nor its examples require starting Wizard101 or accessing a client installation.

## Acceptance

The proposal is ready for implementation when the findings guide contains:

- one synthetic verified example with all required verification fields;
- one synthetic refuted example with all required refutation fields;
- repeatable checks and concrete evidence for both outcomes;
- a clear warning that neither example is evidence about Wizard101; and
- no client-derived data, captured bytes or copied external material.

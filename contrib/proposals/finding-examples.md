<!-- Project Ambrose by Imjustchico: Proposal for teaching finding quality with one verified and one refuted worked example. -->

# Proposal: worked finding examples

## Item

C-52: add a proposal for what a good finding looks like, with one verified example and one refuted example.

## Purpose

The findings guide already defines the JSON fields and explains that a merged finding starts as `claimed`. It also explains that later re-derivation changes the result to `verified` or `refuted`. Contributors still have to infer what enough evidence looks like and how a false lead should be recorded.

This proposal adds two small, synthetic examples to the findings documentation:

1. a claim that survives a repeatable check and is marked `verified`; and
2. a plausible claim whose check fails and is marked `refuted`.

The examples must be clearly labelled as teaching examples. They must not describe real Wizard101 behavior, use a real client revision as evidence, or be cited by a roadmap milestone.

## Example 1: verified

Use a harmless protocol-shaped fixture rather than a client message:

```json
{
  "subject": "Synthetic fixture field width",
  "area": "protocol",
  "claim": "The synthetic fixture's mode field occupies three bits and accepts values from 0 through 7.",
  "revision": "r000000.Synthetic_0_0",
  "method": "experiment",
  "how_to_repeat": [
    "Create the repository's synthetic fixture with the documented three-bit mode field.",
    "Run the focused fixture test with values 0 and 7, then run it with value 8.",
    "Inspect the test result and the encoded field width without copying any fixture bytes into the finding."
  ],
  "evidence": [
    "The focused test accepts values 0 and 7.",
    "The focused test rejects value 8.",
    "The decoder reports a three-bit field for mode."
  ],
  "disproof": "A focused fixture test that accepts 8, rejects a value from 0 through 7, or reports a field width other than three bits.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-19",
  "status": "verified",
  "verified_by": "Synthetic fixture validation",
  "verified_on": "2026-09-19",
  "verified_how": "The focused test accepted the stated boundary values, rejected 8, and reported the expected three-bit width."
}
```

The important pattern is not the fixture itself. The claim is bounded, the steps name the check, the evidence reports observations rather than intentions, and the disproof gives a result that would change the conclusion.

## Example 2: refuted

Use a second synthetic fixture to show that a failed claim remains valuable:

```json
{
  "subject": "Synthetic fixture terminator",
  "area": "protocol",
  "claim": "The synthetic fixture always ends with a zero-valued terminator byte.",
  "revision": "r000000.Synthetic_0_0",
  "method": "experiment",
  "how_to_repeat": [
    "Create the repository's synthetic fixture with the normal encoder.",
    "Decode several fixtures with the focused fixture test.",
    "Record the final byte value reported by the test without copying the fixture bytes into the finding."
  ],
  "evidence": [
    "The first fixture ended with a zero-valued byte.",
    "A fixture containing an optional field ended with a non-zero value.",
    "The decoder accepted both fixtures without treating the final byte as a terminator."
  ],
  "disproof": "A fixture whose final byte is non-zero while decoding succeeds, or decoder behavior showing that the final byte is not required to be zero.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-19",
  "status": "refuted",
  "refuted_how": "The optional-field fixture decoded successfully with a non-zero final byte, so the claimed terminator rule was false."
}
```

The refuted example should remain in the documentation as a model for recording a useful false lead. It must not be presented as a fact about the game or cited as protocol evidence.

## Rules for the examples

- Mark synthetic subjects, revisions, submitters, and evidence as examples so nobody mistakes them for findings about a client.
- Keep the fields valid for the findings checker, including the status-specific verification or refutation fields.
- Do not include client bytes, archive entries, captures, credentials, or copied text.
- State the cheapest experiment that could disprove the claim.
- Keep a verified example from becoming a guarantee: it is verified only against the named synthetic fixture and check.
- Keep a refuted example visible instead of deleting it; its value is showing how a plausible claim is corrected.

## Cheapest disproof of this proposal

Run `python apps/ci/ci_findings.py` against the proposed examples and ask a contributor unfamiliar with the findings guide to explain which example is verified, which is refuted, what evidence supports each, and what would disprove each. If the checker rejects the status-specific fields or the reader cannot distinguish the examples from real game findings, this proposal needs revision before implementation.

## Dependencies and cost

The implementation needs only the existing findings guide, its checker, and a small documentation change. It does not need a client installation, a packet capture, a database, a server build, or any external source. The examples should be added to the guide in the same pull request so the proposal does not alter the findings schema or checker.

## Acceptance

The proposal is ready for implementation when a maintainer can point to one findings-guide section that:

- contains one clearly synthetic verified example;
- contains one clearly synthetic refuted example;
- shows the required status-specific fields;
- gives repeatable evidence and a concrete disproof for each;
- contains no client-derived data or copied external material; and
- leaves the findings schema and checker unchanged.

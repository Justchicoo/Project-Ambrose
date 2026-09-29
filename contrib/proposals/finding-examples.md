<!-- Project Ambrose by Imjustchico: Proposal for two worked finding examples, one verified and one refuted. -->

# Proposal: worked finding examples

## Item

C-52: add a proposal for what a good finding looks like, with one verified example and one refuted example.

## Problem

The findings guide defines the required fields and explains that a merged finding starts as `claimed`, then changes to `verified` or `refuted` only after Ambrose re-derives it. It does not yet show both completed outcomes side by side. The examples must satisfy the same status-specific checks that `apps/ci/ci_findings.py` applies to real findings, so a contributor can copy their shape without guessing which fields each outcome needs.

## Proposed change

Add two clearly labelled teaching examples to `contrib/findings/README.md`. Keep them in the guide rather than `contrib/findings/`, because they are worked examples, and a milestone never cites them as findings.

Base both examples on facts about the r806919 client that the project's own tools check from outside the code, so each one teaches the kind of evidence the track accepts: an observation of the game made with a tool anyone can run against their own install, never the project's code agreeing with itself.

### Verified example

This example demonstrates a narrow claim and the observation that confirms it.

```json
{
  "subject": "ActorDialog in the type dump",
  "area": "objects",
  "claim": "In the r806919.Wizard_1_610 type dump, class ActorDialog has hash 971222955 (0x39e3afab) and six properties.",
  "revision": "r806919.Wizard_1_610",
  "method": "static",
  "how_to_repeat": [
    "Build the `client` tool with the repository's documented CMake preset.",
    "Run `client types \"class ActorDialog\" --client <install>` against an r806919.Wizard_1_610 install.",
    "Read the hash on the class line and the property count below it."
  ],
  "evidence": [
    "The command prints `class ActorDialog  hash 971222955`.",
    "It lists ActorDialogBase and PropertyClass as bases and prints `6 properties`."
  ],
  "disproof": "The same command against an r806919.Wizard_1_610 install prints a different hash or a property count other than six.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-27",
  "status": "verified",
  "verified_by": "client types \"class ActorDialog\" --client <install>",
  "verified_on": "2026-09-29",
  "verified_how": "The command printed hash 971222955 and 6 properties for class ActorDialog on a second r806919.Wizard_1_610 install."
}
```

### Refuted example

This example shows a plausible overgeneralization and the concrete counterexamples that disprove it.

```json
{
  "subject": "TYPE attributes in the message definitions",
  "area": "protocol",
  "claim": "Every field element in the r806919.Wizard_1_610 message definitions carries a TYPE attribute.",
  "revision": "r806919.Wizard_1_610",
  "method": "static",
  "how_to_repeat": [
    "Build the `client` tool with the repository's documented CMake preset.",
    "Run `client wad Messages/PhysicsBehaviorMessages.xml --client <install>` and find MSG_PHYSICS_GRAB's Force field.",
    "Run `client wad WizardMessages2.xml --client <install>` and find MSG_BATTLEGROUNDQUEUEUPDATE's Kicked field.",
    "Run `client wad WizardMessages.xml --client <install>` and find MSG_MINIGAMEREWARDS's GlobalID field."
  ],
  "evidence": [
    "MSG_PHYSICS_GRAB's Force field carries a TPYE attribute and no TYPE attribute.",
    "MSG_BATTLEGROUNDQUEUEUPDATE's Kicked field carries a TYP attribute and no TYPE attribute.",
    "MSG_MINIGAMEREWARDS's GlobalID field carries no attribute at all."
  ],
  "disproof": "Any field element in those definitions without a TYPE attribute disproves the claim that every field carries one.",
  "confidence": "high",
  "submitted_by": "example only",
  "submitted_on": "2026-09-27",
  "status": "refuted",
  "refuted_by": "client wad on PhysicsBehaviorMessages.xml, WizardMessages2.xml and WizardMessages.xml",
  "refuted_on": "2026-09-29",
  "refuted_how": "MSG_PHYSICS_GRAB's Force carries TPYE, MSG_BATTLEGROUNDQUEUEUPDATE's Kicked carries TYP, and MSG_MINIGAMEREWARDS's GlobalID carries no attribute, so not every field carries a TYPE attribute."
}
```

## Rules for the examples

- Keep both examples visibly labelled as worked examples in the guide, and never cite them from a milestone.
- Preserve the status-specific fields required by `apps/ci/ci_findings.py`.
- A real finding names the revision actually observed and reports only evidence someone actually collected.
- Name the command and what it printed; a command name alone is not enough evidence.
- Do not add captures, client files, protocol bytes or output copied from a client; the examples state names, hashes and counts, never file text.

## Cheapest disproof

Run the `client` commands in each example's `how_to_repeat` against an r806919.Wizard_1_610 install. If a command prints something other than what the example records, the example is unsupported and must be updated before it is added to the guide. Validate both JSON objects with `python -c "import re,json,sys; sys.path.insert(0,'apps/ci'); import ci_findings as c; t=open('contrib/proposals/finding-examples.md',encoding='utf-8').read(); f=chr(96)*3; [print(c.problems_for('contrib/findings/'+d['area']+'/example.json', d)) for d in map(json.loads, re.findall(f+'json\n(.*?)'+f, t, re.S))]"`, which must print `[]` twice.

## Dependencies and cost

This documentation-only proposal belongs in `contrib/proposals/`. Its implementation would edit only `contrib/findings/README.md`, which the contributor track allows, and would leave the finding schema and checker unchanged. Repeating the examples needs your own install and the built `client` tool.

## Acceptance

The proposal is ready for implementation when the findings guide contains:

- one verified example with all required verification fields;
- one refuted example with all required refutation fields;
- repeatable checks and concrete evidence for both outcomes;
- a clear label that both are worked examples a milestone never cites; and
- no client files, captured bytes or copied external material.

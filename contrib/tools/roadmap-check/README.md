<!-- Project Ambrose by Imjustchico: Documents the roadmap integrity checker. -->

# Ambrose roadmap checker

This dependency-free Python tool checks every phase file under `doc/roadmap/`.
It reports:

- dependencies that name no milestone;
- duplicate milestone identifiers;
- milestones missing either their phase table row or detailed definition;
- size labels that disagree with the roadmap's counting rule; and
- checked acceptance boxes without inline evidence.

The counting rule is the one stated in the phase 17 review notes: `S` has at
most four deliverables and five acceptance checks, `L` has eight or more of
either, and `M` is the remaining range. The two judgment-sized milestones
named by the roadmap, 17.24 and 17.51, are exempt from the count comparison.

## Run

From the repository root:

```powershell
python contrib\tools\roadmap-check\audit.py
```

Use JSON for CI or a follow-up report:

```powershell
python contrib\tools\roadmap-check\audit.py --format json
```

The command exits `0` when no findings are present and `1` when it reports a
finding. A checked box carries evidence when its line contains an inline
parenthesized reference or a backtick-delimited command, path, or test name.
This deliberately does not infer evidence from prose elsewhere in a phase.

## Baseline

Against the repository revision used for this contribution, the checker found
242 findings: 167 size-label mismatches and 75 checked acceptance boxes
without inline evidence. It found no duplicate IDs, missing milestones, or
unknown dependencies. The findings are the current roadmap audit output, not
automatic permission to change a phase file; a maintainer decides whether a
published size or acceptance claim should be corrected.

## Boundaries

The checker reads roadmap Markdown only. It does not edit roadmap files,
change milestone sizes, or decide whether an evidence claim is true.

The size rule is judged only where a milestone lists deliverables of its own, before any `###` subsection: a milestone whose body is a goal, a size and acceptance checks, with an appended detailed spec, has no deliverables to count and is left alone. `--require-evidence` adds the rule that a ticked acceptance box carries its evidence, which the phases written before that convention do not.

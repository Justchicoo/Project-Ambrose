<!-- Project Ambrose by Imjustchico: Records the C-72 documentation dead-link audit result. -->

# C-72: Documentation dead-link audit

The C-72 checker was run from the repository root on 2026-09-22:

```powershell
python contrib\tools\dead-link-check\audit.py --format json
```

It scanned every Markdown file under `doc/`, checked relative targets and
heading fragments, ignored external URLs and fenced code blocks, and reported
the result as JSON. The run found no missing targets, escaped targets, or
missing anchors.

The checker is [audit.py](../tools/dead-link-check/audit.py), and its matching
rules and limitations are in the [tool README](../tools/dead-link-check/README.md).

<!-- Project Ambrose by Imjustchico: Documents the documentation dead-link checker. -->

# Ambrose dead-link checker

This dependency-free Python tool checks Markdown links under `doc/`. It
reports:

- relative targets that do not exist;
- relative targets that escape the documentation tree; and
- fragments that do not match a heading anchor in the target document.

HTTP, HTTPS, mailto, and protocol-relative links are external and are not
fetched. Fenced code blocks are ignored. Both inline links and reference-style
links are recognized.

## Run

From the repository root:

```powershell
python contrib\tools\dead-link-check\audit.py
```

Use JSON for CI or a follow-up report:

```powershell
python contrib\tools\dead-link-check\audit.py --format json
```

Use `--root` to inspect another checkout. The command exits `0` when no
finding is present and `1` when the audit reports a finding.

## Anchor rules

Heading anchors use the GitHub-style lowercase slug: HTML tags and
punctuation are removed, spaces become hyphens, and duplicate headings get
the `-1`, `-2`, and later suffixes. URL-escaped paths and fragments are
decoded before checking.

## Boundaries

The checker reads Markdown under `doc/` only and writes no files. It does not
fetch external websites, inspect client installations, or alter links.

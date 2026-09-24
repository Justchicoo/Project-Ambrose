<!-- Project Ambrose by Imjustchico: C-32 standalone contrast auditor usage and output contract. -->

# C-32: Contrast auditor

This standalone Python tool reads a design-token JSON file and reports the
contrast ratio for every configured text pair and control-edge pair. It resolves
semantic token references and `ambrose.light` remaps, so the repository's
`design/tokens.json` can be audited without changing the design-token generator.

The tool does not read a client installation, network capture, or generated
client data. It has no third-party dependencies.

## Usage

From the repository root:

```powershell
python contrib\tools\ambrose-contrast-auditor\contrast.py design\tokens.json
```

The default output is a tab-separated report with the theme, foreground,
background, ratio, threshold, and verdict:

```text
theme	pair	ratio	threshold	verdict
dark	text on page	15.11	4.50	PASS
light	text on page	16.48	4.50	PASS
```

Use `--json` for machine-readable output. A malformed token file, unresolved
reference, invalid color, missing configured token, or failed ratio exits
non-zero and writes the reason to stderr. The default is 4.5:1 for text and
3:1 for the control edge.

## Audited pairs

The built-in pair list is intentionally small and explicit:

- `fg-body` against `surface-page`, `surface-card`, `surface-sunken`, and
  `surface-chrome`;
- `fg-muted` against the same four surfaces;
- `fg-faint` against the same four surfaces;
- `edge-control` against those four surfaces.

Text pairs use 4.5:1. `edge-control` uses 3:1 because it is a non-text
control boundary. The output names each pair so a design review can add or
remove a pair deliberately rather than silently accepting an incomplete scan.

## Verification

The focused checks for this contribution are:

```powershell
python contrib\tools\ambrose-contrast-auditor\contrast.py design\tokens.json
python contrib\tools\ambrose-contrast-auditor\contrast.py design\tokens.json --json
python apps\ci\ci_contrib_paths.py --paths contrib/tools/ambrose-contrast-auditor
python apps\ci\ci_findings.py
python apps\codestyle\codestyle.py
python apps\ci\ci_forbidden_files.py
```

The auditor verifies the arithmetic and token resolution. It does not prove
color-vision separation, browser rendering, forced-colors behavior, or that a
component uses the intended semantic token; those remain component-level checks.

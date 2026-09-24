<!-- Project Ambrose by Imjustchico: C-26 client-derived byte checker usage and finding contract. -->

# C-26: Client-derived byte checker

This dependency-free Python tool scans files from a proposed change for
content that must not enter Project Ambrose from a game-client installation.
It checks paths, extensions, known client-container signatures, protocol XML,
type-dump-shaped JSON, binary content, and the repository's one-megabyte file
limit. It does not open or inspect a client installation.

## Usage

Scan paths explicitly:

```powershell
python contrib\tools\ambrose-client-byte-checker\check.py --paths README.md data\example.json
```

Scan files changed by a Git range:

```powershell
python contrib\tools\ambrose-client-byte-checker\check.py --git-range upstream/main...HEAD
```

The default output is one JSON document containing a finding for each file
problem. Use `--pretty` for indented output. A clean scan exits `0`; a scan
with findings exits `1`; invalid arguments or an unreadable path exit `2`.
Findings include a stable rule id, path, evidence, and the byte count. The
checker never prints file contents or matching bytes.

## Rules

- `client-extension`: client archives, models, animations, or packet captures;
- `kiwad-signature` and `bind-signature`: known client archive/data containers;
- `protocol-xml`: client message-definition XML;
- `type-dump`: a JSON object shaped like the generated client type dump;
- `binary-content`: NUL bytes or a high proportion of non-text bytes;
- `oversized-file`: a non-dependency file over 1,000,000 bytes;
- `local-config`: a local `.conf` file rather than a `.conf.dist` template.

The checks are intentionally evidence-based. A clean result does not prove
that a human-authored text file was not inspired by a client; it proves that
these machine-detectable forbidden forms were not found. Reviewers must still
apply the clean-room rule.

## Self-test

```powershell
python contrib\tools\ambrose-client-byte-checker\check.py --self-test
```

The self-test creates temporary files containing synthetic signatures and
deletes them before exiting. The synthetic values are not extracted from a
client installation and are not committed.

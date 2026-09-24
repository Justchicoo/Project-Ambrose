<!-- Project Ambrose by Imjustchico: C-25 message-definition XML differ usage and output contract. -->

# C-25: Message-definition differ

This dependency-free Python tool compares two operator-supplied XML message
definition files and reports added, removed, and changed definitions. The
files are read at runtime and are never copied into the repository. The tool
does not require a game installation, does not launch a client, and does not
decode packet captures.

## Usage

```powershell
python contrib\tools\ambrose-message-definition-diff\diff.py `
  --before C:\private\revision-old\Messages.xml `
  --after C:\private\revision-new\Messages.xml `
  --pretty
```

The result is JSON containing the input paths, definition counts, and arrays
of `added`, `removed`, and `changed` definitions. A definition is keyed by its
`id` attribute when present, otherwise its `name` attribute. Definitions are
the direct child elements of the XML document's root; this avoids guessing
that unrelated nested metadata is a message. Each change includes canonical
before/after structure, with attributes sorted and whitespace-only text
normalized.

Exit codes are `0` when the files are equivalent, `1` when differences are
found, and `2` for invalid arguments, unreadable files, malformed XML,
duplicate keys, or unsupported empty definitions. `--self-test` uses temporary
synthetic XML and deletes it before returning.

The output is a structural diff, not proof that a revision is protocol-safe.
Reviewers must still confirm the revision, source provenance, and semantics
against their own installation. Never commit the XML files or extracted client
bytes.

<!-- Project Ambrose by Imjustchico: C-30 labelled synthetic Ambrose log corpus and validation contract. -->

# C-30: Labelled log corpus

This corpus contains synthetic Ambrose console lines labelled with the
operator searches they should support. It is intended as a golden input for a
future log value-classifier and as a compact review of the categories and
severity distinctions described in `doc/guides/logging.md`.

The lines are hand-written test data. They contain no client-derived text,
packet bytes, captures, credentials, real account names, or personal paths.
Identifiers such as `session 3` and `account 17` are deliberately synthetic.

## Corpus shape

`corpus.json` contains a `version` and an array of records. Each record has:

- `id`: stable case identifier;
- `line`: the complete console line;
- `level` and `category`: expected fixed-column metadata;
- `search_labels`: operator-oriented labels for the run or failure mode;
- `terms`: terms an operator would search for, each with a byte `start` and
  exclusive `end` offset in the UTF-8 line.

The labels describe why a line matters, not what a future parser must infer.
Several records intentionally share a label, because a search such as
`startup` or `authentication` should find more than one stage.

## Validation

Run from the repository root:

```powershell
python contrib\tools\ambrose-log-corpus\validate.py
```

The validator checks the schema, line metadata, UTF-8 byte spans, duplicate
case ids, and required label coverage. It exits non-zero with an explicit
error when a term is absent, an offset points at different bytes, or a record
is missing a required label.

This is a corpus check, not a log parser or a claim that a live server emits
every example verbatim. Runtime wording can change while the category, level,
and operator intent remain the fields a later classifier should preserve.

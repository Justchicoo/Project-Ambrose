<!-- Project Ambrose by Imjustchico: C-42 cron expressions and expected next runs for cross-surface schedule tests. -->

# C-42: Ambrose cron corpus

This corpus gives the future server scheduler and panel editor the same cases to run. It covers ordinary fields, names, lists, ranges, steps, both restricted day fields, Sunday as `0` and `7`, the `@hourly`, `@daily`, `@weekly`, `@monthly`, and `@yearly` macros, and daylight-saving gaps and overlaps.

The JSON contains no client data and the validator uses only Python's standard library. Each case has:

- an expression;
- an IANA time zone;
- an inclusive anchor instant;
- the expected next five scheduled instants in UTC;
- the expected local labels; and
- a note describing the edge case.

The expected values are test vectors, not a claim about a client installation. A scheduler or panel implementation should consume the same file rather than maintaining a second list of examples.

## Running the corpus

The current repository does not yet contain the cron parser named by the roadmap. Until that implementation lands, inspect the vectors as data:

```powershell
python validate.py
```

The validator checks JSON structure, timestamp syntax, and run ordering. When the parser exists, its unit test should also load `corpus.json`, calculate the next five runs from each anchor, and compare both UTC instants and local labels. A mismatch should name the case id, expression, zone, anchor, index, expected value, and actual value.

The DST cases intentionally make the policy visible. A spring-forward local time that does not exist is advanced to the first valid instant after the gap. A fall-back local time that occurs twice runs once at the first occurrence; the second occurrence is not a duplicate run. If the maintainer chooses a different policy, update the vectors and document that decision before using them as an acceptance test.

## Provenance and limitations

These vectors are hand-written from the cron semantics recorded in `doc/PANEL.md` and the phase-17 roadmap. They were checked for JSON syntax and internal ordering. They do not prove a scheduler implementation correct until a real parser executes them, and they do not replace tests for invalid expressions, impossible dates, or IANA time-zone database updates.

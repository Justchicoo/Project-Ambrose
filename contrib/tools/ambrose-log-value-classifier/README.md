<!-- Project Ambrose by Imjustchico: C-29 standalone log value-classifier usage and output contract. -->

# C-29: Log value-classifier

This dependency-free Python tool classifies safe, operator-useful values in an
Ambrose log line and returns their UTF-8 byte ranges. It is deliberately
conservative: fixed metadata such as the timestamp, level, and category is
classified, while arbitrary message text is left alone unless it matches an
explicit value class.

## Usage

Classify one line:

```powershell
python contrib\tools\ambrose-log-value-classifier\classify.py "21:12:55.502 INFO  [server.loginserver] Session 3 authenticated as account 17"
```

Classify lines from standard input:

```powershell
Get-Content logs\Server.log | python contrib\tools\ambrose-log-value-classifier\classify.py --stdin
```

The default output is one JSON object per input line. `--json` accepts a JSON
array from a file or stdin and emits an array of results. Every span contains
`start` and exclusive `end` offsets measured in UTF-8 bytes, the matched text,
and one class from the explicit vocabulary.

## Value classes

| Class | Example | Rule |
| --- | --- | --- |
| `timestamp` | `21:12:55.502` | Console short timestamp at the start of a line |
| `level` | `INFO` | One of `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` |
| `category` | `server.loginserver` | The category between the first brackets |
| `session` | `Session 3` | A synthetic session label and decimal id |
| `message` | `MSG_CREATECHARACTER` | Message names with an uppercase `MSG_` prefix |
| `account` | `account 17` | An explicit account label and decimal id |
| `address` | `127.0.0.1:12000` | An IPv4 or bracketed IPv6 address with an optional port, as one value |
| `quoted` | `"config.xml"` | Text inside double quotes, up to 120 characters |
| `path` | `/opt/ambrose/logs/Server.log` | A path of two or more segments, or a file name ending in a known extension |
| `measure` | `184 ms` | A number with a unit: `ms`, `us`, `ns`, `s`, `B`, `KiB`, `MiB`, `GiB` or `%` |
| `number` | `1138` | Standalone decimal numbers not already consumed by another class |

The classifier never treats an arbitrary word as an account, path, or message.
It does not return payload bytes, passwords, tokens, email addresses, client
paths, or client-derived text.

## Verification

Run the synthetic golden cases:

```powershell
python contrib\tools\ambrose-log-value-classifier\classify.py --golden
```

The command exits non-zero if an expected class or byte span differs. It also
checks a non-ASCII message to prove that offsets are byte offsets rather than
Python character indexes.

This is a lexical classifier for the current documented log shapes. It does
not prove that a future milestone's typed log ranges use the same vocabulary;
new classes must be added with a rule and golden case rather than inferred from
unstructured text.

An address is one span rather than four numbers, and a number without a unit is not a value. Both follow doc/DESIGN.md's rule for the values inside a log line: a bare count carries no meaning worth marking, and colouring the octets of an address separately makes the rainbow that rule exists to prevent. The golden cases cover an address with a port, a bracketed IPv6 address with a port, quoted text, a whole path and two measures, so a pattern that breaks one of them fails `--golden`.

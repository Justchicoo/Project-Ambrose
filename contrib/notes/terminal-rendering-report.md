<!-- Project Ambrose by Imjustchico: C-31 terminal rendering report and repeatable coverage matrix. -->

# C-31: Terminal rendering report

This note reports the terminal rendering behavior that is currently
source-backed and test-backed, then records which real terminal environments
still need visual review. It covers the console line at the 16-color, 256-color
and truecolor capability levels named by the contributor track. It does not
claim that an ANSI sequence looked identical on every emulator.

## Evidence reviewed

The report was derived from:

- `doc/guides/logging.md`, which defines the fixed columns, level-only color,
  redirected-output rule, `NO_COLOR`, `CLICOLOR_FORCE`, and named color
  settings;
- `src/common/Design/Tokens.h`, which records the generated semantic token
  values and their 256-color and 16-color mappings; and
- `src/test/common/Logging/AppenderConsoleTest.cpp`, which asserts ANSI
  sequences, legacy color attributes, redirected output, forced color, and
  `NO_COLOR`.

No client installation, capture, screenshot, credential, or client-derived
file was used.

## Rendering contract

The stable line shape is:

```text
HH:MM:SS.mmm LEVEL [category         ] message
```

The level is padded to five characters, the category column is 18 characters
wide in the documented configuration, and the message starts at the same
column. The level word carries the severity color. The timestamp and category
are quiet prefix material, and the message body is not tinted by severity.
Redirected output is plain text with the full date and no ANSI escape bytes.

| Capability or mode | Source/test evidence | Expected operator-visible behavior | Visual status |
| --- | --- | --- | --- |
| Truecolor-capable ANSI terminal | `AppenderConsoleTest.TerminalGetsAnsiColorsPerLevel` asserts ANSI level sequences | ANSI colors are emitted for the terminal path; level word remains the only severity-colored field | Sequence verified; emulator appearance unverified |
| 256-color token mapping | `Tokens.h` contains an `Index256` for every semantic token and series slot | Generated token choices are available to the terminal renderer without a second palette | Mapping inspected; terminal appearance unverified |
| 16-color terminal | `Tokens.h` contains an `Index16`; `LegacyFallbackUsesAttributes` asserts legacy attributes | The fallback keeps level distinctions using terminal attributes rather than ANSI truecolor | Attribute values verified; terminal appearance unverified |
| Redirected output | `RedirectedGetsNoEscapeSequences` asserts plain output | Files and pipes receive no escape bytes and retain parser-safe text | Verified by test |
| `NO_COLOR` | `NoColorEnvironmentDisablesAuto` asserts zero escapes | Automatic color is disabled even when the destination looks interactive | Verified by test |
| `CLICOLOR_FORCE` | `ClicolorForceUpgradesAuto` asserts ANSI on a redirected test device | An operator may force color for a destination that does not identify as a terminal | Sequence verified; downstream consumer behavior unverified |
| Custom colors | `CustomColorStringApplies` asserts named color application | Configured level/body colors are applied without changing the line structure | Sequence verified; emulator appearance unverified |

## Palette observations

The generated terminal table preserves semantic meaning at reduced color
capabilities rather than attempting to reproduce every RGB value. The dark
theme maps healthy to index 14, waiting to 11, wrong to 9, unknown to 8 and
the focus ring to 11. The light theme maps healthy to 6, waiting to 3, wrong
to 1, unknown to 8 and the focus ring to 3. The same state is therefore still
recognisable by its word and by a stable reduced-palette color.

The important limitation is that a terminal index is not a visual guarantee:
terminal palettes can be user-customized, and accessibility settings can
change perceived brightness. The state word must remain sufficient without
color, as required by `doc/DESIGN.md`.

## Coverage matrix for a real run

The following matrix is the minimum repeatable manual report. Use the same
synthetic lines and a clean terminal profile; do not paste a private server log
into the report.

| Environment | Capability to record | Checks |
| --- | --- | --- |
| Windows Terminal, default profile | ANSI support, theme, font | Fixed columns, level colors, category shortening, focus/readability |
| Windows Console Host or PowerShell host | ANSI support and legacy fallback | Same line with and without virtual-terminal processing |
| `xterm-256color` over SSH | `TERM`, palette and remote locale | 256-color state separation, long categories, redirected output |
| 16-color terminal | `TERM` or console mode | State words remain readable, level colors do not collapse into one apparent color |
| Truecolor terminal | `COLORTERM` and `TERM` | ANSI sequence display, body remains neutral, no background bleed |

For each row, record the terminal name and version, operating system, font,
theme, `TERM`/`COLORTERM` values where applicable, Ambrose revision, the
configuration used, and pass/fail results for each check. Do not record a
personal path or an account name.

## Repeatable synthetic check

Use a local build or the logging unit-test harness to emit these synthetic
records:

```text
INFO  [server.loginserver] startup ready
WARN  [network.opcode   ] refused synthetic message
ERROR [sql.driver       ] database unavailable
DEBUG [server.loading   ] detail record
```

Check that:

1. the level words remain exactly `INFO`, `WARN`, `ERROR`, and `DEBUG`;
2. the message begins in the same column for every category;
3. a long category is shortened only in the terminal column;
4. the debug line is dimmed as a detail line, not mistaken for a state color;
5. the redirected form contains no escape bytes; and
6. `NO_COLOR` removes automatic color while preserving all words.

The sample messages are synthetic and intentionally contain no client or user
data.

## Result and limitations

This C-31 contribution verifies the rendering contract against the repository's
source and unit-test expectations. It does not claim a completed visual survey
of Windows Terminal, Console Host, SSH, or a truecolor emulator because those
interactive environments were not all available for this run. A real terminal
run changes a matrix row only when it records the environment and observes the
checks above. A failure in any row should be reported with the terminal
settings and a minimal synthetic reproduction, not with a private log or
screenshot containing credentials.

<!-- Project Ambrose by Imjustchico: The one look every Ambrose surface uses: principles, colors, type, spacing and the rules each surface follows. -->

# Design

Everything Ambrose shows a person uses this one look: the launcher window, the panel in phase 17, the terminal dashboard and any page a server serves. A surface never invents its own palette or type. Settled on 2026-09-17 at the maintainer's direction, to be refined as surfaces are built.

## Principles

- **The identity is ours.** No KingsIsle art, wordmark, font or color appears anywhere in Ambrose. The only thing taken from the game is factual text, such as a revision number or a zone name.
- **One action per screen is gold.** Play on the launcher, the destructive confirmation in a dialog, the primary button of a form. Everything else is quiet.
- **State has a color, not an icon alone.** Healthy is teal, waiting is gold, wrong is red, unknown is grey, and every one of them also carries words.
- **Say what is guaranteed.** Screens that touch the user's own install repeat what Ambrose does and does not do with it, because that is why the program exists.
- **Numbers are monospaced.** Counts, timings, revisions, paths and log lines line up.
- **Accessible as drawn.** Real buttons, links, inputs and labels; icon-only controls carry a label; text meets 4.5:1 against its ground, and 3:1 at 24 px and above; every touch target is at least 44 px.
- **No decoration that carries no meaning.** No gradient washes, no drop shadows for their own sake, no emoji.

## Color

| Token | Value | Use |
|---|---|---|
| `ground` | `#0B1020` | The page behind everything |
| `chrome` | `#070B16` | Title bars, side bars, anything framing the page |
| `panel` | `#131B31` | Raised cards and controls |
| `sunken` | `#0E1527` | Inputs, log areas, anything set into the page |
| `border` | `#1B2540` | Quiet separators |
| `border-strong` | `#22304F` | Card and control edges |
| `text` | `#F2E8D5` | Body text on the ground |
| `text-muted` | `#A8B6D4` | Secondary text |
| `text-faint` | `#8798BC` | Labels, timestamps, hints |
| `gold` | `#E4B457` | The one action that matters, and waiting states |
| `gold-deep` | `#B98A2D` | The pressed edge of a gold control |
| `teal` | `#5FD3C4` | Healthy, done, verified |
| `violet` | `#C77DFF` | Marks something the user owns, such as their own wizard |
| `ember` | `#E2725B` | Errors and destructive actions |

Light surfaces, when a surface needs one, invert the ground and panels to parchment `#F4EAD5` and `#FFFDF7` and keep the same accents.

## Type

| Role | Family | Use |
|---|---|---|
| Display | Cormorant Garamond | Headings, the wordmark, the Play button |
| Interface | Karla | Every other piece of text |
| Mono | JetBrains Mono | Numbers, paths, logs, commands |

Sizes step 11, 12, 13, 15, 17, 21, 26, 34, 44, 56. Labels are 11 px, uppercase, letter-spaced 0.12em, in `text-faint`. Body is 13 to 15 px. A surface loads at most these three families and always names a fallback stack.

## Spacing, shape and motion

- Spacing steps 4, 6, 8, 10, 12, 14, 16, 20, 22, 28, 34, 40, 44.
- Radius 6 on small controls, 8 to 10 on inputs and cards, 12 on the largest action, 999 on pills.
- Borders are 1 px. The only shadow is the 3 px inset under a gold control.
- Motion is short and rare: 120 ms for a hover, 200 ms for a panel that opens. Nothing loops, nothing bounces.

## Surfaces

- **Launcher window** (3.26): the ground with a 44 px chrome bar, one gold Play, the realm and client state as three cards, and the guarantee line at the foot. Its first run shows each setup step with its real numbers.
- **Panel** (phase 17): the same tokens with a 236 px side bar on the chrome, cards on the ground, monospaced logs in a sunken area, and a gold action only in the top bar.
- **Terminal** (17.01 and 17.11): the same meanings in the 16 terminal colors, teal for healthy, gold for waiting, red for errors, grey for timestamps.
- **Anything a server serves** reads these tokens from the panel's own stylesheet, so a page cannot drift.

Mockups of the launcher and the panel exist outside the repository, because they are working pictures rather than code. This document is what the code follows.

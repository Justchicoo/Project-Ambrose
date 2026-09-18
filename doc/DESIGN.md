<!-- Project Ambrose by Imjustchico: The one look every Ambrose surface uses: principles, colors, type, spacing and the rules each surface follows. -->

# Design

Everything Ambrose shows a person uses this one look: the launcher window, the panel in phase 17, the terminal dashboard and any page a server serves. A surface never invents its own palette or type. Settled on 2026-09-17 at the maintainer's direction, to be refined as surfaces are built.

The values below are generated, not typed. `design/tokens.json` is the one place a design value is written, and `apps/designtokens/designtokens.py` writes from it the tables in this document, the stylesheet every web surface loads, the typed constants the apps import and the header the terminal reads, so this document cannot disagree with the code. 17.73 builds that pipeline and the component set on top of it; 17.06 and 3.26 are built from them. The generator refuses a palette whose text and ground pair falls below the ratio the accessibility principle sets, so a contrast failure cannot be shipped.

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

The names above are the raw values. A component never names one of them: it names a meaning, and the meanings are `surface-page`, `surface-card`, `surface-sunken`, `surface-chrome`, `edge-quiet`, `edge-strong`, `fg-body`, `fg-muted`, `fg-faint`, `action`, `action-pressed`, `state-healthy`, `state-waiting`, `state-wrong`, `state-unknown`, `mine` and `focus-ring`. Only the meanings reach the styling engine, and the raw palette below it is deleted from that engine, so a colour outside this document is not something a component can write. That is also what makes a second theme a remap of the meanings rather than a rewrite of every screen.

A light surface inverts the ground and panels to parchment `#F4EAD5` and `#FFFDF7`, and takes a darker ramp of the same accents, because the dark ones are unreadable on parchment: `gold #7A5A12`, `teal #0F6F63`, `ember #A33A25`, `violet #6B2FA0`. Every pair clears 4.5:1 on both parchments. Settled on 2026-09-18 after the ratios were computed: the dark accents reach only 1.5 to 3.1:1 there, so a status word or a health dot in them cannot be read.

Text on a filled accent is the dark ground, never parchment: ground on gold is 10.26:1 and ground on ember 6.12:1, while parchment on either is under 2.6:1.

A control is not identified by its border alone. `border` and `border-strong` sit near 1.2 to 1.5:1 against the grounds, which is below the 3:1 a control boundary needs, so what marks a control is its sunken fill and its label, with the border as quiet decoration. Focus is a 2 px gold ring, which clears 9.89:1 on every ground, and it is never removed.

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
- Borders are 1 px. The only shadow is the 3 px inset under a gold control. A border is a separator, never the thing that makes a control a control: `border` reads at 1.13:1 to 1.30:1 against the grounds and `border-strong` at 1.31:1 to 1.50:1, so an input is recognised by its sunken fill and its label, and the focus indicator carries the weight. `focus-ring` is 2 px of gold, which reads at 9.89:1 on the ground, and no component overrides it.
- A control whose ground is an accent takes the dark label, not the light one: parchment on gold is 1.58:1 and on ember 2.54:1, while `chrome` on gold is 10.26:1 and `ground` on ember 6.12:1.
- Motion is short and rare, on four durations only: 90 ms for a state that flips, 120 ms for a hover, 200 ms for a panel that opens, 320 ms for a screen that changes. Nothing bounces: no overshoot, no spring past its target, no attention-seeking movement.
- Exactly two things may repeat, and only while they mean something: an indeterminate indicator while a real operation is running, and the dot that shows a live connection. Both stop when the thing they report stops. Everything else plays once.
- Every duration becomes zero when the viewer asks for reduced motion, through the media query and through the setting the panel offers, because the media query is not reliable in every web view.

## Charts

Charts never borrow the four meaning-carrying accents for their series, because gold, teal, ember and violet already say something. A chart takes its colors from a series ramp of its own of at least seven steps, ordered so neighbours differ in lightness as well as hue, checked for the common kinds of color blindness, and readable on both grounds. Series keep their color across every chart on a page, a single-series chart uses the first step, and a threshold line or a danger band uses the meaning colors, since there it means what it says. The ramp's values are generated and gated with the tokens in 17.73.

## Surfaces

- **Launcher window** (3.26): the ground with a 44 px chrome bar, one gold Play, the server and client state as three cards, and the guarantee line at the foot. A realm is a world shard the player picks inside the game; the launcher chooses which Ambrose server to log in to. Its first run shows each setup step with its real numbers.
- **Panel** (phase 17): the same tokens with a 236 px side bar on the chrome, cards on the ground, monospaced logs in a sunken area, and a gold action only in the top bar.
- **Terminal** (17.01 and 17.11): the same meanings, teal for healthy, gold for waiting, red for errors, grey for timestamps, in truecolor when the terminal reports it and in the 256 or 16 colors when it does not. The three values per token are computed once by the generator and read from one header, so the terminal and the panel cannot drift. Nothing in the terminal animates except the focused entry's color, and `--no-motion` turns that off, on by default when the output is not a terminal.
- **Anything a server serves** reads these tokens from the panel's own stylesheet, so a page cannot drift.

Mockups of the launcher and the panel exist outside the repository, because they are working pictures rather than code. This document is what the code follows.

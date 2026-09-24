<!-- Project Ambrose by Imjustchico: The one look every Ambrose surface uses: principles, colors, type, spacing and the rules each surface follows. -->

# Design

Everything Ambrose shows a person uses this one look: the launcher window, the panel in phase 17, the terminal dashboard and any page a server serves. A surface never invents its own palette or type. Settled on 2026-09-17 at the maintainer's direction, to be refined as surfaces are built.

The values below are generated, not typed. `design/tokens.json` is the one place a design value is written, and `apps/designtokens/designtokens.py` writes from it the tables in this document, the stylesheet every web surface loads, the typed constants the apps import and the header the terminal reads, so this document cannot disagree with the code. Every table under a `###` heading here is written by that generator, and `designtokens.py --check` fails when one is edited by hand. 17.73 builds that pipeline and the component set on top of it; 17.06 and 3.26 are built from them. The generator refuses a palette whose text and ground pair falls below the ratio the accessibility principle sets, so a contrast failure cannot be shipped. `packages/ui` holds the components built on these tokens and doc/COMPONENTS.md says what each one is for.

## Principles

- **The identity is ours.** No KingsIsle art, wordmark, font or color appears anywhere in Ambrose. The only thing taken from the game is factual text, such as a revision number or a zone name.
- **One action per screen is gold.** Play on the launcher, the destructive confirmation in a dialog, the primary button of a form. Everything else is quiet.
- **State has a color, not an icon alone.** Healthy is teal, waiting is gold, wrong is red, unknown is grey, and every one of them also carries words. The word is what survives: a screen stays operable with the color gone, in forced-colors mode, in the sixteen terminal colors, and for a reader who cannot tell teal from gold.
- **Say what is guaranteed.** Screens that touch the user's own install repeat what Ambrose does and does not do with it, because that is why the program exists.
- **Numbers are monospaced.** Counts, timings, revisions, paths and log lines line up.
- **Hover intensifies, never reveals.** Hover only strengthens something already on screen. Anything it shows also appears on focus, and on a coarse pointer it is reachable without hovering at all.
- **Nothing is optimistic that the server arbitrates.** A filter, a sort, a column, a collapsed section, the theme and the density change at once. Anything the server decides waits for the server, the control that was operated shows a named pending state until the result arrives, and a refusal is shown where the action was taken, with its reason and its correlation id.
- **Accessible as drawn.** Real buttons, links, inputs and labels; icon-only controls carry a label; text meets 4.5:1 against its ground, and 3:1 at 24 px and above; targets follow the pointer, as Density and targets sets out.
- **No decoration that carries no meaning.** No gradient washes, no drop shadows for their own sake, no emoji.

## Color

The palette runs in three tiers. The raw values are the first tier and nothing outside this document names them.

### The palette

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
| `parchment` | `#F4EAD5` | The light page |
| `parchment-raised` | `#FFFDF7` | Light raised cards and controls |
| `parchment-sunken` | `#EADFC4` | Light inputs and log areas |
| `parchment-chrome` | `#FBF5E7` | Light title bars and side bars |
| `parchment-border` | `#D9CBAB` | Light quiet separators |
| `parchment-border-strong` | `#C3AE86` | Light card and control edges |
| `ink` | `#1B1608` | Body text on parchment |
| `ink-muted` | `#4A3F28` | Secondary text on parchment |
| `ink-faint` | `#5F5238` | Labels, timestamps and hints on parchment |
| `gold-dark` | `#7A5A12` | Gold that can be read on parchment |
| `gold-darkest` | `#6A4E0F` | The pressed edge of a gold control on parchment |
| `teal-dark` | `#0F6F63` | Teal that can be read on parchment |
| `edge-control` | `#546AA5` | A control with nothing else to show it exists |
| `value-number` | `#D1988F` | A number with a unit inside a message |
| `value-number-dark` | `#521915` | A number with a unit that can be read on parchment |
| `value-text` | `#B4A5FB` | Quoted text inside a message |
| `value-text-dark` | `#172075` | Quoted text that can be read on parchment |
| `value-name` | `#8EE4A1` | An identifier inside a message |
| `value-name-dark` | `#043415` | An identifier that can be read on parchment |
| `value-address` | `#28BDFA` | An address, URL or path inside a message |
| `value-address-dark` | `#00435C` | An address, URL or path that can be read on parchment |
| `value-setting` | `#E07AAE` | A configuration option inside a message |
| `value-setting-dark` | `#5B013A` | A configuration option that can be read on parchment |
| `violet-dark` | `#6B2FA0` | Violet that can be read on parchment |
| `ember-dark` | `#A33A25` | Ember that can be read on parchment |

The names above are the raw values. A component never names one of them: it names a meaning, and the meanings are `surface-page`, `surface-card`, `surface-sunken`, `surface-chrome`, `edge-quiet`, `edge-strong`, `edge-control`, `fg-body`, `fg-muted`, `fg-faint`, `action`, `action-pressed`, `state-healthy`, `state-waiting`, `state-wrong`, `state-unknown`, `mine`, `focus-ring` and the three value names in Log lines and values. Only the meanings reach the styling engine, and the raw palette below it is deleted from that engine, so a colour outside this document is not something a component can write. That is also what makes a second theme a remap of the meanings rather than a rewrite of every screen.

### The meanings

| Token | Dark | Light | Use |
|---|---|---|---|
| `surface-page` | `#0B1020` | `#F4EAD5` | The page behind everything |
| `surface-card` | `#131B31` | `#FFFDF7` | Raised cards and controls |
| `surface-sunken` | `#0E1527` | `#EADFC4` | Inputs, log areas, anything set into the page |
| `surface-chrome` | `#070B16` | `#FBF5E7` | Title bars, side bars, anything framing the page |
| `edge-quiet` | `#1B2540` | `#D9CBAB` | Quiet separators |
| `edge-strong` | `#22304F` | `#C3AE86` | Card and control edges |
| `edge-control` | `#546AA5` | `#546AA5` | A control boundary when nothing else shows it exists |
| `value-number` | `#D1988F` | `#521915` | A number with a unit inside a message |
| `value-text` | `#B4A5FB` | `#172075` | Quoted text inside a message |
| `value-name` | `#8EE4A1` | `#043415` | An identifier inside a message, such as an app, a database or a category |
| `value-address` | `#28BDFA` | `#00435C` | An address, URL, path or connection target inside a message |
| `value-setting` | `#E07AAE` | `#5B013A` | A configuration option inside a message |
| `fg-body` | `#F2E8D5` | `#1B1608` | Body text |
| `fg-muted` | `#A8B6D4` | `#4A3F28` | Secondary text |
| `fg-faint` | `#8798BC` | `#5F5238` | Labels, timestamps, hints |
| `action` | `#E4B457` | `#7A5A12` | The one action that matters |
| `action-pressed` | `#B98A2D` | `#6A4E0F` | The pressed edge of that action |
| `state-healthy` | `#5FD3C4` | `#0F6F63` | Healthy, done, verified |
| `state-waiting` | `#E4B457` | `#7A5A12` | Waiting |
| `state-wrong` | `#E2725B` | `#A33A25` | Wrong, and destructive actions |
| `state-unknown` | `#8798BC` | `#5F5238` | Unknown |
| `mine` | `#C77DFF` | `#6B2FA0` | Something the user owns, such as their own wizard |
| `focus-ring` | `#E4B457` | `#7A5A12` | The 2 px ring no component overrides |

A light surface inverts the ground and panels to parchment, and takes a darker ramp of the same accents, because the dark ones are unreadable on parchment: the dark accents reach only 1.5 to 3.1:1 there, so a status word or a health dot in them cannot be read. Settled on 2026-09-18 after the ratios were computed. Every light pair clears 4.5:1 on both parchments, which the generator proves before it writes a file.

A filled accent is the third tier. It keeps its bright value in both themes and takes the dark ground as its label, never parchment, because parchment on gold and on ember is far under the bar while the ground on either is far above it.

### Filled accents

| Token | Value | Use |
|---|---|---|
| `fill-action` | `#E4B457` | The primary button's ground |
| `fill-action-pressed` | `#B98A2D` | The primary button while pressed |
| `fill-danger` | `#E2725B` | The destructive button's ground |
| `fill-healthy` | `#5FD3C4` | A filled healthy badge |
| `fill-mine` | `#C77DFF` | A filled badge on something the user owns |
| `on-fill` | `#0B1020` | The label on any filled accent, in both themes |

A control is not identified by its border alone. `edge-quiet` and `edge-strong` sit near 1.2 to 1.5:1 against the grounds, which is below the 3:1 a control boundary needs, so what marks a control is its sunken fill and its label, with the border as quiet decoration. Settled on 2026-09-18, because a labelled control is not the whole panel: a control with nothing else to show it exists, such as an empty input, an empty search or filter box, the command box before anything is typed, an unchecked box, a switch in the off position or an unselected segment, draws its boundary in `edge-control`, which reads 3.59:1 on the ground, 3.24:1 on panel, 3.45:1 on sunken, 3.73:1 on chrome and 4.41:1 on parchment. One value serves both themes, because a mid-tone edge clears 3:1 against a dark ground and a parchment one alike. It is the only edge that carries that job; `edge-quiet` and `edge-strong` stay quiet separators at their own ratios.

Focus is a 2 px gold ring, which clears 9.89:1 on every ground, and it is never removed. It sits 2 px clear of the control it marks, so both of its neighbours are the surface behind it rather than the control's own fill: gold on ground 9.89:1, on panel 8.92:1, on sunken 9.49:1, on chrome 10.26:1. It is drawn with an outline and an offset rather than a shadow, so it survives forced-colors mode, and it appears on keyboard focus so a pointer click does not draw it.

A page stacks at most three tonal layers, ground, then panel, then sunken; a fourth is indistinguishable from the third. Every layer boundary also carries a 1 px edge, because the tonal steps are small by design: panel on ground is 1.11:1 and sunken on panel 1.06:1. Anything that floats, a dialog, a popover, a menu or the palette, is separated by a scrim over the page, never by a shadow.

## Type

A surface loads at most these three families and always names a fallback stack. The three are vendored as latin variable files in `packages/ui/src/fonts`, so no surface fetches a font from any host.

### Families

| Role | Family | Use |
|---|---|---|
| Display | Cormorant Garamond | Headings, the wordmark, the Play button |
| Interface | Karla | Every other piece of text |
| Mono | JetBrains Mono | Numbers, paths, logs, commands |

### Sizes

| Size | Line height |
|---|---|
| `11px` | `16px` |
| `12px` | `18px` |
| `13px` | `20px` |
| `15px` | `22px` |
| `17px` | `24px` |
| `21px` | `28px` |
| `26px` | `34px` |
| `34px` | `42px` |
| `44px` | `52px` |
| `56px` | `64px` |

Labels are 11 px, uppercase, letter-spaced 0.12em, in `fg-faint`. Body is 13 to 15 px.

- **Figures are tabular and reserve their width.** A number that changes on screen is rendered with tabular figures, and its box reserves its widest value, so nothing around it moves when it changes. JetBrains Mono is tabular already; Karla is not, so a figure in the interface face asks for it by name. A component test asserts a figure's width does not change between 0 and 1,000,000.
- **One formatter, and binary units are written by hand.** Memory, disk and file sizes are binary with the IEC symbols, and throughput matches them with the bit and byte case correct; counts and rates go through the platform's number formatting. A percentage is 0 decimals on a gauge and 1 on a trend, and a live figure's smallest and largest fraction digits are equal so its digit count cannot change under the eye. Every figure goes through one module, mirrored in C++ for the terminal, and no component formats a number itself.
- **Three time formats, each with its job.** Relative time where the question is how long ago, carrying the absolute time and the operator's zone in its own element. Absolute, zone-qualified time where the question is which moment: an audit row leads with the absolute time and carries the relative second, because an audit row is evidence. ISO 8601 wherever a value is copied or exported. Each format is tested against a fixed clock and a fixed zone, and the duration path is tested with the platform's duration formatting missing, because the launcher runs in whatever web view the user's distribution shipped.

## Spacing, shape and motion

- Spacing steps 4, 6, 8, 10, 12, 14, 16, 20, 22, 28, 34, 40, 44. A density attribute on the root scales that tier and nothing else, and a coarse pointer forces the comfortable scale back on so the 44 px touch target rule can never be violated.
- Radius 6 on small controls, 8 on inputs, 10 on cards, 12 on the largest action, 999 on pills.
- Borders are 1 px. The only shadow is the 3 px inset under a gold control. A border is a separator, never the thing that makes a control a control: an input is recognised by its sunken fill and its label, and the focus indicator carries the weight. `focus-ring` is 2 px, and no component overrides it.
- Motion is short and rare, on four durations only: 90 ms for a state that flips, 120 ms for a hover, 200 ms for a panel that opens, 320 ms for a screen that changes. Nothing bounces: no overshoot, no spring past its target, no attention-seeking movement. Animation touches only transform, opacity and filter.
- Exactly two things may repeat, and only while they mean something: an indeterminate indicator while a real operation is running, and the dot that shows a live connection. Both stop when the thing they report stops. Everything else plays once.
- Every duration becomes zero when the viewer asks for reduced motion, through the media query and through the setting the panel offers, because the media query is not reliable in every web view. Two things carry information through motion and each has a still form that the reduced-motion test renders: the live-connection dot becomes a filled dot with the word Live and the age of its sample, and the indeterminate indicator says in words that an operation is running.
- A pressed control changes color and takes the 3 px inset at 90 ms. It never scales, shifts or transforms, and the pressed state is set from the component's own state rather than from the active selector alone, because keyboard activation differs between the two web views Ambrose ships in. Compiled CSS carrying a transform under a pressed state fails the checks job.
- A figure that changes repaints once: one 320 ms fade of a tint behind it, no movement, and the tint is a state color only when the change crosses a threshold. It plays once, so it is not one of the two things allowed to repeat.
- Loading has three thresholds and there are no spinners. Under a second, no indicator at all. From 1 to 10 seconds, a still skeleton at the real final size, one fade, no shimmer. Past 10 seconds, a determinate step list with real figures that the operator may walk away from. A skeleton appears only after 400 ms and stays at least 400 ms once shown, so a fast query never strobes, and an empty state is never rendered while a load is in flight.
- Anything that updates itself carries a pause control and a count of what arrived while paused. A high-rate stream is never itself a live region: announcements are consolidated summaries on a separate polite region, at most one every few seconds, and the pause state and the jump-to-newest outcome are announced when they change. A card announces the age of its newest sample, not its figures.

## Charts

Charts never borrow the four meaning-carrying accents for their series, because gold, teal, ember and violet already say something. A chart takes its colors from a series ramp of its own, ordered so neighbours differ in lightness as well as hue, checked for the common kinds of color blindness, and readable on the grounds of its own theme. Series keep their color across every chart on a page, a single-series chart uses the first slot, and a threshold line or a danger band uses the meaning colors, since there it means what it says. The generator holds every slot above 3:1 on the page and on a card, and holds every pair of slots apart under normal vision and under protanopia, deuteranopia and tritanopia.

### The series ramp

| Slot | Dark | Light | Use |
|---|---|---|---|
| `series-1` | `#C9503F` | `#B45A5A` | Series one, and any single-series chart |
| `series-2` | `#B8C24E` | `#8A7A22` | Series two |
| `series-3` | `#79C98A` | `#2F5A22` | Series three |
| `series-4` | `#2A8F7C` | `#2D7E9C` | Series four |
| `series-5` | `#7D6BD6` | `#3B3AA8` | Series five |
| `series-6` | `#A560A5` | `#6A2A86` | Series six |
| `series-7` | `#E1A6C4` | `#8E2A5C` | Series seven |

A marker drawn on a chart is real markup positioned against the chart's scales, so it can be hovered, focused, read and translated. Nothing that carries meaning is drawn into a canvas, which is the rule a stat tile's number already follows.

## Log lines and values

A log line is colored by part, never by level. The timestamp is faint and the category quiet whatever the line says, the level word takes the level's color, the message is body text, and the only colored runs inside the message are its values. At debug and trace the whole record recedes to `fg-muted`, which is the one place a line-wide treatment is right. The layout is four columns, so the eye reads down a column instead of hunting for the start of each message:

| Column | Width | Content | Token |
|---|---|---|---|
| 1 | 12 + 1 | `HH:MM:SS.mmm` | `fg-faint`, brightening to `fg-muted` on the first line of a new second |
| 2 | 5 + 1 | the level word, padded to five as the file already pads it | the level's own color: teal healthy, gold waiting, ember wrong, ember reversed at fatal, faint at debug and trace |
| 3 | 18 + 2 + 1 | the category in brackets, padded, and shortened in the middle when it is longer | `fg-muted` |
| 4 | the rest | the message, wrapped at a word boundary with its continuation hanging under this column | `fg-body`, with value runs in the ramp below |

The message therefore begins at the same column, 40 by default, on every line, and punctuation between two adjacent value runs takes `fg-faint`. Only a terminal pads, shortens, wraps or colors: the file keeps the full date, no padding, no wrapping and no escape of any kind, and a redirected console writes exactly what the file writes. Every line of a record with several lines carries the full prefix, so text from a player cannot forge a line. The panel's console row is these same four columns as a grid, reading the runs from the record's own typed ranges rather than lexing text in a browser, so the terminal and the panel cannot drift.

A value does not take the level's color. Settled on 2026-09-18: a warning line gold from its level word to its values is the thing this rule exists to prevent, so values take a ramp of their own, beside the chart series ramp and outside the four meanings.

Settled again on 2026-09-22 at the maintainer's direction, after reading real output on the panel: the ramp was three steps of one blue, and on screen an address, a setting and a database name all came out the same blue, so a color said a run was a value without saying which kind. The ramp is now five hues, one per kind, and a kind is what the reader actually wants told apart.

| Token | What it marks | Dark | On ground, panel, sunken, chrome | Light | On parchment, light panel, sunken |
|---|---|---|---|---|---|
| `value-address` | an address, a port, a URL, a path or a connection target | `#28BDFA` | 8.79:1, 7.93:1, 8.44:1, 9.12:1 | `#00435C` | 8.98:1, 9.87:1, 8.10:1 |
| `value-name` | an identifier: an app, a database, a category, a version or a digest | `#8EE4A1` | 12.41:1, 11.20:1, 11.91:1, 12.88:1 | `#043415` | 11.66:1, 12.82:1, 10.52:1 |
| `value-setting` | a configuration option, which is a thing the reader can go and change | `#E07AAE` | 6.82:1, 6.16:1, 6.55:1, 7.08:1 | `#5B013A` | 11.64:1, 12.79:1, 10.50:1 |
| `value-number` | a number with a unit, such as 184 ms or 11 MiB | `#D1988F` | 7.75:1, 7.00:1, 7.44:1, 8.05:1 | `#521915` | 11.65:1, 12.80:1, 10.51:1 |
| `value-text` | text inside quotes | `#B4A5FB` | 8.76:1, 7.91:1, 8.41:1, 9.09:1 | `#172075` | 11.72:1, 12.88:1, 10.57:1 |

Each entry is two values, as the accents are, because no single value clears 4.5:1 on both the ground and parchment. Every value above clears 4.5:1 on every surface of its own theme, and the generator is what proved it: it refuses a ramp that does not. The five were not chosen by eye but searched for, under three constraints at once, which is why they are the colors they are: clear 4.6:1 on all four surfaces of the theme, stay 26 or more CIELAB units from gold, teal, violet, ember, body and faint, and stay as far from each other as that leaves room for. The result holds 35 units between any two in the dark theme and 30 in the light one, and 28 and 24 from the nearest accent, measured with normal vision.

The honest limit, said out loud, and it is a real cost of this change: five hues cannot all survive color blindness the way three steps of one lightness ramp did. Under deuteranopia the green and the salmon converge, and under tritanopia the blue and the green do. That is accepted rather than hidden, for three reasons: a value never carries state meaning, so nothing is lost but which kind of value it is; the level word keeps its own column and its own word, so no state is read from a hue anywhere; and 17.107 puts the kind in the hovercard as words, which is the reading that does not depend on seeing a hue at all. A bare count with no unit is never a value, and no line marks more than eight runs; without those two rules a line holding twenty numbers is a rainbow, which is the failure this ramp exists to avoid.

## Live data

- **Every live figure carries the age of its sample.** A figure on screen is a number and the time it was observed. Past its freshness budget, three sampling intervals by default, it drops to `fg-muted` and says how long since the last sample, in `state-unknown`. It is demoted, never blanked and never silently left looking current, because the expensive failure is a dashboard showing 1,284 players online four minutes after the samples stopped.
- **Connection state is four states, not one word.** Live; reconnecting, with the attempt and a countdown to the next one; disconnected, with what the close code means; and stale, where the socket is up but a stream has gone quiet. Data already on screen is never hidden while reconnecting, it is demoted by the rule above, and a page-level bar states the state in plain words and carries a retry control. Each close code renders its own message, because sign in again, your permissions changed and back off are three different things.
- **A command is never retried automatically.** Reconnection retries the connection, never a command or a power action: those are not idempotent and the operator may have given up. Retry is a button somebody presses.

## Errors and empties

- **An error message has four parts.** What failed, in the operator's words; why, when the server said; what to do next; and the correlation id, monospaced and selectable. It stays useful when the why is withheld, because full error text sits behind a permission and everyone else gets a generic message with an id.
- **Errors go where they happened.** A field error sits beside its field, a request-level failure at the top of the form, an operation failure on the operation, and a component that could not load says so in its own place without blanking the page. Ember and the waiting mark are for failures, never for ordinary status, which is the rule most often broken and the one that teaches operators to ignore red.
- **Three empties, and the component says which.** `empty`, nothing has been created yet: one sentence on what this area holds, the guarantee or constraint where one applies, and the one action that fills it, which is the only place a second gold action is allowed on a page. `filtered`, which names the active filters and offers to clear them. `denied`, which names the permission. A figure whose owning milestone has not landed reports as unavailable naming that milestone, never as zero. Every collection component ships the four states as stories, loading, empty, zero results and error, so this is structural rather than advisory.

## Density and targets

- **Density scales spacing only.** Comfortable is the default and compact reduces the spacing tier and nothing else. Type sizes do not scale, 11 px being the floor, and no control inside an overlay, a menu item, a dropdown item or a picker cell, ever takes the compact tier, because those are already at the target-size floor. A coarse pointer forces comfortable back on, and the choice is remembered per user.
- **Row heights come from one ladder.** Data rows are 32 px compact and 40 px comfortable, 48 px where a row carries two lines, and the header row matches the body. A console log row sits on the same ladder, so a log row and a table row are the same height on the same page.
- **Target size follows the pointer.** On a coarse pointer every target is at least 44 px. On a fine pointer every target is at least 24 by 24 CSS pixels, or has 24 px of clearance to its neighbours measured centre to centre. Settled on 2026-09-18: the 44 px rule holds where it protects somebody, a finger on a phone, and a 32 px compact row cannot hold a 44 px control, so the fine-pointer floor is the one the standard sets and never lower. A review catches a breach by measuring, not by reading: each dense page is walked once under each pointer type, every interactive element's box and its centre-to-centre distance to its neighbours recorded, and anything under its floor named with its page.

## Accessibility beyond the gate

These are the parts an automated gate cannot see, so each one names what proves it.

- **Forced-colors mode is a supported mode.** The panel is checked once in the operating system's high-contrast mode, with a story rendered in it asserting every status region still contains its word. Boundaries that matter are drawn with an outline or a border, never a shadow, so they survive, and the focus ring likewise. Ambrose's whole state language is background and text color and its elevation is tonal, so all of it collapses at once there; the insurance is that every state also carries a word, and this is the test that proves the insurance pays out.
- **Sticky chrome never covers the focused row.** A scroll container with sticky chrome carries scroll padding matching the sticky heights, and focusable rows carry scroll margin. Proved by tabbing to the last visible log row and asserting its box does not intersect the sticky elements.
- **Signing in accepts a paste.** No authentication field blocks paste or autofill, including a one-time code; every recovery code carries a copy control; and password and code fields name their autofill purpose. Proved by filling both fields from the clipboard and asserting the values arrive.
- **Icons carry labels, with a closed list of exceptions.** An icon stands alone only when the text beside it already names the control, when it is close, expand, collapse, copy, external link or drag handle, or when it is redundant to a word already on screen. Everywhere else it carries a visible label, and an accessible name is never supplied by a tooltip alone.
- **A toast is polite, closable and never the only record.** Toasts are read as one message, carry a close control, pause their timer on hover and on focus within, and hold no interactive control other than close; error toasts do not dismiss themselves. Everything a toast says also lands in the notification centre, and for an operator action in the audit log. An action that lives only in a toast is unreachable for somebody using a virtual cursor and gone for everyone else in four seconds.

## Surfaces

- **Launcher window** (3.26): the ground with a 44 px chrome bar, one gold Play, the server and client state as three cards, and the guarantee line at the foot. A realm is a world shard the player picks inside the game; the launcher chooses which Ambrose server to log in to. Its first run shows each setup step with its real numbers.
- **Panel** (phase 17): the same tokens with a 236 px side bar on the chrome, cards on the ground, monospaced logs in a sunken area, and a gold action only in the top bar.
- **Terminal** (17.01 and 17.11): the same meanings, teal for healthy, gold for waiting, red for errors, grey for timestamps, in truecolor when the terminal reports it and in the 256 or 16 colors when it does not. The three values per token are computed once by the generator and read from one header, so the terminal and the panel cannot drift. Nothing in the terminal animates except the focused entry's color, and `--no-motion` turns that off, on by default when the output is not a terminal.
- **Anything a server serves** reads these tokens from the panel's own stylesheet, so a page cannot drift.

Mockups of the launcher and the panel exist outside the repository, because they are working pictures rather than code. This document is what the code follows.

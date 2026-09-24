<!-- Project Ambrose by Imjustchico: C-33 proposal for a computed light-theme accent ramp. -->

# C-33: Light-theme accent ramp

## Proposal

Add a five-step violet ramp for light-theme accents that communicate
selection, ownership, or a secondary interactive affordance. The ramp is
intentionally separate from the four semantic accents already settled in
`doc/DESIGN.md`:

- gold remains action and waiting;
- teal remains healthy and verified;
- ember remains wrong and destructive; and
- violet remains ownership and user-specific state.

The ramp is not a replacement for those meanings, and it is not the chart
series ramp. A component would choose one semantic meaning first, then use a
ramp step only where the component needs an ordered strength or a repeated
selection treatment.

## Proposed values

| Step | Value | Suggested use |
| ---: | --- | --- |
| 1 | `#5B2A86` | strongest selected or owned state |
| 2 | `#63308F` | selected state |
| 3 | `#6B2FA0` | default ownership accent; matches the existing `violet-dark` primitive |
| 4 | `#7440AA` | secondary ownership or hover state |
| 5 | `#7E4BB3` | lightest accent that remains readable as text |

The ordering is increasing lightness. It is therefore usable for ordered
selection strength, but it must not be used to imply severity or time without
a label. The ramp should be referenced through semantic names rather than
literal hex values if it is accepted; this document deliberately does not
edit the source token file.

## Contrast evidence

Ratios below use the WCAG relative-luminance formula, with the proposed color
as foreground:

| Step | On parchment `#F4EAD5` | On raised `#FFFDF7` | On sunken `#EADFC4` |
| ---: | ---: | ---: | ---: |
| 1 | 8.29:1 | 9.74:1 | 7.48:1 |
| 2 | 7.48:1 | 8.79:1 | 6.75:1 |
| 3 | 6.92:1 | 8.13:1 | 6.24:1 |
| 4 | 5.75:1 | 6.75:1 | 5.19:1 |
| 5 | 4.96:1 | 5.82:1 | 4.47:1 |

Steps 1 through 5 clear 4.5:1 on the page and raised surfaces. Step 5 is
below 4.5:1 on the sunken surface, so it must not be used for ordinary text
there; use step 4 or darker, or place the value on a raised surface. None of
the step-to-step ratios is intended to satisfy a text contrast threshold:
adjacent ramp values are neighboring choices, not foreground/background
pairs.

For filled badges, use the existing `on-fill` treatment and test the complete
foreground/background pair. Do not place parchment text on these accents.

## Color-vision and non-color requirements

The ramp is a lightness ramp within the violet family. That reduces hue
dependence, but it does not make color the only signal:

- every selected or owned state also carries a word, icon, outline, or
  position change;
- a ramp step never means success, warning, failure, or unknown by itself;
- selected controls retain the focus ring and keyboard state;
- a disabled state uses disabled semantics, not a lighter violet step;
- charts use their dedicated series ramp instead; and
- a forced-colors mode must preserve the state word and boundary.

The existing design requirement to test protanopia, deuteranopia, and
tritanopia should be applied to the accepted ramp. Under those simulations,
violet can converge toward blue or teal, so the ownership meaning must remain
available in text and structure. This proposal does not claim that the ramp
has passed a simulator; it provides the measured contrast evidence and the
acceptance procedure below.

## Acceptance procedure

1. Compute every proposed foreground/surface ratio with the same luminance
   formula used by the design-token check.
2. Reject any step below 4.5:1 on a surface where it is used as ordinary
   text.
3. Render every step on parchment, raised, and sunken surfaces in normal,
   protanopia, deuteranopia, and tritanopia previews.
4. Check that an ownership or selection state remains identifiable when all
   color is removed.
5. Check keyboard focus, hover, pressed, disabled, and selected states without
   relying on hue alone.
6. Verify that no component maps a ramp step to healthy, waiting, wrong,
   unknown, action, or chart-series meaning.
7. Add the accepted values to `design/tokens.json` only after the maintainer
   chooses the semantic names and confirms that the generated tables remain
   the single source of truth.

## Scope and limitations

This is a computed proposal, not a token change. It has not been visually
tested in the dashboard, a terminal, a screen reader, or an operating-system
forced-colors mode. The ratios do not establish distinguishability under
every display, font, blur, or translucency condition. Components still need
their own text-size, weight, outline, and state-label checks.

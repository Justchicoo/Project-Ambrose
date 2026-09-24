<!-- Project Ambrose by Imjustchico: C-34 audit and acceptance proposal for the seven-slot chart series ramp. -->

# C-34: Chart series ramp audit and acceptance proposal

## Scope

`doc/DESIGN.md` already names seven chart-series slots and says the generator must check their contrast and separation under normal vision, protanopia, deuteranopia, and tritanopia. This proposal records the current values, the measurements that can be reproduced without a browser, and the remaining acceptance work. It does not edit the design token source or silently replace the maintainer's palette.

The ramp must remain separate from the four semantic accents. A chart series is a category or measurement; gold, teal, ember, and violet already carry state or ownership meaning and must not be reassigned as series.

## Current values and contrast evidence

The values below are copied from the current design specification, not from a client file:

| Slot | Dark | Light |
|---|---|---|
| `series-1` | `#C9503F` | `#B45A5A` |
| `series-2` | `#B8C24E` | `#8A7A22` |
| `series-3` | `#79C98A` | `#2F5A22` |
| `series-4` | `#2A8F7C` | `#2D7E9C` |
| `series-5` | `#7D6BD6` | `#3B3AA8` |
| `series-6` | `#A560A5` | `#6A2A86` |
| `series-7` | `#E1A6C4` | `#8E2A5C` |

These WCAG relative-luminance ratios were calculated against the documented page and card surfaces:

| Slot | Dark/page | Dark/card | Light/page | Light/card |
|---|---:|---:|---:|---:|
| `series-1` | 4.24:1 | 3.83:1 | 3.86:1 | 4.54:1 |
| `series-2` | 9.80:1 | 8.84:1 | 3.60:1 | 4.22:1 |
| `series-3` | 9.50:1 | 8.57:1 | 6.74:1 | 7.92:1 |
| `series-4` | 4.79:1 | 4.33:1 | 3.84:1 | 4.51:1 |
| `series-5` | 4.46:1 | 4.03:1 | 7.46:1 | 8.76:1 |
| `series-6` | 4.34:1 | 3.92:1 | 7.68:1 | 9.02:1 |
| `series-7` | 9.42:1 | 8.51:1 | 6.67:1 | 7.83:1 |

The important boundary is explicit: every value clears the documented 3:1 floor for non-text chart marks, but several values do not clear 4.5:1 as body text. The ramp must therefore be used for marks, lines, swatches, and legends with readable text beside them, not as an unlabelled text color.

## Acceptance gate

Before the ramp is marked accepted, the token generator and component tests should:

1. calculate every slot against both surfaces in both themes and fail below 3:1;
2. reject a raw semantic accent (`gold`, `teal`, `ember`, or `violet`) used as a series token;
3. simulate protanopia, deuteranopia, and tritanopia and compare adjacent slots by lightness and perceptual distance;
4. test the seven slots in a real chart, a single-series chart, a legend, a threshold line, and a danger band;
5. verify that a series keeps its slot when the same figure appears on another chart;
6. verify that threshold and danger semantics use the meaning colors rather than consuming series slots; and
7. verify that every mark has a text or accessible-name equivalent and that the chart remains interpretable when color is removed.

The color-vision test must report the case, slot pair, simulation, measured distance, and required minimum. A red/green-looking pair that passes only under normal vision is a failure, not a warning.

## Evidence, disproof, and limitations

The source requirements are `doc/DESIGN.md`, `doc/UI-STACK.md`, and C-34 in `doc/CONTRIBUTOR-TRACK.md`. The contrast table above is reproducible from the listed hex values and documented theme surfaces. The proposal is disproved if a future generator finds a ratio below 3:1, if a required simulation collapses an adjacent pair below its accepted perceptual distance, or if a browser test finds that meaning is lost when color is unavailable.

This audit does not claim to have run the future browser, token-generator, or color-vision tests: the dashboard and chart components are not implemented in this checkout. It also does not claim that contrast alone proves accessibility. The maintainer should settle the remaining perceptual-distance threshold and record the generator output before treating the ramp as accepted.

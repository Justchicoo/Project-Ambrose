<!-- Project Ambrose by Imjustchico: C-35 sequential color ramp proposal for heatmaps, density grids, and queue views. -->

# C-35: Sequential ramp for heatmaps and density grids

## Purpose

This proposal supplies the missing magnitude ramp for 17.98's weekly grid, density views, and queue-length views. It is deliberately separate from the categorical chart series ramp and from the four semantic accents: gold means waiting, teal means healthy, ember means wrong, and violet means ownership. Reusing any of those colors would make a value look like a state.

The ramp is sequential rather than categorical. A quiet cell represents less magnitude and a saturated cell represents more magnitude. Every cell must also expose its numeric value and unit in text or an accessible description; color is a visual aid, never the only meaning.

## Candidate tokens

The slots are ordered from the lowest magnitude (`heat-1`) to the highest (`heat-7`). Dark and light values are separate because the design document already requires theme-specific contrast.

| Slot | Dark | Light | Intended use |
|---|---|---|---|
| `heat-1` | `#D8F3DC` | `#123B2A` | Lowest non-empty value |
| `heat-2` | `#B7E4C7` | `#164D34` | Low value |
| `heat-3` | `#95D5B2` | `#1B603F` | Lower-middle value |
| `heat-4` | `#74C69D` | `#20734A` | Middle value |
| `heat-5` | `#52B788` | `#267F52` | Upper-middle value |
| `heat-6` | `#3A9D75` | `#2B8959` | High value |
| `heat-7` | `#357A5B` | `#318F60` | Highest value |

The dark values are intended for `surface-page = #0B1020` and `surface-card = #131B31`. The light values are intended for `parchment = #F4EAD5` and `parchment-raised = #FFFDF7`.

## Contrast evidence

The following ratios use the WCAG relative-luminance formula applied to the exact candidate values above. They are a design review measurement, not a replacement for the repository's future token-generator check.

| Slot | Dark/page | Dark/card | Light/page | Light/card |
|---|---:|---:|---:|---:|
| `heat-1` | 16.02:1 | 14.46:1 | 10.43:1 | 12.25:1 |
| `heat-2` | 13.47:1 | 12.15:1 | 8.19:1 | 9.62:1 |
| `heat-3` | 11.21:1 | 10.12:1 | 6.30:1 | 7.40:1 |
| `heat-4` | 9.29:1 | 8.39:1 | 4.87:1 | 5.71:1 |
| `heat-5` | 7.65:1 | 6.91:1 | 4.15:1 | 4.87:1 |
| `heat-6` | 5.64:1 | 5.09:1 | 3.64:1 | 4.28:1 |
| `heat-7` | 3.68:1 | 3.33:1 | 3.36:1 | 3.95:1 |

These values meet the documented 3:1 non-text indicator floor on both surfaces. The lightest and darkest cells must not be used as body text colors without a separate text-contrast check; the ramp is for filled cells, swatches, and real-markup chart regions.

## Rendering and accessibility rules

1. Map the data range to the seven slots with a documented clamp. A missing value is a labelled empty state, not `heat-1`, and an unavailable milestone is labelled unavailable, not zero.
2. Show the value, unit, and time window in the cell's accessible name and in the detail view. A screen reader must not need to infer magnitude from a color.
3. Add a text legend naming low, high, and the unit. The legend follows the same theme remap.
4. Keep the ramp stable across pages and time ranges. Changing the scale must be an explicit control that announces its range.
5. Preserve the slot order under protanopia, deuteranopia, and tritanopia. The generator must simulate those conditions and reject adjacent slots that collapse below the project's documented perceptual separation.
6. Keep threshold lines and danger bands in semantic colors. A red or ember threshold means danger; it is not another heatmap slot.
7. Use real markup for weekly-grid cells and event markers so focus, hover, translation, and assistive technology can reach the same information. Do not encode the only meaning into a canvas bitmap.

## Evidence, disproof, and adoption

The need for a separate sequential ramp comes from `doc/ROADMAP.md`, `doc/DESIGN.md`, `doc/PANEL.md`, and the phase-17.98 design notes. The contrast ratios above were computed from the candidate hex values and the documented theme surfaces. The candidate is disproved if the future generator finds a contrast failure, adjacent slots collapse under one of the required color-vision simulations, or an actual weekly-grid review cannot distinguish the ordered values at the supported viewport sizes.

This proposal does not modify `design/tokens.json` or generated output. Adoption should happen only when the maintainer settles the pending ramp decision and the token generator, contrast check, component gallery, and dashboard tests all use the same values. Until then, these are reviewable candidates rather than shipped design tokens.

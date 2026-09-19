<!-- Project Ambrose by Imjustchico: C-36 accessibility checks for the future dashboard under high contrast and screen readers. -->

# C-36: Testing the dashboard with high contrast and a screen reader

This guide is a repeatable test plan for the Ambrose dashboard. It covers Windows high-contrast settings, keyboard-only navigation, and a screen reader. It does not claim that the dashboard currently passes: the repository has the dashboard package and UI decisions, but not the finished pages or a built supervisor endpoint.

## What is being tested

Run the checks against the built dashboard served by the supervisor, not only against a component story. The same route must be checked at:

- 400 pixels wide, without horizontal scrolling;
- the browser's default light theme;
- the browser's default dark theme;
- Windows High Contrast or Contrast Themes; and
- a screen reader with the browser's accessibility tree enabled.

Use a disposable local account and a local server. Do not use a production account, paste credentials into a recording, or include player names, addresses, tokens, database values, captures, dumps, or client files in the evidence.

## Before the run

Record:

1. the Ambrose commit and dashboard build command;
2. Windows version, browser version, display scale, and viewport size;
3. the selected contrast theme;
4. the screen reader and version;
5. the route and permission used; and
6. whether the supervisor and the app whose figures are shown were running.

Build from the repository's pinned dependencies. The UI stack requires the dashboard's lint and type checks, Storybook axe checks, browser tests, and the deliberate unlabelled-button canary to fail. A passing visual page with a failing canary is not an accessibility pass: it means the gate itself is not active.

## Keyboard and high-contrast pass

Start at the sign-in page and use no mouse:

1. Tab through every control and confirm a visible focus indicator remains distinct in both themes and in the contrast theme.
2. Sign in and confirm that the first focus lands on the page heading or the first meaningful control, not on a hidden navigation item.
3. Open and close the navigation, route switcher, dialogs, toasts, tables, and chart controls with Enter, Space, Escape, and the arrow keys where the control advertises them.
4. At 400 pixels, confirm that focus never moves to content outside the viewport and that opening a menu does not create horizontal scrolling.
5. Stop the event stream and confirm every live figure remains readable but is labelled stale with the age of its last sample.
6. Trigger a form error and confirm the error is adjacent to the field, the field is identified as invalid, and the request id is visible.
7. Turn on the high-contrast theme and confirm that state is not conveyed by color alone: online, stale, warning, error, and disabled states must also have text, an icon with an accessible name, or a structural label.

Record the route, control name, key pressed, expected result, actual result, and a screenshot only when the screenshot contains no secret or player data. A failure is reproducible only when another run with the same route and viewport reaches the same control and observes the same result.

## Screen-reader pass

With the same route and account:

1. Navigate by headings and confirm each page has one useful level-one heading.
2. Navigate by landmarks and confirm navigation, main content, status/toast output, and dialogs have useful names.
3. Navigate by buttons, links, form fields, table headers, and alerts. The announced name must include the target or action; an icon-only control must not be announced as “button” without a name.
4. Open a dialog and confirm the title and purpose are announced, focus is moved inside, background controls are unavailable, and focus returns to the trigger after close.
5. Change a setting, submit a command, acknowledge an alert, and switch pages. The result must be announced without requiring the operator to discover a visual toast.
6. Load a graph and confirm its accessible summary names the series, time range, unit, current freshness, and any gap caused by a stopped app. A chart that is only a canvas or SVG drawing is not sufficient.
7. Inspect a stale sample, permission denial, 422 field error, and unavailable milestone figure. Each must be announced distinctly and must not be presented as a zero or a current value.

Use the screen reader's speech viewer or an equivalent transcript for evidence. Redact account names and paths. A transcript is stronger than “the page sounded fine”: include the control sequence, the announced text, and the expected text.

## Failure classification

Classify each failure as one of:

- **keyboard reachability**: a control cannot be focused or operated without a pointer;
- **focus visibility**: focus is lost, trapped incorrectly, or visually indistinguishable;
- **semantic naming**: the accessibility tree omits a role, name, value, state, or relationship;
- **status announcement**: an asynchronous result is visible but not announced;
- **contrast or non-color state**: meaning depends on color or fails the documented contrast ratio;
- **responsive layout**: the 400-pixel route scrolls horizontally or clips the focused target; or
- **test-gate failure**: axe, the canary, Svelte accessibility diagnostics, or a browser test does not fail when it should.

For each failure, preserve the route, build, browser, assistive technology, exact steps, expected result, actual result, and the smallest redacted evidence. Do not “fix” a failure by weakening axe, removing a label, hiding focus, or replacing a meaningful status with color.

## Evidence and current limitation

The requirement for a responsive 400-pixel layout, keyboard reachability, light and dark themes, and axe through Storybook comes from `doc/roadmap/phase-17-operations-console-admin-api-dashboard-and-metrics.md` and `doc/UI-STACK.md`. The contrast and stale-value requirements follow `doc/DESIGN.md` and `doc/PANEL.md`.

This guide was written against the repository state on 2026-09-19. It was not presented as a completed live test: there is no finished dashboard page or built supervisor endpoint in this checkout. The first implementation run must fill in the recorded versions, routes, transcripts, and failures; until then, the C-36 result is **unverified**, not passing.

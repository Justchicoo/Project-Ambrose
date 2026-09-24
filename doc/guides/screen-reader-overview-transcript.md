<!-- Project Ambrose by Imjustchico: C-37 transcript of the current dashboard overview for screen-reader review. -->

# C-37: Screen-reader transcript of the overview

This is a transcript of the overview page that exists in the repository today. It
records the order and wording exposed by the page's semantic markup, followed by
the parts that were confusing without visual context. It is not a claim that the
future metrics overview is accessible: the current page is still a design-system
proof page and does not connect to the supervisor API.

## What was reviewed

The review used the current `apps/dashboard/src/App.svelte` and the shared
`@ambrose/ui` components. The transcript was derived from their rendered
landmarks, headings, links, buttons, figures, captions, and visible text. No
client files, captures, credentials, or screenshots were used. A maintainer
should repeat the transcript with NVDA or another screen reader after the page
is served, because browser and screen-reader verbosity settings can change the
spoken punctuation and landmark wording.

The page currently contains:

- a skip link to `#ambrose-main`;
- an unlabeled header containing the product name `Ambrose`;
- a navigation landmark labelled `Panel sections`;
- two navigation groups, `This machine` and `Panel`;
- one `h1`, `Overview`;
- a `Start all` button;
- three statistic figures; and
- one section titled `What this build proves`.

## Linear transcript

The following is the expected reading order when moving through landmarks and
headings, then activating the page controls in document order. Bracketed text is
the semantic role or state a screen reader is expected to announce; it is not
additional page content.

```text
Skip to content, link
Ambrose, banner
Panel sections, navigation
  This machine
    Overview, current page, link
    Servers, link
    Console, link
  Panel
    Users, link
    Settings, link
Start all, button
Overview, heading level 1
Servers running, figure
  0
  Not connected
Players online, figure
  0
  Not connected
Panel, figure
  17.73
  Design system
What this build proves, heading level 3
The panel, the launcher window and the terminal read one set of tokens.
This page loads them and nothing else yet.
Waiting for the admin API in 17.06
```

The exact treatment of `figure` depends on the browser and screen reader. The
important evidence is that each statistic has a real `figcaption` and that its
value and state word are text, rather than meaning carried only by a colored
dot.

## What was confusing

### The header has no spoken name

The product name is visible as `Ambrose`, but the `header` has no
`aria-label` or heading. A screen reader can announce it as a banner, but it
cannot distinguish this chrome from another banner on a future page. The
overview should keep one predictable page heading and should not add a second
visual heading solely for the landmark. If the final shell needs a named
banner, use a stable accessible name that does not repeat the page title.

### “Start all” has no scope or result

The button is exposed as `Start all, button`. It does not say which apps it
will start, whether some apps are already running, what permission is required,
or where success and failure will be announced. Before this action is wired to
the supervisor, the finished page should provide a nearby status region or
result message and should identify the affected apps without requiring visual
inspection.

### The `Panel` statistic is an implementation label

`Panel, figure / 17.73 / Design system` is understandable to somebody who has
read the source, but not to an operator. It sounds like a numeric panel metric
until the state word is reached. The final overview should give this figure a
stable metric name and unit, or remove it. A milestone/version value should be
announced as such, for example “Panel design-system version 17.73,” rather
than relying on a visual card layout.

### Navigation contains destinations that do not exist yet

The navigation announces `Servers`, `Console`, `Users`, and `Settings` as links,
but the current page only renders the overview content. Activating those links
changes the fragment without exposing a corresponding page in this build. This
is confusing in a transcript and is especially easy to miss when navigating
by links list. Until the destinations exist, they should be disabled or omitted;
when they land, each must expose its own page heading and current-page state.

### The waiting state does not identify its scope

`Waiting for the admin API in 17.06` is visible text, but the announcement does
not say whether the whole panel, the three statistics, or only the card is
waiting. The final live overview should associate stale, unavailable, and
waiting states with the affected metric or app, and should announce changes
through a deliberately scoped status/live region rather than repeatedly
reading the whole page.

## Follow-up acceptance check

When the supervisor-backed overview exists, repeat this review with:

1. NVDA on Windows, using browse mode and the elements list for headings,
   landmarks, links, buttons, and figures.
2. Keyboard-only navigation from the skip link through the navigation and
   `Start all`, confirming that focus remains visible and that the fragment
   links do not strand focus.
3. A disconnected supervisor, a stale event socket, and a successful
   reconnect, confirming that the affected app or metric and its age are
   announced without exposing credentials or client-derived data.
4. A user without the permission to start apps, confirming that the action is
   absent or explicitly unavailable rather than silently failing.

Record the actual spoken wording, browser, screen-reader version, verbosity
settings, and build revision. A future transcript disproves this one if the
rendered order, accessible names, or state behavior differ; that is expected
when the proof page becomes the real overview.

## Evidence and limits

This C-37 contribution documents the current source-backed semantic transcript
and its confusing points. It does not report a live screen-reader session, an
axe result, or an accessibility pass. Those require a served dashboard and the
supervisor states listed above, which are not implemented in this repository
revision.

<!-- Project Ambrose by Imjustchico: The settled front-end stack for every Ambrose surface: what each area uses, why, and what it costs. -->

# UI foundation decision

Ambrose is about to build every screen a person sees: the panel in a browser, the launcher's window in the
operating system's own web view, and the terminal dashboard. They are one product, so they use one stack.
This document settles that stack area by area. Nothing here reopens a choice already recorded under
Decisions, Operations in doc/ARCHITECTURE.md, and nothing here settles an item listed under Decisions needed
in doc/ROADMAP.md; the proposals at the end name those separately.

Every version and licence below was read from the npm registry on 2026-09-18, and the contrast figures were
computed from the exact values in doc/DESIGN.md with the WCAG 2.x formula.

## Summary

| Area | Choice | Version | Licence |
|---|---|---|---|
| Framework | Svelte, runes only | 5.57.0 | MIT |
| Build | Vite in single-page mode, no SvelteKit | 8.3.0 | MIT |
| Svelte plugin | @sveltejs/vite-plugin-svelte | 7.3.0 | MIT |
| Language | TypeScript, pinned to the 6 line | 6.0.3 | Apache-2.0 |
| Package manager | npm workspaces | npm 11 | n/a |
| Styling | Tailwind CSS with @theme, stock palette deleted | 4.3.3 | MIT |
| Tokens | Ambrose generator in Python from design/tokens.json | ours | ours |
| Primitives | Bits UI | 2.19.2 | MIT |
| Component starting points | shadcn-svelte, copied in and owned | CLI 1.7.0 | MIT |
| Panel class helpers | tailwind-variants, tailwind-merge and clsx, which the copied shadcn-svelte components are written against | 3.3.1 / 3.7.0 / 2.1.1 | MIT |
| Panel animations | tw-animate-css, the enter and leave keyframes shadcn-svelte's menus and drawers use | 1.4.0 | MIT |
| Panel icons | @lucide/svelte, the same Lucide set as the vendored icons, imported per icon | 1.47.0 | ISC |
| Dates in pickers | @internationalized/date, which Bits UI's date parts take | 3.12.4 | Apache-2.0 |
| Tables | TanStack Table, headless | 9.2.4 | MIT |
| Virtualisation | virtua | 0.51.3 | MIT |
| ANSI in logs | anser | 2.3.5 | MIT |
| Charts | uPlot | 1.6.32 | MIT |
| Socket reconnect | partysocket | 1.3.0 | MIT |
| Validation | Valibot, schemas generated from C++ | 1.5.0 | MIT |
| Toasts | svelte-sonner | 1.2.1 | MIT |
| Icons | unplugin-icons with @iconify-json/lucide | 24.0.0 / 1.2.134 | MIT / ISC |
| Fonts | Fontsource variable, latin subset, vendored | 5.3.0 | OFL-1.1 |
| Motion | Svelte's own transitions, CSS, View Transitions | in Svelte | MIT |
| Gallery | Storybook with the a11y and vitest addons | 10.6.0 | MIT |
| Test runner | Vitest, pinned to the 4 line | 4.1.11 | MIT |
| Browser tests | @vitest/browser-playwright, Chromium and WebKit | 4.1.11 | MIT |
| End to end | @playwright/test | 1.63.0 | Apache-2.0 |
| Accessibility gate | axe-core through the Storybook addon | 4.13.0 | MPL-2.0, tests only |
| Lint and format | ESLint flat config, typescript-eslint, Prettier | 10.10.0 / 8.70.0 / 3.9.8 | MIT |

## Application shell

**Svelte 5.57.0, runes only.** Svelte 4 is finished: its last release, 4.2.20, is from 2025-05-20, and every
current tool targets 5. Runes matter here for a specific reason: `$state` and `$derived` work in plain
`.svelte.ts` modules, so the panel's live model, which is the app list, the player counts and the event
socket's stream, is ordinary typed TypeScript that components read, not a store abstraction. Runes are turned
on project wide through the compiler option so no file can fall back to legacy reactivity. The whole framework
costs about 12 kB compressed. No Svelte 6 is announced.

**Plain Vite 8.3.0 in single-page mode, not SvelteKit.** Four reasons, and the first is the one that decides it.
doc/PANEL.md requires a strict Content-Security-Policy on every response from the listener in 17.14. A plain
Vite build emits an index.html with no inline script and no inline style, so `script-src 'self'; style-src
'self'` works with no nonce and no hash. Measured on 2026-09-22 against the built panel, it needs one relaxation: Bits UI's scroll lock writes
the page body's style attribute when a dialog, menu, select or sheet opens and writes it back when it closes, which
`style-src 'self'` blocks, leaving the page unable to take a click, so the policy adds `style-src-attr 'unsafe-inline'`.
Style elements and every script stay `'self'`. SvelteKit's static adapter puts an inline bootstrap script into its
fallback page, which under a strict policy needs Kit's hash mode, which writes a second policy as a meta tag
that the C++ header must then be kept in agreement with, and the hash changes every build. That is permanent
friction against a security rule the panel has already fixed. Second, size: the same hello-world app is about
12 kB compressed on plain Vite and about 29 kB on Kit, for a router and a client runtime the panel would
largely not use. Third, nothing Kit exists for applies: there is no server rendering, no prerender, no load
functions, no form actions, and remote functions need a Node server, which is excluded. Fourth, 17.06's
acceptance check demands a route table naming each route's permission and navigation flag, tested by a check
that fails on an incomplete entry; that is a data structure, and a typed array is directly testable where
filesystem routing would force a parallel table anyway. SvelteKit is a defensible choice, it simply buys less
here than it costs, and its own 3.0 line is still in preview.

The build uses `base: './'`, so the identical output works at `/`, under a subpath, behind WebView2's virtual
host and behind a WebKitGTK custom scheme, with no per-host rebuild.

**TypeScript 6.0.3, deliberately not 7.** TypeScript 7.0.2 is the current release and the Go port, and it
ships with no programmatic compiler API until 7.1, so the checking chain around Svelte breaks on it today.
This is not a guess: typescript-eslint 8.70.0 declares its peer as `>=4.8.4 <6.1.0`, and svelte-check 4.7.6
declares `^5.0.0 || ^6.0.0`. Pin `~6.0.3` and assert the installed major in CI, because an unattended bump to
`typescript@latest` stops both lint and type checking on the spot. Revisit when those two publish a 7 peer.

**Routing is ours, about 200 lines over the History API.** The route table must exist as typed data anyway.
Once it does, matching it is a small pure function that tests exhaustively, plus a popstate listener and a
link interceptor. That is a stable API forever and no supply chain surface. Use popstate rather than the
Navigation API, which only reached WebKitGTK in 2.52 from March 2026 and would exclude older distributions.
The launcher needs no router at all: 3.26 has four screens, which is one state union. This choice does not
matter much, and if a library is ever wanted, svelte-spa-router is the mature option at the cost of hash URLs.

**State is runes in `.svelte.ts` modules, with no state library.** Almost all panel state is pushed, not
fetched: the event socket delivers status, stats, logs, players, progress, audit rows and alerts with sequence
numbers and resume. A query cache has nothing to cache for that. The shape is one typed socket client plus a
few rune-backed classes the socket writes into and components read. If the cursor-paginated list pages later
start re-implementing retries and stale-while-revalidate badly, @tanstack/svelte-query is the answer then, not
now.

## Package layout and offline builds

One npm workspace with a build-less shared package. The root declares `workspaces: ["packages/*", "apps/*"]`.
`packages/ui` exports raw `.svelte` source through the `svelte` export condition, so there is no build step,
no watch mode and no stale dist; each app compiles the shared components into its own bundle and its scoped
CSS is extracted into that app's stylesheet. `apps/dashboard` and `apps/launcherui` are each a small Vite app
importing `@ambrose/ui`. This is what makes 3.26's requirement, that the launcher and the panel share one set
of components and one set of tokens, a file import rather than a convention.

npm workspaces over pnpm or Yarn: pnpm is faster and stricter but adds a tool to install everywhere for a
three-package repository, and Yarn's zero-installs would put tens of megabytes of third-party archives into a
repository whose rules are strict about what may be committed. Revisit pnpm past about six packages.

Offline builds work from a primed cache, not a vendored `node_modules`. `npm ci --offline --ignore-scripts`
against a cache directory of roughly 30 to 45 MB installs the whole toolchain with the network never touched,
and disabling lifecycle scripts entirely still builds, which is free supply chain hardening. One trap has to
live in a script rather than in someone's head: about twenty-five lockfile entries are platform-specific
binaries, the Rolldown and Lightning CSS ones, so a cache primed on Windows fails an offline Linux install.
npm 11 fixes this with `--os`, `--cpu` and `--libc`, and the priming script runs both platforms into one
cache. The cache is produced by a script and never committed.

## Styling and tokens

**Tailwind CSS 4.3.3 as the styling engine, with the stock palette deleted.** Tailwind 4 is configured in CSS,
not in JavaScript, and that is why it wins here: `@theme { --color-action: #E4B457 }` both emits a real custom
property on `:root` and creates the `bg-action` and `text-action` utilities, so the generated token file is
the configuration. There is no second place for the palette to live. Writing `--color-*: initial` first
deletes the entire stock palette, after which `bg-red-500` is not a class that exists, and the only colours
the utility language can express are Ambrose's. Its baseline is Chrome 111 and Safari 16.4, which WebView2
clears trivially and WebKitGTK has cleared since 2.40. Utilities also suit agent authorship: the constraint is
in the class name, and a reviewer sees drift in the diff.

The cost is honest. The engine is a Rust binary of about 3 MB per platform, so it is part of the cross-platform
cache priming above. Inside a Svelte `<style>` block, `@apply` and `theme()` need `@reference "tailwindcss";`
first, which re-parses the token sheet per file, so the rule is to style in markup and keep `<style>` for what
utilities cannot express: keyframes, `:global`, and complex selectors.

Tailwind Labs joined Shopify on 2026-09-09, with a public commitment that the framework stays MIT and free and
the same team maintains it. The risk is low and the mitigation is already in the design: components are written
against our own semantic class names, so a move to UnoCSS would be mechanical.

**Tokens come from one source in three tiers.** `design/tokens.json` in the Design Tokens Community Group shape
is the source of truth, and doc/DESIGN.md's tables are generated from it rather than hand-edited, so the
document cannot lie about the values. The generator is `apps/designtokens/designtokens.py`, Python with no
dependencies, in the shape apps/ci and apps/codestyle already use. It emits the Tailwind `@theme` block and the
semantic layer as CSS, typed constants and union types as TypeScript, a C++ header of constexpr colours for the
terminal, and the tables inside doc/DESIGN.md. Generated files are committed so a C++-only or offline build
never runs a generator, and a check regenerates and diffs them.

Style Dictionary 5.5.4 and Terrazzo 2.7.1 are the off-the-shelf equivalents and both are good, but each would
put Node on the path of a C++ header, and neither computes the two things Ambrose actually needs: a contrast
gate and a terminal colour mapping. Because the source file is in the standard shape, moving to either later
is cheap. This is a genuine close call, decided on keeping Node off the C++ build.

The three tiers are: primitives, the raw values from doc/DESIGN.md; semantics, the only names components may
use, such as `surface-card`, `edge-strong`, `fg-muted`, `action`, `state-healthy`, `state-waiting`,
`state-wrong`, `state-unknown`, `mine` and `focus-ring`; and component tokens where a component genuinely needs
a knob. Only the semantic tier enters `@theme`. This is what makes a component physically unable to invent a
colour, and it is what makes the light theme a remap rather than a rewrite: a component never says "gold", it
says "action" or "state-waiting".

**Theme switching by a `data-theme` attribute plus `prefers-color-scheme`, and not the CSS `light-dark()`
function.** Dark is the default on `:root`; light redefines only the semantic tier, applied both under
`:root[data-theme="light"]` and under the media query for a root not explicitly set to dark, so the system
setting is honoured as 17.06 requires and an explicit choice still wins. `color-scheme` is set so native
scrollbars, form controls and the web view's own background follow, which matters most in the launcher where a
white flash on a dark window is very visible. `light-dark()` would be tidier but raises the floor from Safari
16.4 to 17.5 for no gain, and the launcher runs in whatever WebKitGTK the user's distribution shipped.

Density is a root attribute that scales the spacing tier only. Type sizes do not scale, because 11 px is
already the floor, and `@media (pointer: coarse)` forces comfortable back on so the 44 px touch target rule can
never be violated on a phone.

**Enforcement is a Python check in the existing checks job.** Three rules catch essentially all colour drift:
no hex, `rgb(`, `hsl(` or `oklch(` literal anywhere under the apps or the shared package except the generated
token file; no Tailwind arbitrary value for colour or spacing, such as `bg-[#...]` or `p-[13px]`; and no inline
`style` attribute carrying a colour. The third is not pedantry. A Svelte `style:` directive compiles to
`setProperty`, which a strict policy allows, but a literal `style="opacity:.9"` in markup survives into the
compiled template and is blocked under `style-src 'self'`, so it renders wrong only behind the real listener
headers and never in development. An escape hatch exists through an explicit allow comment, so an exception is
visible in review. Stylelint 17.15.0 is the heavier and more correct version of the first rule and is worth
adding when there is enough hand-written CSS to justify it; which of the two enforces it does not matter much,
only that one of them exists from the first page.

## Components

**Bits UI 2.19.2 is the primitive layer for every surface.** It is Svelte 5 native rather than a port, ships
more than forty components with the ARIA attributes, focus management and keyboard navigation built in, and
ships no visual styling at all, which is exactly what a fixed look demands. It covers every primitive the panel
names: dialog, alert dialog, dropdown and context menus, menubar, navigation menu, tabs, tooltip, popover,
select, combobox, checkbox, radio group, switch, slider, label, progress, meter, scroll area, separator,
pagination, PIN input, toolbar, toggle group, and the command primitive the palette is built from. It is by a
wide margin the most used Svelte primitive library, and it is by the same author as shadcn-svelte, so the two
move together. It does not ship a toast or a table, which are covered below.

Ark UI on Zag.js is the real runner-up: its behaviour lives in framework-agnostic state machines, so a fix
reaches four frameworks at once. Against it, the Svelte adapter pulls seventy pinned sub-packages, its API
reads as React translated, and its Svelte usage is a fraction of Bits UI's. One hybrid is legitimate and
planned: where Bits UI lacks a component Ambrose needs, such as a tree view for the file manager, file upload
or a QR code, pull that single Zag machine rather than all of Ark UI. Melt UI is not the answer: its builder
package has had no release in eighteen months and its runes rewrite none in eight, at about one percent of
Bits UI's usage. Skeleton and Flowbite are opinionated design systems whose look is not ours, and Skeleton has
shipped four majors in sixteen months, which is a migration tax with nothing to show for it.

**shadcn-svelte is a scaffold to copy from, not a dependency.** Its CLI drops real, accessible Svelte source
for about seventy components into the repository, and because the code is copied and MIT, Ambrose owns each
file from then on with no upstream that can break it. For a codebase written by agents, starting from a working
dialog or grant editor and changing its colours is far more reliable than authoring from nothing. It composes
exactly the stack chosen here anyway: Bits UI, TanStack Table and Tailwind. Two costs, both real: it requires
Tailwind, which is the honest price of the decision above; and its copied components carry its own token names
and radius scale, so the remap to Ambrose tokens happens once, in one stylesheet, before any component is
copied. Its default look is neutral grey and understated, so the display-type components are a restyle rather
than a re-skin. It is never treated as something to upgrade.

**TanStack Table 9.2.4, headless.** The panel has a dozen tables of different shapes: accounts, characters,
bans, players online, audit rows with cursor pagination, backups, schedules and run history, port allocations,
file listings, world database rows and the permission catalog. Headless is the only correct answer when the
look is fixed, because we render our own markup, which means real table semantics and therefore a table a
screen reader can use. The honest risk is that version 9 is six weeks old and there is no fallback: the version
8 Svelte adapter's last release is from 2025-04-14 and was built for Svelte 4 stores. It is contained by the
adapter being a thin wrapper over the very widely used core, so replacing it with about a hundred lines of our
own is a bounded job. Pin the exact version. AG Grid and Tabulator are capable and wrong here: they render
their own DOM and carry their own theming, so matching a fixed design means fighting them.

**virtua 0.51.3 for virtualised lists.** The console page holds thousands of variable-height log records, must
stick to the newest line, must pause, and must prepend a backlog on resume without the scroll position
jumping, which is where naive virtualisers fail. virtua has an official Svelte 5 adapter with dynamic item
heights, reverse and sticky-to-bottom behaviour and a shift mode for prepending, at about 6 kB compressed with
no dependencies. @tanstack/svelte-virtual is equally good and composes naturally inside a virtualised TanStack
table; using both costs about 13 kB and that is fine. Do not hand-roll this: prepend-without-jump is the case
everyone gets wrong.

**The console renders its own rows, with anser 2.3.5 for ANSI. No terminal emulator.** This goes against the
obvious choice and is the recommendation held most firmly. Ambrose's console is not a terminal: 17.07 and
doc/PANEL.md define it as structured records with sequence, time, level, category, source and message, with
level and category filters, highlighted search, pause with a count, jump to newest, copy, a previous-run view,
and resume by sequence number. A character grid gives none of that. You cannot filter a grid by level, resume
it by sequence number, make its selection reliably reachable, or colour a log level in Ambrose's tokens,
because the emulator owns the colours. A virtualised list of real rows gives all of it plus the keyboard
reachability the accessibility rule demands. The only thing lost is ANSI interpretation of raw captured output,
and anser handles that in about 2 kB, returning structured tokens rather than an HTML string, which matters
because log text is untrusted and must be rendered as text nodes. xterm.js stays on the shelf for the day a
milestone gives an app a real interactive terminal; `console.raw` is a single line written to standard input,
not a terminal, so it does not justify one.

**Validation is Valibot 1.5.0, with schemas generated from the same C++ source as the types.** 17.26 already
requires the TypeScript types for API and socket payloads to be generated from the schemas the C++ side
serialises. The same generator should emit validators, or client-side validation is hand-written and will
drift from the settings schema's own types and bounds, and the settings page will happily submit a value the
server then refuses. Valibot is modular and fully tree-shakeable, so a form imports only the validators it
uses, and it implements the Standard Schema interface so a form layer can be added later. Zod is the safer
choice on sheer ubiquity and models write it more fluently; it is several times larger. This one does not
matter much. Note that Superforms, the best form layer in the Svelte ecosystem, requires SvelteKit even in
single-page mode, so with plain Vite the form layer is about two hundred lines of our own over a typed
`{values, errors, touched, submitting}` store that maps a 422 body onto field errors.

**svelte-sonner 1.2.1 for toasts**, styled entirely through custom properties so it takes the Ambrose tokens
directly, wired into 17.67's notification centre. One rule goes with it, and it is a design rule rather than a
library one: a toast is never the only record of an event. Everything a toast says also lands in the
notification centre and, for an operator action, in the audit log, or an operator who looked away loses the
outcome of a backup. Error toasts do not auto-dismiss.

**The command palette is Bits UI's command primitive inside its dialog**, so it costs no new dependency. Its
filter is replaceable for server-driven results. One Ambrose rule: every result is permission-filtered on the
server, and an action the caller may not run never appears, because a palette that lists a command and then
refuses it is a permissions leak. cmdk-sv is dead and its latest version carries no licence field at all.

**Paneforge 1.0.2 for resizable splits** is deferred to the milestone that needs it, 17.18 or 17.53. A CSS
splitter of eighty lines is genuinely fine. This does not matter much.

## Charts and live data

**uPlot 1.6.32 is the one charting engine, including the sparkline in every overview card.** It is what
Grafana renders its own time-series panels with, and against the alternatives it is not a close call for a
live operations panel. On a twenty-card overview it creates in single-digit milliseconds and updates in under
one, where ECharts takes tens of milliseconds per round and about twenty times the heap; a thirty-day series
of 43,200 points draws in about 3 ms, which is what 17.19's "loads in under one second" needs with room to
spare. It costs about 22 kB compressed with no dependencies. Two properties make it fit Ambrose beyond speed:
it has no animation at all, which is exactly what doc/DESIGN.md's motion rule asks for, and it renders nulls as
gaps natively, which is 17.19's requirement that a stretch while an app was stopped is drawn as a gap rather
than as zero.

What it does not ship, we write: a crosshair tooltip plugin of about eighty lines, a legend built from real
buttons, and no stacked series. None of that is waste, because the legend and the table view have to be our own
markup to meet the accessibility rule anyway. Its npm release is eighteen months old while its repository is
active, so pin 1.6.32, keep the wrapper, `packages/ui/src/internal/plot.ts`, as the only place that touches its API, and expect a version 2 with
breaking changes eventually. It is essentially one author, which MIT and about 50 kB of vendorable source makes
a survivable risk rather than a frightening one.

The rejections are clear. ECharts is the most capable and has the only real built-in accessibility story, but
costs six to eight times the bundle for capability an operations panel never uses, and its aria output is one
sentence on a container, which is weaker than the table view we owe regardless. Chart.js wins nothing: nearly
three times uPlot's size for a fraction of its speed, and the two pieces we would need most, its date adapter
and its streaming plugin, are its least maintained. LayerChart is the Svelte-native option and the one most
expected to win; measured, a single line chart costs about 61 kB compressed plus 14 kB of CSS before the lazy
chunks, it renders SVG, which is the worst shape for a twenty-card overview at one hertz, and it themes against
another project's CSS variable names. Observable Plot is the right tool for exploratory statistics and the
wrong one for a live view, and is worth loading on the 17.69 analytics route alone if that page ever needs
more than hand-drawn bars. **ApexCharts is disqualified on its licence, not its merits:** its community licence
excludes embedding in a product other people run, which is exactly Ambrose, so shipping it would put every
operator into its commercial terms. Its npm metadata says only "SEE LICENSE IN LICENSE", so a scanner reading
package.json alone will not catch it; any licence check must read the file. Plotly is fifty times uPlot's size.

**No sparkline library and no stat-tile library.** Every sparkline package on npm was last published in 2017 or
2018 and none carries types. uPlot draws the tile sparklines with axes, legend and cursor off, so there is one
drawing system rather than two that drift. A stat tile itself is a heading, a monospaced number and a state
word, in real markup, which is what makes it selectable, translatable by 17.66 and readable by a screen
reader. Never draw a tile's number into a canvas.

**partysocket 1.3.0 under our own socket client.** It is a maintained fork of the long-abandoned
reconnecting-websocket, is API-compatible with WebSocket so it can be swapped out in one line, has no
dependencies, and costs under 3 kB. It gives tested exponential backoff and, since June 2026, a close
predicate that maps exactly onto doc/PANEL.md's close codes: reconnect on 4429 with backoff, never on 4401 or
4403, and never on 4400. Two settings are load-bearing and belong in a test rather than a comment:
`maxEnqueuedMessages: 0`, because the default buffers sends while disconnected and flushes them on open, which
would both send frames before our hello frame and re-fire a power or command message minutes after the
operator gave up on it; and `startClosed: true`, so the page connects only once it holds its token. Everything
above the socket is ours and generated from the C++ schemas. Writing the reconnect loop ourselves is about a
hundred and fifty lines and is a defensible alternative; what is not acceptable is a dead package.

**The client-side data policy is settled now because it is where live panels go wrong.** Each series is a
preallocated Float64Array ring, never an array of objects, which is roughly a tenth of the memory and no
garbage collection pause per second. The live window is fifteen minutes at one hertz, which comfortably
exceeds 17.19's sixty points; anything longer is a history request, not memory. History is fetched over HTTP
in columnar form, which measured about half the compressed size of row objects and is already uPlot's native
shape, so nothing is transformed on arrival. Four rules go with it: never render per message, but push into
the ring and repaint on an animation frame; unsubscribe stats when the tab is hidden and resume by sequence
when it returns; treat a dropped marker as a visible gap, because a graph that quietly interpolates across a
drop lies to an operator during exactly the incident they opened it for; and on the send side check
`bufferedAmount` and refuse rather than queue, so a command can never be delivered late.

## The file editor

CodeMirror 6 is **proposed, not settled**, because doc/ROADMAP.md lists the editor library under Decisions
needed as blocking 17.39 and 17.53. The research supports the proposal strongly: 17.53 needs a custom `.conf`
mode with key completion, type and bound hovers and unknown-key warnings driven by the settings schema, which
CodeMirror's stream language, completion source, hover tooltip and linter APIs make a couple of hundred lines
and which Monaco would make a language-server project. It covers every language 17.53 names, themes through
plain CSS so it takes our tokens and JetBrains Mono directly, and its merge package gives the save-conflict
diff in the same theme as the editor. Two things the maintainer should weigh: it is about 116 kB compressed for
a basic setup, so it and its language packs load only on the routes that need them; and every CodeMirror
repository on GitHub was archived on 2026-04-15 with development moved to the author's own Forgejo host, so
while npm publishing is active and the licence is unchanged MIT, it is now a one-maintainer project off the
platform our tooling watches. Monaco is 95 MB unpacked, needs workers and its own theme format, and its one
advantage, a real language server, is not something Ambrose can use because the settings schema arrives from
C++ over the admin API.

## Motion

**Svelte's own primitives are the house engine**, which is zero new dependency and covers nearly everything
doc/DESIGN.md asks for: the transitions for a panel that opens, `animate:flip` for a keyed list that reorders,
`Tween` for a determinate progress bar, and critically `prefersReducedMotion` from `svelte/motion`, a reactive
media query read inside the transition itself, so the accessibility rule is enforced where the animation is
declared rather than in a separate stylesheet that can drift. The transitions compile to real keyframes the
browser runs off the main thread, which is what keeps both web views smooth. The marginal cost over the Svelte
runtime already shipped is about 2 kB.

**Hover, focus, disclosure, dialogs and popovers are CSS only**, using `@starting-style` and
`transition-behavior: allow-discrete`, both widely available since 2024 and present in WebView2 and in every
currently packaged WebKitGTK. That covers the glow on the one gold action, a card that lifts by two pixels, a
log line that arrives, and a dialog that fades and scales out on close, at zero bytes. **View transitions**
handle panel route changes, tab switches and launcher screen changes, always behind a feature guard with a
Svelte crossfade as the fallback, with the sidebar and top bar named so only the content region crosses.

Three libraries are rejected and one is held in reserve. **GSAP is not used**: it is a timeline and
scroll-storytelling engine and Ambrose has no scroll narrative; its core alone is larger than the entire
Svelte runtime; and although its licence is free for this use, it is proprietary rather than permissive, the
intellectual property stays with its owner, and it carries a clause about competing tools that a repository
preferring permissive licences should not have to keep re-reading. **anime.js** is three times the size of the
alternative for the same job and has a version 5 in beta right now, which is exactly the churn to avoid.
**auto-animate** is the wrong philosophy: its premise is animating everything in a container whether it means
anything or not, against a rule that says no decoration that carries no meaning; `animate:flip` gives the same
effect where we decide it applies. **Confetti, particles, Lottie and Rive are all out** on the same rule, and
because a designer-authored asset pipeline has nobody to author it. Motion's mini build, at about 4 kB, is the
named escape hatch for the two or three things Svelte transitions genuinely cannot do, chiefly retargeting an
animation already in flight, imported lazily in the one component that needs it; start without it. The rolling
digit component for live numbers is optional, worth trying against a plain tween, and if adopted must be
wrapped so its loose types stop at our boundary and its animation is bound to the reduced-motion flag, which it
does not check itself.

**Reduced motion is one shared module plus an explicit setting that combines with the media query.** The media
query alone is not reliable on the launcher's Linux target: WebKitGTK only began following the desktop setting
in 2.54, released 2026-09-16, so on every version installed today the preference may never reach the page. On
Windows it is fine. So a single module exports the combined flag and the duration constants, which collapse to
zero when it is set, and the toggle appears on both the launcher's settings screen and the panel's
preferences. Motion never carries information: a log line still arrives, it simply arrives without the fade.
This is also what makes 3.26's and 17.06's accessibility checks provable, by asserting the constants are zero.

**Loading states are determinate, and there are no spinners.** 3.26 already says the first-run screen shows
each setup step with its own real numbers, and that is the answer everywhere: a list of steps, each with a
state dot and its real figure, resolving one at a time. Panel skeletons are the sunken token at the real size
with a single fade and no shimmer, because a shimmer loops and lies about progress. An indeterminate bar is
reserved for the two or three operations with no knowable total and stops the instant the real number arrives.
On a server panel a spinner is worse than nothing, because it hides whether a restart is stuck.

**The terminal dashboard does not animate**, except the focused menu entry's colour transition, which is the
terminal equivalent of the 120 ms hover. FTXUI can animate, but animation drives repeated redraws and the
terminal dashboard is what an operator runs over SSH on the machine that is meant to be running a game server.
A `--no-motion` flag disables it, on by default when the output is not a terminal.

## Icons and fonts

**Icons are compiled to inline SVG at build time by unplugin-icons 24.0.0 with @iconify-json/lucide 1.2.134.**
Only the icons imported become code, nothing is fetched at runtime, and measured over twelve real panel icons
it produced about 4 kB less compressed than the equivalent component package. It is also the only option that
lets a second set fill a gap, which matters because Lucide's 1,853 icons will not have everything the panel
needs and Phosphor's 9,072 will, without a second runtime dependency. The alternative, @lucide/svelte with deep
imports, is perfectly good and simpler for an agent to type; this does not matter much, but two traps do: never
import an icon barrel, which pulls thousands of modules, and never install the full Iconify bundle, which
unpacks to 472 MB. The Iconify API mode fetches icons from a host at runtime and is forbidden outright.

**Fonts are the three Fontsource variable packages at 5.3.0, latin subset only, vendored.** One file per family
covers the whole weight range, which wins as soon as three weights are used and doc/DESIGN.md's ten-step scale
guarantees that. The latin files measure 31 KB, 37 KB and 40 KB, so about 108 KB for the entire type system,
or about 170 KB with latin-ext for 17.66's localisation. Copy only those files and write our own `@font-face`
rules; importing Fontsource's own stylesheet pulls every subset, including scripts we never render. The fonts
are OFL-1.1, which permits bundling in an application and obliges us to ship the licence text with them in
both the panel bundle and the launcher executable, and places no obligation on Ambrose's code. The supervisor
must serve the woff2 media type, which is a one-line thing that shows up as invisible text when forgotten.

## Testing, the gallery and the gates

**Storybook 10.6.0 is the gallery**, with the Svelte Vite framework, the accessibility addon and the Vitest
addon, as a development dependency that is never served to an operator, with telemetry disabled in the
configuration and in CI. It is the only living, typed gallery in this stack, and it gives three things at
once: a browsable catalogue of every component in every state, axe run per story, and stories reused as browser
tests. It also gives the panel and the launcher one shared surface, since the same story proves a component for
both. Histoire is on a beta from January with its own page pointing at a successor that is a week old.
Playwright's own component testing is genuinely attractive and would drop a large development dependency, but
we would write the gallery, the controls, the docs panel and the accessibility panel ourselves. Expect one
migration when Storybook 11 lands; story files are plain data and port cheaply, and our own gallery page inside
the workspace is the fallback.

**Vitest is pinned to 4.1.11, not 5.** This is not caution: @storybook/addon-vitest 10.6.0 declares its peers
as `vitest ^3.0.0 || ^4.0.0` and `@vitest/browser-playwright ^4.0.0`, which was confirmed from the registry,
so a Vitest 5 install is refused outright. Support for 5 is merged upstream but unpublished. Pin the 4 line and
move in one deliberate step later.

**Component tests run in a real browser, in Chromium and in WebKit.** Browser mode has been stable since
Vitest 4 and is not slower at this scale. It buys three things a simulated DOM cannot: the contrast rule, which
axe can only check against real rendering; real focus and keyboard behaviour, which the keyboard-reachable
acceptance checks are about; and WebKit, which matters specifically because the launcher window runs in
WebKitGTK, so a component that works only in Chromium ships broken to half the launcher's users. Pure logic,
meaning the stores, the formatters, the route table, the socket client and the token checks, runs in a plain
Node project with no browser at all.

**The accessibility gate is axe through the Storybook addon, set to fail on error**, plus axe on the real
pages in the end-to-end leg, plus Svelte's own compiler accessibility warnings as errors. It has one documented
failure mode that must be defended against: with the obvious setup a story containing an unlabelled button
passes, because the addon's checks only run once its own annotations are registered in the test setup. A gate
that fails open is worse than no gate, so a canary story that is deliberately unlabelled is asserted to fail.
axe-core is MPL-2.0, which is fine because it runs only in tests and never enters a bundle; importing it from
application code would pull file-level copyleft into what the supervisor serves, so that is a lint restriction
rather than a convention.

**A token contrast test makes doc/DESIGN.md executable.** It parses the colour tables, computes every
documented pair, and fails under 4.5:1, or 3:1 at 24 px and above. It costs milliseconds, needs no dependency,
and it already found something, which is recorded under Proposals below.

**Visual regression uses the runner's own screenshot assertion**, with baselines committed per browser and
platform and generated and verified inside the official Playwright container on Linux only. This is the check
that actually defends the look: a spacing step or a border colour drifting is invisible to a unit test and
obvious to a pixel diff. It is also the flakiest thing here, since rendering varies with fonts, GPU and
browser version, so it runs nightly or on a label rather than on the blocking path, and re-baselining after a
font or browser bump is expected. The hosted services are all rejected: they cost money and they send our
interface to a third party, against the rule that nothing phones home.

**End-to-end tests run against the real C++ supervisor** with @playwright/test 1.63.0, because the panel's
hardest bugs are session, token, socket-resume and permission bugs that only a real browser against a real
server finds. It also gives 3.26's off-screen launcher smoke test a real implementation, and its network
assertions turn "no request to any other host" from a promise into a gate. Browser downloads are large, so
they are cached by version and this leg stays off the per-push job.

**Lint and format are ESLint 10.10.0 flat config with typescript-eslint 8.70.0 and eslint-plugin-svelte
3.23.0, and Prettier 3.9.8.** It is the only combination that lints Svelte templates properly today, including
the accessibility rules that catch problems before a browser runs. Formatting by Prettier removes a whole class
of pointless diff, which matters when the authors are agents. Biome is far faster and one binary, but its
Svelte support is still explicitly experimental; oxlint is faster still and does not replace the template
rules. Revisit both in a year, and do not run two linters over the same rules.

**Two smaller gates are worth having**: knip for dead exports and unused dependencies, on the nightly run; and
an assertion that the built bundle contains no absolute http or https URL, which catches the day someone
pastes a font link or a script tag and is the cheapest possible defence of the offline promise.

## Continuous integration

One job, on Linux only, path-filtered to the front-end folders and the lockfile, in the shape
apps/ci/ci_select_legs.py already uses. It runs type check, lint, format check, the Node-project tests, the
browser tests with the accessibility gate, and the Storybook build. The arithmetic matters against 2,000 free
minutes a month with no payment method: a warm install is under a minute and the checks are seconds to low
minutes, so expect roughly three to five billed minutes per triggering push, and at a realistic rate of
front-end pushes that is a small fraction of the budget. It must never run on Windows, where minutes count
double for no benefit, and the Playwright browsers must come from a cache keyed on their version rather than
being downloaded each run. Screenshots and end-to-end runs are a separate container job on a schedule or a
label. The C++ build stays separate: coupling two very different caches helps nobody, and a release job can
take the built files as an artefact.

The front-end build is gated behind a CMake option, on by default in CI and skippable, so a contributor with
no Node can still build the servers and is told plainly that the panel will serve a placeholder.

## Serving the built files from C++

The built output becomes one generated C++ translation unit at build time: a byte blob plus a manifest of
path, offset, length, media type, entity tag and the offsets of the gzip and brotli variants, produced by a
CMake step and never committed. That gives a single-file supervisor and a single-file launcher, and it is what
makes the launcher's offline promise achievable, because the web view reads from memory through a scheme
handler rather than from disk. Content-hashed assets get an immutable far-future cache header, index.html gets
no-cache, and any request that matches no file and does not begin with the API prefix returns index.html with
status 200. Never fall back for the API prefix, or a mistyped endpoint answers HTML to a fetch and produces a
confusing parse error.

Both web views get a real origin, and the page is never loaded from a file URL, where module loading fails and
storage throws because there is no origin. On Windows, map a virtual host name to the folder and navigate to
it, which gives the document a normal origin so the policy, same-origin fetch and storage all behave; note
that source maps referenced by URL are not fetched under a virtual host, so the launcher uses inline ones if it
wants them. Elsewhere, register a custom scheme and then register it on the security manager as both secure and
CORS-enabled, which is the step people miss and whose absence gives an opaque origin where sub-resources are
blocked and storage raises an error. Because the build uses a relative base, both hosts serve the identical
files. A single typed bridge module exposes one async call, with one implementation per host and one over fetch
and WebSocket for the browser, so the same components sit above all three and no logic moves into the page.

A loopback HTTP listener with a one-time token would be simpler and identical in both engines, and is rejected
because 3.26 says the launcher serves nothing over the network.

## What binds the shipped code

Only Svelte's runtime, which is MIT, and the vendored fonts, which are OFL-1.1 and oblige shipping their
licence text. Everything else recommended in the shipped path is MIT or ISC. Everything else entirely is build
time or test time: TypeScript and Playwright are Apache-2.0; Lightning CSS, which Vite and Tailwind use to
process CSS, is MPL-2.0, which is file-level copyleft on its own source and contributes nothing to our output;
axe-core is MPL-2.0 and must stay in tests. Nothing here needs a paid licence. Two traps are recorded in full
above because a scanner reading package.json alone misses both: ApexCharts publishes only "SEE LICENSE IN
LICENSE" and is not permissive, and dygraphs reads MIT on npm and carries no assertion on its repository.

## What the operations milestones added on 2026-09-18 need

17.74 to 17.104 were read against the summary above, and they add no new library to this stack. What each one
uses is already here, and arrives with the milestone that needs it:

- **uPlot 1.6.32**, with 17.78's event markers and 17.79's window selection drawn as real markup positioned
  against its scales rather than into its canvas, which is the rule doc/DESIGN.md sets for anything that
  carries meaning and is the shape a marker has to take to be hoverable, focusable and translatable anyway.
- **virtua 0.51.3** carries 17.80's result list exactly as it carries 17.07's console, and 17.80's volume
  histogram is the same chart wrapper the graphs use.
- **Bits UI 2.19.2**'s command primitive inside its dialog is 17.88's palette, which is why the palette costs
  no new dependency; its results are permission-filtered on the server, as that milestone's first check
  requires.
- **anser 2.3.5** stays for captured output only. 17.76 carries a log line's value runs as typed ranges on the
  record, so 17.07 renders them from data rather than re-lexing text in the browser, which is what keeps the
  terminal and the panel from drifting.
- 17.98's weekly grid is real markup on a generated ramp, not a heatmap library; the ramp itself is listed
  under Decisions needed in doc/ROADMAP.md, and until it is settled the grid names its quietest hours in
  words.
- The number, unit and time formatting doc/DESIGN.md's Type rules describe is one module of ours, mirrored in
  C++ for the terminal, because the platform's own unit list has no binary units and rendering 8,589,934,592
  bytes as 8.59 GB when the operating system says 8 GB is a small lie that costs trust during an incident.

The checks those rules need are projects in the test setup already chosen, not new tools: **@playwright/test
1.63.0** runs the forced-colors pass, the coarse-pointer pass that proves nothing is hover-only, and the
target-size measurement that walks each dense page under each pointer type; **Storybook 10.6.0** with
**axe-core 4.13.0** runs the gate, now at WCAG 2.2 AA, with its own explicit tests for the three criteria axe
cannot see, a focused row under sticky chrome, a target under its floor and an authentication field that
refuses a paste.

Two library-shaped choices these milestones raise are deliberately not settled here and are listed under
Decisions needed in doc/ROADMAP.md: the one search engine behind 17.80, which is server side rather than front
end, and what records 17.91's on-demand profile. One is settled outside this document: 17.83 writes crash
dumps with the operating system's own writer rather than a crash reporting library, and THIRD-PARTY-NOTICES.md
records why.

## Close calls, stated plainly

- Vitest 4 against Vitest 5 is forced by Storybook's peer range, not chosen.
- unplugin-icons against @lucide/svelte is worth about 4 kB over a dozen icons. Either is fine.
- Valibot against Zod is size against fluency. Either is fine.
- Our own router against svelte-spa-router. Either is fine, as long as the table stays data.
- A Python check against Stylelint for the no-raw-colour rule. Either is fine; both is waste.
- A rolling-digit component against a plain tween for live numbers. Taste, not engineering.
- virtua against TanStack Virtual, and using both. It does not matter.
- Our own token generator against Style Dictionary. Decided on keeping Node off the C++ build; reversible.
- SvelteKit is the one close call decided firmly, because of the policy header, and it is recorded above with
  the reasons so a future maintainer can weigh it again.

## Proposals, which are the maintainer's to settle

These are not settled here. Each needs a decision before the work it blocks.

1. **The light theme as written cannot ship.** doc/DESIGN.md says light surfaces keep the same accents. Computed
   against the parchment ground #F4EAD5, gold is 1.60:1, teal 1.52:1, violet 2.25:1, ember 2.59:1 and gold-deep
   2.61:1; against the light panel #FFFDF7 they reach only 1.88, 1.78, 2.64, 3.04 and 3.06. Every one fails for
   text, and all but two fail even the 3:1 threshold for a non-text indicator. The dark palette by contrast is
   in excellent shape, with the worst pair at 5.48:1. Any component that renders state as coloured text or a
   coloured icon is unreadable in light mode as specified. The proposal is a light accent ramp of four darker
   values, keeping the bright originals for fills with dark text on them: gold #7A5A12 at 5.33:1 on parchment,
   teal #0F6F63 at 5.06:1, ember #A33A25 at 5.51:1 and violet #6B2FA0 at 6.92:1, each clearing 4.5:1 on both
   light surfaces with margin. The alternative is to ship dark only and defer light, which is legitimate since
   both the panel and the launcher are dark by design; either way the generator's contrast gate should exist
   from the first day so the failure cannot be shipped later.
2. **Quiet borders cannot be what identifies a control.** border is 1.13:1 to 1.30:1 against the grounds and
   border-strong is 1.31:1 to 1.50:1, where the guideline wants 3:1 for the visual boundary of a control. The
   proposal keeps the palette and shifts the weight: inputs take the sunken fill and a real label, and the
   focus indicator carries the burden, since gold on ground is 9.89:1, so a 2 px gold focus ring is far above
   the bar and `--focus-ring` is a token no component may override. Raising border-strong instead would change
   the approved look, which is the maintainer's call.
3. **A destructive button's label is the ground colour, not parchment.** Parchment text on ember is 2.54:1;
   the ground colour on ember is 6.12:1. Likewise parchment on gold is 1.58:1 while the chrome colour on gold
   is 10.26:1, which the design already implies and which should be written down.
4. **The terminal sentence should widen.** doc/DESIGN.md says the same meanings in the 16 terminal colours.
   FTXUI reports what the terminal actually supports, so the proposal is truecolor when the terminal reports
   it, then 256, then 16, with the same meanings in every case, and the generated header carrying all three
   values per token computed once rather than eyeballed.
5. **The motion clause needs a decision before components are built.** It currently reads that nothing loops
   and nothing bounces, which read literally forbids shimmer skeletons, any spring with overshoot, looping
   spinners and a pulsing live indicator. The proposal keeps "nothing bounces" absolutely, because overshoot on
   an operations panel reads as instability; replaces "nothing loops" with two named exceptions, an
   indeterminate indicator that may loop only while a real operation runs and stops the instant it resolves,
   and the connected-socket dot, because a live connection is a continuous fact; adds the duration scale
   120, 200, 320 and 500 ms so three surfaces cannot each invent their own; adds that animation touches only
   transform, opacity and filter; and adds that reduced motion collapses every duration to zero and removes no
   information. Leaving the clause as written is entirely workable and produces a restrained, fast product; it
   simply means saying no to skeleton shimmers and to any looping live indicator.
6. **doc/DESIGN.md has no chart guidance at all, and needs a series palette.** The four accents cannot become
   one, because they are status colours with fixed meanings and reusing them as series destroys that. A
   seven-slot set is needed for 17.69's per-school breakdown, and it must not borrow the game's own school
   colours. Separately, doc/PANEL.md names "amber above 80 percent and red above 90 percent", which are not
   token names; they should read gold and ember, or doc/DESIGN.md should gain those names, or the panel and the
   terminal will drift. The common case barely engages any of this, since most graphs carry one or two series.
7. **The file editor library is already listed under Decisions needed** as blocking 17.39 and 17.53, and this
   document proposes CodeMirror 6 with the repository move recorded, without settling it. The QR renderer for
   two-factor enrollment and the event socket protocol are likewise listed there; partysocket is a client
   library choice that assumes nothing about the protocol.

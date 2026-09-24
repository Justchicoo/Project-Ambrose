<!-- Project Ambrose by Imjustchico: What every shared component is for, the order to search in before writing a new one, and how to add one. -->

# Components

`packages/ui` is the one place an Ambrose surface gets a component from. The panel imports it, the launcher window imports it, and anything a server serves reads the same stylesheet, so a screen cannot invent a colour, a spacing step or a duration. doc/DESIGN.md is what the components look like and `design/tokens.json` is where every value in them is written.

The package has no build step. It exports raw `.svelte` source through the `svelte` export condition, so each app compiles the components into its own bundle and there is no stale `dist` to chase.

## Before you write a component

Search in this order and stop at the first answer.

1. **The gallery.** Run `npm run storybook` and look. Nearly every shape a panel page needs is already here under one of the groups below. Reuse it, or add a variant to it, before adding anything.
2. **A primitive from Bits UI.** If the behaviour is a known one, a menu, a dialog, a combobox, a slider, a pin input, a toolbar, Bits UI already has the keyboard handling, the focus management and the ARIA. Wrap it and give it our looks from the tokens. Never re-implement that behaviour by hand.
3. **A starting point copied in.** shadcn-svelte is a scaffold, not a dependency. Copy one component's source into `packages/ui/src/components/`, restyle it onto our tokens, and own it from then on. Never add it to `package.json` and never treat it as something to upgrade.
4. **Something new.** Only when the first three have nothing. Then it is ours from the first line: real markup, tokens only, a story per state, and the rules below.

If a behaviour exists nowhere above, a single Zag.js machine may be pulled for that one component, as doc/UI-STACK.md allows. Never the whole of Ark UI.

## What each component is for

### Foundations

| Component | For |
|---|---|
| `Heading` | A heading in the display family, rendered at the real heading level a screen reader needs, at one of four sizes |
| `Label` | The 11 px uppercase label that names a field or a group, tied to its control when it has one |
| `Mono` | A count, a timing, a revision, a path or a command, monospaced and tabular so a column lines up |
| `Icon` | One of the vendored icons by name, hidden from a screen reader unless it is given a label |
| `VisuallyHidden` | Text a screen reader reads and the eye does not, which is how an icon-only control carries its label |

### Controls

| Component | For |
|---|---|
| `Button` | The one gold action a screen is allowed, the quiet actions beside it, the destructive one, and the same shape as a link |
| `IconButton` | A control that shows only an icon, whose label is required rather than optional |
| `TextField` | A labelled text input on the sunken fill, with its hint and its error tied to it by id |
| `Select` | A single choice, with the keyboard and screen-reader behaviour from the primitive layer |
| `Checkbox` | A checkbox with its own label, including the part-way state |
| `Switch` | A setting that is on or off right now, saying which in words beside the control |

### Containers and overlays

| Component | For |
|---|---|
| `Card` | A raised area with an optional title, its own actions and a footer, which is what most panel pages are built from |
| `Dialog` | A modal with a real title and description, a focus trap and an escape route |
| `ConfirmDialog` | The confirmation a destructive action has to pass, naming the thing it will destroy |
| `Tooltip` | A hint beside a control on hover and on focus, never the only place a meaning is written |
| `Tabs` | Tabs over one region, with the chosen tab marked by an edge as well as by colour |
| `Menu` | The extra actions behind one trigger, with the destructive entry marked |

### Shell and navigation

| Component | For |
|---|---|
| `AppShell` | The frame both surfaces share: the chrome bar, the optional side bar and one main region with a skip link |
| `SideNav` | The side bar's grouped links, where the current page is marked by `aria-current` as well as by its edge |

### State and meaning

| Component | For |
|---|---|
| `StateDot` | A state as a colour and a word together, because a colour alone is not a state a person can read |
| `Badge` | A short standing fact about a row, quiet or filled |
| `ProgressBar` | Progress with its real numbers, and an indeterminate bar only while something is genuinely running |
| `StepList` | What a long operation is doing, as steps with their own states and real figures. This is what stands in for a spinner everywhere in Ambrose |
| `EmptyState` | What a region says when it holds nothing: why, and the one thing to do about it |
| `Toaster` | Where toasts appear. A toast is never the only record of an event |

### Data

| Component | For |
|---|---|
| `DataTable` | A real table with a caption and real header cells, sorted by the headless engine, so a screen reader reads it as a table |
| `StatTile` | One number worth watching, as a heading, a monospaced figure and a state word in real markup |
| `LogList` | Log records as real rows in a virtualised list, with any ANSI in the message rendered as text nodes |
| `Sparkline` | One series drawn small with no axes and no animation, always beside a table of the same numbers |

### The gallery's own pages

`TokenTable` prints every meaning with its value in both themes and what each documented pair reaches against the ratio it has to clear. The icons page is `Icon`'s own "The set in use" story.

## The rules a component follows

- **Tokens only.** A component names a meaning, never a raw value. No hex, no `rgb(`, no Tailwind arbitrary value such as `bg-[#...]` or `p-[13px]`, and no inline `style` carrying a colour. The checks job fails on all three, and the only way past it is an entry in `apps/ci/ci_frontend_allow.json`, which a reviewer sees in the diff.
- **Real markup.** A real `button`, `a`, `input`, `label`, `table`, `th`. That is what makes a control reachable, translatable and readable aloud.
- **Every control carries a label.** An icon-only control takes a required `label`. A region takes an `aria-label`. A chart carries a table of the same numbers.
- **Touch targets are at least 44 px**, and the density attribute never scales below that on a coarse pointer.
- **Focus is never removed.** The 2 px ring is a token no component overrides.
- **Motion comes from the motion module.** The four durations, and zero for all of them when the viewer asks for less. Nothing bounces, and only an indeterminate indicator and the live-connection dot may repeat, while what they report is still happening.
- **No comments.** Every file opens with the branding header and its one-line brief, and carries nothing else. A Svelte file's header is an HTML comment on the first line; a `.ts` or `.css` file's is a block comment.

## Adding one

1. Write `packages/ui/src/components/<Name>.svelte`. Give it the branding header and a brief.
2. Export it from `packages/ui/src/index.ts`, in its group.
3. Write `packages/ui/src/components/<Name>.stories.svelte` beside it, with a story per state. Use the `template` snippet so the story renders exactly your markup:

```svelte
<Story name="Disabled">
    {#snippet template()}
        <Thing disabled />
    {/snippet}
</Story>
```

4. Add its row to the table above.
5. Run the gates below. A component with no story fails the checks job, and a story with an accessibility violation fails the test run.

## The gates

`npm run verify` runs the five that block a push, in order: `check`, `lint`, `format:check`, `checks` and `test:logic`. Run that rather than picking from the table, because `check` and `checks` are one letter apart and are different gates: the first is types, the second is this project's own rules about colours, arbitrary values and stories. Running four of the five and assuming that was the set is how an arbitrary Tailwind width reached main.

| Command | What it proves |
|---|---|
| `python apps/designtokens/designtokens.py --check` | The stylesheet, the TypeScript, the C++ header and the tables in doc/DESIGN.md still match `design/tokens.json` |
| `python apps/designtokens/icons.py --check` | Every vendored icon still matches `design/icons.json` |
| `npm run verify` | The five gates below that block a push, in one command |
| `npm run checks` | No raw colour, no arbitrary value, no inline style carrying a colour, every component has a story, the fonts match their packages, and the built bundle names no other host |
| `npm run check` | Types, across every app and the shared package |
| `npm run lint` and `npm run format:check` | The linter, including the Svelte accessibility rules, and the formatter |
| `npm run test` | The logic and token checks, then every story in Chromium and in WebKit with axe failing the run on an error |
| `npm run test:canary` | **Has to fail.** It renders one deliberately unlabelled control, so a gate that quietly stopped checking is caught |
| `npm run storybook:build` | The gallery builds as static files, which are never served to an operator |
| `npm run e2e` | The built panel against a real browser: it loads from its own origin, reads the tokens and asks no other host for anything |
| `npm run screenshots` | The pixel check. Run and re-baselined only inside the official Playwright container, never on the blocking path |

## Tokens and icons

Change a value in `design/tokens.json`, then run `python apps/designtokens/designtokens.py`. It writes the stylesheet, the TypeScript module, the C++ header the terminal reads and the tables inside doc/DESIGN.md together, and it refuses to write anything when a documented pair falls under its ratio, naming the pair and what it reached.

Add an icon by adding its Lucide name to `design/icons.json` and running `python apps/designtokens/icons.py`. It writes `design/icons/<name>.svg` and the typed `IconName` union, so the repository owns the file and the build compiles it into inline markup. Nothing is fetched from any host at any time.

Refresh a font by copying the latin variable file out of its `@fontsource-variable` package into `packages/ui/src/fonts/`; the checks job compares the bytes and fails when they differ.

## Offline and without Node

`python apps/ci/ci_npm_cache.py` primes one cache with every package the lockfile names, including the platform binaries a Windows install skips and a Linux one needs, so `npm ci --offline --ignore-scripts` installs with the network never touched.

The front-end build sits behind the `FRONTEND` CMake option, which is off by default. With it off the servers build and run as usual and CMake prints what it left out; with `-DFRONTEND=ON` the build installs and builds both apps.

<!-- Project Ambrose by Imjustchico: Every third-party library this software uses, what each one is licensed under, and what that requires of a build that ships. -->

# Third-party notices

Project Ambrose itself is MIT licensed, in LICENSE. It uses the libraries below. Anything publicly licensed may be used here; the only rule is that what a licence asks for is done, and written down in this file when the dependency is added. A dependency that would need a paid licence for this use, or that forbids other people running the result, is not used.

## What ships in the servers and tools

| Library | Used for | Licence | What it asks |
|---|---|---|---|
| Asio (standalone) | Sockets, timers and the thread pools every app runs on | BSL-1.0 | Keep the notice |
| Botan 3 | SHA-2, Twofish, the random number generator and encryption at rest | BSD-2-Clause | Keep the notice |
| fmt | Formatting in every log line and message | MIT | Keep the notice |
| MariaDB Connector/C | Talking to the databases | LGPL-2.1-or-later | Keep the notice, and link it dynamically so a user can replace it. Ambrose loads it as a shared library and never builds it in, which is what keeps this simple |
| nlohmann/json | Reading and writing the type dump and JSON files | MIT | Keep the notice |
| pugixml | Reading the client's XML: message definitions, configurations, name tables | MIT | Keep the notice |
| zlib | Inflating the archives and blobs the client stores | Zlib | Keep the notice |
| Crow | The admin API's HTTP and WebSocket listener in every app and the supervisor | BSD-3-Clause | Keep the notice |
| OpenSSL | TLS on the panel listener, through Crow | Apache-2.0 | Keep the notice |
| SQLite | The panel's own store: sessions, audit rows and settings | blessing | A public-domain dedication: nothing beyond keeping it with the source |
| Zydis and Zycore | Decoding x86 instructions in the type extractor | MIT | Keep the notice |
| Unicorn 2 | Running the user's own client program in a sandbox to rebuild its type data | GPL-2.0-or-later | Only `typeextract` links it, and only as a shared library. That one program is therefore distributed under GPL-2.0-or-later; every other program here stays MIT. A build without the tools has no GPL code in it |

## What ships in the panel and the launcher window

Installed with milestone 17.73 at the versions doc/UI-STACK.md settled. Every one is permissive, and every one is vendored, so a surface loads nothing from anyone else's host.

| Library | Version | Used for | Licence | What it asks |
|---|---|---|---|---|
| Svelte | 5.57.0 | The application shell every surface is built with | MIT | Keep the notice |
| Bits UI | 2.19.2 | Keyboard and screen-reader behaviour behind our own looks | MIT | Keep the notice |
| TanStack Table core | 9.2.4 | The headless engine under our own table markup | MIT | Keep the notice |
| TanStack Table, Svelte adapter | 9.2.4 | Binding that engine to runes | MIT | Keep the notice |
| virtua | 0.51.3 | Long lists that stay fast, including the console | MIT | Keep the notice |
| uPlot | 1.6.32 | Every chart and sparkline | MIT | Keep the notice |
| anser | 2.3.5 | Reading the ANSI in a log line into text rather than markup | MIT | Keep the notice |
| partysocket | 1.3.0 | Reconnecting the live socket | MIT | Keep the notice |
| Valibot | 1.5.0 | Checking what a form sends before it leaves the page | MIT | Keep the notice |
| svelte-sonner | 1.2.1 | Toasts | MIT | Keep the notice |
| clsx | Joining class names in the copied shadcn-svelte components | MIT | Keep the notice |
| tailwind-merge | Resolving conflicting Tailwind classes in those components | MIT | Keep the notice |
| tailwind-variants | The variant shapes those components are written against | MIT | Keep the notice |
| tw-animate-css | The enter and leave keyframes their menus, dialogs and drawers use | MIT | Keep the notice |
| @internationalized/date | The calendar maths Bits UI's date parts take | Apache-2.0 | Keep the notice |
| Lucide icons | 1.2.134 of the Iconify set | The icon set, vendored into `design/icons` and subset to what is used | ISC | Keep the notice |
| Cormorant Garamond, Karla and JetBrains Mono | Fontsource 5.3.0 | The three typefaces, vendored as latin variable files | OFL-1.1 | Ship the licence text beside the fonts, which `packages/ui/src/fonts/LICENSE` does, and never sell the fonts on their own. It places nothing on Ambrose's own code |

shadcn-svelte is not installed and never will be: it is a scaffold whose source is copied in once per component and owned from then on, under MIT.

## What builds and tests the servers, and never ships

A development dependency. It is linked into the test programs only, and no server or tool carries it.

| Library | Used for | Licence | What it asks |
|---|---|---|---|
| GoogleTest and GoogleMock | Every C++ unit test | BSD-3-Clause | Keep the notice |

## What builds and tests the front end, and never ships

These are development dependencies. None of them is served to an operator or linked into a binary.

| Tool | Version | Licence | Note |
|---|---|---|---|
| Vite | 8.3.0 | MIT | |
| @sveltejs/vite-plugin-svelte | 7.3.0 | MIT | |
| Tailwind CSS and its Vite plugin | 4.3.3 | MIT | The generated theme is the only palette it can express |
| TypeScript | 6.0.3 | Apache-2.0 | Pinned to the 6 line, which svelte-check and typescript-eslint accept |
| svelte-check | 4.7.6 | MIT | |
| ESLint, typescript-eslint, eslint-plugin-svelte, @eslint/js, globals | 10.10.0, 8.70.0, 3.23.0, 10.0.1, 17.12.0 | MIT | |
| Prettier and prettier-plugin-svelte | 3.9.8, 3.4.0 | MIT | |
| Storybook, its Svelte framework and the a11y, vitest and Svelte CSF addons | 10.6.0, 10.6.0, 10.6.0, 10.6.0, 5.1.3 | MIT | Telemetry is off in the configuration |
| Vitest and @vitest/browser-playwright | 4.1.11 | MIT | Pinned to the 4 line, which the Storybook test addon requires |
| Playwright and @playwright/test | 1.63.0 | Apache-2.0 | |
| unplugin-icons | 24.0.0 | MIT | Compiles the vendored icons to inline markup at build time |
| @iconify-json/lucide | 1.2.134 | ISC | Read once by `apps/designtokens/icons.py` to write the committed SVG files |
| @fontsource-variable/* | 5.3.0 | OFL-1.1 | Read once to copy the latin variable files into the repository |
| @types/node | 24.10.3 | MIT | |
| axe-core | 4.13.0 | MPL-2.0 | File-level copyleft on its own source. It runs only in tests, and a lint rule refuses to import it from application code, so it never enters what the supervisor serves |
| Lightning CSS | through Vite and Tailwind | MPL-2.0 | The same reasoning: it processes CSS at build time and contributes nothing to the output |

The full transitive set is in `package-lock.json`. Rolldown and Lightning CSS ship a native binary per platform, which is why `apps/ci/ci_npm_cache.py` primes one cache covering every platform the lockfile names.

## Things this repository does not contain

- No Wizard101 file, asset, text or artwork. Tools read a user's own installation at run time and write nothing into it.
- No code copied from another Wizard101 server project, and none from Pterodactyl, whose MIT-licensed source is read only as a reference for how a hosting panel behaves.
- No proprietary SDK. A feature that needs one is built only for someone who holds their own licence, behind a build option that is off by default.
- No crash reporting library. Settled on 2026-09-18 for 17.83: a crash dump is written with the operating system's own writer, `MiniDumpWriteDump` from the Windows debugging library that ships with the system on Windows and a small writer of our own on Linux, so nothing is added to the build for it. Crashpad is the library that would otherwise do this job, and it is recorded here as an option to revisit rather than a dependency, because building it costs more continuous integration time than this project's budget has. Nothing in that milestone's grouping, symbolization or page depends on which writer produced the dump.

## Adding a dependency

Add its row here in the same commit, with what it is for and what its licence asks. Check the licence text the package ships, not a summary: two of the libraries considered for the panel were dropped because their published text did not match what their pages claimed. Prefer MIT, ISC, BSD, Apache-2.0 and BSL-1.0. A copyleft library is welcome where it stays in its own program, as Unicorn does in `typeextract`; if one would bind a server or the panel, say so in the commit and record the consequence in this file.

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
| Zydis and Zycore | Decoding x86 instructions in the type extractor | MIT | Keep the notice |
| Unicorn 2 | Running the user's own client program in a sandbox to rebuild its type data | GPL-2.0-or-later | Only `typeextract` links it, and only as a shared library. That one program is therefore distributed under GPL-2.0-or-later; every other program here stays MIT. A build without the tools has no GPL code in it |

## What will ship in the panel and the launcher window

These are settled in doc/UI-STACK.md and arrive with milestone 17.73. Every one is permissive, and every one is vendored, so a surface loads nothing from anyone else's host.

| Library | Used for | Licence |
|---|---|---|
| Svelte, Vite and the Svelte plugin | The application shell every surface is built with | MIT |
| Tailwind CSS | The token engine: the generated theme is the only palette that exists | MIT |
| Bits UI | Keyboard and screen-reader behaviour behind our own looks | MIT |
| shadcn-svelte | Starting points copied into the repository once and then owned, never a dependency | MIT |
| TanStack Table | Tables that sort, filter and page | MIT |
| virtua | Long lists that stay fast | MIT |
| uPlot | Every chart and sparkline | MIT |
| anser | Colouring the log lines a server sends | MIT |
| partysocket | Reconnecting the live socket | MIT |
| Valibot | Checking what a form sends before it leaves the page | MIT |
| svelte-sonner | Toasts | MIT |
| Lucide icons | The icon set, vendored and subset to what is used | ISC |
| Cormorant Garamond, Karla and JetBrains Mono | The three typefaces, vendored as variable fonts | OFL-1.1 |

Tools that never ship inside a build, such as Storybook, Vitest, Playwright, ESLint, Prettier and the vcpkg toolchain, carry their own licences and are listed in the lockfiles rather than here.

## Things this repository does not contain

- No Wizard101 file, asset, text or artwork. Tools read a user's own installation at run time and write nothing into it.
- No code copied from another Wizard101 server project, and none from Pterodactyl, whose MIT-licensed source is read only as a reference for how a hosting panel behaves.
- No proprietary SDK. A feature that needs one is built only for someone who holds their own licence, behind a build option that is off by default.
- No crash reporting library. Settled on 2026-09-18 for 17.83: a crash dump is written with the operating system's own writer, `MiniDumpWriteDump` from the Windows debugging library that ships with the system on Windows and a small writer of our own on Linux, so nothing is added to the build for it. Crashpad is the library that would otherwise do this job, and it is recorded here as an option to revisit rather than a dependency, because building it costs more continuous integration time than this project's budget has. Nothing in that milestone's grouping, symbolization or page depends on which writer produced the dump.

## Adding a dependency

Add its row here in the same commit, with what it is for and what its licence asks. Check the licence text the package ships, not a summary: two of the libraries considered for the panel were dropped because their published text did not match what their pages claimed. Prefer MIT, ISC, BSD, Apache-2.0 and BSL-1.0. A copyleft library is welcome where it stays in its own program, as Unicorn does in `typeextract`; if one would bind a server or the panel, say so in the commit and record the consequence in this file.

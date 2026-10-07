<!-- Project Ambrose by Imjustchico: Rules every AI agent follows when working in this repository. -->
# Project Ambrose

An experimental Wizard101 server written from scratch in C++ by AI agents under human direction. Read README.md, CONTRIBUTING.md, and doc/ARCHITECTURE.md first.

## Rules for AI agents

- Server code is C++20. Tools may use any language that does the job well, following the Tools decision in doc/ARCHITECTURE.md.
- Follow the structure, methods, and settled Decisions in doc/ARCHITECTURE.md, which mirror AzerothCore. Put new code in the folder its subsystem belongs to, and respect the layering order.
- Never commit client-derived files or code generated from them. Protocol definitions and the type dump load at runtime from the user's install.
- Work from doc/ROADMAP.md. Every milestone is open to every session and contributor, in any phase and any order, with nothing held or reserved and nobody's permission needed; doc/MILESTONE-TRACK.md says how to start one so others see it. Build one milestone per pull request, and when it rests on something not built yet, build that in the same pull request or take the dependency first. A milestone is finished only when every acceptance check in its phase file passes, and its checkboxes are ticked in the same commit. Resolve the phase's review notes as you reach the milestones they name.
- Read doc/TOOLS.md before starting a milestone, and look at what is already built under src/tools and apps. Use the tool that exists instead of writing the same thing again inside the milestone, and when one cannot do what the milestone needs, teach it that function rather than working around it. Every milestone should leave the suite able to read and decode more than it could at the start, so the next one begins further along. A local script that does a job a tool should own is a workaround; move it into the tool.
- Settle anything listed under Decisions needed in doc/ROADMAP.md with the option it recommends, at the maintainer's standing direction and without waiting to be asked, and record it under Resolved there and in doc/ARCHITECTURE.md in the same change.
- Every file starts with the Project Ambrose branding header and a one-line brief of what the file holds and does, in the format doc/ARCHITECTURE.md gives for its file type. Write no other comments anywhere.
- Clean-room implementation. AzerothCore and other emulators may be studied for structure and patterns, and other Wizard101 server projects for protocol and game behavior. Never copy, translate, or port their code, and never commit their data files.
- Never commit files extracted from the game client. Tools that read a user's own installation at runtime are fine.
- Every commit carries an AI attribution trailer naming the model that wrote it. A commit authored by a bot account, such as Dependabot's, names its tool in the author field instead and needs no trailer.
- Verify before claiming. State how a change was built and tested, and say plainly when something could not be verified.

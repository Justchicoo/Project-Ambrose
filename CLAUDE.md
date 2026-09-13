<!-- Project Ambrose by Imjustchico: Rules every AI agent follows when working in this repository. -->
# Project Ambrose

An experimental Wizard101 server written from scratch in C++ by AI agents under human direction. Read README.md, CONTRIBUTING.md, and doc/ARCHITECTURE.md first.

## Rules for AI agents

- Server code is C++.
- Follow the structure and methods in doc/ARCHITECTURE.md, which mirror AzerothCore. Put new code in the folder its subsystem belongs to, and respect the layering order.
- Work from doc/ROADMAP.md. Implement one milestone at a time, in order, after its dependencies are done. A milestone is finished only when every acceptance check in its phase file passes, and its checkboxes are ticked in the same commit. Resolve the phase's review notes as you reach the milestones they name.
- Do not settle anything listed under Pending decisions in doc/ARCHITECTURE.md on your own. Propose it, let the maintainer decide, then update that document.
- Every file starts with the Project Ambrose branding header and a one-line brief of what the file holds and does, in the format doc/ARCHITECTURE.md gives for its file type. Write no other comments anywhere.
- Clean-room implementation. AzerothCore and other emulators may be studied for structure and patterns, and other Wizard101 server projects for protocol and game behavior. Never copy, translate, or port their code, and never commit their data files.
- Never commit files extracted from the game client. Tools that read a user's own installation at runtime are fine.
- Every commit carries an AI attribution trailer naming the model that wrote it.
- Verify before claiming. State how a change was built and tested, and say plainly when something could not be verified.

# Project Ambrose

An experimental Wizard101 server written from scratch in C++ by AI agents under human direction. Read README.md and CONTRIBUTING.md first.

## Rules for AI agents

- Server code is C++. Do not choose the C++ standard, build system, or dependencies unilaterally; propose them and let the maintainer decide, then record the decision in the repo.
- Clean-room implementation. Other Wizard101 server projects may be read to understand protocol and game behavior. Never copy, translate, or port their code, and never commit their data files. Reimplement from understanding.
- Never commit files extracted from the game client. Tools that read a user's own installation at runtime are fine.
- Every commit carries an AI attribution trailer naming the model that wrote it.
- Verify before claiming. State how a change was built and tested, and say plainly when something could not be verified.

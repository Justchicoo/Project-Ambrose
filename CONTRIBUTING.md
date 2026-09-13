<!-- Project Ambrose by Imjustchico: How to contribute, including the rules for AI-written changes. -->
# Contributing

Project Ambrose is an AI-driven project. Contributions written with AI tools are expected, not just allowed.

## How to contribute

1. Fork the repository and create a branch for your change.
2. Read [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) so your change lands in the right place.
3. Use any AI coding tool you like to write the change.
4. Open a pull request that explains what the change does and how it was verified.

## Requirements

- **C++20** for server code. Tools may use any language that does the job well.
- **Follow the architecture.** The layout, layering, and methods in doc/ARCHITECTURE.md mirror AzerothCore. Database changes go in dated update files, content goes in the world database, and custom content goes in scripts or modules instead of core edits.
- **Branding header, no other comments.** Every file starts with the Project Ambrose header and a one-line brief of what it holds and does, in the format doc/ARCHITECTURE.md gives for its file type. Nothing else in the file is a comment.
- **From scratch.** Study AzerothCore for structure and other projects for game behavior, then reimplement. Do not copy, translate, or port code from any of them, and do not commit their data files.
- **No game files.** Never commit files extracted from the game client.
- **Disclose the AI.** Add a trailer to each commit naming the model or tool that wrote it, for example:

  ```
  Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
  ```

- **Verify it.** Say in the pull request how the change was tested. Unverified changes will not be merged.

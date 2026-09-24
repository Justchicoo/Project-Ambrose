<!-- Project Ambrose by Imjustchico: The questions every contributor pull request answers, taken from the review rulebook so a pull request arrives with its proof, on either the contributor track or a roadmap milestone. -->
## What this is

_One of two things, and one only per pull request._

- **Contributor track item:** its id and title, for example `C-61: a corpus of duration strings`.
- **Roadmap milestone:** its id and title, for example `4.04: world wire math and LocationString`. It has to be in doc/MILESTONE-TRACK.md's "Open now" table, and the branch has to be named `milestone/<id>-<short-name>`.

## What this adds

## How it was checked

_The commands you ran and what they printed. For a milestone, say which platform you built and tested on, and the line `ctest` ended with. For a schema or fixture describing a route that already exists, say which real server answer it was checked against. For a checker, name the broken copy of the input that made it fail._

## Acceptance checks

_Milestones only. Which boxes in the phase file this ticks, and what proves each one. Then every box still empty, and why it could not be run, such as a check that needs a second machine or a client session. An empty box nobody explains reads as a milestone half-built._

## Checklist

- [ ] One item or one milestone, and only the paths its track allows
- [ ] Every new file starts with the Project Ambrose header and a one-line brief, and carries no other comment
- [ ] No game files, captures of other people, credentials or private paths, and `git status` is clean
- [ ] Every commit carries a trailer naming the AI that wrote it
- [ ] The checks under "Before the pull request" pass, in `contrib/AI-START-HERE.md` or, for a milestone, `contrib/AI-MILESTONES-HERE.md`

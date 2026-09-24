<!-- Project Ambrose by Imjustchico: How to report a security problem privately, what counts as one, and what happens after a report. -->
# Security policy

Project Ambrose is pre-alpha and is not meant to face the internet yet. The servers, the admin API and the panel still handle passwords, session tokens and database credentials, so a weakness in them matters even now.

## Reporting a problem

Report it privately through GitHub: open the repository's **Security** tab and choose **Report a vulnerability**. That opens a private advisory that only the maintainer can read.

Please do not open a public issue, pull request or Discord message about an unfixed weakness.

A useful report says:

- what is affected: a server, the admin API, the panel, the launcher or a tool;
- how to reproduce it, with the build or commit you used;
- what an attacker gains.

## What counts

- Authentication or session flaws in the login server, the admin API or the panel.
- Anything that lets a client crash a server, read another account's data or run code.
- Secrets that end up in logs, the live log stream, crash reports or the repository.
- Path traversal or injection in anything that takes a file name, a query or a command.

Questions about how Wizard101 itself behaves are not security reports; the contributor track in [doc/CONTRIBUTOR-TRACK.md](doc/CONTRIBUTOR-TRACK.md) is the place for those.

## What happens next

The maintainer confirms the report, fixes it on `main` with a regression test, and credits the reporter in the advisory unless they ask not to be named. Only the latest `main` is supported; there are no release branches to back-port to yet.

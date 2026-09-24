<!-- Project Ambrose by Imjustchico: A practical guide to using an AI tool for compliant Project Ambrose contributions. -->

# Contributing with an AI tool

Project Ambrose is built with AI tools under human direction. An AI assistant can make a contribution quickly, but it must work from the repository's rules rather than inventing missing behavior. This guide describes a repeatable workflow for one contributor-track item.

## Start with the prompt that already exists

`contrib/AI-START-HERE.md` holds a prompt written to be pasted whole into any assistant. It already carries the document list below, the state of the project, every rule in the form its checker enforces, and what each open item needs of your machine. Paste it rather than assembling the same thing by hand, and use this guide for the part it cannot do: driving the assistant and judging what it gives back.

## The repository rules it works from

Before asking an AI tool to write anything, give it the relevant repository documents:

1. `doc/CONTRIBUTOR-TRACK.md` for allowed paths, open items, and merged work.
2. `contrib/findings/README.md` for finding shape and evidence rules.
3. `CONTRIBUTING.md` for project-wide contribution requirements.
4. The item-specific documentation only, such as `doc/TOOLS.md`, `doc/CAPTURE.md`, `doc/CLIENT.md`, configuration references, or the relevant guide.
5. `README.md` and `doc/ROADMAP.md` for the actual project status.
6. The file-header table in `doc/ARCHITECTURE.md`; do not edit architecture or roadmap files for a contributor-track item.

The supplied prompt should state the item id, the available environment, and the rule that the contribution must stay on the contributor track. If the assistant cannot see the repository, paste the relevant documents instead of asking it to guess.

## Match the item to what is available

Tell the assistant what is actually available:

- a client installation;
- a packet capture;
- a running Ambrose server;
- a full C++ build;
- MySQL or MariaDB;
- Windows or Linux;
- client-driver prerequisites; or
- only the ability to write a proposal.

Do not choose an item that needs a client, capture, database, or platform you do not have. A proposal can proceed with reasoning; a finding needs repeatable observation; a tool needs a runtime input and a focused output.

## Plan before writing

First, before any of that: if the plan is to add something the project lacks, have the assistant look for whether it lacks it. Two proposals on this track have set out to build work that already existed, one a startup check that `src/test/apps/AppSmokeTest.cmake` had been running all along, the other a manual setup path that milestones 3.20 and 3.22 automated. `doc/ROADMAP.md`'s "Where we are" paragraph says what is done in one pass, the phase file covers the area in detail, and a guard usually exists as a CTest entry. It costs five minutes and it is the difference between a contribution that lands and one that has to be re-aimed in review.

Ask the assistant to state:

- the exact item and allowed destination;
- the smallest behavior or claim that will be added;
- what evidence would prove it useful or correct;
- what would disprove it;
- the cheapest experiment that could invalidate the approach; and
- the checks that will run before commit.

For a tool, prefer the smallest input that answers the question. Classify files before opening them, compare cheap metadata before hashing, and never scan or copy client data unnecessarily. For a finding or note, record observations rather than presenting assumptions as facts. For a proposal, label the result as a proposal and give dependencies and acceptance criteria.

## Keep the change clean-room and in scope

The contribution may add or edit only under these prefixes:

- `contrib/tools/`
- `contrib/findings/`
- `contrib/notes/`
- `contrib/proposals/`
- `contrib/locale/`
- `apps/clientdriver/scenarios/`
- `data/sql/updates/pending_db_world/`
- `data/fuzz/`
- `doc/guides/`

Keep one thing per pull request. Do not edit `src/`, roadmap or phase files, CI files, repository manifests, or architecture documents.

Never commit client files, extracted assets, type dumps, protocol XML, captures, archive contents, encoded bytes, or anything generated from a client installation. A runtime tool may read the user's own installation, but it must not write inside that installation or store its contents in the repository.

Write everything from scratch. Do not copy code or text from another emulator, Wizard101 server, wiki, or public site. If a public source is necessary, name it and wait for maintainer acceptance of its license.

## Require useful failures

An assistant should not return a success-shaped fallback when input is invalid. Require it to:

- name the path and reason for an error;
- exit non-zero for invalid command-line input or an unusable required file;
- continue past optional unreadable inputs when the report can remain honest;
- identify skipped inputs and explain their effect;
- avoid broad exception swallowing; and
- document what would make the output wrong.

For a tool that follows a log, make clear that a quiet result is not proof of quiet traffic when the server's logging budget or logger level can suppress records.

## Use the exact file style

Every added file needs the Project Ambrose branding header and one-line brief in the format from `doc/ARCHITECTURE.md`, with no other comments. Files must be UTF-8 without a byte-order mark, use LF line endings, have no trailing whitespace, and end with a newline. JSON and binary files are exempt from the header rule and must not contain a comment key.

Run the repository style checker after writing:

```powershell
python apps/codestyle/codestyle.py
```

Fix every reported issue rather than weakening the checker or adding unrelated formatting changes.

## Review the assistant's output

Read the complete diff yourself. Check:

- every changed path is allowed;
- the contribution implements only the selected item;
- every assertion has evidence or is clearly marked as a proposal;
- no client-derived bytes or credentials are present;
- the README or guide says how the result was checked;
- errors are explicit and actionable;
- the code follows existing naming and layering rules; and
- no generated build output or temporary test data is tracked.

If the assistant reports a test result, inspect the command and output. A plausible summary is not evidence by itself. If a check could not run, preserve that limitation in the pull request.

## Commit and validate in the required order

Start each item on its own branch from an up-to-date `upstream/main`:

```powershell
git fetch upstream
git switch --detach upstream/main
git switch -c contrib/<item-name>
```

Before committing, check uncommitted paths:

```powershell
python apps/ci/ci_contrib_paths.py --paths <files>
```

After committing, run the range checks from the repository root:

```powershell
git fetch upstream
python apps/ci/ci_contrib_paths.py --range upstream/main...HEAD
python apps/ci/ci_findings.py
python apps/codestyle/codestyle.py
python apps/ci/ci_forbidden_files.py
```

If a scenario changed, also run:

```powershell
python apps/clientdriver/tests/test_clientdriver.py
```

Build and run the smallest relevant test command. CI build legs run only after a maintainer adds an appropriate `ci:` label, so a green path/style check does not prove that C++ compiles.

Every commit in the branch range needs an AI attribution trailer naming what wrote it. The checker matches case-insensitively and wants only a name and an address in angle brackets, so your assistant's own trailer is fine. For example:

```text
Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
```

## Write the pull request description

Include:

- the contributor-track item id;
- the files and behavior added;
- the source of the information;
- exact commands run and their results;
- anything that could not be verified;
- what would disprove the result; and
- any runtime prerequisites for a reviewer.

After a pull request is merged, do not reuse its branch. Refresh `upstream/main` and start the next item from a new branch so the next pull request contains only its own change.

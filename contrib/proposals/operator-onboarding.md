<!-- Project Ambrose by Imjustchico: Proposal for a first-ten-minutes operator onboarding path. -->

# Proposal: operator onboarding in the first ten minutes

## Item

C-53: define what a new operator should see and complete during the first ten minutes with Ambrose.

## Problem

Less of this is manual than it looks, and the proposal below is what is left.

3.22 is built: a first start finds the client installation, builds its type dump and extracts the name tables without asking. 3.20 added the guided setup behind it. A server started with no local configuration already exits naming the full path it looked for and the `.conf.dist` to copy. 5.08 is an installer. The settled direction is to remove steps, not to document them, so an onboarding path that walks five manual steps pushes against the thing it is trying to help.

What a new operator actually lacks is not the steps. It is knowing what just happened on their behalf, which of the remaining decisions is theirs, and how to tell a healthy result from a quiet failure.

## Proposed first-ten-minutes path

The onboarding surface shows what the first start did on its own, and asks only where automation genuinely cannot decide. Each thing it shows carries one success signal, one recovery link, and a retry that changes nothing silently.

1. **What was found.** The installation the first start located, the revision it reports, the type dump it built and the name tables it extracted.
   - Success: each is named with where it was written, none of it inside the client installation.
   - Recovery: what to do when no installation was found, and how to name one explicitly.
2. **What still needs a decision.** Today that is the database endpoint, and nothing else: host, port, user and database as separate redacted fields.
   - Success: the local `.conf` exists and names the operator's own endpoint, with the password never printed.
   - Recovery: the configuration layering, so an operator can see which layer their value came from.
3. **Whether it works.** The documented `--check` run.
   - Success: readiness reported, a clean shutdown, exit zero.
   - Recovery: the first error's category linked to the logging guide, and the exact configuration path that was used.
4. **What to watch.** Where the console and file log are written, and which categories own which subsystem: `server.<app>`, `server.config`, `server.logging`, `network` and `sql`.
   - Success: the operator can find `logs/Server.log` and read a healthy start.
   - Recovery: missing appenders, database failures and listener-port conflicts, each named.

The surface should display the current project status beside this: a real client reaches character select, while world, quests, combat, pets, housing and the operations panel are not yet available. That prevents a successful login-server check from being mistaken for a complete game-server deployment.

## Information the operator should see

Every step should show:

- the command or control being used;
- the working directory;
- the configuration file path;
- a short expected output or state;
- a link to the relevant existing guide;
- a redacted error summary when it fails;
- a retry action that does not silently change the operator's settings.

The path should preserve the existing headless-server model. It should not require a browser, a client installation, a KingsIsle launcher, or a remote service for the first successful check. Client setup can be a later, explicitly labeled path for operators who have their own installation.

## Safety and state rules

The onboarding path must:

- write only to the build output, the operator's local configuration, the configured log directory, and the disposable databases it owns;
- never write inside a client installation;
- never print passwords, access tokens, or complete database connection strings;
- identify commands that create or update a database before running them;
- provide a cleanup action for the disposable database and temporary logs;
- make destructive or irreversible actions require an explicit confirmation;
- keep a failed step's output available for copying without requiring a rerun.

## Cheapest disproof

Run a first start on a clean checkout with no local configuration and watch what it already tells the operator. If it names the installation it found, what it built, the path it wanted and the file to copy, then most of this proposal is already implemented and only the database decision and the status boundary remain. Then run it with an unavailable database: if the operator cannot tell which step failed, what state changed, and how to retry without losing the error, the remaining four are still worth building.

The lesson that produced this section: before writing that a project has not done something, look for whether it has. doc/ROADMAP.md's "Where we are" names 3.20, 3.22 and 5.08 in one paragraph, and the first version of this proposal was written without reading it.

## Dependencies and cost

This proposal depends on the existing build presets, `doc/config/README.md`, application configuration references, the logging guide, and the documented `--check` contract. It does not require a client installation or a new server subsystem. An implementation could begin as command-line onboarding text and later be surfaced by the panel or installer without changing the server's core layering.

## Acceptance

The proposal is ready for implementation when a new operator can:

- move through the five steps in order;
- distinguish a successful build, database setup, startup check, and running process;
- find the relevant logs after a failure;
- retry a failed step without hidden configuration changes;
- understand the current pre-alpha feature boundary; and
- complete the path without exposing credentials or touching client files.

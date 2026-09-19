<!-- Project Ambrose by Imjustchico: Proposal for a first-ten-minutes operator onboarding path. -->

# Proposal: operator onboarding in the first ten minutes

## Item

C-53: define what a new operator should see and complete during the first ten minutes with Ambrose.

## Problem

The project documents individual build, configuration, logging, and database steps, but a new operator still has to decide their order and how to tell whether each step succeeded. That makes a first run feel like a collection of disconnected commands, especially when the server stops at a missing local configuration or database connection.

The onboarding path should prove one small, useful outcome without promising features that are not built yet. Today that outcome is a healthy local server startup and a log that explains readiness; it is not a world, quest, combat, or panel session.

## Proposed first-ten-minutes path

The first-run experience should present five ordered steps, each with one action, one success signal, and one recovery link:

1. **Choose the build.** Show the supported compiler, CMake, and vcpkg prerequisites, then configure and build with the platform preset.
   - Success: the selected server executable and its `.conf.dist` file exist beside each other.
   - Recovery: point to the platform build section and the missing `VCPKG_ROOT` check.
2. **Create local configuration.** Copy the matching `.conf.dist` to the ignored local `.conf` file and show the required database and listener settings.
   - Success: the local file exists and contains the operator's own database endpoint without exposing the password in output.
   - Recovery: point to the configuration layering and application-specific option reference.
3. **Prepare a disposable database.** Run the existing database import path against a local development database.
   - Success: the database exists, updates are applied, and the operator can identify the database without printing credentials.
   - Recovery: show the database host, port, user, and database name as separate redacted fields.
4. **Run the startup check.** Execute the application's documented `--check` mode.
   - Success: the process reports readiness, shuts down cleanly, and exits zero.
   - Recovery: link the first error category to the logging guide and show the exact local configuration path used.
5. **Start and observe.** Start the server in the foreground and show where the console and file log are written.
   - Success: the operator can find `logs/Server.log` and identify the `server.<app>`, `server.config`, `server.logging`, `network`, and `sql` categories.
   - Recovery: explain missing appenders, database failures, and listener-port conflicts.

The onboarding surface should display the current project status beside this path: a real client currently reaches character select, while world, quests, combat, pets, housing, and the operations panel are not yet available. That prevents a successful login-server check from being mistaken for a complete game-server deployment.

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

Test the proposed sequence on a clean checkout with a deliberately missing local configuration and then with an unavailable database. If the operator cannot identify which step failed, what state was changed, and how to retry without losing the useful error, the five-step sequence is insufficient and this proposal should be revised before implementation.

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

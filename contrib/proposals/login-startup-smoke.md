<!-- Project Ambrose by Imjustchico: Proposal for a CI startup smoke check that proves a login server can become ready and shut down cleanly. -->

# Proposal: login-server startup smoke check

## Item

C-20: add a quality-bar guard for the server's documented `--check` startup path.

## Problem

The configuration documentation promises that `loginserver --check` starts fully, opens and validates its databases, binds its listener, logs `ready`, shuts down gracefully, and exits zero. The current contributor checks validate paths, findings, style, and forbidden files, but they do not prove that a built login server can complete that lifecycle.

A compile can pass while a configuration template, database update, listener bind, or shutdown path is broken. The failure then waits for a maintainer's requested CI build or a manual operator run.

## Proposed guard

Add one non-networked login-server smoke test to the build/test preset on legs that provide MySQL or MariaDB:

1. Create a disposable login database with a unique test name.
2. Generate a temporary local `loginserver.conf` from the distributed configuration.
3. Set the database connection to the disposable database and bind the login listener to `127.0.0.1` on an operating-system-selected free port.
4. Run `loginserver --check` with a bounded timeout.
5. Require exit code `0`, a readiness record from the `server.loginserver` category, and a graceful shutdown record.
6. Drop the disposable database in a finally-style cleanup step, including after timeout or process failure.

The test must use a unique working directory and database name for each run. It must not read a game client installation, contact KingsIsle services, or depend on a real account. It should exercise only the server's own startup and shutdown path.

## Required assertions

The smoke check should fail when:

- the distributed config cannot be loaded;
- a required database cannot be created, opened, or updated;
- the listener cannot bind;
- the process exits before logging readiness;
- `--check` exits non-zero;
- the process does not stop within the timeout;
- cleanup cannot drop the disposable database.

The output should include the first failure, the process exit code, the selected port, and the temporary database name. It must redact passwords and avoid printing the database connection string in full.

## Why `--check` is the right boundary

The option is already documented as the complete validation path. Reusing it avoids a second test-only startup mode that could drift from the real operator path. A successful check proves the application can load configuration, initialize the database and socket layers, reach readiness, and stop without requiring a client or a running realm.

The smoke test should not assert a specific log sentence beyond a stable readiness event or category/field. Human-readable wording can change without changing the lifecycle contract.

## Cheapest disproof

Before implementing the CI test, run `loginserver --check` against a temporary database with an intentionally occupied port and then with an invalid database credential. If either case exits zero, or if the process leaves a live listener or database behind, this proposal is wrong until the existing `--check` contract is corrected.

## Dependencies and cost

The implementation needs the existing login-server binary, the test database connection mechanism described in `doc/ARCHITECTURE.md`, and a MySQL or MariaDB service already used by database integration tests. It should be a small test harness rather than a new server feature. CI legs without a database should report the check as unavailable or omit the database-dependent leg, not silently skip a configured smoke test.

## Acceptance

The proposal is ready for implementation when a maintainer can point to one test command that:

- starts from a clean build output and distributed config;
- uses a disposable database and free local port;
- proves readiness and graceful shutdown;
- leaves no database, process, listener, or credential behind;
- produces a useful failure report; and
- runs in the same preset that already runs the server tests.

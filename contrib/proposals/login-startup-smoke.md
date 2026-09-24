<!-- Project Ambrose by Imjustchico: Proposal for a CI startup smoke check that proves a login server can become ready and shut down cleanly. -->

# Proposal: login-server startup smoke check

## Item

C-20: add a quality-bar guard for the server's documented `--check` startup path.

## Problem

Most of this is already guarded, and the proposal below is what is left.

`src/test/apps/AppSmokeTest.cmake` runs as `AppSmoke.loginserver` in the same preset that runs the server tests. It copies the shipped `loginserver.conf.dist` into a work folder, sets `BindIP=127.0.0.1` and the port to `0` so the operating system picks a free one, runs `--check` under a bounded timeout, and requires exit `0` with `loginserver ready` and `loginserver stopped` in the output. With `AMBROSE_TEST_DB` set it creates, updates, opens, closes and drops uniquely named login and character databases, and it asserts that a bad login database string exits 1.

So the lifecycle is proven today: configuration loads, databases open and update, the listener binds, readiness is logged, shutdown is graceful, and nothing is left behind. Two holes remain, and both are the kind that stay quiet rather than failing loudly.

## Proposed guard

Add two cases to the existing smoke check rather than a second harness.

1. **A port that is already taken.** Every run today asks for port `0`, which always succeeds, so no test has ever seen the bind fail. Open a listener on a local port, point the server's own port option at it, run `--check`, and require a non-zero exit naming the port. Without this, a bind failure is a path no test walks.
2. **A leg with no database.** Without `AMBROSE_TEST_DB` the database half of the check simply does less, and the test still passes, so a leg configured without a database looks identical to a leg where the databases were proven. Have the check report itself unavailable in that case, so a green result always means the same thing.

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

Run `loginserver --check` with its port set to one already held by another process. If it exits zero, the bind is not on the path `--check` walks and the first case above is worthless until that is corrected. Then run the existing `AppSmoke.loginserver` with and without `AMBROSE_TEST_DB`: if the two results are distinguishable without reading the log, the second case is already solved and this proposal is wrong.

The lesson that produced this section: the cheapest disproof of "nothing proves X" is to look for the test that proves X. The first version of this proposal described six steps that `AppSmokeTest.cmake` had already implemented, because it was written without that search.

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

<!-- Project Ambrose by Imjustchico: A safe, reproducible Docker workflow for running Ambrose with a local database and client mount. -->

# Running Ambrose with Docker

This guide describes the container boundary and the operator workflow that
milestone 17.23 is expected to package. It is a guide and acceptance checklist,
not a committed image, Compose file, client archive, or generated client
data. The Docker image and Compose definition are not present in the current
repository, so the commands in the packaging section remain a target workflow
until that milestone lands.

## What the stack must contain

The packaged stack has four responsibilities:

| Service | Responsibility | Persistent data |
| --- | --- | --- |
| MariaDB | Login, character, and world databases | database volume |
| Supervisor | Process control, panel, audit, and jobs | config, data, backups |
| Loginserver | Authentication and character list | logs and configuration |
| Gameserver and patchserver | Realm traffic and optional patch service | logs and configuration |

The supervisor owns process lifecycle and the panel. Browsers talk to the
supervisor only; database and app ports stay on the private Compose network
unless an operator explicitly publishes one for a local test.

The image must include time-zone data for the schedule engine. The exact
distribution and package are a packaging decision tracked by the roadmap, so
an image that cannot resolve `America/New_York`, `Europe/London`, and
`Australia/Lord_Howe` must fail its health check rather than silently using
UTC.

## Host prerequisites

Use a machine where Docker Engine and the Compose plugin are installed. Allocate
enough memory for MariaDB, the supervisor, and the selected server apps. Keep
the project checkout, database volumes, logs, and backups on a disk with
operator-controlled permissions.

Prepare a private client directory outside the repository only when the
selected server flow needs client data:

```text
C:\Games\Wizard101\        # example location; do not copy it into the checkout
```

The directory is mounted read-only. The container may build its own type dump
and local indexes in the Ambrose data volume, but it must never write into the
client mount. Do not bind-mount `Data`, `Bin`, WADs, protocol XML, screenshots,
captures, or generated dumps into a tracked repository path.

Create a local environment file outside Git for database passwords, panel
secrets, and the client path. Do not put credentials in a Compose file,
command history, issue, pull request, or log excerpt. Rotate a disposable
password if it appears in output.

## Target first-start workflow

When 17.23 is available, the clean-machine workflow should be:

1. Create a private data directory and a local environment file.
2. Point the read-only client mount at the operator's own installation, if
   client data is needed.
3. Start MariaDB and wait for its health check.
4. Start the supervisor and apps with `docker compose up`.
5. Follow the supervisor and loginserver logs until the ready lifecycle line.
6. Open the panel on its loopback binding and create the first owner through
   the one-time local setup flow.
7. Confirm the loginserver is reachable from the private Compose network and
   that the panel reports health.
8. Run a local login smoke check with a disposable account, if the operator
   has a compatible client and has completed the safe-session-capture
   prerequisites.

The setup path must not require a manually copied type dump. Automatic setup
uses the mounted installation and writes generated data to the Ambrose data
volume. If the client mount is absent, `Setup.Mode = off` is the explicit
headless option; startup must report that no client data is available rather
than searching the host or downloading anything.

## Network and volumes

Use an internal application network for MariaDB and app-to-supervisor traffic.
Publish only the panel's loopback port for a local operator test. Do not
publish MariaDB, the login port, or the patch port to all host interfaces by
default.

Use separate named or host-managed volumes for:

- `config`: local `.conf` files and generated configuration state;
- `data`: Ambrose's type cache, setup state, SQLite supervisor store, and
  application data;
- `logs`: application and supervisor logs;
- `backups`: operator-created backups; and
- the database's own storage.

Do not place secrets in an image layer. Do not use a host bind mount for
`/app/client` with write access. A backup of the data volume is not a backup
of the client installation and must not be described as one.

## Health checks and shutdown

The image health check should verify the local supervisor readiness endpoint
and that its time-zone lookup works. App health should be reported separately,
so a stopped gameserver does not make MariaDB look unhealthy.

Stop with `docker compose down` after the apps have received their normal
shutdown command. A forced container removal is a recovery action: inspect
the logs, verify database recovery on the next start, and do not treat a
cleanly stopped service as a crash. Keep restart policies conservative during
first setup so a configuration error is visible instead of restarting
indefinitely.

## Pterodactyl boundary

The Pterodactyl egg is a separate packaging target. It must use the ready
lifecycle line as its startup completion signal and `shutdown` as its stop
command. It must not expose a client installation to the panel or replace
Ambrose's supervisor permission model with a wildcard container command.

Run the Compose workflow first. Test the egg only on an operator-owned
Pterodactyl installation with disposable credentials and a private test
server.

## Verification checklist

The Docker packaging pull request should report:

- the Docker and Compose versions used;
- whether the run used a client mount, and that it was read-only;
- `docker compose config` completed without secrets being printed;
- MariaDB became healthy before the apps started;
- the supervisor and loginserver reached their ready lines;
- the panel remained on its intended bind address;
- the three required time-zone names resolved inside the image;
- a restart preserved databases, configuration, logs, and backups;
- a normal `docker compose down` produced no crash-shaped stop result; and
- any client login result, clearly marked as unavailable when no compatible
  client or safe local session existed.

The current repository cannot satisfy these checks because milestone 17.23's
Dockerfile and Compose definition have not landed. That is an explicit
limitation, not a reason to commit a placeholder image or claim a container
run succeeded.

## Safety limits

Never run KingsIsle's launcher or patcher. Never point a container at a public
client download, a third-party capture, or another user's database. Keep the
panel on loopback until TLS and the remote-access decision are implemented.
Follow [safe-session-capture.md](safe-session-capture.md) before collecting any
traffic, and retain only hand-written metadata rather than raw captures.

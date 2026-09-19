<!-- Project Ambrose by Imjustchico: An end-to-end Linux build and local login-server setup guide for Project Ambrose. -->

# Running Ambrose on Linux

This guide covers a clean Linux build through a local login-server startup. It was checked against the repository build, configuration, and logging documentation on 2026-09-18. The commands below are the documented path; the execution environment for this contribution was Windows, so Linux execution remains to be confirmed on the target distribution.

## Prerequisites

Use a supported Linux distribution with:

- CMake 3.25 or newer;
- GCC 13 or newer, or a supported Clang toolchain;
- vcpkg with `VCPKG_ROOT` set;
- Git;
- a reachable MySQL or MariaDB server for the login database.

The first configure may build dependencies from source and can take a while. Keep the repository and vcpkg on local storage with enough free space for the build tree and dependency cache.

## Prepare the checkout

Clone your fork and enter the repository:

```bash
git clone https://github.com/<your-account>/Project-Ambrose.git
cd Project-Ambrose
git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git
git fetch upstream
```

Do not copy client files, extracted assets, captures, or generated client data into the checkout. Ambrose reads a user's own installation at runtime; those files do not belong in the repository.

Set the vcpkg location in the shell that will configure the build:

```bash
export VCPKG_ROOT="$HOME/vcpkg"
```

If vcpkg is elsewhere, use that absolute path. Confirm it points to the vcpkg checkout before configuring:

```bash
test -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

## Configure and build

Configure with the repository's Linux preset:

```bash
cmake --preset linux-gcc
```

Build the debug preset:

```bash
cmake --build --preset linux-gcc-debug
```

The build places executables and their distributed configuration files below:

```text
build/linux-gcc/bin/Debug/
```

Run the unit tests before starting a server:

```bash
ctest --preset linux-gcc-debug
```

The test preset also runs repository style and CI checks. If you only need to compile without tests, configure a separate build with `-DBUILD_TESTING=OFF`; do not use that result as test evidence.

For an optimized local server, use the matching release preset:

```bash
cmake --build --preset linux-gcc-release
```

## Prepare the login server

Change to the directory containing the login-server executable:

```bash
cd build/linux-gcc/bin/Debug
cp loginserver.conf.dist loginserver.conf
```

The local configuration file is required. The server reports the missing path and the `.dist` file to copy if it is absent.

The default login database setting is:

```text
127.0.0.1;3306;ambrose;ambrose;ambrose_login
```

Create a database account that can create and update the Ambrose databases, or edit `LoginDatabaseInfo` in `loginserver.conf` to match an existing disposable development database. Keep credentials in the local ignored configuration and never commit them.

If the database server is not running or the connection string is wrong, startup stops with a database error. Fix that before changing protocol or client settings.

## Initialize the databases

The repository includes `dbimport` for creating the databases needed by the server. From the same binary directory, inspect its usage first:

```bash
./dbimport --help
```

Run the documented database-import command for the version of the checkout you built. Use a disposable development database, because updates and test setup are not production backups.

After import, confirm that the login database exists and that the account in `LoginDatabaseInfo` can connect to it. The server applies unapplied updates at startup and records them in the database.

## Check configuration without keeping the server running

Every server app accepts `--check`. It starts fully, including database and socket setup, logs `ready`, shuts down gracefully, and exits 0. A startup failure exits 1:

```bash
./loginserver --check
```

Use this after changing the local config or database settings. It is safer than discovering a syntax or bind failure during a real client session.

The default login listener is:

```text
BindIP = 0.0.0.0
LoginServerPort = 12000
```

For a local-only test, set `BindIP = 127.0.0.1`. If another process already uses the port, choose another local port and pass the same value to the client or launcher used for the test.

## Start and inspect logs

Start the login server from its binary directory:

```bash
./loginserver
```

It logs to the console and to `logs/Server.log` relative to the working directory when the file appender is enabled. Keep the process in the foreground for the first startup so an error is visible immediately.

The first healthy run should load configuration, open the database, bind the listener, and report readiness. If the server exits:

1. read the first error, not only the final line;
2. check the named configuration key or database setting;
3. rerun `./loginserver --check`;
4. inspect `logs/Server.log` after the correction.

The logging categories most useful during setup are `server.loginserver`, `server.config`, `server.logging`, `network`, and `sql`. Raise only the relevant logger to debug while investigating, then restore the normal level.

## Optional sanitizer builds

Linux has repository presets for runtime checking:

```bash
cmake --preset linux-gcc-asan
cmake --build --preset linux-gcc-asan
ctest --preset linux-gcc-asan
```

ThreadSanitizer and fuzz builds use their own compiler and preset requirements. Use those only when the matching compiler is installed. ThreadSanitizer may require:

```bash
sudo sysctl vm.mmap_rnd_bits=28
```

Apply system changes only on a development machine where you understand the security and operational impact.

## Common pitfalls

### `VCPKG_ROOT` is missing

Configure fails before compilation. Export `VCPKG_ROOT` in the same shell and rerun the preset.

### The local config is missing

Copy the matching `.conf.dist` beside the executable. Do not edit the distributed template or commit the local file.

### The database cannot be reached

Check that MySQL or MariaDB is listening, that the host and port are reachable, and that the account can access the configured database. A wrong password or database name is not fixed by rebuilding.

### The port is already in use

Set a free `LoginServerPort` or `WorldServerPort` in the local config and use the same endpoint in the client-side test setup.

### A client installation is unavailable

The current repository can build and start server components without committing a client installation. If a later test needs client data, point the documented runtime setting at your own install; never place that install inside the repository.

## Verification status

The repository path check, style check, forbidden-file check, and whitespace check were run against this guide. The Linux commands and expected behavior were cross-checked against `README.md`, `doc/config/README.md`, `doc/config/loginserver.md`, and `doc/config/logging.md`. A Linux host was not available for this contribution, so distribution-specific package names and the actual Linux process run still need confirmation by a Linux contributor.

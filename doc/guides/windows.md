<!-- Project Ambrose by Imjustchico: An end-to-end Windows build and local login-server setup guide for Project Ambrose. -->

# C-66: Running Ambrose on Windows

This guide covers a clean Windows checkout through a debug build, tests, and a
local login-server configuration. It keeps the repository checkout separate
from vcpkg and from all client data. The commands use PowerShell; run them from
the repository root unless a later step says otherwise.

## Prerequisites

Install the following before configuring:

- Windows 10 or newer, x64;
- Visual Studio 2022 or newer with **Desktop development with C++**, the
  Windows SDK, and MSVC tools;
- CMake 3.25 or newer;
- Git;
- vcpkg;
- Python 3 for the repository checks; and
- a reachable MySQL or MariaDB server for the login database.

The first configure builds the vcpkg dependencies from source and can take
about an hour. Later configure runs reuse the vcpkg package cache. Keep vcpkg
outside the repository so generated packages and tool downloads never appear
in the contribution diff.

Check the tool versions:

```powershell
cmake --version
git --version
python --version
```

Confirm that MSVC is available from a **Developer PowerShell for Visual
Studio**, or let the Visual Studio CMake integration select the newest
installed x64 toolset:

```powershell
Get-Command cl.exe
```

## Prepare the checkout and vcpkg

Clone your fork and add the canonical repository as `upstream`:

```powershell
git clone https://github.com/<your-account>/Project-Ambrose.git
Set-Location Project-Ambrose
git remote add upstream https://github.com/Justchicoo/Project-Ambrose.git
git fetch upstream
git switch --create my-windows-build upstream/main
```

Do not copy a Wizard101 installation, archive, type dump, capture, or
generated client data into the checkout. Ambrose reads a user's own
installation at runtime; those files do not belong in the repository.

If vcpkg is not installed, clone and bootstrap it in a separate directory:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
& C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Set `VCPKG_ROOT` in the same PowerShell session used for CMake:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

The final command must print `True`. If it prints `False`, fix the path before
configuring; the preset cannot find its toolchain without this variable.

## Configure and build

Configure with the repository's Windows preset:

```powershell
cmake --preset windows-msvc-x64
```

Build the debug preset:

```powershell
cmake --build --preset windows-debug
```

The build places executables and distributed configuration files below:

```text
build\windows-msvc-x64\bin\Debug\
```

Run the matching test preset:

```powershell
ctest --preset windows-debug
```

The test preset includes the repository codestyle and CI checks. Run it from a
real git checkout, not an exported folder: the forbidden-file check uses
`git ls-files`, and build artifacts copied into the tree are reported as
unknown files.

For an optimized local server, use:

```powershell
cmake --build --preset windows-release
```

If configuration stops while vcpkg is building a dependency, keep the same
`VCPKG_ROOT` and rerun the configure command after correcting the reported
network, disk-space, or toolchain problem. Completed packages remain in the
external vcpkg tree and are reused.

## Prepare the login server

Change to the debug binary directory and copy the distributed configuration:

```powershell
Set-Location build\windows-msvc-x64\bin\Debug
Copy-Item loginserver.conf.dist loginserver.conf
```

The local configuration is required. If it is absent, the server reports the
path it wanted and the `.conf.dist` file to copy. Never edit the distributed
template or commit the local file.

The default login database setting is:

```text
127.0.0.1;3306;ambrose;ambrose;ambrose_login
```

Use a disposable development database account, or edit `LoginDatabaseInfo` in
the ignored local `loginserver.conf`. Keep credentials in that local file and
never put them in a guide, issue, capture, or commit.

## Initialize and check the database configuration

From the same binary directory, inspect the database importer:

```powershell
.\dbimport.exe --help
```

Run the import command documented by that executable for the checkout you
built. Use a disposable development database because schema updates and test
setup are not production backups.

Every server app accepts `--check`. It creates or updates the configured
databases, binds its socket, reports readiness, shuts down cleanly, and exits
zero:

```powershell
.\loginserver.exe --check
```

An exit code of one means startup failed. Read the first error, correct the
named configuration or database problem, and run `--check` again before
starting a real server.

For a local-only listener, set this in the local configuration:

```text
BindIP = 127.0.0.1
LoginServerPort = 12000
```

If port 12000 is occupied, choose another local port and use the same endpoint
in the launcher or client-driver setup.

## Start and inspect logs

Start the login server in the foreground during the first run:

```powershell
.\loginserver.exe
```

The console shows startup progress. When the file appender is enabled, logs
are written below the configured `LogsDir`, commonly
`build\windows-msvc-x64\bin\Debug\logs\Server.log`.

The first healthy startup loads configuration, opens the database, binds the
listener, and reports readiness. If it exits:

1. read the first error, not only the final line;
2. check the named configuration key or database setting;
3. rerun `.\loginserver.exe --check`;
4. inspect the log after correcting the problem.

Useful categories during setup include `server.loginserver`, `server.config`,
`server.logging`, `network`, and `sql`. Raise only the relevant logger while
investigating, then restore the normal level.

## Common pitfalls

### `VCPKG_ROOT` is missing

The configure preset uses `$env{VCPKG_ROOT}` as its toolchain path. Set the
variable in the same PowerShell session and confirm the toolchain file exists.

### Visual Studio is not found

Install the C++ workload and Windows SDK, then use a Developer PowerShell or
select the installed Visual Studio instance in CMake Tools. A plain PowerShell
without `cl.exe` on `PATH` cannot compile manually, although the preset can
still discover an installed Visual Studio through CMake.

### The local config is missing

Copy the matching `.conf.dist` beside the executable. Do not commit the local
file.

### The database cannot be reached

Check that MySQL or MariaDB is listening, that the host and port are reachable,
and that the configured account can access the selected database. Rebuilding
does not repair a wrong password or database name.

### A port is already in use

Change `LoginServerPort` or `WorldServerPort` in the local configuration and
use the same endpoint in the local test setup.

## What was walked

This guide was walked from a clean checkout on Windows 10.0.19045 with
Visual Studio Build Tools 18.6.3 (MSVC 19.51.36246.0), Windows SDK
10.0.26100.0, CMake 4.4.3, Python 3.13.1, and vcpkg tool version 2026-07-27.
The `windows-msvc-x64` configure and `windows-debug` build completed
successfully after vcpkg built all 15 requested packages. `dbimport --help`
completed successfully. The test preset ran every registered test, a count that depends on which
targets the build produced: codestyle passed, and two cases failed on that
machine. Their names were not recorded, and the cause is not the checkout's
own history: the CI self-tests build a throwaway repository in a temporary
folder for each case, with their own author identity and signing disabled, so
local commits cannot reach them. If they fail for you, run
`ctest --preset windows-debug --output-on-failure -R ci` and report the names.

The login-server `--check` path was also exercised with a temporary copied
configuration. It reached client-data discovery and type-dump loading, then
failed as expected because no MySQL or MariaDB server was listening on
`127.0.0.1:3306`. A successful database-backed `--check` requires a local
database installation and credentials; none were recorded here. The temporary
configuration and generated client-data selection file were removed. Do not
attach credentials, client files, captures, type dumps, or generated build
directories.

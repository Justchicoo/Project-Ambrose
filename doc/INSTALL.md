<!-- Project Ambrose by Imjustchico: Clone, build, configure and start the Ambrose servers. -->

# Installing Project Ambrose

The installer supports Ubuntu 24.04 with GCC and Windows 11 with Visual Studio
2022. It uses vcpkg for every library, such as OpenSSL, Botan and MariaDB Connector/C. Install CMake
3.25 or newer and set `VCPKG_ROOT` before starting.

## What a clean machine needs

- **Your own Wizard101 install.** The servers read their game data from it and never change it. They find it
  themselves in the usual places; otherwise set `ClientDir` in each server's `.conf`, or `AMBROSE_CLIENT_DIR`.
- **A MySQL 8 or MariaDB 10.11 server** with an account that may create the `ambrose_*` databases. The shipped
  configuration names `ambrose` with password `ambrose` on `127.0.0.1:3306`; change the `*DatabaseInfo` lines
  in `dbimport.conf` and the servers' `.conf` files to use another.

## Linux

On Ubuntu, `deps --install` installs GCC, CMake, Ninja, Git and vcpkg into `$HOME/vcpkg`, and
`--with-database` also installs MariaDB and makes the `ambrose` account the shipped configuration names:

```bash
apps/installer/ambrose.sh deps --install --with-database
export VCPKG_ROOT="$HOME/vcpkg"
apps/installer/ambrose.sh deps
apps/installer/ambrose.sh compile
apps/installer/ambrose.sh conf
apps/installer/ambrose.sh db
apps/installer/ambrose.sh run supervisor
```

## Windows

Open a Visual Studio Developer PowerShell, set `VCPKG_ROOT`, and run:

```powershell
.\apps\installer\ambrose.ps1 deps
.\apps\installer\ambrose.ps1 compile
.\apps\installer\ambrose.ps1 conf
.\apps\installer\ambrose.ps1 db
.\apps\installer\ambrose.ps1 run supervisor
```

`compile` installs into `env/dist` by default. Set
`AMBROSE_INSTALL_PREFIX`, `AMBROSE_BUILD_TYPE` and `AMBROSE_PRESET` to choose
another install folder, build type or CMake preset.

`conf` copies each installed `.conf.dist` file beside its executable only when
the corresponding `.conf` file is absent. It is safe to run it again after
editing a configuration file.

`db` runs `dbimport`, which creates and updates the login, characters and world
databases using the connection settings in `dbimport.conf`. Edit that file
before running `db` when the default local MySQL account is not available.

`run supervisor` starts loginserver, gameserver and patchserver together. The
individual commands `run loginserver`, `run gameserver` and `run patchserver`
are also available.

Each server logs `<app> ready` once it serves. `apps/installer/tests/clean_ubuntu.sh` follows these steps in a
fresh `ubuntu:24.04` container, with your install mounted read-only through `AMBROSE_CLIENT_DIR`, and passes only
when loginserver, gameserver and patchserver all log `ready`.

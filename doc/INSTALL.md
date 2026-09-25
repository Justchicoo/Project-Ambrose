<!-- Project Ambrose by Imjustchico: Clone, build, configure and start the Ambrose servers. -->

# Installing Project Ambrose

The installer supports Ubuntu 24.04 with GCC and Windows 11 with Visual Studio
2022. It uses vcpkg for every library, such as OpenSSL, Botan and MariaDB Connector/C. Install CMake
3.25 or newer and set `VCPKG_ROOT` before starting.

## Linux

Install GCC, CMake, Ninja, Git and vcpkg, then run:

```bash
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

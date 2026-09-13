<!-- Project Ambrose by Imjustchico: How server configuration files, layers, environment overrides, and reloads work. -->
# Configuration

Every app reads its settings through `ConfigMgr`. Options are documented per app in `doc/config/<app>.md`, because config files hold no comments beyond their branding header. The logging options shared by every app are described in [logging.md](logging.md).

## File format

Each non-empty line is `Key = value`.

- Keys start with a letter and use letters, digits, `_`, and `.`, for example `WorldServerPort` or `Appender.Console`. Keys are case-sensitive.
- Values run to the end of the line with surrounding spaces trimmed. A value may contain `=`, but it may not start with one, so `Foo == bar` is an error.
- Wrap a value in double quotes to keep leading or trailing spaces. Inside quotes, `\"`, `\\`, `\n`, and `\t` are escapes.
- Lines starting with `#` are skipped, but the codestyle checker only allows them as the two-line branding header.
- Lines end with LF or CRLF. A carriage return anywhere else in a line is an error.
- A key defined twice in one file is an error. Every malformed line is reported with its file and line number, and a file with any error is not loaded.

## Layers

Later layers override earlier ones for the same key. Every default sits below every local edit, so a module's shipped defaults never undo a setting in the app's own config.

| Order | Source | Example |
|---|---|---|
| 1 | Shipped defaults | `gameserver.conf.dist` |
| 2 | Module defaults, sorted by file name | `conf.d/pets.conf.dist` |
| 3 | Local config, required | `gameserver.conf` |
| 4 | Module config, sorted by file name | `conf.d/pets.conf` |
| 5 | Environment variables | `AMBROSE_WORLD_SERVER_PORT=14000` |
| 6 | Command-line overrides | passed by the app at startup |

`conf.d` is the folder next to the local config. If the local config is missing, loading fails with a message naming its path and the `.dist` file to copy. A path that exists but is not a regular file, such as a folder, is reported as an error rather than skipped.

Every resolved value records where it came from: its layer, file, and line. The operations dashboard uses this to show effective settings against their defaults.

Each app's `<app>.conf.dist` is copied next to its executable on every build, and `cmake --install` places it in `etc/`.

## Environment variable names

An option's environment variable is `AMBROSE_` followed by the key in upper case, with an underscore inserted:

- in place of each `.`, `_`, or `-`
- before an upper-case letter that follows a lower-case letter or a digit
- before an upper-case letter that starts a new word after an acronym

| Key | Environment variable |
|---|---|
| `WorldServerPort` | `AMBROSE_WORLD_SERVER_PORT` |
| `BindIP` | `AMBROSE_BIND_IP` |
| `HTTPServerPort` | `AMBROSE_HTTP_SERVER_PORT` |
| `Appender.Console` | `AMBROSE_APPENDER_CONSOLE` |
| `Rate.XP.Kill` | `AMBROSE_RATE_XP_KILL` |

Two keys that map to the same variable, such as `Rate.XP` and `Rate_XP`, fail the load. An empty variable counts as unset on every platform.

Environment variables are read each time an option is requested, so they can supply options that no file defines. Such options do not appear in `GetKeysByString`, which lists only keys from files and overrides. Set variables before the app starts its threads. `conf/dist/env.dist` is a template of common variables.

## Typed options and warnings

`GetOption<T>(name, default)` supports strings, booleans, integers, and floating point. Character types are rejected at compile time. Booleans accept `1`, `0`, `true`, `false`, `yes`, and `no` in any case. A missing option or a value that does not parse, including an integer out of range for the requested type, returns the default and records one warning per option.

Warnings are buffered until the app installs a warning sink with `SetWarningSink`, which receives the buffered warnings first and then each new one as it happens. Apps install the logging sink at startup, so warnings appear as WARN lines in the `server.config` category. Calls to the sink are serialized, and the sink must stay valid until it is replaced. A successful load or reload lets each warning fire again.

## Reload

`Reload()` reads every layer again. If any layer has an error, the reload fails and the previous values stay in effect. Loads and reloads run one at a time, so a reload never undoes a newer `LoadInitial`.

## Build options

`conf/dist/config.cmake.dist` is a template for local CMake defaults. Copy it to `conf/config.cmake` and edit it. It only seeds a new build folder, so after editing it reconfigure with `cmake --fresh` or delete `CMakeCache.txt`. Values passed with `-D` on the command line or through presets still take precedence.

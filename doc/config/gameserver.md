<!-- Project Ambrose by Imjustchico: Every option in gameserver.conf.dist with its type, default, and meaning. -->
# gameserver options

See doc/config/README.md for the file format, layers, and environment variable names.

| Option | Type | Default | Environment variable | Meaning |
|---|---|---|---|---|
| `LogsDir` | string | `logs` | `AMBROSE_LOGS_DIR` | Folder for log files, relative to the working directory unless absolute |
| `WorldServerPort` | uint16 | `12000` | `AMBROSE_WORLD_SERVER_PORT` | TCP port the realm listens on for game clients |

<!-- Project Ambrose by Imjustchico: A reproducible C-43 load-report record with an explicit prerequisite and evidence boundary. -->

# C-43 load report

## Status

This report is **not measured**. The checkout used for this note does not
contain a runnable `loginserver.exe` or `gameserver.exe`, and the C-10 load
generator is not present on `upstream/main`. No generic TCP service is treated
as an Ambrose server, so no latency or capacity result is claimed.

## Required setup

Before filling this report, build the Ambrose server and the C-10
`ambrose-load-generator` from reviewed branches. Run both on a disposable
local database and bind the server to loopback. Use a fresh run directory
outside the repository and a payload agreed with the server endpoint; do not
place credentials, captures, client files, or response bytes in the report.

Record the machine and build before the run:

| Field | Value |
| --- | --- |
| Date and local timezone | `<fill in>` |
| Operating system | `<fill in>` |
| CPU model and logical processors | `<fill in>` |
| Memory | `<fill in>` |
| Build configuration and compiler | `<fill in>` |
| Ambrose commit | `<fill in>` |
| Load-generator commit | `<fill in>` |
| Server endpoint | `127.0.0.1:<port>` |
| Database mode | `<disposable local database details, no credentials>` |

## Run matrix

Use the same endpoint and payload shape for every row. Keep the payload
description semantic and do not paste its bytes into this note.

```powershell
.\ambrose-load-generator.exe `
  --host 127.0.0.1 --port <port> `
  --clients <clients> --requests <requests> `
  --payload-hex <private-runtime-value> `
  --response-bytes <count> --timeout-ms <timeout>
```

| Clients | Requests/client | Response bytes | Timeout (ms) | Exit code | Attempts | Failures | Timeouts | p50 (ms) | p95 (ms) | Max (ms) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` | `<fill in>` |

Stop the run if the private server becomes unhealthy. A nonzero exit code,
timeouts, or server errors must be reported rather than omitted from the
matrix.

## Interpretation boundary

The load generator reports client-side connection, request, failure, timeout,
and latency metadata. It does not prove a server capacity limit, database
capacity limit, or production-safe threshold. A completed C-43 report must
include the server log outcome, the exact bounded matrix, the named hardware,
and the Ambrose and generator commits. It must also state whether the server
remained healthy after each row.

Private run directories, payloads, captures, credentials, and full logs stay
outside Git. Public evidence is limited to the metadata in this report.

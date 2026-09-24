<!-- Project Ambrose by Imjustchico: Usage and safety boundary for the bounded TCP load generator. -->

# Ambrose load generator

This dependency-free C++20 tool opens bounded TCP sessions against an
Ambrose endpoint and reports connection, request, timeout, and latency
metadata. It does not contain a game protocol or client-derived payload.
Request bytes are supplied by the operator at runtime as a short hexadecimal
vector.

## How to build

```powershell
cd contrib\tools\ambrose-load-generator
cmake -S . -B build
cmake --build build --config Debug --target ambrose-load-generator
```

## How to run

```powershell
.\build\Debug\ambrose-load-generator.exe `
  --host 127.0.0.1 --port 4000 `
  --clients 4 --requests 25 `
  --payload-hex 00 `
  --response-bytes 1 --timeout-ms 1000
```

Each request uses a fresh TCP connection. `--response-bytes 0` measures
connect-and-send behavior without waiting for a response; a positive value
requires exactly that many response bytes before the request succeeds. The
tool prints counts and latency percentiles, never payload contents.

## Bounds and safety

- The default host must be `127.0.0.1` or `::1`; use `--allow-private` for an
  RFC1918 or RFC4193 address after confirming that the server is yours.
- Public addresses are always refused. DNS names and proxy settings are not
  used.
- Clients are limited to 256, requests per client to 10,000, payload bytes to
  4,096, response bytes to 4,096, and timeout to 60,000 milliseconds.
- The load is concurrent but bounded by the client count. Stop the run if the
  private server becomes unhealthy.
- Payloads, credentials, captures, client files, and response bytes are not
  written to disk or committed. Use disposable accounts and a private
  loopback test as required by `doc/guides/safe-session-capture.md`.

## Verification

Run `--self-test` for argument and hexadecimal-vector checks. For an endpoint
test, use a temporary local TCP echo service and keep the run on loopback.

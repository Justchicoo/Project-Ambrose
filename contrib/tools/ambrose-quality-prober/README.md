<!-- Project Ambrose by Imjustchico: C-27 standalone TCP session quality prober usage and output contract. -->

# C-27: Session quality prober

This dependency-free Python tool measures the quality of a TCP request/response
session against an operator-owned endpoint. It reports successful round-trip
times, jitter, connection failures, response timeouts, and a bounded summary
of the run. It sends only the configured synthetic probe payload; it does not
launch a client, inspect a capture, or contact a public service.

## Usage

Probe a local echo-compatible endpoint:

```powershell
python contrib\tools\ambrose-quality-prober\probe.py --host 127.0.0.1 --port 12000 --count 10
```

Targets are restricted to loopback by default. Add
`--allow-private-network` only when the endpoint is an operator-owned private
network service; public targets are rejected.

The default request is the ASCII payload `AMBROSE_PROBE_C27\n`. Use
`--payload` only with a test endpoint that explicitly expects that synthetic
request. `--connect-timeout` and `--response-timeout` are independent
deadlines. `--interval` spaces requests so a probe does not become a load
generator.

Output is JSON with the target, counts, RTT summary, jitter, timeout count,
failure count, and an overall `pass`, `fail`, or `unable` status. `--pretty`
formats the JSON for a report. Exit codes are:

- `0`: all requested exchanges succeeded and quality thresholds passed;
- `1`: the endpoint answered, but a configured quality threshold failed;
- `2`: invalid arguments or an invalid response;
- `77`: the endpoint could not be tested, such as an unavailable service or
  a service that did not return the probe payload.

Use `--max-rtt-ms` and `--max-jitter-ms` to add evidence-gated thresholds.
Without them, the tool measures and reports rather than inventing a pass/fail
quality policy.

## Local validation

Run the built-in loopback echo fixture:

```powershell
python contrib\tools\ambrose-quality-prober\probe.py --self-test
```

The fixture binds an operating-system-selected loopback port, echoes only the
synthetic C-27 payload, and shuts down before the command exits. It is not an
Ambrose server and does not prove the Ambrose protocol.

## Limits and safety

Run this only against a server you own or are authorized to test. Keep the
target on loopback or an operator-owned private network, start with a small
count, and do not use it as a stress tool. TCP connect time is not application login latency,
and an echo-compatible response is only a transport-quality measurement.
Protocol-specific probes belong in the client driver or a future protocol
tool. A timeout or refused connection is reported explicitly, never converted
into a zero or a successful fallback.

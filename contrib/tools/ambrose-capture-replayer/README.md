<!-- Project Ambrose by Imjustchico: C-23 private capture manifest replayer usage and safety contract. -->

# C-23: Capture replayer

This dependency-free Python tool replays a hand-written manifest of request
and expected-response byte vectors against an operator-owned TCP endpoint. The
manifest is read at runtime and must remain outside the repository. The tool
reports every response mismatch using lengths and SHA-256 digests; it never
prints or stores payload bytes.

## Manifest

Create the manifest in a private temporary directory:

```json
{
  "version": 1,
  "exchanges": [
    {
      "name": "handshake response",
      "request_hex": "010203",
      "expected_response_hex": "040506",
      "response_bytes": 3,
      "delay_ms": 0
    }
  ]
}
```

`response_bytes` is required and bounds the read for one exchange. The tool
opens one TCP connection, sends each request in order, reads exactly the
declared response length, and compares it with the expected bytes. A manifest
may contain at most 100 exchanges and each vector is limited to 1 MiB.

## Usage and safety

```powershell
python contrib\tools\ambrose-capture-replayer\replay.py `
  --manifest C:\Temp\private-session\manifest.json `
  --host 127.0.0.1 --port 12000 --pretty
```

Targets must be loopback by default. Add `--allow-private-network` only for an
operator-owned private IP; public hosts and hostnames are rejected. Use small
manifests and timeouts. Do not use this as a load generator. Never replay a
capture containing credentials, session keys, tokens, or another user's
traffic.

Exit codes are `0` when every exchange matches, `1` when one or more responses
differ, `2` for invalid input, and `77` when the endpoint cannot be tested.
The JSON report contains counts, names, response lengths, and hashes only.
It does not prove that a protocol is correct: it proves only that this private
vector produced the recorded response on this endpoint.

## Self-test

```powershell
python contrib\tools\ambrose-capture-replayer\replay.py --self-test
```

The self-test uses a loopback echo fixture and synthetic bytes, then removes
its temporary manifest. It does not use a capture, client installation, or
external network.

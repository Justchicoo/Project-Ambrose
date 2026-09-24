<!-- Project Ambrose by Imjustchico: A safe guide to reading metadata-only capture decoder output for a local login session. -->

# C-67: Reading a login capture with the metadata decoder

The capture decoder answers a narrow question: which Ambrose frames crossed a
private loopback TCP connection, in which direction, and with which protocol
metadata. It does not decode fields, print payloads, or prove that a login
account or client was valid. Use it only with a capture made from your own
client and your own local Ambrose server, following
[`safe-session-capture.md`](safe-session-capture.md).

## Build and self-test

Build the dependency-free C++20 tool outside the server build tree:

```powershell
Set-Location contrib\tools\ambrose-capture-decoder
cmake -S . -B build
cmake --build build --config Debug --target ambrose-capture-decoder
.\build\Debug\ambrose-capture-decoder.exe --self-test
```

The self-test creates a synthetic frame in memory and should print one
metadata line followed by `self-test: passed`. It creates no capture and does
not read a client installation.

## Decode a private capture

Keep the capture and decoder diagnostics outside the repository. The port is
the local login-server TCP port used by the capture:

```powershell
.\build\Debug\ambrose-capture-decoder.exe `
  --capture C:\Temp\ambrose-capture\login.pcapng `
  --port 12100
```

The decoder accepts little-endian pcapng with Ethernet or raw-IP loopback
packets. It follows TCP payload bytes in capture order, keeps separate
client-to-server and server-to-client streams, and reports a frame only after
all of its bytes are available. A missing or out-of-order segment is not
reconstructed; the result is reported as an incomplete stream.

Each frame line has this shape:

```text
frame=3 packet=8 direction=client-to-server kind=dml service=7 order=27 declared_length=... body_length=... first_dml_length=...
```

- `frame` is the one-based frame number within that directional TCP stream.
- `packet` is the decoder's count of packets containing a matching TCP
  payload, not the original pcapng block number.
- `direction` is relative to the selected server port.
- `kind=control` reports a control opcode; `kind=dml` reports a dynamic
  message list (DML) service and order.
- `declared_length` is the frame body length from the frame header.
- `body_length` is the bytes after the control byte or DML header.
- `first_dml_length` is the length of the first DML message in the frame.

The tool never prints addresses, ports, timestamps, payload bytes, strings,
credentials, or decoded client fields. Do not add those values manually to a
guide or issue.

## What a healthy login looks like

The decoder reports numeric service/order pairs, not message names. For the
login service, service `7` and the Ambrose-authored login definitions map the
important pairs as follows:

| Direction | Service/order | Message | Healthy-login meaning |
| --- | ---: | --- | --- |
| client to server | `7:27` | `MSG_USER_AUTHEN_V3` | The client submits the current authentication request. |
| server to client | `7:14` | `MSG_USER_AUTHEN_RSP` | The server accepts the credentials when its `Error` field is success. |
| server to client | `7:20` | `MSG_USER_ADMIT_IND` | The server admits the session; `Status=1` is the successful state. |
| client to server | `7:8` | `MSG_REQUESTCHARACTERLIST` | The authenticated client requests its character list. |
| server to client | `7:12` | `MSG_STARTCHARACTERLIST` | The server begins the character-list response. |
| server to client | `7:3` | `MSG_CHARACTERINFO` | One character record, repeated for each character. |
| server to client | `7:4` | `MSG_CHARACTERLIST` | The list terminates; `Error=0` is the successful result. |

The normal evidence is therefore directional and ordered: an authentication
request, a successful authentication response, admission, a character-list
request, and the character-list response. A test account with no characters
may show no `7:3` rows between `7:12` and `7:4`. A keepalive
`MSG_LOGIN_NOT_AFK` (`7:18`) may appear before, between, or after these
messages and is not a login failure.

The following is an illustrative metadata-only sequence. The lengths are
intentionally elided because they depend on the private account, revision,
and encoded fields:

```text
frame=1 packet=4 direction=client-to-server kind=dml service=7 order=27 declared_length=... body_length=... first_dml_length=...
frame=1 packet=6 direction=server-to-client kind=dml service=7 order=14 declared_length=... body_length=... first_dml_length=...
frame=2 packet=7 direction=server-to-client kind=dml service=7 order=20 declared_length=... body_length=... first_dml_length=...
frame=2 packet=9 direction=client-to-server kind=dml service=7 order=8 declared_length=... body_length=... first_dml_length=...
frame=3 packet=11 direction=server-to-client kind=dml service=7 order=12 declared_length=... body_length=... first_dml_length=...
frame=4 packet=12 direction=server-to-client kind=dml service=7 order=4 declared_length=... body_length=... first_dml_length=...
packets_with_tcp_payload=12 streams=2
```

This example is a shape to recognize, not a captured result. Do not claim
that a real login succeeded from frame order alone: verify the relevant
server log, database result, and the actual message fields in a private
environment. The decoder intentionally cannot make that determination.

## Reading anomalies

- A client `7:14`, `7:20`, `7:12`, or `7:4` line is directionally suspicious:
  those are server messages in the login table, so check stream assignment
  and the capture port before drawing a conclusion.
- A server `7:27` line is likewise suspicious because it is the client
  authentication request.
- `kind=control` is transport/session metadata, not a login message. Record
  its opcode and direction without guessing its payload.
- `incomplete_stream=...` means the capture ended with buffered bytes that did
  not form a complete frame. Treat the run as incomplete, not as evidence of
  a malformed protocol frame.
- `streams` counts directional TCP streams selected by the port filter.
  `packets_with_tcp_payload` counts only matching packets with non-empty TCP
  payloads.

If a line does not fit the expected sequence, preserve only the safe metadata:
the client revision, direction, frame and packet numbers, service/order or
control opcode, lengths, and the observed limitation. Never attach the pcapng
file, a packet dump, a screenshot containing client text, or a decoder output
that includes private paths.

## Cleanup and repeatability

After recording metadata:

1. stop the client, server, and capture process normally;
2. remove the private pcapng and any `tshark` diagnostic file;
3. destroy or rotate the disposable account and password;
4. remove temporary logs and run folders; and
5. check `git status` for captures, client files, credentials, or generated
   output before committing anything.

Record the client revision, server revision, date, local port, decoder commit,
and whether the stream was complete. A metadata observation remains limited
evidence until the project reproduces it with its own implementation or a
separate local test.

## Verification and limits

The decoder's `--self-test` was run from this checkout on 2026-09-22. This
guide was checked against the decoder implementation, the login message
definitions, and the safe-capture rules. No private capture was read,
retained, or added to the repository while writing this guide.

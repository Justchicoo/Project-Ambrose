<!-- Project Ambrose by Imjustchico: A safe workflow for capturing local sessions without retaining credentials or client data. -->

# C-46: Capturing a session safely

Captures are useful evidence for protocol findings, but a packet capture can contain passwords, session keys, account identifiers, client-derived payloads, and other users' data. Treat the capture as a private temporary artifact, not as a report attachment or repository input.

## Stay on the permitted network

Capture only a session between your own client installation and your own Ambrose server on loopback. Do not run KingsIsle's launcher or patcher, contact KingsIsle servers, or capture another user's traffic.

The client-driver capture helper uses `tshark` on the Npcap loopback adapter and filters the selected TCP port. Run its preflight check first:

```powershell
python apps\clientdriver\drive.py check
```

The command exits `77` when the machine cannot run the driver. Treat that as a prerequisite limitation, not as evidence that a scenario or protocol claim failed.

## Use a disposable account and server

Before starting:

1. Use a local Ambrose database that contains no real user's credentials.
2. Create a disposable test account with a password used only for this run.
3. Use a test server and a temporary working directory.
4. Confirm the login and game ports are loopback-only.
5. Close unrelated packet-capture tools and clients.
6. Record the client revision from your own installation's `Bin\revision.dat`; do not copy client files into the repository.

If the purpose is only to study framing or a pre-authentication handshake, stop the run before entering a password. If authentication is required, use only the disposable account and destroy or rotate it immediately after the capture.

## Start and stop the capture

The client driver starts `tshark` with a filter for the selected TCP port and writes the capture to its run folder. Keep that folder outside the repository:

```powershell
python apps\clientdriver\drive.py run --scenario <scenario> --capture-dir C:\Temp\ambrose-capture
```

Use the actual command and scenario supported by the current driver; run `python apps\clientdriver\drive.py --help` when the options differ. Never place the capture under `contrib/`, `src/`, `data/`, or another tracked folder.

Let the scenario finish or stop the driver normally so `tshark` receives its console-control shutdown and finalizes the file. If the process must be killed, treat the capture as possibly truncated and record that limitation. Do not assume that a file is readable merely because it exists.

Keep the `.pcap` and the helper's `.tshark.txt` diagnostic file private. The diagnostic file can contain interface or path information, while the capture can contain credentials and client data.

## Review before sharing anything

Do not open a capture in a public issue, commit it, or upload it to an external service. First inspect it locally and decide what question it answers. Remove or destroy the capture when the evidence is recorded.

Safe report material is metadata such as:

- client revision;
- date and local test setup;
- direction and frame numbers;
- message or opcode names;
- payload lengths;
- field names and offsets;
- hashes of private files; and
- the observed result and its limits.

Do not include:

- raw packet bytes;
- hexadecimal or base64 payloads;
- passwords, session keys, bearer tokens, or cookies;
- account names, email addresses, or personal paths;
- client archives, assets, type dumps, or extracted text;
- screenshots that show credentials or client content; or
- traffic belonging to another user.

An offset, length, field name, or hash is useful only when it does not allow the underlying client data to be reconstructed. If unsure, keep the artifact private and ask the maintainer through the project's private channel.

## Record a finding without the capture

Write the finding from your own observation and identify the revision read from your own installation. State:

1. the single claim;
2. the exact steps another contributor can repeat with their own local setup;
3. frame numbers, directions, sizes, field names, or other metadata observed;
4. what result would disprove the claim;
5. the confidence level; and
6. whether the finding is still `claimed`.

The repository's finding schema is documented in `contrib/findings/README.md`. A merged finding remains `claimed` until Ambrose re-derives it with its own capture or test. Never paste a packet into JSON, Markdown, a test fixture, or a commit.

## Cleanup checklist

After recording the metadata:

- stop the client and Ambrose processes;
- verify that `tshark` has exited and the file is finalized;
- destroy the disposable account or rotate its password;
- remove the private `.pcap` and `.tshark.txt`;
- remove temporary logs and run folders;
- check that no capture, screenshot, client file, or credential is staged in Git; and
- keep only the hand-written finding or note that contains the non-sensitive facts.

If a credential appeared in a log, screenshot, command history, or capture, rotate it before continuing. Deleting the file is not a substitute for rotating a secret that may already have been copied.

## Cheapest disproof

Before collecting a full session, run a short pre-authentication capture against the disposable server and inspect its frame count and directions locally. If the question can be answered from the handshake alone, do not capture login or world traffic. If the driver cannot isolate the intended loopback port, stop and fix the filter before using the capture as evidence.

## Verification and limitations

This guide was checked against `doc/CAPTURE.md`, the client-driver capture helper, and the contributor-track clean-room rules on 2026-09-19. The repository does not provide an automatic sanitizer that can guarantee a packet capture contains no secrets. Human review, disposable credentials, private handling, and deletion remain required.

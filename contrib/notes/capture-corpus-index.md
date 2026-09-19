<!-- Project Ambrose by Imjustchico: C-28 index of capture subjects, coverage, and evidence gaps. -->

# C-28: Capture corpus index

This note indexes the capture subjects that are currently documented for
Project Ambrose and distinguishes known coverage from open questions. It is an
inventory of evidence topics, not a packet archive. No `.pcap`, diagnostic
capture, screenshot, client file, payload, credential, or raw byte is stored
in this repository.

## Evidence handling rule

Raw captures remain on the contributor's private machine. A public contribution
may record only metadata that cannot reconstruct the underlying traffic:
revision, date, direction, frame number, message or opcode name, payload
length, field name, offset, hash, and an observed result with limits. A hash is
not a substitute for reviewing the private artifact. If a capture contains a
credential, rotate the credential before deleting or sharing anything.

## Current index

| Id | Subject | Source recorded in repository | Coverage | Open question |
| --- | --- | --- | --- | --- |
| K-01 | SessionOffer length and SessionAccept response | `doc/CAPTURE.md`, finding (a) | Partial: a 23-byte offer was accepted by the 1.610 client in a local reference-server session; Ambrose's decoder retains trailing bytes | Whether the 28-byte patch-tool offer is accepted by the same client and what its extra bytes mean |
| K-02 | Server keepalive opcode 3 and client response opcode 4 | `doc/CAPTURE.md`, checklist (b) | None recorded | Server body layout, response body, and timing |
| K-03 | Long-frame length field | `doc/CAPTURE.md`, finding (c) | Negative only: the largest observed message was 1,138 bytes | Whether `0x8000` is body-only or includes the frame header, and the following `u32` meaning |
| K-04 | Multiple DML messages in one frame | `doc/CAPTURE.md`, checklist (d) | None recorded | Whether the length can cover multiple sub-headers and how a decoder iterates them |
| K-05 | Client idle keepalive cadence | `doc/CAPTURE.md`, checklist (e) | None recorded | Time between client opcode-3 frames and the elapsed field |
| K-06 | Offer and accept timestamps | `doc/CAPTURE.md`, finding (f) | Partial: a reference server's full-seconds `TimeHigh` was accepted; Ambrose uses upper seconds bits pending capture | What the 1.610 client echoes in SessionAccept and whether it validates `TimeHigh` |

The index deliberately uses `K-` identifiers rather than pretending these are
findings. `doc/CAPTURE.md` remains the source of the current facts, and a
future protocol finding should cite the relevant index row plus the private
capture metadata used to derive it.

## What the corpus covers

The documented material covers:

- one local pre-authentication/session-offer observation;
- one observed maximum message size below the long-frame marker;
- one timestamp behavior observation; and
- the exact list of six framing/control questions the project still needs to
  answer.

It does not cover a complete login, realm-list, character-select, world-entry,
idle, zone, or logout session. It does not establish server keepalive cadence,
long-frame interpretation, coalesced DML behavior, or client idle timing.
Silence in this index means no repository-safe evidence was recorded, not that
the behavior never occurred.

## Adding an entry

Before collecting evidence, define the smallest capture that could answer the
question. Prefer a loopback pre-authentication run when it is sufficient. Use a
disposable account and local Ambrose server, record the revision from the
contributor's own installation, and keep the capture directory outside the
repository. Stop the driver normally so the capture is finalized.

After private review, add one row only when the row can state:

1. the single subject and a stable identifier;
2. the source document or finding that records the result;
3. whether coverage is positive, negative, partial, or absent;
4. the safe metadata retained; and
5. the question that would disprove or refine the current conclusion.

Delete the private `.pcap`, `.tshark.txt`, temporary logs, and disposable
account when the metadata has been recorded. Do not paste a packet into this
note or into a fixture. Follow `doc/guides/safe-session-capture.md` for the
full cleanup and credential-rotation checklist.

## Verification and disproof

This C-28 index was checked against `doc/CAPTURE.md` and the safe-capture guide
on 2026-09-19. It is correct only for the repository facts named in those
documents. A future capture disproves or changes a row when its privately
reviewed metadata answers the row's open question, contradicts the listed
coverage, or reveals that the cited source changed. The raw artifact still
does not belong in the repository.

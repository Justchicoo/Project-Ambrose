<!-- Project Ambrose by Imjustchico: How a maintainer records wire facts from their own client session, and the framing and control-message facts found so far. -->

# Wire capture verification

Some framing and control-message facts cannot be learned from the client's files. They are settled by watching the maintainer's own Wizard101 client talk to a local server. Captures stay on the maintainer's machine and are never committed. Only the facts learned from them are written here, as plain statements and hand-written byte vectors.

## Recording a session

1. Run an Ambrose server, or any local server that speaks the handshake, on the loopback address.
2. Put a transparent TCP proxy or a loopback packet capture between the client and that server. The tool must log every raw frame in both directions, with a timestamp and direction, including control frames. Any tool the maintainer trusts is fine.
3. Launch the client from the maintainer's own install with `-L 127.0.0.1 <login port>` and `-P 0`, log in, enter the world, and stay idle for at least 3 minutes so keepalives appear. Then zone to a busy area so large messages appear.
4. Stop the capture and answer the checklist below from the raw frames. Record each answer here with the client revision and date, and turn it into a hand-written vector in `src/test/server/shared/Network/ControlMessagesTest.cpp` or `FrameTest.cpp`.

## Checklist

| Item | What to look for | Status |
|---|---|---|
| (a) SessionOffer length | The server's first frame, and whether the client answers with SessionAccept (opcode 5) | Partly answered, see below |
| (b) Server keepalive | Any server frame with opcode 3, its body layout, and whether the client answers with opcode 4 | Open |
| (c) Long frames | A frame whose length field is 0x8000, the u32 after it, and the frame's real size | Open |
| (d) Multiple DML messages in one frame | A DML frame whose length covers more than one sub-header | Open |
| (e) Client keepalive cadence | Time between client frames with opcode 3 while idle, and the elapsed field | Open |
| (f) Offer and accept timestamps | Whether the client accepts an offer whose TimeHigh is the upper 32 bits of Unix seconds (0 today), and what it echoes in SessionAccept | Open |

## Findings

- **(a)** The 1.610 client (r806919) accepts a 23-byte SessionOffer frame, a 14-byte body of session id, two 32-bit time values, and milliseconds. In a local session against a reference server that sends this form, the client went on to send MSG_USER_AUTHEN_V3. A 28-byte offer (a 19-byte body) is what a live KingsIsle patch server sends to patch tooling; whether the 1.610 client accepts that form, and what the extra 5 bytes mean, is still open. Ambrose sends the 23-byte form, and its decoder keeps any trailing offer or accept bytes.
- **(c)** The same session's largest message was 1138 bytes, so it holds no long frame. Both readings of the long length (the body only, or the header and body) are implemented behind `LongFrameLength`, and `BodyOnly` stays the default until a capture settles it.
- **(f)** The reference server sets TimeHigh to the full seconds value rather than the upper 32 bits, and the client accepted it, so the client may not check TimeHigh. Ambrose sends the upper 32 bits until a capture says otherwise.

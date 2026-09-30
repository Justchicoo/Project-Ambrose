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
| (a) SessionOffer length | The server's first frame, and whether the client answers with SessionAccept (opcode 5) | Answered: the 23-byte form |
| (b) Server keepalive | Any server frame with opcode 3, its body layout, and whether the client answers with opcode 4 | Answered |
| (c) Long frames | A frame whose length field is 0x8000, the u32 after it, and the frame's real size | Answered: header and body |
| (d) Multiple DML messages in one frame | A DML frame whose length covers more than one sub-header | Answered: none seen |
| (e) Client keepalive cadence | Time between client frames with opcode 3 while idle, and the elapsed field | Answered: every 10 seconds |
| (f) Offer and accept timestamps | Whether the client accepts an offer whose TimeHigh is the upper 32 bits of Unix seconds (0 today), and what it echoes in SessionAccept | Answered |

## Findings

- **(a)** The 1.610 client (r806919) accepts a 23-byte SessionOffer frame, a 14-byte body of session id, two 32-bit time values, and milliseconds. In a local session against a reference server that sends this form, the client went on to send MSG_USER_AUTHEN_V3. A 28-byte offer (a 19-byte body) is what a live KingsIsle patch server sends to patch tooling; whether the 1.610 client accepts that form, and what the extra 5 bytes mean, is still open. Ambrose sends the 23-byte form, and its decoder keeps any trailing offer or accept bytes.
- **(c)** A long frame's u32 length counts the frame's header and body, not the body alone. In the client driver's badges-and-long-frame runs on 2026-09-30, `server announce --repeat 16` sent one MSG_SERVERMESSAGE of about 32000 bytes, over the 0x7780 a short frame's length can hold. Written with the body-only length (run 20260930-112752), the r806919 client closed its game connection 9 ms after the frame arrived and tried to reconnect. Written with the header-and-body length (run 20260930-113124, and 20260930-114953 once it became the default), the client logged the whole message as a Server Message and stayed connected. `Network.LongFrameLength` now defaults to `HeaderAndBody`, and `BodyOnly` stays available.
- **(f)** The reference server sets TimeHigh to the full seconds value rather than the upper 32 bits, and the client accepted it, so the client may not check TimeHigh. Ambrose sends the upper 32 bits until a capture says otherwise.
- **(a)**, **(b)**, **(e)** and **(f)** from the client driver's c79-idle-login-keepalive run 20260930-111824 on 2026-09-30, with the login server's session log at DEBUG. The r806919 client took Ambrose's 23-byte offer and answered with SessionAccept, which echoes TimeHigh 0, the offer's seconds as TimeLow and its milliseconds, followed by 5 trailing bytes the decoder keeps; so the client accepts an offer whose TimeHigh is the upper 32 bits of Unix seconds. Idle at the login prompt, the client sent a 6-byte KeepAlive every 10 seconds, its elapsed field counting the minutes since the session began (0 for the first minute, then 1), and the server answered each. The server's own KeepAlive, session id and milliseconds, went out every 60 seconds (`Network.KeepAliveInterval`), and the client answered each with a KeepAliveRsp within 0 to 6 ms.
- **(d)** In the same run and in the in-world run 20260930-114953, which logs every frame holding more than one DML message at DEBUG, the client never packed two messages into one frame. The server still reads any number it is sent, up to `Network.MaxDmlMessages`.

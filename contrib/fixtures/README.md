<!-- Project Ambrose by Imjustchico: Synthetic fixture corpora for contributor-track parsers and protocol-independent tests. -->

# Contributor fixture corpora

The C-58 corpus is a hand-shaped, deterministic set of 500 synthetic Ambrose
console records. It contains no captures, client-derived text, credentials,
real account names, or private paths.

Run its validator from the repository root:

```powershell
python contrib\fixtures\validate_c58.py
```

The validator checks record identity, fixed-column level and category fields,
all six severity levels, the documented logging categories, every expected
C-29 value-class span, UTF-8 byte offsets, and the required C-58 count.

The C-59 fixture is a deterministic sixty-second metric ring for the dashboard's
live status view. It contains a two-second gap where samples were missed but the process kept running, and a five-second gap followed by a restart, listed in `restarts` by the time the process came back, so the chart can show a
process stop and a clean relaunch without inventing a bogus zero-value sample.

Run its validator from the repository root:

```powershell
python contrib\fixtures\validate_c59.py
```

The validator checks the ring contract: exactly sixty seconds, a pair of
`times` and `values` arrays for each metric, the null gap and restart, and the
expected session, tick and resident-memory ranges.

The C-61 corpus is a deterministic list of duration strings for the server's ban
parser and the helper in `src/common/Utilities/Duration.cpp`. It covers plain
seconds, compound units, permanent bans, and malformed inputs the parser must
reject.
seconds, compound units, permanent bans, and the malformed inputs the parser
must reject.

Run its validator from the repository root:

```powershell
python contrib\fixtures\validate_c61.py
```

The validator checks each accepted string's exact second count and confirms that
all malformed values are refused instead of being treated as valid.

The C-62 corpus contains synthetic configuration cases covering the shipped
`.conf.dist`, sorted `conf.d` defaults and config files, the required local
`.conf`, environment variables, and command-line overrides.

Run its validator from the repository root:

```powershell
python contrib\fixtures\validate_c62.py
```

The validator applies the same layer order as `ConfigMgr` and compares each
case with its expected effective values.

The C-63 corpus contains deterministic terminal lines for the console layout:
middle-truncated categories, repeated prefixes on multi-line messages, a value
at the end of a line, an empty category, and an empty physical line.

Run its validator from the repository root:

```powershell
python contrib\fixtures\validate_c63.py
```

The validator renders each case using the documented short timestamp and
18-character category column, then compares every expected line byte-for-byte.
## C-74 replay manifests

`c74-login-replay.json`, `c74-wrong-password-replay.json`, and
`c74-keepalive-replay.json` are capture-free version-1 manifests for the
`contrib/tools/ambrose-capture-replayer/replay.py` tool. The login vectors
encode the message-definition bodies: success and refusal use distinct
`MSG_USER_AUTHEN_RSP.Error` values, the successful sequence carries
`MSG_USER_ADMIT_IND.Status=1`, and its character-list completion carries
`MSG_CHARACTERLIST.Error=0`. They contain no client capture, credential,
address, or client-derived payload bytes.

Run `python contrib\fixtures\validate_c74.py` to validate the three manifests.

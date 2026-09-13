<!-- Project Ambrose by Imjustchico: Roadmap phase 1, Foundations to first handshake. -->

# Phase 1: Foundations to first handshake

**Done when:** The retail client, started with -L 127.0.0.1 12000 -P 0, completes SessionOffer/SessionAccept with Ambrose loginserver and stays connected. The server log names its MSG_USER_AUTHEN_V3 (7:27).

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 1.01 | Toolchain hello (FND-1) | S | - |
| 1.02 | Unit test harness (FND-2) | S | 1.01 |
| 1.03 | Codestyle checker (FND-3) | M | 1.01 |
| 1.04 | CI pipeline (FND-4) | M | 1.02, 1.03 |
| 1.05 | common/Utilities (FND-5) | S | 1.02 |
| 1.06 | ByteBuffer and DML primitives (FND-6 + NET-1) | S | 1.05 |
| 1.07 | BitReader/BitWriter (OBJ-1 + FND-6) | S | 1.05 |
| 1.08 | UTF-8/UTF-16, Base64, Hex (FND-6) | S | 1.05 |
| 1.09 | ConfigMgr (FND-9) | M | 1.05 |
| 1.10 | Logging (FND-10) | M | 1.09 |
| 1.11 | Threading and Asio wrappers (FND-11) | M | 1.05 |
| 1.12 | Crypto basics: SHA-256/512, CRC32 both variants, CSPRNG (FND-7 + PAT-2 Crc32) | S | 1.06 |
| 1.13 | zlib Compression and KIWAD archive reader (new DAT-1; FND-8 zlib, PAT-2 KiwadHeader) | M | 1.12, 1.10 |
| 1.14 | Message definition model and ordinal rules (NET-2) | M | 1.06, 1.13 |
| 1.15 | msggen build-time generator (NET-3) | M | 1.14 |
| 1.16 | Message registry and round-trip suite (NET-4) | S | 1.15 |
| 1.17 | KI frame codec and reassembler (NET-5) | M | 1.06 |
| 1.18 | Control messages and capture verification (NET-6) | S | 1.17 |
| 1.19 | Async socket layer and SocketMgr (NET-7) | M | 1.17, 1.11, 1.10 |
| 1.20 | App skeletons (FND-12) | S | 1.10, 1.11 |
| 1.21 | Patch-free dev path documented (PAT-1) | S | 1.19 |
| 1.22 | Session handshake and keep-alive (NET-8) | M | 1.18, 1.19, 1.20, 1.16, 1.21 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Decision 2026-09-13.** Protocol definitions load at runtime (see Decisions in doc/ARCHITECTURE.md). 1.15 becomes the runtime `MessageRegistry` loader plus startup-validated message declarations instead of a build-time generator, and 1.16 tests it against project-authored fixtures, with real-install checks under the `client` CTest label. 1.04 CI therefore needs no client files. Dependencies come from vcpkg manifest mode rather than vendored copies, which replaces the `deps/fmt` and `deps/gtest` deliverables in 1.01 and 1.02.

- **Ordering.** 1.04 CI is built and made mandatory before the 'how CI builds without client files' decision is taken, and before 1.15 msggen makes the build client-dependent. Either 1.04 depends on that decision or CI is rebuilt at 1.15. **Resolved:** the 2026-09-13 decision loads protocol data at runtime, so CI builds and tests without client files and 1.15 needs no CI rebuild.
- **Missing work.** Automated headless test client/bot harness that replays scripted sessions. Almost every acceptance is 'Real client' and not repeatable in CI. 1.22 has a fake client, but nothing grows it into a regression harness.
- **Missing work.** Codestyle checker (1.03) does not check the mandatory one-line brief, the Markdown/SQL/Batch/YAML header forms, or the JSON exemption from ARCHITECTURE.md. **Resolved in 1.03:** the checker validates the brief and every header form in the Conventions table, exempts JSON, and rejects file types it has no rule for.
- **Correction.** 1.14's reason for the 253 GAME ids is incomplete. GameMessages.xml also has 254 tags / 253 ids because MSG_REMOVEOBJECT is duplicated, and the two copies have different descriptions. The totals 1448/1446 are still correct.
- **Correction.** 1.14 fixture 'untyped GlobalID' is incomplete. Three untyped fields exist: MSG_MINIGAMEREWARDS.GlobalID (no TYPE), MSG_PHYSICS_GRAB.Force (TPYE typo), and MSG_BATTLEGROUNDQUEUEUPDATE.Kicked (attribute 'TYP', WizardMessages2). Add a 'TYP' fixture.
- **Correction.** UNVERIFIED, not wrong: 1.21/phase 1 outcome rely on a '-P 0' client launch flag. No local source documents it (Imlight README.md:79 documents only '-L 127.0.0.1 12000'). Also unverified: TemplateManifest 137423 entries, 134076 BINd, 42 ItemSetBonusTemplate rows, 16 magic_school_template rows, CombatSigil8Actor's 8 sub-circles, and the traffic.log line citations.

## 1.01 Toolchain hello (FND-1)

**Goal:** Configure, build, genrev and run on Windows and Linux.

**Size:** S. **Depends on:** nothing

**Acceptance**

- [x] Windows and linux-gcc presets build with no warnings
- [x] gameserver prints 'Project Ambrose rev <shorthash> (<branch>) <date>' matching git rev-parse --short HEAD
- [x] Build without .git prints 'rev unknown'

### Detailed spec from FND-1: Toolchain hello: CMake + fmt + genrev + one app that prints its revision

Prove the whole toolchain works: configure, vendored dep, static lib, generated header, executable, on Windows and Linux.

**Deliverables**

- CMakeLists.txt (root): project(Ambrose CXX), C++20 required, options (BUILD_TESTING, TOOLS, APPS_BUILD), adds deps/ src/
- CMakePresets.json: windows-msvc-x64, linux-gcc, linux-clang with Debug/RelWithDebInfo (JSON, exempt from the header rule)
- src/cmake/: CompilerFlags.cmake (MSVC /W4 /permissive- /utf-8, GCC/Clang -Wall -Wextra), PlatformDetect.cmake, AmbroseMacros.cmake (ambrose_add_library helper that globs a folder and sets include dirs)
- deps/fmt/ (vendored release, with its own CMakeLists) plus deps/CMakeLists.txt
- src/genrev/CMakeLists.txt + GenRev.cmake: at build time runs `git rev-parse --short HEAD`, branch and commit date into build/src/genrev/GitRevision.h, with fallback values when git is absent (source tarball)
- src/common/GitRevision.h/.cpp: GitRevision::GetHash/GetBranch/GetDate/GetFullVersion()
- src/common/Banner.h/.cpp: Ambrose::Banner::Show(appName, logFn)
- src/server/apps/gameserver/Main.cpp: prints the banner and GetFullVersion(), exits 0

**Acceptance**

- [x] `cmake --preset windows-msvc-x64 && cmake --build` and the linux-gcc preset both finish with no warnings
- [x] gameserver prints 'Project Ambrose rev <shorthash> (<branch>) <date>' and the hash matches `git rev-parse --short HEAD`
- [x] Changing HEAD (new commit) and rebuilding regenerates GitRevision.h without a clean build
- [x] Building from an exported tree without .git prints 'rev unknown' and does not fail
- [x] Real client: nothing observable (no networking yet)

**Risks**

- Vendoring fmt vs using a package manager (vcpkg/Conan) is a pending decision; the vendored-folder shape follows AzerothCore
- Generated header must be regenerated per build without forcing a full rebuild (use configure_file with copy_if_different)

## 1.02 Unit test harness (FND-2)

**Goal:** One unit_tests exe run by CTest.

**Size:** S. **Depends on:** 1.01

**Acceptance**

- [x] ctest passes GitRevisionTest on both presets
- [x] A failing test makes ctest exit non-zero
- [x] -DBUILD_TESTING=OFF configures without gtest

### Detailed spec from FND-2: Unit test harness

Every later milestone can add GoogleTest tests that CTest runs from one executable.

**Deliverables**

- deps/gtest/ vendored (GoogleTest + GoogleMock)
- src/test/CMakeLists.txt: single `unit_tests` target, sources globbed from src/test/** mirroring src/, linked to common (later shared, database, game)
- src/test/main.cpp (gtest_main or own main that sets up test logging later)
- src/test/common/GitRevisionTest.cpp
- src/test/mocks/ folder with CMake wiring for gmock helpers
- ctest registration via gtest_discover_tests

**Acceptance**

- [x] `ctest --test-dir build --output-on-failure` runs and passes GitRevisionTest on both presets
- [x] A deliberately failing test makes ctest exit non-zero
- [x] -DBUILD_TESTING=OFF configures without gtest
- [x] Real client: n/a

**Risks**

- GoogleTest is a pending decision (Catch2 is the alternative); gtest_discover_tests needs the exe runnable at build time (cross builds)

## 1.03 Codestyle checker (FND-3)

**Goal:** Reject a missing branding header or any other comment.

**Size:** M. **Depends on:** 1.01

**Acceptance**

- [x] The current tree passes
- [x] `int x = 1; // note` fails; `auto s = "http://x";` and `R"(/* x */)"` pass
- [x] SQL `-- extra` fails; `SELECT '--';` passes
- [x] A .h without an AMBROSE_ guard fails

### Detailed spec from FND-3: Codestyle checker: branding header and no other comments

A tool rejects any file that lacks the exact Project Ambrose header for its type or has any other comment.

**Deliverables**

- apps/codestyle/codestyle.py (Python 3, header '# Project Ambrose by Imjustchico' + brief) or a C++ tool in src/tools; language choice to confirm
- Per-type rules taken from doc/ARCHITECTURE.md Conventions table: C/C++ (`/*`, ` * Project Ambrose by Imjustchico`, ` * <non-empty brief>`, ` */`), # family (CMake, sh, ps1, py, yml, conf/.conf.dist, .gitignore/.gitattributes/.editorconfig), SQL `--`, Batch `REM`, Markdown `<!-- Project Ambrose by Imjustchico: <brief> -->`; JSON exempt
- Comment lexers that respect string literals: C++ `//` and `/* */` outside "...", '...' and raw strings R"d(...)d"; SQL `--`/`/* */` outside quotes; CMake `#` and `#[[ ]]` outside quoted args; shell/Python `#` outside quotes, with shebang allowed on line 1 before the header; Batch `REM`/`::`; Markdown `<!-- -->`
- Excludes: deps/**, build*/**, .git/**, empty .gitkeep files, generated build/src/genrev
- Extra checks: LF endings except .bat/.ps1 (matches .editorconfig/.gitattributes), trailing whitespace, final newline, include guard `AMBROSE_<FILE>_H` in .h
- apps/codestyle/tests/ fixtures (good/bad samples) + test runner
- Exit code 0/1 with file:line messages; `--fix-header` is not provided (headers need a human-written brief)

**Acceptance**

- [x] Running on the current  tree passes (README.md, CONTRIBUTING.md, CLAUDE.md, doc/ARCHITECTURE.md, .gitignore, .editorconfig, .gitattributes all have valid headers today)
- [x] Fixture: `int x = 1; // note` fails at its line; `auto s = "http://x";` passes; `R"(/* x */)"` passes
- [x] Fixture: SQL file with `-- extra` after the header fails; `SELECT '--';` passes
- [x] Fixture: header brief empty or misspelled branding fails
- [x] Fixture: a .h without AMBROSE_ guard fails
- [x] Real client: n/a

**Risks**

- The no-comments rule conflicts with AzerothCore-style commented .conf.dist files, and with Doxygen or license text in vendored deps (deps/ must be excluded)
- A lexer that misreads C++ raw strings, digit separators (1'000) or character literals gives false positives
- Python in apps/ may clash with 'Server code is C++' in CLAUDE.md; tooling is probably fine but the maintainer should confirm

## 1.04 CI pipeline (FND-4)

**Goal:** Build, test, style and forbidden-file scan on every PR.

**Size:** M. **Depends on:** 1.02, 1.03

**Acceptance**

- [x] `// todo` in a .cpp fails codestyle (dispatch run 34771249175)
- [x] A file beginning 'KIWAD' fails ci-forbidden-files
- [x] A commit without an AI trailer fails
- [x] A clean PR is green on all 3 legs (verified on push run 34771622038, which runs the same jobs a pull request does; the pull request commit range is covered by ci.selftest)

### Detailed spec from FND-4: CI pipeline

Every push and PR builds, tests, style-checks and scans for forbidden content on Windows and Linux.

**Deliverables**

- .github/workflows/core-build.yml (GitHub requires this location; logic lives in apps/ci scripts)
- apps/ci/ci-build.sh and ci-build.ps1: configure with preset, build, ctest
- apps/ci/ci-codestyle.sh: runs apps/codestyle
- apps/ci/ci-forbidden-files.py: fails on *.wad, *.pcap(ng), files starting with 'KIWAD' or 'BINd', any committed copy of a client protocol XML (root element <...Messages> with <_ProtocolInfo>), the wiztype dump JSON shape ({version, classes}), .conf files (not .dist), and any file over a size limit outside deps/
- apps/ci/ci-commit-trailer.py: every commit in the PR has a Co-Authored-By AI trailer (CONTRIBUTING.md requirement)
- Build matrix: windows-latest MSVC, ubuntu-latest GCC and Clang; ccache/sccache caching

**Acceptance**

- [x] A PR that adds `// todo` to a .cpp fails the codestyle job
- [x] A PR that commits a file beginning with bytes 'KIWAD' fails ci-forbidden-files
- [x] A commit without an AI trailer fails ci-commit-trailer
- [x] A clean PR is green on all three matrix legs with unit_tests executed (push run 34771622038: windows-msvc-x64 on Visual Studio 18 2026, linux-gcc, and linux-clang each ran 8/8 tests)
- [x] Real client: n/a

**Risks**

- Private repo: GitHub Actions minutes are limited, especially Windows runners
- Pre-existing local pre-push hook (CLAUDE.local.md) must not conflict with CI assumptions

## 1.05 common/Utilities (FND-5)

**Goal:** Time, random, strings, TokenBucket.

**Size:** S. **Depends on:** 1.02

**Acceptance**

- [x] StringTo<uint32>("4294967296") is nullopt
- [x] GetMSTimeDiff handles uint32 wrap
- [x] TokenBucket refills on a fake clock

### Detailed spec from FND-5: common/Utilities: time, random, strings, safe parsing

Shared helpers every subsystem needs exist and are tested.

**Deliverables**

- src/common/Utilities/Timer.h: GameTime-style steady clock helpers (GetMSTime, GetMSTimeDiff with wraparound), IntervalTimer, TimeTracker
- src/common/Utilities/Duration.h: Milliseconds/Seconds/Minutes aliases
- src/common/Utilities/Random.h/.cpp: thread-local engine, urand/irand/frand/roll_chance, RandomEngine satisfying UniformRandomBitGenerator
- src/common/Utilities/StringUtil.h/.cpp: Tokenize (string_view), Trim, ASCII ToLower/ToUpper, case-insensitive compare, StringTo<T> via from_chars returning std::optional, fmt-based StringFormat
- src/common/Utilities/EnumFlag.h, Optional/Types.h (uint8..uint64, int8..int64 aliases)
- src/common/Utilities/TokenBucket.h: rate limiter (the reference server has one in Shared/Networking/TokenBucket.cs; behavior only)
- src/test/common/Utilities/*Test.cpp

**Acceptance**

- [x] StringTo<uint32>("4294967296") is nullopt; StringTo<int32>("-5") == -5
- [x] GetMSTimeDiff handles a uint32 wrap
- [x] Tokenize("a  b", ' ', keepEmpty=false) gives {a,b}
- [x] urand(1,1)==1; 10^6 draws of urand(0,9) stay within range
- [x] TokenBucket allows N tokens and refills on a fake clock
- [x] Real client: n/a

## 1.06 ByteBuffer and DML primitives (FND-6 + NET-1)

**Goal:** Bounds-checked LE read/write for BYT UBYT USHRT INT UINT FLT GID STR WSTR.

**Size:** S. **Depends on:** 1.05

**Acceptance**

- [x] Round-trip per type incl. NaN FLT and GID 0xFFFFFFFFFFFFFFFF
- [x] STR length past the end throws and allocates nothing
- [x] WSTR 'Ab' encodes as 02 00 41 00 62 00
- [x] Random-truncation fuzz is clean under ASan

### Detailed spec from FND-6: common/Encoding: byte buffer, bit stream, text encodings

Bounds-checked little-endian primitives cover every DML field type in the client XML and every bit-width type in the ObjectProperty dump.

**Deliverables**

- src/common/Encoding/ByteBuffer.h/.cpp: growable LE buffer, typed Read/Write for uint8/int8/uint16/int16/uint32/int32/uint64/int64/float/double, read position, throws ByteBufferException on overrun (never an unchecked allocation from a length prefix)
- DML helpers (possibly promoted to shared/Messages by NET): STR = uint16 length + bytes, WSTR = uint16 code-unit count + UTF-16LE, GID = uint64. The 9 TYPE values seen across all 26 Root.wad *Messages.xml files: STR, GID, UINT, INT, UBYT, FLT, BYT, WSTR, USHRT (no DBL)
- src/common/Encoding/BitStream.h/.cpp: BitReader/BitWriter, LSB-first, arbitrary widths 1-64, byte realign, needed for type-dump primitives bui2, bui4, bui5, bui7, s24, u24 and bool
- src/common/Encoding/Utf.h/.cpp: UTF-8 <-> UTF-16LE with invalid-sequence handling (Locale .lang files and WSTR/std::wstring are UTF-16)
- src/common/Encoding/Base64.h, Hex.h
- src/test/common/Encoding/*Test.cpp

**Acceptance**

- [x] Round-trip each DML type; reading a STR whose length prefix passes the buffer end throws and allocates nothing
- [x] BitWriter write(0b101,3), write(0x7F,7), realign produces the expected bytes; BitReader reads the same values; s24 sign-extends -1 (delivered in 1.07)
- [ ] UTF-8 'Wizardé\U0001F600' -> UTF-16LE -> UTF-8 is identical; an unpaired surrogate is replaced or rejected (documented) (tracked under 1.08)
- [ ] Base64 matches RFC 4648 vectors (tracked under 1.08)
- [ ] Optional integration test (skipped unless AMBROSE_CLIENT_DIR is set): decode the UTF-16 text of one Locale/*.lang entry from the user's Root.wad without error (tracked under 1.08)
- [x] Real client: n/a

**Risks**

- Bit order (LSB-first) for ObjectProperty is inferred from the reference codec, not yet checked against a capture; OBJ must confirm it
- Whether WSTR's length is a code-unit or byte count must be checked against a real capture by NET

### Detailed spec from NET-1: DML primitive codec and ByteBuffer

Every one of the 9 wire field types used by the client XML can be read and written with bounds checks.

**Deliverables**

- src/common/Utilities/ByteBuffer.h/.cpp: little-endian growable buffer with read cursor, and a ByteBufferException on underflow
- src/server/shared/Messages/DmlTypes.h: DmlType enum {BYT,UBYT,USHRT,INT,UINT,FLT,GID,STR,WSTR} with wire sizes. BYT=i8, UBYT=u8, USHRT=u16, INT=i32, UINT=u32, FLT=IEEE754 f32, GID=u64. STR=u16 byte count + raw bytes (std::string, may hold binary ObjectProperty blobs). WSTR=u16 UTF-16 code-unit count + 2*count bytes UTF-16LE (std::u16string)
- Also accepted as aliases, not used in r806919 XML: SHRT, DBL, BOOL, UBYTE, USHORT, all reported as warnings
- src/test/common/Utilities/ByteBufferTest.cpp, src/test/server/shared/Messages/DmlTypesTest.cpp

**Data sources**

- None (pure code). Encodings verified against Imcodec.IO BitWriter.WriteString/WriteWString and BitReader.ReadString/ReadWString (non-compact path)

**Acceptance**

- [x] Round-trip test per type, including min/max values, NaN for FLT, and 0xFFFFFFFFFFFFFFFF for GID
- [x] STR of 0 bytes encodes as 00 00; STR of 65535 bytes round-trips; a length prefix larger than the remaining bytes throws and does not allocate
- [x] WSTR 'Ab' encodes as 02 00 41 00 62 00 (the count is code units, not bytes)
- [x] Reading past the end throws; the fuzz-style test (random truncation of valid buffers) never crashes under ASan

**Risks**

- STR carries arbitrary bytes. Never transcode it as UTF-8, or ObjectProperty blobs get corrupted

## 1.07 BitReader/BitWriter (OBJ-1 + FND-6)

**Goal:** LSB-first KI bit streams with back-patching.

**Size:** S. **Depends on:** 1.05

**Acceptance**

- [x] bit 1, u32 0xAABBCCDD, 3 bits 0b101 gives 01 DD CC BB AA 05
- [x] s24 -2 and bui5 31 round-trip
- [x] Read past end sets failed flag, no crash

### Detailed spec from OBJ-1: Bit stream reader and writer

Any code can read and write KI bit-packed streams exactly as the client does, safely on truncated or hostile input.

**Deliverables**

- src/common/Serialization/BitReader.h/.cpp: LSB-first bit order within each byte; byte-aligned little-endian u8/i8/u16/i16/u32/i32/u64/i64/f32/f64 (these realign to the next byte); ReadBits(n) for 1..32 bits (bool = 1 bit, bui2/bui4/bui5/bui7, s24/u24 with sign extension); raw byte spans; BitPos/SeekBit; every read bounds-checked and returning a failure state instead of throwing or causing UB
- src/common/Serialization/BitWriter.h/.cpp: the matching writes, SeekBit back-patching (needed for the versionable size fields), growable buffer
- src/test/common/Serialization/BitStreamTest.cpp

**Acceptance**

- [x] Unit test: writing bit 1, then u32 0xAABBCCDD, then 3 bits 0b101 produces the hand-derived bytes 01 DD CC BB AA 05, and reads back identically
- [x] Unit test: s24 value -2 round-trips; bui5 31 round-trips; mixed bool/u16 sequences realign correctly
- [x] Unit test: reading past the end sets a failed flag, returns zeros and never crashes (run under ASan/UBSan in CI)
- [x] Unit test: back-patching a u32 at an earlier bit position rewrites it exactly

**Risks**

- Bit order is easy to get backwards. It was verified against Root.wad BINd files and captured blobs, so both directions must be pinned by golden tests

### Detailed spec from FND-6: common/Encoding: byte buffer, bit stream, text encodings

Bounds-checked little-endian primitives cover every DML field type in the client XML and every bit-width type in the ObjectProperty dump.

**Deliverables**

- src/common/Encoding/ByteBuffer.h/.cpp: growable LE buffer, typed Read/Write for uint8/int8/uint16/int16/uint32/int32/uint64/int64/float/double, read position, throws ByteBufferException on overrun (never an unchecked allocation from a length prefix)
- DML helpers (possibly promoted to shared/Messages by NET): STR = uint16 length + bytes, WSTR = uint16 code-unit count + UTF-16LE, GID = uint64. The 9 TYPE values seen across all 26 Root.wad *Messages.xml files: STR, GID, UINT, INT, UBYT, FLT, BYT, WSTR, USHRT (no DBL)
- src/common/Encoding/BitStream.h/.cpp: BitReader/BitWriter, LSB-first, arbitrary widths 1-64, byte realign, needed for type-dump primitives bui2, bui4, bui5, bui7, s24, u24 and bool
- src/common/Encoding/Utf.h/.cpp: UTF-8 <-> UTF-16LE with invalid-sequence handling (Locale .lang files and WSTR/std::wstring are UTF-16)
- src/common/Encoding/Base64.h, Hex.h
- src/test/common/Encoding/*Test.cpp

**Acceptance**

- [x] Round-trip each DML type; reading a STR whose length prefix passes the buffer end throws and allocates nothing (delivered in 1.06)
- [x] BitWriter write(0b101,3), write(0x7F,7), realign produces the expected bytes; BitReader reads the same values; s24 sign-extends -1
- [ ] UTF-8 'Wizardé\U0001F600' -> UTF-16LE -> UTF-8 is identical; an unpaired surrogate is replaced or rejected (documented) (tracked under 1.08)
- [ ] Base64 matches RFC 4648 vectors (tracked under 1.08)
- [ ] Optional integration test (skipped unless AMBROSE_CLIENT_DIR is set): decode the UTF-16 text of one Locale/*.lang entry from the user's Root.wad without error (tracked under 1.08)
- [x] Real client: n/a

**Risks**

- Bit order (LSB-first) for ObjectProperty is inferred from the reference codec, not yet checked against a capture; OBJ must confirm it
- Whether WSTR's length is a code-unit or byte count must be checked against a real capture by NET

## 1.08 UTF-8/UTF-16, Base64, Hex (FND-6)

**Goal:** Text encodings for .lang and WSTR.

**Size:** S. **Depends on:** 1.05

**Acceptance**

- [ ] UTF-8 'Wizardé\U0001F600' round-trips via UTF-16LE
- [ ] Base64 matches RFC 4648 vectors

### Detailed spec from FND-6: common/Encoding: byte buffer, bit stream, text encodings

Bounds-checked little-endian primitives cover every DML field type in the client XML and every bit-width type in the ObjectProperty dump.

**Deliverables**

- src/common/Encoding/ByteBuffer.h/.cpp: growable LE buffer, typed Read/Write for uint8/int8/uint16/int16/uint32/int32/uint64/int64/float/double, read position, throws ByteBufferException on overrun (never an unchecked allocation from a length prefix)
- DML helpers (possibly promoted to shared/Messages by NET): STR = uint16 length + bytes, WSTR = uint16 code-unit count + UTF-16LE, GID = uint64. The 9 TYPE values seen across all 26 Root.wad *Messages.xml files: STR, GID, UINT, INT, UBYT, FLT, BYT, WSTR, USHRT (no DBL)
- src/common/Encoding/BitStream.h/.cpp: BitReader/BitWriter, LSB-first, arbitrary widths 1-64, byte realign, needed for type-dump primitives bui2, bui4, bui5, bui7, s24, u24 and bool
- src/common/Encoding/Utf.h/.cpp: UTF-8 <-> UTF-16LE with invalid-sequence handling (Locale .lang files and WSTR/std::wstring are UTF-16)
- src/common/Encoding/Base64.h, Hex.h
- src/test/common/Encoding/*Test.cpp

**Acceptance**

- [ ] Round-trip each DML type; reading a STR whose length prefix passes the buffer end throws and allocates nothing
- [ ] BitWriter write(0b101,3), write(0x7F,7), realign produces the expected bytes; BitReader reads the same values; s24 sign-extends -1
- [ ] UTF-8 'Wizardé\U0001F600' -> UTF-16LE -> UTF-8 is identical; an unpaired surrogate is replaced or rejected (documented)
- [ ] Base64 matches RFC 4648 vectors
- [ ] Optional integration test (skipped unless AMBROSE_CLIENT_DIR is set): decode the UTF-16 text of one Locale/*.lang entry from the user's Root.wad without error
- [ ] Real client: n/a

**Risks**

- Bit order (LSB-first) for ObjectProperty is inferred from the reference codec, not yet checked against a capture; OBJ must confirm it
- Whether WSTR's length is a code-unit or byte count must be checked against a real capture by NET

## 1.09 ConfigMgr (FND-9)

**Goal:** Typed options from .conf.dist/.conf/env.

**Size:** M. **Depends on:** 1.05

**Acceptance**

- [ ] AMBROSE_WORLD_SERVER_PORT env override works
- [ ] `Foo == bar` fails with its line number
- [ ] Reload() picks up changes

### Detailed spec from FND-9: common/Configuration: ConfigMgr and .conf.dist convention

Apps read typed options from <app>.conf, falling back to .conf.dist defaults, with environment overrides.

**Deliverables**

- src/common/Configuration/Config.h/.cpp: sConfigMgr singleton, LoadInitial(file, args), Reload(), GetOption<T>(name, default, quiet) for bool/int types/float/string, GetKeysByString(prefix) for Appender.* / Logger.*
- Parser: `Key = value`, quoted strings, `#` lines (header only, per codestyle), duplicate-key error, unknown-line error with line number
- Override order: .conf.dist < .conf < conf.d/*.conf < env vars AMBROSE_<KEY_UPPER_UNDERSCORED>
- Missing key logs one warning (after logging exists; buffered until then)
- CMake: install/copy each app's <app>.conf.dist next to the binary (etc/ in install)
- conf/dist/: CMake config template (config.cmake.dist) and env template (env.dist) per ARCHITECTURE 'Build and environment config templates'
- src/test/common/Configuration/ConfigTest.cpp

**Acceptance**

- [ ] GetOption<uint32>("WorldServerPort", 12000) returns the file value, default when missing, and env value when AMBROSE_WORLD_SERVER_PORT is set
- [ ] Malformed line `Foo == bar` fails load with line number
- [ ] Reload() picks up a changed file
- [ ] `bool` accepts 1/0/true/false case-insensitively
- [ ] Real client: n/a

**Risks**

- No-comments rule: .conf.dist cannot document options inline like AzerothCore's does; option documentation needs another home (doc/ or a generated table)
- Env-var name mangling rules must be decided once and documented

## 1.10 Logging (FND-10)

**Goal:** Named loggers and appenders from config.

**Size:** M. **Depends on:** 1.09

**Acceptance**

- [ ] Logger.sql.sql at Warn drops LOG_INFO while root Info prints
- [ ] A format mismatch is a compile error
- [ ] File appender writes Server.log

### Detailed spec from FND-10: common/Logging: loggers, appenders, levels from config

All code logs through named loggers configured by Appender.* and Logger.* options.

**Deliverables**

- src/common/Logging/Log.h/.cpp: sLog, LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(filter, fmtstr, args...) using fmt compile-time checked formats
- Logger hierarchy by dotted name ('server.loginserver', 'network.opcode', 'sql.sql', 'sql.updates') inheriting from the nearest parent
- Appender.h, AppenderConsole (colored per level, Windows console too), AppenderFile (per-name file, optional timestamped filename, flush policy)
- Config format: `Appender.Console = 1,3,0` (type,level,flags,...) and `Logger.root = 3,Console Server` in each .conf.dist
- Optional async mode on an owned thread (Threading ProducerConsumerQueue arrives in FND-11; add async there or keep sync here)
- Test appender capturing messages for unit tests (src/test/mocks)

**Acceptance**

- [ ] Logger.sql.sql at level Warn drops LOG_INFO("sql.sql", ...) while Logger.root at Info still prints server.* messages
- [ ] A format string/argument mismatch is a compile error
- [ ] File appender writes Server.log in LogsDir with the expected line prefix (time, level, logger)
- [ ] Real client: n/a

**Risks**

- Mixing sync and async logging at shutdown can lose the last lines; flush on exit must be tested

## 1.11 Threading and Asio wrappers (FND-11)

**Goal:** Queues, pools, timers, resolver.

**Size:** M. **Depends on:** 1.05

**Acceptance**

- [ ] 4x100k producer/consumer exactly-once; Cancel wakes consumers
- [ ] DeadlineTimer fires; cancel prevents it
- [ ] TSan clean

### Detailed spec from FND-11: common/Threading and common/Asio wrappers

Thread-safe queues, worker threads and thin Asio wrappers that network and database code build on.

**Deliverables**

- Decision point: Boost.Asio (AzerothCore precedent) vs standalone Asio; Boost is not vendored (too large), found via find_package, which affects install docs and CI
- src/common/Threading/ProducerConsumerQueue.h: blocking Pop with Cancel, WaitAndPop
- src/common/Threading/ThreadPool.h (wraps asio::thread_pool), ThreadName helper, LockedQueue.h, MPSCQueue.h
- src/common/Asio/IoContext.h, Strand.h, DeadlineTimer.h, Resolver.h (IPv4/IPv6 resolve to endpoint), IpAddress.h (parse, is-loopback), SignalSet helper
- src/test/common/Threading/*Test.cpp, src/test/common/Asio/*Test.cpp

**Acceptance**

- [ ] ProducerConsumerQueue: 4 producers × 100k items, 4 consumers, all items consumed exactly once; Cancel wakes blocked consumers
- [ ] DeadlineTimer fires on an io_context within tolerance; cancel prevents the handler
- [ ] Resolver resolves 'localhost' to loopback
- [ ] ThreadSanitizer (linux-clang preset with -fsanitize=thread) run of these tests is clean
- [ ] Real client: n/a

**Risks**

- Boost vs standalone Asio changes include paths and error_code types in every networking file; decide before NET starts

## 1.12 Crypto basics: SHA-256/512, CRC32 both variants, CSPRNG (FND-7 + PAT-2 Crc32)

**Goal:** Hash and checksum primitives with known-answer tests.

**Size:** S. **Depends on:** 1.06

**Acceptance**

- [ ] SHA-256('abc')=ba7816bf...; SHA-512('abc')=ddaf35a1...
- [ ] CRC32('123456789') init 0xFFFFFFFF + final xor = 0xCBF43926
- [ ] KI variant (init 0, no xor) = 0x2DFD2D88 (verified)
- [ ] Incremental equals one-shot

### Detailed spec from FND-7: common/Cryptography part 1: hashes, CRC32, CSPRNG

The hash and checksum primitives login and patch flows need are available with known-answer tests.

**Deliverables**

- deps decision: OpenSSL (system, as AzerothCore) or vendored minimal implementations
- src/common/Cryptography/SHA256.h/.cpp, SHA512.h/.cpp: incremental Update/Finalize
- src/common/Cryptography/CRC32.h/.cpp: reflected polynomial 0xEDB88320 with caller-supplied initial value (the reference uses both Calculate(0,...) for patch file lists and ~crc for WAD segments)
- src/common/Cryptography/CryptoRandom.h: GetRandomBytes via OS CSPRNG
- src/common/Cryptography/ConstantTime.h: constant-time equal
- src/test/common/Cryptography/*Test.cpp

**Acceptance**

- [ ] SHA-256('abc') = ba7816bf...; SHA-512('abc') = ddaf35a1... (FIPS 180-4 vectors)
- [ ] CRC32('123456789') with init 0xFFFFFFFF and final xor = 0xCBF43926
- [ ] Optional integration test with AMBROSE_CLIENT_DIR: a KIWAD entry's stored crc field matches CRC32 of its stored bytes (tells us which init/xor variant KIWAD uses)
- [ ] Real client: n/a

**Risks**

- OpenSSL on Windows adds a DLL dependency and a vcpkg/installer step
- Which CRC variant KIWAD entries use is not verified (the reference uses different init values in different places)

### Detailed spec from PAT-2: KI CRC-32 and KIWAD header measurement primitives

The server can compute the exact CRC, HeaderSize and HeaderCRC values the client checks files against.

**Deliverables**

- src/common/Utilities/Crc32.h/.cpp: table-driven reflected CRC-32, poly 0xEDB88320, init 0, no final XOR, streaming update API
- src/server/shared/Archive/KiwadHeader.h/.cpp: reads 'KIWAD' magic, version, count, optional flag byte (version>=2), 21+nameLen per entry, returns TOC byte length (reuses or feeds the shared KIWAD reader from DAT/WLD if that exists first)
- src/test/common/Utilities/Crc32Test.cpp, src/test/server/shared/Archive/KiwadHeaderTest.cpp

**Data sources**

- Verified against Aurorium data/V_r806919.Wizard_1_610/LatestFileList.xml: CRC matches for sampled files, HeaderSize == KIWAD TOC length for all 3589 type 3/5 WADs, HeaderCRC == CRC(first HeaderSize bytes)

**Acceptance**

- [ ] Unit: Crc32("123456789") == 0x2DFD2D88 (this variant; zlib's standard value 0xCBF43926 must NOT be produced)
- [ ] Unit: incremental update over split buffers equals one-shot result
- [ ] Unit: synthetic in-memory KIWAD v2 with 3 entries yields TOC length 14 + sum(21+nameLen)
- [ ] Env-gated test (AMBROSE_CLIENT_DIR set, skipped otherwise): for every Data/GameData/*.wad in the user's install, KiwadHeader length is <= file size and parsing never over-reads

**Risks**

- KIWAD reader may be duplicated with the DAT/WLD domain; agree on one shared/Archive owner

## 1.13 zlib Compression and KIWAD archive reader (new DAT-1; FND-8 zlib, PAT-2 KiwadHeader)

**Goal:** Read the user's WAD entries and inflate them with a size cap.

**Size:** M. **Depends on:** 1.12, 1.10

**Acceptance**

- [ ] Inflate past cap fails cleanly
- [ ] Synthetic KIWAD v2 with 3 entries gives TOC length 14 + sum(21+nameLen)
- [ ] Client-gated: Root.wad lists 173088 entries (verified); LoginMessages.xml inflates; a flags-15 BINd inflates at offset 13
- [ ] Client-gated: header parse never over-reads on any GameData/*.wad

### Detailed spec from FND-8: common/Cryptography part 2: Twofish-OFB; common/Utilities: zlib

The cipher used for Rec1 in MSG_USER_AUTHEN_RSP / MSG_USER_VALIDATE exists, and zlib-compressed client data can be inflated.

**Deliverables**

- src/common/Cryptography/Twofish.h/.cpp: written from the published Twofish spec (OpenSSL does not ship Twofish), 128-bit key block encrypt, plus an OFB mode wrapper with no padding
- deps/zlib/ vendored; src/common/Utilities/Compression.h/.cpp: Inflate(expectedSize) with a hard output cap, Deflate
- src/test/common/Cryptography/TwofishTest.cpp, src/test/common/Utilities/CompressionTest.cpp

**Acceptance**

- [ ] Twofish-128 matches the spec's published known-answer vectors (zero key and plaintext, and the iterated table)
- [ ] OFB encrypt then decrypt is identity for 0, 1, 15, 16, 17 and 1000 bytes
- [ ] Inflate of a stream that decompresses past its cap fails cleanly (zip bomb guard)
- [ ] Optional integration with AMBROSE_CLIENT_DIR: inflate a compressed Root.wad entry (e.g. LoginMessages.xml) and, for a 'BINd' entry, the zlib payload at offset 13
- [ ] Real client: n/a (NET checks Rec1 against a live login)

**Risks**

- Twofish key and nonce derivation for Rec1 is NET's job; FND only guarantees the cipher is correct
- LoginMessages.xml fields PassKey3 and Rec1 exist (checked), but the exact hashing the client does is only known from the reference server and is unverified against a capture

### Detailed spec from PAT-2: KI CRC-32 and KIWAD header measurement primitives

The server can compute the exact CRC, HeaderSize and HeaderCRC values the client checks files against.

**Deliverables**

- src/common/Utilities/Crc32.h/.cpp: table-driven reflected CRC-32, poly 0xEDB88320, init 0, no final XOR, streaming update API
- src/server/shared/Archive/KiwadHeader.h/.cpp: reads 'KIWAD' magic, version, count, optional flag byte (version>=2), 21+nameLen per entry, returns TOC byte length (reuses or feeds the shared KIWAD reader from DAT/WLD if that exists first)
- src/test/common/Utilities/Crc32Test.cpp, src/test/server/shared/Archive/KiwadHeaderTest.cpp

**Data sources**

- Verified against Aurorium data/V_r806919.Wizard_1_610/LatestFileList.xml: CRC matches for sampled files, HeaderSize == KIWAD TOC length for all 3589 type 3/5 WADs, HeaderCRC == CRC(first HeaderSize bytes)

**Acceptance**

- [ ] Unit: Crc32("123456789") == 0x2DFD2D88 (this variant; zlib's standard value 0xCBF43926 must NOT be produced)
- [ ] Unit: incremental update over split buffers equals one-shot result
- [ ] Unit: synthetic in-memory KIWAD v2 with 3 entries yields TOC length 14 + sum(21+nameLen)
- [ ] Env-gated test (AMBROSE_CLIENT_DIR set, skipped otherwise): for every Data/GameData/*.wad in the user's install, KiwadHeader length is <= file size and parsing never over-reads

**Risks**

- KIWAD reader may be duplicated with the DAT/WLD domain; agree on one shared/Archive owner

## 1.14 Message definition model and ordinal rules (NET-2)

**Goal:** Client XML text becomes validated protocols with wire ids.

**Size:** M. **Depends on:** 1.06, 1.13

**Client messages:** MSG_PING, MSG_USER_AUTHEN_V3, MSG_ATTACH, MSG_BADGES, MSG_REMOVEOBJECT, MSG_PETHATCHREADYSTATUS, MSG_MINIGAMEREWARDS, MSG_PHYSICS_GRAB, MSG_CLIENTZONED

**Acceptance**

- [ ] Fixtures: explicit order, duplicate tag, lowercase tags (MSG_DailyQuestUpdate), TPYE typo, untyped GlobalID
- [ ] Client-gated: 29 protocols, 1448 records, 1446 ids (corrected from 26/971/969; includes GAME2 55=10, WIZARD2 53=254, WIZARD3 56=213)
- [ ] Spot checks: SYSTEM MSG_PING=1; LOGIN MSG_USER_AUTHEN_V3=27; GAME MSG_ATTACH=7, MSG_CLIENTMOVE=36, MSG_LOGINCOMPLETE=108, MSG_NEWOBJECT=122, MSG_SERVER_ERROR=223; WIZARD MSG_UPDATEMANA=233; WIZARD2 MSG_CLIENTZONED=64
- [ ] Exactly 9 field types; sorting by _MsgName instead of tag fails the GAME test

### Detailed spec from NET-2: Message definition model and ordinal rules

A library turns the client's message XML text into a validated list of protocols, messages and typed fields with correct wire ids.

**Deliverables**

- src/server/shared/Messages/MessageDefinition.h/.cpp: ProtocolDef {serviceId, protocolType, version, description, sourceFile}, MessageDef {tag, msgName, handlerName, description, accessLevel, order, fields}, FieldDef {name, DmlType}
- src/server/shared/Messages/MessageDefinitionParser.h/.cpp. It takes XML text. It keys protocols by ServiceID, never by ProtocolType: WizCombatMessages (51) says DOODLEDOUG_MESSAGES, and CatchAKey (54) and ShockALock (44) both say MG3_MESSAGES. It ignores the root element name. The element tag is the identity. It skips '_'-prefixed metadata children. When the first record has _MsgOrder or _MsgType, ids are explicit. Otherwise it sorts by tag using byte-ordinal comparison, merges duplicate tags (GameMessages has MSG_REMOVEOBJECT twice, WizardMessages has MSG_PETHATCHREADYSTATUS twice) and numbers 1..N. It accepts TPYE as TYPE and treats a missing TYPE on a field named GlobalID as GID, each with a warning
- Validation errors: duplicate explicit order, id > 255, unknown type, duplicate ServiceID across files
- XML parser choice: a small in-house reader or a vendored lib in deps/ (pending decision)
- src/test/server/shared/Messages/MessageDefinitionParserTest.cpp with hand-written fixture XML that is Ambrose-authored, not client-extracted
- Client-backed test, skipped unless AMBROSE_CLIENT_DATA_DIR is set, that loads the real Root.wad through the archive reader

**Client messages:** All 971 records in the 26 XML files. Explicitly exercised: MSG_PING, MSG_PING_RSP, MSG_CUSTOMDICT, MSG_RAW_TEXT, MSG_SERVERMESSAGE, MSG_FORCE_DISCONNECT, MSG_USER_AUTHEN_V3, MSG_CHARACTERSELECTED, MSG_LATEST_FILE_LIST_V2, MSG_ATTACH, MSG_BADGES, MSG_REMOVEOBJECT, MSG_SERVER_ERROR, MSG_PETHATCHREADYSTATUS, MSG_MINIGAMEREWARDS, MSG_PHYSICS_GRAB

**Data sources**

- Root.wad entries: AISClientMessages.xml, BaseMessages.xml, ExtendedBaseMessages.xml, GameMessages.xml, LoginMessages.xml, PatchMessages.xml, PetMessages.xml, ScriptDebuggerMessages.xml, TestManagerMessages.xml, WizardMessages.xml, Messages/{Cantrips,CatchAKey,ChooChooZoo,Concentration,DoodleDoug,Dueling_Diego,HotShots,Housing,MoveBehavior,PhysicsBehavior,PotionMotion,Quest,ShockALock,SkullRiders,Soblocks,WizCombat}Messages.xml
- Some entries may be BINd containers (zlib at offset 13); the archive reader must handle both
- Reference for cross-checking only: a local packet capture (private, never committed)

**Acceptance**

- [ ] Fixture tests cover: explicit-order file, sorted file with a duplicate tag, lowercase tags (Housing has MSG_DailyQuestUpdate and MSG_DailyPvPUpdate, which sort after all uppercase MSG_D...), the TPYE typo, and a GlobalID field with no TYPE
- [ ] Client-backed test: 26 protocols, 971 records, 969 ids; per service GAME(5)=253, WIZARD(12)=253, WIZARDHOUSING(50)=212, PET(9)=55, WizCombat(51)=36, LOGIN(7)=29, Soblocks(25)=23, ScriptDebugger(10)=20, QUEST(52)=19, Cantrips(57)=18, EXTENDEDBASE(2)=6, Physics(16)=6, MoveBehavior(15)=4, PATCH(8)=3, SYSTEM(1)=2, TestManager(11)=2, AIS(19)=1, minigames 40-47 and 54 = 3 each
- [ ] Client-backed ordinal spot checks: SYSTEM MSG_PING=1, MSG_PING_RSP=2; EXTENDEDBASE MSG_CUSTOMDICT=1, MSG_CUSTOMRECORD=2, MSG_FORCE_DISCONNECT=3, MSG_RAWRECORD=4, MSG_RAW_TEXT=5, MSG_SERVERMESSAGE=6; LOGIN MSG_CHARACTERSELECTED=3, MSG_SELECTCHARACTER=10, MSG_USER_AUTHEN_V3=27; PATCH MSG_LATEST_FILE_LIST_V2=2; GAME MSG_ATTACH=7, MSG_ATTACHFAILED=8, MSG_BADGES=10, MSG_CLIENTMOVE=36, MSG_LOGINCOMPLETE=108, MSG_NEWOBJECT=122, MSG_REMOVEOBJECT=182, MSG_SERVER_ERROR=223 (tag; its _MsgName is MSG_SERVERERROR); WIZARD MSG_MINIGAMEREWARDS=92, MSG_PETHATCHREADYSTATUS=122; Physics MSG_PHYSICS_GRAB=3
- [ ] Client-backed type census equals GID 782, STR 652, UINT 453, INT 394, UBYT 286, FLT 216, BYT 144, WSTR 33, USHRT 27 (2988 fields), with exactly 2 warnings (TPYE, untyped GlobalID)
- [ ] Sorting by _MsgName instead of tag makes the GAME test fail (191 positions differ), so the test guards the rule

**Risks**

- Needs a KIWAD reader from another domain for the client-backed test
- Whether the real client reads the TPYE field and the untyped GlobalID field, or drops them, is unknown and changes the wire layout of MSG_PHYSICS_GRAB and MSG_MINIGAMEREWARDS

## 1.15 msggen build-time generator (NET-3)

**Goal:** Compiled message structs from the user's install, nothing committed.

**Size:** M. **Depends on:** 1.14

**Client messages:** all 1446 ids

**Acceptance**

- [ ] Fixture XML output matches a golden file
- [ ] Full build compiles all structs warning-free
- [ ] git status clean after build; rebuild without changes does not rerun msggen

### Detailed spec from NET-3: msggen build-time code generator

At build time the user's client XML becomes compiled C++ message structs with Encode/Decode, without committing anything derived from the client.

**Deliverables**

- src/tools/msggen/: a host executable (depends only on shared+common). Usage: msggen --client-data <GameData dir> --out <dir>
- Emits one header+source per protocol, e.g. <build>/gen/Messages/GameMessages.h, with namespace Ambrose::Msg::Game and struct MSG_ATTACH { static constexpr uint8 ServiceId=5, Order=7, AccessLevel=1; static constexpr char const* Name; fields...; void Encode(ByteBuffer&) const; bool Decode(ByteBuffer&); }
- Every generated file starts with the Project Ambrose branding header
- Emits a manifest (a hash of the input XML) so rebuilds only happen when the client changes
- src/cmake/macros/GenerateMessages.cmake: add_custom_command with an AMBROSE_CLIENT_DATA_DIR cache var; output under ${CMAKE_BINARY_DIR}/gen (git-ignored); target ambrose-messages linked by shared
- A clear configure-time error when AMBROSE_CLIENT_DATA_DIR is missing
- src/test/tools/msggen/EmitterTest.cpp using fixture XML

**Client messages:** All 969 message ids

**Data sources**

- User's client install: <GameData>/Root.wad message XML (read at build time)

**Acceptance**

- [ ] The fixture XML emits code that matches a golden output kept in the test (Ambrose-authored fixture, not client data)
- [ ] A full build with AMBROSE_CLIENT_DATA_DIR pointing at a 1.610 GameData dir compiles all 969 structs with no warnings under /W4 or -Wall
- [ ] git status after a build shows no new tracked or untracked files outside build/
- [ ] Touching nothing and rebuilding does not rerun msggen; changing the client dir reruns it

**Risks**

- The build becomes impossible without a client install. CI needs either a runner with a client or a stub mode (see open questions)
- Name collisions: field names like 'Type' or 'Message' and C++ keywords need sanitizing

## 1.16 Message registry and round-trip suite (NET-4)

**Goal:** (service, order) resolves to a name, layout and factory.

**Size:** S. **Depends on:** 1.15

**Acceptance**

- [ ] Round-trip passes for all 1446
- [ ] (5,7) is MSG_ATTACH with access 1; (7,27) is MSG_USER_AUTHEN_V3; (12,92) is MSG_MINIGAMEREWARDS; (5,254) not found
- [ ] Truncated body returns false; trailing bytes flagged

### Detailed spec from NET-4: Message registry and exhaustive round-trip suite

Any (service, order) pair resolves at runtime to a name, access level, field layout and factory, and every generated message provably round-trips.

**Deliverables**

- Generated <build>/gen/Messages/MessageRegistry.cpp. It fills src/server/shared/Messages/MessageRegistry.h: a 256x256 table of MessageInfo {name, accessLevel, fieldCount, decodeToDynamic, encodedMinSize}
- src/server/shared/Messages/DynamicMessage.h: a field-by-field decoded view for logging unknown or unhandled messages by name
- src/test/server/shared/Messages/GeneratedRoundTripTest.cpp (generated or table-driven): fills every message with deterministic pseudo-random values and checks encode -> decode -> equality and encoded size == sum of field sizes

**Client messages:** All 969 message ids

**Data sources**

- Generated code from NET-3

**Acceptance**

- [ ] The round-trip test passes for all 969 messages
- [ ] Lookups: (5,7) returns MSG_ATTACH with access level 1; (7,27) returns MSG_USER_AUTHEN_V3; (12,92) returns MSG_MINIGAMEREWARDS; (5,254) returns not found
- [ ] A decode of a truncated body returns false, and a body with extra trailing bytes is flagged as a size mismatch rather than silently accepted

**Risks**

- Test runtime and binary size with 969 structs; keep the registry data-only

## 1.17 KI frame codec and reassembler (NET-5)

**Goal:** 0xF00D frames from fragmented reads.

**Size:** M. **Depends on:** 1.06

**Acceptance**

- [ ] Control frame with 14-byte body has len 19; DML with 10-byte body has len 19 and dmlLen 14
- [ ] One byte at a time yields one frame; 3 frames in one buffer yield 3
- [ ] Bad magic or len > MaxFrameSize errors before allocation
- [ ] 10k randomized splits clean under ASan/UBSan

### Detailed spec from NET-5: KI frame codec and stream reassembler

Raw TCP bytes are split into complete control frames or DML messages regardless of how reads are fragmented, and outgoing frames are built byte-exactly.

**Deliverables**

- src/server/shared/Network/Frame.h/.cpp. FrameHeader: u16 magic 0xF00D (wire 0D F0); u16 len (len counts bytes after itself, including the trailing null); if len==0x8000, a u32 long length follows; then u8 isControl, u8 opcode (control only), u16 reserved. Non-control frames add u8 serviceId, u8 order, u16 dmlLen (= body+4), then the body, then u8 0x00
- src/server/shared/Network/FrameReassembler.h/.cpp: accumulates bytes, yields frames, and enforces a configurable max frame size. On a bad magic it reports a protocol error; it does not resync by skipping bytes
- FrameWriter: builds control and DML frames. Long-frame encoding lives behind one function so NET-6's capture can settle its exact semantics
- src/test/server/shared/Network/FrameTest.cpp

**Data sources**

- Layout learned from Imcodec.MessageLayer/MessageEncoder.cs and a local packet capture tool/sniff.py Framer (behavior reference only)

**Acceptance**

- [ ] Hand-built vectors: a control frame with a 14-byte body has len = 14+5 = 19; a DML frame with a 10-byte body has len = 10+9 = 19 and dmlLen = 14
- [ ] Feeding one valid frame one byte at a time yields exactly one frame; feeding 3 frames in one buffer yields 3
- [ ] Garbage prefix or wrong magic gives a protocol error; len above Network.MaxFrameSize gives an error before any allocation
- [ ] A DML frame containing two back-to-back DML messages (dmlLen chaining) decodes into 2 messages (the Imlight decoder supports this; see open question on whether the client sends it)
- [ ] Randomized split-point test over 10k generated frames passes under ASan/UBSan

**Risks**

- Long-frame semantics conflict: Imcodec writes the u32 as the message body length with the 0x8000 marker when the body is over 0x777F, while sniff.py computes total = 8 + u32 + 1, which disagrees for DML frames by 8 bytes. Must be confirmed on the wire (NET-6)

## 1.18 Control messages and capture verification (NET-6)

**Goal:** SessionOffer/KeepAlive/KeepAliveRsp/SessionAccept exact.

**Size:** S. **Depends on:** 1.17

**Acceptance**

- [ ] Hand-written vectors round-trip
- [ ] Checklist recorded: offer length (23 vs 28 bytes), server keepalive layout, long-frame semantics, multi-DML frames, keepalive cadence

### Detailed spec from NET-6: Control messages and wire capture verification

The four control messages are encoded exactly as the 1.610 client expects, and the open framing questions are settled against real captures.

**Deliverables**

- src/server/shared/Network/ControlMessages.h/.cpp, hand-written (they are not in the XML). SessionOffer (opcode 0): u16 sessionId, i32 timeHigh, i32 timeLow, u32 millis, plus whatever trailing bytes the capture proves. KeepAlive (3) client->server: u16 sessionId, u16 millis, u16 elapsedMinutes. Server->client: u16 sessionId, u32 millis. KeepAliveRsp (4): same layout as client KeepAlive. SessionAccept (5): u16 reserved, i32 timeHigh, i32 timeLow, u32 millis, u16 sessionId
- Capture procedure doc/ (Markdown with header) describing how a maintainer records raw frames from their own client session with their own tooling, and which facts to extract. Captures themselves are never committed
- Golden byte-vector tests written by hand from the documented facts (not pasted capture files)

**Client messages:** Control opcodes 0 SessionOffer, 3 KeepAlive, 4 KeepAliveRsp, 5 SessionAccept

**Data sources**

- Behavior reference: Imcodec.MessageLayer/ControlMessageProtocol.cs, Imlight Shared/Services/ControlService.cs, Aurorium src/wizard_patcher.rs (SESSION_OFFER_LENGTH = 28)
- Maintainer's own client capture (local only)

**Acceptance**

- [ ] Unit tests round-trip each control message and match the hand-written vectors
- [ ] Verification checklist answered and recorded in the doc: (a) SessionOffer body length the 1.610 client accepts. Imlight sends a 23-byte frame; the Aurorium patch fetcher expects a 28-byte offer from the live KI patch server. (b) Server keepalive layout and whether the client answers it with opcode 4. (c) Long-frame length semantics for a frame over 0x7780 bytes. (d) Whether the client ever packs 2+ DML messages in one frame. (e) Client KeepAlive cadence
- [ ] FrameWriter long-frame test updated to the confirmed semantics

**Risks**

- The 28-byte offer comes from a newer live server; the 1.610 client may accept either form. If the extra bytes are a signed-key block, a server without KI's key may need to send an empty one

## 1.19 Async socket layer and SocketMgr (NET-7)

**Goal:** Accept many connections and queue frame writes.

**Size:** M. **Depends on:** 1.17, 1.11, 1.10

**Acceptance**

- [ ] 200 clients x 1000 fragmented frames arrive intact and in order
- [ ] Mid-frame close leaks nothing
- [ ] DelayedClose flushes before FIN
- [ ] A duplicate port bind fails loudly

### Detailed spec from NET-7: Async socket layer and SocketMgr

Each app can listen on its configured port, accept many connections, and read and write frames asynchronously on a network thread pool.

**Deliverables**

- src/server/shared/Network/Socket.h/.cpp: a Boost.Asio (pending decision) TCP socket wrapper with an async read loop into FrameReassembler, a write queue that coalesces pending frames into one write, and CloseSocket/DelayedClose (close after the queue drains)
- src/server/shared/Network/SocketMgr.h, AsyncAcceptor.h, NetworkThread.h: N io threads, least-loaded socket placement, SO_REUSEADDR off on Windows, TCP_NODELAY on
- Config keys in conf/dist/loginserver.conf.dist, gameserver.conf.dist and patchserver.conf.dist: BindIP, Port (12000/12333/12500 are the conventional defaults), Network.Threads, Network.MaxFrameSize, Network.OutKBuff, Network.TcpNoDelay
- src/test/server/shared/Network/SocketIntegrationTest.cpp with a loopback fake client

**Data sources**

- None

**Acceptance**

- [ ] Loopback test: 200 concurrent fake clients each send 1000 fragmented frames; every frame arrives intact and in order per connection
- [ ] A peer closing mid-frame releases the socket with no leak (ASan/LSan clean)
- [ ] DelayedClose sends the final queued frame before FIN (needed for MSG_CHARACTERSELECTED and MSG_FORCE_DISCONNECT)
- [ ] Starting two apps on the same port fails loudly with a logged bind error

**Risks**

- The Boost.Asio vs standalone Asio decision is pending
- Imlight treats each recv() as a full packet (SocketListener.ProcessReceivedData), which breaks under fragmentation. Do not copy that behavior

## 1.20 App skeletons (FND-12)

**Goal:** loginserver, gameserver, patchserver lifecycle.

**Size:** S. **Depends on:** 1.10, 1.11

**Acceptance**

- [ ] --version exits 0
- [ ] Missing conf names the path and exits 1
- [ ] Ctrl+C exits 0 within 2 s

### Detailed spec from FND-12: App skeletons: loginserver, gameserver, patchserver start and stop cleanly

All three executables run the standard lifecycle (args, config, logging, banner, io loop, signal shutdown), ready for NET and database to plug in.

**Deliverables**

- src/server/apps/{loginserver,gameserver,patchserver}/Main.cpp + CMakeLists.txt (targets link common only for now)
- src/server/apps/<app>/<app>.conf.dist listing every option the skeleton reads (LogsDir, Appender.*, Logger.*, BindIP, port option, Updates.* placeholders later)
- Command-line: -c/--config <file>, -v/--version, --help (hand-written parser, or Boost.Program_options as AzerothCore does; decision)
- SIGINT/SIGTERM and Windows console Ctrl+C handler trigger graceful stop; process exit code 0
- Main loop tick with configurable update diff for gameserver (World update placeholder for WLD)
- Optional Windows service/daemon hooks deferred

**Acceptance**

- [ ] `gameserver --version` prints GitRevision full version and exits 0
- [ ] Starting gameserver without gameserver.conf logs a clear error naming the expected path and exits 1; with a copied .conf.dist it logs the banner and 'ready'
- [ ] Ctrl+C logs 'shutting down' and exits 0 within 2 seconds on Windows and Linux
- [ ] Real client: nothing yet. Launching the client with `-L 127.0.0.1 <LoginServerPort>` still gives a connection failure because no socket is bound. The first client-visible step (sending the SessionOffer on accept) is NET-1.

**Risks**

- Default ports are not settled (the reference README uses 12000 for login); leave them to NET or the maintainer

## 1.21 Patch-free dev path documented (PAT-1)

**Goal:** Run the client without KingsIsle patch hosts.

**Size:** S. **Depends on:** 1.19

**Acceptance**

- [ ] With -P 0 a listener on :12500 records zero connections while the client connects to :12000
- [ ] The default behaviour without -P is recorded in doc/PATCHING.md
- [ ] No 'Patch failed' dialog with -P 0

### Detailed spec from PAT-1: Patch-free development path (client -P 0) proven and documented

Every other domain can run the retail client against loginserver/gameserver with no patchserver and no contact with KingsIsle patch hosts.

**Deliverables**

- doc/PATCHING.md: how to launch WizardGraphicalClient.exe with -L <host> <port> -P 0 (and optional -A <locale>) from the user's own install; warning never to run the retail launcher against a pinned install
- apps/launcher/ (repo tooling): run-client.bat.dist / run-client.ps1.dist template reading the install path from a local, git-ignored config
- conf/dist/worldserver.conf.dist + loginserver.conf.dist option Patch.Enabled = 0 (default for dev) read by gameserver/loginserver (consumed by PAT-9)

**Data sources**

- Bin/WizardGraphicalClient.exe usage string: '-P <Patching Enabled (0|1)>', '-PT - <Patch Client Patch Time>', '-A <locale>', '-L <login server name | IP> <Port>'
- Bin/PatchConfig.xml in the user's install (PatchServerHostname/PatchServerPort/LoginHostname/CommandLine)

**Acceptance**

- [ ] With a plain TCP listener bound to 127.0.0.1:12500 and the client launched with -L 127.0.0.1 12000 -P 0, the listener records zero connections while the client connects to port 12000 (any login-port listener, no loginserver needed)
- [ ] Repeat without -P: record whether the client contacts the PatchConfig.xml host (patch.us.wizard101.com:12500) by default; result written into doc/PATCHING.md (resolves the default-value open question)
- [ ] Client shows the login screen with no 'Patch failed - Error connecting Patch Server' or GUI_PatchingFailed dialog when -P 0 is used

**Risks**

- Unverified: the default value of PatchingEnabled when -P is omitted; a launch without -P may reach out to KingsIsle
- With patching disabled the client may refuse zones whose WADs are missing locally (the MSG_PATCHINGBLOCKED path), so dev installs must be complete

## 1.22 Session handshake and keep-alive (NET-8)

**Goal:** A real client completes the handshake with loginserver.

**Size:** M. **Depends on:** 1.18, 1.19, 1.20, 1.16, 1.21

**Client messages:** MSG_USER_AUTHEN_V3

**Acceptance**

- [ ] Fake client: wrong accept id closes; no accept in 15 s closes; keepalive echo correct
- [ ] Real client: log shows SessionOffer sent, SessionAccept with matching id, then 'LOGIN MSG_USER_AUTHEN_V3 (7:27)'
- [ ] Idle 5 minutes at login: keepalives both ways, no drop

### Detailed spec from NET-8: Session object, handshake and keep-alive

A real Wizard101 client completes the session handshake with an Ambrose server and stays connected while idle.

**Deliverables**

- src/server/shared/Network/SessionBase.h/.cpp: unique u16 session id allocator (never 0, recycled only after close), offer timestamp/millis stored and exposed (LOG needs them for MSG_USER_AUTHEN_V3 decryption), and states Offered -> Accepted -> (app-defined)
- Sends SessionOffer immediately on accept, ahead of any other work. Waits for SessionAccept with Network.SessionAcceptTimeout (default 15s); a mismatched sessionId gives a protocol error and close
- Answers client KeepAlive with KeepAliveRsp echoing the elapsed field. Optional server keepalive every Network.KeepAliveInterval (60s) with Network.KeepAliveTimeout (15s)
- DML frames received before SessionAccept are queued, not dropped (Imlight SessionActor._preInitMessages suggests the client can send early)
- Round-trip time measured from offer to accept and from keepalive exchanges
- src/server/apps/loginserver: a minimal bootstrap that runs only the handshake and logs every decoded DML message by name via MessageRegistry

**Client messages:** Control 0/3/4/5, MSG_USER_AUTHEN_V3 (logged only, handled by LOG)

**Data sources**

- User's 1.610 client executable (run locally by the maintainer)

**Acceptance**

- [ ] Unit test with a fake client: offer bytes as specified; accept with the wrong id closes; no accept in 15s closes; keepalive echo is correct
- [ ] Real client: start loginserver, launch the client pointed at 127.0.0.1:12000 (-L 127.0.0.1 12000), and trigger login. The server log shows SessionOffer sent, SessionAccept received with a matching id, then a decoded 'LOGIN MSG_USER_AUTHEN_V3 (7:27)' line proving the client accepted the session and moved on to authenticate
- [ ] Real client idle for 5 minutes at the login stage: keepalives are logged in both directions and the server never drops the session

**Risks**

- Exactly when the client opens the login connection (at launch or on pressing Login) is unverified
- Imlight suspends heartbeat timeouts during the login-to-game handoff (MSG_OPCODE_HALT in ControlService.cs); a too-strict timeout may drop clients during character select

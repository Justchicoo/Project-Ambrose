<!-- Project Ambrose by Imjustchico: Roadmap phase 16, Patch server and tooling ownership. -->

# Phase 16: Patch server and tooling ownership

**Done when:** The retail client patches against Ambrose with 0 files altered, restores a deleted WAD, and streams missing zone packages. The manifest reloads and every patch option changes without a restart. Users produce their own type dump with Ambrose tooling. This track can run in parallel any time after phase 2.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 16.01 | FileBinary table codec (PAT-3) | S | 1.14, 1.13 |
| 16.02 | LatestFileList model and XML (PAT-4) | S | 16.01 |
| 16.03 | Install scanner manifest generator (PAT-5) | M | 16.02, 1.12 |
| 16.04 | patchserver TCP service 8 (PAT-6) | M | 16.03, 2.09 |
| 16.05 | HTTP file service with Range (PAT-7) | M | 16.03, 1.19 |
| 16.06 | Real-client no-op patch and repair (PAT-8) | M | 16.04, 16.05, 1.21 |
| 16.07 | Login/game patching switches (PAT-9) | S | 2.14, 4.14, 4.16 |
| 16.08 | MSG_NEXT_VERSION and patch hardening (PAT-11) | S | 16.04, 16.05 |
| 16.09 | DOWNLOADPACKAGE format RE (PAT-10 part 1) | M | 16.06, 16.07 |
| 16.10 | In-game package streaming (PAT-10 part 2) | M | 16.09, 6.14 |
| 16.11 | Project-owned type dumper spike (OBJ-20 part 1) | M | 3.03 |
| 16.12 | Type dump v2 emission and validator (OBJ-20 part 2) | M | 16.11, 3.01 |
| 16.13 | Player launcher that patches from Ambrose (new) | M | 16.06, 16.08 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Oversized.** 16.11 project-owned type dumper spike (M). Walking a live client process's type system is a substantial RE and tooling effort.

## 16.01 FileBinary table codec (PAT-3)

**Goal:** LatestFileList.bin byte-exact.

**Size:** S. **Depends on:** 1.14, 1.13

**Client messages:** MSG_CUSTOMDICT, MSG_CUSTOMRECORD

**Acceptance**

- [ ] Synthetic list round-trips
- [ ] 3 _TableList rows begin 03 00 00 00 02 01 28 00 04 00 'Name' 09 28 ...
- [ ] Dev-gated: reference .bin re-emits identically (1144891 bytes)

### Detailed spec from PAT-3: FileBinary table codec for LatestFileList.bin

The server can read and write the client's binary table-list format byte for byte.

**Deliverables**

- src/server/shared/Patch/BinaryTableFile.h/.cpp: per table u32 recordCount, then an ExtendedBase (service 2) MSG_CUSTOMDICT (order 1) listing fields as u16-length name + u8 type + u8 flag (observed 0x28), ending with _TargetTable (STR) plus the table-name string, then recordCount MSG_CUSTOMRECORD (order 2) messages of field values; message header is u8 service, u8 order, u16 length including the 4-byte header
- Field type mapping shared with NET's DML codec (observed 3=UINT, 9=STR)
- src/test/server/shared/Patch/BinaryTableFileTest.cpp

**Client messages:** MSG_CUSTOMDICT, MSG_CUSTOMRECORD

**Data sources**

- ExtendedBaseMessages.xml (service 2; no _MsgOrder, so orders come from alphabetical name order: MSG_CUSTOMDICT=1, MSG_CUSTOMRECORD=2, which matches the observed bytes)
- Observed structure of Aurorium data/V_r806919.Wizard_1_610/LatestFileList.bin (first table: count 3591, dict 02 01 28 00)

**Acceptance**

- [ ] Unit: write then read a synthetic list (_TableList, About{Version=1}, Base with 2 records) round-trips all fields
- [ ] Unit: first bytes of a written list with 3 _TableList rows begin 03 00 00 00 02 01 28 00 04 00 'Name' 09 28 0C 00 '_TargetTable' 09 28 0A 00 '_TableList' when names match the retail dictionary
- [ ] Env-gated dev test (AMBROSE_REFERENCE_LIST points at a list the developer obtained themselves, never committed): parse the reference .bin, re-emit it, and get identical bytes (1144891 bytes for r806919)

**Risks**

- Meaning of the per-field flag byte 0x28 is unverified; copy it as observed until NET's DML work explains it
- Unverified whether the client needs the exact table order of the retail list (alphabetical in the reference)

## 16.02 LatestFileList model and XML (PAT-4)

**Goal:** One manifest, XML and BIN.

**Size:** S. **Depends on:** 16.01

**Acceptance**

- [ ] Model -> XML -> model and BIN lossless
- [ ] Reference XML gives 3590 tables (Base 140, PatchClient 97)

### Detailed spec from PAT-4: LatestFileList model and XML writer/reader

One in-memory manifest can be emitted as both LatestFileList.xml and LatestFileList.bin.

**Deliverables**

- src/server/shared/Patch/LatestFileList.h/.cpp: Package{name, records}, FileRecord{SrcFileName, TarFileName, FileType, Size, HeaderSize, CompressedHeaderSize, CRC, HeaderCRC}, About{Version}
- src/server/shared/Patch/LatestFileListXml.h/.cpp: writes <LatestFileList><_TableList>(RECORD Name)...</_TableList><About><RECORD><Version TYPE="UINT">1</Version></RECORD></About><Package><RECORD>...</RECORD></Package>...; reader tolerant of whitespace
- src/test/server/shared/Patch/LatestFileListXmlTest.cpp

**Data sources**

- Aurorium data/V_r806919.Wizard_1_610/LatestFileList.xml (structure only; never committed)

**Acceptance**

- [ ] Unit: model -> XML -> model and model -> BIN -> model are both lossless
- [ ] Unit: _TableList row count equals package count + About; empty TarFileName is written as an empty STR element
- [ ] Env-gated: parsing a developer-supplied reference XML gives 3590 tables (Base 140 records, PatchClient 97, every other table exactly 1 record 'Data/GameData/<table>.wad')

**Risks**

- The XML library is pugixml through vcpkg, settled in doc/ARCHITECTURE.md, so no library choice is pending

## 16.03 Install scanner manifest generator (PAT-5)

**Goal:** Manifest from the user's install.

**Size:** M. **Depends on:** 16.02, 1.12

**Acceptance**

- [ ] Synthetic dir gives {Base, ZoneA, ZoneB} with correct Size/CRC/HeaderSize/HeaderCRC
- [ ] Reference diff: 100% match on type 3/5 CRC/HeaderSize/HeaderCRC, all 3590 packages
- [ ] Second run faster, identical .bin

### Detailed spec from PAT-5: Install scanner: generate the manifest from the user's client install

A tool builds a patch output directory (manifest plus revision name) from the user's own install with no KingsIsle data committed.

**Deliverables**

- src/tools/patchlist_generator/ (tool depends only on shared+common): walks the install; package rules: every Data/GameData/<X>.wad except Root.wad goes to package X; Root.wad, Bin/** and Data/GameData non-WAD files go to Base; launcher files go to PatchClient; Src/Tar mapping rules (e.g. Src 'Windows/Bin/PatchConfig.xml' -> Tar 'Bin/PatchConfig.xml') loaded from a conf/dist rules file
- Revision name from Bin/revision.dat (content 'r806919.Wizard_1_610'), giving a 'V_r806919.Wizard_1_610' output directory
- CRC cache keyed by path+size+mtime so reruns skip rehashing Root.wad (295 MB)
- FileType assignment: 3 = WAD (default), 5 = WAD listed in a rules override, 1 = plain file, 4 = rules override (see open questions)
- Optional --reference <LatestFileList.xml> mode that copies FileType/CompressedHeaderSize/package membership from a list the user supplies and prints a diff report
- conf/dist/patchlist_generator.conf.dist

**Data sources**

- User's install: Bin/revision.dat, Bin/**, Data/GameData/*.wad, Data/GameData/** non-WAD files, PatchClient/**
- Optional user-supplied reference LatestFileList.xml for fields not derivable offline

**Acceptance**

- [ ] Unit (synthetic temp dir): 2 zone WADs + Root.wad + Bin/a.dll give packages {Base, ZoneA, ZoneB} with the correct Size/CRC/HeaderSize/HeaderCRC
- [ ] Env-gated diff run on r806919 with a reference list: Size, CRC, HeaderSize and HeaderCRC match 100% of type 3/5 records, and package membership matches for all 3590 tables
- [ ] Second run finishes much faster than the first (CRC cache hit) and gives an identical .bin

**Risks**

- CompressedHeaderSize could not be reproduced with Python zlib at any level (only about 8 of 400 WADs matched), so it may not be derivable offline
- FileType 5 vs 3 is not name-derivable (Ahmarra-WorldData is 3 while GUI-WorldData is 5)
- Type-4 files carry HeaderSize == zlib(level 6) size + 12 (checked on 4 DLLs), which suggests a compressed download variant this tool must also produce

## 16.04 patchserver TCP service 8 (PAT-6)

**Goal:** Reply to MSG_LATEST_FILE_LIST_V2.

**Size:** M. **Depends on:** 16.03, 2.09

**Client messages:** MSG_LATEST_FILE_LIST_V2, MSG_LATEST_FILE_LIST

**Acceptance**

- [ ] Fake client gets ListFileSize/ListFileCRC equal to the .bin
- [ ] Probe without session accept still gets a reply
- [ ] ListFileURL contains '/V_r806919.Wizard_1_610/'
- [ ] Corrupt manifest reload keeps the old one; port change rebinds live

### Detailed spec from PAT-6: patchserver app: TCP service 8 answering MSG_LATEST_FILE_LIST_V2

A client or probe that connects to the patch port gets a correct pointer to our manifest.

**Deliverables**

- src/server/apps/patchserver/Main.cpp + PatchServerApp (standalone Asio acceptor, as settled in doc/ARCHITECTURE.md)
- src/server/game-independent handler file src/server/apps/patchserver/Handlers/PatchHandler.cpp registering MSG_LATEST_FILE_LIST_V2 (state: never authenticated) and MSG_LATEST_FILE_LIST (v1, same reply without Locale)
- Reply fields: LatestVersion=1, ListFileName='LatestFileList.bin', ListFileType=1, ListFileTime=manifest mtime (unix), ListFileSize and ListFileCRC of the .bin, ListFileURL='<BaseUrl>/<V_revision>/Windows/LatestFileList.bin' (path style unverified), URLPrefix='<BaseUrl>/<V_revision>', URLSuffix='', Locale echoed
- Manifest loaded at startup and reloaded from the console or the admin API with an atomic swap; a manifest that fails to load or validate keeps the old one serving and reports every error. This hooks into the 4.15 reload framework and the 17.12 admin API when those land
- conf/dist/patchserver.conf.dist: PatchServer.BindIP, PatchServer.Port=12500, Http.PublicBaseUrl, Patch.OutputDir, Patch.SupportedLocales, all live settings once 4.16 lands; a port or address change rebinds live, and a failed bind keeps the old listener

**Client messages:** MSG_LATEST_FILE_LIST_V2, MSG_LATEST_FILE_LIST

**Data sources**

- PatchMessages.xml (service 8)
- Behaviour reference: Aurorium src/wizard_patcher.rs (probe sequence, reply parse offsets); Imlight src/Imlight.CoreLib/Patch/Services/PatchService.cs (ListFileType must be 1; CRC of the .bin)

**Acceptance**

- [ ] Integration test (src/test): fake client completes the NET session handshake, sends MSG_LATEST_FILE_LIST_V2 with Locale 'English', and gets a reply whose ListFileSize/ListFileCRC equal the .bin on disk
- [ ] A probe that sends the V2 request right after the 28-byte session offer, without a session accept first (the Aurorium probe byte sequence 0D F0 27 00 00 00 00 00 08 02 22 00 + 30 zero bytes), still gets a reply
- [ ] Reply ListFileURL contains '/V_r806919.Wizard_1_610/'
- [ ] Unknown locale: behaviour matches the client string 'Locale (%s) not supported by the server' (exact reply to be decided from the PAT-8 client test, logged)
- [ ] Reloading a corrupt manifest keeps the old manifest serving and reports the error, and changing PatchServer.Port rebinds without a restart while a failed bind keeps the old listener

**Risks**

- Depends on NET's session layer (0xF00D framing, session offer/accept, keepalive) being reusable by a third app
- Unverified whether the real client sends a session accept before the V2 request

## 16.05 HTTP file service with Range (PAT-7)

**Goal:** Serve manifest-listed files.

**Size:** M. **Depends on:** 16.03, 1.19

**Acceptance**

- [ ] GET of every file CRC matches manifest
- [ ] Range 0-(HeaderSize-1) CRC == HeaderCRC
- [ ] Traversal and unlisted paths 400/404
- [ ] HEAD Content-Length == Size
- [ ] Http.MaxConnections change applies live

### Detailed spec from PAT-7: HTTP file service with Range and Src->local path mapping

The client can fetch the manifest, whole files and byte ranges from our server, served straight from the user's install.

**Deliverables**

- src/server/apps/patchserver/Http/ (Crow on the standalone Asio layer, the HTTP library doc/ARCHITECTURE.md settled for the admin API): GET and HEAD for /<V_revision>/<SrcFileName>; LatestFileList.bin/.xml served from Patch.OutputDir, everything else mapped Src->Tar->install path using the manifest (only paths listed in the manifest are servable)
- Range: bytes=a-b support (206 + Content-Range), Content-Length on every response, correct 404/416
- Path traversal rejection (.., absolute, drive prefixes), case-insensitive lookup on Windows installs
- Optional serving of the compressed variant for FileType 4 once its URL/format is confirmed (see open questions)
- patchserver.conf.dist: Http.BindIP, Http.Port, Http.MaxConnections, all live settings once 4.16 lands; a port or address change rebinds live, a failed bind keeps the old listener, and Http.MaxConnections applies to the next connection

**Data sources**

- User's install files
- Behaviour reference: Aurorium src/routes/file.rs (traversal guard, LatestFileList served from requested revision); asset URL form url_prefix + '/' + SrcFileName from Aurorium src/fetcher/asset_fetcher.rs

**Acceptance**

- [ ] Integration: GET of every manifest file (env-gated) returns bytes whose Crc32 equals the manifest CRC
- [ ] Integration: Range bytes=0-(HeaderSize-1) on a zone WAD returns HeaderSize bytes with CRC == HeaderCRC
- [ ] Unit: GET /V_x/../Bin/x and a path not in the manifest return 400/404 without touching the filesystem outside the install
- [ ] HEAD returns Content-Length == Size (the client string 'FileInfo filesize mismatch with URL filesize' shows it checks this)
- [ ] Lowering Http.MaxConnections while running refuses the next connection over the new limit without a restart, and in-flight downloads complete

**Risks**

- The HTTP library is Crow on the standalone Asio layer, as settled in doc/ARCHITECTURE.md; whether Range responses need Ambrose's own handling on top of it is unverified
- Unverified whether the client uses HTTP Range for 'DownloadSegment ... Offset=%u, Size=%u' or a different segment URL scheme
- Retail uses HTTP (user agent 'KingsIsle Patcher'); TLS need is unverified

## 16.06 Real-client no-op patch and repair (PAT-8)

**Goal:** Client patches against Ambrose.

**Size:** M. **Depends on:** 16.04, 16.05, 1.21

**Client messages:** MSG_LATEST_FILE_LIST_V2

**Acceptance**

- [ ] Complete install: 'Files altered: 0' then login screen
- [ ] Deleted Aquila-AQ_Z01_MountOlympus.wad restored with matching CRC
- [ ] Corrupted Bin/*.ini re-downloaded; patchserver down shows failure path

### Detailed spec from PAT-8: Real-client end-to-end: no-op patch and repair of a missing file

The retail client patches against Ambrose, finds nothing to change on a complete install, and restores a deleted file from our server.

**Deliverables**

- doc/PATCHING.md section: point a COPY of the install's Bin/PatchConfig.xml PatchServerHostname/Port at 127.0.0.1:12500 (user action on their own copy, never the pinned install) and launch with -P 1
- patchserver request logging (connection, V2 locale, every HTTP path + range) at a debug log level

**Client messages:** MSG_LATEST_FILE_LIST_V2

**Data sources**

- User's install copy
- Client-side log output (the -G <log file> option)

**Acceptance**

- [ ] Client on a complete install: patchserver logs one V2 request and one GET of LatestFileList.bin; the client's patching window (WizardPatchingWindow / MaximizedClientPatcher.gui) completes with 'Files altered: 0' in the client log, then the login screen appears
- [ ] Delete one small zone WAD (e.g. Data/GameData/Aquila-AQ_Z01_MountOlympus.wad) from the copy: the client requests exactly that file (whole or header+segments), the restored file's CRC matches the manifest, and a relaunch reports 0 altered
- [ ] Corrupt 1 byte in a Base file (a Bin/*.ini): the client re-downloads it and the log shows no CRC mismatch errors ('Downloaded file's length or CRC not matching the original's' absent)
- [ ] Stop patchserver and relaunch with -P 1: the client shows its patch-failure path (record the exact dialog in docs); with -P 0 it goes straight to login

**Risks**

- The client may keep per-package VER_*.dat / CRC_*.dat caches that hide changes; the doc must say where they are and how to clear them (unverified location)
- Test on a copy only, because patching can rewrite the pinned install

## 16.07 Login/game patching switches (PAT-9)

**Goal:** Correct behaviour with patching on or off.

**Size:** S. **Depends on:** 2.14, 4.14, 4.16

**Client messages:** MSG_USER_AUTHEN_V3, MSG_USER_AUTHEN_V2, MSG_USER_VALIDATE, MSG_PATCHINGBLOCKED, MSG_LOGPATCHCLIENTPATCHTIME, MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT, MSG_DOWNLOADBROWSER

**Acceptance**

- [ ] PATCHINGBLOCKED and LOGPATCHCLIENTPATCHTIME registered 'logged in'
- [ ] -P 0 zones in with no download-package messages
- [ ] Revision mismatch rejected only with Login.RequireRevision=1
- [ ] Login.RequireRevision and Patch.Enabled changes apply live

### Detailed spec from PAT-9: Game/login integration switches for patching

Login and game servers behave correctly whether patching is enabled or disabled.

**Deliverables**

- loginserver: accept and log PatchClientID / Revision / DataRevision from MSG_USER_AUTHEN_V3 (and V2/MSG_USER_VALIDATE), and optionally reject a Revision that differs from the patchserver manifest revision (live setting Login.RequireRevision, applied from the next login)
- gameserver: live setting Patch.Enabled, applied from the next zone transfer; when 0 never send MSG_DOWNLOADPACKAGE / MSG_DOWNLOADPACKAGEELEMENT / MSG_DOWNLOADBROWSER
- gameserver handlers for client MSG_PATCHINGBLOCKED (log PackageName/ZoneName and keep the player in place) and MSG_LOGPATCHCLIENTPATCHTIME (log only), in game/Handlers/PatchHandler.cpp

**Client messages:** MSG_USER_AUTHEN_V3, MSG_USER_AUTHEN_V2, MSG_USER_VALIDATE, MSG_PATCHINGBLOCKED, MSG_LOGPATCHCLIENTPATCHTIME, MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT, MSG_DOWNLOADBROWSER

**Data sources**

- LoginMessages.xml (service 7)
- WizardMessages.xml (service 12)
- GameMessages.xml (service 5)

**Acceptance**

- [ ] Unit: dispatch table has MSG_PATCHINGBLOCKED and MSG_LOGPATCHCLIENTPATCHTIME registered with state 'logged in'
- [ ] Real client with -P 0 logs in and zones in without the gameserver sending any download-package message (sniffer or server log shows none)
- [ ] Login with mismatched Revision is rejected with a clear login error only when Login.RequireRevision=1
- [ ] `.settings set Login.RequireRevision 1` on a running loginserver rejects the next mismatched login without a restart, and setting Patch.Enabled to 0 stops download-package messages from the next zone transfer

**Risks**

- Belongs partly to the LOG and WLD domains; the milestone ids for them are guesses

## 16.08 MSG_NEXT_VERSION and patch hardening (PAT-11)

**Goal:** Safe public exposure.

**Size:** S. **Depends on:** 16.04, 16.05

**Client messages:** MSG_NEXT_VERSION

**Acceptance**

- [ ] NEXT_VERSION{Base,1} replied, connection healthy
- [ ] 200 concurrent ranges correct; 201st refused
- [ ] Reload mid-download completes with old CRC
- [ ] Rate limit changes apply live

### Detailed spec from PAT-11: MSG_NEXT_VERSION and operational hardening

The patchserver handles every service-8 message safely and survives public exposure.

**Deliverables**

- Handler for MSG_NEXT_VERSION: reply with the same PkgName and current Version, which means no newer version (exact semantics to confirm; client strings 'Version advanced' / 'Patch failed - Unsupported version' / 'Error downloading RTPatch file')
- Per-IP connection and request rate limits, idle timeout on the TCP service, max HTTP body/header sizes, each a live setting with bounds that applies from the next connection or request
- Manifest hot reload from the console or the admin API, plus a patchserver.conf option, with atomic swap so in-flight downloads keep the old file set and a failed reload keeps the old manifest and reports every error; this uses the 4.15 reload framework when it lands
- CI check in apps/ that no LatestFileList.*, *.wad or client files are staged (supports the never-commit rule)

**Client messages:** MSG_NEXT_VERSION

**Data sources**

- PatchMessages.xml
- Client Patcher.cpp strings around MSG_NextVersion

**Acceptance**

- [ ] Integration: MSG_NEXT_VERSION{PkgName='Base', Version=1} gets a reply and the connection stays healthy
- [ ] Load test: 200 concurrent HTTP range requests complete with correct CRCs; the 201st connection over the limit gets refused cleanly
- [ ] Reload during a large download: the download completes with the old CRC, and new requests see the new manifest
- [ ] Lowering the per-IP request limit on a running patchserver takes effect for the next request without a restart
- [ ] CI job fails when a *.wad or LatestFileList.bin is added to a commit

**Risks**

- MSG_NEXT_VERSION may be a dead RTPatch path in 1.610; replying wrongly could put the client in a patch-failure loop, so the reply stays gated by a live setting

## 16.09 DOWNLOADPACKAGE format RE (PAT-10 part 1)

**Goal:** Decode the Data STR of download messages.

**Size:** M. **Depends on:** 16.06, 16.07

**Client messages:** MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT

**Acceptance**

- [ ] doc/ records the Data format from our own server or client analysis

### Detailed spec from PAT-10: In-game package streaming (MSG_DOWNLOADPACKAGE and segment downloads)

A client missing zone WADs streams them from Ambrose during play instead of being blocked.

**Deliverables**

- RE note in doc/ of the Data string format of MSG_DOWNLOADPACKAGE / MSG_DOWNLOADPACKAGEELEMENT (from the client via Ghidra, or from sniffing our own server. A user may also capture their own live KingsIsle session as their own choice, on their own account and machine: that may break KingsIsle's terms and risks the account, and such captures are never committed)
- gameserver: before a zone transfer, send MSG_DOWNLOADPACKAGE for the destination package when Patch.Enabled=1 and the client reports it is not patched
- patchserver: segment-download support confirmed (Range or other URL form), plus the compressed type-4 variant if required

**Client messages:** MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT, MSG_PATCHINGBLOCKED

**Data sources**

- Bin/WizardGraphicalClient.exe strings: ClientPatcher::RequestPatch/RequestPackage, KIHttpFileSession.cpp 'DownloadSegment Error - Length or CRC mismatch (%s). Offset=%u, Size=%u'

**Acceptance**

- [ ] Delete a zone WAD from the install copy, walk into that zone: the client shows its in-game patch progress (WizardPatchingWindow) or blocks with MSG_PATCHINGBLOCKED, downloads the package from patchserver, then loads the zone
- [ ] Server log shows the WAD header range fetched first, then entry segments, all passing the client's CRC checks (no 'DownloadSegment Error' in the client log)

**Risks**

- The Data field format is completely unverified; this may need a Ghidra pass
- Low priority: dev installs are complete, so this matters only for public distribution

## 16.10 In-game package streaming (PAT-10 part 2)

**Goal:** Missing zone WADs stream during play.

**Size:** M. **Depends on:** 16.09, 6.14

**Client messages:** MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT, MSG_PATCHINGBLOCKED

**Acceptance**

- [ ] Delete a zone WAD, walk in: progress, download, zone loads; no 'DownloadSegment Error'

### Detailed spec from PAT-10: In-game package streaming (MSG_DOWNLOADPACKAGE and segment downloads)

A client missing zone WADs streams them from Ambrose during play instead of being blocked.

**Deliverables**

- RE note in doc/ of the Data string format of MSG_DOWNLOADPACKAGE / MSG_DOWNLOADPACKAGEELEMENT (from the client via Ghidra, or from sniffing our own server. A user may also capture their own live KingsIsle session as their own choice, on their own account and machine: that may break KingsIsle's terms and risks the account, and such captures are never committed)
- gameserver: before a zone transfer, send MSG_DOWNLOADPACKAGE for the destination package when Patch.Enabled=1 and the client reports it is not patched
- patchserver: segment-download support confirmed (Range or other URL form), plus the compressed type-4 variant if required

**Client messages:** MSG_DOWNLOADPACKAGE, MSG_DOWNLOADPACKAGEELEMENT, MSG_PATCHINGBLOCKED

**Data sources**

- Bin/WizardGraphicalClient.exe strings: ClientPatcher::RequestPatch/RequestPackage, KIHttpFileSession.cpp 'DownloadSegment Error - Length or CRC mismatch (%s). Offset=%u, Size=%u'

**Acceptance**

- [ ] Delete a zone WAD from the install copy, walk into that zone: the client shows its in-game patch progress (WizardPatchingWindow) or blocks with MSG_PATCHINGBLOCKED, downloads the package from patchserver, then loads the zone
- [ ] Server log shows the WAD header range fetched first, then entry segments, all passing the client's CRC checks (no 'DownloadSegment Error' in the client log)

**Risks**

- The Data field format is completely unverified; this may need a Ghidra pass
- Low priority: dev installs are complete, so this matters only for public distribution

## 16.11 Project-owned type dumper spike (OBJ-20 part 1)

**Goal:** Walk the client's type system.

Moved on 2026-09-17 to 3.21, which builds the dump by emulating the client program on disk instead of reading a running client. The acceptance below is met there.

**Size:** M. **Depends on:** 3.03

**Acceptance**

- [ ] Spike output lists classes and bases from the maintainer's client; never writes into the client dir

### Detailed spec from OBJ-20: Project-owned type dumper

Users generate their own type dump from their own client, so Ambrose does not depend on third-party dump files.

**Deliverables**

- src/tools/typedump: attaches to or reads the user's running client and walks its type system to emit dump format v2 (classes, bases, properties with flags/container/hash and enum options)
- A validator mode that runs the OBJ-2 hash check and OBJ-4 canonicalization on the output

**Acceptance**

- [ ] On the maintainer's r806919 client, the output is structurally equal to the reference dump: 6981 entries, 49461 properties, identical hashes and flags
- [ ] The tool never writes into the client directory

**Risks**

- Requires reading a live process's memory, which is platform-specific and fragile across client revisions. Decided on 2026-09-16 at the maintainer's direction: the dumper is planned as an experimental opt-in tool. Reading a running client's memory may break KingsIsle's terms, so the user runs it only as their own choice, on their own account and machine, at their own risk
- Cannot be verified in CI

## 16.12 Type dump v2 emission and validator (OBJ-20 part 2)

**Goal:** Users generate their own dump.

Moved on 2026-09-17 to 3.21, whose tool writes format v2 and validates it. The acceptance below is met there.

**Size:** M. **Depends on:** 16.11, 3.01

**Acceptance**

- [ ] 6981 entries, 49461 properties, identical hashes and flags to the reference dump

### Detailed spec from OBJ-20: Project-owned type dumper

Users generate their own type dump from their own client, so Ambrose does not depend on third-party dump files.

**Deliverables**

- src/tools/typedump: attaches to or reads the user's running client and walks its type system to emit dump format v2 (classes, bases, properties with flags/container/hash and enum options)
- A validator mode that runs the OBJ-2 hash check and OBJ-4 canonicalization on the output

**Acceptance**

- [ ] On the maintainer's r806919 client, the output is structurally equal to the reference dump: 6981 entries, 49461 properties, identical hashes and flags
- [ ] The tool never writes into the client directory

**Risks**

- Requires reading a live process's memory, which is platform-specific and fragile across client revisions. Decided on 2026-09-16 at the maintainer's direction: the dumper is planned as an experimental opt-in tool. Reading a running client's memory may break KingsIsle's terms, so the user runs it only as their own choice, on their own account and machine, at their own risk
- Cannot be verified in CI

## 16.13 Player launcher that patches from Ambrose (new)

**Goal:** Players start the game from a launcher that patches only from the Ambrose patch server and never reaches KingsIsle.

**Size:** M. **Depends on:** 16.06, 16.08

**Acceptance**

- [ ] Research notes name every host, port and file the retail WizardLauncher.exe uses
- [ ] On a copy of the install, a launcher run patches from Ambrose and the client reaches the configured login server
- [ ] A full run makes no connection to any KingsIsle host
- [ ] The pinned development install is unchanged

### Detailed spec: player launcher

The maintainer's direction on 2026-09-14: WizardLauncher.exe patches the client from KingsIsle's servers to their latest revision, which must never happen to the pinned development install. A separate copy may be patched that way as the user's own choice, on their own account and machine: fetching from KingsIsle's patch servers may break KingsIsle's terms, and the copy then leaves the revision Ambrose supports. Development keeps starting WizardGraphicalClient.exe directly through the launcher of milestone 3.25, which skips the retail launcher. Players get a launcher that patches from Ambrose instead.

**Deliverables**

- Static analysis of WizardLauncher.exe with Ghidra or radare2 on a copy of the file, never run against the pinned install: where it reads its patch server host and port (Bin/PatchConfig.xml, command-line arguments, the registry, or built-in addresses), every host and protocol it contacts (patch TCP service, HTTP file host, news pages, telemetry), and how it starts WizardGraphicalClient.exe and with which arguments. The findings go in doc/CLIENT.md as behavior notes, with no client code or data copied
- If the retail launcher takes its endpoints from data it reads: a launcher mode that writes the Ambrose endpoints into the player's own copy of those files at run time, refuses to start while any endpoint still names a KingsIsle host, and then starts WizardLauncher.exe. This mode does not modify client binaries
- Otherwise: a patching mode of the 3.25 launcher that runs the 16.04 and 16.05 patch protocol against the configured patch server, verifies and repairs files against the manifest with progress shown to the player, and starts WizardGraphicalClient.exe with the configured login server; settings come from launcher.conf
- Player documentation in doc/PATCHING.md for installing and running the launcher

**Acceptance**

- [ ] The notes list every host, port and file the retail launcher uses, with the addresses of the code that reads each one
- [ ] On a copy of a complete install, a launcher run reports 'Files altered: 0'; with one zone WAD deleted, the run restores it with a matching CRC; the client then reaches the login screen of the configured login server
- [ ] During a full run, the Windows Firewall log or a local capture shows connections only to the configured Ambrose hosts
- [ ] File hashes of the pinned development install match before and after the tests

**Risks**

- A modified KingsIsle executable is never distributed. Redirection comes from data the launcher reads, from Ambrose's own launcher, or, as an experimental opt-in feature planned as a follow-up milestone in this phase, from a patch the user applies locally to their own copy of WizardLauncher.exe (a binary diff, or a hook DLL of Ambrose's own code), never to the pinned development install and at the user's own risk
- The retail launcher may reach KingsIsle, for example for its news page, before or regardless of its configuration, which leaves the Ambrose launcher path or a local patch
- Cannot be verified in CI

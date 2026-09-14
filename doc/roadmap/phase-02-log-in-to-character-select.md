<!-- Project Ambrose by Imjustchico: Roadmap phase 2, Log in to character select. -->

# Phase 2: Log in to character select

**Done when:** The correct password lands on an empty character select screen. A wrong password shows the client's invalid-login dialog and allows a retry.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 2.01 | MySQLConnection and raw queries (FND-13) | M | 1.10 |
| 2.02 | Prepared statements (FND-14) | M | 2.01 |
| 2.03 | DatabaseWorkerPool (FND-15) | M | 2.02, 1.11 |
| 2.04 | Transactions, QueryCallback, DatabaseLoader (FND-16) | M | 2.03 |
| 2.05 | Updater: AutoSetup and base populate (FND-17 part 1) | M | 2.04, 1.12 |
| 2.06 | Updater: dated update application (FND-17 part 2) | M | 2.05 |
| 2.07 | dbimport (FND-20) | S | 2.06 |
| 2.08 | Pools wired into apps, DB appender (FND-21) | S | 1.20, 2.07 |
| 2.09 | Dispatch table, session states, LOGIN table (NET-9 + LOG-1) | M | 1.22, 1.16 |
| 2.10 | Outbound send API and base messages (NET-10) | S | 2.09 |
| 2.11 | Twofish-256 OFB (FND-8 + LOG-3 cipher) | M | 1.12 |
| 2.12 | Rec1, ClientKey1, PassKey3, session keys (LOG-3) | S | 2.11, 1.08 |
| 2.13 | db_login schema, AccountMgr, console account create (LOG-2) | M | 2.08 |
| 2.14 | Authentication: AUTHEN_V3 -> AUTHEN_RSP + ADMIT_IND (LOG-4) | M | 2.09, 2.10, 2.12, 2.13 |
| 2.15 | Login AFK timeout and shutdown notice (LOG-14) | S | 2.14 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 2.13 acceptance runs a console `account create test test` in loginserver, but the command/console framework (CommandMgr, 4.02) comes later and itself depends on 2.13. Either add a minimal console to 2.13 or 1.20, or move account creation after 4.02.

## 2.01 MySQLConnection and raw queries (FND-13)

**Goal:** Open a connection and read typed fields.

**Size:** M. **Depends on:** 1.10

**Acceptance**

- [x] Field '255' as TINYINT UNSIGNED gives Get<uint8>()==255
- [x] With AMBROSE_TEST_DB: SELECT 1; KILL CONNECTION_ID() then reconnect succeeds

### Detailed spec from FND-13: database: MySQL connector dependency and MySQLConnection with raw queries

The database layer can open a connection, run a query, and read typed fields.

**Deliverables**

- Decision point, settled in doc/ARCHITECTURE.md: MariaDB Connector/C from vcpkg (`libmariadb`, found with `find_package(unofficial-libmariadb)`), working with both MySQL 8 and MariaDB servers
- src/server/database/Database/MySQLConnection.h/.cpp: connection info from the string 'host;port;user;password;database' (AzerothCore convention, e.g. LoginDatabaseInfo), Open/Close, Execute(sql), Query(sql) -> QueryResult, escape, automatic reconnect on CR_SERVER_GONE_ERROR/CR_SERVER_LOST with backoff, SSL option
- src/server/database/Database/QueryResult.h/.cpp, Field.h/.cpp: typed getters Get<uint8..uint64,int*,float,double,std::string,std::vector<uint8>>, IsNull, with type-mismatch assertion logged to sql.sql
- src/server/database/Database/DatabaseEnvFwd.h
- database library target linking shared (empty for now) + common
- src/test/server/database/FieldTest.cpp (no server needed); DB integration tests gated by AMBROSE_TEST_DB connection string, GTEST_SKIP otherwise

**Acceptance**

- [x] Unit: Field built from the string '255' as TINYINT UNSIGNED gives Get<uint8>()==255; NULL field IsNull()
- [x] Integration (with AMBROSE_TEST_DB): SELECT 1 returns one row, one field == 1
- [x] Integration: killing the connection server-side (KILL CONNECTION_ID()) followed by a query reconnects and succeeds
- [x] Wrong password logs a sql.sql error with the MySQL error code and Open() returns the error
- [x] Real client: n/a

**Risks**

- Connector licensing (GPL libmysqlclient vs LGPL MariaDB C) for a private repo that may go public
- Windows install of the connector for contributors and CI

## 2.02 Prepared statements (FND-14)

**Goal:** Typed binding per database.

**Size:** M. **Depends on:** 2.01

**Acceptance**

- [ ] SELECT ? + ? with 3, 4 returns 7
- [ ] Blob with NUL round-trips
- [ ] A bad statement stops startup with 'Could not prepare statement LOGIN_...'

### Detailed spec from FND-14: database: prepared statements with typed binding

All SQL runs through registered, typed prepared statements per database.

**Deliverables**

- src/server/database/Database/PreparedStatement.h/.cpp: PreparedStatementBase with SetData(index, value) for bool/uint8..uint64/int8..int64/float/double/string/std::vector<uint8>/std::nullptr_t, template PreparedStatement<ConnectionType>
- MySQLPreparedStatement.h/.cpp: binds MYSQL_BIND, checks the parameter count against the placeholder count, stores the query text for logging
- PreparedQueryResult (binary protocol rows) with the same Field API
- Implementation/LoginDatabase.h/.cpp, CharacterDatabase.h/.cpp, WorldDatabase.h/.cpp: statement enums (LOGIN_SEL_UPDATES_EXAMPLE placeholder only) and DoPrepareStatements() with PrepareStatement(id, sql, CONNECTION_SYNC|CONNECTION_ASYNC)
- Fail-fast: any statement that fails to prepare aborts pool open with the id and SQL logged

**Acceptance**

- [ ] Integration: a prepared SELECT ? + ? with uint32 3 and 4 returns 7
- [ ] Binding a string with embedded NUL and a 1 MB blob round-trips byte-exactly
- [ ] Leaving a parameter unset makes Execute fail with a logged 'parameter N not bound' (assert in debug)
- [ ] A syntax error in a registered statement stops startup with 'Could not prepare statement LOGIN_...'
- [ ] Real client: n/a

**Risks**

- MYSQL_TYPE and bool handling differ between MySQL 8 and MariaDB connectors (my_bool removal)

## 2.03 DatabaseWorkerPool (FND-15)

**Goal:** Sync and async pools.

**Size:** M. **Depends on:** 2.02, 1.11

**Acceptance**

- [ ] 1000 AsyncQuery from 8 threads complete
- [ ] No concurrent use of one connection
- [ ] Close drains deterministically under ASan

### Detailed spec from FND-15: database: DatabaseWorkerPool with sync and async connections

Game code can run blocking queries on a small connection pool and queue async queries served by worker threads.

**Deliverables**

- src/server/database/Database/DatabaseWorkerPool.h/.cpp: Open(info, asyncThreads, syncThreads), Close, Execute/DirectExecute, Query/Query(stmt), AsyncQuery returning QueryCallback, KeepAlive ping timer
- DatabaseWorker.h/.cpp: thread draining a ProducerConsumerQueue<SQLOperation*>
- SQLOperation.h, AdhocStatement.h/.cpp, QueryHolder.h/.cpp (batch of prepared queries loaded async, used later for character login)
- DatabaseEnv.h/.cpp: extern LoginDatabase, CharacterDatabase, WorldDatabase
- MySQL server and client version checks at open (min version constant)

**Acceptance**

- [ ] Integration: 1000 AsyncQuery calls from 8 threads all complete, with results matching inputs
- [ ] Sync Query on a pool of 2 connections from 4 threads blocks correctly and never uses a connection concurrently (checked with CONNECTION_ID() per thread)
- [ ] Close() while async work is queued drains or cancels deterministically, with no leak under ASan
- [ ] KeepAlive keeps a connection past wait_timeout=5 set in the test session
- [ ] Real client: n/a

**Risks**

- Callback delivery threading: callbacks must run on the owning app's update thread, not DB workers (see FND-16)

## 2.04 Transactions, QueryCallback, DatabaseLoader (FND-16)

**Goal:** Atomic writes and callbacks on the update thread.

**Size:** M. **Depends on:** 2.03

**Acceptance**

- [ ] A unique-key violation rolls back to 0 rows
- [ ] Deadlock loser retries
- [ ] loginserver logs 'Opened database connection pool login: 1 async, 1 sync'

### Detailed spec from FND-16: database: transactions, QueryCallback chaining, DatabaseLoader

Multi-statement atomic writes and main-thread callback processing work, and apps open all their pools through one loader.

**Deliverables**

- Transaction.h/.cpp: Append(stmt or adhoc), CommitTransaction (async), DirectCommitTransaction, retry on deadlock (ER_LOCK_DEADLOCK) up to a time limit, TransactionCallback
- QueryCallback.h/.cpp with WithCallback/WithPreparedCallback and chaining; QueryCallbackProcessor.h/.cpp pumped from the app update loop
- src/server/database/Database/DatabaseLoader.h/.cpp: AddDatabase(pool, name) that queues open, prepare, and later the updater; Load() runs them in order and unwinds on failure
- Config options per app .conf.dist: LoginDatabaseInfo, CharacterDatabaseInfo, WorldDatabaseInfo, *.WorkerThreads, *.SynchThreads, MaxPingTime. They apply live on a config reload (through 4.15 when it lands): a changed connection string opens and validates a new pool, then swaps it in and drains the old one, and a failure keeps the old pool and logs the error; thread counts resize live; MaxPingTime applies from the next ping

**Acceptance**

- [ ] Integration: a transaction inserting 2 rows where the second violates a unique key leaves 0 rows
- [ ] Forced deadlock between two transactions: the loser retries and both finally commit
- [ ] A QueryCallback chain (query A then B using A's result) runs both callbacks on the processor thread (checked by thread id)
- [ ] loginserver with a bad LoginDatabaseInfo exits 1 with a clear error; with a valid one logs 'Opened database connection pool login: 1 async, 1 sync'
- [ ] Integration: changing LoginDatabaseInfo and reloading config on a running loginserver swaps to the new pool without dropping queued queries; an unreachable string keeps the old pool and logs the error
- [ ] Real client: n/a

## 2.05 Updater: AutoSetup and base populate (FND-17 part 1)

**Goal:** Create empty DBs and import base/.

**Size:** M. **Depends on:** 2.04, 1.12

**Acceptance**

- [ ] A fresh server gets the login DB created with base imported and updates/updates_include tables

### Detailed spec from FND-17: database/Updater part 1: base import and in-order update application

An empty database is created from base/ and then brought current by applying dated update files exactly once, recorded in `updates`.

**Deliverables**

- data/sql/base/db_{login,characters,world}/updates.sql and updates_include.sql: tables `updates` (name PK, hash CHAR(64) SHA-256 hex, state ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING'), timestamp, speed) and `updates_include` (path, state) seeded with $/data/sql/updates/db_<name> RELEASED and $/data/sql/custom/db_<name> CUSTOM
- src/server/database/Updater/DBUpdater.h/.cpp: Create database (if Updates.AutoSetup), Populate from base/ when the schema has no tables, Update(): read includes, list *.sql, validate names YYYY_MM_DD_NN.sql for RELEASED (any name for CUSTOM), sort, apply unapplied ones, record hash/state/speed
- UpdateFetcher.h/.cpp: file discovery and hashing (SHA-256 of file bytes with line endings normalized to LF)
- SQL file execution: split-free multi-statement execution via CLIENT_MULTI_STATEMENTS on a dedicated connection, or shelling out to the mysql CLI as AzerothCore does (decision), stopping on the first error with file and statement logged
- Config: Updates.EnableDatabases (bitmask login=1, characters=2, world=4), Updates.AutoSetup, Updates.SourcePath
- `db update` on a running server applies pending data-only update files live, then reloads the stores they touch (through 4.15 when it lands); a file with schema statements is refused, since schema updates the running binary needs are a documented restart case

**Acceptance**

- [ ] Integration: on a fresh server, loginserver startup creates the login DB, imports base, applies data/sql/updates/db_login/2026_01_01_00.sql, and a second start applies nothing ('database is up to date')
- [ ] A test update with a syntax error stops startup, names the file and error, and records no row for it
- [ ] Files are applied in name order even when created out of order on disk
- [ ] A CUSTOM file in custom/db_login is applied with state CUSTOM
- [ ] A badly named file in updates/ (e.g. 2026-1-1.sql) is refused with an error
- [ ] Real client: n/a

**Risks**

- CLIENT_MULTI_STATEMENTS error handling mid-file is tricky (must drain all result sets); the CLI route needs mysql on PATH
- A DELIMITER statement for stored procedures only works with the CLI

## 2.06 Updater: dated update application (FND-17 part 2)

**Goal:** Apply YYYY_MM_DD_NN.sql once, in order.

**Size:** M. **Depends on:** 2.05

**Acceptance**

- [ ] Second start logs 'database is up to date'
- [ ] A syntax error names the file and records no row
- [ ] Name order is respected; a CUSTOM file gets state CUSTOM; 2026-1-1.sql is refused

### Detailed spec from FND-17: database/Updater part 1: base import and in-order update application

An empty database is created from base/ and then brought current by applying dated update files exactly once, recorded in `updates`.

**Deliverables**

- data/sql/base/db_{login,characters,world}/updates.sql and updates_include.sql: tables `updates` (name PK, hash CHAR(64) SHA-256 hex, state ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING'), timestamp, speed) and `updates_include` (path, state) seeded with $/data/sql/updates/db_<name> RELEASED and $/data/sql/custom/db_<name> CUSTOM
- src/server/database/Updater/DBUpdater.h/.cpp: Create database (if Updates.AutoSetup), Populate from base/ when the schema has no tables, Update(): read includes, list *.sql, validate names YYYY_MM_DD_NN.sql for RELEASED (any name for CUSTOM), sort, apply unapplied ones, record hash/state/speed
- UpdateFetcher.h/.cpp: file discovery and hashing (SHA-256 of file bytes with line endings normalized to LF)
- SQL file execution: split-free multi-statement execution via CLIENT_MULTI_STATEMENTS on a dedicated connection, or shelling out to the mysql CLI as AzerothCore does (decision), stopping on the first error with file and statement logged
- Config: Updates.EnableDatabases (bitmask login=1, characters=2, world=4), Updates.AutoSetup, Updates.SourcePath
- `db update` on a running server applies pending data-only update files live, then reloads the stores they touch (through 4.15 when it lands); a file with schema statements is refused, since schema updates the running binary needs are a documented restart case

**Acceptance**

- [ ] Integration: on a fresh server, loginserver startup creates the login DB, imports base, applies data/sql/updates/db_login/2026_01_01_00.sql, and a second start applies nothing ('database is up to date')
- [ ] A test update with a syntax error stops startup, names the file and error, and records no row for it
- [ ] Files are applied in name order even when created out of order on disk
- [ ] A CUSTOM file in custom/db_login is applied with state CUSTOM
- [ ] A badly named file in updates/ (e.g. 2026-1-1.sql) is refused with an error
- [ ] Real client: n/a

**Risks**

- CLIENT_MULTI_STATEMENTS error handling mid-file is tricky (must drain all result sets); the CLI route needs mysql on PATH
- A DELIMITER statement for stored procedures only works with the CLI

## 2.07 dbimport (FND-20)

**Goal:** Create and update all 3 DBs without a server.

**Size:** S. **Depends on:** 2.06

**Acceptance**

- [ ] Creates ambrose_login/characters/world, exits 0; rerun says up to date
- [ ] A broken update exits 1 and names the file

### Detailed spec from FND-20: src/tools/dbimport

A standalone tool creates and updates all three databases without starting any server, for installs and CI.

**Deliverables**

- src/tools/dbimport/Main.cpp + CMakeLists.txt + dbimport.conf.dist (LoginDatabaseInfo, CharacterDatabaseInfo, WorldDatabaseInfo, Updates.*, Appender/Logger)
- Runs DatabaseLoader with open, AutoSetup, populate and update for each enabled DB, then exits: 0 on success, 1 on any failure
- CI job switched to dbimport for SQL validation

**Acceptance**

- [ ] Against an empty server, `dbimport` creates ambrose_login, ambrose_characters and ambrose_world, applies base and updates, exits 0; a second run logs up to date for all three
- [ ] A broken update exits 1 and names the file
- [ ] Real client: n/a

**Risks**

- ARCHITECTURE.md says tools depend only on shared and common, but dbimport must link the database layer; the doc needs an exception

## 2.08 Pools wired into apps, DB appender (FND-21)

**Goal:** Apps open DBs and run the updater.

**Size:** S. **Depends on:** 1.20, 2.07

**Acceptance**

- [ ] gameserver on an empty DB logs updater output for 3 DBs then 'ready'
- [ ] Appender.DB writes login.logs
- [ ] DB outage logs reconnect attempts

### Detailed spec from FND-21: database/Logging: DB appender, and wiring pools into the app skeletons

Apps open their databases and run the updater at startup, and log lines can be stored in the database.

**Deliverables**

- src/server/database/Logging/AppenderDB.h/.cpp: appender type 'DB' inserting into login.`logs` (time, realm id, type, level, text) through an async prepared statement, only enabled after pools open
- data/sql/updates/db_login/<date>_NN.sql creating `logs`
- loginserver opens LoginDatabase; gameserver opens Login, Character and World; patchserver opens Login (or none, per PAT decision). Each runs DBUpdater first when Updates.EnableDatabases allows
- Graceful shutdown closes pools after the io loop stops

**Acceptance**

- [ ] gameserver startup on an empty DB server logs updater output for all three databases, then 'ready'
- [ ] With `Appender.DB = 4,2,0` and `Logger.server.gameserver = 2,Console DB`, startup INFO lines appear in login.logs
- [ ] Stopping the DB server during run logs reconnect attempts rather than crashing
- [ ] Real client: n/a

**Risks**

- The DB appender must not recurse when SQL errors are themselves logged (sql.* must not route to the DB appender)

## 2.09 Dispatch table, session states, LOGIN table (NET-9 + LOG-1)

**Goal:** Route each message to one handler in the required state.

**Size:** M. **Depends on:** 1.22, 1.16

**Client messages:** MSG_USER_AUTHEN_V3, MSG_LOGIN_NOT_AFK, MSG_ATTACH, MSG_PING

**Acceptance**

- [ ] Wrong state is dropped with 'received <NAME> in state X'
- [ ] STATUS_NEVER strikes; MaxStrikes disconnects
- [ ] LOGIN table has 29 entries matching _MsgOrder
- [ ] Coverage check fails the build on a missing id
- [ ] Real client: MSG_USER_AUTHEN_V3 reaches LoginSession::HandleUserAuthenV3

### Detailed spec from NET-9: Message dispatch table and session states

Each incoming DML message is routed to exactly one Session::Handle<Message> member, only when the session is in the required state and on the correct thread.

**Deliverables**

- src/server/shared/Messages/MessageHandlerTable.h: template MessageHandlerTable<SessionT> indexed [service][order] -> {status: STATUS_NEVER|STATUS_CONNECTED|STATUS_AUTHED|STATUS_LOGGEDIN|STATUS_INWORLD, mode: PROCESS_INPLACE|PROCESS_THREADUNSAFE|PROCESS_ZONE, handler: void (SessionT::*)(Msg&)}. It is built from DEFINE_HANDLER(Msg, status, mode, &SessionT::HandleX) so the generated struct type supplies (service, order)
- src/server/game/Server/GameSession.h/.cpp with src/server/game/Server/Protocol/GameMessageTable.cpp. src/server/apps/loginserver/Server/LoginSession.h/.cpp with LoginMessageTable.cpp. src/server/apps/patchserver/Server/PatchSession.* with PatchMessageTable.cpp
- Every client->server message not yet implemented is registered STATUS_NEVER with HandleNULL, so unimplemented is explicit. Server->client-only messages are marked STATUS_NEVER with HandleServerSide
- Inbound queue: PROCESS_INPLACE runs on the network thread; others are queued and drained in Session::Update(diff) (world tick) or by the zone that owns the player
- Handler grouping per doc: src/server/game/Handlers/<Subsystem>Handler.cpp (empty stubs created only when a subsystem lands)
- Network.MaxStrikes is a live setting read at each strike, so a change applies to the next strike (registered with 4.16 when it lands)
- src/test/server/game/Server/MessageTableTest.cpp

**Client messages:** MSG_USER_AUTHEN_V3, MSG_LOGIN_NOT_AFK, MSG_ATTACH, MSG_PING

**Data sources**

- Generated registry (NET-4)

**Acceptance**

- [ ] Test: a message in the wrong state is dropped with a logged 'received <NAME> in state X', and the handler is not called
- [ ] Test: a STATUS_NEVER message increments a strike counter; Network.MaxStrikes (default 10) disconnects
- [ ] Test: a body that fails Decode is dropped, logged with the service/order name, and counts a strike
- [ ] Test: every (service, order) in MessageRegistry has an entry in each app's table, so a coverage check fails the build if an id is missing
- [ ] Real client: LOGIN MSG_USER_AUTHEN_V3 reaches LoginSession::HandleUserAuthenV3 (a stub that logs); a crafted GAME MSG_ATTACH (5:7) from a test client to loginserver is rejected by state

**Risks**

- The XML has no direction attribute, so the client->server / server->client split must be curated by hand per message, over 969 ids
- _MsgAccessLvl (present on only 14 messages, e.g. MSG_ATTACH=1, MSG_USER_AUTHEN*=1) is not proven to mean the same as session state; treat it as a hint only

### Detailed spec from LOG-1: loginserver app skeleton, session accept and LOGIN dispatch table

A real client started with -L 127.0.0.1 12000 connects to our loginserver, completes the session handshake, stays connected, and every LOGIN (service 7) message it sends reaches a named handler slot.

**Deliverables**

- src/server/apps/loginserver/Main.cpp: loads loginserver.conf, opens the login and characters DB pools, runs the acceptor and signal handling
- src/server/apps/loginserver/Server/LoginSocket.{h,cpp}: per-connection session built on the NET session (SessionOffer, SessionAccept, KeepAlive, KeepAliveRsp) that remembers SessionID, offer seconds and offer milliseconds for the crypto in LOG-3
- src/server/apps/loginserver/Server/LoginOpcodes.{h,cpp}: dispatch table of all 29 LOGIN messages by _MsgOrder (the ids come straight from LoginMessages.xml), each tagged with required state (Never / Authenticated / CharacterSelected) and a Handle<Message> member; unimplemented entries are logged and dropped, never fatal
- conf/dist/loginserver.conf.dist: BindIP, LoginServerPort=12000, LoginDatabaseInfo, CharacterDatabaseInfo, KeepAliveInterval, SessionAcceptTimeout, MaxConnections. BindIP and LoginServerPort rebind live, opening the new listener before closing the old one, and a failed bind keeps the old listener; MaxConnections, KeepAliveInterval and SessionAcceptTimeout apply from the next connection or timer; the database strings follow 2.04
- src/test/server/apps/loginserver/LoginOpcodesTest.cpp

**Client messages:** MSG_USER_AUTHEN_V3

**Data sources**

- Root.wad LoginMessages.xml (service 7, 29 messages with explicit _MsgOrder)
- Root.wad BaseMessages.xml, ExtendedBaseMessages.xml

**Acceptance**

- [ ] Unit: the dispatch table holds exactly 29 entries whose ids match _MsgOrder 1..29 and whose names match LoginMessages.xml (the test reads the XML from the user's install via the DAT WAD reader and skips when no install is configured)
- [ ] Unit: a message received in the wrong state (for example MSG_REQUESTCHARACTERLIST before authentication) is dropped and logged, and the session stays open
- [ ] Real client: WizardGraphicalClient.exe -L 127.0.0.1 12000 connects; the server log shows SessionOffer sent and SessionAccept received with a matching SessionID; the connection survives at least 2 minutes of keepalives; the client shows its login UI (no -U) or sits waiting for authentication, with no crash and no disconnect dialog
- [ ] Real client: MSG_USER_AUTHEN_V3 (order 27) arrives, is decoded field by field (Version=W.1.610.x, Revision=r806919.Wizard_1_610, Locale, MachineID) and logged

**Risks**

- The session-offer fields for the packet encryption added in 2021 are not understood; the reference server gets by without them (Imlight docs kinp/controlmessages.md). If a newer revision requires them, login is blocked until NET reverse-engineers them.
- Service 2 (EXTENDEDBASE) has no _MsgOrder, so its ids come from sorting names; MSG_SERVERMESSAGE was observed as order 6. NET must own that rule.

## 2.10 Outbound send API and base messages (NET-10)

**Goal:** SendMessage, ping reply, server message, kick.

**Size:** S. **Depends on:** 2.09

**Client messages:** MSG_PING, MSG_PING_RSP, MSG_SERVERMESSAGE, MSG_FORCE_DISCONNECT, MSG_RAW_TEXT, MSG_CUSTOMDICT, MSG_CUSTOMRECORD, MSG_RAWRECORD

**Acceptance**

- [ ] MSG_SERVERMESSAGE frame is service 2 order 6 with a correct WSTR
- [ ] KickPlayer keeps the socket open until MSG_FORCE_DISCONNECT flushes

### Detailed spec from NET-10: Outbound send API and base-service messages

Game code sends any generated message with one call, and the server can show a message to or kick a client.

**Deliverables**

- SessionBase::SendMessage<T>(T const&): encode into a pooled buffer, frame, and queue. SendMessageDelayedClose<T> for final messages
- src/server/shared/Network/SystemMessages.cpp: MSG_PING handled inplace by replying MSG_PING_RSP
- Helpers: Session::SendServerMessage(std::u16string const&, bool modal) via EXTENDEDBASE MSG_SERVERMESSAGE (2:6, Modal UBYT, Message WSTR). Session::KickPlayer(uint32 type, std::string reason) via MSG_FORCE_DISCONNECT (2:3, Type UINT, TimeStamp STR, Message STR) then DelayedClose
- MSG_RAW_TEXT, MSG_CUSTOMDICT, MSG_CUSTOMRECORD and MSG_RAWRECORD registered STATUS_NEVER until a use is found

**Client messages:** MSG_PING, MSG_PING_RSP, MSG_SERVERMESSAGE, MSG_FORCE_DISCONNECT, MSG_RAW_TEXT, MSG_CUSTOMDICT, MSG_CUSTOMRECORD, MSG_RAWRECORD

**Data sources**

- Generated registry

**Acceptance**

- [ ] Unit: SendMessage of MSG_SERVERMESSAGE produces a frame with service 2, order 6 and a correct WSTR body
- [ ] Unit: KickPlayer leaves the socket open until MSG_FORCE_DISCONNECT is flushed
- [ ] Real client (once a character is in the world, or at the earliest stage it renders): a GM command (cs_server 'announce' or similar, owned by EXT/WLD) sends MSG_SERVERMESSAGE and the client visibly shows the text; 'kick' sends MSG_FORCE_DISCONNECT and the client shows its disconnect dialog instead of a silent 'connection lost'

**Risks**

- Whether the client sends MSG_PING, and what it does with MSG_SERVERMESSAGE before entering the world, is unverified

## 2.11 Twofish-256 OFB (FND-8 + LOG-3 cipher)

**Goal:** Cipher for Rec1, written from the spec.

**Size:** M. **Depends on:** 1.12

**Acceptance**

- [ ] Twofish 256-bit (and 128-bit) KATs pass
- [ ] OFB identity for 0, 1, 15, 16, 17 and 1000 bytes

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

### Detailed spec from LOG-3: KI login crypto primitives: Rec1, ClientKey1, PassKey3, session keys

The server can decrypt and encrypt Rec1 and verify ClientKey1 and PassKey3 exactly as the client computes them, all under unit test.

**Deliverables**

- src/server/shared/Cryptography/Twofish.{h,cpp} or a wrapper around the chosen library: Twofish-256 in OFB mode, no padding
- src/server/shared/Cryptography/Rec1.{h,cpp}: key = bytes 0x17+i for i in 0..31, with key[4]=sid&0xFF, key[5]=0, key[6]=sid>>8, key[8]=secs&0xFF, key[9]=secs>>16, key[12]=secs>>8, key[13]=secs>>24, key[14]=ms&0xFF, key[15]=ms>>8; IV = bytes 0xB6-i for i in 0..15; Decode and Encode
- src/server/shared/Cryptography/ClientKey.{h,cpp}: VerifyCK1(verifier, sid, secs, ms, ck1), which checks ck1 == base64(SHA-512(verifier || ascii("{sid}{secs}{ms}"))); GenerateSessionKey(), which returns base64(SHA-256(random || salt)) as 44 characters
- src/server/shared/Cryptography/PassKey3.{h,cpp}: base64(SHA-512(sessionKey || ascii("{sid}{secs}{ms}")))
- src/test/server/shared/Cryptography/{Rec1Test,ClientKeyTest,PassKey3Test}.cpp

**Data sources**

- Formulas studied from Imlight src/Imlight.CoreLib/Shared/Cryptography/{Rec1,ClientKey,PassKey3}.cs and docs/docs/modules/internals/auth/{auth,validation}.md (behavior only, reimplemented from the description)

**Acceptance**

- [ ] Unit: Twofish passes the published Twofish 256-bit known-answer vectors
- [ ] Unit: the Rec1 key and IV derivation for sid=0x1234, secs=0xAABBCCDD, ms=0x0123 yields the exact byte layout above, and Encode followed by Decode round-trips arbitrary lengths (OFB, no padding, output length equals input length)
- [ ] Unit: VerifyCK1 accepts a vector computed independently in a test script from the formula and rejects a one-character change in the password, the sid or the milliseconds
- [ ] Unit: PassKey3 output is 88 base64 characters, matching the 88-byte PassKey3 seen in the capture

**Risks**

- Pending stack decision: OpenSSL has SHA and base64 but no Twofish. The options are Botan, Crypto++, or our own implementation from the public Twofish spec, and the choice affects deps/ and vcpkg.
- The number formatting in the salt (decimal, no separators, milliseconds not zero-padded) is inferred from the reference; if it is wrong, every login fails. Confirm with a real client in LOG-4.

## 2.12 Rec1, ClientKey1, PassKey3, session keys (LOG-3)

**Goal:** Login crypto matches the client.

**Size:** S. **Depends on:** 2.11, 1.08

**Acceptance**

- [ ] Key/IV derivation for sid=0x1234 secs=0xAABBCCDD ms=0x0123 matches the layout
- [ ] VerifyCK1 accepts an independent vector and rejects a 1-char change
- [ ] PassKey3 is 88 base64 chars

### Detailed spec from LOG-3: KI login crypto primitives: Rec1, ClientKey1, PassKey3, session keys

The server can decrypt and encrypt Rec1 and verify ClientKey1 and PassKey3 exactly as the client computes them, all under unit test.

**Deliverables**

- src/server/shared/Cryptography/Twofish.{h,cpp} or a wrapper around the chosen library: Twofish-256 in OFB mode, no padding
- src/server/shared/Cryptography/Rec1.{h,cpp}: key = bytes 0x17+i for i in 0..31, with key[4]=sid&0xFF, key[5]=0, key[6]=sid>>8, key[8]=secs&0xFF, key[9]=secs>>16, key[12]=secs>>8, key[13]=secs>>24, key[14]=ms&0xFF, key[15]=ms>>8; IV = bytes 0xB6-i for i in 0..15; Decode and Encode
- src/server/shared/Cryptography/ClientKey.{h,cpp}: VerifyCK1(verifier, sid, secs, ms, ck1), which checks ck1 == base64(SHA-512(verifier || ascii("{sid}{secs}{ms}"))); GenerateSessionKey(), which returns base64(SHA-256(random || salt)) as 44 characters
- src/server/shared/Cryptography/PassKey3.{h,cpp}: base64(SHA-512(sessionKey || ascii("{sid}{secs}{ms}")))
- src/test/server/shared/Cryptography/{Rec1Test,ClientKeyTest,PassKey3Test}.cpp

**Data sources**

- Formulas studied from Imlight src/Imlight.CoreLib/Shared/Cryptography/{Rec1,ClientKey,PassKey3}.cs and docs/docs/modules/internals/auth/{auth,validation}.md (behavior only, reimplemented from the description)

**Acceptance**

- [ ] Unit: Twofish passes the published Twofish 256-bit known-answer vectors
- [ ] Unit: the Rec1 key and IV derivation for sid=0x1234, secs=0xAABBCCDD, ms=0x0123 yields the exact byte layout above, and Encode followed by Decode round-trips arbitrary lengths (OFB, no padding, output length equals input length)
- [ ] Unit: VerifyCK1 accepts a vector computed independently in a test script from the formula and rejects a one-character change in the password, the sid or the milliseconds
- [ ] Unit: PassKey3 output is 88 base64 characters, matching the 88-byte PassKey3 seen in the capture

**Risks**

- Pending stack decision: OpenSSL has SHA and base64 but no Twofish. The options are Botan, Crypto++, or our own implementation from the public Twofish spec, and the choice affects deps/ and vcpkg.
- The number formatting in the salt (decimal, no separators, milliseconds not zero-padded) is inferred from the reference; if it is wrong, every login fails. Confirm with a real client in LOG-4.

## 2.13 db_login schema, AccountMgr, console account create (LOG-2)

**Goal:** Accounts exist and operators can create them.

**Size:** M. **Depends on:** 2.08

**Acceptance**

- [ ] verifier == base64(SHA-512(utf8(password))), 88 chars; duplicate username rejected case-insensitively
- [ ] `account create test test` inserts once; rerun says it exists

### Detailed spec from LOG-2: db_login base schema, LoginDatabase statements and account creation

Accounts exist in MySQL and an operator can create one with a password, so authentication has something to check against.

**Deliverables**

- data/sql/base/db_login/: account (id BIGINT UNSIGNED PK, username VARCHAR(32) UNIQUE, verifier CHAR(88) = base64(SHA-512(password)), email, security_level TINYINT, chat_mode TINYINT, locked TINYINT, purchased_slots INT, online TINYINT, joindate, last_login, last_ip, last_machine_id BIGINT UNSIGNED), account_banned (account_id, bandate, unbandate, bannedby, reason, active), ip_banned, machine_banned (machine_id, ...), updates, updates_include
- src/server/database/Implementation/LoginDatabase.{h,cpp}: prepared statements LOGIN_SEL_ACCOUNT_BY_NAME, LOGIN_SEL_ACCOUNT_BY_ID, LOGIN_INS_ACCOUNT, LOGIN_UPD_LAST_LOGIN, LOGIN_SEL_BANS (account, ip, machine)
- src/server/game/Accounts/AccountMgr.{h,cpp} (sAccountMgr): CreateAccount, ChangePassword, SetSecurityLevel, Ban/Unban; username normalization and length rules
- src/server/apps/loginserver/Console: a minimal `account create <user> <pass>` and `account set gmlevel` console so login can be tested before the gameserver exists
- src/test/server/game/Accounts/AccountMgrTest.cpp

**Database tables**

- account
- account_banned
- ip_banned
- machine_banned
- updates
- updates_include

**Acceptance**

- [ ] Unit: CreateAccount stores verifier == base64(SHA-512(utf8(password))), an 88-character string, and rejects a duplicate username case-insensitively
- [ ] Unit: the updater applies data/sql/updates/db_login files in order and records them in updates
- [ ] Manual: `account create test test` on the loginserver console inserts one row, and a second run reports that the account exists

**Risks**

- The verifier the client protocol needs (base64 SHA-512 of the password, unsalted) is password-equivalent: anyone who reads db_login can log in as any user. This is forced by the client's ClientKey1 scheme, so db_login needs strict access control. Maintainer decision.
- Layering: loginserver (an app) linking game/Accounts is allowed by apps -> game, but it drags the game library into the loginserver. The alternative is an AccountMgr kept in apps/loginserver plus a copy for cs_account. Needs a decision.

## 2.14 Authentication: AUTHEN_V3 -> AUTHEN_RSP + ADMIT_IND (LOG-4)

**Goal:** Valid credentials reach character select; bad ones are refused cleanly.

**Size:** M. **Depends on:** 2.09, 2.10, 2.12, 2.13

**Client messages:** MSG_USER_AUTHEN_V3, MSG_USER_AUTHEN_RSP, MSG_USER_ADMIT_IND, MSG_USER_AUTHEN, MSG_USER_AUTHEN_V2, MSG_WEB_AUTHEN, MSG_WEB_VALIDATE, MSG_SERVERMESSAGE

**Acceptance**

- [ ] Unit: wrong sid, wrong CK1, banned machine, locked account each give their error and no session row
- [ ] Real client: correct password reaches empty character select; log shows AUTHEN_RSP Error=0 and ADMIT_IND Status=1
- [ ] Real client: wrong password shows the invalid-login dialog and allows retry
- [ ] Banned account is refused visibly

### Detailed spec from LOG-4: Authentication: MSG_USER_AUTHEN_V3 -> MSG_USER_AUTHEN_RSP + MSG_USER_ADMIT_IND

A real client with valid credentials is authenticated and admitted to character select, and bad credentials, bans and revision mismatches are refused cleanly.

**Deliverables**

- src/server/apps/loginserver/Handlers/AuthHandler.cpp: HandleUserAuthenV3 decrypts Rec1 with the session's offer values and parses the plaintext as 'sid username ck1' (split on spaces; exactly 3 parts; sid must equal this session's id), then checks optional revision enforcement (Login.EnforceRevision, Login.AllowedRevision), account existence, account, IP and machine bans (MachineID GID), locked accounts, and CK1
- On success: generate a session key, store it in account_session, update last_login/last_ip/last_machine_id, and send MSG_USER_AUTHEN_RSP{Error=0, UserID=account id, Rec1=Rec1.Encode(sessionKey), Reason='', TimeStamp='', PayingUser=1, Flags=0, SupportID='', PublicPlayerName=''} then MSG_USER_ADMIT_IND{Status=1, PositionInQueue=0}; mark the session Authenticated
- On failure: MSG_USER_AUTHEN_RSP{Error=<code>, Reason=<text>} and keep the socket open for a retry, closing it after N failures (config Login.MaxAuthAttempts) with an IP lockout lasting Login.LockoutSeconds, like AzerothCore's WrongPass policy
- Duplicate login policy (config Login.DuplicateLoginPolicy): an account already marked online either rejects with Error=AuthenFailed plus MSG_SERVERMESSAGE, or kicks the old session
- Login.EnforceRevision, Login.AllowedRevision, Login.MaxAuthAttempts, Login.LockoutSeconds and Login.DuplicateLoginPolicy are live settings read on each attempt, so a change applies without a restart (registered with 4.16 when it lands)
- MSG_USER_AUTHEN (13), MSG_USER_AUTHEN_V2 (22), MSG_WEB_AUTHEN (24) and MSG_WEB_VALIDATE (25) are answered with an AUTHEN_RSP failure and logged
- data/sql/updates/db_login/<date>_01.sql: account_session (account_id PK, machine_id, session_key CHAR(44), created, expires)
- src/server/apps/loginserver/Server/AuthResult.h: error code constants

**Client messages:** MSG_USER_AUTHEN_V3, MSG_USER_AUTHEN_RSP, MSG_USER_ADMIT_IND, MSG_USER_AUTHEN, MSG_USER_AUTHEN_V2, MSG_WEB_AUTHEN, MSG_WEB_VALIDATE, MSG_SERVERMESSAGE

**Data sources**

- Sniffer capture traffic.log lines 1-3 (observed field values)
- Imlight src/Imlight.CoreLib/Auth/UserAuthenticator.cs and Login/Services/AuthenticatorService.cs (behavior only)

**Database tables**

- account
- account_session
- account_banned
- ip_banned
- machine_banned

**Acceptance**

- [ ] Unit: a fake session with known sid, secs and ms, fed a Rec1 built by the test from 'sid user ck1', authenticates; a wrong sid in the plaintext, a wrong CK1, a banned machine id and a locked account each give the expected error code and no session row
- [ ] Unit: changing Login.MaxAuthAttempts on a running loginserver applies to the next attempt without a restart
- [ ] Real client (in-client login UI, no -U): the correct password moves to the character select screen (empty list for a new account) with no error dialog; the server log shows AUTHEN_V3, AUTHEN_RSP Error=0 and ADMIT_IND Status=1, matching capture lines 1-3
- [ ] Real client: a wrong password shows the client's invalid-login dialog and allows a retry without restarting the client
- [ ] Real client: an account banned via account_banned is refused with a visible message and never reaches character select
- [ ] Sniffer (optional): with the proxy in front, the AUTHEN_RSP Rec1 decrypts to a 44-character base64 key

**Risks**

- Error code values (AuthenFailed=0x3B689180, AccountBanned=0x538FBC0, MachineBanned=0x44FB7BF8, Timeout=0x512C42FF, ErrorNoLock=0x67DD13EA, ...) come from the reference and look like string-ID hashes; which client dialog each one shows is unverified.
- How the client builds CK1 on the -U '..<id> <PasswordHash> <user>' launch path, as opposed to the in-client login UI, is unverified; the two may differ in whether the client hashes the password itself.
- Flags (account permission bits such as chat and gifting) are sent as 0 in the capture; their meanings are unverified.

## 2.15 Login AFK timeout and shutdown notice (LOG-14)

**Goal:** Polite idle drop and shutdown notice.

**Size:** S. **Depends on:** 2.14

**Client messages:** MSG_LOGIN_NOT_AFK, MSG_DISCONNECT_LOGIN_AFK, MSG_LOGINSERVERSHUTDOWN

**Acceptance**

- [ ] Fake clock: idle session closed; MSG_LOGIN_NOT_AFK every 30 s keeps it
- [ ] Real client: AFK message rather than connection-lost; shutdown notice shown

### Detailed spec from LOG-14: Login AFK timeout and graceful shutdown notice

Idle clients at the login screen are dropped politely, and a login server shutdown tells connected clients instead of silently cutting them off.

**Deliverables**

- LoginSocket idle tracking: MSG_LOGIN_NOT_AFK (BadgeNameID ignored) and any other client message reset the timer; after Login.AfkTimeout send MSG_DISCONNECT_LOGIN_AFK{Warning} and close; the timer is suspended once CharacterSelected
- Shutdown: on SIGINT or console `server shutdown`, send MSG_LOGINSERVERSHUTDOWN{Message} to every session, then close after a grace delay
- conf/dist/loginserver.conf.dist: Login.AfkTimeout, Login.AfkWarning, Login.ShutdownGrace. They are live settings read at each timer check, so a change applies without a restart (registered with 4.16 when it lands)

**Client messages:** MSG_LOGIN_NOT_AFK, MSG_DISCONNECT_LOGIN_AFK, MSG_LOGINSERVERSHUTDOWN

**Acceptance**

- [ ] Unit: with a fake clock, a session with no traffic for AfkTimeout seconds is closed, while one sending MSG_LOGIN_NOT_AFK every 30s is kept
- [ ] Unit: with a fake clock, lowering Login.AfkTimeout on a running loginserver closes an idle session at the new timeout
- [ ] Real client: leave the client on character select past the timeout; it shows the client's AFK disconnect message rather than a generic connection-lost error
- [ ] Real client: stopping the loginserver while on character select shows a server-shutdown notice

**Risks**

- The Warning byte (warn versus disconnect) and the Message UINT for shutdown are both unverified; the reference sends Warning=1 with a '???' note.

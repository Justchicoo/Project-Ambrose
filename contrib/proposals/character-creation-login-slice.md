<!-- Project Ambrose by Imjustchico: Handoff proposal for the first authenticated character-creation slice, its current implementation draft, verification status and maintainer review checklist. -->
# Character creation login slice

## Purpose

This proposal documents the first implementation slice beyond character listing: an authenticated client sends `MSG_CREATECHARACTER`, Ambrose decodes the `WizardCharacterCreationInfo` object, creates an account-owned character and stores its appearance for later listing.

The current working draft is in the login-server source files on the contributor branch. It must be reviewed and moved to the maintainer's active phase branch before it can be merged, because the contributor track cannot contain changes under `src/`.

## Why this slice

The repository already has the necessary foundations:

- `MSG_CREATECHARACTER` is present in the ObjectProperty field table as an unwrapped `WizardCharacterCreationInfo`.
- `CharacterRepository::Create()` persists the character row, appearance row and GUID high-water mark in one transaction.
- `LoginScreenInfoBuilder` and the typed `WizardCharacterCreationInfoView` define the fields used by the character-select flow.
- The login harness can authenticate a loopback client and load the project's synthetic character type dump without client-derived files.

This makes creation the smallest visible step that extends the existing login and character-list path.

## Draft implementation

The current uncommitted draft changes these files:

- `src/server/apps/loginserver/Server/LoginMessages.h` declares `LoginMessages::CreateCharacter` with its `CreationInfo` field.
- `src/test/mocks/LoginMessageFixtures.h` declares the message order used by the test catalog.
- `src/server/apps/loginserver/Server/LoginMessageTable.cpp` accepts the message for authenticated sessions.
- `src/server/apps/loginserver/Server/LoginSession.h` exposes the handler and creation operation.
- `src/server/apps/loginserver/Handlers/CharacterHandler.cpp` decodes the object, copies identity, location and appearance fields into `CharacterSummary`, allocates the next GUID and calls `CharacterRepository::Create()`.
- `src/test/server/apps/loginserver/Handlers/CharacterHandlerTest.cpp` builds a valid creation blob, sends it after authentication and checks the persisted row.

The draft deliberately uses the authenticated session account instead of trusting `m_userID` from the client object. It also allocates the GUID while holding a process-local creation lock, so concurrent login sessions do not select the same high-water mark.

## Maintainer review checklist

Before accepting or adapting the draft, review these points against the phase acceptance checks and a retail-client capture:

- Define the server response message and error semantics for success, invalid data, no slots and database failure. The current draft logs failures but does not send `MSG_CREATECHARACTERRESPONSE`.
- Confirm the retail message order and all response fields from the user's own message definitions.
- Replace or strengthen process-local GUID allocation if multiple login-server processes or realms can create characters concurrently.
- Validate purchased character slots before insertion and define the behavior for deleted characters and duplicate names.
- Decide whether server-side name validation must use `CharacterNameMgr` and whether custom names and generated name indices are mutually exclusive.
- Confirm the default starting world, zone, level, timestamps and appearance values from the phase's real-client acceptance check.
- Add malformed-blob, missing-account, closed-database, full-slots and invalid-name cases.
- Add the real-client check to the relevant phase file only after the maintainer accepts this handoff.
- Add the required commit attribution trailer when the source changes are committed.

## Verification performed

The editor diagnostics report no errors in the touched C++ and test files. `git diff --check` passes.

A full CMake configure/build was attempted with the repository's `windows-msvc-x64` preset. It could not start because vcpkg reported that it could not find a valid Visual Studio instance. Therefore the integration test has not been executed in this environment, and no runtime success is claimed.

## Disproof and next step

This proposal is disproved or incomplete if a retail capture shows a different request shape, if the client requires a response before leaving the creation screen, if the type dump stores any copied field under a different type, or if concurrent creation can occur across processes. The next maintainer action is to move the source draft into the appropriate phase branch, settle the response contract from the maintainer's client, then run the database-backed test and the real-client create/list/restart check.

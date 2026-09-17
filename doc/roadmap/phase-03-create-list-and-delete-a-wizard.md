<!-- Project Ambrose by Imjustchico: Roadmap phase 3, Create, list and delete a wizard. -->

# Phase 3: Create, list and delete a wizard

**Done when:** A player creates a wizard (school, look, name). It appears on character select with the right appearance, persists across restarts, and can be deleted.

| ID | Milestone | Size | Depends on |
|---|---|---|---|
| 3.01 | KI string hash and property hash (OBJ-2, absorbs LOG-7 StringId) | S | 1.01 |
| 3.02 | SerializerBinary blob envelope (OBJ-3) | S | 1.07, 1.13 |
| 3.03 | Type dump loader and TypeRegistry (OBJ-4) | M | 3.01 |
| 3.04 | Dynamic property object model (OBJ-5) | M | 3.03 |
| 3.05 | Compact network codec (OBJ-8) | M | 3.04, 3.02 |
| 3.06 | Hostile-input hardening and fuzzing (OBJ-19) | S | 3.05 |
| 3.07 | Typed wrappers over dynamic objects (OBJ-10) | M | 3.04 |
| 3.08 | db_characters schema, CharacterRepository, GuidGenerator (LOG-5) | M | 2.08 |
| 3.09 | Character list (LOG-6) | M | 2.14, 3.08, 3.05 |
| 3.10 | Versionable decode core (OBJ-6 part 1) | M | 3.04, 3.02 |
| 3.11 | BINd files, bindecode CLI, corpus sweep (OBJ-6 part 2) | M | 3.10, 1.13 |
| 3.12 | Text XML ObjectProperty reader (OBJ-13) | S | 3.04, 1.13 |
| 3.13 | Locale .lang loader and localetool (OBJ-14 + QST-1) | S | 1.08, 1.13 |
| 3.14 | Name tables and creation config extractor (LOG-7) | M | 3.11, 3.12, 3.13, 2.07 |
| 3.15 | CreationInfo decode and validation (LOG-8 part 1) | M | 3.09, 3.14, 3.06 |
| 3.16 | Character creation persist and real-client flow (LOG-8 part 2) | M | 3.15 |
| 3.17 | Character deletion (LOG-9) | S | 3.09 |
| 3.18 | Updater part 2: rehash, rename, dead refs, pending, modules (FND-18) | M | 2.06 |
| 3.19 | CI pending SQL promotion and SQL validation (FND-19) | S | 1.04, 3.18, 2.07 |
| 3.20 | Find client data on the user's machine and guided setup | M | 3.13, 3.14, 1.08, 2.07 |
| 3.21 | Type data from the user's own client program | L | 3.03, 3.20, 1.13 |
| 3.22 | Automatic first-run setup | M | 3.21 |
| 3.23 | Keep up with KingsIsle's client revisions | M | 3.22 |

## Review notes for this phase

The roadmap critic flagged these. Resolve each one before or while implementing the milestones it names.

- **Ordering.** 3.16 creates a 'level-1 wizard', but no milestone ever grants the starting kit (spells, deck item, starter gear, potions, start zone/quest). Spellbook (8.05), decks (8.11) and equip (8.10) never go back to creation, and 14.02 only mentions 'playercreateinfo' in passing. A new character reaching the first duel in 9.07 would have no deck.
- **Oversized.** 3.11 BINd files, bindecode CLI and 134k-entry corpus sweep (M). Split the decoder/CLI from the sweep and its unknown-class triage.
- **Correction.** 3.12 cites 'CharacterCreationConfig.xml'. The Root.wad path is CharacterCreation/CharacterCreationConfig.xml (plain XML, root class 'class WizCharacterCreationConfig').

## 3.01 KI string hash and property hash (OBJ-2, absorbs LOG-7 StringId)

**Goal:** Client-identical constexpr hashes.

**Size:** S. **Depends on:** 1.01

**Acceptance**

- [x] KiStringHash("class Duel")==85019234 (verified; StringHashTest, also as a static_assert)
- [x] PropertyHash("class SharedPointer<class CombatParticipant>","m_flatParticipantList")==3375244498
- [x] StringId('Fire')==2343174, 'Ice'==72777, 'Balance'==1027491821 (verified; the string ID is the KI string hash itself)
- [x] Client-gated: all 6981 class and 49461 property hashes match (TypeDumpHashClientTest against the r806919 dump)

### Detailed spec from OBJ-2: KI string hash and property hash

The server computes class, property, and state-name hashes identical to the client's, including at compile time.

**Deliverables**

- src/common/Cryptography/StringHash.h: constexpr KiStringHash(std::string_view) (XOR of (c-32) shifted by 5 per char with wraparound, then absolute value) and Djb2
- constexpr PropertyHash(typeName, propName) = KiStringHash(type) + (Djb2(name) & 0x7FFFFFFF), mod 2^32
- src/test/common/Cryptography/StringHashTest.cpp

**Acceptance**

- [x] Unit test: KiStringHash("class Duel") == 85019234 and PropertyHash("class SharedPointer<class CombatParticipant>", "m_flatParticipantList") == 3375244498 (values derived from strings; no client file committed)
- [x] Unit test: a static_assert on one hash proves constexpr evaluation
- [x] Client-gated integration test (runs only when AMBROSE_TYPEDUMP is set): all 6981 class hashes and 49461 property hashes in the user's dump match. I confirmed this 100% in Python against r806919 (TypeDumpHashClientTest in client_tests; the variable is named AMBROSE_TYPE_DUMP_PATH, matching the environment name of the TypeDumpPath option that 3.03 adds)

### Detailed spec from LOG-7: Character name tables and creation config extraction

The server knows the valid first, middle and last name index ranges per gender, the disallowed combinations, and the allowed schools, all read from the user's own client install.

**Deliverables**

- src/tools/extractor (name module): reads Root.wad CharacterNames.xml (tables FirstName_HumanMale, FirstName_HumanFemale, MiddleName_Human, LastName_Human; the per-locale copies list the same keys, so take one), Locale/en-US/CharacterNames.lang (UTF-16 key/blank/text triplets), CharacterNamesDisallowedList.xml (a BINd ObjectProperty file, not zlib-wrapped here) and CharacterCreation/CharacterCreationConfig.xml (WizCharacterCreationConfig: the allowed schools Fire, Ice, Storm, Life, Myth, Death, Balance)
- The extractor writes world DB rows: character_name_part (table_name, idx, locale_key, text_en), character_name_disallowed, character_create_school (school_name, school_id = KI string-ID hash)
- data/sql/base/db_world/: the empty table definitions only (no extracted rows committed)
- src/server/game/Characters/CharacterNameMgr.{h,cpp} (sCharacterNameMgr): IsValidIndices(nameIndices, gender), FormatName(nameIndices, gender), IsDisallowed(). `.reload character_name` (through 4.15 when it lands) rebuilds the name parts and disallowed list off to the side, validates them, swaps, and keeps the old tables on failure
- src/server/shared/Util/StringId.{h,cpp}: the KI string-ID hash, if OBJ has not already provided it
- src/test/server/game/Characters/CharacterNameMgrTest.cpp

**Data sources**

- Root.wad CharacterNames.xml (315130 bytes)
- Root.wad Locale/<locale>/CharacterNames.lang
- Root.wad CharacterNamesDisallowedList.xml (BINd header, flags 0x07)
- Root.wad CharacterCreation/CharacterCreationConfig.xml
- Root.wad CharacterCreation.xml (quiz, client-side only; not needed by the server)

**Database tables**

- character_name_part
- character_name_disallowed
- character_create_school

**Acceptance**

- [x] Unit: StringId('Fire') == 2343174, StringId('Ice') == 72777 and StringId('Balance') == 1027491821, matching the reference enum values (StringHashTest; StringHash::StringId in src/common/Cryptography/StringHash.h replaces the separate StringId files)
- [x] Unit: FormatName with middle=0 and last=0 returns only the first name, and out-of-range indices are rejected (built in 3.14, which repeats this check: CharacterNamesTest)
- [x] Unit: reloading sCharacterNameMgr applies an edited character_name_part row, and a reload with an invalid row keeps the old tables and reports it (built in 3.14, which repeats this check: CharacterNameMgrDatabaseTest)
- [x] Tool run against the local install fills character_name_part with non-zero counts for all 4 tables and exactly 7 character_create_school rows; git status shows no new data files (built in 3.14, which repeats this check: the Extractor CTest and CharacterNameExtractorClientTest)

**Risks**

- The internal layout of CharacterNamesDisallowedList.xml (BINd, version 7) is not yet decoded; it needs the OBJ reader for BINd files.
- Whether the client sends nameIndices built from the locale-specific table or the shared one is unverified.

## 3.02 SerializerBinary blob envelope (OBJ-3)

**Goal:** 4-byte stored/zlib wrapper.

**Size:** S. **Depends on:** 1.07, 1.13

**Acceptance**

- [x] Stored wrap of 10 bytes has header 0x8000000A (BlobEnvelopeTest)
- [x] Size mismatch rejected; oversize rejected without allocating (BlobEnvelopeTest, counting allocations)

### Detailed spec from OBJ-3: Zlib and SerializerBinary blob envelope

ObjectProperty blobs can be packed and unpacked in the 4-byte envelope that the client runs on many message string fields before parsing them.

**Deliverables**

- src/common/Compression/Zlib.h/.cpp: inflate/deflate (RFC1950) with a caller-supplied maximum output size to block decompression bombs. Already built in 1.13 as src/common/Utilities/Compression.{h,cpp}, which caps output while streaming, so the envelope uses it
- src/server/shared/ObjectProperty/BlobEnvelope.h/.cpp: Wrap(bytes, Compress|Store) and Unwrap. The header is u32: with bit31 set it is stored and the low 31 bits are the length; with bit31 clear it holds the uncompressed size and zlib data follows. Built with the header little-endian, Unwrap taking the caller's cap, and a status that tells a truncated blob, a length above the cap, a length that disagrees with the payload, and a corrupt stream apart
- src/test/server/shared/ObjectProperty/BlobEnvelopeTest.cpp

**Acceptance**

- [x] Unit test: a stored wrap of 10 bytes gives header 0x8000000A followed by the payload; Unwrap returns the same bytes
- [x] Unit test: a compressed wrap round-trips; a header size that disagrees with the inflated length is rejected; a declared size above the limit is rejected without allocating
- [ ] Real client, once NET/WIZ send MSG_BADGES (GameMessages.xml) with BadgeInfo wrapped: the badge window opens without a crash. Captures show an unwrapped blob in that field crashes the client

**Risks**

- Whether a field needs the envelope is decided per message field, not globally. The capture shows MSG_LOGINCOMPLETE.Data zlib-wrapped, while MSG_LOGINCOMPLETE.CriticalObjects and LOGIN MSG_CHARACTERINFO.CharacterInfo are sent unwrapped and accepted. The message layer needs a per-field policy table

## 3.03 Type dump loader and TypeRegistry (OBJ-4)

**Goal:** Schema lookup by class/property hash from the user's dump.

**Size:** M. **Depends on:** 3.01

**Acceptance**

- [x] Synthetic dump: alias collapse, base chain, id order, enum lookup (TypeRegistryTest)
- [x] Client-gated: ~2205 property classes and 140 enums, no unclassified type (TypeRegistryClientTest: 2197 counting the PropertyClass root)
- [x] 'class WizClientObject' has 14 properties starting m_inactiveBehaviors, m_globalID.m_full, m_permID

### Detailed spec from OBJ-4: Type dump loader and TypeRegistry

The server loads the user's client type dump and answers every schema question by class hash, class name, or property hash.

**Deliverables**

- src/server/shared/ObjectProperty/TypeDumpLoader.h/.cpp: parses dump format v2 (classes{hash:{name,bases,hash,properties{name:{type,id,offset,flags,container,dynamic,singleton,pointer,hash,enum_options?}}}})
- Canonicalization: collapse the `X*` and `SharedPointer<X>` aliases into X; separate enums (140), std-container and primitive pseudo-classes (186), and the ~2205 real property classes. Built with six kinds; r806919 gives 2197 property classes counting the PropertyClass root, 140 enums, 13 value types, 37 primitives, 168 std containers and 37 opaque classes, and 16493 properties. Of its 4397 alias entries, 9 name templates the dump lists without a class prefix and join those classes, and 8 stand in for a class the dump does not list
- src/server/shared/ObjectProperty/TypeRegistry.h/.cpp (singleton accessed as sTypeRegistry): ClassInfo (name, hash, base chain, ordered PropertyInfo list), PropertyInfo (name, hash, ordinal/id, flags, container Static/List/Vector, ValueKind, element class, enum table). Built with ClassInfo and PropertyInfo in TypeInfo.h and the catalog as an immutable generation; the enum table is per property with sorted name and value indexes, and the dump's __DEFAULT (integer or text), __BASECLASS and text options are kept alongside the integer options
- src/server/shared/ObjectProperty/PropertyFlags.h: Save 0, Copy 1, Public 2, Transmit 3, AuthorityTransmit 4, Persistent 5, Deprecated 6, NoScript 7, DirtyEncode 8, Blob 9, Immutable 16, FileName 17, Color 18, Bits 20, Enum 21, Localized 22, StringKey 23, ObjectId 24, ReferenceId 25, ObjectName 27, HasBaseClass 28
- ValueKind classifier over the measured vocabulary: bool, char, unsigned char, short, unsigned short, int, unsigned int, unsigned __int64, gid, float, double, wchar_t, std::string, std::wstring, bui2/4/5/7, s24/u24, the fixed math types, enum, object (inline/pointer/SharedPointer). Built with bit fields as any bi<N> or bui<N> of 1 to 32 bits with the width kept, plus __int64, and the value types r806919 uses: Vector3D, Quaternion, Matrix3x3, Euler, Color, Point<int>, Point<float>, Size<int>, Rect<int>, Rect<float>, SerializedBuffer, SimpleVert and SimpleFace
- Load-time validation: recompute every hash and fail loudly on mismatch; SHA-256 of the dump logged for revision pinning. Built to also refuse broken, empty or misshapen dumps, known fields of the wrong JSON type or missing, duplicates, gaps in property ids, oversized values, bad containers, keys that differ from the hash and inconsistent base chains
- conf/dist gameserver.conf.dist and loginserver.conf.dist options: TypeDumpPath
- Reload: `.reload typedump` (through 4.15 when it lands) loads TypeDumpPath into a new registry off to the side, validates every hash, rebinds the typed views (3.07), and swaps; any failure keeps the old registry and reports every error. Live PropertyObjects keep the registry generation they were built from; if that cannot be made safe, a type-dump change is documented as a restart case instead
- src/test/server/shared/ObjectProperty/TypeRegistryTest.cpp using a small synthetic dump written by us (invented classes), never client data

**Acceptance**

- [x] Unit test on the synthetic dump: alias collapse, base-chain lookup, ordering by property id, enum option lookup in both directions
- [x] Unit test: reloading from a synthetic dump with a bad hash keeps the previous registry serving and reports the mismatch
- [x] Client-gated test: the r806919 dump loads into 2205 property classes and 140 enums with no unclassified property type; load time and memory are logged (target under 2 s, under 150 MB) (TypeRegistryClientTest; 2197 counting the PropertyClass root, loading in about 180 ms into about 11 MiB in an optimized build, resolved defaults included)
- [x] Client-gated test: the class 'class WizClientObject' has 14 properties in id order, starting with m_inactiveBehaviors, m_globalID.m_full, m_permID

**Risks**

- The dump is 13.8 MB of JSON, and the JSON library choice is a pending stack decision (not yet in ARCHITECTURE.md). OBJ-15 adds a binary cache. Resolved: ARCHITECTURE's stack settles nlohmann-json, which 3.01 added to vcpkg
- 12 pointer aliases and 6 SharedPointer aliases have no plain class entry. They must become their own classes rather than be dropped

## 3.04 Dynamic property object model (OBJ-5)

**Goal:** Instantiate and edit any class at runtime.

**Size:** M. **Depends on:** 3.03

**Acceptance**

- [x] Derived-class list passes IsA; clone equals original
- [x] Wrong kind rejected; Bits value 5 renders 'A|C' and parses back

### Detailed spec from OBJ-5: Dynamic property object model

Any of the ~2205 client classes can be instantiated, inspected, and edited at runtime without generated C++ per class.

**Deliverables**

- src/server/shared/ObjectProperty/PropertyValue.h: variant over the ValueKinds plus std::vector<PropertyValue> for List/Vector and std::unique_ptr<PropertyObject> (or shared) for object slots, with null allowed. Built with std::unique_ptr, one alternative per C++ storage type (so Gid shares uint64, the bit fields share int32 and uint32, and an enum is an int64), the fixed-layout value types as plain structs in PropertyTypes, and `AsObject` for the child object, because `GetObject` is a Windows header macro
- src/server/shared/ObjectProperty/PropertyObject.h/.cpp: holds a ClassInfo* and values stored by property ordinal; Get/Set by name, ordinal, or hash; IsA(base); default construction; deep Clone; equality. Built to also keep the catalog it came from alive, edit list elements and child objects in place under the same checks (SetElementAt, EraseElementAt, EditObjectAt), report why a write is refused, refuse children from another catalog generation and writes that would make an object own itself, take a value only when its write succeeds, and compare exactly, floating values by bit pattern. Defaults are resolved and validated once at load in PropertyDefaults.h/.cpp
- Enum helpers: integer<->string for enum and Bits properties using enum_options (Bits values are '|'-joined names). Built in PropertyEnums.h/.cpp, with every enum value kept as its 32 bits read as unsigned and Bits names chosen greedily in dump order, multi-bit options included
- src/test/server/shared/ObjectProperty/PropertyObjectTest.cpp, plus the client-gated src/test/client/PropertyObjectClientTest.cpp, which builds all 2197 r806919 property classes with their defaults

**Acceptance**

- [x] Unit test: on a synthetic class, setting a list of child objects of a derived class passes IsA checks, and clone equals the original
- [x] Unit test: setting the wrong kind (a string into a float property) is rejected
- [x] Unit test: a Bits property with value 5 renders as 'A|C' and parses back

## 3.05 Compact network codec (OBJ-8)

**Goal:** Encode/decode objects in message fields.

**Size:** M. **Depends on:** 3.04, 3.02

**Acceptance**

- [x] Mask filtering, Deprecated skip, DirtyEncode bit, null child, nested derived list (CompactCodecTest)
- [x] Local-gated: BadgeFilterInfoList (1256 B) and BadgeInfoList (363 B) re-encode byte-identically (CompactCodecClientTest)

### Detailed spec from OBJ-8: Compact network codec

Objects inside client messages can be decoded from and encoded to the non-versionable format the client uses on the wire.

**Deliverables**

- ObjectSerializer compact mode: u32 class hash (0 = null), then the class's full property list in id order with no per-property headers. A property is included only if (flags & mask) == mask and it is not Deprecated. The default mask is Transmit|AuthorityTransmit, with Public added for other-player views. Built in src/server/shared/ObjectProperty/ObjectSerializer.h/.cpp, with SerializerOptions::TransmitMask and PublicMask; the captures confirm the wire carries plain class hashes
- Without CompactLength: strings and wstrings use u16 length, containers use u32 count, enums use u32 unless StringEnums is set, bool is 1 bit, nested objects are prefixed by class hash. Built with bits packed least significant first and byte-aligned values starting on the next byte, wide strings as a u16 unit count and UTF-16LE units, bit fields and s24/u24 at their width, and value types as their fields in order; StringEnums writes option names. CompactLength was refused at first because no sample verified compact lengths, and 3.10 later supported it in both formats. SerializeFlags and Compress are refused because they frame a whole blob or file, which BlobEnvelope and 3.11's BindFile handle, and SerializedBuffer, SimpleVert and SimpleFace are not supported yet because their layout is unknown (no r806919 property of those types is transmitted)
- DirtyEncode (flag bit 8) properties carry a 1-bit present prefix; the encoder always sets it unless a dirty set is supplied. Built with the dirty set as a SerializerOptions::IsDirty test, overridden by ForceDirtyEncode; a property marked absent decodes to its default
- A symmetric API: Decode(bytes, mask, limits) -> PropertyObject and Encode(obj, mask, flags) -> bytes. Built as ObjectSerializer::Decode(catalog, bytes, options) and Encode(object, options), with options holding the mask, flags, limits (MaxDepth 64 under a hard ceiling of 128, MaxObjects 65536, MaxContainerCount 65536 and MaxDecodedBytes 16 MiB by default, made live settings in 3.06), whether trailing bytes are allowed, the classes a root may be and whether it may be null, and the dirty test. Each class's default object size is measured at load so the memory budget charges objects, list elements, defaults and strings before allocating them. Results carry a status, the bytes read and the property path a failure happened at. A count the remaining bytes cannot hold is refused before anything is allocated, and a child's class is checked as soon as its hash is read
- src/test/server/shared/ObjectProperty/CompactCodecTest.cpp, plus the client-gated src/test/client/CompactCodecClientTest.cpp, which round-trips a default object of all 2197 r806919 property classes with both masks and, when AMBROSE_OBJECT_SAMPLES_DIR names a folder of captured blobs named after their class, checks every capture

**Acceptance**

- [x] Unit test with synthetic classes: mask filtering, Deprecated skipping, DirtyEncode bit, null child, nested list of derived objects (CompactCodecTest, with golden bytes for every value layout the codec writes)
- [x] Local-gated test (sniffer captures on the maintainer's machine, not committed): BadgeFilterInfoList (1256 bytes) and the inflated BadgeInfoList (363 bytes) decode consuming exactly all bytes, and re-encode byte-identically (CompactCodecClientTest; all 42 captures, BadgeInfoList sizes 363, 386, 400, 466 and 512 bytes, the enveloped ones unwrapped first)
- [ ] Real client, with LOG wiring: the server encodes a WizardCharacterCreationInfo into MSG_CHARACTERINFO.CharacterInfo (LoginMessages.xml) and the character appears on the selection screen; the client's MSG_CREATECHARACTER.CreationInfo decodes to the chosen name parts, school and appearance

**Risks**

- The only compact samples available were produced by the reference server and accepted by the client, not captured from retail. DirtyEncode semantics are unverified because no sample exercises them
- Whether inline (non-pointer) class properties such as WindowBubble or Point carry a class-hash prefix in compact mode is unverified for non-math classes
- Built note: the badge captures confirm only class hashes, int, unsigned int, bool, std::string and pointer lists. Wide strings, bit fields, enums, small and 64-bit integers, floats and value types follow the reference and are pinned by golden bytes until 3.09 and 3.15-3.16 captures confirm them

## 3.06 Hostile-input hardening and fuzzing (OBJ-19)

**Goal:** Client blobs cannot crash or exhaust the server.

**Size:** S. **Depends on:** 3.05

**Acceptance**

- [x] 1M mutations under ASan/UBSan with no crash (DecoderFuzzTest in the linux-gcc-asan leg)
- [x] Vector count 0x7FFFFFFF in a 10-byte blob rejected immediately
- [x] Zero versionable property size returns an error (VersionableDecodeTest: refused with BadSize in the object that holds it, and reported as a size mismatch by a property whose nested object holds it)

### Detailed spec from OBJ-19: Hostile-input hardening and fuzzing

Client-sent ObjectProperty blobs cannot crash, hang, or exhaust the server.

**Deliverables**

- Limits enforced in all decoders: max nesting depth, max container count (checked against remaining bits before allocating), max total objects, max inflated size, rejection of zero-sized versionable properties (infinite-loop guard). The limits are live settings with defaults and bounds (ObjectProperty.MaxDepth, ObjectProperty.MaxContainerCount, ObjectProperty.MaxObjects, ObjectProperty.MaxInflatedSize), read per decode so a change applies to the next blob (registered with 4.16 when it lands). Built as SerializerLimits::Load, clamping each option and reporting it, and SerializerLimits::Apply, which both servers call at startup; a decode whose options carry no limits reads the applied snapshot when it starts. Added ObjectProperty.MaxDecodedBytes for the memory budget 3.05 introduced. The zero-size guard arrived with 3.10's versionable decoder, which refuses any property size smaller than its own header
- A class allow-list per message field (e.g. MSG_CREATECHARACTER.CreationInfo accepts only WizardCharacterCreationInfo). Built as ObjectFields.h/.cpp, a table naming each field's classes, whether its blob is enveloped and whether it may be empty, used by ObjectSerializer::DecodeField and EncodeField. It lists MSG_BADGES BadgeInfo and BadgeFilterInfo (enveloped), MSG_CHARACTERINFO.CharacterInfo and MSG_CREATECHARACTER.CreationInfo (unwrapped; the creation field's envelope is confirmed in 3.15)
- src/test/server/shared/ObjectProperty/DecoderFuzzTest.cpp (seeded random mutations of synthetic golden blobs) plus a libFuzzer target where the toolchain allows. Built with the golden corpus shared in src/test/mocks/ObjectFuzzCorpus.h/.cpp; seeds carry a mode byte and include stored and compressed envelopes decoded through DecodeField; the test runs a million mutations under AddressSanitizer and a hundred thousand elsewhere (AMBROSE_FUZZ_ITERATIONS overrides), checking that anything decoded re-encodes and decodes back equal and that no decode allocates more in total than the memory budget and inflation limit allow. The libFuzzer target src/test/fuzz/ObjectPropertyFuzzer.cpp builds with AMBROSE_BUILD_FUZZERS in the linux-clang-fuzz preset, which CI runs for 500,000 inputs from the seed corpus. ObjectFieldTest covers the field rules and the limits from configuration

**Acceptance**

- [x] The fuzz test runs 1M mutations under ASan/UBSan with no crash and no allocation over the configured cap (DecoderFuzzTest; the linux-gcc-asan leg runs the million)
- [x] Unit test: a vector count of 0x7FFFFFFF in a 10-byte blob is rejected immediately (DecoderFuzzTest, which also checks nothing over 4 KiB is allocated)
- [x] Unit test: a zero property size in versionable mode returns an error instead of looping (VersionableDecodeTest, 3.10)

## 3.07 Typed wrappers over dynamic objects (OBJ-10)

**Goal:** Compile-checked views validated at startup.

**Size:** M. **Depends on:** 3.04

**Acceptance**

- [x] A view naming a missing property fails startup precisely (TypedViewTest)
- [x] Client-gated: first views bind against r806919 (TypedViewClientTest; BindFileClientTest reads the decoded hat through the views)

### Detailed spec from OBJ-10: Typed wrappers over dynamic objects

Game code uses compile-checked C++ accessors for the few dozen classes it touches, and startup verifies each one against the loaded registry.

**Deliverables**

- src/server/shared/ObjectProperty/TypedView.h: a template base plus declaration macros that give a class name and (type string, property name) pairs. The hash is computed constexpr with OBJ-2 and the property ordinal is cached once at bind time. Built with each view as a class deriving from TypedView, a constexpr array of ViewField::Of<C++ type>(position, dump type, property name) entries and a ViewDefinition, and one macro, AMBROSE_TYPED_VIEW, that makes the view's constructor private and gives the base access to it. Accessors read through Read<Field>(), typed by the field's declared storage type, and static assertions keep fields in enum order and on real storage types. Views are built with View::From(object), which returns nothing unless the object's catalog bound the view and the object is of the view's class; accessors are named GetTemplateId(), IsOnPet() and ShouldRename() in the project's style
- TypedViewRegistry: at startup and on every registry reload each view resolves its class and properties in sTypeRegistry; a missing class, property or type mismatch lists every problem, and is a fatal error at startup or refuses the swap on reload. Built with the bindings stored in each catalog generation, so views over objects from an older generation keep that generation's ordinals after a reload; the loader also refuses a dump whose derived class gives an inherited property a different id or container, which all 14,400 inherited properties of r806919 keep, and a registry's first load closes it to new views
- First views: WizardCharacterCreationInfo, WizClientObject, ClientObject, CoreObject, GameObjectTemplate, WizItemTemplate, TemplateManifest, TemplateLocation, RequirementList, NamedEffect
- A codestyle note for apps/codestyle: views carry only the branding header, no comments. Already enforced for every file by codestyle's no-comments rule, so nothing view-specific was added

**Acceptance**

- [x] Unit test: a view over a synthetic class binds; a view naming a nonexistent property fails startup with a precise message (TypedViewTest)
- [x] Unit test: reloading a synthetic registry that drops a bound property refuses the swap and leaves the views bound to the old registry (TypedViewTest)
- [x] Client-gated test: all first views bind against r806919; reading WizItemTemplate::templateId() on the decoded hat returns 1652259 (binding in TypedViewClientTest; the decoded hat in BindFileClientTest, 3.11)
- [x] Unit test: accessing a field through a view costs one indexed load (no hash lookup per access) (TypedViewTest checks each cached ordinal, that a read returns the object's stored value itself, and that a view given another ordinal reads that property instead)

**Risks**

- Pure build-time codegen from the dump would put client-derived output in the build, and CI has no dump. Hand-written views with constexpr hashes avoid committing extracted data. The maintainer should confirm this approach. Settled under the maintainer's standing direction to decide: hand-written views, recorded in doc/ARCHITECTURE.md

## 3.08 db_characters schema, CharacterRepository, GuidGenerator (LOG-5)

**Goal:** Store, load, count and soft-delete wizards.

**Size:** M. **Depends on:** 2.08

**Acceptance**

- [x] 3 characters round-trip every appearance field (CharacterRepositoryTest)
- [x] Soft-deleted characters are excluded from list and count (CharacterRepositoryTest)
- [x] GUIDs never repeat across 1e6 and resume after restart (GuidGeneratorTest, CharacterRepositoryTest)

### Detailed spec from LOG-5: db_characters base schema and CharacterRepository

Wizards can be stored, loaded per account, counted and soft-deleted, with appearance kept in typed columns, independent of the network.

**Deliverables**

- data/sql/base/db_characters/: characters (guid BIGINT UNSIGNED PK, account BIGINT UNSIGNED, name_indices INT UNSIGNED, custom_name VARCHAR(64) NULL, should_rename TINYINT, school_id INT UNSIGNED (string-ID hash, e.g. Fire=2343174), level INT, xp INT, world INT, zone VARCHAR(128), zone_display VARCHAR(128), pos_x, pos_y, pos_z, orientation FLOAT, created, last_logout, online TINYINT, deleted_at DATETIME NULL, deleted_account BIGINT UNSIGNED NULL), character_appearance (guid PK plus one column per WizardCharacterBehavior property: gender, race, head_hands_model, hair_model, hat_model, torso_model, feet_model, wand_model, skin_color, skin_decal, hair_color, hat_color, hat_decal, torso_color, torso_decal, torso_decal2, feet_color, feet_decal, skin_decal2, extended_hair_color, extended_skin_decal, after_combat_dance, after_combat_victory_dance, new_player_options, new_player_options2), updates, updates_include. Built as the dated update data/sql/updates/db_characters/2026_09_16_00.sql, since base/ holds only the updater's own tables. created, last_logout and deleted_at are Unix seconds in BIGINT UNSIGNED like the login tables rather than DATETIME; character_appearance also keeps behavior_template_name_id, stores gender and race as INT UNSIGNED so any enum value round-trips, and cascades when its character row is removed; a check constraint keeps deleted_at and deleted_account set together, and id_sequences keeps the highest guid ever used
- src/server/database/Implementation/CharacterDatabase.{h,cpp}: prepared statements CHAR_SEL_CHARACTERS_BY_ACCOUNT, CHAR_SEL_CHARACTER, CHAR_INS_CHARACTER, CHAR_INS_APPEARANCE, CHAR_UPD_SOFT_DELETE, CHAR_SEL_COUNT_BY_ACCOUNT, CHAR_UPD_ONLINE. Built in src/server/database/Database/Implementation, plus CHAR_UPD_RESTORE for undelete, CHAR_INS_ID_SEQUENCE and CHAR_SEL_MAX_GUID for the guid high-water mark; soft delete only matches offline characters, and the database layer gains DirectExecuteCounted so updates report the rows they changed
- src/server/game/Characters/CharacterRepository.{h,cpp} and CharacterSummary.h (a plain struct the login screen needs). Built as the characters library: synchronous Create, LoadByAccount, Load, CountByAccount, SoftDelete, Restore, SetOnline and GetMaxGuid, plus statement builders and a row reader for 3.09's asynchronous list; a zero guid or account, a character marked deleted, and text that is not UTF-8, holds control characters or is too long are refused before the database is touched; SoftDelete answers CharacterOnline for an online character
- src/server/game/Globals/GuidGenerator.{h,cpp}: 64-bit character GID allocation. Built as the globals library: a lock-free sequential allocator that never hands out zero or the same id twice, resumes above a stored high-water mark, and refuses once the 64-bit range is used
- src/test/server/game/Characters/CharacterRepositoryTest.cpp, plus src/test/server/game/Globals/GuidGeneratorTest.cpp; both database tests pass on MariaDB 10.11 and MySQL 8

**Data sources**

- Type dump r806919.Wizard_1_610.json: class WizardCharacterBehavior (hash 1926270215) property list and bit widths (bui2, bui4, bui5, bui7)

**Database tables**

- characters
- character_appearance
- updates
- updates_include

**Acceptance**

- [x] Unit (DB-backed test fixture or in-memory fake): inserting 3 characters for an account and loading them returns 3 summaries with every appearance field round-tripped bit for bit (CharacterRepositoryTest, every field of the summary compared, for random characters and for characters at every width's smallest and largest value)
- [x] Unit: a soft-deleted character is excluded from the account list and the count but can still be read by guid for undelete (CharacterRepositoryTest, which also restores it)
- [x] Unit: the GUID generator never repeats across 1e6 allocations and survives a restart, resuming from max(guid) (GuidGeneratorTest claims the million from eight threads released together and also while another thread resumes; CharacterRepositoryTest resumes from the high-water mark even after the newest row is removed)

**Risks**

- Retail character GIDs look structured (captured CharID 5739324522485080744 is 0x4FA5...); whether the client reads type bits in the high byte is unverified. Coordinate with OBJ on the GID format. Built note: the available captures come from the reference server, whose ids look random with the top nibble 4, so they say nothing about retail. Character guids start at 1 for now; 3.09's character list shows whether the client accepts them, and the object id layout is settled with OBJ in phase 4
- The appearance column list is tied to this revision's type dump; a revision bump that adds fields needs a dated update file.

## 3.09 Character list (LOG-6)

**Goal:** Select screen shows the account's wizards.

**Size:** M. **Depends on:** 2.14, 3.08, 3.05

**Client messages:** MSG_REQUESTCHARACTERLIST, MSG_STARTCHARACTERLIST, MSG_CHARACTERINFO, MSG_CHARACTERLIST

**Acceptance**

- [x] Blob starts with class hash 292458316 and decodes identically; empty equipment gives 157-221 bytes (LoginScreenInfoBuilderTest, LoginScreenInfoClientTest; corrected: an empty equipment list gives 96 bytes plus the location, and the captured 157-221 byte blobs carried equipped items)
- [ ] Real client: 3 seeded characters show gender, hair, colors, name from name_indices, level and school
- [x] 0 characters shows an empty screen without errors (the server side passes in CharacterHandlerTest; the screen waits for the real client) (2026-09-17, retail r806919 client: the client logged 'CHARACTER LIST' and 'WizardCharacterSelect: The scene has been loaded successfully' for an account with no characters, with no error dialog)

### Detailed spec from LOG-6: Character list: REQUESTCHARACTERLIST -> STARTCHARACTERLIST / CHARACTERINFO* / CHARACTERLIST

The character select screen shows the account's wizards with correct appearance, name, level, school and location.

**Deliverables**

- src/server/apps/loginserver/Handlers/CharacterHandler.cpp: HandleRequestCharacterList sends MSG_STARTCHARACTERLIST{LoginServer=<live setting Login.Name, read per request>, PurchasedCharacterSlots=account.purchased_slots}, then one MSG_CHARACTERINFO per character, then MSG_CHARACTERLIST{Error=0}; if the account is missing, CHARACTERLIST{Error=1}. Built asynchronously: the account, then the count, then the list only when the count is not zero, every character encoded before anything is sent, so a failed query or a character that cannot be encoded also answers CHARACTERLIST{Error=1} alone. The login server now opens the characters database, and Login.Name defaults to Ambrose and may be empty, as the captures show the client accepting. A request made during a listing is answered by one more listing, the account's slots come from their own statement, and at most 256 wizards are listed
- src/server/game/Characters/LoginScreenInfoBuilder.{h,cpp}: builds WizardCharacterCreationInfo {m_templateID=1, m_name=custom_name or empty, m_globalID, m_userID, m_avatarBehavior=WizardCharacterBehavior from appearance, m_equipmentInfoList=EquippedItemInfoList (empty until items exist), m_location=zone_display, m_level, m_world, m_schoolOfFocus, m_nameIndices} and serializes it with Transmit|AuthorityTransmit flags, no SerializerBinary wrapper, not versionable. Built to also set m_shouldRename, m_quarantined=false and m_lastLoginTime from last_logout, and to encode through the MSG_CHARACTERINFO.CharacterInfo field rule
- src/test/server/game/Characters/LoginScreenInfoBuilderTest.cpp, plus src/test/server/apps/loginserver/Handlers/CharacterHandlerTest.cpp and the client-gated src/test/client/LoginScreenInfoClientTest.cpp

**Client messages:** MSG_REQUESTCHARACTERLIST, MSG_STARTCHARACTERLIST, MSG_CHARACTERINFO, MSG_CHARACTERLIST

**Data sources**

- Type dump: WizardCharacterCreationInfo (292458316), CharacterCreationInfo (641636619), WizardCharacterBehavior (1926270215), EquippedItemInfoList (1089850051), EquippedItemInfo (1850291511)
- Sniffer capture lines 4-9 (sequence and blob sizes; the sniffer flags these blobs as 'unwrapped@+0' yet the client displayed them)

**Database tables**

- characters
- character_appearance
- account

**Acceptance**

- [x] Unit: the serialized blob starts with class hash 292458316 (WizardCharacterCreationInfo), and decoding it with our OBJ codec returns identical field values; with an empty equipment list the size falls in the observed 157-221 byte range (LoginScreenInfoBuilderTest; corrected in LoginScreenInfoClientTest against r806919: an empty equipment list gives 96 bytes plus the location's bytes, and the captured blobs were larger because the reference server filled the equipment list)
- [x] Unit: the property flag mask excludes m_shouldRename, m_quarantined and m_lastLoginTime (flags 24), per the type dump (corrected: flags 0x18 are exactly Transmit|AuthorityTransmit, so under that mask those three properties are included; the one left out is m_behaviorTemplateNameID, flags 0x27. The reference server's blobs, which the client displayed, used the same mask; LoginScreenInfoBuilderTest checks both)
- [ ] Real client: an account seeded with 3 characters via data/sql/custom shows 3 wizards; each shows its gender, hair and colors, and the name built from name_indices (first=(idx>>16)&0xFF, middle=(idx>>8)&0xFF, last=idx&0xFF), level and school, matching capture lines 4-9
- [x] Real client: an account with 0 characters shows the empty select screen or the create prompt without errors (2026-09-17, retail r806919 client: the client logged 'CHARACTER LIST' and 'WizardCharacterSelect: The scene has been loaded successfully' for an account with no characters, with no error dialog)

**Risks**

- Whether m_location must be a locale key or a literal display string is unverified; a wrong form shows a blank or raw key on the select screen.
- The equipment preview (what EquippedItemInfo.m_itemID refers to) is unverified; filling it is planned with equipment in 8.10, and characters appear without gear until then.

## 3.10 Versionable decode core (OBJ-6 part 1)

**Goal:** Decode versionable bit framing and CompactLength.

**Size:** M. **Depends on:** 3.04, 3.02

**Acceptance**

- [x] Hand-built bytes with an unknown property (skipped) and unknown nested class (skipped, reported) (VersionableDecodeTest)
- [x] Golden tests cover strings of 128 bytes or more (31-bit long length) (VersionableDecodeTest, with wide strings and lists too)

### Detailed spec from OBJ-6: Versionable BINd decoder

Every BINd client file (templates, spells, states, decks, the manifest) decodes into PropertyObjects.

**Deliverables**

- src/server/shared/ObjectProperty/ObjectSerializer.h/.cpp, Decode path. Serializer flags: SerializeFlags 1, CompactLength 2, StringEnums 4, Compress 8, ForceDirtyEncode 16. Built in ObjectSerializer itself, for encoding too: SerializerOptions::Versionable picks the format for Decode and Encode, renamed from DecodeCompact and EncodeCompact. CompactLength and StringEnums work in both formats, and SerializeFlags and Compress stay refused there because they frame a whole file, which BindFile reads in 3.11
- BindFile.h/.cpp: magic 'BINd'; u32 flags; if flags&8, one padding bit (so a byte), u32 uncompressed size, then a zlib stream (data at offset 13). Built in 3.11
- Versionable framing: u32 class hash (0 means null); u32 object size in bits counted from its own start; repeated {u32 property size in bits, u32 property hash, value}. Unknown property hashes are skipped by size, unknown classes are skipped by size and recorded, and per-property size mismatches resync to the declared end. Built with every skip reported in DecodeResult::Issues by kind, hash, bits skipped, property path and detail. Checked against r806919's Root.wad before committing: a property's size counts from where the previous property ended, before the u32 size realigns to a byte, and an object's size counts from its size field. A value is read inside a bit limit at its property's end. Some values are reported and keep their default, or an earlier copy's value: a value that runs past that end (a list count the bits left cannot hold included), cannot be laid out, names no enum option, or is an object of the wrong class or a null inline object. One that ends early is reported and keeps its value. Sizes that cannot fit are refused with BadSize by the object holding them, and reported as a size mismatch by the property whose nested object holds them. An unknown class in a pointer slot decodes to null, and in an inline slot to a default object of the property's class; an unknown root class is refused. Properties are read and written only when the mask selects them, and others are skipped and reported. A DirtyEncode property carries no present bit and is left out when clean; r806919's files leave out those at their defaults. Default inline objects count toward the depth and object limits
- CompactLength encoding for strings, wstrings and container counts: 1 bit, then 7 bits if the bit is 0 or 31 bits if it is 1 (wstring count is in UTF-16 units). StringEnums: enum and Prop_Bits properties are carried as strings. Fixed math types are byte-aligned float/int/byte tuples. Built as specified: the encoder takes the 31-bit form for 128 or more, and StringEnums also covers int and unsigned int properties with the Enum flag, which read and write the same way as Bits ones. Matrix3x3 stays nine floats, because no Root.wad file holds one
- A decode-limits struct (max depth, max elements, max bytes). The live SerializerLimits of 3.05 and 3.06 bound both formats
- src/tools/bindecode: a CLI that prints any WAD entry as JSON for debugging, reading the user's install. Built in 3.11
- src/test/server/shared/ObjectProperty/VersionableDecodeTest.cpp with hand-built golden bytes for synthetic classes. Built with a literal object, assembled trees, sizes that do not fit, the limits, and round trips in both formats with and without compact lengths and text enums. The decoder fuzz test and libFuzzer target gained versionable and compact-length seeds

**Acceptance**

- [x] Unit test: hand-assembled versionable bytes for a synthetic class decode correctly, including an unknown property (skipped) and an unknown nested class (skipped, reported) (VersionableDecodeTest)
- [x] Client-gated test: TemplateManifest.xml decodes to a TemplateManifest with 137423 TemplateLocation entries, the first being {ObjectData/PlayerObject.xml, 1} (BindFileClientTest, 3.11)
- [x] Client-gated test: ObjectData/CrownItems/Series58/Hats/Crowns-S58-Hats-L110-BS-008-01.xml decodes to a WizItemTemplate with m_templateID 1652259, m_displayName 'Items_00028316', a JewelSocketBehaviorTemplate holding 3 sockets, and m_equipRequirements of ReqSchoolOfFocus 'Balance' plus ReqMagicLevel 110 (BindFileClientTest, 3.11)
- [x] Client-gated sweep over all 134076 Root.wad BINd files: zero crashes and zero property-size mismatches on known classes; a report of unknown class hashes with counts and paths (feeds OBJ-11) (BindFileClientTest, 3.11, over the 134,640 BINd files r806919 actually holds; a scratch sweep before committing 3.10 decoded 134,635 of the 134,640 BINd files with the Save mask, with no size mismatch, unknown or unselected property, invalid object, unsupported type or unknown enum name, 26,921 unknown nested classes reported, and 5 files refused because the dump does not list their root class; re-encoding the 114,687 issue-free files with defaults as clean reproduced 114,342 byte for byte)

**Risks**

- My sweep showed mismatches on CharacterElement.m_flags (Bits), AvatarTextureOption.m_textures and TemplateLocation.m_filename until two rules were applied: Bits as strings, and a 31-bit long-length form. Imcodec's reference reader uses 15 bits for long strings, which looks like a latent bug. Golden tests must cover strings of 128 bytes or more
- Matrix3x3 serialized width is unconfirmed: Imcodec reads 12 floats, but the name suggests 9. Still unconfirmed after 3.10: no Root.wad file holds a Matrix3x3, Euler, Quaternion or SerializedBuffer value

## 3.11 BINd files, bindecode CLI, corpus sweep (OBJ-6 part 2)

**Goal:** Every Root.wad BINd decodes.

**Size:** M. **Depends on:** 3.10, 1.13

**Acceptance**

- [x] TemplateManifest.xml gives 137423 TemplateLocation entries, first {ObjectData/PlayerObject.xml, 1} (BindFileClientTest)
- [x] Crowns-S58-Hats-L110-BS-008-01.xml is WizItemTemplate 1652259, 'Items_00028316', 3 sockets, ReqSchoolOfFocus Balance + ReqMagicLevel 110 (BindFileClientTest, through the typed views)
- [x] Sweep of 134076 BINd: zero crashes, unknown-class report (BindFileClientTest; corrected: r806919's Root.wad holds 134,640 BINd files among 173,088 entries. 134,635 decode with no issue but 26,921 uses of 104 classes the dump does not list, which the test reports with counts and first paths, and the other 5 have a root class it does not list)

### Detailed spec from OBJ-6: Versionable BINd decoder

Every BINd client file (templates, spells, states, decks, the manifest) decodes into PropertyObjects.

**Deliverables**

- src/server/shared/ObjectProperty/ObjectSerializer.h/.cpp, Decode path. Serializer flags: SerializeFlags 1, CompactLength 2, StringEnums 4, Compress 8, ForceDirtyEncode 16. Built in 3.10
- BindFile.h/.cpp: magic 'BINd'; u32 flags; if flags&8, one padding bit (so a byte), u32 uncompressed size, then a zlib stream (data at offset 13). Built for reading and writing:
  - reading refuses a file that is not BINd, ends inside its header, carries unknown flag bits, would inflate past MaxInflatedSize or holds a corrupt stream, and decodes the object with the Save mask under generous default limits for the user's own data, returning the root class hash and the decode's issues;
  - writing sets SerializeFlags and, as the client's own files do, leaves out dirty-encoded properties at their defaults unless ForceDirtyEncode is set.
  BindSweep.h/.cpp sweeps every BINd entry of an archive on every hardware thread and merges the tallies in entry order
- Versionable framing: u32 class hash (0 means null); u32 object size in bits counted from its own start; repeated {u32 property size in bits, u32 property hash, value}. Unknown property hashes are skipped by size, unknown classes are skipped by size and recorded, and per-property size mismatches resync to the declared end. Built in 3.10
- CompactLength encoding for strings, wstrings and container counts: 1 bit, then 7 bits if the bit is 0 or 31 bits if it is 1 (wstring count is in UTF-16 units). StringEnums: enum and Prop_Bits properties are carried as strings. Fixed math types are byte-aligned float/int/byte tuples. Built in 3.10
- A decode-limits struct (max depth, max elements, max bytes). The SerializerLimits of 3.05, with BindFile::GetDefaultLimits raising every limit to its ceiling for the user's own data
- src/tools/bindecode: a CLI that prints any WAD entry as JSON for debugging, reading the user's install. Built with --client, --wad and --type-dump defaulting to AMBROSE_CLIENT_DIR, Root.wad and AMBROSE_TYPE_DUMP_PATH. It prints entries as ordered JSON through PropertyJson.h/.cpp and their issues on standard error, and --compact prints one line per entry. --list prints entry names containing a pattern, and --sweep reports failures, unknown classes and other issues grouped by kind and hash on --threads threads (1-1024). It exits 0, 1 when something cannot be read or decoded, and 2 on bad usage, a sweep counting files that fail only for an unknown root class as a success. Arguments and environment variables are read as UTF-8 on Windows too. The BinDecode CTest checks the usage paths, and with the client variables set also a listing, the hat's JSON and a missing entry
- src/test/server/shared/ObjectProperty/VersionableDecodeTest.cpp with hand-built golden bytes for synthetic classes. Built in 3.10, with BindFileTest (headers, refusals, the dirty rule and a synthetic archive swept on 1, 3 and 16 threads), PropertyJsonTest and the client-gated BindFileClientTest added here

**Acceptance**

- [x] Unit test: hand-assembled versionable bytes for a synthetic class decode correctly, including an unknown property (skipped) and an unknown nested class (skipped, reported) (VersionableDecodeTest, 3.10)
- [x] Client-gated test: TemplateManifest.xml decodes to a TemplateManifest with 137423 TemplateLocation entries, the first being {ObjectData/PlayerObject.xml, 1} (BindFileClientTest)
- [x] Client-gated test: ObjectData/CrownItems/Series58/Hats/Crowns-S58-Hats-L110-BS-008-01.xml decodes to a WizItemTemplate with m_templateID 1652259, m_displayName 'Items_00028316', a JewelSocketBehaviorTemplate holding 3 sockets, and m_equipRequirements of ReqSchoolOfFocus 'Balance' plus ReqMagicLevel 110 (BindFileClientTest)
- [x] Client-gated sweep over all 134076 Root.wad BINd files: zero crashes and zero property-size mismatches on known classes; a report of unknown class hashes with counts and paths (feeds OBJ-11) (BindFileClientTest; corrected to the 134,640 BINd files r806919 holds, with no issue other than unknown classes and 5 files whose root class the dump does not list)

**Risks**

- My sweep showed mismatches on CharacterElement.m_flags (Bits), AvatarTextureOption.m_textures and TemplateLocation.m_filename until two rules were applied: Bits as strings, and a 31-bit long-length form. Imcodec's reference reader uses 15 bits for long strings, which looks like a latent bug. Golden tests must cover strings of 128 bytes or more
- Matrix3x3 serialized width is unconfirmed: Imcodec reads 12 floats, but the name suggests 9. Still unconfirmed: no Root.wad file holds one

## 3.12 Text XML ObjectProperty reader (OBJ-13)

**Goal:** Plain-XML config files load.

**Size:** S. **Depends on:** 3.04, 1.13

**Acceptance**

- [ ] CharacterCreationConfig.xml decodes with non-empty m_creationOptions and m_schoolOptions (waits for 6.10: the file reads as well-formed, but the r806919 type dump lists none of WizCharacterCreationConfig, AllowedCreationOption or AllowedSchoolOption, so XmlObjectReaderClientTest checks that its root class is reported instead)

### Detailed spec from OBJ-13: Text XML ObjectProperty reader

The handful of plain-XML ObjectProperty files (character creation config, action lists, colors) load into PropertyObjects.

**Deliverables**

- src/server/shared/ObjectProperty/XmlObjectReader.h/.cpp: <Objects><Class Name="class X"> elements, property elements by name, nested <Class> for objects, repeated elements (with key attribute) for containers, enum names and '|' Bits text, bool 'true'/'false', UTF-8 BOM tolerant. Built on pugixml, already in the stack:
  - each Class becomes an object of its class starting from its defaults, and elements fill its properties;
  - containers take every element of their name in document order, the key attribute is not needed for that, and an empty element is a null pointer;
  - values read as numbers, bools, option names and flag lists (also on int and unsigned int properties with the Bits or Enum flag), UTF-8 text, colors as AARRGGBB hex as Colors.xml writes them, and math types as numbers separated by commas or spaces;
  - unknown classes and properties, objects of the wrong class, values that do not read, and a static property given twice are skipped and reported in DecodeIssue form with their property path and line, the property keeping its default (or, when repeated, its last value);
  - malformed XML, text or a second element beside the root, a root other than Objects, and documents past the depth, object, element or memory limits (the parsed document's memory estimated before parsing) are refused;
  - text is read as UTF-8, joined across comments and CDATA, whitespace-only values kept, and each container element checked on its own.
- src/test/server/shared/ObjectProperty/XmlObjectReaderTest.cpp with a synthetic XML file. Built with the client-gated src/test/client/XmlObjectReaderClientTest.cpp

**Acceptance**

- [x] Unit test on synthetic XML covers nested lists and enums (XmlObjectReaderTest, with flag lists, colors, vectors, wide text, reported problems and refused documents)
- [ ] Client-gated test: CharacterCreation/CharacterCreationConfig.xml decodes to WizCharacterCreationConfig with non-empty m_creationOptions and m_schoolOptions; ActionList.xml, Chatter.xml, Colors.xml and InputBindings.xml parse with no unknown properties (partly passes in XmlObjectReaderClientTest: ActionList.xml and InputBindings.xml read with no issue at all. The r806919 type dump does not list the root classes of CharacterCreationConfig.xml, Chatter.xml or Colors.xml (WizCharacterCreationConfig, ChatterManager, ShoppingColors), so each reads as well-formed and reports only that class; decoding them waits for 6.10's supplemental schemas)
- [ ] Real client, with LOG wiring: the character-creation screen offers exactly the schools and options the server validates against

**Risks**

- Needs an XML parser dependency (pugixml or similar), which is not yet a listed stack decision. Resolved: pugixml was already in the stack for the message definitions
- The character creation files' classes are missing from the r806919 type dump, so the server cannot validate creation against CharacterCreationConfig.xml through this reader until 6.10. The 3.14 extractor can read the school names from the XML directly
- The AARRGGBB order of colors follows the palette Colors.xml holds, which reads as sensible colors only in that order; no other source confirms it yet

## 3.13 Locale .lang loader and localetool (OBJ-14 + QST-1)

**Goal:** Resolve locale keys from the install.

**Size:** S. **Depends on:** 1.08, 1.13

**Acceptance**

- [x] Synthetic UTF-16 fixture parses; leading-zero keys stay strings (LangFileTest)
- [x] en-US 5132 tables / 217032 keys; Items_00028316 = 'Cute Fairy Kei Broadbrim'; QuestTitle_00001718 = 'To Ravenwood!'; ZoneLocName_1451497 = 'Wizard City|Ravenwood' (LocaleStoreClientTest)
- [x] localetool find 'To Ravenwood!' prints QuestTitle_00001718 (the LocaleTool CTest; QuestTitle_1625D7 holds the same text and is printed too)

### Detailed spec from OBJ-14: Locale .lang loader

Any localized key a template or message references (for example Items_00028316) resolves to display text in any installed language.

**Deliverables**

- src/server/shared/Locale/LangFile.h/.cpp: UTF-16LE with BOM, CRLF lines; header line '1:<TableName>', then triplets of key line, metadata line (usually blank), text line. Built: a lone LF also ends a line, a final line break is optional, a last entry may have empty text, and one or two blank lines after the last entry are padding: most r806919 es, el and pl files end with one, and most de, fr and it files with two. A file over 64 MiB, without the byte order mark, with an odd length, an unpaired surrogate, a missing header, an empty key or an unfinished entry is refused with the reason
- src/server/shared/Locale/LocaleStore.h/.cpp (sLocaleStore): lookup of '<Table>_<Key>' for locale en-US/de/es/fr/it/pl/el; both 8-digit numeric keys and named keys supported. Built for every locale folder Root.wad holds (de, el, en-US, es, fr, gr, it, pl on r806919): loading groups the .lang files by locale and parses the default one at once, and the others parse once on first use. A file that cannot be read or parsed is skipped and named on its locale's table, so one bad file costs only its own keys; a locale none of whose files load fails. On r806919 every locale loads, and pl skips only WizardFurniture.lang, whose 10143 lines end in the middle of an entry. Lookups read an immutable snapshot, and a key defined twice keeps its later text and is counted; r806919's en-US repeats 40 keys, 39 of them with different text, and which copy the client keeps is not confirmed. LocaleTable::FindKeys and GetEntries serve localetool
- Optional support for the BINd Locale/<lang>/StringTable.xml via OBJ-6. Not built: only de, es and it hold one, and every key the roadmap names resolves from the .lang files
- conf option ClientDataDir, DefaultLocale. DefaultLocale is a live setting; `.reload locale` (through 4.15 when it lands) rebuilds the store off to the side from ClientDataDir, swaps it, and keeps the old store on failure. Built as the game server's ClientDir, the same option the login server uses for the install, and Locale.Default (en-US). The server loads the default locale at startup, logs a warning for each file it skipped, and refuses to start when the locale cannot load at all. LocaleStore::Load and SetDefaultLocale build and check the new data, including every locale already in use, before swapping it in and keep the old store on failure, for 4.15's reload triggers to call
- src/test/server/shared/Locale/LangFileTest.cpp with a synthetic .lang. Built with LocaleStoreTest on synthetic archives and the client-gated LocaleStoreClientTest

**Acceptance**

- [x] Unit test: a synthetic UTF-16 file with numeric and named keys and a non-blank metadata line parses correctly (LangFileTest)
- [x] Unit test: a reload that meets a malformed .lang file keeps the previous store resolving keys and names the file (LocaleStoreTest: a malformed file in a locale that otherwise loads is skipped and named on the table, and a reload whose default locale or a locale already in use cannot load keeps the previous store with the reason; it also covers a default locale change)
- [x] Client-gated test: en-US loads 5132 tables and 217032 keys; Items_00028316 resolves to 'Cute Fairy Kei Broadbrim'; the German Items table resolves the same key (LocaleStoreClientTest: 5132 files holding 217032 entries under 216992 distinct keys with 40 repeats and no file skipped, and the German text differs from the English; every installed locale also loads, with pl skipping only WizardFurniture.lang)
- [x] Client-gated test: every m_displayName in the 2000-template sample resolves or is reported as missing (LocaleStoreClientTest over the first 2000 object templates in Root.wad: 1999 resolve or are empty, and ObjectData/AV/AV-Pixie.xml names WizQst72D26_00000006, which en-US does not define)

**Risks**

- The meaning of the metadata line (non-blank in some keys across 247 en-US files) is unknown. Still unknown; it is kept on each parsed entry but not stored in the locale tables, and r806919's en-US holds 9,180 non-blank metadata lines
- The 'gr' folder has only 3 files, while 'el' has 4417. Both load; which one the client uses for Greek is not confirmed
- The client's own handling of a malformed file such as pl/WizardFurniture.lang is unknown; the store skips the file rather than guessing where its entries realign

### Detailed spec from QST-1: Locale .lang reader and key resolver

Server and tools can resolve a client locale key such as QuestTitle_00001718 to its text from the user's own install, and check that a key exists, without committing any text.

**Deliverables**

- src/server/shared/Locale/LangFile.h/.cpp: parse a UTF-16 .lang file. Line 1 is '1:<Stem>'; after it come triplets of key, comment, text. Full key = '<Stem>_<Key>'.
- src/server/shared/Locale/LocaleStore.h/.cpp: lazy per-language index over Root.wad Locale/<lang>/*.lang, with HasKey and Resolve.
- src/tools/localetool: 'find <text>' prints matching keys, 'check <key>' exits nonzero if the key is missing, 'dump <stem>'. Built with --client, --wad, --locale and -- to end the options, plus a locales command listing each locale's files, keys, repeats and skipped files, which exits 1 when a locale cannot load; the LocaleTool CTest checks usage and, with AMBROSE_CLIENT_DIR set, find, check, dump and locales, and reports itself skipped without it
- src/test/server/shared/Locale/LangFileTest.cpp

**Data sources**

- Root.wad Locale/en-US/*.lang (5132 files), e.g. QuestTitle.lang, WizardQuestGoals.lang, ZoneLocName.lang, NPCFormats.lang, WC-NPCs.lang, WizQst*.lang, Quest.lang (madlib format strings)

**Acceptance**

- [x] Unit test with an in-memory UTF-16 fixture written by the test (no client text committed): header stem and triplets parse; a key made of digits with leading zeros stays a string. (LangFileTest)
- [x] Integration test, skipped unless AMBROSE_CLIENT_DIR is set: Resolve('QuestTitle_00001718') == 'To Ravenwood!', Resolve('WizardQuestGoals_TalkNPC') == 'Talk To', Resolve('ZoneLocName_1451497') == 'Wizard City|Ravenwood', Resolve('NPCFormats_Name') contains '$NPC_NAME$'. (LocaleStoreClientTest; NPCFormats_Name is '#1:$NPC_NAME$')
- [x] localetool find 'To Ravenwood!' prints QuestTitle_00001718. (the LocaleTool CTest)

**Risks**

- Some .lang files use different key styles (numeric vs named, e.g. Quest.lang BountyChat) and some stems contain spaces or commas ('Persona, First.lang'). The key-join rule must handle both. Resolved: the full key is the header's stem, an underscore and the key line, whatever characters the stem holds, so "Persona, First" and "Persona,First" stay distinct tables

## 3.14 Name tables and creation config extractor (LOG-7)

**Goal:** World rows for name parts, disallowed list, schools.

**Size:** M. **Depends on:** 3.11, 3.12, 3.13, 2.07

**Acceptance**

- [x] FormatName with middle=0, last=0 gives first name only; out-of-range rejected (CharacterNamesTest and CharacterNameMgrDatabaseTest)
- [x] Tool fills 4 name tables and exactly 7 character_create_school rows; git status clean (the Extractor CTest and CharacterNameExtractorClientTest, on MariaDB 10.11 and MySQL 8; rows go only to the database or a --sql file the user names)

### Detailed spec from LOG-7: Character name tables and creation config extraction

The server knows the valid first, middle and last name index ranges per gender, the disallowed combinations, and the allowed schools, all read from the user's own client install.

**Deliverables**

- src/tools/extractor (name module): reads Root.wad CharacterNames.xml (tables FirstName_HumanMale, FirstName_HumanFemale, MiddleName_Human, LastName_Human; the per-locale copies list the same keys, so take one), Locale/en-US/CharacterNames.lang (UTF-16 key/blank/text triplets), CharacterNamesDisallowedList.xml (a BINd ObjectProperty file, not zlib-wrapped here) and CharacterCreation/CharacterCreationConfig.xml (WizCharacterCreationConfig: the allowed schools Fire, Ice, Storm, Life, Myth, Death, Balance). Built as the extractor tool in src/tools/extractor, over code 3.20 moved to src/server/shared/ClientData and src/server/database/Extraction, whose `names` command reads every table CharacterNames.xml holds, not only the four human ones:
  - The per-locale copies do not list the same keys. de, en-US, es and fr hold 250 names in each human table, while el, it and pl hold 154 male and 144 female first names, 85 middle names and 79 last names, so every table is kept per locale.
  - A table's Section minus its -<locale> suffix names its .lang stem: CharacterNames, PetNames or AdventurePartyNames. A table without a Locale, such as the pet name tables, is kept in each locale that has its stem. r806919 gives 63 tables holding 7955 names.
  - Position 0 of MiddleName_Human and LastName_Human is empty and means no middle or last name. Keys skip retired numbers (the male table has no First_Boy_28), so a name index is a position in the table, not the number in its key.
  - CharacterCreationConfig.xml is read as plain XML by element name, since the type dump lacks its classes. It also lists one m_creationOptions template id, 1, kept in character_create_option.
  - The disallowed list decodes through typed views the extractor binds to its own type registry.
- The extractor writes world DB rows: character_name_part (table_name, idx, locale_key, text_en), character_name_disallowed, character_create_school (school_name, school_id = KI string-ID hash). Built as data/sql/updates/db_world/2026_09_16_00.sql: character_name_part (table_name, locale, idx, locale_key, text) keeps each row's text in its own locale instead of text_en; character_name_disallowed (id, locale_id, gender, first_idx, middle_idx, last_idx); character_create_school (school_id, school_name, sort_order); and character_create_option (sort_order, template_id). The extractor replaces all four tables in one transaction, writes the same script to a file with --sql, or checks everything and writes nothing with --dry-run. Text travels as hex literals, a --sql file is replaced only once written whole, a dry run given a database checks its world tables, and a database without the tables is refused with a pointer to dbimport
- data/sql/base/db_world/: the empty table definitions only (no extracted rows committed). Built as the dated update above, as every schema change is; no extracted rows are committed
- src/server/game/Characters/CharacterNameMgr.{h,cpp} (sCharacterNameMgr): IsValidIndices(nameIndices, gender), FormatName(nameIndices, gender), IsDisallowed(). `.reload character_name` (through 4.15 when it lands) rebuilds the name parts and disallowed list off to the side, validates them, swaps, and keeps the old tables on failure. Built over CharacterNameSet in src/server/shared/Characters/CharacterNames.{h,cpp}, which the extractor also uses to validate before it writes:
  - Name indices pack the first, middle and last positions as 8 bits each (first << 16 | middle << 8 | last), and a set high byte is refused.
  - FormatName gives the first name alone when middle and last are 0. Otherwise it gives the first name, a space, then the middle and last names joined.
  - Checks take a locale, defaulting to the manager's default locale, which the game server sets from Locale.Default. Check also names the reason a name fails.
  - Load reads both tables in one consistent snapshot, requires each table's positions to run from 0 without gaps, validates, and swaps. On failure it keeps the previous tables and reports every problem, for 4.15's reload triggers to call, and it warns when the new tables leave the default locale without human names.
  - The game server loads the tables at startup when the world database is open, refuses to start when they are invalid, and warns when they are empty or lack human names for Locale.Default.
- src/server/shared/Util/StringId.{h,cpp}: the KI string-ID hash, if OBJ has not already provided it. Provided in 3.01 as StringHash::StringId in src/common/Cryptography/StringHash.h
- src/test/server/game/Characters/CharacterNameMgrTest.cpp. Built with CharacterNamesTest, CharacterNameExtractorTest on a Root.wad the test builds, the client-gated CharacterNameExtractorClientTest and the Extractor CTest

**Data sources**

- Root.wad CharacterNames.xml (315130 bytes)
- Root.wad Locale/<locale>/CharacterNames.lang
- Root.wad CharacterNamesDisallowedList.xml (BINd header, flags 0x07)
- Root.wad CharacterCreation/CharacterCreationConfig.xml
- Root.wad CharacterCreation.xml (quiz, client-side only; not needed by the server)

**Database tables**

- character_name_part
- character_name_disallowed
- character_create_school

**Acceptance**

- [x] Unit: StringId('Fire') == 2343174, StringId('Ice') == 72777 and StringId('Balance') == 1027491821, matching the reference enum values (done in 3.01)
- [x] Unit: FormatName with middle=0 and last=0 returns only the first name, and out-of-range indices are rejected (CharacterNamesTest: the first name alone, first space middle and last, out-of-range first, middle and last indices, set high bits, and unknown genders and locales)
- [x] Unit: reloading sCharacterNameMgr applies an edited character_name_part row, and a reload with an invalid row keeps the old tables and reports it (CharacterNameMgrDatabaseTest on MariaDB 10.11 and MySQL 8: an edited middle name applies, and an empty first name, a gap in a table's positions, a bad gender and a dropped table each keep the previous tables with the reason)
- [x] Tool run against the local install fills character_name_part with non-zero counts for all 4 tables and exactly 7 character_create_school rows; git status shows no new data files (the Extractor CTest creates a world database with dbimport, runs `extractor names` against it twice, and starts the game server on it, which loads 63 tables holding 7955 names in 7 locales and 4 disallowed names; CharacterNameExtractorClientTest counts 250 en-US names in each human table and 7 schools in the database; output goes only to the database or a --sql file the user names)

**Risks**

- The internal layout of CharacterNamesDisallowedList.xml (BINd, version 7) is not yet decoded; it needs the OBJ reader for BINd files. Resolved: 3.11's BindFile decodes it as a DisallowedNameList of DisallowedName (m_locale, m_gender, m_first, m_middle, m_last), and r806919 holds 4 names
- Whether the client sends nameIndices built from the locale-specific table or the shared one is unverified. Still unverified; the tables differ by locale, so checks take a locale, and 3.15 and 3.16 must learn the client's locale (MSG_ATTACH carries one)
- A disallowed name's m_locale is 1 or 2. Locale 1's indices form English names in the 250-name tables, and locale 2 is not identified, so checks match every locale unless the caller names a locale id
- One disallowed name has the last index 999, above any 8-bit index. Checks treat an index of 256 or more as matching any index, which is unconfirmed
- Gender 0 selects the female first names and 1 the male ones, matching eGender, and the high byte of name indices is assumed unused; the real-client creation checks in 3.16 confirm both

## 3.15 CreationInfo decode and validation (LOG-8 part 1)

**Goal:** Reject invalid creation requests.

**Size:** M. **Depends on:** 3.09, 3.14, 3.06

**Client messages:** MSG_CREATECHARACTER, MSG_CREATECHARACTERRESPONSE

**Acceptance**

- [ ] Bad school hash, gender=2, hair beyond bui4, bad name index, 7th character each give ErrorCode!=0 and write nothing
- [ ] A garbage blob gives ErrorCode!=0 without closing the session

### Detailed spec from LOG-8: Character creation: MSG_CREATECHARACTER -> MSG_CREATECHARACTERRESPONSE

A player can go through the client's creation flow (quiz, school, appearance, name) and the new wizard appears on the select screen and persists.

**Deliverables**

- CharacterHandler::HandleCreateCharacter: deserialize CreationInfo as WizardCharacterCreationInfo (Transmit|AuthorityTransmit, unwrapped); on a decode failure send ErrorCode!=0 and keep the session
- Validation: count < Character.MaxPerAccount (live setting, default 6) + purchased_slots; m_schoolOfFocus is in character_create_school; m_avatarBehavior.m_eGender is Female=0 or Male=1 (Neutral=2 rejected); m_eRace == Human (79806088); every appearance field fits its bit width; nameIndices pass CharacterNameMgr and the disallowed list; optional uniqueness (live setting Character.UniqueNames); m_name (custom name) is used when security_level allows it, or for any account when an opt-in experimental live setting, off by default, allows custom names, which lets players pick names the retail flow never offers; a custom name is still validated for length and characters and refused when invalid. These settings are read per request (registered with 4.16 when it lands)
- Starting state from world DB playercreateinfo (school_id, zone, location, orientation, level, world), AzerothCore precedent: write the characters and character_appearance rows in one transaction
- `.reload playercreateinfo` and `.reload character_create_school` (through 4.15 when it lands) rebuild their rows off to the side, validate them, swap, and keep the old rows on failure
- CharacterHandler::HandleLoginLogCharacterCreation: store Stage and Parameter on the session and log them (telemetry, no reply)
- data/sql/base/db_world/: playercreateinfo definition; data/sql/custom example rows
- src/test/server/apps/loginserver/CreateCharacterTest.cpp

**Client messages:** MSG_CREATECHARACTER, MSG_CREATECHARACTERRESPONSE, MSG_LOGINLOGCHARACTERCREATION, MSG_REQUESTCHARACTERLIST

**Data sources**

- Type dump WizardCharacterCreationInfo / WizardCharacterBehavior (enum eGender options Female=0, Male=1, Neutral=2; eRace Human=79806088)
- World DB rows from LOG-7
- Imlight Login/Services/CharacterService.cs (behavior only)

**Database tables**

- characters
- character_appearance
- playercreateinfo
- character_create_school
- character_name_part

**Acceptance**

- [ ] Unit: a blob built by our serializer with valid fields creates exactly one character; a bad school hash, gender=2, hair_model beyond bui4, an out-of-range name index and a 7th character each return ErrorCode!=0 and write nothing
- [ ] Unit: a truncated or garbage blob returns ErrorCode!=0 without crashing or closing the session
- [ ] Unit: lowering Character.MaxPerAccount on a running server refuses the next create over the new limit without a restart
- [ ] Real client: completing the creation flow returns to character select with the new wizard at level 1 with the chosen school, look and name; restarting the client and logging in again still shows it
- [ ] Real client: on an account already at the slot limit, the create attempt shows the client's failure message and the list is unchanged

**Risks**

- Which ErrorCode values the client maps to which dialog is unverified; the reference only ever sends 0 or 1.
- Whether the client automatically sends MSG_REQUESTCHARACTERLIST after a successful create, or expects the server to push the list, is unverified.
- Strict appearance validation against avatar option templates in ObjectData (AvatarOption, WizardCharacterBehaviorTemplate) is planned, not yet scheduled; until it lands, bit-width checks alone let through combinations the client UI never offers.
- The starting zone and location (the reference uses WizardCity/Tutorial_Exterior) belong to the tutorial and world domains; until they exist, playercreateinfo must point at a zone the gameserver can load.

## 3.16 Character creation persist and real-client flow (LOG-8 part 2)

**Goal:** New wizard persists and lists.

**Size:** M. **Depends on:** 3.15

**Client messages:** MSG_CREATECHARACTER, MSG_CREATECHARACTERRESPONSE, MSG_LOGINLOGCHARACTERCREATION, MSG_REQUESTCHARACTERLIST

**Acceptance**

- [ ] A valid blob creates exactly one character in one transaction
- [ ] Real client: the creation flow returns to select with the new level-1 wizard; it survives restart
- [ ] At the slot limit the client shows failure and the list is unchanged

### Detailed spec from LOG-8: Character creation: MSG_CREATECHARACTER -> MSG_CREATECHARACTERRESPONSE

A player can go through the client's creation flow (quiz, school, appearance, name) and the new wizard appears on the select screen and persists.

**Deliverables**

- CharacterHandler::HandleCreateCharacter: deserialize CreationInfo as WizardCharacterCreationInfo (Transmit|AuthorityTransmit, unwrapped); on a decode failure send ErrorCode!=0 and keep the session
- Validation: count < Character.MaxPerAccount (live setting, default 6) + purchased_slots; m_schoolOfFocus is in character_create_school; m_avatarBehavior.m_eGender is Female=0 or Male=1 (Neutral=2 rejected); m_eRace == Human (79806088); every appearance field fits its bit width; nameIndices pass CharacterNameMgr and the disallowed list; optional uniqueness (live setting Character.UniqueNames); m_name (custom name) is used when security_level allows it, or for any account when an opt-in experimental live setting, off by default, allows custom names, which lets players pick names the retail flow never offers; a custom name is still validated for length and characters and refused when invalid. These settings are read per request (registered with 4.16 when it lands)
- Starting state from world DB playercreateinfo (school_id, zone, location, orientation, level, world), AzerothCore precedent: write the characters and character_appearance rows in one transaction
- `.reload playercreateinfo` and `.reload character_create_school` (through 4.15 when it lands) rebuild their rows off to the side, validate them, swap, and keep the old rows on failure
- CharacterHandler::HandleLoginLogCharacterCreation: store Stage and Parameter on the session and log them (telemetry, no reply)
- data/sql/base/db_world/: playercreateinfo definition; data/sql/custom example rows
- src/test/server/apps/loginserver/CreateCharacterTest.cpp

**Client messages:** MSG_CREATECHARACTER, MSG_CREATECHARACTERRESPONSE, MSG_LOGINLOGCHARACTERCREATION, MSG_REQUESTCHARACTERLIST

**Data sources**

- Type dump WizardCharacterCreationInfo / WizardCharacterBehavior (enum eGender options Female=0, Male=1, Neutral=2; eRace Human=79806088)
- World DB rows from LOG-7
- Imlight Login/Services/CharacterService.cs (behavior only)

**Database tables**

- characters
- character_appearance
- playercreateinfo
- character_create_school
- character_name_part

**Acceptance**

- [ ] Unit: a blob built by our serializer with valid fields creates exactly one character; a bad school hash, gender=2, hair_model beyond bui4, an out-of-range name index and a 7th character each return ErrorCode!=0 and write nothing
- [ ] Unit: a truncated or garbage blob returns ErrorCode!=0 without crashing or closing the session
- [ ] Unit: lowering Character.MaxPerAccount on a running server refuses the next create over the new limit without a restart
- [ ] Real client: completing the creation flow returns to character select with the new wizard at level 1 with the chosen school, look and name; restarting the client and logging in again still shows it
- [ ] Real client: on an account already at the slot limit, the create attempt shows the client's failure message and the list is unchanged

**Risks**

- Which ErrorCode values the client maps to which dialog is unverified; the reference only ever sends 0 or 1.
- Whether the client automatically sends MSG_REQUESTCHARACTERLIST after a successful create, or expects the server to push the list, is unverified.
- Strict appearance validation against avatar option templates in ObjectData (AvatarOption, WizardCharacterBehaviorTemplate) is planned, not yet scheduled; until it lands, bit-width checks alone let through combinations the client UI never offers.
- The starting zone and location (the reference uses WizardCity/Tutorial_Exterior) belong to the tutorial and world domains; until they exist, playercreateinfo must point at a zone the gameserver can load.

## 3.17 Character deletion (LOG-9)

**Goal:** Soft-delete own wizard.

**Size:** S. **Depends on:** 3.09

**Client messages:** MSG_DELETECHARACTER, MSG_DELETECHARACTERRESPONSE

**Acceptance**

- [ ] Another account's CharID, a missing id or an online character gives ErrorCode!=0
- [ ] Real client: confirm word removes the wizard and it stays gone after relog; DB row has deleted_at

### Detailed spec from LOG-9: Character deletion: MSG_DELETECHARACTER -> MSG_DELETECHARACTERRESPONSE

A player can delete one of their own wizards from the select screen, and the data is soft-deleted and recoverable by a GM.

**Deliverables**

- CharacterHandler::HandleDeleteCharacter: require that the character belongs to the session's account, is not online, and is not already deleted; set deleted_at and deleted_account, clear account; reply MSG_DELETECHARACTERRESPONSE{ErrorCode}
- Live settings Character.DeleteMode (soft or hard) and Character.KeepDeletedDays, read on each delete and purge so a change applies without a restart (registered with 4.16 when it lands)
- src/test/server/apps/loginserver/DeleteCharacterTest.cpp

**Client messages:** MSG_DELETECHARACTER, MSG_DELETECHARACTERRESPONSE

**Database tables**

- characters

**Acceptance**

- [ ] Unit: deleting your own character gives ErrorCode=0 and removes it from the list; a CharID belonging to another account, a nonexistent id or an online character each give ErrorCode!=0 and change nothing
- [ ] Real client: the delete confirmation (the client asks the player to type a confirmation word, per LocalError.lang) removes the wizard from the select screen, and it stays gone after a relog
- [ ] DB: the row still exists with deleted_at set

**Risks**

- Whether the client refreshes the list itself after DELETECHARACTERRESPONSE is unverified. The reference does not resend the list, and the client still updated in practice, but that is not captured.

## 3.18 Updater part 2: rehash, rename, dead refs, pending, modules (FND-18)

**Goal:** Team SQL workflow.

**Size:** M. **Depends on:** 2.06

**Acceptance**

- [ ] applied {A:h1}, disk {B:h1} is a rename with 0 applies
- [ ] Changed hash with Redundancy=0 errors
- [ ] 4 dead refs with CleanDeadRefMaxCount=3 errors
- [ ] AllowPending=1 applies pending_db_world as PENDING

### Detailed spec from FND-18: database/Updater part 2: rehash, rename, redundancy, dead references, pending_ and module includes

The updater copes with renamed, edited, deleted and pending update files the way a real team workflow needs.

**Deliverables**

- Same hash under a new name: record renamed, not re-applied
- Changed hash on an applied RELEASED file: re-apply only if Updates.Redundancy, otherwise error; Updates.AllowRehash fills empty hashes
- Applied file no longer on disk: warn, delete the row when Updates.CleanDeadRefMaxCount allows (default 3; -1 unlimited; 0 never)
- ARCHIVED state for files squashed into base/
- Updates.AllowPending (dev-only, default 0) adds data/sql/updates/pending_db_<name> with state PENDING; pending names must match rev_<unix-timestamp>_<slug>.sql (proposed)
- Module includes: modules/<m>/data/sql/db-<name>/ registered as MODULE via updates_include generated at configure time
- Updates.Redundancy, Updates.AllowRehash, Updates.CleanDeadRefMaxCount and Updates.AllowPending are live settings read on each updater run, including `db update` on a running server (registered with 4.16 when it lands)
- Unit tests for UpdateFetcher decision logic with an in-memory applied-set (no DB)

**Acceptance**

- [ ] Unit: applied {A:h1}, disk {B:h1} gives rename A->B, 0 applies
- [ ] Unit: applied {A:h1}, disk {A:h2}, Redundancy=0 gives an error; Redundancy=1 gives a re-apply
- [ ] Unit: 4 dead refs with CleanDeadRefMaxCount=3 gives an error, not a delete
- [ ] Integration: with AllowPending=1 a pending_db_world file is applied as PENDING; with 0 it is ignored
- [ ] Real client: n/a

**Risks**

- Applying PENDING files locally then merging them under a new dated name will look like a rename (same hash), which is intended but must be tested

## 3.19 CI pending SQL promotion and SQL validation (FND-19)

**Goal:** Pending SQL becomes dated files and every file applies in CI.

**Size:** S. **Depends on:** 1.04, 3.18, 2.07

**Acceptance**

- [ ] Editing an existing updates file fails ci-sql-check
- [ ] Merged pending_db_world/rev_1767225600_npc.sql becomes <today>_00.sql
- [ ] Pending SQL with a syntax error fails the DB job

### Detailed spec from FND-19: apps/ci: pending_ SQL promotion and SQL validation jobs

Pending SQL from merged PRs becomes correctly numbered dated files, and CI proves every SQL file applies on a real database.

**Deliverables**

- apps/ci/ci-pending-sql.py: on push to main, move data/sql/updates/pending_db_<name>/*.sql to updates/db_<name>/YYYY_MM_DD_NN.sql (NN = next free for that UTC date, in pending-name order), then commit with an AI trailer as a bot commit (approved on 2026-09-16 at the maintainer's direction)
- apps/ci/ci-sql-check.py: on PR, fails if any existing file in updates/db_* or base/ was modified or deleted (unless the PR is labelled squash), checks naming rules and SQL headers
- Workflow job with a MariaDB/MySQL service container that runs the pending and updates files on a fresh DB (through dbimport once FND-20 lands)

**Acceptance**

- [ ] A PR editing data/sql/updates/db_world/2026_01_01_00.sql fails ci-sql-check
- [ ] Merging a PR with pending_db_world/rev_1767225600_npc.sql produces updates/db_world/<today>_00.sql, or _01 if _00 exists
- [ ] A PR whose pending SQL has a syntax error fails the DB job, naming the file
- [ ] Real client: n/a

**Risks**

- A bot pushing to main in a private repo needs a token and branch-protection exceptions
- Two PRs merged the same day race for NN; the job must be serialized (concurrency group)
- Since 2026-09-16 core-build builds only on a schedule, by label or on demand (Continuous integration in doc/ARCHITECTURE.md), so pending SQL promotion on push to main and the SQL checks on pull requests need their own workflow or jobs. They must run on every merge and pull request, and stay cheap: a Linux runner with a database container, not a full build

## 3.20 Find client data on the user's machine and guided setup

**Goal:** A server or tool that lacks client data finds it on the user's own machine and offers to use it.

**Size:** M. **Depends on:** 3.13, 3.14, 1.08, 2.07

Added on 2026-09-16 at the maintainer's direction, and built before 3.15: whenever a server or tool cannot work because client files or other local data are missing, it looks for them on the user's own PC and asks whether to use them. Files found on the user's machine are the user's own provided files, so nothing is downloaded or committed.

**Acceptance**

- [x] A fake install tree is found through a Steam library file, a KingsIsle default folder and the environment, validated, de-duplicated, and its revision read from Bin/revision.dat (ClientLocatorTest, which also covers installed programs, Wine, Lutris and Proton prefixes, WSL drives, both libraryfolders.vdf layouts and the folder budget)
- [x] On a terminal, a server with an empty ClientDir lists the installs it found, and choosing one writes conf.d/client-data.conf and starts with it; a run whose input is not a terminal never waits and logs the installs and the setting to add (ClientSetupTest with scripted answers, SetupPromptTest, and the AppSmoke tests, whose piped runs must never print the question)
- [x] bindecode, localetool and extractor without --client use an install the user confirms, and print the installs and the flag to pass when not on a terminal (ClientSetupTest for the shared flow; the Extractor CTest runs the tool without a terminal)
- [ ] The game server with empty name tables offers to extract them from the install and loads them without a restart when accepted (built: the offer runs the in-process extraction and reloads the names; a run on a real terminal is still to be recorded)
- [x] Real client: the maintainer's own retail install is found (ClientLocatorClientTest printed C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through the installed program Wizard101)

### Detailed spec

**Deliverables**

- src/server/shared/ClientData/ClientInstall.{h,cpp}: inspects a folder as a Wizard101 install (Data/GameData/Root.wad and Bin/WizardGraphicalClient.exe) and reads its revision and version from Bin/revision.dat, such as r806919.Wizard_1_610. Built as ClientInstall in ClientLocator.h
- src/server/shared/ClientData/ClientLocator.{h,cpp}: finds installs in AMBROSE_CLIENT_DIR, the Windows uninstall entries naming Wizard101, the KingsIsle default folders, every Steam library's steamapps/common/Wizard101 (the Steam path from the registry and libraryfolders.vdf), and on Linux the Steam and Proton libraries, Wine and Lutris prefixes and WSL drive mounts; the pinned revision sorts first. The registry and file system sit behind an interface so tests use fakes. Built with ClientSystem and LocalClientSystem in ClientSystem.{h,cpp}, which read both registry views under the user and the machine; a search looks two folders deep only below installed programs and visits at most 512 folders (3.22 probes the fixed places before those walks and gives Lutris and Proton prefixes their own budgets of 1024 folders each), and paths show with forward slashes
- Type dump discovery: a revision-named dump beside the install, in the Ambrose user data folder or the working directory, checked by its header before it is offered. Built: the first bytes must open a JSON object naming version and classes, and every .json in the data folder is considered
- src/server/shared/App/SetupPrompt.{h,cpp}: numbered choice and yes/no prompts only when input and output are terminals, with Setup.Prompt (default 1) and Setup.PromptTimeout (seconds, default 120, 0 waits); without a terminal the candidates and the exact setting are logged instead. Built also with Setup.Discover (default 1); tools read AMBROSE_SETUP_DISCOVER, AMBROSE_SETUP_PROMPT and AMBROSE_SETUP_PROMPT_TIMEOUT. Enter picks the first find, a number picks one, other text is a path, and s skips; a timeout or closed input skips every later question
- A confirmed choice is written to conf.d/client-data.conf beside the app's configuration, holding the branding header and ClientDir and TypeDumpPath, and applied through a configuration reload. Built in src/server/shared/App/ClientSetup.{h,cpp}: the file is merged with what it held, written through a temporary file, and quoted, and keys set by the environment or the command line are never asked about
- The extraction parsing moves to src/server/shared/ClientData and the SQL script writer to the database layer, so the game server can extract the name tables in-process and the extractor tool stays a thin front end. Built as src/server/shared/ClientData/{CharacterNameExtractor,NameViews} with ExtractFromInstall, and src/server/database/Extraction/{WorldSqlScript,CharacterNameScript}
- Wiring: the game and login servers ask when ClientDir or TypeDumpPath is empty or invalid; the game server offers name extraction when the world tables are empty; bindecode, localetool and extractor ask when no install is named
- An install whose revision is not the pinned r806919 is offered with a warning that its message and type data may not match
- Tests: ClientLocatorTest over fake trees and a fake registry, SetupPromptTest with scripted input and a non-terminal run, the AppSmoke tests confirming piped runs never prompt, and a client-gated test that the maintainer's install inspects as r806919

**Risks**

- Steam's libraryfolders.vdf format has changed before; the parser must tolerate both known layouts
- A prompt on a terminal blocks startup until answered or timed out, so services must run without a terminal or with Setup.Prompt = 0

## 3.21 Type data from the user's own client program

**Goal:** Ambrose builds the type dump itself, exactly and automatically, from the user's own WizardGraphicalClient.exe for whatever revision is installed.

**Size:** L. **Depends on:** 3.03, 3.20, 1.13

Added on 2026-09-17 at the maintainer's direction, and built before 3.15: users should never have to find a type dump. A spike emulated the r806919 client's own type registration from the program file on disk, without launching the game or reading a running process, and reproduced the reference dump exactly. This replaces the live-process dumper that 16.11 and 16.12 planned.

**Acceptance**

- [x] On the maintainer's r806919 install, typeextract writes a dump equal to the reference dump in every class name, base, property order, type, id, offset, flag, container, dynamic, singleton, pointer, hash and enum option. The only differences allowed: it also lists 5 classes and 4 properties the reference dump missed, and it keeps 60 empty enum option values, in 30 properties, as empty text where the reference dump wrote 0 (TypeExtractionClientTest pins exactly those 5 classes and 60 values with 0 other differences on Windows, and on Linux reading a copy of the client; 6986 classes and 49465 properties, and the dump builds the server's type catalog)
- [x] On a second installed revision (r801440), discovery finds every entry point without per-revision addresses and the dump passes validation (TypeExtractionClientTest with AMBROSE_SECOND_CLIENT_DIR: 6987 classes, 49456 properties and 3054 races, validated and built into a type catalog)
- [x] Validation refuses to write a dump, and names what failed, when a type name does not hash to its hash, a property hash does not match its type and name, ids are out of order, a container is unknown, or the client called a Windows function the layer lacks (TypeWalkerTest, TypeExtractionTest and GuestProcessTest; validation also refuses text that is not UTF-8, copies of a type or option that disagree, a race missing from any eRace property, and any dump the server's type loader refuses)
- [x] The race enum's options come from the install's Races.xml through the client's own race adder, in file order (3065 on r806919, and every eRace property is checked to hold every race)
- [x] The tool only reads the install: nothing is written into the client folder and the game is never launched (the dump goes to --out or the Ambrose data folder, the Windows layer refuses process exits and thread creation, and no process is started)
- [x] Unit tests without a client cover the PE reader, the loader's relocations, forwarded exports and TLS, the Windows API layer, the call budget and fault reports, and discovery over synthetic code and heaps (typeextract_tests: PeImageTest, CodeIndexTest, ClientDiscoveryStaticTest, ClientDiscoveryRuntimeTest, MachineTest, GuestHeapTest, GuestProcessTest, WindowsApiTest, TypeWalkerTest, TypeDumpWriterTest and TypeExtractionTest; the TypeExtract CTest covers the command line)

### Detailed spec

**Deliverables**

- src/tools/typeextract: a tool linked with Unicorn 2 (GPL-2.0, approved by the maintainer on 2026-09-17) and Zydis. It runs as its own process, so a fault or runaway emulation never takes a server down with it
- Image reader: PE32+ sections, exports with forwarders, imports, base relocations, TLS and the exception table, whose unwind chains give each function's start
- Emulation: WizardGraphicalClient.exe maps at its preferred base. The C and C++ runtime DLLs the install ships (ucrtbase, vcruntime140, vcruntime140_1, msvcp140 and concrt140, reached through the api-ms-win-crt forwarders) load with relocations and run their startup, so formatting and type names come from the client's own runtime; every other import is a stub. A Windows layer implements the kernel functions the runtime uses: heap, TLS and FLS, locks and events, code pages, SLists and module lookups. Every call runs under an instruction budget and must return to its sentinel, and a fault names the module and offset. Built in Core/Emulation (Machine, GuestHeap, GuestProcess, WindowsApi). A missing runtime DLL or export is an error naming what needs it; the guest sees C:\Wizard101\Bin as its folder; character types and case mapping come from Windows' own tables for the characters code page 1252 reaches; and a pseudo kernel32 module answers GetProcAddress only for functions the layer has
- Discovery with no per-revision addresses: the C++ initializer table from the CRT startup's _initterm call, the type map by scanning the emulated heap for map nodes whose type name hashes to their key, the Type constructor by voting over the calls before type vtable writes, the PropertyList initializer by voting over the calls after property list references, and the race adder from the RaceManager strings. Built in Core/Analysis (CodeIndex, ClientDiscovery). The C initializer table from `_initterm_e` is found and run first too, because the client's startup code there prepares state the C++ initializers rely on
- Extraction: runs every C++ initializer, then every function that constructs a type or initializes a property list (the lazy getters), then the race adder for each race in Root.wad's Races.xml. It walks the type map, validates, and writes format v2 with the revision and the executable's SHA-256 through a temporary file. Built in Core/Extraction (TypeWalker, TypeDumpWriter, TypeExtraction, ClientLayout). The client registers 14 types and one enum option twice; the first copy is kept and copies that disagree fail validation. Extraction then checks every race landed and builds the server's type catalog from the result before writing. An optimized build extracts r806919 in about 15 seconds; the temporary file is unique and created exclusively
- CLI: --client (or the install found on the machine), --out (default: types/<revision>.json in the Ambrose data folder), --compare <dump> to print every difference from another dump, exit codes 0, 1 on failure and 2 on bad usage. Built in src/tools/typeextract/Main.cpp: the dump to compare is read before extracting, and a revision or data folder that cannot name the default file asks for --out
- Tests: typeextract_tests over synthetic PE images, code and heaps; a client-gated test that extracts AMBROSE_CLIENT_DIR, compares the result with AMBROSE_TYPE_DUMP_PATH and validates AMBROSE_SECOND_CLIENT_DIR when set. On Linux, Unicorn builds as a shared library through the overlay triplet deps/vcpkg/triplets/x64-linux.cmake, because its static library defines crc32 as zlib does. ClientLocator also finds extracted dumps in the data folder's types folder

**Risks**

- A future client may change the engine's struct layouts. Validation catches it, and the layout is then updated
- A future runtime DLL may call Windows functions the layer lacks. Those calls are listed in the error so the layer can grow

## 3.22 Automatic first-run setup

**Goal:** A first start asks nothing: servers and tools find the install, build the type dump and extract the name tables themselves.

**Size:** M. **Depends on:** 3.21

Added on 2026-09-17 at the maintainer's direction: someone who runs the server for the first time only has to play.

**Acceptance**

- [x] With ClientDir and TypeDumpPath empty, the game and login servers pick the install with the newest revision, save it to conf.d/client-data.conf, run typeextract when no dump exists for that revision, load the result and start, all without asking (ClientSetupTest.AutoPicksTheNewestInstallSavesOnlyClientDirAndUsesTheBuiltDump, NewestPrefersHigherRevisionsThenTheProgramThenDiscoveryOrder and AutoSavesNoInstallWithoutADumpAndReportsBuilderFailuresAndMachinesWithoutAnInstall; TypeDumpCacheTest runs a fake extractor; on the maintainer's machine a fresh gameserver --check built the dump from r806919 and started, and loginserver --check then reused it)
- [x] The game server with empty name tables extracts them from the install and loads them without asking (the fresh gameserver --check logged 'The world database has no character name tables, so they are extracted from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610)', 'Extracted 63 character name tables holding 7955 names' and loaded them; the Extractor CTest starts the game server against extracted tables)
- [x] Setup.Mode = ask restores the 3.20 questions and Setup.Mode = off does nothing; the tools follow AMBROSE_SETUP_MODE and never wait on a question unless asked to (ClientSetupTest.AskOffersTheFindsAndABuildOnlyWhenThereIsSomethingToOffer, AskUsesACurrentBuiltDumpWithoutAskingSoABuildAfterAYesLasts, AskOffersABuildWhenNoDumpFoundIsNamedForTheInstall, OffSearchesSavesAndBuildsNothingAndModesParse and ToolsFollowTheirSetupMode; the AppSmoke ask run on a synthetic install reaches the question gate without a terminal; the BinDecode, Extractor and LocaleTool CTests run each tool in off and auto on a synthetic install)
- [x] The 3.20 review findings are resolved, among them: a saved value that a later conf.d file shadows is reported rather than claimed as saved, a stop signal ends a waiting question, the login server never saves ClientDir without a type dump, the Linux search budget is not spent on duplicate Steam folders, and a dump with its keys in another order is accepted (ClientSetupTest.ASavedValueAnotherFileOverridesIsReportedInsteadOfClaimed, SetupPromptTest.AStopRequestEndsAWaitingQuestionWithAndWithoutATimeout, ServerAppTest.ASignalPolledDuringStartEndsTheRunCleanly, ClientSetupTest.TheLoginServerNeverSavesAnInstallWithoutATypeDump, ClientLocatorTest.DuplicateSteamFoldersAndLibraryFilesSpendNoExtraFolderQueries and ATypeDumpHeaderNamesVersionOrClassesAsAKeyInAnyOrder; the 3.22 review's 21 verified findings are fixed with regression tests, among them the game server no longer saving an install without a dump into the file the login server reads, and a build lock the operating system holds)
- [x] Real client: a fresh configuration on the maintainer's machine starts both servers with no manual setup (2026-09-17 on the maintainer's machine, with ClientDir and TypeDumpPath empty and a fresh data folder: gameserver --check picked C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), built the type dump in 87 seconds in a Debug build, saved ClientDir to conf.d/client-data.conf, extracted 63 name tables holding 7955 names and reported ready; loginserver --check then reused the dump and reported ready, and the play test server switched to automatic setup and reused its existing dump)

### Detailed spec

**Deliverables**

- Setup.Mode (auto, ask or off; default auto) replaces Setup.Prompt and Setup.Discover, which are removed. Setup.PromptTimeout stays and applies to ask mode. Built in src/server/shared/App/ClientSetup.{h,cpp}: the tools read AMBROSE_SETUP_MODE and AMBROSE_SETUP_PROMPT_TIMEOUT, and any other mode is reported and runs as auto. Auto picks the install with the newest revision, preferring one with the client program when revisions tie, saves only ClientDir to conf.d/client-data.conf, and never asks; ask restores the 3.20 questions and offers a build when no dump is found; off searches for nothing in the servers, and in the tools prints the finds and the flag to pass. Keys set by the environment or the command line are never changed
- Setup.TypeExtractor (empty: typeextract beside the server's executable) and Setup.TypeExtractTimeout (seconds, default 900, 0 for no limit) in both servers' configuration. An empty TypeDumpPath means the dump TypeDumpCache keeps for the install in use, and that path is never saved
- src/common/Utilities/ChildProcess.{h,cpp}: starts a program with arguments, streams its output lines to the log, and reports its exit code, with a timeout and a stop that ends the child. Built: arguments pass exactly as given, with no window and no input, and each output line reaches a callback as UTF-8, split past 64 KiB, which TypeDumpCache forwards to the server's log or the tool's standard error. A timeout or stop ends the child and everything it started, through a kill-on-close job object on Windows and a process group sent SIGTERM and then SIGKILL on POSIX, by force after a 2-second grace. Batch files are refused, because cmd.exe reparses their arguments
- src/server/shared/ClientData/TypeDumpCache.{h,cpp}: the cached dump for an install (types/<revision>.json in the Ambrose data folder), checked against the executable's SHA-256 it records, and built by running typeextract beside the server's executable when missing or stale. Built: a dump is current when the revision and executable SHA-256 in its header match the install, whatever order its root keys take. typeextract runs with --client, --out, --quiet and --exit-when-input-ends, holding an input pipe only the caller holds so it ends with its caller, under an operating system lock (LockFileEx or flock) on a lock file beside the dump that also records the process id, time and host name. Other callers wait while the lock is held, however long, and take a lock file no process holds at once. A stop ends the wait or the build, the built dump is checked again before it is used, and every failure names its cause
- The game server's name extraction runs automatically when the world tables are empty, then reloads the names. Built: automatically in auto mode, after a yes in ask mode, and never in off mode, which logs the extractor's names command instead
- The login server never saves ClientDir for an install it has no type dump for, and refuses to start with an install in use but no type dump, naming where ClientDir came from
- A stop during setup: ServerApp::PollStopRequested runs queued signal handlers while OnStart runs, so a stop ends a waiting question, a build or the wait for one, and the server exits without reporting ready
- Discovery additions in ClientLocator: Snap Steam beside the native and Flatpak Steam folders; on WSL, each Windows drive's per-user AppData/Local KingsIsle folder and the Windows Steam folder, whose libraryfolders.vdf drive paths map to /mnt. Installs note whether Bin/WizardGraphicalClient.exe is there and sort newest revision first, unknown revisions last. Steam libraries and folders compare by canonical path, so each is searched once; quoted install locations are cleaned, and Windows uninstall entries expand environment variables and are listed once
- The tools: bindecode, localetool and extractor follow AMBROSE_SETUP_MODE, ask only in ask mode, check their arguments before searching, and save nothing. bindecode and extractor use the type dump built by the typeextract beside them and stop it on Ctrl+C or SIGTERM. bindecode opens an archive named by path first and, when the archive lies in an install, takes the dump from that install
- The 3.20 review findings, fixed and covered by tests. Built: a saved value another file overrides is reported instead of claimed, a stop request ends a waiting question, the login server never saves ClientDir without a type dump, duplicate Steam folders and Proton prefixes no longer spend the Linux search budget, a dump header is accepted with its keys in any order, and a line arriving as a timeout fires still counts as a timeout (ClientSetupTest, SetupPromptTest, ClientLocatorTest, TypeDumpCacheTest, ChildProcessTest and ServerAppTest)

## 3.23 Keep up with KingsIsle's client revisions

**Goal:** Ambrose follows whatever revision the user's install has, including KingsIsle's latest, with no pinned revision.

**Size:** M. **Depends on:** 3.22

Added on 2026-09-17 at the maintainer's direction: the live client moves past r806919, and Ambrose must keep up.

**Acceptance**

- [ ] No runtime check or warning names r806919, and installs sort newest revision first
- [ ] Extracted data is kept per revision: type dumps as types/<revision>.json and name tables tagged with the revision they came from, and each is rebuilt when the install's revision or executable changes
- [ ] A running server notices its install's revision change, extracts in the background, and reloads message definitions, types and names live, keeping the old ones if anything fails
- [ ] Client-gated tests take their expected counts from the installed revision rather than r806919 constants, and still pass on r806919

### Detailed spec

**Deliverables**

- ClientInstall drops the pinned revision: candidates sort by revision number, newest first
- A revision watch on Bin/revision.dat and the executable's size and time, which triggers the live rebuild
- The world name tables record their revision; a start or reload against a different revision re-extracts them
- Tests keyed by the installed revision, with r806919-only facts kept as checks that apply only when that revision is installed

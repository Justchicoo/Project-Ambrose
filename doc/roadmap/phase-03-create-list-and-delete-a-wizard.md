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
- [ ] Unit: FormatName with middle=0 and last=0 returns only the first name, and out-of-range indices are rejected (built in 3.14, which repeats this check)
- [ ] Unit: reloading sCharacterNameMgr applies an edited character_name_part row, and a reload with an invalid row keeps the old tables and reports it (built in 3.14, which repeats this check)
- [ ] Tool run against the local install fills character_name_part with non-zero counts for all 4 tables and exactly 7 character_create_school rows; git status shows no new data files (built in 3.14, which repeats this check)

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
- Without CompactLength: strings and wstrings use u16 length, containers use u32 count, enums use u32 unless StringEnums is set, bool is 1 bit, nested objects are prefixed by class hash. Built with bits packed least significant first and byte-aligned values starting on the next byte, wide strings as a u16 unit count and UTF-16LE units, bit fields and s24/u24 at their width, and value types as their fields in order; StringEnums writes option names. CompactLength, SerializeFlags and Compress are refused as unsupported, because no sample verifies compact lengths and the envelope is BlobEnvelope's job, and SerializedBuffer, SimpleVert and SimpleFace are refused as having no known layout (no r806919 property of those types is transmitted)
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
- [ ] 0 characters shows an empty screen without errors (the server side passes in CharacterHandlerTest; the screen waits for the real client)

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
- [ ] Real client: an account with 0 characters shows the empty select screen or the create prompt without errors

**Risks**

- Whether m_location must be a locale key or a literal display string is unverified; a wrong form shows a blank or raw key on the select screen.
- The equipment preview (what EquippedItemInfo.m_itemID refers to) is unverified and deferred to the item domain; characters appear without gear until then.

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

- [ ] CharacterCreationConfig.xml decodes with non-empty m_creationOptions and m_schoolOptions

### Detailed spec from OBJ-13: Text XML ObjectProperty reader

The handful of plain-XML ObjectProperty files (character creation config, action lists, colors) load into PropertyObjects.

**Deliverables**

- src/server/shared/ObjectProperty/XmlObjectReader.h/.cpp: <Objects><Class Name="class X"> elements, property elements by name, nested <Class> for objects, repeated elements (with key attribute) for containers, enum names and '|' Bits text, bool 'true'/'false', UTF-8 BOM tolerant
- src/test/server/shared/ObjectProperty/XmlObjectReaderTest.cpp with a synthetic XML file

**Acceptance**

- [ ] Unit test on synthetic XML covers nested lists and enums
- [ ] Client-gated test: CharacterCreation/CharacterCreationConfig.xml decodes to WizCharacterCreationConfig with non-empty m_creationOptions and m_schoolOptions; ActionList.xml, Chatter.xml, Colors.xml and InputBindings.xml parse with no unknown properties
- [ ] Real client, with LOG wiring: the character-creation screen offers exactly the schools and options the server validates against

**Risks**

- Needs an XML parser dependency (pugixml or similar), which is not yet a listed stack decision

## 3.13 Locale .lang loader and localetool (OBJ-14 + QST-1)

**Goal:** Resolve locale keys from the install.

**Size:** S. **Depends on:** 1.08, 1.13

**Acceptance**

- [ ] Synthetic UTF-16 fixture parses; leading-zero keys stay strings
- [ ] en-US 5132 tables / 217032 keys; Items_00028316 = 'Cute Fairy Kei Broadbrim'; QuestTitle_00001718 = 'To Ravenwood!'; ZoneLocName_1451497 = 'Wizard City|Ravenwood'
- [ ] localetool find 'To Ravenwood!' prints QuestTitle_00001718

### Detailed spec from OBJ-14: Locale .lang loader

Any localized key a template or message references (for example Items_00028316) resolves to display text in any installed language.

**Deliverables**

- src/server/shared/Locale/LangFile.h/.cpp: UTF-16LE with BOM, CRLF lines; header line '1:<TableName>', then triplets of key line, metadata line (usually blank), text line
- src/server/shared/Locale/LocaleStore.h/.cpp (sLocaleStore): lookup of '<Table>_<Key>' for locale en-US/de/es/fr/it/pl/el; both 8-digit numeric keys and named keys supported
- Optional support for the BINd Locale/<lang>/StringTable.xml via OBJ-6
- conf option ClientDataDir, DefaultLocale. DefaultLocale is a live setting; `.reload locale` (through 4.15 when it lands) rebuilds the store off to the side from ClientDataDir, swaps it, and keeps the old store on failure
- src/test/server/shared/Locale/LangFileTest.cpp with a synthetic .lang

**Acceptance**

- [ ] Unit test: a synthetic UTF-16 file with numeric and named keys and a non-blank metadata line parses correctly
- [ ] Unit test: a reload that meets a malformed .lang file keeps the previous store resolving keys and names the file
- [ ] Client-gated test: en-US loads 5132 tables and 217032 keys; Items_00028316 resolves to 'Cute Fairy Kei Broadbrim'; the German Items table resolves the same key
- [ ] Client-gated test: every m_displayName in the 2000-template sample resolves or is reported as missing

**Risks**

- The meaning of the metadata line (non-blank in some keys across 247 en-US files) is unknown
- The 'gr' folder has only 3 files, while 'el' has 4417

### Detailed spec from QST-1: Locale .lang reader and key resolver

Server and tools can resolve a client locale key such as QuestTitle_00001718 to its text from the user's own install, and check that a key exists, without committing any text.

**Deliverables**

- src/server/shared/Locale/LangFile.h/.cpp: parse a UTF-16 .lang file. Line 1 is '1:<Stem>'; after it come triplets of key, comment, text. Full key = '<Stem>_<Key>'.
- src/server/shared/Locale/LocaleStore.h/.cpp: lazy per-language index over Root.wad Locale/<lang>/*.lang, with HasKey and Resolve.
- src/tools/localetool: 'find <text>' prints matching keys, 'check <key>' exits nonzero if the key is missing, 'dump <stem>'.
- src/test/server/shared/Locale/LangFileTest.cpp

**Data sources**

- Root.wad Locale/en-US/*.lang (5132 files), e.g. QuestTitle.lang, WizardQuestGoals.lang, ZoneLocName.lang, NPCFormats.lang, WC-NPCs.lang, WizQst*.lang, Quest.lang (madlib format strings)

**Acceptance**

- [ ] Unit test with an in-memory UTF-16 fixture written by the test (no client text committed): header stem and triplets parse; a key made of digits with leading zeros stays a string.
- [ ] Integration test, skipped unless AMBROSE_CLIENT_DIR is set: Resolve('QuestTitle_00001718') == 'To Ravenwood!', Resolve('WizardQuestGoals_TalkNPC') == 'Talk To', Resolve('ZoneLocName_1451497') == 'Wizard City|Ravenwood', Resolve('NPCFormats_Name') contains '$NPC_NAME$'.
- [ ] localetool find 'To Ravenwood!' prints QuestTitle_00001718.

**Risks**

- Some .lang files use different key styles (numeric vs named, e.g. Quest.lang BountyChat) and some stems contain spaces or commas ('Persona, First.lang'). The key-join rule must handle both.

## 3.14 Name tables and creation config extractor (LOG-7)

**Goal:** World rows for name parts, disallowed list, schools.

**Size:** M. **Depends on:** 3.11, 3.12, 3.13, 2.07

**Acceptance**

- [ ] FormatName with middle=0, last=0 gives first name only; out-of-range rejected
- [ ] Tool fills 4 name tables and exactly 7 character_create_school rows; git status clean

### Detailed spec from LOG-7: Character name tables and creation config extraction

The server knows the valid first, middle and last name index ranges per gender, the disallowed combinations, and the allowed schools, all read from the user's own client install.

**Deliverables**

- src/tools/extractor (name module): reads Root.wad CharacterNames.xml (tables FirstName_HumanMale, FirstName_HumanFemale, MiddleName_Human, LastName_Human; the per-locale copies list the same keys, so take one), Locale/en-US/CharacterNames.lang (UTF-16 key/blank/text triplets), CharacterNamesDisallowedList.xml (a BINd ObjectProperty file, not zlib-wrapped here) and CharacterCreation/CharacterCreationConfig.xml (WizCharacterCreationConfig: the allowed schools Fire, Ice, Storm, Life, Myth, Death, Balance)
- The extractor writes world DB rows: character_name_part (table_name, idx, locale_key, text_en), character_name_disallowed, character_create_school (school_name, school_id = KI string-ID hash)
- data/sql/base/db_world/: the empty table definitions only (no extracted rows committed)
- src/server/game/Characters/CharacterNameMgr.{h,cpp} (sCharacterNameMgr): IsValidIndices(nameIndices, gender), FormatName(nameIndices, gender), IsDisallowed(). `.reload character_name` (through 4.15 when it lands) rebuilds the name parts and disallowed list off to the side, validates them, swaps, and keeps the old tables on failure
- src/server/shared/Util/StringId.{h,cpp}: the KI string-ID hash, if OBJ has not already provided it. Provided in 3.01 as StringHash::StringId in src/common/Cryptography/StringHash.h
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

- [x] Unit: StringId('Fire') == 2343174, StringId('Ice') == 72777 and StringId('Balance') == 1027491821, matching the reference enum values (done in 3.01)
- [ ] Unit: FormatName with middle=0 and last=0 returns only the first name, and out-of-range indices are rejected
- [ ] Unit: reloading sCharacterNameMgr applies an edited character_name_part row, and a reload with an invalid row keeps the old tables and reports it
- [ ] Tool run against the local install fills character_name_part with non-zero counts for all 4 tables and exactly 7 character_create_school rows; git status shows no new data files

**Risks**

- The internal layout of CharacterNamesDisallowedList.xml (BINd, version 7) is not yet decoded; it needs the OBJ reader for BINd files.
- Whether the client sends nameIndices built from the locale-specific table or the shared one is unverified.

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
- Validation: count < Character.MaxPerAccount (live setting, default 6) + purchased_slots; m_schoolOfFocus is in character_create_school; m_avatarBehavior.m_eGender is Female=0 or Male=1 (Neutral=2 rejected); m_eRace == Human (79806088); every appearance field fits its bit width; nameIndices pass CharacterNameMgr and the disallowed list; optional uniqueness (live setting Character.UniqueNames); m_name (custom name) ignored unless security_level allows. Both settings are read per request (registered with 4.16 when it lands)
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
- Strict appearance validation against avatar option templates in ObjectData (AvatarOption, WizardCharacterBehaviorTemplate) is not scoped here; bit-width checks alone let through combinations the client UI never offers.
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
- Validation: count < Character.MaxPerAccount (live setting, default 6) + purchased_slots; m_schoolOfFocus is in character_create_school; m_avatarBehavior.m_eGender is Female=0 or Male=1 (Neutral=2 rejected); m_eRace == Human (79806088); every appearance field fits its bit width; nameIndices pass CharacterNameMgr and the disallowed list; optional uniqueness (live setting Character.UniqueNames); m_name (custom name) ignored unless security_level allows. Both settings are read per request (registered with 4.16 when it lands)
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
- Strict appearance validation against avatar option templates in ObjectData (AvatarOption, WizardCharacterBehaviorTemplate) is not scoped here; bit-width checks alone let through combinations the client UI never offers.
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

- apps/ci/ci-pending-sql.py: on push to main, move data/sql/updates/pending_db_<name>/*.sql to updates/db_<name>/YYYY_MM_DD_NN.sql (NN = next free for that UTC date, in pending-name order), then commit with an AI trailer as a bot commit (maintainer approval needed)
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

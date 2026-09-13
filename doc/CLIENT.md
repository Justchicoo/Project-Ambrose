<!-- Project Ambrose by Imjustchico: Client strategy: what can be modded, rewritten, or replaced, and at what cost. -->

# Client strategy

Short answer: yes, we can do a lot with the client. But the FusionFall example isn't what it sounds like, and "add anything" doesn't come in one step. It comes in stages, each costing a lot more than the one before.

**The FusionFall premise didn't check out.** I found no public FusionFall project that runs the game on Three.js or any new renderer:
- GitHub code search for "fusionfall three" returned 0 hits.
- The OpenFusionProject org has 17 repos and none is a renderer.
- The real community client (OpenFusion) keeps the original Unity 2.5 Web Player and its C# game code. It runs that player in a small native host (ffrunner, started by a Tauri launcher) with binary-patched DLLs served from a community CDN.
- OpenFusion's own FAQ says it adds no new content. Retrobution, the project that does, builds new bundles in 2008-era Unity editors and calls itself limited.
- The one real remake in a new engine, FusionFall Legacy, took about 5 years, never shipped, and was ended by Cartoon Network's DMCA notice of 2020-04-16.

So FusionFall is evidence for keeping the original client and wrapping or patching it from outside. It is not evidence for a quick web client with no limits.

**Why the stock client is already very moddable.** Measured on r806919 (Wizard 1.610):
- **Patch checks are CRC-32 only.** The patcher checks everything against a file list (LatestFileList.bin) that the patch server itself sends, using CRC-32 and sizes. The CRC variant (poly 0xEDB88320, init 0, no final XOR) matched 62 of 62 sampled files. WAD header CRCs matched 58 of 58 and entry CRCs 54 of 54. The file list decodes completely: 3592 tables, 3825 file records.
- **No signature link found.** The exe contains Crypto++ RSA code, but none of its strings tie it to patching.
- **The game is mostly data.** Root.wad alone holds 104,869 object templates, 18,173 spells, 33,932 locale files, 3,985 cinematics, 1,824 state machines, 158 Lua scripts and 42 GUI layouts (960 .gui files across all WADs).

This means a patch server we run can probably deliver new or changed data WADs without touching the exe. That is still unproven: nobody has yet watched a real client accept a rebuilt WAD.

**The scale of a new client.**
- **Assets:** 3,589 WADs, 550,616 entries, 34.6 GB unpacked.
- **Protocol:** 971 network messages across 26 XML files.
- **Formats:** 81% of models are Gamebryo 20.6 (NiMesh plus the newer "evaluator" animation), which no public library fully loads. 160 block types appear (all stock Gamebryo), plus two particle-system generations. Nearly all game data is BINd, a binary format keyed by hashes, so decoding it needs the client's type registry. The GUI and cinematic systems, and 9 minigame DLLs, are all built into the executable.
- **Precedents:** OpenMW (a Morrowind engine rewrite) is still pre-1.0 after about 17 years with about 505 contributors. xoreos is pre-alpha after 16 years.

**The ladder, cheapest first:**
- **L0:** Server-side content only, on the untouched retail client.
- **L1:** Data-only modding served by our own patch server.
- **L2:** Tooling that builds overlay WADs from our own source plus the user's install (a Keira-style editor, template/locale/zone exporters).
- **L3:** A launcher that changes client behavior at runtime (DLL injection / hooks) for the few things data can't do.
- **L4:** Our own standalone viewer/editor (a native C++ NIF/KF loader and zone viewer).
- **L5:** A full replacement client, which is the only rung that truly means "add anything".

L0 to L2 are what can realistically be built; L4 is a good long-term side project; L5 is a multi-year stretch goal.

## Lessons from FusionFall

- FusionFall's success came from keeping the original client (the Unity Web Player in ffrunner, with patched DLLs) and wrapping it from outside, not from rewriting it. Wizard101's native exe still runs on current Windows, so we need even less wrapper work.
- The one real FusionFall rewrite in a new engine (Legacy) ran about 5 years, drifted in scope, never shipped, and was killed by the 2020-04-16 DMCA notice. Publicity plus trademarked assets made it an easy target.
- Adding content inside a closed client hits hard limits: Retrobution hand-fixes bundle tables, bad references crash the client, and the community calls it limited. Wizard101 data modding will hit the equivalent wall at unknown classes, messages and UI controls.
- OpenFusion pins exactly two client builds with a public support matrix. Ambrose should likewise pin r806919.
- ffproto generates wire code from one JSON spec. Ambrose should generate its codecs from the 26 message XMLs (971 messages) the same way.
- OpenFusion's distribution depends on a community CDN serving copyrighted bundles and patched binaries. That carries more legal exposure than Ambrose's bring-your-own-install rule, so don't copy it.
- The launcher pattern carries over well: environment-variable feature flags, DXVK for Vulkan, an FPS-cap fix, an HTTPS proxy, and a hashed manifest with repair.
- From-scratch JS engine runtimes (UWP.js) show that parsing assets is easy and executing game logic is where they stall; the same applies to a Wizard101 web client.

## Options, cheapest first

### L0 - Server-side content on the unmodified retail client

- **Enables:** New quests, NPC and mob spawns, dialogue flow, loot, shops, drop tables, combat encounters, zone links and GM building tools. It uses only templates, zones, models and locale strings the client already has. Nothing on the client changes.
- **Cannot do:** Show new text, models, items, zones or UI the client doesn't already contain. Any template ID the server sends must already exist in the client's TemplateManifest.xml. Server-sent chat text works, but locale-keyed text must already exist.
- **Effort:** Low incremental effort; this is the planned server work (world DB, ScriptMgr, GM commands). The Ambrose tools are still empty scaffolds (src/tools/{dbimport,template_extractor,wad_extractor,zone_extractor}), so a template/ID picker has to be built first. Weeks to reach first custom content once the server is running.
- **Technical risk:** Low. Only the server and the wire protocol are involved.
- **Legal and policy risk:** Lowest. No client files are changed or redistributed; players bring their own install. The remaining exposure is the private server itself and use of the Wizard101 name and trademarks.
- **Prerequisites:** A working game server with zone streaming and object spawning; A template_extractor index (local, never committed) so content authors can pick existing template IDs; World DB schema plus dated SQL updates

### L1 - Data-only modding through our own patch server

- **Enables:** The client downloads new or changed WADs: new object templates (an ObjectData XML plus a TemplateManifest entry), new locale strings, textures, reused or newly authored .nif models, new zone WADs (nav, collision, spawn and trigger XML), .gui layouts built from existing controls, spells and decks (data side), cinematics, sounds and client Lua in Scripts/. The client is pointed at us by editing PatchConfig.xml (patch host) and passing -L host port (login). The exe isn't changed.
- **Cannot do:** New message types (each needs a C++ handler, and message-module CRCs may break the handshake). New ObjectProperty classes or properties the client registry doesn't know. New UI control types or behaviors beyond the existing controls and Lua bindings. New minigames (native MG_*.dll). New cinematic action or behavior types. Any rendering or shader change.
- **Effort:** Medium. The patchserver needs only one TCP message (MSG_LATEST_FILE_LIST_V2) plus static HTTP. A manifest builder must compute Size, CRC, HeaderSize, HeaderCRC and the gzip header size, then the list CRC. A few weeks for the patch server and manifest builder.
- **Technical risk:** Medium, with three open questions. (a) Unproven: that the client accepts a rebuilt WAD with new CRCs. (b) Unknown: which WAD wins when two contain the same path, so every template or locale change may mean rebuilding the 295 MB Root.wad. (c) Reusing an existing template ID silently replaces the retail template ('Duplicate template ID ... Replacing'). Also, running KingsIsle's launcher against the official servers reverts the files, so our install must be a separate folder.
- **Legal and policy risk:** Medium. Serving changed KingsIsle WADs is redistributing derived copyrighted files, even if only to the user's own client. It stays defensible only if the served WADs are built locally on the user's machine from their own install plus our original content, and never hosted publicly or committed. The patch channel can technically push EXEs and DLLs (FileTypes 1 and 4) with only a CRC check, so Ambrose's patchserver must refuse to serve executables.
- **Prerequisites:** A clean-room CRC-32 helper in src/common, tested against real install files; A KIWAD writer (the reader side exists as a scaffold); A LatestFileList.bin/.xml writer; A separate client install folder pinned to r806919; A reserved custom template-ID range, checked against the retail TemplateManifest; A BINd encoder driven by the type registry, so template XML is produced from typed data

### L2 - Content toolchain that builds patch overlays from our own sources

- **Enables:** A repeatable, reviewable workflow. One source (world DB rows plus declarative content files) generates both the server SQL and the client overlay WAD (templates, TemplateManifest entries, locale, zone spawn and trigger XML). Pieces: a Keira-style editor that shows diff SQL and writes pending_db_world updates; a WoW Database Editor-style reload command sent to the dev server; a read-only template browser and spell inspector; a validator that rejects unknown classes, properties or ID collisions; GM build-session commands that write SQL. This is the "add almost anything data can express" rung.
- **Cannot do:** Everything L1 can't do. It also can't author new Gamebryo 20.6 models by itself; that needs a NIF writer or exporter, which is L4-grade format work.
- **Effort:** Medium to high. It is ongoing tooling work: roughly months for a usable editor plus exporters, and it grows with each content type.
- **Technical risk:** Medium. The BINd encoder has to round-trip byte-for-byte with retail files. Keeping one schema across editor, validator and docs takes discipline.
- **Legal and policy risk:** Low to medium. Our original content and tools are fine. Generated overlays are built per user, never committed. Keira3 (AGPL), WoW Database Editor, WowPacketParser (GPL) and katsuba may only be studied as patterns, never copied. Any capture-to-SQL tool that needs live KingsIsle captures carries terms-of-service and account-ban risk; decide that policy separately.
- **Prerequisites:** L1 proven with a real client; A type-registry dump from the user's own exe; template_extractor and wad_extractor implemented

### L3 - Launcher plus runtime client hooks (ffrunner-style)

- **Enables:** The things data can't reach, done at runtime on the user's machine: new message handlers, exposing extra functions to Lua, new UI hooks, quality-of-life fixes (FPS cap, resolution), a DXVK/Vulkan wrapper, redirecting WAD loading to an overlay folder (avoiding Root.wad rebuilds), and debug tooling. It follows the FusionFall/OpenFusion pattern (environment-variable feature flags, DXVK, hashed-manifest repair).
- **Cannot do:** Change code paths the hooks can't reach cleanly. Every client revision breaks the hooks, so the revision must stay pinned. It is not a new engine: renderer and format limits stay.
- **Effort:** High, and it grows per feature. Each hook needs reverse engineering in Ghidra or radare2 of a 56 MB x64 exe with no symbols (it does contain 635 source paths and 2,223 RTTI class names to navigate by).
- **Technical risk:** High. Crashes are fragile and version-locked. Anti-cheat or integrity checks were not investigated. Injected code is hard to test without running the client.
- **Legal and policy risk:** Highest of the practical rungs. It modifies KingsIsle's copyrighted binary in memory, which is typically forbidden by the EULA and may raise DMCA 1201 anti-circumvention questions depending on what is bypassed. Distributing a patched exe is off the table. A hook DLL containing only our own code, applied locally, is the least-bad form. Security teams also treat injectors as malware-like.
- **Prerequisites:** A clear, scoped list of features L1/L2 provably cannot do; Pinned revision r806919; A documented project policy on binary modification (the current rules only cover not committing extracted files); Legal review before any public release

### L4 - Standalone native C++ viewer/editor for original assets

- **Enables:** Our own renderer that reads the user's install: a zone viewer, placement editor (spawns, triggers, teleporters) with gizmos, model and animation preview, and a spell and template inspector. It becomes the content-authoring front end for L2, and it is the foundation for any future client. It also yields a nifcensus acceptance tool.
- **Cannot do:** Play the game. It doesn't reproduce KingsIsle's exact look (shaders and materials are rewritten), full particle and cinematic parity, the GUI behavior, Lua minigames, or networking. It doesn't create new model formats on its own.
- **Effort:** High. Parse about 160 block types across 5 NIF/KF versions (81% are 20.6 NiMesh/NiDataStream), textures (DXT1/3/5 DDS plus NiPixelData), skinning, KFM 20.6.0.0b (not covered by kfmxml) and evaluator/B-spline animation. Roughly 3 to 6 months to a static zone viewer, and more than a year for skinned animated characters and particles (my estimate, not measured).
- **Technical risk:** Medium to high. No public library loads 20.6 NiMesh plus evaluators; NifSkope's support is partial and GPL. nifxml's layouts haven't been tested against a full parse of Wizard101 files. The KI shader look and particles are hard to match.
- **Legal and policy risk:** Low to medium. It loads the user's own install and ships no assets. nifxml (MIT) may be used as a format spec. Never open the leaked Gamebryo SDK repos  and don't reuse the Emergent-copyrighted KI_TextureBlender.hlsl. Screenshots and videos of KI assets are normal fan use but shouldn't be branded as official.
- **Prerequisites:** The shared ObjectProperty/BINd codec and type registry; A clean-room NIF reader generated from nifxml; A renderer choice (bgfx or OpenSceneGraph) plus Dear ImGui; A private repo

### L5 - Full replacement client (the 'FusionFall Three.js' dream)

- **Enables:** True 'no limitations': new message types, systems, UI, shaders, platforms (Emscripten web build, Linux, Mac), new asset formats, and removing all dependence on the KingsIsle executable.
- **Cannot do:** Remove the dependency on KingsIsle's assets. It still needs the user's own install (34.6 GB unpacked, 550,616 entries). Converted assets can't be hosted, so the only web-friendly form, a JS/three.js client with pre-converted glTF, conflicts directly with the no-redistribution rule. It can't reach parity quickly.
- **Effort:** Very high: multi-year and many-contributor by precedent (OpenMW about 17 years and still pre-1.0; xoreos pre-alpha at 16 years; FusionFall Legacy 5 years and never shipped). Scope includes client handling for 971 messages; the GUI Window/Control system (960 .gui files, 8 SWF screens); the cinematic action and behavior vocabularies (3,985 cinematics, 1,824 state machines); 158 Lua scripts and their API; 9 minigames; ODE-like collision (BCD/NAV); audio (Miles-driven MP3/OGG/WAV); and combat presentation.
- **Technical risk:** Very high. Most client behavior isn't in the data and has to be reverse engineered from observation. Scope creep is the historical killer.
- **Legal and policy risk:** Medium to high. The code is ours and it reads the user's own files, which is defensible like OpenMW. But a polished playable Wizard101 clone is a high-profile trademark and DMCA target (FusionFall Retro and Legacy were taken down on 2020-04-16). Any web version that serves assets would be infringement.
- **Prerequisites:** L4 complete (loader, animation, particles); The server stable enough to act as the protocol oracle; Explicit scope: one gameplay loop on one pinned revision; A private repo and a legal posture decision

## Recommendation

1. **Commit to L0 plus L1/L2 as the answer to "add anything".** Data is where nearly all Wizard101 content lives (templates, zones, spells, locale, GUI, cinematics, Lua), and the patch pipeline is checked only by CRC-32 against a list our server writes. That gives new items, NPCs, zones and quests without touching the executable.
2. **Before building L2, run the two first experiments below.** They test whether a rebuilt WAD is accepted and which WAD wins when two contain the same path. The answer to the second decides the overlay design (a small custom WAD versus rebuilding the 295 MB Root.wad).
3. **Treat L4 (a native C++ viewer/editor) as a long-running side track.** It pays for itself as the spawn and zone authoring tool, shares the BINd codec with the server, and is the only honest path toward L5. Build it clean-room from nifxml, and never from GPL loaders or the leaked Gamebryo SDK.
4. **Use L3 hooks only when a named feature provably needs them,** and only after the project writes a policy on runtime binary modification. Never distribute a patched exe, and never let the patchserver serve executables.
5. **Don't plan around a Three.js/web client.** The FusionFall example doesn't exist publicly, no NIF loader exists for any web engine, and serving converted assets breaks the no-redistribution rule. A later Emscripten build of the L4/L5 C++ core is the only web route that fits the project rules.

**Guardrails for every rung:** keep the client on a separate install pinned to r806919; build overlays per user from their own install; reserve a custom template-ID range; keep all repos private; avoid KingsIsle branding.

## First experiments

- [ ] Offline CRC round-trip (no client run): take one WAD such as Aquila-AQ_Z00_Hub.wad, decode it and re-encode it byte-for-byte with our own KIWAD writer. Recompute entry CRCs, HeaderSize, HeaderCRC, the gzip header size and file CRC, and confirm they match the retail LatestFileList.bin values. This proves the manifest builder before any live test.
- [ ] Patch acceptance test (needs the user's approval, since it runs the client against a local server): a copy of r806919 in a separate folder; PatchConfig.xml pointed at localhost; a minimal patchserver answering MSG_LATEST_FILE_LIST_V2; one locale string changed in Root.wad; the manifest rebuilt. Pass if the client downloads the WAD and shows the changed string.
- [ ] Override-order test: add a tiny new WAD, listed in the manifest, containing the same Locale path with different text. Check whether it overrides Root.wad, is ignored, or is never downloaded. This decides between an overlay WAD and rebuilding Root.wad.
- [ ] New-template test: add one ObjectData template (a reskin of an existing item, reusing an existing class) with an ID in a reserved custom range, plus a TemplateManifest entry. Have the dev server spawn it. Also record what happens when the server sends an ID the client doesn't know (crash, placeholder, or ignore).
- [ ] Handshake CRC probe: in a local capture of our own dev server session, check whether MessageManager module or service CRCs (GetServiceCRC) are sent at login. This tells us whether editing *Messages.xml is even possible.
- [ ] Offline nifcensus spike: a clean-room parser generated from nifxml, run over every .nif/.kf in Root.wad plus Mob-WorldData.wad, reporting unknown blocks and leftover bytes per block type. This measures how well nifxml matches the 20.6 NiMesh/evaluator files before committing to L4.
- [ ] Static-analysis check (Ghidra/radare2, read-only): find cross-references from 'VerifierFilter: digital signature not valid' and from the Crypto++ RSA verifier to see whether any patch, WAD or template loading path calls signature verification.

## Verification of load-bearing claims

- **Held up** (medium confidence). That FusionFall has no public Three.js/new-renderer port. Based on 0 GitHub code hits, 17 OpenFusionProject repos with no renderer, and web searches. Ask the user where they saw the claim; it may be a private Discord work-in-progress.
- **Held up** (medium confidence). The patch file list and WADs carry no signature and are checked only by CRC-32 (poly 0xEDB88320, init 0, no final XOR). The CRC values are measured; the absence of any RSA check on the patch path is inferred from strings only.
- **Corrected** (medium confidence). It is untested whether a rebuilt or newly listed WAD is actually accepted and loaded by the client.
  - Correction: For WADs that replace existing ones, this is not untested. A third-party mod (Wizard101 Renewal Project) shipped rebuilt WADs overwriting stock ones in GameData, and the retail client loaded them once patching was skipped. So the client does not reject a modified WAD on disk; the patcher just reverts it when the manifest CRC or size doesn't match. What is still unproven: (a) whether the patcher accepts a rebuilt WAD when a custom LatestFileList lists it with matching CRC and size. The "DownloadSegment ... CRC mismatch" string suggests yes, if the values match. (b) Whether the client loads a WAD with a brand-new name listed there. Nobody in the project has tested either one against r806919.
- **Corrected** (medium confidence). Which of two WADs containing the same path takes precedence (unknown).
  - Correction: Mostly known. The client names a resource as "|world|zone|path", which resolves to exactly one WAD (world-zone.wad), so the same path in two WADs is two separate resources and neither wins. Precedence only matters in three cases:
(a) High-detail vs normal equipment WADs, chosen by the texture-detail setting.
(b) Classic Mode, which uses explicit file-to-WAD maps (ClassicModeZoneList.txt and ClassicModeElementList.txt).
(c) Lookups that don't name a WAD. These go through ResourceManager::Search over a fixed list of shared WADs, plus a locale override step (Overrides.xml, m_fullReplacement) and FileSystem mounts ordered by m_nPriority.
Only the exact order in (c) still needs disassembly to confirm.

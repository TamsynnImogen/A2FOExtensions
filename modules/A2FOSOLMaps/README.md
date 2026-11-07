# A2FOSOLMaps

**Current bring-up: Phase 2j.** SOL discovery/MapDetails works, Legacy mission
DSL/DRL is temporarily suppressed, and placed SOL objects are now instantiated
directly from the parsed ODF/team/label/Matrix34 records through Armada's native
`GameObjectClass::Find` / `GameObjectClass::Construct` path. Cameras, paths,
points and mission-side Legacy tables are not enabled yet.

Experimental Star Trek: Legacy `.sol` map compatibility for Armada II / Fleet Operations.

## Current phase

Phase 2j implements the browser/parser work plus the object-only runtime bridge:

- Legacy BinaryMode SOL v3021 tagged-record parsing.
- Map metadata, bounds and start-location decoding.
- Common placed-object decoding (ODF, id, position, team, instance name and Matrix34 transform).
- Extension-root indexing and Fleet Ops KnownMaps integration through `bzn\\*.sol`.
- Native `MapDetails` synthesis for the map browser and selected map.
- SOL probe-result synthesis so Fleet Ops receives a valid reader/context/details triple without parsing Legacy front matter as A2 BZN.
- Early native `EmptyMission` and empty GameObject-list scaffolding.
- **Parsed-object spawning:** each SOL object is resolved with `GameObjectClass::Find`, constructed with its Legacy Matrix34/team/instance label via `GameObjectClass::Construct`, then published to the current mission.
- Missing ODF classes are logged and skipped instead of aborting the entire map.
- A2-only craft-class/post-mission tables and Legacy DSL/DRL execution remain suppressed during object-only bring-up.

The parser has been validated against `mpbajor_all.sol` (Legacy format 3021): 8 start locations and 19 placed map objects are decoded.

For the current bring-up phase, place SOL maps in the normal `bzn/` directory.

## Temporary mission-script isolation

During SOL runtime bring-up, Legacy mission scripts are deliberately suppressed when native Fleet Ops `MapDetails` are created. The parser still records the original DRL/DSL filename (for example `mpbajor_all.sol` references `CoopWaveR.drl`), but the native mission-script field is published empty. This lets map/world loading be tested independently from Legacy mission-script compatibility.

## Runtime status

Phase 2j intentionally no longer asks A2/FO's polymorphic `GameObjects::Load` to
consume Legacy object payloads. On the Bajor test map that native path created
only the first `ambientlight` and then diverged, producing a visually blank map.
The runtime now uses the parser's complete 19-object table as the authoritative
bridge input.

This is still object-only compatibility. Legacy cameras, paths, points, mission
state and DSL/DRL behaviour remain future work, with the intended mission path
being the MMM/A2FOMissions Lua compatibility layer rather than native Legacy
script deserialization.

`A1Compat.dll` remains optional. If its STA1 policy is active it owns the shared
MapDetails/LoadGame hooks and delegates SOL requests; otherwise A2FOSOLMaps owns
the checked call sites directly.

## MapDetails field mapping note

Fleet Ops' native in-memory `MapDetails` layout does not use the same order as
Legacy's serialized SOL prefix.  The native ArmadaL serializer confirms
`MPDFilename` at `+0x24`, `MPDDescription` at `+0x34`, and `MPDFullpath` at
`+0x44`.  SOL discovery translates into those native offsets explicitly; the
FO full path is published as `bzn\\<basename>.sol` while the Legacy embedded
`sol\\...` path remains parser metadata.


## Phase 2b hook correction

The Jan_B September 2026 ArmadaL image encodes the `AiMission::LoadMission` call at RVA `0x00202608` as `E8 F3 ED DF FF` (target RVA `0x00001400`). Phase 2a accidentally expected `E8 F3 ED FF FF`, so the SOL module rejected itself before runtime loading. Phase 2b corrects that signature and preflights all related call sites before applying any patch, preventing a failed initialization from leaving dangling calls into an unloaded module.

### Phase 2d object-only bring-up

For Legacy SOL maps the outer ArmadaL LoadGame compatibility probe is allowed to
pass version 3021 without rewriting the FileReader's real stream version.  The
runtime object bridge still consumes the parser-validated SOL object table.
Legacy's serialized `EmptyMission`, `.dsl`, and `.drl` payload is deliberately
not handed to A2/FO yet: A2FOSOLMaps constructs a native `EmptyMission` directly
and skips the first two native post-mission tables.  This mode exists only to
isolate map/object compatibility.  Legacy cameras, paths, points, and mission
scripting will be restored through the later MMM/Lua compatibility path.


## Phase 2e/2f native-probe companion synthesis

The phase-2d trace still stopped at ArmadaL RVA `0x00202f28` (`mov eax,[esi+4]`)
with `esi == 0`.  The important new evidence is that the probe returned a valid
3021 `FileReader` at result `+0x0c`, but result `+0x10` and `+0x14` were both
null.  Disassembly shows that the native A2/FO probe performs several
format-specific reads *before* its final version gate.  SOL 3021 diverges in
that metadata, so the probe exits through an earlier failure path and never
reaches the branch that phase 2d patched.

Phase 2f therefore stops trying to make the native BZN metadata probe understand
SOL.  It lets the probe open the file and retain the real 3021 `FileReader`, then
synthesizes the two normal accepted-path companions that the caller requires:

- the native 0x2c default load-context object (constructor at ArmadaL RVA
  `0x001da980`, whose tag is `NONE`), and
- the already-supported native `MapDetails` object populated from the parsed SOL.

Only when the reader, context and MapDetails are all non-null does the bridge
present version 2177 to the caller and mirror the native success byte.  The
actual stream remains version 3021 and the later object bridge still realigns
the same live `FileReader` to the parser-validated SOL object table.

This avoids both the obsolete temporary opcode rewrite and the null dereference
at `0x00202f28`.


## Phase 2f
For SOL object-only bring-up the module now publishes a native EmptyMission during LoadGame probe synthesis. Fleet Ops calls the current AiMission from its setup path before the later mission load call site, so waiting until that later hook left AiMission::current null and crashed at ArmadaL+0x1452. Legacy DSL/DRL data remains intentionally bypassed.


## Phase 2g early GameObject-list shell

The phase-2f trace proves the probe synthesis and early `EmptyMission` are now
valid, but setup next enters ArmadaL RVA `0x000d2980` before
`LoadGame_MainLoad` reaches the serialized SOL object table.  That routine
iterates `GameObject::objectList` and crashed at RVA `0x000d299c` because the
global owner pointer at RVA `0x00361084` was still null.  Native A2/FO normally
creates that list lazily in the first `GameObject` constructor.  SOL probe
synthesis has not constructed a GameObject yet.

Phase 2g therefore publishes the same empty list shell used by Armada's native
constructor before returning a compatible probe result: a 12-byte owner, a
12-byte self-linked sentinel at owner `+0x04`, and a zero count at owner
`+0x08`.  The normal native `GameObject` constructor can append to this list
later and Armada's normal cleanup routine understands and frees the layout.
No placeholder GameObject is created, so the SOL object table remains the first
source of actual world objects.

## Phase 2j MainLoad object-only recovery

The phase-2g map-editor test no longer faulted.  The probe successfully produced
a live 3021 reader, synthesized load context and MapDetails, early EmptyMission,
and the empty GameObject list shell, but `LoadGame_MainLoad` returned false before
its ordinary `GameObjects::Load` call site.  Fleet Ops therefore unwound cleanly
back to the main menu rather than crashing.

Phase 2j wraps the outer GameSetup -> LoadGame_MainLoad call.  Normal BZN maps
remain untouched.  For a selected SOL it first lets the native main loader run;
if that succeeds, or reaches the existing SOL GameObjects bridge, its result is
preserved.  If it fails before GameObjects are reached, the wrapper records the
reader cursor, realigns the same live reader to the parser-validated SOL object
count record, reapplies parsed world bounds, and invokes Armada's native
`GameObjects::Load` directly.  The temporary EmptyMission remains active and
Legacy DSL/DRL/post-mission data is still intentionally excluded.

This is an object-only compatibility experiment rather than a claim that the
A2/FO front matter is equivalent to SOL.  The new diagnostic line reports the
native failure cursor and whether direct SOL object loading succeeded, allowing
the next incompatibility to be isolated without another blind crash cycle.


## Phase 2j object bridge

Legacy SOL polymorphic object payloads are no longer passed to Armada II's
`GameObjects::Load`. The parser already extracts each placed object's ODF,
instance label, team and 3x4 transform, so Phase 2j resolves the ODF through
`GameObjectClass::Find`, constructs the native object with
`GameObjectClass::Construct`, and publishes it to the temporary `EmptyMission`.
Missing ODFs are logged and skipped so a content problem does not abort the
whole map. Mission DSL/DRL, cameras, paths and other post-mission Legacy tables
remain deliberately disabled during object-only bring-up.


### Phase 2j AI-null publication guard

The object-only recovery path bypasses A2/FO front matter that normally creates the global AI singleton before objects are published to the mission. Some object types (the Bajor planet is the first observed case) call `GameObject::IsAIControlled` during `AiMission::AddObject`. The stock helper blindly dereferences the AI singleton and crashes when it is null. Phase 2j makes that helper null-safe only in semantics: when AI and its team table exist it calls Armada's original AI-team query; while they are absent it reports the object as non-AI-controlled. This is intended for SOL/map-editor bring-up and does not synthesize AI state.


## Phase 2k: parser-spawned Nebula simulation guard

The parsed-object bridge now reaches all 18 resolvable Bajor objects, but the
first `mnebula12` encountered by the simulation loop crashes in native
`Nebula::Simulate` at Armada RVA `0x000a3c96`.  `Nebula::Load` normally fills
the per-instance serialized project/AiPath companion at object offset `+0x188`;
Phase 2j deliberately bypasses that incompatible Legacy payload, leaving the
field null.  Phase 2k hooks native `Nebula::Simulate`: ordinary/native nebulas
continue through the original gateway, while a selected SOL nebula with a null
`+0x188` companion runs only the common `GameObject::Simulate` path.  This is a
bring-up compatibility fallback, not a final translation of Legacy nebula path
state.

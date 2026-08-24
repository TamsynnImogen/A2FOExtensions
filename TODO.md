# TODO

- Re-enable the shell display monitor only after its fullscreen handling can be
  proven not to alter legacy menu and modal-dialog layouts.

## Photon and Quantum Torpedo Stores

[PLANNED]

* [ ] Allow one weapon ODF to declare both `photonTorpedoCost` and
  `quantumTorpedoCost`. The pre-fire check must require sufficient ammunition
  in both pools, and a committed trigger must deduct both costs atomically and
  exactly once. Preserve the existing single-pool and no-cost behaviours and
  add tests for insufficient pools, multi-projectile triggers, and invalid
  values.

## Point-Defense Firing Cycles

[IMPLEMENTED, REQUIRES MANUAL VALIDATION]

Extend both `classLabel = "PointDefenseLaser"` and
`classLabel = "OrdnanceDefenseWeapon"` through a dedicated optional module so
they support the complete numbered firing-cycle contract:

```ini
shotDelay0 = 0.1
shotDelay1 = 0.1
shotDelay2 = 2.0
saveFireCyclePoint = 2
shotCycleResetTime = 15.0
```

The existing unnumbered `shotDelay` support must remain the backwards-
compatible default whenever no `shotDelayX` entries are present. The new
module must add timing only and preserve each classlabel's native target
selection, interception, hit-chance, reload-modifier, and attack behaviour.

* [x] Parse contiguous `shotDelay0..X` values with safe bounds and validation;
  numbered delays take precedence over unnumbered `shotDelay`.
* [x] Implement `saveFireCyclePoint` and `shotCycleResetTime` with CannonImp-
  compatible semantics without reusing CannonImp's incompatible object/class
  layouts.
* [x] Keep cycle position and idle/reset timing per weapon instance rather
  than mutating the shared weapon class.
* [x] Advance the cycle once per successful point-defense shot/interception;
  verify how `OrdnanceDefenseWeapon` handles multiple candidates in one tick.
* [x] Preserve native reload modifiers for every selected numbered delay.
* [x] Serialize and restore cycle state across save/load, and clean sidecar
  state during destruction and ownership/status transitions.
* [ ] Verify deterministic single-player and multiplayer behaviour in game.
* [x] Add focused host tests and document precedence, reset behaviour, invalid
  values, and compatibility fallback.
* [x] Fix A2FO Arc Lab rendering imported SODs mirrored. Audit the SOD-to-viewer
  coordinate-system/handedness conversion, triangle winding, UV orientation,
  hardpoint positions, and displayed fire-arc directions together; validate
  locally without changing any saved ODF values.
* [ ] Confirm the corrected Windows build against the tester's asymmetrical,
  labelled `fed_enterprise.sod` asset when it is available again.

## Dynamic Ambient Swarms

[IMPLEMENTED, REQUIRES MANUAL VALIDATION] `A2FOSwarmSystem.dll` provides
sparse `swarm0..63` definitions on ordinary host ODFs. Members are native
render instances with sidecar movement state and never become selectable,
collidable, AI-controlled, saveable map units. See
`modules/A2FOSwarmSystem/README.md`.

* [x] Parse multiple independent swarm definitions through native
  `ParameterDB`, including native string-vector launch and interaction lists.
* [x] Spawn shared-model visual instances with bounded per-definition and
  per-host counts, randomized routes/speeds, interaction dwell, and optional
  return cycles.
* [x] Keep movement host-local, refresh animated hardpoint destinations, and
  reconstruct automatically after load while cleaning up removed hosts.
* [x] Keep members outside the host/model combined bounding radius with swept
  segment checks and tangential sliding, without enabling engine collision.
* [x] Prevent same-definition clumping using scaled visual bounds, persistent
  dwell offsets, and bounded allocation-free separation passes.
* [x] Limit concurrent interaction/return hardpoint reservations and require a
  roaming leg after every visit so authored points cannot collect the swarm.
* [x] Unit-test deterministic random movement, roaming bounds, host transform
  round trips, smooth arrival, host exclusion/tunnelling, member separation,
  and visual facing transforms.
* [ ] Validate launch, roaming, interaction, return, visibility, destruction,
  and save/load reconstruction on a stationary station in Fleet Operations.
* [ ] Validate translating, rotating, and warping hosts, multiple swarm types,
  malformed commands, missing hardpoints, and missing models in game.
* [ ] Benchmark increasing members and simultaneous hosts; record CPU frame
  cost, draw calls, RAM, and VRAM before changing the documented safety caps.

## Architecture and Refactoring

* [x] Make native destruction handlers declare which ODF fields they require;
  the core itself does not hard-code wreckage fields.
* [x] Split HybridBuild and Fleet Ops `info.ini` defaults into dedicated
  `A2FOHybridBuild.dll` and `A2FOInfoIni.dll` modules. The core retains only
  shared/timing-sensitive dispatch hooks, and FeaturePack retains shared queue
  and ResearchStation hook ownership through a callback bridge.
* [x] Rename the `ODFRecursive` source folder to `A2FOFeaturePack`.
* [x] Document hook ownership and add a core-owned native destruction dispatcher. See `docs/architecture.md`.
* [x] Add compatible API revision/capability checks for native modules.
* [x] Make native-module registration transactional so failed initializers
  cannot leave dangling callbacks.
* [x] Add the optional shared `A2FORGBTextures.dll` module.

## Build and Repository Cleanup

* [x] Remove `ExampleModule.dll` from release builds while retaining it as an SDK example.
* [x] Add `.gitignore` entries for:
  * `build/`
  * log files
  * generated DLLs and executables
  * temporary compiler files
* [x] Replace temporary Stage notes as the primary documentation with `docs/architecture.md`, retaining the old notes as labelled history.

## Performance and Diagnostics

* [PERFORMANCE] Add optional profiling for:
  * hook invocation counts
  * callback execution time
  * module execution time
  * ODF and FPQ indexing and lookup time
  * repeated `ParameterDB` lookups
  * object update loops
  * formation and AI searches
  * construction-footprint checks
  * texture loading, conversion, and upload
  * model and texture duplication in memory
  * logging volume and formatting overhead
* [PERFORMANCE] Record callback frequency as well as total and maximum duration,
  so a cheap callback invoked excessively is still visible as a hotspot.
* [PERFORMANCE] Add a compact end-of-session or on-demand profiling report with
  per-hook and per-module totals.
* [PERFORMANCE] Establish repeatable baselines against the clean Sigma engine
  before changing hot paths. Optimise only hotspots confirmed by profiles.
* [PERFORMANCE] Ensure profiling and verbose logging can be disabled in release builds.
* [x] Bound derived emissive `D3DPOOL_MANAGED` composites to eight states per
  material and a 96 MiB global target (retaining one live state per material),
  store sparse mostly-black source maps losslessly, and remove the redundant
  full-resolution mip-upload copy.
* [x] Retain only destruction-handler ODF fields after class-load dispatch,
  omit classes without destruction commands, clean TextureVariants' per-craft
  render sidecars on native Craft cleanup, and remove one heap allocation per
  ambient-swarm member.
* [x] Add a non-invasive 64-bit Linux telemetry helper for Wine process CPU,
  PSS/private memory, threads, handles, NVIDIA load, and GPU-wide VRAM.
* [x] Discard inactive TextureVariants damage policies and avoid collecting
  class mesh/faction-node caches when their corresponding ODF policy is absent.
* [x] Retain only root DDS and flattened TGA routes in A2FORGBTextures, time
  the persistent-shield global safety scan instead of walking every object on
  every frame, and bound per-class cache diagnostics.
* [x] Read only the 18-byte header for already-compatible true-colour TGAs,
  keep a key-only compatibility cache, and reserve full-file decoding for
  formats which actually require conversion.
* [x] Add revision-15 masked Craft-event registration; register
  TextureVariants for cleanup only, skip EnergySystems simulation globally
  when no store policy exists, and bypass inactive DirectionalShields/Turrets
  work before reading Craft fields.
* [x] Add revision-16 masked weapon-trigger registration and migrate FireArcs,
  NormalWeaponTech, and Turrets to precheck-only callbacks, removing three
  no-op cross-DLL calls after every accepted weapon shot.
* [x] Avoid negative animated-hardpoint node caches and their linear searches
  for ordinary models without matrix-animation channels.
* [ ] Run repeated shell -> match -> shell -> same-match telemetry sessions and
  investigate only memory, thread, or descriptor growth that remains after the
  same scene has settled a second time. An initial 8-second startup capture
  showed stable thread (32), descriptor (163), and GPU-wide VRAM
  (466--467 MiB) counts after loading, but was too short to assess retention.

## ODF Semantics and Compatibility

### Recursive ODF Precedence Across Mod Roots

[BUG]

An active child mod must override matching ODF basenames from shared `Data` and
all parent mods regardless of whether either file is in a built-in ODF folder
or a dynamically registered recursive folder.

Observed failure with the Noxter addon:

```text
active child: Noxter/odf/other/races.odf
parent:       Fleet Ops 4.0/odf/system_system/races.odf
result:       the parent's recursive races.odf was selected
```

Renaming the parent's directory to the built-in `odf/system` layout avoids the
recursive alias path, but is only a workaround. The recursive lookup must not
give a parent file priority merely because its directory was registered by the
extension.

* [ ] Reproduce the failure with a minimal Data -> parent -> active-child test
  fixture containing one shared ODF basename.
* [ ] Audit native hash entries and recursive winner selection when the parent
  copy is recursive and the child copy is in a built-in directory, and in the
  inverse arrangement.
* [ ] Apply mod-root priority before directory type: active child, then nearest
  parent through the full parent chain, then shared Data.
* [ ] Preserve primary-root and loose-before-packed precedence only as
  tie-breakers within the same mod root.
* [ ] Add regression coverage for built-in/built-in, recursive/recursive,
  built-in/recursive, recursive/built-in, and loose/FPQ combinations.
* [ ] Extend diagnostic lookup logging to include the winning physical root,
  mod priority, directory, and loose/packed status.

[API] Retain Jan_B's legacy magic-value behaviour for compatibility, while
adding explicit module-owned commands. When both forms are present, the
explicit command takes precedence and the legacy value is only a fallback.

Initial replacements to investigate:

```text
cocoon = "8472_cocoon2"

hullDamageMultiplier = 2.0
shieldDamageMultiplier = 1.0

canFireWhileCloaked = 1

excludeFromAIFleets = 1
excludeFromAIStrategicGoals = 1

despawnWhenNoChildren = 1
despawnWhenUnableToSpawn = 1
```

These replace or clarify behaviour currently selected through combinations of
the following unrelated fields:

* `crewHitPercent = 0`
* `lodShift = 2.0`
* `avoidanceClass = 151`
* `collisionRadius = 2`
* warp-in speed, special energy, and child-count state

Implementation requirements:

* [ ] Confirm the exact legacy trigger and hook path for every replacement
  against `Engine_Changelog.txt` and the clean Sigma engine.
* [ ] Parse the explicit commands in `A2FOFeaturePack.dll`, rather than adding
  more feature-specific policy to the core proxy DLL.
* [ ] Define deterministic precedence when replacement ODFs inherit or override
  an explicit command.
* [ ] Log deprecated magic-value use once per ODF in diagnostic builds.
* [ ] Test explicit, legacy-fallback, and explicit-overrides-legacy cases.
* [ ] Verify save/load and multiplayer synchronisation for commands which alter
  simulation behaviour.

## Asset Pipeline and Rendering

### Texture Compatibility and Loading

[RESEARCH] Build a compatibility matrix before changing the texture loader:

| Feature | Plain texture | Bump map | Borg texture | Bump + Borg |
| --- | --- | --- | --- | --- |
| Static TGA | [ ] | [ ] | [ ] | [ ] |
| Animated TGA | [ ] | [ ] | [ ] | [ ] |
| DDS | [ ] | [ ] | [ ] | [ ] |
| Animated DDS | [ ] | [ ] | [ ] | [ ] |

Also validate team colours, alpha/lightmaps, texture suffixes, construction
Borgification, rank/replacement model swaps, and save/load.

* [QUICK WIN] Add RLE true-colour TGA decoding before upload through the
  existing texture path.
* [RESEARCH] Determine whether RLE greyscale TGA is useful to supported assets
  and add it only if the engine has a meaningful destination format.
* [ ] Keep uncompressed TGA behaviour unchanged and add malformed/truncated RLE
  input tests.
* [ ] Make the currently intended DDS formats reliable before adding new ones.
* [ ] Repair DDS selection for bump maps, Borgification, texture suffixes, and
  animated sequences.
* [ ] Log unsupported DDS compression clearly and fall back safely.
* [LATER] Consider decoding unsupported modern DDS compression to an ordinary
  runtime texture, documenting the extra load-time and memory cost.

### JSON5 Configuration

[QUICK WIN] Investigate JSON5 for human-authored A2FO module and tooling
configuration. Prefer a maintained parser over a new handwritten parser.

* [ ] Decide which extension-owned configuration files may use JSON5; do not
  silently change the syntax of engine-owned files.
* [ ] Keep the parsed internal representation strict and validate unknown keys,
  types, and ranges with useful source locations.
* [ ] Confirm the selected parser can be built safely for the 32-bit Windows
  target and has an acceptable licence and dependency footprint.

### SODX Model Format

[DESIGN] Define SODX as a documented glTF 2.0/GLB profile with standard glTF
geometry, hierarchy, transforms, preview materials, and animation. Store
Armada/Fleet Operations semantics in a versioned extension, tentatively named
`A2FO_model_sodx`, rather than creating an unrelated closed binary container.

Generic glTF tools should still be able to render the ordinary model data when
they do not understand the SODX extension. Runtime-required semantics belong in
the extension; editor notes and nonessential provenance may use glTF `extras`.

The shared representation must cover:

* meshes, vertex groups, UVs, and LODs;
* node hierarchy, transforms, and original SOD node types;
* hardpoints, target points, docking paths, and build nodes;
* Armada materials, blending, lighting, culling, and texture roles;
* diffuse, bump, Borg, team-colour, alpha, and animated textures;
* transform animations, texture animations, and event bindings;
* collision, shield, selection, construction, damage, and Borg branches;
* emitters and emitter parameters;
* rank and replacement-model relationships;
* source SOD version and opaque legacy data needed for lossless round trips.

#### SODX 0.1: Offline Reference Toolchain

The first milestone deliberately does not require Fleet Operations runtime or
renderer changes:

```text
SOD  ---\
         > shared model representation ---> SODX/GLB
glTF ---/                              `---> compiled SOD
```

* [ ] Extract the existing SOD/glTF viewer's common model representation into
  a reusable `sodx-core` library rather than coupling the format to viewer-only
  structures.
* [ ] Document coordinate, unit, transform, matrix, winding, and texture-name
  conversions explicitly.
* [ ] Write a versioned extension schema and validator.
* [ ] Implement deterministic static-model SOD-to-SODX export first.
* [ ] Add hierarchy, hardpoints, Armada materials, and texture-role metadata.
* [ ] Add transform and texture animations plus event bindings.
* [ ] Add LOD, collision, selection, damage, construction, Borg, and emitter
  semantics.
* [ ] Implement `SOD -> SODX -> SOD` golden round-trip tests across every
  observed SOD version, comparing hierarchy, matrices, materials, texture
  names, animations, and bounds.
* [ ] Preserve unknown legacy fields losslessly where practical, without making
  an opaque embedded SOD the authoritative representation.
* [ ] Let the existing viewer act as the reference renderer, compiler, and
  validator for SODX 0.1.

#### SODX Authoring and Runtime

* [LATER] Add Blender import/export after the reference converter and schema
  are stable. The add-on should consume the same format rules rather than
  independently redefining SOD semantics.
* [LATER] Decide whether `.sodx` is the canonical extension or a packaged alias
  for ordinary `.glb`; provide explicit importer registration where tools
  reject the unfamiliar suffix.
* [LATER] Add optional mesh compression only after uncompressed round trips are
  reliable.
* [LATER] Load SODX natively in OpenSourced Space RTS.
* [LATER] Investigate direct Fleet Operations SODX loading or a compiled model
  cache. Preserve ordinary SOD output as the compatibility path.

### Decal Mapping and Runtime Overlays

[DESIGN] Add a visual decal-mapping mode to A2FO Arc Lab and a matching
optional runtime module. The first use case is Fleet Operations hull naming
without requiring every imported model and texture set to be rebuilt around
an authored `stlogo` mesh. The same bounded system should later support ship
registries, faction insignia, hull markings, and cosmetic scorch marks.

The authoring workflow should load the resolved ship ODF/SOD and a sample
transparent texture, let the user place it directly on the hull, then export
copy-ready ODF commands describing the selected texture source and ship-local
position, rotation, size, surface offset, and UV mirroring.

[IMPLEMENTED, REQUIRES MANUAL VALIDATION] The first bounded mapped-plane path
now handles subsystem and hull damage decals. Arc Lab previews an alpha texture
on a selected hardpoint with numeric offset/rotation/size controls and exports
copy-ready `<system>ScorchX` commands. The core DX8 renderer draws per-instance
depth-tested quads from live subsystem or hull damage thresholds. Surface
click placement, mirroring, and the broader validation matrix remain below.
Hull-name placements can now follow the selected
`logoFileNames`/`possibleCraftNames` row, with optional per-plane filename
suffixes for split hull artwork.

* [x] Define bounded indexed decal ODF commands whose source follows Fleet
  Operations' selected `logoFileNames`/`possibleCraftNames` row.
* [ ] Add decal preview geometry to Arc Lab with click-to-place surface-normal
  alignment, translate/rotate/resize controls, fine numeric nudging, alpha
  preview, and z-fighting/intersection warnings.
* [ ] Add port, starboard, dorsal, and ventral authoring views plus a mirror
  operation which corrects placement, orientation, and text-reading direction.
* [x] Generate a copyable ODF block for subsystem/hull scorch decals.
* [ ] Optionally insert/update only the extension-owned decal block in a
  selected loose ODF.
* [x] Implement lightweight per-craft textured quads in the core-owned DX8
  renderer without modifying the SOD or competing for a second render hook.
* [ ] Preserve native ship-name row selection, ownership/visibility, cloaking,
  capture, model replacement, destruction, and save/load behaviour.
* [ ] Batch or otherwise bound decal draws and cap the number per craft so the
  feature remains safe in the 32-bit renderer.
* [ ] Validate flat, curved, concave, animated, multi-LOD, and faction/Borg
  texture-variant hulls. Document when multiple smaller planes are preferable
  to one large decal.
* [LATER] Reuse the mapped-plane path for bounded cosmetic impact/scorch marks
  once reliable hit positions and sensible persistence rules are available.

### Nebula Patch Renderer

[IMPLEMENTED, REQUIRES MANUAL VALIDATION] Use
[armadaNebulaPatch](https://github.com/FNSOIDATHQ/armadaNebulaPatch)
as a reverse-engineering reference for Storm3D and the Fleet Operations render
pipeline. `A2FONebulaRenderer.dll` now ports its DX8 shader behaviour through
the checked A2FO API, without its competing startup proxy, hook toolkit,
MinHook/runtime DLLs, or unfinished DX9 implementation. The four attributed
MIT shader files are packaged under `build/Shaders/dx8`.

Verified useful areas include:

* Direct3D 8/9 mode detection and access to the underlying D3D9 device;
* ordinary and DOT3 mesh render paths;
* Storm3D vertex buffers, index buffers, texture objects, and texture slots;
* vertex declarations and programmable shader injection;
* world, view, projection, camera, material, and directional-light data;
* loader configuration and explicit exported activation functions.

Upstream update audit (2026-08-11):

* Main repository commit
  [`d01c838`](https://github.com/FNSOIDATHQ/armadaNebulaPatch/commit/d01c8384b9a8e5ee16f9a9540936f3b852cb916c)
  advances `shaderPlusArmada` from `9c83242` to `f867372`, exposing four
  previously unlinked commits: FX/HLSL support, a complete DX9 VS/PS draw
  prototype, directional-light/camera data and reset handling, and a dummy
  standard-MeshVB path. The DX8 source is byte-for-byte unchanged, so this
  update does not directly improve the current A2FO DX8 renderer or bloom.
* Useful recovered DX9 bindings include the selected device through device
  vtable slot 48/token 3, native vertex/index-buffer getters, a 68-byte mesh
  vertex declaration, active lights at `ST3D_GraphicsEngine + 0x60`, selected
  device/index fields at `+0xC0/+0xCC`, and the camera pointer at `+0xFC`.
  Treat these as unverified upstream observations until converted to checked
  RVAs, signatures, and bounded A2FO structures.
* Do not import the prototype directly. Its `pbrLite.fx` and `dot3.fx` assets
  are absent from the repository; its standard-mesh hook still displays a
  blocking debug message; it replaces whole functions with unchecked absolute
  jumps; directional-light writes are not bounded to the two-element array;
  texture `QueryInterface` references are not released; and reset cleanup only
  releases one of the two FX objects.
* Decision for the current 20-day functionality sprint: retain the stable DX8
  backend. Revisit this snapshot afterward as a reverse-engineering reference
  for a separately engineered, checked DX9 backend rather than a cherry-pick.

Follow-up work:

* [x] Convert the four useful DX8 absolute addresses to checked Armada/Fleet
  Ops RVAs and validate them against the supported PE identities.
* [x] Replace the upstream middle-function early return with an inline gateway
  which disables the pixel shader and resumes Fleet Operations' alpha path.
* [x] Preassemble the custom pixel shader through the shipped
  `D3DX81ab.dll`; the active/reference shaders pass an assembler smoke test
  under Wine.
* [x] Add the six ODF-driven subsystem emissive channels (`emissiveWarp`,
  `emissiveImpulse`, `emissiveShields`, `emissiveLifeSupport`,
  `emissiveSensors`, and `emissiveWeapons`), combine active maps without extra
  model passes, and feed the result to the DX8 shader's second sampler.
* [x] Add opt-in `ART_CFG.h` emissive and bump suffix conventions which inspect
  each SOD material's live diffuse texture, retain explicit ODF/SOD overrides,
  and rebuild only newly bump-enabled meshes through native DOT3 MeshVB.
* [x] Reintroduce bumped-material specular rendering only after it is scoped
  away from Fleet Operations' native DOT3 light-accumulation draws.
* [x] Cover classic/non-DOT3 SODs through scoped fixed-function additive
  stages after native material setup in both MeshVB and legacy non-VB face
  paths; restore all stage state immediately after every draw.
* [ ] Restore selective native emissive framebuffer bloom without re-enabling
  the unstable implementation unchanged. Reproduce and isolate the lost-state
  failure across `dxwrapper -> d3d8to9 -> ReShade`, then replace the private
  render-target/draw-replay path with a checked approach which preserves all
  shaders, streams, render targets, viewports, texture stages, and scene state.
  It must survive ordinary play, UI and edit-mode transitions, repeated map
  loads, Alt-Tab/device loss and reset, and operation with and without ReShade.
  Keep the current direct self-lit emissive material path as the safe fallback
  and make bloom opt-in until those tests pass.
* [ ] Manually validate emissive texture loading, UV alignment, subsystem
  disable/repair transitions, device reset, and Fleet Ops 4 bloom response.
* [ ] Extend subsystem emissive resolution to textures stored only in FPQ
  archives; the initial D3DX loader accepts loose image files.
* [ ] Manually validate the corrected early-hook build in normal play,
  nebulae, transparent geometry, map-editor
  crystal text, device loss/reset, and hardware which rejects pixel shader 1.4.
* [ ] Record the discovered Storm3D structures and calling conventions in a
  versioned bindings layer with sources and confidence levels.
* [ ] Convert any additional DX9 or standard-mesh addresses to RVAs/signatures
  and validate them against the clean Sigma executable and supported Fleet
  Operations builds before installing hooks.
* [ ] Audit the prototype's per-draw texture `QueryInterface` lifetime, shader
  reset/recreation, directional-light bounds, error handling, and standard-mesh
  debug path before adapting any renderer logic.
* [ ] Locate or recreate the referenced `pbrLite.fx`; it is not present in the
  current shader source repository.
* [ ] Use the recovered mesh and texture paths to support renderer profiling,
  texture-memory investigation, and eventually richer SODX materials.
* [LATER] Investigate normal and additional material maps after the emissive
  cache and existing texture matrix have completed manual validation.

## Features to Investigate

### Indexed Hull-Mounted Turrets

[IMPLEMENTED, REQUIRES MANUAL VALIDATION] `A2FOTurrets.dll` accepts sparse
`turret0..64`/`turretHardpoint0..64` pairs on Craft-derived parent ODFs. A
linked `classLabel = "turret"` object owns its native weapons and hitpoints
while following the mount with configurable yaw, pitch, slew rates, and rest
angles. See `modules/A2FOTurrets/README.md`.

* [ ] Validate creation, hardpoint placement, target tracking, fire arcs,
  damage, and individual turret destruction on a purpose-built test ship.
* [ ] Validate parent movement, warp, capture/team changes, parent destruction,
  and multiple sparse turret indices.
* [ ] Validate save/load with live turrets and confirm that no duplicates are
  spawned regardless of object load order.
* [ ] Serialize destroyed-mount state so a turret destroyed before saving does
  not return after loading that save.
* [ ] Profile ships with representative and deliberately excessive turret
  counts; every mount is currently a full engine object.
* [ ] Validate multiplayer synchronization before treating the feature as
  multiplayer-safe.
* [LATER] Add optional two-piece yaw-base/pitch-barrel articulation if rigid
  whole-SOD rotation proves insufficient for authored turret models.

### Command Palette Layout Modes

[BLOCKED: COMMUNITY APPROVAL REQUIRED]

Do not implement or repurpose `paletteMode` until the Armada/Fleet Operations
community has had an opportunity to object. Although Fleet Operations does not
expose the setting in its interface, Armada's three native runtime paths remain
functional and an unknown mod or user profile may still depend on them.

Proposed moddable `gui_interface.cfg` contract:

```ini
paletteMode = 0  // repaired automatic popup palette
paletteMode = 1  // Armada II / Fleet Operations static Palette A
paletteMode = 2  // Armada I-style static Palette B
```

Mode `1` must remain the default when the command is absent so existing mods
retain their current Armada II/Fleet Operations interface without data edits.
Modes `0` and `2` require explicit opt-in.

Current evidence:

* [x] Confirm native mode `0` selects the floating `paletteWidth` by
  `paletteHeight` popup path.
* [x] Confirm native mode `1` selects `staticPaletteWidthA`,
  `staticPaletteHeightA`, and `popupPaletteXA`/`popupPaletteYA`.
* [x] Confirm native mode `2` selects `staticPaletteWidthB`,
  `staticPaletteHeightB`, and `popupPaletteXB`/`popupPaletteYB`.
* [x] Live-test modes `0`, `1`, and `2` through the active per-mod profile.
  Mode `2` works; mode `0` currently appears only after a command hotkey and
  is not useful as an automatic popup interface.

Work after community approval:

* [ ] Read an optional `paletteMode` from `gui_interface.cfg` and define its
  precedence over the existing per-user `Settings.xml` value.
* [ ] Preserve mode `1` as the fail-safe value for missing, malformed, or
  out-of-range configuration.
* [ ] Repair mode `0` so selecting an object with available commands can show
  its popup palette without first invoking a command hotkey.
* [ ] Retain hotkey toggle/back-navigation behaviour without making the popup
  dependent upon that hotkey.
* [ ] Keep mode `1` on the static A geometry and mode `2` on the static B
  geometry rather than introducing hardcoded layouts.
* [ ] Test selection changes, empty selections, submenu navigation, multiple
  selection, construction placement, cinematics, save/load, every supported
  aspect ratio, and mods which omit `paletteMode`.
* [ ] Document the compatibility decision and community response before
  enabling the feature in a release build.

### Collapsible Fleet Sidebar

[IDEA, HIGH VIABILITY]

Add an optional in-game sidebar which lists the player's available numbered
fleets. In the first version, a fleet means one of Armada's native control
groups rather than an AI `AIFleet` or a transient `UserFormationFleet`. This
keeps Ctrl-number binding, member removal, selection, camera focus, formation
data, and save/load under the existing `cGroupMgr`/`cGroup` implementation.

The sidebar should use Armada's native `DisplayInterface`, `StandardButton`,
sprite, text, and view-registration paths. Do not implement it as an external
window or a renderer-specific D3D overlay. Layout and artwork should be
moddable through the active `gui_interface.cfg`, with safe defaults when the
new entries are absent.

Initial scope:

* [ ] Confirm the live `cGroupMgr` location inside `cOverViewImp` and document
  the checked layouts needed to enumerate groups and member handles without
  mutating them.
* [ ] Add a dedicated optional `A2FOFleetSidebar.dll` module and a bounded
  native interface view which can be expanded or collapsed with one button.
* [ ] Show only populated groups, initially labelled `Fleet 0` through
  `Fleet 9`, with member count and an optional aggregate health indicator.
* [ ] Select a fleet through the native `cOverViewImp::mSelectGroup` path and
  centre the camera through `mFocusCameraOnShipGroup`, preserving the existing
  keyboard behaviour.
* [ ] Refresh rows safely when ships die, transfer owner, leave a group, are
  replaced, or a saved game restores the overview and group manager.
* [ ] Define GUI CFG rectangles, sprites, colours, row height, expanded width,
  collapsed position, and screen-edge anchoring without hardcoding one race
  interface or resolution.
* [ ] Suppress or hide the panel during cinematics, full-interface hide,
  replays, editor mode, shell screens, and other states where the native game
  interface is unavailable.
* [ ] Ensure the expanded panel owns mouse input only inside its visible
  rectangle and does not interfere with world selection, edge scrolling,
  tooltips, the minimap, or the command palette.
* [ ] Decide whether collapsed/expanded state is session-local or persisted in
  the user's settings. It is presentation state and must never enter the
  synchronized simulation stream.
* [ ] Validate all supported aspect ratios, every stock Fleet Operations race
  GUI, empty/full groups, rapid rebinding, save/load, observer/replay state,
  and a two-peer match to confirm the module remains local UI only.

Possible later additions include moddable fleet names, representative
wireframes, alert/order status, and squad-aware counts. None should require
the Squadron System module, and the sidebar must remain useful with ordinary
native control groups alone.

### Single-Player Mission Selector Redesign

[IMPLEMENTED AND MANUALLY VALIDATED] Replace the fixed native Single Player mission selector with a
dedicated A2FO module while retaining Armada's existing mission-launch and
campaign-progression paths.

Planned scope:

* [ ] Add campaign icons and a banner to the implemented scrollable campaign
  list and overview text.
* [x] Change the selector background with the selected campaign through
  optional moddable image metadata and a safe common/dark fallback.
* [x] Show each campaign's missions in a scrollable list with native
  unlock/progression state.
* [x] Show the selected mission's thumbnail, description, and objectives.
* [x] Provide Back and Start Mission controls which reuse the native shell and
  mission setup paths.
* [x] Scale or adapt the layout safely across Fleet Operations' supported
  resolutions and aspect ratios.
* [x] Host the browser through Armada's native display window and borderless
  Single Player dialog resource instead of a detached application window.
* [x] Keep campaign and mission display metadata/assets moddable without
  turning the selector into an in-game campaign editor.

Explicitly out of scope:

* in-game campaign creation;
* mission star ratings and scoring persistence;
* a per-mission difficulty selector.

INI-defined custom campaigns and BZN lists are implemented. Their `unlocked`
policy is currently static; independent saveable custom-campaign progression
remains future work.

### Ten Independent Resources

[CORE IMPLEMENTED, ECONOMY AND UI INTEGRATION IN PROGRESS]
`A2FOResources.dll` extends the six native resources with independent
tritanium, supply, credits, and collective-connections pools. Starting values,
Producer costs/refunds, native-extension access, and a text-based second
resource-panel row are implemented. The remaining systems must use the new
indices `6..9` directly; Fleet Operations' historical tritanium and supplies
helpers alias native latinum and biomatter and are not suitable fallbacks.

* [x] Store, initialize, read, add, and set all four independent team balances.
* [x] Enforce `tritaniumCost`, `supplyCost`, `creditsCost`, and
  `collectiveconnectionsCost` in Producer affordability, payment, cancellation,
  and refund paths.
* [x] Draw independent balances in configurable `resource_6..resource_9`
  rectangles without pairing any pool with a native resource.
* [x] Append the four non-zero costs to build-item tooltip text.
* [x] Parse Race-specific `Res`, `Tooltip`, `VerboseTooltip`, and `Icon` localization
  keys for all ten resources and consume them for the added row/API; preserve
  Armada's native officer customization.
* [x] Consume the five remaining original-resource short and verbose tooltip
  fields at targeted ResourceComponent entry points; cache added-row
  presentation outside the frame loop.
* [IMPLEMENTED, REQUIRES MANUAL VALIDATION] Apply native `Res` integration for
  crew, dilithium, latinum, metal, and biomatter at the ten targeted ModeInfo
  cost-localization calls. Resolve Team, Race, literal/key localization, and
  tooltip strings only when the Race cache changes; the palette-hot route now
  performs only canonical-key comparisons and returns the cached string. The
  native top resource panel itself renders numeric values only.
* [x] Trace Fleet Operations' high-byte font glyphs and apply canonical compact
  icons to all ten resources. Preserve full Race-specific names in verbose
  costs and reuse only the historical icon artwork for the four independent
  pools.
* [ ] Extend mining end-to-end: resource-node selection, freighter sidecar
  cargo/capacity, collection and delivery, processors, AI targeting, and cargo
  UI. Native `resourcesCanHandle` and freighter storage only cover indices
  `0..5`.
* [ ] Extend direct and route trading: buy/sell commands, route-completion
  revenue, affordability, AI decisions, and trade UI. Native `BUY_RESOURCE`
  parameters only identify resources `0..5`.
* [ ] Add independent `ResourceWeapon` income fields and native-extension or
  script bindings for grants, drains, mission rewards, and cheats.
* [ ] Add four unique icon assets and icon-aware rendering for the resource
  panel, build/research/evolve/trade tooltips, and shortage feedback. The
  current added-cost tooltip layout is text-only.
* [ ] Extend resource gifting/transfers and every selected-object cargo display.
* [ ] Include the new costs in AI economic planning and define how the game's
  resource-cost slider scales them.
* [ ] Add versioned save/load persistence and validate deterministic two-peer
  multiplayer synchronization.

### Expanded Construction Queues

[IMPLEMENTED, PARTIAL MANUAL VALIDATION] Shipyards and construction facilities can use all ten native queue slots conveniently and can repeat an item continuously. Basic Ctrl-fill, Ctrl+Alt refill, and delete-to-cancel behaviour work in game. Save/load and the wider validation matrix remain pending. See `docs/queue-enhancements.md`.

Possible controls:

* **Ctrl + click:** Fill all remaining queue slots with the selected item.
* **Ctrl + Alt + click:** Begin continuously producing the selected item until the option is selected again or cancelled.
* Display a coloured overlay on the build button while continuous production is enabled, such as:
  * green for active continuous production;
  * yellow for paused or waiting.
* Stop continuous production when:
  * [ ] the player selects the command again;
  * [ ] the yard is destroyed;
  * [ ] the item becomes unavailable;
  * [ ] required technology is lost;
  * [x] the player manually deletes a queued item (confirmed in game; the remaining queue drains normally).

Questions to investigate:

* [x] The Producer and UI both have a ten-slot limit.
* [x] Continuous production refills the existing native queue.
* [x] Resource shortages pause production and retry periodically.
* Whether AI-controlled yards should be able to use the same system.
* Whether the controls can be exposed through configurable hotkeys.
* [x] Activation uses a synchronized typed-class marker and state is embedded in Producer save data.
* [x] Confirm basic Ctrl-fill, Ctrl+Alt automatic refill, and delete-to-cancel behaviour in single player.
* [ ] Validate active and resource-paused continuous production across save/load.
* [ ] Confirm a cancelled repeat remains cancelled across save/load and older unmarked saves still load normally.
* [ ] Complete the remaining single-player and two-peer multiplayer validation matrix.
* [ ] Add active/paused build-button overlays.

#### Hybrid Producer build methods

[IN PROGRESS: HYBRIDBUILD RESEARCHSTATION SUPPORTS ALL FOUR METHODS]

* [x] Register the staged `hybridbuild -> research` classlabel alias so one
  stable ODF-facing identity can initially inherit all ResearchStation pod
  behaviour, then migrate toward sidecars and a standalone factory.
* [x] Manually confirm that changing the supported station to
  `classLabel = "hybridbuild"` preserves its pods and all four hybrid
  menus/queue/execution paths.
* [x] Preserve the pre-alias classlabel through core API revision 3 and require
  the `hybridbuild` source identity before publishing hybrid runtime lists.
  Ordinary `research` stations retain native-only menu/queue behaviour.
* [x] Manually reconfirm the current hybrid station after source-identity
  gating and verify a control `research` station does not enter the hybrid
  registry.

Allow one Producer-derived unit to expose all four explicit production lists
while retaining `buildItem<N>` as the classLabel-selected compatibility path:

```ini
constructItem0 = "fed_mining"
yardItem0      = "fed_scout"
researchItem0  = "fed_phaser_upgrade"
evolveItem0    = "bio_cruiser"
```

All methods use one ten-slot typed FIFO and only its front job may run. An
evolution order is appended behind existing work, then acts as a terminal
barrier: all four lists reject new work until the evolution is actioned or
cancelled. The replacement object receives a fresh queue and its own lists.
See `docs/hybrid-production.md` for the complete contract and native-layout
safety constraint.

* [x] Define explicit `constructItem<N>`, `yardItem<N>`, `researchItem<N>`, and
  `evolveItem<N>` command spellings alongside legacy `buildItem<N>`.
* [x] Implement case-insensitive command-key parsing and 57-slot list bounds.
* [x] Implement explicit-list precedence and reject a target class appearing
  in more than one explicit method list on the same producer.
* [x] Implement the platform-independent ten-job typed FIFO with stable queue
  IDs, target project IDs, optional placement data, and one active front job.
* [x] Implement and test evolution barrier, cancellation/unlock, defensive
  completion clearing, capacity, deterministic ordering, and repeat fairness.
* [x] Parse the first supported lists from Fleet Ops' existing
  ResearchStation-class load callback and resolve them through
  ParameterDB/project IDs into a class registry. The parser requires at least
  one explicit list to begin at index 0, then retains sparse entries through
  index 56. A generic Producer-class parser was rejected after it destabilized
  the startup class-loading sweep.
* [x] Publish the ResearchStation slice with separate stable runtime
  tables. `constructItem<N>` is shown under its dedicated Construction button;
  `yardItem<N>` is shown under the native Build button;
  `researchItem<N>` (or the unchanged legacy/tier table when no explicit
  research list exists) is shown under Research; and `evolveItem<N>` is shown
  under Evolve. Construct/yard/evolve entries are never appended to upgrade-pod
  tables.
* [x] Route ResearchStation `researchItem<N>` through the unmodified native
  research path and `yardItem<N>` through signature-checked generic Producer
  start/cancel/finish/construction-matrix paths, avoiding the overlapping
  ResearchStation and Shipyard subclass tails.
* [x] Route `evolveItem<N>` through generic Producer timing/FIFO callbacks,
  a base-only transform matrix, and the safe portion of Evolver's final object
  handoff. Enforce one terminal evolution barrier across direct, hotkey,
  Ctrl-fill, repeat, and synchronized command paths.
* [x] Add API revision 4 cocoon-class association and a protected 28-byte
  sidecar for hybrid Evolver start/update/stop/cancel/render calls. Restore the
  overlapping ResearchStation pod fields immediately after every native call,
  and clean the cocoon before station destruction/final removal.
* [x] Route `constructItem<N>` through Armada's native cursor placement and a
  protected ConstructionRig sidecar. Bind one native placement interface to
  each synchronized queue ID so queued stations retain independent positions
  and rotations, then select only the active job's transform for construction.
* [x] Render waiting construction placements through Armada's native
  placeholder path as yellow translucent station ghosts while their HybridBuild
  owner remains selected.
* [x] Reuse ResearchStation's inherited native ten-slot Producer FIFO for both
  yard and research orders. Its one-job busy check is suppressed during both
  hybrid Build and Research UI refreshes, and only while the shared queue has a
  free slot; execution remains one front item at a time.
* [x] Isolate hybrid Build and Evolve onto distinct unused Fleet Ops root
  controls and return a newly selected single hybrid station to root mode
  once. Research retains the native shared control; ordinary classes retain
  native Build/Research/Evolve/Trade behavior.
* [ ] Finish manual validation of Evolve terminal-barrier cancellation and
  queue-wireframe combinations. The button/list, final `fresear` replacement,
  and protected cocoon display/cleanup paths are working in game.
* [x] Keep hybrid ResearchStations on Fleet Ops' compatible single-object
  display, intercept its two patched dispatcher calls, invoke the native
  callbacks first, then preserve their result registers while binding the ten
  Fleet Ops-extended `BuildWireframe` controls at `ShipDisplay + 0x120` to a
  queue-ID-deduplicated view of the active class plus inherited Producer FIFO.
  `ShipDisplay + 0x38c` is the unrelated weapon/buff system-icon array and must
  remain untouched. The queue projection includes
  ResearchStation's active pod when it has moved outside the linked queue.
  Empty hybrid and research-only stations retain Fleet Ops' original display.
* [x] Manually confirm that Build and Research show only their own lists and
  that mixed yard/research work occupies all ten visible FIFO slots, advances
  without clearing the remaining queue, and displays active pods correctly.
* [x] Keep unique queued pods disabled in place without replacing adjacent
  research buttons, and retain the Research menu after a pod order.
* [x] Match native full-builder presentation by suppressing the race insignia
  behind an occupied tenth slot and draw the pod's single `_s` wireframe beside
  Fleet Ops' mouse-over progress bar.
* [ ] Recheck active and queued cancellation/refund behavior across both lists.
* [ ] Reconfirm level-4 upgrade-pod publication after the yard table was
  isolated. The configured maximum remains six and the runtime still resolves
  the level-4 sidecar candidate; validate that it is enabled whenever the
  shared queue is not full.
* [ ] Generalize safe command-panel publication to every supported hybrid host;
  do not expose a button until that host/method pair has an execution adapter.
* [ ] Carry method and queue ID through the synchronized command path so peers
  never infer a method from mutable UI state.
* [ ] Serialize captured constructor placements as part of their queued jobs.
* [ ] Add remaining requested host/method adapters, including Shipyard-hosted
  research. Their state must not share overlapping native subclass tails.
* [ ] Stop continuous refill as soon as evolution is queued and keep it blocked
  until the barrier is cancelled; ordinary repeats return to the queue tail.
* [ ] Disable all four build-list buttons while an evolution barrier exists and
  restore them immediately when that job is cancelled.
* [ ] Save/load typed queue state without changing Fleet Ops' stream layout,
  including placement, active method, and evolution lock.
* [ ] Validate resources, cancellation, placement, AI, save/load, and two-peer
  synchronization for mixed queues before enabling the parser in release.
* [x] Manually validate the ResearchStation first slice: both entries appear,
  research attaches its pod, and queued yard/research work starts and finishes
  without either subclass tail being touched by the wrong path.
* [ ] Recheck editor mode and cancellation/refund behavior under Gamescope's
  SDL backend.

### Squadron System

[IDEA, VIABLE, LARGE GAMEPLAY SYSTEM]

Add Dawn of War-style squadrons: one logical selectable and commandable unit
made from several real native Craft objects. Every member must retain its own
hull, shields, crew, subsystems, weapons, targets, collision, movement, and
destruction. Do not build the gameplay system from render-only swarm instances
or one aggregate Craft with simulated member health.

Squad membership belongs in extension-owned deterministic sidecar state. Do
not consume Armada's ten player control groups as the membership store: a
squad must be allowed to appear in any native control group, and several
squads and ordinary ships may share one fleet. Once a squad selection has been
expanded to its surviving Craft members, ordinary multi-selection, formation,
command, weapon, and damage paths should remain native wherever possible.

Provisional authoring shape, to be finalized only after the construction and
save/load prototypes prove the required identities:

```ini
squadMember0 = "fed_squad_member"
squadMember1 = "fed_squad_member"
squadMember2 = "fed_squad_member"

squadReinforceAtYard = 1
```

The buildable squad definition should own the aggregate button, tooltip,
build time, and initial cost policy. The member ODFs remain authoritative for
their individual combat and presentation behaviour. The first supported
version should use a fixed homogeneous composition even if the indexed format
is kept capable of later mixed squads.

Core prototype:

* [ ] Add a dedicated optional `A2FOSquadrons.dll` module and define strict
  bounds for members per squad and live squads per team.
* [ ] Prototype one fixed three-member squad built from an ordinary Shipyard
  item, with all three outputs created as normal mission-published Craft.
* [ ] Before admitting the build job, atomically validate the aggregate
  resource cost, technology requirements, all member ODFs, and enough unit-cap
  capacity for the complete initial squad. A partial initial spawn must either
  roll back safely or never begin.
* [ ] Give every squad a deterministic identity and every member a stable slot
  identity which survives leader death, handle changes during loading, and
  reinforcement. Reuse the proven native object-label reconnection pattern
  where possible without exposing internal labels to the player.
* [ ] Selecting any live member should select the whole surviving squad once,
  while Shift-selection, box selection, deselection, control-group binding,
  camera focus, and selection limits retain understandable native behaviour.
* [ ] Route movement, attack, guard, repair, halt, stance, autonomy, special-
  weapon, and formation orders through the synchronized native group command
  paths after selection expansion. Do not mirror unsynchronized UI state into
  simulation decisions.
* [ ] Define loose native formation behaviour for the first version, then add
  a bounded cohesion/leash policy only if members routinely split or cease to
  behave as one tactical unit.
* [ ] Remove destroyed members from the live membership set while preserving
  their vacant configured slots. Replace a dead logical leader deterministically
  without changing squad identity or orders.
* [ ] Decide how native veterancy, capture, assimilation, recrewing, transport,
  refit, separation weapons, and scripted object replacement apply to a
  member. Features without a safe squad-wide policy should be rejected or
  disabled for squad members in the first release.

Yard-only reinforcement:

* [ ] Detect a compatible same-team Shipyard's native repair/waiting/active
  state and define exactly when the squad counts as being `during repair`.
* [ ] Require all surviving members to reach the same yard or a bounded yard
  staging area before reinforcement becomes available; prevent one distant
  member from granting reinforcement to the rest of the squad.
* [ ] Expose a manual Reinforce action first. Automatic reinforcement may be
  considered later, but must never spend resources unexpectedly by default.
* [ ] Admit each missing configured slot as a real synchronized yard job, with
  per-member cost, build time, cap, cancellation/refund, queue-wireframe, and
  launch handling. Reinforce one member at a time rather than creating a whole
  batch in one simulation tick.
* [ ] Attach the completed member to its original stable slot and current
  squad, then bring it into formation through native movement rather than
  teleporting it beside the squad.
* [ ] Define what happens if repair is halted, the squad leaves, resources or
  cap become unavailable, ownership changes, or the yard is destroyed while a
  reinforcement is waiting or active.
* [ ] Keep reinforcement compatible with the existing ten-slot Producer queue,
  queue enhancements, HybridBuild jobs, and RefitYards without allowing two
  modules to own the same native queue hook independently.

Persistence, presentation, and validation:

* [ ] Reconstruct membership, configured composition, stable slots, missing
  members, leader choice, active orders, and queued reinforcement after
  save/load without changing the native stream layout unsafely.
* [ ] Use native multi-selection presentation for the first playable version.
  Later add a dedicated squad panel showing each member and its individual
  health only after core selection and combat behaviour is stable.
* [ ] Make the Collapsible Fleet Sidebar squad-aware as an optional integration:
  one logical squad may be summarized as one fleet entry component while the
  native control group still contains the real member handles.
* [ ] Profile pathfinding, simulation, selection rendering, and weapon cost at
  representative and deliberately excessive squad counts. Every member is a
  full engine Craft and therefore has a real CPU cost.
* [ ] Add AI production, squad-order, retreat/repair, and reinforcement policy
  only after human-controlled single-player behaviour passes the full matrix.
* [ ] Validate individual damage/death, leader loss, partial squads, mixed
  selections, control groups, repair/reinforcement, save/load, capture and
  destruction edge cases, then complete deterministic two-peer multiplayer
  tests before declaring the feature multiplayer-safe.

Explicitly out of scope for the first version: freely splitting or merging
squads, changing a squad's authored composition in the field, strict Dawn of
War-style cover/morale systems, and a new squad-wide veterancy model.

### Configurable Ship-System Upgrade Pods

[IMPLEMENTED, MANUAL VALIDATION REQUIRED]

Armada II ship-system upgrade pods use `upgradeLevel`, but the supported
engine path is hardcoded around a maximum level of `3`. Investigate replacing
that fixed check with a bounded, synchronized policy so mods can deliberately
enable additional tiers without making arbitrary values valid.

`upgradePodMaximumTier` in inherited `RTS_CFG.h` selects the permitted
maximum. The native bridge imposes a hard safety ceiling of `16`, validates
the requested value, hooks only the known supported binaries, and defaults to
level `6` when no root supplies a setting. Higher declared levels are
retained in A2FO sidecar state while the engine-facing value is projected onto
tier 3, preventing Armada's hardcoded Team upgrade arrays from being indexed
out of bounds. Because this affects simulation data, every multiplayer peer
must load the same `RTS_CFG.h` tier limit.

#### Tiered Upgrade-Station Build Lists

The vanilla upgrade-station layout uses special hardcoded build-list positions:

```text
buildItem4 = "level_2_pod"
secondaryBuildItem0 = "level_3_pod"
```

Tier 1 pods already use the ordinary build list and do not need new commands.
Add a consistent indexed scheme for tier 2 and above:

```text
tier0BuildItem0 = "level_2_pod"
tier0BuildItem1 = "another_level_2_pod"

tier1BuildItem0 = "level_3_pod"
tier2BuildItem0 = "level_4_pod"
```

The implemented command family is `tier<Tier>BuildItem<Index>`. The command
tier is zero-based beyond the vessel's built-in level-1 systems, so command
tier 0 selects `upgradeLevel = 2`, tier 1 selects level 3, and tier 2 selects
level 4. Tier and item indices are parsed numerically up to the bounded Fleet
Ops primary/secondary build-list capacities rather than being implemented as a
short fixed list of literal command names.

Backward compatibility:

* Standard tier 1 `buildItem<N>` entries retain their vanilla behaviour.
* The original `buildItem4` level-2 and `secondaryBuildItem0` level-3 routes
  continue to work when the corresponding new tier list is absent.
* A `tier0BuildItem<N>` or `tier1BuildItem<N>` command replaces only index
  `N` with the explicit level-2 or level-3 item. Unspecified indices retain
  their legacy entries, so unrelated research is never compacted or moved.
* Upgrade levels above 3 require both an enabled `RTS_CFG.h` tier limit and matching
  `tier<Tier>BuildItem<Index>` entries.
* The item index identifies the same upgrade chain across every tier. A pod
  built from `tier2BuildItem4`, for example, replaces the pod previously built
  from `tier1BuildItem4`; both ODFs must declare the same `upgradeSystem`.
  Runtime matching uses that system rather than construction order.
* Missing tier lists preserve the existing vanilla/Fleet Ops build lists.
  Invalid or out-of-range entries are ignored and logged.
* Upgrade stations still need enough `podHardpoints` entries for the physical
  pods they can hold; the new build-list commands do not create hardpoints.

Implemented native work:

* [x] Clamp the engine-facing Team upgrade index to tier 3 while retaining the
  real configured tier in class and live-pod sidecars.
* [x] Apply the multiplier belonging to the highest attached tier for each
  team/system and restore the next-highest attached pod when one is removed.
* [x] Preserve distinct same-tier checks for tier 4 and above instead of
  allowing every projected tier-3 pod to compare equal.
* [x] Parse `tier<Tier>BuildItem<Index>` during ResearchStation class loading,
  preserving legacy lists when no new list for that tier is supplied.
* [x] Maintain a private secondary build list for each live upgrade station and
  advance only the matching occupied system to its next configured tier after
  replacement, while copying every unrelated native/Fleet Ops research slot
  unchanged and preserving the native level-2 prerequisite relationship.
* [x] Read the inherited startup-only `upgradePodMaximumTier` setting from
  `RTS_CFG.h`, with a default of 6 and hard maximum of 16.

Manual validation remaining:

* [ ] Confirm tier 4-6 construction, attachment, replacement, destruction, and
  multiplier changes for all five ship systems.
* [ ] Confirm a tier-4 pod does not compare as the same pod type as tier 3 or
  tier 5 after their engine-facing indices are projected to tier 3.
* [ ] Confirm tier-specific buttons, tooltips, AI choices, pod hardpoints, and
  station build routing across ordinary and replacement ODFs.
* [ ] Verify save/load with multiple attached native and extended tiers, then
  destroy pods after loading and confirm the next-highest multiplier returns.
* [ ] Verify unmodified stations and old saves retain vanilla behaviour.
* [ ] Complete a two-peer multiplayer synchronization test with identical
  `RTS_CFG.h` and ODF files.

### Borg Features

#### Race-Specific Assimilation Replacement

[IDEA]

Replace the current mod workaround which uses an automatically firing special
weapon to detect when a vessel changes owner to the Borg and then swaps its
ODF. A native ownership-change/capture event could perform the replacement
directly, without requiring a hidden weapon on every compatible vessel.

Proposed unit ODF commands use indexed race/ODF pairs:

```text
capture0Race = "borg"
capture0Odf = "bor_galaxy"

capture1Race = "romulan"
capture1Odf = "rom_galaxy"
```

When the unit changes owner, the new owner's race would be compared with each
`capture<N>Race` entry. A matching `capture<N>Odf` would replace or transform
the captured unit into the faction-specific version.

Technical questions:

* Which Armada ownership-change function provides a single synchronized event
  for capture, assimilation, transfer, and scripted ownership changes.
* Whether the replacement should preserve position, rotation, damage, crew,
  special energy, veterancy, orders, hotkey groups, and object identity.
* How to prevent the replacement itself from retriggering the capture rule.
* Whether the mapping should apply only to assimilation or to every ownership
  change.
* How indexed pairs should be validated when an entry is incomplete or its
  target ODF cannot be found.
* How the transformation should persist through save/load and remain
  synchronized in multiplayer.

#### Collective Borg Experience

[IDEA]

Add a faction-selectable experience mode. The normal mode retains per-unit
experience, while the collective mode contributes experience to a faction-wide
pool and derives Borg ranks from shared thresholds.

Proposed faction ODF commands:

```text
xpMode = "collective"
xpMode = "individual"

collectiveXPRequired1 = 5000
collectiveXPRequired2 = 15000
collectiveXPRequired3 = 35000
collectiveXPRequired4 = 70000
collectiveXPRequired5 = 120000
```

`individual` remains the default and therefore does not normally need to be
declared. In `collective` mode, eligible units contribute to the shared pool
and receive the collective rank reached by the faction.

Proposed unit ODF command:

```text
inheritCollectiveRank = 1
```

The default is `1`. This allows newly built or assimilated units to inherit the
current collective rank immediately. It is particularly useful for multiphase
assimilation, where an initially assimilated unit later changes into its fully
assimilated ODF and should retain the rank already reached by the Collective.
Setting the command to `0` would allow special units to opt out of collective
rank inheritance.

Proposed behaviour:

* Experience enters the collective pool through the same unit-destruction
  awards which normally grant individual XP. Assimilation and support actions
  do not add separate XP awards; the implementation must redirect each normal
  destruction award once so it cannot be counted for both the unit and pool.
* Every eligible unit updates immediately to the current collective rank when
  the faction crosses a threshold. Newly created eligible units also begin at
  the faction's current rank.
* Collective Borg units do not use individual veteran progression because that
  conflicts with the shared-XP design. A special unit which needs ordinary
  personal ranks or veteran bonuses must set `inheritCollectiveRank = 0` and
  remain outside collective rank inheritance.
* Assimilating a unit does not add its existing XP to the Borg pool and does
  not alter the former owner's collective state. Its carried individual XP and
  rank are reset rather than transferred.
* An initially captured unit therefore starts again from zero personal XP. On
  each intermediate or fully assimilated ODF replacement,
  `inheritCollectiveRank = 1` applies the Borg faction's current collective
  rank; an opted-out unit remains at its starting rank instead.
* The collective XP total and current threshold are faction state. They should
  be saved and synchronized using the same conceptual path Fleet Operations
  uses for individual XP, but stored once for the faction rather than once per
  unit. The exact Fleet Ops storage and network hooks still need research.

### Noxter Features

#### Seed and Breeder Production System

[IDEA]

Replace conventional Noxter shipyards with physical Seed units and Breeder
organisms. Expose them through the semantic classlabels `seed` and `breeder`,
backed by safe native Craft- and Producer-derived hosts plus extension-owned
sidecar state rather than attempting to add incompatible native C++ classes.

##### Seed Behaviour and ODF Contract

Each Seed carries the complete heritable production profile for one organism:

```text
classLabel = "seed"
offspring = "noxter_unit"

dilithiumMetabolismCost = 20
tritaniumMetabolismCost = 10
biomatterMetabolismCost = 5

seedRange = 50
metamorphosisTime = 2.0
```

Prefer the explicit `offspring` command to an unindexed `buildItem`, avoiding
confusion with the engine's normal indexed `buildItem<N>` construction lists.
The offspring ODF remains authoritative for its ordinary build time, costs,
caps, model, and other production data. The Seed supplies only genetic and
metabolic properties which are copied permanently into the Breeder when it is
consumed.

The intended interaction is contextual right-click on a friendly, living,
unseeded Breeder. The Seed moves into range, validates the target again, commits
the specialisation, and is then consumed through a native destruction/removal
path. A dedicated `Implant Seed` target command is an acceptable first version
before exact contextual right-click handling is complete.

Specialisation is irreversible. If two synchronized Seed orders reach the same
Breeder, the first valid order wins and later Seeds remain unconsumed. Invalid,
hostile, dead, already-seeded, or disallowed targets must not consume the Seed.

##### Breeder Production State

Use a hidden one-slot native Producer job rather than bypassing production
outright. The player sees no editable construction queue, but native production
continues to provide build time, resource deduction, caps, spawn handling,
effects, save/load support, and deterministic multiplayer behaviour.

```text
Unseeded      -> accepts one Seed; no production or metabolism
Metamorphosis -> temporarily inactive while specialisation completes
Constructing  -> continuously produces the inherited offspring
Metabolising  -> unable to build; living on reserves until its next meal
Paused        -> metabolism clock frozen while special energy drains
Starving      -> missed a metabolic meal; hull damage and no hull repair
```

After an offspring completes, wait only the configured short `broodDelay`, then
start the next hidden job. Beginning a valid offspring counts as feeding the
Breeder and resets its metabolism clock.

##### Periodic Metabolism and Starvation

Metabolism is paid as a periodic atomic meal rather than a per-second trickle.
By default the interval is the inherited offspring's build time; an optional
Seed `metabolismTime` command may override it. The clock advances only while the
Breeder is unpaused and not constructing.

When an interval expires, deduct all configured metabolism resources together.
If any required resource is unavailable, deduct none and enter starvation.
While starving, apply the Breeder ODF's `starve` value as deterministic hull
damage per simulation second and reject all positive hull repair and passive
hull regeneration. Shield recharge remains unaffected unless a separate option
is deliberately introduced.

The Breeder should retry frequently while starving. Starting an offspring or
successfully paying one metabolic meal ends starvation immediately and allows
hull repair again. Unit-cap blockage, obstructed spawn points, technology
gating, invalid ODFs, and genuine resource shortage must be distinguished so
the design can explicitly decide which conditions advance metabolism rather
than treating every failed queue insertion as insufficient food.

##### Pause Behaviour

After specialisation, expose one production toggle instead of ordinary queue
commands. Pausing freezes the existing metabolism countdown and production,
drains special energy at `pauseDrain` per second, and automatically resumes at
zero energy; manual early resume is allowed. Pausing must not reset the hunger
clock, otherwise repeated toggling grants free metabolic intervals.

Pause is preventative rather than a cure for an already missed meal. An
already-starving Breeder cannot clear starvation or regain repair merely by
pausing; it must begin production or pay metabolism. Define during implementation
whether the pause command is disabled while starving or merely freezes other
activity without clearing starvation damage.

##### Additional Design and Correctness Requirements

* [ ] Decide whether `broodDelay`, metabolism costs, and `metabolismTime` all
  belong to the Seed phenotype, while `starve`, pause reserve, and physical
  durability remain properties of the Breeder ODF.
* [ ] Validate offspring against a permitted mobile-unit whitelist so a Seed
  cannot encode stations, map objects, or otherwise unsafe classes.
* [ ] Define whether later technology loss is ignored after the genetic
  blueprint has been implanted; current preference is to validate technology
  once at seeding while retaining normal unit caps during production.
* [ ] Decide whether being unit-cap blocked continues the metabolism clock;
  current design says yes, requiring the player to make room or pause.
* [ ] Consider an irreversible `Reabsorb Breeder` command which returns a
  configurable fraction of biomass/resources and matches the Apocrypha lore.
* [ ] Add metamorphosis and starvation animation, sound, effect, and UI states
  without making simulation depend on presentation.
* [ ] Persist the offspring project/ODF identity, inherited metabolism vector,
  seeded state, production state, hunger clock, starvation accumulator, pause
  state, and special-energy reserve through save/load.
* [ ] Synchronize Seed targeting, winner selection, consumption, production,
  metabolic payments, pausing, and starvation deterministically in multiplayer.
* [ ] Apply starvation through native health/damage handling and block every
  hull-increase path while starved rather than writing raw hull memory.
* [ ] Document and signature-check every new Fleet Ops/Armada address used for
  resource tests/deduction, special energy, target actions/orders, health,
  repair suppression, Producer simulation, and save/load.

#### Mother Influence Network

[IDEA] The Mother projects an initial influence area around herself.

Inside that area, the Noxter can:

* place most stations;
* specialise or activate Breeders;
* regenerate more effectively;
* use certain advanced abilities;
* receive coordination or combat bonuses;
* communicate with the wider swarm.

Specialised stations then extend the field:

```text
Mother
  └── influence area
        └── Relay organism
              └── extended influence area
                    └── Nest / Breeder / defensive organism
```

Each relay must remain connected to the Mother through overlapping influence areas. This would make it both a territorial system and a network system.

##### Disconnected Areas

For example:

```text
Mother → Relay A → Relay B
```

If Relay A is destroyed:

```text
Mother    Relay B
   ✕ connection
```

Relay B remains physically alive, but:

* it stops projecting influence;
* stations depending on its field become inactive;
* linked abilities stop functioning;
* nearby units lose swarm bonuses;
* Breeders may pause production;
* regeneration may stop or slow.

##### Visual Representation

* Display active influence as a translucent, animated nebula-like cloud.
* Make the field denser and brighter near Mothers and relay organisms.
* Fade or desaturate disconnected fields before they disappear.
* Use subtle spores, pulses, or biological particles to distinguish it from ordinary map nebulae.
* Ensure the edge remains readable enough for station placement.
* Consider different visual states for:
  * fully connected influence;
  * weakened or recently disconnected influence;
  * hostile or corrupted influence;
  * influence projected by different Mother strains.

Technical questions:

* Whether influence is calculated as overlapping circles, a connected graph, or both.
* How frequently connectivity should be recalculated.
* Whether moving units can temporarily project influence.
* How inactive stations behave when disconnected.
* Whether influence state must be stored directly in save games or rebuilt on load.
* How the influence network should be synchronised in multiplayer.

#### Bacteria Mining

[IDEA]

The loop becomes:

```text
Resource node
    ↓ infected by Noxter organism
Infested node grows a harvesting structure
    ↓ produces resource sacs over time
Collector gathers the sacs
    ↓
Digester consumes them
    ↓
Player receives dilithium or tritanium
```

The main difference is that the collector no longer extracts resources directly. The infected node performs the extraction and packages the material biologically.

##### Proposed Stages

###### 1. Infection

A Noxter seeding organism targets an unoccupied resource moon or asteroid.

During infection:

* the node cannot yet produce resources;
* the infecting organism may remain attached and vulnerable;
* construction can be interrupted;
* the infection may require Mother Influence.

After the growth timer completes, the node receives an organic structure wrapped around it.

###### 2. Maturation

The structure could begin with low productivity and mature over time:

```text
Larval colony → mature extractor → engorged extractor
```

Possible effects:

* faster sac production at later stages;
* more sacs can wait at the node;
* visual growth around the asteroid;
* destroying it resets all maturation progress.

###### 3. Sac Production

The infected node periodically creates a physical sac object.

For example:

```text
sacInterval = 12
maximumStoredSacs = 4
sacResourceValue = 75
```

If all storage points are occupied, production pauses until a collector removes one.

That gives enemies a reason to raid the extractor even when they cannot destroy it: accumulated sacs represent resources that have been produced but not yet secured.

###### 4. Collection

The collector travels to the infected node, attaches to or approaches a sac, and carries it back.

Possible visual representations:

* an object attached beneath the collector;
* a swollen cargo organ;
* a small object following the collector;
* an internal cargo count with a pickup animation.

The collector could potentially carry several sacs, depending on balance.

###### 5. Digestion

The collector delivers the sacs to a Digester organism.

The Digester destroys or consumes each sac and credits the resource to the player.

This means the Digester replaces the normal refinery role, but the fiction is entirely biological.

Technical questions:

* Whether the infected structure replaces the resource node or attaches to it.
* Whether stored sacs are physical map objects or abstract cargo slots.
* Whether enemy players can destroy or steal accumulated sacs.
* Whether collectors can carry multiple sacs.
* Whether standard mining AI can be adapted or requires a new order type.
* How sac production, collection, and delivery should be synchronised in multiplayer.
* Whether disconnected extractors stop producing, decay, or retain stored sacs.

#### Enemy Infestation Cycle

[IDEA]

```text
Enemy vessel
    ↓ infected by larva or spore
Incubation period
    ↓ internal damage and visible organic growth
Larva burst
    ↓
Ship destroyed or disabled
    ↓
Noxter organisms spawn from the wreck
```

##### 1. Infection

A specialised Noxter unit uses an infestation weapon on a valid target.

Possible restrictions:

* target must be below a health threshold;
* shields must be down;
* only certain ship sizes can be infected;
* stations may require a stronger infestation organism;
* Borg or fully synthetic targets could be resistant.

The infection should be a status effect rather than immediate control.

##### 2. Incubation

While infected, the enemy vessel remains under its original owner’s control, but suffers escalating effects:

* gradual hull damage;
* reduced crew;
* slower repairs;
* reduced weapon or engine performance;
* periodic loss of special energy;
* visible organic growth across the hull.

The owner then has time to react rather than losing the ship instantly.

Possible countermeasures:

* return to a repair yard;
* use a medical or cleansing ability;
* sacrifice or decommission the ship;
* destroy the infecting organism before implantation completes.

##### 3. Larva Burst

When the incubation timer completes—or when the infected vessel dies—the infestation erupts.

The burst could:

* destroy the host;
* spawn several small Noxter larvae;
* create one larger organism based on the host’s size;
* damage nearby vessels;
* leave an infected wreckage object;
* temporarily spread spores to nearby damaged enemies.

For example:

```text
Scout host       → 1 larva
Destroyer host   → 2–3 larvae
Cruiser host     → 1 mature combat organism
Battleship host  → multiple larvae plus a larger organism
Station host     → temporary nest or breeder
```

Technical questions:

* How infection state should be attached to and removed from engine objects.
* Whether infection timers should run through a dedicated native module or a
  generic status-effect dispatcher.
* How host size or class maps to burst results.
* What happens when an infected target is destroyed before incubation completes.
* How cleansing, repair-yard treatment, or immunity should work.
* Whether visible organic growth requires model swapping, overlays, attachments, or particle effects.
* How infection state and spawned results are synchronised in multiplayer and save games.

##### Shared Infestation Framework

[API] Investigate a generic infestation system that can support both resource-node infection and enemy-vessel infestation.

The shared framework should consider:

* target validation;
* incubation timers;
* visible host effects;
* interruption and cleansing;
* transformation or spawned offspring;
* behaviour when the host dies prematurely;
* ownership and team handling;
* save-game persistence;
* multiplayer synchronisation;
* generic callbacks for native modules.

A generic API could allow feature modules to define:

```text
target type
infection duration
required conditions
periodic effects
completion result
premature-destruction result
visual state
cleansing conditions
```

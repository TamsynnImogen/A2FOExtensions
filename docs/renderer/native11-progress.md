# Native D3D11 renderer completion checklist

This checklist tracks the path from the current isolated renderer to an
accepted gameplay renderer. Checkmarks mean the stated bounded evidence exists;
they do not imply deployment or visual acceptance. Update this file as new
stages are frozen.

Latest numbered checkpoint: **stage 52**. The subsequent
[native loader geometry checkpoint](native11-loader-scaling.md) passes all 106 private
runtime cases and 24 strict host/sanitizer variants; stage 53 remains open. The strict build passes. This is
staged evidence; no installed game files have changed. Older service clients and
seven reference previews remain covered by the compatibility suite.

The newer [native retirement candidate](native11-lifecycle.md) passes all **108**
private runtime cases. Requested destruction ownership now precedes asset capture;
retirement revokes model/texture versions, pending IO and world source tickets.
This advances gate 56; its device/restart and live acceptance work remains open.

The [fresh view and sprite source candidate](native11-fresh-view-sprites.md)
passes **117/117** private runtime cases, **32** host/sanitizer variants and **32**
Rust tests. Fresh distance/fade sampling and one checked native sprite source
route are connected; they do not close gates 53 or 57. All nine implementation
gates 53–61 still require work.

The earlier [private live-ownership candidate](native11-live-ownership.md) passes
[122/122 private runtime cases](../../build/native11/live-owner-compat-full-results.json)
and a strict build. Its saved 90-second private run displays the animated legacy
menu, with scene/model/DDS/TGA/sprite/material/node/clock diagnostics armed and
one 25,610-byte model copy. That run’s first load template was unavailable because
of a concrete loaded profile/ownership mismatch. The world-light reader now
waits for copied classes and the first owned view. Its independent worker/main
fixture checks thread refusal but does not establish that the clock was first
initialized on a worker; reviewed startup places early clock initialization in
`DLL_PROCESS_ATTACH`. This is not complete frame capture or native D3D11
presentation takeover. The previous 118+1 results remain historical evidence.

The previous [Fleet constructor/template checkpoint](native11-fleet-templates.md)
passes **124/124** private runtime cases and the strict build. Its final
90-second private run captures **30 nodes** and one load template from an actual
25,610-byte model read, with the animated legacy menu visible, no new fault and
verified cleanup. Load-template construction admission is now independent of
runtime playback admission. A2FOAnimations’ later matrix-playback detour remains
unqualified for runtime sampling; no complete native frame or presentation
handover is available. Seven protected installed hashes remain unchanged.

The previous [native animation fallback checkpoint](native11-animation-fallback.md)
passes **132/132** private runtime cases and the strict build. The own builtin
hook is qualified only with zero configured clip classes and no in-flight class
updates or custom evaluations; snapshots compare source epochs before publication.
The final private menu run logs fallback qualification and retains the actual
30-node template capture, with no new fault and verified cleanup. Custom clips,
live skirmish pose acceptance and a complete world-frame producer remain open.

The previous [world-model association checkpoint](native11-world-associations.md)
passes **134/134** private runtime cases, the strict build and a pure-reader
Clang ASan/UBSan run. It owns current database/model identities and raw placements
for the checked ordered world roster, including members absent from visible
capture, and revokes current access when its sources change. It does not supply
render-selected model, effective material, visibility or fresh per-part pose
authority. All nine implementation gates remain open. Its final 90-second
private legacy menu run stayed alive, logged no new fault and verified cleanup.
Four placement-code regions and three vtable slots matched in both early and
late loaded-process reads. Live skirmish association acceptance remains open.

The previous [world Craft-view checkpoint](native11-world-views.md) passes
**137/137** final-artifact private cases and the strict builds. It binds fresh
logical camera/position/range/fade inputs to the checked roster, compares two
full view sweeps and rewalks the model list before publication. The final
90-second private legacy run qualifies twelve static view regions and two
slots plus placement ownership, logs no new fault and verifies cleanup. A real
late near-fade mismatch was fixed by capturing the runtime values; 49,152 new
native CPU comparisons verify that arithmetic across twelve PC/RC modes.
Live skirmish view acceptance, class/FOW/material/part-pose authority and a
complete frame producer remain open. All nine implementation gates remain open.

The previous [world logical-preparation checkpoint](native11-world-preparations.md)
passes **143/143** final-artifact private cases and the strict builds. It binds
fresh systems/Borg/hull state to the checked world roster and qualifies Fleet
Ops' exact saved-original base setup chain. The original CPU setup matches
49,152 comparisons across twelve x87 modes; single/bulk reader Clang ASan/UBSan
and source/lifecycle fixtures pass. Its 90-second private legacy menu run logs
actual Fleet setup qualification, retains the 30-node template, logs no new
fault and verifies cleanup. All 44 post-install diagnostic snapshots validate
the five-region Fleet closure and gateway; early/late Root, placement and view
checks pass. Seven installed hashes and seven previews remain unchanged.
Effective materials, geometry and live skirmish acceptance remain open;
all nine implementation gates remain open.

The earlier [prepared-visibility helper checkpoint](native11-prepared-visibility.md)
passes **144/144** final-artifact private cases, strict i686 builds and Clang
ASan/UBSan for both new and legacy reader fixtures. It rechecks held logical
ship state around class-role/topology/context capture and supplies fresh
system/Borg/hull values without reading cached render-instance outputs. It
retains a copied logical owner and rejects stale or forged state. The core
image remains the exact previously startup-verified binary; this helper is not
yet linked to a qualified native geometry producer. Full class/FOW/material
source qualification and all nine implementation gates remain open.

The previous [core world base-visibility checkpoint](native11-world-base-visibility.md)
passes **147/147** final-artifact private cases, strict changed-source/fixture
builds and batch/leaf-regression Clang ASan/UBSan. Core now consumes prepared
visibility for the checked world roster and pins completed-load template owners,
including actors absent from visible capture. Two world sweeps, shared context
and template tuple checks, final source rewalks and publication/getter revocation
protect the immutable frame. The final 90-second private legacy menu run logs
real base-source admission, matches all six regions and the slot early/late,
retains the load template, logs no new fault and verifies cleanup. Seven
installed hashes, the helper and seven previews remain unchanged. Actual
skirmish/context/clock-owner acceptance and complete class/FOW/material/geometry
authority remain open. No native geometry provider or presentation takeover is
registered; all nine implementation gates remain open.

The previous [completed-load raw material baseline checkpoint](native11-material-baselines.md)
passes **148/148** final-artifact private cases and the strict changed-source
and fixture builds, plus host and Clang ASan/UBSan reader/cache checks. Core
captures all raw mesh texture slots and the load-time Borg alternate alongside
the completed-load template, with exact model/template/texture owners. Later
draw-time slot swaps cannot refresh it; texture retirement and address reuse
revoke access. Optional material failure preserves node templates and cannot
be backfilled by a late getter or duplicate load notification. The private
inspection fixture now normalizes/restores the additional mesh-type slot;
production source checks remain intact. Effective class/Borg/faction/subsystem
selection and group mapping remain open. All nine implementation gates remain
open; no native geometry provider or presentation takeover is registered.
The final 90-second private legacy menu run captures **24 mesh baselines** and
the actual **30-node / 25,610-byte** load template, logs no new fault and verifies
cleanup. The legacy menu screenshot was reviewed. Seven protected installed
hashes, the helper and seven reference previews remain unchanged.

The previous [base/Borg policy and class-table checkpoint](native11-borg-materials.md)
passes **151/151** final-artifact private cases, strict new-source and fixture
builds, host checks and Clang ASan/UBSan. The original CPU selector/code cave/
existing-vector setter matches **9,600** comparisons, including preserved-slot
history, and rejects **32** source mutations. The reader correlates the complete
ordered native class table with load-time baselines and exact current class/
model/texture versions, without reading current slots. Core and helper remain
the exact prior startup-verified binaries; these helpers are not yet connected
to the world material producer. No new game launch or installed write occurs.
Class type/readiness, actor/caller binding, prior slot state, group mapping and
extension overlay qualification remain open; all nine implementation gates
remain open.

The previous [Core world/actor base/Borg integration](native11-world-borg-materials.md)
passes **153/153** final-artifact private cases, strict changed-source and
rebuilt fixture checks, host tests and Clang ASan/UBSan. Core now consumes the
checked model/preparation roster, real completed-load material baseline getter,
class-table reader and original write policy. It publishes immutable candidate
operations for observed and unseen actors, using fresh per-actor Borg state.
Two sweeps, shared owner/table checks, final source rewalks, owner-thread access
and source/lifetime/late-publication revocation protect the snapshot. The helper
and seven reference previews remain unchanged. Class readiness/caller/overlay
qualification, complete prior slot state and effective material/geometry
submission remain unfinished; all nine implementation gates remain open.
Its final 91-second private legacy menu run qualifies the eight named-operation
regions early/late, retains the real 24-mesh baseline and 30-node template, logs
no new fault and verifies cleanup. The final screenshot was reviewed; seven
installed hashes remain unchanged. Live skirmish actor/material acceptance is
still open.

The [live observed material path](native11-live-materials.md) now connects Core's
sealed authored-material/selected-diffuse capture to the API11 D3D11 helper.
All-part node/group identities survive the handoff, cached parts resolve their
materials each frame, and textured/white transitions are explicit. All
[157 final-artifact private cases](../../build/native11/live-material-full-results.json)
pass, including the four new fixtures, GPU pixels, resource retirement, replay
refusal and both shared-target preview/world publication transitions. Six focused
checks cover the material path and repaired x87/session-scope regressions.
Seven reference previews are byte-identical to the prior checkpoint. Complete unseen
actor materials, dynamic native shader/overlay fields and in-game acceptance
remain open; this advances the observed-material part of gate 53.
The matching Core/helper private legacy startup stays alive for **90.9 seconds**,
retains the 24-mesh baseline and 30-node template, logs no new fault and verifies
cleanup. Its menu screenshot was reviewed. This exercises startup compatibility;
observed material submission is verified by the private GPU/pipeline fixtures,
with live skirmish acceptance still open. The
[frozen receipt](../../build/native11/live-material-milestone.json) records exact artifacts.

The [live skirmish inspector](native11-gameplay-inspection.md) adds a user-driven
test copy and read-only localhost viewer. Core exports source-checked DX11 ship
snapshots once per second through bounded BMP/JSON files. The final **160/160**
private cases and **15 Python tests** pass, including the mounted-NVMe regression,
source revocation and stale-image refusal. The actual launcher and exporter pass
a **90.3-second** private startup with no new fault and verified cleanup. A separate
game copy is prepared at `A2FO-Native11-Gameplay-Test`, with matching Core/helper
and a pinned viewer. Live skirmish visual acceptance is now available to the user;
complete-world rendering, physical shadows and the nine implementation gates
remain open. The original installation is unchanged.

The [base Craft material route](native11-material-route.md) now gates candidate
Borg operations on the actor's checked render dispatch and primary database.
The current getter also rechecks correlated table rows/count/pointer/alternates,
so same-owner in-place mutations revoke access. All **161/161 final-artifact
private cases**, the four strict production checks and both host/sanitizer
runs pass. This is staged integration, without a new game launch. Class readiness, complete materials and all nine gates remain open.

Complete offscreen effective poses/materials, full engine draw capture, native
Windows, engine presentation/startup and in-game acceptance remain open. The full
gameplay renderer is unreleased.

## Current evidence

- [x] Independent native D3D11 renderer and static SOD/texture preview path.
- [x] HWND-free offscreen service and owned scene/content snapshot path.
- [x] Private headless GPU and policy fixtures: 161 current cases pass. This is a case count,
  not a coverage percentage or proof of full gameplay.
- [x] Checked Fleet constructor chain and actual 30-node completed-load template capture.
- [x] Owned builtin native animation fallback with source-epoch and quiescence checks.
- [x] Checked current-database/raw-placement associations across the ordered world roster, with source revocation.
- [x] Owned world-roster distance/fade snapshots with runtime near-fade parameters; no full visibility claim.
- [x] Completed-load raw material slots with immutable texture versions and independent retirement checks; no effective selection claim.
- [x] Original CPU base/Borg write policy and bounded class-table reader; no live producer integration claim.
- [x] Core world/actor correlation of base/Borg candidate operations; no effective material submission claim.
- [x] API11 observed authored-material/selected-diffuse submission from Core to D3D11, with separate private preview eligibility.
- [x] Separate manual skirmish inspection copy, source-checked DX11 snapshot export and read-only browser viewer.
- [x] Base Craft per-actor material selector route and correlated-table getter currentness; class readiness and full materials remain open.
- [ ] Full native gameplay rendering and presentation.
- [ ] Deploy renderer changes to the installed game or hook. Latest receipt
  supplies a separate inspection copy; protected live files remain unchanged.
- [ ] Native Windows and in-game visual acceptance.

Evidence links: [Core world/actor base/Borg operations](native11-world-borg-materials.md),
[base/Borg material operations](native11-borg-materials.md),
[raw material baselines](native11-material-baselines.md),
[core world base visibility](native11-world-base-visibility.md),
[prepared visibility inputs](native11-prepared-visibility.md),
[world logical preparation](native11-world-preparations.md),
[world Craft views](native11-world-views.md),
[world-model associations](native11-world-associations.md),
[native animation fallback](native11-animation-fallback.md),
[Fleet constructor and live template evidence](native11-fleet-templates.md),
[migration stages and verification](native11-migration.md),
[live ownership and private menu evidence](native11-live-ownership.md),
[native primitive API and ownership](native11-primitives.md),
[synthetic stage 40 showcase](../../build/native11/native11-showcase-stage40.png),
[latest numbered stage 52 receipt](../../build/native11/animation-clock-milestone.json),
[all-model-parts checkpoint receipt](../../build/native11/all-model-parts-milestone.json),
[load-template checkpoint receipt](../../build/native11/load-template-milestone.json),
[LOD-input checkpoint receipt](../../build/native11/lod-input-milestone.json),
[native geometry checkpoint receipt](../../build/native11/native-geometry-milestone.json),
[Venture preview](../../build/native11/venture.png),
[Mediterranean preview](../../build/native11/mediterranean.png), and
[bound-snapshot preview](../../build/native11/fleet-bound-snapshot.png).
Previews and private fixtures demonstrate isolated paths, not live game parity.

## Completion path

### 1. Native renderer foundation and offscreen service

- [x] Own D3D11 device/resources, scene contract, static asset adapter and
  offscreen renderer service; verify with the strict builds and private GPU/ABI
  fixtures recorded in the migration notes.
- [x] Preserve failure/retry, resource lifetime, lighting and pixel behavior in
  bounded headless fixtures.
- [ ] Native Windows acceptance: build and run the same service and focused
  rendering checks on a supported native Windows target.

### 2. Authoritative scene and asset capture

- [x] Freeze exact-target camera conversion and bounded post-frustum scene
  capture stages as documented in the migration record.
- [ ] Complete live high-level scene and visibility capture. Acceptance:
  compare camera, view identity, transforms and visible-object set against the
  exact installed engine for representative views, including class-specific
  paths; prove hidden objects are not exposed and simulation is not replayed.
- [ ] Finish model/database and texture destruction, restart and fallback
  invalidation. Acceptance: repeated load/unload/replacement cycles release all
  owned snapshots and never reuse stale model or texture data.

### 3. Texture production: DDS and TGA

- [x] Freeze stage 8 bounded DDS resolver observation and immutable snapshot
  path; its receipt records 25 private runtime cases and staged-only status.
- [x] Guarded base-level TGA stream capture and explicit source provenance.
  Private fixtures preserve actual-read bytes, coexist with a validated RGB
  OpenRead owner, and render the expected pixel after the source is removed.
  Native engine execution and live provider acceptance remain unchecked below.
- [ ] Prove live image/provider acceptance across representative DDS and TGA
  inputs. Acceptance: source, decoded/uploaded pixels and selected logical
  texture agree; unsupported or ambiguous sources fail safely with the defined
  fallback.
- [x] Bounded source/store/in-flight allocation and malformed, missing, oversized,
  replacement and restart recovery in isolated producer fixtures.
- [ ] Prove production-scale resource use and recovery with complete mod assets.

### 4. Materials, nodes, animation and object lifetime

- [x] Select exact texture dependencies for the pristine static SOD policy.
  Capture and rendering share one bounded parser; immutable model versions reuse
  cached dependency lists. Required missing/ambiguous/unsupported textures fail
  atomically, while unrelated records do not block a model bind. Private DLL
  fixtures prove retained ownership, expected pixels and exact Venture/Med
  dependencies; seven real-asset previews match stage 9 byte for byte.
- [x] Native service accepts host-selected diffuse textures per static emitted
  part and material-view model ID. Identical SOD bytes can render with different
  textures; binding errors preserve the previous image. Version 3 retains the
  older service tables and preserves shadow/relight settings. This is renderer
  support, not automatic live selection capture.
- [x] Implement the checked craft render-time slot-zero observer for the pinned
  engine. Shared parser node/group metadata maps unambiguous authored mesh names
  to observed selections. Private event-injection fixtures retain two instances'
  different textures through producer replacement/removal and API3 GPU readback;
  a separate bridge fixture preserves full x87/SSE/call state. Native engine
  execution and live acceptance remain unchecked below.
- [ ] Map live material selection, variants, colour keys and dynamic colour.
  Acceptance: representative game objects match their effective materials and
  variant selection, including models sharing an authored texture name.
- [x] Reuse resource epochs and stable instance view IDs across unchanged
  material palettes, including pose, camera and order changes. The core uses
  the stream under its material lock. A stable-context API4 host fixture
  verifies new pixels without asset rereads or changed GPU residency.
- [ ] Accept capture cost and caching for production scenes. Palette changes
  currently invalidate the whole epoch; no real-time world performance claim
  follows from the bounded stream fixtures.
- [x] Capture bounded rigid node poses after native animation evaluation and
  distinguish hidden self from pruned children. API4 resolves pose/visibility
  on every submission without reuploading hot assets; required-state batches
  cannot fall back to authored poses. Private bridge and retained-snapshot GPU
  fixtures pass.
- [ ] Accept effective node transforms, animation state and visibility in-game.
  Acceptance: compare moving/animated and hidden parts against the game at
  matched frames, including destruction and replacement.
- [ ] Complete model/database/texture lifetime handling through unload, cache
  invalidation, restart and device recreation; prove no stale handles or leaks
  in repeated headless cycles.

### 5. Physical lighting, shadows and render integration

- [x] Headless directional, point and spot lighting and opt-in shadow-map
  primitives have controlled renderer fixtures.
- [x] Parse copied class-ODF lighting controls and assemble an owned complete
  world-source list into native lights with matching per-source shadow masks.
  Headless API4 pixels verify source direction, colour/intensity, point movement,
  spotlight rotation/cutoff, falloff and independent coloured-light occlusion.
  The source list is supplied by the host; this does not discover game lights.
- [ ] Connect physical light sources and shadows to authoritative game world data.
  Acceptance: source selection, occlusion and caster/receiver transforms match
  controlled in-game reference scenes.
- [ ] Activate and accept the staged ODF light controls in the game, including
  class/object association, native-light overlap, global/ambient source and lifetime. Acceptance: each source parameter changes the
  expected pixels in controlled fixtures and representative scene comparisons.
- [x] Wire a default-off owned offscreen session to core view end with source
  tickets, class versions, completeness gates, current-image readback and
  diagnostics. A missing checked caster provider blocks helper loading.
- [ ] Integrate complete native world submission and presentation with a safe fallback.
  Acceptance: native frame ownership occurs only after complete scene capture
  and successful presentation; failure returns to the verified legacy path.

### 6. Full world, UI, effects, video and menus

- [ ] Cover background, terrain, stations, ships and remaining world draw types.
  Acceptance: capture/render inventory accounts for every observed draw type;
  unsupported types cannot be silently discarded.
- [ ] Port HUD, minimap, text, sprites, particles, weapon effects and video.
  Acceptance: representative gameplay and cutscene captures retain every
  element and ordering needed by the legacy frame.
- [ ] Port menu 3D scenes and native presentation behavior. Acceptance: all
  supported menus, transitions, resize and failure paths render and recover.

### 7. Startup migration, retirement and release acceptance

- [ ] Give native scene/asset capture and presentation their own startup path.
  Acceptance: native initialization is independent of the legacy DX8/DXVK
  renderer admission path and its optional graphics hooks.
- [ ] Check on-disk startup/import migration for each supported executable and
  hook image, with exact input hashes and rollback artifacts. Acceptance:
  clean launch succeeds with D3D8/9/D3DX providers unavailable, and removal
  checks prove no remaining legacy renderer dependency is required.
- [ ] Stage and verify rollback/deployment packages without changing protected
  installations until the staged image is accepted. Acceptance: hash-verified
  before/after receipt, repeatable rollback, and exact target identity recorded.
- [ ] Native Windows and user in-game acceptance. Acceptance: full startup,
  gameplay, menus, effects, video, device/reset and exit walkthrough passes;
  visual differences and fallback behavior are reviewed and recorded.
- [ ] Prepare Nexus showcase content and controls after accepted gameplay
  rendering. Acceptance: reproducible showcase scenes and documented controls
  work from the packaged build.

## Later backlog (not a native-renderer completion gate)

- [ ] Review proposed third avatars and optional Cardassian, Species 8472/Undine
  and Noxter content after the renderer completion gates are accepted.

## Near-term numbered milestones

- [x] Stage 13: checked post-animation rigid node state capture and API4 handoff.
- [x] Stage 14: stable owned material-frame streams and resource epoch reuse.
- [x] Stage 15: copied ODF light policy, owned world-light assembly and native service submission.

- [x] Stage 16: bounded copied class-light store and opt-in existing ODF dispatcher producer.
- [x] Stage 17: pinned complete world-list light sampling, immutable class/version and pose snapshot, independent of camera visibility.
- [x] Stage 18: native API5 world submission separates visible surfaces from shadow-only caster geometry; older API contracts remain intact.
- [x] Stage 19: owned render-session coordinator with stable callbacks, complete lighting/caster admission, cache/recovery and frame lifetime.
- [x] Stage 20: core end-of-view integration and diagnostics under explicit opt-in; unavailable/incomplete producers cannot take over graphics. Engine/in-game acceptance remains open.

Stages are bounded engineering checkpoints. Stage 20 does not complete live engine acceptance, complete native caster/visibility coverage, full gameplay rendering or release acceptance. Stages 26–40 below extend native scene composition; they do not promise a gameplay release at stage 40. Native offscreen pose/material evaluation and Windows/game acceptance remain separate gates.

- [x] Stage 21: owned GameObject membership and observed-geometry coverage audit.
- [x] Stage 22: owned observed-world assembly with explicit other-domain absence.
- [x] Stage 23: optional shared mesh uploads across material views. Strict build,
  focused GPU case and all 46 private runtime cases pass; staged only.
- [x] Stage 24: public API6 mesh-sharing control and owned session integration.
  Strict build, focused cases and all 49 private runtime cases pass; staged only.
- [x] Stage 25: structured readiness diagnostics and checkpoint acceptance.
  Bounded owned JSON, admission diagnostics, stale-source invalidation and a
  post-copy revocation fixture pass focused checks and all 50 private runtime
  cases; staged only.


## Stages 26–40: native scene composition

These milestones implement native capabilities and explicit host contracts. The
native engine still supplies only post-frustum effective poses/materials. No
previous frame or authored pose is promoted to an authoritative offscreen caster.
Each checkmark requires its implementation and focused tests; the stage 40
checkpoint additionally requires the full compatibility suite and preserved
installation hashes.

- [x] Stage 26: audited candidate-provider integration in the core world pipeline.
- [x] Stage 27: opt-in opaque-first and back-to-front transparent draw ordering.
- [x] Stage 28: per-surface cutout threshold and caster/receiver control.
- [x] Stage 29: explicit depth policy and retained-target composition passes.
- [x] Stage 30: per-surface point/linear and clamp/wrap texture sampling.
- [x] Stage 31: affine UV transforms for atlas and scrolling textures.
- [x] Stage 32: opt-in world-distance fog with surface exclusion.
- [x] Stage 33: selectable hard, bilinear and wider shadow filtering.
- [x] Stage 34: bounded screen-space sprite/atlas clipping geometry.
- [x] Stage 35: bounded camera-facing world billboard geometry.
- [x] Stage 36: reusable readback staging and target recreation recovery.
- [x] Stage 37: owned primitive-scene resource cache and transactional admission.
- [x] Stage 38: append-only public primitive-scene service with C/C++ ABI proof.
- [x] Stage 39: owned primitive-session coordinator and lifetime/reentry gates.
- [x] Stage 40: combined native showcase, compatibility verification and frozen package.

## Stages 41–50: complete-frame admission and native presentation

Scope and remaining engine gates: [stage 41–50 contract](native11-stage41-50.md).
Service and lifecycle details: [native presentation API8](native11-presentation.md).
Stages 41–49 passed the seven focused private cases; stage 50 additionally
requires the full compatibility run and frozen installation audit.

- [x] Stage 41: complete-frame domain inventory and explicit absence contract.
- [x] Stage 42: owned ordered pass admission and matching resource manifests.
- [x] Stage 43: host-window native D3D11 swapchain and GPU-only image copy.
- [x] Stage 44: stale-target, resize, window-loss and occlusion guards.
- [x] Stage 45: append-only public API8 and API1–7 compatibility.
- [x] Stage 46: complete-frame presentation coordinator and fallback result.
- [x] Stage 47: independent native service startup and explicit shutdown.
- [x] Stage 48: source revocation, suspension and lifecycle recovery.
- [x] Stage 49: private hidden-window presentation and failure-path acceptance.
- [x] Stage 50: full compatibility suite and frozen package/installation audit.

## Stages 51–65: actual engine integration and release

Current unnumbered work: [combined world/HUD integration candidate](native11-gameplay-integration.md).
The owned-world and sprite paths now compose and present on one D3D11 target in
private fixtures; ten focused runtime cases pass. Actual engine producers and
frame ownership remain incomplete, so this does not close a gameplay gate.

[Owned pose integration](native11-owned-poses.md) now adds independently sampled
instance timing, copied runtime channel data and per-instance transform evaluation.
Its core diagnostics compare against effective part states; native class policy,
visibility and LOD still keep stage 53 open.

[API9 all-model-parts integration](native11-all-model-parts.md) retains damage
and every LOD mesh and binds supplied owned poses/visibility to selected materials.
All 87 private cases pass, including C ABI, GPU scope switching, old clients and
real Venture/Mediterranean source-to-pose mapping. At that checkpoint the live
core observer used pristine sources; API11 now retains exact all-part identities
for observed material submission. Complete native class/LOD selection remains open.

[Load-template integration](native11-load-templates.md) now captures immutable
native node defaults at the completed model-load boundary, retains weak-model
cache ownership and seeds fresh poses without reading last-instance state. A
separate two-pass Craft input reader copies current health/system/class state.
That checkpoint passed 91 private runtime cases and four final-source sanitizer cases.

[LOD and arithmetic integration](native11-lod-inputs.md) carries the original
x87 settings into owned evaluation, captures immutable native LOD lists, reads
fresh camera/context/placement inputs and evaluates guarded per-instance LOD
visibility. Explicit class write masks reject stateful aliases even when stores
leave flags unchanged. A synthetic two-object API9 GPU fixture verifies selected
LOD footprints and changes on the next frame. All 100 private runtime cases and
18 strict host/sanitizer variants pass. Class/material authority, loader vertex
scaling, complete world/domain production and native presentation/startup still
keep stage 53 and full gameplay testing open.

[Native loader geometry](native11-loader-scaling.md) adds original load-word
provenance and positive uniform vertex scaling before normal reconstruction.
API10 and the owned world session connect this policy to D3D11; old interfaces
retain their behavior. All 106 private runtime cases, 24 strict host/sanitizer
variants and 32 Rust tests pass. This closes the bounded scaling gap, while
complete class/material and world-domain production remains open.

[Fixed scope and acceptance gates](native11-stage51-65.md) and
[animation timing/diagnostic evidence](native11-animation-clock.md). Stages 53–65 require
real producers/integration and acceptance; fixture-only extensions cannot close them.

- [x] Stage 51: pinned engine evaluation/state/presentation inventory.
- [x] Stage 52: verified native animation timing and keyframe-phase CPU foundation.
- [ ] Stage 53: authoritative complete per-instance offscreen geometry producer.
- [ ] Stage 54: complete remaining world-domain producers.
- [ ] Stage 55: physical game light/shadow integration and acceptance.
- [ ] Stage 56: complete live asset and device/restart lifecycle handling.
- [ ] Stage 57: actual HUD, minimap, text and sprite producers.
- [ ] Stage 58: complete weapon, particle, selection and transient effect producers.
- [ ] Stage 59: video and native menu scene producers/transitions.
- [ ] Stage 60: complete engine-frame native presentation and fallback ownership.
- [ ] Stage 61: independent game startup and exact-image dependency migration.
- [ ] Stage 62: representative gameplay parity and performance acceptance.
- [ ] Stage 63: verified exact-target deployment and rollback package.
- [ ] Stage 64: native Windows and user in-game acceptance on recorded artifacts.
- [ ] Stage 65: final release gate, frozen package and supported-scope record.

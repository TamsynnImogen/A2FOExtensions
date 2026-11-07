# Stages 51–65: engine integration and release gates

Stage 50 established a native presentation receiver with host-supplied frames.
The remaining work below is the actual engine producer/integration path. These
numbers are a fixed work breakdown, not a promise of completion through isolated
fixture counts. Unsupported producers remain unavailable; absence is never
inferred from a missing capture. Stages are checked only for the stated evidence.

51. Pin and trace engine evaluation, shared animation state and final presentation boundaries.
52. Recover and verify the native animation timing/keyframe-phase policy as an owned CPU foundation.
53. Produce authoritative per-instance offscreen poses, materials, visibility and LOD without replaying simulation.
54. Capture remaining world domains, including GameFeatures/background/terrain, with complete draw accounting.
55. Activate physical game light sources, native-light overlap policy and source-dependent shadows.
56. Close model/texture/database destruction, replacement and device/restart lifetime handling in the live producer path.
57. Produce actual HUD, minimap, text and sprite passes from the game's UI state.
58. Produce weapon, selection, particle and other transient effect passes.
59. Produce video and 3D menu scenes with accepted transitions and frame ordering.
60. Connect complete engine frames to native presentation with an accepted fallback/ownership protocol.
61. Provide independent game startup and verify executable/hook dependency migration for exact supported images.
62. Compare representative game scenes, animation, visibility and performance; resolve visual discrepancies.
63. Prepare and verify deployment, backup and rollback packages for the accepted exact target.
64. Complete native Windows and user in-game lifecycle/visual acceptance, recording exact tested binaries.
65. Freeze the release only after the preceding required gates pass; publish accurate supported scope and remaining limitations.

Current findings: instance RenderInternal mutates shared engine state and calls
animation channels before drawing. Its stored node hierarchy is therefore not
an independently fresh per-instance offscreen snapshot. A plain world-list walk
or last-frame matrix cache cannot satisfy stage 53. The final RefreshDisplay
EndScene/Present chain is a verified candidate boundary, not an independently
accepted replacement startup path. No legacy drawing is suppressed on this evidence.


## Current nine-gate status

All nine remain open. The bounded evidence below is not a full-game acceptance claim.
The previous [world Craft-view checkpoint](native11-world-views.md) passes
137/137 final-artifact private cases, the strict builds and pure-reader Clang
ASan/UBSan. Ordered distance/fade snapshots retain the exact model source and
reject input/roster drift. Mutable near-fade settings are now captured, with
49,152 additional native CPU comparisons. The final 90-second private legacy
run qualifies static placement/view regions and runtime fade values, logs no
new fault and verifies cleanup. All seven protected installed hashes remain
unchanged. This is not live skirmish view acceptance, complete world-frame
production or native presentation takeover.

The earlier [world logical-preparation checkpoint](native11-world-preparations.md)
passes **143/143** final-artifact cases, strict builds and pure-reader sanitizers.
It owns fresh systems/Borg/hull inputs for the checked roster without copying
last-rendered model flags. Its 90-second private legacy run qualifies the real
Fleet saved-original base setup, verifies all five callback-closure regions and
the gateway across 44 post-install observations, retains the load template,
logs no new fault and verifies cleanup. This does not establish full class/FOW,
selected material or geometry authority. All nine implementation gates stay open.

The subsequent [prepared-visibility helper](native11-prepared-visibility.md)
passes **144/144** final-artifact cases and strict/sanitizer reader checks. It
combines checked node roles/context with bracketed logical preparation rather
than cached render-instance flags. It remains separate from the native game
producer and does not close class/FOW/material or geometry authority. The core
is unchanged from the verified 90-second private run; all nine gates stay open.

The previous [world base-visibility core integration](native11-world-base-visibility.md)
passes **147/147** final-artifact cases. Core consumes the prepared reader for
its checked roster, pins completed-load template tuples and evaluates each
actor under its captured arithmetic context, with current source/parent/context/
lifecycle checks and late-publication discard. Its final private legacy startup
qualifies six base-policy regions and one slot early/late, retains the template,
logs no new fault and verifies cleanup. This is base node policy only; full
class/FOW/material, live skirmish acceptance and a native geometry provider remain
open. The material callback still needs an explicit ownership/state contract.

The previous [completed-load material baseline integration](native11-material-baselines.md)
passes **148/148** final-artifact cases, strict changed-source and fixture builds,
host reader/cache checks and Clang ASan/UBSan. Raw slots and the load-time Borg
alternate now retain exact template/model/texture owners before later draws can
alter shared slots. Later cache misses cannot invent a baseline, and optional
capture failure preserves the separate node template. Effective class material
selection, group mapping and extension overlay ordering still need source
qualification; all nine implementation gates remain open.
Its final 90-second private legacy run captures 24 mesh baselines alongside the
30-node template, logs no new fault, retains loaded source compatibility and
verifies cleanup; installed files and seven reference previews are unchanged.

The previous [base/Borg material policy and class-table helpers](native11-borg-materials.md)
pass **151/151** final-artifact cases, strict new-source/fixture builds, host
checks and Clang ASan/UBSan. Original selector/code-cave/existing-vector setter
parity covers **9,600** comparisons and **32** source mutations. The table reader
retains the exact load baseline/class tuple and validates every eligible row
without reading draw-time slots. Core remains the exact preceding startup-
verified binary; class/actor/caller qualification and world producer connection
are still required. A write set leaves unspecified slots to owned prior state;
it does not establish an effective material. All nine gates remain open.

The previous [Core world/actor Borg operation integration](native11-world-borg-materials.md)
passes **153/153** final-artifact cases, strict changed-source/fixture checks and
host/Clang sanitizer batch tests. Core binds actual preparation/model parents,
the load-baseline cache and checked table/policy helpers into owner-thread
candidate operation snapshots, including unobserved actors. Fresh logical Borg
inputs, two sweeps and final source checks reject stale or replaced owners.
This closes the helper-to-Core correlation step; effective material submission,
class readiness/caller/overlay qualification and all nine gates remain open.
The final 91-second private legacy menu run validates all eight named-operation
regions early/late and retains the real load baseline, with no new fault and
verified cleanup. Live skirmish actor/material acceptance remains open.

The [API11 live observed material path](native11-live-materials.md) connects sealed
Core captures to real D3D11 preparation and optional private part-state previews.
Its source supplies authored SOD values and observed selected diffuse textures;
the API resolves every cached part on each frame. Exact node/group identities,
resource owners, source tickets and separate preview/world eligibility are
checked. This completes that observed submission handoff, while complete-world
material producers and all nine gates remain open.
The final-artifact suite passes **157/157** private cases; six focused checks
include repaired epoch-comparison and callback-scope regressions. Seven prior
reference previews remain byte-identical and installed files remain unchanged.

The [live skirmish inspector](native11-gameplay-inspection.md) is the first manual
visual test route for observed DX11 ships, poses and materials. Its source-checked
snapshot exporter, localhost viewer and separate game copy pass **160/160** private
cases and **15** Python tests. The actual batch launcher/export startup runs for
**90.3 seconds**, with no new fault and verified cleanup. This makes that bounded
inspection available; it does not close complete-world, shadow, presentation or
user-acceptance gates.

The [base Craft material route](native11-material-route.md) additionally pins
the selector's conditional caller branch, supplies per-actor bypass/dispatch
status and revalidates correlated table bytes on current access. This advances
material source qualification but leaves class readiness, full overlays and
all nine implementation gates open. No new game startup is performed.

| Gate | Current progress | Required to close |
| --- | --- | --- |
| 53 | Owned poses, load templates, LOD/scalar arithmetic and fresh distance/fade inputs verified; actual live 30-node load template captured; checked builtin native fallback qualified with epoch/quiescence guards; owned current-DB/raw-placement associations and fresh world-roster distance/fade snapshots with runtime parameters; fresh logical systems/Borg/hull preparation with checked Fleet saved-original chain; prepared visibility now consumed by core world snapshots with source/template/context/lifecycle guards | Complete class/procedural/FOW/material authority, custom-clip snapshots, authoritative per-member selected model/material/visibility and actual complete-world producer |
| 54 | Checked GameObject membership and coverage auditor | GameFeatures, backgrounds/terrain and complete draw accounting |
| 55 | Typed light sources and native shadow renderer receivers | Complete live caster/light integration and native overlap policy |
| 56 | Model/texture destruction retirement, pointer reuse and pending-IO revocation integrated; Fleet terminal and texture observers reported armed; FOFS-loss fixture revokes stale database records | Device/restart handling, observed live destruction and lifecycle acceptance |
| 57 | Checked native sprite source route, immutable traces and exact colour policy integrated; live observer reported armed under checked coexistence owner | Complete source coverage and styles for HUD, text and minimap |
| 58 | Primitive/billboard rendering receivers | Native weapon, selection, particle and transient-effect sources |
| 59 | Native presentation/menu building blocks | Actual video/menu scenes and accepted transition ordering |
| 60 | Owned complete-frame receivers and existing-HWND presentation fixtures | Connect complete engine frames, fallback and presentation ownership |
| 61 | Static backend/startup boundary inventory | Independent engine startup and verified dependency migration |

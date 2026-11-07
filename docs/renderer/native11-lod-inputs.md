# Native11 LOD and arithmetic integration

This is work toward stage 53. It is a private integration candidate, with no
installed-game deployment or gameplay-readiness claim. Stage 52 remains the
last completed numbered stage.

## Implemented scope

- The node observer transports the original x87 control word from its saved
  machine state. Owned pose evaluation uses its precision and rounding settings
  and restores the observer environment. Sealed material captures retain this
  provenance; unknown or mixed settings remain distinguishable.
- Completed model loads capture immutable LOD bounds and the actual native
  descriptor order. Two checked reads must agree. Current selection, mutable
  node flags and last-rendered matrices are excluded. Cache accounting includes
  this metadata and retains weak model ownership.
- A guarded owner-thread adapter reads fresh camera, context, LOD overrides,
  quality and object placement. Camera/context identity must match explicitly,
  placement must still match the bound object, and two complete reads must agree.
  It never uses the engine's last-rendered-instance slot.
- Owned LOD evaluation composes native Matrix34 transforms, including the root
  local transform and runtime scales, with the original per-element arithmetic
  association. Camera translation has its separately recovered association.
- Native LOD selection preserves signed overrides, quality limits, forced
  highest detail and list-end clamping. The first threshold compares the
  extended x87 distance; subsequent thresholds use its stored binary32 value.
- Visibility evaluation respects native child/sibling order, own-hidden and
  ancestor-prune flags, nested controls and empty lists. Stateful class/LOD flag
  aliases (including class writes that leave a flag unchanged) and hidden controls that would retain an earlier selection are
  rejected. This does not establish class authority for arbitrary game objects.

## Native evidence

Supported CPU comparison input is Roots `ArmadaL.exe`, SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
The profile generator pins 23 code regions, with HIGHLOW normalization.

| Pinned code | Evidence used |
| --- | --- |
| `465940–465a7a` | Matrix34 local × parent, binary32 stores |
| `639460–63951a` | Node scale applies to basis, translation copied |
| `620ddb–620e76` | Camera-space translation association |
| `630a50–630baf` | Distance, override, quality and selected-node flags |
| `6209b0–620eec` | Database traversal, root composition and active camera |
| `62e780` caller path | Instance placement and conditional runtime scale |

The isolated CPU fixture executes the pinned multiply, scale and LOD routines
without image startup. The camera test copies only the reviewed straight-line
CPU block into a private executable allocation with an appended return. No game
simulation, graphics entrypoint, GUI input or installed file is involved.

Across 12 x87 precision/rounding combinations, the focused native fixture passes
24,576 matrix products, 24,576 camera translations, 24,576 scaled locals and
501,120 LOD selections. It includes prior-selection changes, equal-selection
no-op behavior and a directed extended-versus-binary32 threshold straddle.

The full private suite passes 100 runtime cases; nine strict host tests and nine
Clang ASan/UBSan variants also pass. The synthetic API9 GPU chain takes real Rust
SOD metadata through owned poses, LOD visibility, bound material state and world
submission. Two objects share one model and swap LOD choices on the next frame;
pixel assertions check the selected blue/red geometry and absent damage overlay.
Class inputs and scene completeness are explicit synthetic fixture facts.

The private core build is `20261104-native11-lod-input-integration-wip`.
Canonical 198 exports and the prior core import set are preserved; helper API1–9
and its single export remain compatible. Seven reference previews are byte
identical, and seven protected installed binaries/configurations are unchanged.
Exact artifacts and source hashes are recorded in the
[checkpoint receipt](../../build/native11/lod-input-milestone.json).

## Remaining integration gates

- Establish complete class-specific transform, flag and effective material
  authority, including runtime colour/alpha/UV behavior and loader scalar effects
  on copied vertex geometry.
- Combine those sources into an actual complete-world geometry producer and
  connect all native frame domains. A visible-object list cannot prove offscreen
  casters or absence of other domains.
- Connect engine startup, presentation and fallback ownership, then complete
  representative gameplay, native Windows and user visual acceptance.

The [completion checklist](native11-progress.md) and
[fixed stages 51–65](native11-stage51-65.md) retain these open requirements.

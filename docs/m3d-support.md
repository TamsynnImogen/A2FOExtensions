# Star Trek: Legacy M3D runtime support

`A2FOM3D.dll` implements a model-resolver bridge rather than a disk conversion
pipeline.

## Load boundary

Armada's public geometry loader is at RVA `0x0022cf10`, but Fleet Ops already
detours that routine during `FleetOpsHook.dll` initialization. The active Fleet
Ops replacement resolver is at FleetOpsHook RVA `0x001d49a4`. A2FOM3D chains
that replacement rather than expecting Armada's original prologue to still be
present. This preserves Fleet Ops' VFS/database/fallback behavior for ordinary
SODs while allowing M3D winners to be intercepted first.

The module indexes active extension roots in increasing precedence. Later roots
override earlier ones. A native SOD and M3D in the same root resolve to the SOD;
a child-root M3D can override a parent-root SOD.

For an M3D winner, the file is parsed directly and the first-stage compatible
Storm3D payload is kept only in memory. Armada's parser at RVA `0x0023fa60` is
called with a synthetic instance of its own buffered input class. Its vtable is
at RVA `0x002bc874`; the relevant 32-bit layout is:

| Offset | Meaning |
| ---: | --- |
| `+0x00` | native input vtable |
| `+0x04` | parser-created Storm3D database pointer |
| `+0x08` | format version populated by Storm3D |
| `+0x0c` | diagnostic filename |
| `+0x10` | FILE pointer, null for memory mode |
| `+0x14` | buffer begin |
| `+0x18` | buffer end |
| `+0x1c` | current read pointer |

The native read method uses the memory buffer whenever `+0x14` is non-null.
This lets A2FO reuse Storm3D's database/node/material construction without ever
creating a `.sod` file.

## Fleet Ops 1.93 mesh layout

Disassembly of the supported ArmadaL build shows that Fleet Ops' patched 1.93
mesh reader is newer than the commonly documented Armada SOD layout. After the
blend/material string it expects a 32-bit mesh-render-flags value, a 32-bit
texture count, then a texture name plus 32-bit load flags for every texture.
For format 1.92+ it then expects a second identifier used for the
Borg/assimilation texture; an empty identifier makes Armada derive texture 0 +
`_b`. Only after that does the 1.91+ 16-bit extra field and the three geometry
counts appear. In particular, a native diffuse+bump pair uses mesh render flag
`0x2` and bump texture load flag `0x200`. A2FOM3D emits this runtime layout and
can therefore preserve Legacy diffuse and bump slots in the first-stage
in-memory stream.

The stock memory-input read routine at ArmadaL RVA `0x00240260` blindly copies
from the cursor and does not stop at the stream end. A2FOM3D clones the native
vtable and replaces only that raw-read slot with a bounds-checked equivalent;
an overrun is zero-filled for parser safety, recorded, and the resulting model
is rejected/cleaned instead of being used.

The native parser allocates and registers the database before node parsing. Its
node-failure path returns null without destroying that partial database. When an
M3D compatibility stream is rejected, A2FOM3D mirrors the normal engine owner
cleanup sequence (database destructor RVA `0x00220690`, then engine delete RVA
`0x002527d0`) before falling back to Fleet Ops. This prevents failed loads from
poisoning later database resolution or stalling match loading.

## Renderer follow-up

The transient compatibility stream deliberately handles only the data that has
known Fleet Ops Storm3D semantics. M3D specular/glow/detail slots and richer
Legacy shader flags should remain sidecar metadata and be consumed by
A2FONebulaRenderer/related material hooks. This avoids permanently degrading
M3D into the older SOD format while still reusing the game's stable runtime
object model.

## Node hierarchy ordering

The `2026-09-29-damage-vents` build adds a bounded runtime adaptation for
Legacy damage emitters outside the `damage` branch: recognized static
engine/weapon break vents are attached to `damage/engines` or `damage/target`,
with full model-space transform preservation. Existing damage emitters and
ordinary emitters remain as authored. See
[damage vent evidence and acceptance check](m3d-damage-vents-2026-09-29.md).

Runtime layout correction, 2026-09-29: the native constructor and recursive
collector prove **first child = node+0x1c, next sibling = node+0x20, parent =
node+0x18**. The earlier M3D runtime adapter reversed the forward links and
then rewrote the tree using that mistaken interpretation. Its apparent Akira
89-parent mismatch was an audit bug. The `2026-09-29-native-links` build uses
the correct fields and performs a read-only hierarchy audit; it leaves the
constructor's links intact. See [pinned disassembly](m3d-node-links-2026-09-29.asm).

Armada's SOD node loader resolves each node's parent identifier immediately,
while that node is being read. The lookup only sees nodes that have already
been inserted into the current Storm3D database. If a child appears before its
parent, the lookup returns null and the new node is treated as parentless; the
loader then assigns it to the database root slot at `+0x3c`. Repeating this for
a reverse-ordered Legacy hierarchy leaves the database with only the final
parentless node reachable from the active root even though every node was
successfully allocated.

A2FOM3D therefore emits the transient Storm3D node stream in stable
parent-before-child topological order. Legacy sibling order is preserved where
possible. Dangling/self parents are normalized before root selection, and a
cyclic branch is conservatively reparented to the canonical root so it remains
reachable instead of replacing the active database root later in the stream.

## v19 material-group mesh compaction

Legacy M3D meshes can contain multiple material groups. A2FOM3D exposes those groups as separate Storm3D mesh nodes. v19 compacts each split node to only the position and UV entries referenced by that group's faces, then remaps the indices before writing the in-memory Storm3D 1.93 stream. This avoids duplicating a large sparse source mesh into every group and targets MeshUpdate/MeshVB failures seen on the Akira main hull groups. Bump binding remains disabled in this diagnostic branch while diffuse/mesh stability is verified.

## Interior geometry restored (2026-09-29)

The Akira-specific `in_*` diagnostic exclusion has been removed. Previously,
`in_l_engine_0`, `in_pod_0`, `in_r_engine_0`, and `in_f_akira_a` were serialized
as null transforms, omitting geometry and runtime texture bindings. They now
use the ordinary material-group mesh path, including compaction, numeric-parent
hierarchy mapping, diffuse material lookup, and local bounds. No authored mesh
name is excluded. Verify the Akira interior and hull together in a fresh game
session; conversion tests alone cannot verify draw-state or visibility behavior.

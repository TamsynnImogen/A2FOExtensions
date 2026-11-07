# A2FOM3D

Built-in preparation retains old marked generated SOD cache files and their
markers byte-for-byte, ignoring them when selecting M3D winners. It does not
run the standalone compatibility module's legacy disk-cache cleanup.

Runtime Star Trek: Legacy `.m3d` model compatibility for Fleet Operations.

## How it works

`A2FOM3D.dll` is auto-enabled when installed, even for managed mods whose
`[modules]` lists were written before M3D support existed. An explicit
`rejectX = "A2FOM3D"` still disables it.

The authored model stays as `.m3d`. Fleet Ops itself replaces Armada's stock
SOD resolver at Armada RVA `0x0022cf10` before extension modules initialize.
A2FOM3D therefore chains from FleetOpsHook.dll's replacement resolver (RVA
`0x001d49a4`) instead of trying to overwrite the already-installed Armada
detour. When an ODF asks for a basename such as `F_Akira`, extension-root
precedence is evaluated first:

- a native SOD in the same or a later root wins normally;
- an M3D in a later child root can override a parent-mod SOD;
- if the winning asset is M3D, it is parsed on demand in memory.

For the first runtime stage, M3D geometry is translated into an in-memory
Storm3D 1.93 compatibility stream and handed directly to Armada's own parser at
RVA `0x0023fa60` through the engine's native memory-backed input-stream layout.
No `.sod` file or persistent cache is created.

Old `.sod.a2fo-m3d` caches created by the v1/v2 prototype are removed safely at
startup before source precedence is indexed.

ODFs keep using the normal basename:

```ini
basename = "F_Akira"
```

with only:

```text
sod/F_Akira.M3D
```

required in the mod.

## Current runtime coverage

The 2026-09-29 interior-restoration update removes the diagnostic exclusion for
`in_l_engine_0`, `in_pod_0`, `in_r_engine_0`, and `in_f_akira_a`. All authored
mesh groups now follow the same renderable conversion path, retaining their
parent transforms, materials, diffuse binding, and local bounds. The startup
log identifies this build as `2026-09-29-interiors`.

The existing numeric-parent resolution, parent-first stream order, duplicate-name
handling remain in use. This update does not change
the current diffuse-only renderer mode; bump references remain sidecar metadata.
Headless conversion checks do not establish in-game rendering acceptance.

- Legacy M3D layouts 1.94 through 2.06.
- Older `Storm3D_SW` / `StarTrekDB` streams carrying an `.m3d` extension.
- Node hierarchy and transforms.
- Hardpoint/null nodes, mesh nodes, sprites, LOD nodes and emitters.
- Geometry, UVs, lighting-material groups, culling and diffuse textures; authored
  bump references are retained as metadata while runtime bump binding is disabled.
- Multiple Legacy texture groups are split in the transient compatibility
  stream so each group can retain its diffuse texture.
- Matrix animation channels (type 0), scale channels (type 5), and type-4
  texture-animation references.
- Runtime model results are cached by basename and scale after the first load.

## Runtime compatibility details

Fleet Ops' patched 1.93 mesh reader differs from the older public SOD format
documentation: it stores a mesh render-flags field followed by a texture count
and per-texture load flags, then an additional 1.92+ Borg/assimilation texture
identifier before the geometry counts. An empty identifier means "derive texture
0 + `_b`". A2FOM3D emits that native layout. Diffuse textures use load flags
`0`. The native paired-bump representation uses DOT3 render flag `0x2` and
texture load flag `0x200`, but this diagnostic branch emits diffuse only and
retains authored bump references outside the stream.

The native memory reader does not bounds-check its memcpy path. A2FOM3D uses a
guarded clone of the native input vtable for M3D streams so a future unknown
field is logged and rejected instead of turning into an access violation.

If Armada rejects a transient stream after it has allocated a Storm3D database,
A2FOM3D now destroys and frees that partial database before falling back to the
normal Fleet Ops resolver. This prevents failed M3D experiments from leaving a
zombie database registered in the engine and stalling later game loading.

## Next native stage

Legacy has explicit specular, glow and detail texture slots plus shader and
material flags that are richer than Fleet Ops' native 1.93 mesh representation.
They should not be flattened into permanent SOD data. The intended next stage is
to retain those M3D records as runtime sidecar metadata keyed to the generated
Storm3D mesh/material and feed them into A2FOExtensions' existing renderer hooks.

That will allow M3D-only rendering features to survive while the stock Storm3D
object hierarchy, animation, hardpoints, culling and instance code continue to
be reused.

Legacy type-2 effect nodes currently become null transforms because Armada II's
known Storm3D runtime has no direct SOD equivalent. Legacy-only animation
channel types 1-4 are currently skipped with diagnostics; type 0 and type 5 are
preserved.

## Fleet Ops resolver finalization

FleetOpsHook's public SOD resolver performs one additional retain/finalize step
on a successful native load: the backing resource referenced at model `+0x40`
has its reference count at `+0x10` incremented and a resolver-mode byte at
`+0x14` set to `0` for the normal resolver or `1` for the no-fallback resolver.
Because A2FOM3D intercepts the resolver entry point, M3D results must reproduce
that bookkeeping before returning to Fleet Ops. Cached M3D results repeat the
retain step for each resolver request, matching the native ownership contract.

## Hierarchy normalization and runtime verification

Legacy 1.96+ uses numeric parent IDs. The module resolves the complete ID graph only after all nodes are parsed, because Legacy files commonly store children before parents. Parent ID `0` is treated as the root sentinel only when the file does not define a real node ID `0`.
The compatibility parser keeps that sentinel distinct from a real node ID and
will not accidentally attach roots to a node whose own exported ID is zero.

Duplicate Legacy node names are disambiguated temporarily with their numeric IDs while Armada parses the compatibility stream, so repeated names such as `lights`, `orientation`, or `break` cannot collapse distinct parent branches. After the runtime nodes exist, A2FOM3D audits the native graph without rewriting its links, then restores the original Legacy node names.

The `2026-09-29-damage-vents` build adapts static leaf emitters under recognized
Legacy engine/weapon `break` branches to existing `damage/engines` or
`damage/target` selectors, preserving their model-space transforms. This makes
`damage` their grandparent and lets Armada control their subsystem damage
visibility. Other node links remain authored; animated or ambiguous cases are
left unchanged. See [scope, evidence and manual test](../../docs/m3d-damage-vents-2026-09-29.md).

The `2026-09-29-native-links` build corrects the runtime layout to first child at
`+0x1c` and next sibling at `+0x20` (parent is `+0x18`). The earlier module had
these forward links reversed: its audit incorrectly reported 89 mismatches for
the Akira, then its repair rewrote a valid native tree into that incorrect
layout. Mesh counts still looked correct to the same reversed walker even
though native traversal could no longer reach the geometry properly. The
destructive repair has been removed; all authored interior meshes remain.
See [binary evidence](../../docs/m3d-node-links-2026-09-29.asm).

Armada's 1.93 loader stores only one parentless node in `model+0x3c`; if a
Legacy M3D contains multiple top-level nodes, the last one would otherwise
replace the active render root even though parsing succeeds. A2FOM3D now inserts
one neutral `a2fo_m3d_root` control node for multi-root files and parents every
Legacy root beneath it.

After native parsing, A2FOM3D walks the live Storm3D tree and logs reachable
node/mesh/renderer counts. Mesh nodes whose renderer pointer is still null get
one native `MeshUpdate(0)` rebuild attempt using the same Armada routine used by
A2FONebulaRenderer. This provides a precise distinction between hierarchy,
mesh-instantiation, and later renderer/culling/material failures.

### Legacy LOD compatibility

For the current compatibility path, Legacy type-11 LOD control nodes are emitted
as neutral Storm3D transform nodes. Legacy and Armada/Fleet Ops use different
distance/child-selection state for these nodes; passing them through unchanged can
make an otherwise-correct M3D change or disappear as camera zoom changes. Child
geometry remains attached and visible while the Legacy LOD semantics are mapped
properly.


## v21 bounds compatibility diagnostic

Runtime M3D meshes now carry local-space min/max bounds calculated directly
from the Legacy group vertices.  A2FOM3D writes those bounds to the live
Storm3D mesh before `MeshUpdate(0)` so distance/culling decisions do not depend
on stale bounds from the transient compatibility stream.  Legacy local node
matrices are still serialized unchanged; parent-before-child normalization
reorders records only and does not modify the 3x4 transforms.

## M3D-only final draw compatibility

When the runtime adapter is in diffuse-only mode, live M3D meshes are registered
with the core renderer.  Fleet Ops may still route those meshes through its
DOT3 MeshVB path even with no runtime bump texture attached.  The renderer now
recognizes only those registered M3D meshes and, for the final indexed DOT3
draw, temporarily restores an ordinary fixed-function diffuse stage: pixel
shader off, stage 0 bound to the mesh's authored diffuse texture, stage 1
empty, and colour/alpha modulation against vertex lighting.  Native SOD meshes
retain their existing Fleet Ops DOT3 behavior.  All touched D3D state is saved
and restored around the individual draw.

## Built-in availability

`A2FOExtensions.dll` provides this feature under the original `A2FOM3D`
selection name, with an independent menu policy. Turning it off affects future
resolver lookups; existing resolver caches and loaded assets remain. The
standalone SDK target remains available, but normal releases do not require its
DLL and stale standalone copies are ignored during discovery.

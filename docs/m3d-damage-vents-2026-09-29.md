# M3D damage vent compatibility — 2026-09-29

The previous native-links DLL now loads the Akira with all 90 expected runtime
nodes and zero parent mismatches. The remaining always-on plasma vents came
from a format-semantic difference: the shipped Legacy Akira places its three
type-12 `plasmalrg` nodes under `break` under `l_engine`, `r_engine`, and
`weapons`. None of those paths includes `damage`.

The damage-vents converter maps those static emitter leaves to
`damage/engines` (two nacelle vents) and `damage/target` (weapons-pod vent).
`damage` is therefore their grandparent. Full affine compensation preserves
each emitter's model-space position and orientation, including inherited scale.
All three original break helpers, mesh parents, all 13 mesh groups, all 5,898
triangles, and all four `in_*` meshes remain present. The M3D asset is unchanged.

This is an explicit Armada runtime compatibility mapping, not a correction to
the original authored numeric parent graph. The runtime hierarchy audit now
checks the adapted compatibility graph. The MilkShape importer/exporter is
unchanged by this fix.

## Native evidence

Workspace and installed ArmadaL.exe SHA-256:
`63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36`.
See [disassembly](m3d-damage-vents-2026-09-29.asm):

- `0x4bf7e6..0x4bf878`: class initialization looks up the `engines` and
  `target` nodes and stores them at class offsets `+0x418` and `+0x410`.
- `0x4cb090`: instance initialization sets the five damage visibility bytes
  `+0x9c..+0xa0` to one.
- `0x4cb1e6..0x4cb244`: each draw writes bit `0x2` on the engines/target
  nodes using instance bytes `+0x9d`/`+0x9e`, before the native render call
  at `0x4cb318`.

The mapping reuses those native subsystem damage controls. It does not add
a custom hull-health threshold or a new hook. Actual healthy/damaged/repaired
particle behavior requires a client test.

## Deliberately bounded mapping

Only type-12 leaf emitters directly beneath `break` are candidates. Recognized
part names are `l_engine`, `r_engine`, `engine`, and `weapons`, compared without
case. A unique corresponding null selector immediately under `damage` must
already exist. Files with animations or references, emitter children, ambiguous
selectors, cyclic transform ancestry, or singular target transforms are left
unchanged with a diagnostic. Other part names and ordinary always-on emitters
are left as authored. This does not yet translate every Legacy break convention.

## Verification

- `make build/m3d_conversion_test` and
  `./build/m3d_conversion_test '/home/tamsynn/Games/st legacy/M3D/F_Akira.M3D'`:
  pass. Synthetic fixtures cover child-first duplicate branches, translated,
  rotated and nonuniformly scaled transforms, existing damage emitters,
  ordinary emitters, missing/duplicate/singular selectors, animations,
  references, and emitter children. Actual Akira asserts three adapted vents,
  two engines parents, one target parent, damage grandparent, 90 nodes,
  13 mesh groups and 5,898 triangles.
- Full recursive corpus: 258 modern conversions, 48 old SOD passthroughs,
  1,738 groups/bindings. The same 897 known failures remain (895 chunk-layout
  files and two unsupported version-1.93 M3Ds), failure-set hash
  `2e2a17514218e576`. No new conversion failures or missing bindings.
- Read-only independent source review found no concrete blocker.
- Rebuilt Windows DLL and `m3d_runtime_hierarchy_test.exe`; headless Wine
  hierarchy regression passed. Existing thiscall/cast/unused-helper compiler
  warnings remain. No GUI or live game test was performed.

## Deployment and manual check

Build target: `build/modules/A2FOM3D.dll`. Install target:
`/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOM3D.dll`.
Backup of the preceding DLL, affected source, and latest game log:
`.deployment-backups/m3d-damage-vents-20260929/`.

Installed and built DLL SHA-256:
`28df88e257ab7eefc8ac3893337db9fb82e4d846986765b12fafb6d188a93864`.

After a full restart, the log should identify `2026-09-29-damage-vents`, list
three `Mapped Legacy break emitter` messages for Akira, and retain zero runtime
hierarchy mismatches and 13 mesh renderers. Check a fresh healthy Akira for no
damage vent emission, damage its engines/weapons enough to activate native
subsystem effects, then repair and confirm the vents stop. Compare a healthy
and a damaged Akira together to check per-instance switching.

# A2FOM3D interior geometry restoration — 2026-09-29

User request: update A2FOM3D and remove the `in_*` mesh-group exclusion.

## Result

Removed `hide_problem_interior_mesh` and its null-transform emission branch.
`in_l_engine_0`, `in_pod_0`, `in_r_engine_0`, and `in_f_akira_a` now follow the
ordinary mesh/material-group conversion path. Existing numeric-parent mapping,
parent-first ordering, duplicate-name handling, live hierarchy repair, group
compaction, and diffuse-only draw behavior are retained. No new native offsets,
hooks, ABI changes, or core-renderer changes were introduced in this update.

Startup marker: `M3D compatibility build 2026-09-29-interiors`.

## Build and deployment

Source: `/home/tamsynn/A2FOHookExtensions/modules/A2FOM3D/`.
Build: `make build/modules/A2FOM3D.dll -j 2` succeeded. Compiler reported existing
thiscall/FARPROC-cast/unused-helper warnings in unchanged code. PE inspection
confirms an i386 DLL and A2FO_ModuleInit/A2FO_ModuleShutdown exports.

Built DLL: `/home/tamsynn/A2FOHookExtensions/build/modules/A2FOM3D.dll`.
Installed DLL: `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOM3D.dll`.
Deployment timestamp: 2026-09-29 23:24:26 +0100; size 11,993,664 bytes.
Both SHA-256 values:
`0b8f6260aaea264f83863ddde4774513af0b2e14d947e89d8ee052539f79ec3a`.

Backup: `.deployment-backups/m3d-restore-interiors-20260929T222026Z/` contains
prechange module sources, docs, tests, built DLL, `installed-A2FOM3D.dll`, and
`A2FOExtensions-before.log`. Old deployed SHA-256:
`7e594899dfd1c3cef44889779cc761c860ce3a1ccc6301e19ac89e7f785e760c`.
The scoped source delta is `restore-interiors.patch` in that backup folder.

The pre-deployment log explicitly selected the `test` mod and loaded this global
Data/modules DLL with `Mods/test/sod/F_Akira.M3D`. No game process was running
when deployed. The original log was preserved and the game was not launched.

## Headless evidence

- `make build/m3d_conversion_test` and `./build/m3d_conversion_test` passed.
- Tests verify all four formerly excluded names and a mixed-case variant retain
  type-1 nodes, two compacted groups, geometry/UV indices, materials, normalized
  diffuse names, numeric parent links, runtime texture bindings and local bounds.
- The same tests compiled against the prechange converter fail at the type-1
  mesh assertion, confirming the regression test detects the original exclusion.
- Read-only recursive corpus audit: 258 modern files converted, 48 older SOD
  streams passed through, two v1.93 M3Ds unsupported. 895 files under `chunks/`
  fail parsing with identifier-length errors; these are not claimed as passing.
  All 258 converted files retain all 1,738 material-group texture bindings.
- Before/after comparison against the backed-up converter confirms identical
  failing path/error lists (897 entries; sorted FNV64 digest `2e2a17514218e576`).
  The v1.93/chunk limitations predate this change. Texture bindings increased
  from 1,729 to 1,738, removing group/binding mismatches in F_Akira,
  F_Ambassador, F_Nova, and F_Steamrunner; there are now zero mismatches.
- The installed Akira fixture matches the reference-file SHA-256
  `d239fb66b1bd82d7ea2e420b2a14a7fa0878b75cf03c45c6b3ff108c5e6e06a6`.
  It now produces 13 mesh-group texture bindings (previous runtime log: nine).
  Four restored interior groups contain 2,943 source triangles and use the
  `damageinterior` diffuse reference. Their parent links are:
  `in_l_engine_0 -> l_engine`, `in_pod_0 -> weapons`,
  `in_r_engine_0 -> r_engine`, `in_f_akira_a -> hull_0`.

## Manual acceptance still required

Restart Fleet Ops, select the same test mod, and start a fresh match containing
its Akira fixture. Inspect hull/interior alignment and textures while rotating
and zooming the camera; ensure hull geometry remains visible with interiors
restored. Check damage behavior as applicable.

The new log should contain the `2026-09-29-interiors` startup marker, four
interior `damageinterior` texture bindings, and an Akira runtime tree with 13
mesh groups/renderers (expected; not yet observed in-game). There should be no
old "interior mesh ... emitted as a null transform" diagnostics. Check hierarchy
missing/mismatch and diffuse-miss counts. Copy the log before another launch.

Bump textures remain disabled at runtime in this diagnostic branch. This task
removes the geometry suppression; it does not claim visual or in-game acceptance
or complete Legacy damage-material semantics.

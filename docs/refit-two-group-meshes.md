# Refit two-group (base + team) mesh replacement

Created 2026-11-06 by Claude. This work is opt-in: `[Refit]TwoGroupMeshes=1` enables it, and the
default is 0. It follows from [the SOD coverage census](refit-sod-coverage-census.md), finding 2. There,
all 162 multi-group FO4 Nexus meshes are exactly two opaque groups, a base material plus its `team_*`
variant, which is 13.6% of all triangles.

**Status.** Implemented and qualified at the same level as the existing single-group path on **both
Vulkan and DX12**: private headless runs, live submission of both groups, and same-candidate white/black
pixel controls on `fed_saber`. With Codex's tolerant parity matcher, **4 of 5 fixtures are fully
replaced**, and with the approved sliver tolerance **all 5** are; the earlier `rom_talon` refusal was 4 sub-pixel edge-on slivers;
see "Re-check on the new matcher". It is not yet run on a
physical GPU or in gameplay, and its visual parity is not accepted. The
candidate was not deployed. Codex's staged Core `13cc3d62…` is unaffected.

## Native behaviour (proven before replacement)

The static evidence comes from pinned `ArmadaL.exe` `4300ab71…`:
- The legacy body `0x631fd0` and dispatcher `0x632799` walk `node+0x104` as **inline 0x1c-byte group
  records**, with the count at `node+0x100`.
- The per-group draw `0x645cf0` reads triangles at `+0xc` and the global budget at `0x72c3f4`.
- RHW workspace vtable `0x6bcd8c` slot 6 is the flush/DIP (`0x648af0`, draw caller `0x648c01`).

The live evidence comes from the observation-only `[Refit]DiagnoseGroups=1` run of Core `712ae33f…`,
revision `groups-diag-Duel-1794007617445230499`. The fixture was `fed_saber`, whose `Mesh01` has
g0 `team_basic_shaderbump` with 24 triangles and g1 `basic_shaderbump` with 734 triangles. Across 8
frames, each two-group mesh scope produced **exactly 2 checked opaque draws, in group order**. Both had
slot 71, the checked caller, FVF `0x1c4`, no alpha test and a matching workspace. Their visible counts
were 16/24 and 318/734. Counts are the visible subset, so identification must use exact geometry
parity, never counts.

## Implementation

- `renderer/refit/mesh_scope.*`: new `select_opaque_group2`. It requires exactly two native groups and
  one binding per (node, group); every group must be observed, and both bindings must name the same
  authored node. The other gates match `select_opaque_mesh2`, which is unchanged.
- `core/refit_scene.cpp`:
  - `MeshScope` now holds shared per-mesh state (camera, lights, raster offset, view ticket) plus up to
    two `GroupPlan`s, each with its own packet, geometry, native group record and draw state.
  - Preparation is all-or-nothing: if any group fails, the whole mesh stays native.
  - At draw *n* only plan *n* is considered. It must still pass that group's material recheck and
    exact geometry parity, or the native draw is retained.
  - An extra draw after a replacement is fatal, as before.
  - Single-group scopes keep their original logic, with plan 0 for every draw.
- The renderer needs no change. `render3d/core/src/scene/mod.rs:468` already applies the team colour
  to groups whose material starts with `team` when the part carries `A2FO_REFIT_TEAM`. The native
  emissive pass stays native, as for single groups.
- `[Refit]DiagnoseGroups=1` is an observation-only aid, default 0, capped at 8 meshes. It always
  returns 0, so the native draw is kept, and it is bypassed when `TwoGroupMeshes=1`.

## Checks

- Host tests: `tests/refit_mesh_group_test.cpp` (new) and the existing `tests/refit_mesh_scope_test.cpp`
  (unchanged) pass under `g++ -std=c++20 -Wall -Wextra -Werror -D_GLIBCXX_ASSERTIONS`. Sanitizer
  runtimes are not installed on this host.
- Core candidate `build/refit/two-group-meshes-b/A2FOExtensions.dll`, SHA-256
  `523afa1c6ee063719b3a478511db1310c5953595d3e4478d21eb623afb6d9a7a`, built through
  `build_core_candidate.py`. It had zero compiler warnings, no source drift and passed the 198-export
  check.
- Live private Vulkan runs on the existing trial, each 100 s with sandbox exit 0 and Xvfb reaped.
  Hashes are in `build/refit/two-group-meshes-evidence/evidence.json`.
  - **Normal materials:** both groups submitted ("group 0" and "group 1"), with no parity, material or
    extra-draw refusal. The single-group Mediterranean was still submitted.
  - **White/black same-candidate controls:** in the saber ROI, 1,754 bright pixels under white versus
    60 under black. The 60 are the native nacelle and emissive glows, present in the normal capture too.
    Mediterranean 5,145 vs 0; empty ROI 0 vs 0. See `saber-normal-white-black.png`: the whole hull,
    base and team, turns white or black while the native glows are unchanged.
  - **Default off (`TwoGroupMeshes=0`, same candidate):** no two-group preparation or submission. All
    8 diagnostics retained native draws, and single-group submission was unchanged.

## Runner defect found (not fixed here)

`build/refit/revise_trial.py --material-control` edits the config with `^HeadlessMaterialControl=.*$`.
A spaced key such as `HeadlessMaterialControl = none` does not match, so the runner **appends** a new
line at the end of the file, which lands under `[Native11]`, and the control silently stays inactive.
The first white/black pair, `two-group-white/black-Duel-…`, is void for this reason and is preserved as
such. The workaround used was to normalise the key to `HeadlessMaterialControl=none` under a
preparation receipt. A runner fix should match `\s*=\s*` or refuse an ambiguous file.

## Trial hand-back

All fixture and config changes have preparation receipts under the trial's `revisions/`:
`groups-diag-setup-*`, `two-group-setup-*`, `control-config-normalise-*`, `two-group-off-setup-*` and
`restore-codex-fixture-*`. The trial was returned to Codex's state:
- The config and `refit_light_probe.odf` were restored byte-exactly, hash-verified against the first
  setup receipt.
- Codex's Core `13cc3d62…` was reinstalled through a normal runner pass, revision
  `restore-codex-core-Duel-1794008917368757474`.

Use that receipt as the next `--previous`.

## DX12 (2026-11-07)

DX12 used the same candidate `523afa1c…` and the `fed_saber` fixture, with 150 s observations as in
Codex's DX12 runs. The control key was normalised up front.
- **Normal:** both groups submitted. The single-group Mediterranean was submitted. No refusals.
- **White/black controls:** saber 1,846 vs 62 bright pixels (the native glows are 60 in the normal
  capture); Mediterranean 5,145 vs 0; empty 0/0. See `dx12-saber-normal-white-black.png`.
- **`TwoGroupMeshes=0`:** no two-group preparation or submission; 16 native-retained draws over 8
  diagnosed meshes; single-group submission unchanged.
- The DX12 trial was restored byte-exactly to Codex's fixture and Core `13cc…`, revision
  `restore-codex-core-Duel-1794009908997981956`.

## Wider fixtures (Vulkan, normal materials)

The probe `basename` was swapped per run, each under its own `fixture-*-setup-*` receipt.

| Model, mesh | Groups (SOD order) | Result |
|---|---|---|
| `fed_saber` `Mesh01` | team 24 / base 734 | both replaced (above) |
| `kli_mixedtech_yard_fed` `Box01` | `basic_klingon` 752 / `team_basic_federation` 56 | **both replaced**. Base-first order also works: native draws follow SOD group order, not "team first". |
| `fed_premonition` `base_fed` | team 8 / `basic_federation` 504 | g0 replaced; g1 **refused** with `projected geometry mismatch`, native retained |
| `rom_talon` `Box01` | team 8 / `basic_romulan` 404 | g0 replaced; g1 **refused** with `geometry triangle count mismatch`, native retained |
| `rom_eresis` `Box02` (LOD) | base 1218 / team 4 | g1 replaced; g0 **refused** with `projected geometry mismatch`, native retained |

A failure-only explanation build (`two-group-explain`, `751726bc…`; it only adds a capped log line)
gave stable details across all 6 frames:
- **premonition g1 / eresis g0:** expected and native visible counts equal (305/305, 609/609). There
  was no clipping, the cull orientation was right, and max dz was about 0.002. But after the
  lexicographic canonical sort, 43/305 and 56/609 triangles differ, with xy deltas up to about 137 px
  and uv deltas up to about 0.98. That is the signature of **sorted-order misalignment**: a tiny
  quantisation change alters a triangle's first corner and shifts the sorted sequence. It is not a real
  geometry or UV difference. Codex's concurrent matcher rewrite (below) targets exactly this.
- **talon g1:** expected 193 front faces, but the native code drew 189. The `--faces` analysis of the
  SOD finds no duplicate, repeated-index or zero-area faces in any fixture. The 4 missing triangles
  are therefore a runtime culling edge (near-edge-on faces the native back-face test drops). This is a
  parity tolerance question, not a two-group defect.

In every refusal the mismatching group's native draw was kept and the other group was still replaced.
That is the intended fail-safe.

## Coordination note for Codex

During this work (`2026-11-07 00:19`), Codex modified `renderer/refit/geometry_parity.cpp`. It replaced
the sorted comparison with a tolerance-bounded bipartite matcher, and added
`tests/refit_geometry_motion_test.cpp`. It kept Claude's `explain_geometry_parity2`. Claude then stopped
all Core edits and builds and restored both trials.
- Rerunning `fed_premonition` and `rom_eresis` with `TwoGroupMeshes=1` on a build containing the new
  matcher should show whether the two `projected geometry mismatch` refusals disappear.
- `rom_talon`'s count mismatch needs a decision on near-edge-on visibility.
- `explain_geometry_parity2` still describes failures with the **old** sorted comparison. If it is
  kept, update it to report the new matcher's unmatched triangles instead. **Done 2026-11-07**, see
  below.

## Re-check on the new matcher (2026-11-07, Vulkan)

Candidate `build/refit/two-group-matcher/A2FOExtensions.dll` (`ab7dd083…`) is the two-group code
plus Codex's tolerant matcher, built from current source. Before building, these all passed:
`refit_geometry_motion_test`, `refit_mesh_group_test` and `refit_mesh_scope_test`.

| Fixture | Old sorted check | New matcher |
|---|---|---|
| `fed_saber` | both groups | **both groups** (no regression) |
| `fed_premonition` | g1 refused | **both groups** |
| `rom_eresis` (LOD) | g0 refused | **both groups** |
| `kli_mixedtech_yard_fed` | both groups | not rerun |
| `rom_talon` | g1 refused (count) | g1 refused (count): unchanged |

So the two `projected geometry mismatch` refusals were the sorted-order fragility.

**`rom_talon` root cause.** `explain_geometry_parity2` now does rotation-tolerant greedy matching with
the check's own corner tolerances. It replaces the old sorted comparison, and the now-unused
`less_triangle` was removed. It was built as `two-group-matcher-explain2` (`b217a78b…`, zero
warnings) and run as revision `talon-explain-Duel-1794012676347737898`. Identical over 6 frames:
- all **189 native triangles match** expected ones (`native_unmatched=0`);
- the **4 expected-only triangles have screen areas of 3.4e-5 to 9.5e-5 px²**, against a median
  of 1.41 px².

These are edge-on slivers whose facing sign is numerically indeterminate between the engine's float
path and our double-precision projection. They cover no pixels whichever way they are culled.

**Proposal (admission change, not made; this is Codex's active file).** In `check_geometry_parity2`,
classify expected triangles with `|area| < ε` as *optional*. They may be matched or absent, but every
native triangle must still match exactly one expected triangle. Native slivers already pass through
the matcher.
- A principled ε is at or below one sample's worth of coverage. Even a conservative 1e-3 px² is about
  10× above the observed slivers and about 1,400× below the median.
- This keeps the "every native triangle is accounted for" guarantee. It only stops treating
  invisible slivers as missing geometry.
- The same rule may help motion cases where tiny slivers flip facing between frames.

Trial hand-back: the Vulkan trial was restored to the fixture as found after Codex's
`geometry-motion-vulkan` run (`restore-codex-fixture-*`). Codex's `85439152…` was reinstalled through
the runner, revision `restore-codex-core-Duel-1794012797401370856`; use it as the next `--previous`.

## Sliver tolerance implemented (2026-11-07, approved by the user)

- **Change.** `renderer/refit/geometry_parity.*` adds
  `geometry_sliver_area2 = 1e-3` px², defined as half the absolute cross product.
  `check_geometry_parity2` still runs the **strict one-to-one match first**, so every mesh it
  admitted before takes the identical path. Only after a strict failure does it set aside slivers on
  both sides: expected slivers, culled slivers, and native slivers. It then requires:
  - all non-slivers to pass the strict `match_triangles`, with corner tolerances and one common
    reciprocal-W scale;
  - every native sliver to match a distinct authored sliver, front-facing or culled, by
    position/depth/UV within the same tolerances.

  Expected slivers may be absent. Any failure returns the original strict error.
  `visible_triangles` is then the native count, which `geometry_matches` already requires.
- **Host tests:** new `tests/refit_geometry_sliver_test.cpp`, plus the existing
  `refit_geometry_motion_test`, both under `-Wall -Wextra -Werror -D_GLIBCXX_ASSERTIONS`.
  - Admitted: sliver absent natively, sliver present (strict path), and a culled sliver drawn
    natively.
  - Still rejected: a missing solid triangle, an unmatched native sliver, a duplicated native
    sliver, and a solid UV or reciprocal-W mismatch even with slivers present.
- **Candidate:** `build/refit/sliver-tolerant/A2FOExtensions.dll`, `6903cfdd…`. Zero warnings, no
  drift, 198 exports.
- **Live Vulkan:**
  - `rom_talon` now has **both groups replaced**, with no refusals.
  - `fed_saber` and `fed_premonition` are unchanged (both groups).
  - The `rom_talon` white/black control gives 712 vs 60 bright pixels in the ship ROI (60 = the
    selection marker, present in all captures); Mediterranean 5,145 vs 0; empty 0/0. See
    `talon-sliver-normal-white-black.png`.
- **Result: all 5 two-group fixtures are fully replaced.**
- Trial hand-back: fixture restored as found, and Codex's `85439152…` reinstalled. Next
  `--previous`: `restore-codex-core-Duel-1794013887897697105`.

## Remaining for this feature

- ~~Rerun the DX12 fixtures~~ **Done:** on candidate `6903cfdd…`, all 5 fixtures (`fed_saber`,
  `fed_premonition`, `rom_talon`, `rom_eresis` (LOD), `kli_mixedtech_yard_fed`) had both groups
  replaced on DX12, 150 s each, no refusals (`dx12-sliver-*` revisions). The DX12 trial was
  restored to Codex's Core `a5b4a87e…` and its fixture. Next: LOD transitions, damage
  subtrees and movement. The sliver rule may also reduce motion-related parity refusals; this is
  unmeasured.
- A team-colour parity check against native, e.g. differing team colours.
- Physical-GPU and gameplay acceptance (Codex's manual package), and checklist integration.

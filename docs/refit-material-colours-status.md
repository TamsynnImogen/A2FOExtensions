# Material colours: takeover status and merge plan

Created 2026-11-07 by Claude, continuing Codex's unfinished `[Refit]MaterialColours` thread after Codex
ran out of usage. Codex's design and evidence are in `build/refit/material-colours-candidate/` and
`build/refit-material-parity-native/`. This file records what Claude verified and changed.

## What the feature is

`[Refit]MaterialColours=1` folds each group's authored lighting material into the existing ABI2
inputs:
- ambient and light colours, using `renderer/refit/material_colours.hpp`;
- for team materials, the team colour as effective diffuse.

It covers only lighting models **0 (Constant) and 1 (Lambert)**; model 2 (Phong) stays native.
Lambert meshes are drawn natively with the 28-byte RHW layout (FVF `0x144`). The ABI2 replacement
callback admits only `0x1c4`, so Codex added the opt-in Display export
`A2FORefitDisplay_ReplaceMeshRhw_v3` (Display `d323e9c1…`) and `renderer/refit/rhw_workspace.hpp`
to decode both layouts.

**Coverage that matters**, from the census in [refit-sod-coverage-census.md](refit-sod-coverage-census.md):
of the eligible plus two-group triangles of the 230 unit models, **44% are Lambert, 55% Phong and 1%
Constant**. Constant/Lambert support is therefore worth about 45% of hull triangles. Phong is the
larger, separate piece of work.

## Live result (Codex's `run-controls-v2.py`, run by Claude)

Codex's first control failed safe with `unsupported material colour model`, because its fixture
material was Phong. v2 uses Lambert fixtures (`fixtures-lambert/`). The controller requires per-model
"material colours drawn" lines in the `on` runs and no workspace-coefficient mismatch. On DX12, `off`,
`on` and `native` all passed, and both fixture models drew through the colour path.

Colour parity, DX12, against `native` (the game's own drawing). The tool is
`tools/refit_colour_parity.py`:

| Fixture | off vs native | on vs native |
|---|---|---|
| receiver (plain Lambert), 5,045 ship px | mean abs 34.9, 1.3% within 8/255 | **mean abs 3.6, 93.2% within 8/255** |
| team (team-coloured Lambert), clean 90-px hull patch | mean abs 31.9 | **mean abs 17.0** |

The team fixture is right in hue but too dark by a mostly green additive amount: native
(81,123,57), on (68,90,50). The whole-ROI team score is polluted by the cursor arrow and selection
marker, which differ between runs.

**Native facts checked statically**, from Codex's saved disassembly:
- Material-copy `0x6481d0` writes, for team materials with team colours on: workspace ambient =
  engine constant `0x7a8f98…` (zero at runtime, since the draw-time recheck passed), and diffuse =
  specular = Storm team colour.
- Lambert `0x646bd0` seeds each vertex with scene ambient (`storm+0x28`) plus workspace ambient,
  then adds per-light terms.

That is the same formula the shader evaluates, so the team gap is **not yet explained**. The next
step is the opt-in `[Refit]DiagnoseColours=1` oracle. It reads the engine's own lit per-vertex
colour (RHW `D3DCOLOR` at +16) and logs min/max/mean next to scene ambient, team and workspace
coefficients. The darkest vertex isolates the ambient term.

## Merge into Root

Codex's snapshot was based on frozen `13cc` and does not contain the two-group or sliver work. A
three-way merge (`git merge-file`, base `odf-native-colour-guard/source`) has six conflicts in
`core/refit_scene.cpp`, all resolved:
- Colour state moved into the per-group `GroupPlan`. A base group and its `team_` group carry
  different materials.
- Colour preparation runs per group, from shared presentation and lights.
- `workspace_matches`/`geometry_matches` take the plan and RHW layout.
- The two-group diagnostic keeps its own ABI2 workspace twin.
- All config keys are preserved: `DiagnoseGroups`, `TwoGroupMeshes`, `MaterialColours`, and the new
  `DiagnoseColours`.

`host.h` merged cleanly. The merged file compiles for i686 with `-Wall -Wextra -Wpedantic -Werror`, and
Codex's `refit_material_colours_test` and `refit_rhw_workspace_test` pass. Root is updated only after
the v2 Vulkan half confirms the feature.

## Phong (model 2): native facts and the open design choice

This comes from Codex's saved `build/refit-material-parity-native/phong.txt` (`0x647060`).
- Native Phong is **per-vertex (Gouraud)**. Each vertex's lit diffuse is clamped to [0,1] per channel
  (`0x647416–0x647489`) and packed into the RHW diffuse `D3DCOLOR`.
- A separate per-vertex specular colour (`ebx+0x48`, clamped by `0x647660`) is packed into the RHW
  **specular** `D3DCOLOR`, which is the 32-byte `0x1c4` layout's field at +20. D3D adds it after
  texturing, and the highlight is interpolated across the triangle.

Two viable directions need an owner decision before implementation:
- **(A) Per-pixel modern specular.** Add per-part specular colour/power to a new ABI revision and a
  specular term to `render3d/core/shaders/sod.hlsl` (regenerate SPIR-V/DXIL). Highlights become
  crisper, but they cannot match native pixel-for-pixel, so acceptance must be visual rather than a
  parity metric.
- **(B) Native-lit vertex colours.** Pass the engine's own per-vertex lit diffuse and specular,
  already read and triangle-paired by the parity matcher, into the modern draw. This gives exact
  colour parity for Constant, Lambert and Phong alike, with modern rasterisation, MSAA and depth, but
  no per-pixel lighting gain. It needs per-vertex colour transfer in the ABI and the shader.

The current `MaterialColours=1` refuses Phong groups, so **a mesh with any Phong group stays fully
native when colours are on**. Most two-group hulls (`*_shaderbump`) are Phong. Enabling colours
therefore currently reduces two-group coverage. Keep it off in the manual package until Phong is
handled.

## Root merge done and qualified (2026-11-07)

The merge described above was applied to Root, with a backup of every replaced file in
`build/refit/root-colours-merge-backup-20261107T022332Z/`.
- Codex's ABI3 Display change was taken into Root's `third_party` after verifying Root's copies equalled
  the frozen baseline. Root `display/` now equals the source of Display `d323e9c1…`.
- Core `build/refit/colours-two-group/A2FOExtensions.dll` (`e1ee05ec…`): zero warnings, no drift, 198
  exports. All 7 host suites pass on Root: geometry sliver/motion/rhw-assignment, mesh group/scope, and
  material colours/rhw workspace.
- **Live, same Lambert fixtures, Codex's controller logic** (path-adapted copies in that directory):

| Backend | Receiver: off vs native | Receiver: on vs native | Two-group regression |
|---|---|---|---|
| DX12 | 35.25 | **2.65** (95.0% within 8) | n/a |
| Vulkan | 35.09 | **2.68** (94.9% within 8) | `fed_saber` and `rom_talon` both groups, old Display |

Two Vulkan runs failed on the madExcept load-time freeze (see the handoff), and the retries passed.

## Team-fixture residual explained (native normal length)

The `[Refit]DiagnoseColours=1` oracle (Core `fb83d0ae…`) logged the engine's per-vertex lit colours on
DX12:
- **The minimum vertex colour equals the workspace ambient** (20/5/3 = 0.08/0.02/0.01 × 255; scene
  ambient is 0).
- The receiver's maximum implies a light term of about 0.96.
- The "team" fixture's maximum implies a light term of **about 1.74 on every channel**, with green
  clamped at 255.
- There is a single white directional light, intensity 1, direction (0.58, −0.58, 0.58), and
  `team_on=0` in this Duel. The "team" fixture is just a second model at the origin.

N·L above 1 with a unit light means non-unit native vertex normals, and 1.74 ≈ √3 is exactly what an
unnormalised (1, −1, 1) cube-corner normal gives for this light. Both fixtures have unit node scales
(`refit_sod_census --nodes`), so the excess comes from the engine's vertex-normal generation on that
mesh, not from transforms. The modern shader normalises, so it is darker exactly where the engine
over-lights and then clamps per vertex.

This is a native quirk, not a colour-model error. Matching it would require reproducing the engine's
normal generation (not yet located) and per-vertex clamping. Decide whether to match native or keep
correct normalised lighting. Native per-vertex colours remain available as an oracle.

## Phong implemented as per-pixel Blinn-Phong (2026-11-07, owner chose per-pixel)

- **Renderer** (`build/refit/renderer-phong/`, `ab13779d…`):
  - `Params` gains specular colour and power (zero means none, so every engine and menu path is
    unchanged).
  - `sod.hlsl` c44 holds specular rgb and power, c45 the eye. The Blinn half-vector highlight covers
    directional, point and spot lights, front-facing only, power > 0, and is **added after texturing**
    as the engine's specular is.
  - ABI3 `Frame3` gains an optional `A2FORefitPartMaterial3 {specular[3], power}` array: none, or
    one per part. Values must be finite and non-negative, with power at most 1024.
  - Shaders were regenerated with the pinned toolchain into `build/refit/phong-shader/`. The warning
    set is unchanged from baseline.
- **Core** (`build/refit/phong/`, `83af9c8d…`):
  - `material_colours.hpp` accepts model 2. Diffuse, ambient and lights follow Lambert. Specular is
    the team colour for active team materials and the material's own specular otherwise, matching
    material-copy `0x6481d0`. Power always comes from the material.
  - The draw-time recheck also compares workspace specular `+0x70` and power `+0x7c`.
  - Phong without ABI3 stays native.
- **Tests:** `refit_material_colours_test` (now on the Root header, with 9 Phong cases) passes. The
  GPU fixture passes on Vulkan and DX12 (`build/refit-phong-checks/20261107T120621Z/`): highlight
  present; power 0 and zero specular give none; a grazing eye is weaker; 5 malformed inputs are
  rejected; every earlier ABI2 and spot check still passes.
- **Live DX12, real Phong content** (`fed_saber` two-group `*_shaderbump` plus Mediterranean,
  `MaterialColours=1`): both now replace with colours and specular, with no workspace refusals
  (revisions `phong-dx12-off/on/native-*`).
  - Mediterranean error against native: **12.5 → 7.9**, pixels within 8/255: 28% → **71%**.
  - Saber: 18.3 → 16.8; that ROI is polluted by the cursor and selection overlays.
  - Comparison image: `build/refit-phong-checks/live-dx12/mediterranean-off-on-native.png`.
  - Per-pixel will not match native Gouraud exactly by design; acceptance is visual.
- **Not staged.** The manual package runs Core `10ecb1a5` and Renderer `bdb3b522`, with colours off.

# Refit host spotlights (ABI3)

Implemented 2026-11-07 by Claude, following Codex's plan in
[refit-spotlight-next-step.md](refit-spotlight-next-step.md). The work covers ODF spot lights end to end:
`a2foLightType = "spot"` with `a2foLightConeInner/Outer`, through Core, a new Renderer ABI3 and the
SOD shader. It was qualified on private Vulkan and DX12. Physical-GPU and gameplay acceptance are
still pending, and geometry shadows remain separate, unfinished work.

## Contract

`renderer/refit/host.h`:
- `A2FORefitApi3` is the unchanged ABI2 table plus `prepare_parts3`. It is served by
  `A2FORefitRenderer_GetInterface(3)` and marked by `A2FO_REFIT_CAP_SPOTS` (1024).
- `A2FORefitFrame3` is a full ABI2 frame (with `size = sizeof(Frame3)`) plus up to
  `A2FO_REFIT_MAX_SPOTS` (4) `A2FORefitSpot3` records. Each record is 56 bytes: position, travel
  direction, colour, intensity, start, range, and **full** inner/outer cone angles in radians,
  `0 < inner <= outer < pi`.
- Colour is full inside the inner cone and fades to none at the outer cone; equal angles give a hard
  edge. Distance falloff is the same start/range as point lights.
- An invalid record gives `A2FO_REFIT_INVALID`, and more than 4 spots gives `A2FO_REFIT_LIMIT`, as
  for over-long light spans. Rejection is transactional and retires the pending packet.
- **ABI1/2 records, sizes, enums and behaviour are unchanged**, and the 52-byte light record is not
  reinterpreted.

## Implementation

- **Renderer** (`third_party/armada2-refit-native/render3d/core`):
  - `scene/lights.rs`: `Spot`, `Lights::spots`, `pack_spots`, `SOD_CONSTANT_REGS = 44`.
  - `a2fo_host.rs`: `Spot3`, `Frame3`, `Api3`, `prepare_parts3`, and spot validation. `prepare_parts`
    and `prepare_parts3` share one `prepare_frame`, so ABI2 validation is unchanged.
  - Both SOD constant packers (`scene/host_parts.rs` and the ABI1 path in `scene/mod.rs`) now fill
    44 registers. Engine/menu paths always have zero spots.
  - `shaders/sod.hlsl`: registers c28–c43 and a cone term. SPIR-V/DXIL were regenerated with
    Codex's pinned toolchain (`build/refit/shader-tools`, DXC 1.9.2609, `spirv-val`); outputs and
    previous arrays are in `build/refit/spotlight-shader/20261107T111514Z/`. The exported symbol
    set is unchanged.
- **Core:**
  - `renderer/refit/lights.*`: `append_world_lights3` routes ODF spots into spot records (degrees to
    radians) and converts everything else with the unchanged ABI2 rules.
  - `core/refit_scene.cpp`:
    - It adopts ABI3 only if `GetInterface(3)` returns the right size, version and capability, and
      shares ABI2's function pointers.
    - Spots are stored per mesh scope. `prepare_plan` uses `prepare_parts3` only when spots exist,
      so spot-free frames make the exact ABI2 call.
    - Material controls clear spots.
    - `MaterialColours` plus spots is refused (kept native), because the colour model does not yet
      fold spots.

## Evidence

| Check | Result |
|---|---|
| Renderer build `bdb3b522…` (`build/refit/renderer-spotlight/`) | builds; the 15 warnings are the same set as the unchanged Root baseline |
| Core build `10ecb1a5…` (`build/refit/spotlights/`) | 0 warnings, no drift, 198 exports |
| Host suites (9, including extended `refit_lights_test`) | pass |
| GPU fixture `tests/refit_parts_runtime_test.cpp` (`build/refit-spotlight-checks/20261107T112012Z/`) | **PASS on Vulkan and DX12**: every original ABI2 check, plus spot controls (on-axis lit; away dark; outside outer cone dark; partial fade between inner and outer; full inside inner; hard edge; zero intensity; out of range; second spot non-interfering; ABI3 table extends ABI2; unknown version null; 10 malformed inputs rejected with no packet left; ABI2 after ABI3 has no leftover spot) |
| Live, no ABI3 (old Renderer) | spots refused, every mesh kept native (`replaced 0`): fail-safe |
| Live, ABI3, red spot cone 20°/30° aimed at vs away from the Mediterranean | Vulkan: **2,101** receiver px with red Δ≥20 vs 180 in empty space. DX12: **1,973** vs 73. Green/blue unchanged. All candidate meshes replaced. See `build/refit-spotlight-checks/vk-receiver-away-toward.png` |

Live receipts are in the trials' `revisions/`: `vk-spot3-toward/away-*`, `dx12-spot3-toward/away-*`,
and `vk-spot-toward/away-*`, which are the no-ABI3 negative control. Both trials were restored
afterwards, with Codex/colour Core and the original Renderer `e9a78291…`.

**Before this change, a single ODF spot anywhere in the world refused every mesh**
(`ODF light conversion refused: unsupported world light type`). With ABI3 those meshes are replaced
and lit.

## Not done

- Folding spots into `MaterialColours`; the combination is refused.
- Physical-GPU or gameplay acceptance, and staging. The manual package still runs Renderer
  `e9a78291…`, so spots there stay fail-safe.
- Geometry shadows.

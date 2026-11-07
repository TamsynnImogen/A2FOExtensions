# Next renderer step: host spotlights

Status: **implemented and qualified on private Vulkan/DX12, 2026-11-07 (Claude). See [refit-spotlights.md](refit-spotlights.md).** Original plan below (2026-11-06). The current qualified diagnostic remains Core `13cc3d62…`, Display `3c317ae7…`, Renderer `e9a78291…` in `build/refit/odf-lighting-checkpoint/`.

## Actual rendering path

- `renderer/refit/host.h` and `third_party/armada2-refit-native/render3d/core/src/a2fo_host.rs` share a 52-byte light record. Kinds 1 and 2 mean directional and point. Both ABI1 and ABI2 use this record; do not enlarge it or reinterpret a kind they currently reject.
- API2 `draw_prepared` calls `core/src/scene/host_parts.rs::{ready,draw}`. It binds the shaders generated from **`render3d/core/shaders/sod.hlsl`**, not the separate hull module's shader.
- `host_parts::draw` packs 28 float4 pixel constants: directionals at c4–c7, their colours at c8–c11, eight nearest point positions/falloff starts at c12–c19, and colours/falloff ranges at c20–c27.
- ABI1 `core/src/scene/mod.rs::draw_part` independently builds the same constant block. A shared SOD shader expansion must update both packers and keep existing callers supplying valid empty spotlight data.
- Generated shader modules are `core/src/shaders/sod_spv.rs` and `sod_dxil.rs`. `tools/shaderc/hlsl-rs.sh` generates SPIR-V, signed DXIL and the alpha-tested DXIL variant. Regenerate these from HLSL; do not hand-edit byte arrays.
- `scene/clusters.rs::add_spot` is a beam-wash effect path. Its cone-shaped GPU data is not generic host spotlight support and does not qualify the SOD path.

## Implementation and acceptance order

1. Specify a separately versioned host light/frame/API extension. Keep ABI1/2 layouts, enum rejection, resource ownership and export behavior unchanged. Define directional, point and spot limits, copied packet lifetime, and transactional rejection of invalid angles, directions and non-finite values before implementation.
2. Carry position, normalized ray-travel direction, colour/intensity, distance falloff and inner/outer **full** cone angles through the new packet. Convert full angles to half-angle cosine cutoffs consistently. Define a hard edge when the two angles are equal; avoid division by zero.
3. Add the cone term to the actual SOD shader and update both constant-block builders. Retain ambient, other sources and emissive contributions outside the spot. Rebuild and qualify both shader backends and the existing API1/2 tests.
4. Add meaningful synthetic controls: inside/outside cone, inner/outer transition, opposite direction, zero/unsaturated intensity, distance falloff, malformed input, immutable ownership and reset. Run Vulkan and DX12 independently.
5. Connect the new host capability to the fresh Refit ODF path, preserving preparation/DIP freshness checks and native fallback when unsupported. `LightConfig` already parses spot angles; `append_world_lights2` deliberately refuses spots today. Repeat private live controls on the exact new candidate.

This adds cone illumination only. Geometry shadows still require a complete current caster frame, offscreen blockers, a qualified frame boundary and per-source shadow data. The current per-mesh API2 cannot establish that from a previous frame's snapshot.

## Toolchain already staged

All tools are under `build/refit/shader-tools/`; no system package was installed.

- Pinned DXC: `tools/dxc-1.9.2609/bin/dxc`, downloaded through the upstream version/SHA-256-checked fetcher. Reported compiler version is 1.9.2609.5.
- SPIR-V validator: `spirv-prefix/usr/bin/spirv-val`, with project-owned libraries in `spirv-prefix/usr/lib64`. Fedora `spirv-tools` and `spirv-tools-libs` 2026.1-1.fc44 RPM signatures and digests verified. Extraction receipt: `spirv-prefix-receipt.json`; only selected regular binaries/libraries/licenses were extracted.
- `baseline-sod-check/receipt.json` records a successful compile of the **unchanged** SOD HLSL into owned scratch `sod_spv.rs` and `sod_dxil.rs`, including SPIR-V validation and DXIL variants. Vendor HLSL/generated files were unchanged. This is compiler proof, not new renderer runtime evidence.
- `baseline-sod-check/committed-output-comparison.json` confirms both generated Rust shader arrays are **byte-identical** to the current committed SPIR-V and DXIL arrays. No renderer artifact was rebuilt or replaced for this check.

The baseline receipt records the exact command. For a new shader build, provide the owned DXC and validator directories in the child process PATH, its library directories in the child LD_LIBRARY_PATH, and the pinned DXC path in `DXC`. Keep scratch compilation separate until the source/ABI design is reviewed.

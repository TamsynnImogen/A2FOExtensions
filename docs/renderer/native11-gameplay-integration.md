# Gameplay integration candidate

Status: **private frontend candidate; not ready for full gameplay testing**.
The installed game and its renderer settings remain unchanged. This work does
not close stages 53–65 in [the progress checklist](native11-progress.md).

## Verified components

- [x] Decode validated raw BGRA/RGBA and BC1/2/3 texture data into owned HUD pixels.
- [x] Assemble clipped, ordered sprite draws with atlas UVs, tint, transparency,
  additive blending, texture replacement and stable resource epochs.
- [x] Draw owned SOD world geometry and sprite overlays on the **same API8 session
  and D3D11 target**, then present the completed fixture frame.
- [x] Reject incomplete domain declarations, mismatched frame identities,
  stale resource owners and revoked sources before publication.
- [x] Check source freshness around world shadow configuration, world submission,
  overlay submission, presentation, asset callbacks and readback.
- [x] Preserve submitted resource ownership across occlusion or later failure;
  explicit startup cleanup and close retry handle initialization failures.
- [x] Run the five new CPU/session/GPU cases and five affected world cases under
  private Wine/Xvfb with D3D8, D3D9 and D3DX disabled: **10/10 passed**.

The fixture uses a real SOD model parser, selected world texture, Rust TGA/DDS
parser, D3D11 uploads and pixel readback. Its hidden window is never shown or
activated. The fixture's explicit absence declarations describe only its small
scene; they provide no evidence that a game domain is absent.

## Remaining before a full gameplay candidate

- [ ] Authoritative fresh offscreen instance poses, effective materials,
  visibility and LOD. Shared engine node state from the last rendered instance
  cannot supply this.
- [ ] Actual world feature, background and terrain producers with draw accounting.
- [ ] Actual HUD/text/minimap and transient effect producers. `SpriteFrameStream`
  receives owned CPU sprites; the engine producer is **not connected yet**.
- [ ] Video/menu scene production and transition ordering.
- [ ] Complete game-frame source inventory, frame boundary and presentation
  ownership, including fallback after device loss and partial frame failure.
- [ ] Complete live resource lifetime handling, independent game startup,
  representative parity/performance checks and manual game acceptance.
- [ ] Tested exact-target deployment and rollback package.

The reviewed sprite boundary in the pinned Roots ArmadaL image is the CPU
rectangle/UV dispatch at RVA `0x23b486` in `ST3D_Sprite::ClipAndRender2D`.
Its arguments precede backend NPOT texture-coordinate adjustment. No new hook is
installed there by this candidate; material blending, sampling, alpha policy,
font/minimap coverage and alternate draw routes still need verified adapters.

`GameFeature_RenderAll` at RVA `0x6e4b0` reads the registry pointer at RVA
`0x336c84`, tests feature flag `0x4` at offset `0x20`, then calls virtual slot
`0x20`. This is distinct from the AddObject/DeleteObject event dispatchers.
Registry discovery alone cannot certify rendered feature coverage.

These addresses were checked against SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.

## Reproduce the checks

From the repository root:

```sh
make -j2 BUILD_DIR=build/native11 \
  build/native11/native11_texture_bgra_native.exe \
  build/native11/native11_sprite_frame_stream_native.exe \
  build/native11/native11_sprite_stream_gpu.exe \
  build/native11/native11_gameplay_session_native.exe \
  build/native11/native11_gameplay_gpu.exe \
  build/native11/native11_world_session_native.exe \
  build/native11/native11_world_pipeline_native.exe \
  build/native11/native11_observed_world_native.exe
python3 build/native11/run_private_native11.py \
  --case texture-bgra --case sprite-frame-stream --case sprite-stream-gpu \
  --case gameplay-session --case gameplay-gpu --case world-session \
  --case observed-world --case world-pipeline --case world-pipeline-shared \
  --case world-candidate-provider --results gameplay-integration-results.json
```

The private runner requires the existing pinned helper and provider dependencies.
Results are recorded in `build/native11/gameplay-integration-results.json` with
executable and helper hashes. The prior frozen stage-52 package remains the last
full-suite checkpoint. This candidate does not claim a fresh full-suite run,
game startup, modified executables, deployed binaries or in-game acceptance.

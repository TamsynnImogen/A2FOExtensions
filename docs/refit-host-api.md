# Refit native scene host API (v1 and v2)

This documents the C ABI in [`renderer/refit/host.h`](../renderer/refit/host.h) and the boundaries exercised by the headless fixtures. ABI v2 and its scoped-parts consumer are implemented in the current workspace candidate; observer and asset-byte evidence is still separate from live scene rendering and FO scene acceptance. Refit's in-game UI is out of scope: preserve the FO HUD and existing display ownership.

## ABI and ownership contract

- `A2FORefitGetInterface(version)` returns the versioned `A2FORefitApi`; v1 uses `cdecl` and natural x86 struct layout. Display facade COM entry points retain `stdcall`.
- Caller spans must be valid, non-overlapping, naturally aligned for the call. Model, texture and frame inputs are copied; caller pointers do not survive a call. Use one owner thread and do not reenter.
- The host owns Display and its HWND. Keep both helper DLLs loaded for the process lifetime once this ABI initializes.
- Reject invalid parameters, missing devices and wrong-thread calls before GPU attachment. Those validation failures may be corrected. GPU attachment is one-shot: after an attachment attempt, including an `initialize` result of `A2FO_REFIT_DEVICE`, restart the process before trying again. On failure, keep the existing renderer authoritative.
- Validation rejection leaves the previous frame usable. A `DEVICE` failure can have recorded part of a frame and invalidates readback until a later successful frame.
- `reset_content(new_source_epoch)` establishes a new content epoch. Frame/readback records carry source epoch and serial; fixture evidence covers rejection of stale revisions/epochs. Do not reuse stale content as current.
- `A2FORefitDisplay_Configure_v1` selects backend 1 (Vulkan), 2 (D3D12), or 3 (auto), with the supplied same-process HWND on the owner thread. Display does not change HWND visibility/focus. `Prepare_v1` can configure before the game HWND exists; the first valid owner-thread CreateDevice binds it.
- `A2FORefitDisplay_Passthrough_v1(enabled)` is optional and off by default. It lets the host explicitly select whether existing FO D3D8 fixed-function submissions pass through; it does not load Refit UI or admit stock/GOG hooks.

The header defines status values `OK`, `INVALID`, `STATE`, `DEVICE`, `CONTENT`, `LIMIT`, and `STALE`; capabilities for SOD, poses, textures, lights, readback and post; HDR/MSAA4/bloom flags; TEAM/UNLIT object flags; and directional/point light kinds. Matrices and spans follow the exact field definitions in the header; callers must use that header as the ABI authority rather than duplicate layouts from prose.

## ABI v2: scoped opaque parts

`A2FORefitGetInterface(2)` returns `A2FORefitApi2`. ABI v2 keeps ABI v1 unchanged: the first eight function pointers have the v1 signatures; v2 appends `prepare_parts`, `draw_prepared`, `upload_encoded_texture`, and `upload_model_transformed`. Check the exact struct sizes/capability bits in `host.h` at compile time rather than duplicating them here.

- `A2FO_REFIT_FRAME_DIRECT` is required. Direct mode loads the current engine color and depth unchanged, draws the supplied SOD node/group parts, and retains existing sky/effect/HUD ordering. It performs no post-processing, color clear, or HUD depth clear.
- `A2FORefitPart2` identifies model/revision, node/group, transform, material values, explicit texture name/revision and alpha. The current direct path accepts opaque or cutout groups only when alpha is exactly 1. Unsupported groups are refused by preflight and retain their original draw.
- `upload_encoded_texture` accepts original DDS/TGA bytes with flags 0 for this ABI revision; dimensions/decoded data are validated before GPU allocation. `upload_model_transformed` applies a synchronous callback to copied mutable XYZ triples before normal reconstruction. No callback/context pointer survives upload; it must not reenter the API, inspect mutable engine state, or unwind across the C ABI.
- `prepare_parts(frame)` validates and copies the prepared frame without issuing a draw. `draw_prepared(epoch,serial)` consumes exactly that prepared pair once on the owner thread. This is renderer capability only: the host separately owns suppression of the original draw.
- Preflight rejection preserves the original draw. A `DEVICE` failure during `draw_prepared` may already have written some color/depth, has no GPU rollback, and is not atomic. The caller must retire the attempted frame or explicitly fall back; it must not claim all-or-nothing replacement.
- `A2FORefitDisplay_ReplaceOpaque_v2` installs an owner-thread callback for eligible back-buffer draws before acquiring a native backend command/context. The original draw is suppressed only if the replacement reports success; the host restores the scoped callback at the original instance-render return. It does not infer stock call-stack categories.

Current Core consumer: `core/refit_scene.cpp` is in the newer staged Core candidate `build/refit/fleet-workspace-candidate/A2FOExtensions.dll` (SHA-256 `c1d5ba03a29ede2a7d98f1d28a382a1193d366139f4c22829aec3c94bfe7ff2e`); the full Core build and 198-export contract passed. `[Refit]LiveParts=1` opts into a scoped replacement of eligible opaque FVF instance parts. Startup arms CPU model/texture/node/material observers before workspace publication. The generated profile pins FleetOpsHook SHA-256 `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`; its constructor wrapper at RVA `0x1cedec` retains original native forwarding and team setup. `build/refit-fleet-constructor-checks/20261106T102918Z-2803027-1793960958085065673/receipt.json` and `build/refit-workspace-checks/result.txt` pass ABI/guard, exact-entry, callback-isolation and stack-balance checks. Unsupported instances retain their original draw. A replacement `DEVICE` failure can leave a partial/mixed frame and blocks further replacement for the process attempt. These source/build/observer checks do not prove modern scene API world pixels, live FO lighting/shadows, or gameplay.

### ABI v2 fixture evidence

`build/refit-parts-checks/20261106T095022Z/2771905/receipt.json` reports pass on Vulkan and D3D12 under private Wine 11.13 with Lavapipe. It binds the fixture to Display SHA-256 `9343f4e912a8468c9c1f3f1bd1bc19e532b59f00a0979963fa11f7473f3ef636` and Renderer SHA-256 `cc4babca8a90d23e6571c2b27dbb3b3a574e0c626e41be90fb78ca5dd6137f51`. Both backend logs report ABI2 direct parts, encoded textures, transforms, copies, depth, fixed-function color and rejection checks passing. `build/refit-packet-checks/result.txt` records `exit 0; all assertions passed`; the private Wine Native11 material-bridge check passed as well. These are fixture and packet proofs, not an FO world frame, live lighting/shadow test, FO HUD-over-scene test, or gameplay acceptance.

The updated invalid-call diagnostic Display also passed the same ABI2 fixture on Vulkan and D3D12 in `build/refit-parts-checks/20261106T102701Z/2800722/receipt.json`. Its Display SHA-256 is `2130684bfd1142814e8c2ae74f974f711a234cae9418f1fddc091e91a7f4bbef`; Renderer remains `cc4babca8a90d23e6571c2b27dbb3b3a574e0c626e41be90fb78ca5dd6137f51`.

## Live CPU source observation (Vulkan diagnostic)

The private Vulkan workspace/map diagnostic receipt `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/fleet-map-Duel-1793961046435223671/receipt.json` binds Core SHA-256 `c1d5ba03a29ede2a7d98f1d28a382a1193d366139f4c22829aec3c94bfe7ff2e` and Display SHA-256 `2130684bfd1142814e8c2ae74f974f711a234cae9418f1fddc091e91a7f4bbef`. It reports an isolated private Wine run with Armada alive through 20 samples/40 seconds. The current log reports model/texture lifetime and destruction observers, owned DDS/TGA/model stream reads, per-instance material versions and post-animation node hierarchy observers armed. It records the first model asset read copied into owned bytes (25,610 bytes) with scalar metadata only.

This is CPU-side data/lifetime evidence only. It does not prove parsed or submitted live model geometry, current texture decode/upload into a world draw, scene pixels, lighting, shadows, or FO HUD composition over a world frame. An earlier workspace-context mismatch/refusal remains in prior trial logs. A newer Direct3D Duel repeat first reported a Direct3D error; a retry with the updated diagnostic Display is ongoing and has no result claim yet. The Vulkan CPU-observer run does not qualify D3D12 rendering or gameplay.

## Current implemented/fixture boundary

The frozen checkpoint at `build/refit/hud-post-baseline/checkpoint.json` records two DLLs and a 1,692-file source snapshot, built from upstream commit `1e2a7d2fc213282ef7f9ef00e1b11011da23ac68`:

| DLL | SHA-256 |
| --- | --- |
| Display | `6f500bd979666752015a862338d67db4cdda6890e63790394a6ee5012f933dfd` |
| Renderer | `a611319afba060c79d04e3e33ec3644c1768a8ae10f3ee2cc6f78363febe033e` |

The `tests/refit_host_runtime_test.cpp` fixture and receipts in `build/refit/hud-post-baseline/runtime/` exercised Vulkan and DX12 under private Wine 11.13, Xvfb and software Lavapipe. Modes 0 and 7 passed. The fixture covers owned SOD/BGRA texture copies, camera/node poses, directional/point-light controls, stale/revision/epoch rejection, one-sided/mirrored/hidden geometry, native presentation/readback, and a synthetic FO-style HUD composite. A DX12 readback-after-present issue was fixed in `third_party/armada2-refit-native/gpu/src/dx12/cmd.rs`. Mode 7 reported `Rgba16F` format 113, four samples and flags 7 without fallback on either backend. The bloom pass ran, but visible halo quality is unqualified. The synthetic HUD corner was `(0,128,0)` and model pixel `(0,0,127)`.

The imported upstream `README.md` still describes its DX12 fixed-function path as incomplete. That statement is stale for this candidate: the candidate implements the DX12 fixed-function passthrough, and the DX12 post-present readback issue was fixed in `third_party/armada2-refit-native/gpu/src/dx12/cmd.rs`. The checkpoint's runtime receipts are the evidence for that limited correction; this does not claim a live FO presentation path.

These are private-Wine software-GPU fixture results, not native Windows, physical-GPU, live FO, or gameplay results. No Refit UI, D3D8 or D3D9 runtime was loaded by these fixtures; this does not yet prove the in-game FO display/factory integration.

## Exact-image profile checker and startup status

`tools/check_refit_fo_profile.py` is an offline PE verifier. It permits only the two exact FO image SHA-256 values listed in the tool, then checks PE identity, the factory IAT, the complete 56-byte mode finder and six calls to it. The result is deliberately limited to those factory/mode symbols; other Refit GOG addresses are not admitted. It does not execute or patch the executable and does not prove runtime startup.

Example invocation for both explicitly supplied images:

```sh
python3 tools/check_refit_fo_profile.py \
  --image "/path/to/FO/ArmadaL.exe" \
  --image "/path/to/FO4/ArmadaL.exe" \
  --output build/refit/fo-profile-offline.json \
  --write-header renderer/refit/fo_profile.hpp
```

For a non-mutating generated-header consistency check, use `--check-header renderer/refit/fo_profile.hpp` instead of `--write-header`. The checkpoint's `fo-profile-offline.json` is the recorded offline result; the tool's image inputs are the two pinned paths/hashes recorded there. Do not substitute other executable files or infer profile coverage from a matching version label.

`renderer/refit/startup.cpp` is wired from process-attach operation 11 in `core/dllmain.cpp` and listed in `Makefile` `CORE_SOURCES`. The Core build passed with `make -j4 build/A2FOExtensions.dll`; the success log is `build/refit/core-factory-build.log`. An initial locked media-cache fetch failure is retained at `build/refit/core-factory-build-initial-failure.log`. `build/rust-dll/receipt.json` records exit 0 and no changed inputs. `build/refit/core-export-check.json` passes with 198 exact exports against `core/A2FOExtensions.def` and 11 imports; the startup object is linked. This verifies source wiring, Core compilation/linking and the export contract only.

The current frozen checkpoint is `build/refit/factory-checkpoint/checkpoint.json`: 2,369 integrity-verified files, with hashes for Core `A2FOExtensions.dll` (`8a88889ae4eb039ff67b092b006c684e5257c266c47893736261ebb06f258107`), Media `A2FOMedia.dll` (`a9dddb904a067ba25e90c2f09d3682c31eb2d64b8f92e61584a32b4b8b852f19`), Display (`6c09e505ea512fb763f1fe2ba08cdcd7210ba0aa266688820a16443e34b7b804`) and Renderer (`d441ce6653ee392921bc1798ae0f48333dfb9a1163002bebd39cdfa026a34886`). Its seven protected live-install hashes are unchanged. This checkpoint is distinct from the still-frozen 1,692-file `hud-post-baseline`; it includes prior class-completion WIP, whose broader 164-suite was not rerun. Do not imply that suite is qualified.

The host integration remains limited to the explicitly named `refit-vulkan-test` and `refit-d3d12-test` child backends. It explicitly refuses both `/d3d9` and `-d3d9` in this D3D8 factory route and retains the legacy renderer; a D3D9 bridge is not implemented.

## Private FO startup/device diagnostic

Actual Fleet Operations startup diagnostics have now run on both backends using the immutable artifacts from `factory-checkpoint`, in separate copied install trees under system Wine 11.13 and with independently copied initialized private GPU-fixture prefixes. The integrity-checked evidence bundle is `build/refit/fo-startup-evidence/receipt.json` (66 files), with per-backend receipts, fresh Core and Display logs, main-menu captures, runner, and failed-attempt/metadata-repair records. Each backend receipt reports `launch-diagnostic-complete`, an isolated run, 20 process samples over 40 seconds, Armada alive at the sample limit, and unchanged protected live hashes/source private-copy configuration/provider.

- Vulkan trial: `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/receipt.json`. The Core log records the factory armed and returned; the fresh `A2FORefitDisplay.log` reports llvmpipe (LLVM 22.1.8), Vulkan 1.4.354, swap chain 1280x1004, target 1280x1024 and D16 depth. FO reached the stock main menu.
- DX12 trial: `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364/receipt.json`. The Core log records the factory armed and returned; the fresh `A2FORefitDisplay.log` reports feature level 11_0, swap chain/target 1280x1024 and D16 depth, with `frame 1 presented`. Pipeline library error `0x887a0004` was nonfatal; plain pipelines were used. FO reached the stock main menu.

Both runs used a hash-checked copy of an already initialized private GPU-fixture prefix. Earlier fresh-prefix kernel32/bootstrap and timeout failures remain recorded; these results do not show that fresh-prefix initialization issue resolved. The child AssetVersion metadata correction and initial warning are documented in the evidence bundle. An older `A2FORefitMenus` module was rejected in these copies; Refit in-game UI was not loaded, and separate animated 3D menu backgrounds remain unverified. The runner is `tools/run_refit_fo_bootstrap.py`; `--seed-prefix` accepts the recorded initialized fixture prefix and `--retry-trial` archives/stops an owned failed trial before retry.

Use only the fresh Core and Display logs for the earlier startup claims. Each trial's `A2FORefitRenderer.log` under `prefix/drive_c/users/tamsynn/AppData/Local/armada2-refit/logs/` is an unchanged copied seed log (including a stale Vulkan line in the DX12 trial); it does not establish current Render3D module activity. Despite device initialization and the DX12 display presenting frame 1, no live FO 3D world frame was captured or qualified. The newer CPU observers and model-byte capture do not establish scene API world pixels. These diagnostics establish menu/device startup only, not the FO HUD over a 3D scene, gameplay, native Windows, physical GPU, deployment, or acceptance.

The CPU startup policy receipt `build/refit-startup-checks/20261106T023157Z-2671771-1793932317035882701/receipt.json` and transaction receipt `build/refit-transaction-checks/20261106T023432Z-2672659-1793932472781163285/receipt.json` passed their recorded private Wine/Xvfb checks. The transaction fixture exercised six mode-call patches over three RX pages, fault rollback, owned PIC finder, real thread states and a bounded loader-phase probe. The probe starts a fresh thread, signals with `SetEvent` and waits up to 100 ms; if it cannot confirm the intended phase, the original factory remains in place before hashing or loading helpers. Non-owner factory calls in states 1 and 2 returned `NULL` without state mutation or competing legacy-factory changes.

`core/nebula_renderer.cpp` reserves the native graphics route before attempting the native backend, suspending legacy DXVK-only material/draw hooks. If later base FO startup falls back, rendering uses base FO; restarting with the normal backend restores A2FO effects. This explicit behavior may change visuals during fallback and has not been qualified for equivalence.

Updated deferred-factory DLLs were exercised with both backend fixture selections using `Prepare` and flags 7 in `build/refit-host-checks/20261106T022527Z/2666481/receipt.json`, and default `Configure` with flags 0 in `build/refit-host-checks/20261106T023433Z/2672721/receipt.json`. Their hashes are Display `6c09e505ea512fb763f1fe2ba08cdcd7210ba0aa266688820a16443e34b7b804` and Renderer `d441ce6653ee392921bc1798ae0f48333dfb9a1163002bebd39cdfa026a34886`. These are mutable deferred-factory fixture DLLs, distinct from the frozen `hud-post-baseline` artifacts, and do not prove actual FO startup, first frame, deployment or gameplay. Do not claim full-profile admission or a gameplay-ready renderer.

## Lifecycle gaps before integration

- [x] Build and export contract passed for the wired Core source; see the build and export receipts above. This does not close live startup or renderer integration gates.
- [ ] Verify `/d3d9` and `-d3d9` refusal and untouched legacy renderer in actual FO startup; D3D9 rendering remains unsupported by this bridge.
- [x] Private trial receipts/logs confirm FO process admission, exact profile and module hashes, selected test backend, factory return, device initialization and stock main-menu arrival on both backends.
- [ ] Capture and qualify live FO geometry/materials submitted through the scoped host scene API on each backend, with world pixels. Do not use DX12 display `frame 1 presented` or CPU observer/model-byte evidence as proof of the first scene frame.
- [ ] Preserve safe fallback to the existing renderer on unsupported image, rejected initialization, or device failure; prove this in the real FO startup path.
- [ ] Keep process-lifetime pinning and the one-attachment-attempt rule explicit in the owning loader. No live unload or retry is supported by this ABI contract.
- [ ] Add/qualify spotlight and shadow support separately; current host API/light fixture coverage does not establish them.
- [ ] Obtain native Windows/hardware and actual FO runtime evidence before deployment or gameplay claims.

## Reproduction pointers

The successful runtime commands, compile arguments, tool versions, environment, logs, fixture inputs and artifact hashes are recorded in:

- `build/refit/hud-post-baseline/runtime/ldr/receipt.json`
- `build/refit/hud-post-baseline/runtime/hdr-msaa-bloom/receipt.json`
- `build/refit/hud-post-baseline/checkpoint.json`

Both receipts record the original `i686-w64-mingw32-g++` compile command and per-backend `wine` fixture invocation. Inspect those receipts to replay the exact recorded artifact set; do not treat merely rerunning an arbitrary build as reproducing the frozen checkpoint. The checkpoint reports twelve builder/profile Python tests passed. This documentation refresh did not rerun them.

# Refit native renderer: current integration status

Scope: integrate the native DX12/Vulkan 3D renderer from `third_party/armada2-refit-native/` into A2FO. **Refit's in-game UI is excluded.** Keep the existing Fleet Operations HUD and its rendering path. This checklist records the current headless candidate and the remaining gates; a backend fixture pass is not Fleet Ops gameplay acceptance.

## Current status snapshot

- **Completed evidence:** mesh-scoped ABI/source and the first two-backend private scene submission are frozen in `build/refit/first-live-owned-scene-checkpoint/checkpoint.json` (2,708 files, 64,363,196 bytes). The checkpoint binds Core `90440273…a1f6ddcd`, Display `3c317ae7…eaf865d3`, and Renderer `ef1dd423…c1b0fac28`; Core source attestation covers 921 files and the export receipt passes 198 exports. Seven protected live hashes are unchanged; no Refit UI was loaded and nothing was deployed.
- **Current live diagnostic:** both private 100-second Duel runs (Vulkan and DX12) log a first owned single-group submission through the native scene API. Same-candidate white/black captures show modern pixel contribution in both backends while retaining the original FO HUD; ROI controls report 5,053 bright pixels for white and 0 for black. This qualifies controlled contribution for Core `c20cfef1…3464b79`, Display `3c317ae7…eaf865d3`, Renderer `2726ffee…e0adb87b`, not full-image parity or gameplay.
- **Material/raster status:** canonical native material identity and parameters are profile-checked; `Storm`/`a8` override is rejected and texture slot 0 is re-read at draw. Camera pixel centers were corrected through the Raster2 query, and UV/global reciprocal-W scaling matches the guarded native path. The modern shader still does not consume all diffuse/specular/power/model inputs, so color parity remains open. Refit scopes read registered Storm lights. The opt-in directional/point ODF bridge samples fresh per-view sources and rechecks them at draw; four private software-GPU static colour controls now show intensity-dependent receiver pixels. Spot, movement, range, material response and shadows remain open. End-of-frame ODF snapshots may be stale and are not a valid current-light source.
- **Current candidate:** Renderer `e9a78291…a3dbbfca` passes the same strict Vulkan/DX12 fixture, including white/black and reset controls, and both normal-material FO runs survive 100 seconds with captures. The earlier `2726ffe…` Renderer has the same-candidate live ROI controls; the newer Renderer changes fallback allocation and validation ordering. Keep these evidence scopes distinct. Full scene/material coverage, lighting/shadows and gameplay acceptance are the remaining implementation and qualification work.
- **Remaining acceptance:** the scoped submission and same-candidate controlled pixel contribution are proven. Replacement-material parity, full content coverage, HUD composition across the complete scene, ODF lighting/shadows, effects, physical-GPU validation, performance, multiplayer, deployment, and manual gameplay remain open. Refit in-game UI remains excluded; keep the FO HUD.

Final diagnostic bundle: `build/refit/native-scene-pixel-checkpoint/` contains 2,787 recorded files before its README, all 1,660 files in the final vendor source audit, the 921 attested Core sources, exact candidate DLLs, and backend evidence. The earlier first-scene snapshot omitted four local vendor files; its `source-coverage-notice.json` records that limitation. The final bundle includes them.

**Movement-flicker investigation (2026-11-07):** exact triangle canonicalization can falsely reject a mesh after tiny projection changes and switch back to FO shading. The strict motion regression reproduces this on the13cc checker. An independently reviewed tolerant cyclic, one-to-one matcher passes it and the original parity suite, preserving numeric tolerances, winding and bounded fail-closed checks. Isolated Core `85439152…6c6049c` is bound to the933 frozen baseline sources with only this checker changed; unrelated newer Root work is excluded. Both actual-DLL100-second private scene checks passed (native mesh submission, capture and owned cleanup). The candidate is staged only in the manual copy;55 inputs and the7 unchanged live hashes pass. Receipts/backups are tracked in `build/refit/geometry-motion-candidate/`. The report's visual cause and fix are still unconfirmed; separate triangle-count and material-parity limitations remain.

**User retest and follow-up hardening:** the user reports moving-ship brightness is “much better”; the latest DX12 session records150.3s, mesh submission and natural exit/cleanup0. Core854 remains the manual test baseline, with visual acceptance still partial. Future sessions now pin exact binary/config hashes before and after;18 independently rerun safety/identity tests and read-only55-input integration pass. A separate unstaged Coree280 corrects order-dependent shared-reciprocal-W assignment rejection (972 positive permutations plus negative controls); it has source/export proof and both100-second private backend qualifications tracked under `build/refit/geometry-rhw-candidate/`. Neither this report nor the guard fix closes full material, lighting/shadow, gameplay or Vulkan desktop gates.

## Requested completion checklist at a glance

The **first user-operated DX12 replacement session succeeded** in the isolated package at `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test`; see [the manual guide](refit-manual-test.md). Session `20261107T000335Z-7a4bfbab-dx12-replacement` records native scene mesh submission and natural exit0. The user reported brightness/colour flicker while moving a ship, so visual acceptance remains open. Package integrity covers55 inputs; nine launcher safety regressions and the earlier two150-second owned-stop checks pass. Vulkan desktop acceptance, broader gameplay and stability remain open. The original live installation is unchanged.

Fresh ODF lighting work remains partial. Core `13cc3d62…8be2cf0c` has a 933-source unchanged snapshot at `build/refit/odf-native-colour-guard/current-core-source-check.json`; strict parser/config, copied-event and class-store tests pass at `build/native11-prefixed-light-checks/20261106T165039Z/receipt.json`. Four private Wine/software-GPU static receiver pairs changed only ODF intensity 3 versus 0 within each pair, with the same Core/helpers/settings: directional Vulkan and DX12, point Vulkan and DX12. Their analysed receiver ROIs have 4,828, 4,905, 1,976 and 1,975 pixels respectively with red increase at least 20. Directional results are summarized at `build/refit-odf-directional-control-results.json`; individual analyses are under `build/refit/light-controls-vk-dir-1793984111079143360/`, `light-controls-dx12-dir-1793984389096129977/`, `light-controls-vk-point-1793984626764814072/` and `light-controls-dx12-point-long-1793985094097082647/`. All four have independent receipt/log/capture reviews, including DX12 point at `build/refit/light-controls-dx12-point-long-1793985094097082647/independent-review.md`. The first Vulkan directional manager marked a valid zero-source control failed; independent analysis corrected that assertion while preserving the failed summary. An earlier DX12 point 100-second attempt remained in loading and is preserved as incomplete acquisition, not a renderer failure; the accepted retry observed 150 seconds per side. This establishes narrow static colour response, not lighting/shadow/gameplay acceptance.

| Work item | Current position |
| --- | --- |
| Explicit renderer host API | ABI2 opaque parts work; complete world/lighting contract remains open. |
| Fleet Ops profiles and scene boundaries | Two pinned profiles and checked mesh scopes work; complete scene ownership remains open. |
| Native Vulkan | GPU fixtures, scoped FO submissions, controlled pixels and normal-material captures pass under private Wine/software GPU. Physical gameplay acceptance remains open. |
| Native DX12 | The same separate fixture and private FO evidence passes. Physical gameplay acceptance remains open. |
| Presentation, resizing, lifetime and recovery | Window handoff/reset fixtures pass; device removal and full gameplay mode coverage remain open. |
| Cameras, objects, visibility and transforms | Authored geometry matches the live native projected mesh on the supported route, including UVs, winding and reciprocal W. Wider coverage remains open. |
| Assets, materials, animation and LOD | Owned SOD, DDS/TGA and authoritative posed parts work within the admitted scope. Opt-in `[Refit]TwoGroupMeshes=1` adds base + team two-group meshes; all 5 fixtures (including LOD `rom_eresis`) are fully replaced on private Vulkan and DX12, with white/black controls ([refit-two-group-meshes.md](refit-two-group-meshes.md)). Complete material and content coverage remains open. |
| Geometry, weapons, particles and effects | Unsupported draws retain the original FO submission through native Display. Dedicated modern scene coverage remains open. |
| HUD, minimap, text and 3D menus | Original FO HUD appears in private captures. Refit UI stays excluded. Complete Nexus overlays and animated menu acceptance remain open. |
| Directional, point and spot ODF lights | Fresh namespaced directional/point bridge implemented with paired live controls (Codex). **Spotlights implemented via Renderer ABI3 (Claude, 2026-11-07)**: GPU fixture controls pass on Vulkan and DX12, and live ODF spot aimed/away controls pass on both ([refit-spotlights.md](refit-spotlights.md)). Physical-GPU/gameplay acceptance and staging remain open. |
| Geometry shadows | Complete current caster inputs and the Refit shadow contract remain open. |
| Performance, memory, stability and multiplayer | Bounded fixtures and 100-second scene observations pass; gameplay-scale and multiplayer qualification remain open. |
| Recoverable candidate package | Source/artifact/evidence diagnostic bundle is prepared without live deployment. Full gameplay release candidate remains open. |
| Independent gameplay and visual acceptance | Required separately on both backends; remains open. |

The newer ODF colour-light diagnostic is frozen in `build/refit/odf-lighting-checkpoint/manifest.json` (SHA-256 `c616b7e4819d94ecaf293abe98cc0e7152fa032c2e53aa9b004934780f85f3c5`). It preserves 1,179 copied files plus README and manifest, including all 933 attested Core source files, exact artifacts, four independently reviewed controls and the nonqualifying loading attempt. The full unchanged vendor baseline is an external hash-pinned reference. All seven protected live-install hashes remain unchanged; no gameplay candidate was deployed.

## Verified headless candidate

Checkpoint: `build/refit/hud-post-baseline/checkpoint.json`, created 2026-11-06, upstream commit `1e2a7d2fc213282ef7f9ef00e1b11011da23ac68`. The frozen checkpoint contains 1,692 source files and two DLLs. Artifact SHA-256 values:

| Artifact | SHA-256 |
| --- | --- |
| `A2FORefitDisplay.dll` | `6f500bd979666752015a862338d67db4cdda6890e63790394a6ee5012f933dfd` |
| `A2FORefitRenderer.dll` | `a611319afba060c79d04e3e33ec3644c1768a8ae10f3ee2cc6f78363febe033e` |

- [x] Vulkan and DX12 each ran the runtime fixture in LDR mode 0 and HDR/MSAA/bloom mode 7 under private Wine 11.13, Xvfb and Lavapipe. Receipts and logs are under `build/refit/hud-post-baseline/runtime/ldr/` and `runtime/hdr-msaa-bloom/`; exact runner evidence is recorded in the checkpoint.
- [x] The scene fixture covered owned SOD and BGRA texture copies, camera and node poses, directional/point-light positive and opposite controls, stale model revision/source epoch rejection, one-sided/mirrored/hidden geometry, and presentation readback. A readback-after-present defect in `third_party/armada2-refit-native/gpu/src/dx12/cmd.rs` was fixed for this candidate.
- [x] The synthetic FO-style HUD composition retained corner RGB `(0,128,0)` while the model pixel remained `(0,0,127)` on both backends. This verifies the fixture composite and preservation of the half-alpha HUD color; it is not a live FO HUD capture.
- [x] In mode 7 both logs report `Rgba16F` format 113 with four samples and flags 7, without fallback. The bloom pass runs; visible halo strength and quality are not qualified.
- [x] Refit UI was not loaded. No `UI.asi`, D3D8 or D3D9 runtime was used by these backend fixtures.
- [x] Twelve builder/profile tests passed for the checkpoint. The frozen source/artifact snapshot and receipts bind the results to their recorded inputs.
- [x] `tools/check_refit_fo_profile.py` checks only the exact SHA-256 of two FO executables, their factory IAT and a complete 56-byte mode finder plus six call sites. It does not qualify other GOG symbols or engine layouts.

## ABI v2 scoped-parts implementation (in progress)

- [x] ABI v2 is defined in `renderer/refit/host.h` as an extension that leaves ABI v1 sizes and the first eight function pointers unchanged. It adds explicit prepare/draw for opaque parts, encoded DDS/TGA uploads, and a synchronous model-position transform callback.
- [x] ABI v2 GPU fixture receipt `build/refit-parts-checks/20261106T095022Z/2771905/receipt.json` passed on Vulkan and D3D12 using private Wine 11.13 and Lavapipe software GPU paths. Tested Display hash: `9343f4e912a8468c9c1f3f1bd1bc19e532b59f00a0979963fa11f7473f3ef636`; Renderer hash: `cc4babca8a90d23e6571c2b27dbb3b3a574e0c626e41be90fb78ca5dd6137f51`. Fixture logs report direct parts, encoded textures, transforms, copies, depth, fixed-function color, and rejection checks passing.
- [x] `build/refit-packet-checks/result.txt` reports exit 0 and all assertions passed. The private Wine Native11 material-bridge check also passed; these checks are not FO scene acceptance.
- [x] The full Core build and 198-export contract passed. The newer staged workspace candidate `build/refit/fleet-workspace-candidate/A2FOExtensions.dll` has SHA-256 `c1d5ba03a29ede2a7d98f1d28a382a1193d366139f4c22829aec3c94bfe7ff2e`.
- [x] The startup bridge now invokes CPU model/texture/node/material observers before workspace publication. The generated profile pins FleetOpsHook SHA-256 `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`; its constructor wrapper at RVA `0x1cedec` preserves original native forwarding and team setup. Workspace guard/mutation fixture `build/refit-fleet-constructor-checks/20261106T102918Z-2803027-1793960958085065673/receipt.json` passed; `build/refit-workspace-checks/result.txt` passed callback isolation, exact original entry, and `ret8` stack balance.
- [x] Latest ABI2 GPU fixture receipt `build/refit-parts-checks/20261106T102701Z/2800722/receipt.json` passed on Vulkan and D3D12 using Display SHA-256 `2130684bfd1142814e8c2ae74f974f711a234cae9418f1fddc091e91a7f4bbef` and Renderer SHA-256 `cc4babca8a90d23e6571c2b27dbb3b3a574e0c626e41be90fb78ca5dd6137f51`.
- [x] Transactional window-handoff receipt `build/refit-window-checks/20261106T113541Z-2834787-1793964941386334699/receipt.json` passed fixed and movable window cases on both Vulkan and DX12. It binds guarded Display SHA-256 `6361156f2b08a7c47e02324f1d4e7c913eecd1b79c73b37807a53b20950abdcb` and Renderer SHA-256 `cc4babca8a90d23e6571c2b27dbb3b3a574e0c626e41be90fb78ca5dd6137f51`; each of its four case logs reports `PASS`. Reset handling invalidates attached content to `STALE`, accepts `reset_content(2)`, then reuploads and verifies draw/readback.
- [x] Separate GPU alpha-cutout receipt `build/refit-alpha-gpu-checks/20261106T115551Z/2855521/receipt.json` passed on Vulkan and DX12 using Display SHA-256 `6361156f2b08a7c47e02324f1d4e7c913eecd1b79c73b37807a53b20950abdcb` and Renderer SHA-256 `ef1dd423f0866b569444043586314c57e60af3515a9be7ac71249b0c1f0bac28`. It verifies cutout output alpha is enabled only for cutout mode; TGA alpha 64/192 and DXT1 transparent-index 3/opaque-index 0 controls preserve original color/depth for discarded fragments and write color/depth for covered fragments on both backends. This qualifies the fixture path only, not FO SOD/cutout visual parity.
- [x] The exact paired per-mesh bridge receipts pass: `build/refit-mesh-bridge-checks/20261106T125406Z/2885239/receipt.json` preserves PRE/original/POST ordering and return state, while `build/refit-caller-abi-checks/20261106T132443Z/2899201/receipt.json` validates four D3D8 draw entries, stdcall stack/register state, and caller/argument capture on Vulkan and DX12. The mesh material guard `build/refit-mesh-material-guard-checks/private-20261106T1350/receipt.json` passes its pre-batch profile, partial-install, observer-ownership, and argument guards without executing a native function.
- [x] Mesh GPU fixture `build/refit-mesh-workspace-gpu-checks/reused-private-20261106T1325/receipt.json` passes on Vulkan and DX12. It binds Display SHA-256 `12d051072a48705d577306297c16a019373387c24862d42d9c0710d6b903e239` and Renderer SHA-256 `ef1dd423f0866b569444043586314c57e60af3515a9be7ac71249b0c1f0bac28`; this is backend fixture evidence, not a live FO replacement draw.
- [x] Historical material-base Core build `build/refit/material-base-pass/receipt.json` and export check `build/refit/material-base-pass/export-check.json` pass with Core SHA-256 `3336b986940afc093ff8fa78783589661987ffbcf3b8e5f31c926a5f24b24b21` and all 198 exports. The paired private Vulkan trial is `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/material-base-Duel-1793972628814762750/receipt.json`; its receipt-bound `current-run-logs/A2FOExtensions.log` reports `SubmitMeshes=1`, a single-group prepared packet, and `observation only, native submission retained`. The private diagnostic profile explicitly opts in; source default remains `SubmitMeshes=0`.
- [x] First native scoped submissions are recorded on both private backends. Vulkan receipt `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/geometry-owned-Duel-1793974485725683741/receipt.json` and DX12 receipt `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364/revisions/geometry-owned-Duel-1793974610697502651/receipt.json` both report `launch-diagnostic-complete`, alive at 100 seconds, and no input automation. Receipt-matched current logs record `first owned single-group mesh submitted through native scene API`; each capture shows the original FO ship and HUD. The earlier pre-batch observer line saying “observation only, native submission retained” describes its observation stage; the later submission event is the newer milestone. These are still software-GPU/Wine diagnostics.
- [x] Frozen provenance `build/refit/first-live-owned-scene-checkpoint/checkpoint.json` contains 2,708 files (64,363,196 bytes), Core/Display/Renderer hashes, both backend receipts/captures, 921 attested Core sources, and unchanged protected hashes. It records Refit UI unloaded, not deployed, not gameplay accepted, not hardware accepted, and no controlled pixel proof. Component receipt source lineages differ; this is not a clean full release rebuild.
- [x] Geometry guard validates the projected triangle multiset between owned SOD GPU-cull output and the native RHW index stream (80 source triangles, 38 observed native triangles); captured geometry matches within the checkpoint's tolerances. The path requires an unlimited native draw budget, falls back for clipping/degeneracy, and uses the corrected Raster2 pixel-center query. This is geometric/raster evidence, not final pixel or material parity.
- [x] Raster/draw-entry, paired-parts, all-regions guard, and group-material-query receipts pass: `build/refit-raster-draw-entry-checks/20261106T141327Z/2926065/receipt.json`, `build/refit-raster-parts-checks/private-reuse-20261106T141405Z/receipt.json`, `build/refit-mesh-all-regions-guard-checks/private-20261106T141610Z/receipt.json`, and `build/refit-group-material-query-checks/20261106T141639Z/receipt.json`.
- [x] Pixel-control Core `build/refit/pixel-control-candidate/current-core-source-check.json` binds SHA `c20cfef10fe7010da90b115a64207089531441ce30de10a3b8f6dc4073464b79`; the strict test exposed the untextured-black fallback in old Renderer `ef1dd423…c1b0fac28` (`build/refit-white-fallback-checks/old-renderer-20261106T143121Z/receipt.json`, expected failure). Renderer `2726ffeec1b5b191ac6f406d155ba084c69c359fc821c038be7dec35e0adb87b` passes Vulkan and D3D12 white-pixel/depth, black-negative, and post-reset white-cache checks in `build/refit-white-fallback-checks/corrected-20261106T143224Z/receipt.json`. Later same-candidate FO contrast results in `build/refit-live-pixel-controls.json` compare the white/black ROI `[1035,425,1155,600]`: 5,053 bright white pixels, 0 bright black pixels, with 5,126 changed pixels in Vulkan and 5,119 in DX12 at the specified all-channel threshold. Random map/race/sky/timing differ, so this proves controlled pixel contribution only, not whole-image parity or gameplay.
- [x] New private FO white/black control trials for Core `c20cfef1…73464b79`, Display `3c317ae7…eaf865d3`, Renderer `2726ffeec1b5…e0adb87b` are summarized in `build/refit-live-pixel-controls.json`. The paired white and serial black runs each remained alive for 100 seconds, had capture exit 0, and retained the stock HUD. Accepted receipts: Vulkan `pixel-white-fixed-Duel-1793975481560652749` vs `pixel-black-serial-Duel-1793975896162692333`; DX12 `pixel-white-fixed-Duel-1793975481560615808` vs `pixel-black-serial-Duel-1793976040860762084`. Earlier parallel-black capture timeouts are preserved but are not part of this accepted pair.
- [x] Newer normal-material runs use the same Core `c20cfef10fe7010da90b115a64207089531441ce30de10a3b8f6dc4073464b79`, Display `3c317ae7d4cea35a7189d2c7476f89fcdbba769281ae76da76083539eaf865d3`, and Renderer `e9a7829199f426dfe51639b992b8f7622692eddf5fbc4afe8d186769a3dbbfca`. `build/refit-normal-material-results.json` records both backends alive at 100 seconds with capture exit 0 and stock FO ships/HUD visible. The same-hash strict GPU fixture passes in `build/refit-white-fallback-checks/lazy-20261106T144459Z/receipt.json`. These are normal-material runtime and fixture checks, not same-candidate controlled white/black contrast or complete parity.
- [x] The private Vulkan workspace trial receipt `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/fleet-map-Duel-1793961046435223671/receipt.json` records the staged Core/Display hashes and process alive through 20 samples/40 seconds. Its fresh `A2FOExtensions.log` records source observers armed and first model asset bytes copied; that log describes scalar metadata only, not scene pixels.
- [x] `core/refit_scene.cpp` provides the opt-in per-mesh path. It pairs exact native mesh entry/exit scopes, correlates `A2FORefitDisplay_LastCaller_v2` `Caller2` sequence/ESI with the draw record and checked instance, then registers `A2FORefitDisplay_ReplaceMesh_v2`. The only eligible model gate is a supported legacy SOD `Storm3D_SW`/`StarTrekDB` format version 1.7 through 1.93, one visible authored group, and the legacy route at opaque depth-writing base pass 0; unsupported or ambiguous meshes keep native draws. `SubmitMeshes` defaults to 0; private diagnostics explicitly set it to 1. The newer geometry-owned trials log first native scene API submission/suppression for this bounded scope on both backends. This does not establish controlled pixels, material parity, or broader replacement coverage.
- [ ] A DEVICE failure during replacement draw can leave partial color/depth writes, may produce a mixed frame, and disables further replacement for that process attempt. The callback does not promise GPU rollback or atomic frame replacement.
- [ ] The scoped-parts API, exact mesh scopes, and base-pass submission do not close full live replacement geometry/material, lighting, shadows, HUD-over-replacement, or gameplay gates. Same-candidate white/black ROI controls show modern pixel contribution with the FO ship/HUD; whole-frame and material parity remain open.

## Integration and acceptance still open

- [x] The experimental `renderer/refit/startup.cpp` bridge is wired at process attach operation 11 in `core/dllmain.cpp` and included in `Makefile` `CORE_SOURCES`. Its Core build and export check passed as summarized below; this source integration is not live FO startup evidence.
- [x] The child backend opt-in is restricted to `refit-vulkan-test` and `refit-d3d12-test`. `/d3d9` and `-d3d9` are explicitly refused by this D3D8 factory path; it logs that the legacy renderer is retained.
- [x] Before a selected native attempt, `core/nebula_renderer.cpp` reserves the graphics route and suspends legacy DXVK-only material/draw hooks. If base FO startup later falls back, it uses base FO rendering; restarting on the normal backend restores A2FO effects. This is a limited fallback, not fully qualified visual equivalence.
- [x] CPU startup policy receipt `build/refit-startup-checks/20261106T023157Z-2671771-1793932317035882701/receipt.json` passed. Transaction receipt `build/refit-transaction-checks/20261106T023432Z-2672659-1793932472781163285/receipt.json` passed for the six mode-call transaction across three RX pages, fault rollback, owned PIC finder, real thread-state checks and bounded loader-phase probing. The phase probe uses a fresh thread, `SetEvent` and a bounded 100 ms wait; if it cannot confirm the required phase, the original factory is retained before hashing or loading helpers. Non-owner factory calls in states 1 and 2 returned `NULL` without state mutation or changes to the competing legacy factory.
- [x] Deferred-factory DLLs were exercised under both backends with `Prepare` and flags 7 (`build/refit-host-checks/20261106T022527Z/2666481/receipt.json`) and default `Configure` with flags 0 (`build/refit-host-checks/20261106T023433Z/2672721/receipt.json`). Those mutable DLLs are Display `6c09e505ea512fb763f1fe2ba08cdcd7210ba0aa266688820a16443e34b7b804`, Renderer `d441ce6653ee392921bc1798ae0f48333dfb9a1163002bebd39cdfa026a34886`. These receipts are child fixtures, not FO startup.
- [x] The wired Core source built with `make -j4 build/A2FOExtensions.dll`; `build/refit/core-factory-build.log` records success. The earlier locked media-cache fetch failure is preserved in `build/refit/core-factory-build-initial-failure.log`. `build/rust-dll/receipt.json` reports exit 0 and `changed_inputs: []`.
- [x] `build/refit/core-export-check.json` passes against `core/A2FOExtensions.def`: 198 exact exports, with 11 imports; `renderer/refit/startup.cpp` is included in the linked Core object set. This closes source wiring/Core build/export-contract only.
- [x] New frozen checkpoint `build/refit/factory-checkpoint/checkpoint.json` records 2,369 integrity-verified files, artifacts and receipts; all seven protected live hashes are unchanged. It keeps the earlier 1,692-file `hud-post-baseline` checkpoint intact and separately records the current startup candidate.
- [ ] Treat the frozen Core artifact as qualified only for its recorded source/link and fixture evidence. It includes prior class-completion WIP; the broader 164-suite was not rerun.
- [x] A private, isolated diagnostic launch reached the stock FO main menu on both backends using exact artifacts from `factory-checkpoint`; receipts report status `launch-diagnostic-complete`, two separate cloned install trees, 20 process samples over 40 seconds, and Armada alive at the observation limit. Startup logs record the selected test backend and factory return, and display logs show device/swap-chain initialization. This proves a bounded menu/device-startup diagnostic only.
- [x] Same-candidate controlled white/black pixel contribution is demonstrated on Vulkan and DX12 by `build/refit-live-pixel-controls.json` for Renderer `2726ffe…e0adb87b`; this qualifies the controlled ROI and modern contribution only. The older frozen `first-live-owned-scene` checkpoint correctly retains `controlled_pixel_proof=false` for its earlier candidate/evidence boundary.
- [ ] Qualify complete material/color parity and full-frame equivalence on the newer normal-material Renderer `e9a78291…a3dbbfca`; no same-hash controlled white/black captures are recorded. Broader geometry/material coverage remains open.
- [ ] Complete the host lifecycle gates before treating this as a full profile: process-lifetime-pinned helper DLLs; one GPU attach attempt; no live unload or retry; no shadows or spot lights. See [the host API contract](refit-host-api.md).
- [ ] Confirm explicit refusal/fallback in the real FO path and qualify the base-FO fallback's visible behavior. Native-route reservation can suspend DXVK-specific A2FO material/draw hooks even when later startup falls back; normal-backend restart restores them.
- [ ] Validate actual Fleet Ops content and scene behavior (including SOD/material/animation coverage, camera and transforms, lighting, effects, visibility and game state ownership) on each backend. The synthetic scene fixture is not that coverage.
- [ ] Keep FO HUD/UI ownership intact and verify the real HUD over a live scene. Do not port or load Refit in-game UI.
- [ ] Qualify bloom appearance, visible lighting/shadow behavior and performance/stability against defined live-game baselines.
- [ ] Complete manual gameplay, startup/return, resize/focus, failure/rollback, and deployment acceptance independently for DX12 and Vulkan. Current candidate is not deployed.
- [x] Historical private Vulkan material-base diagnostic prepared a single-group packet but retained the native draw; it is superseded by the later two-backend geometry-owned submissions below.

### Private FO launch diagnostic (limited evidence)

The following two trials used the frozen `build/refit/factory-checkpoint` Core, Media and Refit helper artifacts. Each ran in a separate copied Fleet Ops installation under system Wine 11.13, with an independent copy of the initialized private GPU-fixture prefix. The runner's receipts report isolated execution, host home and input devices hidden, hardware graphics hidden, external routes disabled, no source writes or input automation, 20 process samples across 40 seconds, and Armada alive at the limit. This is not a fresh-prefix initialization result: earlier kernel32/bootstrap and timeout failures are retained in attempt logs; the diagnostic was resolved using a hash-checked copy of the already initialized fixture prefix. One child AssetVersion compatibility warning was also fixed in the test copy; the earlier warning remains in the attempt record.

The diagnostic runner is `tools/run_refit_fo_bootstrap.py`; `--seed-prefix` explicitly accepts the recorded fixture prefix, while `--retry-trial` archives/stops an owned failed trial before retry. These options do not make a fresh-prefix bootstrap pass.

| Backend | Trial receipt | FO/display evidence |
| --- | --- | --- |
| Vulkan | `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/receipt.json` | Armada reached the stock main menu (`private-root.png`); `A2FOExtensions.log` records the Refit native factory armed and Vulkan factory returned while retaining FO UI. `A2FORefitDisplay.log` reports llvmpipe (LLVM 22.1.8), Vulkan 1.4.354, swap chain 1280x1004, target 1280x1024 with D16 depth. |
| DX12 | `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364/receipt.json` | Armada reached the stock main menu (`private-root.png`); `A2FOExtensions.log` records the Refit native factory armed and DX12 factory returned while retaining FO UI. `A2FORefitDisplay.log` reports feature level 11_0, swap chain/target 1280x1024 with D16 depth and “frame 1 presented”. Pipeline library error `0x887a0004` falls back to plain pipelines and is nonfatal in this run. |

The screenshots show the FO stock main menu (`0.1-test`). Do not use either trial's `A2FORefitRenderer.log` under `prefix/drive_c/users/tamsynn/AppData/Local/armada2-refit/logs/` for current module claims: both are unchanged copies of the seeded log, and the DX12 copy still contains stale Vulkan text. The fresh Core and Display logs establish only the startup/device facts listed above; neither trial proves current Render3D scene activity.

These are the initial menu-only trials. Later private Duel revisions provide additional, backend-specific evidence below; they do not retroactively change these captures or their scope.

The lead froze and integrity-checked `build/refit/fo-startup-evidence/receipt.json` (66 files), including per-backend receipts, Core/backend logs, menu captures, source runner and failed-attempt logs/metadata repair. Candidate source and binaries are bound to the immutable `factory-checkpoint` artifacts. The bundle confirms all seven protected live hashes and source private-copy configuration/provider remained unchanged after both runs. The test copy rejected an older `A2FORefitMenus` module; this is evidence for that copy only. It does not address the separate requested animated 3D menu backgrounds. Refit in-game UI remains excluded.

**Evidence boundary:** these initial runs show FO startup to its stock menu and successful display/device initialization under Wine with software GPU paths. Later Duel runs log a scoped native scene API submission/suppression on both backends. Same-candidate white/black controls show a modern pixel contribution in the selected ROI on both backends alongside the original FO HUD. This does not establish whole-frame equivalence, complete material parity, gameplay, native Windows, physical GPU, deployment, or manual acceptance. Keep all corresponding first-release gates below open.

### Earlier private Duel diagnostics (limited, historical evidence)

| Backend | Revision and receipt | Historical evidence and limits |
| --- | --- | --- |
| Vulkan | `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/scoped-shader-Duel-1793965969864864470/receipt.json` | Receipt status is `launch-diagnostic-complete`, process alive at the 100-second limit, isolated private Wine fixture, and input automation false. Its attested `current-run-logs/A2FOExtensions.log` records `fed_venture` and `fed_mediterraneanY`, first checked render scope, authoritative posed material source, Vulkan host initialization, and a posed packet prepared/scoped replacement registered. The matching `current-run-logs/private-root.png` shows an original FO ship and stock HUD through native Display. This does not confirm a modern replacement draw. |
| DX12 | `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364/revisions/alpha-material-Duel-1793965566879184258/receipt.json` | Receipt status is `launch-diagnostic-complete`, process alive at the 100-second limit, isolated private Wine fixture, and input automation false. Its attested `current-run-logs/A2FOExtensions.log` records `fed_venture` and `fed_mediterraneanY`, first checked render scope, authoritative posed material source and DX12 host initialization. The matching `current-run-logs/private-root.png` shows an original FO ship and stock HUD through native Display. This does not confirm a modern replacement draw. |

Earlier Duel revisions document original FO rendering through native Display on Vulkan and DX12; their receipt-matched log/capture evidence remains below. The material-base trial is historical and is superseded for submission status by the later geometry-owned trials above. Current trial logs are receipt-bound under each revision's `current-run-logs/`; preceding-run data is retained separately. The trials used isolated private config/profile, no input automation, and loaded no `Scene.asi` or Refit in-game UI. They log scoped modern scene API submission/suppression, but do not establish controlled replacement pixels or release qualification.

## Evidence boundary

The frozen renderer checkpoints prove backend fixture execution in private Wine with software Lavapipe. Initial private FO bootstrap evidence proves startup through the stock main menu and device/swap-chain initialization on both backends. Later geometry-owned Duel revisions log scoped native scene API submission/suppression. Same-candidate white/black controls show pixel contribution in the selected ROI on Vulkan and DX12, while normal-material runs use a newer Renderer hash without same-hash controls. Whole-frame and material/color parity, broader content, gameplay, physical-GPU, deployment, and release acceptance remain open.

## Full acceptance checklist

The following gates retain the complete first-release scope. Check an item only when its evidence is tied to the exact candidate source/DLL hashes, supported FO executable/profile, and backend. The headless checkpoint above does **not** complete any live-game, hardware, gameplay, deployment, or release gate below. Keep DX12 and Vulkan evidence independent.

### 1. Source, build, provenance and ABI

- [x] Compare local changes against the pinned upstream manifest; record added, changed and missing files, upstream commit, local patch hashes and license/content provenance. Done 2026-11-07 (Claude): the manifest matches a fresh fetch of `1e2a7d2f…` exactly (1,652 files, 0 hash mismatches); locally 1,629 unchanged, 23 modified, 0 missing, 8 added; every licence/notice is present and unchanged. Ungated stock-build changes are listed (DX12 readback bug fix; checked post/frame chain). See [refit-native-provenance.md](refit-native-provenance.md) and `build/refit-provenance/20261107T011839Z/`.
- [ ] Record which native renderer modules are enabled, excluded or still fallback-owned; name the missing capability for every exclusion.
- [ ] Review the `a2fo-host` entry points for Display and Render3D. Prove DllMain performs no profile scan, hook install, window/input work or GPU startup; initialization must enter through reviewed host admission.
- [ ] Build both components from clean private targets with pinned toolchain/lockfiles and the generated local linker wrapper. Retain exact commands, full logs, tool versions, source hashes and artifact hashes; account for shader blobs/regeneration provenance.
- [ ] Verify both candidates as x86 PE DLLs: imports, exports, dependencies, entry points, resources and unexpected import/export failure conditions against the reviewed ABI.
- [ ] Exercise host fixtures for initialization, version/profile rejection, backend startup failure, one-shot behavior, shutdown boundaries, and repeated loading only where supported by the ABI. Preserve failure output.
- [ ] Rebuild from a clean target and compare receipt, imports/exports and artifact hashes. Explain nondeterminism before freezing a release candidate.
- [ ] Freeze the exact candidate input/output manifest before any staging or deployment review.

### 2. Exact FO startup, factory and mode admission

- [ ] Keep the offline profile checker limited to the exact two pinned FO executable SHA-256 values. Reconfirm PE identity, factory IAT, complete finder bytes and each of the six mode-finder call sites for those exact files.
- [x] Review the now-wired `renderer/refit/startup.cpp` and generated profile data; the Core build and export check passed, as recorded above. This establishes source/link integration, not a live scene bridge.
- [ ] Preserve explicit refusal of FO `/d3d9` and `-d3d9` in this D3D8 factory path, with legacy renderer untouched. A separate D3D9 device/factory bridge is unsupported and must not be inferred from the D3D8 import profile.
- [ ] Verify the factory and mode-call contracts, x86 calling convention/register/stack behavior, thread and object ownership, and version checks from exact FO binaries/traces before the bridge invokes them.
- [ ] Prove unsupported executable/profile, stale profile, unexpected factory, missing module, and partial initialization leave the existing FO renderer authoritative with one actionable diagnostic.
- [x] The frozen private FO trial receipts and fresh Core/Display logs record admitted executable profile, staged module hashes, selected test backend, factory return and device initialization through the stock menu for each backend.
- [ ] Capture and qualify a live FO 3D frame rendered through the modern scoped per-mesh API on each backend. Current Vulkan evidence reaches prepared packet/scope registration but retains the original native draw.
- [ ] Verify startup, menu-to-game and return/shutdown ordering with existing core/menu/media loaders. Demonstrate failure cannot leave patches, callbacks, a half-owned device or duplicate registration active.

### 3. Host lifecycle and ownership

- [ ] Implement the v1 host contract in [refit-host-api.md](refit-host-api.md): one owner thread, no reentry, copied input spans, Display/HWND ownership, and process-lifetime-pinned helper modules.
- [ ] Enforce one GPU attachment attempt per process. After an attachment attempt fails, preserve the existing renderer and require process restart before another attempt; do not add live retry or unload behavior without a separately reviewed ABI change.
- [ ] Validate wrong thread, malformed spans, stale epoch/revision, missing device, limits, content errors and device failures against the stated status behavior. Prove which failures preserve the prior frame and which invalidate readback.
- [ ] Exercise initialization and teardown/restart boundaries with no dangling callbacks, worker activity, GPU resources, stale host pointers or duplicate module registration.
- [ ] Preserve a selectable existing renderer path and log the actual backend and fallback reason on every admitted/rejected startup.

### 4. Native device, frame and presentation path (separate backend results)

Record backend, adapter/device and driver, OS/runtime, window mode, dimensions, settings, candidate hashes, logs and captures per run.

| Native gate | DX12 evidence | Vulkan evidence |
| --- | --- | --- |
| Adapter discovery, required feature/driver checks, device/queue creation and bounded unsupported/failure path | [ ] | [ ] |
| FO window/swap-chain or surface creation; resize, fullscreen/windowed/borderless, focus loss and restoration | [ ] | [ ] |
| D3D8-shaped translation for formats, mipmaps, samplers, FVF/streams, indexed draws, blend/depth/stencil, clip and fog | [ ] | [ ] |
| Upload/readback ownership, barriers/layout transitions, descriptors, synchronization and deferred destruction | [ ] | [ ] |
| Present, pacing, VSync, frame latency, color space, gamma and screenshot/capture behavior | [ ] | [ ] |
| Device loss/removal, swap-chain/surface recreation, failed allocation and recovery without leaked or stale ownership | [ ] | [ ] |
| Deterministic synthetic replay with saved output and backend validation diagnostics | [ ] | [ ] |
| First real FO frame with loaded module hashes, admitted profile, selected device/backend, frame count and capture | [ ] | [ ] |
| Native Windows runtime and physical GPU verification on named supported hardware | [ ] | [ ] |

### 5. Live scene bridge, camera, visibility and module coverage

- [ ] Map FO camera, view/projection, viewport, world coordinates, transforms, clipping and visibility into submitted frame data. Compare captured FO state with the exact frame records on both backends.
- [ ] Validate handedness, winding, culling, normals, UVs, matrix layout and near/far depth with asymmetric models/cameras; explicitly guard against inside-out geometry.
- [ ] Bridge object creation/update/destruction, ownership changes, team colors, selection and tactical overlays with stable identities and safe lifetimes.
- [ ] Keep FO simulation authoritative for movement, combat, fog, damage, orders and timing. Prove rendering/light updates do not mutate simulation or rely on unvalidated Armada addresses.
- [ ] Validate game and editor/script camera paths separately, including world-to-screen picking and cursor alignment where applicable.
- [ ] Record per-view ownership for all eight native Refit modules: world, particles, hulls, effects, ordnance, specials, sky and overlays. For every supported view/effect, establish which module draws it and what explicit fallback remains.
- [ ] Validate actual live FO visibility, fog/shroud and selection/tactical readability. A synthetic hidden-node fixture does not establish game visibility ownership.
- [ ] Validate separate menu-scene and gameplay-scene lifetimes so menus do not leave a gameplay simulation or renderer state active.

### 6. FO assets, formats, materials, animation and LOD

- [ ] Inventory representative FO 4.0 and modded SODs, textures, materials, hardpoints, sprites, particles, skies and terrain/nebula assets. Record supported/unsupported cases and visible fallback for each. **SOD/material part (static) done 2026-11-07 (Claude):** all 927 effective SODs and 10,715 ODFs. For the 230 unit models, 74.5% of triangles are statically eligible, 16.7% are two-group (now handled opt-in) and 7.7% are blocked by undefined lighting materials (load-order-dependent global namespace, proven in `ArmadaL.exe`). See [refit-sod-coverage-census.md](refit-sod-coverage-census.md). Textures, hardpoints, sprites, particles, skies and terrain remain open.
- [ ] Verify actual FO FPQ/archive and loose-file lookup, case handling, path precedence, mod overrides, missing files and live mod changes against FO's existing asset service. Preserve established precedence; do not assume upstream Armada lookup matches FO.
- [ ] Validate SOD geometry, node hierarchy, scale, animation and hardpoint transforms against known FO assets. Treat M3D or any other model format as unsupported until separately evidenced.
- [ ] Verify diffuse, alpha/cutout, emissive, specular, normal/bump, gloss/roughness and environment/reflection behavior against hashed reference assets and captures.
- [ ] Validate compressed DDS formats, mip selection, UV orientation, texture stages, sampler filtering/anisotropy and LOD bias on both backends.
- [ ] Validate node poses, mesh/texture animation, turrets, weapon muzzle/hardpoint transforms, build/research animation, damage states, destruction and wreckage using live FO examples.
- [ ] Exercise LOD transitions, instancing/batching, culling and transparent sort with mixed ships/stations/planets/nebula scenes; compare captures and draw/object counts.
- [ ] Exercise corrupt/missing assets and bounded logging/placeholder/fallback behavior without crash, runaway allocation or unbounded work.

### 7. Existing FO HUD compatibility; Refit UI remains excluded

- [ ] Preserve Fleet Operations' existing HUD, minimap, selection brackets, text/fonts, tooltips, build queues, buttons, mouse hit targets, panels and controls. Do not load or integrate Refit in-game UI or `UI.asi`.
- [ ] On both backends verify the actual FO HUD composited with the live 3D frame, correct alpha/color, alignment, hit targets and resolution/DPI behavior. Synthetic green-corner/model pixels do not satisfy this gate.
- [ ] Confirm the HUD remains on the existing owner/path and is not accidentally bloomed, tone-mapped, overwritten or drawn twice by the replacement renderer.
- [ ] Preserve existing menu navigation, loading, progress, tips, sound/music and cinematic/Bink timing while the renderer changes; verify no renderer initialization interferes with them.
- [ ] Decide explicitly whether the separate animated 3D menu scene and map/Lua menu presentation are in scope for the first supported FO path. If included, verify scripting hooks, initialization, animation, interaction and cleanup; if excluded, document the user-visible fallback. Do not silently import Refit menus/UI.

### 8. Lighting, shadows and ODF control

Four static private software-GPU on/off ODF controls qualify directional colour
on Vulkan and DX12 and point colour on Vulkan and DX12 for one receiver ROI.
All four independent reviews are complete.
These checks do not close the broader rows below, which require movement,
falloff/range, material response, spots, physical shadows, hardware and gameplay.

- [ ] Define and review FO-facing light ownership and records: sun/directional, point, spot/cone, weapons/explosions, planets and nebula contributions; specify coordinate space, color space, attenuation and attachment rules.
- [ ] For directional sun/global light, point lights and spot/cone lights, use positive and negative controls that show position/direction/strength/range/cone changes affect the correct objects and fade/stop correctly.
- [ ] Validate per-pixel hull/station material response and emissive intensity with static/moving objects, multiple simultaneous lights, dark/high-contrast scenes and captures on both backends.
- [ ] Generate native scene depth from submitted geometry. Do not claim complete shadows from a screen-space guess, global ground plane or legacy depth buffer.
- [ ] Validate actual caster/receiver geometry at different heights and camera angles, attached shadows on moving ships, station self-shadow, off-screen casters, nebula occlusion, bias, filtering, range, transparency and light leaks.
- [ ] Validate multi-light and moving-caster cases for stale/detached/flickering shadows, including off-screen casters and object/camera motion.
- [ ] Design and review the requested ODF lighting keys for **source, type, direction, colour, intensity/strength, range and cone**, including units/defaults/ranges, coordinate spaces, source attachment/hardpoint rules, inheritance/override order and malformed-value behavior.
- [ ] Exercise parsed ODF values through base, inherited and mod override cases; capture parsed records and rendered positive/negative controls for every light type on both backends.
- [ ] Demonstrate the named Nexus lighting/shadow showcase with reproducible scene/settings, visible illumination, physical geometry shadows and ODF control on both backends. The current static private point/directional controls do not satisfy this showcase; spotlights and shadows remain open.

### 9. Effects, post-processing and visual quality

- [ ] Validate named FO shields, cloak/refraction, repairs, explosions, debris, plasma/venting, engine trails, warp, torpedoes/beams and special weapons. Record the source/module that rendered each effect.
- [ ] Validate fog of war, shroud, nebula occlusion, selection/target overlays and tactical readability in gameplay, observer and editor views where supported.
- [ ] Qualify HDR/LDR targets, depth resolve, MSAA, exposure/tone mapping, bloom and final gamma using live scene captures. Format/sample/flag logs prove allocation/configuration only; the bloom pass running does not prove visible halo quality.
- [ ] Confirm disabled post effects still yield a complete readable frame and do not change FO HUD colors/alpha or capture semantics.
- [ ] Compare visual captures with the existing renderer at matching camera/settings for geometry counts, materials, visibility, positions, HUD alignment, lighting and effects.

### 10. Stability, performance, memory and multiplayer

- [ ] Stress resource create/destroy, repeated map loads, mod changes, long sessions, shutdown, rapid resize/alt-tab/focus changes and repeated launches; record leaks, GPU validation errors and worker state.
- [ ] Measure startup, first frame, median/tail frame time, RAM/VRAM, upload stalls and shader compilation/cache against the existing renderer using the same scenes/settings and hardware.
- [ ] Set reviewed performance and stability budgets before acceptance. Bound caches, frame queues, uploads and allocation within the 32-bit process address-space budget; test large fleets and long matches.
- [ ] Test low-memory, unsupported driver/GPU and deliberate device failures. Require bounded failure, actionable logs, legacy fallback and verified recovery/rollback.
- [ ] Verify renderer, lighting, effects and backend-specific frame updates do not alter multiplayer simulation/network state or cause desynchronization.
- [ ] Inspect fresh logs and preserve each first failure/reproducer before changing another variable. Distinguish sandbox/Wine limits from product failures.

### 11. Staging, package, deployment and rollback

- [ ] Stage exact DLLs/configuration under a private candidate directory. Bind staged files to source, profile and input hashes; verify PE imports/exports, expected file set and package manifest.
- [ ] Record target install paths, active-process state, pre-deploy hashes and content-addressed backups. Preserve mods, config, assets, logs, old menu modules and existing renderer rollback copies.
- [ ] Produce a recoverable package with exact install/remove file lists, prerequisites, supported profile/backend/hardware, checksums and manual recovery steps. Do not run upstream installers or silently retire other renderers.
- [ ] Do not deploy until the release gates qualify. Once authorized, copy only the reviewed file set and verify post-copy hashes against the staged manifest.
- [ ] After deployment record actual loaded DLL hashes, admitted FO profile, selected backend/device, first-present result, startup logs and rollback location.
- [ ] On failed acceptance, restore originals from verified backups and prove normal startup and renderer hashes after rollback.

### 12. Full manual gameplay acceptance (record DX12 and Vulkan separately)

For every case record executable/profile, mod/map/faction, hardware/driver, settings, duration, candidate hashes, logs, screenshots/video, first defect and comparison with the existing renderer. No headless fixture can mark these rows complete.

The private bootstrap diagnostics reached the stock main menu on both backends, but the cold-start row below remains open because it also requires intro/options/Instant Action setup, game launch/loading, return and clean exit, plus manual acceptance.

| Manual case | DX12 | Vulkan |
| --- | --- | --- |
| Cold start, intro, main menu, options, Instant Action setup, game launch/loading, return to menu and clean exit | [ ] | [ ] |
| Small Instant Action: select, move, attack, build, research, cloak/shields/specials, save/load and return/exit | [ ] | [ ] |
| Multiplayer lobby, chat, supported connection/session types, mixed-backend match, disconnect/rejoin where supported, no desynchronization | [ ] | [ ] |
| Observer and replay playback where supported: camera, speed controls, fog/player visibility, HUD and exit | [ ] | [ ] |
| Mixed FO factions and representative modded SOD ships/stations, nebula, planet, sky and map variants | [ ] | [ ] |
| Mission/campaign scripted events, cinematics, HUD, victory/defeat transition and clean exit | [ ] | [ ] |
| Map editor placement, picking, preview, sky/nebula and exit without corruption | [ ] | [ ] |
| Resolution/window modes, resize, alt-tab/focus, long session, repeated game launches and clean shutdown | [ ] | [ ] |
| Visual parity: materials, animation, effects, lighting/shadows, FO HUD and fog/shroud readability | [ ] | [ ] |
| Performance and stability meet the reviewed budgets on the named supported hardware | [ ] | [ ] |
| Failure path preserves the prior renderer; verified rollback restores hashes and normal game startup | [ ] | [ ] |

## First release gate and later enhancements

The first complete release is a playable Fleet Operations 4.0 renderer on both DX12 and Vulkan for the explicitly supported FO configuration. It must pass the source/build, exact host admission, native device, live scene bridge, supported SOD/material/animation, existing FO HUD, effects, lighting/shadows/ODF, performance/stability, staged recovery package and full manual gameplay gates above. The named Nexus lighting/shadow showcase must demonstrate visible illumination, physical geometry casting/receiving and ODF control with matching captures/logs/reproducible settings on both backends. Refit in-game UI remains excluded; existing FO UI ownership is a required compatibility gate. A reviewed exclusion must state the user-visible fallback for any unsupported gameplay case.

The following are optional later enhancements and do not expand the first release gate without a concrete use case and acceptance test: M3D support, mod-specific content outside the supported sample set, additional material models, extra shadow quality tiers and broader hardware optimization. Keep them separate from required release qualification. Compilation, file counts, software-GPU fixture success and profile parsing alone never establish a complete release.

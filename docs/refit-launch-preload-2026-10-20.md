# IA Launch continuity and fleet preloading

The user reported a flicker when launching from IA rather than controls disappearing into the camera pan, and fleet preparation during Single Player camera changes. The existing run, installed binaries and scene recipe are preserved under `build/refit-launch-20261020-141500Z/before/`. The requested fleet behavior is preload every fleet while retaining authored decloak effects.

## Launch transition

The admitted IA layout identifies its right navigation button from the verified two-button geometry, independent of translated text. A matching genuine down/up can arm a snapshot of the completed UI. A canceled press, disabled button, open combo or ambiguous navigation profile does not arm it. The original event is forwarded once, immediately; no input is synthesized or replayed and no native business action is replaced.

A scoped guard permits that snapshot to be used only if the native IA dialog hides or destroys itself during the same real release dispatch. Returning without closing clears the snapshot; a later unrelated hide cannot trigger a departure. Nested dispatch does not clear the outer guard. This avoids guessing acceptance from a visibility timeout. An asynchronous close outside that dispatch uses the existing loading fallback.

On a matched native close, the persistent parent receives the cached UI immediately. The existing worker fades that image into the live scene over 180 ms while starting the departure camera from the current view. CPU blending happens outside the short native-window copy lock. No extra GPU device or draw loop is created. Window identity/process/thread/parent checks and the bounded handover lease still apply.

The checked native splash keeps that departure epoch. The first native loading callback ends the GDI handover before its original callback can present, including fallback cases; it does not reset the camera. The loading engine retains ownership of progress and completion.

## Fleet preparation

Every ship in the authored scene already entered the scene description, but fully cloaked models were skipped before loading, and offscreen diffuse textures were still lazy. The menu host now prepares every recipe model, part diffuse texture and generated bump normal map before cloak/gone/frustum filtering. A normalized model/device cache prevents repeated preparation. Device reset clears it; new model names from edited recipes are admitted. Existing missing-asset behavior is retained.

Authored visibility, frustum culling and Klingon decloak timing are unchanged. No extra fleets or campaign limits are hardcoded; this preparation works from the scene's ship list, including future scene sources. The scene recipe and user settings are retained.

## Verification

Portable tests cover fade endpoints/midpoint/resizing, launch hit testing, translated text, disabled/open-combo/ambiguous cases and canceled/duplicate releases. The native setup fixture checks dispatch scope, one-time snapshot consumption, nested guards, and stale-snapshot cleanup on rejection. The native loading fixture checks that splash creation does not reset an existing IA departure.

The isolated graphics fixture exercised the actual worker, CPU fade sink, parent/window lifetime and continuation into loading with the same departure epoch. It also checked the prior menu/IA/campaign/warp/restart cases with one renderer. Departure produced 89 frames in three seconds, longest sampled gap 73 ms; UI exchange maximum was 1 ms. These are headless measurements, not native game FPS.

Provider logs and a parsed receipt confirm that all 15 distinct recipe models were prepared exactly once before the first Klingon camera visit, with zero missing models. Decloak remains authored. The source and compiled fixture do not establish native visible smoothness or the precise synchronous/asynchronous message order in the installed IA dialog; full-restart native acceptance remains necessary. No game entrypoint, desktop input or focus automation was used.

## Installed candidate

Final verification passed 13 portable checks, six Wine canvas/setup/clock cases, 25 native loading/provider/campaign flow cases, 38 DLL console processes, and the separate scene and D3D8 loading graphics fixtures. Independent review accepted the scoped native-release, nested-dispatch and preload behavior after guard fixes. The cold headless first scene took 3,664 ms including fleet preparation; its work remains on the existing renderer worker. Preparation intentionally moves asset cost to background startup rather than camera visits.

With the game closed, only RefitMenus and the scene provider were replaced under `/home/tamsynn/NVMeData/Fleet Ops Roots`. All 61 present protected files retained their hashes, including ShellDisplay, scene recipe, settings, engine, renderer wrappers and unrelated modules. Original DLLs and current logs are in `.deployment-backups/refit-launch-20261020-143921Z`. [Deployment receipt](refit-launch-20261020-143921Z-deployment.json) records every before/after hash; source/binary hashes and final evidence are under `build/refit-launch-20261020-141500Z`.

- RefitMenus SHA-256: `578770299b0c997a94726a3f03930282c74f76a5b46e630c7761dca22dadd0fa`.
- Scene provider SHA-256: `a718ed5907bab1d455601677f2d77b67f7f0dd611c50430abe13b83204f724cc`.

Manual acceptance: fully restart, visit Single Player campaign fleets (including Klingon decloak), then IA -> Launch. Check that IA controls fade into the camera pan, the loading handoff does not restart the pan, fleets appear without loading hitches, and gameplay arrives. Native visible smoothness and the exact native close timing remain pending this test.

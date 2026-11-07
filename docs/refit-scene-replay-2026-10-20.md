# Refit scene replay continuity

The user accepted the continuous handover as substantially better, with residual jumps at menu changes and launch. The next run is preserved under `build/refit-jump-20261020-133256Z/before/`. It shows 24 background handover frames over 1,277 ms, an IA visible-show replay at 550 ms after registration, first normal root paint at 853 ms, and loading lasting 14,596 ms with 38 callbacks/presents in the first three seconds and a longest gap of 648 ms. The old bugreport is dated October 19; it is not evidence of a new transition crash.

## Changes

ShellDisplay previously replayed the bitmap captured near native dialog registration when later show/resize events cleared the window. The worker could have advanced the camera for hundreds of milliseconds since that image was captured. The new optional `A2FO_SetShellCanvasRefreshV1` callback obtains the latest immutable CPU snapshot immediately before those early/visible replays. Refresh paints into a temporary memory surface and commits only after successful callback completion and restored GDI state. A refused or throwing callback preserves the existing valid bitmap. RefitMenus registers its existing snapshot copier with this optional export and unregisters both callbacks on owning-thread shutdown.

The original SeedV1 once-per-allocation contract is preserved. Without the optional registration, existing clients keep their cached replay behavior. Hidden/child windows and roots with a successful normal paint are excluded. There are no additional GPU draws, device resets, input synthesis or window activation operations.

The first IA layout used to capture its scene image, compose the complete UI while the handover continued, then stop handover just before publishing the layout. It could therefore publish an older camera pose over newer background frames. It now completes native widget/geometry preparation, stops the short background copy, then captures the image and composes the completed UI. GPU rendering/readback remains asynchronous.

## Verification

The lifecycle regression uses changing scene colours across registration, early erase, visible show and resize. It checks that seed calls remain once per allocation, successful refreshes publish current pixels, callback refusal/throw keeps previous pixels, and hidden/child/completed roots do not refresh. The setup native fixture simulates an older red image and a final green handover image; it requires handover completion before scene capture and verifies the completed layout contains the green image. A shutdown fixture checks that both seed and refresh registrations are cleared exactly once. All test windows remain hidden or simulated; no game entrypoint or user input is driven.

A camera-aspect adjustment was evaluated but excluded: the native loading backbuffer and its final display scaling require separate evidence before changing geometry. This candidate retains the installed resolution/projection policy. Remaining native asset-load presentation gaps and parity with Refit are not established by these tests. Full-restart native acceptance remains required.

## Installed candidate

Final verification passed 13 portable checks, six Wine canvas/setup/clock cases, 25 flow cases and 38 DLL console processes. The new refresh export was confirmed in the ShellDisplay DLL, and independent source review accepted the replay and shutdown cleanup. Existing scene-worker/GPU behavior is retained; this pass did not rerun the unchanged renderer's GPU fixture or claim new native FPS results.

With the game closed, RefitMenus and ShellDisplay were installed into `/home/tamsynn/NVMeData/Fleet Ops Roots`. All 61 present protected files retained their hashes. Original DLLs and current logs are backed up under `.deployment-backups/refit-jump-20261020-134611Z`. [Deployment receipt](refit-jump-20261020-134611Z-deployment.json) contains before/after hashes; final source/binary hashes and test logs are in `build/refit-jump-20261020-133256Z`.

- RefitMenus SHA-256: `b28fc538a359598f5d228cf15bb0d10f136ba05a380f14b841e576443cab898b`.
- ShellDisplay SHA-256: `a32cea71464b47f8229615180257fe35b4dc47140ede4d64f5e0690fe7d09f24`.

The next manual check is a full restart, switching main -> IA -> main -> IA, then launching a match. Check for backward camera snaps around native show/resize and the first completed IA layout. Native loading still recorded a 648 ms callback gap in the preserved run; this candidate does not claim that native asset-load stalls are eliminated.

# Continuous Instant Action scene handover

The native report was correct: the previous candidate requested the IA camera only after the native IA dialog had loaded. The latest preserved run shows its visible-show replay at 1,315 ms and first ordinary paint at 2,087 ms. A retained bitmap and a UI timer cannot animate while native construction blocks that thread. Evidence and original installed binaries/configuration are preserved under `build/refit-continuous-20261020-123610Z/before/`.

## Implementation

The checked main-menu Instant Action business action now requests the authored IA camera before closing the main form. A 16 ms native timer waits for the first successfully presented camera frame, with a two-second preparation fallback, then dispatches the original action exactly once. It does not wait for the complete pan or synthesize input.

The existing scene worker continues presenting completed CPU images during the native construction gap. It uses the existing source form, then the verified persistent game parent, then the admitted IA root. It creates no presentation window and does not change visibility, focus, activation, position or input. GDI copies use a short lock; GPU rendering, readback, asset loading and SDK calls stay outside it. The worker retains its existing device/cache and the one-frame-ahead animation clock.

A ten-second lease bounds this handover. Each target must retain a generation property, the original process/thread and its parent relationship. Native destruction removes the property, so a reused HWND cannot inherit admission. Destination replacement removes the earlier property. Completion serializes with any short copy and clears the identities before the completed IA UI is copied. Loading startup also ends any handover. Failed setup presentation and exceptions stop it.

The existing main-menu background shader now uses an SSE2 path for native-size CPU pixels where available. It preserves integer colour results and scalar fallback, including odd widths. Native resolution and installed renderer/settings are retained. Campaign selection and future BZN scene storage are unchanged.

## Verification and limits

The final isolated scene fixture passed with 31 background frames over a simulated 1,023 ms blocked UI interval. It checked the camera request before native close, source destruction, continued parent presentation without UI pumping, destination replacement, rejected unmarked windows, cleared identities and no late copies after completion. Its pixel sink is a private memory DC: this proves continuous worker production/copying, not the visible native parent or live window GDI cost.

The same fixture retained one renderer through main, IA, custom named campaign views, return, resizing, departure and tunnel. It completed 90 departure frames in three seconds, with a longest sampled gap of 62 ms; the largest UI exchange was 1 ms. These are isolated headless results, not native FPS or proof of parity with Refit. The separate D3D8 loading fixture passed image drawing, overlay order, state restoration and resource teardown.

The live `GetParent(main form) == ParentWindow` relationship remains guarded rather than assumed. A mismatch logs the source, expected parent and observed parent, then forwards the native action normally. Native acceptance requires a full restart, main -> IA -> return -> IA -> launch, observing whether camera movement covers the former blank interval and whether departure/tunnel motion remains fluid. The user has not yet accepted this candidate in-game. No desktop input or focus automation was used.

## Installed candidate

All 13 portable menu/loading checks, seven Wine presentation/content/performance/clock cases, 25 flow cases and 38 DLL console processes passed. Independent review passed the portable handover test and MinGW syntax check, with no material blocker. A minor lease-boundary cleanup edge can leave a property on an expired candidate until its HWND is destroyed; the expired lease prevents painting.

The native-size synthetic menu benchmark used the same Wine prefix, resources and fixture for both paths: scalar median 7,650 us, SSE2 median 5,755 us, with identical complete-frame RGB hashes. The odd 257x127 case and all 25,856 quotient combinations passed. These timings measure the fixture, not native game FPS.

With the game confirmed closed on the host, only `Data/modules/A2FORefitMenus.dll` was replaced in `/home/tamsynn/NVMeData/Fleet Ops Roots`. Its SHA-256 is `7f44fe72f5a5323d20589aa25608ca2b3cd8ab6253102d180772932450bbf576`. All 62 present protected files retained their hashes, including ShellDisplay, provider, scene recipe, settings, renderer wrappers and unrelated modules. Original DLL and fresh logs are in `.deployment-backups/refit-continuous-20261020-131033Z`.

[Deployment receipt](refit-continuous-20261020-131033Z-deployment.json) records before/after hashes; source/binary hashes and final test evidence are under `build/refit-continuous-20261020-123610Z`. Full-restart in-game acceptance remains pending.

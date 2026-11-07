# IA and accepted-launch continuity

The user confirmed improved performance but still reported a short main → IA flicker, a longer Launch flicker and a departure hitch. The previous installed DLLs, settings and fresh logs are preserved in `build/refit-handoff-20261020-172401Z/before`.

## Evidence

The new native run measured a 5,957 ms first scene and IA painting samples of 17–25 ms after the initial frame. Main → IA handover drew 40 background frames over 2,521 ms. Native IA loading then reached gameplay, but no Launch handover or continuing-departure marker appeared. The previous synchronous mouse-release close guard therefore did not cover the observed loading path; the exact asynchronous message order was not logged.

Loading recorded 38 callbacks/presents in the first three seconds, a 1,309 ms longest gap and 30,308 ms total loading. Warp-specific shaders/declarations and the flash sprite were still materialized lazily. The flash texture itself took under 2 ms, so it alone cannot explain that gap.

## Changes

The last successfully painted, admitted IA session is retained with unique properties on its source and parent. At the already verified native splash entry, a verified Instant Action transport flag can consume that UI image and start the existing fade/departure before native splash construction. This covers accepted launches whose IA window closes asynchronously, including a recently destroyed source. It does not infer acceptance from mouse release, a timeout or a generic hide.

A destroyed/closed source has a three-second lease. The parent must remain the matching same-process, same-thread native window; a reused source handle, changed property, expired lease or newer IA candidate rejects the old image. Recognizing a newer IA candidate clears prior retention even if the newer dialog later fails admission. The cached session is bounded to one and is consumed once. Existing synchronous handover remains available. No business action or input is generated or replayed.

The admitted IA window restores its completed UI, or the current retained scene during handover, for background erases. A temporary subclass on the validated persistent parent restores the current fade/scene for erases while native windows and splash resources change. It is removed at handover completion or parent destruction. Its state lookup never waits on an in-flight worker GDI draw; an allocation failure retains native erase behavior. Erase presentation does not advance the animation clock.

The host-only renderer prepares the four warp shaders, two declarations and flash texture during the first normal menu sky pass, using its existing caches. It does not bind, draw, enter departure or advance camera time. Device reset/release clears the preparation marker. A declaration accessor shares the existing creation/cache path with binding.

The previous direct-DIB compositor, complete-frame output, all-fleet preload, Klingon decloak, native resolution and user scene/settings are preserved.

## Verification and acceptance

The pinned offline MSVC scene build and strict MinGW module/fixture builds passed. The console regression run passed eight presentation checks, 25 loading/provider/campaign flow checks, 38 DLL console checks and 13 portable policy checks. The accepted-loading fixture covers asynchronous IA destruction, one-shot consumption, changed parent identity, expired lease, a newer candidate failing admission, native transport gating and abort cleanup. The portable handover test checks nonblocking state access while a synthetic copy is in flight. A separate read-only reviewer found no concrete lifecycle blocker.

All evidence is under `build/refit-handoff-20261020-172401Z`. Two early scene fixture attempts failed: one required four copies within 400 ms of Launch, before observing the first completed departure frame; the other failed the source-camera assertion without enough diagnostic state to explain it. The unchanged production binaries then passed the complete original fixture. The fixture now reports startup latency separately, bounds the wait for four departure/retained copies to two seconds, and independently requires continued copies over the following 400 ms. Source-camera failures now print the identities and views; that assertion remains in place. These failures are preserved as `scene-first-attempt.log` and `scene-second-attempt.log`; the original-fixture pass is `scene-third-original-fixture-pass.log`.

The final instrumented GPU fixture passed (`scene-final.log`): four retained/departure copies within 156 ms after native source destruction, continued copies afterward, 45 completed departure frames in the first 1.6 seconds and 87 in three seconds, with a longest sampled gap of 53 ms. The loading resize plus first completed departure frame took 159 ms. Maximum UI exchange was 3 ms. Initial all-fleet scene preparation remained 5,902 ms in this run; earlier cold attempts took 9,187 and 11,030 ms, so startup performance is still variable. Warp preparation occurred once (42.3 ms), before departure, with all four shaders, two declarations and the sprite ready. All 15 fleet model prewarm markers were present. The separate D3D8 loading fixture passed image presentation, native overlay order, state restoration and resource teardown (`loading-final.log`).

Installed only `Data/modules/A2FORefitMenus.dll` and `Data/modules/refit/A2FORefitScene.dll` under `/home/tamsynn/NVMeData/Fleet Ops Roots`. The original deployment plan stopped before writing because FeaturePack changed meanwhile. The refreshed plan preserved that update and verified all 62 present protected files unchanged. Receipt: [refit-handoff-20261020-235237Z-deployment.json](refit-handoff-20261020-235237Z-deployment.json). Rollback backup: `.deployment-backups/refit-handoff-20261020-235237Z`.

| Installed DLL | SHA-256 |
| --- | --- |
| A2FORefitMenus.dll | `ef20434ff26078df7625898e520911137de435954417b73611d3b0844a944c73` |
| A2FORefitScene.dll | `1959fb7b3a4c9d8fd68a696212addb78711fa57d54fca4103e945b6c04e76fd2` |

 Native visible acceptance remains a full restart and main → IA → Launch test. Engine asset loading can still block its presentation callbacks; resource prewarming and these handoffs do not establish that the measured 1.3-second gap has disappeared.

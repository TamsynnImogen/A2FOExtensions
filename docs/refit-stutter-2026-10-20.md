# Refit menu stutter: native timing and presentation work

The user confirmed that the previous decoder optimization left an 8–10 second background startup and visible pauses/stutter during menu and launch transitions. This pass uses a fresh preserved native run in `build/refit-stutter-20261020-155836Z/before`.

## Native evidence

The installed candidate hashes matched the previous deployment. Its first main render took 7,372 ms: backend preparation 68 ms, provider submission/preparation 7,212 ms, GPU completion/readback 92 ms. Subsequent frames included 49 ms with 39 ms in readback. The first `fent` upload took 1,724.2 ms; that timer includes driver/GPU effects and cannot be called CPU decoding.

Main → IA handover produced 44 background frames over 2,747 ms. IA presentation samples took 71–162 ms, including 17–24 ms native snapshots and 48–109 ms painting. Loading recorded only 35 callbacks/presents during the first three seconds, with a 1,297 ms longest gap and 29,831 ms total loading. The native sample has no matched IA Launch-overlay marker; its exact close dispatch remains unverified.

The reference renders into the game GPU/backbuffer path. This adaptation uses an offscreen D3D9 worker, synchronous readback and GDI presentation; those extra transfers remain an architectural difference. No claim of Refit-equivalent visible smoothness follows from headless FPS.

## Changes

The animation timer now distinguishes published pictures from status changes. A first-frame loading flag cannot repeatedly invalidate an unchanged fallback at 16 ms intervals. A retained picture belonging to an older view, size, epoch or consumed render ticket cannot drive another redraw loop either. The picture and animation acknowledgement contracts are unchanged; a failed presentation still leaves its matching current frame pending.

The IA painter opts into direct compositing only for its creator-owned private top-down 32-bit DIB. Cached premultiplied PNG tiles can blend into those pixels with a runtime-guarded SSE2 implementation and scalar fallback. An RAII scope restores prior target registration across nesting. The path checks selected bitmap, pointer, dimensions, stride, format, clip and identity DC state and flushes queued GDI writes before CPU access. The full-screen solid tint also uses the registered-DIB path, with one combined rounded color product and source alpha zero to match constant-alpha GDI semantics. Unsupported DCs retain the existing AlphaBlend path. It does not infer row order from GetObject: Wine normalizes the reported height sign.

Power-of-two normal-map generation now uses masked neighbour coordinates and hoisted row offsets instead of repeated signed remainder operations. Mips reuse the existing exact box filter. All fleets and normal maps are still prepared eagerly, and authored cloak, scene settings and native resolution remain unchanged.

## Verification

The initial normal-map profile found 19 maps: 159.8 ms image read/decode, 1,106.8 ms gradient/mips and 215.7 ms upload. The optimized GPU profile measured 860.1 ms gradient/mips for the same 19 maps; separate serial runs are sensitive to system load. The host harness matches 179 complete mip chains, including edge wrapping, seeded inputs and 2×N/N×2 sizes. Independent review reproduced equivalence and a 1.65× 1024² host speedup; this is not a native startup measurement.

The scalar and SSE2 tile implementations matched actual AlphaBlend for 576 RGBA cases plus unsupported-DC and nested-scope checks. Constant-alpha tint matched 3,072 RGBA cases across all 256 alpha values, four colors and three background patterns. A 1920×1080 tile benchmark measured 8.020 ms AlphaBlend versus 1.722 ms direct blending. The complete hidden IA fixture in one process measured 24.468 ms GDI painting versus 16.910 ms direct painting. With both tile and tint paths integrated, the final same-process fixture measured 28.787 ms GDI painting versus 8.409 ms direct painting; whole composed frames matched byte-for-byte at 1920×1080 and 1280×720 with both 800- and 1200-wide native source canvases. The scalar path remains available for CPUs without SSE2.

Final build and all 13 portable checks passed. Eight Wine canvas/setup/compositor/render/clock cases, 25 loading/provider/campaign cases and 38 DLL console processes passed. Separate complete-frame equality and IA benchmark checks passed. The GPU fixture passed main/IA/campaign views, Launch fade, same-epoch loading continuation, one retained backend and restart behavior; it produced 86 departure frames in three seconds, maximum sampled gap 109 ms, maximum UI exchange 3 ms. These measurements do not show an improved warp frame rate relative to the prior isolated test. All 15 distinct fleet models prepared exactly once. The separate D3D8 loading fixture also passed image, overlay ordering, resource teardown and graphics-state restoration. Native visible acceptance requires a full restart and manual main → IA → Launch test; no game entrypoint or desktop input automation was used.

## Installed candidate

Only `Data/modules/A2FORefitMenus.dll` and `Data/modules/refit/A2FORefitScene.dll` were replaced with the game closed. All 61 present protected files retained their hashes, including scene recipes/settings, ShellDisplay, engine and renderer wrappers and unrelated modules. Original DLLs and logs are preserved in `.deployment-backups/refit-stutter-20261020-171731Z`. [Deployment receipt](refit-stutter-20261020-171731Z-deployment.json) records before/after hashes.

- RefitMenus SHA-256: `9d99d038f3391bc1406b6bb4edac127c64b6b7a2d85bee9f6411207f45d5971a`.
- Scene provider SHA-256: `de470c616dfd0cd5dd293a9ef0b904e332c80f7d30403aa87a4090d147e730a8`.

Frozen implementation, fixture and binary hashes are retained in the evidence directory. The graphics fixture's first attempt caught its old status-revision assertion after picture/status revisions were separated; that assertion now checks the picture revision, and the full fixture passed after rebuilding. No renderer logic was changed to bypass that check.

Manual acceptance remains: fully restart, measure time from main menu appearance to the 3D background, then test main → IA → Launch for both response delay and camera/fleet/tunnel cadence. Whole-frame pixel equality and isolated timing gains do not establish native visible smoothness. Native engine loading can still block its presentation callbacks; this pass does not move engine D3D8 calls to another thread.

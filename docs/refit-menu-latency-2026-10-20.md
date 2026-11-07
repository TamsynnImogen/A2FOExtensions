# Menu startup and IA latency

The user reported 8–10 seconds before the all-fleet background appeared, followed by both pauses and choppy movement when entering IA and launching. All fleets remain preloaded, authored Klingon decloak remains enabled, and native resolution and scene settings are preserved.

## Changes

DDS decoding now directly copies canonical BGRA rows and uses an exact per-image component lookup for large 16-bit images. Pitch, masks, alpha handling, dimensions and payload validation retain their existing checks. Other formats use the existing path. Mip generation now averages four pixels using independent packed integer lanes, retaining exact channel rounding and odd-dimension behavior.

IA still forwards each genuine pointer event once to the native handler. Hover redraws now invalidate the window for USER32 to coalesce, rather than unconditionally rebuilding the entire UI synchronously after every mouse move. Native canvas leases nested in a real hover dispatch also defer recomposition while an active UI is already visible; the outer dispatch invalidates again after native processing. Buttons, keys and commands retain immediate presentation. No input is synthesized and no native action is replaced.

Bounded logs distinguish backend preparation, provider draw submission, GPU completion/readback and CPU frame copying. IA logs separately measure native snapshot, scene acquisition, UI painting and final presentation. These diagnostics do not establish which stage dominates the installed game until a new native run exercises IA and Launch.

## Evidence

Evidence is retained in `build/refit-latency-20261020-145522Z`, including pre-change installed logs/DLLs and private fixture logs. The supplied native log showed a 6,650 ms first main frame and subsequent view frames around 31–62 ms, but contained no IA/Launch handover or loading-cadence markers. It cannot establish the exact cause of the reported IA/Launch stalls.

Serial isolated startup profiles measured texture read/decode totals of 2,280.6 → 576.7 ms. Full first-frame time was 7,264 → 6,528 ms in that pair. Driver/cache startup varies substantially: the later complete fixture began in 3,295 ms. These are isolated profiles, not a controlled claim about native game startup.

The hidden 1920×1080 IA metadata fixture measured a median 1.004 ms snapshot and 25.000 ms paint+flush over 32 frames. A bounded font-cache experiment showed no measured improvement and was discarded. No quality or fleet-preload reduction was used.

All 65,536 packed values match the original decoder for RGB565, ARGB1555 and ARGB4444. Tests also cover padded/odd rows. The mip harness matches original pixels across 1,189 varied sizes and the actual 3072×1024/768×768 cases. All 25 decoder unit tests and eight stock tests passed. The pinned 32-bit Windows provider build passed with its existing ten warnings.

The complete isolated GPU fixture passed retained device/cache, IA pre-close handover, Launch UI fade, shared departure/loading epoch, Klingon/Borg views, named campaigns and restart cases. It produced 87 departure frames over three seconds, longest sampled gap 62 ms; maximum UI exchange was 4 ms. All 15 scene models prepared exactly once. The separate D3D8 loading fixture passed image presentation, native overlay order, state restoration and teardown. Independent read-only review found no blocker.

## Installation and acceptance

Final verification passed 13 portable checks, six Wine canvas/setup/clock cases, 25 loading/provider/campaign flow cases and 38 DLL console processes. The new hidden fixture assertion confirms guarded native hover canvas leases do not request another scene or compose another UI. The separate scene and D3D8 loading graphics checks passed. Final source and binary hashes are recorded in the evidence directory.

Only `Data/modules/A2FORefitMenus.dll` and `Data/modules/refit/A2FORefitScene.dll` were installed with the game closed. All 61 present protected files retained their hashes. Original DLLs and logs are preserved in `.deployment-backups/refit-latency-20261020-154447Z`. [Deployment receipt](refit-latency-20261020-154447Z-deployment.json) records before/after hashes and protected files.

- RefitMenus SHA-256: `0c9dd481040e3b4883d2a8f0897cd936a3b83cc7169ed99d97c83b3b8be3e3e9`.
- Scene provider SHA-256: `7cac25474e06b8ba2cf001f7b8f10cd674b28be9093424519be5a31b862d1163`.

Native visible smoothness remains pending a complete game restart and manual main → IA → Launch test, checking both the delay before movement and movement cadence. All-fleet preload and authored decloak remain enabled. No game entrypoint, input or focus automation was used.

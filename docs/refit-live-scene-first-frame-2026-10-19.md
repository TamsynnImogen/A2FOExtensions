# LiveScene first-frame delay — 19 October 2026

The user reported that `LiveScene=1` left installed artwork visible. The fresh
game log confirms the intended config and new worker at 1920×1080. The provider
was in first-use texture loading, with no recorded completed frame. The user
confirmed exiting before leaving the menu open for a minute; this attempt does
not establish a failed renderer or an ignored switch.

## Root cause and correction

An instrumented source copy under `build/refit-cold-profile-source/` reproduces
the old helper without changing installed files. In the isolated real-D3D test,
Main takes **43,116 ms**, including **42,022.8 ms in the ship pass**. Individual
texture loads include `gal_saucer` 3,930.7 ms, `ak1_glow` 7,911.6 ms and `neb`
7,967.1 ms. Sky, planet and model/shadow preparation take about 37, 74 and 81 ms.
This identifies texture preparation as the dominant measured cost; blaming the
whole delay on graphics-driver or shader compilation was unsupported.

The custom DDS decoder used 64-bit integer division for every RGB(A) channel.
On this 32-bit no-CRT target that invokes `rt/mem.rs`'s software shift/subtract
division helper. Eight-bit channel masks now return the extracted byte directly;
smaller masks use a proven bounded 32-bit numerator/division. Masks wider than
24 bits retain the original 64-bit formula. No global division helper, ABI,
engine address, renderer feature or texture pixel semantics is changed.

`assets::load_texture` also caches the base width after successfully decoding the
base. Mip lookup therefore avoids decoding that image again. An initial metadata
shortcut for candidate mips was rejected during review: an invalid `_1` must not
hide a valid bare `1` sibling. Final candidate selection retains full decoding
and validation. The parser's new header-only `dimensions` API is available for
tools, but is not used to bypass runtime candidate validation.

## Evidence and limits

The first optimized headless run prepares Main in **3,638 ms** at 1920×1080,
passes main-form destruction/return without recreating the backend, switches
faction frames and shuts down its worker correctly. The final compatibility
preserving rebuild is checked separately before deployment. This is provider /
worker evidence using actual DXVK/NVIDIA, loose Data assets, a private Wine prefix
and a private headless Gamescope compositor. It is not a live Fleet Ops test.
No user input or desktop window is controlled.

The 22 parser unit tests pass and compare channel output against the original formula across
contiguous masks and offsets, including all channel values for up to 10 bits,
plus larger-mask boundary/interior values. Existing DDS format, pitch, truncation,
mask and image tests remain in force. Eight optional stock-suite entries report
success with no configured `A2_GAME`; their skip guards mean this is not a
verified stock-install sweep. The real GPU test separately uses installed Fleet
Ops assets. Header dimensions do not claim that a
payload is decodable. The provider ABI/thread/null-device checks remain separate.

Klingon placement/visibility, missing Borg textures, non-power-of-two texture
support and the full Instant Action layout are not fixed by this performance
change. Actual menu startup/presentation still needs a full restart and user
acceptance.

The final compatibility-preserving provider prepared Main in **3,461 ms**, with
maximum measured menu-side frame exchange 9 ms, and passed all frame/cache/
shutdown checks. One earlier attempt failed creating its fixture window while
console and graphics checks shared a Wine prefix concurrently. Running those
checks sequentially resolved the test display conflict. This was isolated to
the test prefix; no game prefix or desktop was controlled.

## Deployment and restart

Installed at local **01:19:39**:
[receipt](refit-fast-dds-20261019-011939-deployment.json).
Only `Data/modules/refit/A2FORefitScene.dll` changed. Its SHA-256 is
`f817596a3e7205cf7d2acb2b36055015b5cc58a52ed5fba04218051dd1261562`.
All 18 protected files remain unchanged, including the menu host DLL,
`LiveScene`/resolution configuration, Render3D profile, ShellDisplay, executables,
module lists and licence notices. Logs and prior helper are preserved under
`.deployment-backups/refit-fast-dds-20261019-011939/`.

Fully quit/restart Fleet Ops with `LiveScene=1`; installed artwork is still the
fallback while the first scene prepares. The fresh extension log should now
reach `Refit worker timing: view=main` within a much shorter preparation period.
User-observed appearance and responsiveness remain the acceptance check. With
the game closed, restore the backup's helper to the matching live path to roll
back this update, retaining attribution.

Evidence: `build/refit-cold-profile-{build,gpu}.log`,
`build/refit-cold-profile-root/RefitScene/A2FORefitScene.log`,
`build/refit-fast-dds-{parser-tests,provider,gpu}.log` and
`build/refit-dds-cold-load-build.log`.

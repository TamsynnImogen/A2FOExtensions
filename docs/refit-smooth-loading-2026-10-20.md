# Refit smooth loading and initial IA handover correction

The user supplied a native loading screenshot showing 2.9 FPS, a clipped player
panel, an empty preview, and reported that the camera still skipped most of its
pan and that entering Instant Action still flashed black. The new run's logs,
DLLs/configs and screenshot were preserved before changes under
`build/refit-smooth-20261020-102750Z/`.

## Evidence from the reported run

- The renderer retained one device across resolution changes, but its loading
  schedule still imposed a minimum 67 ms idle after each completed frame and
  up to 3x render cost. That caps completion below 15 Hz before asset stalls.
- Native loading reported 19 normal and 132 extra frames, costing 5,034 ms in
  extra-frame API time. The cache was **not** active: Fleet Ops had already
  transferred native device cleanup to its own callback. Three of four asset
  sites were observed because Fleet Ops also replaced the pixel-loading call.
  Earlier stock-image checks missed these live prepatches.
- At the actual 1280x800 loading target, the ODF tip reserves approximately
  x=457..977 down the screen. The old player panel began at x=923, inside that
  protected region; excluding the native tip clipped its left edge. The title
  also intersected native tip/sprite space.
- `bzn/first.png` is a real 160x160 map thumbnail. The old reader looked only
  for BMP and fell back to a 15x15 setup-canvas crop, producing the empty box.
- The IA backing canvas was seeded, but that did not paint its physical window
  before the first native paint completed.

## Changes

The loading world and UI now use separate GPU textures. The CPU paints only an
ARGB UI layer at 100 ms intervals; the GPU scales/shades the world and blends
the UI. Independent image revisions avoid redundant uploads, and both caches
are released before native device cleanup and splash teardown. Both textures
are prepared before drawing either layer; preparation failure leaves target
pixels untouched. A device draw failure after drawing starts cannot be rolled
back atomically. Missing cleanup admission uses transient layers instead.

Loading choreography targets 30 Hz with at least 4 ms yielding for slow frames.
The idle menu's conservative budget is retained. Departure starts at the first
admitted loading draw, preserving the pan while native splash preparation is
running. No hold loop or extra input pump delays game loading. New bounded logs
record callback/present counts in the first three seconds and the longest gap,
so the next native run can distinguish source intent from live performance.

The device observer now preserves the checked Fleet Ops cleanup transfer and
its existing A2FO core renderer observer. The pixel observer preserves the
checked Fleet Ops thiscall wrapper, including its return value; the normal
loader therefore exposes all four asset callback sites. Foreign transfers and
changed signatures retain fallback behavior. Addresses and native instruction
shapes were checked against the installed ArmadaL `817b1ddb...` and Fleet Ops
`3fd18eee...` binaries, not inferred from unrelated upstream labels.

The layout routes its title, preview, player rows and status around native
protected rectangles. Player names and faction details use separate lines;
eight rows fit beside the standard tip. The observed native race ID
`klingonempire` is added as a data-authored Klingon theme alias. Existing team
number mapping is preserved because the native numbering basis is not proven.
PNG, BMP and JPEG thumbnails are searched within each root before trying a
lower-priority root; file size/dimensions/pixel counts are bounded.

ShellDisplay paints retained scene pixels physically before native registration
can initialize assets, again after fitting the root, and during the initial
background erase. It resumes native erase ownership once a normal root paint
has completed. Failed/absent seeds and child windows keep native handling.
The callback reads only a CPU snapshot and starts no GPU work.

## Validation and limits

The private GPU worker test retained one backend through 1920x1080 -> 1280x800
-> 1920x1080 -> 1280x800, with 13 named views and repeated departure. It published
32 frames in 1.6 seconds and 74 in three seconds, including 566 ms of initial
resize/preparation. It produced 180 sampled departure frames, with a longest
sampled gap of 55 ms and maximum UI exchange of 17 ms. This is a private
headless measurement; it does not establish the native game's FPS.

The layered D3D8 test exercises real scaling, alpha, shading, independent cache
revisions, state restoration, transient fallback, and release to the device
reference baseline. It also exercises the production `present()`/`extra_frame()`
path with separate scene/UI caches. The GDI render fixtures cover the actual
1280x800 target, native tip/sprite/bar obstacles, eight player rows and campaign
objectives. The image fixture reads the authoritative installed `first.png` at
160x160 and covers root precedence and invalid-file fallback.

Console fixtures replay the checked Fleet Ops/core cleanup chain as data,
verify foreign/refused patch fallback, and preserve exact thiscall arguments,
return values and one-time native forwarding. They do not invoke the game's
entrypoint. The canvas fixture checks physical pixels before native registration
and before first paint, final-size repaint, erase ownership, and refused seed
fallback. No user input devices or desktop windows were automated.

Native acceptance requires a full restart and main menu -> IA -> launch, then
return and repeat. Check the camera pan, ship departure, preview, full player
names/factions/teams, and gameplay arrival. New logs should show verified
Fleet Ops/core cleanup chaining, caching, 4/4 asset sites, a 160x160 preview,
early physical seed timing and loading cadence. Whole-game performance,
pre-creation window timing, campaign/save loading and multiplayer acceptance
remain manual. Do not equate these fixtures with native game completion.

## Final checks and deployment

The final build passed 12 portable checks, 38 DLL console fixture processes,
25 loading/provider/campaign flow cases, seven Wine canvas/content/render
fixtures, and the private layered-GPU and scene-worker checks. The final canvas
run includes physical-pixel assertions before native registration and after
window fitting, with one seed callback per newly allocated backing canvas.
Source and binary hashes are recorded in
`build/refit-smooth-20261020-102750Z/final-source-binary-hashes.json`.

Installed into `/home/tamsynn/NVMeData/Fleet Ops Roots` with the game closed:

- `Data/modules/A2FORefitMenus.dll`: SHA-256
  `574939191a291ef9c4c14dd8f87d4389f58e7e0a88b8967959fa65ce299b44a6`.
- `Data/modules/A2FOShellDisplay.dll`: SHA-256
  `942d11546651a6ea3920d123de36eaa6e719f1c2dd105c9814b183af1b11971b`.
- `Data/RefitScene/Loading/themes.ini`: only the `klingonempire` alias was added.

All 59 present protected files retained their hashes, including the scene
provider, settings, engine, renderer wrappers and unrelated modules. Original
files and live logs are preserved in
`.deployment-backups/refit-smooth-20261020-111142Z/`. The complete receipt is
[refit-smooth-20261020-111142Z-deployment.json](refit-smooth-20261020-111142Z-deployment.json).
Native visual acceptance remains pending a full restart.

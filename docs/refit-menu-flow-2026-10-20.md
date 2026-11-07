# Refit menu continuity, campaign views and warp loading

The current INI scene remains the authoring source. The host now uses named view
IDs instead of a five-entry faction array, and the expanded mission selector can
select those IDs without depending on its four borrowed native launch slots.
The existing retained graphics worker drives menu scenes and departure/loading;
it never borrows the game's device or runs GPU work on the shell thread.

## Authoring and the later BZN source

The selected `RefitScene/Menu/map.ini` supplies up to 128 `[View.<id>]` sections.
IDs and optional `Label` captions may be up to 128 bytes. IDs follow the
renderer's ASCII case folding. The catalogue rejects duplicate IDs or oversized
input rather than exposing ambiguous scene choices. Main-menu choices are
paged; adding campaigns does not require adding C++ rows. Restart to refresh the
host's catalogue after adding/removing views; the provider retains its existing
live editing of camera and scene properties.

```ini
; RefitScene/Menu/map.ini
[View.frontier_fleet]
Label=Frontier Fleet
Group=fleet
Eye=125,25,285
Target=0,-60,-120
Fov=40

; Active mod's mission_selector.ini
[campaign42]
title=Frontier Campaign
menuView=frontier_fleet

[campaign42.mission7]
file=frontier07.bzn
title=The Long Watch
description=Hold the frontier station.
objectives=Protect the station.\nEscort the transports.
menuView=frontier_fleet
```

A mission view overrides its campaign view. A custom campaign without a view
keeps the current scene. The four native campaigns retain their named defaults;
they are defaults, not catalogue indices or a limit on additional campaigns.
Unknown views or an absent/disabled renderer retain the selector's configured
static background. Existing launch-table bounds and native progression remain
unchanged.

The later BZN source should expose these same named view/group identities, with
editor camera/marker transforms supplying fleet placement and camera anchors.
Keep fleet anchors distinct from the viewing camera and optional target, and
independent of player starts or native campaign slots. The current work does not
claim a BZN camera decoder: typed records, orientation and an asymmetric native
editor round trip remain required by the
[BZN/Lua assessment](menu-bzn-lua-assessment-2026-10-19.md). The authoring source
can then change without changing the campaign-to-view bridge or GPU ownership.

## Presentation and lifecycle

`A2FOMissionSelector` resolves optional exports from an already-loaded
`A2FORefitMenus`. Its paint uses copied scene pixels with saved clipping/viewport
state; missing or refused scenes leave the existing static path available.
The renderer's existing timer invalidates the selector and its child overview
only when a new frame/state is published. There is no second selector timer.
Other recognized shell menus continue to composite native controls over the
retained, dimmed scene; their native layouts are not all redesigned here.

Before native `SetupMission`, the selector copies map/title/briefing/objectives
through the bounded POD interface in `sdk/include/a2fo_menu_scene.h` and requests
departure from the selected view. Rejected launches end departure and clear the
pending metadata. A successful handoff is consumed by one matching campaign
load; later same-basename loads cannot reuse it accidentally.

The existing native load-bar callback also requests departure, covering loads
without the optional selector bridge. The graphics worker calls
`Render3D_MenuWarp`: the camera follows the departing fleet, then the animated
tunnel overlays the scene. Each load has an epoch; late frames from a previous
view, size or departure are rejected. A same-view restart resets choreography
without drawing a second warm-up GPU frame. Main-menu return and the first
observed gameplay frame stop departure; device/cache ownership stays with the
worker.

Native loading callbacks renew a two-second rendering lease. A long blocked
engine step stops scheduling background GPU work after that lease; the next
callback wakes it. Animation is displayed at the engine's loading-frame cadence.
Native loading is not delayed to guarantee a complete cinematic, and no extra
native scene/present calls or foreign-thread device access are added. Very short
loads may finish before the full departure/tunnel becomes visible.

Campaigns show briefing/objectives; Instant Action and multiplayer show native
players and map preview. Mode comes from the checked transport flags, not the
display caption. Native ODF bar/tip regions remain protected. Faction theme
palettes/shapes remain data driven, including custom factions and neutral
fallback for unresolved races.

## Performance and verification boundary

The worker retains the previous adaptive budget: at least 67 ms of idle after
each completed frame, increasing to three times its render duration up to one
second. Loading CPU painting is limited to the same presentation cadence while
the original FO callback runs once on every invocation. Gradient panels avoid
per-row brush allocation, and image coordinate/shading inputs are calculated
per row/column. Compatible devices allocate an exact-size loading texture rather
than padding 1920x1080 to 2048x2048.

Loading GPU textures remain transient and are uploaded for each native callback.
This avoids retaining an old engine device across reset/destruction. CPU cadence
does not imply GPU-upload throttling; whole-game load time still needs measurement.

Evidence is retained under `build/refit-flow-20261020/`. Portable checks cover
catalogue bounds, arbitrary campaigns, mode layout and stale-frame/reload
rejection. Console fixtures check image admission, optional export guards,
native forwarding, metadata copies and hook-chain fallback. The isolated GPU
fixture exercises 13 views, a name longer than 32 bytes, real departure/tunnel
frames, lease suspension/resume, same-map restart and one retained backend.
These are separate from manual full-restart game acceptance.

Manual acceptance must cover campaign selection and launch, IA with a random
race, multiplayer progress, same-map reload, return to menus, real ODF tips/bar
artwork, and windowed/borderless modes. Verify the active mod's actual asset
chain: the Data-only fixture does not establish every installed mod's appearance.

## Verified build and deployment

The host-only texture loader also admitted installed non-power-of-two artwork
previously rejected before upload. The final GPU log confirms 768x768 Borg
textures with their complete ten-level mip chains; captures show the formerly
black diamond now textured. Successful device texture creation remains required;
the upstream non-host renderer retains its earlier power-of-two rule.
Two references, `pa_ecfr0` and `klingon_susa`, remain absent from this loose Data
asset set. No replacement proprietary artwork is fabricated or deployed.

Final checks passed: 12 portable suites, 38 real-DLL console processes across
the two reference FO profiles, seven additional component/provider/selector
checks, and actual headless D3D9 scene plus D3D8 loading fixtures. MinGW builds
use warnings as errors for the Refit path. The provider builds offline with the
pinned Rust 1.96.0 toolchain; its ten pre-existing host-build warnings remain.

| Headless sample | First main frame | Next reported main frame | Longest UI request |
|---|---:|---:|---:|
| Installed baseline | 3206 ms | 77 ms | 1 ms |
| Initial integration | 23342 ms | 115 ms | 16 ms |
| Integration repeat | 2360 ms | 63 ms | 12 ms |
| Final build, including NPOT artwork | 3463 ms | 77 ms | 16 ms |

These are separate process samples with existing OS/driver caches, not a
statistically controlled cold-start benchmark. The integration recipe adds eight
camera views for testing; final rendering also includes newly admitted textures.
The initial long sample is retained, not discarded. All use DXVK 2.6.2 on the
GTX 1660 Ti. Windows tick granularity makes the zero-millisecond faction readings
unsuitable for frame-rate claims.

The loading GPU fixture measured 800x600 upload/draw API calls at 16/20 ms and
1920x1080 at 47/28 ms, with native overlay visibility, full state restoration,
reset and resource teardown passing. These exclude CPU composition and are not
whole-game loading measurements. The earlier loading record reported 44/28 ms
at 1920x1080; these sparse samples do not establish a speedup.

Installed three files in `/home/tamsynn/NVMeData/Fleet Ops Roots`:
`Data/modules/A2FORefitMenus.dll`, `Data/modules/refit/A2FORefitScene.dll`, and
`Data/modules/A2FOMissionSelector.dll`. Fifteen existing protected files retained
their hashes, including the executable, core/ShellDisplay DLLs, wrappers, module
lists, scene INI, render profile and loading themes.

Receipt: [refit-flow-20261020-093159Z-deployment.json](refit-flow-20261020-093159Z-deployment.json).
Backup: `.deployment-backups/refit-flow-20261020-093159Z/` includes the three old
DLLs and available current logs. To roll back with the game closed, copy those
three relative `Data/modules/` files back into the same install. No INI, renderer
profile, artwork, native executable or input settings need restoring.
Native full-restart acceptance remains pending.

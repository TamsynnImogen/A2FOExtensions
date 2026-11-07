# Refit-style loading screen with native Fleet Ops overlays

**20 October update:** [named campaign views and animated loading](refit-menu-flow-2026-10-20.md)
supersede the snapshot-only implementation described below. Earlier checks and
deployment receipts remain historical evidence.

The loading presentation is part of `A2FORefitMenus`. It adapts Refit's mission
column, title/accent rule, briefing/objectives, map preview and bottom percentage
to the existing menu fonts and colours. **Fleet Operations still draws its own
bar, localised tip text and optional tip sprite after the new image.** It also
keeps its loading music, engine progress, transport updates and normal presents.

## Target flow clarified on 20 October 2026

The user's observations of Refit establish the presentation target for the
conversion. These are requirements for fidelity, not a claim that the current
Fleet Ops port implements the complete sequence:

1. Menus appear as popup/overlay panels over the continuing main scene. Opening
   and closing a panel preserves the scene and camera continuity.
2. Launching a game pans the camera behind the fleet currently in focus. The
   fleet enters warp before the presentation moves into loading.
3. Loading has an animated warp background, with Instant Action player details
   and a minimap, or campaign mission objectives, as appropriate to the mode.
4. The loading bar and surrounding UI are styled as part of that presentation.
   Interpret the user's reference to the player's "team" as their playable
   faction/race for skin selection; retain alliance/team numbers separately.

The current port has retained menu scenes, an Instant Action overlay layout,
native loading player data, map previews, objective text and faction-selected
panel palettes/shapes. Other recognized menus still retain native widget
layouts. These pieces do not establish complete popup-menu fidelity.

Remaining work includes the launch camera/departure sequence, a continuing
animated warp loading scene, and visual comparison of mode-specific layouts,
faction artwork and the styled bar. The present retained-frame loading backdrop
is a snapshot. The bar has a modern track/frame around the native ODF-controlled
fill; that does not establish a match to Refit's bar artwork or animation.
Preserve native loading progress, tips, transport state and mod configuration
when extending the presentation. Manual game acceptance is still required.

The vendored renderer supports the departure description:
[`Render3D_MenuWarp`](../third_party/armada2-refit/menu-renderer/src/menu/mod.rs)
and its [departure documentation](../third_party/armada2-refit/menu-renderer/README.md)
describe continuing the selected view, staggered ship departures, the following
camera and a star tunnel during loading. That renderer subtree does not include
the original UI implementation; the mode-specific panels and faction/bar styling
above remain user-observed targets rather than verified original UI code here.

## Configuration

In the active extension root's `A2FORefitMenus.ini`:

```ini
[RefitMenus]
Enabled=1
LoadingScreen=1
```

Restart after changing it. `LoadingScreen=0`, a missing key, changed native
bindings, unsupported device or unavailable presentation leaves the original
Fleet Ops loading screen. `LiveScene` independently controls the menu renderer;
the loading presentation does not start a renderer or create another device.

The last completed animated-menu frame is retained as a shaded background when
available. Otherwise it uses the optional loose file
`RefitScene/Loading/background.bmp`, then a navy gradient. This milestone uses a
snapshot; Refit's animated warp departure, map-sky selection and extra frames
between engine load steps are not ported.

## Modding remains in loadscreen.odf

There is no duplicate load-bar configuration. These native FO fields retain
their original meanings, defaults, lookup rules and drawing implementation:

| Feature | Existing ODF keys |
|---|---|
| Bar colour/transparency interpolation | `loadbarColorStart`, `loadbarColorEnd`, `loadbarTransparencyStart`, `loadbarTransparencyEnd` |
| Bar dimensions and placement | `loadbarHeight`, `loadbarWidth`, `loadbarPositionX`, `loadbarPositionY` |
| Direction | `loadbarVertical`, `loadbarInvert` |
| Tip selection/content | `numberOfTips`, `tip0` through `tipN`, corresponding `tip0Sprite` etc.; indices start at zero |
| Tip layout | `tipPositionX`, `tipPositionY`, `tipWidth`, `tipSpriteX`, `tipSpriteY`, `tipSpriteSize` |

The new painter excludes these configured native overlay regions from its
foreground text/preview. The native drawing still happens last and remains
authoritative even with overlapping or unusual positions. Inverted/vertical
placement includes the original native coordinate conventions; this module does
not silently correct or replace them. No installed ODF or tip strings are changed.
The modern track/frame follows the full native ODF bar rectangle; its fill is
still drawn once by Fleet Ops with the configured colours and transparency.

Fleet Ops' existing `Sprites/fleetops.spr` supplies the named tip sprites and
their TGA atlas coordinates. No replacement parser overrides those definitions.
`misc/gui_interface_misc.cfg` also defines `loadingPlayerBackgroundPanel` and
`Sprites/gui_global.spr` defines its `drop_player_panel` artwork. The modern
player panel uses the Refit theme registry rather than stretching that native
popup art. Existing faction HUD cfg/spr files retain their original semantics;
the new panel does not pretend that HUD cfg fields configure this layout.

The installed Classic mod's loose `odf/other/loadscreen.odf` currently contains
only its cyan start/end colours. Enabling the new presentation does not invent
tips when the mod's native configuration supplies none.

## Content and lookup

The checked engine `GameSetup` getters provide the selected map name and title.
For optional content, SDK extension roots are searched from highest precedence
to lowest: selected mod, parent mods, Data. Only the active root chain is used.

- Briefing: `objectives/<map>_a.txt`, `_main.txt`, `_primary.txt`, or
  `_main_objectives.txt`; first nonempty line is the title, paragraphs are the
  briefing, `*` lines are objectives, and `Hint:` introduces a hint.
- Preview: `bzn/<map>.bmp`.
- Optional static background: `RefitScene/Loading/background.bmp`.

These optional reads support loose files. For Instant Action, the existing
native setup presenter also retains its rendered minimap, keyed to the native
map name, before launch. That copy is used only when no loose BMP exists, so
FPQ-only previews can follow Fleet Ops' own asset selection without a duplicate
archive loader. It uses copied pixels, never retained HWND or engine pointers.
The crop respects both 800×600 and 1200×900 authored widget coordinates. Direct
campaign/save loads without a setup preview still require a loose map BMP;
missing previews omit the image safely. File text retains
the game's ANSI encoding. BMPs may be uncompressed 8-bit indexed or 24/32-bit, within 3840×2160
and eight million pixels. Objective files are limited to 64 KiB. Map filenames
are sanitised before building paths. No BZN scene reader or campaign/faction
assumption is introduced here.

The native adapter now populates player rows through the verified GameSetup
getters: vacant/observer slots are excluded, names are copied as ANSI bytes,
and team/local/race values come from the setup. Returned MSVC strings use a
16-byte native result buffer and are destroyed through the engine's own
MSVCP60 destructor import; no game object is freed by MinGW. Transport virtuals
control whether per-player percentages exist and which slots are send-worthy.
Unsupported bindings omit the player panel. A random race stays unknown until
the engine resolves it; it is never silently labelled Federation.

The local resolved race chooses a palette and panel form from
`RefitScene/Loading/themes.ini`. Extension roots merge low to high precedence;
custom factions may define any `[Theme.<race-name>]`, with optional aliases.
Unknown/malformed faction definitions fall back to the shell theme. Campaign
loads use the mission race for the HUD, while Instant Action uses its current
setup race. The native load-step text is copied through a separately checked
observer and forwarded unchanged to the original status update. No fake
percentage or stage progression is added.

## Native integration and safety

The player UI adapter checks these additional Armada RVAs against the installed
image; the getters are reference hypotheses independently checked by their
instruction bodies and existing map symbols, not imported Rust ABI types:

| Binding | RVA / layout |
|---|---|
| Local slot | `0x147470` |
| Team / race index | `0x146610` / `0x146650` (RET4) |
| Vacant / game-player predicate | `0x146af0` / `0x146b90` |
| Player name | `0x1466e0`, hidden 16-byte string result then slot, RET8 |
| Race by IA slot | `0x8b180`, cdecl; internal name at `Race+0x14` |
| Native string destructor import | `0x3b7dc0`, checked MSVCP60 owner |
| Campaign interface race pointer | `0x337cc0` |
| Transport flags | vtable `+0x10` IA, `+0x18` single, `+0x40` show percentages |
| Per-slot percentage / send policy | vtable `+0x44` float result, `+0x5c` bool result |
| Status observer CALL | `0x2019eb` to `0x198ba0`, cdecl `const char*` |

Transport virtual targets must belong to the admitted engine image. Eight
setup slots and the `0x338`-byte details block are bounded before slot access.
Failed player admission removes only its optional panel; failed status-call
admission leaves the generic status text. Both retain the native loading path.
The status observer is resident and continues forwarding after shutdown.
Read-only disassembly and test evidence for this update are retained under
`build/refit-loading-players/`.

The parent module first admits its existing complete Armada/FO profiles. Loading
then separately checks the signatures in
[`loading_bindings.hpp`](../modules/A2FORefitMenus/loading_bindings.hpp).
The installed inputs used for verification are:

- ArmadaL SHA-256 `817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`.
- FleetOpsHook SHA-256 `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`.

FO RVA `0x1d58b8` is `NewLoadBar(float, ST3D_Device*, float)`, stdcall with twelve
stack argument bytes. Its first six whole instruction bytes are checked before
the resident hook is installed. The detour waits for publication of its gateway,
uses one owning thread and a reentrancy guard, and forwards the original once
even when the new presentation fails or is disabled during shutdown.

The native loading loop at Armada RVA `0x198f36` proves the device's virtual
`+0x38` flush call. The supported wrapper must point that slot at the checked
`ST3D_DeviceDirectX8::Flush` at RVA `0x226440`. The new path flushes already-queued
native background geometry, draws its image, then calls the original FO routine.
That routine queues/draws its bar and tips above the replacement; the engine's
normal final flush and present follow. It is not called twice.

The COM pointer at wrapper `+0x90` must answer `IDirect3DDevice8` QueryInterface.
Direct-FO-D3D9/unknown wrappers retain the native screen. Rendering uses the live
device only on its calling thread, captures/restores a complete D3D8 state block,
and issues no BeginScene, EndScene, Present, display-mode or window-input calls.
The texture is transient and managed, with no retained device/default-pool GPU
resource surviving reset/destruction. Current cost includes rasterising/uploading
one image at each native loading-frame callback; it adds no independent timer.

This deliberately differs from Refit's opaque post-flush overlay, which would
cover Fleet Ops' configured native bar/tips. Upstream native addresses are not
used without the local identity/signature checks.

## Verification and acceptance

Build and portable checks:

```sh
make refit-menus refit-loading-test
make build/refit_loading_native.exe build/refit_loading_render.exe build/refit_loading_gpu.exe
```

Evidence under `build/refit-loading/` records console binding/forwarding and
offscreen GDI checks, synthetic 800×600/1920×1080 captures, and a private headless
D3D8 compositor fixture. GPU checks distinguish actual drawing/state/resource
behaviour from native engine integration. A simulated later rectangle verifies
overlay order; it is not a real `ParameterDB` or live game test.

An initial GPU attempt stalled because the staged `d3d8.dll` was only a DxWrapper
stub without `dxwrapper.dll`. Its log is preserved as `gpu-missing-proxy.log`.
The isolated host now contains the full proxy chain and a minimal private INI;
the runner rejects incomplete stub staging. No installed wrapper settings change.

Manual acceptance after a full restart remains necessary: Instant Action,
campaign/save loading, return/reload, windowed/borderless, multiplayer progress,
and real ODF tip sprites/text plus horizontal/vertical/inverted bars. Check that
the fresh extension log reports the loading path armed/active. If needed set
`LoadingScreen=0` and restart; the original FO presentation returns.

## Attribution

Visual/briefing semantics adapted from Armada II Refit commit
`37d60079169b086dba4216fe55e05cd6015d66c0`, `ui/src/loading.rs`:
Copyright (c) 2026 Cedric Haase, MIT. Existing
[`LICENSE.txt`](../third_party/armada2-refit/LICENSE.txt) and
[`SOURCE.md`](../third_party/armada2-refit/SOURCE.md) are retained.
Noto fonts retain their separate OFL notices. No proprietary art or game map is
bundled by this change.

## Initial checks and deployment (historical)

- MinGW build passed with `-Werror`.
- Two loading-specific portable suites and eight existing menu suites passed.
- Three console component cases passed: binding admission, refused hook, changed
  signature; these include raw stdcall forwarding and ODF overlay rectangles.
- Thirteen existing actual-DLL console cases passed against the installed FO
  profile. Two additional actual-DLL cases passed with `LoadingScreen=1` and with
  its native signature deliberately changed in a private data mapping.
- Offscreen GDI rendering passed at 800×600 and 1920×1080.
- Real D3D8 → DxWrapper → D3D9/DXVK headless drawing passed at both resolutions,
  including device reset, viewport/depth/texture-stage restoration, later overlay
  visibility and resource teardown. Texture upload/draw API wall times in this
  run were 28/12 ms at 800×600 and 44/28 ms at 1920×1080. These exclude GDI/content
  preparation and are not complete load times or GPU timestamp measurements.
- Independent static review checked the callback publication handshake, native
  argument shape, tip offsets and checked flush path. No game was launched.

Deployed at 12:21 local time with `LoadingScreen=1`. Only
`Data/modules/A2FORefitMenus.dll` and `Data/A2FORefitMenus.ini` changed; 22 protected
inputs retained their hashes, including the active loading ODF and renderer files.
DLL SHA-256:
`08bd94dca1e64ddc4b5c076fcf010cb481dc59c22295993a06e4e44a160d5021`.

Receipt: [refit-loading-20261019-112133Z-deployment.json](refit-loading-20261019-112133Z-deployment.json).
Backup: `.deployment-backups/refit-loading-20261019-112133Z/`.
Restore its two `Data/` files to roll back while the game is closed, or disable
only the new presentation using `LoadingScreen=0` and restart. Live loading-screen
appearance, native ODF sprites/tips and multiplayer acceptance remain manual.

## Player/minimap UI update (2026-10-19)

The newer DLL and faction registry are deployed for testing. Validation passed
37 checks: eleven portable menu/loading/layout suites, five native component
cases, two offscreen GDI render cases, and nineteen actual-DLL console cases.
These cover player data copying, owning-runtime destructor dispatch, observer
filtering, random/custom/campaign faction handling, send-worthy transport
percentages, ODF bar extents and tip exclusions, status forwarding/shutdown,
binding failures, and setup crops at both authored sizes. Native name/destructor
and status calls are stubs in the component replay; the game has not been run.
A private data mapping required explicit rebasing of the checked race-lookup
leaf's three absolute operands; no installed executable was changed.

Independent read-only review caught and resolved the 1200-pixel crop error.
The complete-DLL fixture was also corrected to refuse the unrelated optional
mission CALL rather than throw from its loading-only patch service. The earlier
fixture failure is retained under `build/refit-loading-players/`.

After a full restart, launch Instant Action with two different races. Check the
map preview, actual names/teams/races, local faction panel, percentage/stage text,
and configured ODF bar/tips. Multiplayer progress, campaign/save loading and
native sprite appearance remain manual acceptance. The current Classic loading
ODF supplies only bar colours; tips require actual native tip entries. Animated
fleet departure/warp loading is still outside this UI update.

Only the menu DLL and new `RefitScene/Loading/themes.ini` changed; renderer,
ShellDisplay, INIs, CFG/SPR, ODFs and pinned game binaries were preserved.
Receipt: [refit-loading-players-20261019-151055Z-deployment.json](refit-loading-players-20261019-151055Z-deployment.json).

Rollback: with the game closed, restore the backed-up menu DLL and remove only
the new theme registry listed in the backup's `new-files.json`. The preserved
INI/log are diagnostic copies and do not need replacing.

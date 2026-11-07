# Refit menu performance, setup presentation and missing units — 19 October 2026

Historical first correction. The user subsequently confirmed cold-load and
menu-return stalls and supplied a comparison with Refit's scene typography.
See [the asynchronous renderer and scene-style update](refit-scene-style-and-worker-2026-10-19.md)
for the current candidate and remaining live acceptance boundaries.

This correction addresses the user's severe menu lag, incomplete Instant Action
presentation, and missing Klingon/Borg scene units. It changes the optional
menu host, its menu-only renderer provider, and the provider's performance
profile. Live performance and visual acceptance require a full game restart.

## Evidence and diagnosis

The previous live `Data/RefitScene/A2FORefitScene.log` showed nebula bakes taking
5399 ms (and about 3200 ms after device recreation), 1536-pixel sky cubes and
2048-pixel shadows. The old host forced software vertex processing, rendered at
960 pixels wide and repainted at 25 Hz. Timing from frame start could schedule
another expensive frame immediately after its readback completed. Changing
dialog dimensions also recreated the device and its baked resources.

The active core log says **no /mod argument; using Data only**, with one asset
root. The provider indexed 618 SODs and reported absent stock names including
`kbattle`, `kcruise1`, `kdestroy`, `kcruise2`, `bbattle3` and `bqueen`. The prior
three-root fixture's available assets were therefore not representative of this
launch. Unselected sibling mod assets must not be silently added to fix that.

Source inspection confirms that Instant Action's title and Player/Side/Teams
captions are embedded in `bitmaps/multisetup/instactionbkg.png`. Treating all
unchanged pixels as disposable background removed those captions and panel
boundaries. The native screen uses custom shell widgets; its 55 hidden
COMBOBOX children are not a replacement layout waiting to be made visible.

## Changes

- `modules/A2FORefitMenus/menu_scene.cpp` tries hardware vertex processing first,
  retains software fallback, renders at most 640 pixels wide, and leaves at least
  67 ms idle after a completed frame. The idle interval grows to three times
  the preceding render duration, capped at 1000 ms. Timer invalidation observes
  the same budget, avoiding repeated GDI paints while GPU rendering is deferred.
- Main-menu mouse movement repaints only when hover/pressed state changes.
  `presentation.cpp` precomputes crop columns and column brightness, removing
  floating-point coordinate calculations from the inner pixel loop.
- Only the enabled main form animates. Other dialogs reuse the last CPU frame;
  their native event-driven paints do not start a scene timer or resize the
  offscreen device. The snapshot survives destruction of the main form's device.
- The separate `a2fo-menu.ini` profile disables MSAA/bloom and uses sky faces 512,
  surface faces 256, shadows 512, noise 32 and gas volumes at most 64 by 32
  vertically. This config belongs to the optional menu provider, not the game
  mission renderer.
- `scene_composition.cpp` supplies navy/amber setup panels and retains bright
  neutral embedded captions on the two exact Instant Action/multiplayer setup
  asset names. It preserves every native RGB overlay difference, including
  single-channel +1 edges. The lower-left stock ship illustration is replaced
  by the scene. The initial background check no longer creates a full composed
  output image, and composition reuses its output allocation.
- The vendored host-only `src/menu/compat_assets.rs` resolver uses installed
  same-faction models when the original ODF-resolved SOD is absent:

| Stock menu role | Installed fallback |
| --- | --- |
| `kbattle` | `kli_neghvar` |
| `kcruise1` | `kli_vorcha` |
| `kdestroy` | `kli_brel` |
| `kcruise2` | `kli_susa` |
| `bbattle3` | `bor_cube` |
| `bqueen` | `bor_diamond` |

Federation roles have equivalent fallbacks. A present original model always
wins, including custom/Classic assets; only selected SDK roots are searched.
Stock ODF ScaleSOD is ignored for a substituted model, while recipe Scale still
applies. These are approximate scene substitutes: a standard FO cube replaces
the missing fusion-cube role. Their framing and appearance need visual review.

The full Instant Action Refit widget/layout port remains **unfinished**. This
patch repairs its underlay and preserves native controls/hitboxes; it does not
claim that the upstream Rust UI has been ported. No new engine hooks, addresses,
native control relocation, or fabricated input events are introduced.

## Verification

- MinGW module build with `-Werror`; five portable C++ suites pass, covering
  presentation, input model, skin paths, composition and completion-based frame
  scheduling including tick wraparound.
- 52/52 headless native console fixture processes pass across four admitted
  FleetOpsHook file profiles. These exercise checked patch/callback contracts,
  not an actual game UI.
- File audit: all 30 menu/skin signatures and form-layout checks match each of
  the same four profiles.
- Offscreen Win32 GDI fixture passes in a private Wine prefix with DISPLAY and
  WAYLAND_DISPLAY unset. It uses memory DCs, no game window or input automation.
  Mesa diagnostic warnings are present; this is not D3D/GPU scene evidence.
- Rust i686 MSVC provider release build passes (10 existing dead-code warnings).
  The Data-only null-device provider fixture passes initialization, malformed
  input, repeat initialization, owner/foreign-thread and null-device guards.
  Its log resolves all 16 ship entries, including both Klingon birds of prey.
- Four pure resolver tests pass. A separate native `a2fmt` probe resolves and
  parses all 15 unique stock roles' fallback SODs from installed Data, finding
  finite transforms and nonempty geometry. This proves assets parse, not their
  textures, animations, clipping, decloaking or GPU visibility.
- The actual installed Instant Action artwork was passed through the current
  skin/compositor with a synthetic gradient scene and inspected offscreen:
  `build/refit-ia-underlay-preview.png`. Captions/panels remain; the old static
  ship artwork is absent. This is an underlay diagnostic, not a game screenshot.

Logs: `build/refit-menu-lag-{build,native,gdi,data-provider,alias-tests,sods}.log`
and `build/refit-menu-lag-bindings.json`. The new provider resource log is
`build/refit-scene-resources/A2FORefitScene.log`.

## Deployment and next live check

The timestamped deployment receipt alongside this note records installed
hashes, the rollback backup, preserved pre-fix live logs, and hashes of protected
files. Only these three deployed files change:

1. `Data/modules/A2FORefitMenus.dll`
2. `Data/modules/refit/A2FORefitScene.dll`
3. `Data/RefitScene/Render3D.ini`

Installed at local `2026-10-19 00:09:35`:
[deployment receipt](refit-menu-lag-fix-20261019-000935-deployment.json).
Rollback copies of those three relative paths are in
`.deployment-backups/refit-menu-lag-fix-20261019-000935/`. With the game fully
closed, copy each backup file over its corresponding path under the Fleet Ops
Roots game directory to restore the previous candidate. Pre-fix game logs and
the headless test evidence are preserved separately inside that backup.

After a full quit/restart, check main-menu responsiveness and Klingon/Borg
selectors (allow camera blending/decloaking to finish), then Instant Action's
player rows, race/team choices, settings, minimap, launch/back controls and map
selection. Confirm windowed and borderless behavior remains accepted. A setup
screen deliberately holds its scene still. Fresh logs should show the hardware
or software path, smaller bake targets, frame timings and model substitutions.
No current live FPS, visual completion, or performance improvement is claimed
from the headless checks.

The renderer remains an MIT-licensed modified copy of Cedric Haase's pinned
Refit source. Original upstream hashes and Refit/libm notices are preserved;
no proprietary assets are copied into the repository or staged bundle.

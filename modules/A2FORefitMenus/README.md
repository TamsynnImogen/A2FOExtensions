# A2FORefitMenus

Latest transition correction and validation: [accepted Launch handoff, retained erase images and early warp preparation](../../docs/refit-handoff-2026-10-20.md).

Current integration: [named campaign views and animated warp loading](../../docs/refit-menu-flow-2026-10-20.md).
`Enabled=1`, `LiveScene=1`, `LoadingScreen=1`, and `LoadingAnimation=1` enable the full available
presentation path. Menus and the campaign browser reuse the retained renderer;
launch requests Refit's departure camera and warp tunnel during native loading.
The bar, tips, progress and loading lifecycle remain Fleet Ops controlled.
Instant Action/multiplayer show player details and a minimap; campaigns show
briefing/objectives, including metadata handed off by the expanded mission
selector. Faction palette/panel shape still come from `Loading/themes.ini`.
Native in-game acceptance is separate from headless renderer evidence.

The loading animation uses checked splash/status/asset callbacks to draw
between native progress updates. Expensive frames increase the idle budget;
loading never waits in an animation hold loop. Its loading texture is reused
only after the splash hooks and native device cleanup hook are admitted, and
released before native device cleanup or splash teardown. Changed splash
signatures keep native cadence; changed device cleanup keeps transient textures.
The offscreen renderer now resizes its output surfaces while
retaining its device and assets across menu/loading resolution changes.

Latest candidate: [Instant Action Refit layout and darkened scene overlays](../../docs/refit-instant-action-layout-2026-10-19.md). Native state/actions stay authoritative; game acceptance is separate from fixture results.

Current correction: native-resolution rendering and an asynchronous, retained
scene backend, with a DDS conversion/cache fix reducing measured first-frame
preparation from 43.1 seconds to 3.46 seconds at 1920×1080. See
[the first-frame correction](../../docs/refit-live-scene-first-frame-2026-10-19.md) and
[the resolution/loading record](../../docs/refit-native-resolution-and-loading-2026-10-19.md)
for validation, deployment and remaining cold-load limits. The earlier startup
exception and rollback remain documented in [the crash record](../../docs/refit-startup-crash-and-background-config-2026-10-19.md).

Optional Refit-style main menu, shared shell asset skin, and real animated Refit 3D menu backgrounds. With `LiveScene=1`, the menu host renders Refit's Federation fleet, starbase, Klingon and Borg views on an independent offscreen D3D9 device, then composites them beneath native menu controls and labels. The main page lists INI-authored named views in pages of five, with navigation when needed. Other menus retain the last selected view. Installed mod assets supply the models and textures; no game artwork is bundled.

The seven rows are the original **Single Player**, **Instant Action**, **Multiplayer**, **Options**, **Map Editor**, **Credits**, and **Exit** actions. The module reads each action's localized caption, enabled state, and visible state from the real FO form, so conditional entries such as Single Player retain Fleet Ops' current behavior. Selecting a row calls its native FO action. Options continues to expose the installed Mods/modules access. Refit’s stock **Replay Intro** row is replaced by **Map Editor**, using the verified FO action.

The scene presentation uses Refit's open, left-aligned uppercase rows, hover glow
and accent bar, larger title and orange rule. Native captions are uppercased only
for display. Unmodified Noto Sans Condensed Medium/SemiBold fonts are embedded
and registered privately with GDI, avoiding a missing-system-font substitution
under Wine. See `third_party/armada2-refit/fonts/` for their separate OFL licence.
Background scaling is bilinear; the uncovered scene keeps its full brightness.

The shared skin covers the named shell asset families used by Single Player, mission selection, Instant Action, multiplayer connection/setup, map selection, advanced setup, Options, Graphics, Game, Sound, Mods, load/save screens, Admiral's Log and common dialogs. It also handles shared widget state images. Native localization, dynamic lists, map previews, action handlers, sounds and hit masks stay authoritative. Unknown files, mission preview images, faction icons, game textures and indexed TBitmap assets are left native. The live scene replaces matching remembered backgrounds through both VCL TImage and legacy shell bitmap drawing paths. Unknown/changed images retain their native artwork. This does not replace every screen's widget layout.

## Build and configuration

The module is opt-in and ships disabled. Copy `A2FORefitMenus.ini.example` to `A2FORefitMenus.ini` in the extension root and set `Enabled=1` to request it. With the example's `Enabled=0`, no hook is installed. Module loading is also controlled by the active module list in `info.ini`; append the next unused `activeN = "A2FORefitMenus"` entry to the applicable `[modules]` section. A mod with its own `[modules]` selection must enable the module there.

The background renderer has an independent switch in that same file:

```ini
[RefitMenus]
Enabled=1
InstantAction=1
LiveScene=0
SceneRenderWidth=0
```

`LiveScene=0` uses the installed menu background images and starts no Refit
scene renderer, D3D device or scene worker. It keeps the menu presentation and
shared artwork/widget skin enabled. Set `LiveScene=1` to request animated 3D
backgrounds instead. Fully restart the game after either change; this is not a
hot renderer-unload switch. The example explicitly selects static backgrounds.

`SceneRenderWidth=0` renders at the physical menu client size, bounded to 3840
pixels wide, 2160 high and eight million pixels. At 1920×1080 it renders exactly
1920×1080. An explicit width from 320 through 3840 selects a smaller budget while
preserving aspect ratio and never upscaling beyond the client size. Invalid
values are logged and use the bounded native size. Restart after changes.

Build and focused checks:

```sh
make refit-live-menus
make refit-menus-test
REFIT_WINEPREFIX=/absolute/path/to/existing/private/test-prefix make refit-menus-native-test
```

`InstantAction=1` (default) enables the checked setup layout when the updated
ShellDisplay canvas/presenter API and complete native widget model are available.
Set `InstantAction=0` and restart to keep the native setup layout. This switch
is independent of `LiveScene`; static artwork remains available with rendering off.

The native test command uses an existing private Wine test prefix. Current checks include eight portable suites, 52 console DLL fixture processes, an offscreen GDI fixture, the menu-only provider ABI fixture, and checked native paint callbacks. A separate isolated headless GPU fixture exercises actual 1920×1080 main/Klingon/Borg rendering and backend retention. See [the resolution/loading record](../../docs/refit-native-resolution-and-loading-2026-10-19.md) for current evidence and deployment. The [scene-style correction](../../docs/refit-scene-style-and-worker-2026-10-19.md), [previous performance correction](../../docs/refit-menu-lag-and-setup-fix-2026-10-19.md), [original live-scene record](../../docs/refit-live-scenes-2026-10-18.md) and [earlier main-menu record](../../docs/refit-menus-main-menu-2026-10-18.md) remain historical evidence. Headless checks do not establish live visual acceptance.

## Compatibility and fallback

The FormShow hook is at FO RVA `0x1cc330`; the exact main-form VMT is at `0x1cb7a8`. The form HWND is at `+0x1b4`, proven by native `TWinControl.GetHandle`; `+0x1c4` is its parent window and is never used as the presentation target. Its Windows class must be `TMainForm`. Startup checks the complete FO image profile and the required native signatures before installing the hook. It supports the three original whole-file FO profiles plus the reviewed October 12 Roots DLL (CRC `0x7cd6661e` and complete SHA-256 `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`), and excludes the canonical Armada shortcut. The checks and bindings are in `module.cpp` and `bindings.hpp`.

The original form is shown normally. After validating the form and all original controls, the module hides only those visual controls, paints into the existing window using GDI and the physical `GetClientRect`, and restores their saved visibility if the presentation is disabled or falls back. The controls are destroyed normally with their form. Native actions and localized captions stay attached to the form. Unknown identities, changed signatures, unavailable controls, or attach/paint failures keep or restore the native menu without enabling the replacement. The module is pinned resident because native callbacks may remain installed; shutdown queues restoration on the native UI thread. Restart the game to load or remove it.

Two separately checked shared loaders are FO RVAs `0x16cfa0` (TBitmap) and `0x16d278` (TBitmap32). The wrapper calls the original loader first and preserves its entire return register, including GR32's successful-PNG/false-return behavior. Only explicit relative shell asset names in `skin.cpp` qualify. For true-colour TBitmap, a scratch GDI image is styled and committed without changing native bitmap format/palette. GR32 styling retains alpha, and both paths preserve magenta keys and dimensions. Original bitmap masks and geometry remain. If either skin hook cannot be installed, installed callbacks forward and the checked main page remains available. Already styled assets revert on a full restart with the module disabled; shutdown cannot undo loaded asset caches.

The main background is copied from `form+0x37c -> TImage+0x198 -> TPicture+0x0c`, requiring the exact native TBitmap class. The original stays owned by the form; an independent pixel copy supplies aspect-cover cropping and stronger left-side dimming behind the button column. Missing artwork uses the existing gradient fallback. The additional bindings and layout evidence are in `skin_bindings.hpp`; the file-only audit now checks 30 signatures across all four profiles.

`A2FOShellDisplay` can alter shell-window sizing and GDI presentation. Their combined load order and physical client-area behavior still need manual validation at each tested resolution. The scene worker creates an independent, non-exclusive offscreen device and its own hidden owner window. It never shows/activates that window or calls Present, and does not replace the game device or fullscreen behavior.

## Live scenes and fallback

Menus opened during a match use a retained image of that match instead of the
front-end fleet. A checked observer at Armada RVA `0x198a16` forwards the native
chat render and records mission-frame generations; the verified
`g_chat_display` lifetime distinguishes gameplay from the front end. This has
no pause timeout. Nested menus share one CPU snapshot, and resuming gameplay
invalidates it before the next menu visit. Native background loading attempts
capture before the form is shown, with first paint as a fallback. The existing
menu compositor dims the gameplay image behind the controls.

The observer also works with `LiveScene=0`; it borrows the existing D3D8 game
device for a bounded front-buffer read, creates no scene worker, and retains no
GPU resources. Unknown signatures leave the observer uninstalled. Failed
capture uses a neutral backdrop instead of an old fleet/match image. The
thread that drew the mission must perform capture. Real-game capture timing,
windowed/borderless/fullscreen behaviour and pause/resume remain manual
acceptance checks; the hidden GPU fixture tests the production capture/crop
routine and Reset, without displaying an actual game.

Fleet Operations replaces `ChatDisplay::Render` itself during Cleanshot
startup. The observer accepts the original entry or that exact admitted FO
transfer, callback and executable gateway, including the original copied
prologue and continuation. It observes the separate unmodified caller and
continues through FO's entry, preserving Cleanshot's conditional HUD suppression.
Startup failures now log the individual binding and observed entry bytes.
The regression fixtures replay the real FO callback from a data-only mapping;
they test forwarding and suppression, and reject altered targets, callback
bytes, non-executable callback pages and invalid gateways. This closes the
unpatched-input gap in the initial backdrop tests; a passing fixture remains
separate from in-game visual acceptance.

`LiveScene=1` requests the separate `modules/refit/A2FORefitScene.dll` provider and
`RefitScene/` configuration tree. `LiveScene=0` retains the prior static artwork
presentation. These are subordinate to `Enabled=1` and the existing module list.
Build the complete scene bundle with `make refit-live-menus`; the ordinary module
build alone deliberately does not require Rust.

The provider is a modified, pinned build of Refit's renderer. Its DLL entry point
installs no game hooks. Stock executable reads, patches and sounds are disabled;
only the menu's sky, world and hull modules are registered. The versioned bridge
copies the SDK's Data/parent/selected-mod asset roots and owns no game objects.
It supports loose SOD/ODF/SPR/TGA assets and base-level DDS BC1/BC2/BC3 or masked 16/24/32-bit
RGB(A). FPQ archive reading and DX10 DDS are not added by this bridge.

The main scene animates at up to roughly 15 updates per second, at the bounded
native client size by default. GPU rendering,
shader/model loading and readback run on one worker thread, which also owns the
provider and device. UI callbacks only send requests and borrow immutable CPU
frames. The worker waits after each completed render: at least 67 ms, increasing
to three times its duration (capped at 1000 ms). Timer-driven UI paints occur only
when a new frame/loading state is published. Hardware vertex processing is tried
before software fallback. The menu-only bake
profile uses 1024-pixel sky/surface faces and reduced shadow/nebula targets.
Named scene selectors use the user's actual mouse events. Refit camera blending,
orbit, motion and faction decloaking remain intact. The optional
`A2FOMissionSelector` bridge selects a campaign/mission's `menuView` and starts
its departure before forwarding to native `SetupMission`; direct native loading
also requests departure through the existing loading callback. No artificial
minimum load duration or extra native Present calls are introduced.

Recognized submenu backgrounds now dim the selected rendered scene while native
content is overlaid. Submenus reuse the worker's retained device/cache and can
request animation at their physical client size. An old main-menu timer cannot
pause the current submenu. First-use loading remains asynchronous. Unknown
artwork retains its native presentation. Three consecutive draw failures keep
the latest frame/artwork fallback; shutdown only signals the worker.

Instant Action can use the responsive Players/Map setup layout, native preview
and colour swatch pixels, and bottom navigation. A checked Shell::Widget
constructor observer reads native labels, button states and hidden combo data.
No hidden COMBOBOX is made visible; real incoming pointer coordinates are
translated once into the original controls' coordinate space. Fleet Ops owns
settings, selections and launch behavior. A complete model and verified dialog
input bridge are required before presentation takes over. Unsupported controls
or canvas/model mismatch retain the native layout. Multiplayer and the other
menu widget layouts remain native. See the latest candidate record above.

Known stock recipe ship roles can fall back to installed same-faction Fleet Ops
SODs (for example `kbattle` to `kli_neghvar`, `bbattle3` to `bor_cube`, and
`bqueen` to `bor_diamond`). A present original ODF-resolved SOD always wins.
Fallback models use their own native scale rather than an unrelated stock
ODF's ScaleSOD; recipe Scale still applies. The host logs substitutions and
unresolved models. It never adds an unselected mod's roots to find artwork.

TImage.Paint (`FO+0xa43e0`) uses the native CalcDestRect (`+0xa420c`). Legacy
ShellBitmapImp Draw/Draw2/Paint (`+0x1f8db4`, `+0x1f8dfc`, `+0x1f8e44`) keep the
native ECX-self/stack-HDC ABI and original drawing fallback. The compositor
matches a bounded catalogue of loaded backgrounds and retains pixels changed by
native label painting. It never replaces the game's persistent bitmap contents
with scene frames. Missing provider, unavailable device, unknown artwork or
failed composition leaves the prior native/static presentation available.
Rust panics retain upstream's process-abort policy; they cannot use that fallback.

## Live acceptance

The main-menu actions and display sizing were tested live by the user before this shared skin update. After a complete restart, manually check the main artwork, Single Player/mission selection, Instant Action/map selection, multiplayer/setup, Options/Graphics/Game/Sound/Mods, load/save, Admiral's Log and dialog buttons. Confirm labels, hover/disabled states, previews, sliders, checkbox glyph/caption clicks, list scrolling and return paths. Keep the accepted ShellDisplay borderless/windowed behavior and test both modes. See the shared skin record for the new deployment receipt and rollback. GUI/input automation is not part of these checks.

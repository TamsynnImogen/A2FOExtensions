# Refit live 3D menu backgrounds — 18 October 2026

Historical first deployment. The user subsequently reported severe menu lag,
incomplete Instant Action presentation, and missing faction units. The current
candidate and its evidence are in [the 19 October correction](refit-menu-lag-and-setup-fix-2026-10-19.md).

This follows the user's request to use Refit's interactive backgrounds instead
of the previous Fleet Ops artwork. The new path uses Refit's real map, renderer
and shaders, not screenshots, videos or a recreation of the scene in GDI.
Source/build/headless checks are separate from in-game visual acceptance.

## Implementation

Upstream is `cedrichaase/armada2-refit` commit
`37d60079169b086dba4216fe55e05cd6015d66c0`. The modified source is vendored in
`third_party/armada2-refit/menu-renderer/`; original hashes are preserved in
`UPSTREAM-MANIFEST.json`. See `LOCAL-CHANGES.md` for adaptations.

`menu_scene.cpp` creates a non-exclusive, independent D3D9 device on the existing
menu HWND. It requests FPU preservation and no window changes, renders offscreen,
reads back pixels, and never calls Present. No HWND is created. Game rendering,
resolution, fullscreen settings and the accepted A2FOShellDisplay DLL are not
replaced. D3D9 and the provider are loaded lazily, only for requested scenes.

The provider's `DllMain` does nothing. Its explicit 24-byte version-1 host ABI
receives the borrowed device, resource root and SDK asset roots. Engine memory
reads return safe defaults; executable hook functions and engine sound calls
are disabled. Only Refit's World, Hulls and Sky modules are registered. Existing
mission-specific source remains in the vendor tree but is not registered by the
host. Do not substitute the original Render3D.asi: its normal startup installs
stock Armada2.exe hooks.

The main page displays the real main/Federation/starbase/Klingon/Borg views.
The five selector buttons use the user's genuine mouse events. Refit supplies
camera blending, planet orbit/rotation, sky/stars, ship motion and faction cloak
transitions. Other menus keep the selected camera view. Campaign hover bindings
and mission-loading warp transitions are not connected in this integration.

Native localized controls, actions, lists, previews and widget hit masks remain.
VCL and legacy menu background callbacks composite the scene beneath those
controls. A bounded catalogue records up to sixteen loaded backgrounds (8 million
pixels total). A background needs at least 90% unchanged sampled and full RGB
pixels to qualify. Pixels changed by native label painting are retained. The
scene is painted into the current DC, not into persistent native asset storage.
A new or heavily altered background can conservatively keep its prior artwork.

## Native bindings

All addresses below are FleetOpsHook RVAs, not ArmadaL.exe addresses. The same
source signatures were checked against all four admitted FO file profiles,
including installed Roots SHA-256
`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`.

| RVA | Role / ABI |
| --- | --- |
| `0x1cc330` | Existing main FormShow: EAX self, EDX sender |
| `0x16cfa0`, `0x16d278` | Existing TBitmap/TBitmap32 loaders; original full return value retained |
| `0xa43e0` | TImage.Paint: EAX self, no stack arguments |
| `0xa420c` | Native CalcDestRect: EAX self, EDX output RECT; retain Stretch/Center/Proportional placement |
| `0xaad0c` | GetParentForm parent-chain evidence: `mov esi,[esi+0x30]` |
| `0x1f8db4`, `0x1f8dfc`, `0x1f8e44` | ShellBitmapImp Draw/Draw2/Paint: ECX self, stack HDC, callee ret 4 |

The legacy prologues' `push ecx` initializes the local self pointer before the
subsequent `[ebp-4]` read. This is not an uninitialized local. Draw/Draw2 use
zero-extended 16-bit positions; Paint reads full signed 32-bit positions.
Native forwarding fixtures exercise all three contracts and four stack alignments.

VCL images must have the native DFM component name BackgroundImage (Name+8,
also read at FO+0xaad82), preventing previews from qualifying just because
they share dark pixels. They follow bounded Parent+0x30 links to an allowlisted same-thread shell
form's own HWND at +0x1b4. The +0x1c4 ParentWindow field is not the target.
Legacy drawing accepts same-thread #32770 dialogs whose dialog procedure belongs
to admitted Armada/FO or the existing ShellDisplay bridge. A logical memory DC
has no HWND, so its fallback association reads GetActiveWindow without changing
focus. Its exact ownership association still needs native acceptance.

## Assets and deployment layout

- `Data/modules/A2FORefitMenus.dll`: checked native integration.
- `Data/modules/refit/A2FORefitScene.dll`: menu-only Rust provider, outside the
  native module enumeration directory.
- `Data/modules/refit/licenses/`: Refit and libm notices.
- `Data/RefitScene/Render3D.ini`: host renderer settings (MSAA defaults to 0).
- `Data/RefitScene/Menu/map.ini`: editable original Refit scene recipe.
- `Data/RefitScene/{skies,surfaces,nebulae}/`: original MIT configuration recipes.
- `Data/RefitScene/A2FORefitScene.log`: renderer diagnostics after first use.

No proprietary textures/models/fonts are distributed. Asset roots follow SDK
precedence: Data, parent mods, selected mod. Lookup is case-insensitive and
includes nested ODFs, flat Textures and Textures/RGB. TGA takes precedence over
DDS in one root; a child DDS can replace a parent's TGA. DDS aliases participate
in the existing texture/mipmap-name index. Loose files are supported; FPQ archive
reading and DX10 DDS are not implemented. Missing assets can leave scene objects
or texture details absent without proving a hook problem.

The scene updates at most 25 times per second and at most 960 internal pixels
wide, preserving physical client aspect ratio. UI controls remain native/GDI.
Readback and CPU composition are a compatibility bridge; this is not a new
in-match renderer. First scene use indexes assets synchronously and can pause
while those files are read (the three-root headless run took about 13 seconds).
Timers invalidate visible enabled owning windows only. Device resources are
forgotten and unbound before release/recreation. Shutdown disables rendering
atomically; window destruction releases resources on the owning UI thread.

## Build and verification

Run `make refit-live-menus refit-menus-test build/refit_menu_native.exe
build/refit_scene_provider.exe`. The scene script uses rustup 1.96.0, Windows
x86 MSVC, rust-lld and the pinned `a2fo-menu-host` feature, offline with Cargo.lock.
See `refit-scene-provider-build.md` for the no-window provider fixture command.

Current evidence:

- Native module and fixture compile with warnings treated as errors.
- Four portable suites pass: presentation, model, skin, scene compositor.
- File audit: 30/30 signatures match each of four supplied FO profiles.
- Native console matrix: 52/52 processes pass on module
  `3ce9dfb13ba406900a84b75c3bb605cd7f079fad40bfe96dcebaae4fa6c31db6`.
  An earlier transient 30-second timeout is preserved separately; the final
  runner permits 90 seconds per process.
- Provider ABI/null-device/thread guard fixture passes without mapping or running
  a game executable. It indexes Data/Classic/test and reads Menu/map.ini.
- All 20 parser unit tests pass, including 16/24/32-bit DDS and BC1/2/3.
- The installed Classic data run parses 464 SODs (11 older files are deliberately
  refused), 1,097 ODFs, 50 SPR files and 2,000 damaged inputs without parser panic.
  The original upstream stock-only texture tests are **not** claimed as passing
  against this mod's flat layout: three fail on missing Textures/RGB and expected
  stock mip/set names. That failure log is preserved, not edited into a pass.
- A separate read-only flat/nested image probe examines 3,813 installed textures
  across Data/Classic/test: 3,364 TGA and 449 DDS; 3,804 decode. Nine do not:
  six 8-bit DDS/luminance or other unsupported encodings, two DDS explosion sheets
  exceed the 8-million-pixel decoder guard, and Classic's 8472_behemoth.tga is a
  paletted TGA. These are compatibility limits, not an all-assets pass. The
  relevant log gives exact filenames. No decoder limit was relaxed just to
  accept large game effects outside this menu renderer.

Logs: `build/refit-live-scene-{build-final,native,bindings}-20261018.*`,
`build/refit-scene-{rust-final,provider,formats,images}-20261018.log`.
No game window, focus change, keyboard/mouse event or input automation was used.
The headless checks do not prove a successfully rendered GPU frame.

## Risks, acceptance and rollback

This remains a first native integration candidate. After completely exiting and
restarting the game, inspect the main scene and each selector, then Single Player,
Instant Action/map selection, multiplayer/setup and Options/Graphics/Sound/Mods.
Check that text remains readable, native previews/lists/checkboxes still work,
back paths return correctly, and selected resolution/borderless mode still match.
Inspect both VCL and legacy screens for background positioning and clipping.
Confirm Alt-Tab, resize and transitions into/out of a match without GPU failures.

A missing provider, failed device, nonmatching background or draw failure keeps
native/static artwork. Rust panic is different: upstream's abort/ExitProcess
policy remains, so a renderer panic terminates the process. The vendored unsafe
COM calls, process-wide state and untested real-device lifecycle need native
acceptance; they are not evidence of game readiness from headless checks alone.

Set `[RefitMenus] LiveScene=0` in Data/A2FORefitMenus.ini and restart to retain the
previous static Refit menu presentation. Full rollback restores the backed-up
menu DLL and INI and removes only newly installed provider/resource files listed
in the deployment receipt. Do not replace ShellDisplay, game binaries, settings
or existing working extension modules as part of rollback.

MIT attribution is preserved for Cedric Haase and libm, including libm's embedded
third-party notices. Modified host/parser paths are documented; no upstream
attribution is removed. Deployment receipt and exact backup are linked below
once installation finishes.

## Installed candidate

Deployment receipt: [refit-live-scenes-20261018-233216-deployment.json](refit-live-scenes-20261018-233216-deployment.json).

Backup: `/home/tamsynn/A2FOHookExtensions/.deployment-backups/refit-live-scenes-20261018-233216`.

Menu DLL: `3ce9dfb13ba406900a84b75c3bb605cd7f079fad40bfe96dcebaae4fa6c31db6`.
Provider DLL: `7d33a84545529d73e8901de199f0c3e52b17357b5d9404d4d8aa477300a6b89b`.

Installed with LiveScene=1. Protected ShellDisplay, engine, core, module lists
and settings hashes were checked unchanged. Restart and native visual acceptance
remain outstanding.

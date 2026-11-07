# Shared Refit menu skin and background artwork — 18 October 2026

The main page was the only replaced menu. The user requested updates across
the menus and explicitly selected **Include background artwork**. This change
extends the presentation through the common shell bitmap loaders rather than
replacing native list, slider, checkbox, map preview or network behavior.

## Resulting scope

The main menu now copies its active mod's original background picture and draws
the Refit column over it, with aspect-cover cropping and dimming for contrast.
Other menus retain their native layouts. Their recognized background and
shared widget assets get a navy/amber treatment at load time. The artwork and
native localized text remain. This is a shared artwork/theme pass across the
menus, not a full port of Refit's Rust screen implementations.

| Screen family | Recognized asset paths beneath `bitmaps` |
| --- | --- |
| Main / Single Player | `main/mainbkgr`, `single/singleplay` |
| Mission selection | `MissionSelect/missionselectbg`, OK/cancel/mission field states |
| Instant Action / multiplayer setup | `multisetup/instactionbkg`, `MultiplaySetupBkg`, setup/opponent/footer buttons |
| Map / advanced setup | `multisetup/mapselect/MapSelectBkg`, `multisetup/advanced/ADVANCEbkg`, associated widgets |
| Options / Graphics / Game / Sound / Mods | explicit `Backgrounds` names, including `OptionsBG`, `GraphicBG`, `GameoptBG`, `SoundOptionsBackgroundScreen`, `AdvoptBG`, `ModoptBG` |
| Load/save / common dialogs | `Backgrounds/LoadGameBG`, `LoadSaveBG`, `SaveGameBG`, `Dlg1ButtBG`, `Dlg2ButtBG`, associated shared buttons |
| Multiplayer connection / lobby | explicit `multiconnect` backgrounds and widgets |
| Admiral's Log | `AdmiralsLog/BACKGROUND` |
| Shared controls | explicit `SharedButtons`, slider and scrollbar state images |

The exact allowlist is in `modules/A2FORefitMenus/skin.cpp`. It rejects absolute
paths, traversal, unknown names and unsupported extensions. Map/mission
preview art, faction icons, ship/HUD textures and unknown mod assets stay
native. Native formats, transparent silhouettes, alpha and magenta keys are
preserved; indexed TBitmap assets are skipped rather than changing their
palette. Already loaded assets require restart to return to the unskinned
version. Native layouts, action handlers, sound, localization and settings
remain authoritative.

## Native integration evidence

| Anchor | Evidence / use |
| --- | --- |
| FO `0x16cfa0`, 9 whole bytes | Delphi EAX=TBitmap, EDX=AnsiString shell loader; original called before styling |
| FO `0x16d278`, 6 whole bytes | GR32 loader with the same registers; full original EAX retained even when PNG success returns false |
| FO `0x94000`, `0x93e78`, `0x93de0`, `0x8f538` | TBitmap width/height/canvas and TCanvas HDC helpers |
| FO `0xec004`, `0xec120` | Native GR32 stride, bits and width/height layout evidence: width +44, height +40, bits +54 |
| FO `0x1cbe1e`, `0xa44d0` | Main `BackgroundImage` +37c, image picture +198, picture graphic +0c |

RVAs are used relative to the admitted image. Four complete FO profiles match
the 24 file/instruction checks; the October Roots profile still requires its
full SHA-256. The main background requires the exact native TBitmap VMT, and
GR32 buffers require TBitmap32 class metadata. The original graphics engine,
FleetOpsHook file, extension core, ShellDisplay v15, saved settings and module
lists are unchanged by this implementation.

Sources: `notes/pass27/dialogfunctions.asm.txt`, `notes/pass27/jetbutton.asm.txt`,
`notes/clean-baseline/menu-source/clean-map-names.tsv`, native image disassembly,
and recovered `rebuild/src/ui/graphics.cpp`, `controls.cpp`, `main_form.cpp` and
`rebuild/src/hooks/shell_bitmap_imp.cpp`. These are under
`reverse_engineering/FleetOpsHook`. In particular, `ShellBitmapImp::Init`
uses the same TBitmap loader for legacy Armada dialogs, then performs native
localized bitmap-text processing. The GR32 loader similarly runs before the
individual FO forms draw localized text. The skin does not remove those labels.

## Checks and acceptance

Three portable suites pass: main layout/interaction, native action model and
skin path/pixel/bounds checks. The real DLL passes 52/52 console processes:
13 cases against four supported FO files paired with installed ArmadaL. These
cover disabled/invalid settings, wrong identity/signatures/owners, partial
installation, native action forwarding, four incoming stack alignments,
TBitmap memory-DIB styling, zero GDI alpha, GR32 real alpha/key preservation,
unknown-preview pass-through, invalid dimensions/null buffers and shutdown.
Read-only GR32 pixel buffers are rejected without changing their protection.
Offscreen render checks pass for artwork visibility, left contrast, crop
geometry, labels, buttons and invalid-artwork fallback.
The optional BMP-input path also renders the actual installed mod background
to [a local offscreen preview](../build/refit-all-menus-artwork-preview-20261018.png).
This is a renderer preview, not an in-game screenshot, and contains existing
installed game artwork; it is not included in any release package.

The initial alignment test incorrectly measured the already aligned inner
gateway. It was corrected to measure the incoming caller stack; all four
caller alignments then passed without a production ABI change. Review also
caught an early TBitmap pixel-format mutation; the conversion was removed,
and the reviewer confirmed the corrected scratch-copy path has no remaining
blocker from that finding.

Evidence: `build/refit-all-menus-{build,native,render}-20261018.log` and
`build/refit-all-menus-bindings-20261018.json`. No engine entry points, actual
windows, synthesized input or focus automation run in these tests. These
checks do not prove every native form loads its expected assets at runtime.

After a full restart, check the main artwork, Options and all settings pages,
Single Player/mission selection, Instant Action/map/advanced setup,
multiplayer connection/lobby, load/save, Admiral's Log and confirmation
dialogs. Inspect hover/disabled states, localized labels, previews, slider
values, checkboxes, list scrolling and return paths in framed and borderless
modes. Look for `Refit shell artwork/widget styled:` and the main artwork
attachment diagnostic in a fresh log. Live visual acceptance remains pending.

The upstream MIT notice remains in `third_party/armada2-refit`. No installed
game artwork is included in release packages or replaced on disk.

## Deployment

Only `Data/modules/A2FORefitMenus.dll` was replaced, with the old module and
current logs saved under `.deployment-backups/refit-all-menus-20261018-223531`.
The [deployment receipt](refit-all-menus-20261018-223531-deployment.json)
records hashes and all ten unchanged protected files. Installed menu DLL:
`50f6e497a2f2d90b8683766c601fe737cd4ac09317c20d50d41c1508f30dd9fe`.
The accepted ShellDisplay v15 module remains byte-identical. This deployment
requires a complete game exit/relaunch; it does not hot-reload resident hooks.

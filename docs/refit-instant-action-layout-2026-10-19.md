# Instant Action Refit layout — 19 October 2026

The candidate replaces the Instant Action setup presentation with a responsive
Players/Map arrangement over the selected, darkened Refit scene. The game still
owns every player, race/team selection, map setting and launch/load action.
Other recognized menus receive a darkened animated scene but retain their native
widget layouts. This is a bounded Instant Action milestone, not an all-menu port.

## Presentation and configuration

`[RefitMenus] InstantAction=1` defaults on when the existing module is enabled.
`InstantAction=0` retains native setup presentation after restart. `LiveScene=0`
continues to select installed artwork without starting a provider or GPU worker;
`LiveScene=1` uses the retained asynchronous scene. Existing resolution/window
mode settings and renderer/profile files are unchanged by deployment.

Players occupy the left panel, with eight rows of player/side/team combos,
colour swatches and Random/palette controls. The right panel contains native map
information, a larger preview and settings. Navigation remains along the bottom.
Native status messages can be drawn below Players. Captions/selections/enabled
states come from the game; fallback section headings are English. Native minimap
and swatch pixels are copied from the existing logical canvas, not regenerated.

## Checked native boundary

`setup_native.cpp` observes `Shell::Widget::Widget(HWND)` at ArmadaL RVA
`0x1a8060` / preferred VA `0x5a8060`. It validates the supported executable and
all 21 constructor bytes before requesting an instruction-aligned five-byte SDK
hook. The tiny original constructor runs first; the observer records only its
object/window pair. It writes no widget fields, installs no Instant Action dialog
inline hook, and does not compete with A2FOInstantActionSettings' existing hook.
The module is already pinned resident for its callbacks.

Read-only anchors from Refit are independently compared with the pinned image:

| Concept | Preferred VA / field |
| --- | --- |
| Base widget vtable | `0x6bb2d4` (excluded while constructing/destructing) |
| Combo vtable | `0x6bad2c`; slot 8 points to `0x59d6e0` |
| Button HDC update method | `0x5a4af0`, vtable slot 8 |
| Label Draw method | `0x5a7590`, vtable slot 1 |
| std::string label vtable | `0x6bbaa4` |
| Selection-box bounds | vtable `0x6bb0e4`, rectangle `+0x178` |
| Button HWND, label, bounds | `+0x20`, `+0x3c`, `+0xc4` |
| Shown / enabled / mode | `+0xe4` / `+0xe8` / `+0xc0` |
| Combo item height / pull rectangle | `+0x17c` / `+0x180` |
| Combo open / hidden HWND / direction | `+0x190` / `+0x198` / `+0x1b4` |
| Label HWND / string / bounds / shown | `+4` / `+8` / `+0x408` / `+0x45c` |

Unknown methods, invalid HWNDs, unreadable fields and invalid rectangles are
rejected. Combo data uses CB_GETCOUNT/CURSEL/LBTEXT properties, never activation
or generated user input. Object records are thread-local and bounded.

Only the exact Instant Action background asset can admit a `#32770` dialog.
The ShellDisplay logical canvas, exact native dialog input bridge, unique complete
eight-by-three combo model, and navigation controls must all be available.
Visible editable/unknown native child interfaces keep the whole dialog native.
Read-only status HWNDs are hidden without activation only after successful paint
and restored on fallback/shutdown. Native UI thread ownership is retained.

The installed FO callback RVA `0x1d0768` registers **1200x900**, as confirmed in
the current DLL disassembly and actual log. The older rebuilt source's 800x600
constant is stale for this file. Logical canvas size and widget field coordinates
are therefore treated separately. The layout admits only a unique complete
800x600 or 1200x900 widget profile, normalizes it for layout, and maps real pointer
messages back through that profile and ShellDisplay's actual canvas size. A
mismatch or ambiguous profile retains native presentation. This is validated
geometry handling; which profile the live widget model selects remains a manual
acceptance observation, recorded once in the activation log.

The additive `sdk/include/a2fo_shell_canvas.h` API exposes copies, not borrowed
internal DCs: `A2FO_CopyShellCanvasV1`, `A2FO_SetShellCanvasPresenterV1`, and
`A2FO_ShellDialogBridgeReadyV1`. The last checks the current DWLP_DLGPROC, not just
historical registration. Refit checks it before both presentation and pointer
translation. Presenter FALSE/exception restores original canvas presentation.
The optional callback runs at canvas finish, preventing native controls from
flashing underneath a completed overlay. Existing windows without a presenter
use their original finish path. No synthetic mouse/key events, focus changes,
desktop automation or actual game launches are part of verification.

## Evidence and acceptance boundary

Build and test logs are under `build/refit-instant-action-*`. Portable checks
include the prior seven suites plus setup layout, coordinate profiles, unknown
controls/fallback, palette/preview/nav mapping, dropdowns and labels. Memory-only
Wine fixtures cover canvas copy/presenter success/exception/fallback, lifecycle,
dialog bridge ownership/change/failure, scrolling and GDI rendering at 1920x1080
and 1280x720. The four-profile console matrix tests the real menu DLL without
starting game entrypoints. The isolated GPU worker tests retained 1920x1080
main/Klingon/Borg frames, setup transitions and shutdown.

The hidden native fixture validates the pinned file identity before copying PE
sections into its private fixture allocation. Wine's LoadLibrary(EXE) mapping is
not at the preferred base. Only the fixture copies of three verified vtable anchors
are rebased; production tables are untouched. The unchanged, verified 21-byte
constructor is executed directly to check its ABI and observer pass-through.
The fixture never resolves imports, runs TLS callbacks or invokes a game entrypoint.
This is not proof of the full game hook or live setup widget model.

After a complete restart, open Instant Action and check the log for
`Instant Action widget observer armed` and `Instant Action Refit layout active`.
Confirm eight player rows, colour palette, race/team choices, map selection,
preview/start positions, settings, launch/load and returning to the main menu.
Open/close a dropdown near the bottom and scroll a long list. Verify that the
background stays animated and that returning does not reload the scene. Also
check the accepted borderless/windowed modes. Preserve the fresh log before
restarting if fallback occurs. Live usability and visual acceptance remain pending.

## Completed checks

- Strict MinGW DLL/fixture builds and eight portable suites passed.
- Canvas copy/presenter, lifecycle, synthetic dialog bridge, cached scrolling,
  and setup GDI fixtures passed in the dedicated Wine prefix.
- The hidden widget decoder/unchanged constructor fixture passed against the
  installed ArmadaL file. It is isolated from the live game and desktop.
- The real GPU worker passed setup transition/cache retention and Main/Klingon/
  Borg frame/shutdown checks at 1920x1080. Measured Main preparation was 4059 ms;
  the old main timer did not pause setup. This does not establish live menu FPS.
- Independent review found and prompted the dialog-bridge readiness gate; the
  follow-up review found no remaining blocker in that scope.
- The final console matrix and deployment hashes are recorded in the receipt.

## Attribution and rollback

Layout/field references derive from Cedric Haase's MIT-licensed armada2-refit,
commit `37d60079169b086dba4216fe55e05cd6015d66c0`. Preserve its MIT notice,
`third_party/armada2-refit/SOURCE.md`, and the separate unchanged Noto font OFL.
Game assets remain installed content and are not distributed by this change.
Only the host menu DLL and ShellDisplay DLL are deployment candidates. The
adjacent deployment receipt records their previous/new hashes, backup, protected
files, source snapshot and executed checks. Roll back both DLLs together with
the game closed; do not replace the accepted renderer provider/configs.

## Follow-up: native screen remained visible

The user's subsequent screenshot still shows the native Instant Action controls
and old navy underlays. The fresh log confirms the observer armed and the
1200x900 #32770 bridge/canvas, but contains no setup admission/model/activation
message. Thus the deployed candidate did not establish live layout activation.
The prior fixture passes did not cover the real background-to-observer route.

Source review found that only legacy ShellBitmap paints called the observer;
the shared TImage catalogue-composition path could draw the same underlay without
registration. It also found that a logical memory DC was resolved using active
window state. Both paths now admit the exact named background in compose_scene.
The additive `A2FO_ShellCanvasWindowV1` resolves a logical DC from its current
thread-local paint lease. Unknown DCs still return null; no HWND relationship is
guessed. Redundant bitmap-pointer tracking/snapshots have been removed.

Observer admission now logs bounded reasons for an unready observer, wrong
thread/class, unavailable bridge/canvas, unsupported dimensions, subclass or
presenter-registration failure, and successful registration. The exact #32770
and unique complete model requirements remain. This closes known static path
gaps; live activation is not claimed until a restart and new screenshot/log.
The unmapped relationship between a possible TMultiplayerSetup VCL form and the
generic native dialog wrapper remains an explicit uncertainty.

The memory fixture checks lease resolution while active, rejection for unknown
DCs/disabled canvas, and disappearance after release. Source review and strict
compilation cover both composition call paths. New evidence is recorded under
`build/refit-setup-admission-*` and the follow-up deployment receipt.

## Follow-up: logical canvas overwritten by a physical resize

The next user screenshot remained native. The new admission diagnostic narrowed
this to `unsupported logical canvas 1920x1080`, despite registration at 1200x900.
The same log reports a 3072x1296 scene, exactly the second application of the
registered 1.6x1.2 scale. `move_window`/`position_window` previously overwrote a
registered root's authored dimensions with every incoming size before applying
that scale. The later physical-sized caller is not mapped; the immediate native
registration source itself forwards its authored size unchanged.

A synthetic lifecycle regression fails on the previous implementation with
`native MoveWindow owner fit does not scale dialog twice` (saved before log).
The correction admits only an exact owner-client physical-fit request for a
registered borderless native #32770 root whose dialog callback is still owned by
ShellDisplay. It retains the registered logical dimensions and lets the existing
bounds conversion produce one physical fit. Unmatched origin/size requests,
ordinary VCL roots, children, changed callbacks and SWP_NOSIZE retain their
existing behavior. SWP_NOMOVE preserves the authored position. Successful
normalization logs dimensions, origin and the native caller address once.

The green lifecycle test also verifies the exported 1200x900 canvas, an offset
owner origin, both APIs, repeated calls/recovery, flag handling, ordinary authored
resizing and callback ownership. The canvas/presenter, lifecycle, dialog,
scrolling and GDI suite passes after the correction. Only ShellDisplay is a new
deployment candidate for this follow-up; RefitMenus, renderer and settings stay
as installed. Live activation still requires the actual complete widget model
and cannot be claimed from this synthetic regression alone.

## Follow-up: native right status pane rejected

The next actual log proves that physical-fit normalization ran (caller
`0x00607a99`), the logical canvas stayed 1200x900, and the setup presenter
registered. It then reports `a visible native child needs its original
interface`. This is a later gate than the earlier admission/resize failures.
The former child scan admitted a read-only Edit only at x<450 and y>=300;
Instant Action's right status pane could never pass that positional rule.

The pinned ArmadaL RT_DIALOG resource **2096 / 0x830** contains 55 initially
hidden COMBOBOX controls and four visible Edit templates. IDs **2093 / 0x82d**
and **2120 / 0x848** have style `0x50000844` (read-only, multiline,
autoscroll). IDs 2094 and 2138 are editable. In the GameSetup initialization
branch at preferred VA `0x5c5522..0x5c5593`, the first three Edit HWNDs are
hidden; the right pane, 2120, is positioned at authored
`(1023,511,150,210)` and bound to the status output. The captured disassembly
is `build/refit-setup-native-disassembly.txt`. These are binary/resource
observations; the live log did not previously print the rejected child's ID.

The new scan admits only direct child EDITs with those two known read-only
multiline IDs, valid positive in-canvas bounds, and current matching styles.
It rechecks cached panes and reads their text without interaction. Explicit
message metadata lets the right-hand native pane move below Players without
being mistaken for map information. Unknown or editable children still retain
the whole native interface. Any future rejection logs class, ID and style.
Only panes hidden by this adapter are restored; visibility ownership precedes
USER32 calls, survives failed restoration for retry, and handles synchronous
restoration during hide. The presenter retains its session across destruction.

The expanded hidden fixture exercises the **complete snapshot path**, not just
the pure layout algorithm: 24 decoded combos, two native navigation widgets,
1200x900 canvas versus 800x600 widget coordinates, and right Edit 2120. It failed
on the former scan (`build/refit-status-pane-before.log`) and passes after the
change. Interactive Edit rejection and visibility ownership/failure/reentrancy
are checked with visibility calls stubbed; no real test HWND is shown. Pure
layout checks cover both coordinate profiles, message priority, multiple panes,
and noninteractive message hit testing. Memory-only GDI checks remain separate
from actual game presentation and clicks.

This follow-up replaces only **A2FORefitMenus.dll**. The currently installed
ShellDisplay normalization, accepted scene provider, renderer configuration,
and INI are preserved. The deployment receipt records exact hashes and rollback.
Fresh restart, `Instant Action Refit layout active` in the log, and visual/action
acceptance are still required before calling the live screen corrected.

## Follow-up: team swatches and minimap sampled the wrong rows

The following user screenshot and fresh log confirm **live layout activation**,
with `widget space=1200x900, canvas=1200x900`. The previous admission blocker is
resolved. However, the colour chips contain dark background pixels and the map
preview is blank. The native bitmap crop, not the selected team colours, is wrong.

A memory-only asymmetric GDI probe demonstrates the failure: for a top-down
12-row DIB, `StretchDIBits` with source y=2 and height=2 reads row 8 on this Wine
runtime. The setup's whole-canvas crops had the same nonzero-y pattern. The new
pixel fixture fails on the prior code: row 0's expected RGB `0xf12b35` becomes
background RGB `0x101620` (`build/refit-colour-crop-before.log`). The former
uniform-colour rendering fixture could not detect this coordinate error.

`setup_presentation.cpp::art` now describes a top-down **window of source rows**:
the pixel pointer starts at the requested top row, the header keeps the original
image width/stride but uses the requested row count, and `ySrc` is zero. This
avoids depending on a Wine-specific y inversion; the complete vertical extent is
copied on either interpretation. Positive crop bounds are checked without signed
addition overflow, and zero copied scanlines now count as presentation failure.
Whole-background rendering retains the same pointer and dimensions; no temporary
pixel buffer or additional allocation is introduced. Team values, colour channels,
widget fields, hit coordinates and native actions are unchanged.

Microsoft's [StretchDIBits documentation](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits)
describes the top-down DIB origin and zero-scanline failure semantics. It does not
justify applying the observed Wine y inversion to all Windows targets, hence the
zero-y row-window approach. Native Windows visual acceptance remains separate.

The expanded memory-DC fixture checks eight distinct player swatches, four
palette chips and four asymmetric minimap quadrants, through the real layout and
paint functions. Both 800x600 and 1200x900 widget/canvas profiles are tested at
1920x1080 and 1280x720 output. No game entrypoint, visible window or generated
input is involved. Only the menu DLL is a deployment candidate; the renderer,
ShellDisplay and configuration remain installed as before. The colour-crop
deployment receipt records the final test outcomes and rollback backup.

## Transparent PNG skin over the retained scene

At the user's request, fixed Instant Action artwork now uses generated RGBA
PNGs in `assets/refit-menus/ui/`. The pack contains panel/header artwork and
normal, hover, pressed, disabled and dropdown states. Transparent cut corners
and translucent interiors reveal the darkened animated scene. The game still
supplies every changing caption, selection, colour and map pixel. Empty native
status messages no longer draw an empty panel over the scene.

`tools/generate_refit_menu_skin.py --pack` preserves the eight editable source
PNGs and generates an atlas plus the embedded premultiplied BGRA resource 301.
The optional default generator recreates the reference design; `--check` modes
are read-only. The package uses no new game artwork. Existing source and font
attribution remain. See the asset directory's README for editing/build steps.

`setup_skin.cpp` validates the resource and builds resized nine-slice DIBs on
demand. The UI thread's cache is bounded to 96 components and 8 million pixels;
each component takes one AlphaBlend per draw, with no PNG decoding or resizing
in a warm frame. The cached pixels contain only the translucent UI artwork,
never a captured background, so the scene continues animating underneath.
Invalid/missing skin resources retain the prior GDI artwork. This integration
currently covers Instant Action, not all other menus' native layouts.

The memory-only render fixture requires the real linked skin resource, then
tests all eight parts over two different background colours at two component
sizes. It verifies transparent corners and composited centre RGB within GDI
rounding tolerance, including repeated cache keys. The original asymmetric
swatch/palette/minimap assertions still run at both coordinate profiles and
both output sizes. Linked tests, final DLL checks and deployment details are
recorded in `refit-transparent-ui-20261019-deployment.json`; actual game
transparency, menu responsiveness and action acceptance still need a restart.

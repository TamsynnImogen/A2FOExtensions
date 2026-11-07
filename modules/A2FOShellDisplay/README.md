# A2FOShellDisplay

Optional native module for the Fleet Operations shell's main game window.
It uses the primary resolution selected in Graphics Options, displays a
framed window when **Window Mode** is checked, and borderless fullscreen when
it is unchecked. Borderless fills the selected adapter's entire monitor,
including at lower selected rendering resolutions; the framed window keeps
the selected client size and is centred in the work area. The shell continues using a
non-exclusive Direct3D device. Version 6 also owns Fleet Operations/Armada
movie presentation: startup intro windows inherit the shell's framed/borderless
state and client geometry, while intro and full-window movie frames are fitted
and centred without changing their aspect ratio. Native embedded gameplay
movies keep their authored rectangles. The existing full-window shell canvas,
Options geometry correction and checkbox hit-test fixes remain enabled. Gameplay
retains its existing display behavior.

The October 21 minimap correction keeps movie texture UV reciprocals private
to the Bink texture-setup instructions. The original shared `1/128` and `1/256`
constants remain untouched; radar also uses them. The 512×256 movie texture and
its two UV operands are committed as one checked instruction-span patch, which
leaves the original setup unchanged on failure. See the
[capture, regression checks and manual test](../../docs/renderer/minimap-shared-uv-fix-20261021.md).

## Install and build

```sh
make shell-display
make shell-display-test
```

Copy `build/modules/A2FOShellDisplay.dll` to `Data/modules/` and enable
`A2FOShellDisplay` in the module-selection menu. Alternatively append the next
unused `activeN = "A2FOShellDisplay"` entry to the relevant `[modules]` section
of `info.ini`. A mod with its own `[modules]` selection must enable it there.
Restart the game to load or remove a native module. No extra INI is required.

## Implementation and evidence

The code follows the recovered shell/window routines, rather than polling or
restyling windows from a background monitor. The old core monitor remains
disabled. Only this new DLL, its build target, tests and documentation are added.

| Site | Role |
| --- | --- |
| FO RVA `0x1d3520` | Scope the existing thiscall/RET12 display wrapper to primary mode 3, outside editor mode. Capture the fullscreen preference before FO temporarily clears it. |
| Armada RVA `0x22c4e0` | Replace only the mode-3 fixed-resolution lookup with the already selected primary mode descriptor. Other displays/lookups use the original method. |
| FO RVA `0x1d0310` | Wrap the cdecl native window callback. Preserve selected device dimensions and change its border parameter in the shell scope. |
| FO RVAs `0xad390`, `0xad35c` | Call genuine Delphi client height/width setters after the callback: monitor size for borderless, selected size for framed. Bypass the legacy 1200x900 minimum in the shell only. |
| FO RVAs `0xb2708`, `0xb2744` | Position through native VCL setters on the selected D3D adapter monitor, without activation or input synthesis. |
| FO RVA `0x173a64` | Wrap native child-dialog registration, keeping its logical canvas and scaling registered HWNDs to the selected output. Delphi EAX/EDX/ECX plus three stack arguments, RET12. |
| FO RVA `0xcdbc8` | Keep registered controls' cached VCL bounds logical when `TWinControl.UpdateBounds` reads physical HWND geometry. Preserve the native `AdjustSize` call at `0xc4c20`; unregistered controls forward unchanged. Delphi EAX=self, no stack arguments. |
| FO CALL RVA `0x166323` | Adapt the custom `TJvgCheckBox` release-time cursor query to its logical parent canvas. Preserve the Win32 stdcall ABI and native failure; outside a tracked release dispatch the query passes through. |
| Armada and FO USER32 imports | Adapt logical paint DCs, client rectangles, child geometry, invalidation and mouse coordinates for registered shell windows only. |
| Fleet Ops RVA `0x1aa6ab` | Route TBinkIntro rendering through the shared shell presentation snapshot; resize/restyle its HWND and use an aspect-fitted D3D9 viewport. |
| Armada RVA `0x0639ba` | Scale the original GDI Bink path into the same shell-sized client area while preserving movie aspect ratio. |
| FO pointer RVA `0x210874`, Armada CALL RVA `0x0638af` | Scope native intro frame capture to `TBinkIntro.play`, fit decoded 32-bit frames to the client using GDI, and retain native flip fallback. |
| Armada RVAs `0x0e5da5`, `0x0e6153` | Aspect-fit full-window texture movies; embedded viewscreen/gameplay movie rectangles remain native. |

The selected mode is `engine+0x104 -> configuration+0x24`; primary device is
`configuration+0x1c` and fullscreen preference is byte `configuration+1`.
Profile `+0x84` excludes the editor. FO restores its configuration byte and
`reallyFullscreen` lifecycle through its original wrapper. A changed shell
selection forces the native recreation path so a same-resolution border toggle
also runs the window callback. Repeated unchanged shell entries do not force it.
Profile and D3D helper slots use three pointer loads: helper slot, FO's native
pointer variable, native global, then the actual object. Both chains match the
original instructions.

The source references are
`reverse_engineering/FleetOpsHook/rebuild/src/hooks/window_hooks.cpp`,
`rebuild/src/ui/graphic_options_form.cpp`,
`notes/clean-baseline/window-clean.asm.txt`, and the original and installed
binary disassembly. The legacy/deployed clamp caves are at FO `0x1d304f`
and `0x1d3066`. The rebuild's clean source directly calls the genuine setters.

FO support is pinned to file length 3,043,840 and whole-file CRC32 profiles.
The October 12 Roots profile also requires its complete SHA-256:

| Input | SHA-256 | CRC32 |
| --- | --- | --- |
| Clean RE DLL | `b53e91281be33ee998e30bd971b6c944cb4f5fb2174fb9f0cc756ca3ef94e8a9` | `07bd1e4f` |
| Original checkout DLL | `0843058788c6ffd1138d69871e2d9b1911bb6be8ee06db42f59f56b9a4494a97` | `3ed28fbe` |
| September Roots DLL | `d2a0829e87aaa927e27146b90be61fc6359c6cbf3d81aed2c644019dd375b9b6` | `46a0dd96` |
| October 12 Roots DLL | `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11` | `7cd6661e` |

The October profile uses `modules/refit_common/fleetops_profile.hpp`, shared
with the Refit menu adapter. All existing live instruction and hook-owner
checks remain required. The Refit menu draws in the same selected display:
`WindowMode=False` requests borderless fullscreen; zero saved display width
and height select the adapter desktop mode. See the
[October fullscreen integration record](../../docs/refit-menu-fullscreen-roots-2026-10-18.md).
The v15 follow-up separates physical monitor dimensions from render resolution;
see [the borderless sizing record](../../docs/borderless-monitor-2026-10-18.md).

Armada uses the shared supported-image identity checker, including the
September 30 Roots engine. Live instruction signatures reject competing hook
owners. The startup CALL may still be native or already redirected by FO;
FO's later installer reaches the same wrapped function. Unknown identities or
signatures install nothing. If a later hook installation fails, the DLL remains
resident with its installed callbacks disabled and passing through: the core's
registration rollback does not undo executable patches.
Gateways and runtime enablement are published atomically. A callback arriving
during the core's jump-before-gateway publication interval yields until that
gateway is available, then forwards to it.
`commonMemoryFixes` also changes the shell depth immediate at Armada `0x22c4d5`
from 16 to 32 during startup (one-byte payload at FO `0x1cd680`, writer at
`0x1ccda2`). Preflight accepts both evidenced states and rejects other depths.

### Shared shell / intro presentation

Version 14 bridges registered native `#32770` dialogs' game-owned
`DWLP_DLGPROC` callbacks. The Windows outer/default dialog procedure keeps
physical coordinates; Armada's custom handler receives the same logical
mouse and geometry messages as other shell handlers. Callback ownership
must resolve to one of the verified game images. Unknown owners and failed
installation retain the native path with a diagnostic. See
[`docs/shell-windows-dialog-v14-2026-10-02.md`](../../docs/shell-windows-dialog-v14-2026-10-02.md).

Version 13 adapts the original NextGrid `ScrollWindowEx` path so the persistent
logical canvas moves with the visible rows. Scroll distances, input rectangles
and returned update geometry cross the logical/physical boundary correctly,
including aliased clip/update rectangles. Partial paints expose only their
outward-rounded dirty area with a one-pixel guard, instead of requesting a full
menu repaint. Child-moving and smooth-scroll calls retain their original path.
The buffered v12 intro presenter remains in place. See
[`docs/shell-mod-scroll-v13-2026-10-02.md`](../../docs/shell-mod-scroll-v13-2026-10-02.md).

Version 12 removes the visible black-clear step between native intro frames.
It composes each complete frame and its bars in a reusable offscreen DIB while
the decoded pixels are valid, then performs one window blit after the original
engine method releases its video surface. Sound, timing, skipping and native
fallback remain unchanged. See
[`docs/shell-intro-flicker-v12-2026-10-02.md`](../../docs/shell-intro-flicker-v12-2026-10-02.md).

Version 11 fits decoded frames from the native engine intro player into the
actual saved-size client window. The test mod's official 640x480 A2 videos
produce a centred 1440x1080 picture at 1920x1080, with black side bars. A scoped
Bink copy hook presents supported 32-bit pixels using GDI while the native
player retains decoding, timing, sound and skipping. Unsupported formats or
failed GDI presentation fall back to the original engine flip. Other movie
callers retain native behavior. See
[`docs/shell-intro-native-fit-v11-2026-10-02.md`](../../docs/shell-intro-native-fit-v11-2026-10-02.md).

`tests/shell_native_intro_test.cpp` exercises the real presenter against a
memory DIB and synthetic callbacks, including aspect fitting, source stride,
black bars, orientation, scoped forwarding and native fallback. It creates no
game window. Visible playback and the transition back to the shell require
manual acceptance.

Version 10 corrects the source of the intro resolution. A live test showed the
engine temporarily publishing 1280x960 before native movie playback, despite
the saved desktop setting and following shell using 1920x1080. Movie entry now
refreshes the presentation from the loaded mod's FoSettings, replacing the
temporary mode and client dimensions only when the saved selection differs.
This includes the saved 0x0 desktop selection. Nested menu movies still bypass
the refresh. See
[`docs/shell-intro-resolution-v10-2026-10-02.md`](../../docs/shell-intro-resolution-v10-2026-10-02.md).

Version 9 applies the saved client resolution at the movie-start call shared by
both the D3D9 and native engine renderers. For D3D9 it also resizes the native
window before the intro constructor creates its auto-sized device. The main
VCL form uses the genuine client setters to avoid the legacy size clamp; cached
presentation is reused only while the actual client geometry/style still match.
The shell resize callback now forwards the selected dimensions too.

`tests/shell_intro_test.cpp` includes the real movie implementation with fake
USER32/VCL methods. It maps original and installed images as data to verify the
new CALL signatures, selected framed/borderless dimensions, both movie-start
return paths, native size override/retry, nested menu bypass and installation
failure/UV rollback. It creates no HWNDs and executes no native game methods.
Actual video playback, device creation and the transition back to the shell
still require manual acceptance. See
[`docs/shell-intro-resolution-v9-2026-10-02.md`](../../docs/shell-intro-resolution-v9-2026-10-02.md).

Version 7 fixes startup ordering: the intro can run before the native shell has
published its first presentation snapshot. The Bink hook therefore seeds the
initial width, height, bit depth, adapter and Window Mode state directly from
FleetOpsHook's already-loaded `TFoSettings` singleton. Once the shell activates,
the normal live presentation snapshot replaces that seed. The startup proxy also
joins deferred `A2FO_Initialize` on Armada's first task-switch export call, so
time-sensitive intro hooks are installed deterministically without loading native
modules from `DllMain`.


`presentation.hpp` publishes the selected primary mode, Window Mode state and
actual shell client rectangle as a small lock-free snapshot. The shell activation
hook publishes the graphics selection before the native transition, and the
window callback refreshes it with the resulting HWND/client size. This gives the
startup movie path the same source of truth without polling `info.ini` or changing
FO's Direct3D fullscreen lifecycle.

Fleet Ops' TBinkIntro handle exposes the decoded movie width/height used by its
native renderer. The wrapper fits that aspect ratio into the current render target
and centres the viewport. If the movie uses a separate HWND, only frame-related
window style bits are mirrored from the shell window (or reconstructed from the
published borderless flag during early startup), then it is centred on the same
monitor using the shell's client geometry. No display-mode switch is introduced.

### Full-window canvas

The registered native dialog keeps its original 800x600 or 1200x900 coordinate
system. Completed GDI drawing is stretched independently in X and Y to fill
the selected client size. This intentionally stretches the original aspect
ratio when the selected resolution differs. Bitmap buttons and their native
hit masks remain in the same logical coordinate system; mouse positions are
converted back before native VCL/Armada dispatch. Native Windows edit/list
controls retain physical system-procedure input and receive scaled HWND fonts.
Dirty rectangles/regions are scaled too, so hover redraws target the right area.

Each drawing acquisition has a distinct HDC and bitmap object. The bitmaps
share a persistent file mapping, keeping completed pixels while preserving
independent DC state and permitting releases in any order. Mapping handles are
duplicated for each acquisition; GDI is flushed before sharing/presentation.
See Microsoft's [CreateDIBSection contract](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection).
The final physical DC retains USER32's clipping region and is restored after
the blit. Registration records remain logical, so native window movement does
not restore the old physical canvas size. Existing records are refitted when
the primary shell resolution changes. The scale uses the main form's actual
client size, which can be smaller than a desktop-sized windowed selection
(1920x1060 was observed for the 1920x1080 device).

VCL's native `UpdateBounds` queries `GetWindowRect` on size/position messages,
then writes Left/Top/Width/Height at `+0x40..+0x4c`. Translating the messages
alone did not prevent physical dimensions entering those fields in v3.
Options' `FormShow` uses those cached dimensions to centre its button rows,
which then passed through another scale. Version 4 retains logical cached
bounds and calls the original `AdjustSize` tail for registered controls.
An owned `WS_POPUP` waits for explicit shell registration; only `WS_CHILD`
controls inherit an existing canvas.

Custom `TJvgCheckBox` controls are painted onto their parent, with no separate
HWND. Their native release handler at `0x1662fc` re-reads `GetCursorPos`, then
calls Delphi `TControl.ScreenToClient` (`0xc5254`). That method subtracts the
parent's physical screen origin plus the checkbox's logical Left/Top; it does
not use the adapted USER32 `ScreenToClient` import. Version 5 converts that
single cursor query into a logical parent offset anchored to the same physical
origin during a tracked `WM_LBUTTONUP`. Native glyph/label bounds and state
changes remain in the original handler. Global cursor queries and native
Windows checkbox input keep their existing paths. No cursor is moved.

FO's startup replaces Armada's old background capture at `0x2083f0` with
FO's `SetCurrentBackgroundNew` at `0x1f8f1c`. The installed replacement captures
1200x900, matching its authored shell canvas. The native snapshot/clean path
is retained; the old Armada 800x600 implementation is not the active path.

The main form's HWND is at `+0x1b4`, verified by the actual `TWinControl.GetHandle`
body at FO `0xcd81c` in all three supported DLLs. Native default-procedure
forwarding is evidenced at FO `0xcaeec` and `0x9f4cb`; opaque Windows procedure
tokens are passed through `CallWindowProcA`, never directly invoked.

Only registered native shell canvases are adapted, on their UI thread. The
main game window, gameplay/editor rendering, responsive A2FOMissionSelector
and unregistered small popup dialogs retain their existing paths. Third-party
module imports are untouched. Foreign IAT owners disable the canvas layer;
resolution/border hooks remain available. Partial installation stays inert.

## Validation and limits

`tests/shell_display_native_smoke.cpp` loads the actual module and maps game
images as data without their entry points. Synthetic gateways test selected
mode forwarding, borders, exact client-size setter arguments, forced recreation,
editor/gameplay/secondary exclusions, invalid selections, startup CALL ownership,
identity/signature rejection and partial-install behavior. The genuine VCL and
D3D methods are not executed by this test. The assembly bridge is inspected and
exercised against synthetic Delphi-register setter stubs.
Regression coverage includes the actual startup depth payload, the three-load
profile chain and synthetic D3D8/D3D9 monitor vtables.

The first manual launch on 2026-10-02 showed the v1 DLL being rejected with
`Native shell resolution block mismatch`; no v1 patches were installed.
Version 2 corrected the startup depth check and pointer chains; the user
confirmed its basic resolution/border fix on 2026-10-02. The user reported
that v3 regressed when opening Options: the menu became partial-size and
returning to the game showed background without UI. The user reported v4
seemed to work, with everything else fine, but checkbox clicks failed.
**Version 5 addresses the separate checkbox query and awaits live acceptance.**

`tests/shell_canvas_test.cpp` exercises actual GDI memory DIBs, native-style
bitmap uploads, text/control regions, last-row/column coverage, inverse clicks,
negative coordinates, partial paints, DC state, independent shared acquisitions
and mapping lifetime. Outputs tested: 1920x1080, 1600x900, 800x600, 640x480.
The actual DLL smoke resolves the pinned images' USER32 import slots and checks
canvas installation, Delphi registration and bounds-hook ABI, the checkbox
stdcall query callback, partial-install pass-through and foreign import/signature
rejection (including the checkbox CALL). It creates no HWNDs and
executes no original game methods.

`tests/shell_canvas_lifecycle_test.cpp` includes the real canvas implementation
with a synthetic USER32 host. It checks registration, cached logical bounds,
actual 1920x1060 client fitting, popup versus child adoption, Options button
centering and inverse clicks, a 1280x960 round trip, Options close/menu return,
and untracked game geometry forwarding. The native `AdjustSize` target is a
test RET. Its controlled cursor host also models native checkbox origin
subtraction and `PtInRect`: glyph and caption toggles, outside misses, resize,
negative screen origin, inactive/untracked queries and cursor-query failure
are covered. Replacing the adapter with the old raw query fails this fixture.
These tests do not establish live VCL layout, native list/edit
behavior, gameplay resume or graphics-options changes.

`tests/shell_canvas_dialog_test.cpp` models Windows' separate outer dialog
procedure and custom callback with a synthetic USER32 host. It exercises
physical default handling, logical game input, nested dispatch, native-control
forwarding and callback ownership/lifecycle. Windows menu acceptance remains
pending; this fixture creates no HWNDs or original game objects.

The module takes effect on native shell transitions; an activation before
deferred module loading retains native behavior until the next transition.
Unusual custom HWND controls, non-client drawing and undocumented mod-specific
paint paths remain live compatibility checks.

## Manual acceptance

1. Fully restart Fleet Ops. Check `Data/A2FOExtensions.log` for
   `[A2FOShellDisplay] Initialized v16`, `Full-window native shell canvas hooks
   installed` and `Shell canvas: 1200x900 -> ...; class=...; logical VCL bounds
   preserved` (or 800x600) diagnostics.
2. With intros enabled, restart once in Window Mode and once with Window Mode
   disabled. Verify the intro uses the same framed/borderless geometry as the
   shell that follows it, does not switch display mode between movie and menu,
   and preserves its source aspect ratio (letterbox/pillarbox as needed). Check
   `Intro window matched shell` in the log. The native engine path should log
   `Native intro frame fitted: source=640x480; client=1920x1080; destination=1440x1080 at 240,0`
   for the official A2 videos at desktop 1920x1080. The D3D9 path instead logs
   `Intro Bink viewport`. Version 12 native frame diagnostics also contain
   `buffered present after surface release`; check that the picture remains
   steady between frames.
3. Choose 1920x1080 (or another supported widescreen resolution),
   enable Window Mode and return to the main menu. Verify a framed window at
   the selected client size, with background artwork reaching all four edges.
   Check main-menu buttons, hover/pressed states and click targets. Open Instant
   Action, Multiplayer and Graphics Options; test lists, drop-downs and text
   entry. Close Options and repeat opening/closing it: the main menu must
   retain full-window backgrounds and controls. Check the mission selector
   still fills the window normally.
   Open the Fleet Ops mod selection screen. Scroll its list with the wheel,
   scrollbar arrows and thumb; reverse direction and select entries while
   scrolling. Rows, icons and selection highlights should refresh together
   without restoring old rows. The log should contain
   `Shell scroll canvas synchronized: class=TNextGrid`.
   Click both the glyph and caption of Graphics Options checkboxes, including
   Window Mode, and toggle them on and off. Check Sound Settings too. Clicking
   outside a checkbox must leave its state unchanged. Save, reopen, and verify
   the selected settings remain selected.
4. Disable Window Mode at the same resolution. Return to the main menu and
   verify borders disappear. Select the desktop resolution and verify the shell
   fills the selected display. Switch back to windowed at that same size.
5. Check a smaller supported size such as 800x600: background and controls
   should fit instead of overflowing. Change resolution again while Graphics
   Options is open, revisit menus and move the framed window to check redraws.
6. Launch a match, open its escape/graphics menus, return to the shell, then
   open the editor. Specifically check that resuming from the game's Options
   menu restores its UI, rather than showing only the background. Verify
   gameplay/editor behavior remains as before. Repeat
   using the installed D3D8/D3D9 path and secondary monitor if available.
7. Open Admiral's Log and visit its tabs. Its eight native popup panes must
   inherit the surrounding dialog's canvas instead of independently filling
   the desktop; the bottom buttons must remain visible and clickable. The log
   should contain `Nested shell pane` for the verified Fleet Ops pane callback
   at RVA `0x1d0850` (Armada dialog resource `0x123`). Other owned popups retain
   independent layout. Check taskbar/Alt-Tab while Options and the log are
   open: registered secondary menus/panes use `WS_EX_TOOLWINDOW` with
   `WS_EX_APPWINDOW` cleared, preserving ownership and activation behavior.
   Primary windows, ordinary child controls and unknown dialogs are excluded.

The lifecycle fixture covers inherited pane bounds at a negative desktop
origin, repeat registration, contained controls/Done hit bounds, and secondary
window styles. These are headless checks; native desktop presentation and the
complete Admiral's Log remain manual acceptance checks after a full restart.

Rollback to the confirmed display fix: restore `A2FOShellDisplay.dll` from the
v3 deployment backup and restart (that backup contains the proven v2 DLL).
The v4 deployment backup contains the regressed v3 DLL. Disable the module
and restart to remove all of its behavior. The v5 deployment backup contains
the v4 DLL with its user-reported working layout but failed checkbox clicks.
Original FO, Armada and
A2FOExtensions core DLLs are retained.


## Checkpoint — 2026-10-28: six render/UI families complete behind native adapters

This batch completes the remaining portable controllers and source/headless
validation for A2FOCraftIdentity (module and object editor),
A2FODirectionalShields, A2FOFeaturePack (recursive ODF, buildyard pseudo-technology,
queue enhancements and upgrade pods), A2FOShellDisplay (Bink and canvas),
A2FORGBTextures and A2FOTextureVariants: eleven inventory units in total.

Rust now controls CraftIdentity first-match fields/atlas lookup, full-byte string
trim, display-mode suffix admission, drawn predicates and visible hover setup;
RGB DDS/TGA publication traversal and precedence; FeaturePack ODF hash-chain
membership; directional effect-tree traversal; canvas ancestry, point-map pass
order and reverse lease selection; and texture scorch admission, publication
comparison and hardpoint selection. Native failure/reset paths and lazy reads
are preserved. Canvas ancestry releases each borrowed record before the next
lookup. Full u32 scorch bands and raw effect pointers remain data, separate
from callback error metadata. Display-mode suffix whitespace is classified
before the parsed-range decision, as in the original CRT path.

This is portable-controller completion, not a pure-Rust binary. Native code
retains Win32/COM/GDI/CRT and engine calls, hooks/signatures/addresses, checked
fields, native strings/maps/iterators/locks, exception scopes, exact GNU x87
projection/geometry/rounding primitives, and FeaturePack's existing STL sort.
The sort retains casefold-equivalent filename ordering and comparator/allocation
exception behavior. Object-editor and Bink controllers were already in Rust;
this batch supplies their current build and headless validation. Existing native
kernel files and ABI boundaries are pinned rather than rewritten speculatively.

Validation passed: 249 Rust tests; formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and all six standalone DLLs. Twenty native
suites produced 267 matching before/current trace pairs (534 executions,
6,832 stdout lines), with inherited mutation and allocation-failure cases.
A direct production Bink fixture compares 276 numeric rows across 12 x87 modes;
canvas fixtures cover ancestry depth bounds, reverse/scoped leases and point
mapping. Seven additional directional tree cases cover signed branches, missing
and unreadable nodes, the head sentinel and cycle bound in both module forms.
A separate production scorch comparison passes 228 rows across all 12 x87
precision/rounding modes, including guards, boundary bands, modulo hardpoint
selection, publication state, raw sticky/C1 status, CW and stack balance.
Built-in batch9 admission also passes. Runtime fixture assets are isolated per
execution and retained; both sides see the same logical working-directory path.

The source boundary check preserves 56 complete native files and all text
outside 16 approved controller bodies plus new adapter helpers in six files.
All 417 source-referenced Rust implementations link (the verifier checks 420
including three defaults). Core 197 exports/ordinals and each standalone export
set remain identical, with no new system imports. The build pins 791 product
inputs including owned .inl files. Six objects depending on changed sources
were rebuilt; remaining cache entries and 7,851 third-party source pins were
verified against the passed Squadrons predecessor. Original/current fixtures
use independently pinned pre-batch and candidate Rust archives. Established
fixture-only GNU ABI/custom allocator diagnostic flags remain recorded in the
compile commands. Failed link/probe attempts are preserved.

Evidence: build/rust-migration-stage10/render-ui-batch/checkpoint/validation.json.
The eleven units are completed_pending_census. Official totals remain 116/151
with 35 uncredited units: seventeen completed units await the shared strict
census refresh, and eighteen units remain unfinished. Frozen membership,
baseline provenance and strict archive/common-DLL bindings are unchanged.
Canonical artifacts and earlier checkpoints are preserved. No deployment or
real game launch occurred; GPU and in-game acceptance remain pending.

# Refit main-menu fullscreen integration — 18 October 2026

The user confirmed the new main-menu presentation works, but it still opened
in a framed window instead of matching the in-match display. The fresh
`Data/A2FOExtensions.log` established that RefitMenus armed and attached,
while A2FOShellDisplay rejected the installed FleetOpsHook identity. The menu
replacement was active; the selected-resolution display adapter was not.

The actual saved settings already request `WindowMode=False` with
`displayWidth=0` and `displayHeight=0`, meaning desktop resolution. No settings
change is needed. A2FOShellDisplay's existing implementation captures the
native fullscreen preference before FO temporarily clears it for display 3,
uses the selected primary mode and creates a borderless fullscreen window.
It preserves the non-exclusive shell device lifecycle and the existing
gameplay/editor/secondary-display behavior.

## Change

Admit only the reviewed October 12 Roots DLL in addition to the old profiles:
file size 3,043,840; CRC32 `7cd6661e`; full-file SHA-256
`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`.
The complete SHA check is shared at
`modules/refit_common/fleetops_profile.hpp`; the old menu helper name remains
available through its forwarding header. All display-hook, code-signature,
data and hook-owner checks remain in place. No new addresses or display
policies are introduced.

The paired installed ArmadaL SHA-256 remains
`817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`.
The prior October binary audit established that only 30 FO bytes differ from
the approved September reference, outside this display/menu/VCL/resource
scope. The October input is byte-identical to the retained release reference.

## Verification

- Rebuilt A2FOShellDisplay and A2FORefitMenus; portable menu suites pass.
- Existing ShellDisplay suite passes: canvas GDI, lifecycle, native-dialog
  dispatch, scrolling, intro, native intro and real-DLL display smoke.
- Display smoke also passes separately using the actual installed
  ArmadaL/FleetOpsHook pair. Images are mapped as data; display, resize and
  native gateways are synthetic. No game entry point or live window runs.
- Rebuilt menu passes 11/11 console processes with the installed pair,
  including the shared SHA helper, guarded admission and native action/ABI
  fixtures. Independent source review found no blocker in this change.

Evidence is under `build/refit-menu-fullscreen-{build,tests,installed,menus}-20261018.log`.
Tests use the explicit disposable Wine prefix with display variables removed.
No input devices, GUI windows or focus are automated.

## Deployment and remaining acceptance

Only `Data/modules/A2FOShellDisplay.dll` is replaced, with the previous DLL and
current logs backed up. The working RefitMenus DLL, engine, FleetOpsHook,
extension core, module lists and saved settings are preserved. A deployment
JSON receipt records before/after hashes and rollback paths.

After a complete exit/relaunch, the fresh log should show
`[A2FOShellDisplay] Initialized v14` and
`Shell window: borderless, requested=...`, with client dimensions matching
the selected desktop mode. RefitMenus should then log its main-menu
attachment. Manually confirm the window fills the display, the menu and
pointer align, and Instant Action/return-to-menu transitions retain the
selected display. The user has confirmed the menu presentation; live
fullscreen and combined display behavior remain pending this restart.

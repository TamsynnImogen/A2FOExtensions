# Graphics Options restoration and October Roots compatibility

Current build ID: `20261015-graphics-options-effect-row-2`.

## Effect-row correction (v2)

The user's live screenshot confirms the renderer selector and effect switches
are visible, but Emissive Maps overlaps the native Bump Mapping caption.
The v1 synthetic initial-coordinate checks did not establish final native
layout. The precise native setup operation responsible was not proven.

V2 positions both non-windowed effect checkboxes after parenting, native
glyph/font setup, captions and checked state. Their row starts at the live
native Bump Mapping control's `Top + Height + 8`, rather than deriving it from
the form scale. Reopening the page repositions the retained controls against
the current native bounds. `TControl.SetBounds` at checked RVA `0xc4b80` updates
VCL layout state and invalidates old/new rectangles. The existing Delphi
SetBounds bridge passes width then height, matching its `ret 8` convention;
it is used only for these non-windowed controls, never the windowed combos.
The log records the native bump top/height and both final effect tops.

Regression fixtures include post-setup position changes and changed native
bounds on reopening, with explicit separation from Bump Mapping, each other,
and the gamma label. The evidenced fixtures place the effects at y=457 for
Roots and y=313 for the clean layout. Live visual acceptance of v2 is pending.

The Graphics renderer selector and Emissive Maps / Specular Maps controls were
still implemented in `core/renderer_options.cpp`, but deferred initialization
no longer called their installer. The supplied remote extension log therefore
contained no Renderer Options initialization. The current system-renderer
safety policy also explicitly disabled mapped-material effects because its
saved DXVK state did not match the active DLL files.

This change restores the checked installer before native-module discovery.
It corrects the form HWND field from `+0x1c4` to `+0x1b4`, verified in the full
18-byte `TWinControl.GetHandle` implementation at FleetOpsHook RVA `0xcd81c`.
Added-control positions follow the native Bump Mapping control: the clean
800x600 form uses `(440,288)` and Roots' 1200x900 form uses `(660,432)`.
The restart message now sits below the sliders instead of crossing a native
checkbox row. The native Bump Mapping checkbox remains independently owned by
Fleet Operations. Installation is idempotent.

Supported renderer choices remain System Direct3D 9 (WineD3D under Wine) and
DXVK/Vulkan. A separate OpenGL backend is not implemented by the existing
selector/helper. Effect switches save immediately to `[Effects]` in
`Data/A2FORenderer.ini`; their rendering still requires managed DXVK. Renderer
changes require a full exit and relaunch and use the existing post-exit helper.
The user identified the missing third choice as a separate Windows OpenGL
wrapper. Its payload/integration was not found in the installed tree or retained
renderer source history; reconnecting that choice awaits its package/location.

## Installed engine rejection

The user's current installation is `/home/tamsynn/NVMeData/Fleet Ops Roots`.
Its actual `Data/A2FOExtensions.log` reported initialization followed by
`no built-in or approved A2FOArmadaProfiles.txt fingerprint matched` and
`No hooks installed`. Its startup proxy successfully attached the DLL.
This establishes identity rejection, rather than a missing startup import.

The installed engine pair matches `Updated_Roots_Engine_12_October_2026.zip`:

| Image | SHA-256 |
| --- | --- |
| ArmadaL.exe | `817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18` |
| FleetOpsHook.dll | `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11` |

Compared with the approved October 3 executable, all headers and section
layouts match; only 130 bytes differ. At RVA `0x359a5`, a seven-byte native
sequence becomes a jump plus NOPs to `0x404750`. That handler selects a pushed
flag from a native object field and returns to `0x359ac`; the overwritten
`.test` span ends at `0x4047dd`. Other raw sections and native rotation code
are unchanged. These areas do not overlap extension hook sites. The exact
new release is added as a second approval row; existing approval is retained.
The engine executable itself is not edited or replaced.

The paired DLL differs from the September baseline in 30 bytes. A conditional
branch at RVA `0x13f24a` now targets a 33-byte handler in raw `.text` padding
at RVA `0x1fe110`. All 23 Graphics/VCL signatures and all seven Collective XP
hook signatures remain unchanged. Collective XP separately checks the complete
virtual `.text` CRC. Its additional exact accepted CRC is `0x28c90d19`, still
requiring a supported native-rotation engine identity and every hook signature.
No address or ABI is supplied by the compatibility profile.

Raw comparisons and isolated binary copies are under
`.build/graphics-roots-20261015/`. The generated profile was reviewed against
these differences before inclusion. Existing per-hook rejection remains active.

## Validation

- Core and Collective XP MinGW builds pass. Existing unrelated compiler
  warnings remain; no warnings-as-errors result is claimed for the core.
- Graphics signature tests pass with the original, clean recovery and new
  installed FleetOpsHook DLLs mapped without executing their entry points.
- Synthetic FormShow tests cover the true HWND versus poisoned old offset,
  renderer/effect creation, form ownership, visibility, reopening/reuse,
  idempotent installation, foreign-image rejection, effect persistence, and
  ignoring the native Bump Mapping checkbox for extension effect settings.
- Both logical layout fixtures (800x600 and 1200x900) pass bounds checks.
- New-engine identity/profile suite: 28 mapped-image positive/negative checks,
  capability checks and malformed-memory checks pass.
- Native station-rotation preflight passes with no extension patches.
- Rebuilt Collective XP positive preflight and corrupted-award-site rejection
  pass on the new paired binaries. Recording callbacks do not patch the engine.
- The final deployed profile and DLLs also pass the native-rotation and
  Collective XP preflights against the installed engine pair.

Tests unset DISPLAY and WAYLAND_DISPLAY. No game launch, input injection,
window activation or live GUI automation was used. These checks establish
build/static/synthetic compatibility, not in-game or multiplayer acceptance.

## Deployment and manual acceptance

The update contains only `A2FOExtensions.dll`, `A2FOArmadaProfiles.txt`, and
`modules/A2FOCollectiveXP.dll`, with instructions and a deployment receipt.
The receipt records source/build/deployed hashes and the rollback backup path.
It preserves the pre-update local logs and files. It does not bundle or replace
ArmadaL.exe or FleetOpsHook.dll.

After a full manual restart:

1. Check the fresh extension log for `approved Roots compatibility profile
   accepted`, `Renderer options: restart-applied renderer and Game Monitor
   selectors enabled`, and the new build ID in the renderer audit log.
2. Open Options -> Graphics. Verify Renderer, Emissive Maps and Specular Maps
   are visible, alongside the native Bump Mapping checkbox. Leave/reopen the
   page, change each effect and confirm saved state.
3. Select DXVK/Vulkan, exit completely, let the helper finish and relaunch.
   Confirm `Managed DXVK payload detected as the active renderer`. An error
   about an unrelated active DLL requires inspecting/preserving that wrapper;
   do not bypass the helper's overwrite guard.
4. Load a known emissive/specular model and compare each switch. Exercise
   station rotation and a faction using Collective XP if enabled, including
   save/load. Preserve any fresh failed-session log before another launch.

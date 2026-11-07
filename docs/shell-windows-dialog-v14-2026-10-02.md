# Native Windows menu input correction — v14 candidate

## Report and evidence

The remote Windows user reports displaced interactive areas in Instant Action,
Multiplayer and Admiral's Log. The Linux installation works. Desktop scaling
was enabled, but changing it to 100% did not resolve the report. These facts do
not establish a DPI cause.

The supplied `/home/tamsynn/Downloads/A2FOExtensions.log` identifies shell
**v12**, successful extension/module loading, a 2560x1440 client, and registered
`TMainForm` and native `#32770` dialogs with a 1200x900 logical canvas.
The startup-proxy log contains accumulated launches without timestamps;
its isolated LoadLibrary error does not establish failure of the launch shown
in the successful extension log. Original logs and pre-change sources/DLL are
preserved in `.deployment-backups/windows-dialog-v14-20261002T145134Z`.

The existing outer HWND wrapper converts game messages to logical coordinates
and restores their physical values when calling Windows system procedures.
For native dialogs, the Windows default dialog procedure then calls the
separate custom dialog callback. That callback bypassed the conversion.
This is a structural gap; identifying it does not prove the remote symptom's
complete cause.

Static inspection of the installed Armada executable (SHA-256
`470bf8f4b0196e22fe179d3845cab671e74d9af19cf90a7dafed0b0bbd394318`)
confirms `CreateDialogParamA` at call RVA `0x1e3c04`, with game-owned callback
RVA `0x1f6010`, template `0x123`. The callback dispatches messages to a game
object supplied by `WM_INITDIALOG` or read from HWND `GWL_USERDATA`. The
USER32 IAT slot is RVA `0x3b80e0`, image base `0x400000`. A separate modal
call at RVA `0x100c77` supplies callback `0x100610` to `DialogBoxParamA`.
Resource IDs were not assigned menu names. Installed FleetOpsHook SHA-256 is
`d2a0829e87aaa927e27146b90be61fc6359c6cbf3d81aed2c644019dd375b9b6`.
Discovery used SHA-256, `objdump -p`, filtered IAT-call searches and bounded
disassembly; it did not execute game methods.

## Change

After adopting a registered `#32770` window, the canvas checks its
`DWLP_DLGPROC` callback's owning module. Only callbacks resolving to the
verified ArmadaL/FleetOpsHook images receive a bridge. The outer Windows
procedure retains physical coordinates. The bridge applies the existing
logical conversion to mouse, size and window-position messages at the game
callback boundary, and calls the original using genuine `CallWindowProcA`.
Return values, including FALSE/default handling, remain unchanged. Nested
dispatch preserves the original physical message for Windows forwarding.

Native child controls retain physical handling. Unknown/opaque callback owners
and installation failures retain the native callback with a log diagnostic.
Destruction restores the original callback only while the slot remains owned
by the bridge. Disabled shell hooks pass messages through. No process DPI
policy change is introduced. V13 scrolling and v12 buffered intros are retained.

The boundary follows Microsoft's
[DefDlgProcA contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defdlgproca),
[GetWindowLongPtrA callback contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptra)
and [SetWindowLongPtrA contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptra).

## Verification and Windows acceptance

The regression suite is `make shell-display-test`, with the separate synthetic
dialog host in `tests/shell_canvas_dialog_test.cpp`. Tests use fake HWND records
and memory surfaces, without input automation or game startup. Build identity
and completed checks are recorded in the adjacent candidate manifest.

Send the v14 candidate DLL to the affected Windows user. With the game closed,
back up and replace only `Data/modules/A2FOShellDisplay.dll`; retain module
enablement, then fully restart. Verify `Initialized v14` and
`Shell dialog input bridge installed: owner=ArmadaL; callback RVA=...`.
The first mouse message per bridged dialog logs physical/logical coordinates.

At the existing 2560x1440 setting, test hover, clicks, list selection, dropdowns
and text entry in Instant Action, Multiplayer and Admiral's Log. Check native
edit/list controls and dialog close/reopen, then repeat at a smaller selected
resolution. Preserve the fresh extension log before another launch overwrites
it. If the callback bridge reports skipped/failed, that log is needed to trace
the actual route. Retest intro playback and mod-menu scrolling.

On 2026-10-02, after receiving v14, the user reported: “seems to be working
now”. This records initial user confirmation for the reported Windows menu
alignment issue. The full per-control and resolution checks above were not
reported individually. The packaged DLL remains unchanged; its included
candidate manifest describes the build-time validation state. The working
Linux installation remains on v13.

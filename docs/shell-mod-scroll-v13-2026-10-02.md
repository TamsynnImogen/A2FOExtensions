# Fleet Ops mod selection scroll correction — v13

## Report and original consumer

The user accepted the v12 intro behavior, then reported laggy refresh while
scrolling the Fleet Ops mod selection screen. Its DFM identifies
`TModSettingsForm.grdModSelect` as a `TNextGrid` with slide rows.

Recovered `rebuild/src/ui/grid.cpp` is explicitly a structural stand-in; it
invalidates the whole grid when scrolling and does not reproduce the original
incremental scroll path. The installed FleetOpsHook DLL instead calls
`ScrollWindowEx` in `TNxCustomScrollControl.ScrollContent` at RVAs `0x17f048`
and `0x17f07e`, and in `TNextGrid.ShowRow` at `0x19ff8e`, `0x1a0045`,
`0x1a00af` and `0x1a01a0`. These calls use `SW_INVALIDATE` (2), and alias
the input clip rectangle with the output update rectangle. They reach the
USER32 import at RVA `0x24c0b4` through the thunk at `0x9684`.

V12 adapted paint and invalidation imports but left `ScrollWindowEx` native.
It therefore scrolled physical pixels using logical distances/rectangles,
without shifting the module's persistent logical backing pixels. A subsequent
partial repaint could publish stale cached rows. Its `BeginPaint` wrapper also
expanded every dirty rectangle into a whole logical menu repaint.

## Correction

For tracked shell windows and ordinary non-animated scrolls, the hook snapshots
the input rectangles before touching outputs, converts the native call into
physical coordinates, and converts its update outputs back to logical ones.
Successful scrolls also shift the existing logical backing canvas with
`ScrollDC`. The affected cached pixels are published with clipping to that
area, avoiding accumulated rounding drift. Newly exposed logical pixels are
invalidated in physical coordinates. Missing caches or failed scroll/present
operations request an affected-area repaint to recover backing pixels.

Partial `BeginPaint` leases now return an outward-rounded logical dirty
rectangle, with one extra logical pixel at each edge for scaled sampling.
The logical DC uses that clip while the native physical paint DC retains its
original clipping and original `PAINTSTRUCT` for `EndPaint`. The persistent
canvas preserves pixels outside that dirty region.

Untracked windows, disabled canvas hooks, child-moving scrolls and smooth
scrolls forward unchanged. No polling, timers, background painting, input
synthesis or new binary method hooks are introduced. The existing ownership
check for USER32 imports includes the added `ScrollWindowEx` slot. Buffered
intro playback is unchanged.

The API geometry and update-region contract follows Microsoft's
[ScrollWindowEx documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-scrollwindowex)
and [ScrollDC documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-scrolldc).

## Verification and manual acceptance

Run `make shell-display-test`; the new memory-DIB scroll fixture joins the
existing module, canvas, lifecycle and intro checks. It creates no real HWND
and does not execute original game methods. Deployment details and hashes are
in the adjacent v13 manifest.

Fully restart Fleet Ops and open its mod selection screen. Check wheel,
scrollbar arrows and thumb tracking in both directions, including changing
selection while scrolling. Rows, icons, text and selection highlights should
stay aligned and refresh without restoring stale rows. The log should show
`Initialized v13` and `Shell scroll canvas synchronized: class=TNextGrid`.
Repeat with a smaller saved size and check Options/main-menu partial redraws.
Intro behavior should remain as accepted in v12.

Headless fixtures verify cached pixel movement and paint/update geometry;
they do not establish perceived live responsiveness or exclude costs in native
mod detail/screenshot loading. Manual menu acceptance remains necessary.

# Native mod intro frame fitting — v11

## Live evidence and target

The v10 test log confirms the intro now uses the saved 1920x1080 client size,
and the user reported an improvement. The remaining picture sits at the top
left with unused space: this mod uses the native engine player, whose existing
video surface does not grow when the window does.

The user has now placed the official A2 `Activision.bik`, `Paramount.bik`,
`MadDocSoftware.bik` and `Intro.bik` directly in `Data/Mods/test/animations`.
All four have 640x480 source dimensions. This change targets their native
intro playback; it does not edit the assets or settings.

## Implementation and verified binary contract

`TBinkIntro.play` owns a thread-local scope for its native renderer and Bink
handle. FleetOpsHook's `NextBinkFrame` pointer at RVA `0x210874` is replaced
only when it still points to Armada RVA `0x637f0`, with its verified prologue.
The original four-argument cdecl method retains decoding, frame advancement,
timing and repeat handling. Its `show` flag is suppressed within this scope so
the original small-buffer flip does not overwrite the fitted image.
The installed `play` loop calls this slot synchronously at FO RVA `0x1aa69e`,
returning to the same play stack; the scope therefore covers its native frame
calls without changing other callers.

The Armada indirect CALL at RVA `0x638af` calls the seven-argument stdcall
`BinkCopyToBuffer` import. Its original copy runs first, returning zero on
success. The hook then presents those still-locked pixels using GDI. Supported
Bink copy formats 3 and 5 correspond to the verified 32-bit BGRx/BGRA engine
surface cases. Source pitch is preserved using the DIB width and a cropped
source rectangle; rows are top-down. The complete client is cleared black,
then the original image is aspect-fitted and centred.

If the format, dimensions, DC or drawing is unsupported, a successful copy
falls back to the original engine display flip after the original method has
released its locked surface. A failed original copy does not flip. Other
handles, other callers, `show=false` calls and disabled hooks forward normally.
The DLL remains resident after partial installation, with callbacks disabled
and forwarding to their original targets. Imported Bink ownership and native
signatures are checked before patching.

No separate D3D9 device is forced onto the native D3D8 path. The existing D3D9
intro viewport path remains available for mods using that renderer.

## Validation and manual acceptance

The headless memory-DIB fixture covers 640x480 fitting to 1920x1080
(1440x1080 at x=240), 1600x900 (1200x900 at x=200), and 800x600 (full client).
It checks black bars, row orientation, padded columns, caller arguments,
repeat/result forwarding and native fallback. The existing shell, canvas,
canvas-lifecycle and movie-start suites also remain required.
Initialization tests reject changed signatures and foreign pointer/import
owners before any patch attempt, and keep presentation disabled after either
installer callback fails.

Fully restart `STA2 Classic: Test` with intro videos enabled. Check the log for
`Initialized v11`, `Native engine intro frame fitting enabled`, and
`Native intro frame fitted: source=640x480; client=1920x1080; destination=1440x1080 at 240,0`.
All four clips should be centred at full client height, with black side bars,
normal sound and skipping. The shell should keep its selected size after them.
Then repeat with a smaller saved windowed resolution and in borderless mode.

Headless checks verify geometry, GDI output into memory and forwarding; they do
not establish that GDI drawing appears correctly over the live native display
surface under Wine. Visible playback remains a manual acceptance requirement.
Deployment hashes, backups and validation results are recorded in the adjacent
v11 deployment manifest.

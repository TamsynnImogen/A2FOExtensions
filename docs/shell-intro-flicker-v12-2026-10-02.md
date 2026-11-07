# Native intro flicker correction — v12

## Live report and presentation order

The user reports that v11's intro scaling now works, with very flickery output
as the remaining issue. The installed log confirms a native-engine movie,
640x480 source, 1920x1080 client and centred 1440x1080 destination at x=240.

V11 cleared the real client black with `PatBlt`, then stretched the image onto
it, on every decoded frame. These were separate visible operations. Both ran
inside the Bink copy hook, before the native engine unlocked/released its
surface. Either the visible clear or competing surface update could produce
flicker. The live report does not distinguish between them.

## Correction

V12 composes the bars and scaled picture into an owned top-down 32-bit memory
DIB. The buffer is reused through one synchronous movie scope, rebuilt only
when client dimensions change, and released when playback returns. GDI work
is flushed while the decoded source pixels are still locked and valid.

After the original `NextBinkFrame` returns, the hook copies the complete
offscreen image to the movie HWND in one `BitBlt`. No visible black clear or
stretch operation runs against that HWND. The final blit reads only owned
pixels; it never retains a pointer into the unlocked native surface.

Installed Armada RVA `0x638af` is the Bink copy call; native surface
unlock/release occurs at `0x638ba` through `0x638c6`, before the original
method returns. FO calls that method synchronously at RVA `0x1aa69e`.
Frame timing, audio, repeats, result forwarding and skipping remain native.
Unsupported inputs, allocation failures, unavailable DCs and failed blits
retain native flip fallback after surface release. Failed copies do not flip.
No new binary hook sites are introduced by v12.

## Verification and live acceptance

The memory-DIB fixture checks full-frame fitting, bars, orientation and padded
source stride. It also verifies that composition targets offscreen memory,
the final visible copy runs after the original frame method returns, and the
same scope reuses its buffer. Initialization-failure and fallback tests remain
in place, along with the shell/canvas/movie-start suite.

Fully restart the test mod. Its log should show `Initialized v12` and a native
frame-fit diagnostic ending with `buffered present after surface release`.
Check that all official intros retain their size and centring, remain steady,
play sound normally and skip normally, then return to the correctly sized shell.

The original play loop still pumps messages after presenting each frame
(FO RVAs `0x1aa6b9`, `0x1aa6ee`). Static evidence does not establish that paint
messages caused the reported flicker. Headless tests cannot establish visible
stability in the live Wine display, so this correction still needs manual
acceptance. Backup and deployment hashes are in the adjacent v12 manifest.

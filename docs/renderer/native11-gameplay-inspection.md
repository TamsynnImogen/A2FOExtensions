# Live skirmish renderer inspection

This test build makes the observed DX11 ship view available while a user drives
FO4 Nexus. It renders captured models, part poses, authored material values and
selected diffuse textures through API11, then exports a snapshot once per second.
A separate browser inspector displays those DX11 pixels and the capture state.

The current scene contains observed objects. Complete-world rendering, native
HUD/background/effect passes, physical light/caster coverage and presentation
takeover remain unfinished. The inspector uses neutral ambient light and does
not cast shadows. Its snapshot rate is separate from the game's frame rate.

## Start the test

The prepared game copy lives at:

```text
/home/tamsynn/NVMeData/Projects/A2FO-Native11-Gameplay-Test
```

Run `Launch Game.sh` there and use PortProton's normal Launch button if its
dialogue appears. The included `Native11 Inspection.bat` starts the copied
`Data/ArmadaL.exe` with `/mod "FO4 Nexus"`; extra command-line parameters passed
after an executable are not forwarded by the installed PortProton launcher.
The batch file is also exercised by the private startup check.

Start a skirmish, then open <http://127.0.0.1:18711/>. If the inspector is not
running, start `View Renderer.sh` in the same test directory. Both launchers
are user driven; no script opens a browser or controls input devices.

Select and move the Venture and Mediterranean, zoom and rotate the camera, then
compare their positions, scale, facing, textures and part animations with the
DX11 snapshot. Try a small mixed fleet and damaged units after those basic ships.
The menu itself does not exercise the high-level skirmish entity capture, so
the inspector initially waits for a skirmish. A capture error is displayed as
unavailable, and an old image is hidden after five seconds without a new frame.

## Files and isolation

The test copy has its own Core, helper, renderer configuration and pinned viewer.
The preparer verifies successful test cases, executable/input hashes, and all
seven protected installed files before and after preparation. The original game
installation and the source private copy are preserved. Existing copied preview
output is retained under `Seed Preview` before a fresh output directory is made.

The inspector is read-only and bound to `127.0.0.1`. Its only image source is
`Data/Native11Preview`: an ownership marker, producer lock, two BMP slots and
atomically published JSON metadata. It does not browse the game directory.
Only the next unused slot is written before its matching status is published;
the reader checks status again after reading the image. Current source owners
and lifecycle tickets are checked around readback and publication.

Output-folder and output-file reparse points are refused. Directory ancestors
may be volume mount points or junctions; Wine reports the Linux home/NVMe mounts
with the reparse attribute. The real mounted-volume regression exercises that
path. This differs from refusing every ancestor with that attribute, which
prevented the initial startup from creating the export directory. Windows also
uses reparse attributes for mounted folders, as described in the
[Microsoft filesystem documentation](https://learn.microsoft.com/en-us/windows/win32/fileio/determining-whether-a-directory-is-a-volume-mount-point).

Useful files in the test copy:

- `gameplay-trial-receipt.json`: exact prepared Core/helper/viewer and source hashes.
- `Data/Native11Preview/status.json`: capture state, reason, frame and dimensions.
- `Data/A2FOExtensions.log`: source admission and capture diagnostics.
- `Data/A2FORenderer.log` and `Data/A2FOStartupProxy.log`: renderer/startup diagnostics.

Close the game and viewer normally when finished. The test copy can remain for
later comparison; no rollback of the installed game is required.

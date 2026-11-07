# Mod intro saved-resolution correction — v10

## Live failure and cause

The user tested v9 in `STA2 Classic: Test` and reported no change: its inherited
A2 Classic intro remained a smaller borderless window while the following shell
filled the 1920x1080 display. The preserved installed log confirms v9 was loaded
and the new movie-start hook ran four times using the native engine renderer.

The sequence was:

1. Shell selected/client 1920x1080.
2. Shell selected/client 1280x960 immediately before the movies.
3. Intro matched the cached selection/client 1280x960.
4. Four native-engine movie starts.
5. Shell returned to 1920x1080.

The loaded mod settings file has WindowMode=False, displayWidth=0,
displayHeight=0, displayBitDepth=32 and adapter 0. The 0x0 selection means the
adapter desktop mode. The intro incorrectly trusted the temporary engine
selection published by the shell hook; the FoSettings seed returned early
whenever any snapshot existed.

## Correction

At the constructor HWND call and common movie-start call, the hook now refreshes
the selection from the already loaded mod FoSettings even when a snapshot exists.
If saved width/height/depth/window mode differ, it publishes the saved mode and
effective client dimensions, clearing the temporary shell HWND so stale geometry
and frame bits cannot win. A saved 0x0 selection resolves through the selected
adapter's current display mode.

An agreeing snapshot retains its actual shell geometry. Repeated movie entries
do not increment the presentation generation unnecessarily. Refresh happens at
movie boundaries, and nested menu movies remain on their original bypass path.
No movie assets, settings, core DLL or executable are changed.

## Verification and next live test

The synthetic regression reproduces a cached 1280x960 engine mode against a
saved 1920x1080 selection, the stored 0x0 desktop selection, and a saved smaller
1600x900 mode. It exercises native and D3D9 movie-start forwarding and asserts
the effective client dimensions follow settings. Other v9 shell/canvas and
installation-failure tests remain in place.

Fully restart the test mod using the current saved desktop setting. The log
should contain `Initialized v10` and
`Intro saved resolution restored: temporary=1280x960; selected=1920x1080; borderless`.
The intro window should now use the saved desktop size. Then check smaller
windowed/borderless selections and the transition back to the menu.

Headless checks do not establish visible movie scaling or playback. The native
engine's existing video backbuffer is not recreated by this correction; the
next live test must also check movie proportions and placement within the window.

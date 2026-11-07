# Borderless monitor sizing — 18 October 2026

The user confirmed window mode, border removal and resolution selection work,
but a lower selected resolution produces a smaller borderless window. The
fresh installed log shows `requested=1280x800, device=1280x800, client=1280x800`
in borderless mode. This was the old sizing policy, not a failure to load the
Refit menu adapter.

## v15 behavior

`modules/A2FOShellDisplay/module.cpp::resize` keeps the selected mode passed
to the original resize callback and the graphics engine. In primary shell
mode 3 only, the physical borderless client instead takes the selected D3D
adapter's `MONITORINFO.rcMonitor` width and height, with its exact left/top
origin. Framed mode retains the selected client size and work-area centering.
Unavailable monitor information falls back to selected dimensions.

The existing genuine VCL height/width setters at FleetOpsHook RVAs `0xad390`
and `0xad35c` apply client size, and left/top setters `0xb2708` and `0xb2744`
apply placement. No new hook sites, binary identities or unchecked addresses
are introduced. The October Roots full-file SHA check and all existing byte
and owner gates remain. Editor, gameplay and secondary-display activation
paths retain their native callbacks. No graphics settings are rewritten.

The canvas already derives its physical output scale from its owner's actual
client rectangle (`canvas.cpp::output_scale`). The presentation snapshot keeps
selected mode dimensions separate from actual client dimensions. The intro
window path (`bink_video.cpp::apply_intro_window`) also fills its monitor in
borderless mode and publishes that physical size without replacing the saved
selected mode. This prevents a lower-resolution intro from reinstating a small
borderless window. Existing nested-menu movie bypasses remain intact.

For example, selecting 1280x800 on a 1920x1080 desktop should log
`requested=1280x800, device=1280x800, client=1920x1080` in borderless mode.
The `device` diagnostic records dimensions forwarded to the native callback;
it is not an independent GPU backbuffer measurement.

## Verification

- Built the real 32-bit display DLL and updated fixtures; the native fixture
  compiles with `-Werror`.
- All seven existing headless suites pass: offscreen GDI canvas, lifecycle,
  native dialog dispatch, scrolling, intro, native intro, real-DLL display.
- The real-DLL display fixture also passes against the installed ArmadaL and
  October Roots FleetOpsHook pair, mapped without running game entry points.
- Native fixture geometry mocks cover a lower 1280x800 selection on 1920x1080,
  negative monitor origins, a selection larger than the desktop, framed
  work-area centering, missing monitor fallback, and non-shell exclusion.
  It checks the original callback still receives the selected dimensions.
- Intro fixtures cover full monitor client dimensions, preserved selected mode
  in the snapshot, negative origins, missing monitor fallback, frame changes,
  repeated-call caching, resize retries, and nested movie pass-through.

Evidence: `build/borderless-monitor-tests-20261018.log` and
`build/borderless-monitor-installed-20261018.log`. Wine tests run with display
environment variables removed and an explicit disposable prefix. No actual
windows, game launch, input events or GUI focus are automated. The GDI fixture
uses memory surfaces. These checks establish source/DLL and synthetic geometry
behavior; compositor coverage and native D3D presentation still need live
acceptance after restart.

## Deployment and manual acceptance

Deployed only `Data/modules/A2FOShellDisplay.dll`, preserving the working
RefitMenus module, settings, module lists, engine and FleetOpsHook files.
Ten protected-file hashes are unchanged. The [deployment receipt](borderless-monitor-20261018-221407-deployment.json)
records DLL/log backups and hashes. Installed DLL SHA-256:
`c364fa31788d6c1a08769d93ed9db56c93c6a218e4bb626eb3ff25711dc5bd19`.
Backup: `.deployment-backups/borderless-monitor-20261018-221407`.
At deployment the saved settings still have `WindowMode=True` and 1280x800;
these user-selected settings were preserved.

Fully exit and relaunch Fleet Ops with **Window Mode unchecked**. At a lower
selected resolution such as 1280x800, the menu should fill the monitor, and its
pointer and buttons should align. Check Options, Instant Action and return to
the main menu, then check framed mode still uses the selected size. The log
should show `Initialized v15` and borderless client dimensions matching the
monitor. Live acceptance is pending; no desktop-mode switch or exclusive
fullscreen change is part of this fix.

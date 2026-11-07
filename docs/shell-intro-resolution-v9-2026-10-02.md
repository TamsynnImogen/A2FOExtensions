# Mod intro resolution — A2FOShellDisplay v9

The requested target is `STA2 Classic: Test` (`Data/Mods/test`). Its `info.ini`
inherits `STA2 Classic Mod`, whose loose movie is
`Data/Mods/STA2 Classic Mod/animations/Intro.bik`. The test mod enables
`A2FOShellDisplay` as `active31` and selects `SettingsDirectory = "A2FO Settings Test"`.
The installed log resolves that directory to
`Data/settings_saves_logs/mods/A2FO Settings Test/`.

## Change and evidence

The previous integration applied intro sizing only when Fleet Ops' D3D9 frame
renderer or Armada's GDI movie callback ran. `TBinkIntro.play` also supports the
engine renderer, which takes neither of those paths. The latest pre-change log
showed movie hooks installed, but contained no intro-window/viewport messages;
that does not establish which playback path ran.

- FO `+1aa3bb` now wraps the no-argument HWND getter at `+1ddb94`. It applies the
  saved resolution before `TBinkIntro.Create` creates the D3D9 device with
  zero backbuffer dimensions (native auto-size).
- FO `+1d3d3d` wraps `TBinkIntro.play` (`+1aa634`), preserving EAX object and AL
  boolean result. It applies the same geometry for both rendering paths and
  logs `Intro movie start: renderer=D3D9` or `renderer=native engine`.
- Intro sizing updates the verified main VCL form using its real client setters
  at `+ad390/+ad35c`, as well as positioning the HWND without activation.
- Cached HWND/generation state is reused only if actual client dimensions and
  framed/borderless style match. Native overrides and failed sizing are retried.
- The scoped shell resize callback forwards selected mode width/height instead
  of the authored callback dimensions. Other modes/displays retain native sizes.
- Optional movie callbacks remain native until all patches complete. Call-install
  failures leave backing untouched; a failed UV patch rolls back its size patch.
  Shutdown disables the movie wrappers.

Signatures/ABI were inspected in installed FleetOpsHook SHA-256
`d2a0829e87aaa927e27146b90be61fc6359c6cbf3d81aed2c644019dd375b9b6`.
Armada SHA-256 is
`470bf8f4b0196e22fe179d3845cab671e74d9af19cf90a7dafed0b0bbd394318`.

## Headless verification

`make shell-display-test` passes: actual module smoke with synthetic native
gateways, memory-GDI canvas, synthetic canvas lifecycle, and the new intro test.
The intro test and actual-DLL smoke also pass against the installed Roots pair,
mapped as data. The intro fixture checks 1280x720 borderless, 1600x900 framed,
800x600 retry, movie return forwarding, nested bypass and installation failures.
These checks create no HWNDs or execute original game/VCL/D3D methods.

## Manual acceptance still required

1. In the test mod, save a clearly smaller resolution such as 1280x720 with
   Window Mode off. Fully exit and restart into that mod with intro videos on.
   Its intro window should be 1280x720 borderless and centred; the following menu
   should have the same client size. Check the whole sequence, including skips.
2. Repeat with Window Mode on. The intro should have a frame and use the saved
   client resolution, subject to the normal desktop/work-area limits.
3. Repeat with desktop resolution. Verify movie proportions and menu transition;
   confirm Options, checkbox input and returning to gameplay still work.
4. Look in `Data/A2FOExtensions.log` for `Initialized v9`,
   `Intro movie start: renderer=...`, and `Intro window matched shell: ...`.
   If no movie-start line appears, the actual mod intro is reaching another
   playback entry; that remains a tracing task, not proof this path activated.

This change does not recreate the native engine's pre-existing video backbuffer.
Live playback must establish its scaling, movie proportions and return to menu.
No manual in-game acceptance has been performed in this session.

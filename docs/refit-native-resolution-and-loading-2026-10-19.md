# Refit menu resolution and loading — 19 October 2026

Later correction: [DDS first-frame loading fix](refit-live-scene-first-frame-2026-10-19.md)
reduces the measured cold render from 43.1 seconds to 3.46 seconds. The larger
loading figures below describe the earlier provider and are historical.

The reported screenshot was still running the rollback menu DLL (`acda0f91…`):
boxed rows, substituted serif text, a 640×360 scene and synchronous first-frame
preparation. The live config had subsequently been changed back to `LiveScene=1`.
Its latest log records a 43,638 ms first frame. This correction preserves that
explicit 3D choice and adds a separate resolution setting.

## Changes

- `SceneRenderWidth=0` selects the physical client resolution. A 1920×1080 menu
  gets a 1920×1080 render target instead of an enlarged 640×360 frame. Bounds are
  3840 wide, 2160 high and eight million pixels. Optional widths 320–3840 preserve
  aspect ratio and cannot exceed the client size. Invalid values log a fallback
  to bounded native size. Native-size composition has a direct pixel path.
- The menu-only profile increases sky faces from 512 to 1024 and planet surface
  faces from 256 to 1024. Shadows/nebula targets stay reduced; bloom/MSAA stay off.
  This does not configure in-match rendering or change the provider binary.
- The previously developed worker and private Noto fonts replace the rollback
  DLL. Provider/device creation, drawing, loading and readback run on one worker.
  Main-form destruction pauses it and retains the backend/cache. UI callbacks
  exchange requests and completed immutable snapshots. Static artwork remains
  available before the first frame. Native submenus retain the last snapshot.
- Open uppercase condensed rows replace the boxed serif treatment. Fleet Ops'
  localized native actions remain authoritative, including Map Editor.

## Configuration

In `Fleet Ops Roots/Data/A2FORefitMenus.ini`:

```ini
[RefitMenus]
Enabled=1
LiveScene=1
SceneRenderWidth=0
```

`LiveScene=0` uses installed background artwork and creates no scene worker,
provider or D3D device. It keeps the replacement menu presentation. Restart the
game after any change. `SceneRenderWidth=1280`, for example, offers a lower pixel
budget; it does not guarantee faster cold asset/driver initialization.

## Verification and remaining limits

MinGW `-Werror`, seven portable suites, 52/52 native console fixture processes,
four admitted FO profiles × 30 signatures and the offscreen GDI checks pass.
The GDI fixture verifies the actual private font, title/row states, interpolation
and preservation of adjacent native-resolution pixels. These are contract and
paint checks, not an actual Fleet Ops run.

`tests/refit_scene_worker_native.cpp` additionally runs the exact production
worker against the actual renderer/provider, loose game assets and installed
DXVK 2.6.2. It creates hidden non-activating fixture/device windows inside a
private Gamescope headless compositor and a dedicated Wine prefix. It neither
launches the game nor controls desktop input. It checks disabled mode, actual
1920×1080 frames, main-form destruction/return without another backend creation,
Klingon/Borg frame production and worker-owned cleanup. Captures wait for three
subsequent published faction frames and eight seconds, covering the camera
blend and staggered Klingon decloak. Runner cleanup terminates only its own process
group; the compositor can print an X11 teardown error after the fixture passes.

Earlier real GTX 1660 Ti fixture runs measured cold Main preparation at 46–61 s
and first Borg preparation at 19–20 s. Menu-side frame exchange took at most
16 ms while that work ran. **Cold background preparation is still slow**: this
change moves it out of the menu call and retains resources on return; it does
not establish that the underlying loading cost has been eliminated. The 16 ms
measurement excludes complete native event/paint latency. Actual game startup,
input responsiveness, window/borderless presentation and driver contention still
need user acceptance after a full restart.

The first stable native-profile run measured Main at 42,657 ms, Borg at 16,603 ms
and maximum menu-side exchange at 1 ms. This is isolated worker evidence, not
an end-to-end native menu benchmark.
The final capture run (including the longer decloak wait) measured Main at
43,952 ms and maximum menu-side exchange at 12 ms, and passed cleanup/cache checks.

Models and available textures come from the selected Fleet Ops asset roots;
they differ from stock Refit artwork. Missing loose textures/FPQ-only assets and
the complete Instant Action widget-layout port remain separate limitations.
The Borg capture has visible model silhouettes with largely missing textures;
the provider reports `borg_cubic_*` and `borg_diamond` texture lookup failures.
The Klingon capture still lacks the intended prominent fleet, with only a small
faint ship visible even after the longer wait. Model placement/visibility needs
separate investigation. Faction frame production is not a claim that those
visuals are finished.
The earlier FleetOpsHook early-initialization exception remains unresolved;
its captured stack does not establish that the menu worker caused it.

Evidence: `build/refit-native-resolution-{build,native,gdi}.log`,
`build/refit-native-resolution-bindings.json`, and
`build/refit-native-resolution-worker-stable.log`.

## Deployment

Deployment changes only the menu DLL, its two settings, and the menu-only
`RefitScene/Render3D.ini`. Existing executables, FleetOpsHook, core extensions,
ShellDisplay, provider DLL, module lists, other settings and attribution are
protected by hashes. A timestamped receipt and backup preserve replaced files,
pre-update logs and verification evidence. Live game acceptance remains pending.

Installed at local **01:06:41**, with all 16 protected files unchanged:
[deployment receipt](refit-native-resolution-20261019-010641-deployment.json).
The menu DLL SHA-256 is
`edd08ae51fe73d066192248311048f8bf8bf40df3d1d12454203d6c0001eeded`.
Backup: `.deployment-backups/refit-native-resolution-20261019-010641/`.
With the game closed, restore its three `Data/` files to the matching live paths
to roll back this deployment. Do not remove existing attribution notices.

Fully quit and restart Fleet Ops. Confirm open condensed rows, sharp Main
artwork, navigation while the background prepares, and repeated submenu returns.
The fresh extension log should show `1920x1080` in `Refit scene worker:` for a
1920×1080 client, and the effective configuration path/values. Try `LiveScene=0`
plus a restart to check the installed-artwork mode. No game launch or input
automation was performed during deployment.

# Shell / intro presentation integration — 2026-10-02

`A2FOShellDisplay` v6 now owns movie presentation that previously lived in
`A2FOFeaturePack`.

## Behaviour

- The selected shell resolution and Window Mode state are published before the
  native primary-shell transition and refreshed with the resulting shell HWND
  and actual client size.
- Fleet Ops' startup `TBinkIntro` window mirrors the shell's frame/borderless
  presentation and centres on the same monitor.
- Fleet Ops D3D9 intro frames are aspect-fitted to the render target using the
  native Bink handle's width and height.
- Armada's GDI movie path remains aspect-correct and now consumes the same shell
  presentation where a movie HWND is available.
- Armada full-window texture movies are aspect-fitted rather than stretched to
  the viewport.
- Native nested shell Bink elements and embedded gameplay/viewscreen movies keep
  their authored rectangles.
- No exclusive display-mode switch is introduced; the shell remains on FO's
  existing non-exclusive path.

## Ownership change

The Bink source files moved from `modules/A2FOFeaturePack/` to
`modules/A2FOShellDisplay/`. FeaturePack no longer installs any Bink call-site
patches, preventing duplicate hook ownership when both modules are enabled.

## Manual validation

1. Restart with intros enabled, 16:9 resolution, Window Mode on. The intro and
   following shell should use the same framed client geometry.
2. Restart with Window Mode off. Both should be borderless and centred/fill the
   display according to the selected resolution.
3. Test 4:3, 16:10 and ultrawide modes. The movie should remain proportional,
   with unused target area outside the fitted movie instead of stretching.
4. Enter Single Player and trigger any nested/menu movie content; layout must
   remain native.
5. Trigger bridge/comms/gameplay movies; their inset placement must remain
   unchanged.
6. Confirm `A2FOExtensions.log` contains `Intro window matched shell` and
   `Intro Bink viewport` once, and no Bink hook ownership/signature errors.

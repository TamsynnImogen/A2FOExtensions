# Shipyard identity and ammunition investigation

## Confirmed registry bug

`render_selected_info_panel` submitted ammunition and directional shields for
both panel kinds, but returned early for `SelectedPanelKind::builder` before
`ensure_craft_identity` and the captain/registry draw calls. Consequently a
shipyard could have valid identity rows and GUI rectangles and still never
submit registry text. This also affected repair-only shipyards using that panel.

Removed that early return. Both render hooks now use the same identity
selection and coordinate path. Registry strings remain arbitrary text, aligned
to `possibleCraftNames`; a `possibleCaptainNames` list is not required.
`infoSingleCaptainTextArea` must still exist as the shared coordinate anchor,
and `infoSingleRegistryTextArea` enables and positions the registry row.
No new ODF/GUI command, core change or module API change is needed.

## Ammunition findings

The September 19 ammunition-anchor fix was already present in the installed
CraftIdentity DLL (SHA-256
`c7a37e354e3fc7f1fe1cfd13cf8cebfa368d3f87befe4920f67b07be03755f94`).
No additional ammunition placement change was justified by this investigation.

The supported local Armada executable confirms:

- ShipDisplay's common `PostLoad` creates the captain component at `+0xbc`
  before either selected-panel render path runs (`0x000f07d5..0x000f0880`).
- GUIText's constructor (`0x0010c1f0`) copies its supplied rectangle into
  the live `+0x58` rectangle immediately. Selecting a yard first does not
  require rendering a ship to initialize this geometry.
- `LoadRectangleWithOffset` (`0x000f3e70`) subtracts `infoPanelOffset`
  (`ShipDisplay+0x1f0`) from the loaded rectangle's top and bottom. The
  fixture's ten-pixel adjustment is this offset, not a font adjustment.
- `DisplayInterface::DrawText(rectangle)` (`0x0011b160`) adds the active
  display origin at `+0x04/+0x08` to the rectangle before submitting text.

The current Roots log reports no configured registry rectangle and automatic
Photon/Quantum/Shuttle placement. That configuration cannot reproduce a
reporting mod's custom rectangles. Keep `infoSingleCaptainTextArea` defined
even for objects with no captain identity, and use the same
`infoSinglePhotonTorpedoesTextArea`, `infoSingleQuantumTorpedoesTextArea` and
`infoSingleShuttleCraftTextArea` fields for ships and yards.

## Validation and limits

`tests/craft_identity_panel_smoke.cpp` includes the production module and calls
both selected-panel hooks. It maps a private local EXE without its entry point
and executes the native rectangle loader, GUIText constructor and text-to-screen
coordinate conversion. Configuration lookup and final graphics submission are
stubbed; the game, graphics renderer and input devices are not run.

The test failed on the original builder early return (missing identity draw)
and passed after its removal. Coverage includes all three ammunition rows and
their hover rectangles, first-selection shipyards, ship/yard switching,
different panel origins, hidden producer labels, configured name fallback,
free-form registry with no captain-name list, missing GUI registry area,
out-of-range identity rows and clearing selection.

Build and run the optional private-fixture smoke:

```sh
make build/craft_identity_panel_smoke.exe
env WINEPREFIX=/tmp/sta-sod-workshop-wine-20260919 WINEDEBUG=-all DISPLAY= WAYLAND_DISPLAY= \
  timeout 30s wine build/craft_identity_panel_smoke.exe \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe'
```

The native GUI setup and full Fleet Operations graphics hooks are not run by
this harness. In-game confirmation is still needed in the reporting mod:
select a repair-only yard first, compare all three configured ammunition rows
and registry text, then switch between a ship and that yard. Verify the labels
remain within the visible panel and do not overlap build controls.

## Deployment

Only `Data/modules/A2FOCraftIdentity.dll` was replaced.

- SHA-256: `6d8d8aca2ee531145986de015d65389b6373075e4aa44828afa4ba3c46213858`.
- Previous DLL and manifest: `Data/rollback/2026-09-20-shipyard-identities-015434/`.
- `make test`, `make verify`, native panel smoke and headless DLL-load smoke passed.
- In-game validation in the reporting mod remains pending.

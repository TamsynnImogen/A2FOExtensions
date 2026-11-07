# Nexus 0.2 selected-unit UI

The existing FO4 frame, native ship status, wireframe, subsystem icons and
command palette remain the foundation. The right-hand readout now has two
distinct regions instead of one narrow stack of diagnostic rows.

| Medium panel element | Native panel rectangle (x y width height) |
| --- | --- |
| Captain | 627 18 360 20 |
| Registry | 627 42 360 20 |
| Photon or quantum ammunition | 627 72 360 20 |
| XP track | 627 101 360 7 |
| Shield quadrant graphic | 1050 14 96 96 |
| Forward percentage | 1070 14 56 18 |
| Aft percentage | 1070 91 56 18 |
| Port percentage | 1005 54 40 18 |
| Starboard percentage | 1152 54 40 18 |

The ring and its own numbers intentionally share space. They are separate
from identities, ammunition and XP. Percentage labels require the visual-only
`directionalShieldValueDisplayMode=1` on the existing 114 directional ships.
The inactive combined F/A/P/S diagnostic rows are hidden. The shield graphic
appears only when the selected craft actually has directional shields.

The tall producer panel uses a compact identity header: captain at
`627 10 270 18`, registry at `915 10 290 18`. Its queue and progress controls
retain their parent geometry. Other extension rows are explicitly hidden on
this panel; the current Mediterranean has identity data only.

These coordinates are in the existing CFG system, not screenshot pixels.
The parent aspect-ratio files and native UI transforms still apply. The frame
stays 1249 by 163 units. Every new visible rectangle fits above the native
medium construction/resource bars, or above the tall queue as appropriate.
Actual font fit and native rendering still require an in-game check.

## Assets and loading

The original local `test/textures/shield_quad.tga` and `shield_quad_side.tga`
were copied byte-for-byte into this project's `assets/Textures` directory,
with their original paths and hashes recorded in `assets/provenance.json`.
The builder uses those pinned project copies, so it does not depend on the
test mod remaining installed. Nexus uses unique
texture names `nx_ui_shield_quad` and `nx_ui_shield_quad_side` to avoid replacing
the source mod's textures. Each is an uncompressed 128-by-128 32-bit TGA.
The output hashes and local source paths are recorded in
`ui-assets-manifest.json`. Their distribution provenance remains to be
established before publishing the mod.

The child `Sprites/gui_global.spr` is the complete parent file plus one
`@include nx_ui.spr` in its initial include block. The new table defines the
existing CraftIdentity identifiers `dsq` and `dsqs`, with interface material
and a 128-unit reference. Both tables retain CRLF endings. No world sprite
nodes are introduced. The native `large_shield_bar` supplies the XP track.

## Verification and next visual check

Static checks cover table placement, exact parent preservation, sprite names,
TGA headers and hashes, per-element configuration, panel bounds and control
clearance. Repeated builds must produce identical payload bytes. The source
and installed parent, runtime DLL and player module policy remain protected.

After a full restart, check Venture and Mediterranean identities, then a Saber
for shield quadrants and an Intrepid/Akira/Sovereign for ammunition, facings
and XP. Verify the numbers remain readable on the arcs, the ring drains and
changes colour correctly, and the constructor's native queue still works.
Try the other supported screen ratios before accepting the layout generally.
If the percentages appear without the ring, preserve the fresh log before
restarting and inspect sprite/texture resolution. A local layout or asset
check does not prove the game has drawn the sprite.

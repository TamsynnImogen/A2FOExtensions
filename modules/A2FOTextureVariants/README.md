# A2FOTextureVariants

`A2FOTextureVariants.dll` adds ownership-aware hull textures and faction SOD
nodes, and fixes the stock Borg alternate-texture gate so DDS-only `_b`
textures are accepted. It is a globally installed optional native module for
the supported Armada II 1.1 / Fleet Operations Roots binaries.

## Faction ODF command

Add `factionTextureSuffix` to a faction ODF:

```cpp
// klingon.odf
factionTextureSuffix = "_k"
```

If a model mesh normally uses `kli_birdofprey`, the module looks for:

```text
Textures\kli_birdofprey_k.dds
Textures\kli_birdofprey_k.tga
```

DDS wins when both formats exist. The replacement is selected independently
for every mesh; a missing suffixed texture leaves that mesh on its normal
base/Borg texture. The SOD does not need a faction-specific material entry.

The suffix may contain up to 32 ASCII letters, digits, underscores, or
hyphens. An underscore is recommended but not required. Empty/missing values
disable the feature for that faction. Path separators and dots are rejected.

## Faction SOD nodes

No new ODF command is needed. The module also captures the existing internal
`name` from every faction ODF. A SOD node with that name is shown only while
the unit is owned by that faction. Matching is case-insensitive.

For example:

```cpp
// kling.odf
name = "klingon"
```

A SOD hierarchy may contain a parent node named `klingon`; every mesh and node
attached below it becomes visible for Klingon ownership and hidden for every
other loaded faction. Multiple matching nodes are supported. Nodes which do
not match a loaded faction name are untouched.

Names may contain up to 63 ASCII letters, digits, underscores, or hyphens.
Faction-specific parent nodes should be siblings rather than descendants of
another faction node. The stock `borg` node remains entirely native: Armada
continues to select it with the original Borg ownership flag after the module
handles other faction nodes.

## Ownership and capture

The current `Race*` is read from the live owning `GameObject` during the same
`CraftInstance::Update` route Fleet Operations uses for its Borg check. A
captured unit therefore switches to the new owner's suffix on its next update
and render; the unit's build faction and SOD name do not control selection.

Storm3D models are shared by every instance of a class. Immediately before
each craft is rendered, the module:

1. caches all mesh/base-texture pairs and loaded-race node matches the first
   time the class is seen;
2. calls Fleet Operations' native `SetBorgMeshTextures` routine unchanged;
3. replaces diffuse slot 0 where the current owner's suffixed asset exists and
   restores other meshes before another owner is rendered;
4. selects the current owner's faction nodes and hides the other loaded-race
   nodes.

That ordering retains Fleet Operations' Jan_B Borg/bump-map patch and its
native `borg` node toggle. Only the diffuse slot is changed by faction texture
variants; faction node selection changes the same hidden/disabled flag pair
used by Armada's native Borg node.

## Subsystem damage meshes

A ship or station ODF may give each native subsystem one or more numbered SOD
nodes. Numbering is one-based, may be sparse, and supports indices 1 through
64:

```cpp
engineMesh1 = "nacelle_l"
engineMesh1explosion = "xfirebsm"
engineMesh2 = "nacelle_r"
engineMesh2explosion = "xfirebsm"
```

The five command stems are:

```text
sensorMeshX
engineMeshX
weaponMeshX
lifeSupportMeshX
shieldGeneratorMeshX
```

Append lowercase `explosion` to the same numbered mesh key for its optional
explosion ODF, as in `shieldGeneratorMesh3explosion`. A mesh entry works
without an explosion entry and simply disappears. An explosion entry without
its paired mesh is ignored.

When that subsystem changes from operational/temporarily disabled to truly
destroyed, the module chooses one configured node which actually exists in
the SOD, hides that node and its complete descendant subtree for that
individual craft, and builds the paired native explosion at the selected
node's exact world transform. Descendants are discovered from the SOD
hierarchy by pointer, so they do not need unique extra ODF commands. Attached
geometry, sprites, emitters, hardpoint nodes, Borg additions, and native damage
geometry therefore cannot remain drawn in the missing part's space. Unrelated
sibling branches are never included.

The choice is pseudo-random but derived from the craft handle, subsystem, and
destruction count, keeping it identical on multiplayer peers. The selected
root mesh reconstructs as subsystem hitpoints return and reaches its ordinary
scale at completion; descendants remain hidden until the repair completes.
A later destruction makes a fresh choice.

If a save is loaded while a subsystem is already destroyed, a deterministic
mesh is hidden immediately but no false load-time explosion is created.
Once the selected subsystem's hitpoints begin rising, its root mesh grows back
in place with eased progress rather than popping into existence at completion.
Armada's native `xspark` welding effect samples deterministic moving points
across the mesh's actual local bounding surface and finishes with a small
completion burst. A configured group/non-mesh node still receives the safe
node-anchored fallback effect. This remains localized repair feedback: the
native pod/construction shader can only rebuild an entire CraftClass and would
incorrectly ghost the whole ship.

The subtree override is applied after Fleet Operations selects its native
damage and `borg` nodes, immediately around the final shared-model render, and
then every touched flag and scale is restored. Native Borg/damage descendants
which were already hidden are never revealed by reconstruction, and another
ship using the same shared SOD cannot inherit the first ship's missing part.
This is a render-hierarchy rule: a hardpoint node in the hidden subtree is not
drawn, but the current implementation does not remove it from a live Weapon's
simulation-side hardpoint list.

Node matching is case-insensitive. Names use up to 63 ASCII letters, digits,
underscores, or hyphens. Each configured part should normally be visible in
the base SOD and should not also be an ownership parent node. Explosion ODFs
should be visual-only unless the mod deliberately wants their normal native
gameplay effects.

## Borg DDS repair

Armada's model loader originally preflights a generated `_b` alternate through
a TGA-only filename helper. A DDS file can be loadable by Fleet Operations yet
still be rejected before the enhanced texture loader is reached. This module
keeps the original TGA test first and, only if it fails, accepts the exact
`Textures\<borg-name>.dds` path tested by Fleet Operations' texture loader.
The ordinary `ST3D_Texture::Find` path then performs the actual load.

No faction command is required for the Borg repair. Existing TGA `_b`
textures and explicit SOD Borg texture declarations keep their native
behaviour.

## Current boundaries

- Static DDS and TGA diffuse variants are supported. Animated DDS variants
  have not been validated.
- Faction node matching uses the internal Race ODF `name`, not `displayName`,
  the ODF filename, or `factionTextureSuffix`.
- Finalized `CraftInstance` ships and stations are covered. Cosmetic
  construction renderers have not been claimed by this module.
- When `A2FOCraftIdentity.dll` is active it forwards the completed CraftClass
  ODF policy through a cooperative observer. If it is not selected,
  TextureVariants installs its own validated Fleet Ops constructor chain so
  these commands do not depend on another optional module.
- Texture lookup results, including missing variants, are cached for the
  session. Restart the game after adding or renaming a variant.
- Damage-mesh selection is deterministic per craft and destroy/repair cycle;
  multiplayer still needs an explicit visual smoke test.

## Manual test matrix

1. Keep a Borg `_b.dds` and remove/rename its `_b.tga`; verify assimilation
   still changes the hull.
2. Set Klingon `factionTextureSuffix = "_k"`, provide one `_k.dds`, and verify
   a Klingon-owned unit uses it.
3. Capture that unit with a faction that has no suffix; verify the base texture
   returns.
4. Capture it back to Klingon; verify `_k` returns on the next update.
5. Use a multi-material SOD with one missing `_k` asset; verify per-mesh
   fallback and bump mapping remain intact.
6. Add sibling `federation` and `klingon` parents with distinct child meshes;
   verify only the owner's subtree is visible, then capture the unit and
   verify the visible subtree switches on its next render.
7. Confirm an existing `borg` parent still follows native Borg assimilation
   and remains hidden for non-Borg ownership.
8. Configure two `engineMeshX` nodes, destroy engines, and verify exactly one
   complete part subtree disappears and its paired explosion occurs at that
   part. Include attached sprite/emitter and Borg/damage children in the test.
9. Repair the engine subsystem; verify welding sparks move around the missing
   part, only its root geometry grows during repair, its descendants return at
   completion, and a small spark burst accompanies completion.
10. Destroy, repair, and destroy it again; verify a valid numbered part is
    selected each time. Save/load while destroyed and confirm there is no
    false load-time explosion.

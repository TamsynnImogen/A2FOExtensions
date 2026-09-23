# A2FOODFVariants

`A2FOODFVariants.dll` changes a captured craft to an ownership-specific ODF
without requiring a ReplaceWeapon or techtree/XML replacement trick.

It shares the same `factionTextureSuffix` policy used by
`A2FOTextureVariants`, so texture and ODF ownership variants agree when both
modules are installed.

## Naming

Given:

```cpp
// kling.odf
factionTextureSuffix = "_k"
```

capturing `fed_venture.odf` with that race looks for:

```text
fed_venture_k.odf
```

The stock Borg race defaults to `_b` when it has no explicit
`factionTextureSuffix`, preserving the existing convention:

```text
fed_venture.odf -> fed_venture_b.odf
```

An explicit suffix in `borg.odf` overrides that default. An explicit empty
suffix disables ODF variants for that Race.

## Capture behaviour

The module observes the core-owned `Craft::Simulate` event dispatcher. When a
live craft's owner Race changes it:

1. remembers the canonical/base ODF;
2. resolves the new owner's suffix;
3. looks for `base + suffix`;
4. if that variant is unavailable, falls back to the base ODF instead of
   retaining the previous owner's variant;
5. constructs the target class and uses Armada's native
   `Evolver::mSwapObjects` handoff;
6. restores hull %, shield %, special-energy %, crew, velocity, and subsystem
   damage/disable state;
7. expires the old craft through Armada's normal cleanup path.

The canonical base name is retained in a per-craft sidecar, so repeated
captures resolve cleanly:

```text
fed_venture.odf
  -> fed_venture_b.odf
  -> fed_venture_k.odf
  -> fed_venture.odf
```

It never produces stacked names such as `fed_venture_b_k.odf`.

ODF and texture variants are independent. A faction can have a suffixed
texture but no suffixed ODF, or vice versa; a missing ODF variant does not
prevent capture.

Fresh map-editor objects are reconciled on their first simulation tick. This
means a unit can be placed in the editor, assigned to Borg (or any other race
with a suffix), and then switched directly into game mode without first
"priming" the object in game mode under its original race. An ODF that already
ends in the current race suffix is treated as already reconciled, preventing
stacked names such as `ship_b_b.odf`.

## Current state preservation

Preserved by the initial implementation:

- ownership/selection/common object relationships through native Evolver swap;
- hull percentage;
- shield percentage;
- special-energy percentage;
- current crew, clamped to the replacement ODF's maximum;
- linear velocity;
- subsystem damage fractions, operational flags, forced-disable flags and
  disable timers.

Fleet Operations-specific sidecars such as veterancy, cargo/passengers and
cloak state should still be tested explicitly before treating those as
preserved guarantees.

## Module selection

Install `A2FOODFVariants.dll` in the normal global `Data\modules` directory.
With managed module selection, add it to a free slot in the mod's existing
`[modules]` list, for example:

```ini
[modules]
active30 = "A2FOODFVariants"
```

The exact `activeX` number is not significant; use a free index rather than
replacing another selected module. `A2FOTextureVariants` is an optional
companion, not a load-time dependency.

## Requirements

The module requires the v4 core Race-loaded and Craft-event dispatch APIs and
the validated ArmadaL.exe native routines used by the tested `_b` prototype.
It installs no competing Craft/Race hooks of its own.

### Renderer integration

Late-loaded ownership variants can finish attaching their SOD/material state
only after an actual craft instance has been constructed. The module therefore
refreshes `A2FONebulaRenderer` immediately after constructing the replacement
craft and retries once on that replacement's first `SIMULATE_POST`. This keeps
final-suffix auxiliary maps such as `ship_emissive_warp_b` and
`ship_specular_b` available after an ODF swap without changing the normal
startup registration path.

# A2FOEnergySystems

`A2FOEnergySystems.dll` adds two independent per-Craft ammunition stores:
Photon Torpedoes and Quantum Torpedoes. A launcher only uses a store when its
weapon ODF declares the corresponding cost; all other weapons retain native
behaviour.

## Craft ODF commands

```cpp
maxPhotonTorpedoes = 80
photonTorpedoRate = 1.0
photonTorpedoRechargeMode = 1

maxQuantumTorpedoes = 20
quantumTorpedoRate = 0.25
quantumTorpedoRechargeMode = 2
```

- `maxPhotonTorpedoes` / `maxQuantumTorpedoes`: store capacity. Missing or
  zero disables that store on the Craft.
- `photonTorpedoRate` / `quantumTorpedoRate`: torpedoes restored per second.
- recharge mode `1`: recharge automatically.
- recharge mode `2`: recharge only while within range of a same-team resupply
  provider.
- recharge mode `0`: do not recharge.

New Craft stores start full. Values are clamped to their current ODF maximum.

## Selected-craft UI

Selecting a Craft with either store displays its current and maximum Photon
and Quantum values as completed whole ammunition. Fractional recharge remains
internally precise and becomes visible when the next whole unit completes. This UI
uses `A2FOCraftIdentity.dll`'s selected-panel observer; ammunition simulation
remains available when CraftIdentity is not selected, but these rows do not.

The automatic rows use offsets `+16` and `+40` from the selected name anchor,
keeping both inside the standard selected panel. A GUI may
position and colour them explicitly:

```text
infoSinglePhotonTorpedoesTextArea = 386 186 340 20
infoSingleQuantumTorpedoesTextArea = 386 214 340 20

photonTorpedoColor = 1 0 1
quantumTorpedoColor = 1 0 1
```

Rectangles use the same `x y width height` coordinates as
`infoSingleCaptainTextArea`. Missing colours use `infoTextColor`, then the
live selected-panel text colour. Crafts without a configured store do not
show an empty row.

Each Craft can replace the default labels and tooltip copy or select an icon:

```cpp
photonTorpedoDisplayMode = 1
photonTorpedoValueDisplayMode = 0
photonTorpedoLabel = "Photon Magazine"
photonTorpedoTooltip = "Photon Torpedo Ammunition"
photonTorpedoVerboseTooltip = "Photon torpedoes recharge continuously."

quantumTorpedoDisplayMode = 2
quantumTorpedoValueDisplayMode = 1
quantumTorpedoIcon = "all_interface"
quantumTorpedoIconPos = 71 151 34 34
quantumTorpedoTooltip = "Quantum Torpedo Ammunition"
quantumTorpedoVerboseTooltip = "Quantum torpedoes require resupply."
```

Mode `1` draws the label and value. Mode `2` draws the requested atlas
crop and places the selected compact value after it.
`*Icon` may name a registered interfaceDB sprite or a stock atlas such as
`all_interface`; `*IconPos` is the `x y width height` source rectangle inside
that texture. The renderer positions the icon at its store's text row. If the
sprite or crop is unavailable, mode `2` retains the compact value and does not
restore the text label.
Labels and both tooltip commands accept either a
`Dynamic_Localized_Strings.h` key or literal text. Tooltips use Armada's
normal/verbose hover timing over the complete row and icon area.

`*ValueDisplayMode` mirrors Fleet Operations' `specialEnergyDisplayMode` and
defaults to `0`: `0` is integer percent, `1` is integer `current/maximum`, and
`2` is localized ready/reload status. Mode `2` uses `GUI_SD_SPE_READY` when
full, rounded-up seconds while recharging, and `GUI_SD_AMMO_WAITING` (fallback
`Resupply`) when a resupply-only store is not currently supplied. Mode `3`
replaces the value text with a left-to-right capacity bar while retaining the
text label or icon selected by `*DisplayMode`.

The value and icon are green above 50%, yellow from 25% through 50%, and red at
or below 25%. GUI colours can be overridden with `photonTorpedoColor`,
`photonTorpedoLowColor`, `photonTorpedoCriticalColor`, and the equivalent
`quantumTorpedo*Color` fields.

## Weapon ODF commands

Use exactly one cost on a launcher:

```cpp
photonTorpedoCost = 1
```

or:

```cpp
quantumTorpedoCost = 1
```

The declared cost applies to each projectile, not to an entire volley. Before
a standard launcher selects a pooled ordnance object for its next shot, the
matching store must contain the cost; the store is debited at Armada's common
post-fire commit after that projectile is launched. Fleet Operations'
`CannonImp` has its own pooled selector and repeat-launch loop, so its guided
and position launch methods are checked and debited directly instead. Thus a four-projectile
volley with `quantumTorpedoCost = 1`
consumes four Quantum Torpedoes. A partial volley stops when its store can no
longer pay for the next projectile. A weapon declaring both costs is ignored
as invalid; a weapon declaring neither is unchanged.

## Resupply providers

`classLabel = "shipyard"` and `classLabel = "RepairShip"` are providers by
default. Their default range is 200 world units. Any Craft can explicitly
enable, disable, or resize this behaviour:

```cpp
torpedoResupply = 1
torpedoResupplyRange = 300
```

`torpedoResupply = 0` also disables the automatic provider role on a yard or
RepairShip. Mode 2 currently requires the provider and receiving Craft to
have the same team number.

## Save games and native access

The two current values are appended to configured Crafts' native save data
and restored on load. Begin a new game after enabling this module for a mod
that uses these commands; an older save has no torpedo-store block.

The DLL exports `Get`, `GetMaximum`, `Set`, and `Add` functions for both
Photon and Quantum Torpedoes. They accept a live Craft pointer and use `float`
amounts.

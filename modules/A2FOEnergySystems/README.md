# A2FOEnergySystems

`A2FOEnergySystems.dll` adds three independent per-Craft ammunition stores:
Photon Torpedoes, Quantum Torpedoes, and Shuttle Craft. A launcher only uses a store when its
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

maxShuttleCraft = 6
shuttleCraftRate = 0.05
shuttleCraftRechargeMode = 2
```

- `maxPhotonTorpedoes` / `maxQuantumTorpedoes` / `maxShuttleCraft`: store capacity. Missing or
  zero disables that store on the Craft.
- `photonTorpedoRate` / `quantumTorpedoRate` / `shuttleCraftRate`: units restored per second.
- recharge mode `1`: recharge automatically.
- recharge mode `2`: recharge only while within range of a same-team resupply
  provider.
- recharge mode `0`: do not recharge.

New Craft stores start full. Values are clamped to their current ODF maximum.

## Selected-craft UI

Selecting a Craft with any store displays its current and maximum Photon,
Quantum, and Shuttle Craft values as completed whole units. Fractional recharge remains
internally precise and becomes visible when the next whole unit completes. This UI
uses `A2FOCraftIdentity.dll`'s selected-panel observer; ammunition simulation
remains available when CraftIdentity is not selected, but these rows do not.

The automatic rows use offsets `+16`, `+40`, and `+64` from the selected name
anchor, keeping all three inside the standard selected panel. A GUI may
position and colour them explicitly:

```text
infoSinglePhotonTorpedoesTextArea = 386 186 340 20
infoSingleQuantumTorpedoesTextArea = 386 214 340 20
infoSingleShuttleCraftTextArea = 386 238 340 20

photonTorpedoColor = 1 0 1
quantumTorpedoColor = 1 0 1
shuttleCraftColor = 1 0 1
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

shuttleCraftDisplayMode = 1
shuttleCraftValueDisplayMode = 1
shuttleCraftLabel = "Shuttle Craft"
shuttleCraftTooltip = "Shuttle Craft"
shuttleCraftVerboseTooltip = "Embarked shuttlecraft complement."
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
`quantumTorpedo*Color` and `shuttleCraft*Color` fields.

## Weapon ODF commands

Use exactly one cost on a launcher:

```cpp
photonTorpedoCost = 1
```

or:

```cpp
quantumTorpedoCost = 1
```

or:

```cpp
shuttleCraftCost = 1
```

The declared cost applies to each projectile, not to an entire volley. Before
a standard launcher selects a pooled ordnance object for its next shot, the
matching store must contain the cost; the store is debited at Armada's common
post-fire commit after that projectile is launched. Fleet Operations'
`CannonImp` has its own pooled selector and repeat-launch loop, so its guided
and position launch methods are checked and debited directly instead. Thus a four-projectile
volley with `quantumTorpedoCost = 1`
consumes four Quantum Torpedoes. A partial volley stops when its store can no
longer pay for the next projectile. A weapon declaring more than one cost is
ignored as invalid; a weapon declaring none is unchanged.

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

Providers may control self-resupply and filter other Craft independently:

```cpp
canResupplySelf = 0
resupplyValidTargets = "type:fighter"
```

`canResupplySelf` defaults to `1` for compatibility. Set it to `0` when the
provider must not act as its own provider; another nearby provider can still
supply it. This setting only affects recharge mode `2`; automatic recharge
mode `1` works independently. The target filter applies only to other Craft,
so it does not change the separate self-resupply setting. Use an explicit
`0` or `1`; an invalid explicit value disables self-resupply and is logged.

`resupplyValidTargets` is one quoted scalar string containing whitespace-
separated rules, not a native ODF pair array. With the field omitted, other
Craft are unrestricted. An explicit empty string denies all other Craft, and
`*` allows all. In an explicit rule list, other Craft that match no rule are
denied unless `*` is present. Rules are case-insensitive:

- `type:fighter` allows receivers tagged with that type.
- `!type:bomber` denies receivers tagged with that type.
- `odf:unit_basename` allows that exact receiver ODF basename.
- `!odf:unit_basename` denies that exact receiver ODF basename.

To allow all other Craft except bomber-tagged ones, include the wildcard:

```cpp
resupplyValidTargets = "* !type:bomber"
```

An optional `.odf` suffix is accepted on ODF names. For a matching receiver,
an exact ODF rule takes precedence over a type rule, which takes precedence
over `*` or the default. Malformed rules or conflicting duplicate rules fail
closed for other Craft and are logged. For example, allow fighter-tagged
Craft except one exact ODF, while explicitly allowing a different ODF:

```cpp
resupplyValidTargets = "type:fighter !odf:fed_noncombat_fighter odf:fed_resupply_shuttle"
```

Receivers set one shared type tag with `resupplyTargetType`; it is compared
with the provider's `type:` rules. A receiver without that field can still
match an exact ODF rule or a wildcard/default. For example, a carrier can
resupply nearby fighter-tagged Craft, while a fighter can receive Photon and
Quantum ammunition from that carrier:

```cpp
// Carrier provider
torpedoResupply = 1
torpedoResupplyRange = 300
canResupplySelf = 0
resupplyValidTargets = "type:fighter"

// Fighter receiver
resupplyTargetType = "fighter"
photonTorpedoRechargeMode = 2
photonTorpedoRate = 1.0
maxPhotonTorpedoes = 8
quantumTorpedoRechargeMode = 2
quantumTorpedoRate = 0.25
maxQuantumTorpedoes = 4
```

The same provider filter applies to Photon, Quantum, and Shuttle Craft stores.
There are no separate per-store provider controls. Existing same-team,
distance, provider-expiry, and other resupply eligibility rules still apply.

## Resupply weapons

`classLabel = "resupplyweapon"` is a status indicator for a provider. Its
weapon icon in the selected panel is grey, like a `UtilityWeapon`, while no
valid receiver is in resupply range. It turns green while one is, and the
weapon fires automatically at that Craft, so the icon's reload fill follows
the weapon's `shotDelay`. To keep the shot silent and invisible, omit
`fireSound` and use a zero-damage ordnance with no visible effect:

```cpp
// Weapon ODF, mounted on the provider
classLabel = "resupplyweapon"
resupplyValidTargets = "type:fighter"   // optional; omitted = any receiver
ordName = "resupply_marker"             // zero damage, no visible effect
shotDelay = 2.0
range = 300                             // at least torpedoResupplyRange
```

The icon colours come from A2FOCraftIdentity: idle uses
`passiveWeaponIconColor` (neutral grey when absent), and active uses
`resupplyWeaponIconColor` (default `0.10 1.00 0.10`). A research-locked
resupply weapon keeps the normal disabled or hidden icon. The DLL exports
`A2FOEnergySystems_GetResupplyWeaponState(weapon)`, which returns `-1` for an
ordinary weapon, `0` when idle and `1` when active.

A receiver is valid, which makes the icon green and lets the weapon fire, only
when all of the following are true:

- its owner is a resupply provider (see above), and
- a same-team Craft in provider range has a recharge-mode-`2` store
  (Photon, Quantum, or Shuttle Craft) that actually went up on this tick
  because of that provider.

If every nearby Craft is already full, nothing goes up, so the icon stays grey
and the weapon does not fire. Automatic (mode `1`) recharge never counts, and neither does the
provider reloading its own stores, so it never fires at its own ship.

The weapon's `resupplyValidTargets` uses the same rule syntax as the provider
field, but selects which **receivers** this weapon reacts to. For example, a
carrier can resupply every ship but play its launch-bay effect only for
fighters. It cannot widen what the provider supplies. The provider's own
filter is applied first. When several Craft are being reloaded, the weapon
stays on its current receiver until that receiver stops reloading, then moves
to the next.

`resupplyweapon` is internally hosted by Armada's native `cannon` class, so
ordinary weapon commands (`ordName`, `fireSound`, `shotDelay`, `range`,
hardpoints, and so on) work as usual. Normal targeting is switched off: Craft
AI and player orders cannot activate it, and any shot aimed at something other
than the current receiver, or at a ground position, is refused at ordnance
selection. A receiver stays valid for 0.5 seconds after its last reloaded
tick. Give the ordnance zero damage; the weapon really fires at an allied
Craft. Its `range` must cover the provider's `torpedoResupplyRange`, or the
icon can be green while the cannon is out of range to fire.

A receiver needs this module's `resupplyTargetType` or a matching ODF name to
pass a type or ODF rule. Native Fleet Operations `validTargets` is not
consulted for receiver selection.

## Save games and native access

The three current values are appended to configured Crafts' native save data
and restored on load. The save block remains 16 bytes: saves written by the
previous two-store version retain their Photon and Quantum values, while the
new Shuttle Craft store starts full. Saves from before the module was enabled
still have no ammunition-store block.

The DLL exports `Get`, `GetMaximum`, `Get...ReloadSeconds`, `Set`, and `Add`
functions for Photon Torpedoes, Quantum Torpedoes, and Shuttle Craft. They
accept a live Craft pointer and use `float` amounts.


## Checkpoint — 2026-10-27: EnergySystems and D3D8 unsupported-call components complete

A2FOEnergySystems now routes its remaining portable ammunition-number, colour,
rectangle, quote-removal, timer-subtraction/admission and amount-addition policy
through Rust. Existing Rust controls retain class loading, firing, resupply,
persistence, public amount APIs, parsing and lifecycle sequencing. Native hooks,
addresses/signatures, checked access, STL lifetime, CRT conversion/formatting,
exception scopes and the disabled direct UI renderer gate remain native.

The remaining D3D8 census unit, renderer/d3d8_compat/game/unsupported.cpp, already
routes all 23 unsupported-method diagnostics and return decisions through Rust.
New full-source tests verify all 23 under normal/custom return, throwing and
reentrant callbacks, including unchanged poisoned output buffers. Native COM
boundaries and exception capture/rethrow remain C++; no new graphics features
or support for those unsupported operations are claimed.

Validation passed: 215 Rust tests, formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and standalone EnergySystems DLL; 134 full-source
EnergySystems trace pairs/268 executions (2,810 equal stdout lines), 1,296
residual numeric rows across 12 x87 precision/rounding modes, the 6,600-row range
regression, 92 D3D8 cases and built-in batch9 admission. Exceptional timer and
amount values include NaNs/infinities; x87 sticky flags, C1, control word and stack
balance are checked for the residual numeric cases. All 44 referenced Rust
implementations link; integrated 197 and standalone 18 exports/ordinals are
unchanged, with no new system imports. Twenty-four native functions/layouts,
constants, SDK/ASM and D3D8 boundary source pins are preserved.

Evidence: build/rust-migration-stage10/energy-systems/checkpoint/validation.json.
Both units are completed_pending_census; strict totals remain 116/151 with 35
uncredited records. Four completed components (FireArcs, Nebula controller,
EnergySystems and D3D8 unsupported-call policy) await shared census validation;
31 other units still need completion/validation. Existing strict common-DLL and
archive bindings were not weakened. Isolated candidate: energy-systems/candidate.
Earlier artifacts, checkpoints and failed attempts are retained. Incremental
native cache reuse is authenticated; volatile Cargo build locks are explicitly
excluded from product source pins. No deployment or game launch occurred;
GPU and in-game acceptance remain pending.

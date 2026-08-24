# A2FOExtensions modular runtime

This package preserves the proven startup chain while separating checked engine
hooks, reusable semantic dispatch, and optional native feature modules.

## User-facing features

- Recursive ODF discovery from arbitrary subdirectories.
- Recursive ODF indexing inside active loose roots and `odf.fpq` archives.
- Correct Data → `ParentMod` → active-mod file precedence.
- Optional `A2FOCheats.dll` enhancement makes `showmethemoney` grant
  individually configurable amounts of Dilithium, Tritanium, Metal, Supplies,
  and Crew (10,000 each by default), and restores the missing `m`, `dis`,
  `crash`, and true team-elimination `elim` cheats.
- Optional ODF-driven captain names and ship registries, row-aligned with the
  native `possibleCraftNames` choice and displayed only for the selected craft
  through `infoSingleCaptainTextArea` and `infoSingleRegistryTextArea`.
- Optional selected-craft rank XP bar, plus the missing native shield-bar
  tooltip with per-Craft short and verbose text overrides.
- Optional always-visible native shield geometry while a configured object's
  current shield strength remains above zero.
- Optional per-instance SOD matrix animation for hardpoint/null nodes, so
  weapons and other gameplay queries follow both directly animated hardpoints
  and animated ancestors.
- Optional DX8 per-pixel ship lighting derived from armadaNebulaPatch, with
  the remaining Fleet Operations alpha-render path preserved and per-diffuse
  subsystem-aware emissive texture channels. Global `ART_CFG.h` emissive and
  bump suffixes can discover material maps without per-ship declarations,
  while explicit ODF/SOD assignments remain authoritative. The stable material
  glow and selective framebuffer bloom run on the managed DXVK backend. The
  Windows system renderer keeps Fleet Operations' complete native DX8 path and
  receives no A2FO mapped-material hooks or SOD texture-slot mutations.
- Optional recursive map-editor menus: a `buildItemX` target containing its own
  `buildItemX` rows opens as another submenu, with native Back navigation.
- Optional live yard submenus: `buildItemXRefitY` turns one normal Producer
  item into a presentation-only parent whose child ODFs retain their own tech,
  cost, build-time, tooltip, and synchronized construction behaviour.
- Optional BuildYard module gates: `moduleXPseudoTechnology` evaluates a fake
  technology-tree project ID in addition to native
  `moduleXRequiredTechnology`, allowing module-specific prerequisite chains.
- Experimental indexed hull-mounted turrets through matching `turretX` and
  `turretHardpointX` ODF commands, with independent weapons, hitpoints, yaw,
  pitch, slew rates, ownership changes, and save/load reconnection.
- Optional `A1Compat.dll` support for the Armada 1 `wingman` classlabel,
  mapped safely to `craft` only through the `STA1 Classic` mod chain.
- Armada 1 `Addon` ODF overlay support through `A1Compat.dll`, preserving A1's
  within-root rule that `Addon` wins over a same-basename structured ODF.
- Runtime translation of Armada 1's named `teamcolor.odf` entries into Fleet
  Operations' Instant Action/minimap player palette, without rewriting A1 data.
- Direct A1 multiplayer-map support for BZN bounds and companion MDF player
  starts, translated into native A2 MapDetails records at runtime.
- Armada 1 starbase officer-quarter compatibility: A1 ODF limits and gains,
  sequential `oqN` model reveal, native FO officer-cap changes, ownership
  reversal, queue admission limits, and compatibility save state.
- `hybridbuild` opt-in classlabel mapped to `research` for the staged
  HybridBuild implementation, including separate construct/yard/research/evolve
  menus, one shared ten-slot queue, queued station placement previews, and
  protected native construction/evolution sidecars.
- Per-Evolver and HybridBuild `cocoon` ODF command for custom cocoon models.
- Native ODF-driven wreckage or replacement objects when units are destroyed.
- Deterministic `wreckageChance` support suitable for synchronized games.
- Ctrl-click to fill all ten native construction-queue slots.
- Ctrl+Alt-click for continuous production and automatic queue refilling.
- Continuous production stops when the queue is manually altered or the yard
  is destroyed.
- Resource-shortage pause and automatic production retry.
- Experimental save/load markers for continuous-production state.
- Automatic aspect-correct scaling of Fleet Operations' D3D9 intro and
  Armada's GDI and menu/campaign Bink movie paths to the active viewport.
- `DefaultGameSpeed` field in `info.ini`, accepting speeds 1–6.
- `SettingsDirectory` field for redirecting mod configuration and profile
  files.
- `%APPDATA%`, `%USERPROFILE%`, and other Windows environment-variable
  expansion.
- Absolute, relative, shared, and per-mod settings-directory layouts.
- Data-level shared settings roots with active-mod folders stored below
  `mods\<folder>`.
- Configurable ship-system upgrade pods through level 16, with an inherited
  `RTS_CFG.h` maximum of level 6 by default.
- Tier-indexed upgrade-station lists which preserve unrelated research and
  progress level 2 → level 3 → higher levels independently for each system.
- Upgrade-pod progression is independent of the order in which systems are
  constructed; removing a higher pod restores the next-highest multiplier.

## Modding framework

- Versioned native module API with backward-compatible capability revisions.
- Automatic deterministic loading of globally installed `Data\modules\*.dll`.
- Per-mod module selection through the Mods-screen **Modules** button and
  `[modules]` policy; mod-local native DLL folders are ignored.
- Data, `ParentMod`, and active-mod configuration and asset precedence.
- Optional direct Armada 1/2 legacy texture bridge for `Textures\RGB`,
  `Textures\Index8`, and `Textures\Compressed` across the same mod roots,
  including bounded expansion of RLE-compressed TGA types 9, 10, and 11.
- Native destroyed-object event dispatcher.
- Native Producer admission/completion/destruction event dispatcher.
- Transactional native-module registration with rollback after failed
  initialization.
- SDK header and example native module.
- Central logging for the core and native modules.
- Checked hook signatures and supported-binary validation.

## Current modding commands

The exhaustive index is
[`docs/modder-command-reference.md`](docs/modder-command-reference.md). It
lists every extension-owned ODF, INI, CFG, asset, SOD, control, and
authoring convention, including features which require no new command. The
sections below provide the most commonly used examples and explanations.

### `info.ini`

Place these fields in the active mod's `[mod]` section:

```ini
[mod]
DefaultGameSpeed = 3
SettingsDirectory = My Mod
```

`DefaultGameSpeed` supplies the initial game speed from 1 through 6. An
existing saved profile still takes precedence.

`SettingsDirectory` redirects the mod's settings files and supports:

- bare directory names;
- relative paths;
- absolute Windows and UNC paths;
- Wine `Z:\` paths;
- variables such as `%APPDATA%` and `%USERPROFILE%`;
- a Data-level shared root with active mods stored below
  `mods\<folder>`.

See [`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md) for the
full resolution rules.

Native modules are selected separately from `[mod]`:

```ini
[modules]
required0 = "A1Compat"
reject0 = "A2FOCheats"
active0 = "A2FOFireArcs"
active1 = "A2FOSwarmSystem"
```

`requiredX` modules are always selected and block launch when absent.
`rejectX` modules are incompatible and cannot be selected. `activeX` records
the optional modules selected for that mod. Names are case-insensitive and may
omit `.dll`; sparse numeric indices are accepted. Parent requirements and
rejections are inherited, while optional selections belong to the most
specific mod. Existing mods without a `[modules]` section retain legacy
load-all behaviour until a selection is saved. Native DLLs must be installed
centrally under `Data/modules`; a mod's own `modules` folder is never loaded.

### Edit-menu ODF commands

The stock `editmenu.odf` and first category level remain unchanged. A category
`buildItemX` may point either to an ordinary `itemX` placement list or to a
file containing another set of `buildItemX` commands:

```text
// ef_ships.odf
menuTitle = "Federation Ships"
buildItem1 = "ef_combat.odf"
buildItem2 = "ef_support.odf"
```

Any target containing at least one `buildItemX` is a submenu; a target without
one remains a native object-list leaf. Each visible page retains the native
12-entry limit, recursive depth is capped at 32, cycles are rejected, and Back
returns one level at a time. See
[`modules/A2FOEditMenu/README.md`](modules/A2FOEditMenu/README.md).

### Single-player mission selector

`A2FOMissionSelector.dll` replaces the separate Single Player and fixed-row
Mission Select dialogs with one scrollable campaign/mission browser. The four
stock campaigns retain native availability, progression, filenames, and
launch. Optional `mission_selector.ini` metadata supplies campaign text,
switchable campaign backgrounds, mission descriptions/objectives, preview
images, replacement BZNs, and additional custom campaign sections; missing
metadata falls back safely to native filenames. Custom campaigns use static
unlock policy until independent saveable progression is implemented. The
browser reuses Armada's borderless shell dialog and native game-window owner,
so it remains embedded in and fills the Fleet Operations client area. See
[`modules/A2FOMissionSelector/README.md`](modules/A2FOMissionSelector/README.md).

`A2FOInstantActionSettings.dll` restores the Instant Action **Load Settings**
control while retaining Armada's native host checks and validation. It supplies
a guarded click fallback when Fleet Operations' replacement Load button misses
the original route and corrects the byte alignment of Fleet Operations' spaced
`setupDetails` profile payload. See
[`modules/A2FOInstantActionSettings/README.md`](modules/A2FOInstantActionSettings/README.md).

`A2FOBuildTooltips.dll` inserts build time in the native parenthesised cost row
as compact `N [build-time icon]` in normal tooltips and expanded `N seconds` in verbose
tooltips. It calls Armada's native adjusted
build-time query, so the value follows the Instant Action build-time setting
and the same local-team modifiers used by construction. With
`A2FOResources.dll`, it inserts every non-zero additional-resource cost into
the same row before the build-time token. See
[`modules/A2FOBuildTooltips/README.md`](modules/A2FOBuildTooltips/README.md).

`A2FOResources.dll` increases the usable resource total from six to ten by
adding independent tritanium, supply, credits, and collective-connections
balances. Object ODF costs, Race starting values, Producer deductions/refunds,
a second resource-panel row, and native get/set/add accessors are included.
Race ODF presentation fields are parsed for all ten resources; values may be
localization keys or literal text. The added four use them in the second panel
row, tooltips, build-cost text, and native API. Short/verbose hover overrides
are also supported for the six native resources. Compact costs and the added
row use Fleet Operations font-icon glyphs, while verbose costs retain the
Race-specific `Res` names. Native presentation integration is applied at
Armada's targeted cost-text sites through a Race-localized cache;
the top resource panel itself renders numbers without a native label. These
pools do not alias latinum, metal, officers, or biomatter. See
[`modules/A2FOResources/README.md`](modules/A2FOResources/README.md).

`A2FOEnergySystems.dll` adds independent Photon and Quantum Torpedo capacity
to Craft ODFs. Each store has its own maximum, fractional recharge rate, and
mode: disabled recharge, automatic recharge, or resupply-only recharge near a
same-team shipyard, `RepairShip`, or explicit provider. Weapon ODF costs are
charged once per successfully launched projectile, including every shot in a
multi-projectile volley; an exhausted store blocks only the launcher which
uses it. New stores start full and their current values persist in new save
games.

`A2FOCraftIdentity.dll` can present each capacity independently as percent,
integer `current/maximum`, localized ready/reload status, or a capacity bar.
Each row supports a per-Craft label or texture-atlas icon, short and verbose
tooltips, explicit GUI placement, and green/yellow/red capacity colours. See
[`modules/A2FOEnergySystems/README.md`](modules/A2FOEnergySystems/README.md).

The same selected-panel module can independently recolour the five native
subsystem icons and their adjacent value text for healthy, low, critical,
disabled, and destroyed states. The native hull/shield/crew icon-values use
that same live-state palette in the mouse-over strip, as does crew in the
selected presentation. The native officer icon/value has its own independent
`officerIconColor` command, while the native special-energy icon/value has an
independent `specialEnergyIconColor`; neither inherits those health colours.
Omitted colours retain Armada's native presentation.

`A2FODirectionalShields.dll` adds strictly opt-in forward, aft, port, and
starboard shield stores while retaining Armada's aggregate shield display and
native shield recharge. Hits select a facing from the attacker's target-local
position; a depleted facing passes later damage to the ordinary hull/system
path even while another facing is charged. Shield-hit effects follow the
struck facing's percentage, are suppressed on exposed facings, and stop when
their facing collapses. It composes through the checked
`A2FOWeaponDamageControls` damage hook.

The selected-panel UI supports a four-sprite arc ring with per-Craft segment
placement, proportional-drain or colour-only display, configurable compass
mapping, green/orange/red/black states, and localized per-facing tooltips with
live strength. Numeric F/A and P/S rows remain available when the sprite ring
cannot be drawn. See
[`modules/A2FODirectionalShields/README.md`](modules/A2FODirectionalShields/README.md).

### `RTS_CFG.h` cheat amounts

Override any of the five `showmethemoney` grants with literal values from 0
through 100,000,000:

```cpp
int SHOWMETHEMONEY_DILITHIUM = 10000;
int SHOWMETHEMONEY_TRITANIUM = 10000;
int SHOWMETHEMONEY_METAL = 10000;
int SHOWMETHEMONEY_SUPPLIES = 10000;
int SHOWMETHEMONEY_CREW = 10000;
```

Values inherit per field through Data, parent mods, and the active mod. Missing
or invalid fields keep the inherited value, falling back to 10,000.

### Persistent shield visibility

Add this to a ship or station ODF:

    alwaysShowShields = 1

The default is 0 and preserves Fleet Operations' normal hit-driven shield
display. When enabled, the native shield effect remains visible while current
shield strength is greater than zero, disappears when shields reach zero, and
returns automatically as shields recover. Shield strength, damage, and
regeneration are not changed.

See
[modules/A2FOAlwaysShowShields/README.md](modules/A2FOAlwaysShowShields/README.md)
for runtime ownership and inheritance details.

### Faction-owned model textures and SOD nodes

Faction ODFs can select a suffix for every owned ship/station model:

```cpp
factionTextureSuffix = "_k"
```

A mesh using `hull` then prefers `hull_k.dds`, falling back per mesh to
`hull_k.tga` and finally its ordinary base/Borg texture. Capturing the unit
changes the suffix to its new owner's faction on the next update. The same
module shows a SOD node whose name matches the owning faction ODF's existing
`name` value and hides nodes matching other loaded factions. Armada's native
`borg` node remains authoritative. The module also lets native Borg `_b`
alternates pass the model preflight when only the DDS exists. See
[`modules/A2FOTextureVariants/README.md`](modules/A2FOTextureVariants/README.md).

The same module can swap out a model part when a subsystem is destroyed:

```cpp
engineMesh1 = "nacelle_l"
engineMesh1explosion = "xfirebsm"
engineMesh2 = "nacelle_r"
engineMesh2explosion = "xfirebsm"
```

It selects one valid numbered SOD node per destruction, hides it only on that
craft, places the paired explosion at the node, emits localized native repair
sparks while hitpoints rise, and restores the part when the subsystem is fully
operational. `sensor`, `engine`, `weapon`, `lifeSupport`, and
`shieldGenerator` are the supported command stems.

### Weapon shield and hull damage controls

Ordinary weapon ODFs can independently disable shield or hull damage:

```cpp
canDamageShields = true
canDamageHull = true
shieldDamageModifier = 1.0
hullDamageModifier = 1.0
```

The booleans default to `true` and the modifiers default to `1.0`, leaving
every existing weapon unchanged. `shieldDamageModifier` multiplies damage
while shields absorb the hit; `hullDamageModifier` multiplies damage against
exposed hull and shield-breaking spillover. Set
only `canDamageHull = false` for a shield-only weapon: shields take their
normal share and any spillover is discarded. Set only
`canDamageShields = false` to make shields block the complete hit until they
are down, after which the weapon can damage hull normally.
Setting both to `false` suppresses the weapon's primary shield and hull damage.
Both modifiers also accept Armada's `hitChance`/`damageBase` target-table form:

```cpp
shieldDamageModifier = 1.0 "fed_sovereign.odf" 0.5 "bcruise1.odf" 2.0
hullDamageModifier = 1.0 "fed_sovereign.odf" 1.5 "bcruise1.odf" 0.25
```

The first value is the fallback and each quoted unit ODF/value pair overrides
it when that unit class is the target.
These commands belong in the general **weapon ODF**, not its ordnance ODF.
See
[`modules/A2FOWeaponDamageControls/README.md`](modules/A2FOWeaponDamageControls/README.md).

### Weapon fire arcs — read this first

Fire-arc commands belong in the general **weapon ODF**, not its ordnance ODF.
They describe an owner-local permission volume: a weapon may fire when the
target is inside it, even if the ship is stationary or its engines are
disabled. They do not order the ship to rotate or move.

The simplest useful form is a box with independent horizontal and vertical
coverage:

```cpp
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 0
fireArcYawAngle = 90
fireArcPitchAngle = 60
```

The centre angles select a direction; the angle fields are total widths. In
this example the weapon reaches 45 degrees left/right and 30 degrees up/down.

```text
Yaw:     0 forward, 90 right, 180 rear, 270 left
Pitch: +90 up,       0 level,            -90 down
```

Yaw wraps around the ship. Pitch does not wrap and is clamped to `-90..+90`.
Use `box` for most arrays and hemispheres. Use `cone` with `fireArcAngle` only
when a circular fixed-cannon or barrel-shaped volume is wanted. Native range,
target-validity, and obstruction checks remain active.

Custom fire arcs are globally enabled by default. `int firearc = 0;` in
`RTS_CFG.h` disables both custom firing enforcement and its hover preview;
`int firearc = 1;` explicitly enables them. Data, parent, and active-mod files
use normal overlay precedence.

For a configured weapon with an existing `weaponXiconpos` system icon,
hovering that icon projects the live arc from every linked hardpoint into the
tactical view. Cyan lines show the boundary and a gold line shows its centre;
the complete wireframe turns green when the weapon's live target enters the
arc. The hover preview does not alter the icon's normal input or tooltip.

Those defaults can be changed in the active interface `.cfg` with
`fireArcBoundaryColor`, `fireArcCenterColor`, and
`fireArcValidTargetColor`, using the usual three `0..1` colour channels.

The detailed guide includes orientation diagrams, upper/lower hemisphere
examples, box-versus-cone corner behaviour, aliases, validation rules, runtime
ordering, and technology-tree troubleshooting:
[`modules/A2FOFireArcs/README.md`](modules/A2FOFireArcs/README.md).

### Point-defense firing cycles

`PointDefenseLaser` and `OrdnanceDefenseWeapon` weapon ODFs can use a
contiguous numbered delay sequence:

```cpp
shotDelay0 = 0.1
shotDelay1 = 0.1
shotDelay2 = 2.0
saveFireCyclePoint = 2
shotCycleResetTime = 15.0
```

Each successful shot/interception advances once. The final entry loops to
`saveFireCyclePoint`, and a ready weapon which remains idle for
`shotCycleResetTime` returns to `shotDelay0`. Numbered delays override the
ordinary `shotDelay`; without `shotDelay0`, the module enforces unnumbered
`shotDelay` as a single-delay cycle. For `PointDefenseLaser`, this replaces
Roots' late Jan_B timer check with a pre-fire countdown gate. Targeting,
interception, hit chance, attack behavior, and native reload modifiers are
unchanged. See
[`modules/A2FOPointDefenseCycles/README.md`](modules/A2FOPointDefenseCycles/README.md)
for validation, save/load, and runtime details.

### Dynamic ambient swarms

Any rendered `GameObject` ODF can define one or more independent groups of
lightweight visual traffic:

```cpp
swarm0 = "fbee"
swarm0Count = 10
swarm0Radius = 50.0
swarm0Hardpoint = "dock01" "dock02"
swarm0Interaction = "hp01" "hp02"
swarm0InteractionTime = 3.0
```

Each member is a native shared-model instance plus host-local movement state,
not a ship or map unit. It has no AI, weapons, selection, collision, physics,
commands, or save record. Routes, speeds, interaction visits, dwell periods,
and occasional returns are randomized per member. Lightweight swept sphere
avoidance keeps the visuals outside the host mesh, while local separation keeps
members from clumping at shared destinations, without making them physics
objects. Multiple numbered swarm definitions, moving hosts, hardpoint lists,
and automatic reconstruction after load are supported. See
[`modules/A2FOSwarmSystem/README.md`](modules/A2FOSwarmSystem/README.md) for the
complete command table, limits, lifecycle, and runtime boundary.

### Animated hardpoints

`A2FOAnimatedHardpoints` makes existing SOD matrix channels affect gameplay
hardpoint positions and transforms. It requires no ODF command: animate the
null node normally in the model. A hardpoint also follows matrix animation on
any ancestor in its SOD hierarchy. See
[`modules/A2FOAnimatedHardpoints/README.md`](modules/A2FOAnimatedHardpoints/README.md)
for the runtime boundary and compatibility details.

For visual authoring, the cross-platform
[`A2FO Arc Lab`](tools/A2FOArcLab/README.md) debug tool loads the ship ODF and
SOD, discovers each weapon's linked hardpoints, draws live box/cone coverage,
and tests a movable target probe through the exact same C++ geometry used by
the DLL. It can cycle individual hardpoints or display every linked hardpoint
at once, then copy or save the finished weapon-ODF block.

### Object ODF commands

Treat a compatibility object as an ordinary craft:

```text
classLabel = "wingman"
```

This alias is supplied by `A1Compat.dll` in the `STA1 Classic` parent mod; it
is not enabled globally for unrelated FO4 or STA2 mods. If a wingman ODF and
its include chain omit one of the stock craft identity flags or subsystem
damage percentages, A1Compat supplies the corresponding value from STA1
Classic's `a2craft.odf`. Any value declared by the object or inherited from a
parent ODF remains authoritative.

The same missing-only behavior applies `a2const.odf`'s six common constructor
commands to objects whose original classlabel is `constructionrig`.

Objects with the original `freighter` classlabel similarly receive the seven
mining/resource defaults from `a2freight.odf` when those commands are absent.

Mount linked turret objects on a Craft-derived parent with indexed pairs:

```text
turret0 = "bsg_dual_turret"
turretHardpoint0 = "hp_turret00"
```

Indices 0 through 64 may be sparse. The referenced ODF uses
`classLabel = "turret"`, owns its weapons and hitpoints, and can configure yaw,
pitch, slew rates, and rest angles. See
[`modules/A2FOTurrets/README.md`](modules/A2FOTurrets/README.md) for the full
contract and current first-version limitations.

Ordinary cannon, phaser, pulse, and torpedo weapon ODFs can also use the
active Fleet Operations technology tree. Add the weapon ODF to the usual
`.tt` file just like any other project:

```text
fbphas.odf 1 fresearch.odf
```

An unlisted normal weapon is treated as `0` and remains available. Listed
weapons use Fleet Operations' native prerequisite evaluation; special weapons
retain their existing native path. See
[`modules/A2FONormalWeaponTech/README.md`](modules/A2FONormalWeaponTech/README.md).

Give a Craft-derived object candidate captain names and registries:

```text
possibleCraftNames = "USS Enterprise" "USS Excelsior"
possibleCaptainNames = "Captain A" "Captain B"
possibleCraftRegistry = "NCC-1701" "NX-2000"
```

Entry `N` in both companion lists follows Fleet Operations' native ship-name
entry `N`. Enable their selected-panel positions with
`infoSingleCaptainTextArea` and `infoSingleRegistryTextArea` in the active GUI
configuration, and optionally set `captainNameColor` and `shipRegistryColor`.
See
[`modules/A2FOCraftIdentity/README.md`](modules/A2FOCraftIdentity/README.md)
for placement, fallback, determinism, and save/load details.

Select a custom cocoon model for an Evolver (`.sod` is optional):

```text
classLabel = "evolver"
cocoon = "custom_cocoon.sod"
```

Request a replacement or wreckage object when a craft is destroyed:

```text
wreckage = "my_ship_wreck"
wreckageChance = 50
```

`wreckage` is the replacement ODF basename. `wreckageChance` accepts 0–100
and defaults to 100. Select `A2FOWreckage.dll` for the mod to enable this
native policy.

`A2FORefitYards.dll` lets a Craft declare `refitItem0` through
`refitItem15`. Selecting one returns the Craft to the nearest same-team
Shipyard with `refitHardpoint`; the destination then uses the yard's native
Producer queue, costs, `buildTime`, progress UI, and cancellation/refund path.
The source pathfinds to the hardpoint's oriented outside approach, then only
that source object's collision-avoidance participation is suspended while a
five-second synchronized position/orientation transition docks it. Active
cancellation hands the original ship to the yard's native completed-build
output queue, giving it the same launch path as a finished ship and restoring
its collision state under native queue protection. The completed object
replaces the original through the safe Evolver identity handoff. See
[`modules/A2FORefitYards/README.md`](modules/A2FORefitYards/README.md).

Higher ship-system upgrade pods continue to use Armada's existing field:

```text
upgradeLevel = 4
```

A2FO safely retains levels above 3 in sidecar state rather than indexing past
Armada's fixed Team arrays. Select the permitted maximum in inherited
`RTS_CFG.h`:

```cpp
int upgradePodMaximumTier = 6;
```

Valid values are 3–16. The default is 6, and a valid child-mod assignment
overrides its parent's assignment.

Fleet Operations treats a child `RTS_CFG.h` as a complete native replacement,
not a partial overlay. Copy the full parent file into the child before adding
the assignment, retaining includes such as `ART_CFG.h`; a minimal child file
will discard the parent's native camera, renderer, map, interface, and gameplay
settings.

### Upgrade-station ODF commands

The station command tier is zero-based after the ship's built-in level-1
systems, so command tier 0 selects upgrade level 2:

```text
tier0BuildItem0 = "pod_weapons_2"
tier1BuildItem0 = "pod_weapons_3"
tier2BuildItem0 = "pod_weapons_4"
tier3BuildItem0 = "pod_weapons_5"
tier4BuildItem0 = "pod_weapons_6"
```

Use the same item index and `upgradeSystem` for every level in one chain. Each
command replaces only its matching index; unspecified indices retain their
vanilla or Fleet Ops entries. The referenced pod's `upgradeLevel` must equal
the command tier plus 2. See
[`docs/upgrade-pods.md`](docs/upgrade-pods.md) for validation and fallback
rules.

### Construction controls

- **Ctrl + click:** fill every remaining position in the ten-slot native
  construction queue.
- **Ctrl + Alt + click:** activate continuous production and automatic queue
  refilling.

While continuous production is active, selecting the active item normally,
selecting a different item, deleting or clearing an item, or destroying the
yard disables repeat. Resource shortages pause production and trigger
automatic retries.

The queue save/load and multiplayer behaviour remain partially validated.
Basic level-2 → level-3 → level-4 upgrade-pod progression is confirmed in
game; higher levels, save/load, and multiplayer still need validation. Button
overlays, configurable hotkeys, and the proposed Noxter mechanics are not yet
implemented.

## Core/module boundary

The core owns shared call sites, dispatch ordering, engine-object lifetimes, and
registration rollback. Native modules handle features that need deeper
engine/filesystem access, such as recursive ODF discovery, cocoon SOD selection,
and Producer integration. Optional gameplay policies are isolated in native
modules which register against narrow semantic APIs. See
[`docs/architecture.md`](docs/architecture.md) for the ownership map and
hook-maintenance rules.

Queue controls and their current validation status are documented in
[`docs/queue-enhancements.md`](docs/queue-enhancements.md). The complete hook
ledger—including supported binary identities, every direct patch and byte
signature, helper/global RVAs, startup-loader provenance, and object-layout
offsets—is in [`docs/addresses.md`](docs/addresses.md).
The two optional Fleet Ops mod-information fields are documented in
[`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md).
The complete modder command and feature index is in
[`docs/modder-command-reference.md`](docs/modder-command-reference.md).
Copyable Race/Craft/weapon ODF blocks and the complete GUI/misc integration
pack for resources, font glyphs, ammunition, directional shields, shield
tooltips, and the XP bar are in
[`docs/odf-gui-integration-guide.md`](docs/odf-gui-integration-guide.md).
Legacy texture-folder activation and precedence are documented in
[`modules/A2FORGBTextures/README.md`](modules/A2FORGBTextures/README.md).
Faction-owned model texture suffixes, Race-name SOD nodes, and the Borg DDS
alternate repair are documented in
[`modules/A2FOTextureVariants/README.md`](modules/A2FOTextureVariants/README.md).
Captain/registry ODF lists, selected shield/XP tooltips, and the ranked-craft
XP bar are documented in
[`modules/A2FOCraftIdentity/README.md`](modules/A2FOCraftIdentity/README.md).
Persistent shield visibility is documented in
[modules/A2FOAlwaysShowShields/README.md](modules/A2FOAlwaysShowShields/README.md).
Recursive editor-menu ODF nesting is documented in
[`modules/A2FOEditMenu/README.md`](modules/A2FOEditMenu/README.md).
The Instant Action Load Settings repair is documented in
[`modules/A2FOInstantActionSettings/README.md`](modules/A2FOInstantActionSettings/README.md).
Build-button time text and modifier behaviour are documented in
[`modules/A2FOBuildTooltips/README.md`](modules/A2FOBuildTooltips/README.md).
The four additional independent resources and their ODF/GUI commands are
documented in
[`modules/A2FOResources/README.md`](modules/A2FOResources/README.md).
The combined campaign and mission browser is documented in
[`modules/A2FOMissionSelector/README.md`](modules/A2FOMissionSelector/README.md).
Three-dimensional weapon firing volumes are documented in
[`modules/A2FOFireArcs/README.md`](modules/A2FOFireArcs/README.md).
Photon and Quantum Torpedo ammunition is documented in
[`modules/A2FOEnergySystems/README.md`](modules/A2FOEnergySystems/README.md).
Native destroyed-craft wreckage replacement is documented in
[`modules/A2FOWreckage/README.md`](modules/A2FOWreckage/README.md).
Independent weapon shield/hull damage controls are documented in
[`modules/A2FOWeaponDamageControls/README.md`](modules/A2FOWeaponDamageControls/README.md).
Normal-weapon technology-tree enforcement is documented in
[`modules/A2FONormalWeaponTech/README.md`](modules/A2FONormalWeaponTech/README.md).
Point-defense numbered shot-delay cycles are documented in
[`modules/A2FOPointDefenseCycles/README.md`](modules/A2FOPointDefenseCycles/README.md).
Dynamic render-only ambient swarms are documented in
[`modules/A2FOSwarmSystem/README.md`](modules/A2FOSwarmSystem/README.md).
Indexed hull-turret ODF commands and validation status are documented in
[`modules/A2FOTurrets/README.md`](modules/A2FOTurrets/README.md).
Ship-to-yard refit commands and current save limitation are documented in
[`modules/A2FORefitYards/README.md`](modules/A2FORefitYards/README.md).
DX8 per-pixel lighting, subsystem emissive commands, damage decals, selected
`logoFileNames` hull-name decals, installation, and current shader limitations
are documented in
[`modules/A2FONebulaRenderer/README.md`](modules/A2FONebulaRenderer/README.md).
Armada 1 parent-mod scope and installation are documented in
[`docs/a1-compatibility.md`](docs/a1-compatibility.md).
Upgrade-pod configuration and ODF commands are documented in
[`docs/upgrade-pods.md`](docs/upgrade-pods.md).

## Expected runtime layout

```text
Armada II/
├── A2FOExtensions.dll
├── A2FORendererHelper.exe
├── A2FORenderer.ini
├── Win2kDisableTaskSwitch.dll
├── Win2kDisableTaskSwitch.original.dll
├── d3d9.dll                 (active only when DXVK is selected)
├── renderers/
│   └── dxvk/
│       └── d3d9.dll             (32-bit DXVK payload)
├── modules/
│   ├── A2FOAlwaysShowShields.dll
│   ├── A2FOAnimatedHardpoints.dll
│   ├── A2FOBuildTooltips.dll
│   ├── A2FOCheats.dll
│   ├── A2FOCraftIdentity.dll
│   ├── A2FOEditMenu.dll
│   ├── A2FODirectionalShields.dll
│   ├── A2FOEnergySystems.dll
│   ├── A2FOInstantActionSettings.dll
│   ├── A2FOMissionSelector.dll
│   ├── A2FOFireArcs.dll
│   ├── A2FOFeaturePack.dll
│   ├── A2FOHybridBuild.dll
│   ├── A2FOInfoIni.dll
│   ├── A2FONebulaRenderer.dll
│   ├── A2FONormalWeaponTech.dll
│   ├── A2FOPointDefenseCycles.dll
│   ├── A2FOResources.dll
│   ├── A2FORGBTextures.dll
│   ├── A2FOSwarmSystem.dll
│   ├── A2FOTextureVariants.dll
│   ├── A2FOTurrets.dll
│   ├── A2FOWeaponDamageControls.dll
│   ├── A2FOWreckage.dll
│   └── A1Compat.dll
├── Shaders/
│   └── dx8/
│       └── pixel/
│           ├── ps.nvv
│           └── ps_specular.nvv
├── RTS_CFG.h                (`upgradePodMaximumTier`; mod-overridable)
└── A2FOExtensions.log
```

The core retains its original log filename. Module and script messages are
prefixed in the shared log so startup order and overlay selection remain easy
to diagnose.

## Building on Nobara/Fedora

This project must be compiled as **32-bit Windows x86**, because Armada II and
Fleet Operations are 32-bit processes. Do not use the 64-bit MinGW compiler.

Install the toolchain:

```bash
sudo dnf install mingw32-gcc-c++ mingw32-binutils make
```

Build the release artifacts:

```bash
chmod +x build.sh
./build.sh
```

Or use Make directly:

```bash
make -j"$(nproc)"
make verify
make smoke
```

Outputs:

```text
build/A2FOExtensions.dll
build/A2FORendererHelper.exe
build/Win2kDisableTaskSwitch.dll
build/modules/A2FOAlwaysShowShields.dll
build/modules/A2FOAnimatedHardpoints.dll
build/modules/A2FOBuildTooltips.dll
build/modules/A1Compat.dll
build/modules/A2FOCheats.dll
build/modules/A2FOFeaturePack.dll
build/modules/A2FOHybridBuild.dll
build/modules/A2FOInfoIni.dll
build/modules/A2FOCraftIdentity.dll
build/modules/A2FOEditMenu.dll
build/modules/A2FODirectionalShields.dll
build/modules/A2FOEnergySystems.dll
build/modules/A2FOInstantActionSettings.dll
build/modules/A2FOMissionSelector.dll
build/modules/A2FOFireArcs.dll
build/modules/A2FONebulaRenderer.dll
build/modules/A2FONormalWeaponTech.dll
build/modules/A2FOPointDefenseCycles.dll
build/modules/A2FOResources.dll
build/modules/A2FORGBTextures.dll
build/modules/A2FOSwarmSystem.dll
build/modules/A2FOTextureVariants.dll
build/modules/A2FOTurrets.dll
build/modules/A2FOWeaponDamageControls.dll
build/modules/A2FOWreckage.dll
build/Shaders/dx8/pixel/ps.nvv
build/Shaders/dx8/pixel/ps_specular.nvv
build/licenses/armada-nebula-patch.txt
```

Build the optional parent-mod package with `make sta1-classic` and inspect it
with `make verify-sta1-classic`. The staged files appear under
`build/sta1-classic/`; install that content as `Data/Mods/STA1 Classic`.

The SDK example is deliberately excluded from releases. Build and inspect it
separately with `make sdk-examples verify-sdk`.

The build links the MinGW runtime statically. `make verify` rejects outputs
that depend on deploy-time MinGW DLLs such as `libwinpthread-1.dll`; Armada's
`Data` directory should not need compiler runtime files added to it.
On Linux, `make smoke` also verifies under Wine that the core DLL loads and
exports `A2FO_Initialize`.

For non-invasive performance and leak measurements, `make telemetry` builds a
native 64-bit Linux monitor at `build/a2fo_telemetry`. It observes the Wine
process through `/proc` and, when requested, NVML; it does not inject into the
game or control windows/input. It records GPU-wide telemetry and attempts to
record per-process VRAM when the Wine graphics process is exposed by NVML.
Usage and baseline interpretation are recorded in
[`PERFORMANCE_LOG.md`](PERFORMANCE_LOG.md).

Before installing the proxy, rename the original shipped startup DLL:

```text
Win2kDisableTaskSwitch.dll
    -> Win2kDisableTaskSwitch.original.dll
```

Then copy the newly built `Win2kDisableTaskSwitch.dll`,
`A2FOExtensions.dll`, `A2FORendererHelper.exe`, the `modules` directory, and
the `Shaders` directory into the Fleet Operations `Data` directory.

## Renderer selection

A2FOExtensions adds a **Renderer** list to Fleet Operations' native Graphics
Options screen:

- **System Direct3D 9 (Windows / WineD3D)** uses Windows' native D3D9 on
  Windows and WineD3D under Wine.
- **DXVK (Vulkan)** uses the 32-bit DXVK `d3d9.dll` stored at
  `Data/renderers/dxvk/d3d9.dll`.

Renderer choice is installation-wide and is saved in
`Data/A2FORenderer.ini`. A visible note on the form explains that a game
restart means fully exiting and relaunching Fleet Operations; the in-engine
reset cannot replace a loaded graphics DLL. The game never replaces its live graphics DLL:
`A2FORendererHelper.exe` waits for Armada to exit, then activates or removes
the A2FO-managed `Data/d3d9.dll` for the next start. On the first DXVK switch,
an existing system wrapper such as ReShade is preserved byte-for-byte at
`Data/renderers/system/d3d9.dll`; switching back restores that file. Once the
backup exists, the helper refuses to replace an active DLL that matches neither
the managed DXVK payload nor the saved system wrapper. Results and errors are
recorded in `Data/A2FORenderer.log` and `A2FORenderer.ini`.

At startup, A2FO also compares the active `Data/d3d9.dll` with the managed
payload. That file state is authoritative over stale `AppliedBackend` metadata
when selecting the DXVK-specific DOT3/bloom safety path, and the Graphics page
reconciles its applied-state display from the same files.

The same screen adds **Emissive Maps** and **Specular Maps** switches beside
Fleet Operations' native **Bump Mapping** switch. These two effects apply
immediately, default to enabled, and are persisted in the `[Effects]` section
of `Data/A2FORenderer.ini`; changing them does not require a restart. With the
managed DXVK backend, emissive maps also receive selective native framebuffer
bloom. It defaults on and can be disabled for the next start with
`EmissiveBloom=0` in the same section; the system renderer retains sharp
emissive centres without the private bloom targets.

DXVK is optional and is not produced by this source build. Install the 32-bit
DXVK D3D9 DLL in the payload location above before selecting it. Wine launchers
should retain a native-then-builtin D3D9 override such as `d3d9=n,b`: with the
managed DLL present Wine loads DXVK, and without it Wine falls back to WineD3D.

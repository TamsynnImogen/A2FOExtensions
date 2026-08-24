# Modder command and feature reference

This is the central index of the public modding surface shipped by
A2FOExtensions. It lists every extension-owned ODF, INI, CFG, asset, and
authoring convention. The linked module guides remain authoritative for
runtime details, validation rules, and current limitations.

For copyable, file-by-file setup of the ten-resource panel, resource/build-time
font glyphs, selected shield and XP UI, Photon/Quantum ammunition, and
directional-shield sprites, see the
[ODF and GUI/misc integration guide](odf-gui-integration-guide.md).

Unless a section says otherwise, ODF commands use Armada's normal
case-insensitive `ParameterDB` lookup and may be inherited through `#include`.
Use the spelling shown here in new files. Boolean commands accept the usual
`0`/`1` form; modules which also accept words document that explicitly.

## Module and feature catalogue

Native DLLs are installed centrally in `Data/modules`. A mod selects them by
name in its `info.ini`; a mod-local `modules` directory is deliberately not
loaded.

| Module | Modder-facing feature or configuration |
| --- | --- |
| [`A1Compat`](../modules/A1Compat/README.md) | `a1compat.ini`, the `wingman` alias, A1 missing-only ODF defaults, `Addon` overlay, and officer-quarter compatibility |
| [`A2FOAlwaysShowShields`](../modules/A2FOAlwaysShowShields/README.md) | `alwaysShowShields` object command |
| [`A2FOAnimatedHardpoints`](../modules/A2FOAnimatedHardpoints/README.md) | SOD matrix animation for gameplay hardpoint/null transforms; no ODF command |
| [`A2FOBuildTooltips`](../modules/A2FOBuildTooltips/README.md) | adjusted build-time text in normal and verbose build-button tooltips; no ODF command |
| [`A2FOCheats`](../modules/A2FOCheats/README.md) | configurable `showmethemoney` resources and restored single-player chat cheats |
| [`A2FOCraftIdentity`](../modules/A2FOCraftIdentity/README.md) | captain/registry lists, ammunition/directional-shield UI, XP bar, and selected shield/XP tooltips |
| [`A2FODirectionalShields`](../modules/A2FODirectionalShields/README.md) | optional forward, aft, port, and starboard Craft shield facings |
| [`A2FOEditMenu`](../modules/A2FOEditMenu/README.md) | recursive `buildItemX` edit-menu submenus |
| [`A2FOFeaturePack`](../modules/A2FOFeaturePack/README.md) | recursive ODF/FPQ discovery, BuildYard pseudo-technology gates, queue controls, extended upgrade pods, and viewport-correct Bink movies |
| [`A2FOFireArcs`](../modules/A2FOFireArcs/README.md) | three-dimensional weapon fire volumes and tactical hover preview |
| [`A2FOEnergySystems`](../modules/A2FOEnergySystems/README.md) | Photon and Quantum Torpedo ammunition, recharge, and resupply |
| [`A2FOHybridBuild`](../modules/A2FOHybridBuild/README.md) | `hybridbuild`, four production lists, shared queue, placements, and cocoons |
| [`A2FOInfoIni`](../modules/A2FOInfoIni/README.md) | `SettingsDirectory` and `DefaultGameSpeed` |
| [`A2FOInstantActionSettings`](../modules/A2FOInstantActionSettings/README.md) | restored Instant Action `Load Settings` behavior; no new command |
| [`A2FOMissionSelector`](../modules/A2FOMissionSelector/README.md) | scrollable stock/custom campaign browser and `mission_selector.ini` |
| [`A2FONebulaRenderer`](../modules/A2FONebulaRenderer/README.md) | DX8 per-pixel lighting, emissive/specular maps, damage decals, and ship-name logo decals |
| [`A2FONormalWeaponTech`](../modules/A2FONormalWeaponTech/README.md) | normal-weapon `.tt` prerequisite enforcement; no new ODF command |
| [`A2FOPointDefenseCycles`](../modules/A2FOPointDefenseCycles/README.md) | CannonImp-style numbered point-defense firing delays |
| [`A2FOResources`](../modules/A2FOResources/README.md) | four independent resources, object costs, Race starting values, panel row, and native accessors |
| [`A2FORGBTextures`](../modules/A2FORGBTextures/README.md) | presence-based legacy RGB/Index8/Compressed TGA loading; no ODF command |
| [`A2FOSwarmSystem`](../modules/A2FOSwarmSystem/README.md) | lightweight render-only ambient swarms |
| [`A2FOTextureVariants`](../modules/A2FOTextureVariants/README.md) | faction textures/nodes, Borg DDS repair, and subsystem damage meshes |
| [`A2FOTurrets`](../modules/A2FOTurrets/README.md) | indexed independently armed hull turrets |
| [`A2FORefitYards`](../modules/A2FORefitYards/README.md) | synchronized ship refits through native Shipyard queues |
| [`A2FOWeaponDamageControls`](../modules/A2FOWeaponDamageControls/README.md) | independent shield/hull permission and damage multipliers |
| [`A2FOWreckage`](../modules/A2FOWreckage/README.md) | deterministic native `wreckage` replacement policy |

The core also supplies deterministic module loading, module policy, checked
semantic dispatch, and the versioned
[`native module SDK`](../sdk/README.md).

## `info.ini`

### Mod defaults

Place these in `[mod]`:

| Command | Value | Meaning |
| --- | --- | --- |
| `SettingsDirectory` | path | Redirect the mod's configuration/profile directory. Bare, relative, absolute, UNC, Wine `Z:\`, and `%VARIABLE%` forms are supported. |
| `DefaultGameSpeed` | integer `1..6` | Initial game speed when a saved profile has not already supplied one. |

See [Fleet Operations info defaults](fleetops-info-defaults.md) for precedence
and path resolution.

### Module policy

Place these in `[modules]`. `X` is a sparse non-negative integer:

| Command | Meaning |
| --- | --- |
| `requiredX` | Select the named installed module and block launch if it is missing. |
| `rejectX` | Mark the named installed module incompatible with this mod chain. |
| `activeX` | Record an optional module selected for this specific mod. |

Names are case-insensitive and `.dll` is optional. Requirements and rejections
inherit through `ParentMod`; the most specific mod owns the optional `activeX`
list. A mod chain with no `[modules]` section retains legacy load-all behavior.
The Mods screen's **Modules** button edits only `activeX` rows.

## Ten-resource extension

`A2FOResources.dll` retains Armada/Fleet Operations' six native pools and adds
four independent sidecar pools: tritanium, supply, credits, and collective
connections. They are deliberately separate from latinum, metal, officers,
and biomatter even though older Fleet Operations interfaces used those native
slots as aliases.

Object costs are non-negative integers and default to zero:

```ini
tritaniumCost = 120
supplyCost = 8
creditsCost = 25
collectiveconnectionsCost = 3
```

Race ODFs may define `normalTritanium`, `lotsTritanium`, `normalSupply`,
`lotsSupply`, `normalCredits`, `lotsCredits`,
`normalCollectiveConnections`, and `lotsCollectiveConnections`. GUI rectangles
`resource_6` through `resource_9` optionally position the second resource row.

All ten resources accept Race-specific presentation fields. The added four are
consumed by A2FO's panel, tooltip, build-cost, and API paths. Targeted
ResourceComponent paths consume the short and verbose tooltip fields for crew,
dilithium, latinum, metal, and biomatter. Native compact costs use cached icon
glyphs, including the separately stored officer field, while verbose costs use
the cached `Res` names. The native top resource panel has no label text to
replace. Use one of
`crew`, `officer`, `dilithium`, `latinum`, `metal`, `biomatter`, `tritanium`,
`supply`, `credits`, or `collectiveconnections` followed by `Res`, `Tooltip`,
`VerboseTooltip`, or `Icon`. A value names a `Dynamic_Localized_Strings.h` key when one
exists and otherwise acts as literal text. For example:
`creditsRes = "GUI_CP_FED_CREDITS_RES"`. A native field such as
`metalRes = "Duranium"` changes the verbose palette cost name, while
`metalIcon` changes its compact glyph. Neither changes the top-panel number.

See [the resource module guide](../modules/A2FOResources/README.md) for layout,
native accessors, and the current save-persistence limitation.

## Photon and Quantum Torpedo stores

`A2FOEnergySystems.dll` adds two per-Craft ammunition pools. Craft ODFs use
`maxPhotonTorpedoes`, `photonTorpedoRate`, and
`photonTorpedoRechargeMode`, or `maxQuantumTorpedoes`,
`quantumTorpedoRate`, and `quantumTorpedoRechargeMode`. Mode `1`
recharges continuously; mode `2` recharges only near a same-team provider.
Weapon ODFs consume them with `photonTorpedoCost` or
`quantumTorpedoCost`. The selected cost is charged once for each successfully
launched projectile, so multi-projectile volleys consume one cost per shot.

Configured stores appear in the selected-craft panel as whole-number
`current/maximum` values. GUI rectangles `infoSinglePhotonTorpedoesTextArea` and
`infoSingleQuantumTorpedoesTextArea` optionally position the two rows;
`photonTorpedoColor` and `quantumTorpedoColor` optionally colour them.
Without explicit rectangles the rows use offsets `+16` and `+40` from the
selected name anchor.

Each Craft ODF may customize their presentation independently:

```cpp
photonTorpedoDisplayMode = 1
photonTorpedoValueDisplayMode = 0
photonTorpedoLabel = "Photon Magazine"
photonTorpedoTooltip = "Photon Torpedo Ammunition"
photonTorpedoVerboseTooltip = "The ship's photon torpedo reserve."

quantumTorpedoDisplayMode = 2
quantumTorpedoValueDisplayMode = 1
quantumTorpedoIcon = "all_interface"
quantumTorpedoIconPos = 71 151 34 34
quantumTorpedoTooltip = "Quantum Torpedo Ammunition"
quantumTorpedoVerboseTooltip = "The ship's quantum torpedo reserve."
```

Display mode `1` shows `label: current/maximum`; mode `2` shows the selected
atlas crop followed by `current/maximum`. `*Icon` may name a registered GUI
sprite, or one of the stock atlas names `all_interface`, `all_interface2`,
`all_interface_races`, `all_interface_ranks`, and `all_interface_ranks2`.
`*IconPos` is the `x y width height` source rectangle inside that texture; the
renderer places it automatically at the store's text row. A missing or invalid
icon retains the compact `current/maximum` value without restoring the label.
Label and tooltip values name a
`Dynamic_Localized_Strings.h` key when one exists and otherwise act as literal
text. Native normal/verbose tooltip timing applies to both the row and icon.

The separate `*ValueDisplayMode` follows Fleet Operations'
`specialEnergyDisplayMode` convention and defaults to `0`:

- `0` displays the remaining store as an integer percent.
- `1` displays integer `current/maximum` amounts.
- `2` displays `GUI_SD_SPE_READY` when full, rounded-up seconds while actively
  recharging, or `GUI_SD_AMMO_WAITING` while a resupply-only store is out of
  provider range. The latter key falls back to `Resupply` when it is absent.
- `3` replaces the value text with a left-to-right capacity bar. Text mode
  retains the label; icon mode retains the icon.

Both the text and icon are green above 50%, yellow from 25% through 50%, and
red at or below 25%. GUI configuration may override those colours independently
for each store:

```cpp
photonTorpedoColor = 0.0 1.0 0.0
photonTorpedoLowColor = 1.0 1.0 0.0
photonTorpedoCriticalColor = 1.0 0.0 0.0
quantumTorpedoColor = 0.0 1.0 0.0
quantumTorpedoLowColor = 1.0 1.0 0.0
quantumTorpedoCriticalColor = 1.0 0.0 0.0
```

Shipyards and `RepairShip` classes provide resupply within 200 units by
default. `torpedoResupply` overrides provider status and
`torpedoResupplyRange` overrides its range. See
[the energy-system guide](../modules/A2FOEnergySystems/README.md) for the full
contract and save-game note.

## Optional directional shields

`A2FODirectionalShields.dll` divides an explicitly opted-in Craft's native
shield total into four facings:

```cpp
maxShields = 650
directionalShields = 1
forwardShieldStrength = 200
aftShieldStrength = 150
portShieldStrength = 150
starboardShieldStrength = 150
```

The enable command is mandatory and all four strengths must be positive. A
Craft without `directionalShields = 1` receives no sidecar state or altered
damage routing, even if strength fields are present. The four values should
sum to an explicitly declared native `maxShields`. A mismatch rejects the
policy. If `maxShields` is absent, as is common in A1 ODFs, the module derives
the native shield ceiling from the four values. `maxHealth` and `healthRate`
remain the independent hull pool and repair rate, while native `shieldRate`
recharges the aggregate and is shared across depleted facings. Select
`A2FOWeaponDamageControls` as well, because it owns the checked
`Craft::Damage` bridge.

`A2FOCraftIdentity` can display the four current/maximum values in the
selected-Craft panel. Use
`infoSingleDirectionalShieldsForwardAftTextArea` and
`infoSingleDirectionalShieldsPortStarboardTextArea` for the two rows, with an
optional `directionalShieldColor`. The arc ring also accepts
`directionalShieldLowColor` and `directionalShieldCriticalColor`; defaults are
green above 50%, orange from 25% through 50%, and red at or below 25%. A fixed
`128 128`
`infoSingleDirectionalShieldsGraphicArea` enables the optional arc-ring origin
when the loaded GUI sprite table defines `dsf`, `dsb`, `dsl`, and `dsr`. The
visible segment rectangles can be overridden per Craft ODF with
`forwardShieldPos`, `aftShieldPos`, `portShieldPos`, and
`starboardShieldPos`, each using `x y width height` coordinates relative to
that graphic area. Their defaults are `26 0 76 20`, `26 108 76 20`,
`0 26 20 76`, and `108 26 20 76`. The first pair depletes horizontally from
its centre and the second vertically; this eased presentation never delays
the underlying shield value.

The same ammunition and directional-shield fields are drawn for selected
stations using Armada's tall build-queue panel. A2FO rebases the native
`infoBuildName`/`infoBuildClass` text context onto the configured
`infoSingleCaptainTextArea`, so the existing `infoSingle*` extension
rectangles remain authoritative and no build-panel duplicates are required.

The active mod's `ART_CFG.h` can select the presentation mode and assign each
logical facing to one of the four physical compass slots:

```cpp
int directionalShieldDisplayMode = 1;
int directionalShieldForwardPosition = 0;
int directionalShieldAftPosition = 2;
int directionalShieldPortPosition = 3;
int directionalShieldStarboardPosition = 1;
```

Display mode `1` is the normal proportional drain. Mode `2` keeps every arc
at full size and communicates its health only through the green, orange, and
red state colours; an exactly depleted arc becomes black. Position values are
`0 = north`, `1 = east`, `2 = south`,
and `3 = west`. Each position must be used exactly once; an out-of-range or
duplicate final mapping is ignored and the existing per-Craft ODF placement
is retained. Higher-precedence extension roots override individual ART
assignments. See
[the directional-shield guide](../modules/A2FODirectionalShields/README.md)
for hit classification, layout, and the current save limitation.

## `a1compat.ini`

The presence of `a1compat.ini` anywhere in the active extension-root chain is
the activation marker for `A1Compat`. Its one optional setting is:

```ini
[A1Compat]
SafeMode = 0
```

`SafeMode` accepts `1/true/yes/on/enabled` and
`0/false/no/off/disabled`, case-insensitively. It defaults to false. Safe mode
keeps the `wingman` alias, missing-only class defaults, and `Addon` overlay,
but disables the riskier executable hooks, diagnostics, and officer-quarter
runtime while diagnosing an A1 conversion.

## `mission_selector.ini`

Place the file in a mod root or its `misc` directory. Higher-precedence roots
override individual values.

Sections use `campaignN` and `campaignN.missionM`. Campaign indices are
`0..127`; mission indices are `0..511` and may be sparse. Campaigns `0..3`
map to Armada's four native campaigns, whose native mission slots are
`0..9`. Campaigns `4..127` are custom.

| Section | Field | Meaning |
| --- | --- | --- |
| campaign | `title` | Displayed campaign name |
| campaign | `overview` | Campaign summary |
| campaign | `background` | PNG, BMP, JPEG, or JPG image shown behind the selector while this campaign is selected |
| campaign | `unlocked` | `0` or `1`; custom campaigns default to unlocked |
| mission | `file` | BZN filename; required for custom missions and optional as a stock-slot launch override |
| mission | `title` | Displayed mission name |
| mission | `description` | Mission narrative/details |
| mission | `objectives` | Objective text; `\n` creates line breaks |
| mission | `thumbnail` | PNG, BMP, JPEG, or JPG preview path |
| mission | `unlocked` | `0` or `1`; custom missions default to unlocked |
| mission | `nativeCampaign` | Advanced borrowed launch campaign, `0..3` |
| mission | `nativeMission` | Advanced borrowed launch slot, `0..9` |

See the [mission-selector guide](../modules/A2FOMissionSelector/README.md) for
native progression, campaign-background fallback, automatic thumbnail lookup,
and custom-launch behavior.

## `RTS_CFG.h` and interface CFG commands

### Cheat amounts

These integer declarations accept literal values from `0` through
`100000000`; every field defaults to `10000`:

- `SHOWMETHEMONEY_DILITHIUM`
- `SHOWMETHEMONEY_TRITANIUM`
- `SHOWMETHEMONEY_METAL`
- `SHOWMETHEMONEY_SUPPLIES`
- `SHOWMETHEMONEY_CREW`

`A2FOCheats` also restores the single-player chat commands `m`, `dis`, `elim`,
and `crash`. Fleet Operations' multiplayer cheat gate remains authoritative.

### Fire-arc switch and colours

`int firearc = 0;` disables custom fire-arc enforcement and preview;
`int firearc = 1;` enables them. The default is enabled.

The active interface CFG may override three RGB float triplets:

- `fireArcBoundaryColor`
- `fireArcCenterColor`
- `fireArcValidTargetColor`

### Upgrade-pod maximum

`int upgradePodMaximumTier = 6;` selects the highest permitted pod tier.
Valid values are 3–16 and the default is 6. Files are read in extension-root
order, so a child mod's valid assignment overrides its parent's assignment.
Fleet Operations does not merge native `RTS_CFG.h` files: a child copy shadows
the complete parent file. Preserve the full parent contents and includes when
adding this command to a child mod. A minimal one-command child file can remove
`ART_CFG.h` and the parent's camera, renderer, map, interface, and gameplay
defaults.

### Craft identity panel

The active GUI configuration accepts:

| Command | Meaning |
| --- | --- |
| `infoSingleCaptainTextArea` | Selected-panel `x y width height` rectangle |
| `infoSingleRegistryTextArea` | Selected-panel `x y width height` rectangle |
| `captainNameColor` | Optional captain RGB float triplet |
| `shipRegistryColor` | Optional registry RGB float triplet |
| `shipNameColor` | Native low-strip colour and, with `A2FOCraftIdentity`, selected ship-name colour |
| `infoTextColor` | Shared selected-panel fallback; remains the ship-class colour when no more specific class colour is supplied |
| `systemIconHealthyColor` | Optional native subsystem and mouse-over hull/shield/crew icon-value colour above 50% |
| `systemIconLowColor` | Optional native subsystem and mouse-over hull/shield/crew icon-value colour above 25% through 50% |
| `systemIconCriticalColor` | Optional native subsystem and mouse-over hull/shield/crew icon-value colour at or below 25% while operational |
| `systemIconDisabledColor` | Optional native subsystem icon/value-text colour for timed/control-disabled systems |
| `systemIconDestroyedColor` | Optional native subsystem icon/value-text colour for destroyed or not-yet-operational repaired systems, and for zero hull/shields/crew |
| `specialEnergyIconColor` | Optional fixed colour for the native selected-panel special-energy icon and adjacent value text; independent of all live-state colours |
| `officerIconColor` | Optional fixed colour for the native selected-panel officer icon and adjacent value text; independent of all live-state colours |
| `infoSingleShieldBarArea` | Existing selected shield-bar rectangle; A2FO adds its hover region |
| `infoSingleExperienceBarArea` | Optional ranked-craft XP-bar `x y width height` rectangle |
| `experienceBarColor` | Optional ranked-craft XP-bar RGB float triplet |
| `experienceBarBackgroundColor` | Optional empty XP-track RGB float triplet |
| `captainName` | Legacy captain-rectangle fallback |
| `shipRegistry` | Legacy registry-rectangle fallback |

Missing captain and registry colours fall back through `infoTextColor` and the
native captain-component colour. `shipNameColor` affects only the native
ship-name rows; it is not a fallback for A2FO's additional text elements.
Each system-icon colour is independent; a missing state keeps Armada's native
colour for that state.

## Race ODF commands and conventions

| Command/convention | Meaning |
| --- | --- |
| `factionTextureSuffix` | Ownership suffix such as `_k`; per-mesh DDS wins over TGA, then the base/Borg texture remains as fallback. |
| `name` | Existing Race identity; a same-named SOD parent node is shown only for that owning faction. |

`factionTextureSuffix` permits up to 32 ASCII letters, digits, underscores, or
hyphens. Race-node matching is case-insensitive. The native `borg` SOD node
and `_b` alternate remain native; `A2FOTextureVariants` additionally repairs
the preflight so `_b.dds` works without a matching TGA.

## Object, ship, and station ODF commands

### Persistent shields and identities

| Command | Meaning |
| --- | --- |
| `alwaysShowShields` | Keep native shield geometry visible while shield strength is above zero; default `0`. |
| `possibleCaptainNames` | Captain rows aligned to native `possibleCraftNames`. |
| `possibleCraftRegistry` | Registry rows aligned to native `possibleCraftNames`. |
| `shieldTooltip` | Optional selected shield-bar short tooltip or localization key. |
| `shieldVerboseTooltip` | Optional selected shield-bar verbose tooltip or localization key. |

The list row selected by Fleet Operations for `possibleCraftNames` selects the
same row in both companion lists.

### Subsystem damage meshes

Use one-based, sparse indices `1..64`:

| Command family | Native subsystem |
| --- | --- |
| `sensorMeshX` / `sensorMeshXexplosion` | Sensors |
| `engineMeshX` / `engineMeshXexplosion` | Engines |
| `weaponMeshX` / `weaponMeshXexplosion` | Weapons |
| `lifeSupportMeshX` / `lifeSupportMeshXexplosion` | Life support |
| `shieldGeneratorMeshX` / `shieldGeneratorMeshXexplosion` | Shield generator |

The mesh value is a SOD node. The optional lowercase `explosion` companion is
an explosion ODF. On subsystem destruction, one configured valid node and its
entire descendant subtree are hidden per craft; the paired explosion is
placed at the node. Repair grows the root mesh back with localized welding
effects and restores descendants at completion.

### Ambient swarms

`X` is a sparse index `0..63`:

| Command | Default | Meaning |
| --- | ---: | --- |
| `swarmX` | required | SOD/database name |
| `swarmXCount` | `1` | Visual instance count, maximum 256 per definition |
| `swarmXScale` | `1.0` | Uniform model scale |
| `swarmXRadius` | `50.0` | Maximum roaming distance beyond the host exclusion surface |
| `swarmXMinRadius` | `0.0` | Minimum roaming distance |
| `swarmXMaxRadius` | radius | Explicit maximum roaming distance |
| `swarmXMinSpeed` | `6.0` | Minimum speed |
| `swarmXMaxSpeed` | `10.0` | Maximum speed |
| `swarmXTurnRate` | `2.0` | Direction blend rate per second |
| `swarmXHardpoint` | host origin | Launch/return hardpoint string list |
| `swarmXHardpointCapacity` | `1` | Concurrent returns per launch point; `0` is unlimited |
| `swarmXInteraction` | none | Interaction hardpoint string list |
| `swarmXInteractionCapacity` | `1` | Concurrent visits per interaction point; `0` is unlimited |
| `swarmXInteractionChance` | `0.35` | Chance to visit after a roaming leg |
| `swarmXInteractionTime` | `3.0` | Dwell seconds |
| `swarmXInteractionRadius` | `2.0` | Random offset around the interaction point |
| `swarmXReturnToHardpoint` | `1` | Permit occasional launch-point returns |
| `swarmXAvoidHost` | `1` | Avoid the host bounding sphere |
| `swarmXHostClearance` | `0.5` | Extra host/swarmer surface gap |
| `swarmXSeparation` | `1.0` | Extra spacing between swarm members |

These are render-only shared-model instances, not selectable or simulated
ships. A host is capped at 1024 members in total.

### Hull-mounted turrets

The parent uses sparse indices `0..64`:

- `turretX` — child turret ODF basename.
- `turretHardpointX` — matching parent SOD hardpoint.

The child ODF uses `classLabel = "turret"` and may set:

| Command | Default | Meaning |
| --- | ---: | --- |
| `turretYawMin` | `-180` | Minimum yaw in degrees |
| `turretYawMax` | `180` | Maximum yaw |
| `turretPitchMin` | `-10` | Minimum pitch |
| `turretPitchMax` | `85` | Maximum pitch |
| `turretYawRate` | `90` | Yaw degrees per second |
| `turretPitchRate` | `60` | Pitch degrees per second |
| `turretRestYaw` | `0` | Idle yaw |
| `turretRestPitch` | `0` | Idle pitch |
| `turretReturnToRest` | `1` | Return to rest without a target |

### Replacement, cocoon, and upgrade pods

| Command | Meaning |
| --- | --- |
| `wreckage` | Replacement ODF created at a destroyed craft's transform by `A2FOWreckage`. |
| `wreckageChance` | Deterministic chance percentage `0..100`; default `100`. |
| `cocoon` | Evolver/HybridBuild cocoon SOD, with optional `.sod`. |
| `upgradeLevel` | Existing pod level; the extension safely supports configured levels through 16. |

### Refit yards

Source Craft may expose up to sixteen destinations with `refitItem0` through
`refitItem15`. A compatible station must use `classLabel = "shipyard"` and
declare `refitHardpoint`; in the current implementation it must name the same
SOD node as the inherited `buildHardpoint`.

The source's root palette receives one Refit navigation button. HybridBuild
selects an unused popup control only after native and special-weapon actions
have been bound, so the opener does not replace teleport or another existing
command. It opens a
destination page containing the configured classes and a native Back control.
Selecting a destination sends the ship to the nearest same-team refit yard,
using normal pathfinding to reach an outside approach on the negative forward
axis of `refitHardpoint`, then admits the destination to that yard's native
Producer FIFO. Destination costs and `buildTime`, cancellation/refunds, queue
presentation, and progress are therefore native. An active cancellation
uses the yard's native completed-build output queue and launch path before the
source can request another refit. Saving during a refit is not supported in
this first implementation.

### A1 object and officer compatibility

`classLabel = "wingman"` aliases to `craft` only when `A1Compat` is activated
by the A1 parent marker. A1Compat supplies the following values only when the
ODF and its includes omit them:

- Wingman: `enginesHitPercent`, `lifeSupportHitPercent`,
  `weaponsHitPercent`, `shieldGeneratorHitPercent`, `sensorsHitPercent`,
  `crewHitPercent`, `hullHitPercent`, `ship`, `has_hitpoints`, `has_crew`,
  `transporter`, `SHOW_MOVEMENT_AUTONOMY`, and `can_explore`.
- Construction rig: `shipclass`, `builder_facility`,
  `SHOW_MOVEMENT_AUTONOMY`, `SHOW_SW_AUTONOMY`, `shipType`, and
  `hotkeyLabel`.
- Freighter: `shipclass`, `maxDilithium`, `alert`, `miner`,
  `SHOW_MOVEMENT_AUTONOMY`, `resourcesCanHandle`, and `hotkeyLabel`.

A1 Race ODFs may omit `normalCrew`, `normalDilithium`, `normalMetal`,
`normalTritanium`, `normalSupply`, and their `lots*` counterparts. A1Compat
uses the active `SHOWMETHEMONEY_*` amounts as missing-only normal defaults and
1.5 times each normal amount for lots. Explicit and inherited fields win. No
defaults are invented for resources without a matching showmethemoney grant.

A1 starbase ODFs use `maximumUpgrades` and `officerGain`. Each corresponding
OfficerUpgrade ODF must declare its stock `race` because A2/FO removed the A1
race-side `officerUpgradeODF` route. Numbered model nodes are named exactly
`oq1`, `oq2`, and so on.

## Producer and upgrade-station ODF commands

### BuildYard module technology

Place `moduleXPseudoTechnology` in the BuildYard configuration ODF named by a
BuildYard class's native `moduleConf` command. `X` is the same zero-based
module index used by `moduleXRequiredTechnology`:

```cpp
module0RequiredTechnology = "fed_module_basic_tech.odf"
module0PseudoTechnology = "fed_module_chassis_gate.odf"
```

The value is a normal project ID. Add the named fake item to the selected
technology-tree file with the prerequisites that should control the module,
for example:

```text
fed_module_chassis_gate.odf 2 fed_research_1.odf fed_research_2.odf
```

The fake item is checked but never constructed. A pseudo-only module uses it
as its technology gate. When both commands are present, both project IDs must
be available. Omitting `moduleXPseudoTechnology` preserves native BuildYard
behavior. All multiplayer peers must use the same configuration ODF and
technology tree.

### Hybrid production

A hybrid station uses `classLabel = "hybridbuild"`. Sparse indices `0..56`
are supported after at least one explicit method begins at item zero:

| Command family | Production method |
| --- | --- |
| `constructItemX` | Constructor placement and construction |
| `yardItemX` | Shipyard production |
| `researchItemX` | Research-station production |
| `evolveItemX` | Evolver transformation |
| `buildItemX` | Compatibility route interpreted from the native class |

The lists share one ten-slot FIFO. The same target may not appear in two
explicit method lists on the same producer.

### Live Producer build submenus

| Command | Meaning |
| --- | --- |
| `buildItemXRefitY` | Make native `buildItemX` a presentation-only parent and add the referenced real build class at child index `Y`. |

`X` and `Y` are zero-based `0..56`. The parent ODF supplies its button/name,
tooltip, and verbose tooltip but is never constructed; cost, time, technology,
and all build behaviour come from each child ODF. The native Back button first
returns to the Producer's main build list. ODFs without Refit children retain
their exact native behaviour. The initial implementation reads these custom
rows from loose ODFs.

### Extended upgrade-station lists

`tierTBuildItemX` replaces one build-list row for upgrade level `T + 2`.
Configured upgrade levels may reach 16, so `T` is `0..14`; usable build-list
indices are `0..56`. Keep one upgrade system on the same `X` across tiers and
make each referenced pod's `upgradeLevel` equal `T + 2`.

## Weapon ODF commands

All commands in this section belong in the general weapon ODF, not the
referenced ordnance ODF.

### Shield and hull damage

| Command | Default | Meaning |
| --- | ---: | --- |
| `canDamageShields` | `1` | Permit damage while shields absorb the hit. If false while hull damage is true, positive shields block the complete hit. |
| `canDamageHull` | `1` | Permit exposed-hull damage and shield-breaking spillover. |
| `shieldDamageModifier` | `1.0` | Multiplier for shield absorption. |
| `hullDamageModifier` | `1.0` | Multiplier for exposed hull and corrected spillover. |

Both modifiers also accept Armada's target-table form:

```odf
shieldDamageModifier = 1.0 "fed_sovereign.odf" 0.5 "bcruise1.odf" 2.0
```

### Three-dimensional fire arcs

| Command | Meaning |
| --- | --- |
| `fireArcMode` | `"box"` or `"cone"` |
| `fireArcYaw` | Horizontal centre in degrees; wraps |
| `fireArcPitch` | Vertical centre; clamps to `-90..90` |
| `fireArcYawAngle` | Box total horizontal width, `0..360` |
| `fireArcPitchAngle` | Box total vertical width, `0..180` |
| `fireArcAngle` | Cone total circular diameter, `0..360` |
| `fireArcCenter` | Compatibility alias for `fireArcYaw` |
| `fireArcWidth` | Compatibility alias for `fireArcYawAngle` |

Widths are totals: `90` means 45 degrees on each side. Supplying
`fireArcAngle` selects cone mode when `fireArcMode` is absent.

### Point-defense firing cycles

For `PointDefenseLaser` and `OrdnanceDefenseWeapon`:

| Command | Meaning |
| --- | --- |
| `shotDelay0` ... `shotDelay63` | Contiguous per-success firing delays |
| `shotDelay` | Existing single delay, used when `shotDelay0` is absent |
| `saveFireCyclePoint` | Zero-based delay index used after the final entry; default `0` |
| `shotCycleResetTime` | Ready-but-idle seconds before returning to `shotDelay0`; zero/absent disables reset |

`shotDelay64` is deliberately rejected as overflow.

### Normal weapon technology

`A2FONormalWeaponTech` adds no ODF command. Put an ordinary cannon, phaser,
pulse, or torpedo weapon ODF in the active `.tt` file with Fleet Operations'
normal prerequisite syntax. Unlisted normal weapons remain available and
special weapons keep their native path.

## Edit-menu ODF commands

The existing edit-menu vocabulary remains:

- `menuNameX` in `editmenu.odf` selects a category.
- `menuTitle` labels a menu level.
- `buildItemX` points to an object-list ODF or another submenu.
- `itemX` names a placeable object in a leaf.
- `forceToNeutral` retains its native placement behavior.

A `buildItemX` target containing at least one `buildItemX` opens recursively.
Each visible level keeps the native 12-entry limit; nesting is capped at 32
levels and cycles are rejected.

## Nebula renderer ODF commands

### Global emissive, bump, and specular suffixes

The active/inherited `ART_CFG.h` can discover maps from every SOD material's
actual diffuse texture without per-ship ODF rows:

```cpp
#define A2FO_EMISSIVE_SUFFIX "_emissive_"
#define A2FO_BUMP_SUFFIX "_bump"
#define A2FO_SPECULAR_SUFFIX "_specular"
#define A2FO_EMISSIVE_BUMP_MULTIPLIER 2.0
#define A2FO_BUMP_LIGHT_BIAS 0.55
#define A2FO_EMISSIVE_DIFFUSE_RESTORE 1.0
```

For diffuse `example`, the emissive rule looks for
`example_emissive_warp`, `example_emissive_impulse`,
`example_emissive_shields`, `example_emissive_life`,
`example_emissive_sensor`, and `example_emissive_weapons`. The bump rule looks
for `example_bump`; the specular rule looks for `example_specular`. Missing
derived files leave that material unchanged. Existing SOD bump slots and
explicit emissive ODF commands take precedence. Empty quoted suffixes disable
their rule. Specular maps are loose, diffuse-UV intensity maps used only on
DOT3/bumped materials: black has no effect and brighter pixels add a stronger
light-dependent gloss.

The managed DXVK backend supports the extension emissive/specular overlay on
DOT3 materials. With System Direct3D 9 / WineD3D, Fleet Operations' bump draw
is left native and unintercepted for driver stability, so native bumps remain
enabled but extension overlays on those bumped materials are unavailable.

`A2FO_EMISSIVE_BUMP_MULTIPLIER` scales the emissive RGB only when the combined
bump/emissive pixel shader is active. It defaults to `1.0` and accepts `0.0`
through `8.0`; values above `1.0` make the glow stronger. Non-bump and
fixed-function emissive paths are unchanged.

`A2FO_BUMP_LIGHT_BIAS` adds ambient light to the DOT3 calculation before it
modulates the diffuse map. It defaults to `0.2` and accepts `0.0` through
`1.0`; higher values brighten bumped hulls independently of emissive strength.

`A2FO_EMISSIVE_DIFFUSE_RESTORE` restores unlit diffuse colour beneath
emissive pixels before adding their emission. It defaults to `0.0`, accepts
`0.0` through `2.0`, and can recover the luminous core of already-saturated
red or blue maps without brightening non-emitting hull areas.

### Subsystem emissive maps

For a single-texture model, use the unnumbered commands:

- `emissiveWarp`
- `emissiveImpulse`
- `emissiveShields`
- `emissiveLifeSupport`
- `emissiveSensors`
- `emissiveWeapons`

For multiple diffuse materials, use sparse indices `0..63`:

- `textureX` identifies the material's diffuse texture.
- `emissiveXWarp`
- `emissiveXImpulse`
- `emissiveXShields`
- `emissiveXLifeSupport`
- `emissiveXSensors`
- `emissiveXWeapons`

Declaring any `textureX` selects indexed mode and ignores inherited
unnumbered maps for that class. Loose DDS, TGA, PNG, and BMP emissive maps are
supported; FPQ-only emissive images are not currently available to D3DX.

### Subsystem and hull damage decals

| Command | Meaning |
| --- | --- |
| `damageThreshold` | Health interval, normally `0.1` for every 10% lost |
| `damageDecalPreview` | `1` shows every configured decal for authoring |

The prefixes are `sensors`, `engines`, `weapons`, `lifeSupport`,
`shieldGenerator`, and `hull`. For one-based entries `1..64`, use:

- `<prefix>ScorchX` — texture name.
- `<prefix>ScorchXHardpoint` — attachment hardpoint.
- `<prefix>ScorchXOffset` — local XYZ offset.
- `<prefix>ScorchXRotation` — XYZ degrees.
- `<prefix>ScorchXSize` — plane width and height.

Legacy definitions using `scorchTextureX` together with one of
`sensorsTargetHardpoints`, `enginesTargetHardpoints`,
`weaponsTargetHardpoints`, `lifeSupportTargetHardpoints`,
`shieldGeneratorTargetHardpoints`, or `hullTargetHardpoints` are converted to
automatic entries. Explicit placement commands take priority.

### Ship-name logo decals

`logoFileNames` is a stock/Fleet Operations list aligned with
`possibleCraftNames`. Each one-based placement `1..64` accepts:

| Command | Required | Meaning |
| --- | --- | --- |
| `logoDecalXHardpoint` | yes | Attachment hardpoint |
| `logoDecalXSuffix` | no | Inserts split-art suffix such as `_upper` into the selected logo filename |
| `logoDecalXOffset` | no | Local XYZ offset |
| `logoDecalXRotation` | no | XYZ degrees |
| `logoDecalXSize` | no | Plane width and height |
| `logoDecalXColourKey` | no | RGB `0..255` triplet converted to transparency for loose files |
| `logoDecalXFlipU` | no | `1` mirrors the sampled texture horizontally |

Unsuffixed entries reuse Fleet Operations' loaded logo texture and support
packed assets. Suffixed entries resolve loose DDS, TGA, PNG, or BMP files.
Native `ScaleSOD` is applied automatically to offsets and sizes.

## SOD and asset conventions without new commands

- `A2FOAnimatedHardpoints`: animate a type-0 null/hardpoint node with an
  ordinary same-name SOD matrix channel. Animated ancestors are inherited.
- `A2FOTextureVariants`: a SOD parent matching the owning Race ODF's `name` is
  selected for that faction; the native `borg` node remains native.
- `A2FORGBTextures`: the presence of loose `Textures/RGB`,
  `Textures/Index8`, or `Textures/Compressed` activates the matching legacy
  lookup bridge. Supported RLE TGA types are 9, 10, and 11.
- `A2FOFeaturePack`: recursive loose ODF folders and `odf.fpq` archives follow
  Data -> ParentMod -> active-mod precedence; A1's registered `Addon` folder
  wins within its own root.
- `A2FOFeaturePack`: Bink intro, GDI movie, and menu/campaign movie output is
  fitted to the active viewport automatically; there is no mod command.

## Controls and authoring tools

- **Ctrl + click** a build button to fill every remaining native queue slot.
- **Ctrl + Alt + click** to enable continuous production. Normal queue
  changes, deletion, clearing, or yard destruction stop it; resource shortage
  pauses and retries it.
- [A2FO Arc Lab](../tools/A2FOArcLab/README.md) loads loose ship ODF/SOD assets,
  resolves parent mods, previews fire arcs and damage/logo decals, supplies
  Top/Bottom/Front/Back/Left/Right/Fit views, and copies or saves generated ODF
  blocks. `--inspect <ship.odf>` performs the same resolution checks headlessly.
- [ODF formatter](odf-formatting.md) groups ODF commands without changing
  parsed values. It defaults to a dry run; `--write` applies atomic validated
  changes, `--report` writes JSON, and `--show-unsafe` reports rejected files.
- The [native module SDK](../sdk/README.md) documents the exported initializer,
  API-version/capability checks, semantic registration interfaces, extension
  roots, producer events, classlabel defaults, and checked patch helpers.

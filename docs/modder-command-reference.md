# Modder command and feature reference

This is the central index of the public modding surface shipped by
A2FOExtensions. It lists every extension-owned ODF, INI, CFG, Lua, asset, and
authoring convention. The linked module guides remain authoritative for
runtime details, validation rules, and current limitations.

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
| [`A2FOCheats`](../modules/A2FOCheats/README.md) | configurable `showmethemoney` resources and restored single-player chat cheats |
| [`A2FOCraftIdentity`](../modules/A2FOCraftIdentity/README.md) | captain/registry lists and selected-object GUI fields |
| [`A2FOEditMenu`](../modules/A2FOEditMenu/README.md) | recursive `buildItemX` edit-menu submenus |
| [`A2FOFeaturePack`](../modules/A2FOFeaturePack/README.md) | recursive ODF/FPQ discovery, queue controls, extended upgrade pods, and viewport-correct Bink movies |
| [`A2FOFireArcs`](../modules/A2FOFireArcs/README.md) | three-dimensional weapon fire volumes and tactical hover preview |
| [`A2FOHybridBuild`](../modules/A2FOHybridBuild/README.md) | `hybridbuild`, four production lists, shared queue, placements, and cocoons |
| [`A2FOInfoIni`](../modules/A2FOInfoIni/README.md) | `SettingsDirectory` and `DefaultGameSpeed` |
| [`A2FOMissionSelector`](../modules/A2FOMissionSelector/README.md) | scrollable stock/custom campaign browser and `mission_selector.ini` |
| [`A2FONebulaRenderer`](../modules/A2FONebulaRenderer/README.md) | DX8 per-pixel lighting, emissive maps, damage decals, and ship-name logo decals |
| [`A2FONormalWeaponTech`](../modules/A2FONormalWeaponTech/README.md) | normal-weapon `.tt` prerequisite enforcement; no new ODF command |
| [`A2FOPointDefenseCycles`](../modules/A2FOPointDefenseCycles/README.md) | CannonImp-style numbered point-defense firing delays |
| [`A2FORGBTextures`](../modules/A2FORGBTextures/README.md) | presence-based legacy RGB/Index8/Compressed TGA loading; no ODF command |
| [`A2FOSwarmSystem`](../modules/A2FOSwarmSystem/README.md) | lightweight render-only ambient swarms |
| [`A2FOTextureVariants`](../modules/A2FOTextureVariants/README.md) | faction textures/nodes, Borg DDS repair, and subsystem damage meshes |
| [`A2FOTurrets`](../modules/A2FOTurrets/README.md) | indexed independently armed hull turrets |
| [`A2FOWeaponDamageControls`](../modules/A2FOWeaponDamageControls/README.md) | independent shield/hull permission and damage multipliers |

The core also supplies deterministic module/script loading, module policy,
Lua hosting, checked semantic dispatch, and the versioned
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
native progression, automatic thumbnail lookup, and custom-launch behavior.

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

### Craft identity panel

The active GUI configuration accepts:

| Command | Meaning |
| --- | --- |
| `infoSingleCaptainTextArea` | Selected-panel `x y width height` rectangle |
| `infoSingleRegistryTextArea` | Selected-panel `x y width height` rectangle |
| `captainNameColor` | Optional captain RGB float triplet |
| `shipRegistryColor` | Optional registry RGB float triplet |
| `captainName` | Legacy captain-rectangle fallback |
| `shipRegistry` | Legacy registry-rectangle fallback |

Missing colours fall back through `infoTextColor`, `shipNameColor`, and the
native selected-name colour.

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
| `wreckage` | Replacement ODF created at a destroyed craft's transform by `scripts/Wreckage.lua`. |
| `wreckageChance` | Deterministic chance percentage `0..100`; default `100`. |
| `cocoon` | Evolver/HybridBuild cocoon SOD, with optional `.sod`. |
| `upgradeLevel` | Existing pod level; the extension safely supports configured levels through 16. |

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

A1 starbase ODFs use `maximumUpgrades` and `officerGain`. Each corresponding
OfficerUpgrade ODF must declare its stock `race` because A2/FO removed the A1
race-side `officerUpgradeODF` route. Numbered model nodes are named exactly
`oq1`, `oq2`, and so on.

## Producer and upgrade-station ODF commands

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

## Lua API

Selected scripts are overlaid by basename from Data through parent mods to the
active mod, then execute once in case-insensitive filename order.

Public values and calls are:

```lua
a2fo.api_version
a2fo.api_revision
a2fo.lua_version
a2fo.extension_roots

a2fo.log(message)
a2fo.require_api(major, revision)
a2fo.has_capability(name)
a2fo.configure_upgrade_pods({ maximum_tier = 6 })
a2fo.on_classlabel(function(classlabel, odf) end)
a2fo.on_evolver_cocoon(function(odf) end)
a2fo.on_object_destroyed({"fieldName"}, function(event) end)

odf:get_string(command)
odf:get_string(command, default)
event:roll_percent(chance)
```

Destroyed-object callbacks return `nil` or a table containing `odf`, optional
`inherit_position`, optional `inherit_rotation`, and optional `owner`
(`"neutral"` or `"original"`). See the complete [Lua API](lua-api.md) for
lifetime, ownership, rollback, and validation rules.

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

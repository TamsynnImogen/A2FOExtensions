# A2FO ODF and GUI/misc integration guide

This is the practical, file-by-file setup guide for the A2FO additions that
need ODF and interface authoring. It covers the ten-resource extension,
build-tooltip glyph, Craft identities, selected shield and XP UI, Photon and
Quantum Torpedo stores, and directional shields.

The [complete modder command reference](modder-command-reference.md) remains
the exhaustive index for every A2FO module, including features which do not
touch the GUI. This guide provides complete copyable integration blocks for
the features above.

## Which file owns each command

| File | A2FO content |
| --- | --- |
| `info.ini` | Select the native modules used by the mod. |
| Race ODF | Starting values and Race-specific names, tooltips, and compact icons for all ten resources. |
| Ship/station ODF | Resource costs, identities, ammunition capacities, resupply, directional shields, shield tooltip text, and per-Craft UI presentation. |
| Weapon ODF | Ammunition cost and shield/hull damage policy. |
| `misc/gui_interface.cfg` | Top resource-panel rectangles, including `resource_6` through `resource_9`. |
| Active selected-panel GUI CFG | Captain/registry, ammunition, directional-shield, native shield-hover, and XP rectangles and colours. Depending on the interface this may be `gui_glob16x12.cfg`, `gui_interface_misc.cfg`, or a Race-specific `gui_*.cfg`. |
| `ART_CFG.h` | Directional-shield display mode and logical-facing-to-compass mapping. |
| `RTS_CFG.h` | Upgrade-pod maximum and optional cheat resource amounts. |
| `Dynamic_Localized_Strings.h` | Directional-shield, shield-bar, XP, ammunition-status, and optional resource/presentation strings. |
| `Sprites/gui_global.spr` | The four directional-shield GUI sprites and any custom ammunition icon sprites. |
| `Sprites/Font*.spr` plus font TGA files | Compact resource and build-time glyphs at bytes `0x80` through `0x8A`. |

ODF lookups are case-insensitive, but use the spelling in this guide. GUI CFG
and sprite names should be treated as exact. Rectangles use
`x y width height`; colours use floating-point `red green blue` values from
`0.0` through `1.0`.

## Module selection

The following is the complete module set for the systems in this guide:

```ini
[modules]
active0 = "A2FOFeaturePack"
active1 = "A2FOResources"
active2 = "A2FOBuildTooltips"
active3 = "A2FOCraftIdentity"
active4 = "A2FOEnergySystems"
active5 = "A2FOWeaponDamageControls"
active6 = "A2FODirectionalShields"
```

Use free `activeX` indices in the mod's existing list rather than replacing
other selected modules. The most-specific child mod owns the complete
`activeX` list; optional selections are not merged from its parent.

Dependencies and optional companions are:

| Feature | Modules |
| --- | --- |
| Four additional resources | `A2FOResources` plus `A2FOFeaturePack` for exact Producer cancellation/refund events |
| Added costs and build time in build-button tooltips | `A2FOBuildTooltips`; select `A2FOResources` too for added-resource costs |
| Photon/Quantum simulation | `A2FOEnergySystems` |
| Photon/Quantum selected-panel UI | `A2FOEnergySystems` plus `A2FOCraftIdentity` |
| Directional-shield gameplay | `A2FODirectionalShields` plus `A2FOWeaponDamageControls` |
| Directional-shield selected-panel UI | Add `A2FOCraftIdentity` |
| Captain/registry, shield hover, and XP bar | `A2FOCraftIdentity` |

## ODF guide

### Four additional resource costs

Put these on any buildable object. Every command is optional, is an
independent pool, and defaults to zero:

```cpp
tritaniumCost = 120
supplyCost = 8
creditsCost = 25
collectiveconnectionsCost = 3
```

Values must be non-negative integers. Credits are not a fallback for another
resource, and none of the four added pools aliases a native pool.

### Race starting resources

Put these in each Race ODF which uses the new resources:

```cpp
normalTritanium = 1000
lotsTritanium = 5000

normalSupply = 100
lotsSupply = 500

normalCredits = 250
lotsCredits = 1000

normalCollectiveConnections = 10
lotsCollectiveConnections = 50
```

The live Instant Action starting-resource mode chooses the `normal` or `lots`
set. A missing field defaults to zero.

### Race-specific presentation for all ten resources

Every native and added resource accepts the same four-field pattern:

```text
<prefix>Res
<prefix>Tooltip
<prefix>VerboseTooltip
<prefix>Icon
```

The ten exact prefixes are:

| Index | Prefix | Default compact glyph |
| ---: | --- | --- |
| 0 | `crew` | `0x88` |
| 1 | `officer` | No default picture; native localized compact text |
| 2 | `dilithium` | `0x84` |
| 3 | `latinum` | `0x81` |
| 4 | `metal` | `0x80` |
| 5 | `biomatter` | `0x82` |
| 6 | `tritanium` | `0x87` |
| 7 | `supply` | `0x86` |
| 8 | `credits` | `0x89` |
| 9 | `collectiveconnections` | `0x85` |

A complete Race override can therefore contain:

```cpp
crewRes = "GUI_A2FO_CREW_RES"
crewTooltip = "GUI_A2FO_CREW_TOOLTIP"
crewVerboseTooltip = "GUI_A2FO_CREW_VTOOLTIP"

officerRes = "GUI_A2FO_OFFICER_RES"
officerTooltip = "GUI_A2FO_OFFICER_TOOLTIP"
officerVerboseTooltip = "GUI_A2FO_OFFICER_VTOOLTIP"

dilithiumRes = "GUI_A2FO_DILITHIUM_RES"
dilithiumTooltip = "GUI_A2FO_DILITHIUM_TOOLTIP"
dilithiumVerboseTooltip = "GUI_A2FO_DILITHIUM_VTOOLTIP"

latinumRes = "GUI_A2FO_LATINUM_RES"
latinumTooltip = "GUI_A2FO_LATINUM_TOOLTIP"
latinumVerboseTooltip = "GUI_A2FO_LATINUM_VTOOLTIP"

metalRes = "GUI_A2FO_METAL_RES"
metalTooltip = "GUI_A2FO_METAL_TOOLTIP"
metalVerboseTooltip = "GUI_A2FO_METAL_VTOOLTIP"

biomatterRes = "GUI_A2FO_BIOMATTER_RES"
biomatterTooltip = "GUI_A2FO_BIOMATTER_TOOLTIP"
biomatterVerboseTooltip = "GUI_A2FO_BIOMATTER_VTOOLTIP"

tritaniumRes = "GUI_A2FO_TRITANIUM_RES"
tritaniumTooltip = "GUI_A2FO_TRITANIUM_TOOLTIP"
tritaniumVerboseTooltip = "GUI_A2FO_TRITANIUM_VTOOLTIP"

supplyRes = "GUI_A2FO_SUPPLY_RES"
supplyTooltip = "GUI_A2FO_SUPPLY_TOOLTIP"
supplyVerboseTooltip = "GUI_A2FO_SUPPLY_VTOOLTIP"

creditsRes = "GUI_A2FO_CREDITS_RES"
creditsTooltip = "GUI_A2FO_CREDITS_TOOLTIP"
creditsVerboseTooltip = "GUI_A2FO_CREDITS_VTOOLTIP"

collectiveconnectionsRes = "GUI_A2FO_CONNECTIONS_RES"
collectiveconnectionsTooltip = "GUI_A2FO_CONNECTIONS_TOOLTIP"
collectiveconnectionsVerboseTooltip = "GUI_A2FO_CONNECTIONS_VTOOLTIP"
```

The example keys must also be defined in `Dynamic_Localized_Strings.h`.
Alternatively, every value may be literal display text. Omit `*Icon` to use
the default glyph table. To customize a compact icon per Race, add the matching
field such as `metalIcon` or `creditsIcon`; its localized value must be the
single raw font byte containing the intended picture. Do not write the visible
characters `0x89` and expect them to be decoded.

`Res` changes verbose cost names, `Tooltip` and `VerboseTooltip` change hover
copy, and `Icon` changes compact costs and the added resource row. The stock
top panel draws only numbers for native resources, so `metalRes` does not add
a native top-panel label.

### Captain, ship name, and registry rows

The three lists are aligned by index:

```cpp
possibleCraftNames = "USS Enterprise" "USS Excelsior"
possibleCaptainNames = "Captain Picard" "Captain Sulu"
possibleCraftRegistry = "NCC-1701-E" "NCC-2000"
```

Fleet Operations' selected `possibleCraftNames` row selects the same row in
both A2FO lists. Missing or out-of-range companion entries display nothing.

### Selected native shield tooltip

These optional Craft ODF fields replace only the text before the live
percentage and values:

```cpp
shieldTooltip = "GUI_SD_SHIELD_TOOLTIP"
shieldVerboseTooltip = "GUI_SD_SHIELD_VTOOLTIP"
```

A2FO appends the live result, for example
`Shield Integrity at 100% 875/875`. The GUI must define
`infoSingleShieldBarArea`; A2FO uses the existing bar and does not redraw it.

### Photon and Quantum Torpedo stores

Put capacities, recharge, and presentation on the Craft ODF:

```cpp
maxPhotonTorpedoes = 80
photonTorpedoRate = 1.0
photonTorpedoRechargeMode = 1

maxQuantumTorpedoes = 20
quantumTorpedoRate = 0.25
quantumTorpedoRechargeMode = 2

photonTorpedoDisplayMode = 1
photonTorpedoValueDisplayMode = 1
photonTorpedoLabel = "GUI_A2FO_PHOTON_LABEL"
photonTorpedoTooltip = "GUI_A2FO_PHOTON_TOOLTIP"
photonTorpedoVerboseTooltip = "GUI_A2FO_PHOTON_VTOOLTIP"

quantumTorpedoDisplayMode = 2
quantumTorpedoValueDisplayMode = 1
quantumTorpedoIcon = "all_interface"
quantumTorpedoIconPos = 71 151 34 34
quantumTorpedoTooltip = "GUI_A2FO_QUANTUM_TOOLTIP"
quantumTorpedoVerboseTooltip = "GUI_A2FO_QUANTUM_VTOOLTIP"
```

Store commands are:

| Command | Meaning |
| --- | --- |
| `maxPhotonTorpedoes`, `maxQuantumTorpedoes` | Capacity. Missing or zero disables that store. New stores begin full. |
| `photonTorpedoRate`, `quantumTorpedoRate` | Fractional ammunition restored per game second. |
| `*RechargeMode = 0` | No recharge. |
| `*RechargeMode = 1` | Automatic recharge. |
| `*RechargeMode = 2` | Recharge only within range of a same-team provider. |
| `*DisplayMode = 1` | Draw a text label. This is the default. |
| `*DisplayMode = 2` | Draw an atlas/sprite icon instead of the label. Both `*Icon` and a valid `*IconPos` are required. |
| `*ValueDisplayMode = 0` | Integer percentage. This is the default. |
| `*ValueDisplayMode = 1` | Integer `current/maximum`. |
| `*ValueDisplayMode = 2` | `Ready`, remaining recharge seconds, or `Resupply`. |
| `*ValueDisplayMode = 3` | Capacity bar instead of value text. The `large_shield_bar` GUI sprite must exist. |

`*Icon` may be a registered GUI sprite name or one of the supported stock
atlas names: `all_interface`, `all_interface2`, `all_interface_races`,
`all_interface_ranks`, or `all_interface_ranks2`. `*IconPos` is the source
crop inside that sprite's texture, not the destination rectangle. The GUI row
defines the destination.

Put exactly one ammunition cost in the general weapon ODF, not its ordnance:

```cpp
photonTorpedoCost = 1
```

or:

```cpp
quantumTorpedoCost = 1
```

The cost is paid once per successfully launched projectile. A four-shot volley
with cost `1` consumes four units. A weapon declaring both costs is invalid and
uses neither store.

Shipyards and `classLabel = "RepairShip"` provide mode-2 resupply within 200
world units by default. Any Craft can override that policy:

```cpp
torpedoResupply = 1
torpedoResupplyRange = 300
```

`torpedoResupply = 0` disables even the automatic yard/RepairShip provider
role.

### Directional shields

Directional shields are strictly opt-in on the Craft ODF:

```cpp
maxShields = 650
directionalShields = 1
forwardShieldStrength = 200
aftShieldStrength = 150
portShieldStrength = 150
starboardShieldStrength = 150

forwardShieldPos = 26 0 76 20
aftShieldPos = 26 108 76 20
portShieldPos = 0 26 20 76
starboardShieldPos = 108 26 20 76
```

Rules:

- `directionalShields = 1` is required. Strength fields alone do nothing.
- All four strengths must be finite and greater than zero.
- When the ODF explicitly declares native `maxShields`, the four values must
  sum to it. A mismatch rejects the directional policy.
- If an A1-style Craft omits `maxShields`, A2FO derives it from the four
  strengths.
- `maxHealth` and `healthRate` remain the separate hull pool and repair rate.
  Native `shieldRate` recharges the directional pool.
- Each `*ShieldPos` is optional and inherited separately. Width and height
  must be positive. The values above are the defaults inside a 128-by-128
  graphic area.

The stock shield bar remains the aggregate of all four facings. Hits are
routed to the facing determined from the attacker's target-local direction;
an empty facing exposes hull even while another facing remains charged.

### Related ODF additions which need no extra GUI code

```cpp
// Keep native shield geometry visible while shields remain above zero.
alwaysShowShields = 1

// Create a replacement object when this Craft is destroyed.
wreckage = "fed_wreckage"
wreckageChance = 75
```

Weapon ODFs can independently control shield and hull damage:

```cpp
canDamageShields = 1
canDamageHull = 1
shieldDamageModifier = 1.0
hullDamageModifier = 1.0
```

The XP bar has no Craft ODF switch: it appears only when Fleet Operations has
attached valid rank-enhancement state and a next-rank XP threshold. The build
time token also has no object ODF switch; it uses Armada's adjusted native
build time when `A2FOBuildTooltips` is selected.

## Resource panel GUI

Put the new resource rectangles in the same loaded GUI CFG that defines
`resource_0` through `resource_5`, normally `misc/gui_interface.cfg`:

```cpp
// A2FOResources second row
resource_6 = 50 30 120 18   // tritanium
resource_7 = 250 30 120 18  // supply
resource_8 = 450 30 120 18  // credits
resource_9 = 650 30 120 18  // collective connections
```

These defaults are used even when the four fields are absent, but explicit
definitions make the layout portable. Keep at least one valid native
`resource_0` through `resource_5` rectangle: A2FO derives the live coordinate
scale and text context from a native row.

Increase the existing `resourcePanelArea` and its background artwork so the
second row is not clipped. For example:

```cpp
resourcePanelArea = 0 8 1053 50
```

Do not replace the interface's `resourcePanel`, `resourcePanelSize`, or
`resourcePanel_X` artwork declarations with values from another GUI; extend
the panel already used by that mod.

## Selected-object GUI

Place this block in the loaded CFG which already defines
`infoSingleNameTextArea`, `infoSingleWireframeIconArea`, and
`infoSingleShieldBarArea`. The coordinates below are a working long-panel
example and must be adjusted to the active interface artwork:

```cpp
// Anchor and optional identity rows
infoSingleCaptainTextArea = 386 130 340 20
infoSingleRegistryTextArea = 386 154 340 20
captainNameColor = 1.0 0.0 1.0
shipRegistryColor = 1.0 0.0 1.0

// Photon and Quantum Torpedo rows
infoSinglePhotonTorpedoesTextArea = 386 186 340 20
infoSingleQuantumTorpedoesTextArea = 386 214 340 20
photonTorpedoColor = 0.0 1.0 0.0
photonTorpedoLowColor = 1.0 1.0 0.0
photonTorpedoCriticalColor = 1.0 0.0 0.0
quantumTorpedoColor = 0.0 1.0 0.0
quantumTorpedoLowColor = 1.0 1.0 0.0
quantumTorpedoCriticalColor = 1.0 0.0 0.0

// Directional-shield text fallback and graphical origin
infoSingleDirectionalShieldsForwardAftTextArea = 386 238 340 18
infoSingleDirectionalShieldsPortStarboardTextArea = 386 258 340 18
infoSingleDirectionalShieldsGraphicArea = 26 56 128 128
directionalShieldColor = 0.1 1.0 0.1
directionalShieldLowColor = 1.0 0.5 0.0
directionalShieldCriticalColor = 1.0 0.05 0.02

// Existing native shield bar hover region and optional XP bar
infoSingleShieldBarArea = 26 126 103 10
infoSingleExperienceBarArea = 10 148 512 8
experienceBarColor = 0.2 0.65 1.0
experienceBarBackgroundColor = 0.25 0.25 0.25
```

`infoSingleCaptainTextArea` is also A2FO's selected-panel coordinate anchor.
Keep it defined even if the Craft does not use `possibleCaptainNames`.
Without it, custom ammunition rectangles, the native shield hover region, and
the XP bar cannot be translated into the selected panel reliably.

The directional graphic area should remain 128 by 128. When all four sprites
load, the ring replaces the two numeric directional-shield rows. When any
sprite is unavailable, the numeric rows remain as the safe fallback. The
first directional text row is also used to draw the arc tooltip, so give it
enough width for verbose text.

Ammunition and directional colours use these thresholds:

- healthy: above 50%;
- low: above 25% through 50%;
- critical: 25% or below;
- a mode-2 directional arc at exactly zero is black.

## `ART_CFG.h`

Add these global declarations to the active or inherited `ART_CFG.h`:

```cpp
// 1 = proportional drain, 2 = full-size colour-only arcs
int directionalShieldDisplayMode = 1;

// 0 = north, 1 = east, 2 = south, 3 = west
int directionalShieldForwardPosition = 0;
int directionalShieldAftPosition = 2;
int directionalShieldPortPosition = 3;
int directionalShieldStarboardPosition = 1;
```

All four positions must be unique and within `0..3`. Invalid or duplicate
mappings retain the per-Craft ODF layout. Child extension roots may override
individual declarations.

The fire-arc preview colours belong beside other colour declarations in the
active interface GUI CFG, not in `ART_CFG.h`:

```cpp
fireArcBoundaryColor = 0.10 0.90 1.00
fireArcCenterColor = 1.00 0.82 0.12
fireArcValidTargetColor = 0.15 1.00 0.20
```

## `RTS_CFG.h`

The upgrade-pod maximum now belongs in the inherited `RTS_CFG.h`:

```cpp
int upgradePodMaximumTier = 6;
```

Valid values are 3 through 16. Fleet Operations shadows a parent's complete
`RTS_CFG.h` when a child supplies one, so copy the full parent file and add the
declaration; do not replace it with a one-line file.

The fire-arc module's global enable belongs in the same file:

```cpp
int firearc = 1;
```

`1` enables A2FO fire-volume enforcement and its icon-hover preview; `0`
returns weapons to their native Fleet Operations arc path.

`A2FOCheats` optionally reads:

```cpp
int SHOWMETHEMONEY_DILITHIUM = 10000;
int SHOWMETHEMONEY_TRITANIUM = 10000;
int SHOWMETHEMONEY_METAL = 10000;
int SHOWMETHEMONEY_SUPPLIES = 10000;
int SHOWMETHEMONEY_CREW = 10000;
```

These cheat names pre-date the independent four-resource extension and follow
Fleet Operations' historical helper slots. They do not configure the new
independent A2FO Tritanium and Supply starting pools; use the Race ODF
`normal*`/`lots*` fields for those pools.

## `Dynamic_Localized_Strings.h`

Add the following hardcoded A2FO UI keys before the file's terminating entry,
or update their values when the loaded table already defines them. Do not
create duplicate keys. Retain the comma style used by the parent file:

```text
"GUI_SD_SHIELD_TOOLTIP", "Shield Integrity at",
"GUI_SD_SHIELD_VTOOLTIP", "Shield Integrity at",

"GUI_SD_EXPERIENCE_TOOLTIP", "Experience",
"GUI_SD_EXPERIENCE_VTOOLTIP", "Experience progress toward the vessel's next rank.",

"GUI_SD_SPE_READY", "Ready",
"GUI_SD_AMMO_WAITING", "Resupply",

"GUI_SD_DIRSHIELD_FORWARD_TOOLTIP", "Forward Shields",
"GUI_SD_DIRSHIELD_FORWARD_VTOOLTIP", "Forward shields protect the vessel's forward arc.",
"GUI_SD_DIRSHIELD_AFT_TOOLTIP", "Aft Shields",
"GUI_SD_DIRSHIELD_AFT_VTOOLTIP", "Aft shields protect the vessel's rear arc.",
"GUI_SD_DIRSHIELD_PORT_TOOLTIP", "Port Shields",
"GUI_SD_DIRSHIELD_PORT_VTOOLTIP", "Port shields protect the vessel's left arc.",
"GUI_SD_DIRSHIELD_STARBOARD_TOOLTIP", "Starboard Shields",
"GUI_SD_DIRSHIELD_STARBOARD_VTOOLTIP", "Starboard shields protect the vessel's right arc.",
"GUI_SD_DIRSHIELD_STRENGTH", "Current strength",
```

`GUI_SD_SPE_READY` is already present in many Fleet Operations string tables;
do not add a duplicate key when the inherited file supplies it.

`A2FOBuildTooltips` also reuses the stock `GUI_CP_AMOUNT_SEPARATE` and
`GUI_CP_END_EXTRA` keys. They normally already exist; missing keys safely fall
back to `/` and `)` respectively. Verbose `second`/`seconds` text is generated
directly and needs no new localization key.

The example ODF presentation keys used earlier can be defined as follows:

```text
"GUI_A2FO_PHOTON_LABEL", "Photon Torpedoes",
"GUI_A2FO_PHOTON_TOOLTIP", "Photon Torpedo Ammunition",
"GUI_A2FO_PHOTON_VTOOLTIP", "Photon torpedoes recharge continuously.",
"GUI_A2FO_QUANTUM_TOOLTIP", "Quantum Torpedo Ammunition",
"GUI_A2FO_QUANTUM_VTOOLTIP", "Quantum torpedoes require resupply.",

"GUI_A2FO_CREW_RES", "Crew",
"GUI_A2FO_CREW_TOOLTIP", "Crew",
"GUI_A2FO_CREW_VTOOLTIP", "Available crew not currently assigned to active service.",
"GUI_A2FO_OFFICER_RES", "Officers",
"GUI_A2FO_OFFICER_TOOLTIP", "Officers",
"GUI_A2FO_OFFICER_VTOOLTIP", "Officers available to command ships and stations.",
"GUI_A2FO_DILITHIUM_RES", "Dilithium",
"GUI_A2FO_DILITHIUM_TOOLTIP", "Dilithium",
"GUI_A2FO_DILITHIUM_VTOOLTIP", "Current dilithium reserves.",
"GUI_A2FO_LATINUM_RES", "Latinum",
"GUI_A2FO_LATINUM_TOOLTIP", "Latinum",
"GUI_A2FO_LATINUM_VTOOLTIP", "Current latinum reserves.",
"GUI_A2FO_METAL_RES", "Metal",
"GUI_A2FO_METAL_TOOLTIP", "Metal",
"GUI_A2FO_METAL_VTOOLTIP", "Current metal reserves.",
"GUI_A2FO_BIOMATTER_RES", "Biomatter",
"GUI_A2FO_BIOMATTER_TOOLTIP", "Biomatter",
"GUI_A2FO_BIOMATTER_VTOOLTIP", "Current biomatter reserves.",
"GUI_A2FO_TRITANIUM_RES", "Tritanium",
"GUI_A2FO_TRITANIUM_TOOLTIP", "Tritanium",
"GUI_A2FO_TRITANIUM_VTOOLTIP", "Current tritanium reserves.",
"GUI_A2FO_SUPPLY_RES", "Supply",
"GUI_A2FO_SUPPLY_TOOLTIP", "Supply",
"GUI_A2FO_SUPPLY_VTOOLTIP", "Current supply reserves.",
"GUI_A2FO_CREDITS_RES", "Credits",
"GUI_A2FO_CREDITS_TOOLTIP", "Credits",
"GUI_A2FO_CREDITS_VTOOLTIP", "Current credit reserves.",
"GUI_A2FO_CONNECTIONS_RES", "Collective Connections",
"GUI_A2FO_CONNECTIONS_TOOLTIP", "Collective Connections",
"GUI_A2FO_CONNECTIONS_VTOOLTIP", "Current collective connection reserves.",
```

These `GUI_A2FO_*` names are examples rather than hardcoded engine keys. A
Race may reference different keys or literal strings.

## Directional-shield GUI sprites

Add these four entries directly to the loaded `Sprites/gui_global.spr`, or put
them in a GUI-only table included from `gui_global.spr` before ordinary sprite
declarations:

```text
@reference=128
@tmaterial=interface

dsf shield_forward 0 0 128 128
dsb shield_back 0 0 128 128
dsl shield_left 0 0 128 128
dsr shield_right 0 0 128 128
```

The corresponding textures are:

```text
shield_forward.tga
shield_back.tga
shield_left.tga
shield_right.tga
```

Use alpha-bearing DDS or safe uncompressed TGA assets visible during GUI
startup. A startup-loaded `gui_global.spr` cannot depend on the later
`A2FORGBTextures` initialization to discover an otherwise invisible legacy
texture. Do not add `@sprite_node` declarations: these are GUI database
sprites, not world sprites.

Preserve the legacy sprite table's CRLF line endings. A late `@include` after
ordinary sprite declarations may be ignored, and LF-only inserted lines can be
merged with adjacent directives by Armada's parser. Keep custom sprite names
at 27 characters or fewer.

Custom Photon/Quantum icon sprites may be registered in the same GUI table.
The Craft's `*Icon` field names the first `.spr` column and `*IconPos` selects
the crop within that sprite's texture.

## Resource and build-time font glyphs

A2FO emits this exact contiguous byte block for compact costs and the added
resource row:

| Byte | Required picture |
| --- | --- |
| `0x80` | metal |
| `0x81` | latinum |
| `0x82` | biomatter |
| `0x83` | energy, reserved and not a resource |
| `0x84` | dilithium |
| `0x85` | collective connections |
| `0x86` | supply |
| `0x87` | tritanium |
| `0x88` | crew |
| `0x89` | credits |
| `0x8A` | build time |

There is currently no default officer picture. An explicit Race
`officerIcon` can point to another valid single-glyph localization value.

Editing only `all_interface_font40.tga` affects only GUI text which uses that
font atlas. Populate the same `0x80..0x8A` frames in every atlas which can draw
the resource panel or build tooltip in the target interface, commonly:

```text
all_interface_font12.tga
all_interface_font16.tga
all_interface_font20.tga
all_interface_font28.tga
all_interface_font32.tga
all_interface_font40.tga
```

The matching `Sprites/Font*.spr` UV animation and width animation must also
contain a frame for every byte. Fleet Operations tables commonly already have
high-byte frames; in that case replace the art at the existing frame
coordinates rather than changing character order.

For a custom 1024-by-256 font atlas, the working development layout used these
additional UV keyframes after the `0x7F` frame:

```text
256 0  # 0x80 metal
296 0  # 0x81 latinum
336 0  # 0x82 biomatter
376 0  # 0x83 energy
416 0  # 0x84 dilithium
456 0  # 0x85 collective connections
496 0  # 0x86 supply
536 0  # 0x87 tritanium
576 0  # 0x88 crew
616 0  # 0x89 credits
656 0  # 0x8A build time
```

Its matching medium-font width keyframes were:

```text
33  # 0x80 metal
30  # 0x81 latinum
32  # 0x82 biomatter
32  # 0x83 energy
32  # 0x84 dilithium
27  # 0x85 collective connections
33  # 0x86 supply
30  # 0x87 tritanium
26  # 0x88 crew
37  # 0x89 credits
30  # 0x8A build time
```

Those coordinates and widths are an example for that exact atlas, not a
universal requirement. Scale or re-author them for a different texture. Extend
the existing font animations and their declared frame counts; do not create a
second animation with the same name.

## Visibility checklist

If a feature does not appear, check these conditions before changing hooks:

- Added resources: `A2FOResources` and `A2FOFeaturePack` selected, Race
  `normal*`/`lots*` values non-zero, `resource_6..9` inside an unclipped panel,
  and the font contains `0x85..0x89`.
- Added build costs: object costs non-zero, `A2FOBuildTooltips` selected, and
  the compact font contains the resource glyphs plus `0x8A`.
- Ammunition values: store maximum greater than zero, with
  `A2FOEnergySystems` and `A2FOCraftIdentity` selected. Automatic rows use the
  selected-name anchor; custom row rectangles additionally require
  `infoSingleCaptainTextArea`.
- Ammunition icon: display mode `2`, both `*Icon` and `*IconPos` present, crop
  within the real texture dimensions, and the GUI sprite loaded at startup.
- Ammunition bar: value display mode `3` and a valid `large_shield_bar` sprite.
- Directional gameplay: all four positive strengths, correct `maxShields`
  total, and both `A2FODirectionalShields` and
  `A2FOWeaponDamageControls` selected.
- Directional graphic: `A2FOCraftIdentity` selected, 128-by-128 graphic area,
  all four `ds*` GUI sprites, and all four startup-visible textures. Missing
  art deliberately falls back to numeric rows.
- Shield tooltip: `infoSingleCaptainTextArea` and
  `infoSingleShieldBarArea` both present.
- XP bar: both selected-panel rectangles present and the selected Craft has a
  valid Fleet Operations next-rank threshold. Unranked and maximum-rank Craft
  intentionally show no bar.

## Current persistence and economy boundaries

- The four added resources are not yet written to save games. A loaded game
  initializes them from the Race's selected normal/lots values.
- Added resources are integrated with production affordability, payment, and
  refunds, but not yet with mining, freighter cargo, trade routes,
  `ResourceWeapon`, scripted grants, gifting, or AI economy planning.
- Photon and Quantum current values are appended to new Craft save data. Start
  a new game after first enabling the module for a mod.
- Directional-facing distribution is not yet separately saved. A loaded
  aggregate shield percentage is redistributed proportionally across facings.

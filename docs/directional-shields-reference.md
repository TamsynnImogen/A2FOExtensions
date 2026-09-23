# A2FO Extensions: directional shields reference

Checked against the local source on 23 September 2026.

Directional shields provide four independent facings: forward, aft, port and
starboard. This reference collects the gameplay, interface, editor and build
commands in one place.

Related documentation:

- [Directional-shield gameplay and basic UI](../modules/A2FODirectionalShields/README.md)
- [CraftIdentity graphics and tooltips](../modules/A2FOCraftIdentity/README.md)
- [Current medium/tall panel customisation](selected-panel-customisation.md)
- [Object-editor controls and save/load](native-object-editor-properties.md)

Two older statements in the existing documentation are outdated: the current
renderer can scale the graphic area, and the updated CraftIdentity module
implements per-facing persistence.

## Module selection

Enable the modules in your mod's `info.ini`:

```ini
[modules]
active0 = "A2FOWeaponDamageControls"
active1 = "A2FODirectionalShields"
active2 = "A2FOCraftIdentity"
```

Use free `activeX` numbers in your existing list.

| Module | Purpose |
| --- | --- |
| `A2FOWeaponDamageControls` | Required: owns the shared damage hook. |
| `A2FODirectionalShields` | Required: facing capacities, damage routing and recharge distribution. |
| `A2FOCraftIdentity` | Provides selected-panel graphics, values, tooltips, editor controls and extended persistence. |

The two gameplay modules support either load order. Without the damage bridge,
directional gameplay remains inactive. The child mod's module list is complete;
selections are not merged from its parent.

## Craft ODF gameplay commands

Put these commands in the ship or station's Craft ODF:

```cpp
maxShields = 650

directionalShields = 1
forwardShieldStrength = 200
aftShieldStrength = 150
portShieldStrength = 150
starboardShieldStrength = 150
```

| Command | Meaning |
| --- | --- |
| `directionalShields` | Explicit activation gate; omitted or disabled means native shields. |
| `forwardShieldStrength` | Maximum forward capacity. |
| `aftShieldStrength` | Maximum aft capacity. |
| `portShieldStrength` | Maximum port capacity. |
| `starboardShieldStrength` | Maximum starboard capacity. |
| `maxShields` | Native total shield capacity, checked against the facing total. |

Rules:

- All four strengths must be finite and greater than zero.
- Strength commands alone do not activate the feature.
- An explicit or inherited `maxShields` must be positive and match the four
  strengths' sum. The comparison tolerance is the greater of `0.01` or `0.01%`
  of the larger total.
- If `maxShields` is entirely absent, the four strengths establish the native
  shield maximum. This supports A1-style ODFs.
- Invalid configurations are rejected and retain native shield behaviour.
- Field names are case-insensitive and use normal ODF inheritance.
- Numeric strengths accept decimals, scientific notation and an optional `f`
  suffix.
- The activation parser also accepts positive numbers or `true`, `yes`, `on`.
  Disable with `0`, `false`, `no`, `off`. `enabled` is not accepted.

Native `maxHealth` and `healthRate` remain the hull capacity and repair rate.
Native `shieldRate` supplies shield recharge; there are no separate per-facing
recharge commands.

## Combat and recharge behaviour

The attacker's position in the target's local horizontal axes selects the
facing. Exact diagonal boundaries belong to forward or aft. An empty facing
exposes the hull even while another facing remains charged.

Recharge is shared evenly between depleted facings, redistributing unused
recharge when a facing fills. Ownerless or otherwise unresolved damage follows
native aggregate handling; aggregate losses are reconciled proportionally
across the facings. The ordinary shield bar shows their combined total.

Normal shield-hit effects follow the struck facing's percentage. Effects on an
empty facing are suppressed or stopped. The separate `A2FOAlwaysShowShields`
feature remains independent.

## Craft ODF presentation commands

These optional commands are handled by `A2FOCraftIdentity`:

```cpp
// Segment rectangles within the logical 128-by-128 graphic.
// Format: x y width height

forwardShieldPos = 26 0 76 20
aftShieldPos = 26 108 76 20
portShieldPos = 0 26 20 76
starboardShieldPos = 108 26 20 76

directionalShieldValueDisplayMode = 2
```

The rectangle values above are the defaults. Each command inherits
independently; width and height must be positive.

| `directionalShieldValueDisplayMode` | Result |
| --- | --- |
| Omitted | Legacy behaviour: ring when available, otherwise two diagnostic rows. |
| `0` | Hides the four individual value labels. |
| `1` | Rounded percentage numbers without `%`, such as `75`. |
| `2` | Current/maximum values, such as `150/200`. |

Modes `1` and `2` allow values alongside the ring.

## Global appearance and orientation: ART_CFG.h

```cpp
int directionalShieldDisplayMode = 1;

int directionalShieldForwardPosition = 0;
int directionalShieldAftPosition = 2;
int directionalShieldPortPosition = 3;
int directionalShieldStarboardPosition = 1;
```

| Setting | Meaning |
| --- | --- |
| Display mode `1` | Arc fill drains proportionally, leaving a dim empty track. |
| Display mode `2` | Full-size arcs change colour; an empty arc becomes black. |
| Position `0` | North/top |
| Position `1` | East/right |
| Position `2` | South/bottom |
| Position `3` | West/left |

The four positions must be unique and within `0..3`. Invalid mappings retain
the ODF layout. The facing's value and tooltip move with its display position;
this does not change combat direction.

These settings load when CraftIdentity initializes and inherit through the
extension-root chain.

## Selected-panel GUI CFG

Place these commands in the active selected-panel GUI CFG. Depending on the
mod, this may be `gui_glob16x12.cfg`, `gui_interface_misc.cfg` or a race-specific
GUI file.

```cpp
// Numeric fallback rows.
infoSingleDirectionalShieldsForwardAftTextArea = 386 238 340 18
infoSingleDirectionalShieldsPortStarboardTextArea = 386 258 340 18

// Whole ring.
infoSingleDirectionalShieldsGraphicArea = 26 56 128 128

// Independent value-label rectangles.
infoSingleDirectionalShieldsForwardValueTextArea = 58 78 64 18
infoSingleDirectionalShieldsAftValueTextArea = 58 144 64 18
infoSingleDirectionalShieldsPortValueTextArea = 46 111 44 18
infoSingleDirectionalShieldsStarboardValueTextArea = 90 111 44 18

// Ring colours.
directionalShieldColor = 0.1 1.0 0.1
directionalShieldLowColor = 1.0 0.5 0.0
directionalShieldCriticalColor = 1.0 0.05 0.02

// Individual value-label colours.
directionalShieldValueColor = 0.8 1.0 0.8
directionalShieldValueLowColor = 1.0 0.7 0.1
directionalShieldValueCriticalColor = 1.0 0.15 0.05
```

Rectangles are `x y width height`; colours are RGB components from `0` to `1`.

| Remaining capacity | Colour |
| --- | --- |
| Above 50% | Normal |
| Above 25%, up to 50% | Low |
| 25% or below | Critical |
| Exactly zero, arc display mode `2` | Black |

Missing positions use automatic fallbacks. The current renderer scales the
four segments with the graphic rectangle, so `128 128` is the baseline rather
than a mandatory size.

## Independent medium and tall layouts

Use the general panel command pattern:

```text
infoSingle<Element><Property>
infoBuild<Element><Property>
```

`infoSingle` controls the medium selected-object panel; `infoBuild` controls the
tall producer panel. Each omitted tall property inherits its medium equivalent
independently.

All directional-shield element names are:

```text
DirectionalShieldsGraphic
DirectionalShieldsForwardAftText
DirectionalShieldsPortStarboardText

DirectionalShieldsForward
DirectionalShieldsAft
DirectionalShieldsPort
DirectionalShieldsStarboard

DirectionalShieldsForwardValueText
DirectionalShieldsAftValueText
DirectionalShieldsPortValueText
DirectionalShieldsStarboardValueText
```

The available property suffixes are:

| Property | Use |
| --- | --- |
| `Area` | Position and dimensions. Zero width or height hides the element. |
| `Color` | Normal RGB tint. |
| `LowColor` | Low-capacity RGB tint. |
| `CriticalColor` | Critical-capacity RGB tint. |
| `Sprite` | Individual facing's horizontal bar fill sprite. |
| `BackgroundSprite` | Horizontal bar's empty-track sprite. |
| `BackgroundColor` | Horizontal bar's empty-track tint. |

The sprite/background properties apply to individual facing bars, not text
elements. Without a facing `Sprite`, that facing retains its curved arc.

For example, display forward shields as a separately positioned bar on the tall
panel:

```cpp
infoBuildDirectionalShieldsForwardArea = 20 180 160 12
infoBuildDirectionalShieldsForwardSprite = "large_shield_bar"
infoBuildDirectionalShieldsForwardBackgroundSprite = "large_shield_bar"

infoBuildDirectionalShieldsForwardColor = 0.2 0.8 1.0
infoBuildDirectionalShieldsForwardLowColor = 1.0 0.7 0.0
infoBuildDirectionalShieldsForwardCriticalColor = 1.0 0.1 0.1
infoBuildDirectionalShieldsForwardBackgroundColor = 0.2 0.2 0.2

infoBuildDirectionalShieldsForwardValueTextArea = 190 180 80 20
infoBuildDirectionalShieldsForwardValueTextColor = 1.0 1.0 1.0
```

Replace `Forward` with `Aft`, `Port` or `Starboard` for the other facings. Keep
`directionalShieldValueDisplayMode = 1` or `2` in the ODF to show their value
labels.

Individual facing areas are panel-relative. Bar fill crops from the right as
capacity drops, and zero capacity leaves the track visible. Custom sprite names
must exist in the active interface sprite tables; a missing named bar sprite
falls back to `large_shield_bar` with a diagnostic.

Coordinates must fit the existing panel and avoid its native controls—the
extension does not enlarge the panel.

## Curved-ring sprites and textures

Register these four sprites in the loaded `gui_global.spr`, or a GUI-only table
included from its initial include section:

```text
@reference=128
@tmaterial=interface

dsf shield_forward 0 0 128 128
dsb shield_back 0 0 128 128
dsl shield_left 0 0 128 128
dsr shield_right 0 0 128 128
```

| Sprite | Texture |
| --- | --- |
| `dsf` | `shield_forward.tga` |
| `dsb` | `shield_back.tga` |
| `dsl` | `shield_left.tga` |
| `dsr` | `shield_right.tga` |

Use alpha-bearing textures. These are interface sprites; do not add
`@sprite_node` declarations.

Preserve **CRLF line endings** in legacy `.spr` files. Late includes can be
ignored, and LF-only additions can corrupt parsing.

If registered in the startup `Data/Sprites/gui_global.spr`, the textures must
already be startup-visible in `Data/Textures`, using supported DDS or safe
uncompressed TGA files. That interface database is built before
`A2FORGBTextures` initializes.

The normal ring retains a centred horizontal fill for forward/aft and a centred
vertical fill for port/starboard. Fill transitions ease over roughly 450 ms;
that animation does not delay gameplay damage. Missing ring sprites fall back
to numeric diagnostics.

## Localized tooltips

Add optional tooltip text to `Dynamic_Localized_Strings.h`:

```text
"GUI_SD_DIRSHIELD_FORWARD_TOOLTIP", "Forward Shields",
"GUI_SD_DIRSHIELD_FORWARD_VTOOLTIP", "Forward shields protect the vessel's forward arc.",

"GUI_SD_DIRSHIELD_AFT_TOOLTIP", "Aft Shields",
"GUI_SD_DIRSHIELD_AFT_VTOOLTIP", "Aft shields protect the vessel's rear arc.",

"GUI_SD_DIRSHIELD_PORT_TOOLTIP", "Port Shields",
"GUI_SD_DIRSHIELD_PORT_VTOOLTIP", "Port shields protect the vessel's left arc.",

"GUI_SD_DIRSHIELD_STARBOARD_TOOLTIP", "Starboard Shields",
"GUI_SD_DIRSHIELD_STARBOARD_VTOOLTIP", "Starboard shields protect the vessel's right arc.",

"GUI_SD_DIRSHIELD_STRENGTH", "Current strength"
```

The module appends live current/maximum values. Missing localization entries
use English fallbacks.

## Object-editor overrides and persistence

With the updated modules, Fleet Ops' existing Craft property grid exposes
current and maximum values for all four facings, in Basic and Advanced views.

- The class must already enable directional shields.
- Every maximum must be positive; current must be between zero and that maximum.
- Changes affect that individual ship.
- Use the facing rows when editing shields: their sums determine the native
  aggregate.
- OK applies the values; Cancel discards them.

The current CraftIdentity code saves all four current/maximum pairs in a
versioned `A2FOEDIT` record and refreshes those values at save time.
DirectionalShields restores them after loading.

**Saves containing that record require the updated DLLs and are incompatible
with older or unextended loaders.** Old maps without the record remain
supported. Without a valid restored per-facing record, loading distributes the
saved aggregate percentage proportionally.

This supersedes the older directional-shield README's blanket "facings are not
saved" statement. The editor documentation records host checks, but still
marks in-game persistence validation as pending.

## Build and host-check commands

Build the three modules from source:

```bash
cd /home/tamsynn/A2FOHookExtensions

make -j"$(nproc)" \
  build/modules/A2FOWeaponDamageControls.dll \
  build/modules/A2FODirectionalShields.dll \
  build/modules/A2FOCraftIdentity.dll
```

The build requires the **32-bit Windows MinGW toolchain**. On Nobara/Fedora:

```bash
sudo dnf install mingw32-gcc-c++ mingw32-binutils make
```

Focused host checks can be built and run with:

```bash
make build/directional_shields_test build/craft_identity_test

./build/directional_shields_test
./build/craft_identity_test
```

Install the resulting DLLs into the existing extension installation's `modules`
directory. Shared diagnostics go to `A2FOExtensions.log`.

This reference was checked against source and documentation. The modules were
not rebuilt and the game was not run as part of preparing it.

## Current limitations

There are currently no player commands for reinforcing, balancing or
transferring shield power, no clickable arc controls, and no dorsal/ventral
facings. Those remain future possibilities.

Multiplayer determinism and the broader weapon/repair compatibility checklist
remain marked for manual validation in the
[feature status document](../directionalshields-TODO.md).

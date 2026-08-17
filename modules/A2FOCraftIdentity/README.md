# Craft captain and registry identities

`A2FOCraftIdentity.dll` adds two optional companion lists to every
Craft-derived ODF. They are row-aligned with Armada's existing
`possibleCraftNames` list:

```text
possibleCraftNames = "USS Enterprise" "USS Excelsior"
possibleCaptainNames = "Captain Picard" "Captain Sulu"
possibleCraftRegistry = "NCC-1701-E" "NCC-2000"
```

If Fleet Operations assigns ship-name row `N`, the module displays captain
row `N` and registry row `N`. It reads Fleet Operations' native craft-name
index rather than making another random choice, so all three values remain
together across save/load and native ship renaming. It does not consume the
synchronized random-number stream or add anything to the save format.

Keep the three lists in the same order and preferably the same length. A
missing or out-of-range companion row is left blank; indices never wrap.
An intentionally empty quoted entry may be used to leave one field blank
without shifting later rows.

## Selected-object GUI configuration

The identities are drawn only in the panel for the single selected object,
not the low `SDInfoBar` mouse-over strip. Armada already reads
`infoSingleCaptainTextArea`; the module adds the matching registry rectangle:

```text
infoSingleCaptainTextArea = 446 244 340 20
infoSingleRegistryTextArea = 446 264 340 20

captainNameColor = 1 0 1
shipRegistryColor = 1 0 1
```

The four rectangle values use Fleet Operations' existing `x y width height`
format and are relative to `infoPanelArea`, just like
`infoSingleNameTextArea`. Choose positions inside the selected information
panel which do not overlap other controls. A missing rectangle hides that
field without disabling identity assignment.

For compatibility, the older `captainName` and `shipRegistry` rectangles are
accepted as fallbacks when their `infoSingle*TextArea` equivalents are absent,
but they are still rendered in the selected panel. New GUI files should use
the `infoSingle*TextArea` names above.

`captainNameColor` and `shipRegistryColor` are optional. A missing field uses
`infoTextColor`, then `shipNameColor`, then the selected ship-name component's
native colour.

The same panel can add a ranked-craft XP bar and a native hover region over
Fleet Operations' existing shield-strength bar:

```text
infoSingleShieldBarArea = 26 126 103 10
infoSingleExperienceBarArea = 10 148 512 8
experienceBarColor = 0.2 0.65 1.0
experienceBarBackgroundColor = 0.25 0.25 0.25
```

`infoSingleShieldBarArea` remains Fleet Operations' normal selected shield-bar
rectangle; A2FO does not redraw or alter that bar. It uses the rectangle only
to add the missing shield tooltip. By default it displays the live and maximum
shield pools as whole numbers after their percentage, matching the native hull
format—for example `Shield Integrity at 100% 875/875`. A Craft ODF may
customize the text before those values:

```cpp
shieldTooltip = "Shield Strength"
shieldVerboseTooltip = "Current shield strength and shield-system condition."
```

Both values may be literal text or keys in `Dynamic_Localized_Strings.h`.
When absent, A2FO resolves `GUI_SD_SHIELD_TOOLTIP` and
`GUI_SD_SHIELD_VTOOLTIP`, with safe English fallbacks.

`infoSingleExperienceBarArea` is optional. When the selected Craft has Fleet
Operations rank-enhancement state and a valid next-rank threshold, A2FO draws
`current XP / next-rank XP` as a left-to-right bar using
`experienceBarColor`. The complete empty track remains visible using
`experienceBarBackgroundColor`; when absent, it uses a near-black fallback.
Hovering it shows the same live values. The tooltip keys
are `GUI_SD_EXPERIENCE_TOOLTIP` and `GUI_SD_EXPERIENCE_VTOOLTIP`; missing keys
fall back to `Experience` and an English explanation. Unranked and maximum-rank
Craft do not show the XP bar.

The selected-panel observer also presents A2FOEnergySystems ammunition. Its
automatic Photon and Quantum rows use offsets `+16` and `+40` from the live
name anchor; GUI files may override them with
`infoSinglePhotonTorpedoesTextArea` and
`infoSingleQuantumTorpedoesTextArea`. Craft ODFs may customize either row:

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

Mode `1` displays the per-Craft label followed by integer
`current/maximum`. Mode `2` replaces the label with the selected texture-atlas
crop and places the value after it. `*Icon` accepts a registered interfaceDB
sprite or a stock atlas name such as `all_interface`; `*IconPos` is its
`x y width height` source rectangle. The icon is positioned automatically at
the store's text row. Missing sprites or invalid rectangles retain the compact
value without restoring the label.
Labels and tooltip strings resolve through `Dynamic_Localized_Strings.h` when
they name an existing key and otherwise display literally. Both rows use the
same proven native normal/verbose tooltip path as directional-shield arcs.

`*ValueDisplayMode` is independent of the label/icon presentation mode. It
defaults to `0`: `0` shows integer percent, `1` shows integer
`current/maximum`, and `2` shows localized `GUI_SD_SPE_READY` at full capacity,
rounded-up remaining recharge seconds, or `GUI_SD_AMMO_WAITING` while a
resupply-only store is out of range. Missing `GUI_SD_AMMO_WAITING` falls back
to `Resupply`. Mode `3` replaces the value text with a left-to-right capacity
bar while retaining the selected label or icon presentation.

Text and icons share capacity colours: green above 50%, yellow from 25% through
50%, and red at or below 25%. `photonTorpedoColor`,
`photonTorpedoLowColor`, `photonTorpedoCriticalColor` and their `quantum*`
counterparts may override the defaults in the GUI configuration.

The same selected-panel draw path can show directional-shield diagnostics
when `A2FODirectionalShields.dll` is active on the selected Craft:

```text
infoSingleDirectionalShieldsForwardAftTextArea = 386 238 340 18
infoSingleDirectionalShieldsPortStarboardTextArea = 386 258 340 18
infoSingleDirectionalShieldsGraphicArea = 26 56 128 128
directionalShieldColor = 0.1 1.0 0.1
directionalShieldLowColor = 1.0 0.5 0.0
directionalShieldCriticalColor = 1.0 0.05 0.02
```

The first row displays forward/aft current and maximum strength; the second
displays port/starboard. Missing rectangles use automatic positions relative
to the live captain rectangle. The three colours are optional. Arc facings
above 50% use the healthy colour, facings from 25% through 50% use the low
colour, and facings at or below 25% use the critical colour. Their defaults are
green, orange, and red respectively. The numeric fallback continues to use
`directionalShieldColor`, then the selected panel's shared text colour.

The optional 128-by-128 graphic area supplies the origin for a four-arc ring.
Its width and height should remain `128` because the current renderer uses the
textures' native logical size. Each Craft ODF may position and resize the
visible segments inside that area using `x y width height` rectangles:

```cpp
forwardShieldPos = 26 0 76 20
aftShieldPos = 26 108 76 20
portShieldPos = 0 26 20 76
starboardShieldPos = 108 26 20 76
```

Those values are also the defaults, so existing Craft remain unchanged.
Commands may be supplied independently and inherit through the ODF chain;
missing facings retain their default rectangle. Width and height must be
positive. The renderer scales only the selected Craft's submitted segment and
restores the shared GUI sprite immediately after each draw.

Global display behavior belongs in the active `ART_CFG.h`:

```cpp
int directionalShieldDisplayMode = 1;
int directionalShieldForwardPosition = 0;
int directionalShieldAftPosition = 2;
int directionalShieldPortPosition = 3;
int directionalShieldStarboardPosition = 1;
```

Mode `1` drains the bright portion of each segment proportionally. Mode `2`
keeps the complete segment visible and changes only its green/orange/red
colour, becoming black at exactly zero strength. The compass values are
`0 = north`, `1 = east`, `2 = south`, and
`3 = west`. The renderer identifies those four slots from the ODF rectangles,
then places the requested logical arc in each one; its live value and tooltip
move with it. All four values must be unique. Invalid or duplicate mappings
leave the existing ODF layout unchanged. ART assignments inherit through the
extension-root chain, with child values overriding parent values.

Each visible arc owns a normal and verbose hover tooltip. Add these entries to
the active mod or inherited parent's `Dynamic_Localized_Strings.h`:

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

The module appends the live `current / maximum` value to the localized short
tooltip. The verbose tooltip uses the localized description followed by the
localized strength label and the same live value. Hover regions use Armada's
logical interface coordinates, matching its resolution-adjusted cursor and
including per-Craft position overrides. If a localization key is absent, the
English text above remains as a safe fallback.

Add these names to any sprite table already included by the active mod's
`sprites.spr`:

```text
@reference=128
@tmaterial=interface

dsf shield_forward 0 0 128 128
dsb shield_back 0 0 128 128
dsl shield_left 0 0 128 128
dsr shield_right 0 0 128 128
```

The corresponding alpha-bearing `shield_*.tga` files may live in the active
RGB texture path when that GUI table is loaded after modules initialize. A
sprite registered in Armada's startup `Data/Sprites/gui_global.spr` needs a
startup-visible DDS or safe uncompressed TGA in `Data/Textures`, because the
central interface database is built before `A2FORGBTextures` initializes.
Preserve CRLF line endings when editing legacy `.spr` files; LF-only inserted
records can be merged with neighbouring directives by Armada's parser.
Forward and back retain a centred horizontal portion; left and right retain a
centred vertical portion. The bright portion eases toward the live value over
roughly 450 ms while a dim full arc remains as its empty track. This animation
is cosmetic only. The sprite cache is mission-lifetime: GUI/interface database
changes discard it, and every ST3D sprite's native frame-list sentinel is
validated before drawing. Aborting a mission can destruct those sprite objects
without unloading this module. Stale entries immediately fall back to text;
native sprite lookup is deferred until the interface database has remained at
one address for 500 ms, then the ring is re-resolved. Missing sprites disable
only the ring; the numeric rows remain as its fallback and gameplay remains
available. When the ring draws successfully, it replaces those diagnostic text
rows.

Place these definitions directly in the loaded `gui_global.spr`, or in a
GUI-only table included from that file's initial include section. Do not append
a late `@include`: the legacy parser may silently ignore includes encountered
after ordinary sprite declarations. Do not publish them through the world
`sprites.spr`/`testsprite.spr` chain and do not add `@sprite_node`
declarations—the renderer needs GUI database sprites, not scene nodes. For a
child mod, remember that a relative include opened by a physically inherited
parent table may resolve beside that parent; edit the `gui_global.spr` that is
actually loaded, or own a complete child GUI table deliberately.

Keep custom sprite identifiers at 27 characters or fewer. The legacy GUI
loader silently omits longer names; the `a2fo_shield_*` names above stay below
that boundary.

## Runtime boundary

The module accepts only the supported ArmadaL/Fleet Operations signatures. It
chains Fleet Operations' `CraftClass(ParameterDB)` constructor handler, copies
the three string vectors through Armada's `ParameterDB`, and appends two
rectangle-aware native GUI text draws to the stock selected-object render pass
while its display/scissor state remains active. The module exposes a bounded
selected-info observer used by EnergySystems so the same proven hook remains
the sole owner. The selected ship-name
component supplies Fleet Operations' display, font, scaling and clipping
state. It also resolves the directional module's bounded value exports lazily,
so module load order does not require a second selected-panel hook. The ring
uses Fleet Operations' loaded sprite database. Full facings draw their complete
bright arc; partial facings crop the sprite's texture window and destination
rectangle around the relevant horizontal or vertical centre. The texture
window and sprite colour are restored after every draw. The optional tooltip
hooks override only the four transformed arc rectangles and otherwise chain
Fleet Operations unchanged. If preflight fails, installed hooks remain safe
pass-throughs and the log reports that the runtime is disabled.

This completed-CraftClass boundary also forwards optional class observers to
the shield, nebula-renderer, and texture-variant modules. The texture observer
registers numbered subsystem damage meshes without competing for a second
CraftClass constructor hook.

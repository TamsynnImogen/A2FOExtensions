# Medium and tall selected-panel controls

A2FOCraftIdentity renders its additions on both the medium selected-object
panel and the tall producer panel, including repair-only yards. Controls still
require their gameplay data: an empty identity, absent ammunition store,
disabled directional shields or craft without a next XP rank stays absent.
No selection and multiple selection clear extension hover regions.

## CFG keys

Use `infoSingle<Element><Property>` for medium and
`infoBuild<Element><Property>` for tall. Each omitted tall property inherits
its medium equivalent independently. Existing global colours, ODF display
modes and automatic placements remain fallbacks. A tall colour override does
not require duplicating its rectangle, sprite or other colours.

| Property | Meaning |
| --- | --- |
| `Area` | `x y width height`, in the native info-panel coordinate system |
| `Color` | Independent RGB tint (`0..1`) |
| `LowColor` | RGB tint at 50% or below, for ammunition and shields |
| `CriticalColor` | RGB tint at 25% or below, for ammunition and shields |
| `BackgroundColor` | RGB tint for the empty bar track |
| `Sprite` | Quoted interface SPR identifier for a bar's fill |
| `BackgroundSprite` | Quoted interface SPR identifier for its empty track |

Rectangles control placement and available dimensions; text retains the native
GUI font. A zero-width or zero-height rectangle hides that element. Coordinates
use the native `infoPanelOffset` and current display origin in both panels.
Positions must fit within the mod's `infoPanelArea_Middle` / `infoPanelArea_Tall`
and avoid native build controls: this feature does not enlarge the panels.

Supported element names:

- `CaptainText`
- `RegistryText`
- `PhotonTorpedoesText`
- `PhotonTorpedoesLabelText`
- `PhotonTorpedoesValueText`
- `PhotonTorpedoesIcon`
- `PhotonTorpedoesBar`
- `QuantumTorpedoesText`
- `QuantumTorpedoesLabelText`
- `QuantumTorpedoesValueText`
- `QuantumTorpedoesIcon`
- `QuantumTorpedoesBar`
- `ShuttleCraftText`
- `ShuttleCraftLabelText`
- `ShuttleCraftValueText`
- `ShuttleCraftIcon`
- `ShuttleCraftBar`
- `DirectionalShieldsGraphic`
- `DirectionalShieldsForwardAftText`
- `DirectionalShieldsPortStarboardText`
- `DirectionalShieldsForward`
- `DirectionalShieldsForwardValueText`
- `DirectionalShieldsAft`
- `DirectionalShieldsAftValueText`
- `DirectionalShieldsPort`
- `DirectionalShieldsPortValueText`
- `DirectionalShieldsStarboard`
- `DirectionalShieldsStarboardValueText`
- `ExperienceBar`

The three ammunition `Text` elements retain the legacy combined row. Setting
an independent `LabelText` or `ValueText` area/colour separates label and value.
In ODF icon mode the label remains replaced by its icon. Icon areas resize the
destination independently of the existing ODF `*IconPos` source crop. `BarArea`
sets the exact bar dimensions instead of the legacy automatic eight-pixel
height; select the existing ODF `*ValueDisplayMode = "bar"` to show it.

Each shield-facing area is panel-relative and independent of the whole graphic.
Without facing overrides the graphic scales all four segments together. The
existing ODF and ART_CFG facing mapping still chooses each default arc's shape.
Value text still respects ODF `directionalShieldValueDisplayMode`.

Sprite properties apply to `PhotonTorpedoesBar`, `QuantumTorpedoesBar`,
`ShuttleCraftBar`, `ExperienceBar`, and the four individual
`DirectionalShieldsForward/Aft/Port/Starboard` elements. A facing with `Sprite`
uses a horizontal capacity bar; one without it retains the curved directional
arc and its existing display mode. Text and icon elements do not consume the
bar sprite properties; ammunition icons retain their ODF sprite/source crop.

The fill uses the sprite's registered UV window and crops its right edge with
remaining capacity; it does not squeeze a complete texture into the remaining
width. An omitted background sprite uses the fill sprite. An omitted fill
sprite uses `large_shield_bar`. A missing named sprite falls back to that same
stock sprite, with a diagnostic. UV width and colour are restored after every
draw, even when multiple controls share one sprite. Zero capacity leaves the
track visible. Use interface sprites from the active GUI SPR chain, with valid
textures and identifiers of at most 27 characters.

The existing native shield bars already use the `infoSingleShieldBar` sprite
name, `infoSingleShieldBarArea` medium rectangle and `infoBuildShieldBar` tall
rectangle. Those native keys remain authoritative. A2FO now adds the missing
shield hover region in both panels, following the matching native rectangle;
it preserves the native shield renderer and disabled-system appearance.
The extension XP bar is now rendered in both panels.

## Example

```text
// Medium layout and inherited defaults.
infoSingleCaptainTextArea = 386 130 340 20
infoSingleRegistryTextArea = 386 154 340 20
infoSingleRegistryTextColor = 0.75 0.85 1.0
infoSinglePhotonTorpedoesLabelTextArea = 386 186 180 20
infoSinglePhotonTorpedoesBarArea = 580 190 140 12
infoSinglePhotonTorpedoesBarColor = 0.2 0.9 1.0
infoSinglePhotonTorpedoesBarLowColor = 1.0 0.7 0.0
infoSinglePhotonTorpedoesBarCriticalColor = 1.0 0.1 0.1
infoSinglePhotonTorpedoesBarSprite = "ammo_fill"
infoSinglePhotonTorpedoesBarBackgroundSprite = "ammo_track"
infoSinglePhotonTorpedoesBarBackgroundColor = 0.25 0.25 0.25
infoSingleExperienceBarArea = 386 260 334 10
infoSingleExperienceBarSprite = "xp_fill"

// Tall layout: other properties inherit individually.
infoBuildCaptainTextArea = 386 170 340 20
infoBuildRegistryTextArea = 386 194 340 20
infoBuildPhotonTorpedoesLabelTextArea = 386 226 180 20
infoBuildPhotonTorpedoesBarArea = 580 230 140 16
infoBuildExperienceBarArea = 386 290 334 12
infoBuildExperienceBarColor = 0.6 0.3 1.0

// Each facing can instead use its own separately placed sprite bar.
infoBuildDirectionalShieldsForwardArea = 20 180 160 12
infoBuildDirectionalShieldsForwardSprite = "shield_fill"
infoBuildDirectionalShieldsForwardColor = 0.2 0.8 1.0
infoBuildDirectionalShieldsForwardValueTextArea = 190 180 80 20
infoBuildDirectionalShieldsForwardValueTextColor = 1.0 1.0 1.0
```

The example sprite names are placeholders: define them in the mod's interface
SPR table. The other ammunition stores accept the same suffixes independently.

## Verification

`tests/craft_identity_panel_smoke.cpp` exercises the production hooks using a
private local Armada executable mapped without running its entry point. Native
rectangle loading and text submission are exercised; configuration and final
sprite drawing use headless capture stubs. It covers separate medium/tall
rectangles, field inheritance, independent label/value placement, tall XP,
bar textures and UV cropping, zero/full/invalid capacity, restored shared sprite
state, missing-sprite fallback and stale sprite rejection. Full game rendering,
clipping and the target mod's art still require manual in-game checking.

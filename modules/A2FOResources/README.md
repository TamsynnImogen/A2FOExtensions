# A2FOResources

Adds four independent team resource pools beside Armada II's six native
resources, increasing the usable total from six to ten:

- tritanium
- supply
- credits
- collective connections

These are not aliases for latinum, metal, officers, or biomatter. Each pool is
checked, deducted, and refunded independently by Fleet Operations' Producer
paths.

`A2FOFeaturePack.dll` must be selected with this module because it owns the
shared Producer cancellation/delete/clear hooks which publish exact refund
events. `A2FOBuildTooltips.dll` is optional and adds the four non-zero costs to
build-button text when present.

## Object ODF costs

```ini
tritaniumCost = 120
supplyCost = 8
creditsCost = 25
collectiveconnectionsCost = 3
```

Missing commands default to zero. Values must be non-negative integers.

## Race ODF starting resources

```ini
normalTritanium = 1000
lotsTritanium = 5000
normalSupply = 100
lotsSupply = 500
normalCredits = 250
lotsCredits = 1000
normalCollectiveConnections = 10
lotsCollectiveConnections = 50
```

The normal/lots selection follows the live Instant Action starting-resource
setting. Missing commands default to zero.

## Race-specific resource presentation

All ten resources accept the same Race ODF presentation fields. The added four
consume all four fields. For native resources, A2FO consumes the short and
verbose tooltip fields at ResourceComponent entry points and the `Res` fields
at targeted verbose cost-text sites. Compact costs use `Icon`; A2FO also
supplies the officer icon at its separate normal cost-text site. The complete
pattern is:

```ini
crewRes = "GUI_CP_ROM_CREW_RES"
crewTooltip = "GUI_RD_ROM_CREW_TOOLTIP"
crewVerboseTooltip = "GUI_RD_ROM_CREW_VTOOLTIP"
crewIcon = "GUI_CP_ROM_CREW_ICON"

tritaniumRes = "GUI_CP_ROM_TRI_RES"
tritaniumTooltip = "GUI_RD_ROM_TRI_TOOLTIP"
tritaniumVerboseTooltip = "GUI_RD_ROM_TRI_VTOOLTIP"
tritaniumIcon = "GUI_CP_ROM_TRI_ICON"
```

The supported prefixes are `crew`, `officer`, `dilithium`, `latinum`,
`metal`, `biomatter`, `tritanium`, `supply`, `credits`, and
`collectiveconnections`. Append `Res`, `Tooltip`, `VerboseTooltip`, or `Icon`
to each prefix. ODF command matching is case-insensitive.

Each value may be a key from `Dynamic_Localized_Strings.h` or literal display
text. A matching key is localized; an unknown key is retained literally.
Missing name and tooltip fields retain the native presentation for resources
0 through 5 and the A2FO default name for resources 6 through 9. Missing icon
fields use A2FO's dedicated high-byte font glyphs. Officers have no default
picture yet and therefore retain Armada's localized compact text; an explicit
`officerIcon` field may still provide one. The resource balances remain
independent. Existing officer commands remain fully compatible, and the
process-wide localization lookup is deliberately not intercepted.

For native resources, `Tooltip` and `VerboseTooltip` replace the corresponding
ResourceComponent hover text. Native compact cost text uses `Icon`, while
verbose cost text uses `Res`. Both are resolved once per Race and returned
from a cache. The hot palette route does not perform Team lookup, locking,
memory queries, or localization. Armada's stock top resource panel renders
only numeric values, with no label string for `metalRes` (or its peers) to
replace. For resources 6 through 9, `Icon` is used by the added-resource panel
row and compact build costs, `Res` is used by verbose build costs, and the two
tooltip fields drive its hover forms. Added-row presentation strings are
resolved once per Race rather than in the render loop.

The active font uses this contiguous glyph block:

| Byte | Picture |
|---|---|
| `0x80` | metal |
| `0x81` | latinum |
| `0x82` | biomatter |
| `0x83` | energy (reserved; not a resource) |
| `0x84` | dilithium |
| `0x85` | collective connections |
| `0x86` | supply |
| `0x87` | tritanium |
| `0x88` | crew |
| `0x89` | credits |
| `0x8A` | build time |

Classic-font mods need only populate these eleven slots; their ordinary
alphabet and remaining metrics can stay unchanged.

## Resource panel

The four balances are drawn as an independent second resource row. Its default
rectangles are:

```ini
resource_6 = 50 30 120 18   // tritanium
resource_7 = 250 30 120 18  // supply
resource_8 = 450 30 120 18  // credits
resource_9 = 650 30 120 18  // collective connections
```

These coordinates use the same `gui_interface.cfg` coordinate space as
`resource_0` through `resource_5`; the module derives Fleet Operations' live
screen scaling from one shared live panel text context. No added resource is
paired with or falls back to a native resource. A GUI should also make
`resourcePanelArea` and its background tall enough for the second row.

## Native API

Resource indices continue the native six:
`6=tritanium`, `7=supply`, `8=credits`, and
`9=collective connections`. The module exports `A2FOResources_Get`,
`A2FOResources_Set`, `A2FOResources_Add`, `A2FOResources_GetCost`, and
`A2FOResources_GetPresentationText` for other native extensions. The last
accepts a Team, any resource index `0..9`, and one of the four presentation
constants (`RES`, `TOOLTIP`, `VERBOSE_TOOLTIP`, or `ICON`) to return the
Race-localized presentation text. Function-pointer types
and resource constants are in `api.hpp`; resolve the exports from
`A2FOResources.dll` at runtime.

The implementation covers independent storage, starting values, production
affordability/payment, cancellation and queue refunds, the second resource
panel row, build-tooltip costs, and native extension access. Save-game
persistence is not yet enabled: loading a save starts these four sidecar pools
from the selected race's normal/lots values. This remains explicit until a
versioned save extension can coexist safely with unextended saves.

Mining and freighter cargo, direct and route trading, `ResourceWeapon` income,
scripted grants, gifting, and AI economic planning are not yet extended to
indices 6 through 9. Fleet Operations' historical tritanium and
supplies helpers alias native latinum and biomatter, so they must not be used as
compatibility fallbacks for these independent pools.

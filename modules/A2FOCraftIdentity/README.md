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

## Runtime boundary

The module accepts only the supported ArmadaL/Fleet Operations signatures. It
chains Fleet Operations' `CraftClass(ParameterDB)` constructor handler, copies
the three string vectors through Armada's `ParameterDB`, and appends two
rectangle-aware native GUI text draws to the stock selected-object render pass
while its display/scissor state remains active. The selected ship-name
component supplies Fleet Operations' display, font, scaling and clipping
state. If preflight fails, installed hooks remain safe pass-throughs and the
log reports that the runtime is disabled.

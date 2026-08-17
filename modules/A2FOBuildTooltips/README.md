# Build-button tooltip times

A2FOBuildTooltips.dll inserts build time into Armada's native parenthesised
cost row as another compact amount/unit token. The compact unit is the
build-time glyph at byte `0x8A`:

    /42 [build-time icon]

The compact token is used by the normal tooltip. The verbose tooltip expands
the unit to `42 seconds` (or `1 second`), matching its full resource names.
Both forms place the time immediately before the closing bracket.

When `A2FOResources.dll` is active, the same tooltip inserts every non-zero
`tritaniumCost`, `supplyCost`, `creditsCost`, and
`collectiveconnectionsCost` value before the build-time token and Armada's
native closing cost bracket.
Normal tooltips use each Race-specific `Icon` glyph, matching Armada's compact
native resource presentation; verbose tooltips use the full `Res` names.
Objects which use only the original six resources retain one
native cost row with the build-time token at its end.

The displayed value comes from Armada's native adjusted build-time query for
the local team. It therefore follows the Instant Action build-time setting and
the same team and AI modifiers used when construction starts. The number is
shown in game seconds; changing game speed changes how quickly those seconds
pass in real time, just as it does for the rest of the game simulation.

Compact additional-resource costs use the local Race ODF's `tritaniumIcon`,
`supplyIcon`, `creditsIcon`, and `collectiveconnectionsIcon` localization keys
when present, otherwise using A2FO's dedicated glyph slots.
Verbose costs use the corresponding `Res` fields.

Install the DLL in the central `Data/modules` directory, then opt a mod into it
with an `active` entry in that mod's `info.ini`:

    active0 = "A2FOBuildTooltips"

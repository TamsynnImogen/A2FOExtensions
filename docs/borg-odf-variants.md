# Ownership ODF variants

The original Borg-only `_b` prototype has moved out of the core and become the
optional [`A2FOODFVariants`](../modules/A2FOODFVariants/README.md) module.

Borg keeps the stock `_b` convention automatically. Other races use the same
`factionTextureSuffix` value consumed by `A2FOTextureVariants`, allowing ODF
and texture ownership variants to remain synchronized without either module
depending on the other DLL.

See the module README for naming, fallback, state preservation, repeated
capture behaviour, and current test targets.

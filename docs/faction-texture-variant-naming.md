# Faction texture variant naming

Faction/ownership suffixes are always the final suffix in a texture name.

For a base material named `ship` and Borg suffix `_b`:

- diffuse: `ship_b`
- bump/normal convention: `ship_bump_b`
- specular: `ship_specular_b`
- emissive: `ship_emissive_b`
- subsystem emissive example: `ship_emissive_warp_b`

For a custom Klingon suffix `_k`, the same material becomes `ship_k`,
`ship_bump_k`, `ship_specular_k`, and `ship_emissive_warp_k`.

The renderer first looks for the faction-specific auxiliary map and falls back
to the unsuffixed base auxiliary map when no faction map exists. For example,
`ship_emissive_warp_b` falls back to `ship_emissive_warp`.

The older experimental ordering (`ship_b_emissive_warp`, `ship_b_specular`)
is not the faction-map convention.

Bump/normal maps currently remain shared CraftClass/SOD material state in
A2FOExtensions, so faction-specific bump names are reserved for the future
per-draw slot-1 implementation. The naming rule is documented now so content
made for A2FOExtensions and STA64 can use the same convention.

# A2FOCollectiveXP

`A2FOCollectiveXP.dll` gives each player using an opted-in Fleet Operations
faction an independent fleet rank and XP pool. The adapter observes live native craft with valid
positive next-rank XP metadata; each craft's native chain contributes to its
owner's next-rank threshold. Ordinary, Sleeper, and Saboteur XP awards add to
that pool without applying the modifier. Promotions use Fleet Operations' own
`Craft_mLevelUp` replacement path.

This is the first native-integrated implementation. Policy, queue-admission,
and native-adapter fixture tests exist, but the feature has **not** been
accepted in a live game. The implementation is not feature-complete: the shared
build menu still shows base-class names, icons, costs, and tooltips; captured
craft cannot be safely demoted; and preservation of existing module sidecars
through native replacement has not been demonstrated. See the [full behavior,
compatibility evidence, and manual checklist](../../docs/collective-xp.md).

## Configuration

Opt factions into the feature in their race ODF:

```ini
collectiveXP = 1
collectiveXPModifier = 1
```

`collectiveXP` accepts only `0` or `1`; absent or `0` keeps that faction on the
stock XP path. `collectiveXPModifier` must be finite and greater than zero, and
defaults to `1`. Invalid settings leave the faction stock. The modifier scales
only the class-derived threshold; awards are pooled unscaled. With no eligible
next-rank threshold, XP remains in the pool but the collective level cannot
advance.

Rank classes must define a valid native chain with `XPlevel` starting at 1,
`nextrankXP`, and `nextrankClass`. `originalClass` is used to identify a
family when the observed craft is already at a later rank; the referenced
chain is validated before use. No filename suffix is inferred.

## Dependencies and build

Build `A2FOCollectiveXP.dll` with its native bridge and `collective_xp.cpp`.
`A2FOFeaturePack.dll` is required for the updated Producer queue-admission
callback. `A2FOCraftIdentity.dll` is optional and exposes the collective pool
in its selected-craft XP bar when its progress export is available. Select the
modules for the mod in `info.ini`, retaining its other entries:

```ini
[modules]
required0 = "A2FOCollectiveXP"
required1 = "A2FOFeaturePack"
active0 = "A2FOCraftIdentity"
```

```sh
make collective-xp-test
make build/modules/A2FOCollectiveXP.dll
make build/modules/A2FOFeaturePack.dll
# Optional selected-panel progress integration:
make build/modules/A2FOCraftIdentity.dll
```

These commands build or run local fixtures; they do not deploy modules or
establish in-game acceptance. Keep the module and faction configuration
consistent across saves and loads. Enabled teams write a 33-byte per-team
`CXP1` record; with the module loaded and that team opted in, a missing or
invalid record causes the module's load hook to reject the load rather than
reset XP. Behavior when the module or opt-in configuration is absent is
unsupported; the stock game may not recognize the extension record, so do not
assume it will reject the load. Existing saves cannot be migrated by enabling
the feature mid-match. Native construction objects are explicitly excluded;
custom construction paths still need an in-game check.

## Verification status

`make collective-xp-test` covers rank-chain validation, pool policy,
persistence codec, and queue resolver paths. `make collective-xp-native-test`
runs the x86 Wine bridge, adapter and queue-receiver fixtures with display
variables unset; it tests synthetic native callbacks, not a live match. Neither
target establishes live replacement state, saved-match interoperability,
multiplayer synchronization, or visual presentation. Use the manual matrix in
the full reference before treating a build as game-verified.

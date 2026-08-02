# Configurable ship-system upgrade pods

This feature is implemented for the supported Armada 1.1 and Fleet Operations
binaries. Basic level-2 → level-3 → level-4 progression, including
construction-order-independent system matching, is confirmed in game. Higher
levels and the wider save/load and multiplayer matrix still require validation.

## Enable a bounded maximum

The selected `scripts\UpgradePods.lua` controls the maximum tier:

```lua
a2fo.require_api(1, 2)
a2fo.configure_upgrade_pods({ maximum_tier = 6 })
```

The allowed range is 3–16 and the default is 3. A child mod can override the
shared policy by supplying its own `UpgradePods.lua`; only the highest
precedence copy executes.

Higher-tier pod ODFs continue to use the ordinary Armada command:

```text
upgradeLevel = 4
```

A2FO retains that declared tier separately and exposes only tier 3 to Armada's
fixed Team arrays. This prevents an out-of-bounds tier index. For each team and
ship system, the highest attached declared tier supplies the effective upgrade
multiplier. Removing it restores the next-highest attached tier.

## Upgrade-station build lists

The station command tier is zero-based after the vessel's built-in level-1
systems. It maps to `upgradeLevel = tier + 2`:

| Station command | Pod ODF value |
| --- | --- |
| `tier0BuildItem<N>` | `upgradeLevel = 2` |
| `tier1BuildItem<N>` | `upgradeLevel = 3` |
| `tier2BuildItem<N>` | `upgradeLevel = 4` |

For example:

```text
tier0BuildItem0 = "pod_weapons_2"
tier0BuildItem1 = "pod_engines_2"

tier1BuildItem0 = "pod_weapons_3"
tier2BuildItem0 = "pod_weapons_4"
tier3BuildItem0 = "pod_weapons_5"
tier4BuildItem0 = "pod_weapons_6"
```

The item index identifies one upgrade chain and must stay consistent across
levels. For example, `tier1BuildItem4` and `tier2BuildItem4` describe the
level-3 and level-4 replacements for the same system pod. The referenced pod
ODFs must therefore retain the same `upgradeSystem`. Indices may contain gaps.
Each command changes only its matching index; it never compacts or moves
another research item. An explicitly empty value removes only that indexed
item. If no new command exists for an index, Fleet Ops' existing `buildItem`
or `secondaryBuildItem` entry at that index remains unchanged.

Each referenced ODF must be an upgrade `ResearchPod` whose `upgradeLevel`
equals the command tier plus 2. Invalid entries are ignored and logged. If a
tier contains non-empty commands but none resolve successfully, its existing
entries are preserved. Command tier 0 uses Fleet Ops' Producer list (57 usable
slots), while command tier 1 updates the corresponding entries in its expanded
58-entry secondary list. Higher tiers stay in A2FO sidecar state so they cannot
overwrite or displace unrelated Fleet Ops research. Each live extended station
receives an exact private copy of the native secondary list; only the matching
occupied system advances to its next configured pod level. A2FO identifies
attached pods by `upgradeSystem`, not by their construction-order position in
Fleet Ops' live pod array. It also preserves the native level-2 prerequisite
relationship after presenting level 4+, preventing level 2 from reappearing
between levels 3 and 4. The station still needs sufficient `podHardpoints` for
its physical pod positions.

## Validation still required

- construct, attach, replace, and destroy levels 4–6 for all five systems;
- confirm tier identity, buttons, tooltips, AI selection, and hardpoint routing;
- save/load with several native and extended tiers attached, then remove them
  in descending order and verify multiplier restoration;
- load unmodified stations and older saves;
- run a two-peer synchronized match using identical Lua and ODF files.

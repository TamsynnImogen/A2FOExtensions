# A2FOExtensions modular runtime

This package preserves the proven startup chain while separating checked engine
hooks, reusable dispatch, optional native features, and mod-authored Lua logic.

## User-facing features

- Recursive ODF discovery from arbitrary subdirectories.
- Recursive ODF indexing inside active loose roots and `odf.fpq` archives.
- Correct Data → `ParentMod` → active-mod file precedence.
- `wingman` classlabel compatibility alias mapped to `craft`.
- `hybridbuild` opt-in classlabel mapped to `research` for the staged
  HybridBuild implementation, including separate construct/yard/research/evolve
  menus, one shared ten-slot queue, queued station placement previews, and
  protected native construction/evolution sidecars.
- Per-Evolver and HybridBuild `cocoon` ODF command for custom cocoon models.
- Lua-driven wreckage or replacement objects when units are destroyed.
- Deterministic `wreckageChance` support suitable for synchronized games.
- Ctrl-click to fill all ten native construction-queue slots.
- Ctrl+Alt-click for continuous production and automatic queue refilling.
- Continuous production stops when the queue is manually altered or the yard
  is destroyed.
- Resource-shortage pause and automatic production retry.
- Experimental save/load markers for continuous-production state.
- `DefaultGameSpeed` field in `info.ini`, accepting speeds 1–6.
- `SettingsDirectory` field for redirecting mod configuration and profile
  files.
- `%APPDATA%`, `%USERPROFILE%`, and other Windows environment-variable
  expansion.
- Absolute, relative, shared, and per-mod settings-directory layouts.
- Data-level shared settings roots with active-mod folders stored below
  `mods\<folder>`.
- Configurable ship-system upgrade pods through level 16, with the included
  Lua policy enabling levels through 6.
- Tier-indexed upgrade-station lists which preserve unrelated research and
  progress level 2 → level 3 → higher levels independently for each system.
- Upgrade-pod progression is independent of the order in which systems are
  constructed; removing a higher pod restores the next-highest multiplier.

## Modding framework

- Versioned native module API with backward-compatible capability revisions.
- Automatic deterministic loading of `modules\*.dll`.
- Data, `ParentMod`, and active-mod module/script overlay.
- Embedded Lua 5.4.8 runtime.
- Restricted Lua environment with memory, file-size, and instruction limits.
- Lua classlabel, Evolver-cocoon, and object-destroyed callbacks.
- Bounded temporary ODF views exposed safely to Lua.
- Native destroyed-object event dispatcher.
- Transactional module and Lua registration with rollback after failed
  initialization.
- SDK header and example native module.
- Central logging for the core, modules, and Lua scripts.
- Checked hook signatures and supported-binary validation.

## Current modding commands

### `info.ini`

Place these fields in the active mod's `[mod]` section:

```ini
[mod]
DefaultGameSpeed = 3
SettingsDirectory = My Mod
```

`DefaultGameSpeed` supplies the initial game speed from 1 through 6. An
existing saved profile still takes precedence.

`SettingsDirectory` redirects the mod's settings files and supports:

- bare directory names;
- relative paths;
- absolute Windows and UNC paths;
- Wine `Z:\` paths;
- variables such as `%APPDATA%` and `%USERPROFILE%`;
- a Data-level shared root with active mods stored below
  `mods\<folder>`.

See [`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md) for the
full resolution rules.

### Object ODF commands

Treat a compatibility object as an ordinary craft:

```text
classLabel = "wingman"
```

Select a custom cocoon model for an Evolver (`.sod` is optional):

```text
classLabel = "evolver"
cocoon = "custom_cocoon.sod"
```

Request a replacement or wreckage object when a craft is destroyed:

```text
wreckage = "my_ship_wreck"
wreckageChance = 50
```

`wreckage` is the replacement ODF basename. `wreckageChance` accepts 0–100,
defaults to 100, and requires the included `scripts\Wreckage.lua`.

Higher ship-system upgrade pods continue to use Armada's existing field:

```text
upgradeLevel = 4
```

A2FO safely retains levels above 3 in sidecar state rather than indexing past
Armada's fixed Team arrays.

### Upgrade-station ODF commands

The station command tier is zero-based after the ship's built-in level-1
systems, so command tier 0 selects upgrade level 2:

```text
tier0BuildItem0 = "pod_weapons_2"
tier1BuildItem0 = "pod_weapons_3"
tier2BuildItem0 = "pod_weapons_4"
tier3BuildItem0 = "pod_weapons_5"
tier4BuildItem0 = "pod_weapons_6"
```

Use the same item index and `upgradeSystem` for every level in one chain. Each
command replaces only its matching index; unspecified indices retain their
vanilla or Fleet Ops entries. The referenced pod's `upgradeLevel` must equal
the command tier plus 2. See
[`docs/upgrade-pods.md`](docs/upgrade-pods.md) for validation and fallback
rules.

### Lua API commands

The principal public entry points are:

```lua
a2fo.require_api(1, 2)
a2fo.has_capability("configurable_upgrade_pods")
a2fo.log("message")

a2fo.configure_upgrade_pods({ maximum_tier = 6 })
a2fo.on_classlabel(function(classlabel, odf) end)
a2fo.on_evolver_cocoon(function(odf) end)
a2fo.on_object_destroyed({"fieldName"}, function(event) end)
```

The upgrade-pod maximum accepts 3–16 and defaults to the native maximum of 3
when no selected script claims the policy. Callback ODF views provide bounded
`odf:get_string()` access, while destroyed-object events provide deterministic
`event:roll_percent()`. See [`docs/lua-api.md`](docs/lua-api.md) for callback
contracts and return values.

### Construction controls

- **Ctrl + click:** fill every remaining position in the ten-slot native
  construction queue.
- **Ctrl + Alt + click:** activate continuous production and automatic queue
  refilling.

While continuous production is active, selecting the active item normally,
selecting a different item, deleting or clearing an item, or destroying the
yard disables repeat. Resource shortages pause production and trigger
automatic retries.

The queue save/load and multiplayer behaviour remain partially validated.
Basic level-2 → level-3 → level-4 upgrade-pod progression is confirmed in
game; higher levels, save/load, and multiplayer still need validation. Button
overlays, configurable hotkeys, and the proposed Noxter mechanics are not yet
implemented.

## Core/module/script boundary

The core owns shared call sites, dispatch ordering, engine-object lifetimes, and
registration rollback. Native modules handle features that need deeper
engine/filesystem access, such as recursive ODF discovery, cocoon SOD selection,
and Producer integration. Lua scripts supply optional logic through narrow
semantic APIs when conditions and composition make scripting worthwhile. See
[`docs/architecture.md`](docs/architecture.md).

Queue controls and their current validation status are documented in
[`docs/queue-enhancements.md`](docs/queue-enhancements.md). Supported binary
identities and checked addresses are recorded in
[`docs/addresses.md`](docs/addresses.md).
The two optional Fleet Ops mod-information fields are documented in
[`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md).
Upgrade-pod configuration and ODF commands are documented in
[`docs/upgrade-pods.md`](docs/upgrade-pods.md).

## Expected runtime layout

```text
Armada II/
├── A2FOExtensions.dll
├── Win2kDisableTaskSwitch.dll
├── Win2kDisableTaskSwitch.original.dll
├── modules/
│   └── A2FOFeaturePack.dll
├── scripts/
│   └── UpgradePods.lua      (bounded tier policy; mod-overridable)
└── A2FOExtensions.log
```

The core retains its original log filename. Module and script messages are
prefixed in the shared log so startup order and overlay selection remain easy
to diagnose.

## Building on Nobara/Fedora

This project must be compiled as **32-bit Windows x86**, because Armada II and
Fleet Operations are 32-bit processes. Do not use the 64-bit MinGW compiler.

Install the toolchain:

```bash
sudo dnf install mingw32-gcc-c++ mingw32-binutils make
```

Build the release artifacts:

```bash
chmod +x build.sh
./build.sh
```

Or use Make directly:

```bash
make -j"$(nproc)"
make verify
make smoke
```

Outputs:

```text
build/A2FOExtensions.dll
build/Win2kDisableTaskSwitch.dll
build/modules/A2FOFeaturePack.dll
```

The SDK example is deliberately excluded from releases. Build and inspect it
separately with `make sdk-examples verify-sdk`.

The build links the MinGW runtime statically. `make verify` rejects outputs
that depend on deploy-time MinGW DLLs such as `libwinpthread-1.dll`; Armada's
`Data` directory should not need compiler runtime files added to it.
On Linux, `make smoke` also verifies under Wine that the core DLL loads and
exports `A2FO_Initialize`.

Before installing the proxy, rename the original shipped startup DLL:

```text
Win2kDisableTaskSwitch.dll
    -> Win2kDisableTaskSwitch.original.dll
```

Then copy the newly built `Win2kDisableTaskSwitch.dll`,
`A2FOExtensions.dll`, and the `modules` directory into the Fleet Operations
root directory.

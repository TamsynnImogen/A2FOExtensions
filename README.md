# A2FOExtensions modular runtime

This package preserves the proven startup chain while separating checked engine
hooks, reusable dispatch, optional native features, and mod-authored Lua logic.

## User-facing features

- Recursive ODF discovery from arbitrary subdirectories.
- Recursive ODF indexing inside active loose roots and `odf.fpq` archives.
- Correct Data → `ParentMod` → active-mod file precedence.
- Optional `A2FOCheats.dll` enhancement makes `showmethemoney` grant
  individually configurable amounts of Dilithium, Tritanium, Metal, Supplies,
  and Crew (10,000 each by default), and restores the missing `m`, `dis`,
  `crash`, and true team-elimination `elim` cheats.
- Optional ODF-driven captain names and ship registries, row-aligned with the
  native `possibleCraftNames` choice and displayed only for the selected craft
  through `infoSingleCaptainTextArea` and `infoSingleRegistryTextArea`.
- Experimental indexed hull-mounted turrets through matching `turretX` and
  `turretHardpointX` ODF commands, with independent weapons, hitpoints, yaw,
  pitch, slew rates, ownership changes, and save/load reconnection.
- Optional `A1Compat.dll` support for the Armada 1 `wingman` classlabel,
  mapped safely to `craft` only through the `STA1 Classic` mod chain.
- Armada 1 `Addon` ODF overlay support through `A1Compat.dll`, preserving A1's
  within-root rule that `Addon` wins over a same-basename structured ODF.
- Armada 1 starbase officer-quarter compatibility: A1 ODF limits and gains,
  sequential `oqN` model reveal, native FO officer-cap changes, ownership
  reversal, queue admission limits, and compatibility save state.
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
- Optional direct Armada 1/2 legacy texture bridge for `Textures\RGB`,
  `Textures\Index8`, and `Textures\Compressed` across the same mod roots.
- Embedded Lua 5.4.8 runtime.
- Restricted Lua environment with memory, file-size, and instruction limits.
- Lua classlabel, Evolver-cocoon, and object-destroyed callbacks.
- Bounded temporary ODF views exposed safely to Lua.
- Native destroyed-object event dispatcher.
- Native Producer admission/completion/destruction event dispatcher.
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

### `RTS_CFG.h` cheat amounts

Override any of the five `showmethemoney` grants with literal values from 0
through 100,000,000:

```cpp
int SHOWMETHEMONEY_DILITHIUM = 10000;
int SHOWMETHEMONEY_TRITANIUM = 10000;
int SHOWMETHEMONEY_METAL = 10000;
int SHOWMETHEMONEY_SUPPLIES = 10000;
int SHOWMETHEMONEY_CREW = 10000;
```

Values inherit per field through Data, parent mods, and the active mod. Missing
or invalid fields keep the inherited value, falling back to 10,000.

### Object ODF commands

Treat a compatibility object as an ordinary craft:

```text
classLabel = "wingman"
```

This alias is supplied by `A1Compat.dll` in the `STA1 Classic` parent mod; it
is not enabled globally for unrelated FO4 or STA2 mods. If a wingman ODF and
its include chain omit one of the stock craft identity flags or subsystem
damage percentages, A1Compat supplies the corresponding value from STA1
Classic's `a2craft.odf`. Any value declared by the object or inherited from a
parent ODF remains authoritative.

The same missing-only behavior applies `a2const.odf`'s six common constructor
commands to objects whose original classlabel is `constructionrig`.

Objects with the original `freighter` classlabel similarly receive the seven
mining/resource defaults from `a2freight.odf` when those commands are absent.

Mount linked turret objects on a Craft-derived parent with indexed pairs:

```text
turret0 = "bsg_dual_turret"
turretHardpoint0 = "hp_turret00"
```

Indices 0 through 64 may be sparse. The referenced ODF uses
`classLabel = "turret"`, owns its weapons and hitpoints, and can configure yaw,
pitch, slew rates, and rest angles. See
[`modules/A2FOTurrets/README.md`](modules/A2FOTurrets/README.md) for the full
contract and current first-version limitations.

Give any ordinary weapon an optional owner-local three-dimensional firing
volume:

```text
fireArcMode = "box"
fireArcYaw = 0
fireArcPitch = 0
fireArcYawAngle = 90
fireArcPitchAngle = 60
```

`box` uses independent total yaw and pitch coverage; `cone` uses
`fireArcAngle` for a true circular cone. The shorter `fireArcCenter` and
`fireArcWidth` commands define a horizontal box with unrestricted pitch.
Weapons without the new commands retain native Fleet Operations firing-arc
behavior. See
[`modules/A2FOFireArcs/README.md`](modules/A2FOFireArcs/README.md) for the
complete orientation, precedence, validation, and fallback rules.

Ordinary cannon, phaser, pulse, and torpedo weapon ODFs can also use the
active Fleet Operations technology tree. Add the weapon ODF to the usual
`.tt` file just like any other project:

```text
fbphas.odf 1 fresearch.odf
```

An unlisted normal weapon is treated as `0` and remains available. Listed
weapons use Fleet Operations' native prerequisite evaluation; special weapons
retain their existing native path. See
[`modules/A2FONormalWeaponTech/README.md`](modules/A2FONormalWeaponTech/README.md).

Give a Craft-derived object candidate captain names and registries:

```text
possibleCraftNames = "USS Enterprise" "USS Excelsior"
possibleCaptainNames = "Captain A" "Captain B"
possibleCraftRegistry = "NCC-1701" "NX-2000"
```

Entry `N` in both companion lists follows Fleet Operations' native ship-name
entry `N`. Enable their selected-panel positions with
`infoSingleCaptainTextArea` and `infoSingleRegistryTextArea` in the active GUI
configuration, and optionally set `captainNameColor` and `shipRegistryColor`.
See
[`modules/A2FOCraftIdentity/README.md`](modules/A2FOCraftIdentity/README.md)
for placement, fallback, determinism, and save/load details.

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
[`docs/queue-enhancements.md`](docs/queue-enhancements.md). The complete hook
ledger—including supported binary identities, every direct patch and byte
signature, helper/global RVAs, startup-loader provenance, and object-layout
offsets—is in [`docs/addresses.md`](docs/addresses.md).
The two optional Fleet Ops mod-information fields are documented in
[`docs/fleetops-info-defaults.md`](docs/fleetops-info-defaults.md).
Legacy texture-folder activation and precedence are documented in
[`modules/A2FORGBTextures/README.md`](modules/A2FORGBTextures/README.md).
Captain/registry ODF lists and GUI fields are documented in
[`modules/A2FOCraftIdentity/README.md`](modules/A2FOCraftIdentity/README.md).
Three-dimensional weapon firing volumes are documented in
[`modules/A2FOFireArcs/README.md`](modules/A2FOFireArcs/README.md).
Normal-weapon technology-tree enforcement is documented in
[`modules/A2FONormalWeaponTech/README.md`](modules/A2FONormalWeaponTech/README.md).
Indexed hull-turret ODF commands and validation status are documented in
[`modules/A2FOTurrets/README.md`](modules/A2FOTurrets/README.md).
Armada 1 parent-mod scope and installation are documented in
[`docs/a1-compatibility.md`](docs/a1-compatibility.md).
Upgrade-pod configuration and ODF commands are documented in
[`docs/upgrade-pods.md`](docs/upgrade-pods.md).

## Expected runtime layout

```text
Armada II/
├── A2FOExtensions.dll
├── Win2kDisableTaskSwitch.dll
├── Win2kDisableTaskSwitch.original.dll
├── modules/
│   ├── A2FOCraftIdentity.dll
│   ├── A2FOFireArcs.dll
│   ├── A2FOFeaturePack.dll
│   ├── A2FOHybridBuild.dll
│   ├── A2FOInfoIni.dll
│   ├── A2FONormalWeaponTech.dll
│   ├── A2FORGBTextures.dll
│   └── A2FOTurrets.dll
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
build/modules/A2FOHybridBuild.dll
build/modules/A2FOInfoIni.dll
build/modules/A2FOCraftIdentity.dll
build/modules/A2FOFireArcs.dll
build/modules/A2FONormalWeaponTech.dll
build/modules/A2FORGBTextures.dll
build/modules/A2FOTurrets.dll
build/sta1-classic/modules/A1Compat.dll
```

Build the optional parent-mod package with `make sta1-classic` and inspect it
with `make verify-sta1-classic`. The staged files appear under
`build/sta1-classic/`; install that content as `Data/Mods/STA1 Classic`.

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

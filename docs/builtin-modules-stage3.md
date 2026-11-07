# Built-in modules: third slice

The third built-in slice adds `A2FOTeamChangeWeapons` and
`A2FOWeaponDamageControls` to `A2FOExtensions.dll`. Together with the five
modules from [stage one](builtin-modules-stage1.md) and
[stage two](builtin-modules-stage2.md), seven existing module names are now
provided by the core. Keep the original names in the Modules menu and each
mod's `info.ini`.

## Selection and compatibility

The existing `[modules]` policy is unchanged: `requiredX` locks a module on,
`rejectX` excludes it, and inheritance and saved optional selections retain
their existing rules. Mods without a saved `[modules]` section keep the
legacy load-all behavior. The core prepares each non-rejected built-in once,
including initially inactive modules; rejected modules are not prepared.
Stale standalone DLLs for these modules are ignored during discovery, so they
do not register a second copy. Standalone targets remain available for SDK
compatibility, but neither feature has a separate DLL release dependency.

`A2FOTeamChangeWeapons` requires successful native hook setup plus the weapon-class
and craft dispatchers. `A2FOWeaponDamageControls` requires successful native hook
setup and the weapon-class dispatcher. If a module's required setup is not
available, that module is unavailable for the process.

## Live policy and feature behavior

Team metadata is captured during preparation even when
`A2FOTeamChangeWeapons` is off. Its live flag gates only future ownership
change requests. Turning it off cancels queued requests, and activation tokens
also invalidate requests queued before a quick off/on transition even if the
simulation has not advanced. Cleanup and successful PostLoad retain their
existing pending-request cleanup behavior. Already-activated native effects
and countdowns are not reversed.

`A2FOWeaponDamageControls` also collects weapon metadata while off, but applies
its custom `canDamageShields`, `canDamageHull`, `shieldDamageModifier`, and
`hullDamageModifier` settings only while enabled. The original `CraftDamage`
path and shared DirectionalShields bridge remain active independently of
this flag. This stage also rebuilds DirectionalShields with core-bridge
discovery and compatibility with older standalone providers. No hook
addresses or ABI change as part of this integration.

## Focused verification

The repository's focused checks for this integration are:

- `make builtin-tests`
- `make builtin-native-tests`
- `make verify-builtins`

The local [validation receipt](../build/builtin-stage3-validation.json) records
the build, portable state test, 25 headless native fixture runs, export checks,
and artifact hashes. Synthetic coverage does not establish live-game,
multiplayer, or save acceptance. No deployment is included in this stage.

## Manual acceptance

After a separately authorized deployment, restart so the process loads the
new core. Verify the original names and selections in the Modules menu, then
toggle each feature off and on with Save. Check initially inactive modules,
required modules that remain enabled, rejected modules that remain excluded,
and inheritance across the existing mod roots.

For `A2FOTeamChangeWeapons`, queue an ownership-change request and toggle off,
then on again before the next simulation tick; confirm the canceled request
does not activate the weapon. Also verify normal enabled team-change behavior
and that an already-started native effect or countdown continues normally.
For `A2FOWeaponDamageControls`, compare the custom damage commands while
enabled and disabled. Confirm DirectionalShields still handles its shared
bridge behavior independently. Native game, multiplayer, and save acceptance
remain pending.

# Built-in modules: second slice

The second built-in slice adds `A2FOInfoIni` and `A2FOMuzzleFlashes` to
`A2FOExtensions.dll`, alongside `A2FOWreckage`, `A2FONormalWeaponTech`, and
`A2FOBuildTooltips` from [stage one](builtin-modules-stage1.md). Keep the
original module names in each mod's `info.ini` and the Modules menu.

## Selection and compatibility

The existing `[modules]` policy is unchanged. `required` locks a module on;
`reject` excludes it. Existing inheritance rules remain in force, and mods
without a `[modules]` section retain legacy load-all behaviour. The core
prepares each non-rejected built-in once, including initially inactive ones;
rejected modules are not prepared. Old standalone DLLs for these five modules
are ignored by discovery, preventing duplicate registration. Standalone
targets remain available for compatibility testing, but the five features
have no separate DLL release dependencies.

Availability is process-wide: `A2FOInfoIni` needs successful initialization
and all five settings/game-speed dispatcher hooks; `A2FOMuzzleFlashes` needs
its inline ordnance hook and the weapon-class dispatcher. If required setup is
unavailable, the module is unavailable for that process.

## Live policy and feature behavior

Live changes gate future callbacks. For `A2FOInfoIni`, a disabled provider
returns no defaults; already loaded settings, profile speeds, and paths are
not undone. The existing timing-critical settings and game-speed hooks remain
installed.

For `A2FOMuzzleFlashes`, weapon metadata remains prepared while the feature is
off, and the live flag gates only flash emission. Original ordnance
construction and the existing A2FOAnimations firing notification remain
active independently of that flag. Native particles expire normally. The
other three modules retain the behavior described in stage one.

## Focused verification

Run the repository's focused checks when validating this integration:

- `make builtin-tests`
- `make builtin-native-tests`
- `make verify-builtins`

These cover builds and synthetic runtime behavior and do not establish in-game
acceptance. Recorded results and artifact hashes are in the local
[validation receipt](../build/builtin-stage2-validation.json). No deployment
is included in this stage.

## Manual acceptance

After a separately authorized deployment, restart so the process loads the
new core. In a test mod, verify the five modules' initial selections, then
toggle each off and on with Save. Confirm required modules stay on and rejected
modules stay off. Confirm choices remain isolated between mod roots.

For InfoIni, check `SettingsDirectory` and `DefaultGameSpeed` with the module
enabled and disabled, including the documented caveat that disabling it does
not reverse values already loaded in the current process. For muzzle flashes,
check configured and unconfigured weapons, then toggle the feature off and on;
confirm ordnance and A2FOAnimations firing events continue while flash
emission is off, and existing particles expire normally. Native game, save,
and multiplayer acceptance remain unverified.

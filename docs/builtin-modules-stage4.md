# Built-in modules: fourth slice

The fourth built-in slice adds `A1Fallbacks` and
`A2FOAnimatedHardpoints` to `A2FOExtensions.dll`. Together with the previous
seven modules, the core now provides nine existing module names. `A1Compat`
and `A2FOAnimations` remain separately selected modules.

## Selection and availability

The existing `[modules]` policy is unchanged: `requiredX` locks a module on,
`rejectX` excludes it, and inheritance and saved optional selections keep their
existing rules. Mods without a saved `[modules]` section retain legacy
load-all behavior. The original module names remain in the menu and saved
selections. The core prepares each non-rejected built-in once, including
initially inactive modules. Each new module is available only if its own
executable signature check and native hook setup succeed; neither adds a shared
dispatcher dependency.

Standalone DLL targets remain available for SDK compatibility, but releases no
longer require the `A1Fallbacks.dll` or `A2FOAnimatedHardpoints.dll` outputs.
Stale standalone copies are ignored during module discovery.

## Live behavior

While `A1Fallbacks` is off, its wireframe hook forwards the native resolver
result unchanged. When on, it keeps valid native wireframes and only fills in
when the entire native wireframe set is missing: first with the object's build
button, then the current owner's faction icon. This applies to selected-object
imagery and build-queue icons.

When `A2FOAnimatedHardpoints` is off, its hooks forward the original logical
node results unchanged. When on, gameplay hardpoint queries evaluate visible
matrix-animation channels. The first enabled query after an off/on cycle
invalidates cached poses, even for the same instance and animation time.
Metadata and channels remain available without repeated patching or unloading. Native side effects already
applied are not rolled back. The optional `A2FOAnimations` bridge remains
supported; no companion DLL update is required.

## Focused verification

The focused checks for this integration are:

- `make builtin-tests`
- `make builtin-native-tests`
- `make verify-builtins`

The local [validation receipt](../build/builtin-stage4-validation.json) records
the build, portable state test, 32 headless native fixture runs, export checks,
and artifact hashes. Synthetic coverage does not establish in-game visual,
save/load, or multiplayer acceptance. No deployment is included in this stage.

## Manual acceptance

After a separately authorized deployment, restart the game and verify the
original module names and selections. Toggle each module off and on with Save;
check required modules, rejected roots, inheritance, and legacy load-all
behavior. For `A1Fallbacks`, check native wireframes and build-queue icons, then
exercise missing-wireframe fallback selection. For animated hardpoints, check
null hardpoints, moving weapons, controlled animations through the optional
bridge, multiple instances, and re-enabling while paused. Save/load and
multiplayer acceptance remain unverified.

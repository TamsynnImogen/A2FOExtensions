# Built-in modules: first slice

The first built-in slice moves three optional modules into
`A2FOExtensions.dll`: `A2FOWreckage`, `A2FONormalWeaponTech`, and
`A2FOBuildTooltips`. Their code and registrations are prepared by the core;
they remain selectable by their existing module names in each mod's
`info.ini`.

## Selection and compatibility

The `[modules]` section supports `activeN`, `requiredN`, and `rejectN`
entries. `required` locks a module on; failed preparation is reported as
unavailable rather than silently claimed active. `reject` excludes it. Existing inheritance rules still apply:
required and rejected names accumulate through the selected mod-root chain,
while the active list comes from the last applicable `[modules]` section.
When no selected `info.ini` has a `[modules]` section, legacy load-all mode
enables installed modules, including these built-ins. Old standalone DLLs
with these names are ignored by discovery, avoiding duplicate registration.

The standalone module targets remain available for SDK and compatibility
testing. They are not release dependencies for these three features; the
release core contains their implementation. The startup proxy, helper DLLs,
and assets remain separate parts of the established startup chain.

## Startup and live menu behavior

The core prepares each non-rejected built-in once during startup, even when
its initial policy is inactive. This makes its fields and shared hook bridges
ready for later menu changes. A rejected built-in is not prepared. If required
signatures or shared dispatchers are unavailable, the built-in is unavailable
for the lifetime of that process.

The Mods-screen checkboxes show the current policy. Changes take effect only
when Save is pressed. For the exact running mod-root chain, the core validates
the whole requested set and then publishes each live enable flag atomically;
if a requested feature is unavailable, the policy is saved but no live flags
are changed. Changing the selected mod to a different root chain saves its
policy for the native mod-launch path or a new process. Non-built-in modules
also remain marked for the next launch.

Live changes only gate future callbacks. They do not unload built-in code,
remove installed patches, free retained state, undo wreckage already created,
or reverse shots already fired. A different selected mod may still require
the native launcher to start its process; this slice does not eliminate that
process path.

## Focused verification

The repository provides these focused targets:

- `make builtin-tests` exercises portable built-in state behavior.
- `make builtin-native-tests` runs synthetic native fixtures using Wine. Its
  private prefix defaults to `/tmp/a2fo-builtin-tests-wine` and can be changed
  with `A2FO_BUILTIN_WINEPREFIX`.
- `make verify-builtins` checks built-in entrypoint exports and runtime dependencies.
- `make build/A2FOExtensions.dll` builds the release core DLL.

These checks do not establish in-game acceptance. Record each command's actual
result when running them; this document does not claim a pass count.

## Manual acceptance

After deploying through the project's separately authorized release process,
restart once so the process loads the new core. In the running mod's Mods
screen of a test mod with the three optional features initially deselected,
verify they start off, then exercise on, off,
and on again across matches, saving each change. Check that destroyed craft
produce configured wreckage, an ordinary weapon obeys its technology-tree
requirement, and build tooltips show the expected time and applicable cost.
Also check that required and rejected entries remain locked, and that selecting
another mod keeps its saved choices isolated from the running mod. Confirm
that the other mod uses the native launch path where needed. Save/load and
multiplayer behavior remain pending acceptance.

No deployment or in-game acceptance is included in this stage document.

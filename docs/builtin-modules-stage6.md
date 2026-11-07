# Built-in modules: sixth slice

The sixth slice adds `A2FOInstantActionSettings` to `A2FOExtensions.dll`,
bringing the core-provided inventory to twelve module names. Its original
selection name is retained. The release no longer requires the standalone
DLL; its SDK compatibility build remains available. Stale standalone copies
are ignored by module discovery.

## Preparation and selection

The existing `activeX`, `requiredX`, `rejectX`, inheritance, saved optional
selection, and legacy load-all rules are unchanged. A non-rejected built-in
is prepared once, including when initially inactive. Availability requires
the existing supported Armada identity, valid FleetOps image, native
signatures, exception-handler registration, and complete native hook setup.
There is no new shared dispatcher dependency.

If preparation fails after installing some hooks, their code and state remain
resident, but all built-in repair paths are gated off. Installed hooks forward
their native gateways, and the exception handler returns continue-search.
The module is unavailable for that process. No hooks are removed or repatched
when the menu selection changes.

The implementation uses the existing native bridges, profile decoder, and
button-bounds helpers. No addresses, calling conventions, executable
admission rules, or profile formats were changed. The standalone build keeps
its existing behavior.

## Refit loading compatibility

`A2FORefitMenus` previously admitted the race lookup detour only when its
destination belonged to `A2FOInstantActionSettings.dll`. Its loading adapter
now also recognizes the exact core export
`A2FOBuiltinInstantActionSettings_RaceById`, retaining the executable-owner
and remaining native instruction checks. An unrelated address in the core
is not admitted. This keeps the loading player panel available even when the
built-in starts deselected or its preparation stopped after the race hook.

The staged milestone includes a companion `A2FORefitMenus.dll` build. Any
later deployment using Refit menus must update that DLL alongside the core.

## Live behavior

While deselected, the dialog, loader, file-reader, blob-reader, and race hooks
forward native calls. The label exception handler does not claim faults.
While selected, the existing Load Settings click fallback, missing-race guard,
scoped profile-path redirection and blob decoding, and exact-site corrupt-label
guards are enabled. The path override still comes from the existing settings
directory API; it is not a dependency on an optional module DLL.

Each built-in load scope records its activation generation. Turning the module
off and on invalidates a load already on the stack, so later reads in that old
scope remain native. A fresh load uses the new activation. Nested loads restore
their outer tracking context. A temporarily redirected path is restored after
the native file-reader call even if the policy changes during that call.

Live changes affect future callbacks. They do not undo an already loaded
setup, decoded profile data, cursor movement, or repaired label. A full restart
is the reliable way to reset previously applied native state.

## Verification and acceptance

Focused checks are `make builtin-tests builtin-native-tests verify-builtins`
and `make build/modules/A2FOInstantActionSettings.dll` for standalone
compatibility. The new native fixture uses allocated synthetic PE images,
native gateways, and the actual module and assembly bridges. It does not run
an engine entry point or automate windows or input devices. See the
[validation receipt](../build/builtin-stage6-validation.json) for actual
commands, outcomes, and hashes. Refit loading's native fixture separately
checks the standalone owner, exact core owner, foreign core address, and
changed native tail using data-only mapped binaries.

No deployment is included. After a separately authorized deployment and full
restart, manually check optional, required, and rejected module selections;
Load Settings through both supported UI routes; per-mod profile paths; profile
payload alignment; missing saved races; affected toggle labels; and Refit
loading player/race names and percentages. In-game
visuals, save/load acceptance, and multiplayer remain unverified.

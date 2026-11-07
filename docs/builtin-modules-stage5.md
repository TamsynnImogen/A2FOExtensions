# Built-in modules: fifth slice

The fifth slice adds `A2FOCameraControls` and `A2FOViewDistance` to
`A2FOExtensions.dll`, bringing the core-provided inventory to eleven names.
The two features retain their independent menu selections and global INIs.
The release no longer requires their standalone DLLs; SDK compatibility
targets remain available. Stale standalone copies are ignored by discovery.

## Preparation and policy

The existing `activeX`, `requiredX`, `rejectX`, inheritance, saved optional
selection, and legacy load-all rules are unchanged. Non-rejected features
are prepared once, even if initially inactive. Their assembly and diagnostic
symbols are isolated so both implementations can reside in the same core.
The existing executable identities, complete GOG file hash, relocation
requirements, checked patch sites, and data-permission checks are retained.

The INIs are read only during preparation:

- `Data/A2FOCameraControls.ini`: `[CameraControls] Enabled` and `PanSpeed`.
- `Data/A2FOViewDistance.ini`: `[ViewDistance] Enabled` and `MinimumDistance`.

Missing configuration, `Enabled=0`, `PanSpeed=1`, and
`MinimumDistance=0` are valid passive configurations: no patch is installed,
and selecting the module cannot override that INI choice. Invalid settings,
unsupported hosts, signature/relocation mismatches, and patch failures make
the affected feature unavailable. Availability is fixed for the process.

## Parser behavior and live changes

Preparation leaves each bridge disarmed until the core publishes the startup
policy. Save on the running mod's Modules screen changes the existing aligned
atomic bridge value, without modifying the assembly instructions, calling
C++ from those bridges, repatching, or unloading code. An unavailable request
leaves all current live selections unchanged. Another selected mod's roots
cannot alter the running mod's gates.

Camera panning scales a newly parsed native `FASTSCROLL_COEFFICIENT` only
while selected. View distance raises finite, nonnegative far-plane and culling
values to the configured minimum before native restoration-baseline capture.
When deselected, the bridges perform their original native store/load.
Mission scripts can still override culling afterward.

These gates affect future parser visits. They do not restore already parsed
engine values, and menu changes alone need not cause another parse. Restart
the whole process to apply changed INIs or reliably restore native values.
The existing diagnostic status structure is retained, with separate core
exports `A2FOBuiltinCameraControls_QueryStatus` and
`A2FOBuiltinViewDistance_QueryStatus`. An installed or armed bridge does not
prove the parser visited it or that a visible gameplay effect occurred.

## Verification and acceptance

Focused checks are `make builtin-tests builtin-native-tests verify-builtins`,
plus the retained standalone checks `make refit-modules refit-modules-test`
and `make refit-modules-native-test REFIT_WINEPREFIX=/absolute/disposable/prefix`.
The native fixtures map a reference executable as data and execute private
instruction harnesses; they never invoke its entry point or game loop.
See the [validation receipt](../build/builtin-stage5-validation.json) for
actual commands, outcomes, and artifact hashes.

No deployment is included. After a separately authorized deployment and full
restart, manually verify the eleven menu entries, optional/required/rejected
choices, both INIs, native and configured right-drag panning, maximum zoom,
fog/hidden objects, and mission-script culling restoration. In-game visuals,
save/load, and multiplayer acceptance remain unverified.

# A2FOInstantActionSettings

This optional native module restores **Load Settings** in Fleet Operations'
Instant Action setup screen. It keeps Armada's original host check, validation,
and UI update behavior.

Fleet Operations replaces the setup form and its button wrappers. On affected
installations, **Save Settings** still reaches Armada normally while the visible
**Load Settings** control misses the native click route. The module lets the
original dialog procedure handle every click first. It recognizes both the
child button's `BN_CLICKED` command and the native mouse route. If the native
`LoadSettings` routine was not called, it invokes that same routine once. The
mouse route can also repair missing wrapper bounds from the adjacent working
Save control.

Armada's loader otherwise opens only a bare `Settings.prf`, which misses Fleet
Operations' per-mod user directory. The module obtains the already-resolved
directory through the extension API and redirects only that rejected bare
profile open to `<SettingsDirectory>\\Settings.prf`.

Fleet Operations writes the text profile payload as
`setupDetails = <hex>`. Armada's binary-blob reader does not skip the space
after `=`, shifting every setting by half a byte. While the tracked Instant
Action load is reading its exact 824-byte setup structure, the module decodes
that one line with the correct byte alignment and then returns control to
Armada's existing validation and post-load processing. Other profile fields
and other uses of Armada's blob reader remain untouched.

A saved setup can also name a race that is no longer present in the active
mod. Armada's starting-unit pass normally dereferences the missing Race and
crashes. The module guards only that starting-unit lookup: valid races remain
unchanged, while an unavailable race receives an empty starting-object table
and is reported in `A2FOExtensions.log`.

Fleet Operations can also pass one of Armada's native
`ShellMultipleToggleButton` controls an option whose inline label bytes occupy
the field Armada expects to contain a string pointer. The module guards the
exact supported-binary label scans, clears only unreadable option labels, and
lets the affected option render blank instead of crashing while the Instant
Action form is constructed or cycled.

There are no ODF or INI commands. Select `A2FOInstantActionSettings` in the
mod's `[modules]` list. Diagnostics are written to `A2FOExtensions.log` under
the `A2FOInstantActionSettings` tag. The diagnostics report Ferengi and
technology-level values before and after Armada's profile reader, after the
enclosing setup-dialog dispatch, and when Fleet Operations refreshes its
Advanced Settings controls.

The runtime patch is restricted to the supported ArmadaL.exe identity and
known function signatures. Unsupported binaries leave the module resident but
disable the repair.

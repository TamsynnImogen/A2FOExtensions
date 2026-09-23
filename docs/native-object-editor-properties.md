# Native object-editor properties

Requirement 1 extends Fleet Ops' existing `TGameObjectDialogForm` /
`TNextInspector` property grid, with the extra rows under `Craft`. They appear
in both Basic and Advanced views. The separate legacy Armada dialog is also
supported, but hooking that legacy dialog alone does not affect Fleet Ops'
inspector. The existing ship-name and system-health rows remain in place.

## Fields

- Captain: an independent per-ship text override.
- Registry: an independent per-ship text override.
- Forward, aft, port and starboard shields: current and maximum for each facing.
- `Reset captain to ODF` and `Reset registry to ODF`: one-shot checkboxes that
  clear the corresponding override on OK. The legacy dialog instead provides
  `Use ODF name list` checkboxes.

Changing an identity field selects a manual override; leaving an automatic
name unchanged does not pin it unnecessarily. An explicitly blank manual
entry is allowed. Text is limited to 511 bytes in the game's existing ANSI
text encoding. Reset checkboxes take precedence over their text fields.

Shield editing requires an existing `directionalShields = 1` class and the
updated `A2FODirectionalShields.dll`. It does not enable directional shields on
other classes. Each maximum must be positive; current must be between zero and
that maximum. Non-finite numbers, trailing junk and overflowing totals are
rejected. Changes are per instance, not changes to the ship class or its ODF.

For a directional-shield ship, the four facing pairs are authoritative and
the native shield totals are restored from their sums after Apply. Use the
facing rows rather than editing `curShields` / `maxShields` aggregates. The
legacy dialog makes its aggregate shield controls read-only. Other native
properties remain editable. OK validates and applies the draft; Cancel
discards it. Only the legacy dialog requires a nonempty native object label.

## Persistence and compatibility

`A2FOCraftIdentity.dll` owns the editor controls and optional version-1
`A2FOEDIT` record. The record stores identity override flags, the two strings,
and all four current/maximum shield pairs. Active directional-shield values
are refreshed at save time, including damage and recharge since editing.

The metadata is emitted through the game's byte writer immediately before
Craft's first scalar field. The reader supports native text, binary and
tagged-binary streams. It only advances the input cursor after a complete,
recognized, valid record. Old maps without a record retain their original
read position and use the existing identity and shield defaults.

Maps/savegames containing the new record require the updated
`A2FOCraftIdentity.dll` to load. They are not backward-compatible with an older
DLL or an unextended game. Keep original maps before saving with this feature.
Install the updated identity and directional-shield DLLs together to restore
all eight shield values. This implementation does not change the map-file
format globally or claim that older game builds can skip the new record.

The save/load patches are internal call-site patches, not detours at
`Craft::Save` or `Craft::Load`. The ammunition persistence hooks in
`A2FOEnergySystems` retain those entry points. Shield restoration uses the
shared Craft post-load lifecycle after the directional-shield module resets
its runtime stores. Cleanup removes per-object overrides.

## Native hook sites

Fleet Ops inspector virtual addresses at image base `0x5A800000`:

- `0x5A9B90D8`: build Craft properties; append extension rows after native rows.
- `0x5A9BAF34`: OK button; validate before the native Apply and Close calls.
- `0x5A9B88C8`: form destruction; release the current draft after native cleanup.
- `0x5A9B8904`: native property-descriptor builder, called through a Delphi ABI
  bridge rather than patched.

Inspector installation and per-object row creation are logged in
`A2FOExtensions.log`. An unsupported inspector no longer fails silently.

These are virtual addresses for the supported ArmadaL image, before
subtracting its `0x400000` base:

- `0x501560`: native object-dialog procedure, six-byte prologue hook.
- `0x500FE0`: native object-dialog destructor/commit, six-byte prologue hook.
- `0x4C299D`: first scalar writer call inside Craft save.
- `0x4C2359`: first scalar reader call inside Craft load.

Persistence and inspector signatures are checked before installation. A
partial installation keeps the affected hooks in pass-through mode. The
optional legacy dialog is independent of the Fleet Ops inspector. The normal
game renderer, cloak pipeline and weapon effects are outside this change.

## Validation status

Both DLLs have been cross-built and deployed to Fleet Ops Roots after backing
up the installed pair under `Data/.deployment-backups/`. The deployed files
were compared byte-for-byte with the build outputs. PE checks confirmed the
32-bit format, required bridge exports and system-only runtime dependencies.

The existing craft-identity and directional-shield host checks passed. Focused
object-editor checks also passed for text, binary and tagged-binary save
records, old records without a prefix, malformed/truncated records, invalid
shield values, and explicit blank identity overrides.

In-game behavior remains untested. Before using the feature on working maps,
check OK/Cancel, independent ships of the same class, old-map loading, and
save/reload of manual identity plus unequal facing values. Include a map with
ammunition overrides to exercise coexistence with `A2FOEnergySystems`.

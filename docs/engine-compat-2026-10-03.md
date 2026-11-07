# ArmadaL October 3 compatibility and reviewed profiles

The new executable stopped A2FOExtensions before hooks were installed because
its `.test` CRC changed. The timestamp and image size stayed the same.

## Binary audit

September 30 SHA-256: `470bf8f4b0196e22fe179d3845cab671e74d9af19cf90a7dafed0b0bbd394318`.
October 3 / installed SHA-256: `3423ad8f04dc4a9eafe812fea81c6c53fea0df640b10bcc975902e207f834306`.
Installed FleetOpsHook SHA-256: `d2a0829e87aaa927e27146b90be61fc6359c6cbf3d81aed2c644019dd375b9b6`.

The downloaded archives were extracted into `.build/engine-compat-20261003/`;
the old archive matches the recorded September 30 identity and the new archive
matches the installed image. File comparison finds **43 changed bytes**, all
in `.test`. Headers, `.text`, `.rdata`, initialized `.data`, imports, resources
and relocations are byte-identical. FleetOpsHook is unchanged.

At Armada RVA `0x4045af`, the native rotation path now jumps to `0x404730`.
That cave compares `[eax+0x70]` with `0x52535250`, skips to the existing
`0x40462e` continuation on equality, otherwise replays the displaced load
and rejoins at `0x4045b5`. Bytes through `0x40475f` also clear previously
present cave bytes. This is static instruction evidence; the meaning of the
marker and all upstream gameplay consequences have not been independently
established. Searches of runtime core/sdk/module sources found no direct
reference into these changed ranges. Our extension station rotation remains
inactive for this family, avoiding competing native placement/order formats.
The core and existing hook sites in `.text` retain the previous file bytes.

## Compatibility mechanism

`sdk/include/a2fo_supported_armada.hpp` retains existing built-in profiles.
For a new image in the supported Roots layout, it can also read
**`A2FOArmadaProfiles.txt` beside that Armada executable**. The shipped record
approves this exact October 3 image and declares the existing
`roots_native_rotation` capability. StationRotation and CollectiveXP use a
shared capability predicate instead of testing one release date.

The external path requires the fixed x86 PE family, section geometry, entry
point, preferred base, timestamp, section alignment, `.rdata` virtual CRC,
raw sizes/offsets/flags, and exact CRCs of the full first `0x1000` file bytes
and **all seven complete raw sections**, including padding. Checks use file
bytes because FleetOpsHook may have already detoured loaded code. A missing,
stale, truncated, malformed or unknown-capability profile rejects the image.
The profile format is bounded to 8 KiB / 32 rows and contains no hook addresses,
patch instructions or ABI overrides. Header reads now validate memory bounds.
The file is a local approval record, not a cryptographically signed attestation.
Existing canonical/normalized legacy validation behavior is retained.

Future **reviewed updates within this family need only an additional profile
record**, without recompiling the core and ten consumers. This reduces release
maintenance; it does not automatically establish ABI compatibility. Changes to
the layout, `.rdata` contract, native capabilities or extension hook behavior
still require engineering work. Per-hook signature validation remains active.
Do not generate-and-install a profile blindly or reuse a native-rotation
capability for an engine that does not supply that behavior.

## Future update workflow

Use the stdlib-only tool from the repository root:

```sh
python3 tools/armada_compat_profile.py inspect /path/to/new/ArmadaL.exe
python3 tools/armada_compat_profile.py compare /path/to/previous/ArmadaL.exe /path/to/new/ArmadaL.exe
python3 tools/armada_compat_profile.py profile /path/to/new/ArmadaL.exe --output /path/to/proposed-profile.txt
```

The last command generates a **proposal**, never installs it. Review changed
instructions and callers, direct runtime references, hooks, ABI/data assumptions,
native rotation ownership, and the actual paired FleetOpsHook. Run identity,
native-rotation and XP preflight checks with the proposal beside an isolated
copy of the new engine. Then append the reviewed row to the installed profile,
keeping one `A2FO_ARMADA_PROFILES_V1` header. Record hashes/evidence and retain
rollback copies. Fully restart the game and manually verify affected behavior.
The `# SHA-256` comment records binary identity; CRC rows drive runtime matching.

## Validation and deployment

- Explicit MinGW build of core, renderer helper and ten identity consumers passed.
  Existing renderer thiscall/narrowing/unused-function warnings remain.
- `roots_profile_smoke.exe`: **28 mapped-image checks** passed, plus capability
  and malformed-memory checks. This includes each complete section/header
  mutation, malformed/stale/removed approvals, excessive rows, and accepting a
  second approved resource-only fixture without rebuilding the test executable.
- Existing September 5/30 identity and mixed-profile regressions: **24 checks
  passed**. The canonical image fixture was unavailable and explicitly skipped;
  the clean Armada2.ldl is a different family and was not counted as canonical.
- Actual rebuilt StationRotation DLL: ModuleInit succeeds, native diagnostic
  present, **zero extension hook/patch requests** on the new approved engine.
- Actual rebuilt CollectiveXP DLL: positive preflight and mutated-award rejection
  pass using the installed FleetOpsHook. Hooks are recording callbacks; no
  native engine code is run or patched by these tests.
- DLL load/export smoke passes for core and all ten updated modules.
- Python tool tests: **6 passed**. Independent read-only review found no confirmed
  defect. Existing unrelated workspace edits/deletions were preserved.
- Full legacy `make test` was not run: tracked legacy test sources are already
  absent in this checkout. Focused checks have separate targets.
- Tests ran with DISPLAY and WAYLAND_DISPLAY unset. No GUI/input automation
  was used. Headless checks establish neither gameplay nor multiplayer behavior.

Source: `/home/tamsynn/A2FOHookExtensions`; builds: `build/`; deployment:
`/home/tamsynn/NVMeData/Fleet Ops Roots/Data`.
Build ID: `20261003-roots-reviewed-compatibility-profiles-1`.
Core SHA-256: `94754f2225e2eb2122c890f2803930b6e891cd159c4891f01ce4008a4c6f7ac8`.
All 13 deployed artifacts were atomically replaced and their hashes verified.
Complete paths/hashes are in `engine-compat-2026-10-03-deployment.json`.
Logs and evidence are in `.build/engine-compat-20261003/`.
Portable package: `dist/A2FOExtensions-engine-compat-20261003.zip`.
The executable and FleetOpsHook were not replaced by this deployment.

Rollback backup: `/home/tamsynn/A2FOHookExtensions/.deployment-backups/engine-compat-20261003-112749`. It preserves engine identities, prior live logs,
DLLs and the edited source files with a preservation manifest. After closing
the game, restore the corresponding DLLs/helper from `deployed/` and remove
`Data/A2FOArmadaProfiles.txt` (newly added by this deployment). Preserve any
new log before relaunch, since the extension log is overwritten on each launch.

## Manual acceptance pending

1. Fully close and reopen Roots and load the same mod that failed.
2. Inspect `Data/A2FOExtensions.log` for
   `approved Roots compatibility profile accepted`, module initialization,
   and `Using Roots native station rotation` if that module is enabled.
3. Start a skirmish. Exercise normal UI, selection, production and rotated
   station placement; verify native rotation and save/load.
4. If the faction uses collectiveXP, verify awards/promotion and save/load.
   Check videos/shell display if enabled. Record any crash or signature mismatch.

Do not treat deployment hashes or headless tests as in-game acceptance.

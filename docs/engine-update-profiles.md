# Engine update profiles

The external profile files let a reviewed engine update use the existing
compatibility code when its PE layout and hook assumptions remain within the
supported family. They do not make arbitrary future Armada or Fleet Ops builds
compatible, discover replacement addresses, or supply hook signatures. Every
installed hook still checks its own exact instruction bytes. Changes to code,
addresses, calling conventions, or object layouts can still require source
changes and a new DLL.

## Oct 23, 2026 reference pair

The staged Oct 23 Armada executable has SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de` and the
Fleet Ops DLL has SHA-256
`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`. The
Fleet Ops DLL is byte-identical to the Oct 12 DLL. The Armada executable differs
from the Oct 12 reference in 246 bytes across 23 spans; those changes include
two `.rdata` float constants and native build-order/rendering code. Its
timestamp, image size, and fixed section geometry remain unchanged.

The staged Armada profile row is:

```text
A2FO_ARMADA_PROFILES_V1
roots_native_rotation 3C4C76BD 00405000 A3656514 17A62CB7 2E8496AD 3049EF39 18C427CE 5EF3B4CF 9EB18CA0 E7A4061B
```

The row binds the timestamp, `SizeOfImage`, CRC-32 of the first `0x1000` file
bytes, and raw CRC-32 values for all seven sections in fixed order: `.text`,
`.rdata`, `.data`, `.idata`, `.rsrc`, `.reloc`, and `.test`. The parser also
requires the exact existing PE header fields, section geometry and section
characteristics. A matching profile grants the existing Roots native-rotation
identity only; it cannot encode addresses, signatures, or ABI changes. Built-in
legacy identity checks retain priority.

The staged Fleet Ops profile row is:

```text
A2FO_FLEETOPS_PROFILES_V1
roots_fleetops 51F6475C 00322000 002E7200 7CD6661E
```

This binds the fixed Fleet Ops PE layout plus timestamp, image size, file size,
and whole-file CRC-32. The data file is named `A2FOFleetOpsProfiles.txt` and
must sit beside `FleetOpsHook.dll` (the game root). The Armada file is named
`A2FOArmadaProfiles.txt` and sits beside `ArmadaL.exe` in the game's `Data`
directory. Each profile file may contain up to 32 rows after its version
header; comments beginning with `#` are allowed.

## Inspecting and proposing profiles

The helper is read-only with respect to game installations. It prints PE
details, compares two files, or writes a proposed profile to the explicit
`--output` path. For example:

```sh
python3 tools/armada_compat_profile.py inspect path/to/ArmadaL.exe
python3 tools/armada_compat_profile.py compare old/ArmadaL.exe new/ArmadaL.exe
python3 tools/armada_compat_profile.py profile path/to/ArmadaL.exe --output /tmp/A2FOArmadaProfiles.proposed.txt
python3 tools/armada_compat_profile.py fleetops-profile path/to/FleetOpsHook.dll --output /tmp/A2FOFleetOpsProfiles.proposed.txt
```

`profile` accepts only the fixed Armada Roots PE layout; `fleetops-profile`
accepts only the fixed Fleet Ops layout. Both outputs are proposals and state
that a manual binary and hook audit is required. Review the binary diff and
hook assumptions before copying an approved row to its game-side profile file.
Generating a profile does not install it, approve the binary, validate runtime
behaviour, or certify an update for release. The Oct 23 build passed 50 new
mapped-image profile checks, 28 existing profile checks, legacy identity
regressions, 112 extension fixture cases, and seven Refit checks against the
new input pair. These are headless checks; in-game acceptance remains pending.
See `build/engine-compat-20261023/validation.json` for artifact hashes,
deployment status, and rollback information.

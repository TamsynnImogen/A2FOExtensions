# Roots September 30 engine compatibility — 2026-10-01

The updated Roots installation at `/home/tamsynn/NVMeData/Fleet Ops Roots/`
was rejecting Armada before extension hooks loaded. Its last failure log said
`ArmadaL.exe version mismatch (timestamp=3c4c76bd, image=00405000)` and
`No hooks installed`. The PE timestamp/image size still matched the prior
September engine; changed code fingerprints caused the rejection.

## Changes

- Added the exact `janb_20260930` profile in `sdk/include/a2fo_supported_armada.hpp`,
  shared by core and the optional modules that validate Armada.
- Required matching `.text`, `.test`, and initialized `.data` fingerprints for
  both September profiles. `.rdata`, section layout, entry point, preferred
  base, timestamp and image size checks remain. Altered/mixed images reject.
- Core logs acceptance of the September 30 build. Build identifier:
  `20261001-roots-september30-engine-compat-1`.
- A2FOStationRotation returns successfully without installing hooks on this
  exact profile. The upstream engine already changes construction positioning,
  footprint handling and rotation bindings (`rotate_station`,
  `rotate_station_left`). This avoids competing rotation/order formats.
- Collective XP accepts the new FleetOpsHook code CRC only with the new Armada
  profile, and still requires every award/rank/team persistence/reset signature.
- Rebuilt core, renderer helper and eight affected optional modules; deployed
  with atomic file replacement and verified matching source/deployment SHA-256.
  Existing unrelated checkout edits/deletions were preserved.

## Binary evidence

| Image | SHA-256 | Relevant CRC-32 |
| --- | --- | --- |
| New ArmadaL.exe | `470bf8f4b0196e22fe179d3845cab671e74d9af19cf90a7dafed0b0bbd394318` | `.text 98ed8dbd`, `.rdata cd47b19c`, `.test 6592de0f`, `.data 3049ef39` |
| Previous September ArmadaL.exe | `63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36` | `.text 5458da70`, `.rdata cd47b19c`, `.test 785c09e4`, `.data 830d0160` |
| New FleetOpsHook.dll | `d2a0829e87aaa927e27146b90be61fc6359c6cbf3d81aed2c644019dd375b9b6` | `.text ca18aa0c` |
| Previous September FleetOpsHook.dll | `0843058788c6ffd1138d69871e2d9b1911bb6be8ee06db42f59f56b9a4494a97` | `.text 5e621788` |

Armada section RVAs and headers are unchanged. Comparing the pinned September
references found 410 changed bytes in `.text`, 45 in initialized `.data`, and
3,946 in `.test`. Direct source RVA comparison flagged the station rotation
sites (including `0x031500` and several footprint call sites); other nearby
HybridBuild placement sites retain their checked instructions.

FleetOpsHook has 22 changed code bytes: a cave at RVA `0x137e3c` and a jump plus
field-offset change at `0x1ec8e5`. Disassembly shows a resource-pointer null
check. All six Collective XP award/rank/team persistence sites, and Armada's
team-reset site, remain byte-identical. This is a bounded signature/static
compatibility audit, not a full engine behavior audit.

## Validation

- Explicit MinGW build targets for all ten changed deployment artifacts passed.
  Existing thiscall/narrowing/unused-function warnings remain in renderer code.
- `roots_engine_identity_smoke.exe`: 25 checks passed, exit 0: new/old/canonical
  acceptance, mutation rejection for both September profiles, and mixed
  code/patch/data rejection. The final source was rebuilt and rerun.
- `roots_native_rotation_smoke.exe`: actual rebuilt DLL plus mapped new engine,
  exit 0; ModuleInit succeeds, native diagnostic present, zero hook/patch calls.
- `roots_collective_xp_smoke.exe`: actual rebuilt DLL plus mapped new engine and
  FleetOpsHook, exit 0; four inline-hook and three jump requests match real
  signatures. Changing the mapped award byte prevents all hook/patch requests
  and leaves the module in stock/pass-through mode.
- DLL load/export smoke passed for core and all eight updated modules. Modules
  remain loaded for process lifetime, matching the game. An initial temporary
  probe that unloaded the core with its deferred worker still running failed;
  that harness was corrected, without changes to core lifecycle behavior.
- Independent review found the initial missing old-profile `.data` pairing;
  it was fixed. Reviewer independently reran rotation and XP tests, both exit 0.
- Full legacy `make test`/`make smoke` were not run: many required tracked test
  sources were already deleted in this working tree. New focused tests have
  separate Make targets and do not restore unrelated deleted files.

The smoke tests map game images with `DONT_RESOLVE_DLL_REFERENCES` and use
recording hook callbacks. They do not execute the engine or test gameplay.
No GUI/input automation was used.

Build/load/identity output is in `roots-engine-compat-2026-10-01-evidence/`.

## Deployment and rollback

Deployment time (UTC): `2026-10-01T07:52:15.257771+00:00`.

- Source: `/home/tamsynn/A2FOHookExtensions`.
- Build: `/home/tamsynn/A2FOHookExtensions/build`.
- Deployment: `/home/tamsynn/NVMeData/Fleet Ops Roots/Data`.
- Core SHA-256: `8d6c7d0425304d846d9599596cbda540238c64db44e92ec9e78e0646925f1850`.
- Complete artifact hashes, paths and timestamps:
  `roots-engine-compat-2026-10-01-deployment.json`.
- Pre-change DLLs, logs and edited source backup:
  `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/rollback/roots-engine-compat-20261001-014654` (66 files with a preservation manifest).

To roll back this compatibility deployment after closing Roots, restore the
corresponding ten files from the backup's `deployed/` tree. Keep the engine
binaries in place; this deployment did not replace either engine file.

## Manual acceptance still pending

1. Fully close and reopen Roots. Load the same mod that failed before.
2. Check `Data/A2FOExtensions.log` for September 30 acceptance and module
   initialization, plus `Using Roots native station rotation`.
3. Start a skirmish. Place/build a rotated station with the configured native
   rotation bindings; verify its footprint/placement and save/load behavior.
4. For a faction with `collectiveXP` enabled, verify pooled awards, promotion,
   and save/load. Factions without opt-in should keep their existing behavior.

A deployed hash proves freshness. Headless checks do not establish in-game,
save/load or multiplayer acceptance. Copy the live log before another launch,
since each launch overwrites it.

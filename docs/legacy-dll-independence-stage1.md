# Legacy DLL independence: first implementation batch

Date: 30 October 2026. **Implemented and headless-tested in an isolated trial; manual game acceptance remains pending. The live Fleet Ops Roots installation is unchanged.**

Selected work: `Win2kDisableTaskSwitch.original.dll`, game-local `imagehlp.dll`, and `Dxver.dll`. This is a first implementation milestone, not a claim that every installation can already delete all three files or that A2FOExtensions is now pure Rust.

## Results by DLL

| Selected file | Current result | Remaining gate |
| --- | --- | --- |
| `Win2kDisableTaskSwitch.original.dll` | Replaced its supported startup duties with a 4,608-byte Rust proxy. The trial contains no original DLL in its Data tree. Native console tests verify direct FleetOpsHook load before core attachment, deferred initialization, Unicode paths and failures. | Actual startup and early-hook behavior in FOtest/test. The `Win2kDisableTaskSwitch.dll` compatibility filename remains required. |
| Local `imagehlp.dll` | Base and Fleet Ops 4.0 executables now import `UnDecorateSymbolName` from the OS `DBGHELP.dll` instead. The trial's Classic-mod ImageHlp copy is quarantined outside the runnable tree. No engine code or function-pointer slots changed. | Real-game symbol/error paths and mod launch selection. This uses an external OS implementation, not a new Rust undecorator. |
| `Dxver.dll` | The trial is checked to contain none. FOtest/test and their selected parents were already staged without Dxver, so no fictional removal credit is claimed. The staging utility can quarantine it in a future candidate. | Full caller/non-use proof for the older FCM and Orb of Time copies. Those live copies remain untouched; no guessed version-check stubs were introduced. |

## Trial and rollback locations

- Runnable trial: [`build/legacy-dll-independence-20261030/test-installation/`](../build/legacy-dll-independence-20261030/test-installation/).
- Trial executable: [`Data/ArmadaL.exe`](../build/legacy-dll-independence-20261030/test-installation/Data/ArmadaL.exe).
- Quarantined trial files: [`test-installation.retired/`](../build/legacy-dll-independence-20261030/test-installation.retired/). They were moved only from the copied trial, not from the source or live game.
- Copy/source-integrity receipt: [`test-installation.receipt.json`](../build/legacy-dll-independence-20261030/test-installation.receipt.json).
- Final static/headless trial checks: [`trial-validation.json`](../build/legacy-dll-independence-20261030/trial-validation.json).
- Separate FOtest/test user-settings names: [`settings-isolation.json`](../build/legacy-dll-independence-20261030/settings-isolation.json).

The source was the existing independent stage-17 acceptance copy, refreshed with its later runtime-support repairs. The new trial contains 29,103 regular files, copied without hardlinks. All 29,105 source files matched their hashes before and after staging. Two copies were quarantined: the original startup DLL and Classic-mod ImageHlp. Five supplied files were replaced in the new tree: proxy, paired core, two executables and the exact base compatibility profile. Two trial-only info.ini settings-directory edits are separately recorded. Assets, saves, mail consent and live settings were not edited.

Rollback for this milestone is to use the untouched source/live installation. Do not mix the new proxy with an older core. If restoring the copied trial manually, restore its proxy/core/executable/profile set together from the source receipt, and restore the two quarantined copies; the supplied preparation tool intentionally refuses to overwrite an existing nonempty trial.

## Startup implementation and pairing

[Rust proxy source](../rust/a2fo_startup_proxy/lib.rs), [exports](../rust/a2fo_startup_proxy/exports.def), [builder](../tools/build_rust_startup_proxy.py).

The normal production Make target for the startup proxy now uses Rust. The old C++ source remains as an excluded comparison/reference input; no production proxy rule compiles it. The new DLL has the same two decorated exports and ordinals as the current proxy, imports only Kernel32, uses explicit module-relative UTF-16 paths, and contains no `.original.dll` load path. Whole-program Rust optimization removes unused compiler/runtime dependencies; no fake exception-personality stub or C++ runtime was linked into this proxy.

Its current startup chain is:

```text
Armada -> Win2kDisableTaskSwitch.dll [Rust, 4,608 bytes]
               -> ../FleetOpsHook.dll
               -> A2FOExtensions.dll [early attach, deferred worker]
```

This retains the established early nested-load contract. It does not claim a new loader-lock-safe bootstrap architecture or removal of the proxy filename; that requires the separate executable/startup migration described in the [viability report](dll-consolidation-viability-2026-10-29.md).

Review found that the existing `A2FO_Initialize` can rethrow C++ exceptions. The new Rust proxy therefore resolves **only** `A2FO_InitializeNoUnwind`, and rejects an older core lacking it. The paired core uses [a contained native boundary](../core/initialize_boundary.hpp) around its still-native initializer, returning failure and resetting the initialization admission state on a caught exception. No foreign C++ exception crosses the new Rust call boundary. This does not replace general SEH/fault handling or complete the core Rust migration.

The fresh core preserves all 197 previous exports, names, ordinals and forwarders, and appends `A2FO_InitializeNoUnwind` at ordinal 198. Its DLL dependency set is unchanged. The linker checker pins the original 197-entry contract byte-for-byte and permits only this exact extension; it does not merely accept an arbitrary export count.

The core build reused 493 hash-verified native objects from the existing accepted candidate and replaced the core entry translation unit, then freshly linked through Cargo's Rust DLL root. This is a mixed-language incremental build, not a clean pure-Rust build or a repetition of the entire historical acceptance matrix.

## ImageHlp import migration

[Hash-gated patcher](../tools/retarget_imagehlp.py), [mutation/guard tests](../tests/test_retarget_imagehlp.py).

Only the two explicitly pinned input executable hashes are accepted. The patcher verifies the single named `UnDecorateSymbolName` import, changes its descriptor's DLL-name bytes to `DBGHELP.dll`, updates the PE checksum, and proves every other byte is unchanged. It preserves executable sections, resources, section geometry, imported function names and IAT locations. Unknown inputs, unexpected imports, output overwrites, symlinks and paths outside private build output are rejected.

The base Roots profile was extended with the exact new header and complete-section fingerprints, retaining the previous rows. Engine validation was not weakened. Native positive/negative profile tests cover the changed base image. Separate data-only mappings of the final staged executables confirm the base's approved Roots identity and the parent's existing normalized identity. Microsoft's [UnDecorateSymbolName contract](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-undecoratesymbolname) documents DbgHelp as the platform provider.

System ImageHlp versus system DbgHelp produced matching return/output behavior for 132 console cases covering observed C++ symbol forms, flags, small/zero/full buffers and invalid names. This does not prove every old native ImageHlp version or arbitrary optional API equivalent. The goal here is removing the local file requirement for the selected game import; Windows/Wine platform DLLs remain external services.

## Evidence and limits

- Seven Rust host tests: load order, bounded/reentrant wait, failed init publication, missing safe core contract and Unicode/bounds/path behavior.
- Twenty-three Python checks: staging/source preservation, traversal/symlink/low-space controls, multiple image/core/profile replacement, exact import rewrite and negative controls, and pinned export-extension guards.
- Native console matrix: eleven successful runs covering before/current startup, original-free startup, Unicode and unrelated working directory, missing FleetOps/core, old core refusal, initializer failure, a caught native C++ exception, OS undecoration comparison, the shared no-unwind boundary and the actual fresh core's worker/refusal/retry behavior in a fake host.
- Fifty data-only mapped-image/profile checks pass on the changed base image and pinned FleetOpsHook, including stale approval, code/section mutations, malformed rows and structural failures. Game entrypoints are never run by those checks.
- Two additional data-only checks accept the final staged base and parent executables under their intended compiled-in/profile identities.
- Runtime-support preflight passes for scene provider, menu configurations and the reporting helper; no report/email was sent.
- Independent review checked the final `candidate-v4` proxy's current source pins, DLL hash and native receipt. Earlier v1–v3 artifacts are historical attempts, not the selected candidate.

The first native baseline probe attempted to call the inactive stock low-level callback directly and failed. The stock callback's plain-return ABI was already identified as ambiguous. The final baseline comparison exercises startup through SetHookID; the replacement's expected stdcall return is tested separately. Both pinned game images retain their unconditional branches bypassing legacy keyboard-hook setup, checked byte-for-byte. No keyboard hook, input event, window automation or game launch was performed. Compatibility with other keyboard-hook callers is not claimed.

Receipts: [native matrix](../build/legacy-dll-independence-20261030/native-runs-v2/receipt.json), [profile matrix](../build/legacy-dll-independence-20261030/native-runs-v2/engine-profiles.json), [proxy build](../build/legacy-dll-independence-20261030/candidate-v4/Win2kDisableTaskSwitch.dll.receipt.json), [core Cargo link](../build/legacy-dll-independence-20261030/core-build/rust-dll/receipt.json), [support preflight](../build/legacy-dll-independence-20261030/runtime-support.json).

## Selected hashes

| Trial file | SHA-256 |
| --- | --- |
| `Data/Win2kDisableTaskSwitch.dll` | `5a34b4541d25c73b81a3307da97e969d0f37906c44e5105672f2db9d7043f12d` |
| `Data/A2FOExtensions.dll` | `8ae923471311aa9794fe6ac2ae5557a3f637c78b99e586ad3a2c9541f01a22ff` |
| `Data/ArmadaL.exe` | `41ee2e6dd877b03fdf2b48eb268fbab28c521b5ad02ac2f688856d7d7eefdaf2` |
| `Data/Mods/Fleet Ops 4.0/ArmadaL.exe` | `370a93632530fa19d0ba14e522e118c637d5611bf406e72cad226fb68b26fb45` |
| Root `FleetOpsHook.dll` (unchanged) | `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11` |

## Manual acceptance before deployment

1. Close the game completely. Launch **this trial's** `Data/ArmadaL.exe` using the normal Wine/PortProton method; do not select the original Roots executable by mistake.
2. Test FOtest and test with full restarts. They have distinct `A2FO Legacy DLL Trial 20261030 ...` settings names so the earlier acceptance settings are preserved.
3. Check new startup logs for direct FleetOpsHook attachment, core startup, supported-engine approval and absence of missing-original/helper errors. The directory contains copied historical logs too; use fresh timestamps/content from the new run.
4. Verify mod selection/relaunch, graphics options, emissives, rendered 3D/refit menus, movies, an Instant Action game, save/load and normal shutdown. Match the trial hashes if a fault appears and preserve its fresh logs.
5. Keep multiplayer pending until a second matched-build peer is available. Keep FCM/Orb Dxver removal pending until those variants receive their own isolated caller/runtime validation.

No live deployment or deletion is part of this milestone. Passing these headless checks makes the trial reviewable for manual testing; it does not establish rendering, gameplay or multiplayer acceptance.

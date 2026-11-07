# Rust rewrite acceptance ledger

This ledger separates implemented source changes, headless checks, and real
Armada/Fleet Operations acceptance. It is not a declaration that the rewrite is
complete. The current main DLL still incorporates substantial native code.

| Requested phase | Current evidence and remaining work |
| --- | --- |
| 1. Rust DLL/runtime and feature state | Rust DLL entry/runtime work is staged. Stage 11 verified two bounded feature state owners; the remaining feature and renderer ownership work is incomplete. |
| 2. Native platform and renderer | Stage 12 verified bounded checked memory patches and D3D11 compressed uploads. Broader Win32, GDI, COM, D3D8/D3D11 resources and engine-memory work remains. |
| 3. Hooks and assembly | Stage 13 verified 48 external bridge families, three FP operations and four patch profiles. Native callback bodies, compiler ABI adapters and pointer-slot hooks remain. |
| 4. MMM | Stage 14 replaces the production loader and handoff state. Worker/Rule, Lua 5.1.5, Luabind, binding state, archives and save/load remain. See [MMM contract](rust-mmm-migration.md). |
| 5. FFmpeg | Stage 15 has a production Rust media backend, including audio, timing, seeking and shutdown. FFmpeg is absent from the production link. Supported codecs and memory-host/performance evidence are documented in [media evidence](rust-media-replacement.md); visible playback, audible sound and real device acceptance remain open. |
| 6. Remove native dependencies | Stage 16 removes the hook/decal C++ facades and three unnecessary explicit link libraries. The production MMM shim was already removed. Active STL, exception, COM/GDI/D3D and MMM consumers remain; see [retirement record](rust-adapter-retirement.md). |
| 7. Acceptance | Stage 17 runs combined checks and reconciles the held census against frozen completion evidence. Manual rendering/gameplay/save/load/multiplayer acceptance remains open. The test payload is staged; the selected installation is unchanged. |

## Census

`rust/expanded_migration_inventory.json` now credits **151/151 portable
implementation units**: the original 116 records are unchanged and the 35 held
completions are `verified_stage17`. This is not a whole-rewrite percentage.
`rust/census_reconciliation.json` pins the archived strict 116-unit result,
all ten held completion checkpoints, current product sources, six later source
changes, required Rust symbols, DLL link/export evidence and combined checks.
The frozen checkpoint flags remain historical; no old receipt was overwritten.
Retired hook/decal source IDs retain their Rust owners and the denominator stays
151. The legacy 152-translation-unit ledger is also preserved as history.

The earlier checker failure came from old receipts referring to mutable build
archives and later source changes. The current checker verifies frozen evidence
and the current candidate instead of changing those old pins. It validates
selected source members against the archived manifests, rather than merely
trusting the existence of a completion JSON. The Cargo-built DLL and separately
built fixture archive are bound through the same pinned Rust core sources.

```sh
python3 tools/verify_rust_expanded_census.py \
  --dll build/rust-migration-stage16/adapter-cleanup/candidate/A2FOExtensions.dll \
  --minimum-percent 100 \
  --receipt build/rust-migration-stage17/acceptance/census-verification.json
```

The combined host gate preserves the working tree's pre-existing test deletions;
40 historical tests are recovered and hash-verified only inside a private
overlay. The native gate composes 30 passed groups with identical 906 input pins,
using current source, documented mock layout repairs and current support
objects. Its record is not one uninterrupted invocation. Stage 11 ownership
proof remains bounded to five selected core suites and 15 executions. The
combined Rust gates retain media lint debt and unsupported DLL host/test-harness
attempts as advisories, never as passed tests. Census negative tests reject
changed membership, rewritten history, missing groups and input drift.

## Stage 17 acceptance reference

The selected installation is `/home/tamsynn/NVMeData/Fleet Ops Roots/`.
The read-only capture and preserved logs are in
`build/rust-migration-stage17/acceptance/installation-before.json` and
`installation-before/`. Its Armada image is SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`;
its FleetOpsHook image is
`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`.
These match the reviewed October 23 fixture pair; the default checkout fixtures
are different images. A supported-image test is still headless evidence.

`build/rust-migration-stage17/acceptance/package/` contains only the proposed
runtime payload: the Stage 16 main DLL, Stage 15 media sidecar, and Stage 14
mission loader. `package-manifest.json` pins these bytes and the verified backup
copies of the selected installation's predecessors. This directory is not an
installed or launched test game. The media sidecar did not previously exist in
the selected installation; rollback must restore that absence.

An independent manual-test installation is prepared at
`build/rust-migration-stage17/acceptance/test-installation/`. It contains only
`FOtest`, `test`, and their respective parents `Fleet Ops 4.0` and
`STA2 Classic Mod`. The copy and payload hashes are recorded in
`test-installation-manifest.json`; no files are hardlinked to the original.
The copied mods use separate settings directories
`A2FO Rust Acceptance Stage17 FOtest` and `A2FO Rust Acceptance Stage17 test`.
Their original configurations and replaced DLLs are backed up inside the copy.
Fleet Ops 4.0 also supplied its own mission loader: the test copy replaces this
parent override with the same staged Rust loader, preventing the older loader
from shadowing the test payload. Its original bytes are preserved.

Launch the copied `FleetOps.exe` manually using the usual Proton setup, with
the copied installation as the working directory. This task has not launched
it or changed a Steam shortcut. Start with `FOtest`, then repeat with `test`.
Record the selected renderer, resolution, map and relevant enabled features.
Run the cases below and preserve the copy's `Data/A2FOExtensions.log`, renderer
log and mission fault log before restarting after a failure; startup can
overwrite logs. Keep save files and screenshots with the observations.
`manual-results.json` records every case as pending. Multiplayer is explicitly
pending because the user has no second peer available yet.

The combined host gate uses the current Makefile and source with 40 previously
hash-pinned historical tests restored only into a private overlay. It executes
41 host binaries and 18 Python tests. The Rust gates pass 309 core tests and
25 media tests, supported Windows builds, formatting, core strict Clippy, and
the DLL's supported Windows library Clippy check. Media strict Clippy warnings
remain advisory debt. Unsupported DLL host/test-harness invocations are retained
as diagnostic failures, rather than being represented as passing tests.

The acceptance run must record a concrete mod, map, graphics configuration,
candidate hashes and both multiplayer peers. Neither source coverage nor a
headless result may mark the manual tests below as passed. The remaining source
ownership and MMM replacement gaps in phases 1–4 also remain open.

## Required game acceptance after the remaining source work

Use a single frozen candidate and record its hashes, exact engine image, mod,
settings, map, saves and logs. Keep the current known-working installation and
rollback material intact. Tests require manual game operation; agent input
or window automation is prohibited in this workspace.

1. Restart fully. Check startup, extension admission, the configuration menus and
   inactive/disabled module behavior with the supported engine images.
2. Exercise A1 compatibility and SOL maps, resource and officer limits, build
   queues, squadrons, shields, weapons, refit workflows and mission transitions.
3. Check real rendering with the relevant D3D paths: textures and variants,
   directional shields, nebulae, turrets, interface canvases and mission backdrops.
   Capture driver-specific errors and compare the original baseline visually.
4. Run existing MMM tutorials and Premonitions. Test new-game startup, failure,
   success, repeated mission changes, save/load and restart. The deferred reload
   callback is a source-level gap that must be resolved before this gate.
5. Check the actual video and audio assets, synchronization, natural EOF, skip,
   seeking, repeated playback and shutdown. A codec that decodes a synthetic
   fixture still needs real-asset and real-time performance checks.
6. Run multiplayer on matching candidate builds: joining, synchronized gameplay,
   transitions, effects and reconnect/exit. Check desyncs and both peers' logs.
7. Reconcile all evidence and unresolved failures before deployment. Deployment
   needs a pinned candidate, content-hash-verified backup and a rollback receipt.

No manual or multiplayer acceptance has been performed by the headless tools.

## Manual fault report and support-file repair — 2026-10-29

The user's first quick test launched the original Roots `Data/ArmadaL.exe`,
which still loaded the older main DLL (`8475c045...a5b1e`). The prepared Rust
copy's main DLL is `9a019cb0...6f2013` and had no fresh game log. These reported
symptoms therefore do not establish a Rust-candidate regression or acceptance.

The captured renderer configuration had `EmissiveMaps=0`. The module-selection
Save path also required `modules/A2FOLogMailerHelper.exe`, which was missing.
This explains the matching incomplete-mailer popup when the saved reporting
preference is enabled. The 3D scene provider
`modules/refit/A2FORefitScene.dll` was absent as well; scene loading fails before
creating its graphics device when that file cannot be loaded. The generic
worker retry message did not identify this missing dependency.

Both the original installation and the independent test copy now have the
existing built scene provider and mailer helper restored, and `EmissiveMaps=1`.
Only this renderer setting changed. Main DLLs, module selections, reporting
preferences, assets and scene configuration were retained. No mailer helper,
game, physical graphics device or input automation was run by the repair.
The provider passed the existing null-device ABI/init/thread-ownership check;
visible effects and the module window still require a full-restart manual retest.

Evidence and rollback hashes are under
`build/rust-migration-stage18/live-faults/`. The initial Stage 17 snapshot is
preserved; its copy had inherited these missing support files. Consolidating
feature DLLs does not remove the separate scene provider or reporting helper.
Check both before calling a prepared bundle ready:

```sh
python3 tools/check_runtime_support.py --data /path/to/test-installation/Data
```

This preflight checks the two companions and scene configuration without loading
code or sending reports. It passes for both repaired installations and rejects
the simulated missing-provider/helper package. For Rust testing, launch the
prepared copy's `Data/ArmadaL.exe` with that copy's `Data` as the working directory,
using the normal Proton setup. The original main DLL has not been replaced.

## User-authorized test deployment — 2026-10-29 14:25 UTC

After the support-file repair, the user explicitly requested replacing the
original installation's main DLL for testing. Its `Data/A2FOExtensions.dll` now
matches the verified Rust candidate (`9a019cb0...6f2013`); the required
`Data/A2FOMedia.dll` sidecar (`a6813eec...ff6bf8`) is installed alongside it.
All 197 exports match the previous installed DLL. The prior main DLL and a
guarded rollback script are preserved under the original installation's
`A2FOExtensions-backups/rust-test-20261029T142502Z/` directory. The deployment
receipt is `build/rust-migration-stage18/live-faults/rust-test-deployment-20261029T142502Z.json`.
The usual original `Data/ArmadaL.exe` can now test this candidate. No game was
launched by the agent; manual acceptance and production acceptance remain open.

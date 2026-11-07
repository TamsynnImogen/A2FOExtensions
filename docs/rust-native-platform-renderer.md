# Native platform and renderer Rust migration

The user authorized replacing the remaining native platform implementations on 2026-10-28, including Win32, COM, GDI, D3D8/D3D11, engine memory and graphics resources. This phase is in progress. The earlier [runtime ownership phase](rust-runtime-ownership.md) also remains incomplete; changing priority does not credit the other feature-module state.

## First implementation slice

- `core/hook.cpp` now adapts the public C++ API to Rust. `native_hooks.rs` directly imports Win32 `VirtualProtect`, `VirtualAlloc`, `VirtualFree`, `GetCurrentProcess` and `FlushInstructionCache`, and owns checked patch/trampoline operations. The caller still supplies live readable spans, validated whole instructions and quiescent patch sites. Expected-byte checks, target-width branch arithmetic, protection restoration, cache-flush order, failed-allocation cleanup and publication order retain their previous contracts. Restore/flush failures remain ignored as before; this migration does not add rollback.
- `d3d_native.rs` now constructs compressed texture/subresource/SRV descriptors, selects native COM methods, owns pending texture/view cleanup, and owns persistent compressed-view state and fallible allocation. It reuses the established upload validation/decision flow. `texture_handle.hpp` carries only an opaque pointer; Rust publishes a replacement before releasing its predecessor. Reads of caller-owned format and bytes occur at the original callback boundaries.
- `com_guard.cpp` retains exact SDK-typed invocations inside C++ exception guards. Rust supplies the method address, descriptors and resource pointers. C++ also retains the public interface, STL image/error adapters and diagnostic text formatting. These remaining adapters are not counted as fully rewritten native implementations. A runtime comparison of 39 SDK size/alignment/offset/vtable-slot/constant values disables native dispatch on mismatch. C++ exceptions return status values before control reenters Rust; they cannot unwind Rust frames. Handle destruction preserves the previous swallowed-Release-exception behavior, while an exception in final upload cleanup terminates as the old noexcept owner did.

Microsoft's [VirtualProtect contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect) and [COM HRESULT guidance](https://learn.microsoft.com/en-us/windows/win32/com/error-handling-in-com) informed the review. Local compiler SDK declarations and the runtime layout check supply the ABI evidence.

## Verification and preservation

The work area is `build/rust-migration-stage12/native-platform-renderer/`. It preserves 1,309 pre-change project inputs, the predecessor artifact hashes and canonical artifact hashes. Candidate builds use private paths; no install or deployment is implied. Versioned attempts retain commands, logs, hashes and results. The bounded acceptance receipt is [acceptance-v1.json](../build/rust-migration-stage12/native-platform-renderer/acceptance-v1.json); the frozen source and artifacts are under `checkpoint-v1/` in that work area.

Accepted checks:

- `rust-attempt2/receipt.json`: formatting, 309 host Rust tests, Linux and i686 Windows Clippy, and both release archives. `dll-clippy-attempt1/receipt.json` separately checks the DLL crate.
- `candidate-v1/rust-dll/receipt.json`: fresh production Cargo DLL link, all 701 input hashes still matching, no changed inputs, and the exact candidate DLL hash. `dll-attempt3/receipt.json` separately confirms the DLL is up to date and builds the standalone StationRotation and NormalWeaponTech DLLs and built-in smoke executable. Earlier build attempts are retained; one was stopped for the exception lifetime correction and another completed its DLL link but was rejected because an unrelated test fixture changed during the broad input check. The accepted build pins its consumed project inputs.
- `boundary-attempt1/receipt.json`: unchanged named exports/ordinals (197 core, 2 StationRotation, 3 NormalWeaponTech) and DLL dependency sets (13 for core).
- `final-native-current1/receipt.json`: three headless Win32 executions for checked executable-memory patches, official-SDK COM upload/view ownership, and wrapped malloc/free failure/reentry checks. The latter checks both owner allocation and upload-subresource allocation failure, cleanup and successful handoff.
- `final-native-baseline1/receipt.json` and `final-native-counterexample1/receipt.json`: preserved native implementation passes the current upload fixture; the initial port fails the exception-destruction ordering assertion exactly as expected. The corrected port passes the same fixture. Together with the unchanged memory fixture's baseline execution, two baseline/current stdout traces match exactly.
- `dll-smoke-attempt1/receipt.json`: actual candidate DLL startup in a fake host, log ordering, deferred-worker completion and initialization retry; all 48 built-in providers and inert exports checked without loading a game image or installing engine hooks.
- `../legacy-probe-fixtures/run-receipt.json`: 37 matching Linux before/current probe rows. The stage-five fake-SDK case now explicitly covers the probe only; native upload and owner coverage moved to the real-SDK-layout fixtures. Its historical upload rows are not credited as current native ABI evidence.

Rejected attempts remain available, including Wine bootstrap timeouts, a harness persistence experiment, and an isolated baseline/counterexample compile that omitted static C++ runtime flags and failed at DLL loading before test execution. The corrected isolated builds verify system-only imports before execution. The work also records a predecessor MMM object-cache reuse attempt; make's dependency stamps subsequently triggered recompilation. Wine helper diagnostics can append after a fixture exits. `runtime-log-finalization-v1/receipt.json` records final log hashes after the private server was stopped; the pre-finalization receipts are preserved. These records do not imply deployment or real-driver acceptance.

`make rust-native-platform-build` builds console fixtures only; Wine execution is explicit and serialized. The fixtures use owned memory pages, synthetic COM objects and a fake DLL host. They do not launch the game or manipulate input or windows. Native Wine evidence does not establish real-driver or in-game acceptance.

## Remaining implementation scope

| Area | Status after this slice |
| --- | --- |
| Checked Win32 patch-memory backend | Rust implementation; C++ public API facade remains |
| Compressed D3D11 upload and view lifetime | Rust implementation; typed C++ exception/STL adapters remain |
| D3D11 UI device, shaders, buffers, draw and texture cache | C++ platform operations and owners remain in `renderer/d3d11/ui_renderer.cpp` |
| D3D11 probe/device/swapchain/presentation | C++ operations and owners remain in `renderer/d3d11/probe_device.cpp` and `.hpp` |
| D3D8 compatibility objects and graphics resources | C++ COM ABI implementations and resources remain under `renderer/d3d8_compat/` |
| Nebula renderer and shared COM owners | C++ resources/calls remain in `core/nebula_renderer.cpp` and `core/com_owner.hpp` |
| Win32 window and GDI rendering | C++ operations remain, including `core/module_menu.cpp` |
| Engine memory access and import/vtable changes | Additional native access/protection paths remain in core and D3D8 compatibility files |
| Remaining Win32 runtime and feature-module resources | Incomplete; lifecycle, loader, file and platform adapters need further conversion and evidence |

No whole-phase percentage or completion claim follows from this bounded slice. Each further owner/API conversion needs its own layout, failure, ordering and reentry evidence plus a whole-DLL compatibility check.

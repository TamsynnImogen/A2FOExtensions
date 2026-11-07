# Hook and assembly migration

The verified scope is 48 external assembly families, three recovery-verifier FP operations, and four bespoke executable-code patch paths. The broader Rust rewrite remains incomplete. Platform resources, native C++ callback bodies/state, compiler-fastcall adapters and IAT/vtable pointer-slot mutation paths are not credited as converted here.

## Implementation

- The 48 families now live in `rust/a2fo_bridges/`, compiled separately by Rust and selected by production Make rules for the main DLL and standalone modules. Their register, flags, stack-cleanup, continuation, x87 and SSE instructions remain explicit global assembly. Historical `.S` sources are retained for comparison. See [bridge build and contracts](../rust/a2fo_bridges/README.md).
- `native_fp.rs` owns the recovery verifier's FXSAVE/FNINIT/FLDCW and FXRSTOR operations. Its raw assembly entries use Win32 cdecl or Linux x86-64 SysV pointer arguments without compiler-generated FP spills. C++ retains the scope and its aligned caller-owned buffer, and calls the private POD/pointer interface.
- `native_patch_profiles.rs` owns the remaining bespoke executable writes. Radar and the renderer preserve the rule that written code remains installed even if protection restoration fails. CraftIdentity preserves both expected-byte checks, including the reread after acquiring protection. Video preserves current-byte copying after allocation, one whole-gateway flush, publication before patching, and free-before-unpublish after a failed target patch. Its original diagnostics and publication callback remain C++ adapters.
- The existing Rust checked JMP/CALL/byte patch and inline-hook implementation remains in `native_hooks.rs`. These primitives and the migrated profiles rely on caller-validated readable extents, whole nonrelocatable instruction spans and quiescent patch sites. No general arbitrary-prologue decoder/relocator or transactional rollback was introduced.

Pointer-slot/IAT/vtable changes are distinct remaining platform paths. C++ still supplies version/site admission, address discovery, native objects, callback adapters and logging. Moving raw bridge instructions does not convert those objects or their lifetime management.

## Verification

Evidence is under `build/rust-migration-stage13/hooks-assembly/`. Accepted results are recorded in [acceptance-v1.json](../build/rust-migration-stage13/hooks-assembly/acceptance-v1.json), with source and artifacts frozen under `checkpoint-v1/`.

- Object comparison: all 48 families, 306 emitted symbol definitions across objects and 227 relocations match the preserved implementation exactly. Three altered COFF objects are rejected by the checker. The actual Make-produced objects are checked again, rather than crediting only independently compiled differential objects.
- Native paired fixtures: 22 direct calling-convention cases, 432 camera cases, 3,456 view cases and 576 FP-environment cases per side produce exact matching transcripts. The tests cover Delphi argument registers, thiscall arities and stack cleanup, callee-saved registers, ParameterDB AL zero extension, DF/EFLAGS, x87 stack/control/status, MXCSR, SIMD state and parser-count wraparound. FP cases include all 12 supported precision/rounding combinations and difficult floating-point bit patterns.
- Fourteen injected-memory cases check failure, byte mutation between checks, protection restoration, relocation bytes, and gateway publication/free ordering. The Linux and Win32 transcripts match. A real Win32 fixture executes owned code pages: a patched target returns 99 while its gateway returns 37; a read-only mapping rejects a write, and failed installation frees its gateway before clearing publication.
- Rust tests, host and i686 Windows Clippy, strict native builds, production Cargo linking, all 42 standalone compatibility links, main/standalone export/dependency checks, DLL fake-host startup and a built-in smoke test are separate gates. Original/reference, source/build, native headless and game acceptance are not interchangeable evidence.

No deployment, game launch, input automation or real-renderer acceptance is implied. The data-only and synthetic execution fixtures never call a game entry point. Native logs are finalized only after the private Wine server has stopped, since helper diagnostics can append after a fixture exits.

## Acceptance records

- `rust-gates-attempt2/receipt.json`: formatting, 309 Rust tests, host/Win32 Clippy and both release archives, after the patch-profile conversion.
- `final-static-gates-attempt1/receipt.json`: formatting of all independent bridge sources, DLL-crate Clippy and the refreshed host recovery FP regression against the final archive.
- `object-parity-attempt3/receipt.json` and `boundary-final-attempt1/receipt.json`: differential instruction/relocation checks, all 48 actual Make-produced objects, main DLL link selection (44 bridge objects), and all 43 DLL export/dependency boundaries. The main retains 197 exports and 13 DLL dependencies.
- `link-gates-attempt3/receipt.json` and `candidate-v1/rust-dll/receipt.json`: final production Cargo DLL link, instruction fixtures and all 42 standalone module links. Unchanged native support objects were reused from the pinned preceding link; newly migrated bridge objects are Rust-produced.
- `native-runs-attempt2/receipt.json`: four exact baseline/current pairs, using the final Make-built executables: 22 call, 432 camera, 3,456 view and 576 environment cases per side.
- `patch-fixtures-attempt1/receipt.json` and `patch-native-runs-attempt1/receipt.json`: 14 host/Win32 model cases with matching output and real Win32 executable-page/trampoline/failure checks.
- `smoke-final-attempt2/receipt.json`: actual DLL fake-host startup, checked-memory/trampoline regression and built-in smoke, all successful. `log-finalization-v1/receipt.json` records the closed-prefix log hashes.

Earlier attempts remain preserved. GNU COFF's trailing padding was made explicit after an initially rejected object comparison. Strict compiler checks caught callback-fixture attributes and retention issues; the corrected tests use real C++ thiscall methods with explicit assembly labels. An earlier smoke wrapper expected a nonexistent output word despite a successful memory test; the corrected clean smoke run passed. These are retained failures and harness corrections, not additional runtime coverage.

## Remaining work

The earlier [runtime ownership phase](rust-runtime-ownership.md) and [native platform/renderer phase](rust-native-platform-renderer.md) remain incomplete. Additional native C++ callback and compiler-ABI adapters, data-pointer hooks, engine-memory discovery/admission and resource ownership need their own conversion and evidence. Real in-game hook admission, graphics, gameplay and multiplayer acceptance remain unverified. This milestone does not establish a completed all-Rust A2FOExtensions build.

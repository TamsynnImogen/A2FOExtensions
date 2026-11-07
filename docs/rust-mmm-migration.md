# MMM mission runtime migration

Stage 14 replaces the production `mmm_loader.dll` implementation with a Rust
DLL and moves A2FOMissions handoff state into Rust. **The complete MMM framework,
Lua 5.1.5 VM and Luabind bindings are not yet replaced.**

The loader preserves the two cdecl exports, ordinals and dependency DLL set.
An authoritative built-in Missions owner blocks standalone fallback when it is
disabled or incomplete. Older cores use the loaded standalone module, then the
core installation's modules directory, then the loader's parent installation.
The original filename limits, backslash-first separator handling, failed-resolve
retry and optional standalone unload callback remain intact. The Rust DLL owns
its entry point and calls no VM or framework destructor from loader detach.

`rust/a2fo_core/src/mmm_runtime.rs` owns first-input/new-game/unload flags and the
opaque Worker handle. Cleanup remains deferred until the next nonnull interface
extension; the new interface is installed before destroying the previous Worker.
A pending native mission result takes precedence over lazy startup. Startup or
resume failure leaves first-input state available for retry. Shutdown preserves
the prior process-lifetime abandonment policy. A standalone CRT finalizer
retains the former global unique_ptr fallback when DLL unloading bypasses the
normal shutdown export; the built-in owner keeps its former absence of a CRT
destructor. Atomic storage protects the
loader-detach marker; engine lifecycle dispatch still uses the mission thread.

`modules/A2FOMissions/module.cpp` is an exception firewall and native operation
adapter. Each call catches C++ exceptions before returning through Rust, then
rethrows on the C++ side. The retained Worker, Rule helper, VM, Lua objects and
native engine bindings still own their existing resources. Moving an opaque
handle does not establish that those native objects have been rewritten.

The historical loader source is retained solely as a comparison reference in
`missions/mmm_loader/shim.cpp`; the Makefile no longer compiles it for the
production loader. `tests/mmm_reference/module.cpp` freezes the prior handoff
implementation for differential tests. Mission scripts and archives are unchanged.

## Verification

Evidence is under `build/rust-migration-stage14/mmm/`. The compatibility tool
builds fixtures and runs host tests without starting Wine by default. Native
execution requires an explicitly reserved private prefix:

```sh
python3 tools/check_mmm_rust_compatibility.py \
  --output build/mmm-compat-review \
  --rust-archive build/rust-migration-stage14/mmm/candidate-v1/rust/i686-pc-windows-gnu/release/liba2fo_core.a \
  --rust-loader build/rust-migration-stage14/mmm/candidate-v1/missions/mmm_loader.dll \
  --native --wine-prefix /tmp/a2fo-shields-wine-prefix
```

The output directory must be new. The test runner disables display drivers and
uses only synthetic executables, fake mission interfaces and fake core/modules.
It compares 12 original/current lifecycle transcripts, four loader routing
cases and two actual FreeLibrary comparisons (live Worker and prior shutdown). These include constructor/start/resume exception retries, pending mission
completion, null extension while unload is pending, interface rebinding before
destruction, shutdown abandonment and edit-mode 51/51 results. Edit-mode memory
reads are mocked; no original game addresses execute. Ten host resolver tests
also cover path limits, lookup order, partial publication and failure retry.

The resume tests explicitly record that destruction clears the native input
callback. Labelled `synthetic-direct-input` events exercise only the controller
branch. **They do not prove that native save/resume re-registers its callback.**
The unvalidated Jan_B reload-rule bridge remains deferred. In-game new mission,
restart, save/load, mission completion and multiplayer acceptance remain open.

## Remaining replacement work

- Replace the retained Worker and Rule implementation, including validated native
  rule registration and safe mission transition cleanup.
- Replace the Lua 5.1.5 VM while retaining numeric, bytecode, library, error,
  coroutine, metatable and garbage-collection semantics required by missions.
- Replace Luabind registrations, class inheritance/conversions, userdata,
  references and callback lifetimes. Native shared pointers and STL layouts
  cannot become Rust ABI objects accidentally.
- Replace application/timer/monitor/hook state, scripting include resolution,
  archive loading, mod/parent lookup and shared storage.
- Preserve and test the save/load byte encoding stored in in-game entities;
  this is separate from VM serialization.
- Exercise existing tutorials, Premonitions and campaign scripts against both
  runtimes, then obtain manual in-game acceptance before deployment.

## VM candidate investigation

An isolated [Rulua candidate](https://github.com/DaragonTech/Rulua/tree/c8e04a5c6ab5991a7883282d8ae4f9942edb4c21)
compiled for i686 Windows and ran two C API comparison suites against the actual
vendored Lua 5.1.5 objects. The results and full diffs are in
`vm-capi-attempt1/receipt.json`. The suites produced 201 and 79 output lines per
runtime. Pointer identities and executable search roots naturally differ; there
are also Windows submodule path-separator and coroutine error-message differences
that need explicit compatibility decisions. Exit zero is not a parity result.

This candidate still compiles a C auxiliary library, varargs adapter, x86
setjmp/longjmp bridge and a native allocator dependency. It also introduces Rust
standard-library Windows imports. It has not been integrated, selected as the
production VM, or counted as a complete Rust replacement. Removing those native
pieces and verifying Lua errors, coroutines, userdata, GC and Luabind ownership
remain required even if this interpreter core is reused.

# Native adapter retirement

The first phase-six retirement pass removes two production C++ translation
units after their final callers were changed to the existing Rust operations:

| Retired production files | Current owner and callers |
| --- | --- |
| `core/hook.cpp`, `core/hook.hpp` | Rust `native_hooks.rs`; core startup, nebula renderer, module menu and renderer options call its C API directly |
| `core/decal_math.cpp`, `core/decal_math.hpp` | Rust `text_geometry.rs`; the nebula renderer passes three float buffers directly |

The checked-patch API exposes Rust boolean/cdecl entry points matching the
existing SDK callbacks. Hook records use the existing C layout directly, with
explicit zero initialization. Publication still happens only after both writes
succeed; failure leaves the caller's hook record unchanged. Site signatures,
addresses, expected bytes, native dispatch order and x87 math are unchanged.
The module SDK header, API version/revision and core export definition are
byte-for-byte unchanged.

The production link also drops `bcrypt`, `atomic` and `m` from its explicit
library list. These were incidental FFmpeg link inputs; the fresh DLL links
without them. This does not remove the compiler's CRT or Windows platform APIs.
The phase-five Rust media sidecar and standalone video DLL are carried forward
with their accepted hashes, without rebuilding unchanged media code.

Exact retired sources are under `tests/native_reference/adapters/` for
historical differential tests. Only tests may compile those copies. Active
production rules cannot name the retired files. The retirement ledger is
`rust/retired_native_adapters.json`; `tools/check_rust_retired_adapters.py`
checks the source graph, Rust symbols, absent C++ facade symbols, removed link
inputs, unchanged SDK definitions and DLL export/ordinal/import contracts.

## Validation

The staged candidate and evidence are under
`build/rust-migration-stage16/adapter-cleanup/`.

- 309 Rust tests, host and i686 Windows Clippy, and formatting passed.
- Seventeen before/current headless comparisons match all 18,690 output rows.
  They cover checked memory patches and trampolines, menu admission/launch,
  shortcut/COM failures, mutable lists, renderer settings/layout/lifecycle,
  and numeric output across the established x87 precision modes.
- SDK patch-callback types and the i686 hook-record size/offsets are checked at
  compile time. The native memory fixture executes only its own synthetic code.
- A freshly linked standalone radar module initializes against an inert
  reference mapping through the real SDK callback path. Its two exports and
  dependency set match the baseline. The main DLL keeps all 197 exports and
  ordinals, with no new dependency DLLs; its fake-host load/lifecycle smoke passes.
- The new direct decal fixture passes. The legacy host hook transcript still
  matches ten rows using a test-only Win32 backend.

The main DLL was freshly linked from four rebuilt core consumers plus unchanged,
validated native objects. The renderer retains pre-existing diagnostic warnings
under its normal production flags; focused fixtures build with the established
strict flags and frozen-source diagnostic allowances. Initial failed probes are
retained separately. The standalone renderer-options test recipe now links its
Rust dependency, and the radar fixture uses the current Rust controller entry.

No game was launched or deployment performed. Canonical DLL hashes are
unchanged; ordinary build caches may have been refreshed by checks. Rendering,
gameplay, save/load and multiplayer acceptance remain phase-seven work.

## Dependencies that must remain

C++ still owns live strings/containers, engine and GUI objects, exception
firewalls and parts of the graphics boundary. MMM still uses Worker/Rule,
Lua 5.1.5, Luabind and Boost. Their consumers have not all moved to Rust, so
removing those sources/libraries would break the runtime. Reference FFmpeg
fixtures/SDK material also remain outside the production build for comparison.

This pass retires proven redundant layers; it does not establish that every
remaining C++ dependency is redundant. Historical inventory IDs and census
records are preserved. No census credit or complete Rust-rewrite percentage is
claimed; the retirement ledger provides the missing-source explanation for
later census reconciliation.

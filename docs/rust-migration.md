# A2FOExtensions Rust migration

## Checkpoint — 2026-10-29: portable census reconciled; manual acceptance pending

Stage 17 credits the 35 held completion records, preserving the original 151-unit
membership and all 116 historical credit records. The current census is
**151/151 portable implementation units**, with **1,714 required Rust symbols**
in the staged main DLL. This measures portable implementation coverage, not a
complete Rust-owned DLL, native resource replacement, or game acceptance.
Historical completion checkpoints retain their original `strict_census_credit:
false` fields; each new credit points to `rust/census_reconciliation.json` and
its current verification. The two retired hook/decal source IDs remain in the
census with their Rust owners; the denominator was not reduced.

The verifier checks all ten frozen completion checkpoints, their source archive
and manifest identities, artifacts and evidence, selected archived C++/Rust
source bytes, current source pins, the six later C++ changes, the actual DLL link
receipt and exports, and combined regression evidence. Cargo builds the core as
a dependency of the Rust-owned DLL; the native fixture static archive is a
separate build of the same pinned sources. Their binary identities are not
conflated. Stage 11 focused evidence covers five selected core suites and 15
executions, not the entire historical core matrix.

Combined checks passed 309 core and 25 media Rust tests, 41 host binaries,
18 host Python tests, 30 native parity groups, five exact-installed-engine
fixture runs, and ten census accounting negative tests. The 30-group result
combines preserved passed runs with identical 906 input pins. Private mock
layout repairs and forced rebuilding of stale test caches are recorded; no
product behavior changed in this acceptance pass. Media strict Clippy debt and
unsupported DLL host/test-harness diagnostics remain explicit advisories.

Evidence is under `build/rust-migration-stage17/acceptance/`. An independent
manual-test installation is ready there for `FOtest` and `test`, including their
parents and isolated settings. The selected original installation
`/home/tamsynn/NVMeData/Fleet Ops Roots/` is unchanged. No game was launched.
Rendering, gameplay, save/load, mission and real media/device acceptance remain
pending; multiplayer is deferred because no second peer is available. Native
state/resource ownership and retained MMM replacement work also remain open.
See [acceptance instructions](rust-rewrite-acceptance.md).

## Checkpoint — 2026-10-28: Breeders, Infestation and M3D runtime units complete behind native adapters

The final stage-ten runtime batch covers three inventory units across `A2FOBreeders`, `A2FOInfestation`, and `A2FOM3D`. Breeders and Infestation already used the Rust controller workflow; this batch validates them afresh. M3D adds portable completion flow in Rust. C++ retains engine, Win32, Delphi and module ABI boundaries, native calls, storage and exception ownership. This is source and headless completion, not a pure-Rust binary.

The pinned candidate passed 300 Rust tests. The integrated DLL retained all 197 export names and ordinals; three standalone DLLs retained their export and import contracts. The native gate passed 90 frozen/current trace pairs across 180 executions, including Breeders, Infestation and M3D runtime and lifecycle behavior. The matrix includes synthetic host M3D conversion and 32-bit hierarchy regressions, guarded byte-boundary reads, and reserve/append/candidate allocation failures. The built-in native admission passed separately.

Evidence and candidate artifacts are frozen at `build/rust-migration-stage10/final-runtime-batch/checkpoint/validation.json`. The three entries move to `completed_pending_census`; strict totals remain 116/151 verified and 35 pending, all 35 awaiting census credit. No `not_migrated` inventory units remain. The candidate has not been deployed or launched in the game; in-game acceptance remains pending.

## Checkpoint — 2026-10-28: shield, hybrid and swarm runtime units complete behind native adapters

The stage-ten shield, hybrid and swarm batch covers three inventory units across `A2FOAlwaysShowShields`, `A2FOHybridBuild`, and `A2FOSwarmSystem`. Rust owns portable visibility, swarm-motion, hybrid production and list-policy decisions. C++ retains engine, Win32, Delphi and module ABI boundaries, native calls, storage and exception ownership. This is source and headless completion, not a pure-Rust binary.

The pinned candidate passed 292 Rust tests. The integrated DLL retained all 197 export names and ordinals; three standalone DLLs retained their export and import contracts. The native gate passed 78 frozen/current trace pairs across 156 executions, including the shared Hybrid production regressions. The Hybrid completion fixture checked live and cached button callbacks, read-order mutations, submenu registration, duplicate-project handling, and publication of four distinct research-station methods on both snapshots. The built-in native admission passed separately.

Evidence and candidate artifacts are frozen at `build/rust-migration-stage10/shield-hybrid-swarm-batch/checkpoint/validation.json`. The three entries move to `completed_pending_census`; strict totals remain 116/151 verified and 35 pending, with 32 awaiting census credit. The three remaining `not_migrated` families are `A2FOBreeders`, `A2FOInfestation`, and `A2FOM3D`. The candidate has not been deployed or launched in the game; in-game acceptance remains pending.

## Checkpoint — 2026-10-28: five presentation/runtime families complete behind native adapters

The stage-ten presentation/runtime completion covers `A2FOVideo`, `A2FOMissionSelector`, `A2FORefitMenus`, `A2FORefitYards`, and `A2FOTurrets`: eight inventory units. Rust controls portable flow and policy decisions. C++ retains Win32, Delphi and engine ABI boundaries, native storage and exception ownership, GPU and media APIs, and precision-sensitive native math. This is source and headless completion, not a pure-Rust binary.

The pinned candidate passed 278 Rust tests. The integrated DLL retained all 197 export names and ordinals, and five standalone DLLs retained their export and import contracts. The full native gate passed 101 frozen/current trace pairs across 202 executions, including the shared video and refit-yard regressions. The built-in native admission passed separately. Timing comparisons cover 96 rows across 12 x87 precision/rounding modes, and yard traversal includes eight live-list mutation/guard cases. A separate native helper gate passes 1,039 iterator, mutation and move-failure cases plus layered alpha.

Evidence and candidate artifacts are frozen at `build/rust-migration-stage10/presentation-runtime-batch/checkpoint/validation.json`. The eight entries move to `completed_pending_census`; official strict credit remains 116/151, with 29 awaiting census credit and six remaining `not_migrated`. The candidate has not been deployed or launched in the game. In-game acceptance remains pending.

## Checkpoint — 2026-10-28: four core units complete behind native adapters

The stage-ten core completion covers `core/dllmain.cpp`, `core/module_menu.cpp`,
`core/nebula_renderer.cpp`, and `core/renderer_options.cpp`. Rust controls
portable policy, menu, renderer, and pixel decisions. C++ retains the Win32,
COM, D3D and engine hook boundaries, native CRT and storage operations, exact
x87 behavior, vertex validation, sorting, exception scopes and GPU work.
This is source and headless completion, not a pure-Rust binary.

The pinned candidate passed 264 Rust tests and the integrated DLL retained
all 197 export names and ordinals with no new system imports. The full native
gate passed 19 matching frozen/current trace pairs across 38 executions;
the built-in native admission also passed. Checks include the whole-source
core menu and renderer paths, preserved callback order, native allocation
behavior, and inherited regression fixtures. The source boundary check pins
110 unchanged core/sdk files and allows 13 controlled bodies in four files.
The settings fixture retains its historical inner startup-wait mock; the
deferred lifecycle has a separate regression fixture.

The evidence and candidate are frozen at
`build/rust-migration-stage10/core-completion/checkpoint/validation.json`.

Only these four inventory entries move from `not_migrated` to
`completed_pending_census`. The official strict census stays at 116/151
verified; 21 entries now await census credit and 14 remain `not_migrated`.
The candidate has not been deployed or launched in the game. Native GPU
behavior and in-game acceptance remain pending.

## Checkpoint — 2026-10-28: six render/UI families complete behind native adapters

This batch completes the remaining portable controllers and source/headless
validation for A2FOCraftIdentity (module and object editor),
A2FODirectionalShields, A2FOFeaturePack (recursive ODF, buildyard pseudo-technology,
queue enhancements and upgrade pods), A2FOShellDisplay (Bink and canvas),
A2FORGBTextures and A2FOTextureVariants: eleven inventory units in total.

Rust now controls CraftIdentity first-match fields/atlas lookup, full-byte string
trim, display-mode suffix admission, drawn predicates and visible hover setup;
RGB DDS/TGA publication traversal and precedence; FeaturePack ODF hash-chain
membership; directional effect-tree traversal; canvas ancestry, point-map pass
order and reverse lease selection; and texture scorch admission, publication
comparison and hardpoint selection. Native failure/reset paths and lazy reads
are preserved. Canvas ancestry releases each borrowed record before the next
lookup. Full u32 scorch bands and raw effect pointers remain data, separate
from callback error metadata. Display-mode suffix whitespace is classified
before the parsed-range decision, as in the original CRT path.

This is portable-controller completion, not a pure-Rust binary. Native code
retains Win32/COM/GDI/CRT and engine calls, hooks/signatures/addresses, checked
fields, native strings/maps/iterators/locks, exception scopes, exact GNU x87
projection/geometry/rounding primitives, and FeaturePack's existing STL sort.
The sort retains casefold-equivalent filename ordering and comparator/allocation
exception behavior. Object-editor and Bink controllers were already in Rust;
this batch supplies their current build and headless validation. Existing native
kernel files and ABI boundaries are pinned rather than rewritten speculatively.

Validation passed: 249 Rust tests; formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and all six standalone DLLs. Twenty native
suites produced 267 matching before/current trace pairs (534 executions,
6,832 stdout lines), with inherited mutation and allocation-failure cases.
A direct production Bink fixture compares 276 numeric rows across 12 x87 modes;
canvas fixtures cover ancestry depth bounds, reverse/scoped leases and point
mapping. Seven additional directional tree cases cover signed branches, missing
and unreadable nodes, the head sentinel and cycle bound in both module forms.
A separate production scorch comparison passes 228 rows across all 12 x87
precision/rounding modes, including guards, boundary bands, modulo hardpoint
selection, publication state, raw sticky/C1 status, CW and stack balance.
Built-in batch9 admission also passes. Runtime fixture assets are isolated per
execution and retained; both sides see the same logical working-directory path.

The source boundary check preserves 56 complete native files and all text
outside 16 approved controller bodies plus new adapter helpers in six files.
All 417 source-referenced Rust implementations link (the verifier checks 420
including three defaults). Core 197 exports/ordinals and each standalone export
set remain identical, with no new system imports. The build pins 791 product
inputs including owned .inl files. Six objects depending on changed sources
were rebuilt; remaining cache entries and 7,851 third-party source pins were
verified against the passed Squadrons predecessor. Original/current fixtures
use independently pinned pre-batch and candidate Rust archives. Established
fixture-only GNU ABI/custom allocator diagnostic flags remain recorded in the
compile commands. Failed link/probe attempts are preserved.

Evidence: build/rust-migration-stage10/render-ui-batch/checkpoint/validation.json.
The eleven units are completed_pending_census. Official totals remain 116/151
with 35 uncredited units: seventeen completed units await the shared strict
census refresh, and eighteen units remain unfinished. Frozen membership,
baseline provenance and strict archive/common-DLL bindings are unchanged.
Canonical artifacts and earlier checkpoints are preserved. No deployment or
real game launch occurred; GPU and in-game acceptance remain pending.

## Checkpoint — 2026-10-28: A2FOSquadrons portable conversion complete

Rust now owns the remaining bar member traversal, health-value accumulation,
weighted colour admission/accumulation/division, repair-yard distance/radius
policy, retry arithmetic/admission, strict additional-cost parsing, peer snapshot
and duplicate-selection scans, and active-technology bounds/counter policy.
The existing Rust class/economics, production, squad registry, selection,
reinforcement and lifecycle controllers remain in use. Normal paid replacement
cost/build-time and native yard completion behavior are preserved.

C++ retains the exact hooks and addresses, checked engine-memory access,
thiscall/SDK/Win32 calls, native containers/string lifetimes, lock and exception
scopes, provider invocation, and native registration marshalling. Rust numeric
kernels reproduce the original GNU++17/O2 x87 instruction order and spills.
Whole-hook parity exposed an overflow C1 difference missed by isolated arithmetic
references; the final Rust values-output copy also preserves the original x87
reload/spill. Earlier failed evidence and candidate artifacts are retained. The new numeric
fixture also avoids a legacy test helper that forced PC24 for PC53/64
economics names; historical fixtures are preserved and actual control words
are asserted throughout the new matrix.

Validation passed: 229 Rust tests, formatting, host/i686 Windows Clippy, both
release archives, integrated/standalone DLLs and built-in batch9 admission.
All 134 original/current trace pairs (268 native executions, 8,200 equal
stdout lines) match across standalone and built-in forms. These include 12 x87
precision/rounding modes, 624 bar sample rows, 1,056 distance/radius rows,
336 full retry-update rows, strict parser cases, nested same-bar/provider reentry,
selection mutation, sequential launch/paid repair, and expected native hook
termination. Numeric fixtures compare raw float words, sticky flags/C1, control
word and stack balance. All 79 referenced Rust implementations link; 197
integrated and three standalone exports/ordinals are unchanged, with no new
system imports. Sixty-eight native functions/layout guards and SDK/ASM pins
remain intact.

The build pins 781 product inputs, including all 14 owned .inl files. Six cached
objects depending on owned .inl files were rebuilt; remaining incremental native
cache and 7,851 third-party source pins were authenticated. This prospectively
extends source coverage beyond earlier component maps; earlier receipts remain
unchanged. Canonical root artifacts and existing checkpoints are preserved.

Evidence: build/rust-migration-stage10/squadrons/checkpoint/validation.json.
Squadrons is completed_pending_census. Strict totals remain 116/151 with 35
uncredited records: six completed components await a shared census refresh,
and 29 other units remain. No strict archive/common-DLL binding was relaxed.
Candidate artifacts are isolated under squadrons/candidate. Nothing was deployed
and no game was launched; GPU and in-game acceptance, including a fresh-skirmish
paid reinforcement check, remain pending. This conversion does not establish a
fix for any previously observed in-game repair failure.

## Checkpoint — 2026-10-27: A2FOResources controller conversion complete

Rust now owns remaining payment admission/commit, refund, public resource API,
cache/tooltip/panel sequencing, native lookup/append policy, class/race event
publication and module initialization decisions. Native callbacks retain checked
reads, SDK/Win32 calls, locks/map entries, string/temporary lifetimes, exact CRT
formatting and the established GNU++17/x87 plus CRT lround rectangle primitives.
Payment rechecks affordability inside the same native lock as debit. Lazy team,
class and cost reads, infinite-resource bypass, A1 legacy/original renderer order,
fresh-cache reuse and the runtime guard after the original render are preserved.
Raw refund event kinds use a u32 output word, preserving UINT32_MAX rejection
without confusing it with callback error metadata. The superseded native index
API bridge remains as a maybe_unused adapter for the historical regression test.

Validation passed: 221 Rust tests, fmt, host/i686 Windows Clippy, both release
archives, integrated and standalone DLLs; six original/current full-source pairs
in both flavors (768 equal stdout lines), 48 matching rectangle/amount projection
rows across 12 x87 modes, 37 presentation assertions per flavor including prior
allocation cases, five new targeted allocation-failure cases/10 executions, and
built-in batch9 admission. Native lock unwind and partial publication match the
original; the noexcept payment allocation case preserves original termination.
All 52 source-referenced Rust implementations link. Integrated 197 and standalone
7 exports/ordinals are unchanged, no new system imports; 41 native functions,
layouts and the constants/SDK/ASM pins remain intact. The prior presentation
fixture is preserved; its generated test-only copy adds an explicit return 0 so
renaming main does not introduce undefined behavior.

Evidence: build/rust-migration-stage10/resources/checkpoint/validation.json.
Resources is completed_pending_census. Strict totals remain 116/151 with 35
uncredited records: five completed components await the shared census refresh,
and 30 other units still need completion/validation. No common-DLL/archive gate
was weakened. Candidate is isolated under resources/candidate; earlier artifacts,
failed attempts and immutable checkpoints are preserved. This is an incremental
native build using authenticated object reuse. No deployment/game launch occurred;
GPU and in-game acceptance remain pending.

## Checkpoint — 2026-10-27: EnergySystems and D3D8 unsupported-call components complete

A2FOEnergySystems now routes its remaining portable ammunition-number, colour,
rectangle, quote-removal, timer-subtraction/admission and amount-addition policy
through Rust. Existing Rust controls retain class loading, firing, resupply,
persistence, public amount APIs, parsing and lifecycle sequencing. Native hooks,
addresses/signatures, checked access, STL lifetime, CRT conversion/formatting,
exception scopes and the disabled direct UI renderer gate remain native.

The remaining D3D8 census unit, renderer/d3d8_compat/game/unsupported.cpp, already
routes all 23 unsupported-method diagnostics and return decisions through Rust.
New full-source tests verify all 23 under normal/custom return, throwing and
reentrant callbacks, including unchanged poisoned output buffers. Native COM
boundaries and exception capture/rethrow remain C++; no new graphics features
or support for those unsupported operations are claimed.

Validation passed: 215 Rust tests, formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and standalone EnergySystems DLL; 134 full-source
EnergySystems trace pairs/268 executions (2,810 equal stdout lines), 1,296
residual numeric rows across 12 x87 precision/rounding modes, the 6,600-row range
regression, 92 D3D8 cases and built-in batch9 admission. Exceptional timer and
amount values include NaNs/infinities; x87 sticky flags, C1, control word and stack
balance are checked for the residual numeric cases. All 44 referenced Rust
implementations link; integrated 197 and standalone 18 exports/ordinals are
unchanged, with no new system imports. Twenty-four native functions/layouts,
constants, SDK/ASM and D3D8 boundary source pins are preserved.

Evidence: build/rust-migration-stage10/energy-systems/checkpoint/validation.json.
Both units are completed_pending_census; strict totals remain 116/151 with 35
uncredited records. Four completed components (FireArcs, Nebula controller,
EnergySystems and D3D8 unsupported-call policy) await shared census validation;
31 other units still need completion/validation. Existing strict common-DLL and
archive bindings were not weakened. Isolated candidate: energy-systems/candidate.
Earlier artifacts, checkpoints and failed attempts are retained. Incremental
native cache reuse is authenticated; volatile Cargo build locks are explicitly
excluded from product source pins. No deployment or game launch occurred;
GPU and in-game acceptance remain pending.

## Checkpoint — 2026-10-27: A2FONebulaRenderer controller conversion complete

The module's remaining portable policy is in Rust: trim/path rules, native string
query admission, bounded texture and node-name scans, comma-separated float
parsing and partial writes, decal scaling, logo colour-key packing, damage
threshold indices, and longest-suffix comparison. The native STL sort retains
its exact tie ordering. Native calls, checked access, descriptor layouts and
storage/exception lifetimes remain C++. The separate core/nebula_renderer.cpp
unit is unchanged; this completes the ODF/material controller component.

Validation passes: 211 Rust tests, formatting, host/i686 Windows Clippy, host/Win
release archives, integrated and standalone DLLs, 1,729 equal original/current
native lines, 360 numeric rows across all 12 x87 precision/rounding modes, six
native allocation-failure cases and the built-in batch9 admission regression.
All 41 source-referenced Rust implementations are linked. Integrated 197 and
standalone 3 public exports/ordinals are unchanged, with no new system imports.
Eighteen native functions/layouts and the constants/ABI source pins are preserved.
The C1 rounding flag survives scaling because Rust writes descriptor fields
directly; native rollback guards preserve buffer frees during exception unwind.

Evidence: build/rust-migration-stage10/nebula-renderer/checkpoint/validation.json.
The component is completed_pending_census; strict census totals remain 116/151
with 35 uncredited records. The common-DLL census gate was not relaxed. Both
FireArcs and Nebula component evidence await that shared refresh. Candidate is
isolated under nebula-renderer/candidate; prior artifacts/checkpoints are retained.
This was an incremental build with authenticated native object cache reuse.
No deployment/game launch occurred; GPU and in-game acceptance remain pending.

## Checkpoint — 2026-10-27: A2FOFireArcs component conversion complete

FireArcs portable policy conversion is complete at source/build/headless level.
Rust owns the remaining text/config control, runtime and class admission,
fail-open geometry routing, native weapon-vector bounds, UI colour refresh and
selection, and x87-compatible radius/colour calculations. Native hooks, binary
signatures/offsets, checked object access, STL/stream lifetimes, CRT calls and
exception scopes retain their C++ boundaries.

Passed: 203 Rust tests, formatting, host and i686 Windows Clippy, both release
archives, integrated and standalone DLL builds, 168 original/current native
pairs (336 executions, 7,601 equal trace lines), 1,656 numeric
rows across all 12 x87 precision/rounding modes, and four existing built-in
lifecycle cases. Five native allocation failure and guard cases also match.
Active fixture scenarios require successful initialization and
an explicit completion marker. The original module uses authenticated frozen
headers and archive10's independent dependency archive. All 30 source-referenced
Rust implementations are linked. Integrated 197 and standalone 3 public
exports/ordinals are unchanged; no new system imports. Seventeen native
functions/classes, constants, and five native headers/assembly files are pinned
unchanged. The incremental build reuses unchanged native object cache.

Evidence: `build/rust-migration-stage10/fire-arcs/checkpoint-v2/validation.json`.
The component is `completed_pending_census`. Strict common-DLL census credit
remains 116/151, with 35 uncredited records; this component awaits that shared
refresh. No strict verifier binding or historical receipt was weakened or
re-credited. The previous A1/SOL and Resources artifacts remain preserved.
No deployment or game launch occurred; in-game acceptance remains pending.

The latest bounded continuation is the Resources presentation slice, recorded at
`build/rust-migration-stage10/resource-presentation/checkpoint/validation.json`.
Rust controls local-team admission and translated/literal/default text selection;
C++ retains the game calls and native string, lock, and exception lifetimes.
All 192 Rust tests, host/Win32 Clippy, release archives, an incremental integrated
DLL build, 37 new native assertions and 160 existing resource comparison lines
pass. The 197 public exports/ordinals are unchanged, with no new system imports.
The build reuses 258 unchanged MMM native objects with recorded provenance.
Archive10 and the canonical release artifacts remain preserved. This adds no
whole-unit credit: 116/151 verified units, 35 uncredited; MMM/FFmpeg replacement
is outside that count. Deployment and in-game acceptance remain pending.

The requested destination is a Rust implementation of A2FOExtensions.dll,
including eventual replacement of the third-party MMM and FFmpeg code. This is
an implementation migration. Wrapping an unchanged C++ DLL in Rust does not
meet that goal.

## Current boundary

The expanded [implementation ledger](../rust/expanded_migration_inventory.json)
tracks **151 unique implementation units**. It replaces 52 aggregation wrappers
with their included module implementations and counts the shared CameraControls /
ViewDistance runtime once. The previous 152-file ledger remains historical;
aggregation wrappers receive no migrated credit.

The current strict census verifies **115/151 units (76.16%)**. SOLMaps adds one
completed unit to the stage-seven checkpoint of 114/151 (37 implementations
added to the previous 77). The 152-file ledger remains historical. This is unit coverage,
not source volume, effort, complete modules or total DLL runtime readiness.
The census was frozen prospectively in
`build/rust-migration-stage7/expanded-census-baseline.json` and replaces aggregation
wrappers with actual implementations; the shared camera/view runtime counts once.

The first nine units are:

| Component | Rust implementation | Temporary C++ boundary |
| --- | --- | --- |
| Built-in identities, path identity, activation generations | `rust/a2fo_core/src/builtin_state.rs` | `core/builtin_state.cpp`; caller-owned aligned state storage |
| Module names, info.ini parsing, inherited selection, selection rewriting | `rust/a2fo_core/src/module_policy.rs` | `core/module_policy.cpp`; STL conversion, Win32 discovery and file persistence |
| Resupply target normalization, rule parsing and precedence | `rust/a2fo_core/src/resupply.rs` | `modules/A2FOEnergySystems/resupply_policy.cpp`; caller-owned maps |
| Energy/ammunition normalization, consumption, recharge and reload | `rust/a2fo_core/src/energy.rs` | `modules/A2FOEnergySystems/energy_systems.cpp`; value conversion |
| Seeded wreckage selection and chance validation | `rust/a2fo_core/src/wreckage.rs` | `modules/A2FOWreckage/wreckage_policy.cpp`; borrowed string views |
| HybridBuild production commands, queue/list decisions and submenu parsing | `rust/a2fo_core/src/hybrid.rs` | `modules/A2FOHybridBuild/{hybrid_production,build_submenu_config}.cpp`; caller-owned storage |
| DDS metadata, mip validation and D3D8 pixel/upload conversion | `rust/a2fo_core/src/textures.rs` | `renderer/textures/{dds,upload}.cpp`; output storage and diagnostic strings |

These C++ boundaries contain adapters and retained platform operations. The
old policy algorithms are no longer used in production. The Rust crate has no
external Cargo dependencies and links statically into the same 32-bit Windows
DLL, so there is no additional runtime Rust DLL to install.
Production uses `no_std` with Rust's `core` and `alloc`. A checked allocator
uses the C runtime already linked by the host; it supports arbitrary requested
alignment and retains its own allocation headers. The Rust standard runtime
is not linked, avoiding its additional Windows API imports.

C++ retains the DLL entry point, native hooks and layouts, Win32/COM/GDI/D3D
operations and resource ownership. Selected module and renderer workflows now run
in Rust; 37 implementation units, native assembly and the MMM/FFmpeg replacements
remain. This is not a complete Rust DLL replacement or full game acceptance.
The starting owned-source census was 152 C++ files / 42,816 lines and 44 assembly
files / 3,309 lines, excluding headers and third-party code. MMM adds 295 build
objects; FFmpeg adds static SDK libraries. These counts describe scale, not a
percentage of semantic completion.

## ABI and lifecycle rules

The public module SDK and exported DLL interface remain the compatibility
contract. Private Rust boundaries use C calling convention, byte slices,
fixed-width integers, POD records, and synchronous visitor callbacks. Strings
retain ANSI bytes, embedded NULs where the original API permitted them, and
existing case and whitespace behavior. Module policy retains the C runtime's
locale-sensitive classification to match its original C++ implementation.
Resupply and built-in identity use ASCII rules as before.
Wreckage chance parsing continues to use the same CRT `strtof`, preserving
its numeric spellings, locale behavior and embedded-NUL handling.

The Windows numeric policies contain small Rust inline-assembly
x87 kernels. Ordinary Rust/SSE or widened f64 expressions can change a final
float bit, or a seeded probability decision under 24-bit x87 precision.
The kernels preserve the caller's control word, declare all eight x87
registers clobbered and balance their register stack as required by the
[Rust assembly contract](https://doc.rust-lang.org/reference/inline-assembly.html#rules-for-inline-assembly).
The comparisons include 24-, 53-, and 64-bit precision modes. No engine hook
addresses or external assembly bridges were changed for these ports.

STL containers and Rust allocations never cross the boundary. A callback must
copy any bytes it retains before returning. C++ callbacks catch exceptions
inside their own frames and either return a failure or rethrow after Rust has
returned. Release Rust builds use `panic = "abort"`; no Rust panic may unwind
through the C ABI. An aborting DWARF personality satisfies the prebuilt
core/alloc unwind metadata on the explicitly supported i686 Windows GNU and
x86_64 Linux targets. Its ABI follows the corresponding
[Rust personality declaration](https://github.com/rust-lang/rust/blob/main/library/std/src/sys/personality/gcc.rs).
Adding a target requires reviewing that boundary. Rust allocation failure
aborts the process; the old resupply parser's C++ allocation-failure catch does
not extend to Rust allocations.

BuiltinState owns zero-initialized, writable, eight-byte-aligned storage.
Only Rust accesses that storage after publication, through AtomicU64 with the
original acquire/release transitions. C++ does not reinterpret its own atomic
objects as Rust atomics. First preparation fixes availability; repeated saves
do not change activation generations. All 24 live and 24 startup-only choices
retain their existing gates. Menu changes never unload or repatch native code.

The current ArmadaL.exe/FleetOpsHook.dll compatibility profiles and signature
checks remain in force. Changing implementation language does not make future
engine layouts automatically compatible.

## Building and checking

The initial integration uses Rust/Cargo 1.96.0, `i686-pc-windows-gnu`, and the
existing MinGW compiler. Install that Rust target if absent:

```sh
rustup target add i686-pc-windows-gnu
make rust-policy-tests
make build/rust_builtin_state_test.exe build/rust_resupply_policy_test.exe
make build/A2FOExtensions.dll verify-builtins
python3 tools/check_rust_migration_parity.py
python3 tools/check_rust_migration_parity.py --windows
python3 tools/verify_rust_migration.py --baseline build/rust-migration-stage1/build-dll.dll
```

Cargo uses `Cargo.lock` and offline mode after toolchain setup.
`make rust-policy-tests` includes Rust tests, formatting, Clippy, the existing
threaded C++ state regression, and policy/runtime resupply regressions. The
`rust_*_test.exe` targets retain assertions for 32-bit Windows verification.
The Windows differential run uses a headless Wine prefix and regular output
files, with display and GUI drivers disabled.

The saved pre-migration implementation and working DLL copies are under
`build/rust-migration-stage1/`. The differential harness compares current
adapters against that preserved C++ implementation; it is deliberately a test
oracle, not a production fallback. The stage folder contains logs and receipts.
The legacy `tests/module_policy_test.cpp` was absent in the starting tree;
the new differential harness provides module-policy adapter coverage without
restoring that unrelated deletion.

Verified for this first stage: 12 Rust unit tests and format/Clippy checks;
1,398 differential cases on Linux and another 1,398 on 32-bit Windows under
Wine; three native policy fixtures; all 112 existing native regressions and 51
final consolidation fixture invocations. All 197 public export names and
ordinals match the saved DLL. No new system DLL imports were introduced.
Those first-stage receipts remain in `build/rust-migration-stage1/`.
The preserved second-stage candidate is `build/rust-migration-stage2/package/A2FOExtensions.dll`;
`candidate.json`, `dll-boundary.json`, and `validation.json` in that stage folder
record its hashes and authoritative results. The second-stage snapshots are
separate from the first-stage oracle and include all six newly ported files.

The second-stage comparison commands are:

```sh
make rust-stage2-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine
python3 tools/check_rust_stage2_parity.py
python3 tools/check_rust_stage2_parity.py --windows
```

The comparison runner rejects stale Rust archives. It checks both the
HybridBuild decision traces and exact store/result bits; Windows additionally
checks the existing DDS/upload ABI and byte payloads. The existing renderer DDS
and upload fixtures run headlessly without creating a D3D device or window.
The new texture planners allocate no Rust memory, retaining C++ transactional
output and allocation-error returns.

Second-stage validation passed with freshly built archives:

| Comparison group | Linux cases | 32-bit Windows cases |
| --- | ---: | ---: |
| Previous three implementations | 1,398 | 1,398 |
| HybridBuild queues, lists, commands and submenus | 1,424 | 1,424 |
| Energy stores and seeded wreckage | 18,009 | 26,139 |
| DDS, mip validation and texture upload | Windows API | 1,153 |

All transcripts matched their saved C++ originals. Fifteen Rust unit tests,
formatting and Clippy passed, together with 112 native regressions, 51 final
consolidation invocations, and five additional headless parser/policy fixtures.
The DLL retains all 197 export names and ordinals and introduces no system DLL
imports. The private Rust symbols for all six new implementations are checked
in the linked artifact. The installed DLL remains unchanged.

## Stage three: 25% by implementation-unit count

| New group | Units | Rust implementation |
| --- | ---: | --- |
| FPQ/ODF paths, race/team policy, editor ODF, upgrade pods, nebula suffixes, refit policy | 8 | `config.rs` |
| Squadron configuration/economics, texture variants, firing cycles, identity/icons, shield display/fill, resources | 8 | `gameplay.rs`, `legacy_float.rs` |
| Monitor selection, load bounds, setup data, scene catalogue/composition, skin, loading theme | 7 | `ui.rs` |
| D3D8 tokens, exact VS1.1 translation, FVF/fans, fallback/shield visibility, editor state | 6 | `lead.rs` |

Each selected C++ unit now adapts its existing public interface to Rust.
Temporary boundaries retain STL/output storage, filesystem operations, formatting
and CRT number/locale primitives. Original public headers remain byte-for-byte
unchanged. No engine addresses or native hook bridges were altered.

Fresh archive comparisons against the preserved C++ implementations passed:

| Group | Linux transcript lines | 32-bit Windows transcript lines |
| --- | ---: | ---: |
| Configuration | 1,363 | 1,363 |
| Gameplay and numeric boundaries | 5,464 | 14,712 |
| UI and filesystem policy | 234 | 234 |
| Shaders, FVF, fallback/shields, editor state | 3,815 | 6,500 |

The Windows comparisons include 24-, 53-, and 64-bit x87 precision modes.
Small Rust kernels preserve the original float/double spills and extended
constants. A focused three-row build-time case guards double-accumulator
rounding; embedded-NUL boolean parsing and exact HLSL output are also compared.
The previous stages were rerun too: **84,630 differential transcript lines**
matched across both platforms in total.

Formatting, host and Windows Clippy, 15 Rust unit tests, the combined DLL/module
build, 112 native regressions, 51 consolidation invocations and 13 focused
headless fixtures passed. The latter cover shield layout, four Refit policies,
shader tokens, captured VS1.1 translation, FVF/fans, DDS/upload, state and resupply.
All 197 public exports and ordinals remain unchanged; all 38 representative
Rust implementations are linked, and there are no new system DLL dependencies.
Missing legacy fixture sources were left untouched; differential fixtures
provide the new policy coverage.

The preserved third-stage candidate is
`build/rust-migration-stage3/package/A2FOExtensions.dll` (build
`20261024-rust-units25`). `candidate.json`, `dll-boundary.json` and
`validation.json` in that folder's parent record its hashes and results. The
installed DLL and both preserved DLL snapshots remain hash-verified and unchanged.
No game was launched. Native in-game acceptance remains pending.

To repeat the policy comparisons:

```sh
make rust-stage3-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine
```

At the third-stage checkpoint, 114 tracked C++ units, 44 assembly files and the
MMM/FFmpeg dependencies remained.
No complete feature-module runtime is claimed fully Rust. Replacing the retained
C++ boundaries and third-party implementations remains part of the destination.

## Stage four: maps, target rules and numeric policy

Six more implementation units now use Rust:

- A1 BZN front matter, mission/object-tail detection, MDF parsing and world coordinates.
- SOL record parsing, map metadata, objects and runtime offsets.
- Fire Arc configuration and turret inherited-target transitions.
- Build-time reconciliation and cost-token formatting.
- Decal XYZ rotation.

The six public headers and native hook call sites remain unchanged. SOL retains
C++ file operations, output storage and `std::stoul` as a primitive callback;
Rust owns record scanning, validation and error/output sequencing. Token
formatting is implemented directly in Rust, including buffer truncation and
terminators. All original algorithms were removed from the six production C++
files, leaving their temporary adapters and platform operations.

| Group | Linux transcript lines | 32-bit Windows transcript lines |
| --- | ---: | ---: |
| A1 map/MDF policy | 383 | 1,017 |
| SOL format | 1,767 | 1,767 |
| Fire Arc and turret policy | 6,885 | 6,885 |
| Build-time text and decal geometry | 4,841 | 13,241 |

All 36,786 new transcript lines matched the preserved originals. The map tests
cover truncation, malformed records, partial output, NaN/infinity and retained
fields; SOL includes a bounded mutation sweep. Windows numeric checks include
24-, 53- and 64-bit x87 precision modes. Small Rust kernels preserve original
float spills, extended constants, coordinate evaluation and rotation rounding.
Review found and resolved a NaN operand-order difference in map bounds.

The earlier stages were rerun against the same fresh archives: **121,416 total
differential transcript lines** matched across both platforms. Formatting,
host/Windows Clippy, 15 Rust tests, the combined DLL/module build, 112 native
regressions, 51 consolidation runs and 14 focused headless fixtures passed.
All 197 public exports/ordinals are preserved; representative Rust symbols for
all 44 migrated units are linked, with no new system DLL dependencies.

The preserved fourth-stage candidate is
`build/rust-migration-stage4/package/A2FOExtensions.dll` (build
`20261024-rust-stage4`). Its parent folder contains `candidate.json`,
`validation.json`, `dll-boundary.json`, source hashes, patch and review receipts.
The installed DLL and preserved source/DLL snapshots remain hash-verified and
unchanged. No game was launched; in-game acceptance is pending.

```sh
make rust-stage4-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine
```

At the fourth-stage checkpoint, 108 tracked units still awaited porting. Temporary C++ adapters, 44 assembly
files and MMM/FFmpeg replacement also remain. No complete feature-module runtime
is claimed fully Rust.

## Stage five: 50% of the tracked implementation units

Thirty-two further units now use Rust-owned algorithms and workflow:

- Eight core units: builtin lifecycle/startup policy, extension roots, module
  loading, monitor policy, checked hook construction and nebula emissive policy.
- Thirteen module units: collective XP and squadron state, swarm/turret/fire-arc
  mathematics, M3D conversion and seven Refit presentation/capture/loading units.
- Eleven renderer units: D3D11 resource/probe handling, compressed D3D8 resources,
  state blocks, catalog/shader caching, drawing, refraction and the D3DX8 bridge.

C++ retains Win32/COM/GDI calls, STL resource storage, exact SDK calling
conventions and checked engine thunks. Private C-compatible POD/callback headers
carry borrowed data synchronously; no Rust/STL object layout crosses the ABI.
The selected units have no production C++ algorithm fallback. The native
assembly and vendored libraries remain separate migration work.

All 178 pre-existing headers except the separately excluded build identity
header are byte-identical, and all 476 frozen input files remain hash-verified.
Windows numeric comparisons exercise 24-, 53- and 64-bit x87 precision. Rust
expression kernels retain legacy float spills, extended square roots and XP
accumulation/assignment rounding; the latter was checked at level-up boundaries.

The new suites matched 17,858 differential transcript lines on Linux and
32-bit Windows. All earlier stages were rerun with the same final archives:
**139,274 comparison lines match overall**. Six offscreen menu,
setup and loading BMP pairs are byte-identical. Skin tests match 576 blend and
3,072 tint cases. D3D, monitor, backdrop and PE-hook tests use mocked native
operations and check failure/cleanup/call order; they do not run a real device,
application window or game. Offscreen Wine timings varied between trials and
do not establish an FPS or stable performance improvement/regression.

Formatting, host/Windows Clippy, 15 Rust tests, the full combined DLL/module
build, 112 native regression invocations, 51 consolidation runs and 14 focused
headless fixtures pass. The DLL retains all 197 public exports and ordinals,
adds no system DLL imports, and links representative Rust entry points for all
76 migrated units. Independent source review covered the new critical native
boundaries; its D3DX exception-status finding was corrected and tested.

The current candidate is
`build/rust-migration-stage5/package/A2FOExtensions.dll` (build
`20261024-rust-units50`). Its parent holds `candidate.json`, `validation.json`,
`source-audit.json`, `source-hashes.json`, `source.patch`, `review.json`,
`dll-boundary.json`, the frozen inputs and individual test receipts.
The installed game DLL is unchanged. No game was launched; native GPU and
in-game acceptance remain pending.

```sh
make rust-stage5-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine
```

At the stage-five checkpoint, **76 tracked units, 44 assembly files and
MMM/FFmpeg replacement remained.**
Temporary C++ native adapters remain; this checkpoint claims no entire feature
module fully rewritten in Rust.

Headless checks are evidence of build, ABI, and fixture behavior. Native
in-game acceptance remains a separate manual step. The installed stage-ten DLL
is preserved while this migration is validated.
Manual acceptance should cover a full restart, the existing mod's module list,
live selection saves, restart-only choices across relaunch, and an existing
resupply scenario after the candidate is deliberately installed for testing.

## Stage six: directional shield policy

Directional-shield arc selection, policy validation, capacity/current totals,
effect decisions, initialization and recharge/damage reconciliation now run in
`rust/a2fo_core/src/shields.rs`. The production C++ unit contains only adapters;
its public header, engine hooks and assembly bridges are unchanged. The private
C ABI uses fixed-width values and borrowed float arrays, with no allocation or
shared Rust/C++ object layout. Both the combined core DLL and standalone shield
module link the Rust implementation.

Exact comparisons against the frozen C++ source pass **2,361 Linux traces and
7,083 32-bit Windows traces**, including 24-, 53- and 64-bit x87 precision.
Coverage includes all public policy functions, sector seams, invalid enums,
null optional arguments, signed zero, NaN/infinity, random transforms/stores,
and repeated damage/recharge sequences. Repeated calls preserve the x87 control
word and stack. Small Rust x87 kernels retain the original expression and float
spill boundaries. The pre-existing missing `tests/directional_shields_test.cpp`
was left untouched; the new differential fixture supplies policy coverage.

The earlier migration comparisons, Rust checks, combined DLL and standalone
shield builds, 112 native regressions and 51 final consolidation invocations
pass. All 197 public exports
and ordinals are preserved, no system DLL imports were added, and 165 old/new
private Rust entry points are verified in the linked core. Independent source
review and independent host/Windows comparisons found no confirmed defect.

The candidate is `build/rust-migration-stage6/package/A2FOExtensions.dll`
(build `20261025-rust-shields`). Its parent contains frozen inputs, parity and
ABI receipts, source hashes/patch, review and validation records. **77 of 152
units are migrated; 75 units, temporary adapters, 44 assembly files and
MMM/FFmpeg replacement remain.** This does not claim a complete module runtime
port. The installed game DLL is unchanged; no game was launched. Manual
acceptance must exercise each shield layout, seam impacts, collapse/impact
effects, recharge and damage after a deliberate candidate installation.

```sh
mkdir -p /tmp/a2fo-shields-wine-prefix
make rust-stage6-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-shields-wine-prefix
```

## Remaining implementation sequence

1. Port the remaining deterministic policies, formats, configuration, caches,
   and state machines; remove each superseded production C++ algorithm after
   differential and fixture checks.
2. Move module runtime ownership, dispatch and the DLL entry point into Rust,
   preserving startup order, callback lifetimes, off gates and shutdown rules.
3. Port checked native hooks, calling-convention bridges and renderer code in
   individually verified groups. Retain exact x86 instructions or explicit
   foreign-ABI bridges until each Rust replacement has evidence.
4. Replace MMM/mission dependencies with Rust implementations that preserve
   mission scripts, Lua-facing semantics, the MMM loader contract and mission
   lifecycle. Keep existing mission assets intact.
5. Replace FFmpeg functionality with Rust implementations covering the video
   and audio formats actually used, including decoding, timing, seeking and
   shutdown. Preserve existing fixtures as acceptance criteria.
6. Remove the temporary adapters and C++ build dependencies once their final
   consumers have moved; verify exports, compatibility profiles and game
   behavior before replacing the working installed DLL.

Third-party replacement is part of the destination, not an optional exclusion.
No replacement codec or mission VM has been selected or claimed implemented.


## Stage seven: expanded 75% checkpoint

The expanded ledger credits 114 of 151 actual implementation units. Stage seven
adds 13 A runtime sources (including the shared camera/view header), 11 B runtime
sources, six bridge/native UI sources and seven renderer sources. Native C++
operations remain behind borrowed POD/callback boundaries. Independent module
controls, public SDK types, native hook addresses and assembly bridges are retained.

The saved candidate is `build/rust-migration-stage7/package/A2FOExtensions.dll`.
Its full SHA-256, 31 standalone DLL build hashes, installed-file comparison and
checks are in that directory's parent `candidate.json` and `validation.json`.
It retains all 197 export names/ordinals and adds no system DLL imports. The
installed DLL is unchanged; no game or real GPU device was launched.

The 30-group final matrix pins the same host/Win32 Rust archives and all input
source hashes before and after execution. It includes full native frozen/current
module and COM mock workflows, partial failures, callback exceptions and mutation.
The UI renderer alone matches 15,075 traces, including cache eviction, buffer
growth, textured drawing, payload validation and state mutation. Native review
found and resolved compressed-mip bounds before hashing, animation registration
exception order and draw-state resampling after callbacks. The final native
regression pass also restored admission before pointer reads in seven adapters,
InfoIni input/output alias order, and HWND sampling after the resize gateway.
Guard-page and disabled poison-pointer cases now cover those admission paths.
Native adapters must preserve admission before constructing Rust call arguments;
borrowed state must be sampled after a callback when the original reads it then.

Earlier numeric fixtures used strict C++17 while production uses GNU++17/O2.
Stage seven corrects the observed excess-precision differences in shields,
rotation, movement/arc math, squadron totals, centered fills, rebuild scaling and
tinting. These fixes receive no additional coverage credit. Pinned production
assembly and native comparisons cover PC24/53/64 where relevant; unmasked x87
traps and live game/GPU acceptance remain outside fixture acceptance.

Repeat the current milestone's headless checks with the preserved snapshots:

```sh
make rust-stage7-checks A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-shields-wine-prefix
python3 build/rust-migration-stage7/run-prior.py --platform host
python3 build/rust-migration-stage7/run-prior.py --platform windows
```

`final-parity.json`, `dll-boundary.json`, `expanded-census-verification.json`,
`source-audit.json`, `source-hashes.json`, `source.patch`, `review.json` and the
regression logs preserve the evidence. Earlier stage packages remain historical.
A normal full-restart game test is still required before deployment acceptance.


## Stage eight continuation: 26 October 2026

The current work-in-progress build is `20261026-rust-stage8-wip`. Pending Turrets,
NebulaRenderer, A1 officer and Hybrid state Rust implementations are registered
in the canonical crate. Race rendering policy and faction mesh/node application
in TextureVariants now run through Rust; that implementation unit remains
partial because its ODF registration and subsystem visual workflows are still
C++. Post-callback native row reads and node-flag read order are preserved.

The checkpoint directory is
`build/rust-migration-stage8/resume-20261026/`. Its held-archive native comparisons
cover 193 Hybrid state trace lines, 72 A1 officer trace lines plus two expected
native termination cases, 64 Turrets standalone/builtin comparisons, 1,225
NebulaRenderer trace lines, and 16 TextureVariants modes. The fixtures include
in-place callback mutation, missing entities, lookup/cache precedence and native
exception containment. The Nebula fixture now resets its generated filesystem
between frozen/current binaries. Frozen `trim()` slash normalization remains
unchanged; a proposed removal was rejected by native comparison.

The combined DLL and four affected standalone modules build. Nineteen Rust unit
tests, formatting, host/Win32 Clippy, 197 public export names/ordinals, all 1,251
required private Rust symbols, and unchanged system imports have been checked.
See `final-dll-boundary.json`, `canonical-archive-inputs-final.json`, the native
child receipts and `review.json`. All 12 lint-dependent comparison groups, all
30 prior-stage parity groups, 112 native regressions and 51 consolidation
invocations passed. A final C++-only exception correction then passed the full
16-mode TextureVariants matrix and seven built-in visual modes. Native callbacks
retain the original exception with `exception_ptr` and rethrow only after Rust
returns; the new typed allocation-failure fixture verifies type and lifetime.
`validation-pipeline.json` preserves the broad pre-correction pass, and
`validation.json` links the final focused checks. These checks do not establish
whole-stage source completeness or game/GPU acceptance.

The official ledger remains **114/151**, the last completed checkpoint. Core,
A1 compatibility, CraftIdentity, Hybrid runtime, core nebula rendering,
TextureVariants, RGBTextures and Video still need source work or completion
review. MMM/FFmpeg replacement and native ABI/platform migration remain beyond
the owned implementation census. No additional unit credit, deployment or DLL
cleanup is claimed by this continuation. All 69 files in the saved installed
inventory and 61 frozen SDK/assembly/export-definition inputs remain unchanged.

## Stage nine integration: 26 October 2026 (active)

The continuation now has an archived, buildable checkpoint at
`build/rust-migration-stage9/checkpoint-archive4/`. It is still a partial rewrite;
the verified census remains **114/151**. Core first-run configuration and shell
policy, TextureVariants, RGB file/cache handling, video sequencing, CraftIdentity
UI workflows and sprite/anchor scopes, A1 configuration, Hybrid menu workflows,
and initial core-nebula resource/cache controls now have additional Rust control
behind synchronous native adapters.

The checkpoint passed the full `make -j2 release` build, host and i686 Windows
Clippy, formatting, and 65 Rust tests. Ten selected frozen/current native groups
passed against the same canonical Rust archive. These include 53 Texture modes,
75 RGB trace lines, 91 Craft trace lines, Hybrid normal and log-failure cases,
49 A1 configuration lines, 14,431 core-nebula lines, and the core ParameterDB,
shell, and configuration matrices. `native-evidence.json` preserves their
receipts. Broader prior-stage regression refresh and remaining source work are
still required before completion credit or deployment.

Native comparison caught an optimized read-order difference in mesh selection:
the frozen GNU++17/O2 scan retains its initial vector data/count across
allocation, so the adapter now retains that same snapshot. Sprite fixtures also
check the icon's live cache-slot reference, the bar's copied pointer, their
different restore orders, selected-anchor callback order, and native allocation
failure behavior. No whole-game, GPU, or native window acceptance is implied.

The full standalone build exposed missing Rust archive links in five existing
module recipes. Breeders, Infestation, MissionSelector, Video, and RGB now link
the archive; their dependency rules and AlwaysShowShields' archive dependency
were repaired. The resulting core DLL is SHA-256
`072c6b8567428d482126de3ea2bddf29a328123c410214c725ad5cbd11ddc262`,
with the same 197 public exports and DLL imports as the prior WIP image, and
1,393 private Rust functions. All 61 pinned SDK/assembly/export-definition
inputs still match. The Windows Rust archive is SHA-256
`b7b24fc36c40e999415ac5bd1a89b9ab67166db70af8c80c14c2248ddec15693`.

No game deployment, cleanup, move, or deletion was performed. The installed
core remains SHA-256
`8475c0450d889a94942f894daad128b929fd65b55cb0b4691d1675ae023a5b1e`.
A fresh check found the old 69-file installation inventory is stale: 18 original
paths match, while 51 old `Data/modules/...` paths are absent. Existing receipts
do not establish the removal's provenance; the unrelated old
`modules-disabled` directory does not contain them. Do not recreate or restore
those paths from this audit alone. See `installed-readback-resolved.json` and
the pending user clarification.

Renderer-options and module-menu workflow slices are integrated in the newer
checkpoint below. Native ABI/platform migration and MMM/FFmpeg replacement
remain open; build-only evidence or a Rust callback alone must not be credited
as a complete implementation unit.

### Integrated archive five

`build/rust-migration-stage9/checkpoint-archive5/` preserves the next partial
checkpoint. Rust now also directs renderer settings file comparison, backend
selection and hook sequencing, plus module-menu shortcut COM sequencing,
recent-desktop selection and live list population. Native strings, COM objects,
Windows operations, hook addresses and ownership remain in the adapters.
The official verified ledger remains **114/151**.

Formatting, host/Windows Clippy, 65 Rust tests and the full release build passed.
All twelve stage-nine comparison groups passed against one Windows archive,
including the new renderer clean/Roots layouts and 583 module-menu trace lines.
The integration refresh also passed all 30 stage-seven groups, five stage-eight
groups, 112 built-in checks and 51 consolidation invocations. Input hashes held
through these runs. `validation.json` and the preserved receipts record scope.

The root host aggregate references tracked-deleted historical tests and a tool.
Those deletions were preserved. An isolated overlay copied present inputs and
recovered 40 HEAD blobs there; its full C++ aggregate and 18 Python tests passed.
Two separately absent CollectiveXP host tests were added, and fifteen missing
module README catalogue links were repaired. This is recovered-test overlay
evidence, not a claim that the unchanged root `make test` target succeeds.

The DLL is SHA-256
`a9dc51dc6acf7d804a07487365285e55de442d5a444daf6b9958bd13eeadfb85`;
the Windows Rust archive is
`82cef4df5c99d80aa8fa5e59fca59ef5edefa8bf0bdcdf11f75e0a78c50c0093`.
The proper PE comparison verifies all 197 public export names and ordinals
against the prior stage-eight candidate, with no added system imports. It
supersedes the archive-four ad-hoc receipt's incorrectly parsed export-name
field. All 61 SDK/assembly pins still match. No deployment, game launch or
installation cleanup occurred; the installation inventory caveat above remains.

The next directional-field parser and x87 resupply-range changes are staged
separately pending native comparison and canonical integration. They receive
no completion credit from the archive-five checks.

### Integrated archive six

`build/rust-migration-stage9/checkpoint-archive6/` preserves the integrated
DirectionalShields field/suffix parser and EnergySystems resupply-range kernel.
The parser retains first-valid-duplicate selection and native `strtof`, string
allocation and typed exception behavior. The range kernel preserves the original
GNU++17 x87 instruction order, including its unordered comparison and exception
flags. Native addresses and admission checks remain at the C++ boundary.

The release build, formatting, both Clippy targets and 69 Rust tests passed.
Canonical frozen/current checks passed all 45 directional modes, 106 energy
modes, six parser trace rows and 6,600 range cases under twelve precision/rounding
combinations. The rebuilt integration passed 112 built-in checks, 51
consolidation cases and the isolated recovered-test host aggregate, including
18 Python tests. The earlier 30/5-group refresh belongs to archive five; it was
not rerun for this narrow change. All 197 exports/ordinals and system imports
are unchanged, as are the 61 native-boundary pins. No deployment occurred.

The DLL SHA-256 is
`f99d1f9f35d1b18ff20f37fc89b51424abe5dec1e8e1ba4cde5fc22618c36794`;
Windows Rust archive SHA-256 is
`a40ebbd9ccc49e308ad7ad116a33759e9d432c5f7ea1d19797188c5942b43a37`.
The ledger remains **114/151**: other policy inside those modules, native
platform migration and third-party replacement remain. A1 mission scanning
and further SOLMaps policies are staged separately, without completion credit.

### Batch seven integration (2026-10-26)

A1 mission-record scanning and additional SOL source, presentation, counter and
world-bounds policy now run in Rust behind private C interfaces. The canonical
release, formatting, host/Windows Clippy and 80 Rust tests passed. Frozen/current
full-source native comparisons matched 36 A1 trace lines and 7,128 SOL trace
lines, including 6,228 world-bounds rows under twelve x87 precision/rounding modes.
The current build also passed all 30 stage-seven groups, 112 built-in checks,
51 consolidation cases and the isolated recovered-test host aggregate. All 197
exports and ordinals remain equal; no imports were added; 61 native pins match.

The refreshed strict census verifies **114/151 units (75.4967%)** and 474 linked
Rust symbols against the current DLL. No whole SOL or A1 unit credit was added:
SOL discovery logging and directory classification still require the staged
port and expanded native comparison. The new staged helpers pass three direct
Rust tests; their 23-case dispatch fixture has no native execution evidence yet.
Native/platform and third-party work remain separate from this unit count.

DLL SHA-256: `4cfd192736b9ef8db475f0fdc728680b3914e5ddb2522a39897fe3199a2edbcb`.
Windows Rust archive: `0f9d83fc0010068673fd6bcffab465533df57e32540b57b2fad2bfa61348d137`.
Evidence is under `build/rust-migration-stage9/batch7-*`; continuation details
are in that directory's `CONTINUE.md`. No deployment or game launch occurred.

## Current checkpoint — 2026-10-27: SOLMaps completion and A1 continuation

SOLMaps is credited as a completed Rust implementation unit with native adapters
retained for ABI, checked memory, IO, native object construction, STL/CRT lifetime,
and hooks. The strict census now verifies **115/151 units (76.16%)**, with 535
required Rust symbols linked. A1 remains incomplete and receives no whole-unit
credit; its integer/text UI helpers are integrated, with other slices staged.

Batch8 passes 89 Rust tests, host/Win32 Clippy, fmt, full release and confirmed
archive-to-DLL relink, 197 exports/ordinals with no new imports, SOL native parity
(7,325 normalized lines, including 6,228 bounds rows across 12 x87 modes), all
30 prior native groups, 112 builtin checks, 51 consolidation runs, and the host
overlay. The first broad regression attempt was killed with signal 9 in the
frozen baseline; its no-drift evidence is retained separately from the passing
retry. No cause was established.

Receipts are under `build/rust-migration-stage9/batch8-*`; the immutable
`checkpoint-archive8/validation.json` preserves this candidate before A1 changes.
No deployment or game launch occurred. In-game acceptance remains pending.


## Checkpoint — 2026-10-27: A1 combined Rust integration

Fourteen A1 Rust slices, including shared reporting admission, are integrated.
They cover assets, class defaults, physics defaults, geometry/layout, GUI query
contracts, world bounds, speed translation, officer arithmetic, quarter traversal,
BZN decisions, SOD bounds repair, and race parsing. A1 remains incomplete and
receives no whole-unit credit; further display, palette, and workflow slices are
staged separately.

The combined candidate passes 136 Rust tests, host/Win32 Clippy and fmt, release
builds and a confirmed archive-to-DLL relink, 197 exports/ordinals with no new
imports, all 13 combined A1 comparisons (922 normalized lines plus assertions),
SOL parity (7,325 lines), A1 mission/config comparisons, 30 prior native groups,
112 builtin checks, 51 consolidation runs, and the full isolated host aggregate.
The strict census remains **115/151 (76.16%)**, with 535 required linked symbols.

The immutable evidence checkpoint is
`build/rust-migration-stage9/checkpoint-archive9/validation.json`. The source,
fixture inputs, fixture binaries, receipts, and three release artifacts are
preserved there. No installation, game, GUI, or deployment changes were made.
Source/build/headless proof does not establish in-game acceptance.



## Checkpoint — 2026-10-27: A1Compat and SOLMaps Rust completion

A1Compat and SOLMaps both carry `verified_stage9` credit. Rust owns their
portable policy and control flow. Native hooks, calling-convention bridges,
checked object access, native storage and allocation lifetimes, CRT/Win32 calls,
and rendering adapters remain in C++.

The strict census passes **116/151 units (76.8212%)**, with **715 required Rust
symbols linked**. This completes these two sections; 35 implementation units
remain outside this milestone in the wider rewrite.

Validation passed: 188 Rust tests, formatting, host and i686 Windows Clippy,
host/Windows release archives, full DLL release and confirmed link, 197 equal
public exports and ordinals, no added system imports, and 149 required A1
Stage 9 symbols. All 28 original-C++/current full-source A1 comparison groups
pass: 56 executables, 3,119 equal normalized trace lines plus assertions. Coverage
includes x87 modes, queue/tree mutations, native allocation failure paths,
resource precedence, officer state, display values and render routing. The
original module uses its authenticated frozen headers and archived dependency
library; the current module uses the released Windows Rust archive.

Fresh SOL comparison passes 7,325 lines, A1 mission scanning passes 36 lines
(18 scenarios), and A1 config passes 49 lines. All 30 prior parity groups,
112 built-in native checks, 51 consolidation checks, and isolated host aggregate
attempt10 pass. The host overlay preserves the 40 recovered historical test
files privately; canonical deletions remain intact. The A1 completion verifier
accepts the passed evidence and rejects all 15 damaged/incomplete controls.

Candidate artifacts:

- DLL: `build/A2FOExtensions.dll`, SHA-256
  `66927a8dac99a6bceb2be0c35f8914c7aed9b025dbcb88cd197b7b8097157e1d`.
- Windows archive: `build/rust/i686-pc-windows-gnu/release/liba2fo_core.a`,
  `033d931fa0c1061b26098be4403995987c7fc90e03605f2733c87e9c4d796971`.
- Host archive: `build/rust/release/liba2fo_core.a`,
  `8913e50068f2e515f0a45487fdb3c1249d2da84bae211180b125c0d3602d1bff`.

The checkpoint command captures the source, all fixture inputs/binaries and
receipts at `build/rust-migration-stage9/checkpoint-archive10/validation.json`
after rechecking every gate. Prior attempts, checkpoints and installation data
are preserved. This candidate is staged; deployment and in-game/GPU acceptance
remain separate.

Manual acceptance after a backed-up deployment requires a full game restart:
load known working A1 and SOL maps, compare officer upgrades and save/reload,
and inspect single/multiple selection crew, energy, labels and UI scaling.
Preserve fresh game logs and compare with the previous known-good build.

### Stage 14: Rust MMM loader and handoff controller

The production compatibility loader and A2FOMissions handoff state now use Rust.
See [the MMM migration contract](rust-mmm-migration.md) for preserved routing,
return values, lazy startup, exception retry and deferred unload behavior. The
retained Worker, Lua 5.1.5 VM, Luabind, native game bindings, archives and save/load
implementation remain to be replaced. This bounded change does not change the
35 pending census records or establish a complete Rust rewrite.


### Stage 15: Rust media runtime (2026-10-29)

Production A2FOVideo now dispatches modern movies into the Rust-owned
`A2FOMedia.dll`. Rust owns decoding, PCM conversion, clocks, seeking, bounded
queues, GDI/WinMM resources and worker teardown. The production build no longer
requires or links the FFmpeg SDK. Native Bink fallback and the existing module
export contracts are retained. See [the replacement evidence](rust-media-replacement.md)
for the staged candidate, headless validation, performance and manual acceptance.
This does not add census credit or establish a complete Rust rewrite; retained
MMM internals and in-game acceptance remain open. No deployment occurred.


### Stage 16: first native adapter retirement (2026-10-29)

Two production C++ units and their headers are retired: checked hooks and decal
rotation. Their final core callers now use the Rust C interface directly; three
obsolete explicit linker libraries are removed. Seventeen native/host comparison
pairs match 18,690 rows, 309 Rust tests pass, and the main 197-export plus radar
module contracts are unchanged. See [the retirement record](rust-adapter-retirement.md).
Live native/STL/COM/MMM consumers remain; census credit and in-game acceptance
are separate. This is staged work, with no deployment.

# A2FOExtensions project notes

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
See [acceptance instructions](docs/rust-rewrite-acceptance.md).

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

## Checkpoint — 2026-10-27: Resources presentation Rust slice

Continued beyond the completed A1Compat/SOLMaps milestone. Rust now controls
local-team index admission and resource presentation translation, literal, and
default selection. C++ retains native calls, the locked presentation map, string
temporaries, exception catches, and the thread-local string lifetime.

Validation passes: 192 Rust tests, formatting, host and i686 Windows Clippy,
both release archives, and an incremental integrated DLL build. The new full-source
frozen/current fixture passes 37 assertions, including allocation failures and
output aliasing; all six frozen project headers authenticate against archive10.
The existing full-source Resources regression passes 160 equal trace lines.
The candidate preserves all 197 exports/ordinals, adds no system imports, and
links the three new Rust implementations.

Evidence and an isolated candidate are under
`build/rust-migration-stage10/resource-presentation/`; its checkpoint is
`checkpoint/validation.json`. The DLL SHA-256 is
`41ed7dba76e1b52033bf16a2b744bf04b9da194af0fef162acba19c7e658d1e0`.
The build reuses 258 unchanged native MMM support objects with recorded source
and cache hashes; it is not a clean rebuild of third-party code. The original
three release artifacts and archive10 are preserved. No deployment or game
launch occurred, and in-game acceptance remains pending.

This slice adds no whole-unit credit: the ledger remains **116/151 (76.82%)**,
with **35 units (23.18%)** still uncredited, including partially converted units.
That count excludes eventual MMM/FFmpeg replacement. The next concrete Resources
slice is payment/refund orchestration, preserving the infinite-resource bypass,
lazy reads, and locked balance mutations.

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

## Checkpoint — 2026-10-26: Rust stage-eight continuation

- Integrated pending Rust ports and moved TextureVariants race/faction mesh/node
  control. Preserved postcallback reads and original C++ exception type/lifetime.
- Core plus four standalone modules build. Focused native comparisons, Rust
  checks, 30 prior parity groups, 112 native checks and 51 consolidation runs pass;
  final exception fix additionally passes all16 Texture and7 visual modes.
- WIP package and detailed receipts: `build/rust-migration-stage8/resume-20261026/`.
  197 exports/ordinals retained; no new system DLL imports. Installed inventory
  unchanged. No deployment, cleanup, game acceptance or new whole-unit credit.
- Full rewrite remains incomplete; official ledger stays114/151. Resume from
  `build/rust-migration-stage8/CONTINUE.md` and [migration guide](docs/rust-migration.md).


## Checkpoint — 2026-10-02: custom checkbox clicks (v5)

- User reported v4 seems to work and other controls seem fine, but checkboxes
  cannot be clicked. Installed log confirms main, Options and Graphics Options
  canvas registration at 1200x900 -> 1920x1080.
- Graphics/Sound checkboxes are parent-painted `TJvgCheckBox` controls. Native
  `WMLButtonUp` at FO `0x1662fc` re-queries the cursor at CALL `0x166323`, then
  Delphi `TControl.ScreenToClient` (`0xc5254`) subtracts physical parent origin
  plus logical checkbox offsets without inverse scaling. Installed disassembly
  confirms this path, and CALL bytes match all three supported FO profiles.
- v5 adapts that one stdcall cursor query during tracked release dispatch,
  returning a logical parent offset at the same physical screen origin.
  Checkbox bounds/state changes stay native. No global cursor query or native
  Windows checkbox input is changed. Foreign CALL owners disable the canvas
  before mutation; partial installation leaves callbacks inert.
- Live glyph/label clicks, state persistence and smaller-resolution checks
  remain pending. v5 is installed in Roots `Data/modules`, enabled by the
  existing selection. Deployment retains v4, v3 and the proven v2 backups.
- Warnings-as-errors build and integrated headless GDI/lifecycle/native tests
  pass. Native smoke passes all three binary profiles, including foreign
  checkbox CALL and installation-failure cases. The controlled cursor fixture
  covers glyph/caption toggles, outside misses, resize and negative origins;
  the original raw cursor path fails the regression. No GUI/input automation.

## Checkpoint — 2026-10-02: Options canvas regression (v4)

- User reported v3's menu reverted to partial-window size upon opening Options,
  and returning to the game showed the background without UI. Installed log
  showed a 1920x1080 device with an actual 1920x1060 window client.
- Native `TWinControl.UpdateBounds` at FO `0xcdbc8` reads physical window
  geometry and writes control `+0x40..+0x4c`; Options `FormShow` then uses
  cached Width/Height for button centering. v4 keeps those fields logical for
  registered controls, retains native `AdjustSize` at `0xc4c20`, and forwards
  all untracked controls. Signatures and the Delphi EAX/no-stack ABI are checked.
- Root scaling now uses actual owner client geometry. Owned popups await
  explicit registration instead of inheriting a parent's child-control scale.
  The active FO background capture is already 1200x900 and remains native.
- Warnings-as-errors build, memory GDI tests, the new synthetic USER32 Options
  lifecycle regression, and actual DLL/data-only native smoke tests pass.
  Original, clean RE and installed Roots binary profiles are covered. No game
  launch, real test HWND or input automation was used.
- v4 is installed in Roots `Data/modules` with v3 backed up; v2's proven display fix is
  still retained in the earlier v3 backup. Game/core binaries and module
  selection are retained. User reported v4 seems to work, with checkbox clicks
  failing; the v5 checkpoint records that remaining correction.
  See [deployment record](docs/shell-display-2026-10-02-deployment.json).

## Checkpoint — 2026-10-02: full-window shell canvas (v3)

- Extended A2FOShellDisplay with a logical native canvas rendered to the full
  selected window. Registration, HWND child geometry, paint DCs, dirty areas
  and inverse mouse coordinates use one scale. Bitmap button masks remain
  native; system control forwarding retains physical input and scaled fonts.
- Verified registration Delphi ABI and main-form HWND `+0x1b4` directly in
  pinned DLL instructions. VCL system forwarding sites `+0xcaeec`, `+0x9f4cb`
  are checked against installed code. The previous `+0x1c4` assumption was
  discarded before deployment. Existing windows refit on resolution changes.
- Headless GDI canvas tests pass at four resolutions including 640x480;
  distinct DCs share pixel storage and retain independent state/lifetime.
  Actual DLL synthetic import/ABI smokes pass for all three supported pairs.
  No window, game launch or input automation was used for verification.
- v2's display fix remains user-confirmed. v3's manual test reported an Options
  transition regression; the v4 checkpoint above records its correction.
  Backgrounds, hover/click alignment, lists/text entry and resolution changes
  still require live acceptance. The responsive mission selector and
  small unregistered popups keep their own paths.
- Deployment backs up the proven v2 DLL and retains game/core binaries and
  existing module selection. See [deployment record](docs/shell-display-2026-10-02-deployment.json).

## Checkpoint — 2026-10-02: shell display module

- Manual v1 launch rejected initialization because FleetOpsHook's startup
  changes the shell depth immediate from 16 to 32; the earlier data-only test
  had not modeled this. v2 accepts the evidenced live mutation, corrects the
  profile/monitor helper's three-load pointer chains and adds regressions.
  The user confirmed v2 worked after the replacement was deployed.

- Added `A2FOShellDisplay`: primary shell window/device follows the selected
  graphics resolution and framed/borderless preference through scoped native
  callbacks. Authored menu artwork/dialogs keep their existing scale.
- Warning-as-error build and synthetic Win32 tests pass against the original,
  clean RE and installed Roots binary pairs. The user confirmed the live fix
  worked on 2026-10-02; individual resolutions and the full D3D/multi-monitor
  matrix were not separately reported. No game launch was automated.
- Installed and enabled in Roots `Data/modules` and base `info.ini`, with the
  previous configuration backed up. Core and original engine DLLs were retained.
  See [deployment record](docs/shell-display-2026-10-02-deployment.json) and
  [module guide](modules/A2FOShellDisplay/README.md).

## Checkpoint — 2026-09-25

- **MMM_loader.dll source:** the project owner confirmed permission to use its
  source code for A2FOExtensions. See [permission record](docs/source-permissions.md).
- **FleetOpsHook reverse engineering:** six passes complete. Resume from the
  [session handoff](reverse_engineering/FleetOpsHook/notes/HANDOFF.md), which links
  the current evidence, validation and next priorities.
- Investigation is stopped for today at the user's request to conserve usage.
  No further analysis, source integration, build or deployment is pending today.

### 2026-10-26: Rust stage-nine active integration

Archived `build/rust-migration-stage9/checkpoint-archive4/` with a scoped
1,100-file source snapshot, canonical Windows Rust archive, core DLL, native
receipts, and export/import/boundary checks. Full release build, both Clippy
targets, formatting, 65 Rust tests, and all ten selected stage-nine native groups
passed. Main DLL SHA `072c6b85...1ddc262`; installed core remains
`8475c045...5b1e`. No deployment or cleanup. The older 69-file installation
inventory cannot prove current loose-module preservation: 51 original
`Data/modules` paths are now absent, provenance unresolved, and no restoration
was attempted. Remaining renderer-options source work is active; broader
regression refresh and the remaining owned/native/third-party rewrite remain
open. Official census stays114/151. See `docs/rust-migration.md` and stage-nine
`CONTINUE.md` for exact evidence and outstanding work.

### 2026-10-26 Rust stage-nine archive five

Integrated renderer-options and module-menu Rust workflow slices. Preserved
checkpoint: `build/rust-migration-stage9/checkpoint-archive5/`; DLL
`a9dc51dc6acf7d804a07487365285e55de442d5a444daf6b9958bd13eeadfb85`,
Win Rust archive `82cef4df5c99d80aa8fa5e59fca59ef5edefa8bf0bdcdf11f75e0a78c50c0093`.
Full release, fmt, host/Win Clippy and65 Rust tests pass. All12 stage9 native
comparison groups pass; broad refresh30 stage7 groups +5 stage8 groups +112
built-in checks +51 consolidation cases pass with held source/artifact hashes.
Proper PE parser proves197 export names/ordinals unchanged, no new imports;
all61 SDK/assembly pins unchanged. This supersedes archive4's malformed parsed
export-name field. No unit credit added (114/151), deployment or game launch.

Root aggregate host test source deletions were preserved. An isolated overlay
recovered40 HEAD test/tool blobs and passed its C++ aggregate plus18 Python
tests. Added two absent CollectiveXP host tests and15 missing README catalogue
links; no production behavior changed for that test repair. Overlay provenance,
inputs and all attempt logs preserved. Root make test still depends on deleted
files; do not restore them without separate scope.

Next staged work: DirectionalShields field/suffix parsing, EnergySystems x87
resupply-range predicate. Source reviews/build-only pass; native and canonical
integration pending, no credit. Native/platform and MMM/FFmpeg replacement remain.
Current installed core hash still8475c045...5b1e;18 old inventory paths match,
51 old Data/modules paths absent with unknown provenance. No stage9 game-file
move/delete/deploy was issued; preserve current install pending user clarification.

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


### Stage 15: Rust media runtime (2026-10-29)

Production A2FOVideo now dispatches modern movies into the Rust-owned
`A2FOMedia.dll`. Rust owns decoding, PCM conversion, clocks, seeking, bounded
queues, GDI/WinMM resources and worker teardown. The production build no longer
requires or links the FFmpeg SDK. Native Bink fallback and the existing module
export contracts are retained. See [the replacement evidence](docs/rust-media-replacement.md)
for the staged candidate, headless validation, performance and manual acceptance.
This does not add census credit or establish a complete Rust rewrite; retained
MMM internals and in-game acceptance remain open. No deployment occurred.


### Stage 16: first native adapter retirement (2026-10-29)

Two production C++ units and their headers are retired: checked hooks and decal
rotation. Their final core callers now use the Rust C interface directly; three
obsolete explicit linker libraries are removed. Seventeen native/host comparison
pairs match 18,690 rows, 309 Rust tests pass, and the main 197-export plus radar
module contracts are unchanged. See [the retirement record](docs/rust-adapter-retirement.md).
Live native/STL/COM/MMM consumers remain; census credit and in-game acceptance
are separate. This is staged work, with no deployment.

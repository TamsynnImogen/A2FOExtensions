# A2FOFeaturePack native module

The output DLL is `A2FOFeaturePack.dll`. It groups the built-in optional native
features behind the core's versioned semantic dispatchers:

- recursive loose-folder and FPQ ODF discovery;
- Ctrl-click ten-slot queue fill;
- Ctrl+Alt continuous production, using synchronized build orders and a
  save/load marker.
- bounded ship-system upgrade tiers selected by `upgradePodMaximumTier` in
  `RTS_CFG.h`;
- `tier<Tier>BuildItem<Index>` parsing for upgrade stations;
- `moduleXPseudoTechnology` gates in BuildYard configuration ODFs, checked
  alongside the native `moduleXRequiredTechnology` field.

When setting `upgradePodMaximumTier` in a child mod, retain a complete copy of
the parent's `RTS_CFG.h`. Fleet Operations shadows rather than merges this
native file, so a minimal child file would remove all other parent settings and
includes even though FeaturePack can read the new command.

The module owns the general Producer queue and ResearchStation class hooks used
by continuous production and configurable upgrade pods. Those same sites are
also needed by the optional HybridBuild runtime, so FeaturePack exports a small
callback bridge which `A2FOHybridBuild.dll` registers after FeaturePack loads.
With HybridBuild absent, every bridge callback is a safe no-op and the general
features continue normally.

`A2FOHybridBuild.dll` also owns the hybrid-only ShipDisplay dispatcher hooks
and queue post-pass. It runs Fleet Ops' compatible single-object callbacks
first, then binds the ten native `BuildWireframe` objects at
`ShipDisplay + 0x120` to the inherited Producer FIFO and any active research
pod outside that linked queue. FeaturePack supplies the shared Producer queue
callbacks and bookkeeping through the bridge; it does not install those
hybrid-only display hooks itself.
HybridBuild's implementation and validation notes now live with
[`../A2FOHybridBuild/README.md`](../A2FOHybridBuild/README.md).

The core owns and signature-checks shared injected call sites, object lifetime
tracking, and semantic dispatch. The feature pack registers only the behaviours
listed above and reads its upgrade-pod setting directly from the inherited
extension roots.

The module contains:

- FleetOps VFS RVAs and scanner signature checks
- recursive loose-folder scanning
- FPQ directory discovery
- virtual-directory registration
- hash winner calculation
- the API v2 lookup-handler implementation

The Armada 1-specific `wingman -> craft` alias is owned by the optional
`A1Compat.dll` packaged with `STA1 Classic`; it is deliberately not global
FeaturePack policy.

BuildYard configuration files referenced through a vessel's native
`moduleConf` command may give any module a symbolic technology gate:

```cpp
module0TechnologyEntry = "bor_test_module_gate"
```

`moduleXPseudoTechnology` remains accepted as a compatibility alias. The gate
is declared in an XML technology-tree file with `MODULE=` rather than `ODF=`:

```xml
<object MODULE="bor_test_module_gate.odf">
    <requirement>
        <obj>romR_leahval2.odf</obj>
    </requirement>
</object>
```

A `MODULE` name is owned by A2FOExtensions and therefore needs no dummy/backing
ODF and no Fleet Operations project ID. Direct `<obj>` checks the owning team's
completed-object count, excluding queued objects. Multiple objects within one
requirement are AND conditions; ungrouped requirements are OR alternatives.
`mustNotHave="true"`, `race`, `logicGroups`, and scoped `ally`/`enemy` conditions
are supported. Requirements sharing a logic group are ANDed, and groups are ORed.
Each peer block requires one eligible active team to meet every condition.

v14 uses the tree Fleet Operations actually loads for each team, including
native includes, parent includes and archives. Its two loader callsite bridges
capture borrowed XML bytes and activate the resulting rules only after native
loading returns normally. Caps, cap lists and button hiding remain unsupported
for MODULE entries. See [v14 rules and test steps](../../docs/buildyard-module-v14.md).

If the same module also declares native `moduleXRequiredTechnology`, the MODULE path ANDs
that stock gate with the symbolic MODULE gate. Omitting the A2FO command keeps
stock BuildYard behavior. A real ODF named by `TechnologyEntry` remains
supported as the older v4 recursive-project compatibility path.

The BuildYard tooltip hooks only Fleet Ops' private `GetTooltip` /
`GetVerboseTooltip` provider, using the native stream path visually confirmed
by the phase 1 TEST build. After stock text generation, an unavailable registered
MODULE slot gets `Requires: ...` with only unsatisfied conditions. Names come
from the requirement ODF's cached `unitName`, falling back to its basename.
AND/OR and `Not` preserve the XML grouping; a satisfied OR branch suppresses
all other branches. The stream is read back to verify its prefix and new bytes.
Shared tooltip keys alone do not affect ungated slots.

A concrete v11 gate defect was corrected: MODULE `<obj>` now uses Fleet Ops'
completed-object count, as native XML does, instead of checking whether the
requirement ODF is buildable. Queued objects do not count. Synthetic IDs, existing gate hooks and native RequiredTechnology compatibility
are retained. No dummy ODF is needed. See
[ABI evidence](../../docs/buildyard-tooltip-abi.md) and
[test steps](../../BUILDYARD_TECHNOLOGY_ENTRY_TEST.md).

Winner selection is calculated from explicit active/parent mod priority,
registered within-root overlay priority, primary-root precedence, and
loose-before-packed precedence. `A1Compat.dll` uses the API v4 revision 6
registry to declare `Addon` as an override-priority ODF directory; FeaturePack
queries that policy when its index is built lazily. Fleet Operations' per-entry
`overridden` flag is retained as a diagnostic but is not authoritative for
recursive entries: with a child mod active, it can incorrectly mark a parent's
recursive loose file as overridden by the packed copy from that same parent. A
genuine child-mod entry still wins through its higher mod priority.

If the module is absent or rejects an unsupported Fleet Ops build, the core
preserves Fleet Operations' original filesystem, classlabel, queue, and
upgrade-pod behaviour. HybridBuild and `info.ini` defaults have independent
module lifecycles.

Every queue address is also signature-checked. Ctrl-fill replaces the clicked
build order with one reserved synchronized typed-class marker while Armada's
network command buffer is active. Every peer consumes that marker and performs
the same Fleet Ops checked native-queue insertions. Continuous production
chains Fleet Ops' Producer callbacks, pauses and retries on resource shortage,
and stops on queue cancellation, destruction, or a different build selection.
Basic fill, automatic refill, and delete-to-cancel behaviour are confirmed in
game. The save/load and multiplayer matrix in
[`../../docs/queue-enhancements.md`](../../docs/queue-enhancements.md) must still
pass before it should be considered release-proven.

FeaturePack is also the one native owner of the Fleet Ops Producer admission,
finish, and destructor sites. Through native API revision 8 it dispatches
class-policy events registered by optional modules, including a claimable
pre-native completion event. This remains useful for consumers whose caller
accepts a non-object completion. A1 officer quarters require an earlier
Starbase-specific boundary because A2's outer `Starbase::FinishBuild` performs
object/output-queue work after the shared Producer callback. `A1Compat.dll`
therefore owns that A1-scoped Armada hook and does not compete at FeaturePack's
Fleet Ops RVAs.

Native API revision 9 also lets HybridBuild's already-owned Armada Producer
construction-effect hook dispatch a claimable pre-effect event. This keeps
non-Craft legacy policy classes out of the cosmetic Craft renderer without
moving or duplicating the shared native hook.

The upgrade-pod hooks retain extended tier identity in sidecar state while
feeding tier 3 to Armada's fixed Team arrays. The highest attached tier for a
team/system supplies the effective multiplier. Configuration, ODF examples,
and the pending manual validation matrix are in
[`../../docs/upgrade-pods.md`](../../docs/upgrade-pods.md).

### v8 tooltip route

v8 hooks Armada's final `Tooltip::Render` boundaries for Fleet Ops-generated
`AUTOTOOLTIP-...` module descriptions. This avoids relying on localization calls
that do not occur during Borg module hover. The v6 shared-localization hook is
retained only as a fallback for ordinary localized tooltip keys.

## Core-provided Breeders

Stage seven provides `A2FOBreeders` in the core while retaining this module's
Producer hook ownership. The startup capability export
`A2FO_ProducerEventsReady` returns true only after the queue's complete checked
hook setup succeeds. The core checks that export on the actually loaded
FeaturePack before making Breeders available. Use the updated core and
FeaturePack together when selecting Breeders; an older or absent provider
leaves it unavailable. See the [seventh built-in milestone](../../docs/builtin-modules-stage7.md).


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

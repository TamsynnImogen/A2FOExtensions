# A2FOFeaturePack native module

The output DLL is `A2FOFeaturePack.dll`. It groups the built-in optional native
features behind the core's versioned semantic dispatchers:

- recursive loose-folder and FPQ ODF discovery;
- the `wingman -> craft` classlabel compatibility alias;
- Ctrl-click ten-slot queue fill;
- Ctrl+Alt continuous production, using synchronized build orders and a
  save/load marker.
- bounded ship-system upgrade tiers selected by Lua;
- `tier<Tier>BuildItem<Index>` parsing for upgrade stations.

The module owns the general Producer queue and ResearchStation class hooks used
by continuous production and configurable upgrade pods. Those same sites are
also needed by the optional HybridBuild runtime, so FeaturePack exports a small
callback bridge which `A2FOHybridBuild.dll` registers after FeaturePack loads.
With HybridBuild absent, every bridge callback is a safe no-op and the general
features continue normally.
For these hybrid stations only, the module also restores Fleet Ops' ten
`infoBuildQueue_*` controls by intercepting Armada's Fleet Ops-patched
single-object dispatcher calls. It runs the untouched compatible callbacks
first, then preserves their result registers while applying a queue post-pass
to the ten native `BuildWireframe` objects at `ShipDisplay + 0x120`. Fleet Ops
extends those objects with the `infoBuildQueueSlot_0` through `_9` frames. The
post-pass reads the inherited Producer FIFO directly and prepends
ResearchStation's active pod when it has moved outside that linked queue.
Active and queued yard/research entries therefore display as one deduplicated
ten-slot view. It does not force ResearchStation through Fleet Ops'
incompatible builder callback.
HybridBuild's implementation and validation notes now live with
[`../A2FOHybridBuild/README.md`](../A2FOHybridBuild/README.md).

The core owns and signature-checks shared injected call sites, object lifetime
tracking, and semantic dispatch. The feature pack registers only the behaviours
listed above. This avoids tiny Lua files that merely toggle native mechanics
while keeping the core itself policy-neutral.

The module contains:

- FleetOps VFS RVAs and scanner signature checks
- recursive loose-folder scanning
- FPQ directory discovery
- virtual-directory registration
- hash winner calculation
- the API v2 lookup-handler implementation

Winner selection is calculated from explicit active/parent mod priority,
primary-root precedence, and loose-before-packed precedence. Fleet Operations'
per-entry `overridden` flag is retained as a diagnostic but is not authoritative
for recursive entries: with a child mod active, it can incorrectly mark a
parent's recursive loose file as overridden by the packed copy from that same
parent. A genuine child-mod entry still wins through its higher mod priority.

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

The upgrade-pod hooks retain extended tier identity in sidecar state while
feeding tier 3 to Armada's fixed Team arrays. The highest attached tier for a
team/system supplies the effective multiplier. Configuration, ODF examples,
and the pending manual validation matrix are in
[`../../docs/upgrade-pods.md`](../../docs/upgrade-pods.md).

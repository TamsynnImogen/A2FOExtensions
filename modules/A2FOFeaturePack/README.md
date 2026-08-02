# A2FOFeaturePack native module

The output DLL is `A2FOFeaturePack.dll`. It groups the built-in optional native
features behind the core's versioned semantic dispatchers:

- recursive loose-folder and FPQ ODF discovery;
- the `wingman -> craft` classlabel compatibility alias;
- the Evolver `cocoon` ODF command;
- Ctrl-click ten-slot queue fill;
- Ctrl+Alt continuous production, using synchronized build orders and a
  save/load marker.
- bounded ship-system upgrade tiers selected by Lua;
- `tier<Tier>BuildItem<Index>` parsing for upgrade stations.

The core owns and signature-checks shared injected call sites, object lifetime
tracking, and cocoon SOD selection. The feature pack registers the behaviours
that should be enabled. This avoids tiny Lua files that merely toggle native
mechanics while keeping the core itself policy-neutral.

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
preserves Fleet Operations' original filesystem, classlabel, and cocoon
behaviour.

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

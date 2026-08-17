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
- automatic aspect-correct scaling of Fleet Operations' D3D9 intro, Armada's
  GDI movie window, and Armada's menu/campaign texture movie path to the live
  viewport.

When setting `upgradePodMaximumTier` in a child mod, retain a complete copy of
the parent's `RTS_CFG.h`. Fleet Operations shadows rather than merges this
native file, so a minimal child file would remove all other parent settings and
includes even though FeaturePack can read the new command.

Movie scaling has no mod command. It activates with the feature pack and
preserves each movie's aspect ratio while fitting and centring it in the
current render/client area. Every path is signature checked independently;
an unavailable path remains native and is reported in `A2FOExtensions.log`.

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

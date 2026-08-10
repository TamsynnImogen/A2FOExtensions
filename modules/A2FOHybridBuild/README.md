# A2FOHybridBuild native module

`A2FOHybridBuild.dll` owns the opt-in `hybridbuild -> research` alias and the
four-method HybridBuild runtime:

- dedicated Construction, Build, Research, and Evolve palettes;
- one shared ten-slot Producer FIFO;
- native research pods and uniqueness rules;
- generic yard production;
- protected Evolver cocoon and replacement sidecars;
- native station placement with one saved position/rotation per queue ID;
- yellow translucent previews for waiting station placements.

`A2FOFeaturePack.dll` continues to own the general Producer queue and
ResearchStation class hooks because those sites are also used by continuous
production and configurable upgrade pods. It exports a small callback bridge;
HybridBuild registers its optional policies through that bridge during module
initialization. This avoids installing either native hook twice. The module
loader's deterministic filename order loads FeaturePack before HybridBuild.

HybridBuild remains the checked owner of Armada's shared Producer construction-
effect hooks. Native API revision 9 dispatches a claimable `STARTING_EFFECT`
event there before the ordinary cosmetic Craft instance is created. Optional
compatibility modules can suppress an unsafe visual effect for a non-Craft
legacy build class without cancelling its timed Producer job; normal and
HybridBuild targets continue through the existing gateway.

Removing this DLL leaves the general feature pack loaded but does not register
the `hybridbuild` classlabel or cocoon command. Hybrid ODFs therefore require
both `A2FOFeaturePack.dll` and `A2FOHybridBuild.dll`.

The complete ODF contract and safety boundary are documented in
[`../../docs/hybrid-production.md`](../../docs/hybrid-production.md).

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
- live Producer build submenus through `buildItemXRefitY`.

Live build submenus are separate from `A2FOEditMenu`. A normal Producer ODF
may turn one existing `buildItemX` into a presentation-only parent:

```ini
buildItem0 = "fscout"
buildItem1 = "fexcelsiormenu"
buildItem1Refit0 = "fexcelsior"
buildItem1Refit1 = "fexcelsiorii"
buildItem2 = "fcruise1"
```

Clicking `fexcelsiormenu` replaces the live yard palette with the two child
classes. The native Back control returns to the yard's main build list; a
second Back returns to Fleet Ops' root palette. The parent is never submitted
as a build order. Its linked ODF supplies only presentation identity (button,
name, tooltip, and verbose tooltip); it needs no cost, build time, or tech-tree
requirements. Every child retains those values from its own normal ODF.

Armada must still instantiate that presentation ODF as an object class. A
minimal parent can inherit the generic Craft defaults and then provide only
its identity:

```ini
#include "craft.odf"

unitName = "Excelsior Variants"
tooltip = "FED_EXCELSIOR_MENU"
verboseTooltip = "FED_EXCELSIOR_MENU_V"
```

The runtime publishes the real children, not the parent, to the Producer's
effective list. Native AI, synchronized order admission, costs, build times,
and technology checks therefore continue to operate on constructible classes.
The parent remains enabled as a navigation control even when none of its
children is currently buildable. Inside the child page, native technology,
busy, and resource rules enable or disable each real item normally. Commands
are zero-based and support indices `0..56`.

The first implementation reads the custom rows from loose ODFs in the active
Data/parent/mod roots. Ordinary native lists remain unchanged for ODFs without
`buildItemXRefitY`.

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

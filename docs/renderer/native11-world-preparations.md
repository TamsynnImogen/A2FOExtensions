# World Craft logical preparation

This candidate reads fresh logical system, race and hull state for every bound
member of the checked world roster. It does not infer fresh state from cached
render-instance results or shared model-node flags. The original base setup
writes system/Borg/hull results into the render instance. Those cached fields
can be stale when that instance has not been prepared for the current frame.

The [pure policy](../../renderer/native11/craft_preparation.hpp) reproduces the
original system hiding, Borg-reference comparison and hull-divisor arithmetic.
The native oracle passes **49,152** comparisons across twelve x87 precision and
rounding modes. It executes only the pinned base setup and its hull-divisor
leaf against synthetic objects, with the Borg lookup cache already seeded.
There is no simulation, renderer or GPU call in that oracle.

The [single-member reader](../../renderer/native11/craft_preparation_capture.hpp)
checks entity, instance, class, model/DB/name and dispatch identity, pins the
class version, and compares two independent reads. The
[world reader](../../renderer/native11/world_preparation_capture.hpp) performs
two full sweeps, compares shared class/race/Borg inputs across members, and
rewalks the actual world list and model/placement associations. Unavailable
rows retain an explicit status and reason. Whole-capture failure preserves the
previous caller output. Callbacks must be pure and the world externally
serialized; matching reads cannot detect an intervening change that reverses.

Fleet Ops redirects the base setup through a callback that invokes the saved
original first, then updates an instance-specific logo texture cache. The
[source guard](../../core/native11_craft_preparation_source.hpp) accepts only
the exact pinned Root/Fleet identities, checked callback closure and exact
saved-original gateway. It normalizes only the six verified redirect bytes for
comparison. The getter and remaining original setup body stay byte checked.
Unknown code changes are rejected. Successful file admission is retained by
the core before any current-source proof can be used.

The [core snapshot](../../core/native11_world_preparation_snapshot.hpp) retains
the exact model roster owner, source proof and arithmetic context. Capture
brackets it with source/current-model checks and rejects late revocation before
publication. Current access checks source, class/world/model tickets, capture
lifecycle and exact snapshot identity. Disable/rearm clears publication. A held
snapshot remains historical data after current access is revoked.

The copied Fleet logo cache is correlation data only. It can lag the logical
selection, and does not establish selected texture ownership, node mapping or
material completeness. Entity display/tint/FOW callbacks are outside this
bounded base-setup proof. These snapshots do not register a geometry provider,
produce a complete native frame, suppress legacy drawing or take presentation
ownership. Stage 52 remains the latest completed numbered stage; all nine
implementation gates 53–61 remain open.

Validation: **143/143 final-artifact private cases** pass. The logical readers
pass Clang ASan/UBSan, and the source guard passes private static-mapping
corruption tests. The 90-second private legacy menu run logs real Fleet setup
qualification and retains the 30-node/25,610-byte model capture. The first
2-second diagnostic sees the original setup before hook installation; all 44
observations from four seconds onward validate the Fleet closure and exact
gateway. Early/late Root, placement and view profiles pass; the run logs no new
fault and cleanup verifies. The final screenshot shows the rendered legacy
menu. This does not validate a skirmish logical-state roster or native D3D11
presentation. Seven protected installed hashes, the renderer helper and seven
reference previews remain unchanged.

[Checkpoint receipt](../../build/native11/world-preparation-milestone.json).


The next material-source investigation has located the exact Fleet writers:
RVA `0x10ce59..0x10cf2e` populates extension `+0x154` from `logoFileNames`,
while `0x10d6b0..0x10d6db` stores the result of a type-4 `stlogo` node lookup
in extension `+0x140`. The latter lookup uses class `+0x1dc` as its database
input; that pointer's role and ownership must be established against the load
baseline before mapping the node into an owned draw. The initializer clears the
node and dynamic array; finalization releases the array. These are observations
from the pinned current Fleet DLL, rather than inferred matching source from a
different binary. Texture-load ownership, retirement and final render ordering
remain to be qualified. This research adds no material authority to the new
logical-state snapshot.

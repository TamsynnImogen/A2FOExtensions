# Current world Craft views

The [world-view reader](../../renderer/native11/world_view_capture.hpp) takes
the checked [world-model associations](native11-world-associations.md), samples
fresh Craft distance and fade inputs in **two full roster sweeps**, and requires
every captured row to agree across sweeps. Shared engine, camera, game and
settings inputs must also agree across members. It then rewalks the checked
world list, model owners and raw placements and requires them to equal the
supplied associations. Failure preserves the caller's previous output.

Each roster member retains an explicit `captured`, `view_unavailable` or
`association_unavailable` status and a reason where unavailable. The
[Craft-view input](../../renderer/native11/craft_view_capture.hpp) uses
`CraftViewSubject` for identity only; it does not fabricate a renderable
`Object` or placement to run the view policy. Fresh logical positions and
camera inputs are copied independently of native cached distance or fade
outputs.

The [core snapshot](../../core/native11_world_view_snapshot.hpp) retains the
exact model snapshot used for its rows. [Capture integration](../../core/native11_capture.cpp)
publishes it only with a current model source and checked Craft-view profile.
`latest_world_views()` rechecks the capture lifecycle, exact model-snapshot
identity, view profile, camera and the model snapshot's source, class, world-light
and model-generation checks. A held snapshot remains historical data after
current access is revoked. Getter success is a point-in-time check, not a
promise that its sources will remain current after return.

Capture requires external serialization against world mutation and pure
read/model-pin callbacks. Two agreeing sweeps and a final rewalk cannot detect
an ABA change that occurs and reverses between observations. These views do
not establish live world acceptance, full visibility or FOW, selected render
model or material, geometry, fresh per-part pose, or provider registration.
All nine gates 53–61 remain open; [stage 52 is the latest numbered checkpoint](native11-progress.md).

Host Clang AddressSanitizer and UndefinedBehaviorSanitizer checks and two pure
private Windows cases passed. Strict source syntax checks and the core DLL's
198 exports and 11 imports passed. The corrected final 137-case suite and
90-second isolated bootstrap are recorded below. No native D3D11 gameplay
acceptance follows from these checks.


The first isolated startup found that the near-fade values at Root RVA
`0x30444c` changed from `(0.5, 2, 0.875)` to `(0.9, 0.99, 0.9)` after
initialization. Treating those values as immutable profile bytes would disable
the reader. The reader now captures the three live values twice, validates
them, and includes them in shared-policy sweep comparisons. Both core view
sampling paths use the captured values. The other twelve static regions and
two vtable slots retain exact ownership checks. The generated profile labels
only this twelve-byte data region as a mutable input.

The dynamic-fade native CPU fixture passes 49,152 additional comparisons over
all twelve supported x87 precision/rounding modes; the original-default
49,152 comparisons also pass. These calls use isolated synthetic object/game
state and recorded private image globals, without simulation or graphics.
The initial unqualified startup and its exact candidate are preserved under
`build/native11/world-view-before-dynamic-fix`. The corrected candidate qualification is recorded below.


The corrected candidate passes **137/137 final-artifact private cases**, the
strict builds and the pure reader Clang ASan/UBSan run. Its final isolated
90-second legacy menu bootstrap remains alive at the limit, logs no new fault
and verifies cleanup. Early and late process reads qualify twelve static view
regions, two view vtable slots, four placement regions and three placement
slots, while validating the copied runtime near-fade values. The actual
30-node load template and 25,610-byte model capture remain available.
This is not live skirmish view/pose acceptance or native D3D11 presentation
takeover.

[Checkpoint receipt](../../build/native11/world-view-milestone.json).

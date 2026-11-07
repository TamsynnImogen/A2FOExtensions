# World base node visibility

The core now consumes the prepared visibility reader at the owned main-view
frame boundary. The [world batch](../../renderer/native11/world_prepared_visibility.hpp)
retains every checked roster member with an explicit association, preparation,
template, visibility-unavailable or captured status. Captured rows own their
load-template tuple, class and copied logical preparation, plus evaluated base
node flags, visibility and low-bit write intent. Two ships sharing one model
can therefore carry different hull/system policy results without reading the
render instance's cached preparation outputs or shared live node flags.

The batch compares two full sweeps. Template identity is the tuple of model,
authored metadata and native load metadata, allowing fresh wrapper allocations
while detecting owner replacement. Shared context and per-model template
inputs must agree across members. The batch finally rewalks the real model list
and logical preparations. Arithmetic evaluation runs under the captured x87
control and restores the caller's word. Whole-batch failure preserves output.
Unavailable rows remain explicit; they do not prove absence or completeness.

The [core snapshot](../../core/native11_world_visibility_snapshot.hpp) owns the
exact preparation/model/world frame. The production template provider calls the
existing checked `native11_animation_clock::model_template` API, which returns
only completed-load metadata. It never synthesizes a template on a cache miss.
The asset integration fixture uses an explicitly separate test-only cache
provider; actual load-template admission remains covered by the existing clock
and live startup evidence.

Core records the active engine/context/quality at main-view begin and requires
it to agree at capture, publication and current access. These are observed
boundary inputs; per-class/per-instance render-context and FOW authority still
require qualification. The current getter is owner-thread-only and rechecks
parent source tickets, model/class/lifecycle, loaded policy code, context and
captured template tuples, then exact published parent identity. Disable/rearm
clears publication; late revocation discards the new packet. Held owners remain
historical immutable data. External world serialization and pure callbacks
are required; repeated reads cannot exclude ABA or changes after observation.

The [base source guard](../../core/native11_craft_visibility_base_source.hpp)
checks six pinned Root regions and one instance vtable slot, normalizing only
declared HIGHLOW relocations. Four code regions must remain executable and
owned by the Root image. Exact file admission is retained separately from
current code checks, avoiding repeated file hashing when optional code is
unavailable. The material callback at `CB2AB` and final-draw call at `CB318`
are deliberately excluded. This is base node-flag policy, not complete class,
Fleet logo/texture overlays, FOW, selected material, pose, LOD, geometry or
caster authority. Excluded callbacks must eventually be composed in their
actual native order, including possible role aliases; their flag writes cannot
be assumed to apply after the combined base policy. No native function is
invoked by these readers and no
geometry provider or presentation takeover is registered.

Validation: **147/147 final-artifact private runtime cases pass**, including
the core publication/revocation fixture, base source guard and two-actor world
batch. Strict changed-source/fixture builds and the core's 198-export, 11-import
contract pass; the batch and leaf-regression Clang ASan/UBSan runs pass. Seven
reference previews and the renderer helper remain unchanged.

The final 90-second isolated legacy menu run logs real base-source qualification,
retains the 30-node/25,610-byte model capture, logs no new fault and verifies
cleanup. Both early and late process reads match all six base regions and the
slot, alongside placement, view and Fleet setup checks. The first snapshot
precedes Fleet's database hook and matches the pinned original destructor;
subsequent Fleet observations are checked explicitly. The final screenshot
shows the rendered legacy menu. This proves startup/source compatibility,
not actual skirmish visibility/template-owner/context acceptance or native
D3D11 presentation. Seven protected installed hashes remain unchanged.
All nine implementation gates 53–61 remain open; stage 52 remains the latest
completed numbered stage.

The negative fixture separates executable-page ownership from code signatures:
CB0E0 and CB390 share one page. Removing execute permission correctly revokes
both child visibility and parent setup; corrupting only the child byte on an
executable page revokes visibility while retaining its logical parent. This
corrected a test-harness assumption; production guards were not weakened.

[Checkpoint receipt](../../build/native11/world-visibility-milestone.json).


The next material-source join cannot use the texture-variant module's mere
presence or an empty cache as proof of unchanged output. The current module has
internal runtime/feature flags and class/variant caches, but no public ownership,
epoch or in-flight/quiescence source contract. Its callback can invoke native
Borg texture selection and then faction texture/node-visibility changes. Those
behaviors require an explicit source/ordering contract before an independent
material producer can certify effective state. No fallback-only proof is
inferred from unpopulated caches.

# Native animation fallback under the builtin clip hook

This boundary lets the Native11 clock reader recognize A2FOAnimations' own
matrix-playback hook when the module has **no configured clip classes**.
Configured custom clips remain unavailable to this runtime pose source, even
if their feature is currently inactive. This is a conservative global rule,
not per-class custom animation support.

The module publishes a private POD descriptor and monotonic source epoch through
[a locked coverage policy](../../modules/A2FOAnimations/native_observation.hpp).
Class registration begins a pending update before changing class or instance
maps and publishes the resulting custom-class presence on every exit. Custom
evaluation has a separate in-flight counter, including direct exported calls.
Thus removing the last class cannot admit fallback while an earlier custom
evaluation is still running. Both kinds of operation advance the epoch on
entry and exit. Reinitialization immediately revokes readiness; failed setup
cannot publish a usable descriptor. Counter/epoch exhaustion fails closed.

The reader never accesses the module's class/state/sample maps or invokes
`state`, `runtime_enabled`, a playback export, or a native evaluator. Locks
protect only the published policy and are released before gameplay callbacks
or native gateways run. Existing legacy animation maps are not made thread safe
by this policy.

The [clock guard](../../core/native11_animation_clock.cpp) checks the exact
six-byte entry branch, the trusted builtin's replacement ownership, and the
executable gateway's original six instructions plus return branch. It restores
only comparison-copy bytes. All remaining clock bytes and dispatch slots must
still match. Time, runtime channels, view and LOD captures compare the source
epoch before publication, so a class reload or evaluation during capture
invalidates the result. Load-template construction admission stays separate.

No hook is invoked or installed by the reader, and no public DLL export or new
runtime DLL dependency is added. A complete world provider, custom animation
snapshots, effective class/material/visibility policy, other render domains and
native presentation remain required. All nine implementation gates 53–61 remain
open.

The final strict build passes [132/132 private runtime cases](../../build/native11/animation-fallback-full-results.json),
including the previous 124 cases, a coverage-policy case, the qualified clock
case and six builtin initialization/registration cases. Core SHA-256 is
`24fc8041d58de78d65d9fe94342d3cd7e6a26c98447902c33c8afdcdf2ec1821`.
Independent PE checks preserve the prior 198 exports and eleven imports; the
renderer helper and seven reference preview outputs are unchanged.

The [saved 90-second private run](../../build/native11/private-game-animation-fallback90/bootstrap-receipt.json)
logs checked builtin native fallback at epoch 3, followed by an actual 30-node
load template and 25,610 copied model bytes. Its final image shows the animated
legacy menu; the game is alive at the observation limit, no new fault is logged
and private process/display cleanup passes. Seven protected installed hashes
remain unchanged. This establishes live source qualification and load-template
capture, not live skirmish pose acceptance or native D3D11 gameplay.

The [checkpoint receipt](../../build/native11/animation-fallback-milestone.json)
binds the final sources, artifacts and private legacy bootstrap evidence.
The next source dependency is an authoritative per-world-member association of
selected model/version, effective material, fresh pose and visibility, including
members absent from visible draws. Neither a visible-draw cache nor a bare world
list provides that authority.

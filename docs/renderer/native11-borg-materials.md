# Original base/Borg material operations

The renderer now has a verified base/Borg write policy and a bounded reader of
the native class's saved texture table. These are standalone helpers; they are
not connected to Core world material publication or native submission yet.
All nine implementation gates 53–61 remain open.

The pinned executable's selector at RVA `BFB70` uses the on-disk code cave at
`40129E`. For each saved row, the base path writes its original texture to slot
3 and then slot 0. The Borg path writes the mesh's `+0x13c` alternate to slot 0
and leaves slot 3 untouched. Consequently, a Borg selection can preserve the
auxiliary value left by an earlier base selection. A write set alone cannot
establish a complete independent material.

`craft_borg_material_writes` emits those exact ordered writes, with every other
slot left for the caller's independently owned prior state. It accepts unique
node indices below 4096, at most 4096 rows, non-null alternates and existing
vectors of 4–8 slots. Null originals are valid. Smaller vectors and native
setter growth remain outside this verified domain. An explicit empty result
does not prove the absence of overrides.

`capture_craft_borg_table` correlates a supplied class with an actual completed-
load material baseline. It checks the class database, signed table count and
8-byte `{mesh, original}` rows against every eligible baseline mesh in the
native numeric link order: recurse `+0x20` before walking `+0x1c`. The original
texture must match the load-time diffuse owner, and `+0x13c` must still match
the captured alternate. It reads two full static database/node/table passes,
pins current class/model/texture versions around them and retains the exact
baseline/class owners. It never reads the current native slot vector, flags,
matrices or scales. Invalid sources preserve the caller's previous output.

The light-class record supplies a copied-field identity and lifetime version;
it does not prove CraftClass type or completed material initialization. Those
proofs, actor/world/frame binding, model selection and callback ordering remain
caller obligations. The reader fails when stored originals or alternates no
longer agree with the completed-load baseline instead of substituting mutable
draw-time slots.

The new source guard checks eight exact executable image regions, including
the selector, code cave, setter, table construction/count/fill, getter and mesh
type, with bounded relocation normalization. It installs no hook. It does not
qualify a render callback route, class initializer dispatch or extension
overlay policy, and does not normalize unknown detours.

Validation includes **9,600 original CPU comparisons**, executing the pinned
selector, code cave and existing-vector setter fast path on owned synthetic
buffers. Consecutive base/Borg and Borg/base selections compare all slot bytes,
including preserved slots, while class/table/mesh headers must remain unchanged.
All **32 source byte, relocation and execution-permission mutations** are
rejected. No native allocation, class initialization, game startup, simulation
or graphics runs in this fixture. The pure policy and table reader also pass
strict host, Windows fixture and Clang ASan/UBSan checks, including ordering,
version replacement, between-pass mutation, overflow and failure atomicity.

The full private suite passes **151/151 final-artifact cases**. The checkpoint's
[receipt](../../build/native11/borg-material-milestone.json) records source and
artifact hashes. Core and the renderer
helper retain the exact preceding startup-verified images; this checkpoint
does not launch the game again or change installed files. Existing reference
renders remain covered by the full private suite.

Next work is the Core world/actor join and qualified class/caller state, complete
prior slot state and group/channel mapping, followed by independently versioned
faction/subsystem overlays in their native order. Effective materials, complete
geometry and native presentation remain unfinished.

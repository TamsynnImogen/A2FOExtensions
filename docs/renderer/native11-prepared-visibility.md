# Prepared Craft visibility inputs

The [prepared reader](../../renderer/native11/prepared_craft_visibility_capture.hpp)
binds a held logical ship-state snapshot to a checked completed-load node
template. It samples logical preparation again before and after the existing
role, topology and context reader, requires exact agreement with the held
snapshot, and copies that preparation into the result. Stale or forged inputs
fail without changing the caller output.

The shared role reader now has two explicit routes. Its existing `Object`
entry uses the stored native preparation outputs for callers at an already
prepared render boundary. The new route supplies fresh systems/Borg/hull
values from the logical snapshot and reads none of the cached instance outputs
at `9c/9d/9e/9f/a0/a1/a4`. Its identity-only subject contains no invented
renderable placement. Both routes retain the same checked class-role mapping,
complete template topology/name/owner checks, context inputs and primary versus
secondary database policy. The original public `Object` function signature is
preserved; the internal result gains an optional logical-preparation owner.

A stable zero context-quality value is valid and produces
`context_enabled=false`. A value changing between the two sampled context
reads is rejected. Reads require an externally serialized world and pure
callbacks; changes after the final observation and ABA changes cannot be
excluded by repeated reads. Caller-owned source code, class behavior, frame,
render context and model/world lifetime checks remain required. Fleet logo
node flag/texture overlays and other class modifiers remain outside this base
reader.

This helper supplies input to `evaluate_craft_visibility`; it does not itself
claim full entity visibility/FOW, selected material, fresh pose, LOD or geometry.
When evaluating arithmetic policy, use the retained preparation's native x87
context. The new helper is tested separately and is not linked into a game
geometry provider. The core binary and private startup evidence remain the
[143-case logical-preparation checkpoint](native11-world-preparations.md).
All nine implementation gates 53–61 remain open.

Focused validation passes strict i686 compilation, Clang ASan/UBSan for both
new and legacy reader fixtures, and both private Windows fixtures. All
**144/144 final-artifact private runtime cases pass**. The original and prepared
routes are reviewed independently; live skirmish acceptance remains open.
The exact core retains its previously verified 90-second private startup run;
no additional startup or native presentation claim follows from the new helper.
Seven protected installed hashes and seven reference previews remain unchanged.

[Checkpoint receipt](../../build/native11/prepared-visibility-milestone.json).

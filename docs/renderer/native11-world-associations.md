# Current world-model associations

The [association reader](../../renderer/native11/world_model_association.hpp)
copies one entry for every member of the checked complete world-light roster,
in list order, including objects without an instance or outside the visible
camera list. It traverses the native list twice and requires both traversals to
match the supplied roster and each other. Each entry reports an explicit
`WorldModelStatus`: bound current database, no instance, unsupported instance,
no database, unsupported database, unreadable data, invalid placement, or model
unavailable. The [implementation](../../renderer/native11/world_model_association.cpp)
checks the instance and database types before reading their dependent fields.

A bound entry holds immutable model bytes pinned by the instance's **current**
database pointer and copied database name. It also copies the original 12-float
native `Matrix34`, the runtime scale and its active flag. The reader does not
apply runtime scale or loader scale to that matrix. A current database binding
does not establish which model a native render selected.

The core's [snapshot](../../core/native11_world_model_snapshot.hpp) pairs these
associations with the complete world-light snapshot and source, model and
capture tickets. [Capture integration](../../core/native11_capture.cpp) checks
the loaded scale profile and publishes only after current source and world-light
checks. `latest_world_models()` rechecks the capture lifecycle, source ticket,
model generation, profile and [world-light lifecycle and class generation](../../core/native11_world_lights.cpp).
The [asset source](../../core/native11_assets.cpp) requires the owned model-load
route for a nonzero generation and current model pin. Retirement, replacement,
asset disable, class replacement, world-light disable or reinitialization, source
ticket change and capture disable can therefore revoke current access while a
previously held immutable snapshot remains readable as historical data.

The caller must serialize capture against world mutation. Two matching reads
cannot detect an ABA change that occurs and reverses between observations. The
packet supplies no selected render model, geometry, visibility or material
authority, fresh per-part pose, or geometry-provider registration. It does not
close any of gates 53–61; [stage 52 remains the latest numbered checkpoint](native11-progress.md).

Pure Clang AddressSanitizer and UndefinedBehaviorSanitizer checks passed, as did
all **134/134** final-artifact private Wine cases and the strict build. The
final 90-second isolated legacy startup run stayed alive at the limit, showed
the legacy menu, logged no new fault and verified cleanup. All four placement
code regions and three vtable slots matched in both early and late process
reads. The actual 30-node load template and 25,610-byte model capture remained
available. This checks loaded profile compatibility; it does not exercise a live
skirmish roster or establish native D3D11 gameplay or deployment.

The [checkpoint receipt](../../build/native11/world-association-milestone.json)
binds source and artifact hashes. The private staging attempts are preserved:
a diagnostic loop initially shadowed the DLL destination path with a slot
integer. Renaming it fixed staging; the previous private core was intact. The
harness now also verifies a sibling temporary copy before replacing the private
DLL and keeps a hash-named copy of the previous image. It assumes one trial
owner and fails closed on an incomplete or mismatched backup.

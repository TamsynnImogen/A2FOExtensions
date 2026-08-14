# Animated hardpoints

`A2FOAnimatedHardpoints.dll` makes SOD matrix-animation channels affect
Armada II gameplay hardpoint queries.

No new ODF command or SOD convention is required. Animate a null/hardpoint node
in the model using an ordinary matrix channel whose identifier matches that
node. Hardpoints also inherit animation from an animated ancestor.

Armada normally evaluates those channels only in the visible/render database.
Weapons and other gameplay systems query the separate logical database, whose
node transforms remain static. This module evaluates the visible matrix
channels at the owning model instance's animation time and uses the matching
visible node for `GetWorldPosition` and `GetWorldTransform` calls.

The behaviour is deliberately limited to type-0 null nodes. Mesh and sprite
animation remains native, and emitter nodes are not changed by this module.

The module supports the repository's checked ArmadaL.exe 1.1/Fleet Operations
Roots image. Unsupported executables leave both native transform functions
unchanged and log that the runtime is disabled.

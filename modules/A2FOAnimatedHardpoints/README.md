# Animated hardpoints

`A2FOAnimatedHardpoints` makes SOD matrix-animation channels affect Armada II
gameplay hardpoint queries. It is an opt-in module built into
`A2FOExtensions.dll`; keep its original name in module selections.

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

When disabled, its hooks forward the original logical-node results unchanged.
When enabled, it evaluates visible matrix channels for gameplay queries. Turning
the module off or on invalidates the cached pose, even for the same model
instance and animation time. Metadata and channels stay available while off;
the module does not repeatedly patch or unload, and native side effects already
applied are not rolled back.

The module supports the repository's checked ArmadaL.exe 1.1/Fleet Operations
Roots image. Availability requires its own successful executable signature
check and native hook setup. Unsupported executables leave both native
transform functions unchanged and log that the runtime is disabled. A stale
standalone `A2FOAnimatedHardpoints.dll` is ignored during discovery. Its
standalone target remains available for SDK compatibility; the release no
longer needs this DLL.

The optional [A2FOAnimations](../A2FOAnimations/README.md) bridge remains
supported for controlled clips. This built-in integration does not require an
updated `A2FOAnimations.dll` companion.

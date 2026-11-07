# Native11 primitive scenes (API7)

API7 adds owned triangle geometry and BGRA textures to the existing offscreen
renderer. It is part of A2FOExtensions' staged native renderer, with no D3D8/9
object or render-state translation at this boundary. It does not discover game
objects or replace the game's frame/presentation path.

The C contract is in `sdk/include/a2fo_native11_primitives_api.h`. Query version
7 and the exact table size; unsupported queries leave caller output untouched.
API1–6 keep their previous sizes, capabilities and defaults. An API7 table's
prefix metadata describes the complete API7 table; query an older version when
passing its table to an older coordinator.

## Ownership and resource identity

Each primitive submission includes a **full** resource manifest. Mesh and texture
IDs are nonzero 64-bit host identities; texture ID zero selects implicit white.
They are not GPU handles. Surface `mesh`/`texture` members in the C++ packet must
remain zero; its separate logical IDs identify resources. Referenced IDs must be
present in the manifest, without duplicates.

A successful generation must increase. An unchanged source epoch requires the
same ID sets and byte-identical geometry/texture payloads. Draw transforms,
materials, UVs, visibility through draw omission, lights, fog and view settings
can change without changing the manifest. Resource changes require a greater
source epoch. Device recreation reuploads the same verified manifest. Explicit
`PrimitiveScene::clear()` retains generation/source watermarks and requires a
new source epoch for the next resource manifest.

The C service borrows arrays only until the synchronous call returns and copies
payloads before use. Callers must supply stable, readable memory and nonthrowing
callbacks; this is not a memory fault barrier. The C++ encoder pins a const owned
packet and supports moves with descriptor-pointer rebinding. Publish without
mutable aliases. Keep the API DLL loaded and callback context alive; explicitly
close sessions on their creating thread before unload, outside loader lock.
Destructors of the client coordinators do not call into a renderer DLL.

## Surface and pass controls

Matrices are row-major, column-vector. Native geometry has clockwise screen-space
fronts, outward normals and 32-byte position/normal/UV vertices. BGRA textures are
tightly packed, top-down, with straight alpha; additive artwork should contain
its intended RGB coverage. Initialize matrices and UV transforms explicitly when
using zero-initialized C records.

- Flags select two-sided, cutout, straight-alpha blend or additive blend. Alpha
  and additive cannot be combined. Cutouts use the per-surface alpha threshold
  in both colour and shadow depth; generic blended geometry casts no solid
  silhouette. Cast and receive controls are separate from camera depth policy.
- Sampling selects point/linear and clamp/wrap. Affine UV transforms apply to
  colour and cutout shadow sampling; invalid or overflowing transforms fail.
- Opt-in ordering sorts ascending layers, then opaque surfaces, then transparent
  surfaces back-to-front by projected mesh-bound centres, with stable ties.
  Transparent centres require positive homogeneous W. Intersecting surfaces may
  need host decomposition or authored capture order; this is not per-pixel OIT.
- Depth modes are automatic, read-only and disabled. By default every call clears
  the entire colour/depth target. LOAD flags preserve a previously initialized
  target for subsequent passes; resize/recreation invalidates LOAD eligibility.
  Viewports constrain draws, not target clears. HUD passes can load prior colour
  and use disabled depth with cast, receive and fog excluded.
- Fog is opt-in linear falloff with radial world-space camera distance and a
  per-surface exclusion. The host supplies the camera position and world units.
  Enabled fog requires a nonnegative start and a greater end distance.
- Shadow filtering selects the existing bilinear 2x2 default, nearest hard
  comparison or 3x3 PCF. Per-source light-mask selection remains explicit. Bias
  accepts stored binary32 values from zero through the closest float to `0.1`,
  inclusive; binary endpoints avoid x87 decimal excess-precision comparisons.

`quad_geometry.hpp` builds clipped screen sprites with adjusted atlas UVs and
camera-facing world billboards from an explicit orthonormal right/up basis.
An entirely clipped sprite produces empty geometry; omit its mesh and draw.
These builders do not read engine memory or evaluate native animation.

## Bounds and recovery

A packet is bounded to 2,048 meshes, 256 textures, 8,192 draws, eight lights,
one million cumulative vertices, three million indices and 128 MiB resource
payload. Renderer logical upload budgets and temporary replacement residency may
refuse an otherwise well-formed packet. Usage counts describe resident logical
uploads, not VRAM or an upload timing counter; targets, staging and shadow maps
are separate allocations.

Descriptor extents, pointers, counts and payload budgets are checked before
copying. Geometric and GPU policy validation can follow temporary uploads.
Candidates have rollback ownership. Input/upload failures keep the previous
cache and image; failed generations can be corrected and retried. Device loss
can invalidate target pixels and requires recovery. Cached readback staging is
reused for one target, dropped on resize/teardown or failed/malformed Map, and
recreated on retry. Client readback publishes caller outputs only after both
metadata and pixel responses satisfy the protocol.

The owned client is `PrimitiveRenderSession` in `primitive_session.hpp`.
`tests/native11_showcase_native.cpp` is a deterministic example of a world pass
and a retained HUD pass, including close, resize and rejected-frame recovery.
The controls above are C/C++ renderer interfaces; no additional ODF or Lua
commands are registered by this primitive service. Native offscreen pose/material
capture, game draw-domain coverage and live Windows/game acceptance remain open.

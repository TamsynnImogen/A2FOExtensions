# Complete authored mesh scope (API9)

The original API1–8 world and preview entry points select pristine, highest-LOD
SOD geometry. That is insufficient for fresh per-instance damage and LOD
selection: excluded meshes cannot become visible through a part-state callback.

API9 adds `submit_world_all_parts` after the unchanged API8 table. Its i686 table
is 80 bytes; the new function pointer starts at byte 76. The helper keeps its
single `A2FO_Native11_GetInterface` export. Query version 9 with the exact table
size and require `A2FO_N11_CAP_ALL_MODEL_PARTS` before using the new function.

The new submission emits nonempty mesh groups in authored node/group order,
including damage subtrees and all LOD levels. It requires both
`A2FO_N11_FRAME_COMPLETE_CASTERS` and `A2FO_N11_FRAME_REQUIRE_PART_STATES`, plus a
part-state resolver. Every emitted part must resolve, including hidden parts.
Missing, malformed or changing sources reject the frame and leave it ineligible
for presentation. Switching between the new function and an old submission
function rebuilds the model cache for the requested scope. Within one scope,
visibility changes reuse resident model and texture resources.

The private Rust adapters share parsing, validation and output preparation with
the old policy. `a2fo_rs_native11_inspect_sod_all` supplies matching source node
and group metadata to the core-side host. It is not a public core DLL export.
`WorldRenderSession::initialize_all_parts` and
`GameplayRenderSession::initialize_all_parts` opt into the new function while
retaining the existing session admission and lifetime checks.

`bind_owned_part_states` translates an owned animation pose and explicit
per-node visibility into an immutable material observation. It validates model
ownership and identity, node/group mapping, selected texture owners and affine
matrices. It replaces every previous part state; it never supplies visibility
from a previous instance or frame.

This is a receiver and binding capability, not a complete game producer.
The current core material observer still uses the pristine mesh source.
Authoritative Craft visibility, native LOD, procedural/class transforms and
effective materials must be bound before the new path can represent the full
game scene. Stage 53 remains open. No legacy drawing is suppressed and no
installed files are changed by this work.

## Native LOD boundary still to port

The pinned `4300ab...b00de` engine's LOD-control routine at VA `0x630a50`
computes view-relative bound distance using x87. Its first threshold compares
an extended result after a non-popping float store; later thresholds reload the
rounded float. A float-only distance approximation can disagree at the closest
threshold. Automatic selection also applies the render-context quality cap;
explicit selection and the force-highest branch use different paths. The shared
last-instance view offset is not an independent source for offscreen casters.
The recovered disassembly is retained with this checkpoint; these observations
are not an implemented or accepted native LOD producer.

## Real asset inspection

The private fixture parses both mesh scopes alongside the complete pose metadata
and checks every emitted node/group against that hierarchy:

| Asset | Pristine mesh groups | All mesh groups | Pose nodes |
| --- | ---: | ---: | ---: |
| Venture | 1 | 3 | 15 |
| Mediterranean | 3 | 7 | 55 |

These are real-asset parser and mapping checks. The visibility/animation GPU
comparisons use controlled synthetic scenes; this does not establish the live
ships' class policy or visual acceptance.

## Frozen verification

The final strict Native11 build and all 87 private runtime cases pass, including
older compiled service clients, API9 C and C++ dispatch, scope switching, hot
visibility changes, callback reentry and failed-frame presentation rejection.
The controlled owned-pose fixture uses the private all-part metadata parser and
binder for two independently timed instances and compares API5/API9 GPU pixels.
Seven reference previews are byte-identical to the preceding checkpoint.
Seven protected installed files remain unchanged. The binder also passes Clang
AddressSanitizer/UndefinedBehaviorSanitizer and the shared Rust model crate passes
all seven host tests. The core retains 198 canonical exports; the helper retains
its single interface export. These are staged checks under private Wine/Xvfb;
no game startup, GUI input automation or live visual acceptance took place.

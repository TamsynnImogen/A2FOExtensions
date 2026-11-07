# Load-time node templates and fresh pose defaults

This advances stage 53. Complete live geometry, class/LOD/material policy and
native gameplay presentation remain open. Installed files are unchanged.

## Completed-load boundary

The existing model bridge calls the native `ReadDatabase` exactly once, then
observes its completed result while the source stream remains alive. The asset
observer publishes the owned SOD bytes and releases its store lock before asking
the animation module to capture a node template. No new native hook was added.
The path remains controlled by the default-off `CaptureAnimationClock` option.

The producer requires the owning thread, no callback reentry, checked node and
model-load hook ownership, and the active model version before and after capture.
A separate generated profile initially pinned 15 normalized regions covering the database
and node loader, initialization and type functions. The subsequent
[LOD input integration](native11-lod-inputs.md) expands it to 23 regions. Every profile byte comes
from the exact `4300ab...b00de` image; HIGHLOW operands are normalized for ASLR.
The checker is `tools/check_native11_template_profile.py`.

The template reader copies a complete, single-root tree twice. Database name,
node names/owners/types, parent/child/sibling links, matrices, scales, flags and
role pointers must agree. Authored nodes map one-to-one to live nodes. Resolved
numeric links preserve native sibling order, including order differing from
file order. Unsupported node types, generated/extra nodes, changing data,
ambiguous names, cycles and excessive depth fail admission. No native function
is called by the reader.

## Immutable defaults and cache

Returned snapshots pin the model, authored metadata and copied template data.
The cache keeps only a weak model owner and bounded strong metadata owners:
64 entries and 64 MiB. It prunes expired models and rejects a different owner
with the same serial. A live model's baseline cannot be replaced by different
metadata owners. There is no eviction of a live baseline and no reconstruction
from a later instance draw. A missed capture, full cache or restart requires a
fresh native model load before that baseline becomes available.

`evaluate_owned_animation_pose_from_template` starts with the captured native
local matrices and scales, then applies fresh matrix/scalar channel writes.
Those writes override their respective fields independently. An active channel
that does not write still rejects: a load default cannot stand in for a shared
native target left over from another instance. The older authored-default
entry point retains its behavior.

The diagnostic now identifies its defaults explicitly:

```text
Native11 owned transform: ... state=matched|differs|unavailable seed=load_template|authored ...
```

Authored fallback remains diagnostic only. It does not approve an offscreen
world frame or complete caster coverage.

## Fresh Craft inputs

The separate `capture_craft_visibility` CPU reader accepts checked world-member,
class-version and load-template owners plus an explicit render context. Two
passes verify instance/class/database identity and the complete static topology,
then copy health, system, assimilation, hidden and context state. It deliberately
never reads mutable live node flags, matrices or scales as defaults. Alternate
models skip the primary-model-only class fields, matching the native branch.
Negative health outside the existing evaluator's contract rejects at capture.

This reader has controlled fixtures and is not registered as a live world
geometry provider. Its result still requires native LOD, material and procedural
policy before it can describe the complete game object.

## Verification boundary and remaining work

Controlled fixtures exercise complete tree copying, signed-zero changes,
mutation rejection, weak ownership, cache replacement refusal and native-default
seeding. The integration guard simulates a completed-load callback using real
Rust SOD metadata and a private inspection mapping. It verifies pinned code and
hook loss, source revocation, foreign-thread refusal and immutable defaults.
The GPU fixture renders two instances with captured-style nonidentity defaults
and checks their expected footprints through API9.

The native loader itself is not executed by those fixtures. Source inspection,
compiled integration and private fixture results remain separate from live game
acceptance. The existing observer bridges normalize callback floating-point
state with `FNINIT`; propagating the game's original arithmetic environment into
final authoritative evaluation remains an integration gate. Class/procedural
transforms, loader-scaled mesh geometry, native LOD selection, effective
materials, complete world membership and lifecycle handling also remain open.
No legacy drawing is suppressed by this work.

## Frozen checkpoint evidence

The final Native11 strict build, rebuilt core contract and all 91 private runtime
cases pass. The latter include older compiled clients, completed-load guard
injection, template copying/cache cases, seeded poses, fresh Craft input capture
and seven byte-identical reference previews. Four final-source Clang
AddressSanitizer/UndefinedBehaviorSanitizer cases also pass. The core retains the
canonical 198 exports and original imports; the D3D11 helper retains one export.
Seven protected installed-file hashes remain unchanged. The candidate build ID
is `20261103-native11-load-template-integration-wip`. These are private staged
checks, not a live game launch or release acceptance.

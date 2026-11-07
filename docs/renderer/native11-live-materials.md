# Live observed material submission

API11 connects the sealed Core material capture to the D3D11 helper. Core can
prepare observed material resources before complete world geometry and lighting
are available. An optional private preview renders the observed objects with
their captured part states; it does not publish a complete-world image, cast
shadows or authorize presentation takeover.

## Submission contract

Every emitted SOD part carries its exact all-part node/group identity and its
immutable authored texture, colour, emission and flags. The host resolves a
complete material for every part on every frame, including cache hits and
authored-white parts. A synchronous callback must invoke its sink exactly once.
The helper copies the borrowed result before returning.

The resolved material controls diffuse texture, colour, emission, flags,
sampler, depth mode, alpha cutoff and UV transform. An empty diffuse identifier
explicitly means white. Texture identifiers name immutable bytes within a
source epoch. New selected identifiers upload through the asset callback;
malformed callbacks fail before drawing. Texture/geometry limits and owner-thread
and reentry guards remain enforced. API1–10 retain their existing interfaces.

Core's supported source is **authored SOD material plus the observed selected
diffuse texture**. It uses identity UVs, linear-clamp sampling, automatic depth
and a 0.5 cutoff. This source does not yet provide arbitrary native runtime
colour/team/alpha, bump channels, shader selection or UV overrides. Those fields
are supported by the API, but their engine producers still need qualification.
The separate world base/Borg operation snapshots remain diagnostic candidates;
this work does not promote them to effective unobserved-actor materials.

## Ownership and publication

The capture retains model bytes, selected texture versions, qualified node/group
identities and part states. Core checks its exact sealed-frame owner, lifecycle,
loaded hook ownership and current model/texture owners before and after helper
calls. A revoked or failed frame cannot publish an older preview. New resource
epochs discard cached resources; a failed preparation requires a fresh frame.
Asset callbacks require an active pending submission; retaining an accepted
frame does not authorize asset reads during readback or other session operations.
The shared render target has separate preview and complete-world eligibility:
either render path revokes the other's image before replacing it.

`MaterialStatus` records the last submission and its source ticket. Readback
additionally rechecks exact resource owners. A prepared status means resources
reached D3D11; only a previewed status means the private target was drawn. Neither
status grants world completeness or native gameplay acceptance. Empty observed
frames remain waiting and do not count as successful submissions.

## Configuration

Both switches default to zero in the production configuration reader:

```ini
[Native11]
LiveMaterials=1
LiveMaterialPreview=0
```

Preparation can run with `OffscreenWorld=0`. Set `LiveMaterialPreview=1` only for
the separate offscreen observed-object preview. It needs captured part states.
Complete world rendering continues to require its independent geometry, light
and caster authorities. The private bootstrap stages the gate-matched helper
with `--renderer-helper`; it never replaces the installed helper.

## Verification

The new fixtures exercise exact all-part identity, per-frame callback updates,
textured/white transitions, distinct actor texture views, malformed callback
recovery, immutable retained bytes, resource retirement, source invalidation,
replay refusal, startup-only configuration and separate world/preview readback.
GPU and Core pipeline fixtures use a private headless Wine/Xvfb target with
D3D8/9/D3DX disabled. Build and runtime receipts are linked from the main
[progress checklist](native11-progress.md). In-game visual acceptance remains
separate from those tests.

The [focused results](../../build/native11/live-material-focused-results.json)
also cover the existing x87 material-content comparison and legacy session
callback-scope regressions. Cache epochs invalidate residency; they do not by
themselves make identical immutable contents unequal.

The [full final-artifact suite](../../build/native11/live-material-full-results.json)
passes **157/157** cases. The seven reference previews remain byte-identical to
the prior checkpoint, and seven protected installed files retain their hashes.
Core keeps its 198 exports and 11 imports; the helper exports only
`A2FO_Native11_GetInterface`. The [checkpoint receipt](../../build/native11/live-material-milestone.json)
binds source, executable, helper, test and startup evidence to the frozen copy.

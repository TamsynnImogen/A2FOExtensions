# FNSOIDATHQ renderer and loader review

2026-11-02. Read-only upstream assessment for Nexus lighting, shadows, and the
experimental D3D11 backend. No upstream DLL was loaded or installed. Source
snapshots are in `/home/tamsynn/.cache/a2fo-upstream-research-20261102/`.

## Pinned sources

| Repository | Revision | Result |
| --- | --- | --- |
| [armadaNebulaPatch](https://github.com/FNSOIDATHQ/armadaNebulaPatch/tree/d01c8384b9a8e5ee16f9a9540936f3b852cb916c) | `d01c8384b9a8e5ee16f9a9540936f3b852cb916c` | Parent project/documentation and three submodule pins. |
| [shaderPlusArmada](https://github.com/FNSOIDATHQ/shaderPlusArmada/tree/f867372937e398b08ab86ae8b80466fdc3f44348) | `f867372937e398b08ab86ae8b80466fdc3f44348` | DX8 shader hooks and experimental DX9 rendering. |
| [hookTools](https://github.com/FNSOIDATHQ/hookTools/tree/5972bf8132223e216707f7dc3001b10997b0f076) | `5972bf8132223e216707f7dc3001b10997b0f076` | Raw JMP, vtable and memory-write helpers. |
| [dllLoaderArmada](https://github.com/FNSOIDATHQ/dllLoaderArmada/tree/d123001e300ec8d101335d66887af3dbab37c816) | `d123001e300ec8d101335d66887af3dbab37c816` | Ordered DLL loading and named-export activation. |

All four carry MIT notices, copyright 2024 dev gao. These revisions match the
main/shader revisions and submodules already recorded in the 2026-08-11 local
review (`third_party/README.md`). This is refreshed inspection, not discovery of
a new upstream release. Preserve the existing notices for derived code.

## What is already used

`core/nebula_renderer.cpp` identifies the extension's DX8 per-pixel lighting as
derived from armadaNebulaPatch. The local project retains four attributed DX8
shader programs and its own checked hook ownership, draw/state restoration,
material/emissive/specular paths, and newer prototype shadow pass. It deliberately
does not import the upstream loader, hook toolkit, MinHook, or DX9 experiment.

The active Nexus path is D3D8/D3D9 through managed DXVK. The extension's D3D11
game translator is a separate opt-in backend; upstream code does not add DX11
effects to either path. No shadow-map implementation was found in these four
upstream repositories.

## Useful lighting evidence

[`dx9plus.cpp`](https://github.com/FNSOIDATHQ/shaderPlusArmada/blob/f867372937e398b08ab86ae8b80466fdc3f44348/dx9plus.cpp)
defines two directional and four point-light records (around lines 85-100),
copies directional RGB and the light-instance transform's front axis into an
effect array (287-303), and uploads world/view/projection matrices. This is a
useful reference for feeding independent light descriptors into our own shader.
The point-light array population is TODO. The directional loop has no visible
array-capacity guard, so it must not be transplanted as a ready implementation.

Its separate per-light helper (around 667-715) normalizes and negates the
directional light's mesh-local front axis for c6, obtains a point-to-source
vector for point lights, and computes linear start/range attenuation of the
light colour. This agrees with the shape of the pinned installed FO helper:
FleetOpsHook SHA-256
`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`,
RVA `0x001e610c` onward; the direction negation is at RVAs
`0x001e615e..0x001e6175`, c6 upload at `0x001e62a2..0x001e62ad`, and falloff
at `0x001e6237..0x001e629a`. This was static disassembly, not execution or a
complete proof of the preceding object/world transform.

The array and c6 paths intentionally use different vector conventions; a sign
change based only on one comment or array assignment is not justified. Validate
the consuming shader and transform together. The required DX9 effect
`shaders\\dx9\\pbrLite.fx` is not checked into upstream. The shader repository's
tracked files do not supply the requested FX/assembly assets, so its actual
pixel-lighting/BRDF cannot be verified from that snapshot. The attributed DX8
assets already retained locally are a separate known input.

No spotlight/cone implementation was found in the inspected paths. The parent
README explicitly records incorrect point-light shading, missing source colours
in its old shader, multi-light restrictions, uncertain matrix conversions, and
an incorrect alpha-path patch. These are reasons to use the source as evidence,
not a drop-in renderer for new light objects.

## Hook and loader comparison

[`hookTools.cpp`](https://github.com/FNSOIDATHQ/hookTools/blob/5972bf8132223e216707f7dc3001b10997b0f076/hookTools.cpp)
writes a five-byte JMP without decoding displaced instructions or checking the
target image/signature. Its trampoline allocation/build is commented out and
the function still overwrites the entry. Existing checked A2FO hook primitives
remain the integration path; no additional hook-tools DLL is needed.

[`dllLoaderArmada/main.cpp`](https://github.com/FNSOIDATHQ/dllLoaderArmada/blob/d123001e300ec8d101335d66887af3dbab37c816/main.cpp)
reads `.\\dll\\dll.list` and optionally calls `dll->function` from `DllMain`.
It does not check the load/export result before invoking the function pointer.
The parent README's older `after.list` description differs from this source.
Our existing deterministic module discovery, checked startup pairing, and
deferred initialization should remain authoritative. Another startup loader
would add a competing load/patch owner and does not supply lighting/shadows.

## Integration decision

1. Keep the existing A2FO loader and checked hook infrastructure.
2. Reuse the native light-list and attenuation evidence when implementing the
   [ODF source contract](odf-light-sources-design.md). Implement independent
   colour/intensity/cone/visibility contributions and source limits explicitly.
3. Continue correcting the current shadow prototype with target-specific
   evidence. Upstream contains no shadow fix to merge and no reason by itself
   to reverse the map light direction.
4. For D3D11 effects, first provide independently sampled scene depth/colour
   and state-safe render passes. Shadow filtering, per-light shadows, AO,
   bloom, and tone mapping need their own implementation and acceptance;
   none is enabled by loading these repositories' DLLs.

No new upstream renderer, hook toolkit, loader or shader dependency was added
by this review. Cache commits/source hashes are recorded in
`build/ingame-shadows/upstream-review-20261102.json`.

# Object editor, muzzle flashes, and cloak rendering

Investigation date: 2026-09-12. Status: source and reference-binary research;
the features below have not been implemented or tested in-game by this work.

## 1. Extend the existing native object editor

Use the map editor's existing object-properties dialog, which already edits
Ship Name and subsystem health. Add Captain and Registry text fields, followed
by Current / Maximum columns for Forward, Aft, Port, and Starboard shields.
These are properties of the selected placed object.

The existing `A2FOEditMenu` module handles the placement-menu hierarchy. Its
`EditMenu::Update` hook is not the object-properties binding point.

### Native entry points

The Armada II 1.1 symbols in the local Heroic installation identify:

| Function | Preferred VA | Armada RVA |
| --- | --- | --- |
| `GameObjectDialog::GameObjectDialog(GameObject*)` | `0x00500cb0` | `0x00100cb0` |
| `GameObjectDialog::~GameObjectDialog()` | `0x00500fe0` | `0x00100fe0` |
| `ObjDlgProc(HWND, UINT, WPARAM, LPARAM)` | `0x00501560` | `0x00101560` |
| `GameObjectDialog::Execute()` | `0x00501ee0` | `0x00101ee0` |

These are symbol-derived research targets. The dialog resource, control IDs,
initialization, acceptance/cancellation behavior, and any Fleet Ops replacement
must be traced before installing a hook. No dialog signature was established
by this investigation. Prefer extending its normal controls and dialog
procedure so the existing manual editing workflow remains intact.

### Captain and registry state

`modules/A2FOCraftIdentity/module.cpp:1947` resolves both strings from the
current native ship-name index. It currently requires a registered class policy
and a nonnegative name index. Its cache is replaced when that index changes.

Implementation requirements:

- Store explicit captain and registry overrides per object, independently of
  the automatic row-aligned defaults and native ship-name index.
- Distinguish an unset override from an intentionally empty string. An explicit
  blank must not unexpectedly restore an ODF default.
- Resolve explicit overrides before rejecting missing name lists or a negative
  name index. Manual names must also work on ships without companion ODF lists.
- Keep explicit overrides when Ship Name changes; provide an explicit way to
  return an identity field to its automatic value.
- Apply edits using the native dialog's acceptance behavior and discard pending
  edits on cancellation. Clean up state when an object is deleted or replaced.
- Persist overrides when saving a map and restore them when reopening the map
  or starting a game from it. Also cover ordinary game save/load.

### Directional shield state

`modules/A2FODirectionalShields/module.cpp:470` retrieves maximum strengths
from a class-wide policy. `shield_value` at line 735 returns those shared
maxima. The public API currently exposes getters and damage-scoping functions,
but no editor setters.

Current facing values are per-Craft, but `A2FO_CRAFT_EVENT_POST_LOAD` at line
648 erases that state and reconstructs it from native aggregate shields.
Individual facing damage is therefore not currently restored by this module.

Implementation requirements:

- Add a per-Craft maximum override and make damage, recharge, reconciliation,
  getters, and hit effects use the same effective policy.
- Apply all eight values together after validation. Require finite numbers,
  positive maxima under the existing shield policy, and
  `0 <= current <= maximum`. Reject invalid edits without partially applying
  the other fields. Supporting a zero-capacity facing would be a separate
  change to the current positive-capacity contract.
- Update native aggregate current and maximum fields (`Craft+0x1c8` and
  `Craft+0x1cc`) to the corresponding sums. Never edit the shared CraftClass
  maximum when changing one placed ship.
- Persist the four current values and four overridden maxima. Restore them
  after the existing post-load reconstruction so it cannot overwrite them.
- Preserve class defaults on unedited ships and keep controls unavailable for
  objects without enabled directional shields.

### Shared persistence work

The SDK has Craft lifecycle notifications but no shared save/write extension
dispatcher. `A2FOEnergySystems` already owns hooks at Craft Load RVA
`0x000c2340` and Save RVA `0x000c2980` and writes its own store block. Coordinate
with that owner and other persistence hooks instead of installing another
independent detour at the same entry.

Design a versioned, optional extension block with an explicit old-map fallback.
Do not assume the ammunition module's serialization can simply be copied:
map writing, text/binary formats, absence of extension data, and load ordering
need their own trace. Saved identifiers must not be raw process pointers.

Acceptance cases: two ships of the same ODF with different overrides; explicit
blank identities; rename after overriding; cancel; lower maximum; invalid
numeric input; deletion/recreation; map reopen; launch from map; game save/load.

## 2. Pulse and torpedo muzzle flashes

No implemented muzzle-flash feature was found in the searched core/modules.
The suitable initial design is an optional, ODF-configured visual effect at
each hardpoint that actually launches a projectile. A native SOD effect is a
candidate presentation because it can carry authored sprite/material animation.
Asset keys and lifetime/scale controls still need to be finalized; no new ODF
commands are implemented by this investigation.

### Use the actual launch boundary

The shared SDK's `A2FO_WEAPON_TRIGGER_COMMITTED` event means that a trigger
request completed, not that a projectile was created. It is not a sufficient
event for muzzle flashes. A rejected or delayed shot must not produce a flash,
and each projectile in a burst must receive its own event.

Existing ammunition accounting has already identified the relevant paths:

| Existing boundary | Image / RVA | Use |
| --- | --- | --- |
| Native common post-fire commit | Armada `0x00270dd0` | Successful ordinary-weapon shot |
| `CannonImp` guided launch | FleetOpsHook `0x001392cc` | Actual guided FO projectile launch |
| Native position launch | Armada `0x002679f0` | Alternate FO launch; ordinary weapons also use this path |
| `CannonImp` ordnance selection | FleetOpsHook `0x0013a550` | Context only; selection is not a shot |

See `modules/A2FOEnergySystems/module.cpp:842` and `:912`, and the existing
per-shot entries in `docs/addresses.md`. These addresses are already owned by
ammunition hooks. A shared projectile-launched notification should capture the
launching weapon, owner, actual hardpoint/transform, and ordnance without
duplicating those hooks or changing ammunition accounting.

The common post-fire boundary may be too late to recover a hardpoint reliably;
capture it at the verified launch call rather than guessing a random hardpoint
or using the current target location. Confirm both native and FO replacements
of pulse and torpedo ordnance.

Effects should follow the emitting ship/hardpoint for their short lifetime.
`A2FOAnimatedHardpoints` already hooks native world-position/world-transform
queries, so use those resolved transforms. Preserve visibility, fog-of-war,
cloak, parent destruction, and missing-asset behavior. Keep cosmetic randomness
out of the synchronized gameplay random stream.

Acceptance cases: pulse and torpedo bursts; multiple firing hardpoints; empty
magazines; arc/technology rejection; animated mounts and attached turrets;
moving ships; offscreen/hidden owners; missing effect assets; owner deletion.

## 3. Cloaking renderer pipeline

### What the addresses establish

The local Heroic Armada II 1.1 reference executable was disassembled at the
selector. It agrees with the addresses in the supplied discussion:

| Address | Observed action |
| --- | --- |
| `0x006327cf` | Read the existing MeshVB pointer from mesh `+0x128` |
| `0x006327dc` | Call the MeshVB eligibility virtual method |
| `0x006327e7` | Call device virtual slot `+0x74` |
| `0x006327ec` | `75 1a`: a true polygon-sort result branches to non-VB |
| `0x006327f0` | A separate external-renderer check also branches to non-VB |
| `0x006327fc` | Call `RenderInternalVB` at `0x00631f10` |
| `0x00632812` | Call `RenderInternalNonVB` at `0x00631fd0` |

The reference symbols place `PolygonSortRequired` at `0x00625510`.
Thus sorted materials are diverted to the slow path; the discussion's sentence
saying non-cloaked objects are excluded has the direction reversed at this
branch. Sorting eligibility is a rendering-state test, not by itself a test
of a particular cloak classlabel.

The reported `0x00631d66` creation switch and mesh `+0x12c` / texture `+0x18`
flag behavior remain leads from Jan_B's experiments. They were not independently
traced here. Do not globally force those flags: draw eligibility cannot supply
missing vertex buffers, tangent data, or a valid normal texture.

### Relevant code already in A2FO

`core/nebula_renderer.cpp` already hooks the selector at Armada RVA
`0x002327cf`. `core/renderer_draw_policy.hpp` defines its fast-alpha admission
policy. The source reads `Data/A2FORenderer.ini` as follows:

```ini
[Compatibility]
FastAlphaMeshVB=1

[Diagnostics]
RendererRouteCounts=1
MappedTextureCloak=1
```

This is a research configuration example; no installed settings were changed.
`FastAlphaMeshVB=0` retains native sorting, `1` admits native-opaque transitions
and additive destinations, and `2` also admits other transparent materials
with approximate triangle ordering.

Crucially, admission additionally requires the active fast non-bump shader
path. The documented path is managed DXVK with bump mapping disabled and
`NeutralBumpWhenDisabled` enabled. System-renderer isolation returns before
installing these material hooks. This is not a general bump-on cloak fix.

The existing code also does more than bypass sorting:

- `a2fo_nebula_try_fast_nonbump_alpha_meshvb` at line 5524 immediately applies
  native `ST3D_TextureMaterial::SetRenderState_ZSort` at RVA `0x00244880`.
- The final FO DOT3 draw hook at line 5501 reapplies that state after FO resets
  it to opaque, and the alpha preparation code at line 4529 copies the live
  Storm3D material alpha into vertex constant `c0.w`.
- Route counters distinguish missing MeshVB, eligibility, polygon sorting,
  external-renderer rejection, and fast-alpha selections.

Use those counters to establish whether fully cloaked ships, transition frames,
and individual materials actually reach this path. The existing mode-1 label
does not prove correct ordering for overlapping faded geometry; mode 2 is
explicitly approximate.

### Why a blend-byte change is insufficient

The supplied [shaderPlusArmada reference](https://github.com/FNSOIDATHQ/shaderPlusArmada/blob/master/dx9plus.cpp)
contains both a programmable rewrite and a reconstruction of the old DOT3
passes. The latter binds the normal map for lighting, accumulates light passes,
then multiplies the diffuse texture into the framebuffer result. Removing the
normal-map bind does not replace that lighting calculation with Lambert
lighting; it leaves the texture-stage calculation without its intended input.

The blend values also matter: `(2, 2)` means `ONE, ONE`, an additive blend.
It can leave the background visible without reproducing cloak opacity.
`SRCALPHA, INVSRCALPHA` is `(5, 6)`. These values are documented in
[Microsoft's D3DBLEND reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dblend).
A conventional target composite is `alpha * litHull + (1 - alpha) * scene`;
changing the first pass of a multipass renderer does not establish that result.

The shader reference also disables `ALPHAOP` while a color operation remains
active. Microsoft documents that combination as undefined in
[D3DTEXTUREOP](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop).
Explicit alpha-stage behavior belongs in the investigation, rather than relying
on how one wrapper happens to treat that state.

The reference's material-index labels are inconsistent: `[9..11]` feeds
`envColor` in one routine but is labelled diffuse in `dot3MeshVBDrawLight9`.
Do not infer that setting those floats to black must change native map ambient
lighting. Establish the native material layout and the executed shader inputs.

### Recommended next renderer work

1. Capture existing route counters for bump-on/off, uncloaked/transition/fully
   cloaked craft, and each renderer backend. Record CPU time and visual results
   separately; the chat's speed figures have not been reproduced here.
2. Trace steady-cloak material alpha, blend pair, depth-test/depth-write state,
   and texture-stage alpha at the final draw. Transitions and fully cloaked
   materials must both retain the native visibility policy.
3. Extend the existing scoped draw path only after identifying the rejected
   route. A correct fast bumped path must calculate lit hull color while
   preserving the scene background, and then composite with the actual cloak
   alpha. Keep a native fallback for unsupported materials and devices.
4. Preserve transparent ordering where required. Cached geometry with a sorted
   index stream is one candidate; skipping triangle sorting is a visual
   approximation, including for overlapping otherwise-opaque fading meshes.
5. For materials without bump maps, preserve valid geometry and lighting input.
   A neutral normal texture or a shader using the geometric normal is a
   candidate. The existing DXVK flat-normal shader is a useful starting point.
6. Retain CPU vertex-color initialization on the legacy path. Skipping it
   leaves prior scratch values available; setting ambient black is not a
   substitute for defined per-frame lighting data.

No renderer branch bytes, game DLLs, installed configuration, or input devices
were changed by this investigation. The local Fleet Ops launcher/DLL files
examined did not expose the reported native callback at its stated VA, so
FleetOpsHook callback-byte validation remains outstanding. Use the supported
image identity in `sdk/include/a2fo_supported_armada.hpp` and `docs/addresses.md`
when preparing an implementation; do not assume every installed copy matches.

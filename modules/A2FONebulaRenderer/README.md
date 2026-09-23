# A2FONebulaRenderer

`A2FONebulaRenderer.dll` enables the DirectX 8 per-pixel ship lighting port from
[armadaNebulaPatch](https://github.com/FNSOIDATHQ/armadaNebulaPatch) into the
A2FOExtensions runtime. The upstream work and bundled shader programs
are Copyright (c) 2024 dev gao and used under the MIT License.

This first integration deliberately supports the normal DX8 renderer only.
Launching with `/d3d9` or `-d3d9` leaves the module loaded but inactive and
records that decision in `A2FOExtensions.log`. The unfinished experimental DX9
path from upstream has not been imported.

## Installation

Copy both release outputs into the game's `Data` directory:

```text
modules/A2FONebulaRenderer.dll
Shaders/
├── dot3_amd.nvv
├── dot3_amd9.nvv
└── dx8/
    └── pixel/
        ├── ps.nvv
        └── ps_specular.nvv
licenses/armada-nebula-patch.txt
```

Fleet Operations' native multipass DOT3 renderer remains active. On an AMD
adapter using System Direct3D 9, the core may substitute a matched declaration
and shader before the shared shader is created. The normal D3D8-to-D3D9 route
uses `dot3_amd.nvv`; Fleet Operations' separate `/d3d9` route uses
`dot3_amd9.nvv`. Both keep the stock lighting math while exposing UV/tangent
fields through neutral D3D9 texture-coordinate semantics. The packaged
`ps.nvv` and `ps_specular.nvv` files are retained as forward-development
assets, but are not selected by the bump-safe runtime. Bumped emissive
materials use a scoped fixed-function stage at the final draw.
Bumped specular maps use a separate, quarter-strength additive
replay immediately afterward, isolated from Fleet Operations' earlier normal-
map light draws.
Removing `A2FONebulaRenderer.dll` before launch disables mapped emissive and
specular rendering; its DXVK hook sites remain native pass-throughs. The
system-backend AMD DOT3 candidates are core compatibility options and are
instead controlled by `AmdNativeDot3Fix` below.

Do not install armadaNebulaPatch's `Win2kDisableTaskSwitch.dll`, `shader+.dll`,
MinHook, hook-tools DLL, runtime DLLs, or `dll/after.list` alongside this port.
A2FOExtensions already owns startup and checked patching, and two competing
startup proxies cannot coexist.

## Runtime behaviour

Armada creates its shared DOT3 shader before ordinary deferred modules load.
The core therefore installs checked pass-through sites during process attach.
At the first DOT3 mesh—outside loader lock—it checks that
`A2FONebulaRenderer.dll` is installed and only then enables D3DX texture
loading. The deferred module acts as the opt-in controller and reports the
already-armed subsystem's status.

The runtime:

- validates the exact supported Armada/Fleet Ops PE identities and every
  renderer signature before enabling;
- leaves Fleet Operations' stock DOT3 vertex-shader path and source untouched
  except for the exact AMD/system-D3D9 compatibility substitution described
  below;
- leaves Fleet Operations' DOT3 `GetShaderHandle` function-pointer slot
  untouched, avoiding redundant shader state calls between its native handle
  lookup and `SetVertexShader` on Windows dxwrapper/d3d8to9 systems;
- resolves the renderer's current live DX8 wrapper only at the scoped final
  material draw, without selecting a pixel shader during Fleet Operations'
  native normal-map light draws;
- preserves Fleet Operations' complete multipass DOT3 bump sequence and adds
  a mapped emissive texture only at the exact final indexed draw;
- replays only specular-mapped final geometry once with a bounded additive
  intensity overlay, then restores the complete preceding D3D8 state;
- combines the active craft's configured subsystem emissive maps into one
  cached texture and binds it to the shader's second sampler; generated
  composites use a bounded least-recently-used cache rather than growing for
  every subsystem and movement state seen during a long battle;
- preserves each loose emissive source's authored RGB values for the sharp
  self-lit material centre, builds a complete mip chain, and uses trilinear
  filtering so thin lights remain stable during camera movement; mostly-black
  source maps are retained losslessly as sparse non-black texels to reduce RAM;
- applies the same composite to classic/non-DOT3 SODs as a scoped additive
  fixed-function texture stage on MeshVB and both observed workspace classes,
  restoring the complete preceding state after each draw;
- retains native DOT3 bump rendering and adds the emissive composite through a
  scoped stage-2 fixed-function combiner around the existing indexed draw; the
  geometry is submitted only once;
- enables the selective private-mask framebuffer compositor only with the
  restart-applied managed DXVK backend, avoiding the unstable
  dxwrapper/d3d8to9/ReShade system-renderer path;
- disables the pixel shader at Fleet Operations' fixed-pipeline transition,
  then resumes the displaced code and all remaining alpha draws.

These mapped-material renderer and SOD-mutation paths are enabled only when the
managed DXVK payload is the active `Data\\d3d9.dll`. On the Windows system
renderer the module remains loaded for configuration/reporting, class
registration is a no-op, and Fleet Operations owns the complete native draw
sequence. The AMD compatibility path changes only the shared shader's input
declaration/source pair before creation; it installs no draw or render-state
hooks.

## AMD native DOT3 compatibility

Roots' normal route translates Armada's D3D8 vertex declaration to D3D9, while
its `/d3d9` route constructs a D3D9 declaration directly. In both cases Fleet
Operations' stock DOT3 stream labels normal, UV, and tangent data with the
special-purpose `BLENDWEIGHT`, `BLENDINDICES`, `NORMAL`, `PSIZE`, and `COLOR`
semantics. Some AMD system-D3D9 paths render that legacy combination
incorrectly.

The compatibility candidate preserves the stream byte layout, stride, shader
math, textures, render states, and all Fleet Operations draws. It moves those
five fields to `v7` through `v11`, which translate to `TEXCOORD0` through
`TEXCOORD4`. Before applying, the core verifies the supported executable and
FleetOpsHook identities, the route-specific creation callback, the stock
declaration/source signature, the replacement asset, and the active adapter
PCI vendor. The replacement is assembled with the D3DX8 or D3DX9 assembler
already used by the selected route. Any failed check retains the stock shader.

`Data\\A2FORenderer.ini` controls the candidate:

```ini
[Compatibility]
; 0 = disabled, 1 = automatic on AMD system D3D9, 2 = force for A/B testing
AmdNativeDot3Fix=1
; Preserve fast DOT3 geometry with a self-contained flat-normal shader when native bumps are off
NeutralBumpWhenDisabled=1
; Suppress bump shading only for the craft currently cloaking/cloaked/decloaking
NeutralBumpWhenCloaked=1
; 0 = native sorter, 1 = opaque fades/additive, 2 = all transparency (test)
FastAlphaMeshVB=1
```

The setting is read at process startup and requires a restart. The AMD fix is
ignored when the managed DXVK payload is active; both System D3D9 launch routes
are covered. Logs identify the selected adapter and whether the remap was
applied or safely skipped.

`NeutralBumpWhenDisabled` activates only while the saved Fleet Operations
`Settings.xml` contains `disable_bump=True` and the core renderer is available.
It preflights `vs_flat_lighting.nvv`, then keeps Storm3D's DOT3 eligibility
enabled at runtime without changing material texture assignments. On DXVK,
the core selects that shader only around Fleet Operations' native per-light
indexed draw. It transforms and normalizes the light through the MeshVB's
tangent basis, then selects its Z component as the result of a fixed flat
normal. Stage 0 changes from `D3DTOP_DOTPRODUCT3` to that diffuse result, so the
bump texture is not sampled and `all_bump.dds` is not required. Because Fleet
Operations' graphics-options object is not created when deferred modules
initialize, a checked two-byte patch bypasses only the matching bump-disabled rejection in
`ST3D_Dot3_MeshVB::CanRender`; its GPU-capability test remains intact. The
persisted native option is not changed. Set the compatibility value to `0` to
restore the native non-VB bump-off path. A restart is required.

Fleet Operations normally moves materials to its CPU per-triangle sorter during
cloak, decloak, construction, and other alpha passes. `FastAlphaMeshVB=0`
retains that behavior. Mode `1` (default) keeps native-opaque whole-model fades
and order-independent additive materials on their existing MeshVB after
applying Storm3D's own z-sort blend state immediately. Mode `2` also admits
ordinary transparent blend modes for maximum performance. It preserves their
authored blend state but draws the existing index order rather than Fleet Ops'
per-frame triangle order, so intersecting transparent surfaces may display
differently. Immediately before the final material draw, redirected draws
reapply Storm3D's z-sort blend state after Fleet Operations' internal opaque
reset and supply the live material/object alpha in vertex constant `c0.w`.
This supplies the alpha missing from static MeshVB vertex colours. The setting
is restart-applied and affects the active DXVK flat-normal route, whether
requested globally or for an individual cloaked craft. Final-draw alpha alone
does not establish correct composition of every earlier multipass lighting
draw; cloak transparency and overlapping surfaces still need in-game checks.

### Per-unit bump suppression while cloaked

`[Compatibility] NeutralBumpWhenCloaked=1` (default) selects the existing
flat-normal vertex-lighting shader for the craft being drawn when its cloak
controller is cloaking, fully cloaked, or decloaking. Once the controller
returns to visible, that unit automatically resumes its authored bump shading
unless the player's global bump-off option is still active. Merely carrying
a cloak weapon does not disable bump shading.

This is draw-scoped: it does not detach bump textures, change shared SOD mesh
flags, modify the saved global bump option, or affect another visible instance
using the same model. Existing per-light state restoration restores the native
shader and texture combiner after each redirected draw. Flat-normal lighting
still responds to the native directional-light calculation; it is not unlit
white/constant shading.

The scope is the managed DXVK DX8 path with a valid existing MeshVB. The
ordinary `FastAlphaMeshVB` admission policy still applies; missing vertex
buffers, rejected material modes, and missing/unsupported flat-normal shaders
retain the native path. The shader is prepared before sorted geometry can be
redirected. This option does not force missing bump maps or tangent buffers
into existence and does not make cached triangle order exact. Set
`FastAlphaMeshVB=0` to retain the native sorter if fast-alpha visuals are not
acceptable, or `NeutralBumpWhenCloaked=0` to disable this per-unit feature.
Both options are read at startup and require a restart.

The first successful cloak-only flat-normal draw is logged separately from
global bump-off draws. Check cloak-in, steady cloak, cloak-out, and a visible
ship sharing the same SOD when validating this option.

That final gateway is an intentional safety change from upstream. The original
patch returned from the renderer in the middle of the function, which fixed
nebula ship rendering but skipped later alpha geometry and made editor crystal
letters black. This port preserves Fleet Operations' remainder instead.

All installed early hooks are pass-through until the complete feature has
enabled. An absent controller/shader, missing D3DX export, changed signature,
or DX9 mode therefore leaves native rendering active and logs the reason.

## Global texture suffixes

Mods can opt into filename-based emissive, bump-map, and specular-map discovery
once in their active or inherited `ART_CFG.h`:

```cpp
#define A2FO_EMISSIVE_SUFFIX "_emissive_"
#define A2FO_BUMP_SUFFIX "_bump"
#define A2FO_SPECULAR_SUFFIX "_specular"
#define A2FO_EMISSIVE_BUMP_MULTIPLIER 2.0
#define A2FO_BUMP_LIGHT_BIAS 0.55
#define A2FO_EMISSIVE_DIFFUSE_RESTORE 1.0
```

The renderer inspects the actual diffuse texture on every loaded SOD material.
For a diffuse called `fbattle`, the example emissive suffix searches for:

- `fbattle_emissive_warp`
- `fbattle_emissive_impulse`
- `fbattle_emissive_shields`
- `fbattle_emissive_life`
- `fbattle_emissive_sensor`
- `fbattle_emissive_weapons`

The bump suffix searches for `fbattle_bump`, and the specular suffix searches
for `fbattle_specular`. Filename matching is case-insensitive and extensions
are optional. Emissive and specular maps retain the loose-file formats and root
precedence described below. Bump maps use Storm3D's native DDS/TGA lookup,
including ordinary Fleet Operations archive resolution.

Specular maps use the diffuse texture's UV layout. Black contributes no gloss;
brighter RGB contributes a stronger broad highlight. The bump-safe runtime
draws that mask at quarter strength in a separate additive replay after Fleet
Operations has completed the native bump and diffuse passes. This is a broad
material-gloss approximation rather than view-dependent Phong/PBR specularity.
A material without a bump map keeps its ordinary renderer.

The DOT3 emissive/specular draw interception is enabled only with the managed
DXVK backend. On the System Direct3D 9 / WineD3D backend, Fleet Operations'
native bump draw is left completely unintercepted because the old Windows
dxwrapper path retains driver-private state across that boundary and produces
vendor-sensitive results. Native bump maps remain available there; extension
emissive/specular overlays on bumped materials require DXVK.

`A2FO_EMISSIVE_BUMP_MULTIPLIER` is retained for the redesigned bumped-material
extension pass. It is temporarily inactive while bumped emissives use the
fixed-function compatibility route. The accepted range remains `0.0` through
`8.0`; omission defaults to `1.0`.

`A2FO_BUMP_LIGHT_BIAS` is also retained for the redesigned extension pass and
is temporarily inactive. Native Fleet Operations lighting now determines the
brightness of bumped hulls. Its accepted range remains `0.0` through `1.0`.

`A2FO_EMISSIVE_DIFFUSE_RESTORE` is likewise retained but temporarily inactive
for bumped materials. It defaults to `0.0` and accepts `0.0` through `2.0`.

Faction diffuse variants from `A2FOTextureVariants` use the same Race ODF
`factionTextureSuffix` values. The mapped-lighting controller also treats stock
Borg `_b` as a known suffix even when `borg.odf` does not declare one. The
official naming rule is **map role first, faction suffix last**. If a material
`fbattle` is rendered as `fbattle_b`, the renderer looks for
`fbattle_emissive_warp_b` and `fbattle_specular_b`, then falls back to the base
`fbattle_emissive_warp` / `fbattle_specular` maps when a faction-specific map
is absent. Names such as `fbattle_b_emissive_warp` are not the faction-map
convention. Explicit ODF emissive declarations remain authoritative and are
reused for faction diffuse aliases. Late-loaded ownership ODF classes whose SOD
already stores a suffixed diffuse such as `fbattle_b` are canonicalized back to
`fbattle` for auxiliary-map discovery, so they resolve
`fbattle_emissive_warp_b` / `fbattle_specular_b` rather than attempting the old
`fbattle_b_emissive_warp` / `fbattle_b_specular` ordering.

This final-suffix convention is also the intended naming for future
ownership-scoped bump/normal variants: `fbattle_bump_b`, `fbattle_bump_k`, etc.
Bump maps are currently class material state rather than ownership-scoped
state, however, so a base bump remains active when only the diffuse switches
faction. Selecting faction-specific bump/normal maps safely requires a
draw-scoped texture-slot-1 override rather than mutating the shared CraftClass
mesh.

Only materials for which the derived file exists are changed. A bump texture
already stored in the SOD wins over the global convention. A derived bump map
is added as native texture slot 1 and that mesh is rebuilt through Armada's
DOT3 MeshVB path at class-load time; the SOD and source texture files are not
rewritten.

Explicit emissive ODF declarations also win. An unnumbered declaration keeps
the existing class-wide wildcard behaviour. In indexed mode, each explicit
`emissiveX<Subsystem>` overrides the derived filename for that channel while
undeclared channels may still use the global suffix. Set any suffix macro to
an empty quoted string, or omit it, to disable that convention. Suffixes accept
up to 64 ASCII letters, digits, underscores, and hyphens.

Fleet Operations' Graphics Options screen retains its native **Bump Mapping**
checkbox and adds independent **Emissive Maps** and **Specular Maps** boxes
beside it. The new switches apply immediately, default on, and persist as
`EmissiveMaps` and `SpecularMaps` under `[Effects]` in
`Data/A2FORenderer.ini`. With DXVK selected, emissive maps also receive native
framebuffer bloom by default. Set restart-applied `EmissiveBloom=0` in the same
section to retain only their sharp self-lit centres.

For renderer diagnosis, restart with
`[Diagnostics] MappedTextureCloak=1` in `Data/A2FORenderer.ini`. The log then
records the native texture stages, colour operations, shaders, blend state,
and fixed/workspace versus DOT3 route once for each visible, cloaking, fully
cloaked, and decloaking state reached by a mapped-lighting craft. Leave it at
`0` or remove it during normal play; no per-draw state inspection occurs when
the option is disabled.

`[Diagnostics] RendererRouteCounts=1` logs 60-frame draw-route totals and the
first failed guard in Armada's MeshVB selector: no MeshVB object, eligibility,
polygon sorting, external renderer, ordinary fast selection, or the scoped
alpha MeshVB selection, split into opaque, additive, and aggressive transparent
counts. It requires a restart and should be removed after diagnosis.

## Subsystem emissive maps

For a multi-textured craft or station, number each diffuse material and use
the same index on its subsystem maps:

```cpp
texture0              = "fbattle"
emissive0Warp         = "Fbattle_emissive_warp"
emissive0Impulse      = "Fbattle_emissive_impulse"
emissive0Shields      = "Fbattle_emissive_shields"
emissive0LifeSupport  = "Fbattle_emissive_life"
emissive0Sensors      = "Fbattle_emissive_sensor"
emissive0Weapons      = "Fbattle_emissive_weapons"

texture1              = "fbattle_secondary"
emissive1Weapons      = "Fbattle_secondary_emissive_weapons"
```

`textureX` identifies the diffuse/base texture used by that SOD material. The
renderer compares it with Storm3D's name for the diffuse texture currently
bound at the draw call, so indices do not have to follow SOD material order.
Names are case-insensitive; directories and file extensions are ignored, so
`fbattle`, `Fbattle.tga`, and `Textures/RGB/Fbattle.dds` identify the same
material. Sparse indices from 0 through 63 are accepted. Each diffuse material
gets its own lazily built subsystem composites and only affects geometry drawn
with that texture.

The original unnumbered form remains supported for existing one-texture ODFs:

```cpp
emissiveWarp        = "fcruise1_warp.dds"
emissiveImpulse     = "fcruise1_impulse.dds"
emissiveShields     = "fcruise1_shields.dds"
emissiveLifeSupport = "fcruise1_lifesupport.dds"
emissiveSensors     = "fcruise1_sensors.dds"
emissiveWeapons     = "fcruise1_weapons.dds"
```

The unnumbered form is a class-wide wildcard and is therefore best kept for
single-texture models. Declaring any `textureX` switches that class to indexed
mode; unnumbered commands are then ignored so an inherited legacy map cannot
bleed onto an unrelated material.

Each image uses the ship's normal diffuse UV layout. Paint parts which should
emit light in colour and leave everything else black. Alpha is ignored. Maps
may have different source dimensions: the first active map establishes the
material composite size and the remaining maps are scaled to match. When several
enabled maps cover the same pixel, the brightest value in each RGB channel is
used. This avoids six extra geometry passes while retaining independent system
failure behaviour.

The commands are inherited through the ordinary ODF/ParameterDB chain. Bare
emissive-map filenames are resolved from loose `Textures`, `Textures/RGB`,
`Textures/Index8`, and `Textures/Compressed` directories, searching Data,
parent mods, and the active mod in normal override order. `.dds`, `.tga`, and
`.png`, and `.bmp` are tried when no extension is written. Explicit
`Textures/...`,
root-relative, drive-absolute, and UNC paths are also accepted.

This first implementation deliberately loads loose image files through
`D3DX81ab.dll`; emissive images packed only inside an FPQ are not yet visible to
the D3DX loader. Keep the emissive images loose for now. `textureX` itself is
only a material identifier, so the corresponding ordinary diffuse texture may
still use Fleet Operations' normal loose or packed asset loading.

System state mapping is:

- `emissiveXWarp` and `emissiveXImpulse` both follow Armada's one native Engines
  system—Armada does not expose separate warp and impulse damage records;
- `emissiveXShields`, `emissiveXLifeSupport`, `emissiveXSensors`, and
  `emissiveXWeapons` follow their matching native CraftSystem records.

The same mapping applies to the legacy unnumbered command names.

An operational system remains continuously lit. A healthy system disabled by
system control or a timed-disable effect flickers irregularly on and off, with
each ship/system using an independent 90 ms phase. A destroyed system—and a
destroyed system still repairing below its full native hitpoint count—keeps its
map off. A missing command adds nothing and preserves normal rendering.

Engine-map intensity also follows live movement. The authored RGB level is
100%; warp emission rises to 125% in Fleet Operations' normal/gravity-well
regime and to 200% only in the native steady at-warp state. While a ship is
actually moving at impulse, impulse emission rises to 150% and warp emission
retains its 125% gravity-well level. Warp-in and warp-out use the lower warp
profile so the full change coincides with the engine's own at-warp state.
Channels saturate at 255, so very bright source artwork may show less motion
variation once its material centre is already white.

Composite textures are created lazily for only the active subsystem and motion
profiles actually encountered by each material. The eight most recently used
composites per material are eligible to remain cached, with a 96 MiB global
target for generated mip chains. One live composite per material is retained
even when that floor exceeds the target, avoiding constant rebuilding when a
scene contains many different ship classes. Eviction changes only derived
cache residency; source artwork, generated pixels, mip levels, and filtering
remain identical.

Emissive sources which are less than half non-black are stored as an exact
index/ARGB sparse list after loading. Denser sources retain the original packed
ARGB array, so the representation is never larger than the previous one.

The shader makes these pixels self-lit and independent of map lighting. The
runtime preserves that sharp material centre. With the managed DXVK backend,
each registered emissive draw is also replayed into a private full-resolution
mask. A half-resolution separable blur is screen-composited before `EndScene`,
producing a genuine coloured halo without blooming unrelated UI or bright map
objects. The compositor remains off on the system renderer because the old
dxwrapper/d3d8to9/ReShade chain cannot reliably restore Armada's opaque state
across UI/edit-mode transitions.

All observed render families are covered. Bump/DOT3 meshes retain Fleet
Operations' native multipass lighting and consume the emissive composite from
stage 2 through a scoped fixed-function addition at the final draw. Ordinary
MeshVB and classic workspace meshes receive it after their native material
setup through Direct3D 8 texture stage 1 when that stage is free, or stage 2
when a native bump/secondary texture already occupies stage 1. Every scoped
route restores the preceding material state. Ships such as the classic
`fbattle.sod`
therefore retain both bump lighting and the additive emissive layer in the
same draw. Registered specular masks are replayed only after that native DOT3
draw and never replace its normal-map lighting shader.

## Subsystem and hull damage decals

The same core DX8 render boundary can draw alpha-textured quads attached to
SOD hardpoints. Each entry belongs to one native subsystem or to hull health
and appears when its numbered damage interval has been crossed:

```odf
damageThreshold = 0.1

// Optional authoring/debug mode: show every configured decal immediately.
// Remove this (or set it to 0) for normal damage-threshold behaviour.
damageDecalPreview = 1

hullScorch1 = "scorch"
hullScorch1Hardpoint = "hp06"
hullScorch1Offset = "0.0 0.0 0.2"
hullScorch1Rotation = "0.0 0.0 0.0"
hullScorch1Size = "6.0 6.0"

enginesScorch1 = "scorch_engine"
enginesScorch1Hardpoint = "hp10"
enginesScorch1Offset = "0.0 0.0 0.15"
enginesScorch1Rotation = "0.0 0.0 0.0"
enginesScorch1Size = "4.0 4.0"
```

Supported prefixes are `sensors`, `engines`, `weapons`, `lifeSupport`,
`shieldGenerator`, and `hull`. Entry 1 appears at one threshold, entry 2 at
two thresholds, and so on. Hull uses the live GameObject current/maximum health
fields at `+0x15c/+0x160`; the other five use their native CraftSystem records.
Decals are per-instance, depth-tested, alpha blended, and follow animated
hardpoint transforms.

`damageDecalPreview = 1` bypasses the health check while placing or diagnosing
decals. It uses the exact same texture, hardpoint transform, and DX8 draw path
as normal damage decals; set it back to `0` once placement is complete.

For compatibility, a ship with only `scorchTextureX` and native
`*TargetHardpoints` lists receives automatically generated entries. Explicit
`<system>ScorchX...` placement commands take priority. A2FO Arc Lab includes a
live decal placement panel and generates the explicit ODF block.

## Selected ship-name logo decals

Permanent mapped logo planes can follow Fleet Operations' selected ship-name
row without repeating texture names in the ship ODF:

```odf
possibleCraftNames = "USS Enterprise" "USS Excelsior"
logoFileNames = "logo_enterprise" "logo_excelsior"

// Uses the selected logoFileNames entry exactly. Packed FPQ textures work.
logoDecal1Hardpoint = "hp_name"
logoDecal1Offset = "0.0 0.0 0.12"
logoDecal1Rotation = "0.0 0.0 0.0"
logoDecal1Size = "5.5 1.2"
// Optional for legacy RGB artwork with an opaque white background.
logoDecal1ColourKey = "255 255 255"
// Optional when the mapped plane is viewed from its reverse-facing side.
logoDecal1FlipU = 1

// Optional split artwork: logo_enterprise_lower.dds, etc.
logoDecal2Hardpoint = "hp_name_lower"
logoDecal2Suffix = "_lower"
logoDecal2Offset = "0.0 0.0 0.12"
logoDecal2Rotation = "180.0 0.0 0.0"
logoDecal2Size = "5.5 1.2"
```

`logoDecalX` is indexed from 1 through 64. The placement is permanent and the
runtime reads the craft's native selected `possibleCraftNames` index every
frame, so capture, save/load, and native name selection continue to choose the
matching `logoFileNames` row.

`ScaleSOD` is applied automatically to decal sizes and offsets, so Arc Lab's
raw-SOD placement remains aligned with the scaled model rendered in game.
`FlipU = 1` reverses the texture horizontally without changing the plane's
position or rotation; this is useful when the exposed hull side is the back
face of the mapped plane.

With no `Suffix`, the renderer reuses Fleet Operations' already-loaded native
logo texture and therefore supports both loose and packed assets. A suffix is
inserted before an existing extension, or appended to an extensionless name:
`name.tga` plus `_upper` becomes `name_upper.tga`; `name` becomes
`name_upper`. Suffixed variants are resolved automatically as loose `.dds`,
`.tga`, `.png`, or `.bmp` files in the normal texture roots. Separate
placements can use `_upper`, `_lower`, `_nacelle_left`, and so on; the modder
only supplies the suffix and transform, not a second row list.

RGBA/32-bit TGA and DDS alpha is blended directly. `ColourKey` is optional and
takes an RGB triplet from 0 through 255; matching pixels become transparent
when a loose logo file is loaded. This is useful for older 24-bit name art,
which has no alpha channel. Packed textures use Fleet Operations' existing
native texture object and therefore need authored alpha rather than the loose-
file colour-key conversion.

## Current shader limitations

The lighting behaviour is still based on the upstream first version, now with
the independent ODF emissive sampler described above. Point lights and light
colours are not fully represented, rim lighting can disappear very close to a
model, and unusual free-camera views may expose matrix/camera assumptions.
Those are rendering-quality limitations rather than unchecked hook failures.
Shader tuning/configuration and a separately engineered DX9 path can be added
after the DX8 integration has been tested in game.

The complete address and ownership record is in
[`../../docs/addresses.md`](../../docs/addresses.md). The upstream licence is
vendored at
[`../../third_party/armada-nebula-patch/LICENSE.txt`](../../third_party/armada-nebula-patch/LICENSE.txt).

## Automatic fast rendering for models without bump maps

```ini
[Compatibility]
FastUnmappedMeshVB=1
```

This restart-applied option defaults to `1` on the managed DXVK/DX8 renderer.
Unlike `NeutralBumpWhenDisabled`, it does not require the saved global bump
option to be off. Unlike `NeutralBumpWhenCloaked`, it also covers visible craft.

A checked hook on the non-DOT3 creation branch at Armada RVA `0x00231d7e`
lets compatible, textured Lambert meshes without a bump map obtain native
DOT3 MeshVB geometry. The native factory creates vertex/index buffers,
UV-seam duplicates and tangent data; the engine retains buffer ownership and
rebuild/destruction responsibilities. Existing native DOT3/standard meshes,
constant-lit effects, Phong materials, malformed geometry and layouts outside
the 16-bit index budget are not forcibly converted. Admission is scoped to
craft rendering; other scene objects retain native selection.

Promoted meshes select the existing geometric flat-normal lighting shader
for every admitted draw, whether visible or cloaked. Fleet Operations still
accesses texture slot 1 before drawing, so a call-local texture array supplies
a valid borrowed diffuse texture in that unused slot. The flat-normal shader
does not sample it as a normal map. No SOD texture assignment, mesh flag,
texture flag, or global bump-map setting is changed. Models with authored bump
maps retain their existing rendering policy. If a bump texture is attached
later to a promoted mesh, it falls back rather than flattening the new map.

Device capability, shader readiness, external-renderer and alpha-material
checks remain. `FastAlphaMeshVB` still determines which sorted materials may
use existing MeshVB index order; this option does not solve exact transparent
triangle ordering or the native multipass cloak-composition limitation.
This is a fast-path preference, not an unconditional force-everything patch.
Lighting uses the existing DOT3 directional-light path, not a guarantee of
pixel-identical ambient/affector lighting compared with CPU Lambert rendering.

Startup logs report whether automatic creation was enabled or safely skipped.
The first few successful preparations log `Prepared native MeshVB buffers for
a model without a bump map`. First visible/cloaked submissions separately log
`Unmapped craft MeshVB draw active while visible` and `Unmapped craft MeshVB
draw active while cloaked or transitioning`. Existing renderer-route counters
should then show Fleet Ops DOT3 submissions for admitted meshes instead of
only `no-MeshVB` fallbacks. Counters describe the whole scene, not ship counts.

Set `FastUnmappedMeshVB=0` and restart to restore native buffer creation.
Runtime validation is still required: compare the same non-bump Warbirds at
the same camera angle, visible and cloaked, then check decloaking, overlapping
ships, lighting, map-editor rendering, and device reset/map reload. The source
change alone is not evidence of a measured FPS improvement.

### Late-loaded ODF variant classes

`A2FOODFVariants` may load a suffixed CraftClass (for example `fbattle_b`)
only when ownership changes. Some Fleet Operations class paths do not expose
usable cached SOD/material state until a craft instance has actually been
constructed. The ODF variant module therefore requests a SOD/ART-only refresh
after constructing the replacement craft and retries once on its first
simulation tick. A null-ParameterDB refresh logs whether SOD materials were
available, making late-load timing failures visible. The final-suffix ownership
convention remains `fbattle_emissive_warp_b` / `fbattle_specular_b`, with
base-map fallback.

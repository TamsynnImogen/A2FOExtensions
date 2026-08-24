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
Shaders/dx8/
└── pixel/
    ├── ps.nvv
    └── ps_specular.nvv
licenses/armada-nebula-patch.txt
```

Fleet Operations' native `Shaders/dot3_directional.nvv` and native multipass
DOT3 renderer remain active and are never replaced. The packaged `ps.nvv` and
`ps_specular.nvv` files are retained as forward-development assets, but are not
selected by the bump-safe runtime. Bumped emissive materials use a scoped
fixed-function stage at the final draw. Bumped specular maps use a separate,
quarter-strength additive replay immediately afterward, isolated from Fleet
Operations' earlier normal-map light draws.
Removing `A2FONebulaRenderer.dll` before launch disables the feature; the core's
early hook sites remain native pass-throughs and the shader files may remain in
place harmlessly.

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
- leaves Fleet Operations' stock DOT3 vertex-shader path and shader source
  untouched;
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

These renderer and SOD-mutation paths are enabled only when the managed DXVK
payload is the active `Data\\d3d9.dll`. On the Windows system renderer the
module remains loaded for configuration/reporting, but class registration is a
no-op and Fleet Operations owns the complete native DX8/DOT3 path.

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
native bump draw is left completely unintercepted because Windows dxwrapper
and some vendor drivers crash when that boundary is wrapped. Native bump maps
remain available there; extension emissive/specular overlays on bumped
materials require DXVK.

`A2FO_EMISSIVE_BUMP_MULTIPLIER` is retained for the redesigned bumped-material
extension pass. It is temporarily inactive while bumped emissives use the
fixed-function compatibility route. The accepted range remains `0.0` through
`8.0`; omission defaults to `1.0`.

`A2FO_BUMP_LIGHT_BIAS` is also retained for the redesigned extension pass and
is temporarily inactive. Native Fleet Operations lighting now determines the
brightness of bumped hulls. Its accepted range remains `0.0` through `1.0`.

`A2FO_EMISSIVE_DIFFUSE_RESTORE` is likewise retained but temporarily inactive
for bumped materials. It defaults to `0.0` and accepts `0.0` through `2.0`.

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

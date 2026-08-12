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
├── pixel/
│   ├── ps.nvv
│   └── ps_1.3.nvv
└── vertex/
    ├── vs.nvv
    └── vs_1.3.nvv
licenses/armada-nebula-patch.txt
```

`vs.nvv` and `ps.nvv` are the active shader pair. The `_1.3` files are the
upstream compatibility/reference pair and are packaged for shader authors.
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
At the first DOT3 mesh—outside loader lock and immediately before Armada reads
the shader path—it checks that `A2FONebulaRenderer.dll` is installed and only
then performs file/D3DX activation. The deferred module acts as the opt-in
controller and reports the already-armed subsystem's status.

The runtime:

- validates the exact supported Armada/Fleet Ops PE identities and every
  renderer signature before enabling;
- uses the core's checked fixed-byte writer for the vertex-shader path;
- assembles the pixel shader through Fleet Operations' existing
  `D3DX81ab.dll`, so no MinHook or MinGW runtime DLL is added;
- routes Fleet Operations' own DOT3 `GetShaderHandle` function-pointer slot at
  first compilation, after Fleet Operations has completed early startup;
- resolves the renderer's current live DX8 wrapper after Armada creates the
  custom vertex shader, then creates its paired pixel shader on that device;
- supplies world, world-view, inverse-world, and camera-direction vertex
  constants before the custom pixel shader is selected;
- combines the active craft's configured subsystem emissive maps into one
  cached texture and binds it to the shader's second sampler;
- preserves each loose emissive source's authored RGB values for the sharp
  self-lit material centre, builds a complete mip chain, and uses trilinear
  filtering so thin lights remain stable during camera movement;
- applies the same composite to classic/non-DOT3 SODs as a scoped additive
  fixed-function texture stage on MeshVB and both observed workspace classes,
  restoring the complete preceding state after each draw;
- accumulates DOT3, MeshVB, GPU-buffer workspace, and CPU-buffer workspace
  emissive geometry into a private
  full-resolution render target, blurs a half-resolution copy horizontally
  and vertically, and screen-blends the result before native EndScene;
- releases all default-pool bloom targets before Armada resets a lost device;
- disables the pixel shader at Fleet Operations' fixed-pipeline transition,
  then resumes the displaced code and all remaining alpha draws.

That final gateway is an intentional safety change from upstream. The original
patch returned from the renderer in the middle of the function, which fixed
nebula ship rendering but skipped later alpha geometry and made editor crystal
letters black. This port preserves Fleet Operations' remainder instead.

All installed early hooks are pass-through until the complete feature has
enabled. An absent controller/shader, missing D3DX export, changed signature,
or DX9 mode therefore leaves native rendering active and logs the reason.

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
Composite textures are created lazily for only the active combinations actually
encountered by each material and are cached afterward.

The shader makes these pixels self-lit and independent of map lighting. The
runtime preserves that sharp material centre and also renders only the active
ODF emissive geometry into a private full-resolution mask. Before Armada ends
the scene, a four-tap half-resolution reduction preserves thin illuminated
geometry, two dense bilinear Gaussian iterations create the broad colour blur,
and the result is screen-blended over the completed frame. Screen blending
avoids additive white clipping without the sub-pixel instability caused by
subtracting a sharp mask. The halo is submitted three times to recover strong
sprite-like energy on DX8's fixed-point render target without requiring an HDR
or D3D9 device replacement.
This creates a genuine soft halo beyond the ship silhouette without ReShade or
an unstable D3D8-to-D3D9 renderer replacement. Because the source mask contains
only registered emissive geometry, bright UI and map objects do not bloom.

All observed render families are covered. Bump/DOT3 meshes consume the
composite in the custom pixel shader; ordinary MeshVB and classic workspace
meshes receive it after their native material setup through Direct3D 8 texture
stage 1. The workspace hook handles both `ST3D_WorkspaceDirectX8`, which keeps
the submitted vertex/index buffers selected on the device, and
`ST3D_WorkspaceDirectX8NonVB`, which submits CPU arrays. Ships such as the
classic `fbattle.sod` use the first of those two layouts even though Armada
reaches it through its `RenderInternalNonVB` path. The GPU-buffer mask is
captured at `Submit`'s exact native indexed-draw instruction, before its rolling
workspace can move on to another batch. The exact hook selects the enclosing
emissive Craft directly when a hull submission occurs outside Armada's narrower
material-pass scope; this prevents a Team-colour child group from becoming the
only captured geometry. The CPU-array layout remains captured immediately after
its UP draw. The isolated fixed-function mask keeps a neutral stage 0 and
samples the emissive map through stage 1, matching the visible emissive layer's
proven UV route on classic mirrored meshes.

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

# Refit cloak refraction prototype

This is the initial staged-build report. A subsequent activation audit found
the installation still used the prior core. The later diagnostic deployment
and exact backup are recorded in [cloak-activation-20261022.md](cloak-activation-20261022.md).

Date: 2026-10-22. Scope: an opt-in rendering prototype, built and tested in
isolation. No installed game executable, DLL, renderer choice or configuration
is changed. In-game appearance and performance remain manual acceptance.

## Implementation

`core/renderer_cloak_refraction.inl` replaces the original COM indexed draw
at the existing checked Fleet Ops DOT3 boundary. The helper consumes the
original six stdcall arguments and preserves the native HRESULT on fallback.
It runs only when `[Cloak] RefitPrototype=1`, the managed DXVK hooks and current
compositor are active, an engine-submitted craft is cloaking/cloaked/decloaking,
and its authored material is opaque. Visible hulls, authored transparent or
additive materials, M3D meshes and unsupported routes retain the existing draw.
No gameplay state, cloak timing, detection, ownership or visibility is written.

`renderer/cloak/refraction.cpp` queries the actual D3D8 object for
`IID_IDirect3DDevice9`; it never casts a guessed wrapper layout. It validates
SM3 support and the existing 68-byte position/normal/UV/tangent stream, then
draws the native indexed range through an independent shader/declaration.
Native c2..c5 clip rows and the current compositor's eight directional-light
inputs provide transforms and diffuse lighting. D3DCompile is loaded lazily
from d3dcompiler_47 or d3dcompiler_43; the core gains no mandatory compiler or
D3D9 import. Missing support retains the existing effect.

The pass copies the current viewport into a private render-target texture.
Material groups of one native mesh share the same pre-mesh copy; a new craft
render scope/mesh gets a unique snapshot key. Copy dimensions, origin, format
and target identity are part of invalidation. D3DSBT_ALL restores the underlying
device after the custom draw, keeping the wrapper's cached state consistent.
An exclusive lock serializes the mutable pass and its reset/release path;
snapshot keys are unique across threads. Reset releases default-pool objects
and permits a previously unavailable pass to retry.

A failure before submission falls back after restoring any changed state and
blocks further attempts until device reset. Once a replacement draw has been
attempted, a draw failure or failed state restoration consumes the call with
E_FAIL and disables the pass; it never submits a duplicate native draw.

The transition uses the native controller at Craft+0x108, state +0x3c and
alpha +0x44: states 1/3 map (alpha-0.2)/0.8 to [0,1], state 2 maps to zero.
State and alpha-update offsets were independently checked in the repository's
ArmadaL.exe (SHA-256
63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36).
These are static checks, not proof of every live Fleet Ops override.

Shader formulas are adapted from Cedric Haase's MIT-licensed Armada II Refit
at 37d60079169b086dba4216fe55e05cd6015d66c0. Source notice and complete licence
are retained under `third_party/armada2-refit/`. The five upstream cloak source
files were also compared with current upstream
869054f8424eaa1cc8e5591557e34db3f40c0351 and were byte-for-byte identical.

## Reproduce validation

```sh
make -j2 renderer-cloak-tests
build/renderer_cloak_policy_test
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-cloak-20261022/dispatch \
  --fixture cloak-dispatch
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-cloak-20261022/managed-images \
  --fixture cloak \
  --d3d8-provider-root '/home/tamsynn/NVMeData/Fleet Ops Roots/Data'
```

Use an existing dedicated a2fo-* prefix. The runner removes desktop display
variables and runs GPU work only beneath a private headless gamescope. It
sends no input and launches no game. The provider DLLs are read-only sources
copied into the private fixture directory. The cloak fixture must print its
final PASS marker AND exit zero; a teardown crash cannot qualify as a pass.

The policy fixture covers transition endpoints, invalid states/nonfinite
alpha, and every combination of admission flags and supported blend values.
The native dispatch fixture includes the actual core helper with inert
records/backend: native stdcall argument and HRESULT preservation, native
index-base forwarding, progression/bounds, snapshot scope, default/additive/
visible pass-through, pre-submission fallback, and consumed failed draw.

The GPU fixture uses the installed D3D8/D3D9 provider copies with a hidden,
nonactivating HWND and a synthetic hull stream over a patterned background.
It exercises the D3D9 bridge, SM3 compile/create/draw, pure-glass distortion
versus zero-distortion control, exact control RGB (one-level tolerance), a
separate visibility transition, unsupported-IUnknown/nonfinite/stride fallback,
D3D8 and D3D9 state restoration, scene reuse/refresh, release, Reset and redraw.
Four PPM readbacks retain the baseline, refracted, control and transition frames.
This is synthetic GPU evidence, not native game visibility or performance proof.

## Manual activation and acceptance

The staged DLL and receipt live in `build/renderer-cloak-20261022/package/`.
Keep a backup of the installed A2FOExtensions.dll before a future deployment.
Merge `[Cloak] RefitPrototype=1` into the existing Data/A2FORenderer.ini;
do not replace that file with the small example or change renderer selection.
Restart the game. It uses the already enabled A2FONebulaRenderer/compositor;
this prototype does not turn an inactive renderer module on automatically.
`Refit cloak prototype DRAW active` means the new draw actually executed.
`unavailable; current cloak retained` means a prerequisite failed and fallback
was used. Set RefitPrototype=0 or remove the key and restart to disable it.

Manual checks: one ship against stars and a nebula through both transitions;
overlapping friendly/detected enemy cloaked ships; unseen enemies remaining
unseen; shields/weapons/selection overlays; visible normal-mapped hulls;
inset/tactical/editor views; reload and display reset; a fleet-size FPS comparison.
The installed system renderer and experimental D3D11 backend remain outside
the prototype, and must keep their existing behavior.

## Deliberate prototype limits

- Applies to the eligible native opaque DOT3 stream, not every hull/render path.
- Bands use local Y and mesh-local bounds. Multi-mesh parts may have seams;
  Refit's world-space longest-axis wave coherence is not fully reproduced.
- Shading uses native diffuse/geometric lights, without the full Refit shading,
  chromatic splitting, shadows or additional emissive/specular texture layers.
- No owner-specific residual hull trace is added: hull floor is zero for every
  submitted craft until ownership/detection have dedicated live fixtures.
- Native inter-object submission order is retained. The pass does not build
  Refit's separate far-to-near whole-hull queue.
- The GPU fixture uses a non-MSAA target. A multisampled or unsupported-format
  copy may decline and use fallback; MSAA acceptance has not been established.
- State-block/copy overhead, shape fidelity and Windows-driver compatibility
  require measurement in the game. No game FPS or visual claim is made.

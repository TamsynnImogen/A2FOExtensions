# Native D3D11 renderer migration

Started 2026-11-02 at the user's request. This replaces the rendering subsystem
in stages; it does not replace Armada's simulation or convert legacy draw calls.
The working Nexus DXVK path and experimental D3D8-facing D3D11 translator remain
separate recovery/comparison routes. No native renderer is enabled by default.

See the [completion checklist](native11-progress.md) for current milestones and
the outstanding deployment and acceptance gates.

## Architecture

`renderer/native11/scene.hpp` defines neutral CPU meshes, BGRA image data,
material/instance transforms, camera and directional/point/spot lights. It
contains no D3D8/9 interface, FVF, shader bytecode, or render-state identifier.
The native renderer owns its D3D11 device, shaders, buffers, resources, depth
and DXGI presentation. It does not use `game::State`, `DrawPipeline`, or their
fixed-function shader translation. Each GPU resource belongs to one renderer;
all GPU calls belong to its owning render thread.

The CPU asset adapter uses the existing MIT `a2fmt` parser, accepting bytes
and returning copied mesh/image data through a C-compatible synchronous sink.
It uses neither the menu renderer's D3D9 device nor its texture/model cache.
The initial static-model policy uses authored base transforms and the highest
detail LOD, emits diffuse materials and reconstructed normals, and omits
animated effects and bump channels explicitly. Runtime LOD, animation, race
nodes, material replacement and asset-root/archive precedence need their own
compatibility work. Static preview coverage is not game rendering parity.

The game scene source must provide authoritative visibility, fog-of-war,
transforms, class/model/material choices, lighting, camera and view identity.
Its source must be checked against the exact installed engine pair. Native
frame takeover must also cover HUD/minimap/text/sprites, all particle/weapon
effects and video before the legacy draw/present path can be retired.

## Work and evidence gates

- [x] Direct D3D11 scene renderer and meaningful GPU pixel/lifetime tests.
- [x] CPU SOD/texture adapter and actual Venture/Mediterranean static-model
  previews with no D3D8/9 imports or draw translation.
- [x] Independently reviewed scene contract, source bounds, material handling,
  lighting convention, resource reset and failure behavior.
- [x] Exact-target camera conversion and bounded post-frustum capture stage,
  checked with native ABI, actual-image guard and fake-host DLL fixtures.
- [x] HWND-free offscreen D3D11 target and versioned C DLL service, with an
  owned-capture adapter, C-compiled client and private GPU failure/retry checks.
- [x] Opt-in native directional, spot and six-face point shadow maps, with
  controlled occlusion tests and explicit fullbright relighting previews.
- [ ] Live high-level scene coverage and visibility/camera acceptance, including
  class-specific paths; it must not replay simulation or reveal hidden objects.
- [ ] Opt-in main-view native scene submission and authoritative native
  presentation, with appropriate fallback before takeover succeeds.
- [ ] Complete background, terrain, stations/ships, material/animation, weapon,
  sprite/particle and all UI/movie coverage. No silently discarded draw types.
- [ ] Per-source shadows, source ODF controls, emissive/bump/specular maps,
  post-processing and repeatable visual acceptance.
- [ ] Checked on-disk import/startup migration for each supported executable
  and FleetOpsHook image, followed by removal tests with D3D8/9/D3DX providers
  genuinely unavailable. A native frame alone does not retire static imports.

## Engine integration evidence

Read-only target: installed `Data/ArmadaL.exe`, SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
Its existing main world CALL at RVA `0x0019886e` and body RVA `0x00198110`
are already guarded by the opt-in shadow integration. That wrapper currently
calls the original world renderer exactly once; it is not a native replacement.

Fresh static disassembly confirms that RVA `0x00197f30` reads the object list
global at VA `0x00761084`, accesses its sentinel at `+4`, reads objects at
node `+8`, and advances via node `+0`. Its light registration uses the object
class at object `+0x40`, class light at `+4`, object colour at `+0x18`, and
`Entity::GetTransform` at RVA `0x000cfd50`. The latter's existing checked
signature reads the instance at object `+4` and returns its matrix at `+0x44`.
RVA `0x00197fb0` skips background flag `0x10000` at object `+0x14` and the
view-culled byte at `+0x183` before invoking render vtable slot `+0x2c`.
These checks establish particular accesses and ordering, not complete fog,
object lifetime, camera or renderability semantics.

Refit's current source is a research reference. Its roadmap says Fleet Ops
has no profile; its field/address assumptions are not automatically accepted
for this engine. The local old analysis image's `63bd645d...` hash also differs
from the installed target. New camera and model-selection findings need fresh
target evidence rather than source-name inference.

The initial renderer milestone does not change executable or FleetOpsHook
files. The user has authorized the migration; later binary transformations
will be checked against exact inputs and paired with rollback. No game launch
or GUI/input automation is used for development checks.

## First native renderer milestone: verified

The renderer, CPU asset adapter and 32-bit integration host are implemented
and pass isolated tests. This is an independent native scene renderer and
static model preview; it has not replaced Nexus's running gameplay renderer.
No live core, executable, FleetOpsHook or mod setting changed in this milestone.

Build targets:

```sh
make BUILD_DIR=build/native11 native11-build native11-tests-build
make native11-assets-test
```

`renderer/native11/renderer.cpp` creates its own D3D11 FL11_0 device, DXGI
swap chain, depth target, immutable buffers and textures, and HLSL 5 shaders.
It implements per-pixel directional/point/spot lighting with colour/intensity,
start-plus-fade attenuation, cone cutoff, ambient and texture-modulated
emission. Instances have independent affine world transforms and inverse-
transpose normals; mirrored transforms retain correct face culling. Caller
order determines alpha/additive order. This first material path has diffuse
textures and generated normals, without bump/specular/emissive map channels,
HDR/tone mapping, shadows, post-processing, or a full animated scene.

Uploads share one 256 MiB budget, in addition to a separately bounded colour/
depth framebuffer allocation. Handles survive resize and are not recycled
after shutdown/reinitialization. Invalid frames are rejected before clearing
the target. Failed resize permits later retry; device removal still requires
the caller's full recreation policy. Native Windows/device-loss and real game
integration are not established by successful Wine GPU fixtures.

The parser adapter uses the vendored MIT `a2fmt` crate directly, not its
D3D9-based menu renderer. Pristine static previews hide the standard `damage`
subtree, including assimilation overlays; highest-detail LOD and authored base
transforms remain explicit. The adapter reverses SOD triangle winding and
negates the reconstructed inward normals to match the native scene contract
(clockwise screen-space fronts and outward normals). This correction was driven by inspecting the
initial renders, which exposed the Borg boxes. Initial images/logs are retained
in `build/native11/all-damage-draft/`. Twelve sprite nodes and two animation
channels in the Mediterranean remain omitted, and one Venture/three
Mediterranean bump maps are not applied. These previews are not full native
material/animation equivalence.

Verification:

- Strict MinGW i686 builds pass. CPU tests pass 1 unit and 12 integration cases,
  including allocation-free SOD table preflight, bounded mesh expansion,
  malformed-later-mesh atomicity, cycles, LOD, material and pristine selection.
- Real 32-bit Rust-to-C++ callbacks pass mesh/index/matrix and BGRA texture
  checks. This is runtime ABI evidence in addition to matching layout asserts.
- The private GPU fixture passes directional intensity, point falloff with
  nonzero start, cone cutoff, inverse-transpose normals under rotated nonuniform
  scale, textured emission, mirrored culling, an end-to-end single-sided SOD
  exterior/normal/mirror/back-face test with a two-sided visibility control,
  depth occlusion, invalid-frame
  preservation, resize and stale-handle checks.
- Both actual ships render via direct native scene data with D3D8, D3D9 and
  D3DX libraries disabled. The GPU fixture verifies those modules are absent.
  Venture: 1 pristine part, 502 triangles. Mediterranean: 3 pristine parts,
  1240 triangles. Both use their real diffuse textures.

The initial loader failure was a missing task-local Wine vkd3d support pair;
the already hash-verified support DLLs were staged beside the private fixtures.
The initial culling assertion used the back of a two-sided lighting plane;
the final fixture uses a separate front-facing mesh for culling. That generic
mesh test did not catch the SOD importer convention mismatch: the user then
identified inside-out actual-model previews. The adapter correction now has
a source-format-to-GPU regression, and both actual previews were regenerated
and inspected. The original images, source and receipt are preserved under
`build/native11/before-sod-facing-fix/`. Neither loader/cull failure was hidden
by dropping assertions or enabling legacy graphics DLLs.

Evidence: `build/native11/runtime-facing-fix.log`, `runtime-results.json`,
`assets-facing-fix-accepted.log`, `build-facing-fix.log`, `assets.log`, `renderer.log`, `venture.log`,
`mediterranean.log` and `milestone.json`. The private runner uses only its own
Xvfb display and isolated Wine prefix, with software GL; it is not a performance
benchmark or a native Windows acceptance run. Preview PNGs are
`build/native11/venture.png` and `mediterranean.png`. Proprietary model/texture
inputs and their resulting preview images remain local test artifacts.

The next integration gate is the high-level live scene source and frame
ownership: verify camera matrices and authoritative visibility for the exact
engine, then transfer presentation and UI/effects without resubmitting old
draw calls. A full dependency retirement still requires migration of the
static imports and every remaining renderer consumer.


## Scene capture stage 1: staged, headless verified

Build identity: `20261102-native11-scene-capture-stage1`. The staged core adds
`[Native11] CaptureScene=1` in inherited `A2FORenderer.ini`; the default is 0.
This option captures telemetry only. It does not select the native renderer,
present a frame, or replace the original game rendering. No live executable,
DLL or setting was changed for this stage.

`core/native11_capture.cpp` guards the exact installed engine SHA-256 and
loaded visibility/frustum bytes before requesting a checked CALL replacement
at RVA `0x000cfdbf`. That five-byte boundary follows the successful instance
frustum test and on-screen flag write in `Entity::Render`. The tap observes the
entity/camera and tail-dispatches the original virtual call once. It preserves
GPRs, flags and x87/SSE state; C++ gets an empty x87 stack and clean MXCSR before
observation. The existing main-world wrapper is shared with shadows so the
subsystems do not compete for the same CALL. Other cameras and nested ownership
cannot contaminate the main-view snapshot.

The collector owns up to 2048 observations and publishes only a validated
frame. Invalid placement, camera reads or capacity overflow discard the whole
pending frame and leave the previous publication unchanged. Source addresses
are telemetry identifiers, not durable pointers. Capture copies the matrix at
instance `+0x44` and the selected database identity at instance `+0x80`.
For a checked plain `ST3D_Database`, it copies the bounded source name at
`database+0x38`. The constructor, loader and render paths establish this name;
it is not inferred from an object's ODF. Unsupported or unreadable model
identities remain explicitly unresolved without discarding valid placement.
There is no scene/model rendering consumer yet.

The camera adapter consumes the CPU combined row-vector screen matrix at
`camera+0x160`. `ProjectPoint` establishes its pixel rule: `(width-1)*U/W` and
`(height-1)*V/W` plus the viewport origin. The adapter transposes to column
vectors and changes screen U/V into native clip X/Y, preserving depth. Native
D3D11 now supports validated viewport subrects as well as the full target.
This avoids borrowing a D3D8/9 device transform or translating its draw calls.

Evidence and limitations:

- `capture-engine-profile.json` pins ten signatures and twelve fresh static
  disassembly regions from the exact installed executable.
- Host camera tests compare pixel/depth projection against an independent
  screen-space reference across rotated/translated views, asymmetric matrices,
  subrects and one-pixel axes. Collector tests cover ownership, invalid data,
  bounds, shared databases, type guards and malformed/owned model names.
- The private 32-bit bridge test seeds seven occupied x87 registers and checks
  original-once dispatch, `ret 4`, GPRs/flags, x87/SSE state and owned observation.
  The pre-reset failure remains in `capture-before-fp-reset/`.
- The exact-image guard fixture statically maps a private copy of the pinned
  executable, calls no engine function, rejects mutations at both ends of the
  loaded frustum context, checks the CryptoAPI SHA identity, and exercises owner
  and selected-model telemetry. The actual patch primitive is mocked in this
  fixture; this is not an in-game hook installation test. The mapping uses
  [Win32's non-initializing executable load semantics](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa).
- The actual staged Rust-owned core DLL passes fake-host attach/deferred-worker,
  rejection and retry checks. All private cases keep D3D8/9/D3DX unavailable.
  This proves unsupported-host failure behavior, not playable engine behavior.

Evidence resides under `build/native11/`; `scene-capture-milestone.json` records
source/artifact hashes, exact target evidence and protected live-file hashes.
The capture stage remains local and disabled in the installed game.

Next work: carry effective instance/ODF scale, class-specific node/material/
animation/cloak state and authoritative asset bytes into native scene data;
then connect owned scene submissions to a separate native target before
attempting presentation takeover. Offscreen shadow-caster eligibility and
classes that bypass `Entity::Render` need explicit coverage. Static capture
and two ships are not complete game scene parity.

## Native content and scene submission stage 2: headless verified

The standalone native renderer now preserves authored DDS mip chains rather
than expanding every DDS to a single BGRA level. The CPU texture protocol is
API neutral and synchronously borrows bounded payloads and up to 15 mip
records. It supports raw BGRA/RGBA, BC1/2/3/4/5/7, signed BC4/5 and tagged sRGB
views. All levels, dimensions, pitches and contiguous byte extents are checked
before immutable D3D11 upload. Array, cube and volume DDS files remain rejected.
TGA and supported single-level legacy raw DDS decoding remain available;
unsupported or damaged mip chains are never silently flattened. A rejected
callback is not retried through another decoder.

DDS alpha metadata is preserved. Generic alpha blending converts associated
colour to straight colour; additive ONE/ONE retains its associated coverage.
Opaque metadata overrides the stored alpha channel. Custom alpha remains an
explicit unsupported material case. sRGB tags control GPU sampling; this stage
has not implemented a complete HDR/output-colour pipeline.

`content.hpp/.cpp` own complete CPU model/texture data through the borrowed
Rust callbacks. The reader receives the exact model/database and mesh texture
identifiers; it owns resolution. Loading is transactional, texture IDs are
deduplicated within a model, and geometry/texture payload budgets are bounded.
This service does not invent a model name from an ODF or resolve active engine
archives by assumption.

`SceneSubmission` consumes an owned capture, composes instance and SOD node
transforms, shares verified texture resources between models, and submits a
complete supported snapshot to a private native target without presenting.
Unresolved identities or failed model uploads cannot clear the prior target.
SHA-256 signatures include texture metadata and every authored mip byte;
conflicting content under one identifier requires an explicit source epoch
change. Source changes and renderer recreation invalidate the appropriate
caches. Failed assets are bounded and cached within their source epoch.

The renderer now supports explicit mesh/texture release, monotonically assigned
non-recycled handles, logical residency accounting and device epochs. Registry
limits apply to resident resources, not accumulated released slots. Release and
accounting remain usable on the device thread when resize has lost its target
views. These byte counters describe logical uploads, not measured VRAM.

Verified evidence:

- Strict i686 C++ builds pass. The asset crate passes 21 tests, including eight
  DDS/generic-texture cases. Host texture-layout, camera and collector tests pass.
- The private native texture fixture samples the authored red/green/blue mip
  levels, known BC7 mode-6 blocks, signed formats, sRGB views and alpha modes.
  It checks immutable source ownership, resize, release, stale handles and epochs.
- The private scene fixture exercises exact opaque IDs, object-times-node
  placement, shared textures, hot caches, model-upload rollback, source/device
  epochs and reentry rejection. It forces an actual depth-view creation failure
  during resize, clears the cache while target views are absent, retries resize,
  then verifies the same device has no leaked logical residency. Only the private
  fixture's COM method slot is temporarily changed, using layout from the SDK;
  no engine method or user application is patched by that fixture.
- Eleven private cases pass with D3D8/9/D3DX unavailable, including the previous
  capture/core fixtures and the new content, submission and fleet hosts.
- `native11_scene_preview.exe` runs the real Venture/Mediterranean assets through
  CPU content, a synthetic three-object owned capture and `SceneSubmission`.
  It produces `build/native11/fleet.png`: three objects, four resident mesh parts,
  two shared authored textures plus white, and 30,630 visible pixels. Placement
  and scale are deliberately synthetic fixture values, not in-game measurements.

Build with `make BUILD_DIR=build/native11 native11-build native11-tests-build
native11-texture-test native11-scene-tests`. Evidence and an immutable local
snapshot are recorded in `build/native11/content-scene-milestone.json` and
`build/native11/stage2-native-content/`.

This layer currently runs in standalone native executables. The core capture
stage and live installation are unchanged. It is not a running gameplay
replacement. Next integration requires a C-compatible helper/consumer boundary,
authoritative loose/archive asset bytes, effective scale and class state, followed
by presentation and complete world/UI/effect coverage. Existing root enumeration
provides child/parent order, but does not itself prove engine archive precedence
or expose bounded raw file bytes; those are explicit unresolved integration gates.

## Offscreen service and capture boundary stage 3

`Renderer::initialize_offscreen` uses `D3D11CreateDevice`, feature level 11_0,
with owned BGRA colour and D24S8 depth targets. It creates no HWND or swap chain.
`render(..., false)` and readback work on its owning thread; requesting present
is rejected before changing the target. Resize retains resource identities and
the device epoch. A failed target recreation leaves rendering/readback unavailable
until retry, while residency accounting and resource release remain usable.

The separately built `A2FONative11Renderer.dll` owns the renderer, CPU content
adapter and scene caches. Its single export, `A2FO_Native11_GetInterface`, supplies
the version-1 table in `sdk/include/a2fo_native11_api.h`. The boundary consists of
C POD records, signed HRESULT results, non-recycled session IDs and synchronous
byte sinks. No STL container, allocator, engine pointer or graphics interface
crosses it. The first successful create binds the service thread; foreign-thread
and reentrant calls fail. Failed first creates release tentative thread ownership.
The host must destroy every session on that thread before unloading the DLL.

The host resolves exact bounded model/texture identifiers against a stable
source epoch. The sink copies bytes before returning, rejects duplicates and
oversized payloads, and never asks the host to free helper memory. Model IDs
currently follow the capture profile's printable ASCII, 1..259-byte contract.
Scene matrices, lights, names and ambient values are copied before asset callbacks.
Missing/invalid content cannot publish a partial scene. The helper supports
pristine static geometry offscreen; the table exposes no presentation capability.

`capture_service::submit_captured` adapts the existing owned capture to the public
C table. It rejects unresolved model identities before calling the helper. A
private client collects synthetic entity/camera/database memory, destroys those
source arrays, then renders that owned capture through a dynamically loaded
helper and checks the resulting pixels. A separately compiled C client verifies
record sizes, offsets and the exported table call. Further tests cover borrowed
asset lifetime, duplicate/oversized/missing sinks, provider failure, callback
mutation, thread/reentry refusal, source epochs, hot caches, resize, stale session
IDs and clean destruction/unload. The offscreen GPU fixture forces a real depth
view creation failure and verifies cleanup plus same-epoch recovery. These are
headless fixture results; no game entry point or user input is exercised.

Stage-3 evidence is recorded in `build/native11/service-milestone.json` and the
immutable local snapshot `build/native11/stage3-offscreen-service/`. Build with
`make BUILD_DIR=build/native11 native11-build native11-tests-build`.

The installed core, game EXEs, FleetOpsHook and Nexus configuration remain
unchanged. Core telemetry is not wired to load this helper yet: authoritative
loose/archive bytes, full transform/class state and complete visibility coverage
remain integration gates. Full native gameplay presentation, world/UI/effect
coverage, per-source shadows and import/startup retirement are still unfinished.

## Instance scale capture stage 4

The pinned executable establishes an additional placement input beyond the
Matrix34 at instance+0x44. CraftInstance's constructor installs vtable 0x6b3e2c;
slot 3 reaches 0x4cb6b0, which calls ApplyScaleEffects at 0x599760 before the
frustum check and the capture tap. That updates a scale triple at +0x28 and its
active byte at +0x74. The plain database slot-0 renderer at 0x6209b0 scales the
three placement basis rows by those values, leaves translation unchanged, then
composes node transforms. In native column-vector form this scales columns of
the captured placement. The resulting matrix still composes object * node.

`Collector` now offers an optional exact-profile CraftInstance vtable. With it
enabled, only the verified craft and resolved plain-database path reads and
applies this scale. An inactive flag ignores stale scale bytes. Nonfinite, zero,
overflowing or unreadable active scale invalidates the whole frame; unknown
instance/database paths retain unsupported telemetry and cannot enter the native
submission adapter. Authored synthetic fixtures retain their explicit placement
contract when the engine-specific profile is not configured.

Before arming the production observation hook, the core verifies four loaded
code regions and three vtable slots as well as the pinned on-disk executable and
prior visibility/frustum guards. The generated scale profile normalizes known
PE HIGHLOW relocations before byte comparison; it does not ignore relocation
fields. `check_native11_capture_profile.py --check-scale-header
core/native11_capture_scale_profile.hpp` checks generation against that image.
The staged core fingerprint is `20261102-native11-instance-scale-stage4`.

The private static-map fixture initially exposed a test-harness difference:
Wine's DONT_RESOLVE inspection mapping of this EXE retained preferred-base
relocation values. The strict core correctly refused it. The corrected fixture
proves that refusal, emulates only the guarded loader fixups on private pages,
and verifies both valid relocated data and mutations of region endpoints,
relocation fields and vtable slots. It calls no engine method and mocks the hook
patch primitive. The saved rejection/diagnostic logs remain part of the evidence.

Host tests check rotated/translated nonuniform and negative scale, inactive stale
bytes, invalid/unreadable scale, unsupported classes and unchanged output on
failure. The DLL client also feeds a captured half-width scale through the public
ABI and checks its GPU footprint. Thirteen private runtime cases pass, including
the rebuilt full core's fake-host lifecycle, capture ABI and static target guards,
resource/failure fixtures and real asset previews. The core retains its 198
canonical exports. No installed executable, DLL or Nexus configuration changed.

Evidence is recorded in `build/native11/scale-milestone.json` and
`build/native11/stage4-instance-scale/`. This verifies one runtime scale path;
per-node/baked ScaleSOD, other instance classes, animation, material/LOD state,
cloaking and complete gameplay coverage still need parity work. The native helper
is not connected to a live authoritative asset reader or native presentation.

Current asset-boundary research is deliberately separate from a usable reader.
The ST3D_Database constructor at 0x620540 creates the selected model database
and copies its filename; it is not a file stream. ReadDatabase's incoming object
performs separate virtual operation-2 reads. A related manager/provider dispatch
at 0x62cf90 remains untyped, and the FOFS item callback returns an object without
a proven byte-span/cursor contract. These observations do not authorize seeking,
replaying a stream or treating an arbitrary object pointer as complete file data.
The next reader integration must establish that contract or reproduce verified
engine archive selection; directory enumeration alone is insufficient.

## Native light-dependent shadow maps and relighting stage 5

The independent native renderer now renders geometry into an R32 typeless depth
array, with D32 views per light face and an R32 shader-readable view. Directional
lights fit an orthographic projection to submitted world bounds; spotlights use
their position, direction and outer cone; point lights render six 90-degree
faces. `FrameView::shadows` explicitly selects light indices, map resolution and
normalized depth bias. Defaults remain disabled. Each depth array is limited to
64 MiB; replacement keeps the prior array until the new allocation succeeds.
Shadow targets are separate from the logical mesh/texture upload counters.

A separate frame constant buffer carries the shadow matrices. The shader applies
visibility to each selected direct-light term, preserving ambient and emissive
contributions. Opaque and cutout geometry casts shadows; generic alpha/additive
materials retain their translucent treatment. Four typed depth loads implement
bilinear PCF. This works with the private provider, which reports R32 load support
without hardware comparison filtering. Receiver-plane depth correction removes
the reproduced sloped-surface self-shadow artifact without a large bias. Its
magnitude check also avoids a compiler intrinsic unavailable in this Wine shader
compiler. Point-face selection matches the CPU planner. Cross-face PCF seam
filtering, extreme near-plane cases and complete game caster coverage remain
outside the current visual acceptance claim.

The public service retains the 40-byte version-1 table. Version 2 provides a
44-byte shadow-control prefix and a 48-byte table with optional material policy.
The getter copies exactly the requested supported size and reports matching
capabilities. Both saved client binaries continue to run against the current
DLL. `configure_shadows` stores an owned session policy; the frame still validates
light count, actual face count, projection and allocation before changing colour.

The real Venture and Mediterranean assets exposed an essential material detail:
every opaque part in these fixtures is authored as fullbright (diffuse RGB zero,
emissive RGB one). Their compatibility preview is intentionally pixel-identical
with shadow maps on or off. `StaticMaterialPolicy` therefore provides explicit
art-directed relighting. It moves a selected fraction of opaque/cutout fullbright
emission into diffuse lighting, while alpha/additive glow keeps its authored
path. The default preserves source materials. A policy change creates its new
scene owner before releasing the prior cache; subsequent submission reloads the
host's assets. This is a mod-author choice, not recovered legacy behavior.
Proper emission masks are still needed for windows/glow embedded in the same
opaque hull texture. There is no automatic mask inference.

`native11_scene_preview.exe --relight [--shadows]` exercises that choice with
2 percent retained emission. The offline preview now uses no HWND. With identical
real assets, light, synthetic camera and placements, the relit directional shadow
comparison changes 1,712 pixels, darkens 1,564 by more than three channel values,
and brightens none. See `build/native11/fleet-lighting-comparison.png` and
`fleet-relit-shadow-comparison.json`. These are renderer fixtures, not game
screenshots or evidence of a running Nexus replacement.

Recovery tests also found and fixed an asset-cache defect: attempting to submit
while resize had lost target views cached `E_UNEXPECTED` as a model failure,
preventing same-epoch recovery. `render_status` now checks strict target/device
availability before asset access. Only stable upload failures are source-cached;
transient out-of-memory/device failures can retry. Resource-count exhaustion is
reported as capacity failure. Tests force target loss, texture allocation failure
and a point array failure after two actual depth views, then verify retry and
preservation of the previous supported frame where the colour target survives.

Verification includes strict C/C++ builds, independent C layout/source review,
CPU projection/bounds tests and nineteen private runtime cases. They cover all
six point faces, light/caster movement, selected-light isolation, ambient/emission,
cutout behavior, allocation rollback, resize, material transitions, old binary
clients and real asset previews. A Wine connection reset interrupted one suite;
its log is preserved. The final suite uses a separate task prefix, copied from
the inactive test baseline with independent files, and a persistent private Wine
server. D3D8/9/D3DX remain disabled in those fixture runs. No engine entry point,
game launch or user input is exercised.

Stage-5 receipts and an immutable local snapshot are recorded in
`build/native11/native-shadows-milestone.json` and
`build/native11/stage5-native-shadows/`. Build with `make BUILD_DIR=build/native11
native11-build native11-tests-build native11-shadow-plan-test native11-scene-tests
native11-texture-test`. No installed executable, DLL or Nexus configuration changed.
The current native helper is still an offscreen service. Authoritative game asset
resolution/state, full world/caster/UI/effect coverage, native presentation,
source ODF bindings, proper material maps and startup/import retirement remain
required for the full native gameplay migration.

### Stage 6: owned bytes from the model loader

`core/native11_assets.cpp` adds a separate default-off, startup-only observer at
the pinned Roots executable's `LoadDatabase -> ReadDatabase` call (RVA `22ce4d`).
`[Native11] CaptureAssets=1` opts into it through the existing inherited settings;
only literal `0` and `1` are accepted. It does not require the world wrapper or
enable native graphics submission. No installed configuration enables it.

The bridge forwards the original thiscall once, including both arguments, then
copies the opened memory stream before its caller frees that buffer. It restores
the parser's returned registers, arithmetic/direction flags, complete x87 state
and SSE state around an observer with a clean floating-point environment.
The exact on-disk SHA, normalized loaded HIGHLOW fields, five bounded code
regions, four stream-vtable slots and checked call bytes must match before the
patch request. No file search, second parser invocation or seek is introduced.
`tools/check_native11_asset_profile.py` reproduces the profile from the pinned
image without running it.

The stream copier uses `[begin, actual_read_end)`, including short reads, and
rejects a consumed cursor outside that span. It validates the stream class before
reading its fields. Returned plain database class and copied name must match.
The immutable store has a 128 MiB payload/2,048-record budget, a 64 MiB per-model
capture limit, and monotonic serials. Every observed replacement invalidates the
old address binding before validation. Readers retain an owned shared reference
and invoke byte sinks after releasing the store lock. Disable clears bindings.

This stores the post-parser buffer and the first parser argument's raw bits; it
does not prove that scalar's effective baked geometry semantics or certify the
buffer as pristine pre-parser input. There is intentionally no production
consumer yet. Database destruction tracking, texture resolution, complete node,
animation and material state, and binding these bytes to the native service
remain separate work. A load observer alone is not a complete asset lifetime
system or live gameplay renderer.

The stage-6 candidate builds with the same 198 core exports and no static
D3D8/9/D3DX imports. Twenty-one private runtime cases pass, including the existing
renderer suite, a fake native parser with eight occupied x87 slots, and a mapped
inspection-image guard test. The latter emulates only the guarded loader fixups,
mocks patch installation, and feeds synthetic streams to the observer; it never
executes engine code. Strict host bounds/store/camera/scene checks also pass.
Receipts are in `build/native11/opened-models-milestone.json` and the immutable
`stage6-opened-models/` snapshot. No executable, hook DLL, installed extension,
Nexus setting or game asset was modified. Native Windows and in-game acceptance
remain untested.

### Stage 7: version-bound scene assets

The post-frustum collector now optionally records a model store serial at the
same point that it copies each plain database name. The core reader requires the
stored database and copied name to match. Missing versions remain telemetry;
the asset adapter refuses them. At frame end, `native11_assets::bind_frame`
resolves those exact database/serial pairs under the store lock, retains their
immutable owners, and obtains the source epoch. It never substitutes the current
version for a stale serial. Every observed model load/invalidation and disabling
the store advances the epoch; exhaustion fails closed.

`renderer/native11/asset_snapshot.*` owns a copied frame, unique model references
and an explicit list of encoded textures. It remaps the copied model names to
opaque `n11:model:<serial>` host IDs. Original filenames and raw parser scalar
metadata remain in the retained model records. Distinct databases with the same
filename therefore do not share a model cache entry. A public C asset callback
serves exact IDs with bounded synchronous sinks. No database pointer is sent to
the renderer DLL. Factories leave the prior output untouched on failure, limit
models to 64 MiB each and combined unique model/texture payloads to 128 MiB,
and permit at most 256 explicitly supplied textures. Published shared payloads
must have no mutable aliases. Callers providing textures must advance their
source epoch whenever those bytes or bindings change.

The latest completed core binding is available through the internal
`native11_capture::latest_bound` owner. Invalid/missing bindings clear that
published owner. Publication checks both enabled state and the lifecycle
generation under the same lock as disable/re-enable; a frame spanning a restart
cannot reappear later. A deterministic fixture pauses immediately before
publication and disables/re-enables on another thread. An otherwise identical
private source variant with only the lifecycle comparison removed fails the
restart assertion. The production DLL has no fixture callback seam compiled in.

The actual DLL fixture renders two different synthetic models named `same.sod`
through retained snapshots, verifies their separate red/green pixels, clears the
source store, tests cache reuse, rejects old serials and changes pixels after
an explicit epoch replacement. The mapped-image guard also exercises the actual
core collector -> serial lookup -> frame-end store binding with synthetic data;
it still mocks patch installation and executes no engine entrypoint.

`native11_scene_preview.exe --snapshot --relight --shadows` uses explicit local
fixture readers to assemble and bind the real Venture/Mediterranean assets, then
clears the store, filesystem maps and synthetic object sources before rendering.
Its RGBA output is byte-identical to the existing relit shadow preview: zero
changed pixels. This proves retained source equivalence for that static fixture,
not live texture resolution or game parity. All 23 private runtime cases pass;
the core keeps the same 198 exports and no static D3D8/9/D3DX imports. Receipts
and immutable files are in `build/native11/bound-assets-milestone.json` and
`stage7-bound-assets/`.

Live texture collection remains unimplemented. Static inspection identifies the
classic TGA pixel-loader's memory-file open at RVA `2435d1`, followed by header
parse and pixel transfer before cleanup. That is a candidate only: the existing
RGB route can own the file-stream entry and pass a temporary physical pathname,
and this call does not establish Fleet Ops DDS/FPQ coverage or variant/material
association. See `build/native11/texture-loader-boundary-research.txt`. No guessed
path fallback has been added to the live adapter. Complete model lifetime,
effective baked/node/animation state, native world/UI/effects/presentation and
startup retirement remain required. No installed executable, DLL, configuration
or game asset changed in this stage.

### Stage 8: DDS resolver observations

`core/native11_textures.*` adds a separate default-off `[Native11]
CaptureTextures=1` setting. It observes the two existing Fleet Ops DDS callbacks'
calls to `GetRealFilename`: parameter-reading RVA `1efec0`, pixel-loading RVA
`1f0152`, original resolver RVA `109944`. These are checked call replacements,
not detours into the previously disabled RGB callback prototypes. The resolver
still runs once with its Delphi EAX/EDX/ECX register arguments and no stack
arguments. Both bridges preserve its resulting GPRs, flags, x87 and SSE state.

The exact installed FleetOpsHook image SHA `3fd18eee…932e4c11`, three loaded code
regions normalized for HIGHLOW relocation, and each call's ownership must match.
The checkout's differently hashed FleetOpsHook is not admitted. A partial
installation stays disabled with a seeded native continuation; a retry validates
the already-owned first call before installing the second. No installed image
or import table is changed by this staged work. The private fixture writes CALLs
only into its non-executed inspection mapping.
When enabled, the checked patches change two CALLs in loaded FleetOpsHook
memory. On-disk game files remain intact.

After the original resolver returns, the observer copies its managed result
while the caller still owns that string. It also copies the texture's native
logical name, flags and colour-key state, checks the requested DDS name against
the caller's verified construction, and rejects the asynchronous route. It
opens the exact resolved filename with Win32, denies concurrent writes through
that handle, and captures a bounded file with DDS magic. This is an independent
file version after resolution, not proof that a later D3DX read sees the same
version. No image decoder, GPU API or legacy parser is called by the observer.

The externally synchronized `ResolvedTextureStore` retains immutable bytes and
original logical/path metadata. It retires an address before replacement,
limits individual sources to 64 MiB and resident encoded payloads to 128 MiB,
and issues monotonic serials/generations. Exact duplicate logical names may
coalesce only when bytes and relevant metadata agree; conflicts fail closed.
Static-preview collection refuses colour-key and surface textures and more
than 256 unique names. There is no case folding, suffix guessing or basename
fallback. Missing texture lookup still fails at the native content reader.

Pending tickets prevent older or pre-disable reads from publishing after a
newer load or restart. A separate 128 MiB aggregate in-flight reservation limits
concurrent file vectors before allocation; it is released after an aborted
vector is destroyed. Resident payload accounting and retained-reader lifetimes
are separate from this reservation and from process/GPU memory. The fixture
uses the same admission policy with a 256-byte limit: two 128-byte reads are
held, a third is refused, then capacity is recovered after release. Production
builds contain neither that reduced limit nor the fixture callback seam.

The core frame binder now collects immutable DDS snapshots before taking its
model-store lock and advances the source epoch when texture generation changes.
This connects captured texture bytes to the existing static-preview host
callback. It is intentionally conservative: an unrelated unsupported or
ambiguous stored texture can refuse the whole binding. Per-model dependency
selection, variant/material association and complete lifetime tracking remain
required before native gameplay rendering is enabled.

Verification includes strict host/store and i686 builds, independent source and
profile review, both assembly bridges with eight occupied x87 slots, profile
mutation and partial-install/retry cases, overlapping reads, restart and capacity
interleavings. A private negative source variant without ticket admission fails
the restart assertion. A valid DDS observed through the actual C++ producer is
retained in a bound model snapshot, its source stores are cleared and file
deleted, then the real renderer DLL produces the expected red pixel through its
public C API. D3D8/9/D3DX imports stay disabled in that fixture. All 25 private
runtime cases pass. The core retains 198 exports and no static D3D8/9/D3DX
imports; the renderer helper's public ABI remains unchanged.

Receipts and immutable artifacts are in `build/native11/resolved-dds-milestone.json`
and `stage8-resolved-dds/`. Native engine entrypoints and the game were not run.
There is still no TGA stream capture, automatic live material/variant mapping,
complete destruction/reload coverage, native world/UI/effect presentation, or
startup retirement. Working installation hashes remain unchanged.

### Stage 9: opened TGA streams and progress tracking

`CaptureTextures=1` now also admits a checked classic-TGA loader observation at
Armada RVA `2435d1`. Its thiscall OpenRead still runs once and returns with the
original `ret8` contract. The bridge forwards its result, stream, requested
pathname, buffered flag, texture in ESI and level in EBX to a bounded observer;
the native GPR/flags/x87/SSE state is restored afterward. Only level zero is
captured. Later levels neither replace nor retire that base image.

The pinned executable hash, three loaded code regions with normalized HIGHLOW
fields, four stream slots and call ownership must agree. OpenRead itself may
retain its exact original entry or the built-in RGB route. The latter is
admitted only when the built-in's own record, six-byte JMP/NOP entry, original
six-byte gateway prefix and back-jump all match. Ownership is checked again at
capture. No competing OpenRead entry hook is installed. Inline hook installation
and observer admission remain startup operations, not a runtime hot-reload API.

Before TGA header parsing, the observer copies the stream's actual `[begin,end)`
span, with its original class and initial cursor checked before use. It does not
read `stream+0x0c`, whose redirected physical pathname can already be expired.
`TextureSourceKind::opened_stream` retains the native logical name and requested
path; its physical `resolved_path` remains empty. DDS resolved-file provenance
is preserved separately. Both paths share source/store/in-flight budgets and
immutable snapshot ownership. A failed base read retires that texture's prior
binding; unknown stream classes, modes, bounds and foreign entry owners are
refused. This covers the observed base stream, not a complete mip/animation or
material-selection model.

TGA configuration removes only stream records. Disabled or failed TGA admission
preserves existing DDS owners. A separate generation rejects streams begun
before TGA reconfiguration without erasing DDS pending reads. Shared callback
metadata uses atomic pointers. A deterministic thread fixture pauses a stream
before commit, reconfigures TGA capture, and verifies rejection; a private
variant accepting any nonzero generation fails that assertion.

The private TGA fixture uses a non-executed, relocated inspection mapping and
mocked CALL installation. It verifies profile mutations, an expired physical
pathname sentinel, exact owned bytes, failure/retry, later-level suppression and
known/foreign RGB gateway handling. Its captured valid TGA reaches the real
native DLL through a retained model snapshot after original buffers and stores
are removed, producing the expected red pixel without D3D8/9/D3DX. Existing DDS
fixtures additionally verify source-isolated TGA admission. All 27 private
runtime cases pass, alongside strict host and i686 builds. The core retains its
198 exports; no installed game file changed.

The maintained [completion checklist](native11-progress.md), linked from the
README, separates bounded headless evidence from native Windows, deployment and
in-game acceptance. The latest receipts are
`build/native11/opened-tga-milestone.json` and `stage9-opened-tga/`. Per-model
dependencies/variants, complete lifetime/animation/visibility, lighting ODF
bindings, world/UI/effects/presentation and independent startup retirement
remain open. No game or engine entrypoint was run in this stage.

### Stage 10: static per-model texture dependencies

Frame binding now inspects the exact immutable model versions recorded by scene
capture and selects only their required texture identifiers. An unrelated
unsupported, ambiguous or excessive texture registry no longer refuses the whole
frame. Missing required names, required colour keys or unsupported surfaces, and
conflicting required records still fail atomically. Untextured parts use the
renderer-owned white texture and need no source texture record.

The pristine static SOD policy is shared by the core dependency inspector and
the renderer asset adapter through `rust/a2fo_native11_model`. This crate uses
`no_std` plus `alloc`; the extension retains its existing CRT allocator and panic
policy. The helper retains its separate standard Rust runtime and C ABI. The
private core parser is not a new DLL export. Vertex assignment still follows
first-seen corners, even though lookup now uses an allocation-compatible tree.

A bounded cache keys successful manifests by immutable model owner, serial and
parser identity, retaining weak model owners. Model snapshots are pinned under
the producer lock; parsing happens outside both producer locks, with a separate
cache lock. Required texture bytes are then retained, and model versions are
revalidated before sealing the bound capture. This is static authored dependency
selection, not authoritative live material or animation state. Shared logical
names with conflicting variants remain refused until that association exists.

Strict host tests and 330 locked Rust tests pass across the core (309), shared
model policy (1) and helper asset adapter (20). The private i686 fixture calls
the real core Rust parser, verifies hot-cache reuse, ignores 274 unrelated
texture records, and renders red/green models with the same filename after
producer stores are cleared. It asserts exact Venture and Mediterranean
dependencies and complete module enumeration without D3D8/9/D3DX loaded. All
28 private runtime cases pass, including the staged core load and older service
ABI clients. Seven previews rendered by the frozen stage 9 executables and the
new shared policy are byte-identical. The extension keeps its canonical 198
exports and identical import set.

The checkpoint is recorded in
`build/native11/model-dependencies-milestone.json` and
`stage10-model-dependencies/`. It remains staged only; installed game images,
hook and configuration are unchanged. Native Windows and live provider/material
acceptance, destruction/animation/visibility, physical ODF light bindings,
world/UI/effects/presentation and independent startup remain open in the
[completion checklist](native11-progress.md). No game or engine entrypoint was
run, and no input devices were automated.

### Stage 11: scoped diffuse texture binding service

The service now accepts explicit texture selections scoped to a material-view
model identity and the zero-based emitted part of the shared static SOD policy.
One authored texture name can select different source IDs on different parts.
Two model IDs can serve identical SOD bytes while selecting different textures.
The host must use distinct model-view IDs for different selections and advance
the source epoch when those selections or bytes change. Existing model IDs are
content identities, not a reliable substitute for live instance identity.

Version 3 adds the 52-byte `A2FO_N11_API3` table and a checked texture-binding
configuration call. Its callback receives the copied model ID, part ordinal and
authored texture ID, and returns an exact printable texture ID through one
synchronous copying sink. The service then uses its existing bounded texture
reader. Missing/invalid selection never falls back to an authored texture.
Untextured parts retain implicit white. Configuration creates a replacement
scene before committing; identical configuration preserves the hot cache.
Changing either texture bindings or art-directed relighting preserves the other
policy and configured shadows. The earlier 40/44/48-byte service tables remain
queryable with their original capability sets and call signatures.

Strict C/i686 checks and private DLL fixtures cover per-part resolved-ID
deduplication, later-part failure, resolver exceptions, malformed IDs, temporary
callback storage, reentry, foreign-thread refusal, source-epoch refresh and
authored-mode reset. The new DLL client renders identical SOD bytes as separate
red and green views, preserves prior pixels across seven callback contract
failures, and checks retained-emission pixels through binding enable/reset.
All 29 private runtime cases pass, including frozen older clients. Existing
static ship/fleet preview pixels remain unchanged. The helper keeps its single
public export and no D3D8/9/D3DX imports. The staged core DLL remains the exact
stage 10 image; installed executables, hook, DLLs and configuration are unchanged.

Source inspection explains why effective selections are a separate capture
step. The current post-frustum observer runs before the craft render route.
`A2FOTextureVariants` observes the `CraftInstance` at its native Borg texture
selection call, preserves that native selection, then applies faction diffuse
and node visibility changes before rendering. Shared SOD state can therefore
belong to another craft at the earlier observation point. No suffix, ownership
or live mesh association is guessed by the new service. The part ordinal is a
static parser emission index, not an engine node index. A future checked
render-time observer must establish that mapping and pin exact texture versions
with instance/lifecycle identity. The existing module's source offsets are
research leads, not newly validated native profiles in this stage.

Receipts and immutable artifacts are in
`build/native11/texture-bindings-milestone.json` and
`stage11-texture-bindings/`. Live material provenance, colour keys, dynamic
colour, effective node visibility/transforms and animation remain open. No game,
native engine entrypoint or GUI input automation was used for this checkpoint.

### Stage 12: checked per-instance render-time diffuse capture

The core now has a default-off `[Native11] CaptureMaterials=1` observer for the
pinned Roots engine. It requires active scene, opened-model and DDS or TGA
producers. It is initialized through the existing inherited renderer settings;
the independent startup and presentation work remains open. Main-wrapper
failure also disables material observation. No standalone module or public core
export is added.

The selected engine image is SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
The checked CALL at RVA `0x0d5a11` dispatches to `0x22e780` inside
`CraftInstance::Render`, after Borg/faction selection and native damage-node
flags, within any existing scoped subsystem overrides. ECX holds the instance
and the stack holds its camera argument. The bridge saves GPRs, flags and
x87/SSE state, initializes the observer's FP environment, restores the native
state, then tail dispatches the original `ret4` function exactly once. It does
not replay simulation, invoke extra engine functions or replace native draws.
Seven loaded code regions, three virtual slots, the exact image and CALL
ownership are checked; foreign owners are refused. The generated profile and
reviewed disassembly remain in `build/native11/material-engine-profile.json`.
Class and method labels follow the repository's inferred names; the direct
evidence is the pinned byte ranges, field access and call flow.

The shared SOD policy can now report each emitted part's original node name,
node index and lighting-group ordinal through a private detailed callback.
The original part ABI and rendering policy remain unchanged. Empty groups keep
their original ordinal without producing a part. One SOD mesh has one authored
diffuse texture; its lighting groups all use native texture slot zero. Group
ordinal is never used as a texture-slot index, and a bump map in slot one cannot
be substituted for diffuse.

At the selected call, the observer correlates instance and camera with the
active visible-object capture, pins that exact model version, and matches
authored mesh names against the runtime database's bounded node graph. Duplicate
names, aliases/cycles, unreadable data, unknown mesh types and missing versions
fail closed. Slot zero is read through the validated mesh layout, without
calling `GetTexture`. Its runtime name, flags and colour-key state must match
the producer's immutable texture owner, which is checked again before the
observation commits. Unsupported colour keys and nonzero source flags are still
refused. The current profile admits the canonical mesh type; it does not guess
other node or derived-mesh layouts.

Each instance retains its observation-time texture versions. A later ship's
change to shared mesh state, or a texture producer replacement before sealing,
cannot rewrite an earlier ship's selection. Model replacement invalidates the
batch because the emitted-part mapping changed. Frame/camera/thread/lifecycle
gates reject missing, duplicate, foreign-camera and obsolete events. Accumulation
and sealing enforce 8,192 parts, 256 unique texture owners and 128 MiB of unique
model/texture payload per batch; these are logical capture budgets, not process
RAM limits. A separate weak-owner source cache is bounded to 8 MiB of metadata
including fixed part/entry sizes. It retains no model payload owner.

The resulting `BoundMaterialCapture` creates per-instance opaque model-view and
texture IDs. Its API3 callbacks serve only retained bytes and exact part
bindings after source buffers, stores and the capture system have been removed.
Known hidden/disabled selected mesh flags refuse pristine submission. Ancestor
visibility, effective transforms, animation, LOD state, dynamic colour and
material changes beyond diffuse remain unapproved. Fresh source epochs are
currently assigned to sealed batches; stable material-view reuse and production
capture cost remain separate completion gates.

Strict host and i686 builds, 331 locked Rust tests, independent source/ABI review
and all 31 private runtime cases pass. The new bridge fixture exercises eight
occupied x87 slots, all XMM registers, flags, arguments, return value and stack
balance. The guard fixture mutates profile bytes/relocations/slots and CALL
ownership, tests failed install/retry, and manually injects events into the real
core C++ model/TGA/material observers. Two instances sharing one SOD retain red
and green selections through texture replacement and producer shutdown, and the
real native renderer DLL produces those pixels through API3. Missing/duplicate
events, wrong camera, model replacement and disable/rearm before publication
are refused. Seven existing ship/fleet previews remain byte-identical. Core
exports remain the canonical 198 and import sets remain unchanged.

This is static-profile and isolated event/bridge evidence, not native engine or
in-game acceptance. Artifacts are frozen in
`build/native11/render-materials-milestone.json` and
`stage12-render-materials/`. Installed game binaries, hook, DLLs and configuration
are unchanged. No game or native engine entrypoint was executed, and no input
devices were automated. The [checklist](native11-progress.md) keeps live material,
node/animation, lighting ODF, full world/UI/effects/presentation, independent
startup, performance and release acceptance work open.


### Stage 13: post-animation rigid node state and visibility

The default-off `CaptureNodes=1` path requires active material capture. Two
checked five-byte instruction prefixes at pinned Roots RVAs `22e89d` and
`22e958` observe the database immediately after native animation evaluation.
Both bridges preserve GPRs and full x87/SSE state, then replay the original
`add esi,44h` and vtable load, including the ADD flags. They do not advance
animation, invoke engine entrypoints, or replace native drawing. Seven checked
code regions and three slots establish the node layout; source image identity
remains `4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.

The owned reader composes parent and node-local Matrix34 transforms, applies
the node scalar only to the basis, and keeps the captured instance transform
separate. First child is `+1c`, next sibling `+20`. Flag bit zero hides only
the node's own geometry; bit one prunes its children. Parent consistency,
cycles/aliases, finite transforms, visible unsupported geometry and exact
source-name mappings are checked. A changed instance placement between scene
and node capture refuses the batch. Requesting node capture suppresses the
earlier authored-state path even if installation fails or is partially armed.

API4 appends a 56-byte service table with a per-emitted-part state resolver.
The callback runs on every submission, including cached and untextured parts.
It replaces the authored local-to-model transform and controls visibility.
Hidden parts remain in ordinal/budget accounting but emit no draw or shadow.
Pose-only changes retain service GPU assets within an unchanged source epoch.
The required-state frame flag refuses missing resolvers before asset reads;
older capture adapters refuse such frames. Older 40-, 44- and 52-byte binary
clients continue to run. No public core export or existing SDK record grows.

Strict host/i686 builds and 35 private runtime cases verify the new contract.
Fixtures exercise both actual bridge bodies, manually inject post-animation
node events into the core producers, and render retained poses through the
real API4 DLL after source memory is poisoned and the inspection image is
unloaded. Malformed state callbacks preserve the prior image and recover
without reloading assets. Seven static ship/fleet preview outputs remain
byte-identical to stage 12. The canonical 198 core exports and imports remain
unchanged. Independent review confirmed the requested-but-unavailable capture
gate and no remaining source defect within its stated scope.

This is rigid-node source/build/headless evidence, not native engine or
in-game acceptance. Morphs, other mesh layouts, full LOD coverage, live
provider/material acceptance and production performance remain open. Stage 13
artifacts are frozen in `build/native11/node-states-milestone.json` and
`stage13-node-states/`; protected installed files are unchanged.


### Stage 14: stable owned material streams and resource epochs

`MaterialFrameStream` retains a single owned current capture and provides stable
SDK callback contexts. A new frame keeps its source epoch when each instance's
entity/database/model owner, authored part order, exact texture owners and
required-state mode are unchanged. Camera, object/world and node poses,
visibility and object enumeration order are frame state; they do not invalidate
static GPU resources. View IDs use epoch plus instance identity instead of the
object's index. Replacement with a different owner forces a new epoch even if
the serial or bytes match. No model/texture pointer is dereferenced as engine
memory or passed through the public renderer ABI.

Updates are transactional. A failed build retains the previous frame and epoch.
Caller-supplied fresh epochs increase monotonically, including across clear;
resource changes never reuse an obsolete epoch. Clear releases the stream's
current owner, while separately retained immutable captures keep their own
lifetime. Synchronous sink reentry cannot update or clear a stream. External
serialization remains required. The core runs its stream under the existing
material lock and drops it on disable/reinitialization; publication still checks
the lifecycle ticket and exact model owner. No new native hook is introduced.

The API4 GPU client configures its host once and verifies moved, hidden and
reordered two-instance frames with no additional asset reads and unchanged GPU
usage. Texture and model replacement advance the epoch and produce the new
pixels; failed updates preserve the prior capture. Weak-owner checks verify
release after clear, followed by monotonic fresh-frame recovery. Strict host and
i686 builds and all 36 private runtime cases pass; seven preview outputs remain
byte-identical to stage 13. Source review found no confirmed defect in resource
identity, transactionality, lifecycle or bounded retention.

This is bounded reuse support, not accepted production performance. A palette
change still invalidates all resources in that epoch, and a changed palette may
require two bounded snapshot builds. The core publishes an immutable capture;
a production service consumer must use a stable stream/context and serialize
its submission. Capture/provider acceptance, live game rendering and native
Windows validation remain open. Artifacts are frozen in
`build/native11/material-stream-milestone.json` and `stage14-material-stream/`.
Protected installed binaries, hooks and configuration remain unchanged.


### Stage 15: copied ODF lighting and owned world-light submission

The native11 source path now implements the ten proposed light commands in a
bounded, transactional configuration parser and an adapter for the existing
`A2FO_GameObjectClassLoadedEvent` copied ODF fields. The adapter retains no event
strings or ParameterDB pointers. Missing or zero `lightSource` creates no
extension source; ordinary FO lighting is not inferred from an emissive texture
or a class name. Enabled sources require type and colour, accept separate
nonnegative intensity, normalize directional/spot ray direction, validate local
falloff, spotlight full angles and the shadow opt-in. Unknown types, duplicates,
malformed/nonfinite values and overflow refuse the configuration. Defaults and
units are extension policy; matching native FO attenuation remains unapproved.

`assemble_world_lighting` accepts owned source IDs, local configurations and
local-to-world poses. Local origins supply point/spot positions; the linear
basis rotates ray direction and normalization removes its length. Translation
does not affect global directional sources. Falloff remains world-distance
policy and cone angles remain full degrees. Ambient is explicit caller input.
A complete world-source list is required; a post-frustum ship list cannot stand
in for all lights. Up to 4,096 source records and eight active lights are
admitted, sorted by unique ID; overload refuses rather than silently dropping
lights. Disabled and zero-energy sources emit no light. Each active source's
shadow setting maps to its exact sorted light index. All enabled sources are
validated before zero-energy removal. No engine light object is created.

`submit_lit_capture` configures that mask and forwards the same owned lights
through the existing API4 capture adapter for illumination and shadow
projection. Upfront API capabilities, frame identity and finite/affine placement
checks precede shadow configuration. Malformed prepared lighting is rejected.
The adapter preserves rendered pixels on failure; shadow configuration remains
independent session state, so a later asset/pose/GPU failure may leave the newly
configured settings. It does not promise rollback of session configuration.

Three strict host tests exercise configuration, copied-event ownership,
transforms, bounds, invalid values and transactional output. The real API4 DLL
fixture follows copied ODF data through world assembly and native submission,
then poisons the original strings. Controlled receiver/caster pixels verify
opposite directional shadows, source colour and intensity, zero-energy ambient,
point movement/range, spotlight rotation/cone cutoff and a red shadow that
preserves an unshadowed green contribution. Light-only changes keep the content
cache hot. Malformed API capabilities and unresolved frames call no shadow
configuration callback. All 37 private runtime cases pass, seven established
preview outputs remain identical and the core retains its canonical 198 exports
and unchanged import list. Artifacts are frozen in
`build/native11/world-lights-milestone.json` and `stage15-world-lights/`.

This checkpoint supplies the owned ODF-to-renderer path. It does not register a
new installed module, auto-discover native game lights, associate live classes
with all world objects, obtain native ambient, or enable ODF commands in Nexus.
Those need checked live source/pose/lifetime profiles, complete caster capture
including offscreen blockers, and prevention of duplicate native illumination.
Native Windows, in-game comparison, full world/UI/effects/presentation and
independent DX11 startup remain open. No installed file was changed, no game or
native engine entrypoint was run, and no input devices were automated.


### Stage 16: opt-in class-light producer and immutable class lifetime

`CaptureWorldLights=1` now requests the ten light fields from the existing
inherited class-ODF dispatcher during startup, before module registration.
The built-in receiver runs independently of external class handlers. Default
zero adds no class hook request or field requests. Dispatcher signature failure
or malformed settings leave requested lighting unavailable; they cannot activate
an empty or guessed source population. No installed setting was changed.

A bounded immutable `LightClassStore` owns each class's copied ODF name, parsed
configuration and monotonic version. Replaced/cleared records remain readable
only through independently held owners. A malformed reload faults its current
identity and blocks complete-world lookup until repaired; unidentifiable,
allocation and capacity faults require explicit clear/restart. The core serializes
lookup/update/clear and checks the whole store's health. Limits are 4,096 records
and 2 MiB of logical key/entry/record/text storage, excluding allocator overhead.
Only light fields enter the parser: other modules' large snapshots cannot
accidentally trigger its 128-field bound. Logger callbacks run outside the lock.

Strict host/core/i686 builds, source review and all 38 private runtime cases
pass. The controlled Win32 producer fixture includes 160 unrelated large fields,
poisoned copied input, malformed reload/recovery, disabled classes, immutable old
owners, monotonic restart and concurrent pinned reads. Canonical core exports
remain 198 and imports unchanged. Evidence is frozen in
`build/native11/class-lights-milestone.json` and `stage16-class-lights/`.
This establishes copied configuration/lifetime handling; complete world
sampling, live engine dispatch and in-game lighting remain separate gates.


### Stage 17: pinned complete world-list light capture

A checked read-only producer now samples the Roots global world list instead of
the post-frustum camera list. Five exact-image code regions establish global
RVA `361084`, its pointed container's `+4` sentinel, doubly linked node
next/previous/object fields, `GameObject+0x40` class identity and generic
`Entity+4` instance / `Instance+0x44` Matrix34. Constructor and unlink bodies
supply list layout/lifetime evidence. HIGHLOW normalization supports a loaded
rebased image; a foreign byte or unreviewed executable refuses admission. No
new hook, native getter, simulation update or draw is executed by the sampler.

Every listed class must have a healthy copied configuration, even if it emits
no light. Enabled sources receive the generic entity pose, independently of
camera visibility and mesh-specific render scaling. Disabled classes need no
instance pose. The reader does not guess fog-of-war or expiry semantics from
raw flags. It checks bounded pointer arithmetic, 4,096 records, list closure,
cycles, duplicates and finite invertible poses, then re-reads the entire list,
poses and immutable class owners before publishing. Caller/engine serialization
during the pass remains required; double reading does not replace a world lock.
The core gates its sampler to the initialization thread and lifecycle, and
retains camera/frame/class-generation metadata with the owned snapshot. Ambient
remains explicit configured input, not a recovered native global.

Strict host/core/i686 builds and all 39 private runtime cases pass. The new
controlled sampler maps the pinned executable without entrypoint execution,
corrects only profiled preferred-base operands when Wine's inspection mapping
leaves them unrelocated, then reads a synthetic world list through the actual
core producer. It exercises profile-byte mutation, moved/removed sources,
unknown classes, broken links, class reload faults and disable/rearm. A decoy
at decimal offset 40 proves the class field is hexadecimal `0x40`. Retained
snapshots survive source poisoning. This is static-profile and injected-memory
evidence; live world traversal, source semantics and in-game acceptance remain
unapproved. Frozen artifacts are `build/native11/world-capture-milestone.json`
and `stage17-world-capture/`. Protected installed files are unchanged.


### Stage 18: API5 and separate shadow-only geometry

The new optional `a2fo_native11_world_api.h` appends a 60-byte API5 table and
`submit_world`; existing API1–4 tables and 136-byte frame / 76-byte object
records keep their layouts. Only the new submit function accepts the explicit
complete-caster frame assertion and shadow-only object role. Legacy SDK calls
reject those flags, and older C++ adapters reject owned world claims instead
of silently dropping them. C11 static layout/query checks and a frozen 56-byte
API4 binary client verify the appended contract alongside older clients.

Shadow-only geometry consumes the same object/part budgets and assets, resolves
current per-part visibility, enters light bounds and depth, and skips camera
colour and depth. Changing the role is frame state and reuses uploaded assets.
Hidden parts cast no shadow. Alpha/additive and cutout casting policies are
unchanged. The service requires the host's completeness assertion; it cannot
prove that all native offscreen blockers were supplied.

The real DLL fixture renders a blue blocker as primary geometry, then changes
its role so the receiver becomes visible while the directional shadow remains.
Disabling the source mask or hiding the blocker clears that footprint without
new asset reads. Invalid world/legacy admissions preserve the previous image,
and counted internal adapter stubs prove world claims never reach a legacy
submit or shadow configuration call. Strict builds, independent source review,
all 41 private runtime cases and unchanged seven-preview hashes pass. Core
exports/imports remain unchanged; the helper retains one query export. Artifacts
are `build/native11/world-casters-milestone.json` and `stage18-world-casters/`.
No native complete-caster producer, in-game shadow acceptance or presentation
takeover is claimed, and protected installations remain unchanged.


### Stage 19: owned-frame render session and lifecycle

`WorldRenderSession` owns an API5 session around immutable material and lighting
packets. It is noncopyable/nonmovable, configures stable callback contexts once,
and requires owner-thread, nonreentrant operation. Packets must assert complete
casters, contain observed per-part states, tie light and geometry camera/frame
generations exactly, and advance the last accepted frame generation. Equal
source epochs must retain identical resource owners and bindings; a caller
cannot reuse a view ID with changed payloads and receive stale cached pixels.
Rejected packets leave the accepted owner/generation intact and may retry that
generation. There is no authored-pose fallback in this live-world coordinator.

Callbacks retain the pending packet only during synchronous submission. A
successful submission replaces the accepted owner; failure releases the pending
one. Resize/readback expose the service's existing target/recovery contract.
Explicit owner-thread close must succeed before unloading the helper. Destructors
never call driver APIs under an unknown thread or loader lock; callers retain
that lifetime responsibility. Reentrant close is refused even during create
before the service ID has been written.

Independent review found and closed three admission gaps: unobserved pose,
changed owners within one epoch, and close reentry during creation. The actual
API5 DLL fixture covers those cases, foreign-thread calls, accepted-frame
replacement and weak-owner release, same-epoch pose changes, malformed-packet
retry, resize and explicit close. Strict builds and all 42 private runtime cases
pass; seven preview hashes, 198 core exports and core imports are unchanged.
Frozen evidence: `build/native11/world-session-milestone.json` and
`stage19-world-session/`. This is an offscreen owned session, not game frame
coverage, native presentation, native Windows or in-game acceptance.

### Stage 20: core offscreen pipeline, completeness and publication gates

The existing main-view wrapper now seals owned material and world-light
snapshots together at view end. The CPU owners publish under the capture lock
and feed the default-off `OffscreenWorld` controller. `WorldAmbient` supplies
explicit finite nonnegative RGB; it is not recovered native ambient.
`CaptureWorldLights=1` arms the exact-profiled sampler through renderer startup.
Error cleanup disables producers, and explicit renderer shutdown closes the
owned session before helper unload on its owner outside loader lock.

A checked geometry provider must assert complete caster and visible-geometry
coverage. Admission checks camera/frame/class versions, observed poses, exact
visible-instance coverage, clip/viewport, model and texture owners, instance
world transforms and part states. Extra primary geometry is refused. Source
tickets captured at main-view begin are revoked on capture arm/disable and
checked after geometry assembly and after GPU submit. Class versions are also
checked before accepting the frame.

The built-in post-frustum collector is **not** a complete caster provider. None
is registered by default: this reports `waiting_geometry`, loads no GPU helper,
and cannot produce a partial world image or replace presentation. A future
provider must establish offscreen pose, visibility and coverage; these claims
are not inferred from a filename, successful list walk or empty source set.
API5 loads lazily from extension roots after admission. Targets resize only
when dimensions change and are bounded to 4096 pixels per axis. Rendering is
offscreen; legacy drawing and presentation remain in control.

Readback checks published source tickets and class versions before and after
GPU copying. Drawn but revoked frames cannot expose their pixels. Metadata
distinguishes the attempted frame from the last published frame. Earlier valid
pixels may remain inspectable after a metadata/provider rejection; source or
class revocation invalidates readback. Payloads stay owned, and provider contexts
must outlive registration and successful owner-thread shutdown.

The private-root fixture copies the helper and writes only its temporary INI.
It checks invalid configuration, missing/wrong geometry without helper loading,
provider reentry, expired tickets, actual shadow pixels, opposite light direction,
stale classes, shutdown and private-file removal. A test-only callback revokes
sources after actual GPU submit but before publication. The call fails, readback
is refused without changing caller output, and a fresh frame recovers. No
engine entrypoint executes.

Strict host/core/i686 builds, source/lifecycle review and all 43 private runtime
cases pass. Seven static previews remain identical. Core exports remain 198,
core imports are unchanged and the helper keeps one query export. Evidence is
`build/native11/world-pipeline-milestone.json` and `stage20-world-pipeline/`.
All seven protected installed files remain unchanged. Native engine/Windows
acceptance, complete world casters and visibility, remaining world draw types,
HUD/effects/video/menus, presentation, independent startup and release acceptance
remain open. No game, presentation takeover or DX11 startup retirement is claimed.


### Stage 21: owned GameObject membership and coverage audit

The exact-profile world list now retains entity, instance, class and class
version for every member, including disabled light classes. Both passes check
instance identity; zero instances stay explicit instead of being assumed
noncasters. The pure coverage audit correlates owned, posed material captures
with that membership, reports missing/offscreen objects, and rejects stale,
extra, duplicate or conflicting observations. Consistent visible/candidate
overlap prefers the visible role; candidate roles are advisory until assembly.
An owned export API reconstructs original model names and exact retained
model/texture/part-state observations without reading engine memory.

GameObject availability is not complete world coverage: GameFeature, background
and effects domains are separate. No unseen object is omitted as a presumed
noncaster. Bounds are 4096 members, 2048 cumulative input geometry entries and
16 candidate captures. Source review, strict host/i686/core builds and 44
private runtime cases pass, including disabled-instance mutation and membership
output tests. Seven preview hashes and canonical PE contracts are preserved.
Evidence: `build/native11/world-coverage-milestone.json`, `stage21-world-coverage/`.
No installation or native engine/in-game acceptance changed.


### Stage 22: owned observed-world assembly

The assembler combines current visible and candidate geometry only after the
GameObject membership audit passes. It rebuilds immutable source observations,
keeps visible instances primary and forces candidate-only instances to
shadow-only. Exact owner collisions and stale or missing entries refuse the
batch. MaterialFrameStream preserves resource epochs across pose changes and
keeps its prior owner on failure.

Other caster domains must be certified ABSENT: GameFeatures, background casters
and effect casters have no representation in this assembler. A true flag cannot
mean present-but-fully-captured. Defaults remain uncertified and block admission.
This does not install an automatic native offscreen pose/material evaluator.

Independent source review corrected that domain contract. Strict builds, host
tests and all 45 runtime cases pass. The real API5 fixture proves a candidate
can cast without appearing in the camera, hidden poses clear shadows without
new mesh uploads, and missing members/domain evidence preserve the prior owner.
Preview and PE contracts remain unchanged. Evidence:
`build/native11/observed-world-milestone.json`, `stage22-observed-world/`.
Live engine/visibility/caster acceptance and other draw domains remain open.


### Stages 23–25: mesh reuse, public control and readiness

Optional shared GPU meshes preserve independent material views. API6 appends
its opt-in configuration while retaining API1–5. The core world admission path
reports owned missing-member diagnostics and explicit other-domain absence gates;
source tickets and class generations invalidate stale reports. Frozen receipts:
`stage23-mesh-sharing`, `stage24-geometry-api`, `stage25-readiness`.

### Stages 26–40: native primitive composition

The candidate-provider adapter audits same-frame owned geometry and assigns
shadow-only roles, without inventing an offscreen engine pose evaluator. Native
surface controls add opt-in ordering, cutout thresholds, caster/receiver policy,
depth/load passes, sampling, affine UVs, distance fog and shadow filtering. CPU
sprite/billboard builders have clipped-UV, winding and lit GPU checks. Readback
staging is reused and invalidated on target or Map failures. Exact binary32
shadow-bias endpoints fix x87 excess-precision rejection without changing global
floating-point settings or the game-hook calling convention.

API7 adds a bounded full-manifest primitive service and an owned client. It keeps
API1–6 layouts/defaults and all frozen older clients. Candidate resources roll
back on failure; poses/material controls reuse hot resources. C11/C++ checks
also cover both vertex-header include orders, move ownership, generation/epoch
rules, foreign/reentrant calls and readback-response validation. See
[native primitive API and ownership](native11-primitives.md).

Frozen integration checkpoints are stages 26, 27, 28, 29, 33 (covers 30–33),
35 (covers 34–35), 36, 39 (covers 37–39), and 40. Numbered features are tracked
individually in the [completion checklist](native11-progress.md); grouping is not
a separate runtime or gameplay-coverage percentage.

The final core build is `20261103-native11-showcase-stage40`. All 60 private
runtime cases pass with legacy D3D8/9/D3DX providers disabled. Canonical 198 core
exports/imports and the helper's single export remain verified. Seven reference
previews and seven protected live files remain unchanged. The synthetic
[showcase image](../../build/native11/native11-showcase-stage40.png) demonstrates
a lit world pass and retained HUD pass; its pixels match the native TGA output.
The final receipt is `build/native11/showcase-milestone.json` and its immutable
archive is `build/native11/stage40-showcase/`.

This is staged engineering acceptance. Native offscreen effective pose/material
capture, full engine draw inventory, presentation/startup, native Windows and
user in-game visual/performance acceptance remain open. The installed game,
executables, FleetOpsHook and mod configuration have not been deployed or edited
by these stages. No game was launched and no GUI input was automated.


## Stages 41–50: complete-frame native presentation (staged)

Stage 50 is the grouped implementation/verification checkpoint for stages
41–50. It adds an explicit complete-frame domain inventory and ordered owned
passes; matching resource manifests, source identities and aggregate limits
are validated before GPU admission. Completeness and absence are host claims,
not automatic discovery of game producers.

The offscreen renderer can attach a borrowed host HWND through its own native
D3D11 device and DXGI swapchain. It copies the final texture on the GPU and
presents once per fresh eligible submission. Window loss, stale dimensions,
configuration, resize, invalid present input, occlusion and driver failures
revoke eligibility. The service never creates, activates or resizes a host
window. Controlled private fixtures verify copied swapchain pixels before
Present and exercise same-window ResizeBuffers failure/retry. Injected driver
errors do not physically remove the GPU; explicit device teardown/recreation
and service restart are separately exercised.

API8 appends two functions; API1–7 tables remain unchanged. The fresh core
retains its canonical 198 exports and identical DLL import list, the helper
retains its one export, and neither gains static D3D8/9/D3DX imports. The final
suite runs 66 private cases, including the frozen stage 40 API7 client as well
as earlier binary clients. All 66 exit zero; seven focused cases also pass.

PresentationSession renders only an admitted complete bundle, checks source
currency around each submission/presentation, and publishes no current frame
for partial failures, revocation or occlusion. NativeStartup loads an explicit
absolute native-service DLL path independently of legacy graphics handles and
supports suspension, resize, source invalidation, cleanup retry and restart.
Tests cover rejected reentrant and foreign-thread shutdown. A fallback result
requests host policy; it does not itself switch the real engine renderer. A
post-Present source check cannot undo a submission already accepted by DXGI.

Evidence is frozen in build/native11/stage50-presentation and recorded in
build/native11/presentation-milestone.json. No installed game file changed,
no game was launched, no native engine entrypoint was executed, and no GUI
input was automated. Seven reference preview TGAs remain byte-identical.
Private test windows remain hidden on an isolated Xvfb/Wine server; native
Windows, visible presentation, real engine startup/takeover, authoritative
offscreen animation/materials and remaining game domains remain open.

See [API8 presentation and ownership](native11-presentation.md) and the
[stages 41–50 contract](native11-stage41-50.md).


## Stages 51–52: pinned evaluation inventory and owned animation timing

The stage 52 checkpoint covers stage 51's twelve-range pinned static inventory
and stage 52's CPU animation phase policy, owned read-only clock reader and
optional core diagnostic. It leaves stages 53–65 unchecked: actual complete
offscreen geometry, other game domains, native engine presentation/startup,
parity/performance and release acceptance are still required.

The owned phase routine passed 6,144 bit-exact comparisons against the pinned
native channel-phase routine in an isolated inspection mapping under Wine.
The fixture applies six reviewed address relocations and resolves the sole
required `_ftol` IAT entry in private memory. It executes the native CPU routine
and its rounding/CRT dependencies; no game startup, simulation or native graphics
runs. Initial failed mapping attempts are retained separately. Full suite:
70/70 runtime exits zero; focused current-artifact run: 5/5. The fresh core
retains canonical 198 exports and identical imported DLL names; the renderer
service API1–8 and reference previews remain compatible.

`CaptureAnimationClock=1` is default-off and requires the existing checked node
observer. It adds no hook and calls no additional native engine routine. The
core reads validated current-instance/channel clock data twice, rejects changed
sources and malformed/oversized vectors, and compares native selected keys with
the owned CPU policy. Owner-thread admission precedes the reentry flag read.
The native material observer calls it only after successful posed observation;
material shutdown disarms it. Diagnostics neither certify offscreen poses nor
fail or take over the game's drawing.

The optional two-file trial tool defaults to dry run and verifies candidate and
protected baseline hashes. Explicit apply backs up the core DLL and Nexus INI,
merges only six capture keys and verifies atomic replacements. Guarded rollback
is retryable after a partial failure and preserves unrelated concurrent edits.
Ten synthetic temporary-directory tests pass. No trial is applied by this
checkpoint and all seven protected installation files remain unchanged.

See [timing and manual trial](native11-animation-clock.md),
[static inventory](native11-engine-inventory.md), and the
[fixed stages 51–65 gates](native11-stage51-65.md).

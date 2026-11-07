# D3D11 migration: investigation and first implementation

The real-window SM5 triangle was subsequently observed by the user and confirmed
by a successful 1920x1080 draw/Present log. The one-frame probe is now off.
See [shader capture](shader-capture-20261021.md) for the next bounded diagnostics
gate and [UI slice](ui-slice-20261021.md) for tested backend-building-block scope.
The subsequent [captured VS1.1 slice](vs11-slice-20261021.md) translates three
native A2FO programs to SM5 with paired pixel comparisons. Full game routing
remains pending; these component milestones do not enable Backend=d3d11.
The [BC5/BC7 texture slice](bc-textures-20261021.md) adds compressed DDS mip-chain
upload and GPU sampling. The [FO4 BC child/resource bridge](fo4-bc-test-20261021.md)
adds immutable D3D8-facing texture/surface ownership and prepared child assets;
the game compatibility device and draw routing still need implementation.

Date: 2026-10-21. Scope: first bounded migration pass. **There is no D3D11 game-rendering backend yet.** The implementation provides optional native-provider API observation and a one-frame D3D11 window probe, with D3D8 retained as the default. The subsequent [transformed-UI slice](ui-slice-20261021.md) adds SM5 draws, texture upload helpers and persistent bytecode caching; whole-frame COM/resource/shader translation remains pending. Device/probe fixtures are separate from Armada/FO acceptance. No game installation is changed by building this work.

## Findings that determine the architecture

1. Keep the existing startup proxy, executable checks, module loader and graphics options. They already own compatibility behavior. The current `[Renderer] Backend=system|dxvk` selects a managed **D3D9 provider**, not a D3D8 compatibility implementation. Reusing that key for D3D11 would break its helper and mapped-material safety policy.
2. Intercept the imported D3D8 factory by name and retain its resolved provider. Do not install an unrelated `d3d8.dll` over the current wrapper. The first observer keeps the native COM object addresses and forwards every intercepted call unchanged.
3. A fixed-function-only shim cannot preserve current A2FO functionality. Native DOT3 and A2FO shader/state-block/resource paths need explicit treatment. Do not disable bump mapping, M3D, emissive or cloak functionality to obtain a picture.
4. Menus, videos and gameplay have several presentation owners. Translating gameplay's D3D8 device alone does not move GDI, Bink D3D9 or the offscreen Refit D3D9 renderer to D3D11.
5. A live D3D11 swap chain and a functioning renderer are distinct milestones. The diagnostic mode presents one SM5 diagnostic triangle, destroys its device, then allows subsequent legacy frames. It never advertises translated gameplay.

## Current call path and static evidence

```text
Armada2.exe / ArmadaL.exe
  -> imported Win2kDisableTaskSwitch.dll (A2FO startup proxy)
       -> preserved Win2kDisableTaskSwitch.original.dll
            -> FleetOpsHook.dll where present
       -> early A2FOExtensions attach
            -> checked built-in hooks
            -> optional named Direct3DCreate8 import observer
            -> deferred module discovery/initialization

native game renderer -> d3d8.dll!Direct3DCreate8 -> IDirect3D8::CreateDevice
                    -> existing D3D8 provider/device/resources
                       [typical managed chain: dxwrapper/d3d8to9 -> D3D9]
                    -> system D3D9 / managed DXVK as already configured
```

`core/startup_proxy.cpp` retains original-first loading and early core attachment. `core/dllmain.cpp::DllMain` installs the optional factory observer after `validate_armada_module`, outside FleetOps-only shader admission, so the import seam is not tied to new binary offsets. It performs no D3D11 loading/device work during attach. Its installer is deliberately startup-only, serialized by DLL attachment; it is not an API for concurrent installation/removal.

The checked-in `ArmadaL.exe` and `reverse_engineering/Armada2/Armada2.exe` import `d3d8.dll!Direct3DCreate8` at preferred VA `0x007b82cc`. The thunk is `0x00673622`, called at `0x00622fe5` and `0x00623043`; factory state is stored at `0x007ab870`. Lead disassembly of `ST3D_DeviceDirectX8::CreateDevice` finds the actual factory-vtable call at `0x006237ab`, offset `0x3c` (slot 15, `IDirect3D8::CreateDevice`), after setup of the output pointer at engine-wrapper offset `0x90` and presentation parameters at `+0xac`. These are static facts for these files, not an assertion that every installed executable has the same layout. The observer discovers the import rather than patching those addresses.

Commands: `i686-w64-mingw32-objdump -p <exe>` and `-d -Mintel --start-address=0x6235d0 --stop-address=0x623900 <exe>`. The clean `Armada2.map` names the function (`reverse_engineering/Armada2/baselines/clean/inputs/Armada2.map:11095`). Map names alone remain hypotheses until cross-checked. Source executable hashes and installed-state hashes are recorded in the first-pass evidence receipt; the source and installed ArmadaL files are different and must not be substituted blindly.

Existing checked renderer seams in `core/nebula_renderer.cpp` include:

| Seam | Armada RVA / owner | Behavior to retain |
| --- | --- | --- |
| engine device resolution | engine current device `+0xc0`, table `+0xcc`, wrapper native device `+0x90` | native COM/provider identity |
| native frame BeginScene | `0x223e9c` | frame-owned effects lifecycle |
| RefreshDisplay final EndScene | `0x224710` | composite immediately before native Present |
| pre-Reset | `0x223ce4` | release default-pool extension objects before native Reset |
| device destruction | FO callback `0x1fd0a0` | release retained extension objects before native destruction |
| native/FO DOT3 draws | Armada `0x2279af`, FO `0x1e67c8` and `0x1e62ec` | shader/constants, pass ordering, alpha and lighting |
| workspace indexed draw | `0x248bfb` | exact submitted streams/counts |
| direct FO D3D9 shader creation | FO `0x1f1474` | separate narrow AMD compatibility route |

These remain owned by the existing checked hook implementation. Most material/draw/frame/reset effects are deliberately DXVK-gated. `load_dxvk_backend_policy()` compares the active D3D9 DLL bytes to the managed payload; an INI claim alone cannot enable effects. The new observer does not override that policy or claim D3D11 is safe for those effects.

`tools/a2fo_renderer_helper.cpp` waits for game exit and changes only its managed D3D9 payload. `core/renderer_options.cpp` exposes that restart-applied choice, monitor selection and live map toggles. Neither is changed by this milestone.

## Other rendering and presentation owners

| Component | Current path | D3D11 migration implication |
| --- | --- | --- |
| native scene/gameplay/editor | D3D8, with FO's optional `/d3d9` bypass | capture actual interfaces per executable/path; D3D8 observer does not cover `/d3d9` |
| Refit 3D menu scene | `modules/A2FORefitMenus/menu_scene.cpp`: independent hidden D3D9 HAL worker, readback, DIB/GDI exchange | keep its host ABI, persistent worker, mailbox/epoch and asset roots; eventual native D3D11 host is a separate migration |
| IA/loading overlay | `loading_gpu.cpp`: native D3D8 image draw and saved/restored device state | state blocks, FVF, texture uploads, alpha and native overlay order are parity requirements |
| ShellDisplay | native logical canvas / physical presentation, monitor and borderless policy | do not replace window ownership, logical coordinates or hitboxes in the compatibility layer |
| intro/Bink | engine path and a separate D3D9 movie device in `A2FOShellDisplay/bink_video.cpp` | verify both, including skip and transition teardown |
| A2FOVideo | FFmpeg decode, DIB/GDI and audio | outside the D3D8 trace; keep lifetimes/order |
| mod manager/tool dialogs | native FO/VCL/GDI windows and ShellDisplay behavior | no blanket swap-chain ownership or window-procedure replacement |
| indexed/compressed/custom textures | existing loader/extensions -> native texture APIs | preserve decoding/mod roots and distinguish file format from GPU format |

The latest installed Roots log inspected during this pass already reports native Fleet Ops DOT3 draws, classic workspaces, M3D diffuse handling, alpha compositor shaders, borderless 1920x1080 and menu/backdrop transitions. That is coarse existing-route evidence, **not** a new D3D8 API trace or a D3D11 run.

## Reference comparison and licence review

| System | Ownership | Useful lessons | Reasons not to transplant wholesale |
| --- | --- | --- | --- |
| Armada/A2FO | original render logic plus checked targeted extensions and several presentation owners | preserve behavior, mod asset roots, native UI/actions and shader/effect ordering | binary-specific hooks and backend safety remain authoritative |
| Armada 2 Refit | game-integrated render hooks plus the local menu-only D3D9 host adaptation | per-device frame state; cached models/world resources; explicit host/thread/reset ownership | the local build intentionally disables upstream engine hooks/input/sounds; its style/menu actions must not replace FO mod behavior |
| d3d8to11 | D3D8 COM facade, D3D11 resources/state and generated SM5 fixed-function shaders | FVF/input layout, transform/lighting/fog/combiner translation; bounded state keys | missing shaders/state blocks/resources/lost-device semantics, skipped indexed fans, TODO combiners/texgen, OIT and shortcut polling |

The complete reference review is [d3d8to11-audit.md](d3d8to11-audit.md), with a [method inventory](d3d8to11-method-status.csv). d3d8to11 was reviewed at `d4b2c6c5d7d1ec3b96881476c83637efed598514`. Refit's existing vendor is pinned to `37d60079169b086dba4216fe55e05cd6015d66c0` with source/manifest/local-change notices. The task's `cedrichaase/armada-2-refit` URL returned 404 in this session; the existing project provenance points to `cedrichaase/armada2-refit` and the local source was used. This is not verification of a newer upstream revision.

d3d8to11 has an [MIT notice](https://github.com/michael-fadely/d3d8to11/blob/d4b2c6c5d7d1ec3b96881476c83637efed598514/LICENSE). Its Patrick Mours headers reference d3d8to9, whose current [BSD 2-clause licence](https://github.com/crosire/d3d8to9/blob/main/LICENSE.md) was checked separately. Both require retained notices with derived redistribution. Some HLSL is marked as derived from Microsoft's `FixedFuncEMU.fx`; its applicable original terms still need locating before direct HLSL reuse. Refit is MIT with preserved libm/font notices. This implementation copies no reference implementation code or shaders; its SDK-typed thunks are generated from public interface declarations. Future copied portions require a file-level provenance entry and the complete applicable notices.

## Compatibility matrix and API usage investigation

[compatibility-matrix.csv](compatibility-matrix.csv) covers all ten concrete observed interface families. Each method separates native Armada II observation, native Fleet Ops observation, static extension call candidates, upstream implementation status and our translation status. Three preserved FO-family game traces now identify 74 observed methods; stock Armada observations remain unknown. Unknown never means unused. `d3d8to11-method-status.csv` also inventories disabled cube/volume code and implementation helpers, so a non-stub body is not proof of an available API.

The optional observer covers the factory, 97 device methods, textures/cube/volume textures, surfaces/volumes, vertex/index buffers and extra swap chains. It keeps native object pointers and original vtables; it redirects observed objects to cloned dispatch tables, not global provider-code detours or replacement resources. Tables are shared by native implementation/type and retained for process lifetime to allow implicit Direct3D resource resurrection after public Release(0). Known getters/QueryInterface results extend observation; native IUnknown and parent GetDevice identity remain intact. Foreign extension interfaces, dynamic factories acquired outside the patched imports, direct D3D9, GDI and independent menu devices are explicitly outside coverage. Provider-internal direct calls can also bypass an API boundary. Unknown providers may rely on their vtable address: this is an opt-in experiment, not a claim that every wrapper is certified.

The observation records numeric input arguments, formats, usage/pool/lock flags, state/texture-stage keys and values, FVF-or-shader handles, primitive types/counts, targets/streams, shader creation/constant ranges, HRESULTs, requested presentation descriptors and viewports. Resource descriptions are captured when queried. It does not dump texture pixels, vertex contents, shader source/constants or every matrix/light value. D3D8 sampler settings are texture-stage states; there is no D3D9-style `SetSamplerState` API. There is no native D3D8 scissor API either: clipping must be traced at engine/viewport/geometry boundaries, not invented as a D3D8 call.

Output: per-process `A2FOD3D8-<pid>-<tick>.jsonl` beside `A2FORenderer.ini`. Unique parameter signatures receive one call sample; per-method and retained-signature counters continue after the sample budget. Histograms cap at 8,192 signatures; samples default to 8,192 (configurable 1–100,000); total output including summaries caps at 4 MiB. Overflow/limit markers make incompleteness explicit. Summaries are cumulative at the first Present and each 600 Present boundaries; call samples flush immediately. Timing includes observer overhead and therefore is not a clean renderer CPU profile. Short, labelled scenario runs are preferable to a single long capture.

Capture procedure, without input automation:

1. Preserve each installation's current logs/config/DLL hashes and record executable, FO version, mod, Wine/Proton prefix, wrapper versions, dimensions, GPU and command line. Keep stock Armada and FO results separate. Use the candidate on a backed-up test install only after the isolated fixtures pass.
2. Start with `Backend=d3d8`, `Trace=1`, unchanged `[Renderer] Backend`. Manually launch, play/skip intros and visit main/IA/editor/mod-manager/multiplayer setup. Quit and label the resulting trace plus user-supplied screenshots/logs. If the menu has no game D3D8 device yet, missing calls are expected; GDI/offscreen-menu timings need the existing menu log.
3. Run a short map per requested faction (A2: Federation/Klingon/Romulan/Borg/Cardassian/8472; FO: Federation/Klingon/Romulan/Borg/Dominion), then representative ships/stations/planets/nebulae/weapon/particle/cloak/shield/construction/damage effects. Include M3D and current extension materials.
4. Repeat representative scenes at low resolution, 1080p and available 1440p, windowed/borderless/fullscreen. Record resize, alt-tab, lost-device and reset observations manually. Never force a reset/input event through desktop automation.
5. Keep `/d3d9` runs separately marked and report no D3D8 coverage for them. Decide later whether to intercept that API or explicitly retain its legacy fallback.
6. Merge only actual game traces with `python3 tools/renderer_usage_matrix.py --armada-trace <file> --fleetops-trace <file> --output <matrix.csv>`. Do not label the synthetic or hidden GPU fixtures as game observations. An absent call remains unknown for other scenarios, factions, mods and backends.

## Proposed translation architecture

```text
existing game renderer, resources, assets and extension calls
  -> D3D8-facing COM compatibility facade (future; native observer is stage zero)
     -> complete state shadow / state-block snapshots
     -> resource identity registry + pool/lock/reset contracts
     -> draw packet (FVF/declaration/shader + streams + stage/state keys)
        -> D3D11 buffers/textures/state-object caches
        -> SM5 fixed-function / legacy shader translation
        -> targets/depth/viewports
        -> single DXGI presentation owner
  -> separate A2FO material/format extensions above that compatibility contract
```

Keep `renderer/d3d8_compat`, `d3d11`, `state`, `resources`, `buffers`, `textures`, `shaders`, `swapchain` and `diagnostics` as separate modules as implementation reaches them. No empty pretend implementations are introduced. A full D3D11 backend must own a complete compatible device lifetime; it cannot switch individual unsupported draws to an unrelated D3D8 device and expect shared depth/targets/state. Backend selection is a startup decision, followed by explicit reset/recreation boundaries, with failures reported rather than silently returning success for dropped rendering.

Resource facade rules: canonical COM identity/QueryInterface/GetDevice/GetContainer; distinct native handles; device references follow API ownership; byte-accurate CPU lock shadows with READONLY/DISCARD/NOOVERWRITE/subrect/pitch semantics; formats/caps advertised only when implemented; DEFAULT resources released for reset and MANAGED/SYSTEMMEM contents retained as required. Preserve all index sizes, BaseVertexIndex and triangle-fan semantics. Keep render-target aliasing, copy operations, mip updates and lost-device response explicit.

State rules: D3D8 defaults, shader handles versus FVF, stream stride/index offsets, row/column matrix conventions, viewport depth/half-pixel conventions, material/light/fog/specular/alpha-test and alpha-blend behavior, texture combiners/texgen and alpha-source rules must be independently tested. Programmable VS1.x/PS1.x paths are a correctness blocker even if fixed-function menu/UI works first. Existing A2FO DOT3/mapped materials and state-block restoration cannot be dropped.

Texture file formats remain above the resource translation layer. Existing TGA/indexed/compressed loading and extension lookup rules remain authoritative. Translate actual GPU formats first; future DDS/BC1–3/BC4–5/BC7/PNG/KTX2 loaders should feed a common descriptor/upload API, not leak file decoding into device-state translation. No new material style or effects are part of parity.

## Persistent shader-cache design

d3d8to11 persists permutation **flags** and recompiles them at startup; that is not a persistent compiled-bytecode cache (`device.cpp:309-420,852-999`). Its thread pool and uber-shader behavior are not brought into this pass.

Our planned cache stores validated compiled VS/PS bytecode, keyed by: compatibility ABI version, complete normalized fixed-function state and FVF/declaration, shader generator/source+include hashes, compiler identity/flags, entry point, shader model, precision/format policy, and any relevant feature-level capability choices. Never key on pointers, file timestamps alone or native handles. Validate magic/version/key/size/digest before `Create*Shader`; reject truncated/corrupt/mismatched entries, recompile and atomically publish. Keep source-root/mod overrides and include content in invalidation. Driver/GPU pipeline caches are separate from portable DXBC. Record hit/miss/load/compile/create times and compiler diagnostics. Startup/UI prewarming may use captured known keys only after semantics are correct; no asynchronous complexity yet.

No shader cache or new shader option is exposed in this first pass because no translated shader exists yet. A cache scaffold with no consumer would not resolve the current menu delay. Known permutations can be precompiled later after mod overrides and shader contracts are settled.

## Menu lifetime and performance plan

Retain the existing persistent worker/device/world, view epoch, camera handoff, asset roots, style and native FO actions. Recent work already keeps menu/loading continuity and prewarms assets. Do not reintroduce per-menu world/device destruction under a D3D11 label. Refit's cached models/world resources are useful; its game-integrated path avoids the local adaptation's synchronous GPU readback + GDI round trip, which remains an architectural cost.

Profile cold and warm runs separately: device creation; bytecode load/compile; texture decode/upload; model/scene load; worker submit/GPU completion/readback; DIB/GDI exchange; first main-menu frame; main→IA; IA→loading; first map frame. Correlate the existing menu timings with API counts, not draw-count guesses. Track cache hits and live-object/byte totals so residency is bounded, and invalidate only on actual device/reset, changed resource roots/mod or asset recipes. Retaining camera alone does not prove scene/resource retention or smooth transitions.

## Staged implementation and exit gates

| Stage | Status / bounded next work | Required exit evidence |
| --- | --- | --- |
| 1 architecture/reference | current source mapped; upstream method audit and matrix produced | checked source/binary evidence, licence/reuse ledger |
| 2 optional observation | implemented; synthetic/native fixtures separate from game traces | labelled Armada/FO captures for menus/editor/gameplay/extension paths |
| 3 D3D11 infrastructure | implemented diagnostic device/context/swapchain/RTV/depth/viewport/clear/Present | hidden provider coexistence fixture, then manual real-window probe and unchanged legacy startup |
| 4 compatibility resources/state | in progress: FVF/fan/format and explicit UI state helpers; full resources/locks/reset pending | resource/lock/reset/state-block/COM fixtures; capability truthfulness |
| 5 fixed-function SM5 + legacy shaders | single-stage transformed UI SM5 + persistent bytecode cache implemented; legacy programmable shaders pending | reference image/readback fixtures, complete keyed bytecode cache; no skipped draws disguised as success |
| 6 UI/2D parity | 19 paired synthetic GPU scenes; native UI admission pending | fonts/filter/alpha/colour-key/half-pixel/clipping/scaling and manual hitbox checks in FO |
| 7 native 3D parity | pending | SOD/M3D, all factions/objects/effects and heavy-map comparisons |
| 8 menu residency | existing implementation retained; profile before changes | cold/warm transitions with resident-resource evidence and native visual confirmation |
| 9 presentation | existing window policy retained; native D3D11 presentation later | resize/alt-tab/reset, monitor/DPI/aspect, windowed/borderless/fullscreen; vsync/pacing independent of simulation |

For each translated slice: executable/provider pinned, synthetic contract test, hidden native fixture, build/import audit, then manual stock and FO acceptance. Do not make D3D11 the default until the whole parity matrix passes. Enhanced materials, normal/specular/emissive expansion, HDR/OIT/bloom/postprocessing/instancing and Vulkan are deferred.

## Configuration, build and manual probe

Keep the existing `[Renderer]`, `[Effects]`, `[Compatibility]` and `[Diagnostics]` values. Merge only this separate optional section into a backed-up test installation's `A2FORenderer.ini`:

```ini
[D3D8Compat]
Backend=d3d8
Trace=0
MaxEvents=8192
DebugLayer=0
```

Default absence is identical to `Backend=d3d8, Trace=0`: no import patch, object metadata or D3D11 loading. `Trace=1` enables observation. `Backend=d3d11-probe` attempts **one SM5 diagnostic triangle frame** after the first successful native **windowed** device Present, targeting its actual HWND/client size, then immediately tears down its D3D11 objects. A failed probe leaves the native HRESULT and D3D8 device unchanged. Exclusive fullscreen is deliberately ineligible; use windowed or borderless for the probe. Unsupported `Backend=d3d11` is refused with an explicit fallback message, because that renderer is not implemented. Probe logging identifies success/failure and hardware creation time; a one-frame triangle may be too brief to see, so use the log too.

The probe requires a same-process HWND on its owner thread, nonzero dimensions ≤16,384 and hardware feature level 11_0. It dynamically loads D3D11 only when invoked, creates a windowed discard swapchain with RTV/D24S8/viewports and disables DXGI window/alt-enter changes through `MakeWindowAssociation`. It changes no styles, focus, input or game window procedures. Microsoft documents calling [MakeWindowAssociation after swapchain creation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-makewindowassociation). Shared-HWND coexistence still needs provider-specific and real-game validation; a hidden fixture is a narrower claim.

Build: `make build/A2FOExtensions.dll renderer-modernisation-tests`. The latter only builds console fixtures; run them using the isolated runner described in [validation.md](validation.md). Generated thunks are checked in; `python3 tools/generate_d3d8_observer.py --check` detects SDK declaration drift. The new feature adds no mandatory D3D11 or DXGI DLL import to the core.

Returning to `Backend=d3d8, Trace=0` and fully restarting disables the experiment. Startup-installed object callbacks are pinned for process lifetime, so hot unloading/replacing the DLL is unsupported. No renderer helper or active D3D9 wrapper replacement is necessary. The first-pass candidate is staged locally; installed game binaries/configuration remain unchanged.

## Acceptance remaining

Every native game item in the user request remains an explicit acceptance target: launch; play/skip intro; main and 3D menus; editor; IA/gameplay; ships/stations/planets/nebulae; weapons/particles/shields/cloak/construction/damage; UI alpha/textures/fonts/filtering/hitboxes; requested factions/mod manager/multiplayer setup; low/1080p/available 1440p; normal/centred/borderless/exclusive modes, resolution transitions and alt-tab. First compare legacy with observer off/on, then run the triangle probe, then compare each translated slice. No screenshot, menu/gameplay parity or performance improvement is claimed without those runs.

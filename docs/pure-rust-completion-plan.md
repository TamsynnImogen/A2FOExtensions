# A2FOExtensions: complete pure-Rust completion plan

Audit date: **29 October 2026**. Status: **rewrite incomplete; this is the remaining-work and acceptance plan, not a completion certificate**.

## 1. What the current build establishes

The project has substantial Rust implementations, but the delivered A2FOExtensions DLL is still a mixed Rust/C/C++ program. Completing the earlier seven migration phases in their *bounded* scopes did not replace every implementation those phase titles describe.

The current main DLL is SHA-256 `9a019cb01a955a26a8884a0578bc01d0ed817b433a19abd794f7e228aa6f2013`, with 197 public exports. On this audit date its bytes match the installed `Fleet Ops Roots/Data/A2FOExtensions.dll` and the tester package. Its recorded stage-16 ordered link manifest supplies **448 C/C++ translation units** (401 C++, 47 C), **44 bridge objects built from Rust**, and **2 resource objects**. This counts supplied objects, not sections surviving linker elimination, lines of code, or percentage remaining. Quoted-include scanning additionally identifies 53 `.cpp` bodies and 21 `.inl` fragments, counted separately from direct compilation units. Executable headers and extracted toolchain-library members need further analysis and are not included in that count. See the source inventory annex.

The 151/151 portable-unit census is complete **for its frozen membership**. It does not count complete feature ownership, platform implementation, the MMM framework, or all shipped programs. The separate ownership checkpoint credits only **StationRotation and NormalWeaponTech with bounded state-ownership evidence**. The other **46 of 48 feature families** still require ownership audits and completion; the two credited families still have native wrappers and require full artifact closure. Shared core is partial. A later renderer slice owns compressed-texture upload/view behavior in Rust, but most rendering/platform implementation remains native.

Already implemented work must be reused and reverified, rather than rewritten merely to generate new credit:

- Rust DLL entry sequencing, portions of runtime globals, loaded-module storage, seven callback-record stores, and the two bounded feature-state owners.
- Portable policy/controller/parser work represented by the 151-unit census.
- All 48 recorded external assembly families expressed through Rust assembly sources; only 44 such objects occur in this particular main-DLL link. The old `.S` names in mappings are reference identifiers, not proof of current assembly compilation.
- Checked memory patch/trampoline operations and selected floating-point operations.
- The bounded compressed-texture upload/view owner.
- The MMM compatibility loader and mission handoff controller, **not** the complete mission runtime.
- Rust media decoding/playback for the documented H.264/AAC and VP9/Opus/Vorbis container combinations. Production FFmpeg linkage is retired.
- The Rust refit scene-provider DLL. This does not make its native RefitMenus consumer Rust.
- Retirement of the old hook and decal-math C++ facades.

Evidence: [runtime ownership](rust-runtime-ownership.md), [platform/renderer](rust-native-platform-renderer.md), [assembly status](../build/rust-migration-stage13/hooks-assembly/WORK_STATUS.md), [MMM](rust-mmm-migration.md), [media](rust-media-replacement.md), [adapter retirement](rust-adapter-retirement.md), [acceptance](rust-rewrite-acceptance.md). Statements in older phase documents about private staging/deployment describe their checkpoints, not today's installed state.

Audit method and reproducible extraction: [build/source findings](../build/rust-purity-audit-20261029/build-inventory/findings.md), [build inventory extractor](../build/rust-purity-audit-20261029/build-inventory/generate.py), [documentation annex generator](../build/rust-purity-audit-20261029/write_report_annex.py). This report was checked against local source/build records; it did not run a new build or game session. Snapshot hashes identify evidence, not completed migration work.

## 2. The definition of 100% Rust

Completion means **all executable A2FO product implementations are Rust**, including Rust `asm!`/`global_asm!` where precise machine instructions are required. This covers the main DLL, startup proxy, every supported standalone feature DLL, mission loader/runtime, media DLL, renderer helper, log-mail helper, scene provider, and any supported shipped utility or service. Moving C++ storage behind a Rust handle, wrapping Lua/FFmpeg with Rust bindings, or compiling C++ through a Cargo build script does not satisfy it.

The finished product must meet all of these conditions:

1. Rust owns A2FO policy, mutable state, containers, allocation/deallocation pairing, lifetimes, resource releases, synchronization, worker shutdown and error handling.
2. No C/C++ implementation or external assembly implementation is compiled into or shipped as an A2FO runtime component. No retained Lua C VM, Luabind, Boost implementation, zlib/minizip C, FFmpeg codec, C++ exception firewall, STL owner, C++ proxy, or C++ helper is hidden beneath Rust APIs.
3. Every executable object/archive member has a recorded origin. C/C++ CRT startup, `libstdc++`, native exception/unwind/thread/runtime helpers and compiler-support routines must be investigated and removed or replaced where they contain implementation code in shipped artifacts. Calling an archive “toolchain glue” does not exempt it. Import libraries that only describe external platform symbols must be distinguished from implementation archives.
4. Every shipped executable and every supported production build variant is covered. A Rust main DLL with a C++ helper or old mission DLL left beside it is incomplete.
5. A clean production build cannot depend on previously compiled native implementation objects. It must work with C/C++ compilation disabled. A linker, PE resource compiler, shader tooling and SDK declaration inputs may remain; they must not compile retained C/C++ runtime implementations.
6. Existing exports, module contracts, data/config/mission/save formats and supported behavior remain compatible. Deleting a feature or silently dropping a supported build target does not complete its port.

### External platform boundary

The proprietary Armada/Fleet Ops engine, Windows APIs, DirectX/COM system implementations, graphics/audio drivers and the operating system remain external services. Rust must implement the A2FO side of those calls directly. This plan does not propose rewriting Windows, DirectX or the closed-source game. Record an explicit allowlist of external binaries, APIs and ownership rules; an arbitrary new native helper is not an allowed platform dependency.

The engine's existing Bink playback service is within that external engine boundary. Its A2FO routing/admission/state must become Rust. The result must be described accurately as a pure-Rust extension calling native engine playback, not as a Rust Bink decoder. If a separately shipped Bink implementation or another codec library becomes an A2FO dependency, it must receive a Rust replacement or fail the purity gate.

Mission Lua scripts remain Lua content. Shaders remain shader programs. Resources, fonts, assets, `.def` export descriptions and C SDK declaration headers may remain as data/interfaces. Python/Make used solely to build, package or test may remain as development tooling. A Python relay or other service needed at runtime is executable product implementation and is included. Historical C/C++ reference fixtures may remain excluded from production, with their exclusion verified. Supported authoring/diagnostic programs are included; currently absent/deleted source trees are not to be restored or declared ported by assumption.

## 3. Completion ledger and evidence rules

Use the work IDs below as closure items. Feature IDs `F01`–`F48` are individual items, not one bulk checkbox. [The inventory annex](pure-rust-completion-inventory.json) records current source/target evidence and the feature file lists. [The file checklist](pure-rust-source-checklist.md) provides readable source and build-target coverage.

Every item needs these records before it closes:

- All production callers, included implementation bodies, state owners, built-in/standalone variants and transitive dependencies enumerated.
- Replacement Rust source, target/platform assumptions, ABI/layout guards, failure/cleanup/locking rules and compatibility decisions reviewed.
- Old/new differential evidence where behavior is observable; mutation, reentry, resource failure and shutdown tests where relevant. Golden output alone does not prove ownership or cleanup.
- Actual production artifact linked and tested, with command/exit status, source/dependency hashes, exports/imports, link map and object/archive-origin evidence.
- Former production implementation removed from **all** active build paths only after its final consumer has moved. Reference evidence retained separately.
- Applicable in-game acceptance completed. Source-complete, built, headless-tested, packaged, deployed and in-game accepted are separate fields.

Pending source analysis is real remaining work. This audit is complete as a closure plan and an inventory of known build inputs; it is not a claim that the behavior of all native code has already been reverse engineered. Any newly discovered runtime implementation joins the ledger automatically. **Zero unclassified production inputs and zero unresolved required compatibility decisions are mandatory final gates.** Do not calculate a whole-project completion percentage from source-file counts or the portable census.

## 4. Shared implementation work

### P00 — Freeze the complete supported product and source graph [LEAD]

Create a current, reproducible product manifest: main DLL, proxy, 48 families in built-in/standalone arrangements, MMM loader, media, scene provider, helpers, utilities/services, resources and deployment paths. Enumerate target-specific features and debug/release variants that are actually supported. Reconcile Makefile rules, Cargo/rustc builds, release/install scripts, SDK examples, tester ZIP and original installation; none alone is the complete product list.

Expand textual includes through headers, generated bindings, resource recipes, static archives, build scripts, subprojects and optional targets. Audit dormant fallback paths and dynamically loaded libraries. Classify every source as production, reference/test, build-only, declaration/data, already Rust, or unsupported historical material. A source with an unknown role blocks closure. Resolve currently absent authoring-tool sources and inherited old installation DLLs explicitly; preserve them while their support status is determined.

**Exit:** every supported deliverable and production input has exactly one owner/work item; no missing target or unexplained object/member; current-vs-checkpoint drift documented. This inventory must be refreshed after subsequent changes.

### P01 — Complete Rust DLL startup, shutdown and native-runtime removal [LEAD]

Sources: `rust/a2fo_dll/{src/lib.rs,build.rs}`, `core/dllmain.cpp`, `core/runtime_handlers.hpp`, `core/module_loader.*`, `tools/build_rust_owned_dll.py`, `tools/rust_dll_linker.py` and their included/shared support.

The Rust entry still calls `a2fo_native_process_attach_step` and `a2fo_native_initialize_worker`. Replace those native operations, initialization/logging/compatibility checks, deferred worker, thread handles, module discovery/load/init/shutdown, path/string ownership and all retained native module-record wrappers. Preserve loader-lock restrictions, attach ordering, retries and failure behavior. Make the process-lifetime policy explicit. If explicit unload remains supported, prove its worker/callback quiescence and resource safety; do not add teardown under loader lock.

Replace native static initialization/destruction and dependency on MinGW C++/CRT bootstrap. The current DLL build script explicitly restores a static runtime tail including `stdc++`, `mingw32`, `gcc`, `gcc_eh`, `mingwex` and `pthread`; removing a flag alone is not proof of removal. Select and verify an x86 target/link/runtime strategy for **each** artifact, including standard-library sidecars. Replace any remaining native support implementation or use a verified Rust implementation; audit compiler-emitted helper calls and archive members.

**Exit:** Rust-owned PE startup to worker to all module lifecycle operations, no native process-attach/worker callbacks, no C++ constructors/firewalls/runtime implementation, actual DLL load/failure/retry/lifecycle tests and origin-clean link. Core and feature-state completion are separate prerequisites, not implied by this item.

### P02 — Finish shared state, registries, SDK services and discovery [LEAD]

Sources include `core/dllmain.cpp`, `module_loader.*`, `module_policy.*`, `game_monitor.*`, `game_monitor_policy.*`, `extension_roots.*`, `fpq_paths.*`, `odf_paths.*`, `builtin_state.*`, all `core/builtin_*.cpp` wrappers, `core/runtime_handlers.hpp` and shared `modules/refit_common/` support.

Move remaining policy maps, caches, root strings, configuration, logging/diagnostic buffers, service tables, enabled-state and callback adapters into Rust. Preserve mod-parent search precedence, encoding/case/path behavior, ODF/FOFS integration, module dependencies and duplicate built-in/standalone selection. Do not interpret foreign MSVC6 STL layouts as Rust containers; access engine-owned data through verified Rust ABI operations or representation-specific readers with validated lifetime rules.

Resolve the recorded live-dispatch mutation hazard: clearing/replacing callback registries during dispatch can stop delivery. Specify snapshot/generation/lease behavior, then test callback removal/addition, nested dispatch, shutdown during callbacks, allocation/logging failure, stale module pointers and duplicate teardown. Preserve the seven Rust-backed stores and test their integration rather than replacing them casually. Preserve SDK C compatibility, API revision, callback contracts, filename/service discovery and existing export ordinals/forwarders.

**Exit:** no native authoritative shared state or registry facade; no dangling callbacks after module transitions; foreign engine storage remains engine-owned; built-in and standalone service interactions pass.

### P03 — Replace Win32 UI, filesystem, COM/GDI and crash/report adapters [LEAD]

Sources include `core/module_menu.cpp`, `renderer_options.cpp`, startup/config/path code, feature Win32 consumers, `core/com_owner.hpp` and the helper sources listed later. Port windows/messages/control creation, native UI-thread dispatch, timers, image/font/bitmap ownership, GDI/GDI+ drawing, shell dialogs/shortcuts, file/process/library/thread/event APIs, registry/settings, reporting and diagnostic operations. Cover resource and callback lifetimes, UTF-16 conversion, return-code interpretation and partial-failure cleanup.

Implement Rust ABI declarations/vtables and error translation; preserving a C++ `try/catch` around a Rust owner is an intermediate state. Inventory calls into foreign code that can throw or fault. Resolve their required containment behavior using an explicitly tested Rust-owned design before retiring firewalls. Rust panic catching is not evidence of foreign C++ exception/SEH compatibility. Where the current target cannot support the required behavior, record and solve the target/ABI blocker; do not silently weaken recovery.

**Exit:** direct verified Rust platform integration, no A2FO C++ UI/resource/error adapter, real API failure tests and unchanged visible settings/menu behavior.

### P04 — Complete D3D11 backend, UI renderer and presentation [LEAD]

Sources: all `renderer/d3d11/`, including `ui_renderer`, `probe_device`, `probe_bridge`, `com_guard`, `compressed_texture`, `texture_handle`, `game/backend`, `game/draw_pipeline`; related state/texture/shader helpers and native COM owners.

Port device/context/swapchain creation and recreation, capability probing, feature-level fallback, HWND association, render targets/depth resources, shaders/input layouts, vertex/index/constant buffers, draw submission, textures/uploads/readback, resource caches, resize, presentation and errors. Complete the compressed-texture slice by removing its C++ typed-call/STL/exception adapters. Preserve publish-before-release order, failure propagation, COM reference ownership and device-loss/reentry/thread rules. Translate all shader/token/cache/format/FVF/texture support still implemented natively, even where adjacent portable code exists.

**Exit:** all D3D11 A2FO operations and authoritative resources Rust-owned; COM layout/refcount/method-slot and failure fixtures pass; visible UI/world rendering and driver/device transitions accepted.

### P05 — Complete the D3D8 compatibility implementation [LEAD]

Sources: all `renderer/d3d8_compat/`, including observer, compressed textures, game factory/device/resources/draw/state-blocks/unsupported/bc-catalog/D3DX bridge, and shared D3D11 game backend/state/shader translation dependencies.

Implement the complete exposed COM surface in Rust: factory/device queries/caps, `QueryInterface`/identity/refcounts, textures/surfaces/buffers, locking/upload/copy/readback, render/depth targets, transforms/materials/lights, render/texture-stage/sampler states, shaders/constants/FVF, draw calls, state blocks, reset/lost-device and documented unsupported behavior. Inventory actual vtable slots and supported methods instead of assuming the filenames cover the ABI. Preserve intercepted imports/vtables and nested calls. Audit D3DX calls and dynamically loaded native libraries individually: direct external platform calls may remain under the boundary; project-owned D3DX adapters/translated behavior must be Rust.

**Exit:** every supported D3D8 method has a Rust implementation and ABI/behavior test or an unchanged documented unsupported result; no C++ COM object or resource owner remains; D3D8/DXVK/native compatibility rendering accepted.

### P06 — Complete Nebula renderer and shared effects [LEAD]

Sources: `core/nebula_renderer.*`, `nebula_emissive.*`, every included `core/renderer_*.inl` and related header implementation, `renderer/cloak/`, `renderer/textures/`, `renderer/shaders/`, `renderer/state/`, `core/builtin_nebula_renderer.cpp` and the NebulaRenderer feature directory.

Port the actual hook callback bodies, render-pass sequencing, mesh/vertex/index-buffer paths, skinning/lighting, emissive/specular maps and bloom, cloak/refraction/compositing, decals, texture suffix/format/variant caches, shader parsing/translation/cache, fixed-function state capture/restoration and draw-depth/reentry tracking. Move all graphics/global/TLS state and COM objects into Rust. Preserve failed-upload state and precise failure/release ordering already found by earlier counterexamples.

**Exit:** no native renderer implementation or shared COM owner remains, effects/settings paths work after a full restart, and visible emissive textures, cloaking, shields, UI and world passes match their accepted baseline. A settings flag or successful headless fake-device test alone cannot close visual acceptance.

### P07 — Complete hooks, engine ABI and precision-sensitive operations [LEAD]

Reuse `rust/a2fo_bridges/manifest.json`, its 48 Rust bridge families, `rust/a2fo_core/src/native_hooks.rs` and existing FP/native patch evidence. Audit all remaining compiler-generated fastcall/thiscall adapters, Delphi register conventions, callback bodies, function pointers, IAT/vtable slots, dynamically emitted machine code and engine-memory readers/writers across **all** feature/core/MMM sources.

Replace native executable operations and callback implementation in Rust; keep necessary exact instructions in Rust assembly. Verify stack cleanup/alignment, registers/flags, x87 stack/control word/rounding/precision, SSE/MXCSR where used, return conventions, TLS/reentry, symbol decoration, relocations and unwind assumptions. Preserve exact supported engine-image hashes, checked byte signatures, trampoline instruction boundaries, patch publication/rollback/protection/cache-flush rules and unsupported-image refusal. Do not transplant historical addresses into another engine build.

**Exit:** zero production `.S/.asm` implementations or C++ callback bodies; all supported call boundaries and patches have source-to-object evidence plus executable x86 fixtures; numeric branch boundaries match across required FP modes. Existing object-byte parity does not by itself verify new Rust callers or foreign unwinding.

### P08 — Complete all 48 feature modules [LEAD/MEDIUM by slice]

The matrix in section 5 is mandatory. For each row, migrate its **whole implementation**, not just named policy functions. Include native `.cpp`, executable header/`.inl` bodies, built-in wrapper, standalone initialization/exports, shared services, callback/state/resource ownership and runtime cleanup. Cross-family consumers in FeaturePack, HybridBuild, RefitYards, Resources, BuildTooltips, Squadrons and identity/rendering features must remain compatible while ownership moves.

Feature closure requires source audit; Rust implementation; built-in and standalone parity; malformed/missing config/data behavior; game start/restart/map transition/object destruction; serialization/network determinism where relevant; removal of final native consumers; and manual acceptance of its visible/gameplay effect. The two bounded owners are reuse/review items, not permission to leave their entrypoints C++.

Preserve observable read order, container iteration/order, identity/key rules and mutation timing. A Rust snapshot or cached field read can change behavior when an engine call or callback mutates the source between operations. Test those mutations, partial failures and floating-point branch boundaries explicitly; avoid letting randomized map iteration or different arithmetic order alter simulation/network results.

## 5. Per-feature completion matrix

All rows also require the universal P08 closure checklist above. “Bounded owner” means only the recorded state slice is credited. “Audit/finish” means the source still needs full authoritative ownership analysis and remaining implementation conversion; it does **not** mean there is no existing Rust logic.

| ID | Family / source root | Recorded state | Remaining implementation focus |
| --- | --- | --- | --- |
| F01 | [A2FOBuildTooltips](../modules/A2FOBuildTooltips/) | Audit/finish | Build-time text, native tooltip/UI hooks and Resources service calls. |
| F02 | [A2FONormalWeaponTech](../modules/A2FONormalWeaponTech/) | Bounded owner; finish | Reuse 7-field Rust owner; finish native init/context/error/ABI wrappers and both load paths. |
| F03 | [A2FOWreckage](../modules/A2FOWreckage/) | Audit/finish | Wreckage configuration, object/event hooks and retained native lifetime state. |
| F04 | [A2FOInfoIni](../modules/A2FOInfoIni/) | Audit/finish | info.ini lookup/defaults/settings service, native buffers and callback lifetimes. |
| F05 | [A2FOMuzzleFlashes](../modules/A2FOMuzzleFlashes/) | Audit/finish | Weapon/fire-event hooks, hardpoint/native object access and effect lifetime. |
| F06 | [A2FOTeamChangeWeapons](../modules/A2FOTeamChangeWeapons/) | Audit/finish | Team-change ODF/weapon lookup and engine-facing event state. |
| F07 | [A2FOWeaponDamageControls](../modules/A2FOWeaponDamageControls/) | Audit/finish | Hull/shield damage metadata, weapon ODF access and native hook dispatch. |
| F08 | [A1Fallbacks](../modules/A1Fallbacks/) | Audit/finish | Missing-only GUI/wireframe fallback, asset lookup and native GUI state. |
| F09 | [A2FOAnimatedHardpoints](../modules/A2FOAnimatedHardpoints/) | Audit/finish | SOD matrix/animation reads, hardpoint queries and hook/object lifetime. |
| F10 | [A2FOCameraControls](../modules/A2FOCameraControls/) | Audit/finish | Camera input admission, pan/drag configuration and native camera state/ABI. |
| F11 | [A2FOViewDistance](../modules/A2FOViewDistance/) | Audit/finish | Far-plane/culling updates and engine access; preserve FP environment and thresholds. |
| F12 | [A2FOInstantActionSettings](../modules/A2FOInstantActionSettings/) | Audit/finish | Load Settings control bounds, setup details, restore/save behavior and UI hooks. |
| F13 | [A2FOEditMenu](../modules/A2FOEditMenu/) | Audit/finish | ODF menu hierarchy, native menu creation/resources and editor interactions. |
| F14 | [A2FOM3D](../modules/A2FOM3D/) | Audit/finish | M3D-to-SOD conversion integration, file/cache ownership, generated-cache markers and cleanup. |
| F15 | [A2FOBreeders](../modules/A2FOBreeders/) | Audit/finish | Breeder classLabel/Producer alias, native class registration and lifecycle. |
| F16 | [A2FOInfestation](../modules/A2FOInfestation/) | Audit/finish | Delayed infestation state, native weapon callbacks, target/object invalidation. |
| F17 | [A2FOSwarmSystem](../modules/A2FOSwarmSystem/) | Audit/finish | Motion policy integration, visual-traffic native objects/configuration and cleanup. |
| F18 | [A2FOODFVariants](../modules/A2FOODFVariants/) | Audit/finish | Ownership/capture ODF variants, native class/ODF references and transition state. |
| F19 | [A2FOFireArcs](../modules/A2FOFireArcs/) | Audit/finish | Arc/config/metadata ownership and native fire decisions, hook/FP compatibility. |
| F20 | [A2FOAnimations](../modules/A2FOAnimations/) | Audit/finish | Source-frame/class/weapon metadata, playback tracking and object/event cleanup. |
| F21 | [A2FOAlwaysShowShields](../modules/A2FOAlwaysShowShields/) | Audit/finish | Visibility policy integration, shield/class metadata and render callbacks. |
| F22 | [A2FOStationRotation](../modules/A2FOStationRotation/) | Bounded owner; finish | Reuse 24-field Rust owner; finish native descriptor/access/hook wrappers and standalone entry. |
| F23 | [A2FORadarDiagnostics](../modules/A2FORadarDiagnostics/) | Audit/finish | Opt-in capture/configuration, native diagnostic reads and output lifetime. |
| F24 | [A2FORecoveryTests](../modules/A2FORecoveryTests/) | Audit/finish | Opt-in recovery diagnostics, signatures/native probes and precision-safe hook entry. |
| F25 | [A2FOCheats](../modules/A2FOCheats/) | Audit/finish | Signature-checked cheat hooks, configuration/activation and engine access. |
| F26 | [A2FOCollectiveXP](../modules/A2FOCollectiveXP/) | Audit/finish | Faction/class/unit/identity maps, counters, native pointers, XP events and deterministic FP. |
| F27 | [A2FODirectionalShields](../modules/A2FODirectionalShields/) | Audit/finish | Facing/layout/hit/visibility state, native shield objects and renderer/UI consumers. |
| F28 | [A2FOEnergySystems](../modules/A2FOEnergySystems/) | Audit/finish | Per-craft torpedo/quantum/shuttle stores, resupply metadata and object destruction. |
| F29 | [A2FOHybridBuild](../modules/A2FOHybridBuild/) | Audit/finish | Queue/config/production runtime, submenu and Delphi UI bridges, cross-module services. |
| F30 | [A2FOMultiYardBuild](../modules/A2FOMultiYardBuild/) | Audit/finish | Synchronized build-order selection/dispatch, yard references and multiplayer ordering. |
| F31 | [A2FONebulaRenderer](../modules/A2FONebulaRenderer/) | Audit/finish | Feature admission/art suffix state plus full shared renderer closure under P04-P06. |
| F32 | [A2FOPointDefenseCycles](../modules/A2FOPointDefenseCycles/) | Audit/finish | Multi-shot firing schedules/cycles, native weapon references and timing. |
| F33 | [A2FORefitYards](../modules/A2FORefitYards/) | Audit/finish | Refit queue/policy, docking transforms, synchronized transitions and queue-service ABI. |
| F34 | [A2FOTurrets](../modules/A2FOTurrets/) | Audit/finish | Linked native turret/craft objects, combat/math integration and destruction ordering. |
| F35 | [A2FORGBTextures](../modules/A2FORGBTextures/) | Audit/finish | Legacy RGB texture reads/uploads, resource cache/ownership and failure paths. |
| F36 | [A2FOTextureVariants](../modules/A2FOTextureVariants/) | Audit/finish | Faction/owner-aware textures/SOD nodes, caches and capture/owner transitions. |
| F37 | [A1Compat](../modules/A1Compat/) | Audit/finish | Entire large native module: BZN/physics/UI/race/team colors, native globals, menu/render resources and all hooks. |
| F38 | [A2FOCraftIdentity](../modules/A2FOCraftIdentity/) | Audit/finish | Selection/identity ODF lists, object-editor state, shield displays, icons and panel resources. |
| F39 | [A2FOFeaturePack](../modules/A2FOFeaturePack/) | Audit/finish | All grouped dispatchers, buildyard trees/technology/tooltips, recursive ODF, queues, upgrade pods and video/service clients. |
| F40 | [A2FOLogMailer](../modules/A2FOLogMailer/) | Audit/finish | Consent/config/report/spool/module state plus native helper and any supported relay under P14. |
| F41 | [A2FOMissionSelector](../modules/A2FOMissionSelector/) | Audit/finish | Mission discovery/selection/buttons/scrolling, native UI objects and activation. |
| F42 | [A2FOMissions](../modules/A2FOMissions/) | Audit/finish | Native Worker/Rule/load lifecycle and complete MMM VM/API/archive/save replacement under P09-P12. |
| F43 | [A2FORefitMenus](../modules/A2FORefitMenus/) | Audit/finish | UI-thread Session/shared ownership, HWND/timers/messages, skins/fonts, setup/loading GPU resources and scene handoffs. |
| F44 | [A2FOResources](../modules/A2FOResources/) | Audit/finish | Four additional resource pools, native team/resource storage, economy hooks and dependent services. |
| F45 | [A2FOSOLMaps](../modules/A2FOSOLMaps/) | Audit/finish | SOL parsing/discovery integration, placed-object construction, map/engine state and transition cleanup. |
| F46 | [A2FOShellDisplay](../modules/A2FOShellDisplay/) | Audit/finish | Window/canvas/presentation, Bink/native-intro clients, UI/image/video resource lifetimes. |
| F47 | [A2FOSquadrons](../modules/A2FOSquadrons/) | Audit/finish | Registry/configuration, squadron state, native economics/pod selection/repair and linked-object cleanup. |
| F48 | [A2FOVideo](../modules/A2FOVideo/) | Audit/finish | Native hook admission, DLL/API/session/window/settings/TLS state and quiescence around Rust media under P13. |

## 6. Mission-system replacement

### P09 — MMM Worker, Rule and mission lifecycle [LEAD]

Sources: `modules/A2FOMissions/module.cpp`, `core/builtin_missions.cpp`, `missions/mmm_loader/`, `rust/a2fo_mmm_loader/`, `rust/a2fo_core/src/mmm_runtime.rs`, `third_party/mmm/source/mmm_loader/Worker.*`, `RuleHelper.*` and dependent mission services.

Replace native Worker/Rule ownership, construction/start/resume/update/destruction, map-title script startup, rule creation/registration, input-handler registration, mission completion and transition cleanup. Reuse the Rust loader/handoff implementation. Preserve engine-thread requirements, deferred first-input behavior, reload and lifetime semantics. Resolve the explicitly deferred **Jan_B reload-rule callback** and distinguish simulated input from actual save-resume callback re-registration. Address process-lifetime rule allocations through an explicit safe lifetime contract rather than casually freeing engine-retained pointers.

**Exit:** no native Worker/Rule or C++ mission entry wrapper; actual new/restarted/resumed/completed missions work, repeated transitions do not retain invalid callbacks, and load/export behavior matches each installed loader search location.

### P10 — Pure-Rust Lua-compatible VM and standard libraries [LEAD]

Replace `third_party/mmm/source/external/lua/` production C implementation. Establish the complete required Lua 5.1 compatibility contract from source, all shipped scripts, archives and public MMM APIs: lexical/syntax behavior; numeric conversions/formatting/rounding; strings/bytes/patterns; tables/metatables; closures/upvalues/environments; standard libraries; errors and protected calls; coroutine/yield boundaries; GC/finalization/weak references; module/include/package search; bytecode loading/version/format and any binary chunks actually supported; C-facing varargs adapters and allocator behavior where still used.

Choose or implement a Rust VM only after differential compatibility tests. The investigated Rulua prototype is **not** a selected/complete replacement: it retains a C auxiliary library, varargs bridge, x86 setjmp/longjmp and native allocator dependencies, with known comparison differences. Those must be removed or replaced, and differences resolved against the required compatibility contract. A Rust wrapper around Lua C, LuaJIT or another native VM fails purity.

**Exit:** no C VM/auxiliary/setjmp implementation in production; all required language/library/error/coroutine/GC behavior passes old/new tests on the real x86 target and required mission scripts run unchanged. Unsupported legacy behavior cannot be silently dropped to pass the gate.

### P11 — Replace Luabind, mission APIs and runtime services [LEAD]

Sources: `third_party/mmm/source/mmm_armada/`, especially `LuaBinding.cpp`, every `*Binding.cpp`, `Globals.cpp`, application/timer/monitor/hook/UI/text-input/media/debugger services, `mmm_common/`, `mmm_database/` and `external/luabind/`/Boost consumers.

Generate a complete mission-facing API inventory: names/constants, classes/inheritance, overloads/default arguments, type conversions, userdata identity, borrowed/shared ownership, entity references and invalidation, callbacks/coroutines, exceptions/errors and destruction order. Port both bindings **and the implementations behind them**. Preserve entity lookup/mutation, world/map/object/player operations, timers/conditions/triggers, mission UI/input/text/audio/video, debugger/diagnostics and every additional API discovered in registration/source tracing. Implement direct verified Rust engine calls without exporting STL, native shared-pointer or Rust internal container layouts across the ABI.

**Exit:** API inventory has zero missing registrations/implementations, script and lifetime differential tests pass, no Luabind/Boost/native mission-runtime consumer remains, and old scripts continue using existing API contracts.

### P12 — Mission archive, discovery and save-format compatibility [LEAD]

Sources: `mmm_luapack/`, `mmm_common/{ModFolder,FilePath}.*`, recursive include logic, `mmm_armada/{Save_Internal,Load_Internal}.*`, persistence bindings and zlib/minizip consumers.

Replace ZIP/mmmpack discovery/loading/storage/compression with pure-Rust implementations. Pin the accepted archive methods/flags, entry naming/case/path separators, folder precedence, duplicates, byte lengths, corrupt/truncated archive behavior and existing campaign pack compatibility. Preserve map-title lookup, script includes, mod/parent search and missing-file diagnostics. If supported pack-writing tools ship, replace their write paths too.

Preserve the existing entity-backed save wire format, including NUL-terminated strings, float/bool representation, 125-byte packet structure, zero-byte mapper encoding, limits and ordering. Establish this from exact production sources and real fixtures; the listed properties are audit leads, not a substitute for a full encoding specification. Cover graph/reference reconstruction, callbacks/timers/rules, mission globals, entity deletion and stale-reference behavior. Perform old-write/new-read and new-write/old-read tests where the baseline supports them, plus actual game saves/resumes. VM snapshots alone do not replace this format.

**Exit:** existing archives/scripts/saves work unchanged; no native pack/zlib/minizip implementation; full script/API/worker save-resume behavior accepted, including the Jan_B registration path.

## 7. Media, companion programs and secondary artifacts

### P13 — Finish Rust media integration and format closure [LEAD/MEDIUM]

The decoder/player backend in `rust/a2fo_media/` and pinned `third_party/rust-media/` is already Rust; retain it. Port remaining `modules/A2FOVideo/module.cpp`, FeaturePack/ShellDisplay video clients, engine window/settings/name lookup, hook admission, dynamic DLL/API handling, TLS recursion state and shutdown quiescence to Rust. Preserve the versioned C table, session IDs, synchronous-callback lifetimes, frame/audio queue bounds, seek/drain/cancellation behavior and allocation ownership across DLLs.

Inventory movie/audio inputs in **all supported installations/mods**, not just one converted intro: container/codec/profile/pixel range/matrix/chroma/audio layout/rate, silent/short/truncated assets, native BIK/Bink requests and lookup precedence. The current backend deliberately supports a documented subset of former FFmpeg capabilities. Verify every required modern-media format against Rust implementations; add missing required formats with pure-Rust code. Never reintroduce FFmpeg/native codec bindings as a shortcut. Keep external engine Bink use explicitly visible in the boundary inventory.

Audit all codec/container and target-specific transitive crates/build scripts, emitted CPU features, licensing and compiler runtime members. Test scalar/SSE2 equivalence, odd extents, seeking both tracks, EOF drains, shutdown/reentry/open races, driver-release failure retention and repeated starts. Close visible presentation, audible sound, real-device timing, skip/close/window transitions and A/V sync manually; headless decoded frames cannot close those items.

**Exit:** no native A2FO video integration code, no native decoder dependency, all actual required assets supported or explicitly routed to the existing engine service, and real playback acceptance passed.

### P14 — Port startup proxy and companion executables [LEAD/MEDIUM]

Convert `core/startup_proxy.cpp` → supported `Win2kDisableTaskSwitch.dll`, preserving forwarded/exported entrypoints, DLL lookup, compatibility checks, process startup and failure behavior. An old C++ proxy left in an existing installation fails product closure even when an update ZIP does not contain it.

Convert `tools/a2fo_renderer_helper.cpp` and **its whole linked renderer graph** → `A2FORendererHelper.exe`. Preserve process/command-line/IPC/version/lifecycle behavior, renderer options, real presentation and parent shutdown. Replacing only `main()` leaves native rendering behind.

Convert `modules/A2FOLogMailer/helper.cpp` and executable header/shared implementations → `A2FOLogMailerHelper.exe`; convert the module side through F40. Preserve consent/defaults, report collection/redaction, spool/status/error formats, process invocation, network/TLS/proxy/timeouts/cancellation and shutdown. Classify `relay.py`: if supported as a deployed reporting service, port its implementation to Rust too; if historical development infrastructure, record evidence of exclusion. Do not send reports or contact recipients during migration tests without separate explicit authorization. Use local inert fixtures.

Audit `tools/a2fo_telemetry.cpp`, SDK `ExampleModule`, and any other supported shipped authoring/diagnostic program. Port supported executable code; keep historical examples only under an explicit nonproduction reference classification. Resolve absent ArcLab/IconLab/TooltipLab/UIEditor sources and build-rule references through P00; do not claim those programs are complete or silently remove supported functionality.

**Exit:** every supported proxy/helper/tool/service is Rust, its real dependencies are audited, exports/IPC/file formats match, and clean packaging includes the matching versions.

### P15 — Verify Rust sidecars, scene provider and all standalone variants [MEDIUM/LEAD for ABI]

Audit the existing Rust scene provider `third_party/armada2-refit/menu-renderer/`, its format-reader subcrate, selected `a2fo-menu-host` features, build script, embedded resources, allocator/memory intrinsics, runtime/link flags and target-specific dependencies. Preserve its versioned host/scene API, initialization/foreign-thread guards, device/window ownership and repeated init/shutdown behavior. No rewrite credit is needed for existing Rust source, but a pure dependency/build-origin check is still required.

Audit the separately built MMM loader and media DLL in the same way. For every feature DLL, verify the standalone build has moved off C++ entrypoints and shared native objects; a Rust built-in does not convert its standalone counterpart automatically. Preserve names, ordinals, calling conventions, SDK negotiation, discovery, DLL search precedence and per-binary allocation boundaries. Check for duplicate module activation and old module shadowing in base/parent/mod directories.

**Exit:** clean Rust builds and compatibility receipts for every supported artifact, no native sidecar hidden outside the main-DLL graph, and explicit package/install ownership of each version.

## 8. Dependency, build and release closure

### P16 — Remove every remaining native implementation dependency [LEAD]

Drive removal from final consumers, including all C/C++ TUs in the annex, included `.cpp/.inl`/header bodies, Lua, Luabind, Boost consumers, zlib/minizip, native runtime/startup/allocator/thread/exception helpers, and any newly discovered library. FFmpeg is already removed from production, but verify that remains true for every optional and standalone path. Preserve its old comparison fixtures as excluded references.

Audit every Cargo workspace, direct rustc invocation, lockfile, feature combination and target-specific normal/build dependency, including refit and vendored codec subprojects. Inspect `build.rs`, `cc`/`cmake`/`pkg-config`, `*-sys` crates, prebuilt blobs, `include_bytes!`, generated native objects and dynamic-library fallback. A crate name or “Rust bindings” label does not establish implementation purity. Generate a provenance/license inventory and source-to-artifact dependency record. Review current advisories as a separate dated dependency-quality check; an advisory result neither proves nor disproves language purity.

Eliminate A2FO runtime dependence on native C++/C implementation archives. Distinguish true external OS imports, import-thunk data and unavoidable compiler-generated ABI glue from retained native runtime implementations, with individual evidence. If a compiler support routine has native implementation provenance, replace it or use a verified Rust alternative before closing; do not waive it by renaming the library. Preserve `.rc/.res` and shaders as resources where applicable.

**Exit:** zero retained native implementation consumers, zero unclassified native members/dependencies, complete source/provenance map and no runtime C/C++ build step.

### P17 — Make the clean-build and purity checks enforceable [MEDIUM; LEAD reviews]

Replace mixed-language production rules and link manifests with Rust-owned targets. Remove `a2fo_native_*` callbacks only once Rust owns their behavior. Keep export-generation, resource and shader steps where needed; isolate reference C++ test builds so they cannot enter release artifacts. Keep immutable historical checkpoints; do not rewrite old receipts to describe new binaries.

Add automated release checks with negative controls:

1. Build every supported production target from an isolated source checkout with empty caches/output and pinned locked dependencies. Disable C/C++ implementation compilers and access to old native object/archive caches. Record any linker/toolchain invocation separately; no reused object escape hatch.
2. Record complete actual linker arguments, maps, archive members selected, build-script outputs, dynamically loaded product dependencies and origin hashes for each DLL/EXE. Import-table scans alone miss statically linked C++.
3. Reject any nonallowlisted native implementation source/object/archive, bundled native VM/codec/runtime, production reference fixture, unclassified generated blob or unsupported dependency. Prove rejection by injecting representative forbidden inputs into a private check fixture.
4. Verify main 197 exports/ordinals/forwarders, every auxiliary contract and supported engine hash profile. Compare headers and ABI layouts without relying solely on symbol names.
5. Verify exact package-to-artifact hashes, filenames, resource paths, case-insensitive collisions, module search precedence and absence of stale shipped binaries. A DLL file extension or Rust `DllMain` symbol is not a purity test.

**Exit:** a repeatable clean build and a fail-closed purity verifier cover every artifact/variant, with an independent review of the evidence. Toolchain-origin unknowns must be resolved before this passes.

### P18 — Combined regression and real in-game acceptance [LEAD coordinates]

Refresh the combined suite after the final implementation changes: Rust unit/integration tests; parsers/config/format edge cases; x86 ABI/assembly; FP precision; checked patches; module/callback mutation; allocation/API failures; ownership/refcount ordering; renderer state/shaders/textures; MMM API/VM/archive/save compatibility; media decode/audio/clock/seek/shutdown; helper IPC; export/import/build-origin/package gates. Preserve and compare meaningful baseline behavior. Fix existing relevant test/lint failures; do not relabel failures or skip entire historical groups to get a green total.

Then manually test the final matched binaries in the user-selected **Fleet Ops Roots**, covering **fo test and test** with their actual parent chains and required original-format content. Use a backed-up test copy first. This plan authorizes report creation, not a new installation/deployment or automated gameplay. No GUI/input automation.

Required manual matrix:

- Startup/proxy/module discovery and selection, settings save/restart, missing optional support, clean logs and shutdown. Confirm mailer consent and absence of missing-helper popup without sending mail.
- Real renderer/D3D8 compatibility/D3D11 paths that the product supports, graphics options, emissive maps/bloom/specular, shield/cloak effects, menus/loading/refit 3D scenes, alt-tab/resize/fullscreen/device recovery where supported, and extended play/resource cleanup.
- Each enabled feature's actual gameplay/UI effect from F01–F48, A1 compatibility and SOL maps, build/refit/research queues, resources/squadrons/identity and object lifecycle. Include interactions, not only individual toggles.
- Existing campaigns/tutorials/Premonitions, mission selection/new start/restart/completion, scripted UI/input/timers/media, multiple save/load cycles, old saves and restored callbacks/rules/references.
- Actual installed movies with audible sound, visible frames, A/V sync, skip, consecutive modern/native-engine playback and shutdown during playback.
- Deterministic gameplay, matched-build multiplayer join/start/save-load where supported, long session and desync behavior with at least two real peers. This is currently pending because a second peer is not available; headless tests do not close it.

Record exact engine/mod/config/artifact hashes, test actions, observed result, fresh logs and unresolved faults. The user's earlier quick test used an older main DLL; phase-18 packaging/config repairs and subsequent DLL installation do not establish acceptance of the current Rust candidate.

**Exit:** source/build purity passes **and** all required functional tests pass on the final package. A final source change invalidates affected acceptance and requires rerun. No deployment-ready claim with a required gate pending.

### P19 — Final package, installation audit and completion certificate [LEAD]

Build the release from the proven clean graph. Include all Rust artifacts, support resources/config examples and required notices/provenance. Make an upgrade manifest for existing installations and base/parent/mod loader locations, with backup/rollback and stale-binary detection. Preserve user settings, assets, saves, logs and mail consent. Do not copy original game executables/assets into a redistributable update unnecessarily.

Audit the installed product as well as the ZIP: old proxy/helpers/standalone DLLs/loaders left outside an update payload can preserve a native implementation. User-authorized installation must be hash-verified and reversible. Verify the post-install state and rerun required smoke tests; keep multiplayer pending until actually performed.

Publish a completion certificate containing the supported product manifest, source revision/snapshot, dependency/runtime allowlist, all artifact hashes, origin-clean build receipts, export/ABI contracts, source-to-Rust replacement ledger, regression/manual results and known support limits. Require **zero unresolved P00–P19 gates, zero incomplete feature rows, zero production native implementation and zero required acceptance failures**. Only then describe the rewrite as 100% Rust and fully accepted.

## 9. Recommended execution order

1. **P00**, then resolve the target/runtime/foreign-exception strategy in **P01/P03/P07** before committing to a foundation that needs C++ indefinitely.
2. Finish shared core/service/ownership foundations **P01–P03**. Reuse the existing Rust owners and their regressions.
3. Complete graphics **P04–P06**, while migrating independent features from **P08** in bounded batches. Keep shared contracts and unsafe ABI decisions with the lead.
4. Complete the independent MMM workstream **P09–P12**, beginning with the VM/binding compatibility inventory and save-format fixtures. Do not defer VM viability until after writing all bindings.
5. Finish media integration/helpers/secondary artifacts **P13–P15**, coordinating their shared rendering and module dependencies.
6. Retire final consumers and make clean-build checks exhaustive **P16–P17**. Apply those checks progressively during every batch, not only at the end.
7. Run final combined and real-game acceptance **P18**, then package/install/certify **P19**.

These are dependency groups, not an effort estimate or a promise that each is one short change. Unsafe hooks, memory layouts, concurrency, engine compatibility and foreign exception handling remain lead work. Bounded policy, documentation, manifest generation and well-specified conversions can be delegated with disjoint ownership and independent review.

## 10. Final yes/no checklist

- [ ] P00: all product artifacts, sources, variants and dependencies classified; no unknown implementation.
- [ ] P01–P03: startup, shared state, SDK services, platform/UI/error handling entirely Rust.
- [ ] P04–P06: all A2FO rendering/compatibility/resources entirely Rust.
- [ ] P07: all hook bodies/bridges/patches/precision operations Rust with verified x86 compatibility.
- [ ] P08/F01–F48: every complete feature implementation and supported standalone path Rust.
- [ ] P09–P12: complete MMM framework, compatible VM/bindings/archives/save-load Rust.
- [ ] P13: Rust media integration and required-format/playback acceptance complete.
- [ ] P14–P15: proxies, helpers, supported utilities/services and all secondary DLLs Rust.
- [ ] P16: no bundled native implementation or unresolved runtime/dependency provenance.
- [ ] P17: clean build plus negative-tested artifact-origin purity checks pass for every product target.
- [ ] P18: final combined regressions and all required manual tests, including multiplayer, pass.
- [ ] P19: final package and installed-state audit match the proven artifacts; completion certificate issued.

**Working through all items, including the zero-unknown final build-origin gate, is the definition of completion. The current portable census and a successful mixed-language DLL build cannot substitute for it.**

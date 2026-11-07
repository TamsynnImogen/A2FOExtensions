# Removing the startup and Bink DLLs: viability assessment

Date: 29 October 2026. Scope: static analysis of the current Fleet Ops Roots installation and A2FO source, with upstream decoder research. **No game launch, input automation, binary modification, installation or removal was performed.** Generated audit files are separate from production sources and installed files.

## Verdict

**Both roles can plausibly move into A2FOExtensions.dll. Neither DLL can simply be deleted from the current installation.**

| Component | Viability | Main work required |
| --- | --- | --- |
| `Win2kDisableTaskSwitch.original.dll` | Strong candidate for removal first | Replace the observed startup duties and two legacy exports; stop forwarding/loading the original. |
| `Win2kDisableTaskSwitch.dll` proxy | Feasible with a changed bootstrapping contract | Make Armada load A2FO directly, preserve FleetOpsHook-before-feature activation, and remove static and dynamic dependencies on the proxy filename. |
| `binkw32.dll` | Technically plausible; substantially larger and not yet demonstrated | Rust Bink decoding/audio plus the engine-facing playback/frame API, all consumers, and import redirection in both Armada and FleetOpsHook. |

These are engineering viability judgements, not verified replacements. The startup role is much smaller than Bink. The preferred endpoint uses only `A2FOExtensions.dll` and `FleetOpsHook.dll` for these roles, with no replacement DLL kept under a legacy filename. Windows/driver DLLs remain external platform dependencies.

The previous [pure-Rust completion plan](pure-rust-completion-plan.md) allowed Bink as an external engine playback service. **The new consolidation objective removes that allowance:** Bink playback must move into Rust before its DLL can disappear. Rust language purity and reducing the number of DLLs are separate acceptance criteria.

## Evidence and its limits

The inspected primary pair is:

- `Data/ArmadaL.exe`: SHA-256 `4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
- `FleetOpsHook.dll`: SHA-256 `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`.
- Current `Data/A2FOExtensions.dll`: SHA-256 `9a019cb01a955a26a8884a0578bc01d0ed817b433a19abd794f7e228aa6f2013`.
- Stock renamed startup DLL: SHA-256 `db6071ae53ed0e5ca2855ba729b763ec7b68eba8c2cae95f61e3f7199548b035`.
- Installed Bink DLL: SHA-256 `9c2b75defd3bf13737fdb6e8b7cd3b67d7b2d6fe0bbf48ec6dc2ecf1b03e559b`.

The [audit JSON](../build/dll-consolidation-audit-20261029/binary-and-source-evidence.json) records PE imports/exports, binary/source hashes, normal-import consumers found in the Data tree, loose-movie headers and bounded disassembly excerpts. [The extractor](../build/dll-consolidation-audit-20261029/inspect_inputs.py) is reproducible. This is a source/header/import audit, not a dynamic execution trace, complete archive scan, tested decoder or proof that every branch is used.

## 1. The loader dependency has to change

The primary Armada executable statically imports:

- `Win2kDisableTaskSwitch.dll`: two named exports.
- `binkw32.dll`: ten named exports.

FleetOpsHook statically imports Bink through **two descriptors**, `Binkw32.dll` and `binkw32.dll`, containing eight and one imports respectively. The nine names are a subset of Armada's ten. These are normal imports, not delay imports, in the inspected engine images.

Consequently, moving implementations into A2FO does not make the loader search A2FO for those functions. The requesting image still names the old DLL. Windows resolves required load-time dependencies as part of process/library loading; a missing required DLL fails loading. See Microsoft's [load-time linking contract](https://learn.microsoft.com/en-us/windows/win32/dlls/load-time-dynamic-linking).

An IAT hook installed after normal loading is too late to make a missing static dependency optional. A DLL forwarder or a hardlink/symlink under the old name would preserve a named compatibility file; it would not meet the stated two-DLL endpoint. Embedding/loading the original native binaries also would not achieve the pure-Rust objective.

### Preferred direction: checked import and startup migration

Use a version-specific, reversible installer/patcher to redirect the relevant import descriptors to A2FO, preserving existing IAT slots and caller ABI. Add the compatibility exports to A2FO. Check all dynamically constructed loads/lookups too. This is a future implementation proposal; no imports have been changed by this assessment.

For Win2k removal this requires an Armada change. For complete Bink removal it requires **both Armada and FleetOpsHook changes**, plus each supported alternative executable with those dependencies. Bink's shorter DLL-name storage cannot blindly be overwritten by the longer A2FO name: use a valid existing name location or correctly rebuilt/extended PE data. Preserve table terminators, alignment, relocations, resources, overlays and image compatibility; verify exact originals and reject unknown binaries.

If both engine images must remain byte-for-byte unchanged, physical deletion is **not achievable through their current standard loading contract**. A different launcher/custom-loader mechanism would be a separate architectural project, with additional maintenance and launch-path coverage. It is not the recommended first approach.

## 2. Win2k startup takeover

The current chain is:

```text
Armada normal import
  -> Win2kDisableTaskSwitch.dll       [A2FO proxy]
       -> Win2kDisableTaskSwitch.original.dll
            -> ..\FleetOpsHook.dll
       -> A2FOExtensions.dll          [early attach, then deferred work]
```

Sources: [startup proxy](../core/startup_proxy.cpp), [proxy exports](../core/startup_proxy.def), [recorded original startup](addresses.md#stock-fleet-ops-startup-load).

The preserved original has only two exports:

```text
?SetHookID@@YAXPAUHHOOK__@@@Z
?LowLevelKeyboardProc@@YGJHIJ@Z
```

In this exact original, their bodies are minimal stubs: SetHookID returns immediately, and LowLevelKeyboardProc sets EAX to zero and returns. Its entry path loads `..\FleetOpsHook.dll`. This makes the *observed game-specific role* much smaller than implementing a general task-switch prevention library.

Do not infer calling convention solely from the decorated name. The observed low-level-procedure stub ends in plain `ret`; its expected callback signature suggests a different stack-cleanup contract. The inspected Armada keyboard setup paths also contain unconditional branches around the old setup. These findings need caller/ABI proof before reusing or activating those exports; they do not justify adding input hooks or assuming every version has the same behavior. This assessment did not execute any keyboard/input path.

There is another `Win2kDisableTaskSwitch.dll` string in Armada outside the import-name entry, referenced by the old `LoadLibraryA` keyboard-setup paths. Even if those branches are currently bypassed, the migration must classify/rewrite or prove exclusion of that dependency for each supported profile. Changing the import descriptor alone is not the complete audit.

### Required startup design

1. Separate A2FO's minimal DLL attachment from engine-dependent activation. The current `a2fo_native_process_attach_step` and Rust entry assume FleetOpsHook is already available for several early hooks; deferred initialization must not race ahead with an absent/uninitialized FleetOpsHook.
2. Arrange a deterministic, checked pre-game startup call that loads FleetOpsHook and then activates A2FO's required early hooks before Armada reaches the original initialization they intercept. One candidate is a pre-CRT executable-entry bootstrap calling a Rust A2FO startup export and then continuing to the preserved original entry. Exact register/stack/entry/relocation handling and compatibility need a prototype and tests.
3. Keep third-party module discovery, UI, decoding/device creation and heavy work deferred until safe. Preserve early versus deferred initialization guarantees and failure/retry policy.
4. Replace the original's load responsibility with explicit path resolution tied to the installation layout, rather than depending accidentally on the current working directory.
5. Implement the two required legacy entrypoints in Rust with validated behavior. Preserve all existing 197 A2FO exports and ordinals; append deliberate compatibility exports rather than letting automatic ordinal assignment reorder them.
6. Retire the original forwarding path, then the proxy filename/import path, in separate verified steps. A temporary Rust proxy is a migration stage, not the final two-DLL result.

### Important dependency-cycle constraint

After Bink imports are redirected, FleetOpsHook will depend on A2FO for Bink entrypoints. Do **not** also give A2FO an unconditional static dependency on FleetOpsHook to force the old initialization order; that creates a circular graph. Do not transplant the nested `LoadLibrary` chain unchanged into A2FO's DllMain. Microsoft's [DLL entry-point guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function) specifically warns against such loading there.

The proposed separation is: load/attach A2FO's minimal dependency layer, call an explicit bootstrap outside loader lock, load FleetOpsHook against A2FO's ready compatibility exports, then activate engine-dependent A2FO hooks at a verified point. Any Bink calls during FleetOpsHook attachment must be traced and supported safely; readiness cannot rely on a worker merely running “soon.” This ordering is a design candidate, not established working behavior.

## 3. Bink takeover

### The actual minimum imported API

The verified primary pair requires these ten distinct names:

| Export | Replacement responsibility to establish and preserve |
| --- | --- |
| `_BinkOpen@8` | Open/configuration flags, file/archive/memory input modes actually used, stable handle and metadata, decoder state, errors. |
| `_BinkClose@4` | Release sessions, decoder/audio resources, outstanding work and all buffers safely. |
| `_BinkDoFrame@4` | Decode the requested frame and audio, preserving observable result/error behavior. |
| `_BinkNextFrame@4` | Frame advancement, end-of-stream/loop behavior and public metadata updates. |
| `_BinkWait@4` | Compatible pacing/readiness and audio/video clock behavior. |
| `_BinkCopyToBuffer@28` | Requested pixel layout, pitch, height, offsets, flags, clipping/orientation/alpha where used, and return values. |
| `_BinkGoto@12` | Seeking/keyframe/audio-history behavior and state/clock reset. |
| `_BinkSetVolume@8` | Actual engine volume scale, per-session/track behavior and shared audio settings. |
| `_BinkSetSoundSystem@8` | Audio-provider/callback selection and associated data/lifetimes. |
| `_BinkOpenDirectSound@4` | Compatible provider entrypoint/device interaction; cannot merely return success without implementing the calling contract. |

The FCM and Orb of Time executables found in the same installation import 14 Bink functions each, adding five more names beyond the primary-pair set:

| Additional export | Additional compatibility work |
| --- | --- |
| `_BinkBufferBlit@12` | Buffer presentation/blit contract and its native window/surface interactions. |
| `_BinkBufferLock@4` | Buffer layout, writable mapping and lock lifetime. |
| `_BinkBufferUnlock@4` | Publication/unlock behavior and error/lifetime rules. |
| `_BinkDDSurfaceType@4` | DirectDraw surface-format identification and compatible format result. |
| `_BinkGetRects@8` | Changed-region reporting, rectangle layout and copy/blit behavior. |

The observed normal-import union is therefore **15 Bink names**, not ten, if these executable variants remain supported. These names are a **verified minimum**, not proof of the complete dynamically used API. The installed Bink DLL exports 73 names. Inventory ordinal/dynamic/third-party users before choosing the compatibility surface. Full general Bink DLL equivalence would require the rest of that surface too; game-specific removal requires every function and semantic actually reachable in the supported product.

The normal imports inspected here use names, not ordinals. Unknown ordinal consumers still need investigation. Combining libraries into one export namespace cannot preserve conflicting ordinal assignments automatically. Preserve A2FO's existing contract and retarget any confirmed legacy ordinal consumer explicitly. The 15 observed Bink names and two Win2k names do not currently exist in the installed A2FO export table.

### Why the current media port is insufficient

The Rust backend currently admits MP4/MOV-family and MKV/WebM-family containers, with documented H.264/VP9 video and AAC/Opus/Vorbis audio. [Container dispatch](../rust/a2fo_media/src/codec.rs) has no BIK path. [A2FOVideo](../modules/A2FOVideo/module.cpp) tries modern replacements, then falls back to the native engine/Bink path. That fallback still requires the DLL.

The complete installation scan found **438 loose `.bik` file occurrences**: 345 with `BIKf` magic and 93 with `BIKi`, and no loose `.bk2` files. These are raw header observations, not tested compatibility results or unique-asset counts. Archive-contained media and differently named assets are not covered.

Native Bink use extends beyond full-screen movies. [ShellDisplay](../modules/A2FOShellDisplay/bink_video.cpp) and [FeaturePack](../modules/A2FOFeaturePack/bink_video.cpp) wrap Fleet Ops D3D9 intro rendering and Armada GDI/texture movie paths. They still call native engine playback/render routines. [Native intro initialization](../modules/A2FOShellDisplay/native_intro.cpp) explicitly looks up `binkw32.dll` and `_BinkCopyToBuffer@28`, checks its identity against Armada's import, and reads width/height directly from the Bink handle.

Therefore the Rust replacement needs an engine-compatible frame API and verified public structure layout/lifetimes, not just a function that plays a file in its own window. Decode BIK frames/audio into Rust-owned sessions and feed the existing engine's buffer/texture consumers, or deliberately replace **every** higher-level movie consumer. The former is the stronger initial architecture candidate because it preserves those consumers' presentation behavior while moving decoding behind the established API. Exact internal field reads and flag/format semantics remain source/binary audit work.

Update the current ShellDisplay module-identity check and all other old-name lookups to recognize A2FO's new ownership. Audit menu backgrounds, looping/small UI videos, texture animations, mission/campaign movies, audio-bearing/silent files, seeking, skip, failures and transitions. Converting only startup videos or returning null/success stubs would drop behavior rather than replace it.

### Existing Rust decoder lead

The upstream [infinitier_bik_decoder documentation](https://docs.rs/infinitier_bik_decoder/latest/infinitier_bik_decoder/) advertises Rust decoding for `BIKf`/`BIKi` and other Bink 1 variants, including DCT/RDFT audio, YUV/RGBA output and optional alpha. It describes a Rust translation of FFmpeg codec code, rather than using FFmpeg bindings. This is useful evidence that implementation need not necessarily start from zero; it is not A2FO validation.

Its [crate manifest](https://raw.githubusercontent.com/ufoplay/infinitier/main/src/codecs/bik_decoder/Cargo.toml) inherits its license and uses a datasource dependency; the [workspace manifest](https://raw.githubusercontent.com/ufoplay/infinitier/main/Cargo.toml) declares `GPL-3.0-or-later`. Before selection, pin the actual source/version, audit licenses/provenance and all transitive native/build dependencies, inspect robustness, compile for the required x86 Windows target and compare decoded video/audio on the actual asset corpus. No dependency was added or candidate compiled by this assessment. A translated Rust implementation can satisfy the language goal; linked FFmpeg C code cannot.

Decoder availability does not supply the old Bink API, engine handle layout, pacing, DirectSound-provider semantics or window/texture integration. Those are separate A2FO implementation tasks. Current support for Bink 2 is neither demonstrated nor required by the loose-file observation alone; establish the full content/support inventory before deciding its scope.

## 4. Supported variants and distribution consequences

A Data-tree normal-import scan parsed 462 PE32 files and found target dependencies in:

- `Data/ArmadaL.exe`: Bink and Win2k.
- `Data/Mods/Fleet Ops 4.0/ArmadaL.exe`: Bink and Win2k.
- `Data/Mods/FCM/Armada.exe`: Bink.
- `Data/Mods/Orb of Time/Armada.exe`: Bink.

The separately inspected root `FleetOpsHook.dll` also imports Bink. The scan included historical files under Data and skipped twelve inputs not accepted by the PE32 parser; it is not an active-launch graph or a statement that all scanned tools need migration. Resolve actual launch selection, each executable's API/ABI/engine profile, skipped files, additional root/plugin/delay-loaded consumers and archive-backed assets before removal. Do not apply the primary-pair offsets to those other executables.

Modified import/startup data changes engine file hashes. Update narrowly scoped compatibility profiles and fixtures against known pristine and patched images, preserving unchanged code signatures where applicable. Never relax all engine validation to accommodate a patch. Package a checked local transformation with backups/rollback rather than assuming one prepatched proprietary executable suits every mod/build. The final package needs source-to-output receipts and an installation scan for old proxy/original/Bink files and callers.

There is also a distinction between retiring these DLLs and the eventual *entire product* having only two DLL files. Today's Rust media and scene provider are separate DLLs. They would need incorporation into the main Rust build, with allocator/panic/runtime/worker/resource isolation reconsidered, instead of being silently left as required sidecars. Other engine dependencies, renderer wrappers and helpers need their own future assessments. This document establishes no viability result for removing FMOD, D3DX, NetworkManager or every other library.

## 5. Recommended implementation sequence and exit gates

1. **Freeze contracts.** Record all supported engine/mod launch variants, imports, dynamic lookup/call sites, export/ordinal contracts, Bink handle fields, movie formats, audio/provider flags and archive input modes. Preserve pristine binaries, native decode/frame/audio baselines and startup logs.
2. **Replace the original startup role first.** In an isolated build/test installation, implement and verify its minimal supported duties in Rust. A temporary Rust proxy may remain while proving startup compatibility; stop loading `.original.dll`. Verify synthetic ABI behavior without installing input hooks or manipulating GUI/input.
3. **Prove direct A2FO bootstrapping.** Prototype the import/startup migration on private copies, with a minimal attach, explicit loader-lock-safe activation, FleetOpsHook ordering, retries and early-hook tests. Test direct Armada launch and every supported launcher/mod path. Remove the proxy filename only after this works without it present anywhere in the test DLL search path.
4. **Prove Rust BIK decoding.** Evaluate a pinned decoder against all required tag/audio/alpha variants, malformed/truncated data and real assets. Compare video pixels/audio output/timing and seek/reset behavior; measure memory and frame-time performance on the real target. Add only audited Rust dependencies.
5. **Implement the Bink compatibility API in A2FO.** Cover the ten primary-pair imports plus the five additional older-executable imports where supported, complete the reachable API/field/ordinal inventory, then cover all supported uses. Reuse Rust audio/clock infrastructure where compatible; add a pull/frame-oriented integration rather than wrapping a blocking full-window player. Preserve existing A2FO ordinals when adding exports.
6. **Retarget both engine consumers and source checks.** Redirect Armada/FleetOpsHook and supported variants; replace old `GetModuleHandle`/provider-identity assumptions. Demonstrate startup with no Bink DLL available and no circular load/initialization dependency. Until that succeeds, removing the DLL is premature even if movies decode in a separate test.
7. **Accept the real product.** Test all required intros, menu/texture videos, campaign/mission movies, loops, volume, skip/seek, fullscreen/window transitions, repeated starts, missing/corrupt content and shutdown. Run normal rendering/gameplay/save-load/multiplayer regression with exact patched engine hashes. Remove files only via authorized, backed-up deployment after acceptance.

The endpoint passes only when the required features work with **all three legacy filenames absent** (`Win2kDisableTaskSwitch.dll`, `Win2kDisableTaskSwitch.original.dll`, `binkw32.dll`), their responsibilities are Rust-owned in A2FO, no renamed/embedded native copy supplies those implementations, and supported launch paths resolve directly to the retained main DLLs. This is the additional closure contract for P01/P07/P13/P14/P15/P19 in the Rust plan.

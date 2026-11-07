# Additional DLL consolidation: CKController, D3D8, Dxver, ImageHlp, NetworkManager

Date: 29 October 2026. Read-only viability assessment. No installed files or production source were changed; no audited executable/DLL, game, networking session, device interaction or GUI/input automation was run. Only static parsers, source reads and bounded disassembly were used. Audit outputs are separate generated files.

Companion to [startup/Bink consolidation](dll-consolidation-viability-2026-10-29.md) and [pure-Rust completion](pure-rust-completion-plan.md).

## Decision table

| DLL | Can it become a thin compatibility DLL? | Can the local file eventually disappear? | Assessment |
| --- | --- | --- | --- |
| CKController | Technically conceivable after implementing the actually required device API; no current game requirement established | Possibly needs no replacement if proved unused | Exact supplied file is a Color Kinetics controller library. Absent from selected Roots installation; no consumer found in the scoped Heroic sibling scan. Do not assume it controls game input. |
| d3d8 | Yes, after the real required graphics implementation lives elsewhere | Yes with import/dynamic-name migration, or a verified system-provider configuration | Major renderer project. Current local file supplies D3D8-to-D3D9 translation; A2FO's existing compatibility code still calls the native provider. |
| Dxver | Plausible for its observed DirectX/version interface | Yes if unused, or after replacing and retargeting every actual caller | Small-looking interface, not yet a fully specified contract. Found mainly in older installations/mods. No observed normal import in the inspected engine set. |
| imagehlp | Plausible for the narrow observed game API | Local redistributed copies may be unnecessary with the correct system provider; complete import removal requires replacement/redirection | Primary Armada imports only `UnDecorateSymbolName`. This is much narrower than the entire library. An OS/Wine-provided ImageHlp is an allowed external platform dependency, not A2FO implementation. |
| NetworkManager | Possible in principle, but **not a safe export-only drop-in** | Technically plausible after a complete compatible implementation plus consumer/hook migration | Largest compatibility risk here: multiplayer/session/event/data/voice interfaces, C++ objects and a fixed-offset FleetOpsHook patch. Keep until independently proven. |

“Thin” means a valid PE DLL with the required exports/ordinals/data and any essential startup bridge. The work still happens somewhere. Forwarding to the same old native implementation under another name does not meet the Rust-replacement goal. Physical removal while keeping normal static imports unchanged fails the loader contract; see [Microsoft load-time linking](https://learn.microsoft.com/en-us/windows/win32/dlls/load-time-dynamic-linking).

## Evidence

[Machine-readable evidence](../build/dll-consolidation-audit-20261029/additional-dll-evidence.json) contains 15 selected DLL/EXE records, hashes, import/export lists, raw version strings, source pins and disassembly provenance. [Extractor](../build/dll-consolidation-audit-20261029/inspect_additional_inputs.py). It also carries the earlier engine-import snapshot, explicitly separated from new file reads. Static normal imports and literal searches do not prove dynamic loading, actual launch selection, non-use, or behavioral compatibility.

### CKController: use the exact supplied file, establish whether it is needed

User-specified file: `/home/tamsynn/Games/Heroic/Star Trek Armada II/CKController.dll`.

- 94,208 bytes; SHA-256 `10c57b7e20b5cbd55097b8978ae114a95fe57215eb0ec7f9c4bcd2e370615bb4`.
- Raw version strings identify `Color Kinetics Inc.`, `Color Kinetics CKController Libraries`, `Generic controller development libraries`, version `3, 0, 10, 1`.
- 35 exports include device enumeration/open/close, DMX file/frame operations, triggers and live/playback control. Imports are Kernel32/User32. This is metadata/API evidence, not a validated hardware protocol.
- No CKController copy was found in the selected Fleet Ops Roots tree. The supplied Heroic copy matches the separately inspected FO2 copy.
- A scoped parse of Heroic's adjacent PE32 EXE/DLL files found no normal import of CKController; an ASCII filename-stem check found the name only in CKController itself. The selected primary Armada/FleetOpsHook images also had no CKController import or ASCII/UTF-16 filename-stem match. This does not exclude constructed names, archive/plugin loading or another launch path.

**Recommended treatment:** first determine whether any supported launch/device feature loads it. If it is unused distribution residue, retire the local copy through a backed-up, separately authorized non-use test; implementing 35 hardware functions would serve no game requirement. If a real dependency is found, specify that device/API behavior before selecting a Rust replacement. A “success” stub that silently drops real device behavior is not equivalent. No deletion is authorized or recommended on the evidence of absence of imports alone.

### d3d8: the DLL entrypoint is small; the renderer behind it is not

The current primary and Fleet Ops 4.0 mod copies have SHA-256 `122928cfe225c25d30decf7184a5d37e490cecf3b58256ba3206c7e1853f8ab8`. Their four named exports are `DebugSetMute`, `Direct3DCreate8`, `ValidatePixelShader`, `ValidateVertexShader`; imports include D3D9 and native C++ runtime libraries. This matches the established standalone d3d8to9 deployment identity. Upstream [d3d8to9](https://github.com/crosire/d3d8to9) describes translation of D3D8 API calls/shaders to D3D9.

Primary Armada statically imports `Direct3DCreate8`. There is also a separate `d3d8.dll` literal outside the import-name record that requires caller tracing. A small factory export returns COM objects whose methods do the real rendering; forwarding only that export does not eliminate their implementation.

Current A2FO evidence is explicit:

- [observer.cpp](../renderer/d3d8_compat/observer.cpp) calls `runtime->factory(c.sdk)` and then observes or wraps the returned native object.
- [game/factory.cpp](../renderer/d3d8_compat/game/factory.cpp) retains an `IDirect3D8*`, calls its AddRef/Release and delegates adapter/device-capability operations.
- The experimental D3D11 path and existing Rust policies therefore do not establish a provider-independent Rust D3D8 implementation.

**Required replacement:** finish a Rust-owned D3D8 factory/device/resource/state/shader/draw/presentation implementation for every supported engine call, with correct COM identity/refcounts/layout and reset/lost-device/failure behavior. Then a valid tiny d3d8 shim can forward its entrypoints to A2FO, or version-specific import/name changes can eliminate the local file. Current renderer/provider-identity checks must be updated deliberately.

Simply deleting the local wrapper can make Windows/Wine select its system D3D8 provider, but that changes the current translation/DXVK chain. It is a separate configuration requiring rendering acceptance, not proof that A2FO took over. Likewise, moving C++ d3d8to9 code into A2FO could consolidate files but would fail the pure-Rust goal. D3D9/DXVK and other renderer dependencies remain separate future closure work.

### Dxver: plausible small replacement, active use unproven

Observed Roots copies: `Data/Mods/FCM/Dxver.dll` and `Data/Mods/Orb of Time/Dxver.dll`. A copy is also present beside the supplied Heroic CKController. The discovered copies share SHA-256 `9ece5c9208a5e095fda9e1e960ad221b72b07432285b8183cbd7c9feb0e3a6b2`, size 57,856 bytes.

Eight exports: `DirectXInstalled_3`, `DirectXInstalled_3a`, `DirectXInstalled_5`, `GetTheVersion`, `VersionCompare`, `MicrosoftDirectXInstalled`, `MicrosoftDirectXInstalled2`, `MicrosoftDirectXInstalled3`. Imports include version-information APIs. These establish an exposed detection/version interface, not its exact parameter/return semantics.

No normal Dxver import was found in the earlier inspected engine images or the Heroic sibling PE32 scan; selected engine filename-stem searches were also negative. Old mod installer metadata names it. This leaves installer/optional/dynamic use unresolved.

**Required decision:** if it has no supported runtime consumer, treat it as a non-use/removal candidate. Otherwise specify its actual ABI, output values/buffers and detection behavior, implement the needed contract in Rust, and preserve missing-component/error results. Do not hardcode “DirectX installed” merely to satisfy a checker. Retarget actual imports/loads for physical removal, or retain a valid forwarding shim for unchanged consumers.

### ImageHlp: distinguish local packaging from the operating system

The selected primary Armada normal-import table requests exactly one ImageHlp function: `UnDecorateSymbolName`. The inspected older FCM/Orb and Fleet Ops 4.0 executable variants also show that named import. Local ImageHlp copies exist in older mod folders (including FCM, Orb of Time and STA2 Classic Mod); no copy sits directly beside the primary `Data/ArmadaL.exe`. Actual provider selection still requires loader/runtime evidence.

The FCM copy is 106,256 bytes, SHA-256 `cf06677fd970d454931b4dee33e50b5a57949a838c18bcb40e8f6f9c9a650147`, with 68 exports. Other copies and hashes are recorded separately in the JSON rather than assumed equivalent.

Microsoft documents [UnDecorateSymbolName](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-undecoratesymbolname) as a C++ symbol undecorator with caller-buffer, flags and success/failure semantics. One inspected Armada call requests flags zero and a bounded output buffer, then uses either the produced text or a fallback. Replacing it with a blank function would alter observable behavior. All callsites and purposes still need tracing; it must not be assumed to serve diagnostics only.

**Two different endpoints:**

1. For **fewer local files**, use a validated OS/Wine-provided API and keep the external platform dependency. That does not require recreating all 68 legacy exports in A2FO. Do not remove or modify the OS/Wine DLL itself.
2. For **removing this engine import**, provide the required compatible Rust undecorator in A2FO and retarget the caller, or use a tiny valid local forwarder. The [msvc-demangler Rust project](https://github.com/mstange/msvc-demangler-rust) is an investigation candidate; its upstream README explicitly does not promise exact Windows undecoration compatibility. Test the actual legacy MSVC names, flags, buffer limits, return values and fallback behavior before selection. No crate was added or tested here.

Using DbgHelp instead is another external-platform route, but merely renames the dependency; it is not a Rust implementation of undecoration.

### NetworkManager: large API plus an internal binary hook

Primary `Data/NetworkManager.dll`: 286,720 bytes, SHA-256 `68ef126c1afd809de6fa3edb08cdf991683e3f757e8237c9dafa986c4dcdc7dd`; 185 exports. Names expose DirectPlay/GameSpy providers, lobby/game/room/player/event objects, data/chat, creation/join/leave, voice and settings. Exports include C++ constructors, virtual tables and static data, not only C-callable free functions. Imports include Winsock, COM and the old C/C++ runtime.

Primary Armada imports 16 names directly, including `Lobby::Create`, object deletion/accessors, voice settings and `VoiceTargetAll` data. That is not the whole call surface: methods reached through returned objects/vtables and retained engine pointers must also be inventoried. C++ name matching alone cannot reproduce object layout, calling convention, destruction/allocation, references and event lifetimes.

**Critical extra dependency:** the pinned FleetOpsHook routine identified in its map as `uNetworkManagerHook.NetworkManagerHook_Init` looks up `NetworkManager.dll` using GetModuleHandleA, adds `0x149e4` to its base and passes that address to its hook helper. The [bounded disassembly](../build/dll-consolidation-audit-20261029/network-manager-hook.txt) and [lookup thunk](../build/dll-consolidation-audit-20261029/network-module-lookup-thunk.txt) record this exact profile-specific sequence. This is not an export lookup. Its original runtime success/behavior is not independently established by this audit.

Consequently, an export-only tiny NetworkManager replacement is not a safe drop-in: FleetOpsHook would still seek and patch the old internal location. Update/remove/retarget that hook as part of the Rust takeover, preserve its intended behavior and readiness order, and audit all other direct offset references. Keeping a renamed native NetworkManager as a target would not remove the implementation dependency.

**Required replacement project:** specify all reachable legacy ABI/data/vtable contracts; implement Rust owners, providers, session discovery/create/join, event queues, messages/channel/reliability/ordering behavior, identity/errors, cancellation/teardown, chat/voice and all required protocol interoperability. Preserve compatibility with supported existing peers, or explicitly choose and test a matched-build-only protocol change as a separate product decision. A local lobby mock or single-player launch cannot validate multiplayer parity. Use synthetic/offline fixtures first; no live-server traffic is needed for the assessment.

A forwarding DLL is an eventual packaging option only after that implementation and the fixed-offset hook migration. Of the five candidates, this carries the greatest multiplayer/ABI uncertainty and should remain last until real two-peer testing is available.

## Recommended priority and completion criteria

1. Establish non-use versus real consumers for CKController and Dxver; do not port apparently dormant libraries merely to move code.
2. Resolve ImageHlp packaging/system use, or prototype the narrow Rust undecoration contract if full import removal is desired.
3. Complete the Rust D3D8 provider as part of the existing renderer plan; preserve the working renderer chain during development.
4. Treat NetworkManager as its own researched ABI/protocol project, including FleetOpsHook's internal hook and actual multiplayer acceptance.

For every supported launch/mod profile: enumerate normal/delay/dynamic loads and all callbacks/data/COM/virtual calls; pin pristine binaries; implement the required Rust behavior; test isolated compatibility/failure/lifetime paths; verify a clean production link without the old implementation; test actual game behavior; then use an authorized backed-up installation migration. Thin shims must have valid required exports and safe startup behavior. Physical removal additionally requires that no supported caller resolves the old local filename, or that its deliberately retained system-provider resolution is accepted as an external platform boundary.

**This assessment supports conditional feasibility, not permission to blank/delete these files or a claim that replacements already exist.**

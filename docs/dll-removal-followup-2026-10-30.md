# Remaining DLL removal assessment — 2026-10-30

The next two candidates for **isolated removal tests** are `d3d9_no_reshade.dll` and `Magnification_Roots.dll`. **Retain `GameuxInstallHelper.dll`: the user identified a potential Armada 2 Refit utility requirement after this audit.** Its lack of discovered game callers does not clear that utility requirement. This assessment does not approve their permanent deletion. No game installation files were changed or binaries executed during this audit.

## Scope and evidence

- Live installation: `/home/tamsynn/NVMeData/Fleet Ops Roots`.
- Existing trial: `/home/tamsynn/A2FOHookExtensions/build/legacy-dll-independence-20261030/test-installation`.
- Read 75 PE32/PE32+ file occurrences: direct EXE/DLL files in the live root, Data, Fleet Ops 4.0, STA2 Classic Mod, FOtest and test; plus trial root and Data. This is a bounded selection, not every installed mod.
- Parsed normal and delayed imports, pinned SHA-256 hashes and scanned case-insensitive ASCII/UTF-16 literal names. Every file's size/mtime stayed stable during its read.
- Independent `llvm-readobj --coff-imports` comparison matched the normal/delayed DLL-name counters for all 75 rows. LLVM reported one diagnostic on the live uninstaller: `RVA 0x0 for delay import directory name not found`. Treat its delay-table interpretation as qualified despite the matching names.
- No active Armada/Fleet Ops game process was observed, so this is not a trace of loaded modules. The large STA64 launchers contain bundled/packed content; their embedded payloads were not extracted. Plain-text absence cannot clear those payloads.

Evidence: [inventory and filename graph](../build/dll-consolidation-audit-20261030/remaining-dll-evidence.json), [read-only inventory script](../build/dll-consolidation-audit-20261030/inspect_remaining.py), [verification and source pins](../build/dll-consolidation-audit-20261030/verification.json).

The graph records **DLL filenames requested by imports**, not which on-disk copy Windows/Wine ultimately selects. Executable directories, explicit paths, flags, already loaded modules and system DLL rules affect resolution. Identical mod-folder copies are not automatically safe to remove.

## Candidate decisions after user clarification

| Priority | DLL | Current evidence | What is still needed |
| --- | --- | --- | --- |
| Retain | `GameuxInstallHelper.dll` | 32-bit, 86,528 bytes. Exports Game Explorer install/uninstall and MSI helper functions. No normal/delayed consumers in this inventory and no external literal-name references in the scanned images. | Keep it available for the potential Armada 2 Refit utility. It is excluded from the next removal-test batch. |
| 1 | `d3d9_no_reshade.dll` | **64-bit**, 136,192 bytes. Version metadata says D3D Wrapper, black_ninja, 0.0.0.0; exports several graphics entry points. The 32-bit Armada executable cannot load this image into its process. No normal/delayed consumers or external literal references found. | Test the actual launcher route as well as direct Armada startup. The 64-bit STA64 launchers have unexamined bundled payloads. The filename alone does not identify this as ReShade or prove it is unused everywhere. |
| 2 | `Magnification_Roots.dll` | 32-bit, 42,496 bytes. Exports the `MagInitialize`/`MagSet*`/`MagGet*` API family. No normal/delayed consumers or literal references found in the scoped images. | Check Roots launcher/display/accessibility features and both mod routes. Its custom filename and lack of discovered callers do not establish that those features never load it dynamically. |

The live base and Fleet Ops 4.0 copies of each listed DLL are byte-identical. Their full hashes and paths are in the inventory. Excluding the retained Gameux helper, the two removal-test candidates are four file occurrences totalling 357,376 bytes. File count and dependency simplification are the main potential gains here.

Microsoft documents [GameUXInstallHelper as installer integration](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/windows-game-explorer-integration), matching this file's exports. Microsoft documents the [MagInitialize API](https://learn.microsoft.com/en-us/windows/win32/api/magnification/nf-magnification-maginitialize) as part of Magnification.dll; this API correspondence does not identify the custom Roots file's exact callers.

## Tunngle: an optional-feature candidate, not an unused file

`Tunngle.dll` has no normal/delayed import consumers, but FleetOpsHook contains a real dynamic-loading path. Its `Tunngle.dll` literal is at file offset `0x173a64`, VA `0x5a974664`. Current binary disassembly at VA `0x5a974488` shows loading, a null-handle test, clearing the loaded flag and returning on failure. See the [current missing-DLL guard](../build/dll-consolidation-audit-20261030/tunngle-current-missing-dll-guard.txt).

The recovered source agrees: `reverse_engineering/FleetOpsHook/rebuild/src/system/tunngle.cpp` loads the DLL, resolves client/login/network functions and maintains `g_dllLoaded`; the multiplayer UI adapter reads that flag. Recovered source is supporting evidence, not proof that every subsequent branch of the installed binary is safe without it.

This makes it a reasonable **separate test for builds that intentionally omit Tunngle integration**. It would retire that feature rather than reimplement it in Rust. Verify the multiplayer connection UI and ordinary LAN/manual-IP behavior without it. Full matched-build multiplayer remains pending because a second peer is unavailable. Do not remove it merely because normal imports are absent.

## DLLs to retain pending actual replacement or integration

| DLL(s) | Why deletion is premature / route to eventual retirement |
| --- | --- |
| `Win2kDisableTaskSwitch.dll` | Both scoped Armada executables still statically import this filename. The Rust proxy removes the original DLL dependency, but removing the proxy itself requires checked engine import/startup migration. A blank file will fail. |
| `D3DX81ab.dll` | FleetOpsHook statically imports it. A2FO also dynamically resolves shader assembly and texture loading from it; `core/nebula_renderer.cpp:2091` and the cloak composite bridge are active consumers. Its removal needs both loader-contract migration and rendering/texture replacements, including emissive maps. |
| `d3dx9_43.dll` | FleetOpsHook statically imports it. A2FO's direct-D3D9 nebula path also resolves its shader assembler from the loaded module. Replace consumers and imports together. |
| `D3DCompiler_43.dll`, `D3DCompiler_47.dll` | A2FO loads 47 then falls back to 43 and performs runtime compilation (`renderer/shaders/bytecode_cache.cpp`, `renderer/cloak/refraction.cpp`). D3DX9 also contains the explicit compiler43 name, despite no normal/delayed compiler import; that dynamic path was not fully traced. The bundled STA64 images contain compiler47 literals too. Do not remove either version as a presumed duplicate yet. Precompiled shaders can reduce compiler use, but must cover user/custom sources and legacy callers. |
| `d3d8.dll`, `d3d9.dll` | Armada statically imports D3D8; that provider imports D3D9; FleetOpsHook also imports D3D9. Refit menu scenes explicitly load D3D9. The current renderer chain cannot simply be deleted. Folding or replacing it needs COM/device/resource parity and actual visual acceptance. |
| `fmodex.dll` | FleetOpsHook statically imports it. Replacing modern movie decoding does not replace the game's FMOD sound/music interface. Its consumers require a separate audio migration. |
| `NetworkManager.dll` | Armada statically imports it; FleetOpsHook also patches inside the DLL at a fixed offset. Its C++ ABI, data/vtables, protocols and hook require migration together. An export-only forwarding stub is insufficient. |
| `msvcrt.dll` | Multiple components import the CRT; RGBTextures also checks the loaded CRT's `fopen` ownership. Removing a local copy is a provider/ABI compatibility test, not proof the CRT implementation was replaced in Rust. System/KnownDLL resolution must be observed before selecting an approach. |
| `A2FOMedia.dll` | Already a Rust media implementation. It can eventually be linked into the main DLL after allocator, thread, shutdown and panic boundaries are reconciled. Deleting the file today breaks its loader path. It does not currently replace Bink decoding. |
| `binkw32.dll` | Armada and FleetOpsHook still import it; existing BIK movies and handle/buffer consumers remain. A compatible decoder plus audio/timing/seeking/copy and ABI/import migration is still required. |
| `FleetOpsHook.dll`, `A2FOExtensions.dll` | These are the user's intended retained DLLs. This audit does not change their language-completion or game-acceptance status. |

The presence of system Windows/Wine runtime services remains separate from the goal of eliminating bundled legacy DLL files. An OS fallback is not a Rust rewrite.

## Earlier removal results, with their limits

- **`Win2kDisableTaskSwitch.original.dll`:** absent from the existing trial; user reported that the correct trial executable worked. Live Roots still has the old proxy and original DLL, so do not delete the original there without installing the paired replacement first.
- **Base trial `ArmadaLoad2.dll`:** absent and user reported successful testing. The live base copy and parent-mod copy remain. Acceptance covers the tested path, not every packed launcher or every mod. See [the recorded user result](../build/legacy-dll-independence-20261030/user-acceptance/20261030T162432Z/armadaload2-user-result.json).
- **Classic local `imagehlp.dll`:** quarantined in the existing trial, with scoped engine imports retargeted to system DbgHelp. This is removal of a local provider requirement, not removal of system symbol services. Keep the existing acceptance limits.
- **`Dxver.dll`:** already absent from the selected trial routes. Older FCM/Orb variants still need their own tests; those variants were not included in today's 75-image audit.
- **`CKController.dll`:** earlier audit was of the supplied Heroic installation, not Roots; it is absent from this selected Roots inventory. Its earlier non-use evidence is scoped and was not refreshed today.

## Next test sequence

1. Preserve the existing successful trial and its logs/receipts. Prepare separate, hash-manifested quarantine variants, one candidate at a time, preserving relative paths for rollback. Neither this report nor the inventory creates those variants.
2. Start with the 64-bit wrapper, then Magnification. Keep GameuxInstallHelper for the potential Refit utility. Account for the base and parent-mod copies; leaving a second accessible copy can mask a dependency.
3. Use the exact trial executable/launcher route. Fully restart between FOtest and test. Preserve fresh loader/module logs and record the actual module paths if diagnostics are available.
4. Check startup, mod selection/relaunch, emissive textures, rendered 3D/refit menus, movies/audio, Instant Action, save/load and shutdown. Check any affected launcher/display features too. Keep installer material available and multiplayer acceptance separately pending.
5. Only combine removals after the individual tests pass, then repeat the combined run. Keep live deployment a separate, backed-up action.

This pass produced an inventory, source/binary evidence and a ranked removal-test list. It did not perform new in-game acceptance or remove any additional DLLs.

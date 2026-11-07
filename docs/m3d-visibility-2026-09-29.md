# Invisible M3D: native hierarchy correction (2026-09-29)

## Observed failure

After restoring interior meshes, the user reported that no ship mesh parts
were visible. The preserved game log proves the interiors build loaded, parsed
90 Akira runtime nodes, created 13 meshes/renderers, and bound 13 diffuse slots
with zero misses. It then reported 89/90 parent mismatches and rewrote the live
hierarchy. No M3D-specific diffuse draw success marker was recorded.

The earlier converter-only tests did not exercise the runtime node ABI. The
runtime diagnostic could still count every mesh by traversing both links while
misinterpreting which link means child, so its count was not rendering proof.

## Proven cause and correction

Installed ArmadaL.exe and workspace ArmadaL.exe both have SHA-256:
`63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36`.

At VA 0x6386d3..0x6386dc the native constructor stores:

- new node +0x18 = parent;
- new node +0x20 = parent's old +0x1c;
- parent +0x1c = new node.

Thus first child is +0x1c and next sibling is +0x20. Recursive collector
0x639db9 loads the first child and 0x639dd9 selects the next sibling after
returning from recursion. [Pinned instructions](m3d-node-links-2026-09-29.asm)
agree with the previously recovered node_collect implementation.

A2FOM3D had these links reversed. The read-only audit saw false mismatches, then
its repair cleared and rewrote forward links using the reversed interpretation.
The result disconnected the hierarchy from the engine's traversal and disagreed
with its existing parent pointers. The module now uses the verified offsets for
all its walks. `audit_runtime_hierarchy` performs no node/geometry writes; the
native constructor's links are preserved. Name restoration and diffuse binding
remain, and all authored in_* geometry stays enabled. No core renderer or other
module was rebuilt or deployed.

`docs/addresses.md` previously repeated the same reversed layout; it now records
the verified correction and flags other modules' subtree-specific uses for a
separate audit. This change does not claim those modules were fixed.

## Verification

- `make build/modules/A2FOM3D.dll -j 2`: passed with the existing thiscall,
  FARPROC-cast and unused-helper warnings.
- `make build/m3d_runtime_hierarchy_test.exe`: passed. The test includes the real
  module audit, constructs a root/two children/two grandchildren with literal
  native offsets, and checks correct parentage and byte-for-byte preservation.
  An intentionally wrong parent and missing node are reported without mutation.
- `DISPLAY= WAYLAND_DISPLAY= WINEDEBUG=-all wine build/m3d_runtime_hierarchy_test.exe`:
  passed on the real Windows i386 ABI with no game launch or GUI input.
- `./build/m3d_conversion_test`: passed, including the restored interior groups.
- Negative control: compiling the same runtime test with only the two offsets
  swapped back fails `correct.mismatched == 0` under Wine (exit 3). This confirms
  the test detects the original ABI mistake rather than mirroring the constants.
- PE export inspection confirms A2FO_ModuleInit and A2FO_ModuleShutdown.

## Deployment

Built: `/home/tamsynn/A2FOHookExtensions/build/modules/A2FOM3D.dll`.
Installed: `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOM3D.dll`.
Both SHA-256: `2fe0bcae98eb14de2f3ee4bf17c45658181e56fb1b22ea4fdc46123c7d10b75f`.
Installed 2026-09-29 23:33:57 +0100; 11,988,897 bytes.
Startup marker: `2026-09-29-native-links`.

Previous sources, deployed DLL, and invisible-run log are preserved in
`.deployment-backups/m3d-invisible-20260929T222922Z/`.
The deployed predecessor hash is
`0b8f6260aaea264f83863ddde4774513af0b2e14d947e89d8ee052539f79ec3a`.
No game process was running at deployment. The latest log was not overwritten
by launching a game during this task.

## Next live check

Fully restart Fleet Ops, select the same test mod, and load a fresh Akira.
The old process/model cache cannot be repaired by replacing a DLL on disk.
Expected new log: native-links marker, hierarchy mismatched=0/missing=0,
13 meshes/renderers and 13 diffuse bindings; no link rewrite message.
Check the ship's actual mesh visibility, alignment, textures, rotation and zoom.
In-game visibility after this correction remains unverified until that check.

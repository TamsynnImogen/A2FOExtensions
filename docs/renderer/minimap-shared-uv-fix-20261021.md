# Minimap markers: shared movie UV constant correction

The targeted Roots capture establishes that the native renderer issues ship and
station marker draws: both filters are zero (all), class sprite pointers are
null for the observed ordinary craft, and sampled D3D8 Clear calls return S_OK.
Their rectangles lie within the current radar viewport. The capture also records
`dxwrapper_loaded=0`. This is stronger than the earlier on-disk configuration
comparison, but does not establish when the original fault began.

The existing A2FOShellDisplay movie setup doubled its texture backing from
256×128 to 512×256 by changing two shared data constants at Armada RVAs
`0x2afb14/0x2afb18`: `1/128,1/256` became `1/256,1/512`. Native radar coordinate
and extent calculations also reference `0x2afb14`. The static file has the
original constants; the module changed their live values.

The station example in the capture exactly reproduces the unintended scale:

| Input/result | Value |
| --- | --- |
| World position X/Z, map extent | 17005.0 / 17316.8, 20000×20000 |
| Radar camera X/Y/W/H | 9.600 / 558.000 / 213.600 / 213.333 |
| Native 128-cell dot rectangle | (107,16) to (110,19) |
| With modified shared reciprocal | (98,571) to (101,573), matches captured Clear |
| With original radar reciprocal | approximately (188,584) to (193,589) |

This demonstrates a real coordinate/size bug. Successful Clear calls alone do
not prove final visibility: clipping, later composition or additional defects
can still affect the final minimap. Manual confirmation remains required.

The correction preserves the larger movie texture and redirects only its two
setup FMUL instructions to module-owned reciprocal floats:

| Armada RVA | Original operand | Movie-only replacement |
| --- | --- | --- |
| `0x0640b6` | engine `+0x2afb18`, 1/256 | private 1/512 (width) |
| `0x0640ce` | engine `+0x2afb14`, 1/128 | private 1/256 (height) |

The dimensions at `0x06406e` and both operands occupy one 102-byte instruction
span ending at `0x0640d4`. A single checked `patch_bytes` call changes those
three ranges and preserves every intervening byte. This avoids partial texture
setup if a later independent patch and rollback both fail. The core primitive
rejects before its memcpy on mismatch or VirtualProtect failure. Installation
retains the project's startup-only patching assumption; this is not an atomic
hot-patch scheme for concurrent execution.

The shared data constants are read for validation and never written. Other
renderer code and FleetOps' radar callback are unchanged. A historical duplicate
Bink source in A2FOFeaturePack is not compiled by that module's active build.

Evidence is preserved under
`build/minimap-investigation-20261021-010750Z/native-capture-20261021-082023Z/`.
The exact installed Armada executable SHA256 is
`817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`.

`make build/shell_intro_test.exe build/modules/A2FOShellDisplay.dll` passed.
Five isolated Wine console checks passed: movie fixture and real-DLL shell smoke
for both checkout and installed image pairs, plus the native-intro memory-DIB
fixture. Results, binary hashes and logs are in
`build/minimap-investigation-20261021-010750Z/movie-fix-tests/`. Desktop display
variables were removed. Images were mapped as data, GUI calls were stubbed or
used memory DCs, and Armada was never started.

Regression assertions cover private movie operands, unchanged shared data,
unchanged intervening instructions, complete setup preservation on patch
failure, changed-operand rejection, and existing movie sizing/forwarding cases.
Independent source review confirmed the native instruction boundaries and
single-write failure semantics.

After deployment, fully restart Roots and check `first`, then STA2 Classic Test
Warzone: ships and stations should appear at their actual minimap positions.
For Roots, the temporary diagnostic remains enabled so its fresh log can confirm
the corrected coordinates. Also check intro playback/skip and menu sizing;
movie quality, aspect ratio and window mode still need visual acceptance.
Preserve each fresh log before another launch. The original core remains
restored and D3D11 is off during this isolated correction.

Deployment changed only `Data/modules/A2FOShellDisplay.dll`, SHA256
`e19906657ed9d93c6edfca99dd8b5231bdb4d6e91a04e5a9399891b954ca9525`.
Backup and deployment receipt: `/home/tamsynn/A2FOHookExtensions/.deployment-backups/minimap-movie-uv-fix-20261021-083018Z/receipt.json`.
The original ShellDisplay DLL is preserved in its `originals/Data/modules/`
subdirectory. All 97 other protected files were hash-verified unchanged.

User reports markers working after deployment. The fresh diagnostic captures
correctly scaled rectangles, including station `(36,581)..(41,586)` for world
X/Z `2752.3/17628.5`. This run is preserved at `build/minimap-investigation-20261021-010750Z/successful-capture-20261021-084049Z`.
Temporary radar capture is now disabled (`Enabled=0`) for subsequent launches.
Both-mod and intro/skip coverage were not separately confirmed.

# Bink-free startup: FleetOpsHook checksum correction

1 November 2026. **The user reports no crash after the correction. Startup is recorded as
passing; media, rendering and gameplay acceptance remain pending. The live Roots
installation has not been changed.**

The reported dialogs say `FleetOpsHook.dll` seems corrupt and then report a null
read at VA `5A86C3AF`, RVA `0006C3AF`. The recent staged startup log ends with
`FleetOpsHook load failed`; its core log contains only initialization started.
This is a loader/startup failure, before Bink movie decoding. Older reports
already present in the copied installation are not evidence for this failure.

## Cause

The import-retarget tool recalculated a standard PE checksum. On this exact
FleetOpsHook binary, `madExcept.CheckFileCrc` has counterintuitive branch behavior:
a checksum mismatch sets BL to one and skips its error path, whereas a checksum
match leaves BL zero and enters it. The original working DLL already has a
nonmatching checksum; the rewritten test DLL accidentally changed that decision.
No claim is made that this inversion was intentional.

| Image | Stored checksum | Native calculation | Checksum decision |
| --- | --- | --- | --- |
| Original `3fd18eee…2e4c11` | `002E94FF` | `002F096A` | skips error path |
| Failing test `1257d3d0…a64e79` | `002E977D` | `002E977D` | enters error path |
| Corrected test `30df4960…af37df` | `002E94FF` | `002E977D` | skips error path |

The native calculation was obtained by mapping the real DLL without invoking its
entry point and executing only its pure `madExcept.CalcCheckSum` routine at RVA
`6C14C`. No game, error dialog, input hook or real audio device was started.

The reported crash instruction dereferences the interface global at `5AA060E4`.
The preceding path calls a routine that clears this same global before releasing
it. A later dereference at `6C3AF` therefore agrees with the reported null read.
There is another conditional branch in this path, so checksum equality alone is
not presented as a complete standalone proof of every runtime condition. Earlier
file/open/parse errors can also enter the error path.

## Correction

For the pinned FleetOpsHook image only, preserve its original PE checksum field.
Continue recalculating Armada engine checksums. Do not patch checker instructions
or disable validation; the exact source hash, import set, file/section geometry,
code, thunks and replacement bounds remain checked.

Compared with the failing test DLL, the corrected DLL changes only bytes at file
offsets `0x158` and `0x159` inside the checksum field. The Bink redirection, executable
code, exports, resources, section geometry and Rust runtime remain identical.
The matching `A2FOFleetOpsProfiles.txt` now fingerprints the corrected DLL.

Corrected SHA-256:
`30df49608e2b992c32d7ac92d85396b29a272ac7a7300b56f104b12638af37df`.
The staged Rust core remains
`b3afa28cb236956f4b28d4279d75998d1ba965b95ef7a98bedc5b8501d54fcb8`.
All three Bink DLLs remain outside the trial in quarantine.

## Verification and evidence

- Native checksum arithmetic distinguishes original, failing and corrected files
  exactly as shown above.
- Eight export/import/checksum regression tests pass. A positive control
  recalculates the checksum and reproduces the bad branch decision.
- Both exact engine and FleetOpsHook compatibility profiles pass in a headless
  map that invokes no game/provider entry point.
- The assembled Rust Bink API smoke still passes file/memory/preload input,
  seeking, copy guards, timing, volume ABI and close with no Bink module loaded.
- The live FleetOpsHook DLL retains its original hash.

Evidence and backups are in
[`build/bink-corrupt-investigation-20261101`](../build/bink-corrupt-investigation-20261101).
The original failing DLL, profile, manifests and fresh logs are preserved under
`before/`. Relevant receipts are
[native arithmetic](../build/bink-corrupt-investigation-20261101/native-checksum-receipt.json),
[two-byte difference](../build/bink-corrupt-investigation-20261101/difference.json),
[private replacement](../build/bink-corrupt-investigation-20261101/installed-private-fix.json),
and [headless checks](../build/bink-corrupt-investigation-20261101/headless-checks.json).

Fully close the previous attempt and launch the same
[test ArmadaL.exe](../build/bink-rust-stage2/test-installation/Data/ArmadaL.exe)
with its Data directory as working directory. Test `fo test` and `test` again.
The user subsequently reported “ok no crash”, confirming startup for this retry.
Rendering, audio and gameplay acceptance remain pending; no automated game launch
was used to obtain that confirmation.

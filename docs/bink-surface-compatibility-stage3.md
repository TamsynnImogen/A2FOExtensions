# Bink surface compatibility: stage three

1 November 2026. **Current and older DirectDraw surface classification is
implemented and verified in an isolated candidate. It has not replaced the
confirmed-working Bink-free test DLL.** Four older imported APIs and further ABI
adapters remain unfinished. Audio listening is pending because the user cannot
test it at present.

## Implemented

The safe Rust `surface::PixelFormat` is the observed 32-byte structure of eight
DWORDs. Separate classifiers preserve the two native generations' result codes,
FOURCC priority, mask fallback and treatment of flags. The Windows boundary calls
`IDirectDrawSurface::GetPixelFormat` at vtable slot 21 using stdcall, initializes
the descriptor size and follows the observed native HRESULT behavior. The caller
must supply a live compatible interface for the duration of the call.

The experimental main DLL keeps all existing 210 exports, ordinals and forwarders,
then adds `_BinkDDSurfaceType@4` at 211 and an explicitly separate
`_BinkDDSurfaceTypeLegacy@4` at 212. `--bink-surface-runtime` selects this contract.
The ordinary build still selects 198; `--bink-runtime` still selects 210. DLL
imports are unchanged. There is no native Bink dependency in the candidate.

## Why the generations need separate adapters

The bundled FCM and Orb DLLs share SHA-256
`6c21011c392c3cdb8d385c399ad80ce071cb7aaee9d96d35706f755eb27d9664`
and have 67 named exports. The primary DLL is the separately pinned `9c2b75de…`
image with 73 exports.

| Descriptor | Primary result | Older result |
| --- | ---: | ---: |
| Indexed 8-bit | 0 | 6 |
| BGR24 | 1 | 0 |
| BGRX32 | 3 | 1 |
| RGB555 without alpha | 9 | 2 |
| RGB565 | 10 | 3 |
| RGB655 | 11 | 4 |
| RGB664 | 12 | 5 |
| ARGB4444 | 7 | unsupported |
| YUY2 | 13 | 7 |
| UYVY | 14 | 8 |
| YV12 | 15 | 9 |

There are further differences: older 24/32-bit classification ignores channel
masks and alpha, while the primary helper distinguishes reversed/alpha formats.
The older `_BinkSetVolume@8` consumes two arguments and returns with ret8; the
primary export bearing the same label consumes three and returns with ret12.
The primary FleetOpsHook adapter supplies that third argument, while the older
executables use their own two-argument contract.

The older buffer caller passes `handle + 0x3c` as its rectangle pointer. The
primary native rectangle code writes its array starting at `handle + 0x38`.
The older copy callers also OR `0x04000000` into their copy flags; full flag
translation is not established. These observations rule out treating the older
executables as compatible solely by adding five export names.

No FCM or Orb executable/import table was changed. Their reachability in the
current Fleet Ops launch/mod workflow also remains to be established.

## Evidence

The fake COM probe makes no window, input, drawing or audio-device calls.
Both native versions and Rust execute the same **208 cases per generation**,
covering masks, bit counts, flag extremes, FOURCC precedence and failed queries.
All **416 cases match exactly**, both in the standalone runtime and in the clean
212-export main DLL. These are classification tests, not real-surface lifetime,
rendering or old-executable acceptance.

The original primary API smoke also passes on the clean candidate, including
file/memory/preload input, volume stack cleanup, seek, skipped frames, copy guards,
errors and close. It confirms native Bink is not loaded. It additionally confirms
that the unaccepted 16-bit evaluation modes fail before changing the destination.
Default runtime unit tests: **22 passed**. Surface/export contract tests: **5
passed**. The original 210-export prefix and DLL import set are unchanged.

Candidate:
`build/bink-legacy-stage3/core-build/A2FOExtensions-surface-only.dll`

SHA-256:
`3255f564302dbe78c20d5a97cb3d65cd8504f1d75cf252547636ecbe6a8bfbd0`.

Receipts: [milestone](../build/bink-legacy-stage3/milestone.json),
[native and standalone runs](../build/bink-legacy-stage3/surface-runs.json),
[clean main DLL](../build/bink-legacy-stage3/surface-only-main-receipt.json),
[build](../build/bink-legacy-stage3/core-build/rust-dll/receipt.json),
[pinned legacy inputs](../build/bink-legacy-stage3/pinned-legacy-inputs.json).

## Pixel-copy experiment: not accepted

ARGB4444, RGB655 and RGB664 packers were explored separately. Their unit tests
check masks and bounds, but native comparisons did not all meet the fixed
one-quantized-channel-unit criterion. The initial full-frame comparison omitted
COPYALL while resetting destinations, exposing native dirty-rectangle/skipped
writes. A corrected comparison forces full copies and retains all failures:

- ARGB4444: four finite source cases meet the criterion; real alpha-bearing media
  was not tested.
- RGB655: all four cases differ substantially in native channel packing.
- RGB664: three cases meet the criterion; the large Intro case reaches two units
  in its green channel.

These modes remain behind `rgb16-evaluation`, **off by default and off in the
clean candidate**. The ordinary pixel-copy formats and flags retain their prior
behavior. No tolerance was widened to mark this experiment accepted.
[Copy-all diagnostic receipt](../build/bink-legacy-stage3/pixel-comparison-copyall/receipt.json).
The first main candidate containing ungated pixel writers was moved under
`core-build/unaccepted-rgb16/`; it is not the clean candidate above.

## Remaining work

Implement and verify `_BinkBufferLock@4`, `_BinkBufferUnlock@4`,
`_BinkGetRects@8` and `_BinkBufferBlit@12`, with the correct versioned handles,
volume/copy adapters, ownership and presentation lifecycle. Do not expose
placeholder success for these APIs or retarget older images before that work.
Resolve pixel packing and real alpha/YUV/palette coverage before promoting the
evaluation writers. Keep exact image/layout/import gates and the FleetOpsHook
checksum correction.

The user-tested copy remains on core `b3afa28c…54fcb8` and corrected FleetOpsHook
`30df4960…af37df`, with all three Bink files still in quarantine. Fresh logs report
both early attachment and completed deferred initialization. They contain first
frames from modern MP4 playback followed by cancellation, which does not establish
media acceptance. Some modules are inactive/unavailable; this does not establish
that every module passed its in-game tests.

The [existing test checklist](../build/bink-rust-stage2/TESTING.txt) still applies.
Startup is user-confirmed; sound listening is unavailable for now, and visual,
rendering, gameplay/save-load and multiplayer acceptance are not inferred.

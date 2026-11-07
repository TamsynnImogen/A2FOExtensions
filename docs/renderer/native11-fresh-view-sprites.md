# Fresh view policy and native sprite source tracing

Status: private integration candidate. Gates **53–61 remain open**. The
[native retirement checkpoint](native11-lifecycle.md) is preserved separately.
No protected installed game file is changed by this checkpoint.

## Fresh per-instance view inputs

The owned Craft policy reproduces the pinned native weighted distance,
Euclidean distance, distance rejection, far fade and near fade. It preserves
native binary32 store points and extended x87 arithmetic, including the
radius-scaled distance threshold. Its inputs come from fresh logical position,
camera position, radius, game settings and mode flags. The reader does not use
instance distance fields `+0x88/+0x8c` or rendered alpha `+0x78` as authority.

`native11_animation_clock::sample_view` binds that reader to the existing
owner thread, exact code/virtual-slot profiles and version-pinned model owner.
Two source reads and a second model pin reject observed mutation or retirement.
The existing node diagnostic can compare its result with cached native outputs;
those cached fields are comparison data only. This does not establish full
class/procedural/FOW policy, effective materials, or complete world coverage.

The CPU oracle executes only the original pinned distance/fade routines on
synthetic object/game records. It passes **49,152 exact result comparisons** over
12 x87 precision/rounding combinations, including all flag/mode combinations
and directed threshold boundaries. The owned zero-distance case avoids creating
a native divide-by-zero exception flag; equivalence covers the returned fields,
not reproduction of native FP status effects.

## Sprite source route

An opt-in `CaptureSprites=1` source observer now connects the completed main-world
capture to the checked `ClipAndRender2D` dispatch and final EndScene boundary.
It copies clipped coordinates/UVs, quantized colour, raw material and sampler
numbers, and immutable texture owners. The native sprite draw and EndScene still
run once. The colour oracle checks **49,152 ARGB results / 196,608 vertices**,
including adjacent representable inputs around byte-rounding thresholds.
Its synthetic texture-zero path does not verify padded-texture UV adjustment.

The source trace observes one native route. It does **not** supply full HUD,
font/minimap coverage, a complete primitive frame, UI render-target certification,
or a replacement renderer. Native style numbers remain uninterpreted; address
U/V and mag/min/mip filters retain distinct checked cache fields. World viewport
and native target sizes are copied correlation metadata.

The later private game bootstrap exposed an incorrect target-size offset in the
first candidate: device `+0x10/+0x14` held zero. The current reader uses the
native pixel dimensions at `+0x84/+0x88`; the generated profile pins their update
function and the same-object call into device opening. The guard rejects invalid
actual dimensions even when the old fields contain plausible values. See the
[private bootstrap evidence](native11-private-bootstrap.md) for the separate
legacy menu run; this correction does not establish complete UI coverage.

The EndScene observer admits either the exact native indirect call or a checked
relative call to the exact FleetOpsHook callback. It verifies the branch actually
present in the host, the DLL identity and its loaded callback bytes. No host
compatibility is inferred from the recovered Armada2.ldl installer catalogue.
Unknown hooks fail closed. A distinct bridge preserves the direct callback's
stdcall return and cursor handling by forwarding it once.

Partial installation leaves tracing disabled; any installed tap still forwards
its original operation. Source tickets, generation, thread/device identity,
texture versions and bounded budgets gate publication. Retirement, replacement,
profile loss, aborted worlds and source changes revoke access to the trace;
previously returned immutable owners stay readable. An unrelated device cannot
seal or erase the main device's pending trace.

## Verification and remaining work

- Strict core/helper and private fixture builds pass.
- **117/117** private runtime cases pass with D3D8/9/D3DX disabled.
- **32** strict host/Clang sanitizer variants and **32** Rust tests pass.
- The later live-bootstrap candidate passes 118 private suite cases plus one
  distinct world-light owner-thread case. Its private 90-second legacy menu
  run refused loaded sprite profile/ownership admission; the isolated sprite
  guard result does not imply live UI capture.
- The newer [live-ownership checkpoint](native11-live-ownership.md) passes
  122/122 private runtime cases. A saved legacy-menu run reports sprite tracing
  armed with the checked Fleet 44-byte dimension NOPs and a trusted refresh
  owner; the coexistence fixture uses a synthetic approval callback. Neither
  result proves complete live UI capture, a full HUD route or native D3D11
  presentation.
- Sprite assembly fixtures cover three forwarding paths across 12 PC/RC modes,
  full entry GPR/flags/FX state, arguments, stack cleanup and native return effects.
- Private COM-only and Fleet-chain image guards check profile rejection,
  partial retry, publication, invalidation, reentry, thread/device mismatch,
  retirement and budgets. They never execute a native graphics operation.
- Seven reference previews remain byte identical and seven protected installed
  binaries/configurations retain their prior hashes.

The next required work remains in the [fixed nine implementation gates](native11-stage51-65.md):
complete fresh world/material/FOW production, remaining world and light domains,
device/restart handling, complete UI/effects/video/menu production, presentation
ownership and independent game startup. Fixture counts are not completion
percentages. The game bootstrap experiment uses a separate private copy and
receipt; legacy startup would not prove independent native DX11 gameplay.

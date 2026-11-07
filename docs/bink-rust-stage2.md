# Rust Bink runtime and removal trial

30 October 2026. The primary Bink API is implemented in an optional Rust runtime
inside A2FOExtensions, and the selected FO test/test installation has a separate
Bink-free trial. **Live installation deployment and in-game acceptance are pending.**
This is not a claim that every Armada executable variant can now retire Bink.

1 November correction: the first manual launch exposed FleetOpsHook's legacy
checksum error path. The [startup-failure report](bink-startup-checksum-fix.md)
records the cause and the corrected test DLL. The same test installation now
preserves FleetOpsHook's original checksum field. The user reports no crash after
retrying; startup passes while media/rendering/gameplay acceptance remains pending.

## Implementation

The [runtime](../rust/a2fo_bink_runtime/README.md) owns movie sessions, a stable
public prefix, frame/loop/seek state, checked pixel copies, rational wait timing,
played-sample audio clocks, volume and joined DirectSound output workers. It uses
Rust decoding and ordinary Windows platform services. It never loads native Bink
or calls FFmpeg. The existing FMOD device can be borrowed through owned COM
references; this does not replace FMOD itself.

The decoder's pre-i colour, odd-size chroma, RDFT Nyquist, DCT shuffle, rounding
and packet PCM-length defects were corrected. Reported valid packet lengths trim
padding only after complete compressed-block decoding, preserving audio overlap.
Sessions retain their source handle or immutable memory snapshot across seeks.
A failed decode is sticky until a successful reposition; bounds and explicit
unsupported-mode errors remain in place.

The alternate export contract retains all 198 existing A2FO names, ordinals and
forwarders, then adds the early-attach bridge and eleven Bink aliases. The installed
`_BinkSetVolume@8` label points to a three-argument stdcall body (ret12), matching
the checked FleetOpsHook adapter that adds track zero to Armada's two-argument
call. The native label alone would have described the wrong calling convention.

`--bink-runtime` is an opt-in build profile; ordinary builds retain their existing
nostd/abort configuration. The private profile hosts Rust std and contains decoder
panics at the Bink ABI. The paired startup proxy invokes the optional early-attach
bridge after FleetOpsHook attaches. This preserves early renderer/gameplay hook
installation even though import redirection maps A2FO before FleetOpsHook.
The native intro provider-owner check recognizes A2FO's replacement export.

Private import transforms are gated by exact input SHA-256 values. Only Bink import
descriptor Name RVAs, previously zero raw padding and engine PE checksums change.
FleetOpsHook retains its pinned legacy checksum field because recomputing it
enters its defective startup error path. Every
IAT/ILT entry, imported function, executable byte, file size, resource and section
extent remains fixed. New exact external fingerprints preserve existing profile
checks; no address or geometry validation is weakened. Wine SEC_IMAGE mapping
reads both new import names from backed padding in the existing mapped page.
Native Windows loader acceptance remains for platform testing.

## Headless evidence

- Full video: **191/191 unique files, 43,833 complete frames**, exact visible YUV
  SHA-256 agreement with FFmpeg 8.1.2; complete file/memory scanner parity.
  The Windows x86 scanner also matches all 191 full video hashes and metadata.
  Its first parallel Wine startup timed out on one file; the preserved serial
  retry passed in 5.8 seconds. This is recorded separately from the initial run.
- Full audio: **114/114 files**, 70,598,266 interleaved samples, packet-trimmed
  FFmpeg comparison maximum signed-16 difference **1**. The independent packet
  grouping removes 52,574 padded sample frames. **Windows x86 independently
  passes all 114 files against the same one-unit criterion.** Windows/Linux
  hashes differ on 113 audio files, so hash differences were followed by actual
  PCM comparisons. This is not bit-exact audio.
- Native audio: all 114 files agree on rate/channels, complete sample counts and
  per-packet counts. **81 pass and 33 fail** the fixed native waveform criterion.
  Thirty-two failures have 8-bit output differences of 2–7 codes; one STIntro
  16-bit case reaches 3,845. Targeted triage shows FFmpeg has the same large
  difference from the old DLL, while Rust follows FFmpeg within one signed-16
  unit. The cause of the old waveform is unresolved; listening acceptance remains.
- Actual assembled A2FO DLL: device-free Windows API smoke covers file, memory
  snapshot and preload input, repeated DoFrame, skipped NextFrame repair, seek,
  copy guards, invalid buffers/handles, waits, volume stack cleanup and close.
  The process confirms **no binkw32.dll module is loaded**.
- Native loader fixtures: preloaded core -> FleetOpsHook attachment -> optional
  early bridge -> deferred initialization, plus bridge refusal and legacy-core
  compatibility. All three pass without executing game code.
- Fake DirectSound COM tests: drain/restart cursor overshoot, safe-write-region
  rebasing, Stop-time cursor advance, fractional cursor accumulation and worker
  join/reference release. These create no device/window or input hooks.
  All 17 Windows runtime tests and all 309 main-core host tests with std-host pass.
- Actual movie state: seek/skip PCM suffixes, repeated-decode idempotence and
  loop reset are compared to an independent sequential session with a mock sink.
  Optimized integration tests pass on both BIKf mono B1COL6 and BIKi stereo Intro.

Receipts: [full video](../build/bink-rust-stage2/full-video-v4/receipt.json),
[FFmpeg audio](../build/bink-rust-stage2/ffmpeg-audio-full-v4/receipt.json),
[Windows video](../build/bink-rust-stage2/windows-full-v4/receipt.json),
[Windows PCM](../build/bink-rust-stage2/windows-ffmpeg-audio-full-v4/receipt.json),
[Windows scanner chain](../build/bink-rust-stage2/windows-ffmpeg-audio-full-v4/scanner-chain.json),
[native audio diagnostic](../build/bink-rust-stage2/native-audio-full-v4/receipt.json),
[loader fixtures](../build/bink-rust-stage2/startup-fixture/receipt.json),
[padding mapping](../build/bink-rust-stage2/abi/padding-map-receipt.json),
[core build](../build/bink-rust-stage2/core-build/rust-dll/receipt.json),
[staging](../build/bink-rust-stage2/test-installation.receipt.json).

The [final acceptance manifest](../build/bink-rust-stage2/acceptance.json) records
the exact hashes and headless gates; the [testing checklist](../build/bink-rust-stage2/TESTING.txt)
gives the manual launch path. The staged core SHA-256 is
`b3afa28cb236956f4b28d4279d75998d1ba965b95ef7a98bedc5b8501d54fcb8`.

The immutable comparison scanner is `build/bink-rust-stage2/frozen-decoder-v4`.
Its Linux SHA-256 is cbccac9fedf4b89f7b519ea63ec090f72aee146b5314217907d4f6482cf5d56e;
its Windows SHA-256 is 573433e714485a3da133c627b0037c0ea85a93cf5d8bff85775573c94091246d.
Earlier v1/v3 outputs include diagnostic failures or an older scanner; they are
not substituted for the v4 receipts.

## Trial and acceptance

Launch `build/bink-rust-stage2/test-installation/Data/ArmadaL.exe` with Data as the
working directory and the existing working Wine/game configuration. Select the
same **fo test** and **test** mods. The sibling quarantine retains the three Bink
files and all replaced DLLs/executables/profiles, with hashes. The prior accepted
trial and the live Roots installation remain intact. Return to the live executable
for rollback; never copy quarantine files into the new trial during acceptance.

Check actual intro/movie video and sound, animated menu/movie textures, 3D menu
scenes, emissives, volume/mute, skips, transitions/loops, repeated mod launches,
and clean shutdown. Verify ordinary gameplay, save/load and rendering regressions
as appropriate. Compare against the original installation and retain fresh
A2FOExtensions/A2FOStartupProxy logs with the exact executable path and symptom.
Matched-build multiplayer remains pending until a second player is available.

Do not deploy the live removal until the exact trial passes those tests. If a
movie/audio/graphics path regresses, preserve its log and source filename and
fix that path before replacing anything else. Other older executable variants
need five additional buffer/DirectDraw/change-rectangle Bink APIs; those are not
implemented by this primary-API trial. Archive discovery, real alpha assets,
negative pitch/unobserved flags and external BINKSND clients remain unsupported
or unaccepted.

The vendored decoder is GPL-3.0-or-later. This private evaluation is optional;
production licensing and distribution must be resolved before wider packaging.
This work does not remove `fmodex.dll`, D3D providers or unrelated legacy DLLs.

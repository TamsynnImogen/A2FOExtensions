# Rust Bink replacement: first implementation milestone

Historical stage-one record. The [stage-two integration record](bink-rust-stage2.md)
supersedes its current-status statements and prefix-only/audio limitations.

30 October 2026. **Implemented an isolated Rust frame decoder/session and comparison tools. It is not yet connected to A2FOExtensions' game-facing Bink API. Keep the installed `binkw32.dll`.** No game DLL, hook, engine executable, production build dependency or live installation file was changed.

## What now exists

- [`rust/a2fo_bink_candidate`](../rust/a2fo_bink_candidate/README.md): a private Rust library and headless CLI that opens BIK files, pulls YUV420 frames and PCM audio, retains alpha separately when present, and supports restart/seek by reopening and decoding preceding frames.
- [`third_party/rust-bink-evaluation`](../third_party/rust-bink-evaluation/PATCH_NOTES.md): a pinned, patched evaluation copy of `infinitier_bik_decoder` 0.0.16. Its original registry archive, source/diff hashes, GPL text and provenance are retained. It is not a production dependency.
- [`tools/check_rust_bink_candidate.py`](../tools/check_rust_bink_candidate.py): an independent FFmpeg CLI comparison harness. FFmpeg is a test oracle only; the candidate does not link or call it.
- Bounded header/pixel/packet/track/index checks, an accumulated PCM allocation cap, explicit error returns and a panic boundary around upstream decoding. These are prototype controls, not proof of safety on arbitrary corrupt content.

The prototype emits visible Y/U/V rows without plane padding, optional alpha and first-track signed 16-bit PCM, with JSON dimensions, rational frame rate, frame/sample counts and byte sizes. It rejects multiple audio tracks. Seeking is sequential decode/discard, with no keyframe acceleration or claim of native `BinkGoto` equivalence.

## Bugs found and corrected

The unmodified candidate decoded files without reporting errors but produced incorrect pixels:

1. Pre-BIKi colour handling was disabled at the caller. BIKf black frames became Y=240, U=0, V=0 instead of Y=16, U=128, V=128. The patch enables the existing legacy colour transform for pre-i variants.
2. Odd-size chroma decoding used floor-half dimensions. A real 1×1 placeholder movie kept zero chroma instead of decoding its U/V values. The non-BinkB path now uses ceil-half dimensions.

The patches were checked against the FFmpeg 6.1 source algorithm and actual decoded output. A separate patch bounds expanded PCM before appending audio blocks. Synthetic regressions and exact patch/provenance records accompany the vendor tree. No global Cargo registry files were edited.

## Verification results

| Check | Result | Limit |
| --- | --- | --- |
| Loose movie corpus | 438 occurrences deduplicated to **191 distinct complete-file hashes**: 104 BIKf and 87 BIKi. | Refreshed the previous inventory; no new-file or archive discovery. |
| Video comparison | **191/191 files, 2,771 frames**, byte-for-byte YUV agreement with FFmpeg 8.1.2. | Up to the first 16 frames each; 19 short files had all source frames compared. Longer files are not fully validated. |
| Seek comparison | Four files starting at frame 8, up to eight frames each, match FFmpeg and the sequential Rust outputs. Audio suffixes also reproduce the sequential Rust outputs. | Confirms repeatability of decode-from-start seeking, not native Bink timing or sample alignment. |
| Windows x86 build/run | `i686-pc-windows-gnu` build succeeded. Four headless console decode cases ran under an existing private Wine prefix and matched Linux video bytes. | Not game, native API, presentation or device acceptance. Stereo intro PCM had 16 one-unit sample differences between targets, with equal lengths. |
| Unit checks | Adapter **5**, vendored decoder **44**, harness **3** passed, independently rerun during review. | Does not establish the full Bink ABI or arbitrary malformed-input robustness. |
| Malformed-input controls | **10/10** rejected: signature, FPS, frame/pixel/track/packet limits, index/header bounds, truncation and overflowing audio prefix. | Specific negative cases, not fuzzing or a security audit. |
| Stability | Compiled input source pins and all 191 input file hashes stayed unchanged through validation. Live Bink/FMOD/core/FleetOpsHook DLL hashes remain unchanged. | Input immutability is still an assumption for each active prototype session. |

Receipts: [corpus](../build/bink-rust-stage1/corpus.json), [video/audio-prefix matrix](../build/bink-rust-stage1/corpus-prefix16-v2/receipt.json), [seek matrix](../build/bink-rust-stage1/seek8-v2/receipt.json), [sequential checks](../build/bink-rust-stage1/seek-sequential-checks.json), [Windows console runs](../build/bink-rust-stage1/wine-console-v2/receipt.json), [negative cases](../build/bink-rust-stage1/malformed-v2/receipt.json), [source/input/live-file checks](../build/bink-rust-stage1/post-checks.json), [milestone record](../build/bink-rust-stage1/milestone.json).

### Audio is not accepted yet

The matrix observed PCM from 114 audio-bearing files. Their chosen sample-prefix lengths matched the oracle, but **none of those prefixes were bit-exact**. The largest observed signed-16-bit sample difference was 87, with worst observed RMS difference about 21.57 units. These are measurements, not an approved tolerance or proof of inaudibility.

The harness trims the oracle to the candidate's reported sample count and requests its reported rate/channels. Therefore equal lengths do not independently prove correct packet sample counts, rate, channel configuration, stream duration or synchronization. Those contracts need a separate, packet-aware audio validation pass. The four-file seek test establishes reproducible Rust audio state, not equivalence to native Bink audio behavior.

No alpha-bearing asset occurred in this corpus run. Alpha, multiple tracks, full-length decoding, native render formats/flags, pacing and device output remain unaccepted. After a decoder error, discard/reopen the session rather than retrying its partially advanced state. Do not replace or edit a source during decoding: seek and audio prechecks reopen it by pathname and could otherwise observe a different file version.

## Build and reproduce

```sh
CARGO_TARGET_DIR=build/bink-rust-stage1/candidate/target cargo test --locked --offline --manifest-path rust/a2fo_bink_candidate/Cargo.toml
CARGO_TARGET_DIR=build/bink-rust-stage1/candidate/target cargo build --locked --offline --release --manifest-path rust/a2fo_bink_candidate/Cargo.toml
CARGO_TARGET_DIR=build/bink-rust-stage1/candidate/target cargo build --locked --offline --target i686-pc-windows-gnu --manifest-path rust/a2fo_bink_candidate/Cargo.toml
python3 -m unittest tools.tests.test_rust_bink_candidate_check -v
```

For a new private comparison directory:

```sh
python3 tools/check_rust_bink_candidate.py --candidate build/bink-rust-stage1/frozen-candidate-v2/bink_candidate --input '/absolute/path/to/movie.bik' --output-dir build/bink-rust-stage1/new-comparison --max-frames 16 --audio-prefix
```

Existing output directories are refused. Proprietary BIK files and generated decoded streams remain local test artifacts, not bundled source fixtures. The candidate's pinned lockfile and [Windows dependency inventory](../build/bink-rust-stage1/windows-dependencies.json) contain no identified native codec/build wrapper; normal Windows/Wine platform runtime services remain external.

Frozen tested binaries:

- Linux release SHA-256: `4d16a460b487dafc321b7d5613622b3c6ee600ab43dff1fe020a9ccbf1020a31`.
- Windows x86 debug SHA-256: `589b2d6aed0f5e1ff47bde0ef93a61cce82a6f17dd5a0c8242f072fcef3e6a94`.

## What comes next before A2FO can replace the DLL

1. Reconcile audio arithmetic/sample counts and validate full-length playback, tracks, alpha, corrupted inputs and performance. Settle the decoder's GPL dependency/provenance choice before any production link; this experiment does not change production licensing.
2. Freeze the complete engine-facing Bink layout/flags/calling conventions and error/lifetime contract. Width/height at handle offsets 0/4 and an observed CopyToBuffer type `0x13` are evidence, not a complete ABI specification. Do not fabricate the remaining structure or copy semantics.
3. Implement the required Bink API in the retained main DLL, with Rust-owned sessions and compatible frame/audio presentation. Update old provider checks and preserve current exports/ordinals. Modern full-window movie playback alone cannot replace menu/texture movie consumers.
4. Retarget every supported engine/FleetOpsHook import and dynamic load in private checked copies, preserve startup ordering, and prove operation with no Bink DLL available. Finish in-game menu/texture/movie/audio/seek/shutdown acceptance before backed-up deployment.

The [combined Bink/FMOD retirement plan](bink-fmod-retirement-plan.md) still governs the endpoint. This milestone supplies tested Rust decoding foundations; it does not retire the DLL or mark A2FOExtensions' Bink takeover complete.

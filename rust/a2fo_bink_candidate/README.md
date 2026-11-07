# Isolated Bink decoder candidate

This GPL-3.0-or-later research candidate uses a [bounded vendored patch](../../third_party/rust-bink-evaluation/PATCH_NOTES.md) of `infinitier_bik_decoder` **0.0.16** from crates.io, original registry SHA-256 `cb63de0d1637c7ccc26598e8b86eabd650847ad3729905271eae507f588a926e`. `Cargo.lock` pins the entire graph. It is an rlib and headless CLI; it is not wired into any game DLL, hook, ABI, or deployed module.

Use `bink_candidate --version` for the candidate and vendored-source version string. Build with `CARGO_TARGET_DIR=/home/tamsynn/A2FOHookExtensions/build/bink-rust-stage1/candidate/target cargo build --manifest-path rust/a2fo_bink_candidate/Cargo.toml`. Run:

```
build/bink-rust-stage1/candidate/target/debug/bink_candidate decode INPUT.bik --output-dir NEWDIR --max-frames 16 [--start-frame 0]
```

`NEWDIR` must not exist. The CLI writes `video.yuv` as consecutive tightly packed visible Y, U, V planes per frame (YUV420P/I420); `alpha.y` when the source reports alpha; `audio.s16le` when a single audio track exists; and `metadata.json`. The source frame index is zero based. `--start-frame` reopens and decodes every earlier frame to preserve decoder state. `rewind()` does the same for frame zero. There is no keyframe seek. `next_frame()` reads one indexed video frame and its optional audio chunk. The crate's streaming API decodes only the first audio track, so this candidate rejects multiple tracks.

Metadata keys: `signature`, `width`, `height`, `chroma_width`, `chroma_height`, `fps_num`, `fps_den`, `source_frame_count`, `start_frame`, `decoded_frame_count`, `pixel_format`, `plane_order`, `has_alpha`, `audio_tracks_in_header`, `audio_sample_rate`, `audio_channels`, `audio_sample_frames`, `video_bytes`, `alpha_bytes`, `audio_bytes`, `seek_mode`. Audio fields are null when no track is present; byte and sample counts reflect only emitted frames. For FFmpeg comparisons use the exact rational `fps_num/fps_den` and the metadata's dimensions. Alpha is a separate luma byte stream.

Upstream `VideoDecoder` maps the bitstream's chroma planes to semantic `u` and `v`: `swap_planes` is true only for signature version byte `>= 'h'`, so BIKf keeps the original U then V mapping. This candidate writes semantic U then V. Full-corpus results and game-facing integration are recorded in [stage two](../../docs/bink-rust-stage2.md); native playback still requires manual acceptance.

The candidate preflights source length, frame count, pixel count, max packet, audio track count/rate, index extent, and frame payload extent before its own allocations. It checks per-frame audio prefix lengths and catches upstream panics as errors. The local vendor patch caps each decoded audio packet at 4,194,304 interleaved samples before appending a block. These checks do not establish upstream safety or correctness on arbitrary malformed media. A failed decode can leave a partial new output directory; remove that experiment output manually after inspection.

Dependency audit (`cargo tree`, host): decoder directly uses `bitflags`, `hound`, `infinitier_datasource`, `log`, and `thiserror`. `infinitier_datasource` pulls `encoding_rs`, `flate2`, and `parking_lot`; `serde_json` is this candidate's CLI metadata dependency. No C/C++ or system codec library appeared in the tree. Build-time Rust proc macros include `rustversion`, `multiversion_no_op`, and `thiserror-impl`; platform `libc` appears through `parking_lot_core`. This is a source/dependency inventory, not a security or license audit of all transitive crates.

## Full-file scanner

`bink_scan INPUT.bik NEW_JSON [--memory] [--audio-output NEW_PCM_FILE]` decodes every indexed frame. It hashes tightly packed visible Y/U/V rows, optional alpha rows, and interleaved signed 16-bit little-endian audio without writing raw video. `--audio-output` also streams the exact PCM bytes being hashed to a new file; silent inputs produce an empty PCM file. `--memory` loads the bounded source into an immutable `Arc<[u8]>` before decoding. Both output paths must name new files, and cannot alias. A decode failure leaves no accepted JSON or PCM output; a commit failure reports cleanup errors if a PCM file could not be removed. The two distinct outputs cannot be committed as one filesystem transaction.

The JSON includes source signature, dimensions, chroma dimensions, frame rate fraction, header file size, max frame size, video/audio flags, declared and decoded frame counts, audio rate/channels, whole-stream byte counts and SHA-256 hashes, per-frame video and audio hashes, per-frame audio sample-frame counts, scan mode, optional PCM output path, and elapsed seconds. An absent audio or alpha stream has a `null` whole-stream hash. Arrays align to zero-based video frame indices. Every reported hash is of candidate output; compare against an independently decoded oracle before treating it as parity evidence.

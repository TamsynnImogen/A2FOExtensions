# A2FOVideo v12

Fleet Ops movie dispatch now uses `A2FOMedia.dll`, a Rust media runtime. The
production build does not link FFmpeg or invoke FFmpeg executables. The separate
FFmpeg CLI is used only to generate and compare test fixtures.

The supported replacement set is MP4 with H.264/AAC and WebM/Matroska with
VP9/Opus/Vorbis. Video output supports 8-bit planar YUV420/422/444 and BGRA/RGBA;
audio conversion supports mono/stereo S16 or planar F32 to 48 kHz stereo S16.
Other codecs formerly registered by the FFmpeg SDK are outside this replacement
set and fail explicitly. Existing `.mp4`, `.webm`, `.mkv` lookup, extension-root
inheritance, same-stem `.bik` replacement order, binary admission and native Bink
fallback remain in the module. Native Bink decoding remains engine-owned.

Rust owns the demuxers, codec state, decoded frames, PCM, resampler, bounded
queues, workers, playback clock, GDI canvas and WinMM buffers. Video is paced
against audio device progress, with a monotonic wall-clock continuation after
audio EOF. Silent movies use the wall clock. AAC priming/valid-length metadata,
Opus pre-skip, and Matroska discard padding (once across a complete laced block)
are applied before output conversion. Natural EOF flushes and drains audio.

The versioned C API in `rust/media_ffi.h` supports seeking, cancellation and
shutdown. Seeking joins the old segment, discards queued output and restarts
both tracks at the requested time. Video uses an indexed keyframe seek, with a
sequential fallback for indexless files. Audio decodes from the beginning and
discards the exact converted prefix to preserve predictor/filter history; a
late seek can therefore take longer. The movie hook does not add a seek UI.

Shutdown disables admission, cancels Rust playback and joins workers. A
same-thread callback that closes or shuts down playback defers destruction
until the synchronous call returns; callback contexts must remain alive until
then. Already admitted native Bink calls remain engine-owned and are waited
out. Installed hook code and the media DLL remain pinned for process lifetime.
If a driver refuses to release a WinMM header, Rust retains its buffers and
retries cleanup before another modern request instead of freeing driver-owned
memory. Static codec tables also remain cached for process lifetime.

The existing native window/settings adapter remains C++; Rust owns presentation
and event polling. Output uses a top-down full-client DIB, aspect-preserving
bilinear composition, opaque black bars and a checked blit with restored DC
state. The event loop services at most 64 paints per poll, preserves pending
close/quit, and leaves other posted input to the game. Skip requires a fresh
press after a 250 ms grace period and foreground release. Reentrant movies are
declined. None of the automated tests creates windows, synthesizes input or
opens physical audio output.

Build with `make video` or `make release`. Put `A2FOMedia.dll` beside
`A2FOExtensions.dll`; for standalone use, beside the parent of `modules/`.
It is a backend, not a module to place in the module scanner directory. The
standalone module retains its two exports; the main DLL retains its 197-export
contract. The media backend exports only `A2FOMedia_GetApiV1`.

`make video-rust-test` runs the Rust unit/fixture checks. `make video-test` builds
and runs the facade and actual 32-bit media DLL with a memory-only host under
headless Wine. `tests/video_perf.cpp` also accepts `DLL --bench MOVIE` for an
unpaced 1280x720 pipeline measurement. `media_bench` separates composition,
video decoding and audio conversion. Keep benchmark loads isolated.

See [replacement evidence](../../docs/rust-media-replacement.md) for staged
artifacts, results and remaining acceptance. The earlier
[v11 diagnostics](../../docs/video-v11-2026-10-03.md) remain historical evidence.
Actual audible output, desktop coverage, A/V sync and supported native Windows
versions require manual testing before deployment.

# Rust media replacement

Phase 5 now has a production Rust backend in `rust/a2fo_media`, used by the
built-in and standalone A2FOVideo module. FFmpeg libraries and their SDK check
have been removed from the production Makefile. The superseded C++ decoder,
audio worker, converter, clock and GDI presenter are replaced; original source
bytes are preserved under `build/rust-migration-stage15/production/before/`.
The original SDK and prototype evidence have not been deleted.

The private candidate is
`build/rust-migration-stage15/production/candidate-v1/`, containing
`A2FOExtensions.dll`, `A2FOMedia.dll` and `modules/A2FOVideo.dll`.
It is staged, not deployed. Canonical build and installed game DLLs are left
untouched. Final source/artifact hashes and commands accompany the acceptance
record in `build/rust-migration-stage15/production/`.

## Ownership and dependencies

The Rust sidecar uses a standard-library runtime with pinned pure-Rust OxideAV
container/codec implementations. This keeps its allocator, panic runtime and
threads separate from the existing no-std Rust core. It exposes a versioned
cdecl C table, with only POD values, opaque integer session IDs, UTF-16 paths
and synchronous callbacks at the boundary. C++ retains hook admission,
engine-window/settings lookup and filename routing. Rust owns playback and
all newly decoded media/device resources.

Sources and MIT licences are retained in `third_party/rust-media/` with exact
upstream commits in `provenance.json`. Registry dependencies are locked and
hashed in the media build receipt. The normal/build dependency tree has no
FFmpeg bindings or native codec implementation. Windows GDI/USER32/WinMM and
the compiler's standard Windows runtime remain platform dependencies.

Two local codec patches are documented in `third_party/rust-media/PATCHES.md`:
laced-block position metadata for correct discard padding, and bounded exact
AAC cosine-basis caching. The AAC cache retains at most 16.25 MiB. Its dense
transform test is bit-exact against the original direct calculation. The Rust
SSE2 pixel path is extent-checked and feature-checked, with scalar tails and
reference tests for every supported matrix/range, siting, odd size and resize.

## Playback contract

The implemented set covers the local H.264/AAC intro conversion and generated
VP9/Opus/Vorbis, silent and short fixtures. This is narrower than all codecs
previously registered by the FFmpeg SDK. Unsupported codecs/channels/formats
fail explicitly so normal native lookup can handle the original request.
Native Bink remains the existing engine fallback.

Video uses a bounded three-frame queue. Audio uses bounded device submissions
and the played-sample clock. Both workers are joined on completion, seek,
cancellation and errors. Audio EOF drains decoder/converter/device state before
release. Seeking restarts both tracks and clocks; audio preserves history by
decoding/discarding the prefix. Reentrant close/shutdown cancels and defers
context destruction until the synchronous call returns. Startup is refused
while shutdown or an active job is draining. Open operations are also tracked.

The module prevents shutdown-caused Rust open failures from starting a new
native fallback. An already admitted native Bink call cannot be cancelled by
this Rust runtime; module shutdown waits for that engine-owned call. Driver
release failures retain their headers/data under Rust ownership, refuse new
modern playback while unresolved, and retry cleanup at startup/shutdown.

## Validation

The earlier isolated prototype is retained at
`build/rust-migration-stage15/media-candidate-v1/` and is superseded for
production ownership. Its seven videos, including all 1,235 1280x720 frames of
`MadDocSoftware.mp4`, matched the independent FFmpeg CLI YUV output exactly.
This establishes compressed video decoding, not visible presentation.

The production tests exercise real readers, conversion, the synchronous C API
and worker lifetime through an injected host. Exact short-fixture stereo48k
counts are AAC 16,798, Opus 16,800, Vorbis 16,801; silent video opens no audio.
A 100 ms audio seek removes exactly 4,800 converted frames. Video seek rejects
earlier frames; indexless WebM uses the sequential fallback. Pixel tests cover
bilinear sampling, matrices/ranges, bars, odd sizes and exact SSE2/scalar output.

The memory-host DLL fixture covers natural EOF, pre-play/mid-play seek, fresh
skip, close/destroy, poll/present/audio failures, cancellation, busy reentry,
same-thread close/shutdown, other-thread shutdown, overlapping close/shutdown,
short ABI tables and terminal handle rejection. Callback open/close counts
must balance before playback returns. A native WinMM fault test verifies that
failed unprepare retains the original allocation until a successful retry.
Facade tests cover extension-root/BIK lookup, native fallback and shutdown
reentry during both initial logging and backend open.

PE checks compare export names, ordinals and forwarders with the preserved
baseline, reject new direct dependencies in the existing DLLs, and reject
FFmpeg imports/symbols in all three new images. The main DLL has 197 exports,
the standalone module two, and the backend one. The dependency check also
rejects FFmpeg static symbols, since a DLL import check alone is insufficient.

The final candidate passed 21 host unit tests, three reader-fixture tests and
one lacing test, plus 22 native i686 unit tests and 23 real-DLL playback cases.
The actual C++ backend loader/open/close/shutdown path and Rust-owned main DLL
load smoke also passed. Export/ordinal contracts match the baseline; no FFmpeg
symbols remain in any of the three candidates.

With no concurrent builds, the complete 1,235-frame intro pipeline took
25.683 seconds unpaced at 1280x720, including all 1,976,927 stereo48k audio
frames. A real-time memory-host run scaled to 1920x1080 and completed every
frame/sample in 41.400 seconds for 41.186 seconds of media. It opened and closed
one audio output. Isolated composition measured 60 frames in 672.506 ms at
720p and 1,225.144 ms scaling 720p to 1080p; isolated audio took 9,058.844 ms.
These are headless CPU/clock results, not audible or visible acceptance.

Final hashes, commands and results are recorded in
`build/rust-migration-stage15/production/acceptance.json`. Initial full-size
composition failed the throughput check; that run is retained as failed
evidence, not accepted performance proof.

## Remaining acceptance

Automated tests do not establish audible sound, perceived motion, real device
position accuracy, visible compositor coverage or in-game A/V synchronization.
Manually test a fresh full game launch, consecutive modern/native movies,
windowed/borderless sizing, skip/close, and shutdown with real video/audio.
Current standard-library imports include Windows API-set libraries and
`bcryptprimitives.dll`; older native Windows compatibility has not been
established. No deployment or full Rust-rewrite completion is claimed here.
Historical stage-nine FFmpeg whole-module fixtures do not validate the new
backend; use the production media checks above.

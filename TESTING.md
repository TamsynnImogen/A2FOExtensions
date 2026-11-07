# A2FOVideo v11 testing

V11 converts/scales AVFrame pixels directly into the owned black DIB frame,
then uses the existing checked physical present. Video decoding is capped at
four CPU threads and silent movies avoid a redundant audio-open thread. The
v10 display was reported essentially working; low FPS and pauses between every
movie prompted this performance pass. Watch `Movie performance`,
`Movie first frame`, `Decoder startup`, `Modern movie lookup`,
`Movie transition: gap before request` and `Movie request finished`.

V8 additionally requires release and a fresh press to skip modern movies.
V7 introduced saved movie geometry and audio draining.

V10 additionally services window layout/paint events during frame pacing,
with close/quit preservation and a same-thread recursive-playback guard. The
v9 live screenshot remained wrong despite full window/DC rectangles. Check
`Movie HWND after layout service`, `Movie device surface`, and the event-service
summary in the fresh log. This candidate still requires a live coverage test.

V9 introduced presentation of a buffered physical frame through a fresh cached DC
and logs native/main/foreground window identities and output clipping. At
1920x1080 borderless, a 1280x720 source should fill the whole client without
showing the desktop on the sides. At a different aspect ratio, margins should
be black. Check `Movie output DC` and `Modern movie physical frame presented`.

## Manual local test

Fully restart Fleet Ops Roots, select the test mod, and let the
`MadDocSoftware.mp4` replacement play. It should have sound; `intro.mp4` remains
silent because its source BIK has no audio track.

Test a framed 1280x720 selection, a borderless 1600x900 selection, and desktop
(0x0) on the 1920x1080 display. The movie client must match each selection,
framed windows should be centred within the work area, and video should preserve
its aspect ratio. Confirm the menu is usable afterwards and skipping works.

Expected current log lines include:

```text
[A2FOVideo] Initialized v11: ...
[A2FOVideo] Movie window: windowed; position=...; outer=...; adapter=0
[A2FOVideo] Movie presentation: selected=1280x720 windowed; target client=1280x720; geometry=applied
[A2FOVideo] Audio stream: codec=aac, rate=22050, channels=1, format=...
[A2FOVideo] waveOutOpen: code=0
[A2FOVideo] Audio completed: ... PCM bytes submitted
```

After launch, let MadDocSoftware continue beyond the first second without
pressing a skip control. It should continue instead of ending at nine frames.
Then release all skip controls and deliberately press Space or Escape to test
skipping. A held skip key should not also skip the following modern movie.
The log records `Movie skip input: initial mask=...` and, only for an accepted
fresh press, `Movie skip input: fresh mask=...`.

On skip, `Audio cancelled` is expected. For `intro.mp4`, `No audio stream found`
is expected. Save `Data/A2FOExtensions.log` after any failed run before restarting,
as it is overwritten by startup.

Headless tests verify synthetic window geometry and real libav PCM output, not
physical window placement, audible Wine output, or perceived A/V sync.
For a headless throughput benchmark (memory DIB, no pacing or live window):

```bash
make build/video_perf.exe
env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine build/video_perf.exe build/video-fixtures/MadDocSoftware.mp4 4
```

The runtime `rendered` rate includes pacing and should approach the source rate
(30/29.97 fps for the current conversions), not the benchmark's maximum rate.
A large transition gap precedes the next request, while lookup/startup timings
measure work inside modern playback. Native request duration includes the whole
BIK movie; it does not identify native first-frame latency.

The current deployment record is in `docs/video-v11-2026-10-03.md`.

## Historical v6 diagnostics

V6 keeps the v5 embedded-libav movie hook and focuses on diagnostics / presentation selection.

## Changes

- Chooses the largest visible ownerless Fleet Ops top-level window for modern movie presentation instead of blindly accepting the temporary intro/movie form.
- Reads the saved Fleet Ops graphics selection directly from the same `FoSettings` fields used by `A2FOShellDisplay` and logs the selected resolution/window mode beside the actual presentation client size.
- Adds explicit audio diagnostics: missing audio stream, codec/sample rate/channels/sample format, `swr_*` setup errors, and WinMM `waveOutOpen` / buffer submission errors.
- Keeps MP4/WebM/MKV lookup, embedded libav decoding, Wine compatibility and native BIK fallback unchanged.

## Important test-file note

The `intro.bik` supplied during development and the converted `intro.mp4` both contain **video only**. They cannot be used to verify audio playback. V6 should log `No audio stream found in modern movie` for that MP4. Use a separate MP4/WebM/MKV containing AAC, Opus, Vorbis, MP3, etc. to test sound.

## Expected log lines

```text
[A2FOVideo] Initialized v6: ...
[A2FOVideo] Movie presentation: selected=1920x1080 borderless; target client=1920x1080
[A2FOVideo] No audio stream found in modern movie: ...
```

For a movie with audio:

```text
[A2FOVideo] Audio stream: codec=aac, rate=48000, channels=2, format=...
[A2FOVideo] waveOutOpen: code=0
```

If `selected` is correct but `target client` is still an authored size such as 1280x960, the remaining resolution problem is in the native intro/window lifecycle rather than libav scaling; the two values make that distinction explicit.

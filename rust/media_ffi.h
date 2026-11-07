#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_MEDIA_VERSION 1u
/* Callbacks are cdecl, synchronous, never unwind, and do not retain buffers.
 * Audio callbacks run on the audio worker; their context must remain live and
 * support concurrent now/sleep calls until play returns. No callback after synchronous play returns.
 * A null host selects Rust's native Win32/GDI/WinMM implementation. */
typedef struct A2FO_MediaPoll {
    uint32_t width, height, keys, foreground, valid, closing;
} A2FO_MediaPoll;
typedef struct A2FO_MediaHostV1 {
    uint32_t size, version;
    void* context;
    int64_t (*now_us)(void*);
    void (*sleep_ms)(void*, uint32_t);
    uint32_t (*poll)(void*, uint32_t, A2FO_MediaPoll*);
    uint32_t (*present)(void*, const uint8_t*, uint32_t, uint32_t, uint32_t);
    uint32_t (*audio_open)(void*);
    /* 1 queued, 0 temporarily full, 2 failed. Stereo48k S16 frames. */
    uint32_t (*audio_write)(void*, const int16_t*, uint32_t);
    uint64_t (*audio_position)(void*);
    /* 0 drained, 1 pending, 2 failed. */
    uint32_t (*audio_pending)(void*);
    void (*audio_reset)(void*);
    void (*audio_close)(void*);
} A2FO_MediaHostV1;
typedef struct A2FO_MediaOpen {
    uint32_t size, version;
    const uint16_t* path;
    uint32_t path_length, threads;
    void (*log)(void*, const char*);
    void* log_context;
} A2FO_MediaOpen;
typedef struct A2FO_MediaInfo {
    uint32_t size, width, height, has_audio;
    int64_t duration_us;
} A2FO_MediaInfo;
typedef struct A2FO_MediaPlay {
    uint32_t size, version;
    uintptr_t window;
    const A2FO_MediaHostV1* host;
} A2FO_MediaPlay;
/* play: 0 failed (native fallback allowed); 1 completed; 2 cancelled;
 * 3 busy (consumed, do not start another native player). shutdown/close waits
 * for other-thread jobs; same-thread reentrant shutdown cancels and defers
 * destruction until play returns. In that reentrant case the host context must
 * remain alive until play returns. startup is refused while shutdown, open, or
 * playback is still draining. DLL stays pinned through installed hooks. */
typedef struct A2FO_MediaApiV1 {
    uint32_t size, version;
    uint32_t (*open)(const A2FO_MediaOpen*);
    uint32_t (*info)(uint32_t, A2FO_MediaInfo*);
    uint32_t (*play)(uint32_t, const A2FO_MediaPlay*);
    uint32_t (*seek)(uint32_t, int64_t);
    void (*cancel)(uint32_t);
    void (*close)(uint32_t);
    uint32_t (*shutdown)(void);
    uint32_t (*startup)(void);
} A2FO_MediaApiV1;
typedef const A2FO_MediaApiV1* (*A2FO_MediaGetApi)(uint32_t);
#ifdef __cplusplus
}
#endif

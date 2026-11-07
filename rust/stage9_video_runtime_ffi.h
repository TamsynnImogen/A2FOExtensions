#ifndef A2FO_STAGE9_VIDEO_RUNTIME_FFI_H
#define A2FO_STAGE9_VIDEO_RUNTIME_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage9VideoStep)(void*, uint32_t, uintptr_t);

uint8_t a2fo_rs_stage9_video_resolve(A2FO_Stage9VideoStep, void*, uint8_t, const uint32_t*);
uint8_t a2fo_rs_stage9_video_replacement_kind(const char*);
uint8_t a2fo_rs_stage9_video_replacement(A2FO_Stage9VideoStep, void*, uint8_t);
uint8_t a2fo_rs_stage9_video_play(A2FO_Stage9VideoStep, void*);
uint8_t a2fo_rs_stage9_video_event_poll(A2FO_Stage9VideoStep, void*, uint8_t, uint8_t);
uint8_t a2fo_rs_stage9_video_wave_submit(A2FO_Stage9VideoStep, void*);
uint8_t a2fo_rs_stage9_video_wave_drain(A2FO_Stage9VideoStep, void*, uintptr_t);
void a2fo_rs_stage9_video_wave_close(A2FO_Stage9VideoStep, void*, uintptr_t);
uint8_t a2fo_rs_stage9_video_audio_decode(A2FO_Stage9VideoStep, void*);
uint8_t a2fo_rs_stage9_video_video_decode(A2FO_Stage9VideoStep, void*);
void a2fo_rs_stage9_video_audio_join(A2FO_Stage9VideoStep, void*, uint8_t, uint8_t);
uint8_t a2fo_rs_stage9_video_open_decoder(A2FO_Stage9VideoStep, void*);

struct A2FO_Stage9VideoSkipState {
    uint64_t ready_at;
    uint32_t previous;
    uint8_t armed;
};
uint32_t a2fo_rs_stage9_video_skip_poll(struct A2FO_Stage9VideoSkipState*,
                                         uint32_t, uint8_t, uint64_t);
uint8_t a2fo_rs_stage9_video_result(uint8_t, uint64_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif

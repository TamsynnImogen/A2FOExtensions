#ifndef A2FO_STAGE10_VIDEO_COMPLETION_FFI_H
#define A2FO_STAGE10_VIDEO_COMPLETION_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage10VideoStep)(void*, uint32_t, uintptr_t);

uint8_t a2fo_rs_stage10_video_audio_frame(A2FO_Stage10VideoStep, void*, uint8_t, uint8_t);
uint8_t a2fo_rs_stage10_video_wait(A2FO_Stage10VideoStep, void*, uint8_t);
uint8_t a2fo_rs_stage10_video_present_admission(A2FO_Stage10VideoStep, void*);

#ifdef __cplusplus
}
#endif

#endif

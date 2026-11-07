#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsIntroPlan {
    uint32_t source_width, source_height;
    int32_t width, height, fitted_width, fitted_height, left, top, pitch;
} A2foRsIntroPlan;
typedef struct A2foRsIntroFrame { uint8_t draw, drawn, copy_ok; } A2foRsIntroFrame;
typedef uintptr_t (*A2foRsIntroStep)(void*,uint32_t,A2foRsIntroPlan*);
typedef int32_t (*A2foRsIntroNext)(void*,uint32_t);
typedef uint8_t (*A2foRsIntroDraw)(void*);
uint8_t a2fo_rs_intro_draw(uint8_t bink, uint8_t pixels, int32_t pitch,
    uint32_t buffer_height,uint32_t x,uint32_t y,uint32_t flags,
    A2foRsIntroStep step,void* context);
uint8_t a2fo_rs_intro_present(uint8_t buffer,A2foRsIntroStep step,void* context);
uint8_t a2fo_rs_intro_scope(uint8_t enabled,uint8_t ready,uint8_t bink,uint8_t window);
void a2fo_rs_intro_copy(A2foRsIntroFrame*,uint8_t ready,uint8_t enabled,
    uint8_t same_bink,int32_t result,A2foRsIntroDraw,void*);
int32_t a2fo_rs_intro_next(A2foRsIntroFrame*,uint8_t ready,uint8_t enabled,
    uint8_t same_bink,uint8_t show,A2foRsIntroNext,void*);
uint8_t a2fo_rs_intro_init(uint8_t valid_api,uintptr_t expected_next,A2foRsIntroStep,void*);
#ifdef __cplusplus
}
#endif

#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsTooltipMeasure {
    float raw,native_duration,modifier,displayed;
    const uint8_t* bytes;
    size_t size;
} A2foRsTooltipMeasure;
typedef uintptr_t (*A2foRsTooltipStep)(void*,uint32_t,uint32_t,A2foRsTooltipMeasure*);
uint8_t a2fo_rs_runtime_tooltip_prepare(A2foRsTooltipStep,void*);
uint8_t a2fo_rs_runtime_tooltip_call(uint8_t,uint8_t,uint8_t,uint8_t,
    A2foRsTooltipStep,void*);
uint8_t a2fo_rs_runtime_tooltip_override(uint8_t,uint8_t,const char*);
uint8_t a2fo_rs_runtime_tooltip_init(uint8_t,A2foRsTooltipStep,void*);
void a2fo_rs_runtime_tooltip_shutdown(A2foRsTooltipStep,void*);
typedef struct A2foRsXpBytes {const uint8_t* data;size_t size;} A2foRsXpBytes;
typedef struct A2foRsXpReply {
    A2foRsXpBytes first,second,third;
    uint64_t word,other,third_word;
    double number,second_number;
} A2foRsXpReply;
typedef struct A2foRsXpProgress {
    uint32_t levels_gained,consumed,level;
    double pool,threshold;
} A2foRsXpProgress;
typedef void (*A2foRsXpEmit)(void*,A2foRsXpBytes);
typedef uint8_t (*A2foRsXpField)(void*,uint32_t,A2foRsXpBytes*,A2foRsXpBytes*);
typedef uint8_t (*A2foRsXpParse)(void*,A2foRsXpBytes,double*,uint32_t*);
typedef uint64_t (*A2foRsXpStep)(void*,uint32_t,A2foRsXpReply*);
void a2fo_rs_runtime_xp_normal(A2foRsXpBytes,A2foRsXpEmit,void*);
void a2fo_rs_runtime_xp_field(uint32_t,A2foRsXpBytes,A2foRsXpField,A2foRsXpEmit,void*);
double a2fo_rs_runtime_xp_number(A2foRsXpBytes,double,A2foRsXpParse,void*);
uint8_t a2fo_rs_runtime_xp_prepare_chain(A2foRsXpBytes,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_family(A2foRsXpBytes,A2foRsXpStep,A2foRsXpEmit,void*);
void a2fo_rs_runtime_xp_class(uint8_t,uintptr_t,A2foRsXpBytes,uint32_t,
    A2foRsXpField,A2foRsXpParse,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_race(uint8_t,uintptr_t,uint8_t,uint32_t,
    A2foRsXpField,A2foRsXpParse,A2foRsXpStep,void*);
uint8_t a2fo_rs_runtime_xp_configure(uint32_t,A2foRsXpStep,void*);
uint8_t a2fo_rs_runtime_xp_any_enabled(A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_report(uint32_t,A2foRsXpProgress,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_reconcile(uint32_t,uint8_t,uint8_t,A2foRsXpStep,void*);
int32_t a2fo_rs_runtime_xp_erase(uintptr_t,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_observe(uintptr_t,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_recalculate(uint8_t,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_craft_event(uint8_t,uint32_t,uintptr_t,uint8_t,
    A2foRsXpStep,void*);
uint32_t a2fo_rs_runtime_xp_save(uint8_t,uintptr_t,uintptr_t,A2foRsXpStep,void*);
uint32_t a2fo_rs_runtime_xp_load(uint8_t,uintptr_t,uintptr_t,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_clean(A2foRsXpStep,void*);
int32_t a2fo_rs_runtime_xp_award(uint8_t,uint8_t,uintptr_t,uintptr_t,A2foRsXpStep,void*);
int32_t a2fo_rs_runtime_xp_level(uint8_t,uintptr_t,A2foRsXpStep,void*);
uintptr_t a2fo_rs_runtime_xp_resolve(uint8_t,uintptr_t,uintptr_t,A2foRsXpStep,void*);
typedef struct A2foRsXpPublicProgress {uint32_t level;double current,required;} A2foRsXpPublicProgress;
uint8_t a2fo_rs_runtime_xp_get_progress(uint8_t,uint32_t,A2foRsXpPublicProgress*,
    A2foRsXpStep,void*);
uint8_t a2fo_rs_runtime_xp_is_enabled(uint8_t,uintptr_t,A2foRsXpStep,void*);
uint8_t a2fo_rs_runtime_xp_init(uint8_t,A2foRsXpStep,void*);
void a2fo_rs_runtime_xp_shutdown(A2foRsXpStep,void*);
#ifdef __cplusplus
}
#endif

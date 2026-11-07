#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* All callbacks are synchronous and must not unwind. Return native pointers
   and integer results as uintptr_t. Native owns strings and exception catches.
   local_team: 0 both function pointers present; 1 signed local-team index bits;
   2 team pointer for the admitted index (arg).
   override: 0 output exists; 1 construct/assign key, with native catch;
   2 key.empty; 3 manager pointer; 4 lookup function pointer; 5 invoke lookup
   and retain translated pointer; 6 translated pointer exists; 7 first char
   nonzero; 8 assign translated (arg=1) or key (arg=0), clearing output on
   caught failure; 9 output nonempty.
   localize: 0 constant empty pointer; 1 default name (arg=0) or icon (arg=1)
   pointer; 2 initialize/access thread-local text; 3 invoke override;
   4 text.c_str pointer. */
typedef uintptr_t (*A2foStage10ResourcePresentationStep)(
    void* context, uint32_t operation, uintptr_t arg);

uintptr_t a2fo_rs_stage10_resource_local_team(
    void* context, A2foStage10ResourcePresentationStep step);
uint8_t a2fo_rs_stage10_resource_override(
    uint32_t resource, uint32_t kind, void* context,
    A2foStage10ResourcePresentationStep step);
uintptr_t a2fo_rs_stage10_resource_localize(
    uint32_t resource, uint32_t kind, void* context,
    A2foStage10ResourcePresentationStep step);

#ifdef __cplusplus
}
#endif

#ifndef A2FO_STAGE8_TEXTURE_RUNTIME_FFI_H
#define A2FO_STAGE8_TEXTURE_RUNTIME_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t (*A2FO_Stage8TextureStep)(void*, uint32_t, uintptr_t);

void a2fo_rs_stage8_texture_race_policy(A2FO_Stage8TextureStep, void*,
                                        uint8_t, uint8_t);
void a2fo_rs_stage8_texture_apply_meshes(A2FO_Stage8TextureStep, void*,
                                         uintptr_t);
void a2fo_rs_stage8_texture_apply_nodes(A2FO_Stage8TextureStep, void*,
                                        uintptr_t);

#ifdef __cplusplus
}
#endif

#endif

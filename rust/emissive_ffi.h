#ifndef A2FO_RUST_EMISSIVE_FFI_H
#define A2FO_RUST_EMISSIVE_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t valid, process;
    size_t radius;
} A2foRsBloomPlan;
int32_t a2fo_rs_emissive_subsystem(uint8_t operational, uint8_t forced, int32_t maximum,
                                   double current, float disable);
uint8_t a2fo_rs_emissive_motion(uint32_t warp_state, float velocity_squared,
                                float impulse_speed);
void a2fo_rs_emissive_intensity(uint8_t state, uint32_t *output);
size_t a2fo_rs_emissive_key(const uint8_t *input, size_t length, uint8_t *output,
                            size_t capacity);
uint32_t a2fo_rs_emissive_merge(uint32_t combined, uint32_t source, uint32_t percent);
uint32_t a2fo_rs_emissive_combine(const uint32_t *pixels, uint8_t mask,
                                  const uint32_t *percent);
A2foRsBloomPlan a2fo_rs_emissive_bloom_plan(size_t length, size_t width, size_t height,
                                            size_t radius, uint32_t strength);
void a2fo_rs_emissive_bloom_process(uint32_t *pixels, const uint32_t *original,
                                    uint32_t *scratch, uint32_t *blurred, size_t length,
                                    size_t width, size_t height, size_t radius,
                                    uint32_t strength);
#ifdef __cplusplus
}
#endif
#endif

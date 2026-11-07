#ifndef A2FO_RUST_TEXT_GEOMETRY_FFI_H
#define A2FO_RUST_TEXT_GEOMETRY_FFI_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
float a2fo_rs_text_reconcile(float raw, float adjusted, float modifier);
uint8_t a2fo_rs_text_build_token(const char *separator, float seconds,
                                uint8_t compact, char *output, size_t capacity);
uint8_t a2fo_rs_text_resource_token(const char *separator, const char *name,
                                   int32_t cost, uint8_t compact,
                                   char *output, size_t capacity);
void a2fo_rs_geometry_rotate_xyz(const float *value, const float *degrees,
                                 float *output);
#ifdef __cplusplus
}
#endif
#endif

#ifndef A2FO_RUST_CONFIG_FFI_H
#define A2FO_RUST_CONFIG_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsBytes { const uint8_t *data; size_t size; } A2foRsBytes;
typedef void (*A2foRsEmit)(void *context, uint32_t kind, size_t index,
                           const uint8_t *data, size_t size);
typedef uint8_t (*A2foRsParseFloat)(void *context, const uint8_t *data,
                                   size_t size, size_t *consumed, float *value);
typedef struct A2foRsRaceInput {
    int32_t slot;
    uint8_t has_slot, has_interface, has_key, has_name;
} A2foRsRaceInput;
typedef struct A2foRsRaceEntry {
    int32_t slot;
    uint8_t replace_key;
} A2foRsRaceEntry;
typedef struct A2foRsColor { float red, green, blue; uint8_t present; } A2foRsColor;
typedef struct A2foRsColorReport { uint32_t indexed, legacy; } A2foRsColorReport;
typedef struct A2foRsArtReport {
    uint32_t valid, invalid, found;
    float emissive_bump_multiplier, bump_light_bias, emissive_diffuse_restore;
} A2foRsArtReport;
/* Byte spans remain owned by C++; callbacks must not throw across Rust frames. */
uint8_t a2fo_rs_config_fpq(const uint8_t*, size_t, uint32_t*, uint32_t*, A2foRsEmit, void*);
void a2fo_rs_config_odf_basename(A2foRsBytes, A2foRsEmit, void*);
uint8_t a2fo_rs_config_odf_alias_equivalent(A2foRsBytes, A2foRsBytes);
void a2fo_rs_config_race(const A2foRsRaceInput*, size_t, size_t,
                         A2foRsRaceEntry*, uint8_t*, int32_t*);
uint32_t a2fo_rs_config_upgrade(A2foRsBytes, uint32_t*);
void a2fo_rs_config_refit_normalize(A2foRsBytes, A2foRsEmit, void*);
void a2fo_rs_config_refit_parse(const A2foRsBytes*, const A2foRsBytes*, size_t,
                                A2foRsEmit, void*);
uint8_t a2fo_rs_config_refit_supported(A2foRsBytes, A2foRsBytes, A2foRsBytes);
uint8_t a2fo_rs_config_edit_submenu(const A2foRsBytes*, size_t);
void a2fo_rs_config_edit_normalize(A2foRsBytes, A2foRsEmit, void*);
uint8_t a2fo_rs_config_edit_parse(A2foRsBytes, A2foRsBytes, A2foRsEmit, void*);
void a2fo_rs_config_team(A2foRsBytes, A2foRsColor*, A2foRsColorReport*,
                         A2foRsParseFloat, void*);
void a2fo_rs_config_art(A2foRsBytes, A2foRsArtReport*, A2foRsEmit, void*,
                        A2foRsParseFloat, void*);
uint8_t a2fo_rs_config_disable_bump(A2foRsBytes, uint8_t*);
void a2fo_rs_config_texture_suffix(A2foRsBytes, A2foRsBytes, A2foRsEmit, void*);
#ifdef __cplusplus
}
#endif
#endif

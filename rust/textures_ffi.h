#ifndef A2FO_RUST_TEXTURES_FFI_H
#define A2FO_RUST_TEXTURES_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foRsMip {
    uint32_t width, height, row_pitch, slice_pitch;
    size_t offset;
} A2foRsMip;
typedef struct A2foRsDdsPlan {
    uint32_t format, width, height, alpha, mip_count, error;
    size_t payload_offset, payload_size;
    A2foRsMip mips[15];
} A2foRsDdsPlan;
typedef struct A2foRsUploadPlan {
    uint32_t format, width, height, row_pitch;
    size_t output_size, required_source;
} A2foRsUploadPlan;
/* All nonempty spans must point to readable/writable memory of their stated length. */
uint8_t a2fo_rs_textures_layout(uint32_t format, uint32_t width, uint32_t height,
                                 uint32_t* row, uint32_t* slice);
/* 0 success, 1 invalid argument, 2 unsupported. error identifies exact diagnostic. */
uint32_t a2fo_rs_textures_parse(const uint8_t* data, size_t size, A2foRsDdsPlan* plan);
/* Validation checks format/dimensions/alpha/count before borrowing mips.
 * A rejected count or null mips pointer needs no readable mip span. For a
 * valid count, mips must contain that many initialized records (at most 15). */
uint32_t a2fo_rs_textures_validate(uint32_t format, uint32_t width, uint32_t height,
                                    uint32_t alpha, const A2foRsMip* mips, size_t mip_count,
                                    size_t payload_size, uint32_t* error);
uint8_t a2fo_rs_textures_upload_plan(uint32_t format, uint32_t width, uint32_t height,
                                      size_t source_size, uint32_t source_pitch,
                                      A2foRsUploadPlan* plan);
uint8_t a2fo_rs_textures_upload_copy(uint32_t format, uint32_t width, uint32_t height,
                                      const uint8_t* source, size_t source_size,
                                      uint32_t source_pitch, uint8_t* output,
                                      size_t output_size);
#ifdef __cplusplus
}
#endif
#endif

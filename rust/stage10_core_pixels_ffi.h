#ifndef A2FO_STAGE10_CORE_PIXELS_FFI_H
#define A2FO_STAGE10_CORE_PIXELS_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {

struct A2foRsSparseEmissivePixel {
    std::uint32_t index;
    std::uint32_t pixel;
};

// Native code owns all buffers and COM surface locks. source rows contain
// width aligned uint32_t pixels and begin stride_bytes apart.
std::size_t a2fo_rs_stage10_core_pixels_nonblack_count(
    const std::uint8_t* source, std::uint32_t width, std::uint32_t height,
    std::size_t stride_bytes);
std::uint8_t a2fo_rs_stage10_core_pixels_sparse_is_smaller(
    std::size_t nonblack, std::size_t pixel_count);
using A2foRsSparsePush = std::uint8_t (*)(
    void* context, std::uint32_t index, std::uint32_t pixel);
// The callback performs each native vector push, catches exceptions, and
// returns zero on failure. The Rust scan then stops and returns zero.
std::uint8_t a2fo_rs_stage10_core_pixels_fill_sparse(
    const std::uint8_t* source, std::uint32_t width, std::uint32_t height,
    std::size_t stride_bytes, A2foRsSparsePush push, void* context);

// Dimensions are nonzero; source and output buffers are distinct.
void a2fo_rs_stage10_core_pixels_next_mip(
    const std::uint32_t* source, std::uint32_t width, std::uint32_t height,
    std::uint32_t* output);
void a2fo_rs_stage10_core_pixels_merge_dense(
    std::uint32_t* combined, const std::uint32_t* source,
    std::size_t count, std::uint32_t percent);
void a2fo_rs_stage10_core_pixels_merge_sparse(
    std::uint32_t* combined, std::size_t count,
    const A2foRsSparseEmissivePixel* source, std::size_t sparse_count,
    std::uint32_t percent);

}

#endif

#ifndef A2FO_STAGE9_RGB_RUNTIME_FFI_H
#define A2FO_STAGE9_RGB_RUNTIME_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {

using A2foRgbEmit = bool (*)(void* context, std::uint32_t operation,
                             const std::uint8_t* bytes, std::size_t length,
                             std::size_t capacity);
using A2foRgbAllocate = std::uint8_t* (*)(
    void* context, std::size_t length);
using A2foRgbLower = std::uint8_t (*)(std::uint8_t value);
using A2foRgbStep = std::uint32_t (*)(void* context, std::uint32_t operation,
                                      std::size_t index);

struct A2foRgbSpan {
    std::size_t start;
    std::size_t length;
};

struct A2foRgbMip {
    std::size_t stem_length;
    std::uint32_t level;
};

struct A2foRgbTgaHeader {
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t color_map_type;
    std::uint8_t image_type;
    std::uint8_t color_map_depth;
    std::uint8_t pixel_depth;
};
struct A2foRgbDiscovery {
    std::uint32_t roots_found;
    std::uint32_t rgb_roots;
    std::uint32_t index8_roots;
    std::uint32_t compressed_roots;
};

// Input is the native, already lowercased and slash-normalized filename.
// The returned span selects a substring of that input.
std::uint32_t a2fo_rs_stage9_rgb_request(
    const std::uint8_t* normalized, std::size_t length,
    A2foRgbSpan* key);
bool a2fo_rs_stage9_rgb_manual_mip(
    const std::uint8_t* key, std::size_t length, A2foRgbMip* mip);
bool a2fo_rs_stage9_rgb_mip_step(
    std::uint16_t base_width, std::uint16_t base_height,
    std::uint16_t previous_width, std::uint16_t previous_height,
    std::uint16_t current_width, std::uint16_t current_height,
    bool same_format);
bool a2fo_rs_stage9_rgb_synthesis_dimensions(
    std::uint32_t child_root, std::uint32_t inherited_root,
    std::uint16_t base_width, std::uint16_t base_height,
    std::uint32_t level, std::uint16_t* expected_width,
    std::uint16_t* expected_height);
bool a2fo_rs_stage9_rgb_inherited_is_large_enough(
    bool header_read, std::uint16_t inherited_width,
    std::uint16_t inherited_height, std::uint16_t expected_width,
    std::uint16_t expected_height);
void a2fo_rs_stage9_rgb_normalize(
    std::uint8_t* bytes, std::size_t length, A2foRgbLower lower);
bool a2fo_rs_stage9_rgb_ascii_equal(
    const std::uint8_t* left, std::size_t left_length,
    const std::uint8_t* right, std::size_t right_length,
    A2foRgbLower lower);
std::uint32_t a2fo_rs_stage9_rgb_name_key(
    const std::uint8_t* normalized, std::size_t length,
    A2foRgbSpan* key);
bool a2fo_rs_stage9_rgb_tga_filename(
    const std::uint8_t* name, std::size_t length);
bool a2fo_rs_stage9_rgb_dds_filename(
    const std::uint8_t* name, std::size_t length);
bool a2fo_rs_stage9_rgb_native_tga_header(
    const std::uint8_t* source, std::size_t length);
bool a2fo_rs_stage9_rgb_header_summary(
    const std::uint8_t* source, std::size_t length,
    A2foRgbTgaHeader* summary);
bool a2fo_rs_stage9_rgb_same_pixel_format(
    const A2foRgbTgaHeader* left, const A2foRgbTgaHeader* right);
std::uint32_t a2fo_rs_stage9_rgb_prepare_tga(
    const std::uint8_t* source, std::size_t length,
    A2foRgbEmit emit, void* context);
bool a2fo_rs_stage9_rgb_resize_tga(
    const std::uint8_t* source, std::size_t length,
    std::uint16_t target_width, std::uint16_t target_height,
    A2foRgbAllocate allocate, void* context);
bool a2fo_rs_stage9_rgb_prepare_workflow(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_resize_workflow(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_discover(
    A2foRgbStep step, void* context, A2foRgbDiscovery* discovery);
bool a2fo_rs_stage9_rgb_dds_alias(
    const std::uint8_t* key, std::size_t length, A2foRgbSpan* basename);
bool a2fo_rs_stage9_rgb_select_root(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_mapped_route(
    std::uint32_t request, bool key_is_tga,
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_fopen_route(
    std::uint32_t request, A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_scan_directory(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_find_child(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_validate_mip_chain(
    std::uint32_t level, A2foRgbStep step, void* context);
std::uint32_t a2fo_rs_stage9_rgb_join_policy(
    const std::uint8_t* left, std::size_t left_length,
    std::size_t right_length);
bool a2fo_rs_stage9_rgb_legacy_directory(
    const std::uint8_t* name, std::size_t length, A2foRgbLower lower);
bool a2fo_rs_stage9_rgb_texture_name(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_key_exists(
    A2foRgbStep step, void* context);
std::uint32_t a2fo_rs_stage9_rgb_direct_index(std::uint32_t request);
std::uint32_t a2fo_rs_stage9_rgb_root_label(std::uint32_t route);
bool a2fo_rs_stage9_rgb_clear_conversion(
    A2foRgbStep step, void* context);
bool a2fo_rs_stage9_rgb_clear_state(
    A2foRgbStep step, void* context);

}

#endif

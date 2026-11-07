#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
struct A2FoD3DMip {
    std::uint32_t width, height, row_pitch, slice_pitch;
    std::size_t offset;
};
struct A2FoD3DImage {
    std::uint32_t format, width, height, alpha;
    const A2FoD3DMip* mips;
    std::size_t mip_count;
    const std::uint8_t* bytes;
    std::size_t byte_count;
};
struct A2FoD3DConstants {
    std::int32_t pointer_error, invalid_arg, unexpected, unsupported, sdk_missing;
    std::uint32_t bc4_unorm, bc4_snorm, bc5_unorm, bc5_snorm, bc7_unorm, bc7_srgb;
    std::uint32_t level10, level11;
    std::uint32_t support_texture2d, support_sample, support_mip;
};
struct A2FoD3DTexturePlan {
    std::uint32_t format, width, height, mip_levels, alpha;
    std::uint64_t block_bytes, rgba8_bytes;
    A2FoD3DMip mips[15];
};
struct A2FoD3DResult {
    std::int32_t hr;
    std::uint32_t operation;
    A2FoD3DTexturePlan plan;
};
struct A2FoD3DReply {
    std::int32_t hr;
    std::uint32_t value;
    std::size_t handle;
    std::uint64_t value64;
};
struct A2FoD3DPath {
    const std::uint8_t* bytes;
    std::size_t length;
};
struct A2FoD3DPathBuffer {
    std::uint8_t* bytes;
    std::size_t capacity;
};
using A2FoD3DCall = int (*)(void*, std::uint32_t, const void*, A2FoD3DReply*) noexcept;
int a2fo_rs_d3d_compressed_upload(const A2FoD3DImage*,
    const A2FoD3DConstants*, int has_device, int has_output,
    void*, A2FoD3DCall, A2FoD3DResult*) noexcept;
int a2fo_rs_d3d_handle_initialize(void*, A2FoD3DCall,
                                   std::int32_t* result) noexcept;
std::int32_t a2fo_rs_d3d_handle_get_view(int has_output, int has_view,
                                          std::int32_t pointer_error,
                                          std::int32_t unexpected,
                                          std::int32_t callback_failure,
                                          void*, A2FoD3DCall) noexcept;
void a2fo_rs_d3d_handle_drop(int has_view, void*, A2FoD3DCall) noexcept;
int a2fo_rs_d3d_probe_bridge(std::uint32_t already_exists,
                              void*, A2FoD3DCall,
                              std::int32_t* result) noexcept;
}

enum : std::uint32_t {
    A2FO_D3D_VALIDATE_IMAGE = 1,
    A2FO_D3D_FEATURE_LEVEL = 2,
    A2FO_D3D_CHECK_FORMAT = 3,
    A2FO_D3D_CREATE_TEXTURE = 4,
    A2FO_D3D_CREATE_VIEW = 5,
    A2FO_D3D_RELEASE_TEXTURE = 6,
    A2FO_D3D_RELEASE_VIEW = 7,
    A2FO_D3D_HANDLE_CREATE = 8,
    A2FO_D3D_HANDLE_COMMIT = 9,
    A2FO_D3D_HANDLE_ADDREF = 10,
    A2FO_D3D_HANDLE_RELEASE = 11,
    A2FO_D3D_PROBE_INIT = 12,
    A2FO_D3D_GET_ENV_PATH = 13,
    A2FO_D3D_GET_TEMP_PATH = 14,
    A2FO_D3D_CREATE_DIRECTORY = 15,
    A2FO_D3D_PROBE_TRIANGLE = 16,
};

enum : std::uint32_t {
    A2FO_D3D_ERROR_NONE = 0,
    A2FO_D3D_ERROR_MISSING_OUTPUT = 1,
    A2FO_D3D_ERROR_MISSING_DEVICE = 2,
    A2FO_D3D_ERROR_FEATURE = 3,
    A2FO_D3D_ERROR_CHECK_FORMAT = 4,
    A2FO_D3D_ERROR_SUPPORT = 5,
    A2FO_D3D_ERROR_CREATE_TEXTURE = 6,
    A2FO_D3D_ERROR_CREATE_VIEW = 7,
};

extern "C" {
struct A2FoD3DProbeParams {
    std::uint32_t width, height, debug_layer;
    std::int32_t invalid_arg, fail, unexpected, sdk_missing, removed, reset;
    std::uint32_t max_dimension, level11;
    std::uint32_t rgba_format, depth_format, render_target_usage;
    std::uint32_t discard_swap, depth_usage, depth_bind, debug_flag;
    std::uint32_t no_window_changes, no_alt_enter;
};
struct A2FoD3DProbeRequest {
    std::uint32_t id, width, height, flags, format, usage, bind;
    std::uint32_t mip_levels, array_size, sample_count, buffer_count;
    std::uint32_t windowed, swap_effect;
    std::int32_t hr;
    std::uint64_t elapsed_ms;
    float viewport_width, viewport_height;
};
struct A2FoD3DVertex {float x,y,z,rhw; std::uint32_t color;};
struct A2FoD3DTriangle {
    A2FoD3DVertex vertices[3];
    std::uint32_t primitive, primitive_count, fvf, stride;
    std::uint32_t color_op, alpha_op, clear_color;
};
std::int32_t a2fo_rs_d3d_probe_initialize(const A2FoD3DProbeParams*,
                                           void*, A2FoD3DCall) noexcept;
std::int32_t a2fo_rs_d3d_probe_present_colour(const A2FoD3DProbeParams*,
                                               std::uint32_t ready_bits,
                                               void*, A2FoD3DCall) noexcept;
std::int32_t a2fo_rs_d3d_probe_present_triangle(const A2FoD3DProbeParams*,
                                                 std::uint32_t ready_bits,
                                                 void*, A2FoD3DCall) noexcept;
void a2fo_rs_d3d_probe_shutdown(void*, A2FoD3DCall) noexcept;
int a2fo_rs_d3d_probe_ready(std::uint32_t ready_bits) noexcept;
}

enum : std::uint32_t {
    A2FO_D3D_PROBE_SHUTDOWN=20, A2FO_D3D_PROBE_SET_PARAMS=21,
    A2FO_D3D_PROBE_VALIDATE_WINDOW=22, A2FO_D3D_PROBE_REPORT=23,
    A2FO_D3D_PROBE_LOAD_LIBRARY=24, A2FO_D3D_PROBE_GET_PROC=25,
    A2FO_D3D_PROBE_TICK=26, A2FO_D3D_PROBE_CREATE_DEVICE=27,
    A2FO_D3D_PROBE_VERIFY_DEVICE=28, A2FO_D3D_PROBE_RELEASE_CONTEXT=29,
    A2FO_D3D_PROBE_RELEASE_DEVICE=30, A2FO_D3D_PROBE_RELEASE_SWAP=31,
    A2FO_D3D_PROBE_DEBUG_LOG=32, A2FO_D3D_PROBE_ELAPSED_LOG=33,
    A2FO_D3D_PROBE_QUERY_DXGI=34, A2FO_D3D_PROBE_GET_ADAPTER=35,
    A2FO_D3D_PROBE_GET_FACTORY=36, A2FO_D3D_PROBE_MAKE_ASSOC=37,
    A2FO_D3D_PROBE_RELEASE_FACTORY=38, A2FO_D3D_PROBE_RELEASE_ADAPTER=39,
    A2FO_D3D_PROBE_RELEASE_DXGI=40, A2FO_D3D_PROBE_GET_BUFFER=41,
    A2FO_D3D_PROBE_CREATE_RTV=42, A2FO_D3D_PROBE_RELEASE_BUFFER=43,
    A2FO_D3D_PROBE_CREATE_DEPTH=44, A2FO_D3D_PROBE_CREATE_DSV=45,
    A2FO_D3D_PROBE_BIND_TARGETS=46, A2FO_D3D_PROBE_CLEAR_TARGET=47,
    A2FO_D3D_PROBE_CLEAR_DEPTH=48, A2FO_D3D_PROBE_PRESENT=49,
    A2FO_D3D_PROBE_REMOVED_REASON=50, A2FO_D3D_PROBE_UI_INIT=51,
    A2FO_D3D_PROBE_UI_TARGET=52, A2FO_D3D_PROBE_UI_CLEAR=53,
    A2FO_D3D_PROBE_UI_DRAW=54, A2FO_D3D_PROBE_UI_SHUTDOWN=55,
    A2FO_D3D_PROBE_CLEAR_STATE=56, A2FO_D3D_PROBE_RELEASE_DSV=57,
    A2FO_D3D_PROBE_RELEASE_DEPTH=58, A2FO_D3D_PROBE_RELEASE_RTV=59,
    A2FO_D3D_PROBE_FREE_LIBRARY=60, A2FO_D3D_PROBE_RESET_MEMBERS=61,
};

enum : std::uint32_t {
    A2FO_D3D_REPORT_VALIDATE=1, A2FO_D3D_REPORT_LOAD=2,
    A2FO_D3D_REPORT_PROC=3, A2FO_D3D_REPORT_CREATE_DEBUG=4,
    A2FO_D3D_REPORT_CREATE=5, A2FO_D3D_REPORT_CREATE_RETRY=6,
    A2FO_D3D_REPORT_VERIFY=7, A2FO_D3D_REPORT_QUERY=8,
    A2FO_D3D_REPORT_ADAPTER=9, A2FO_D3D_REPORT_FACTORY=10,
    A2FO_D3D_REPORT_ASSOC=11, A2FO_D3D_REPORT_BUFFER=12,
    A2FO_D3D_REPORT_RTV=13, A2FO_D3D_REPORT_DEPTH=14,
    A2FO_D3D_REPORT_DSV=15, A2FO_D3D_REPORT_BIND=16,
    A2FO_D3D_REPORT_UNINITIALIZED=17, A2FO_D3D_REPORT_PRESENT_COLOUR=18,
    A2FO_D3D_REPORT_REMOVED=19, A2FO_D3D_REPORT_TRIANGLE=20,
    A2FO_D3D_REPORT_PRESENT_TRIANGLE=21,
};

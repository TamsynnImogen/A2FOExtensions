#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2FORefitNativeRect { int32_t x, y, width, height; } A2FORefitNativeRect;
typedef struct A2FORefitSkinDirect {
    int32_t registered, dc_kind, map_mode, graphics_mode, layout;
    int32_t viewport_x, viewport_y, window_x, window_y;
    int32_t clip_kind, clip_left, clip_top, clip_right, clip_bottom;
    int32_t object_valid, bits_match, section_null, section_offset;
    int32_t header_size, planes, bit_count, compression, header_width, header_height;
    int32_t width, height, stride, bitmap_planes, bitmap_bit_count;
    uint8_t* bits;
} A2FORefitSkinDirect;

int a2fo_refit_skin_atlas_valid(const uint8_t* data, size_t size);
int a2fo_refit_skin_atlas_size_valid(size_t size);
int a2fo_refit_skin_tile_valid(int32_t width, int32_t height);
int a2fo_refit_skin_direct_precheck(int32_t target_valid, int32_t registered,
                                    int32_t registered_width, int32_t registered_height,
                                    A2FORefitNativeRect rect);
int a2fo_refit_skin_direct_property(int32_t step, int32_t value);
int a2fo_refit_skin_direct_origins(int32_t viewport_x, int32_t viewport_y,
                                   int32_t window_x, int32_t window_y);
int a2fo_refit_skin_direct_clip(A2FORefitNativeRect rect, int32_t kind,
                                int32_t left, int32_t top, int32_t right,
                                int32_t bottom);
int a2fo_refit_skin_tile_matches(A2FORefitNativeRect rect,
                                  int32_t bits_valid, int32_t width, int32_t height);
int32_t a2fo_refit_skin_source_axis(int32_t position, int32_t length, int32_t corner);
int a2fo_refit_skin_fill_tile(const uint8_t* atlas, uint8_t* output,
                              uint32_t index, int32_t width, int32_t height,
                              int32_t corner);
int a2fo_refit_skin_direct_valid(A2FORefitNativeRect rect,
                                  const A2FORefitSkinDirect* info);
void a2fo_refit_skin_blend(uint8_t* output, const uint8_t* source, int32_t width);
void a2fo_refit_skin_tint(uint8_t* output, uint32_t rgb, uint8_t alpha, int32_t width);
void a2fo_refit_skin_blend_fast(uint8_t* output, const uint8_t* source,
                                int32_t width, int32_t use_sse2);
void a2fo_refit_skin_tint_fast(uint8_t* output, uint32_t rgb, uint8_t alpha,
                               int32_t width, int32_t use_sse2);
int a2fo_refit_skin_paint_valid(int32_t target_valid, A2FORefitNativeRect rect,
                                 uint32_t part, int32_t corner);
// 0 rejects, 1 inserts, 2 clears the cache before insertion.
int a2fo_refit_skin_cache_decision(int32_t width, int32_t height,
                                   size_t tile_count, size_t pixel_count);
// Native operations own Win32 objects and caches. Return zero for failure.
typedef int (*A2FORefitSkinOp)(void* context, int32_t op);
int a2fo_refit_skin_paint(int32_t target_valid, A2FORefitNativeRect rect, uint32_t part,
                          int32_t corner, A2FORefitSkinOp operation, void* context);

typedef struct A2FORefitGpuImage {
    int32_t width, height;
    const uint32_t* pixels;
    size_t pixel_count;
} A2FORefitGpuImage;
typedef struct A2FORefitGpuCaps {
    uint32_t texture_caps, max_width, max_height;
} A2FORefitGpuCaps;
typedef struct A2FORefitGpuCacheState {
    int32_t present, device_match, uploaded, alpha_preserved;
    uint32_t width, height, texture_width, texture_height;
    uint64_t revision;
} A2FORefitGpuCacheState;
typedef struct A2FORefitGpuVertex {
    float x, y, z, rhw;
    uint32_t colour;
    float u, v;
} A2FORefitGpuVertex;
// All pointers are borrowed for the duration of a call. Callbacks are
// synchronous, must not throw through Rust, and own all D3D8 resources.
// render_state/stage_state indices select the fixed native API key/value
// tables; Rust selects the order and whether the plain or layered table runs.
// capture/restore bracket the saved viewport/state block. Slot 0 is the
// background (or sole image), slot 1 the UI overlay.
typedef struct A2FORefitGpuOps {
    void* context;
    int (*get_caps)(void*, A2FORefitGpuCaps*);
    int (*cache_state)(void*, int32_t, A2FORefitGpuCacheState*);
    void (*reset)(void*, int32_t);
    int (*create)(void*, int32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    int (*lock)(void*, int32_t, uint8_t**, int32_t*);
    int (*unlock)(void*, int32_t);
    void (*mark_upload)(void*, int32_t, uint64_t, int32_t, int32_t);
    int (*capture)(void*);
    void (*restore)(void*);
    int (*viewport)(void*, uint32_t, uint32_t);
    int (*render_state)(void*, int32_t, int32_t);
    int (*pixel_shader)(void*);
    int (*vertex_shader)(void*);
    int (*bind)(void*, int32_t);
    int (*stage_state)(void*, int32_t, int32_t);
    int (*draw)(void*, const A2FORefitGpuVertex*);
} A2FORefitGpuOps;
int a2fo_refit_gpu_draw(A2FORefitGpuImage image, uint64_t revision,
                        int32_t cached, const A2FORefitGpuOps* ops);
int a2fo_refit_gpu_layers(A2FORefitGpuImage background,
                          A2FORefitGpuImage overlay, int32_t out_width,
                          int32_t out_height, uint64_t background_revision,
                          uint64_t overlay_revision, int32_t background_cached,
                          int32_t overlay_cached, int32_t same_cache,
                          const A2FORefitGpuOps* ops);

#ifdef __cplusplus
}
#endif

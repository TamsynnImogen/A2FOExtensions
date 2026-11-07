#pragma once
#include <cstddef>
#include <cstdint>

// Scalar/POD policy bridge; no Direct3D, COM, or STL layout crosses it.
struct A2foGameCaps {
  uint32_t max_texture_width, max_texture_height, max_blend_stages;
  uint32_t max_textures, max_lights, max_streams, max_stride;
  uint32_t vertex_shader_version, max_vertex_consts, pixel_shader_version;
  uint32_t max_blend_matrices, max_blend_matrix_index;
  uint32_t texture_caps, device_caps;
};
extern "C" {
void a2fo_game_identity(float *matrix16);
void a2fo_game_defaults(uint32_t *render256, uint32_t *stage8x33, bool z_enable);
int32_t a2fo_game_matrix_slot(uint32_t state);
bool a2fo_game_viewport(uint32_t width, uint32_t height, float min_z,
                        float max_z);
bool a2fo_game_texture_admission(uint32_t width, uint32_t height,
                                 uint32_t levels, uint32_t usage,
                                 uint32_t format, uint32_t pool,
                                 bool pitch_supported);
bool a2fo_game_buffer_admission(uint32_t size, uint32_t pool);
bool a2fo_game_index_admission(uint32_t size, uint32_t format, uint32_t pool);
bool a2fo_game_image_admission(uint32_t width, uint32_t height,
                               uint32_t pitch);
bool a2fo_game_reset_valid(bool presentation_present, bool recording,
                           bool in_scene, uint32_t multisample,
                           uint32_t backbuffer_count);
bool a2fo_game_reset_resource_block(uint32_t kind, uint32_t pool,
                                    int32_t refs);
void a2fo_game_caps(A2foGameCaps *caps);
uint32_t a2fo_game_factory_format(uint32_t usage, uint32_t resource,
                                  uint32_t format, bool texture_supported);
bool a2fo_game_backbuffer(uint32_t format);
bool a2fo_game_color_backbuffer(uint32_t format);
bool a2fo_game_depth(uint32_t format);
bool a2fo_game_depth_match(uint32_t target, uint32_t depth);
}
extern "C" void a2fo_game_multiply(float *out16, const float *a16,
                                   const float *b16);

struct A2foPeLayout {
  uint32_t dos_size, nt_size, import_size;
  uint32_t dos_magic_at, dos_lfanew_at;
  uint32_t nt_signature_at, nt_machine_at, nt_magic_at, nt_image_size_at;
  uint32_t nt_import_rva_at, nt_import_size_at;
  uint32_t import_name_at, import_original_at, import_first_at;
  uint32_t dos_magic, nt_signature, machine, nt_magic, ordinal_mask;
};
struct A2foPePatchPlan {
  uintptr_t slot, previous;
  uint32_t index, duplicate;
};
using A2foPeSpan = bool (*)(void *context, uintptr_t address, size_t size);
using A2foPeRead = void (*)(void *context, uintptr_t address, uint8_t *out,
                            size_t size);
extern "C" bool a2fo_game_file_bridge_walk(uintptr_t base,
    const A2foPeLayout *layout, A2foPeSpan span, A2foPeRead read,
    void *context, const uintptr_t *slots8, const uintptr_t *callbacks8,
    A2foPePatchPlan *plan);
extern "C" bool a2fo_game_supported_texture(uint32_t format, uint32_t pitch);
extern "C" bool a2fo_game_factory_create_admission(uint32_t adapter,
    uint32_t device_type, uint32_t multisample, uint32_t backbuffers);
extern "C" bool a2fo_game_reset_window(bool same_window, uint32_t backbuffers);
extern "C" uint32_t a2fo_game_depth_bytes(uint32_t format);
extern "C" bool a2fo_game_present_log(uint64_t frames);
extern "C" bool a2fo_game_state_index(uint32_t category, uint32_t index,
                                       uint32_t secondary);
extern "C" bool a2fo_game_scene_transition(bool in_scene, bool begin);
extern "C" bool a2fo_game_surface_admission(uint32_t multisample,
                                             bool lockable);
extern "C" size_t a2fo_game_basename_index(const uint8_t *path, size_t size);
extern "C" bool a2fo_game_read_mode(const uint8_t *mode, size_t size);

struct A2foGamePresentationPlan {
  uintptr_t focus_window, device_window;
  uint32_t width, height, format, backbuffers, multisample;
};
using A2foGameGetClient = bool (*)(void *context, uint32_t *width,
                                   uint32_t *height);
using A2foGameStatus = int32_t (*)(void *context);
using A2foGameCatalog = uint32_t (*)(void *context);
using A2foGameVoid = void (*)(void *context);
using A2foGameUpload = void (*)(void *context, int32_t *result, bool *threw);
extern "C" {
int32_t a2fo_game_create_device_workflow(void *context,
    A2foGamePresentationPlan *plan, bool parameters_present,
    uint32_t adapter, uint32_t type, A2foGameGetClient get_client,
    A2foGameStatus initialize, A2foGameStatus publish,
    int32_t invalid_call, int32_t unavailable);
bool a2fo_game_create_factory_workflow(void *context, bool native_present,
    bool bc_assets, A2foGameCatalog catalog, A2foGameVoid log_failure,
    A2foGameCatalog create);
int32_t a2fo_game_load_texture_workflow(void *context,
    A2foGameCatalog load, A2foGameStatus create,
    A2foGameCatalog copy, A2foGameUpload upload,
    A2foGameVoid release, A2foGameVoid publish,
    A2foGameStatus catalog_failure, int32_t not_found,
    int32_t out_of_memory);
}
using A2foGameResourceCount = uint32_t (*)(void *context);
using A2foGameResourceInfo = void (*)(void *context, uint32_t index,
                                      uint32_t *kind, uint32_t *pool,
                                      int32_t *refs);
using A2foGameResetFail = int32_t (*)(void *context, uint32_t reason,
                                      int32_t code);
extern "C" int32_t a2fo_game_reset_workflow(void *context,
    A2foGamePresentationPlan *plan, bool present, bool recording,
    bool in_scene, uintptr_t old_window, uint32_t old_format,
    A2foGameGetClient get_client, A2foGameResourceCount resource_count,
    A2foGameResourceInfo resource_info, A2foGameStatus clear,
    A2foGameUpload resize, A2foGameStatus commit, A2foGameStatus rebuild,
    A2foGameResetFail fail, int32_t invalid_call);

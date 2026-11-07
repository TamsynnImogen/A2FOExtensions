#pragma once
#include <cstddef>
#include <cstdint>

// Private scalar/byte bridge. C++ owns all COM objects and resource storage.
extern "C" {
int32_t a2fo_resource_private_set(bool present, uint32_t size, uint32_t flags,
                                  uint32_t pointer_size);
int32_t a2fo_resource_private_get(bool size_present, bool found,
                                  bool data_present, uint32_t capacity,
                                  uint32_t needed);
bool a2fo_resource_is_object(uint32_t flags);
bool a2fo_resource_is_readonly(uint32_t flags);
bool a2fo_resource_is_render_target(uint32_t usage);
uint64_t a2fo_resource_hash(const uint8_t *bytes, size_t size);
bool a2fo_resource_block_format(uint32_t format);
uint32_t a2fo_resource_pitch(uint32_t format, uint32_t width);
uint32_t a2fo_resource_rows(uint32_t format, uint32_t height);
uint32_t a2fo_resource_levels(uint32_t width, uint32_t height);
uint32_t a2fo_resource_next_dimension(uint32_t value);
uint32_t a2fo_resource_lod(uint32_t requested, size_t levels);
bool a2fo_resource_buffer_lock(bool output_present, bool locked, size_t length,
                               uint32_t offset, uint32_t size, uint32_t flags);
bool a2fo_resource_rect_lock(bool output_present, bool locked,
                             bool render_target, uint32_t flags,
                             uint32_t format, uint32_t width, uint32_t height,
                             uint32_t pitch, int32_t left, int32_t top,
                             int32_t right, int32_t bottom, size_t *offset);
bool a2fo_resource_surface_lock(bool output_present, bool locked, bool target,
                                bool region_present, uint32_t flags);
bool a2fo_resource_upload_immutable_level(size_t level, size_t expected_count);
bool a2fo_resource_upload_immutable(size_t level, size_t expected_count,
                                     uint64_t actual, const uint64_t *expected);
uint32_t a2fo_resource_upload_plan(bool compressed, bool dxt4, size_t level,
                                   bool has_gpu);
uint32_t a2fo_resource_bc_miss_plan(bool error_present, bool bc_assets);
bool a2fo_resource_target_plan(bool has_texture, bool has_target);
bool a2fo_resource_has_level(bool output_present, size_t level, size_t count);
bool a2fo_resource_can_unlock(bool locked);
bool a2fo_resource_should_upload(bool readonly);
bool a2fo_resource_can_dirty(bool compressed);
bool a2fo_resource_texture_owner(uint32_t kind, bool has_texture);
bool a2fo_resource_can_initialize(uint32_t pitch);
void a2fo_resource_swizzle_bgra(uint8_t *dst, const uint8_t *src, size_t size);
uint64_t a2fo_resource_accumulate_size(uint64_t total, uint32_t size);
}

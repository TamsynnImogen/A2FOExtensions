#ifndef A2FO_STAGE9_A1_GEOMETRY_FFI_H
#define A2FO_STAGE9_A1_GEOMETRY_FFI_H

#include <cstdint>

extern "C" {
void a2fo_rs_stage9_a1_geometry_union(
    const std::int32_t* first, const std::int32_t* second,
    std::int32_t* output);
bool a2fo_rs_stage9_a1_geometry_usable(const std::int32_t* rectangle);
bool a2fo_rs_stage9_a1_geometry_cursor_preflight(
    const std::int32_t* rectangle);
bool a2fo_rs_stage9_a1_geometry_contains(
    const std::int32_t* rectangle, std::int32_t x, std::int32_t y);

using A2foStage9A1GeometryChild = std::uintptr_t (*)(
    void* context, std::uint32_t child_index);
std::uintptr_t a2fo_rs_stage9_a1_geometry_preferred_child(
    void* context, A2foStage9A1GeometryChild callback);
}

#endif

#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

namespace a2fo::renderer::compat {

struct FvfLayout {
    UINT stride = 0;
    bool transformed = false;
    UINT position_components = 0;
    bool normal = false;
    bool diffuse = false;
    bool specular = false;
    UINT texture_count = 0;
    UINT position_offset = 0;
    UINT normal_offset = 0;
    UINT diffuse_offset = 0;
    UINT specular_offset = 0;
    UINT texture_offsets[8] = {};
    UINT texture_components[8] = {};
};

// Primitive and index-format inputs retain the numeric D3D8 API values.
// This boundary intentionally includes neither D3D8 nor D3D11 headers.
HRESULT decode_fvf(DWORD fvf, FvfLayout& layout) noexcept;
HRESULT primitive_vertex_count(UINT type, UINT primitive_count,
                               UINT& vertex_count) noexcept;

// For non-null indices, the caller supplies at least primitive_count + 2 entries.
HRESULT expand_triangle_fan(UINT primitive_count, const void* indices,
                            UINT index_format, UINT vertex_count,
                            std::vector<std::uint32_t>& triangles);

} // namespace a2fo::renderer::compat

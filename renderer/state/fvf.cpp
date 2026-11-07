#include "fvf.hpp"
#include "../../rust/lead_ffi.h"
#include <d3d8.h>
namespace a2fo::renderer::compat {
HRESULT decode_fvf(DWORD fvf, FvfLayout &layout) noexcept {
    layout = {};
    std::uint32_t fields[27]{};
    if (!a2fo_rs_lead_fvf(fvf, fields))
        return D3DERR_INVALIDCALL;
    layout.stride = fields[0];
    layout.transformed = fields[1] != 0;
    layout.position_components = fields[2];
    layout.normal = fields[3] != 0;
    layout.diffuse = fields[4] != 0;
    layout.specular = fields[5] != 0;
    layout.texture_count = fields[6];
    layout.position_offset = fields[7];
    layout.normal_offset = fields[8];
    layout.diffuse_offset = fields[9];
    layout.specular_offset = fields[10];
    for (unsigned i = 0; i < 8; ++i) {
        layout.texture_offsets[i] = fields[11 + i];
        layout.texture_components[i] = fields[19 + i];
    }
    return S_OK;
}
HRESULT primitive_vertex_count(UINT type, UINT count, UINT &vertices) noexcept {
    return a2fo_rs_lead_primitive(type, count, &vertices) ? S_OK : D3DERR_INVALIDCALL;
}
HRESULT expand_triangle_fan(UINT count, const void *indices, UINT format, UINT vertices,
                            std::vector<std::uint32_t> &out) {
    out.clear();
    const auto *bytes = static_cast<const std::uint8_t *>(indices);
    if (!a2fo_rs_lead_fan(count, bytes, format, vertices, nullptr, 0))
        return D3DERR_INVALIDCALL;
    out.resize(static_cast<std::size_t>(count) * 3);
    if (!a2fo_rs_lead_fan(count, bytes, format, vertices, out.data(), out.size())) {
        out.clear();
        return D3DERR_INVALIDCALL;
    }
    return S_OK;
}
} // namespace a2fo::renderer::compat

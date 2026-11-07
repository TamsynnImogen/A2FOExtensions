#include <d3d8.h>
#include "../renderer/state/fvf.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace a2fo::renderer::compat;

namespace {

void require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

void test_fvf() {
    FvfLayout layout{};
    require(decode_fvf(0x144, layout) == S_OK, "Armada 0x144 decoded");
    require(layout.transformed && layout.stride == 28 && layout.position_components == 4 &&
            layout.diffuse && layout.diffuse_offset == 16 && layout.texture_count == 1 &&
            layout.texture_offsets[0] == 20 && layout.texture_components[0] == 2,
            "Armada 0x144 layout");
    require(decode_fvf(0x104, layout) == S_OK, "Armada 0x104 decoded");
    require(layout.transformed && layout.stride == 24 && layout.texture_offsets[0] == 16,
            "Armada 0x104 layout");
    require(decode_fvf(0x44, layout) == S_OK, "Armada 0x44 decoded");
    require(layout.transformed && layout.stride == 20 && layout.diffuse_offset == 16,
            "Armada 0x44 layout");

    const DWORD sized = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE |
        D3DFVF_SPECULAR | D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE1(0) |
        D3DFVF_TEXCOORDSIZE3(1) | D3DFVF_TEXCOORDSIZE4(2);
    require(decode_fvf(sized, layout) == S_OK, "mixed texture dimensions decoded");
    require(layout.stride == 72 && layout.normal_offset == 12 &&
            layout.diffuse_offset == 24 && layout.specular_offset == 28 &&
            layout.texture_offsets[0] == 32 && layout.texture_components[0] == 1 &&
            layout.texture_offsets[1] == 36 && layout.texture_components[1] == 3 &&
            layout.texture_offsets[2] == 48 && layout.texture_components[2] == 4 &&
            layout.texture_offsets[3] == 64 && layout.texture_components[3] == 2,
            "mixed texture offsets and stride");
    require(decode_fvf(D3DFVF_XYZ | D3DFVF_TEX8, layout) == S_OK &&
            layout.stride == 76 && layout.texture_offsets[7] == 68,
            "all eight texture stages");

    const DWORD bad[] = {0, D3DFVF_XYZB1, D3DFVF_XYZ | D3DFVF_PSIZE,
        D3DFVF_XYZ | D3DFVF_RESERVED0, D3DFVF_XYZ | D3DFVF_RESERVED2,
        D3DFVF_XYZ | D3DFVF_LASTBETA_UBYTE4,
        D3DFVF_XYZRHW | D3DFVF_NORMAL,
        D3DFVF_XYZ | D3DFVF_TEXCOORDSIZE1(0),
        D3DFVF_XYZ | (9u << D3DFVF_TEXCOUNT_SHIFT),
        D3DFVF_XYZ | 0x80000000u, D3DFVF_XYZ | 0x4000u};
    for (DWORD value : bad) {
        require(decode_fvf(value, layout) == D3DERR_INVALIDCALL && layout.stride == 0,
                "unsupported FVF rejected and output reset");
    }
}

void test_primitive_counts() {
    UINT count = 99;
    const D3DPRIMITIVETYPE types[] = {D3DPT_POINTLIST, D3DPT_LINELIST,
        D3DPT_LINESTRIP, D3DPT_TRIANGLELIST, D3DPT_TRIANGLESTRIP,
        D3DPT_TRIANGLEFAN};
    const UINT expected[] = {4, 8, 5, 12, 6, 6};
    for (unsigned i = 0; i < 6; ++i) {
        require(primitive_vertex_count(types[i], 4, count) == S_OK &&
                count == expected[i], "primitive count conversion");
        require(primitive_vertex_count(types[i], 0, count) == S_OK && count == 0,
                "zero primitive count");
    }
    const UINT max = std::numeric_limits<UINT>::max();
    require(primitive_vertex_count(D3DPT_LINELIST, max, count) == D3DERR_INVALIDCALL &&
            count == 0, "line list overflow");
    require(primitive_vertex_count(D3DPT_TRIANGLELIST, max, count) == D3DERR_INVALIDCALL &&
            count == 0, "triangle list overflow");
    require(primitive_vertex_count(D3DPT_TRIANGLEFAN, max, count) == D3DERR_INVALIDCALL &&
            count == 0, "triangle fan overflow");
    require(primitive_vertex_count(D3DPT_POINTLIST, max, count) == S_OK &&
            count == max, "point list UINT boundary");
    require(primitive_vertex_count(D3DPT_TRIANGLEFAN, max - 2, count) == S_OK &&
            count == max, "triangle fan UINT boundary");
    require(primitive_vertex_count(static_cast<D3DPRIMITIVETYPE>(99), 0, count) ==
            D3DERR_INVALIDCALL, "invalid primitive type rejected at zero");
}

void test_fans() {
    std::vector<std::uint32_t> triangles;
    require(expand_triangle_fan(2, nullptr, D3DFMT_INDEX16, 4, triangles) == S_OK &&
            triangles == std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}),
            "sequential fan winding");
    const std::uint16_t indices16[] = {4, 2, 1, 3};
    require(expand_triangle_fan(2, indices16, D3DFMT_INDEX16, 5, triangles) == S_OK &&
            triangles == std::vector<std::uint32_t>({4, 2, 1, 4, 1, 3}),
            "indexed 16-bit fan winding");
    const std::uint32_t indices32[] = {70000, 2, 9, 5};
    require(expand_triangle_fan(2, indices32, D3DFMT_INDEX32, 70001, triangles) == S_OK &&
            triangles == std::vector<std::uint32_t>({70000, 2, 9, 70000, 9, 5}),
            "indexed 32-bit fan values");
    require(expand_triangle_fan(2, indices16, D3DFMT_INDEX16, 4, triangles) ==
            D3DERR_INVALIDCALL && triangles.empty(), "indexed out-of-range rejected");
    require(expand_triangle_fan(2, nullptr, D3DFMT_INDEX16, 3, triangles) ==
            D3DERR_INVALIDCALL && triangles.empty(), "sequential out-of-range rejected");
    require(expand_triangle_fan(1, nullptr, D3DFMT_UNKNOWN, 3, triangles) ==
            D3DERR_INVALIDCALL, "unknown index format rejected");
    require(expand_triangle_fan(std::numeric_limits<UINT>::max(), nullptr,
                               D3DFMT_INDEX16, 3, triangles) == D3DERR_INVALIDCALL,
            "fan source count overflow rejected");
    require(expand_triangle_fan(std::numeric_limits<UINT>::max() / 12u + 1u,
                               indices16, D3DFMT_INDEX16,
                               std::numeric_limits<UINT>::max(), triangles) ==
            D3DERR_INVALIDCALL && triangles.empty(),
            "D3D11 index ByteWidth overflow rejected before input read");
    require(expand_triangle_fan(0, nullptr, D3DFMT_INDEX16, 0, triangles) == S_OK &&
            triangles.empty(), "empty fan");
}

} // namespace

int main() {
    test_fvf();
    test_primitive_counts();
    test_fans();
    std::puts("PASS: renderer FVF and fan helpers");
    return 0;
}

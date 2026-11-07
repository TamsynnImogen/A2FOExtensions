#pragma once

#include <windows.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vs11_fixture {

// D3D8 and D3D11 SDK headers must live in separate translation units.
struct Vertex {
    float position[3], normal[3], uv[2], tangent[3], bitangent[3], third[3];
};
static_assert(sizeof(Vertex) == 68, "captured native DOT3 vertex stride");

struct Scene {
    const char* name = nullptr;
    std::vector<std::uint32_t> program;
    std::array<float, 96 * 4> constants{};
    std::array<Vertex, 6> vertices{};
    UINT viewport_x = 0, viewport_y = 0;
    UINT viewport_width = 64, viewport_height = 64;
};

HRESULT render_reference(HWND window, const Scene& scene,
                         std::vector<std::uint8_t>& rgba, UINT& target_samples, std::string& error);

} // namespace vs11_fixture

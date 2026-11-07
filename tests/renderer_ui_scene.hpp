#pragma once
#include "../renderer/d3d11/ui_renderer.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace renderer_fixture {
struct Scene {
    a2fo::renderer::Viewport viewport{0,0,64,64,0,1};
    a2fo::renderer::UiDrawState state{};
    DWORD background=0x00102030,clear_color=0x00dc7010,fvf=68;
    UINT primitive=6,primitive_count=2,stride=20,vertex_count=4,index_format=101;
    std::vector<std::uint8_t> vertices,indices;
    std::vector<a2fo::renderer::UiRenderer::Rect> clear_rectangles;
    UINT texture_format=21,texture_width=0,texture_height=0,texture_pitch=0;
    std::vector<std::uint8_t> texture;
};
// Implemented in a separate D3D8-only TU. No SDK type aliases/COM casts bridge
// the legacy and modern headers; only this neutral scene and RGB pixels cross.
HRESULT render_reference(HWND,const Scene&,std::vector<std::uint8_t>&,std::string&);
}

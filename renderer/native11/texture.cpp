#include "texture.hpp"
#include <limits>

namespace a2fo::native11 {
namespace {
std::uint32_t block_bytes(TextureFormat format) noexcept {
    switch(format) {
    case TextureFormat::bc1:case TextureFormat::bc1_srgb:
    case TextureFormat::bc4:case TextureFormat::bc4_snorm:return 8;
    case TextureFormat::bc2:case TextureFormat::bc2_srgb:
    case TextureFormat::bc3:case TextureFormat::bc3_srgb:
    case TextureFormat::bc5:case TextureFormat::bc5_snorm:
    case TextureFormat::bc7:case TextureFormat::bc7_srgb:return 16;
    default:return 0;
    }
}
bool raw(TextureFormat f) noexcept {
    return f==TextureFormat::bgra8||f==TextureFormat::rgba8||
        f==TextureFormat::bgra8_srgb||f==TextureFormat::rgba8_srgb;
}
}
bool plan_texture(const TextureView& view, TexturePlan& output) noexcept {
    const auto block=block_bytes(view.format);
    if((!block&&!raw(view.format))||static_cast<std::uint32_t>(view.alpha)>4||
       !view.bytes||!view.mips||!view.width||!view.height||
       view.width>16384||view.height>16384||!view.mip_count||view.mip_count>15)return false;
    std::uint32_t limit=1;
    for(auto w=view.width,h=view.height;w>1||h>1;++limit){w=w>1?w/2:1;h=h>1?h/2:1;}
    if(view.mip_count>limit)return false;
    auto w=view.width,h=view.height;std::uint64_t used=0;
    for(std::uint32_t i=0;i<view.mip_count;++i){
        const auto& m=view.mips[i];
        const std::uint64_t row=block?((std::uint64_t(w)+3)/4)*block:std::uint64_t(w)*4;
        const std::uint64_t rows=block?(std::uint64_t(h)+3)/4:h;
        const std::uint64_t size=row*rows;
        if(row>std::numeric_limits<std::uint32_t>::max()||size>std::numeric_limits<std::uint32_t>::max()||
           m.width!=w||m.height!=h||m.row_pitch!=row||m.slice_pitch!=size||
           m.offset!=used||m.byte_count!=size||used>view.byte_count||size>view.byte_count-used)return false;
        used+=size;w=w>1?w/2:1;h=h>1?h/2:1;
    }
    if(used!=view.byte_count)return false;
    output={view.format,view.alpha,view.width,view.height,view.mip_count,used};
    return true;
}
} // namespace a2fo::native11

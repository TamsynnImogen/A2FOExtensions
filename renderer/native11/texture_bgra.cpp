#include "texture_bgra.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::uint64_t decoded_limit=128ull*1024*1024;
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;} catch(...) {error.clear();}
    return false;
}

std::uint8_t five(std::uint32_t x) noexcept {return static_cast<std::uint8_t>((x<<3)|(x>>2));}
std::uint8_t six(std::uint32_t x) noexcept {return static_cast<std::uint8_t>((x<<2)|(x>>4));}
std::uint16_t le16(const std::uint8_t* p) noexcept {return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);}
std::uint32_t le32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
void colour_table(const std::uint8_t* block,bool bc1,std::uint8_t table[4][4]) noexcept {
    const auto c0=le16(block),c1=le16(block+2);
    table[0][0]=five(c0&31);table[0][1]=six((c0>>5)&63);table[0][2]=five((c0>>11)&31);table[0][3]=255;
    table[1][0]=five(c1&31);table[1][1]=six((c1>>5)&63);table[1][2]=five((c1>>11)&31);table[1][3]=255;
    for(unsigned channel=0;channel<3;++channel){
        if(bc1&&c0<=c1){
            table[2][channel]=static_cast<std::uint8_t>((unsigned(table[0][channel])+table[1][channel])/2);
            table[3][channel]=0;
        }else{
            table[2][channel]=static_cast<std::uint8_t>((2u*table[0][channel]+table[1][channel])/3);
            table[3][channel]=static_cast<std::uint8_t>((table[0][channel]+2u*table[1][channel])/3);
        }
    }
    table[2][3]=255;table[3][3]=(bc1&&c0<=c1)?0:255;
}
void alpha_table(const std::uint8_t* block,std::uint8_t table[8]) noexcept {
    table[0]=block[0];table[1]=block[1];
    if(table[0]>table[1]){
        for(unsigned i=2;i<8;++i)table[i]=static_cast<std::uint8_t>(((8u-i)*table[0]+(i-1u)*table[1])/7);
    }else{
        for(unsigned i=2;i<6;++i)table[i]=static_cast<std::uint8_t>(((6u-i)*table[0]+(i-1u)*table[1])/5);
        table[6]=0;table[7]=255;
    }
}
bool supported(TextureFormat f) noexcept {
    switch(f){
    case TextureFormat::bgra8:case TextureFormat::rgba8:
    case TextureFormat::bgra8_srgb:case TextureFormat::rgba8_srgb:
    case TextureFormat::bc1:case TextureFormat::bc1_srgb:
    case TextureFormat::bc2:case TextureFormat::bc2_srgb:
    case TextureFormat::bc3:case TextureFormat::bc3_srgb:return true;
    default:return false;
    }
}
bool raw(TextureFormat f) noexcept {
    return f==TextureFormat::bgra8||f==TextureFormat::rgba8||
        f==TextureFormat::bgra8_srgb||f==TextureFormat::rgba8_srgb;
}
}

bool texture_base_to_bgra(TextureView view,bool magenta_key,
                          std::vector<std::uint8_t>& out,std::string& error) noexcept {
    TexturePlan plan{};
    if(!plan_texture(view,plan))return fail(error,"invalid texture layout");
    if(!supported(plan.format))return fail(error,"unsupported texture format");
    if(plan.alpha==TextureAlpha::premultiplied||plan.alpha==TextureAlpha::custom){
        return fail(error,"unsupported texture alpha mode");
    }
    const std::uint64_t size=std::uint64_t(plan.width)*plan.height*4;
    if(size>decoded_limit)return fail(error,"decoded texture exceeds 128 MiB");
    try {
        std::vector<std::uint8_t> decoded(static_cast<std::size_t>(size));
        const auto* source=view.bytes+view.mips[0].offset;
        if(raw(plan.format)){
            const bool rgba=plan.format==TextureFormat::rgba8||plan.format==TextureFormat::rgba8_srgb;
            for(std::uint32_t y=0;y<plan.height;++y){
                const auto* row=source+std::size_t(y)*view.mips[0].row_pitch;
                for(std::uint32_t x=0;x<plan.width;++x){
                    const auto* pixel=row+std::size_t(x)*4;
                    auto* dest=decoded.data()+(std::size_t(y)*plan.width+x)*4;
                    dest[0]=pixel[rgba?2:0];dest[1]=pixel[1];dest[2]=pixel[rgba?0:2];dest[3]=pixel[3];
                }
            }
        }else{
            const bool bc1=plan.format==TextureFormat::bc1||plan.format==TextureFormat::bc1_srgb;
            const bool bc2=plan.format==TextureFormat::bc2||plan.format==TextureFormat::bc2_srgb;
            const std::size_t blocks_x=(std::size_t(plan.width)+3)/4;
            const std::size_t blocks_y=(std::size_t(plan.height)+3)/4;
            const std::size_t block_size=bc1?8:16;
            for(std::size_t by=0;by<blocks_y;++by)for(std::size_t bx=0;bx<blocks_x;++bx){
                const auto* block=source+by*view.mips[0].row_pitch+bx*block_size;
                const auto* colours=block+(bc1?0:8);
                std::uint8_t table[4][4]{};colour_table(colours,bc1,table);
                std::uint8_t alphas[8]{};if(!bc1&&!bc2)alpha_table(block,alphas);
                std::uint64_t alpha_indices=0;
                if(!bc1&&!bc2)for(unsigned i=0;i<6;++i)alpha_indices|=std::uint64_t(block[2+i])<<(8*i);
                const auto indices=le32(colours+4);
                for(unsigned py=0;py<4;++py)for(unsigned px=0;px<4;++px){
                    const auto x=bx*4+px,y=by*4+py;
                    if(x>=plan.width||y>=plan.height)continue;
                    const unsigned n=py*4+px;
                    const auto* colour=table[(indices>>(n*2))&3];
                    auto* dest=decoded.data()+(y*plan.width+x)*4;
                    dest[0]=colour[0];dest[1]=colour[1];dest[2]=colour[2];
                    dest[3]=bc1?colour[3]:bc2?static_cast<std::uint8_t>(((block[n/2]>>(4*(n&1)))&15)*17):
                        alphas[(alpha_indices>>(n*3))&7];
                }
            }
        }
        for(std::size_t i=0;i<decoded.size();i+=4){
            if(plan.alpha==TextureAlpha::opaque)decoded[i+3]=255;
            if(magenta_key&&decoded[i]==255&&decoded[i+1]==0&&decoded[i+2]==255)decoded[i+3]=0;
        }
        out.swap(decoded);error.clear();return true;
    }catch(...){return fail(error,"texture decode allocation failed");}
}
} // namespace a2fo::native11

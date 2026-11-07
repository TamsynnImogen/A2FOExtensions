#pragma once
// Original constant-colour BC blocks for the private GPU fixture.
#include "../renderer/textures/dds.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace bc_fixture {
using Bytes = std::vector<std::uint8_t>;
struct Colour { int r, g, b, a; };
constexpr std::array<Colour, 4> bc7_top{{{64,128,192,254},{128,64,192,254},
                                           {192,128,64,254},{64,192,128,254}}};
constexpr std::array<Colour, 3> bc7_lower{{{128,192,64,254},{192,64,128,254},
                                             {64,128,64,254}}};
constexpr std::array<std::array<int,2>,4> bc5_unsigned_top{{{{64,192}},{{128,64}},{{192,128}},{{32,224}}}};
constexpr std::array<std::array<int,2>,4> bc5_signed_top{{{{-112,112}},{{64,-96}},{{-64,-64}},{{96,32}}}};
constexpr std::array<std::array<int,2>,3> bc5_unsigned_lower{{{{96,160}},{{160,96}},{{48,208}}}};
constexpr std::array<std::array<int,2>,3> bc5_signed_lower{{{{-64,96}},{{96,-64}},{{-32,-96}}}};

inline void word(Bytes& b, std::size_t at, std::uint32_t n) {
    for (unsigned i=0;i<4;++i) b[at+i]=static_cast<std::uint8_t>(n>>(8*i));
}
inline constexpr std::uint32_t code(char a,char b,char c,char d) {
    return std::uint32_t(std::uint8_t(a)) | (std::uint32_t(std::uint8_t(b))<<8) |
           (std::uint32_t(std::uint8_t(c))<<16) | (std::uint32_t(std::uint8_t(d))<<24);
}
inline void bits(std::array<std::uint8_t,16>& block,unsigned& offset,unsigned n,unsigned count) {
    for(unsigned bit=0;bit<count;++bit,++offset)
        if(bit<32 && (n & (1u<<bit))) block[offset/8] |= std::uint8_t(1u<<(offset%8));
}
inline std::array<std::uint8_t,16> bc7_mode6(Colour c) {
    std::array<std::uint8_t,16> block{};unsigned offset=0;
    bits(block,offset,1u<<6,7); // unary mode 6 prefix, LSB-first
    for(int v:{c.r,c.g,c.b,c.a}) {
        bits(block,offset,unsigned(v>>1),7);
        bits(block,offset,unsigned(v>>1),7);
    }
    bits(block,offset,unsigned(c.r&1),1);
    bits(block,offset,unsigned(c.r&1),1);
    bits(block,offset,0,63); // anchor index has three bits; other indices four
    return block;
}
inline std::array<std::uint8_t,16> bc5(int r,int g) {
    std::array<std::uint8_t,16> block{};
    block[0]=block[1]=std::uint8_t(r);
    block[8]=block[9]=std::uint8_t(g);
    return block; // all 3-bit indices zero for both BC4 planes
}
inline Colour bc7_colour(unsigned mip,unsigned block) {
    return mip==0 ? bc7_top.at(block) : bc7_lower.at(mip-1);
}
inline std::array<int,2> bc5_colour(bool signed_format,unsigned mip,unsigned block) {
    if(mip==0) return signed_format ? bc5_signed_top.at(block) : bc5_unsigned_top.at(block);
    return signed_format ? bc5_signed_lower.at(mip-1) : bc5_unsigned_lower.at(mip-1);
}
inline Bytes make_dds(DXGI_FORMAT format) {
    Bytes b(148,0);
    word(b,0,code('D','D','S',' '));word(b,4,124);
    word(b,8,0x1|0x2|0x4|0x1000|0x80000|0x20000);
    word(b,12,8);word(b,16,8);word(b,20,64);word(b,28,4);
    word(b,76,32);word(b,80,4);word(b,84,code('D','X','1','0'));
    word(b,108,0x1000|0x8|0x400000);
    word(b,128,static_cast<std::uint32_t>(format));word(b,132,3);word(b,140,1);
    const bool bc7 = format==DXGI_FORMAT_BC7_UNORM || format==DXGI_FORMAT_BC7_UNORM_SRGB;
    const bool signed_format = format==DXGI_FORMAT_BC5_SNORM;
    for(unsigned mip=0;mip<4;++mip) {
        const unsigned count=mip==0?4:1;
        for(unsigned block_index=0;block_index<count;++block_index) {
            const auto block=bc7 ? bc7_mode6(bc7_colour(mip,block_index)) :
                                   bc5(bc5_colour(signed_format,mip,block_index)[0],
                                       bc5_colour(signed_format,mip,block_index)[1]);
            b.insert(b.end(),block.begin(),block.end());
        }
    }
    return b;
}
} // namespace bc_fixture

#include "dds.hpp"
#include "upload.hpp"
#include <d3d8.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

using namespace a2fo::renderer;
using Bytes = std::vector<std::uint8_t>;
static std::uint32_t seed=0x83c4a195u;
static std::uint32_t random_word() {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
static std::uint64_t hash(const Bytes& bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (auto c : bytes) { h ^= c; h *= 1099511628211ull; }
    return h;
}
static void word(Bytes& b, size_t i, UINT v) {
    for (UINT n=0; n<4; ++n) b[i+n]=static_cast<std::uint8_t>(v>>(n*8));
}
static constexpr UINT cc(char a,char b,char c,char d) {
    return UINT(std::uint8_t(a)) | (UINT(std::uint8_t(b))<<8) |
           (UINT(std::uint8_t(c))<<16) | (UINT(std::uint8_t(d))<<24);
}
static Bytes dds(UINT code, DXGI_FORMAT format, UINT w, UINT h, UINT levels, UINT alpha=0) {
    const bool dx10 = code == cc('D','X','1','0');
    Bytes b(dx10 ? 148 : 128);
    word(b,0,cc('D','D','S',' ')); word(b,4,124); word(b,8,6);
    word(b,12,h); word(b,16,w); word(b,28,levels);
    word(b,76,32); word(b,80,4); word(b,84,code);
    if (dx10) { word(b,128,UINT(format)); word(b,132,3); word(b,140,1); word(b,144,alpha); }
    for (UINT level=0; level<levels; ++level) {
        UINT row=0,slice=0;
        if (!textures::compressed_mip_layout(format,w,h,row,slice)) return b;
        for (UINT i=0;i<slice;++i) b.push_back(std::uint8_t(i+17*level));
        w=std::max(1u,w/2); h=std::max(1u,h/2);
    }
    return b;
}
static void run_dds(const char* label,const Bytes& b) {
    textures::CompressedImage image;
    image.format=DXGI_FORMAT_BC3_UNORM; image.width=37; image.height=19;
    image.mips.push_back({37,19,5,6,7}); image.bytes={9,8,7};
    std::string error="sentinel";
    const HRESULT hr=textures::parse_compressed_dds(b.data(),b.size(),image,error);
    if (FAILED(hr) && (image.format != DXGI_FORMAT_BC3_UNORM || image.width != 37 ||
        image.height != 19 || image.mips.size() != 1 || image.mips[0].offset != 7 ||
        image.bytes != Bytes({9,8,7}))) std::abort();
    std::printf("D %s %08x %s %u %u %u %u %zu %016llx %zu",
        label,UINT(hr),error.c_str(),UINT(image.format),image.width,image.height,
        UINT(image.alpha_mode),image.bytes.size(),(unsigned long long)hash(image.bytes),image.mips.size());
    for (const auto& m:image.mips)
        std::printf(" %u,%u,%u,%u,%zu",m.width,m.height,m.row_pitch,m.slice_pitch,m.offset);
    std::puts("");
    if (SUCCEEDED(hr)) {
        error="sentinel";
        HRESULT validation=textures::validate_compressed_image(image,error);
        std::printf("V %s %08x %s\n",label,UINT(validation),error.c_str());
        image.mips[0].offset++;
        error="sentinel";
        validation=textures::validate_compressed_image(image,error);
        std::printf("M %s %08x %s\n",label,UINT(validation),error.c_str());
    }
}
static void run_upload(const char* label,UINT format,UINT width,UINT height,
                       const Bytes& data,UINT pitch,size_t declared=0) {
    compat::TextureUpload out;
    out.format=DXGI_FORMAT_BC7_UNORM; out.width=31; out.height=29; out.row_pitch=13;
    out.bytes={3,5,7};
    const HRESULT hr=compat::prepare_texture_upload(format,width,height,data.data(),
        declared ? declared : data.size(),pitch,out);
    if (FAILED(hr) && (out.format != DXGI_FORMAT_BC7_UNORM || out.width != 31 ||
        out.height != 29 || out.row_pitch != 13 || out.bytes != Bytes({3,5,7}))) std::abort();
    std::printf("U %s %08x %u %u %u %u %zu %016llx\n",label,UINT(hr),
        UINT(out.format),out.width,out.height,out.row_pitch,out.bytes.size(),
        (unsigned long long)hash(out.bytes));
}
static void run_validation_variants() {
    auto source=dds(cc('D','X','1','0'),DXGI_FORMAT_BC5_UNORM,9,7,4,2);
    textures::CompressedImage original;
    std::string error;
    if (FAILED(textures::parse_compressed_dds(source.data(),source.size(),original,error))) std::abort();
    for (UINT i=0;i<12;++i) {
        auto image=original;
        switch (i) {
        case 0: image.format=DXGI_FORMAT_BC6H_UF16; break;
        case 1: image.width=0; break;
        case 2: image.height=16385; break;
        case 3: image.alpha_mode=static_cast<textures::AlphaMode>(5); break;
        case 4: image.mips.clear(); break;
        case 5: image.mips.push_back(image.mips.back()); break;
        case 6: image.mips[0].width++; break;
        case 7: image.mips[1].row_pitch++; break;
        case 8: image.mips[2].slice_pitch++; break;
        case 9: image.mips[1].offset++; break;
        case 10: image.bytes.pop_back(); break;
        case 11: image.bytes.push_back(0xa5); break;
        }
        error="sentinel";
        const HRESULT hr=textures::validate_compressed_image(image,error);
        std::printf("X %02u %08x %s\n",i,UINT(hr),error.c_str());
    }
}
static void run_generated() {
    const struct { UINT code; DXGI_FORMAT format; } formats[] = {
        {cc('D','X','T','1'),DXGI_FORMAT_BC1_UNORM},
        {cc('D','X','T','2'),DXGI_FORMAT_BC2_UNORM},
        {cc('D','X','T','3'),DXGI_FORMAT_BC2_UNORM},
        {cc('D','X','T','4'),DXGI_FORMAT_BC3_UNORM},
        {cc('D','X','T','5'),DXGI_FORMAT_BC3_UNORM},
        {cc('A','T','I','1'),DXGI_FORMAT_BC4_UNORM},
        {cc('B','C','4','U'),DXGI_FORMAT_BC4_UNORM},
        {cc('B','C','4','S'),DXGI_FORMAT_BC4_SNORM},
        {cc('A','T','I','2'),DXGI_FORMAT_BC5_UNORM},
        {cc('B','C','5','U'),DXGI_FORMAT_BC5_UNORM},
        {cc('B','C','5','S'),DXGI_FORMAT_BC5_SNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC1_UNORM_SRGB},
        {cc('D','X','1','0'),DXGI_FORMAT_BC2_UNORM_SRGB},
        {cc('D','X','1','0'),DXGI_FORMAT_BC3_UNORM_SRGB},
        {cc('D','X','1','0'),DXGI_FORMAT_BC4_UNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC4_SNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC5_UNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC5_SNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC7_UNORM},
        {cc('D','X','1','0'),DXGI_FORMAT_BC7_UNORM_SRGB},
    };
    const UINT fields[]={0,4,8,12,16,24,28,76,80,84,112,128,132,136,140,144};
    const UINT values[]={0,1,2,3,4,7,8,31,32,124,16384,16385,0xffffffffu};
    char name[48];
    for (UINT i=0;i<300;++i) {
        const auto& f=formats[random_word()% (sizeof(formats)/sizeof(formats[0]))];
        const UINT w=1+random_word()%17, h=1+random_word()%17;
        UINT full=1, tw=w, th=h;
        while (tw>1 || th>1) { tw=std::max(1u,tw/2); th=std::max(1u,th/2); ++full; }
        Bytes b=dds(f.code,f.format,w,h,1+random_word()%full,random_word()%5);
        switch (i%6) {
        case 0: break;
        case 1: {
            const UINT field=fields[random_word()%(sizeof(fields)/sizeof(fields[0]))];
            if (field+4<=b.size()) word(b,field,values[random_word()%(sizeof(values)/sizeof(values[0]))]);
            break;
        }
        case 2: {
            const size_t at=random_word()%b.size();
            b[at]^=std::uint8_t(1u<<(random_word()%8)); break;
        }
        case 3: b.resize(random_word()%b.size()); break;
        case 4: b.insert(b.end(),random_word()%9,std::uint8_t(random_word())); break;
        case 5: {
            const UINT field=f.code==cc('D','X','1','0') ? 144 : 80;
            word(b,field,random_word()); break;
        }
        }
        std::snprintf(name,sizeof(name),"generated_dds_%03u",i);
        run_dds(name,b);
    }
    const UINT uploads[]={D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8,D3DFMT_R8G8B8,
        D3DFMT_X1R5G5B5,D3DFMT_A1R5G5B5,D3DFMT_A4R4G4B4,D3DFMT_R5G6B5,
        D3DFMT_A8,D3DFMT_A8L8,D3DFMT_L8,D3DFMT_DXT1,D3DFMT_DXT2,
        D3DFMT_DXT3,D3DFMT_DXT4,D3DFMT_DXT5};
    for (UINT format:uploads) for (UINT sample=0;sample<32;++sample) {
        const UINT w=1+random_word()%17, h=1+random_word()%17;
        const bool bc=format==D3DFMT_DXT1 || format==D3DFMT_DXT2 ||
                      format==D3DFMT_DXT3 || format==D3DFMT_DXT4 || format==D3DFMT_DXT5;
        const UINT rows=bc ? (h+3)/4 : h;
        const UINT input_row=bc ? ((w+3)/4)*(format==D3DFMT_DXT1 ? 8u : 16u) :
            w*((format==D3DFMT_A8R8G8B8 || format==D3DFMT_X8R8G8B8) ? 4u :
                format==D3DFMT_R8G8B8 ? 3u :
                (format==D3DFMT_A8 || format==D3DFMT_L8) ? 1u : 2u);
        const UINT pitch=input_row+(random_word()%9);
        const UINT required=(rows-1)*pitch+input_row;
        Bytes data(required+random_word()%11);
        for (auto& byte:data) byte=std::uint8_t(random_word());
        if (sample%5==1 && !data.empty()) data.resize(random_word()%required);
        const UINT tested_pitch=sample%5==2 ? input_row-1 : pitch;
        const UINT tested_w=sample%5==3 ? 0 : w;
        const UINT tested_h=sample%5==4 ? 16385 : h;
        std::snprintf(name,sizeof(name),"generated_upload_%08x_%02u",format,sample);
        run_upload(name,format,tested_w,tested_h,data,tested_pitch);
    }
}
int main() {
    run_validation_variants();
    auto bc1=dds(cc('D','X','T','1'),DXGI_FORMAT_BC1_UNORM,5,9,4);
    run_dds("bc1",bc1);
    auto bc5=dds(cc('A','T','I','2'),DXGI_FORMAT_BC5_UNORM,8,8,4);
    word(bc5,24,1); run_dds("bc5_depth1",bc5);
    auto bc7=dds(cc('D','X','1','0'),DXGI_FORMAT_BC7_UNORM_SRGB,7,3,3,4);
    run_dds("bc7_dx10",bc7);
    auto variant=bc7; word(variant,132,2); run_dds("dx10_3d",variant);
    variant=bc7; word(variant,144,8); run_dds("dx10_alpha",variant);
    variant=bc7; word(variant,128,DXGI_FORMAT_BC6H_UF16); run_dds("dx10_format",variant);
    variant=bc1; variant[0]=0; run_dds("magic",variant);
    variant=bc1; word(variant,12,16385); run_dds("dimension",variant);
    variant=bc1; word(variant,28,30); run_dds("mips",variant);
    variant=bc1; variant.pop_back(); run_dds("short_payload",variant);
    variant=bc1; variant.insert(variant.end(),{19,20}); run_dds("trailing",variant);
    variant=bc1; word(variant,80,0); run_dds("no_fourcc",variant);
    variant=bc1; word(variant,80,0x44); run_dds("flags",variant);
    run_dds("empty",{});
    variant=bc7; variant.resize(129); run_dds("short_dx10",variant);
    for (UINT f : {UINT(D3DFMT_A8R8G8B8),UINT(D3DFMT_X8R8G8B8),UINT(D3DFMT_R8G8B8),
                    UINT(D3DFMT_X1R5G5B5),UINT(D3DFMT_A1R5G5B5),UINT(D3DFMT_A4R4G4B4),
                    UINT(D3DFMT_R5G6B5),UINT(D3DFMT_A8),UINT(D3DFMT_A8L8),UINT(D3DFMT_L8),
                    UINT(D3DFMT_DXT1),UINT(D3DFMT_DXT2),UINT(D3DFMT_DXT3),
                    UINT(D3DFMT_DXT4),UINT(D3DFMT_DXT5)}) {
        Bytes data(256); for (size_t i=0;i<data.size();++i) data[i]=std::uint8_t(i*37+11);
        char name[40]; std::snprintf(name,sizeof(name),"fmt_%08x",f);
        run_upload(name,f,3,5,data,40);
    }
    Bytes data(256); for (size_t i=0;i<data.size();++i) data[i]=std::uint8_t(i);
    run_upload("short",D3DFMT_R8G8B8,3,5,data,20,88);
    run_upload("short_pitch",D3DFMT_R8G8B8,3,5,data,8);
    run_upload("bad_format",0,3,5,data,20);
    run_upload("bad_width",D3DFMT_A8,16385,5,data,20);
    run_upload("bad_height",D3DFMT_A8,3,0,data,20);
    run_generated();
}

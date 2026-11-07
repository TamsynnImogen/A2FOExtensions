#include "../renderer/native11/texture.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void invalid(TextureView view,const char* why){
    TexturePlan plan{TextureFormat::bc7,TextureAlpha::custom,777,888,9,123456};
    const TexturePlan before=plan;
    need(!plan_texture(view,plan),why);
    need(plan.format==before.format&&plan.alpha==before.alpha&&plan.width==before.width&&
        plan.height==before.height&&plan.mip_count==before.mip_count&&plan.storage_bytes==before.storage_bytes,
        "rejected texture does not replace plan");
}
}
int main(){
    std::array<std::uint8_t,200> data{};
    TextureMip mips[3]{{7,5,32,64,0,64},{3,2,16,16,64,16},{1,1,16,16,80,16}};
    TextureView view{TextureFormat::bc3,TextureAlpha::straight,7,5,mips,3,data.data(),96};
    TexturePlan plan{};need(plan_texture(view,plan),"NPOT compressed authored chain");
    need(plan.storage_bytes==96&&plan.mip_count==3&&plan.width==7&&plan.height==5,"BC block-row extent");
    auto changed=view;changed.mip_count=4;invalid(changed,"extra levels past full chain");
    changed=view;changed.mip_count=0;invalid(changed,"no levels");
    changed=view;changed.mips=nullptr;invalid(changed,"null descriptors");
    changed=view;changed.bytes=nullptr;invalid(changed,"null bytes");
    changed=view;changed.width=0;invalid(changed,"zero width");
    changed=view;changed.height=16385;invalid(changed,"oversized height");
    changed=view;changed.format=static_cast<TextureFormat>(17);invalid(changed,"unrecognized format");
    changed=view;changed.alpha=static_cast<TextureAlpha>(5);invalid(changed,"unrecognized alpha semantics");
    changed=view;changed.byte_count=95;invalid(changed,"truncated last level");
    changed=view;changed.byte_count=97;invalid(changed,"trailing canonical payload bytes");
    auto original=mips[1];mips[1].offset=63;invalid(view,"overlapping levels");mips[1]=original;
    mips[1].offset=65;invalid(view,"gap between levels");mips[1]=original;
    mips[1].offset=std::numeric_limits<std::uint64_t>::max();invalid(view,"offset overflow");mips[1]=original;
    mips[1].width=4;invalid(view,"wrong child dimensions");mips[1]=original;
    mips[1].row_pitch=8;invalid(view,"wrong compressed pitch");mips[1]=original;
    mips[1].slice_pitch=32;invalid(view,"wrong compressed slice");mips[1]=original;
    mips[1].byte_count=15;invalid(view,"descriptor payload truncation");mips[1]=original;
    TextureMip raw[3]{{7,5,28,140,0,140},{3,2,12,24,140,24},{1,1,4,4,164,4}};
    view={TextureFormat::bgra8_srgb,TextureAlpha::opaque,7,5,raw,3,data.data(),168};
    need(plan_texture(view,plan)&&plan.storage_bytes==168,"authored raw mip extent");
    raw[0].row_pitch=32;invalid(view,"raw noncanonical row padding");
    TextureMip minimum{1,1,8,8,0,8};view={TextureFormat::bc4_snorm,TextureAlpha::unknown,1,1,&minimum,1,data.data(),8};
    need(plan_texture(view,plan)&&plan.storage_bytes==8,"minimum BC mip keeps full block");
    for(auto format:{TextureFormat::bgra8,TextureFormat::rgba8,TextureFormat::bgra8_srgb,TextureFormat::rgba8_srgb}){
        TextureMip level{1,1,4,4,0,4};view={format,TextureAlpha::unknown,1,1,&level,1,data.data(),4};
        need(plan_texture(view,plan),"raw colour format admitted");
    }
    std::puts("PASS native11 texture plan: BC/raw authored mip extents, NPOT/minimum blocks, bounds and atomic rejection");
}

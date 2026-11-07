#include "../renderer/native11/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::vector<std::uint8_t> sod(const char* texture,bool two_groups){
    std::vector<std::uint8_t> b;
    auto word=[&](std::uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);};
    auto number=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);dword(bits);};
    auto text=[&](const char* s){word(static_cast<std::uint16_t>(std::strlen(s)));while(*s)b.push_back(*s++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);
    word(1);text("mat");for(float v:{.1f,.2f,.3f,.25f,.5f,.75f,0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(1);word(1);word(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,4.f,6.f,5.f})number(v);
    text("additive");dword(4);dword(1);text(texture);dword(0);text("");word(0);
    word(3);word(3);word(two_groups?2:1);
    for(float v:{0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f})number(v);
    for(float v:{0.f,0.f,1.f,0.f,0.f,1.f})number(v);
    for(int group=0;group<(two_groups?2:1);++group){
        word(1);text("mat");for(unsigned i=0;i<3;++i){word(i);word(i);}
    }
    b.push_back(0);word(0);word(0);word(0);return b;
}
std::vector<std::uint8_t> tga(){
    std::vector<std::uint8_t> b(24);b[2]=2;b[12]=2;b[14]=1;b[16]=24;b[17]=0x20;
    const std::uint8_t pixels[]{3,2,1,6,5,4};std::memcpy(b.data()+18,pixels,6);return b;
}
struct Fixture {
    std::vector<std::uint8_t> model=sod("",true),texture=tga();
    unsigned reads=0,texture_reads=0,calls=0;
    bool fail_late=false,bad_material=false,throw_late=false;
    std::vector<MaterialSeed> seeds;
    static bool read(void* raw,AssetKind kind,const std::string& name,
        std::vector<std::uint8_t>& output,std::string& error){
        auto& f=*static_cast<Fixture*>(raw);++f.reads;
        if(kind==AssetKind::model){if(name!="view-1")return false;output=f.model;return true;}
        ++f.texture_reads;
        if(name!="selected.tga"){error="wrong texture ID";return false;}
        output=f.texture;return true;
    }
    static bool resolve(void* raw,const std::string& name,const MaterialSeed& seed,
        LiveMaterial& output,std::string& error){
        auto& f=*static_cast<Fixture*>(raw);++f.calls;f.seeds.push_back(seed);
        if(name!="view-1")return false;
        if(f.throw_late&&seed.part_index==1)throw std::runtime_error("late callback");
        if(f.fail_late&&seed.part_index==1){error="late material unavailable";return false;}
        output.texture_identifier=seed.part_index==0?"selected.tga":"";
        output.colour[0]=.4f;output.emissive[0]=.2f;output.flags=seed.part_index?cutout:alpha_blend;
        output.sampler=Sampling::point_wrap;output.depth_mode=DepthMode::read_only;
        output.alpha_cutoff=.3f;output.uv_transform[2]=.25f;
        if(f.bad_material&&seed.part_index==1)output.flags=0x100;
        return true;
    }
    AssetReader reader(){AssetReader result;result.context=this;result.read=read;
        result.resolve_live_material=resolve;return result;}
};
}
int main(){
    Fixture f;ModelContent output;std::string error;
    need(load_model_content(f.reader(),"view-1",output,error,true)&&error.empty()&&
         output.parts.size()==2&&output.textures.size()==1&&f.calls==2&&f.texture_reads==1,
         "all groups resolve including originally untextured part");
    need(output.parts[0].seed.node_index==0&&output.parts[0].seed.group_index==0&&
         output.parts[1].seed.node_index==0&&output.parts[1].seed.group_index==1&&
         output.parts[0].seed.part_index==0&&output.parts[1].seed.part_index==1&&
         output.parts[0].seed.authored_texture.empty()&&output.parts[0].seed.flags==9&&
         output.parts[0].seed.colour[0]==0.f&&output.parts[0].seed.emissive[0]==.25f,
         "detailed parser identity and immutable authored seed retained");
    need(output.parts[0].texture_index==0&&output.parts[1].texture_index==no_texture&&
         output.parts[0].live_material.texture_identifier=="selected.tga"&&
         output.parts[1].live_material.texture_identifier.empty()&&
         output.parts[0].colour[0]==.4f&&output.parts[0].flags==alpha_blend&&
         output.parts[0].live_material.sampler==Sampling::point_wrap&&
         output.parts[0].live_material.depth_mode==DepthMode::read_only,
         "resolved texture, white, and per-part properties owned");
    const auto retained=output.parts.size();f.fail_late=true;
    need(!load_model_content(f.reader(),"view-1",output,error,true)&&
         error=="late material unavailable"&&output.parts.size()==retained,
         "late callback failure is atomic");
    f.fail_late=false;f.bad_material=true;
    need(!load_model_content(f.reader(),"view-1",output,error,true)&&output.parts.size()==retained,
         "invalid later material is atomic");
    f.bad_material=false;f.throw_late=true;
    need(!load_model_content(f.reader(),"view-1",output,error,true)&&output.parts.size()==retained,
         "throwing later callback is contained");
    f.throw_late=false;f.model=sod("selected.tga",true);f.calls=0;f.texture_reads=0;
    need(load_model_content(f.reader(),"view-1",output,error,true)&&
         output.parts[0].seed.authored_texture=="selected.tga"&&
         output.parts[1].texture_index==no_texture&&f.texture_reads==1,
         "authored textured part can resolve white without inherited texture");
    f.reads=0;
    need(!load_model_content(f.reader(),"view-1",output,error,false)&&f.reads==1,
         "live resolution refuses incomplete part scope before publication");
    std::puts("PASS native11 live material content and detailed identity");
}

#include "../renderer/native11/sprite_observation.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t out=0;std::memcpy(&out,&value,4);return out;}
struct Memory {
    std::map<std::uint32_t,std::uint32_t> words;
    unsigned reads=0,flip_at=0;
    bool low_read=false;
    std::uint32_t flip_address=0,flip_value=0;
};
bool read(void* raw,std::uint32_t address,void* output,std::size_t size){
    auto& memory=*static_cast<Memory*>(raw);++memory.reads;
    if(address<0x1000)memory.low_read=true;
    if(memory.flip_at&&memory.reads==memory.flip_at)
        memory.words[memory.flip_address]=memory.flip_value;
    const auto found=memory.words.find(address);
    if(size!=4||found==memory.words.end())return false;
    std::memcpy(output,&found->second,4);return true;
}
struct Owners {
    std::shared_ptr<const TextureAsset> first,second;
    unsigned calls=0;bool replace=false;
};
std::shared_ptr<const TextureAsset> resolve(void* raw,std::uint32_t id){
    auto& owners=*static_cast<Owners*>(raw);++owners.calls;
    if(id!=0x7000)return nullptr;
    return owners.replace&&owners.calls==2?owners.second:owners.first;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t serial){
    auto owner=std::make_shared<TextureAsset>();owner->serial=serial;owner->texture=0x7000;
    owner->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1,2,3});
    return owner;
}
struct Fixture {
    SpriteDispatch dispatch{};SpriteReadProfile profile{};Memory memory{};Owners owners{};
    Fixture(){
        dispatch.device=0x1000;dispatch.dispatch_table=0x2000;dispatch.sprite=0x3000;
        dispatch.position=0x4000;dispatch.size=0x5000;
        dispatch.uv[0]=.125f;dispatch.uv[1]=.25f;
        dispatch.uv[2]=.5f;dispatch.uv[3]=.25f;
        dispatch.native_x87_control=encode_native_x87(0x037f);
        profile={0x2000,0x6000,0x9000};
        auto& w=memory.words;
        w[0x1000]=0x2000;w[0x2000+0xc4]=0x9000;w[0x1000+0x44]=0x8000;
        w[0x3000]=0x6000;w[0x3000+0x24]=bits(1.f);
        w[0x3000+0x28]=bits(0.f);w[0x3000+0x2c]=bits(0.f);
        w[0x3000+0x54]=0x8000;w[0x3000+0x58]=0x7000;
        w[0x8000+0x18]=1;w[0x8000+0x1c]=2;w[0x8000+0x20]=3;
        w[0x8000+0x28]=4;w[0x8000+0x24]=bits(.5f);
        w[0x1000+0x264]=5;w[0x1000+0x268]=6;
        w[0x1000+0x270]=7;w[0x1000+0x274]=8;w[0x1000+0x278]=9;
        w[0x4000]=bits(10.f);w[0x4004]=bits(20.f);w[0x4008]=bits(.25f);
        w[0x5000]=bits(30.f);w[0x5004]=bits(40.f);
        owners.first=texture(1);owners.second=texture(2);
    }
    bool capture(std::shared_ptr<const SpriteObservation>& out,std::string& error){
        memory.reads=owners.calls=0;
        return capture_sprite_observation(dispatch,profile,&memory,read,&owners,resolve,out,error);
    }
};
void rejected(Fixture& fixture,const char* why){
    auto sentinel=std::make_shared<SpriteObservation>();sentinel->packed_argb=0xdeadbeefu;
    std::shared_ptr<const SpriteObservation> output=sentinel;std::string error="stale";
    need(!fixture.capture(output,error)&&output==sentinel&&!error.empty(),why);
}
void cases(){
    Fixture stable;std::shared_ptr<const SpriteObservation> output;std::string error="stale";
    need(stable.capture(output,error)&&error.empty()&&output&&
         stable.memory.reads==48&&stable.owners.calls==2&&
         output->packed_argb==0x80ff0000u&&output->material==0x8000&&
         output->texture==0x7000&&output->texture_owner==stable.owners.first&&
         output->alpha_test==1&&output->src_blend==2&&output->dst_blend==3&&
         output->fill_mode==4&&output->address_u==5&&output->address_v==6&&
         output->mag_filter==7&&output->min_filter==8&&output->mip_filter==9&&
         output->geometry.left==10&&output->geometry.top==20&&output->geometry.depth==.25f&&
         output->geometry.width==30&&output->geometry.height==40&&
         output->geometry.uv.u0==.125f&&output->geometry.uv.v0==.25f&&
         output->geometry.uv.u1==.625f&&output->geometry.uv.v1==.5f,
         "stable two-pass sprite captures raw style, source colour, UV and owner");
    Fixture white;white.memory.words[0x3000+0x58]=0;
    std::shared_ptr<const SpriteObservation> no_texture;
    need(capture_sprite_observation(white.dispatch,white.profile,&white.memory,read,nullptr,nullptr,
         no_texture,error)&&no_texture&&no_texture->texture==0&&
         !no_texture->texture_owner&&error.empty(),"source texture zero needs no owner callback");
    Fixture bad;bad.profile.device_vtable=0x2001;rejected(bad,"wrong device vtable rejected");
    bad=Fixture{};bad.memory.words[0x2000+0xc4]=0x9001;rejected(bad,"wrong draw target rejected");
    bad=Fixture{};bad.memory.words[0x3000]=0x6001;rejected(bad,"wrong sprite vtable rejected");
    bad=Fixture{};bad.memory.words[0x3000+0x54]=0x8001;rejected(bad,"material pointer mismatch rejected");
    bad=Fixture{};bad.memory.words[0x1000+0x44]=0;
    bad.memory.words[0x3000+0x54]=0;bad.memory.words[0x18]=123;
    rejected(bad,"null matching material pointer rejected");
    need(bad.memory.reads==9&&!bad.memory.low_read,"null material never reads low memory");
    bad=Fixture{};bad.dispatch.device=std::numeric_limits<std::uint32_t>::max()-1;
    rejected(bad,"four-byte device span overflow rejected");
    need(bad.memory.reads==0,"overflowing device span rejected before read callback");
    bad=Fixture{};bad.dispatch.device=0;
    rejected(bad,"null device rejected");
    need(bad.memory.reads==0,"null device rejected before read callback");
    bad=Fixture{};bad.dispatch.dispatch_table=
        std::numeric_limits<std::uint32_t>::max()-0xc4u-2u;
    rejected(bad,"four-byte draw target span overflow rejected");
    need(bad.memory.reads==1,"overflowing draw target rejected before its read callback");
    bad=Fixture{};bad.dispatch.native_x87_control=0;rejected(bad,"unknown CW rejected");
    bad=Fixture{};bad.memory.words[0x4000]=bits(std::numeric_limits<float>::infinity());
    rejected(bad,"nonfinite position rejected");
    bad=Fixture{};bad.memory.words[0x4008]=bits(1.1f);rejected(bad,"depth outside unit range rejected");
    bad=Fixture{};bad.memory.words[0x5000]=bits(-1.f);rejected(bad,"negative size rejected");
    bad=Fixture{};bad.dispatch.uv[0]=std::numeric_limits<float>::quiet_NaN();
    rejected(bad,"nonfinite UV rejected");
    bad=Fixture{};bad.dispatch.uv[0]=1000000.f;bad.dispatch.uv[2]=1.f;
    rejected(bad,"computed UV endpoint above bound rejected");
    bad=Fixture{};bad.memory.words.erase(0x3000+0x24);rejected(bad,"unreadable colour rejected");
    bad=Fixture{};bad.memory.flip_at=25;bad.memory.flip_address=0x4000;
    bad.memory.flip_value=bits(11.f);rejected(bad,"changed position in second pass rejected");
    bad=Fixture{};bad.memory.flip_at=25;bad.memory.flip_address=0x3000+0x54;
    bad.memory.flip_value=0x8001;rejected(bad,"changed material pointer rejected");
    bad=Fixture{};bad.memory.flip_at=25;bad.memory.flip_address=0x3000+0x58;
    bad.memory.flip_value=0;rejected(bad,"changed texture pointer rejected");
    bad=Fixture{};bad.memory.flip_at=25;bad.memory.flip_address=0x1000+0x264;
    bad.memory.flip_value=10;rejected(bad,"changed raw address U rejected");
    bad=Fixture{};bad.memory.flip_at=25;bad.memory.flip_address=0x1000+0x274;
    bad.memory.flip_value=10;rejected(bad,"changed raw min filter rejected");
    bad=Fixture{};bad.owners.replace=true;rejected(bad,"owner replacement between resolutions rejected");
    bad=Fixture{};bad.owners.first.reset();rejected(bad,"missing nonzero texture owner rejected");
}
}
int main(){cases();std::puts("PASS native11 sprite observation");}

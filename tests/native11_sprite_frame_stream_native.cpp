// CPU frontend fixture. Its parser stub isolates frame assembly from the Rust
// texture parser, which has separate native tests.
#include "../renderer/native11/sprite_frame_stream.hpp"
#include "../renderer/native11/assets.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
using namespace a2fo::native11;
namespace {
unsigned parses=0;
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
bool close_to(float a,float b){return std::abs(a-b)<0.00001f;}
std::shared_ptr<const TextureAsset> asset(std::uint64_t id,std::uint8_t format=1,
    bool key=false,std::uint8_t blue=7){
    auto value=std::make_shared<TextureAsset>();value->serial=id;value->colour_key=key;
    value->bytes=std::make_shared<const std::vector<std::uint8_t>>(
        std::initializer_list<std::uint8_t>{'T',format,blue,0,255,90});
    return value;
}
CapturedSpriteBatch batch(const std::shared_ptr<const TextureAsset>& texture){
    CapturedSpriteBatch value;value.view=1;value.width=value.height=32;
    value.generation=1;value.source_ticket=3;
    CapturedScreenSprite sprite;sprite.texture=texture;
    sprite.geometry.width=sprite.geometry.height=32;sprite.geometry.depth=.3f;
    sprite.geometry.has_clip=true;sprite.geometry.clip={8,8,24,24};
    sprite.geometry.uv={0,0,1,1};sprite.colour[0]=.25f;
    sprite.additive=true;sprite.sampling=Sampling::point_clamp;
    value.sprites.push_back(sprite);return value;
}
void reject(SpriteFrameStream& stream,const CapturedSpriteBatch& input,std::uint64_t epoch,
    std::shared_ptr<const PrimitiveFrame>& out,std::string& error,const char* why){
    const auto before=out;
    need(!stream.update(input,epoch,out,error),why);
    need(out==before&&!error.empty(),"rejected frame keeps output and reason");
}
}
extern "C" int a2fo_native11_parse_texture(const std::uint8_t* data,std::uint32_t length,
    A2FO_N11_TextureSink sink,void* context){
    ++parses;
    if(!data||length!=6||data[0]!='T')return 0;
    A2FO_N11_TextureMip mip{1,1,4,4,0,4};
    A2FO_N11_Texture input{data[1],1,1,1,1,0,4,data+2,&mip};
    return sink(context,&input);
}
int main(){
    SpriteFrameStream stream;std::shared_ptr<const PrimitiveFrame> out;std::string error;
    auto red=asset(7);auto input=batch(red);
    need(stream.update(input,1,out,error),"initial clipped sprite");
    need(out->meshes.size()==1&&out->meshes[0].vertices.size()==4&&out->textures.size()==1&&
         out->draws.size()==1&&out->source_epoch==1&&parses==1,"unit quad and first parse");
    const auto& d=out->draws[0];
    need(d.mesh==1&&d.texture==7&&close_to(d.surface.local_to_world[0],1)&&
         close_to(d.surface.local_to_world[3],-.5f)&&close_to(d.surface.local_to_world[5],-1)&&
         close_to(d.surface.local_to_world[7],.5f)&&close_to(d.surface.local_to_world[11],.3f),
         "clipped NDC unit transform");
    need(close_to(d.surface.uv_transform[0],.5f)&&close_to(d.surface.uv_transform[2],.25f)&&
         close_to(d.surface.uv_transform[4],.5f)&&close_to(d.surface.uv_transform[5],.25f),
         "clipped atlas UV transform");
    need(d.surface.flags==(two_sided|additive)&&d.surface.sampler==Sampling::point_clamp&&
         close_to(d.surface.colour[0],.25f)&&d.surface.depth_mode==DepthMode::disabled&&
         !d.surface.casts_shadow&&!d.surface.receives_shadow&&!d.surface.fog_enabled,
         "HUD draw material and state");
    need(!out->settings.clear_colour_target&&!out->settings.clear_depth_target&&
         close_to(out->settings.ambient[0],1)&&close_to(out->settings.viewport[2],32),"overlay settings");
    input.sprites.push_back(input.sprites[0]);input.generation=2;
    auto held=out;
    need(stream.update(input,2,out,error)&&out->draws.size()==2&&out->textures.size()==1&&
         out->source_epoch==1&&parses==1,"repeat texture hot resource epoch");
    need(held->draws.size()==1&&held->textures[0].bgra[0]==7,"published old frame immutable");
    input.generation=3;input.sprites[1].texture=asset(7,1,false,12);
    reject(stream,input,3,out,error,"same serial different owner in one batch");
    input.sprites[1].texture=red;
    input.sprites[0].texture=asset(9);input.sprites[1].texture=input.sprites[0].texture;
    need(stream.update(input,3,out,error)&&out->source_epoch==3&&out->textures[0].id==9&&
         parses==2,"palette replacement advances epoch");
    input.generation=4;input.source_ticket=4;
    need(stream.update(input,4,out,error)&&out->source_epoch==4&&parses==2,
         "source ticket advances epoch with decoded cache");
    auto stale=input;reject(stream,stale,5,out,error,"stale generation");
    stale.generation=5;reject(stream,stale,4,out,error,"stale fresh epoch");
    stale.sprites[0].colour[2]=2;reject(stream,stale,5,out,error,"invalid colour");
    stale=input;stale.generation=5;stale.sprites[0].texture=asset(10,3);
    reject(stream,stale,5,out,error,"sRGB refused");
    stale.sprites[0].texture=asset(10,11);
    reject(stream,stale,5,out,error,"unsupported BC4 refused");
    stale.sprites[0].texture=asset(10,1);
    stale.sprites[0].geometry.width=0;
    reject(stream,stale,5,out,error,"malformed geometry refused");
    auto source=input.sprites[0].texture;input.sprites.clear();input.generation=5;
    need(stream.update(input,5,out,error)&&out->textures.empty()&&out->draws.empty()&&
         out->meshes.size()==1,"empty HUD batch retains quad manifest");
    source.reset();red.reset();stream.clear();
    need(held->textures[0].bgra[0]==7,"removed source cannot poison old packet");
    input.generation=6;reject(stream,input,5,out,error,"clear retains epoch watermark");
    need(stream.update(input,6,out,error)&&out->source_epoch==6,"clear accepts fresh epoch");
    input=batch(asset(17));input.generation=7;
    input.sprites[0].geometry.clip={0,0,1e-7f,32};
    need(stream.update(input,7,out,error)&&out->draws.empty(),
         "float-collapsed subpixel clip cannot publish singular draw");
    std::puts("PASS native11 sprite frame stream: clipped UV, hot cache, epochs, ownership and rejection");
}

// Private HWND-free API11 material packet and offscreen GPU regression.
#include "../sdk/include/a2fo_native11_live_material_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
namespace {
void need(bool pass,const char* why){if(!pass){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(std::int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,
    static_cast<unsigned long>(hr));std::exit(1);}}
void identity(float out[16]){for(unsigned i=0;i<16;++i)out[i]=(i%5)==0?1.f:0.f;}
std::uint32_t bits(float value){std::uint32_t out=0;std::memcpy(&out,&value,4);return out;}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r){
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=b;bytes[19]=g;bytes[20]=r;return bytes;
}
enum class Mode {valid,short_packet,duplicate,no_sink,unknown_flags};
struct Host {
    std::vector<std::uint8_t> sod=native11_fixture::model("",false);
    std::vector<std::uint8_t> red=tga(0,0,255),blue=tga(255,0,0);
    const A2FO_N11_API11* api=nullptr;A2FO_N11_Session session=0;
    unsigned model_reads=0,texture_reads=0,policy_calls=0,part_calls=0,live_calls=0;
    std::string left_texture="red.tga",right_texture="blue.tga";
    float left_colour=1.f,left_emissive=0.f;
    std::uint32_t left_flags=0;
    Mode mode=Mode::valid;bool reenter=false,reentry_refused=false;
};
std::int32_t A2FO_N11_CALL asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);const std::string key(reinterpret_cast<const char*>(id),length);
    const std::vector<std::uint8_t>* bytes=nullptr;
    if(kind==1&&(key=="left-view"||key=="right-view")){bytes=&h.sod;++h.model_reads;}
    if(kind==2){bytes=key=="red.tga"?&h.red:key=="blue.tga"?&h.blue:nullptr;++h.texture_reads;}
    return bytes&&bytes->size()<=limit?sink(context,bytes->data(),static_cast<std::uint32_t>(bytes->size())):0;
}
std::int32_t A2FO_N11_CALL part(void* raw,const std::uint8_t*,std::uint32_t,
    std::uint32_t index,const float authored[16],A2FO_N11_PartStateSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.part_calls;if(index)return 0;
    A2FO_N11_PartState state{};state.struct_size=sizeof(state);state.visible=1;
    std::memcpy(state.local_to_model,authored,sizeof(state.local_to_model));return sink(context,&state);
}
std::int32_t A2FO_N11_CALL policy(void* raw,const std::uint8_t*,std::uint32_t,
    A2FO_N11_GeometryPolicySink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.policy_calls;
    A2FO_N11_GeometryPolicy p{sizeof(p),0,bits(1.f),0x10000u|0x037fu};
    return sink(context,&p);
}
std::int32_t A2FO_N11_CALL live(void* raw,const std::uint8_t* model,std::uint32_t length,
    const A2FO_N11_MaterialSeed* seed,A2FO_N11_LiveMaterialSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.live_calls;
    const std::string view(reinterpret_cast<const char*>(model),length);
    need((view=="left-view"||view=="right-view")&&seed&&
        seed->struct_size==sizeof(*seed)&&seed->part_index==0&&seed->node_index==0&&
        seed->group_index==0&&seed->authored_texture_length==0,
        "exact all-part material seed identity");
    if(h.reenter){h.reenter=false;A2FO_N11_LiveMaterials same{sizeof(same),0,&h,live};
        h.reentry_refused=h.api->configure_live_materials(h.session,&same)==E_UNEXPECTED;}
    if(h.mode==Mode::no_sink)return 1;
    const auto& selected=view=="left-view"?h.left_texture:h.right_texture;
    A2FO_N11_LiveMaterial m{};m.struct_size=sizeof(m);m.flags=seed->flags;
    m.sampler=0;m.depth_mode=0;m.alpha_cutoff=.5f;
    std::memcpy(m.colour,seed->colour,sizeof(m.colour));
    std::memcpy(m.emissive,seed->emissive,sizeof(m.emissive));
    if(view=="left-view"){
        for(unsigned channel=0;channel<3;++channel)m.colour[channel]*=h.left_colour;
        m.emissive[2]+=h.left_emissive;
        m.flags|=h.left_flags;
    }
    m.uv_transform[0]=m.uv_transform[4]=1;
    m.texture_identifier=reinterpret_cast<const std::uint8_t*>(selected.data());
    m.texture_identifier_length=static_cast<std::uint32_t>(selected.size());
    if(h.mode==Mode::short_packet)--m.struct_size;
    if(h.mode==Mode::unknown_flags)m.flags=16;
    const auto result=sink(context,&m);
    if(h.mode==Mode::duplicate)sink(context,&m);
    return result;
}
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback request{};request.struct_size=sizeof(request);
    need(api.readback(session,&request)==S_FALSE&&request.width==32&&request.height==32&&
         request.required_bytes==4096,"readback metadata");
    std::vector<std::uint8_t> out(request.required_bytes);request.bgra=out.data();request.capacity=out.size();
    ok(api.readback(session,&request),"readback pixels");return out;
}
std::array<int,3> sample(const std::vector<std::uint8_t>& image,unsigned x){
    const auto index=(16*32+x)*4;return {image[index],image[index+1],image[index+2]};
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Usage result{};result.struct_size=sizeof(result);ok(api.usage(session,&result),"GPU usage");return result;
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"renderer DLL loaded");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API11 api11{};ok(get(11,sizeof(api11),&api11),"API11 query");
    auto& api=api11.base.base.base.base.base.base.base.base.base.base;
    need(api.version==11&&api.struct_size==sizeof(api11)&&
         (api.capabilities&A2FO_N11_CAP_LIVE_MATERIALS)&&api11.configure_live_materials&&
         api11.prepare_live_materials&&api11.submit_live_material_preview,"API11 table");
    const std::array<std::uint32_t,10> prefix_sizes{{40,48,52,56,60,64,68,76,80,88}};
    for(std::uint32_t version=1;version<=10;++version){
        std::array<std::uint8_t,88> prefix{};
        ok(get(version,prefix_sizes[version-1],prefix.data()),"legacy API query");
        std::uint32_t size=0,reported=0;std::memcpy(&size,prefix.data(),4);
        std::memcpy(&reported,prefix.data()+4,4);
        need(size==prefix_sizes[version-1]&&reported==version,"legacy API prefix intact");
    }
    Host h;h.api=&api11;A2FO_N11_Host request{sizeof(request),0,32,32,&h,asset,nullptr};
    ok(api.create(&request,&h.session),"offscreen create");
    A2FO_N11_PartStates states{sizeof(states),0,&h,part};
    A2FO_N11_GeometryPolicies policies{sizeof(policies),0,&h,policy};
    A2FO_N11_LiveMaterials materials{sizeof(materials),0,&h,live};
    ok(api11.base.base.base.base.base.base.base.configure_part_states(h.session,&states),"part states");
    ok(api11.base.configure_geometry_policies(h.session,&policies),"geometry policy");
    ok(api11.configure_live_materials(h.session,&materials),"live materials");
    A2FO_N11_LiveMaterials invalid=materials;--invalid.struct_size;
    need(api11.configure_live_materials(h.session,&invalid)==E_INVALIDARG,"short config refused");
    invalid=materials;invalid.reserved=1;
    need(api11.configure_live_materials(h.session,&invalid)==E_INVALIDARG,"reserved config refused");
    std::string left="left-view",right="right-view";
    A2FO_N11_Object objects[2]{};
    for(auto& object:objects)identity(object.local_to_world);
    objects[0].local_to_world[3]=-.7f;objects[1].local_to_world[3]=.7f;
    objects[0].model_identifier=reinterpret_cast<const std::uint8_t*>(left.data());
    objects[0].model_identifier_length=left.size();
    objects[1].model_identifier=reinterpret_cast<const std::uint8_t*>(right.data());
    objects[1].model_identifier_length=right.size();
    A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);frame.generation=frame.source_epoch=1;
    frame.flags=A2FO_N11_FRAME_REQUIRE_PART_STATES;identity(frame.world_to_clip);
    frame.viewport[2]=frame.viewport[3]=32;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.object_count=2;frame.objects=objects;
    auto before=usage(api,h.session);
    need(api11.prepare_live_materials(h.session,&frame)==S_OK,"prepare live resources");
    auto prepared=usage(api,h.session);
    need(prepared.meshes>before.meshes&&prepared.textures>=before.textures+2&&h.texture_reads>=2,
         "prepare uploads model and selected textures");
    h.reenter=true;
    ok(api11.submit_live_material_preview(h.session,&frame),"first material preview");
    need(h.reentry_refused&&h.live_calls>=2,"live callback and reentry guard");
    auto first=pixels(api,h.session);auto l=sample(first,8),r=sample(first,24);
    need(l[2]>l[0]+50&&r[0]>r[2]+50,"two actors use different selected textures");
    const auto callbacks=h.live_calls;h.left_texture.clear();++frame.generation;
    ok(api11.submit_live_material_preview(h.session,&frame),"white transition hot frame");
    need(h.live_calls>callbacks,"live callback runs on cached geometry");
    auto second=pixels(api,h.session);
    need(second!=first&&sample(second,8)[0]>sample(first,8)[0]+50,
         "selected diffuse switches to explicit white");
    h.left_colour=.2f;h.left_emissive=.25f;h.left_flags=2; ++frame.generation;
    ok(api11.submit_live_material_preview(h.session,&frame),"colour emission and flags frame");
    const auto settled=pixels(api,h.session);
    need(settled!=second&&sample(settled,8)[0]<sample(second,8)[0]&&
         sample(settled,8)[0]>sample(settled,8)[2],
         "authored packet colour and emissive affect GPU pixels");
    for(auto mode:{Mode::short_packet,Mode::duplicate,Mode::no_sink,Mode::unknown_flags}){
        h.mode=mode;++frame.generation;
        need(api11.submit_live_material_preview(h.session,&frame)<0&&pixels(api,h.session)==settled,
             "malformed live callback refused without drawing");
    }
    h.mode=Mode::valid;h.left_texture="blue.tga";++frame.generation;
    ok(api11.submit_live_material_preview(h.session,&frame),"recovery material preview");
    need(pixels(api,h.session)!=settled,"recovered frame changes pixels");
    frame.flags=0;++frame.generation;
    need(api11.submit_live_material_preview(h.session,&frame)==E_INVALIDARG,
         "live preview requires observed part states");
    ok(api.destroy(h.session),"destroy offscreen session");need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path required");
    run(argv[1]);std::puts("PASS native11 live material GPU");}

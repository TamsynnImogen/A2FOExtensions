// Public SDK client: complete caster assertion and shadow-only participation.
#include "../sdk/include/a2fo_native11_world_api.h"
#include "native11_model_fixture.hpp"
#include "../renderer/native11/capture_service.hpp"
#include "../renderer/native11/lighting_service.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
extern "C" std::int32_t native11_c_query5(A2FO_N11_GetInterface,A2FO_N11_API5*);

static_assert(sizeof(A2FO_N11_API5)==60,"i686 API5 layout");
static_assert(offsetof(A2FO_N11_API5,submit_world)==56,"API5 append offset");
namespace {
unsigned downgraded_calls=0;
std::int32_t A2FO_N11_CALL rejected_submit(A2FO_N11_Session,const A2FO_N11_Frame*){++downgraded_calls;return S_OK;}
std::int32_t A2FO_N11_CALL rejected_config(A2FO_N11_Session,const A2FO_N11_Shadows*){++downgraded_calls;return S_OK;}
void need(bool pass,const char* name) {
    if (!pass) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
void ok(std::int32_t hr,const char* name) {
    if (hr<0) { std::fprintf(stderr,"FAIL %s hr=%08lx\n",name,static_cast<unsigned long>(hr));std::exit(1); }
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r) {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=b;bytes[19]=g;bytes[20]=r;
    return bytes;
}
struct Host {
    std::vector<std::uint8_t> receiver=native11_fixture::model("white.tga");
    std::vector<std::uint8_t> blocker=native11_fixture::model("blue.tga");
    std::vector<std::uint8_t> white=tga(255,255,255),blue=tga(255,0,0);
    unsigned reads=0,part_calls=0,receiver_parts=0,blocker_parts=0;
    bool hide_blocker=false;
};
std::int32_t A2FO_N11_CALL asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context) {
    auto& h=*static_cast<Host*>(raw);++h.reads;
    const std::string name(reinterpret_cast<const char*>(id),length);
    const std::vector<std::uint8_t>* bytes=nullptr;
    if (kind==1) bytes=name=="receiver-view"?&h.receiver:name=="blocker-view"?&h.blocker:nullptr;
    if (kind==2) bytes=name=="white.tga"?&h.white:name=="blue.tga"?&h.blue:nullptr;
    if (!bytes||bytes->size()>limit) return 0;
    return sink(context,bytes->data(),static_cast<std::uint32_t>(bytes->size()));
}
std::int32_t A2FO_N11_CALL part_state(void* raw,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t ordinal,const float authored[16],
    A2FO_N11_PartStateSink sink,void* context) {
    auto& h=*static_cast<Host*>(raw);++h.part_calls;
    need(ordinal==0,"stable sole part ordinal");
    const std::string name(reinterpret_cast<const char*>(id),length);
    const bool blocker=name=="blocker-view";
    need(blocker||name=="receiver-view","part callback exact model ID");
    if (blocker) ++h.blocker_parts;else ++h.receiver_parts;
    A2FO_N11_PartState state{};state.struct_size=sizeof(state);
    state.visible=blocker&&h.hide_blocker?0:1;
    std::memcpy(state.local_to_model,authored,sizeof(state.local_to_model));
    return sink(context,&state);
}
void identity(float out[16]) { for(int i=0;i<16;++i)out[i]=(i%5)==0?1.f:0.f; }
struct Scene {
    std::array<A2FO_N11_Object,2> objects{};
    A2FO_N11_Light light{};
    A2FO_N11_Frame frame{};
    const std::array<std::string,2> names{{"receiver-view","blocker-view"}};
    Scene() {
        for (std::size_t i=0;i<objects.size();++i) {
            auto& object=objects[i];identity(object.local_to_world);
            object.model_identifier=reinterpret_cast<const std::uint8_t*>(names[i].data());
            object.model_identifier_length=static_cast<std::uint32_t>(names[i].size());
        }
        objects[0].local_to_world[11]=.6f; // fixture vertex z=.2 -> receiver z=.8
        for (int index:{0,5,10}) objects[1].local_to_world[index]=.2f;
        objects[1].local_to_world[11]=.26f; // blocker vertex z=.2 -> z=.3
        light.type=1;light.direction[0]=.5f;light.direction[2]=1;
        light.colour[0]=1;light.intensity=.6f;
        frame.struct_size=sizeof(frame);frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;
        frame.generation=frame.source_epoch=1;identity(frame.world_to_clip);
        frame.viewport[2]=frame.viewport[3]=32;
        frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=.1f;
        frame.object_count=2;frame.objects=objects.data();frame.light_count=1;frame.lights=&light;
    }
};
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);
    need(api.readback(session,&read)==S_FALSE&&read.width==32&&read.height==32&&
         read.required_bytes==32*32*4,"readback metadata");
    std::vector<std::uint8_t> out(static_cast<std::size_t>(read.required_bytes));
    read.bgra=out.data();read.capacity=out.size();ok(api.readback(session,&read),"readback pixels");
    return out;
}
int channel(const std::vector<std::uint8_t>& data,unsigned x,unsigned y,unsigned c) {
    return data[(y*32+x)*4+c];
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Usage value{};value.struct_size=sizeof(value);
    ok(api.usage(session,&value),"resource usage");return value;
}
bool same_usage(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b) {
    return a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&
        a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;
}
void modules() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    need(snapshot!=INVALID_HANDLE_VALUE,"module snapshot");
    MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
    need(Module32FirstW(snapshot,&entry)!=0,"module enumeration begins");
    do {
        need(_wcsicmp(entry.szModule,L"d3d8.dll")&&_wcsicmp(entry.szModule,L"d3d9.dll")&&
             _wcsnicmp(entry.szModule,L"d3dx",4),"no legacy graphics modules");
    } while (Module32NextW(snapshot,&entry));
    const DWORD last=GetLastError();CloseHandle(snapshot);
    need(last==ERROR_NO_MORE_FILES,"module enumeration completes");
}
void run(const char* path) {
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"renderer DLL load");
    const FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");
    need(symbol!=nullptr,"public interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API api1{};A2FO_N11_API2 api2{};A2FO_N11_API3 api3{};
    A2FO_N11_API4 api4{};A2FO_N11_API5 api5{};
    ok(get(1,sizeof(api1),&api1),"API1 query");
    ok(get(2,sizeof(api2),&api2),"API2 query");
    ok(get(3,sizeof(api3),&api3),"API3 query");
    ok(get(4,sizeof(api4),&api4),"API4 query");
    ok(native11_c_query5(get,&api5),"API5 C query");
    auto internal=std::make_unique<a2fo::native11::Frame>();internal->camera=1;internal->generation=1;
    internal->complete_casters=true;
    auto legacy4=api4;legacy4.base.base.base.submit=rejected_submit;legacy4.base.base.configure_shadows=rejected_config;
    const float ambient[3]{.1f,.1f,.1f};
    need(a2fo::native11::submit_captured(legacy4,7,*internal,1,nullptr,0,ambient)==E_INVALIDARG,
        "internal world claim cannot downgrade through API4");
    auto legacy1=api1;legacy1.submit=rejected_submit;
    need(a2fo::native11::submit_captured(legacy1,7,*internal,1,nullptr,0,ambient)==E_INVALIDARG,
        "internal world claim cannot downgrade through API1");
    a2fo::native11::LightingFrame owned_lights;
    need(a2fo::native11::submit_lit_capture(legacy4,7,*internal,1,owned_lights)==E_INVALIDARG&&downgraded_calls==0,
        "world claim refuses legacy lit adapter before configuration");
    auto& api=api5.base.base.base.base;
    need(api1.version==1&&api2.base.version==2&&api3.base.base.version==3&&
         api4.base.base.base.version==4&&api.version==5&&api.struct_size==60&&
         (api.capabilities&A2FO_N11_CAP_WORLD_CASTERS)&&api5.submit_world&&
         api1.submit==api2.base.submit&&api1.submit==api3.base.base.submit&&
         api1.submit==api4.base.base.base.submit&&api1.submit==api.submit,
         "API1-4 prefixes unchanged and API5 complete");
    A2FO_N11_API5 malformed{};std::memset(&malformed,0x5a,sizeof(malformed));
    const auto before=malformed;
    need(get(5,sizeof(malformed)-1,&malformed)==E_INVALIDARG&&
         std::memcmp(&malformed,&before,sizeof(before))==0,"malformed API5 query untouched");

    Host host;A2FO_N11_Host request{sizeof(request),0,32,32,&host,asset,nullptr};
    A2FO_N11_Session session=0;ok(api.create(&request,&session),"offscreen session");
    Scene scene;A2FO_N11_Shadows shadows{sizeof(shadows),1,256,.001f};
    ok(api5.base.base.base.configure_shadows(session,&shadows),"directional shadows");
    A2FO_N11_PartStates states{sizeof(states),0,&host,part_state};
    ok(api5.base.configure_part_states(session,&states),"configure part visibility before cache warmup");
    scene.objects[1].reserved=A2FO_N11_OBJECT_SHADOW_ONLY;
    ok(api5.submit_world(session,&scene.frame),"shadow-only blocker");
    auto shadow_only=pixels(api,session);
    need(channel(shadow_only,16,16,2)>100&&
         channel(shadow_only,20,16,2)>15&&channel(shadow_only,20,16,2)<55&&
         channel(shadow_only,11,16,2)>100&&
         channel(shadow_only,20,16,2)+60<channel(shadow_only,11,16,2),
         "shadow-only blocker leaves visible red receiver and shadow footprint");
    need(host.reads==4,"distinct models and textures loaded once");
    const auto resident=usage(api,session);const auto reads=host.reads;
    scene.objects[1].reserved=0;++scene.frame.generation;
    ok(api5.submit_world(session,&scene.frame),"primary blue blocker");
    auto primary=pixels(api,session);
    need(channel(primary,16,16,0)>15&&channel(primary,16,16,0)<55&&
         channel(primary,16,16,2)<15&&host.reads==reads&&
         same_usage(resident,usage(api,session)),
         "primary blocker blue at centre without new assets");
    scene.objects[1].reserved=A2FO_N11_OBJECT_SHADOW_ONLY;++scene.frame.generation;
    ok(api5.submit_world(session,&scene.frame),"restore shadow-only role");
    need(pixels(api,session)==shadow_only&&host.reads==reads&&
         same_usage(resident,usage(api,session)),"role change only changes participation");
    const auto baseline=pixels(api,session);
    scene.frame.flags=0;
    need(api5.submit_world(session,&scene.frame)==E_INVALIDARG&&host.reads==reads&&
         pixels(api,session)==baseline,"missing completeness refuses before asset read");
    need(api4.base.base.base.submit(session,&scene.frame)==E_INVALIDARG&&
         host.reads==reads&&pixels(api,session)==baseline,
         "legacy API4 submit rejects shadow-only role");
    scene.objects[1].reserved=0;scene.frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;
    need(api1.submit(session,&scene.frame)==E_INVALIDARG&&
         api2.base.submit(session,&scene.frame)==E_INVALIDARG&&
         api3.base.base.submit(session,&scene.frame)==E_INVALIDARG&&
         api4.base.base.base.submit(session,&scene.frame)==E_INVALIDARG&&
         host.reads==reads&&pixels(api,session)==baseline,
         "all legacy submits reject complete-caster flag");
    scene.objects[1].reserved=2;
    need(api5.submit_world(session,&scene.frame)==E_INVALIDARG&&
         host.reads==reads&&pixels(api,session)==baseline,"unknown world role rejected");
    scene.objects[1].reserved=A2FO_N11_OBJECT_SHADOW_ONLY;
    shadows.light_mask=0;ok(api5.base.base.base.configure_shadows(session,&shadows),"disable shadow mask");
    ++scene.frame.generation;ok(api5.submit_world(session,&scene.frame),"unshadowed receiver");
    auto unshadowed=pixels(api,session);
    need(channel(unshadowed,20,16,2)>channel(shadow_only,20,16,2)+60&&
         host.reads==reads&&same_usage(resident,usage(api,session)),
         "zero mask clears footprint without asset change");
    shadows.light_mask=1;ok(api5.base.base.base.configure_shadows(session,&shadows),"restore shadow mask");
    ok(api5.base.configure_part_states(session,&states),"identical part visibility policy retains cache");
    host.hide_blocker=true;++scene.frame.generation;
    ok(api5.submit_world(session,&scene.frame),"hidden shadow-only blocker");
    auto hidden=pixels(api,session);
    need(host.receiver_parts>0&&host.blocker_parts>0&&host.part_calls>=2&&
         channel(hidden,20,16,2)>channel(shadow_only,20,16,2)+60&&
         host.reads==reads&&same_usage(resident,usage(api,session)),
         "part callback visits shadow-only geometry and hidden part stops casting");
    ok(api.destroy(session),"session destroy");modules();need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv) {
    need(argc==2,"usage: native11_world_casters_native.exe RENDERER_DLL");
    run(argv[1]);std::puts("PASS native11 complete world caster public API5");
}

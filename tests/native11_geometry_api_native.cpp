// Public API6 client for opt-in shared meshes with independent textures and poses.
#include "../sdk/include/a2fo_native11_geometry_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <array>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" std::int32_t native11_c_query6(A2FO_N11_GetInterface,A2FO_N11_API6*);
static_assert(sizeof(A2FO_N11_API5)==60,"i686 API5 prefix");
static_assert(sizeof(A2FO_N11_API6)==64,"i686 API6 table");
static_assert(offsetof(A2FO_N11_API6,configure_mesh_sharing)==60,"API6 append offset");
namespace {
void need(bool good,const char* label) {
    if (!good) { std::fprintf(stderr,"FAIL %s\n",label);std::exit(1); }
}
void ok(std::int32_t hr,const char* label) {
    if (hr<0) { std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,static_cast<unsigned long>(hr));std::exit(1); }
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r) {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=b;bytes[19]=g;bytes[20]=r;return bytes;
}
struct Host {
    std::vector<std::uint8_t> model=native11_fixture::model("authored.tga");
    std::vector<std::uint8_t> red=tga(0,0,255),green=tga(0,255,0);
    A2FO_N11_API6* api=nullptr;A2FO_N11_Session session=0;
    unsigned reads=0,model_reads=0,texture_reads=0,texture_calls=0,part_calls=0;
    bool try_reentry=false,reentry_refused=false;
};
std::int32_t A2FO_N11_CALL asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context) {
    auto& h=*static_cast<Host*>(raw);++h.reads;
    const std::string name(reinterpret_cast<const char*>(id),length);
    const std::vector<std::uint8_t>* bytes=nullptr;
    if (kind==1) { ++h.model_reads;bytes=name=="red-view"||name=="green-view"?&h.model:nullptr; }
    if (kind==2) { ++h.texture_reads;bytes=name=="red.tga"?&h.red:name=="green.tga"?&h.green:nullptr; }
    if (!bytes||bytes->size()>limit) return 0;
    return sink(context,bytes->data(),static_cast<std::uint32_t>(bytes->size()));
}
std::int32_t A2FO_N11_CALL texture(void* raw,const std::uint8_t* model,
    std::uint32_t length,std::uint32_t part,const std::uint8_t* authored,
    std::uint32_t authored_length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context) {
    auto& h=*static_cast<Host*>(raw);++h.texture_calls;
    const std::string view(reinterpret_cast<const char*>(model),length);
    need(part==0&&std::string(reinterpret_cast<const char*>(authored),authored_length)=="authored.tga",
         "texture callback authored part");
    if (h.try_reentry) {
        h.try_reentry=false;
        A2FO_N11_MeshSharing same{sizeof(same),1,{0,0}};
        h.reentry_refused=h.api->configure_mesh_sharing(h.session,&same)==E_UNEXPECTED;
    }
    const char* selected=view=="red-view"?"red.tga":view=="green-view"?"green.tga":nullptr;
    if (!selected||std::strlen(selected)>limit) return 0;
    return sink(context,reinterpret_cast<const std::uint8_t*>(selected),
        static_cast<std::uint32_t>(std::strlen(selected)));
}
std::int32_t A2FO_N11_CALL texture_alt(void* raw,const std::uint8_t* model,
    std::uint32_t length,std::uint32_t part,const std::uint8_t* authored,
    std::uint32_t authored_length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context) {
    return texture(raw,model,length,part,authored,authored_length,limit,sink,context);
}
std::int32_t A2FO_N11_CALL part_state(void* raw,const std::uint8_t* model,
    std::uint32_t length,std::uint32_t part,const float authored[16],
    A2FO_N11_PartStateSink sink,void* context) {
    auto& h=*static_cast<Host*>(raw);++h.part_calls;
    const std::string view(reinterpret_cast<const char*>(model),length);
    need(part==0&&(view=="red-view"||view=="green-view"),"part callback per view");
    A2FO_N11_PartState state{};state.struct_size=sizeof(state);state.visible=1;
    std::memcpy(state.local_to_model,authored,sizeof(state.local_to_model));
    return sink(context,&state);
}
std::int32_t A2FO_N11_CALL part_state_alt(void* raw,const std::uint8_t* model,
    std::uint32_t length,std::uint32_t part,const float authored[16],
    A2FO_N11_PartStateSink sink,void* context) {
    return part_state(raw,model,length,part,authored,sink,context);
}
struct Scene {
    std::array<A2FO_N11_Object,2> objects{};
    A2FO_N11_Frame frame{};
    const std::array<std::string,2> names{{"red-view","green-view"}};
    Scene() {
        for (std::size_t i=0;i<objects.size();++i) {
            auto& object=objects[i];
            for (int j=0;j<16;++j)object.local_to_world[j]=(j%5)==0?1.f:0.f;
            object.local_to_world[0]=object.local_to_world[5]=object.local_to_world[10]=.5f;
            object.local_to_world[3]=i?.5f:-.5f;
            object.model_identifier=reinterpret_cast<const std::uint8_t*>(names[i].data());
            object.model_identifier_length=static_cast<std::uint32_t>(names[i].size());
        }
        frame.struct_size=sizeof(frame);frame.flags=A2FO_N11_FRAME_REQUIRE_PART_STATES;
        frame.generation=frame.source_epoch=1;
        for (int j=0;j<16;++j)frame.world_to_clip[j]=(j%5)==0?1.f:0.f;
        frame.viewport[2]=frame.viewport[3]=128;
        frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
        frame.object_count=2;frame.objects=objects.data();
    }
};
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);
    need(api.readback(session,&read)==S_FALSE&&read.width==128&&read.height==128,
         "readback metadata");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(read.required_bytes));
    read.bgra=bytes.data();read.capacity=bytes.size();ok(api.readback(session,&read),"readback pixels");
    return bytes;
}
void colour(const std::vector<std::uint8_t>& bytes,unsigned x,int r,int g,int b,
    const char* label) {
    const auto* p=bytes.data()+(64*128+x)*4;
    need(std::abs(int(p[2])-r)<5&&std::abs(int(p[1])-g)<5&&
         std::abs(int(p[0])-b)<5,label);
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Usage result{};result.struct_size=sizeof(result);
    ok(api.usage(session,&result),"GPU usage");return result;
}
struct Foreign {A2FO_N11_API6* api;A2FO_N11_Session session;std::int32_t result=0;};
DWORD WINAPI foreign_config(void* raw) {
    auto& f=*static_cast<Foreign*>(raw);A2FO_N11_MeshSharing enabled{sizeof(enabled),1,{0,0}};
    f.result=f.api->configure_mesh_sharing(f.session,&enabled);return 0;
}
void run(const char* path) {
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"helper DLL load");
    const FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");
    need(symbol!=nullptr,"public interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API a1{};A2FO_N11_API2 a2{};A2FO_N11_API3 a3{};
    A2FO_N11_API4 a4{};A2FO_N11_API5 a5{};A2FO_N11_API6 a6{};
    ok(get(1,sizeof(a1),&a1),"API1 query");ok(get(2,sizeof(a2),&a2),"API2 query");
    ok(get(3,sizeof(a3),&a3),"API3 query");ok(get(4,sizeof(a4),&a4),"API4 query");
    ok(get(5,sizeof(a5),&a5),"API5 query");ok(native11_c_query6(get,&a6),"C11 API6 query");
    auto& api=a6.base.base.base.base.base;
    need(a1.version==1&&a2.base.version==2&&a3.base.base.version==3&&
         a4.base.base.base.version==4&&a5.base.base.base.base.version==5&&
         a1.struct_size==sizeof(a1)&&a2.base.struct_size==sizeof(a2)&&
         a3.base.base.struct_size==sizeof(a3)&&
         a4.base.base.base.struct_size==sizeof(a4)&&
         a5.base.base.base.base.struct_size==60&&api.version==6&&api.struct_size==64&&
         (api.capabilities&A2FO_N11_CAP_MESH_SHARING)&&a6.configure_mesh_sharing&&
         !(a1.capabilities&A2FO_N11_CAP_MESH_SHARING)&&
         !(a2.base.capabilities&A2FO_N11_CAP_MESH_SHARING)&&
         !(a3.base.base.capabilities&A2FO_N11_CAP_MESH_SHARING)&&
         !(a4.base.base.base.capabilities&A2FO_N11_CAP_MESH_SHARING)&&
         !(a5.base.base.base.base.capabilities&A2FO_N11_CAP_MESH_SHARING)&&
         a1.submit==a2.base.submit&&a1.submit==a3.base.base.submit&&
         a1.submit==a4.base.base.base.submit&&a1.submit==a5.base.base.base.base.submit,
         "API1-5 prefix metadata and API6 extension");
    A2FO_N11_API6 malformed{};std::memset(&malformed,0x5a,sizeof(malformed));
    const auto before=malformed;
    need(get(6,63,&malformed)==E_INVALIDARG&&
         std::memcmp(&malformed,&before,sizeof(before))==0,"malformed API6 query untouched");
    Host host;host.api=&a6;A2FO_N11_Host request{sizeof(request),0,128,128,&host,asset,nullptr};
    ok(api.create(&request,&host.session),"offscreen session");Scene scene;
    A2FO_N11_Shadows endpoint{sizeof(endpoint),0,512,.1f};
    ok(a6.base.base.base.base.configure_shadows(host.session,&endpoint),"API accepts maximum stored binary32 shadow bias");
    endpoint.depth_bias=std::nextafter(.1f,std::numeric_limits<float>::infinity());
    need(a6.base.base.base.base.configure_shadows(host.session,&endpoint)==E_INVALIDARG,
         "API rejects first binary32 value above maximum bias");
    endpoint.depth_bias=.001f;
    ok(a6.base.base.base.base.configure_shadows(host.session,&endpoint),"restore default shadow bias");
    A2FO_N11_TextureBindings textures{sizeof(textures),0,&host,texture};
    ok(a6.base.base.base.configure_texture_bindings(host.session,&textures),"select per-view textures");
    A2FO_N11_PartStates states{sizeof(states),0,&host,part_state};
    ok(a6.base.base.configure_part_states(host.session,&states),"observe per-view part states");
    Foreign foreign{&a6,host.session};
    HANDLE thread=CreateThread(nullptr,0,foreign_config,&foreign,0,nullptr);
    need(thread!=nullptr&&WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,
         "foreign policy call completed");CloseHandle(thread);
    need(foreign.result==E_UNEXPECTED,"foreign thread mesh policy refused");
    ok(api.submit(host.session,&scene.frame),"default separate geometry");
    auto image=pixels(api,host.session);
    colour(image,32,255,0,0,"default red view");colour(image,96,0,255,0,"default green view");
    need(usage(api,host.session).meshes==2,"default policy uploads two meshes");
    A2FO_N11_MeshSharing sharing{sizeof(sharing),1,{0,0}};
    host.try_reentry=true;ok(a6.configure_mesh_sharing(host.session,&sharing),"enable shared geometry");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"shared geometry frame");
    need(host.reentry_refused,"mesh policy reentry refused from texture callback");
    image=pixels(api,host.session);colour(image,32,255,0,0,"shared red view");
    colour(image,96,0,255,0,"shared green view");
    need(usage(api,host.session).meshes==1,"opt-in policy uploads one mesh");
    const auto hot_reads=host.reads,hot_parts=host.part_calls;
    const auto hot_image=image;
    ok(a6.configure_mesh_sharing(host.session,&sharing),"identical policy no-op");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"hot shared frame");
    need(host.reads==hot_reads&&host.part_calls==hot_parts+2&&
         pixels(api,host.session)==hot_image,"identical config keeps cache, per-view pose callbacks continue");
    A2FO_N11_MeshSharing invalid=sharing;invalid.enabled=2;
    need(a6.configure_mesh_sharing(host.session,&invalid)==E_INVALIDARG,"invalid enabled value");
    invalid=sharing;invalid.reserved[0]=1;
    need(a6.configure_mesh_sharing(host.session,&invalid)==E_INVALIDARG,"reserved field rejected");
    invalid=sharing;--invalid.struct_size;
    need(a6.configure_mesh_sharing(host.session,&invalid)==E_INVALIDARG,"short config rejected");
    need(host.reads==hot_reads&&pixels(api,host.session)==hot_image,
         "invalid mesh policy leaves current target and cache intact");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"hot after invalid configs");
    need(host.reads==hot_reads&&usage(api,host.session).meshes==1&&
         pixels(api,host.session)==hot_image,"invalid policy preserves image and warm cache");
    A2FO_N11_Materials materials{sizeof(materials),1,.02f,0};
    ok(a6.base.base.base.base.configure_materials(host.session,&materials),"change material policy");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"after material policy");
    need(usage(api,host.session).meshes==1&&pixels(api,host.session)==hot_image,
         "material policy rebuild preserves shared geometry and pixels");
    A2FO_N11_TextureBindings textures_alt{sizeof(textures_alt),0,&host,texture_alt};
    ok(a6.base.base.base.configure_texture_bindings(host.session,&textures_alt),"change texture binding policy");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"after texture policy");
    need(usage(api,host.session).meshes==1&&pixels(api,host.session)==hot_image,
         "texture binding rebuild preserves shared geometry and pixels");
    A2FO_N11_PartStates states_alt{sizeof(states_alt),0,&host,part_state_alt};
    ok(a6.base.base.configure_part_states(host.session,&states_alt),"change part state policy");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"after part state policy");
    need(usage(api,host.session).meshes==1&&pixels(api,host.session)==hot_image,
         "part state rebuild preserves shared geometry and pixels");
    A2FO_N11_MeshSharing separate{sizeof(separate),0,{0,0}};
    ok(a6.configure_mesh_sharing(host.session,&separate),"disable mesh sharing");
    ++scene.frame.generation;ok(api.submit(host.session,&scene.frame),"separate geometry again");
    need(usage(api,host.session).meshes==2&&pixels(api,host.session)==hot_image,
         "toggle off restores two meshes and same independent pixels");
    ok(api.destroy(host.session),"session destroy");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
         !GetModuleHandleA("D3DX81ab.dll")&&!GetModuleHandleA("d3dx9_43.dll"),
         "no legacy graphics modules");
    need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv) {
    need(argc==2,"usage: native11_geometry_api_native.exe RENDERER_DLL");
    run(argv[1]);std::puts("PASS native11 public API6 mesh sharing");
}

// Public SDK client for live per-part pose/visibility in the real native11 DLL.
#include "../sdk/include/a2fo_native11_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
extern "C" int32_t native11_c_query(A2FO_N11_GetInterface,A2FO_N11_API*);
extern "C" int32_t native11_c_query2(A2FO_N11_GetInterface,A2FO_N11_API2*);
extern "C" int32_t native11_c_query3(A2FO_N11_GetInterface,A2FO_N11_API3*);
extern "C" int32_t native11_c_query4(A2FO_N11_GetInterface,A2FO_N11_API4*);
static_assert(sizeof(A2FO_N11_API4)==56,"i686 API4 layout");
namespace {
void need(bool v,const char* why){if(!v){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
std::vector<uint8_t> red_tga(){std::vector<uint8_t> b(22);b[2]=2;b[12]=1;b[14]=1;b[16]=24;b[17]=0x20;b[20]=255;return b;}
// The fixture's sole mesh matrix follows the "mesh" and empty parent strings.
// Locate its exact encoded identity rather than depending on an undocumented offset.
std::vector<uint8_t> authored_model(const char* texture){auto b=native11_fixture::model(texture);
    const uint8_t marker[]{4,0,'m','e','s','h',0,0};
    auto p=std::search(b.begin(),b.end(),std::begin(marker),std::end(marker));
    need(p!=b.end(),"fixture mesh header found");auto offset=static_cast<size_t>(p-b.begin())+sizeof(marker);
    need(offset+48<=b.size(),"fixture matrix in bounds");float x=.25f;
    std::memcpy(b.data()+offset+9*4,&x,sizeof(x));return b;}
struct Host {std::vector<uint8_t> mesh=authored_model("red.tga"),white=authored_model("");
    std::vector<uint8_t> red=red_tga();A2FO_N11_API4* api=nullptr;A2FO_N11_Session session=0;
    unsigned reads=0,calls=0,textured=0,untextured=0;bool block=false,poison=false,reentry=false,reentry_refused=false;
    unsigned bad=0;float shift=0;bool visible=true;};
int32_t A2FO_N11_CALL asset(void* raw,uint32_t kind,const uint8_t* id,uint32_t n,uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);need(!h.block,"hot callback cannot read assets");++h.reads;
    std::string name(reinterpret_cast<const char*>(id),n);const auto* b=kind==1?(name=="textured-view"?&h.mesh:name=="white-view"?&h.white:nullptr):
        kind==2&&name=="red.tga"?&h.red:nullptr;
    if(!b||b->size()>limit)return 0;
    return sink(context,b->data(),static_cast<uint32_t>(b->size()));}
int32_t A2FO_N11_CALL resolve(void* raw,const uint8_t* id,uint32_t n,uint32_t ordinal,const float authored[16],A2FO_N11_PartStateSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.calls;need(ordinal==0,"stable emitted ordinal");
    const std::string name(reinterpret_cast<const char*>(id),n);need(name=="textured-view"||name=="white-view","exact model identity");
    if(name=="textured-view")++h.textured;else ++h.untextured;
    need(authored[0]==1&&authored[5]==1&&authored[10]==1&&authored[15]==1&&authored[3]==.25f,
         "callback receives pristine authored matrix");
    if(h.reentry){h.reentry=false;A2FO_N11_PartStates same{sizeof(same),0,&h,resolve};A2FO_N11_Readback rb{};rb.struct_size=sizeof(rb);
        h.reentry_refused=h.api->configure_part_states(h.session,&same)==E_UNEXPECTED&&
            h.api->base.base.base.readback(h.session,&rb)==E_UNEXPECTED&&
            h.api->base.base.base.destroy(h.session)==E_UNEXPECTED;}
    A2FO_N11_PartState state{};state.struct_size=sizeof(state);state.visible=h.visible?1:0;
    std::memcpy(state.local_to_model,authored,sizeof(state.local_to_model));state.local_to_model[3]=h.shift;
    if(h.bad==1)return 1;
    if(h.bad==2){need(sink(context,&state)==1,"first sink accepted");need(sink(context,&state)==0,"second sink refused");return 1;}
    if(h.bad==3)state.visible=2;
    if(h.bad==4)state.reserved=1;
    if(h.bad==5)--state.struct_size;
    if(h.bad==6)state.local_to_model[3]=std::numeric_limits<float>::quiet_NaN();
    if(h.bad==7)state.local_to_model[15]=2;
    if(h.bad==8)state.local_to_model[0]=0;
    if(h.bad==9){state.visible=0;state.local_to_model[0]=0;}
    const auto result=sink(context,&state);
    if(h.poison){auto* borrowed=const_cast<float*>(authored);borrowed[3]=99.f;}
    return h.bad==10?0:result;
}
int32_t A2FO_N11_CALL texture_resolve(void*,const uint8_t* id,uint32_t n,uint32_t part,
    const uint8_t* authored,uint32_t authored_n,uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    need(std::string(reinterpret_cast<const char*>(id),n)=="textured-view"&&part==0&&limit>=7,
         "textured part selected");
    need(std::string(reinterpret_cast<const char*>(authored),authored_n)=="red.tga","authored diffuse passed");
    const char selected[]="red.tga";return sink(context,reinterpret_cast<const uint8_t*>(selected),7);
}
struct Frame {std::array<A2FO_N11_Object,2> objects{};A2FO_N11_Frame frame{};
    std::array<std::string,2> names{{"textured-view","white-view"}};
    Frame(){for(unsigned i=0;i<2;++i){auto& o=objects[i];for(unsigned j=0;j<4;++j)o.local_to_world[j*5]=1;
        o.local_to_world[0]=o.local_to_world[5]=o.local_to_world[10]=.5f;o.local_to_world[3]=i?.5f:-.5f;
        o.model_identifier=reinterpret_cast<const uint8_t*>(names[i].data());o.model_identifier_length=static_cast<uint32_t>(names[i].size());}
        frame.struct_size=sizeof(frame);frame.flags=A2FO_N11_FRAME_REQUIRE_PART_STATES;frame.generation=frame.source_epoch=1;
        for(unsigned i=0;i<4;++i)frame.world_to_clip[i*5]=1;
        frame.viewport[2]=frame.viewport[3]=128;
        frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;frame.object_count=2;frame.objects=objects.data();}
};
std::vector<uint8_t> pixels(A2FO_N11_API& a,A2FO_N11_Session s){A2FO_N11_Readback r{};r.struct_size=sizeof(r);
    need(a.readback(s,&r)==S_FALSE&&r.width==128&&r.height==128,"readback metadata");
    std::vector<uint8_t> b(static_cast<size_t>(r.required_bytes));r.bgra=b.data();r.capacity=b.size();ok(a.readback(s,&r),"pixels");return b;}
void colour(const std::vector<uint8_t>& b,unsigned x,unsigned y,int r,int g,int blue,const char* why){
    const auto* p=b.data()+(y*128+x)*4;need(std::abs(int(p[2])-r)<5&&std::abs(int(p[1])-g)<5&&std::abs(int(p[0])-blue)<5,why);}
A2FO_N11_Usage usage(A2FO_N11_API& a,A2FO_N11_Session s){A2FO_N11_Usage u{};u.struct_size=sizeof(u);ok(a.usage(s,&u),"usage");return u;}
bool same(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){return a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;}
struct Foreign {Host* host;int32_t result=0;};
DWORD WINAPI foreign_config(void* raw){auto& f=*static_cast<Foreign*>(raw);A2FO_N11_PartStates c{sizeof(c),0,f.host,resolve};f.result=f.host->api->configure_part_states(f.host->session,&c);return 0;}
void no_legacy(){HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());need(snapshot!=INVALID_HANDLE_VALUE,"module snapshot");
    MODULEENTRY32W e{};e.dwSize=sizeof(e);need(Module32FirstW(snapshot,&e)!=0,"module first");do{
        need(_wcsicmp(e.szModule,L"d3d8.dll")&&_wcsicmp(e.szModule,L"d3d9.dll")&&_wcsnicmp(e.szModule,L"d3dx",4),"no legacy D3D modules");
    }while(Module32NextW(snapshot,&e));auto last=GetLastError();CloseHandle(snapshot);need(last==ERROR_NO_MORE_FILES,"complete module enumeration");}
void run(const char* path){HMODULE dll=LoadLibraryA(path);need(dll,"DLL load");auto symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));A2FO_N11_API a{};A2FO_N11_API2 a2{};A2FO_N11_API3 a3{};A2FO_N11_API4 a4{};
    ok(native11_c_query(get,&a),"v1 C query");ok(native11_c_query2(get,&a2),"v2 C query");ok(native11_c_query3(get,&a3),"v3 C query");ok(native11_c_query4(get,&a4),"v4 C query");
    need(a.version==1&&a2.base.version==2&&a3.base.base.version==3&&a4.base.base.base.version==4&&
         a4.base.base.base.struct_size==sizeof(a4)&&(a4.base.base.base.capabilities&A2FO_N11_CAP_PART_STATES)&&
         a.submit==a4.base.base.base.submit&&a4.configure_part_states,"compatible API prefixes");
    A2FO_N11_API4 untouched;std::memset(&untouched,0x5a,sizeof(untouched));auto before=untouched;
    need(get(4,sizeof(untouched)-1,&untouched)==E_INVALIDARG&&std::memcmp(&untouched,&before,sizeof(before))==0,"malformed v4 query leaves output");
    Host host;host.api=&a4;A2FO_N11_Host config{sizeof(config),0,128,128,&host,asset,nullptr};ok(a.create(&config,&host.session),"create");Frame f;
    need(a.submit(host.session,&f.frame)==E_INVALIDARG&&host.reads==0,"required state without resolver fails before reads");
    Foreign foreign{&host};HANDLE thread=CreateThread(nullptr,0,foreign_config,&foreign,0,nullptr);need(thread,"foreign worker");
    need(WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,"foreign worker joined");CloseHandle(thread);need(foreign.result==E_UNEXPECTED,"foreign config refused");
    A2FO_N11_PartStates config_state{sizeof(config_state),0,&host,resolve};ok(a4.configure_part_states(host.session,&config_state),"configure pose");
    host.reentry=true;host.poison=true;ok(a.submit(host.session,&f.frame),"first pose");need(host.reentry_refused,"reentry refused");
    auto image=pixels(a,host.session);colour(image,32,64,255,0,0,"replacement pose at left");colour(image,96,64,255,255,255,"white part visible");
    need(host.calls==2&&host.textured==1&&host.untextured==1,"callback every textured and white part");auto resident=usage(a,host.session);
    need(resident.meshes==2&&resident.textures==2,"both models resident with red and implicit white");
    // Object X scale is 0.5: +0.75 local moves each centre 24 pixels.
    // At y=64 the triangle's old and new footprints do not overlap.
    host.block=true;host.poison=false;host.shift=.75f;auto reads=host.reads;ok(a.submit(host.session,&f.frame),"same epoch moved pose");
    need(host.calls==4&&host.reads==reads,"every submission callback without rereads");image=pixels(a,host.session);
    colour(image,32,64,1,2,5,"old pose footprint cleared");colour(image,56,64,255,0,0,"replacement moves textured part");
    colour(image,120,64,255,255,255,"replacement moves white part");
    need(same(resident,usage(a,host.session)),"pose preserves GPU residency");
    host.visible=false;ok(a.submit(host.session,&f.frame),"hidden pose");image=pixels(a,host.session);
    colour(image,56,64,1,2,5,"hidden textured part clears");colour(image,120,64,1,2,5,"hidden white part clears");
    host.visible=true;host.shift=0;ok(a.submit(host.session,&f.frame),"visible pose recovery");image=pixels(a,host.session);
    for(unsigned mode=1;mode<=8;++mode){host.bad=mode;need(a.submit(host.session,&f.frame)==E_INVALIDARG,"invalid state refused");
        need(pixels(a,host.session)==image,"invalid state keeps previous target");}
    host.bad=9;ok(a.submit(host.session,&f.frame),"hidden singular affine allowed");auto hidden=pixels(a,host.session);
    colour(hidden,32,64,1,2,5,"hidden singular part absent");
    host.bad=10;need(a.submit(host.session,&f.frame)==E_INVALIDARG,"return zero after sink refused");need(pixels(a,host.session)==hidden,"post sink failure preserves target");
    host.bad=0;ok(a.submit(host.session,&f.frame),"same epoch failure recovery");image=pixels(a,host.session);colour(image,32,64,255,0,0,"failed state not cached");
    need(same(resident,usage(a,host.session)),"failure and recovery preserve residency");
    A2FO_N11_PartStates invalid{sizeof(invalid),1,&host,resolve};need(a4.configure_part_states(host.session,&invalid)==E_INVALIDARG,"invalid config refused");
    ok(a4.configure_part_states(host.session,&config_state),"identical config no op");reads=host.reads;ok(a.submit(host.session,&f.frame),"hot after invalid/identical config");need(host.reads==reads,"config leaves hot cache");
    A2FO_N11_Materials materials{sizeof(materials),1,.02f,0};ok(a2.configure_materials(host.session,&materials),"changed materials");
    host.block=false;ok(a.submit(host.session,&f.frame),"pose survives material policy");need(host.calls>=2,"pose callback retained");
    A2FO_N11_TextureBindings texture{sizeof(texture),0,&host,texture_resolve};
    ok(a3.configure_texture_bindings(host.session,&texture),"changed texture policy");
    ok(a.submit(host.session,&f.frame),"pose survives changed texture policy");
    colour(pixels(a,host.session),32,64,255,0,0,"pose remains after texture change");
    A2FO_N11_PartStates reset{sizeof(reset),0,nullptr,nullptr};ok(a4.configure_part_states(host.session,&reset),"reset authored pose");
    reads=host.reads;need(a.submit(host.session,&f.frame)==E_INVALIDARG&&host.reads==reads,"required state after reset fails before reads");
    f.frame.flags=0;ok(a.submit(host.session,&f.frame),"optional authored fallback");image=pixels(a,host.session);
    colour(image,40,64,255,0,0,"authored transform after reset");
    ok(a.destroy(host.session),"destroy");no_legacy();need(FreeLibrary(dll)!=0,"DLL unload");}
}
int main(int argc,char** argv){need(argc==2,"expected DLL path");run(argv[1]);std::puts("PASS native11 v4 part state DLL client");}

// Strict public-SDK DLL fixture for version-3 diffuse texture selection.
#include "../sdk/include/a2fo_native11_api.h"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" int32_t native11_c_query(A2FO_N11_GetInterface,A2FO_N11_API*);
extern "C" int32_t native11_c_query2(A2FO_N11_GetInterface,A2FO_N11_API2*);
extern "C" int32_t native11_c_query3(A2FO_N11_GetInterface,A2FO_N11_API3*);
static_assert(sizeof(A2FO_N11_API3)==52,"i686 API3 layout");
static_assert(offsetof(A2FO_N11_API3,configure_texture_bindings)==48,"i686 API3 extension offset");

namespace {
void need(bool value,const char* what){if(!value){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
void ok(int32_t hr,const char* what){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",what,static_cast<unsigned long>(hr));std::exit(1);}}
std::vector<uint8_t> tga(uint8_t b,uint8_t g,uint8_t r){
    std::vector<uint8_t> v(22);v[2]=2;v[12]=1;v[14]=1;v[16]=24;v[17]=0x20;
    v[18]=b;v[19]=g;v[20]=r;return v;
}
struct Host {
    std::vector<uint8_t> sod=native11_fixture::model("hull.tga"),red=tga(0,0,255),green=tga(0,255,0),authored=tga(255,0,0);
    A2FO_N11_API3* api=nullptr; A2FO_N11_Session session=0;
    uint32_t bad=0,reads=0,textures=0,authored_reads=0,resolves=0;
    bool reenter_resolver=false,resolver_reentry_refused=false,flip=false;
    std::string copied_id;
};
int32_t A2FO_N11_CALL asset(void* raw,uint32_t kind,const uint8_t* id,uint32_t length,uint32_t limit,
                            A2FO_N11_BytesSink sink,void* sink_context){
    auto& h=*static_cast<Host*>(raw);++h.reads;
    if(kind==1){if(std::string(reinterpret_cast<const char*>(id),length)!="left-view"&&
                    std::string(reinterpret_cast<const char*>(id),length)!="right-view")return 0;
        return sink(sink_context,h.sod.data(),static_cast<uint32_t>(h.sod.size()));}
    if(kind!=2)return 0;
    ++h.textures;
    const std::string name(reinterpret_cast<const char*>(id),length);
    if(name=="hull.tga")++h.authored_reads;
    const auto& bytes=name=="red-image"?h.red:name=="green-image"?h.green:h.authored;
    if(name!="red-image"&&name!="green-image"&&name!="hull.tga")return 0;
    if(bytes.size()>limit)return 0;
    return sink(sink_context,bytes.data(),static_cast<uint32_t>(bytes.size()));
}
int32_t A2FO_N11_CALL resolve(void* raw,const uint8_t* model,uint32_t model_n,uint32_t part,
                              const uint8_t* authored,uint32_t authored_n,uint32_t max_id,
                              A2FO_N11_BytesSink sink,void* sink_context){
    auto& h=*static_cast<Host*>(raw);++h.resolves;
    if(h.reenter_resolver){h.reenter_resolver=false;A2FO_N11_TextureBindings nested{sizeof(nested),0,&h,nullptr};
        A2FO_N11_Usage usage{};usage.struct_size=sizeof(usage);A2FO_N11_Readback rb{};rb.struct_size=sizeof(rb);
        h.resolver_reentry_refused=h.api->configure_texture_bindings(h.session,&nested)==E_UNEXPECTED&&
            h.api->base.base.usage(h.session,&usage)==E_UNEXPECTED&&
            h.api->base.base.readback(h.session,&rb)==E_UNEXPECTED&&
            h.api->base.base.destroy(h.session)==E_UNEXPECTED;}
    if(part!=0||max_id!=1024)return 0;
    need(std::string(reinterpret_cast<const char*>(authored),authored_n)=="hull.tga","resolver receives exact authored texture ID");
    std::string view(reinterpret_cast<const char*>(model),model_n);
    if(h.bad==1)return 1; // no sink
    if(h.bad==2){sink(sink_context,reinterpret_cast<const uint8_t*>("red-image"),9);
        need(sink(sink_context,reinterpret_cast<const uint8_t*>("red-image"),9)==0,"second binding sink refused");return 1;}
    if(h.bad==3){const uint8_t dummy=0;need(sink(sink_context,&dummy,max_id+1)==0,"oversized binding refused before dereference");return 1;}
    if(h.bad==4){const uint8_t bad[]={'x',0,'y'};return sink(sink_context,bad,sizeof(bad));}
    if(h.bad==5){const uint8_t bad[]={0x7f};return sink(sink_context,bad,sizeof(bad));}
    if(h.bad==6)return sink(sink_context,nullptr,0);
    const char* selected=(view=="left-view")!=h.flip?"red-image":"green-image";
    h.copied_id.assign(selected); // callback's temporary storage is not retained
    char temporary[16]{};std::strcpy(temporary,selected);
    const int32_t result=sink(sink_context,reinterpret_cast<const uint8_t*>(temporary),
                              static_cast<uint32_t>(std::strlen(temporary)));
    std::memset(temporary,'x',sizeof(temporary));
    return h.bad==7?0:result;
}
struct FrameData {
    std::array<A2FO_N11_Object,2> objects{}; A2FO_N11_Frame frame{};
    std::array<std::string,2> names{{"left-view","right-view"}};
    FrameData(){
        for(unsigned i=0;i<2;++i){auto& o=objects[i];for(unsigned k=0;k<4;++k)o.local_to_world[k*5]=1;
            o.local_to_world[0]=o.local_to_world[5]=o.local_to_world[10]=.5f;
            o.local_to_world[3]=i? .5f:-.5f;o.model_identifier=reinterpret_cast<const uint8_t*>(names[i].data());
            o.model_identifier_length=static_cast<uint32_t>(names[i].size());}
        frame.struct_size=sizeof(frame);frame.generation=1;frame.source_epoch=1;
        for(unsigned k=0;k<4;++k)frame.world_to_clip[k*5]=1;
        frame.viewport[2]=frame.viewport[3]=128;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
        frame.object_count=2;frame.objects=objects.data();
    }
};
struct ForeignCall { A2FO_N11_API3* api; A2FO_N11_Session session; Host* host; int32_t result=0; };
DWORD WINAPI foreign_configure(void* raw){auto& c=*static_cast<ForeignCall*>(raw);
    A2FO_N11_TextureBindings b{sizeof(b),0,c.host,resolve};c.result=c.api->configure_texture_bindings(c.session,&b);return 0;}
std::vector<uint8_t> pixels(A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback r{};r.struct_size=sizeof(r);need(api.readback(session,&r)==S_FALSE,"readback metadata");
    std::vector<uint8_t> v(static_cast<size_t>(r.required_bytes));r.bgra=v.data();r.capacity=v.size();
    ok(api.readback(session,&r),"readback pixels");return v;
}
void pixel(const std::vector<uint8_t>& p,unsigned x,unsigned y,int r,int g,int b,const char* why){
    auto* q=p.data()+(y*128+x)*4;need(std::abs(static_cast<int>(q[2])-r)<5&&std::abs(static_cast<int>(q[1])-g)<5&&std::abs(static_cast<int>(q[0])-b)<5,why);
}
void no_legacy_modules(){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    need(snapshot!=INVALID_HANDLE_VALUE,"enumerate loaded modules");MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
    need(Module32FirstW(snapshot,&entry)!=0,"read first loaded module");
    do {const wchar_t* name=entry.szModule;
        need(_wcsicmp(name,L"d3d8.dll")&&_wcsicmp(name,L"d3d9.dll")&&_wcsnicmp(name,L"d3dx",4),
             "no D3D8/9 or D3DX module loaded");
    } while(Module32NextW(snapshot,&entry));
    const auto error=GetLastError();
    CloseHandle(snapshot);
    need(error==ERROR_NO_MORE_FILES,"complete module enumeration");
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll,"load native11 helper");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"get interface export");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API api{};A2FO_N11_API2 api2{};A2FO_N11_API3 api3{};
    ok(native11_c_query(get,&api),"C query v1");ok(native11_c_query2(get,&api2),"C query v2");
    ok(native11_c_query3(get,&api3),"C query v3");need(api3.base.base.version==3&&api3.base.base.struct_size==sizeof(api3)&&
        (api3.base.base.capabilities&A2FO_N11_CAP_TEXTURE_BINDINGS),"version-3 capability and 52-byte table");
    need(api.version==1&&api2.base.version==2&&api.create==api2.base.create&&
         api2.base.create==api3.base.base.create,"version-1/2 compatible API prefixes");
    A2FO_N11_API3 untouched;std::memset(&untouched,0x5a,sizeof(untouched));auto before=untouched;
    need(get(3,sizeof(untouched)-1,&untouched)==E_INVALIDARG&&std::memcmp(&untouched,&before,sizeof(before))==0,
         "malformed v3 query preserves output");
    Host host;host.api=&api3;A2FO_N11_Host hc{sizeof(hc),0,128,128,&host,asset,nullptr};
    ok(api.create(&hc,&host.session),"create DLL session");
    ForeignCall foreign{&api3,host.session,&host};HANDLE thread=CreateThread(nullptr,0,foreign_configure,&foreign,0,nullptr);
    need(thread!=nullptr,"foreign configure worker created");
    need(WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,"foreign configure worker completed");CloseHandle(thread);
    need(foreign.result==E_UNEXPECTED,"foreign-thread texture configuration refused");
    A2FO_N11_Materials materials{sizeof(materials),1,.02f,0};ok(api2.configure_materials(host.session,&materials),"enable retained relight policy");
    FrameData data;
    ok(api.submit(host.session,&data.frame),"authored texture baseline");
    auto image=pixels(api,host.session);pixel(image,32,64,0,0,255,"authored hull.tga baseline");
    A2FO_N11_Shadows shadows{sizeof(shadows),1,128,.001f};ok(api2.configure_shadows(host.session,&shadows),"enable retained shadow mask");
    A2FO_N11_Light lamp{};lamp.type=1;lamp.direction[2]=1;lamp.colour[0]=lamp.colour[1]=lamp.colour[2]=1;lamp.intensity=1;
    data.frame.light_count=1;data.frame.lights=&lamp;
    A2FO_N11_TextureBindings binding{sizeof(binding),0,&host,resolve};
    ok(api3.configure_texture_bindings(host.session,&binding),"configure selected textures");
    host.reads=host.textures=host.authored_reads=host.resolves=0;host.reenter_resolver=true;
    ok(api.submit(host.session,&data.frame),"submit left and right texture views");
    need(host.resolver_reentry_refused,"resolver reentrant configure/readback/destroy refused");
    image=pixels(api,host.session);pixel(image,32,64,255,0,0,"left view selects red-image");
    pixel(image,96,64,0,255,0,"right view selects green-image");
    need(host.resolves==2&&host.reads==4&&host.copied_id=="green-image","one binding per textured part and exact texture fetch");
    host.bad=0;host.reads=0;host.resolves=0;
    // All callback contract failures reject the complete frame, retaining the last image.
    for(uint32_t mode=1;mode<=7;++mode){host.bad=mode;++data.frame.source_epoch;const auto old_authored_reads=host.authored_reads;
        need(api.submit(host.session,&data.frame)<0,"invalid resolver result rejects full frame");
        need(pixels(api,host.session)==image,"resolver failure preserves previous pixels");
        need(host.authored_reads==old_authored_reads,"failed binding never falls back to authored texture fetch");}
    host.bad=0;++data.frame.source_epoch;ok(api.submit(host.session,&data.frame),"new epoch recovers binding");
    image=pixels(api,host.session);pixel(image,32,64,255,0,0,"recovered left pixel");pixel(image,96,64,0,255,0,"recovered right pixel");
    const auto hot=host.resolves,hot_reads=host.reads,hot_textures=host.textures;
    A2FO_N11_TextureBindings invalid{sizeof(invalid),1,&host,resolve};
    need(api3.configure_texture_bindings(host.session,&invalid)==E_INVALIDARG,"invalid configuration rejected");
    host.bad=0;ok(api.submit(host.session,&data.frame),"invalid configuration preserves hot cache");
    need(host.resolves==hot&&host.reads==hot_reads&&host.textures==hot_textures,
         "hot cache did not repoll after invalid configuration");
    A2FO_N11_Materials same_materials{sizeof(same_materials),1,.02f,0};
    same_materials.retained_emission=.03f;
    ok(api2.configure_materials(host.session,&same_materials),"changed material policy preserves binding policy");
    data.frame.light_count=0;data.frame.lights=nullptr;
    need(api.submit(host.session,&data.frame)==E_INVALIDARG,"material rebuild preserves configured shadow mask");
    data.frame.light_count=1;data.frame.lights=&lamp;ok(api.submit(host.session,&data.frame),"submit with retained shadow light");
    need(host.resolves>hot,"material rebuild preserved texture resolver and invalidated asset cache");
    // The callback is polled again only after the source epoch advances.
    host.flip=true;++data.frame.source_epoch;ok(api.submit(host.session,&data.frame),"changed selection in advanced source epoch");
    const auto changed=pixels(api,host.session);pixel(changed,32,64,0,255,0,"changed left selection becomes green");
    pixel(changed,96,64,255,0,0,"changed right selection becomes red");
    A2FO_N11_TextureBindings authored{sizeof(authored),0,nullptr,nullptr};
    ok(api3.configure_texture_bindings(host.session,&authored),"reset authored texture mode");
    ++data.frame.source_epoch;ok(api.submit(host.session,&data.frame),"authored hull.tga fallback after reset");
    const auto reset=pixels(api,host.session);pixel(reset,32,64,0,0,255,"reset restores authored hull texture");
    // Retained emission is visible in darkness. A binding-policy change must
    // not replace the art policy with its default fullbright interpretation.
    host.sod=native11_fixture::model("hull.tga",true);host.flip=false;
    A2FO_N11_Shadows off{sizeof(off),0,128,.001f};ok(api2.configure_shadows(host.session,&off),"disable shadows for dark material proof");
    A2FO_N11_Materials retained{sizeof(retained),1,.25f,0};ok(api2.configure_materials(host.session,&retained),"set quarter retained emission");
    data.frame.light_count=0;data.frame.lights=nullptr;
    data.frame.ambient[0]=data.frame.ambient[1]=data.frame.ambient[2]=0;
    ok(api3.configure_texture_bindings(host.session,&binding),"enable bindings with retained emission policy");
    ++data.frame.source_epoch;ok(api.submit(host.session,&data.frame),"dark scoped fullbright materials");
    const auto dark=pixels(api,host.session);pixel(dark,32,64,64,0,0,"binding configure retains quarter red emission");
    pixel(dark,96,64,0,64,0,"binding configure retains quarter green emission");
    ok(api3.configure_texture_bindings(host.session,&authored),"reset bindings while retaining material policy");
    ok(api.submit(host.session,&data.frame),"dark authored fullbright materials");
    pixel(pixels(api,host.session),32,64,0,0,64,"binding reset retains quarter blue emission");
    need(api.destroy(host.session)==S_OK,"destroy session");
    no_legacy_modules();
    need(FreeLibrary(dll)!=0,"unload helper after session destruction");
}
}
int main(int argc,char** argv){need(argc==2,"expected helper DLL path");run(argv[1]);std::puts("PASS native11 v3 texture binding DLL client");}

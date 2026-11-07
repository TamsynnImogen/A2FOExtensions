// A real DLL client: only the public C header, Win32 loader and owned bytes.
#include "../sdk/include/a2fo_native11_api.h"
#include "../renderer/native11/capture_service.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
extern "C" int32_t native11_c_query(A2FO_N11_GetInterface,A2FO_N11_API*);
extern "C" int32_t native11_c_query2(A2FO_N11_GetInterface,A2FO_N11_API2*);
extern "C" int32_t native11_c_query3(A2FO_N11_GetInterface,A2FO_N11_API3*);

namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
std::vector<uint8_t> model(bool fullbright=false){
    std::vector<uint8_t> b;
    auto word=[&](uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](uint32_t v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);};
    auto number=[&](float v){uint32_t bits;std::memcpy(&bits,&v,4);dword(bits);};
    auto text=[&](const char* p){word(std::strlen(p));while(*p)b.push_back(*p++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);
    word(1);text("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(fullbright?1:0);word(1);word(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})number(v);
    text("default");dword(4);dword(1);text("hull.tga");dword(0);text("");word(0);
    word(3);word(3);word(1);
    for(float v:{-.75f,-.75f,.2f,.75f,-.75f,.2f,0.f,.75f,.2f})number(v);
    for(unsigned j=0;j<6;++j)number(0);
    word(1);text("mat");for(unsigned j=0;j<3;++j){word(j);word(j);}
    b.push_back(0);word(0);word(0);word(0);return b;
}
std::vector<uint8_t> texture(){
    std::vector<uint8_t> b(22);b[2]=2;b[12]=1;b[14]=1;b[16]=32;b[17]=0x28;
    b[20]=255;b[21]=255;return b;
}
A2FO_N11_API api{};
A2FO_N11_API2 api2{};
struct Host {
    std::vector<uint8_t> sod=model(),tga=texture();
    A2FO_N11_Session session=0;
    unsigned reads=0,mode=0,logs=0;
    bool block=false,reenter=false,reentry_refused=false;
    float* ambient_to_clear=nullptr;
};
int32_t A2FO_N11_CALL read(void* raw,uint32_t kind,const uint8_t* identifier,uint32_t length,
                          uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    auto& host=*static_cast<Host*>(raw);++host.reads;
    need(!host.block,"hot cache may not read host again");
    if(host.ambient_to_clear){std::fill(host.ambient_to_clear,host.ambient_to_clear+3,0);host.ambient_to_clear=nullptr;}
    const std::string id(reinterpret_cast<const char*>(identifier),length);
    need((kind==1&&id=="selected-model.sod"&&limit==64u*1024*1024)||
         (kind==2&&id=="hull.tga"&&limit==256u*1024*1024),"exact bounded asset identity");
    if(host.reenter){host.reenter=false;A2FO_N11_Usage usage{};usage.struct_size=sizeof(usage);
        A2FO_N11_Shadows settings{sizeof(settings),1,128,.001f};
        A2FO_N11_Materials materials{sizeof(materials),1,.02f,0};
        host.reentry_refused=api.usage(host.session,&usage)==E_UNEXPECTED&&api.destroy(host.session)==E_UNEXPECTED&&
            api2.configure_shadows(host.session,&settings)==E_UNEXPECTED&&api2.configure_materials(host.session,&materials)==E_UNEXPECTED;}
    auto bytes=kind==1?host.sod:host.tga;
    if(host.mode==3)return 1; // claims success but never provides bytes
    if(host.mode==2){need(sink(context,bytes.data(),limit+1)==0,"oversized sink refused before dereference");return 1;}
    const auto result=sink(context,bytes.data(),static_cast<uint32_t>(bytes.size()));
    if(host.mode==1){need(sink(context,bytes.data(),static_cast<uint32_t>(bytes.size()))==0,"second sink refused");return 1;}
    if(host.mode==4)return 0;
    std::fill(bytes.begin(),bytes.end(),0); // sink must have copied before return
    return result;
}
void A2FO_N11_CALL log(void* raw,const char* text,uint32_t length){
    need(text&&length,"owned error message");++static_cast<Host*>(raw)->logs;
}
struct Worker {A2FO_N11_Host host;A2FO_N11_Session session=0;bool before=false;int32_t result=0;};
DWORD WINAPI worker(void* raw){
    auto& task=*static_cast<Worker*>(raw);
    if(task.before){A2FO_N11_Session output=99;task.result=api.create(&task.host,&output);need(output==99,"failed create preserves handle");}
    else{A2FO_N11_Usage output{};output.struct_size=sizeof(output);task.result=api.usage(task.session,&output);
        need(api.destroy(task.session)==E_UNEXPECTED,"foreign-thread destroy refused");
        A2FO_N11_Session output_session=99;need(api.create(&task.host,&output_session)==E_UNEXPECTED&&output_session==99,"foreign create refused");}
    return 0;
}
void run_worker(Worker& data){
    HANDLE thread=CreateThread(nullptr,0,worker,&data,0,nullptr);need(thread!=nullptr,"private worker created");
    need(WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,"private worker completed");CloseHandle(thread);
}
std::vector<uint8_t> pixels(A2FO_N11_Session session,unsigned w,unsigned h){
    A2FO_N11_Readback out{};out.struct_size=sizeof(out);
    need(api.readback(session,&out)==S_FALSE&&out.width==w&&out.height==h&&out.pitch==w*4&&out.required_bytes==w*h*4,"metadata-only query");
    std::vector<uint8_t> bytes(static_cast<std::size_t>(out.required_bytes),123);out.bgra=bytes.data();out.capacity=bytes.size()-1;
    need(api.readback(session,&out)==HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)&&bytes[0]==123,"small buffer unchanged");
    out.capacity=bytes.size();ok(api.readback(session,&out),"DLL GPU readback");return bytes;
}
void red(const std::vector<uint8_t>& bytes,unsigned w,unsigned h){
    const auto* p=bytes.data()+((h/2)*w+w/2)*4;
    need(p[2]>250&&p[1]<4&&p[0]<4,"DLL owns native red triangle");
}
uint32_t address(const void* p){return static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
bool own_read(void*,uint32_t source,void* output,std::size_t length){
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(source),output,length,&copied)&&copied==length;
}
std::unique_ptr<a2fo::native11::Frame> captured_scene(){
    namespace n=a2fo::native11;
    std::array<uint8_t,8> entity{};std::array<uint8_t,0x84> instance{};
    std::array<uint8_t,0x3c> database{};std::array<uint8_t,0x1c0> camera{};
    const char name[]="selected-model.sod";const uint32_t vtable=0x1234,craft_vtable=0x5678;
    const auto inst=address(instance.data()),db=address(database.data()),text=address(name);
    std::memcpy(entity.data()+4,&inst,4);std::memcpy(instance.data()+0x80,&db,4);
    std::memcpy(instance.data(),&craft_vtable,4);instance[0x74]=1;
    const float scale[3]{.5f,1,1};std::memcpy(instance.data()+0x28,scale,sizeof(scale));
    std::memcpy(database.data(),&vtable,4);std::memcpy(database.data()+0x38,&text,4);
    const float matrix[12]{1,0,0,0,1,0,0,0,1,0,0,0};std::memcpy(instance.data()+0x44,matrix,sizeof(matrix));
    const float s=32.f/62.f,screen[16]{s,0,0,0,0,-s,0,0,0,0,1,0,s,s,0,1},viewport[4]{0,0,32,32};
    std::memcpy(camera.data()+0x160,screen,sizeof(screen));std::memcpy(camera.data()+0x1b0,viewport,sizeof(viewport));
    auto collector=std::make_unique<n::Collector>(nullptr,own_read);auto frame=std::make_unique<n::Frame>();
    need(collector->configure_plain_database_vtable(vtable)&&collector->configure_craft_instance_vtable(craft_vtable)&&
         collector->begin(address(camera.data()),true),"private capture begins");
    collector->observe_post_frustum(address(entity.data()),address(camera.data()));
    need(collector->end(address(camera.data()),*frame)&&frame->object_count==1&&
         frame->objects[0].model_identity==n::ModelIdentity::plain_database_name,"owned selected-model capture");
    // All source arrays disappear here. The native service must use only copies.
    return frame;
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"native service DLL load");
    const FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"versioned export present");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    std::memset(&api,0x5a,sizeof(api));const auto prior=api;
    need(get(99,sizeof(api),&api)==E_NOINTERFACE&&std::memcmp(&api,&prior,sizeof(api))==0,"version mismatch atomic");
    need(get(1,sizeof(api)-1,&api)==E_INVALIDARG&&std::memcmp(&api,&prior,sizeof(api))==0,"size mismatch atomic");
    ok(native11_c_query(get,&api),"C-compiled client versioned table call");
    need(api.version==1&&api.struct_size==sizeof(api)&&api.capabilities==A2FO_N11_CAP_OFFSCREEN_STATIC&&api.reserved==0,"offscreen-only capabilities");
    ok(native11_c_query2(get,&api2),"C-compiled version 2 table call");
    A2FO_N11_API3 api3{};ok(native11_c_query3(get,&api3),"C-compiled version 3 table call");
    need(api3.base.base.struct_size==sizeof(api3)&&api3.base.base.version==3&&
         api3.base.base.capabilities==15&&api3.configure_texture_bindings,"version 3 scoped binding capability");
    need(api2.base.struct_size==sizeof(api2)&&api2.base.version==2&&api2.base.capabilities==7&&
         api2.base.create==api.create&&api2.base.submit==api.submit&&api2.configure_shadows,"shadow capability and compatible call prefix");
    Host host;A2FO_N11_Host config{sizeof(config),0,32,32,&host,read,log};
    Worker failed{config,0,true,0};failed.host.width=0;run_worker(failed);
    need(failed.result==E_INVALIDARG,"failed first create releases thread binding");
    ok(api.create(&config,&host.session),"main-thread offscreen session");
    need(host.session!=0,"opaque session identity");Worker foreign{config,host.session,false,0};run_worker(foreign);
    need(foreign.result==E_UNEXPECTED,"foreign-thread usage refused");
    const std::string name="selected-model.sod";
    A2FO_N11_Object object{};for(unsigned i=0;i<4;++i)object.local_to_world[i*5]=1;
    object.model_identifier=reinterpret_cast<const uint8_t*>(name.data());object.model_identifier_length=name.size();
    A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);frame.generation=1;frame.source_epoch=1;
    for(unsigned i=0;i<4;++i)frame.world_to_clip[i*5]=1;
    frame.viewport[2]=frame.viewport[3]=32;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.object_count=1;frame.objects=&object;host.reenter=true;
    auto captured=captured_scene();const float ambient[3]{1,1,1};
    captured->objects[0].model_identity=a2fo::native11::ModelIdentity::unresolved;
    need(a2fo::native11::submit_captured(api,host.session,*captured,1,nullptr,0,ambient)==E_INVALIDARG&&host.reads==0,"unresolved capture refused before DLL callback");
    captured->objects[0].model_identity=a2fo::native11::ModelIdentity::plain_database_name;
    captured->objects[0].placement_identity=a2fo::native11::PlacementIdentity::unsupported_instance;
    need(a2fo::native11::submit_captured(api,host.session,*captured,1,nullptr,0,ambient)==E_INVALIDARG&&host.reads==0,"unsupported placement refused before DLL callback");
    captured->objects[0].placement_identity=a2fo::native11::PlacementIdentity::craft_runtime_scale;
    ok(a2fo::native11::submit_captured(api,host.session,*captured,1,nullptr,0,ambient),"owned capture through public DLL ABI");
    need(host.reads==2&&host.reentry_refused,"single borrowed reads and reentry guards");
    auto invalid3=api3;invalid3.base.base.capabilities&=~A2FO_N11_CAP_TEXTURE_BINDINGS;
    need(a2fo::native11::submit_captured(invalid3,host.session,*captured,1,nullptr,0,ambient)==E_INVALIDARG,
         "owned adapter rejects missing v3 binding capability");
    host.block=true;
    ok(a2fo::native11::submit_captured(api3,host.session,*captured,1,nullptr,0,ambient),"owned capture through v3 compatible prefix");
    host.block=false;
    auto image=pixels(host.session,32,32);red(image,32,32);
    need(image[(24*32+24)*4+2]<8,"captured half-width scale reaches native GPU placement");
    std::copy(captured->objects[0].world,captured->objects[0].world+16,object.local_to_world);
    host.block=true;ok(api.submit(host.session,&frame),"hot scene resubmit without callbacks");host.block=false;
    object.reserved=1;need(api.submit(host.session,&frame)==E_INVALIDARG,"reserved field refused");object.reserved=0;
    frame.object_count=2049;need(api.submit(host.session,&frame)==E_INVALIDARG,"object budget refused");frame.object_count=1;
    need(pixels(host.session,32,32)==image,"malformed frames preserve prior image");
    for(unsigned mode=1;mode<=4;++mode){host.mode=mode;++frame.source_epoch;
        need(api.submit(host.session,&frame)<0,"malformed provider refuses whole frame");
        need(pixels(host.session,32,32)==image,"malformed provider preserves image");}
    host.mode=0;++frame.source_epoch;host.ambient_to_clear=frame.ambient;
    ok(api.submit(host.session,&frame),"provider recovery in new source epoch");
    red(pixels(host.session,32,32),32,32);
    need(frame.ambient[0]==0,"provider mutation happened after scene copy");
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    std::fill(host.sod.begin(),host.sod.end(),0);std::fill(host.tga.begin(),host.tga.end(),0);host.block=true;
    ok(api.submit(host.session,&frame),"native resources own callback content");red(pixels(host.session,32,32),32,32);
    A2FO_N11_Usage usage{};usage.struct_size=sizeof(usage);ok(api.usage(host.session,&usage),"native residency");
    need(usage.device_epoch==1&&usage.meshes==1&&usage.textures==2,"service owns one model and texture");
    A2FO_N11_Shadows settings{sizeof(settings),2,128,.001f};
    ok(api2.configure_shadows(host.session,&settings),"configure shadow mask before light validation");
    const auto previous=pixels(host.session,32,32);
    need(api.submit(host.session,&frame)==E_INVALIDARG&&pixels(host.session,32,32)==previous,"mask missing from actual light set refuses frame atomically");
    settings.resolution=127;need(api2.configure_shadows(host.session,&settings)==E_INVALIDARG,"invalid shadow settings refused");
    settings.resolution=128;settings.light_mask=1;ok(api2.configure_shadows(host.session,&settings),"version 2 physical shadows enabled");
    settings.light_mask=0; // helper retains an owned copy of the settings
    need(api.submit(host.session,&frame)==E_INVALIDARG,"shadow settings copied before caller mutation");
    a2fo::native11::Light source;source.direction[1]=0;source.direction[2]=1;source.intensity=.5f;
    const float dim_ambient[3]{.1f,.1f,.1f};
    ok(a2fo::native11::submit_captured(api2,host.session,*captured,frame.source_epoch,&source,1,dim_ambient),"version 2 owned capture and native shadow maps");
    const auto shaded=pixels(host.session,32,32);const auto red_channel=shaded[(16*32+16)*4+2];
    need(red_channel>145&&red_channel<160,"shadowed C ABI preserves lighting on a self-receiving surface");
    ok(api2.configure_shadows(host.session,&settings),"disable version 2 shadow maps");
    ok(api.submit(host.session,&frame),"version 1 call signatures remain usable after version 2 configuration");red(pixels(host.session,32,32),32,32);
    ok(api.resize(host.session,48,24),"service target resize");frame.viewport[2]=48;frame.viewport[3]=24;
    ok(api.submit(host.session,&frame),"same owned resources after resize");red(pixels(host.session,48,24),48,24);
    ok(api.usage(host.session,&usage),"resize residency");need(usage.device_epoch==1&&usage.meshes==1,"resize keeps device epoch");
    need(api.resize(host.session,0,24)==E_INVALIDARG,"invalid service resize");
    red(pixels(host.session,48,24),48,24);
    const auto old=host.session;ok(api.destroy(old),"session destruction on owner thread");
    need(api.destroy(old)==E_INVALIDARG&&api.submit(old,&frame)==E_INVALIDARG,"stale session refused");
    ok(api.create(&config,&host.session),"new session after complete cleanup");need(host.session>old,"session IDs never recycled");
    host.sod=model(true);host.tga=texture();host.block=false;
    frame.viewport[2]=frame.viewport[3]=32;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;
    ++frame.source_epoch;frame.light_count=0;frame.lights=nullptr;
    ok(api.submit(host.session,&frame),"authored fullbright material");red(pixels(host.session,32,32),32,32);
    A2FO_N11_Materials materials{sizeof(materials),1,.02f,0};
    ok(api2.configure_materials(host.session,&materials),"explicit fullbright relighting policy");
    ok(api.usage(host.session,&usage),"material cache invalidation residency");
    need(usage.meshes==0&&usage.textures==1,"policy change releases old material cache before reload");
    materials.relight_fullbright=0; // configuration owns its policy
    ok(api.submit(host.session,&frame),"relit material without external source");
    const auto dimmed=pixels(host.session,32,32);need(dimmed[(16*32+16)*4+2]>=4&&dimmed[(16*32+16)*4+2]<=7,"relight retains selected emission fraction");
    const auto reads=host.reads;materials.retained_emission=-1;
    need(api2.configure_materials(host.session,&materials)==E_INVALIDARG&&host.reads==reads,"invalid policy changes no assets");
    A2FO_N11_Light lamp{};lamp.type=1;lamp.direction[2]=1;lamp.colour[0]=lamp.colour[1]=lamp.colour[2]=1;lamp.intensity=1;
    lamp.falloff_range=100;lamp.cone_inner_degrees=30;lamp.cone_outer_degrees=60;frame.light_count=1;frame.lights=&lamp;
    ok(api.submit(host.session,&frame),"relit hull responds to light");red(pixels(host.session,32,32),32,32);
    materials.retained_emission=0;ok(api2.configure_materials(host.session,&materials),"restore authored material policy");
    frame.light_count=0;frame.lights=nullptr;ok(api.submit(host.session,&frame),"authored emission restored");red(pixels(host.session,32,32),32,32);
    ok(api.destroy(host.session),"last session cleanup");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&!GetModuleHandleA("D3DX81ab.dll"),"legacy graphics absent");
    need(host.logs>=4,"provider failures reported without cross-DLL allocation");
    need(FreeLibrary(dll)!=0,"unload after all sessions destroyed");
    std::puts("PASS native11 DLL C ABI v1/v2: thread/reentry, owned callbacks, budgets, readback, source epochs, shadow/relighting controls, resize, lifetime, no legacy device");
}
}
int main(int argc,char** argv){need(argc==2,"expected native helper path");run(argv[1]);}

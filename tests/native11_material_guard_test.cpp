#include "../core/native11_assets.hpp"
#include "../core/native11_asset_profile.hpp"
#include "../core/native11_capture.hpp"
#include "../core/native11_capture_scale_profile.hpp"
#include "../core/native11_materials.hpp"
#include "../core/native11_material_profile.hpp"
#include "../core/native11_textures.hpp"
#include "../core/native11_tga_profile.hpp"
#include "../renderer/native11/capture_service.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace a2fo::native11;
std::uint8_t* image=nullptr;
bool allow_patch=false;
unsigned patches=0,material_patches=0;
bool restart_before_publish=false;
void log(const char*) {}
void need(bool yes,const char* label) {if(!yes){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",label,GetLastError());std::exit(1);}}
std::uint32_t addr(const void* p) {return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
std::uint32_t get(const void* p) {std::uint32_t value=0;std::memcpy(&value,p,4);return value;}
void put(void* p,std::uint32_t value) {std::memcpy(p,&value,4);}
void mapped_put(std::uint8_t* p,std::uint32_t value) {
    DWORD old=0,ignored=0;need(VirtualProtect(p,4,PAGE_READWRITE,&old)!=0,"private mapping writable");
    put(p,value);need(VirtualProtect(p,4,old,&ignored)!=0,"private mapping protected");
}
template<class F> void altered(std::uint8_t* p,F check) {
    DWORD old=0,ignored=0;need(VirtualProtect(p,1,PAGE_READWRITE,&old)!=0,"mutation writable");
    const auto value=*p;*p^=1;check();*p=value;
    need(VirtualProtect(p,1,old,&ignored)!=0,"mutation protected");
}
template<class P,class Slots> void fix_profile(std::uint32_t preferred,const P& regions,
    const Slots& slots) {
    const auto base=addr(image),delta=base-preferred;
    for(const auto& region:regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i],original=get(region.bytes+offset);
        auto* at=image+region.rva+offset;const auto found=get(at);
        need(found==original||found==original+delta,"reviewed HIGHLOW position");
        if(found==original&&delta)mapped_put(at,original+delta);
    }
    for(const auto& slot:slots){auto* at=image+slot[0];const auto found=get(at);
        need(found==preferred+slot[1]||found==base+slot[1],"reviewed vtable slot");
        if(found==preferred+slot[1]&&delta)mapped_put(at,base+slot[1]);
    }
}
template<class Regions> void fix_regions(std::uint32_t preferred,const Regions& regions) {
    const auto base=addr(image),delta=base-preferred;
    for(const auto& region:regions)for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i],original=get(region.bytes+offset);
        auto* at=image+region.rva+offset;const auto found=get(at);
        need(found==original||found==original+delta,"reviewed HIGHLOW position");
        if(found==original&&delta)mapped_put(at,original+delta);
    }
}
void fix_all() {
    namespace a=a2fo::native11_assets::profile;
    namespace c=a2fo::native11_capture::profile;
    namespace m=a2fo::native11_materials::profile;
    namespace t=a2fo::native11_textures::tga_profile;
    fix_profile(a::preferred_base,a::regions,a::slots);
    fix_regions(c::preferred_base,c::regions);
    // The scene profile verifies one scale slot beyond the asset slots.
    const auto base=addr(image);auto* scale=image+0x2b3e38;
    need(get(scale)==c::preferred_base+0xcb6b0||get(scale)==base+0xcb6b0,"scale slot");
    if(get(scale)==c::preferred_base+0xcb6b0&&base!=c::preferred_base)mapped_put(scale,base+0xcb6b0);
    fix_profile(m::preferred_base,m::regions,m::slots);
    fix_profile(t::preferred_base,t::regions,t::slots);
}
std::vector<std::uint8_t> tga(std::uint8_t red,std::uint8_t green) {
    std::vector<std::uint8_t> b(22);b[2]=2;b[12]=1;b[14]=1;b[16]=32;b[17]=0x20;
    b[18]=0;b[19]=green;b[20]=red;b[21]=255;return b;
}
struct LiveTexture {
    std::array<std::uint8_t,0x40> object{};
    std::array<std::uint8_t,0x20> stream{};
    std::string name,requested;
    std::vector<std::uint8_t> bytes;
    LiveTexture(const char* id,std::uint8_t red,std::uint8_t green):
        name(id),requested(std::string("Textures\\")+id),bytes(tga(red,green)) {
        put(object.data()+8,addr(name.c_str()));put(object.data()+0x18,0);
        put(stream.data(),addr(image)+0x2bc874);
        put(stream.data()+0x14,addr(bytes.data()));
        put(stream.data()+0x18,addr(bytes.data()+bytes.size()));
        put(stream.data()+0x1c,addr(bytes.data()));
    }
    std::uint32_t key() const {return addr(object.data());}
    void observe() {a2fo_native11_observe_tga_open(0,stream.data(),requested.c_str(),1,object.data(),0);}
};
struct Scene {
    std::array<std::uint8_t,0x50> database{};
    std::array<std::uint8_t,0x20> stream{};
    std::array<std::uint8_t,0x140> mesh{};
    std::array<std::uint8_t,0x10> mesh_vtable{};
    std::array<std::uint8_t,4> slot{};
    std::array<std::array<std::uint8_t,0x100>,2> instances{};
    std::array<std::array<std::uint8_t,8>,2> entities{};
    std::array<std::uint8_t,0x1c0> camera{};
    std::vector<std::uint8_t> model=native11_fixture::model("hull.tga");
    char name[16]="test.sod",node_name[8]="mesh";
    Scene() {
        const auto base=addr(image);
        put(database.data(),base+0x2bc56c);put(database.data()+0x38,addr(name));
        put(database.data()+0x3c,addr(mesh.data()));
        put(stream.data(),base+0x2bc874);put(stream.data()+0x14,addr(model.data()));
        put(stream.data()+0x18,addr(model.data()+model.size()));put(stream.data()+0x1c,addr(model.data()+1));
        put(mesh.data(),addr(mesh_vtable.data()));put(mesh.data()+8,addr(node_name));
        put(mesh.data()+0xbc,4);put(mesh.data()+0x114,addr(slot.data()));put(mesh.data()+0x11c,1);
        put(mesh_vtable.data()+4,base+0x1968f0);
        const float s=64.f/127.f;
        const float screen[16]{s,0,0,0,0,-s,0,0,0,0,1,0,s,s,0,1};
        const float viewport[4]{0,0,128,128};
        std::memcpy(camera.data()+0x160,screen,sizeof(screen));
        std::memcpy(camera.data()+0x1b0,viewport,sizeof(viewport));
        for(unsigned i=0;i<2;++i){
            auto& instance=instances[i];put(instance.data(),base+0x2b3e2c);
            put(instance.data()+0x80,addr(database.data()));instance[0x74]=0;
            const float world[12]{.5f,0,0,0,.5f,0,0,0,.5f,i?.5f:-.5f,0,0};
            std::memcpy(instance.data()+0x44,world,sizeof(world));
            put(entities[i].data()+4,addr(instance.data()));
        }
    }
    void publish() {a2fo_native11_observe_database(database.data(),stream.data(),0,name);}
    void observe(unsigned i) {a2fo_native11_observe_entity(entities[i].data(),camera.data());}
    void material(unsigned i) {a2fo_native11_observe_materials(instances[i].data(),camera.data());}
    void select(std::uint32_t texture) {put(slot.data(),texture);}
};
struct Host {const BoundMaterialCapture* capture;unsigned model_reads=0,texture_reads=0,resolutions=0;};
std::int32_t A2FO_N11_CALL read_bound(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t maximum,A2FO_N11_BytesSink sink,void* context) {
    auto& host=*static_cast<Host*>(raw);if(kind==1)++host.model_reads;else if(kind==2)++host.texture_reads;
    return BoundMaterialCapture::read_asset(const_cast<BoundMaterialCapture*>(host.capture),kind,id,length,maximum,sink,context);
}
std::int32_t A2FO_N11_CALL resolve_bound(void* raw,const std::uint8_t* model,std::uint32_t model_n,
    std::uint32_t part,const std::uint8_t* authored,std::uint32_t authored_n,std::uint32_t maximum,
    A2FO_N11_BytesSink sink,void* context) {
    auto& host=*static_cast<Host*>(raw);++host.resolutions;
    return BoundMaterialCapture::resolve_texture(const_cast<BoundMaterialCapture*>(host.capture),
        model,model_n,part,authored,authored_n,maximum,sink,context);
}
void handoff(const char* helper,const std::shared_ptr<const BoundMaterialCapture>& capture) {
    HMODULE dll=LoadLibraryA(helper);need(dll,"native renderer DLL load");
    auto symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"public versioned export");
    A2FO_N11_GetInterface get_api=nullptr;static_assert(sizeof(get_api)==sizeof(symbol));
    std::memcpy(&get_api,&symbol,sizeof(get_api));A2FO_N11_API3 api{};
    need(get_api(3,sizeof(api),&api)>=0&&api.base.base.version==3,"public C API v3");
    Host host{capture.get()};A2FO_N11_Host config{sizeof(config),0,128,128,&host,read_bound,nullptr};
    A2FO_N11_Session session=0;need(api.base.base.create(&config,&session)>=0&&session,"native offscreen session");
    A2FO_N11_TextureBindings binding{sizeof(binding),0,&host,resolve_bound};
    need(api.configure_texture_bindings(session,&binding)>=0,"retained selected-texture binding");
    const float ambient[3]{1,1,1};
    need(submit_captured(api,session,capture->frame(),capture->source_epoch(),nullptr,0,ambient)>=0&&
         host.model_reads>=2&&host.texture_reads>=2&&host.resolutions>=2,"public API3 retained submission");
    A2FO_N11_Readback rb{};rb.struct_size=sizeof(rb);
    need(api.base.base.readback(session,&rb)==S_FALSE&&rb.width==128&&rb.height==128&&
         rb.required_bytes==128*128*4,"offscreen readback metadata");
    std::vector<std::uint8_t> pixels(rb.required_bytes);rb.bgra=pixels.data();rb.capacity=pixels.size();
    need(api.base.base.readback(session,&rb)>=0,"offscreen readback bytes");
    auto at=[&](unsigned x){return pixels.data()+(64*128+x)*4;};
    need(at(32)[2]>250&&at(32)[1]<5&&at(32)[0]<5,"left selected red texture");
    need(at(96)[1]>250&&at(96)[2]<5&&at(96)[0]<5,"right selected green texture");
    HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    need(modules!=INVALID_HANDLE_VALUE,"module enumeration handle");
    MODULEENTRY32 entry{};entry.dwSize=sizeof(entry);
    need(Module32First(modules,&entry)!=0,"module enumeration starts");
    do {need(_stricmp(entry.szModule,"d3d8.dll")&&_stricmp(entry.szModule,"d3d9.dll")&&
        _strnicmp(entry.szModule,"d3dx",4),"no legacy graphics DLL resolved");}while(Module32Next(modules,&entry));
    const auto module_error=GetLastError();CloseHandle(modules);
    need(module_error==ERROR_NO_MORE_FILES,"complete module enumeration");
    need(api.base.base.destroy(session)>=0&&FreeLibrary(dll),"destroy native offscreen session");
}
}
extern "C" void a2fo_native11_capture_before_publish() noexcept {
    if(!restart_before_publish)return;
    restart_before_publish=false;
    a2fo::native11_capture::disable();
    need(a2fo::native11_capture::initialize(reinterpret_cast<HMODULE>(image),true,log),"restart before publication");
}
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* target,const std::uint8_t* expected,std::size_t length) {
    const std::uint8_t model[]{0xe8,0x0e,0x2c,0x01,0x00},entity[]{0x8b,0x11,0xff,0x52,0x14},
        tga_call[]{0xe8,0x7a,0xcb,0xff,0xff},material[]{0xe8,0x6a,0x8d,0x15,0x00};
    need(image&&expected&&target&&length==5,"checked patch arguments");
    const bool is_model=site==image+0x22ce4d&&std::memcmp(expected,model,5)==0;
    const bool is_entity=site==image+0xcfdbf&&std::memcmp(expected,entity,5)==0;
    const bool is_tga=site==image+0x2435d1&&std::memcmp(expected,tga_call,5)==0;
    const bool is_material=site==image+0xd5a11&&std::memcmp(expected,material,5)==0;
    need(is_model||is_entity||is_tga||is_material,"reviewed checked patch site and signature");
    need(std::memcmp(site,expected,5)==0,"pristine checked patch bytes");
    if(is_material)++material_patches;
    ++patches;if(!allow_patch)return false;
    if(is_material||is_tga){std::uint8_t call[5]{0xe8};put(call+1,addr(target)-(addr(site)+5));
        DWORD old=0,ignored=0;need(VirtualProtect(site,5,PAGE_READWRITE,&old)!=0,"material CALL writable");
        std::memcpy(site,call,5);need(VirtualProtect(site,5,old,&ignored)!=0,"material CALL protected");}
    return true;
}
int main(int argc,char** argv) {
    using namespace a2fo;
    need(!native11_materials::initialize(GetModuleHandleA(nullptr),false,log)&&patches==0,"default disabled");
    need(!native11_materials::initialize(GetModuleHandleA(nullptr),true,log)&&patches==0,"foreign executable refused");
    need(argc==3,"usage: native11_material_guard_test.exe PRIVATE_PINNED_ARMADAL_EXE NATIVE11_RENDERER_DLL");
    HMODULE module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(module!=nullptr,"private pinned ArmadaL inspection mapping");image=reinterpret_cast<std::uint8_t*>(module);
    fix_all();
    namespace p=native11_materials::profile;
    for(const auto& region:p::regions){
        for(const auto offset:{std::size_t{0},region.size-1})altered(image+region.rva+offset,[&]{
            need(!native11_materials::initialize(module,true,log)&&material_patches==0,"material region mutation refused");});
        for(std::size_t i=0;i<region.relocation_count;++i)altered(image+region.rva+region.relocations[i],[&]{
            need(!native11_materials::initialize(module,true,log)&&material_patches==0,"material HIGHLOW mutation refused");});
    }
    for(const auto& slot:p::slots)altered(image+slot[0],[&]{
        need(!native11_materials::initialize(module,true,log)&&material_patches==0,"material slot mutation refused");});
    allow_patch=false;
    need(!native11_materials::initialize(module,true,log)&&material_patches==1,"checked material CALL failure");
    allow_patch=true;
    need(native11_materials::initialize(module,true,log)&&material_patches==2,"checked material CALL retry");
    need(a2fo_native11_material_draw_original==addr(image)+0x22e780,"original draw continuation");
    const auto installed_call=get(image+0xd5a12);
    mapped_put(image+0xd5a12,installed_call^1u);
    need(!native11_materials::initialize(module,true,log)&&material_patches==2,
         "foreign material CALL ownership refused without overwrite");
    mapped_put(image+0xd5a12,installed_call);
    need(native11_materials::initialize(module,true,log)&&material_patches==2,
         "owned material CALL rearmed without repatch");
    need(native11_assets::initialize(module,true,log),"model observer initialized");
    need(native11_capture::initialize(module,true,log),"scene observer initialized");
    need(native11_textures::initialize_tga(module,true,log),"TGA observer initialized");
    Scene scene;scene.publish();native11::ModelAssetInfo info{};
    need(native11_assets::model_info(addr(scene.database.data()),info)&&info.serial,"model observer pinned version");
    LiveTexture red("red-live",255,0),green("green-live",0,255);red.observe();green.observe();
    need(native11_textures::current(red.key())&&native11_textures::current(green.key()),"both actual TGA streams captured");
    auto frame=[&](bool left_material,bool right_material,bool duplicate=false,bool wrong_camera=false){
        const bool owned=native11_capture::begin(scene.camera.data(),1);need(owned,"manual main camera begin");
        scene.observe(0);scene.select(red.key());if(left_material)scene.material(0);
        if(wrong_camera)a2fo_native11_observe_materials(scene.instances[0].data(),scene.camera.data()+1);
        if(duplicate)scene.material(0);
        scene.observe(1);scene.select(green.key());if(right_material)scene.material(1);
        native11_capture::end(scene.camera.data(),owned);
        return native11_capture::latest_materials();
    };
    need(!frame(false,true),"missing material event rejects frame");
    {
        const bool owned=native11_capture::begin(scene.camera.data(),1);need(owned,"wrong-camera frame begin");
        scene.observe(0);scene.select(red.key());
        a2fo_native11_observe_materials(scene.instances[0].data(),scene.camera.data()+1);
        scene.observe(1);scene.select(green.key());scene.material(1);
        native11_capture::end(scene.camera.data(),owned);
        need(!native11_capture::latest_materials(),"wrong-camera material event cannot complete main frame");
    }
    need(!frame(true,true,true),"duplicate material event rejects frame");
    auto retained=frame(true,true,false,true);
    need(retained&&retained->frame().object_count==2&&
         std::strcmp(retained->frame().objects[0].model_name,retained->frame().objects[1].model_name),
         "per-instance selected material views retained; wrong-camera event ignored");
    restart_before_publish=true;
    need(!frame(true,true),"disable and rearm before publication rejects obsolete material frame");
    need(frame(true,true)!=nullptr,"fresh lifecycle recovers material capture");
    {
        const auto old_texture=native11_textures::current(red.key());
        const bool owned=native11_capture::begin(scene.camera.data(),1);need(owned,"observation-time texture frame begin");
        scene.observe(0);scene.select(red.key());scene.material(0);
        scene.observe(1);scene.select(green.key());scene.material(1);
        red.bytes[20]=0;red.bytes[18]=255;red.observe();
        need(native11_textures::current(red.key())!=old_texture,"texture producer advances version before seal");
        native11_capture::end(scene.camera.data(),owned);
        retained=native11_capture::latest_materials();
        need(retained!=nullptr,"observation-time texture versions remain valid after producer replacement");
    }
    auto old=retained;
    {
        const bool owned=native11_capture::begin(scene.camera.data(),1);need(owned,"stale-model frame begin");
        scene.observe(0);scene.select(red.key());scene.material(0);
        scene.observe(1);scene.select(green.key());scene.material(1);
        scene.publish();
        native11_capture::end(scene.camera.data(),owned);
        need(!native11_capture::latest_materials()&&old->frame().object_count==2,
             "replaced model version before sealing rejects new frame; retained view survives");
    }
    native11_textures::disable();native11_assets::disable();native11_materials::disable();native11_capture::disable();
    need(!native11_capture::latest_materials()&&old->frame().object_count==2,"producer disabled; retained owner survives");
    std::fill(scene.model.begin(),scene.model.end(),0);
    std::fill(red.bytes.begin(),red.bytes.end(),0);std::fill(green.bytes.begin(),green.bytes.end(),0);
    scene.model.clear();red.bytes.clear();green.bytes.clear();
    scene.database.fill(0);scene.mesh.fill(0);scene.mesh_vtable.fill(0);scene.slot.fill(0);
    for(auto& instance:scene.instances)instance.fill(0);
    for(auto& entity:scene.entities)entity.fill(0);
    scene.camera.fill(0);std::memset(scene.name,0,sizeof(scene.name));std::memset(scene.node_name,0,sizeof(scene.node_name));
    red.object.fill(0);green.object.fill(0);scene.stream.fill(0);red.stream.fill(0);green.stream.fill(0);
    need(FreeLibrary(module)!=0,"private mapping unloaded before native submit");image=nullptr;
    handoff(argv[2],old);
    std::puts("PASS native11 material guard and per-instance TGA selection through retained API3 offscreen capture");
}

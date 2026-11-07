#include "../core/native11_textures.hpp"
#include "../core/native11_tga_profile.hpp"
#include "../renderer/native11/capture_service.hpp"
#include "../renderer/native11/open_read_owner.hpp"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
std::uint8_t* image=nullptr;
unsigned patches=0,logs=0;
bool permit_patch=false;
std::uint8_t* gateway=nullptr;
std::uint32_t replacement=0;
bool accept_owner=false;
bool restart_before_commit=false;
bool retire_before_commit=false;
std::uint32_t retire_key=0;
void log(const char*) {++logs;}
void need(bool yes,const char* label) {
    if(!yes){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",label,GetLastError());std::exit(1);}
}
std::uint32_t address(const void* p) {return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
std::uint32_t word(const void* p) {std::uint32_t value=0;std::memcpy(&value,p,4);return value;}
void put(void* p,std::uint32_t value) {std::memcpy(p,&value,4);}
void mapped_word(std::uint8_t* p,std::uint32_t before,std::uint32_t after) {
    need(word(p)==before,"private relocation original");
    DWORD old=0,ignored=0;need(VirtualProtect(p,4,PAGE_READWRITE,&old)!=0,"relocation writable");
    put(p,after);need(VirtualProtect(p,4,old,&ignored)!=0,"relocation protected");
}
template<class Check> void altered(std::uint8_t* p,Check check) {
    DWORD old=0,ignored=0;need(VirtualProtect(p,1,PAGE_READWRITE,&old)!=0,"mutation writable");
    const auto before=*p;*p^=1;check();*p=before;
    need(VirtualProtect(p,1,old,&ignored)!=0,"mutation protected");
}
void jump(std::uint8_t* at,std::uint32_t target) {
    at[0]=0xe9;put(at+1,target-(address(at)+5));
}
bool read_memory(void*,std::uint32_t at,void* out,std::size_t count) {
    SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),out,count,&got)&&got==count;
}
bool owned_open_read(HMODULE module) noexcept {
    if(!accept_owner||!gateway||module!=reinterpret_cast<HMODULE>(image))return false;
    OpenReadOwner owner{address(image)+0x240150,address(gateway),replacement,6};
    return owns_open_read_gateway(nullptr,read_memory,owner);
}
DWORD WINAPI restart_tga_worker(void*) {
    return a2fo::native11_textures::initialize_tga(reinterpret_cast<HMODULE>(image),true,log,owned_open_read)?0:1;
}
void install_fake_owner() {
    gateway=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    need(gateway,"private known-owner gateway allocation");
    std::memcpy(gateway,open_read_entry_bytes.data(),6);
    jump(gateway+6,address(image)+0x240156);
    replacement=address(gateway);
    auto* target=image+0x240150;
    need(std::memcmp(target,open_read_entry_bytes.data(),6)==0,"pristine OpenRead entry");
    DWORD old=0,ignored=0;need(VirtualProtect(target,6,PAGE_READWRITE,&old)!=0,"private OpenRead writable");
    jump(target,replacement);target[5]=0x90;
    need(VirtualProtect(target,6,old,&ignored)!=0,"private OpenRead protected");
}
std::vector<std::uint8_t> red_tga() {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=32;bytes[17]=0x20;bytes[20]=255;bytes[21]=255;return bytes;
}
struct Fixture {
    std::array<std::uint8_t,0x40> texture{};
    std::array<std::uint8_t,0x20> stream{};
    std::vector<std::uint8_t> source=red_tga();
    std::string logical="hull.tga";
    std::string requested="Textures\\hull.tga";
    Fixture() {
        put(texture.data()+8,address(logical.c_str()));put(texture.data()+0x18,0);
        put(stream.data(),address(image)+0x2bc874);
        put(stream.data()+0x0c,0xdeadbeef); // Physical file-name field is deliberately ignored.
        sync_span();
    }
    void sync_span() {
        put(stream.data()+0x14,address(source.data()));
        put(stream.data()+0x18,address(source.data()+source.size()));
        put(stream.data()+0x1c,address(source.data()));
    }
    std::uint32_t key() const {return address(texture.data());}
    void observe(std::uint32_t result=0,std::uint32_t buffered=1,std::uint32_t level=0) {
        a2fo_native11_observe_tga_open(result,stream.data(),requested.c_str(),buffered,texture.data(),level);
    }
};
std::vector<std::uint8_t> model(const char* texture) {
    std::vector<std::uint8_t> b;
    auto half=[&](std::uint16_t n){b.push_back(n&255);b.push_back(n>>8);};
    auto full=[&](std::uint32_t n){for(unsigned i=0;i<4;++i)b.push_back((n>>(i*8))&255);};
    auto real=[&](float n){std::uint32_t bits=0;std::memcpy(&bits,&n,4);full(bits);};
    auto text=[&](const char* p){half(static_cast<std::uint16_t>(std::strlen(p)));while(*p)b.push_back(*p++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);real(1.93f);
    half(1);text("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})real(v);
    b.push_back(2);b.push_back(0);half(1);half(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})real(v);
    text("default");full(4);full(1);text(texture);full(0);text("");half(0);
    half(3);half(3);half(1);
    for(float v:{-.75f,-.75f,.2f,.75f,-.75f,.2f,0.f,.75f,.2f})real(v);
    for(unsigned j=0;j<6;++j)real(0);
    half(1);text("mat");for(unsigned j=0;j<3;++j){half(j);half(j);}
    b.push_back(0);half(0);half(0);half(0);return b;
}
std::shared_ptr<const ModelAsset> resolve_model(void* raw,std::uint32_t db,std::uint64_t serial) {
    return static_cast<ModelAssetStore*>(raw)->find(db,serial);
}
struct Host {const BoundCapture* capture;unsigned reads=0;};
std::int32_t A2FO_N11_CALL read_bound(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t maximum,A2FO_N11_BytesSink sink,void* context) {
    auto& host=*static_cast<Host*>(raw);++host.reads;
    return BoundCapture::read_asset(const_cast<BoundCapture*>(host.capture),kind,id,length,maximum,sink,context);
}
void gpu_handoff(const char* helper,HMODULE module) {
    using namespace a2fo::native11_textures;
    need(initialize_tga(module,true,log,owned_open_read),"TGA observer rearmed for native handoff");
    Fixture fixture;const auto original=fixture.source;fixture.observe();
    std::vector<SnapshotTexture> textures;std::uint64_t generation=0;std::string error;
    need(collect(textures,generation,error)&&textures.size()==1&&
         textures[0].identifier=="hull.tga"&&*textures[0].bytes==original,
         "exact opened TGA snapshot collected");
    ModelAssetStore store;OpenedFile opened;opened.name="test.sod";opened.bytes=model("hull.tga");
    need(store.publish(1,0,std::move(opened)),"synthetic textured SOD stored");
    ModelAssetInfo info{};need(store.info(1,info),"SOD serial available");
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=1;frame->object_count=1;
    for(unsigned i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=128;
    frame->objects[0].database=1;frame->objects[0].model_serial=info.serial;
    frame->objects[0].model_identity=ModelIdentity::plain_database_name;
    frame->objects[0].placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(frame->objects[0].model_name,"test.sod");
    std::shared_ptr<const BoundCapture> bound;
    need(build_bound_capture(*frame,generation,resolve_model,&store,textures,bound,error)&&
         bound->models().size()==1,"TGA and SOD bound to retained owner");
    store.clear();frame.reset();textures.clear();disable();fixture.source.assign(22,0);
    HMODULE dll=LoadLibraryA(helper);need(dll!=nullptr,"native renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"public versioned export");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API2 api{};need(get(2,sizeof(api),&api)>=0&&api.base.version==2,"public C API v2");
    Host host{bound.get()};A2FO_N11_Host config{sizeof(config),0,128,128,&host,read_bound,nullptr};
    A2FO_N11_Session session=0;need(api.base.create(&config,&session)>=0&&session,"native offscreen session");
    const float ambient[3]{1,1,1};
    need(submit_captured(api,session,bound->frame(),bound->source_epoch(),nullptr,0,ambient)>=0&&host.reads>=2,
         "retained TGA and SOD submitted through public API");
    A2FO_N11_Readback readback{};readback.struct_size=sizeof(readback);
    need(api.base.readback(session,&readback)==S_FALSE&&readback.width==128&&
         readback.height==128&&readback.required_bytes==128*128*4,"native readback metadata");
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(readback.required_bytes));
    readback.bgra=pixels.data();readback.capacity=pixels.size();
    need(api.base.readback(session,&readback)>=0,"native readback bytes");
    const auto* center=pixels.data()+(64*128+64)*4;
    need(center[2]>250&&center[1]<4&&center[0]<4,"captured TGA renders red center pixel");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
         !GetModuleHandleA("D3DX81ab.dll"),"no legacy graphics imports resolved");
    need(api.base.destroy(session)>=0&&FreeLibrary(dll)!=0,"destroy session before unload");
    std::puts("PASS native11 opened TGA -> retained snapshot -> native DLL red pixel; source buffer removed before submit");
}
}

extern "C" void a2fo_native11_texture_before_commit() noexcept {
    if(retire_before_commit){retire_before_commit=false;
        need(a2fo::native11_textures::retire_texture(retire_key),
             "TGA pending ticket retired before commit");return;}
    if(!restart_before_commit)return;
    restart_before_commit=false;
    HANDLE thread=CreateThread(nullptr,0,restart_tga_worker,nullptr,0,nullptr);
    need(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"TGA reconfiguration worker completes before commit");
    DWORD result=1;need(GetExitCodeThread(thread,&result)&&result==0,"TGA reconfiguration worker succeeded");CloseHandle(thread);
}
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* target,
    const std::uint8_t* expected,std::size_t length) {
    ++patches;
    const std::uint8_t signature[]{0xe8,0x7a,0xcb,0xff,0xff};
    need(image&&site==image+0x2435d1&&target==reinterpret_cast<void*>(&a2fo_native11_tga_open_tap)&&
         expected&&length==5&&std::memcmp(expected,signature,5)==0&&
         std::memcmp(site,signature,5)==0,"checked TGA CALL contract");
    if(!permit_patch)return false;
    std::uint8_t call[5]{0xe8};put(call+1,address(target)-(address(site)+5));
    DWORD old=0,ignored=0;need(VirtualProtect(site,5,PAGE_READWRITE,&old)!=0,"private CALL writable");
    std::memcpy(site,call,5);need(VirtualProtect(site,5,old,&ignored)!=0,"private CALL protected");
    return true;
}
int main(int argc,char** argv) {
    using namespace a2fo::native11_textures;
    std::vector<SnapshotTexture> output;std::uint64_t generation=0;std::string error;
    need(!initialize_tga(GetModuleHandleA(nullptr),false,log)&&patches==0,"default disabled");
    need(collect(output,generation,error)&&output.empty(),"default disabled collection");
    need(!initialize_tga(GetModuleHandleA(nullptr),true,log)&&patches==0,
         "foreign executable refused before patch");
    if(argc==1){std::puts("PASS native11 TGA default-off guard");return 0;}
    need(argc==2||argc==3,"usage: native11_tga_guard_test.exe [PRIVATE_PINNED_ARMADAL_EXE [NATIVE11_RENDERER_DLL]]");
    HMODULE module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    need(module!=nullptr,"private pinned ArmadaL inspection mapping");
    image=reinterpret_cast<std::uint8_t*>(module);
    namespace p=a2fo::native11_textures::tga_profile;
    const auto numeric=address(image),delta=numeric-p::preferred_base;
    const auto first=p::slots.front();
    if(delta&&word(image+first[0])==p::preferred_base+first[1]) {
        need(!initialize_tga(module,true,log)&&patches==0,"unrelocated inspection mapping refused");
        for(const auto& region:p::regions) {
            need(std::memcmp(image+region.rva,region.bytes,region.size)==0,"preferred-base profile bytes");
            for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
                const auto original=word(region.bytes+offset);
                mapped_word(image+region.rva+offset,original,original+delta);
            }
        }
        for(const auto& slot:p::slots)
            mapped_word(image+slot[0],p::preferred_base+slot[1],numeric+slot[1]);
    } else need(word(image+first[0])==numeric+first[1],"loaded inspection slots are relative");
    for(const auto& region:p::regions) {
        for(const auto offset:{std::size_t{0},region.size-1})altered(image+region.rva+offset,[&]{
            need(!initialize_tga(module,true,log)&&patches==0,"profile boundary mutation refused");
        });
        for(std::size_t i=0;i<region.relocation_count;++i)
            altered(image+region.rva+region.relocations[i],[&]{
                need(!initialize_tga(module,true,log)&&patches==0,"normalized HIGHLOW mutation refused");
            });
    }
    for(const auto& slot:p::slots)altered(image+slot[0],[&]{
        need(!initialize_tga(module,true,log)&&patches==0,"slot mutation refused");
    });
    permit_patch=false;
    need(!initialize_tga(module,true,log)&&patches==1&&!current(1),
         "first checked CALL refusal leaves capture disabled");
    permit_patch=true;
    need(initialize_tga(module,true,log)&&patches==2,"exact pinned profile retries checked CALL");
    need(a2fo_native11_tga_open_original==numeric+0x240150,"OpenRead continuation seeded");
    Fixture fixture;const auto original=fixture.source;
    restart_before_commit=true;fixture.observe();
    need(!current(fixture.key()),"TGA reconfiguration drops in-flight stream commit");
    fixture.observe();auto retained=current(fixture.key());
    need(retained&&*retained->bytes==original&&retained->source_kind==TextureSourceKind::opened_stream&&
         retained->requested_path==fixture.requested&&retained->resolved_path.empty()&&
         retained->logical_name==fixture.logical,"actual opened bytes and explicit stream provenance");
    need(retire_texture(fixture.key())&&!retire_texture(fixture.key())&&!retire_texture(0)&&
         !current(fixture.key())&&*retained->bytes==original,
         "captured TGA retires once while retained owner remains readable");
    fixture.observe();auto after_retire=current(fixture.key());
    need(after_retire&&after_retire->serial>retained->serial,
         "reused TGA texture pointer receives a new version");
    retire_key=fixture.key();retire_before_commit=true;fixture.observe();
    need(!current(fixture.key())&&*after_retire->bytes==original,
         "retirement during pending TGA commit blocks late publication");
    fixture.observe();need(current(fixture.key())&&current(fixture.key())->serial>after_retire->serial,
         "fresh TGA capture recovers after pending retirement");
    auto restored_after_retire=current(fixture.key());
    fixture.source.assign(original.size(),0);fixture.observe(0,1,1);
    need(current(fixture.key())==restored_after_retire&&*retained->bytes==original,
         "mip level one ignored without retiring base");
    fixture.source=original;fixture.sync_span();fixture.observe(1);
    need(!current(fixture.key()),"failed OpenRead retires previous base");
    auto rejected=[&](const char* label){fixture.observe();need(!current(fixture.key()),label);};
    fixture.observe();need(current(fixture.key())!=nullptr,"base rearmed");
    put(fixture.stream.data(),numeric+0x2bc878);rejected("wrong stream class refused");
    put(fixture.stream.data(),numeric+0x2bc874);fixture.observe();
    put(fixture.stream.data()+0x1c,address(fixture.source.data())+fixture.source.size()+1);
    rejected("invalid cursor refused");fixture.sync_span();fixture.observe();
    put(fixture.stream.data()+0x18,address(fixture.source.data())+64u*1024u*1024u+1);
    rejected("oversize stream refused before allocation");fixture.sync_span();fixture.observe();
    fixture.observe(0,0);need(!current(fixture.key()),"unbuffered mode refused");
    fixture.observe();need(current(fixture.key())!=nullptr,"base restored before owner change");
    install_fake_owner();accept_owner=false;fixture.observe();
    need(!current(fixture.key()),"unknown OpenRead chain invalidates prior binding");
    need(!initialize_tga(module,true,log,owned_open_read),"unknown owner refused");
    accept_owner=true;
    altered(gateway,[&]{need(!initialize_tga(module,true,log,owned_open_read),"wrong gateway refused");});
    need(initialize_tga(module,true,log,owned_open_read)&&patches==2,
         "known RGB owner accepted without repatch");
    fixture.observe();need(current(fixture.key())!=nullptr,"known owner admits stream");
    disable();need(!current(fixture.key())&&collect(output,generation,error)&&output.empty()&&
         *retained->bytes==original,"disable clears registry while retained owner survives");
    if(argc==3)gpu_handoff(argv[2],module);
    need(VirtualFree(gateway,0,MEM_RELEASE)!=0,"gateway freed after observations");
    need(FreeLibrary(module)!=0,"private mapping unloaded");
    std::puts("PASS native11 TGA guarded profile, owned stream, invalidation and optional native handoff");
}

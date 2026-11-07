#include "../core/native11_textures.hpp"
#include "../core/native11_texture_profile.hpp"
#include "../renderer/native11/capture_service.hpp"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace a2fo::native11;

namespace {
unsigned patches=0;
volatile LONG logs=0;
bool fail_second=true;
unsigned commit_mode=0;
HMODULE restart_image=nullptr;
volatile LONG budget_mode=0,budget_arrivals=0;
HANDLE budget_ready=nullptr,budget_release=nullptr;
std::uint8_t* image=nullptr;
void log(const char*) {InterlockedIncrement(&logs);}
void need(bool ok,const char* why) {
    if(!ok){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",why,GetLastError());std::exit(1);}
}
std::uint32_t addr(const void* p) {return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));}
std::uint32_t word(const void* p) {std::uint32_t n=0;std::memcpy(&n,p,4);return n;}
void put(void* p,std::uint32_t n) {std::memcpy(p,&n,4);}
void mapped_word(std::uint8_t* p,std::uint32_t before,std::uint32_t after) {
    need(word(p)==before,"private fixup original");
    DWORD old=0,ignored=0;need(VirtualProtect(p,4,PAGE_READWRITE,&old)!=0,"fixup writable");
    put(p,after);need(VirtualProtect(p,4,old,&ignored)!=0,"fixup protected");
}
template<class Check> void altered(std::uint8_t* p,Check check) {
    DWORD old=0,ignored=0;need(VirtualProtect(p,1,PAGE_READWRITE,&old)!=0,"mutation writable");
    const auto before=*p;*p^=1;check();*p=before;
    need(VirtualProtect(p,1,old,&ignored)!=0,"mutation protected");
}
struct DelphiString {
    std::vector<char> bytes;
    explicit DelphiString(const std::string& text):bytes(text.size()+5,0) {
        put(bytes.data(),static_cast<std::uint32_t>(text.size()));
        std::memcpy(bytes.data()+4,text.data(),text.size());
    }
    const char* data() const {return bytes.data()+4;}
};
struct Fixture {
    std::array<std::uint8_t,0x60> texture{};
    std::array<std::uint8_t,24> frame{};
    std::string logical;
    DelphiString requested;
    DelphiString resolved;
    std::uint32_t output=0;
    explicit Fixture(const std::string& name,const std::string& path):logical(name),
        requested("Textures\\"+name+".dds"),resolved(path) {
        put(texture.data()+8,addr(logical.c_str()));
        put(texture.data()+0x18,0x12345678);
        texture[0x2c]=0;texture[0x58]=0;
        put(frame.data()+4,addr(texture.data()));
        put(frame.data()+16,addr(texture.data()));
        output=addr(resolved.data());
    }
    std::uint32_t key() const {return addr(texture.data());}
    void observe(unsigned route,const char* request=nullptr) {
        a2fo_native11_observe_texture_path(&output,request?request:requested.data(),frame.data()+8,route);
    }
};
Fixture* overlap_fixture=nullptr;
std::string overlap_path;
void write_file(const char* path,const std::vector<std::uint8_t>& bytes) {
    HANDLE file=CreateFileA(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    need(file!=INVALID_HANDLE_VALUE,"temporary DDS open");DWORD written=0;
    need(WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size(),"temporary DDS write");
    need(CloseHandle(file)!=0,"temporary DDS closed before observer");
}
DWORD WINAPI commit_worker(void* input) {
    const auto mode=reinterpret_cast<std::uintptr_t>(input);
    if(mode==1){a2fo::native11_textures::disable();return a2fo::native11_textures::initialize(restart_image,true,log)?0:1;}
    if(mode==3)return overlap_fixture&&a2fo::native11_textures::retire_texture(overlap_fixture->key())?0:1;
    write_file(overlap_path.c_str(),{'D','D','S',' ',42,43,44,45});
    overlap_fixture->observe(1);return 0;
}
DWORD WINAPI observe_worker(void* input) {static_cast<Fixture*>(input)->observe(1);return 0;}
void inflight_budget(HMODULE module,const char* path) {
    std::vector<std::uint8_t> bytes(128);std::memcpy(bytes.data(),"DDS ",4);write_file(path,bytes);
    Fixture first("budget_first",path),second("budget_second",path),third("budget_third",path);
    budget_ready=CreateEventA(nullptr,TRUE,FALSE,nullptr);budget_release=CreateEventA(nullptr,TRUE,FALSE,nullptr);
    need(budget_ready&&budget_release,"budget events");InterlockedExchange(&budget_arrivals,0);InterlockedExchange(&budget_mode,1);
    HANDLE workers[2]{CreateThread(nullptr,0,observe_worker,&first,0,nullptr),CreateThread(nullptr,0,observe_worker,&second,0,nullptr)};
    need(workers[0]&&workers[1]&&WaitForSingleObject(budget_ready,10000)==WAIT_OBJECT_0,"two captures hold fixture in-flight budget");
    third.observe(1);need(!a2fo::native11_textures::current(third.key()),"aggregate in-flight cap rejects third payload before allocation");
    SetEvent(budget_release);need(WaitForMultipleObjects(2,workers,TRUE,10000)==WAIT_OBJECT_0,"budget captures complete");
    InterlockedExchange(&budget_mode,0);for(HANDLE worker:workers)CloseHandle(worker);CloseHandle(budget_ready);CloseHandle(budget_release);
    need(a2fo::native11_textures::current(first.key())&&a2fo::native11_textures::current(second.key()),"admitted payloads publish");
    third.observe(1);need(a2fo::native11_textures::current(third.key())!=nullptr,"in-flight capacity released after commit");
    need(a2fo::native11_textures::initialize(module,true,log),"clear budget fixtures without repatching");
}
std::vector<std::uint8_t> dds_red() {
    // DX10 BGRA8, 1x1, opaque red; same header layout as native11_texture_native.cpp.
    std::vector<std::uint8_t> b(152);std::memcpy(b.data(),"DDS ",4);
    put(b.data()+4,124);put(b.data()+8,0x1007);put(b.data()+12,1);put(b.data()+16,1);
    put(b.data()+28,1);put(b.data()+76,32);put(b.data()+80,4);
    std::memcpy(b.data()+84,"DX10",4);put(b.data()+108,0x1000);
    put(b.data()+128,87);put(b.data()+132,3);put(b.data()+140,1);
    b[150]=255;b[151]=255;return b;
}
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
struct Host {const BoundCapture* snapshot;unsigned reads=0;};
std::int32_t A2FO_N11_CALL read_bound(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t maximum,A2FO_N11_BytesSink sink,void* context) {
    auto& host=*static_cast<Host*>(raw);++host.reads;
    return BoundCapture::read_asset(const_cast<BoundCapture*>(host.snapshot),kind,id,length,maximum,sink,context);
}
void gpu_handoff(const char* helper,HMODULE private_image) {
    using namespace a2fo::native11_textures;
    need(initialize(private_image,true,log),"DDS observer rearmed for native handoff");
    char directory[MAX_PATH]{},path[MAX_PATH]{};
    need(GetTempPathA(MAX_PATH,directory)>0&&GetTempFileNameA(directory,"n11",0,path)!=0,"native DDS path");
    const auto red=dds_red();write_file(path,red);
    Fixture texture("hull",path);texture.observe(1);
    std::vector<SnapshotTexture> textures;std::uint64_t generation=0;std::string error;
    need(collect(textures,generation,error)&&textures.size()==1&&textures[0].identifier=="hull"&&
         *textures[0].bytes==red,"observer provides exact owned DDS snapshot");
    ModelAssetStore store;OpenedFile opened;opened.name="test.sod";opened.bytes=model("hull");
    need(store.publish(1,0, std::move(opened)),"synthetic textured SOD stored");
    ModelAssetInfo info{};need(store.info(1,info),"SOD version available");
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=1;frame->object_count=1;
    for(unsigned i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=128;
    frame->objects[0].database=1;frame->objects[0].model_serial=info.serial;
    frame->objects[0].model_identity=ModelIdentity::plain_database_name;
    frame->objects[0].placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(frame->objects[0].model_name,"test.sod");
    std::shared_ptr<const BoundCapture> bound;
    need(build_bound_capture(*frame,generation,resolve_model,&store,textures,bound,error)&&
         bound->models().size()==1,"captured DDS and SOD bind to retained owner");
    store.clear();frame.reset();textures.clear();disable();
    const auto retained_model=bound->model_asset(std::string(bound->frame().objects[0].model_name));
    need(DeleteFileA(path)!=0&&retained_model&&!retained_model->bytes.empty()&&
         retained_model->bytes[0]==static_cast<std::uint8_t>('S'),
         "source stores cleared and file deleted before submit");
    HMODULE dll=LoadLibraryA(helper);need(dll!=nullptr,"native renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"public versioned export");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API2 api{};need(get(2,sizeof(api),&api)>=0&&api.base.version==2&&api.base.struct_size==sizeof(api),"public C API v2");
    Host host{bound.get()};A2FO_N11_Host config{sizeof(config),0,128,128,&host,read_bound,nullptr};
    A2FO_N11_Session session=0;need(api.base.create(&config,&session)>=0&&session,"native offscreen session");
    const float ambient[3]{1,1,1};
    need(submit_captured(api,session,bound->frame(),bound->source_epoch(),nullptr,0,ambient)>=0&&host.reads>=2,
         "retained model and DDS submitted through public API");
    A2FO_N11_Readback readback{};readback.struct_size=sizeof(readback);
    need(api.base.readback(session,&readback)==S_FALSE&&readback.width==128&&readback.height==128&&
         readback.required_bytes==128*128*4,"native readback metadata");
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(readback.required_bytes));
    readback.bgra=pixels.data();readback.capacity=pixels.size();
    need(api.base.readback(session,&readback)>=0,"native readback bytes");
    const auto* center=pixels.data()+(64*128+64)*4;
    need(center[2]>250&&center[1]<4&&center[0]<4,"captured DDS renders red center pixel");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
         !GetModuleHandleA("D3DX81ab.dll"),"no legacy graphics imports resolved");
    need(api.base.destroy(session)>=0&&FreeLibrary(dll)!=0,"native session destroyed before DLL unload");
    std::puts("PASS native11 DDS resolved file -> retained snapshot -> native DLL red pixel; sources removed before submit");
}
}
extern "C" void a2fo_native11_texture_before_commit() noexcept {
    if(InterlockedCompareExchange(&budget_mode,0,0)){
        if(InterlockedIncrement(&budget_arrivals)==2)SetEvent(budget_ready);
        need(WaitForSingleObject(budget_release,10000)==WAIT_OBJECT_0,"held file payload release");return;
    }
    if(!commit_mode)return;
    const auto mode=commit_mode;commit_mode=0;
    HANDLE thread=CreateThread(nullptr,0,commit_worker,reinterpret_cast<void*>(mode),0,nullptr);
    need(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"DDS interleaving worker completed");
    DWORD result=1;need(GetExitCodeThread(thread,&result)&&result==0,"DDS interleaving worker success");CloseHandle(thread);
}
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* target,const std::uint8_t* expected,std::size_t length) {
    const unsigned index=++patches;
    const std::uint8_t first[]{0xe8,0x7f,0x9a,0xf1,0xff},second[]{0xe8,0xed,0x97,0xf1,0xff};
    const auto* signature=index==1?first:second;
    const auto rva=index==1?0x1efec0u:0x1f0152u;
    const auto tap=index==1?&a2fo_native11_texture_parameters_tap:&a2fo_native11_texture_pixels_tap;
    need(image&&site==image+rva&&target==reinterpret_cast<void*>(tap)&&length==5&&
         expected&&std::memcmp(expected,signature,5)==0&&std::memcmp(site,signature,5)==0,
         "checked DDS patch site, target and original bytes");
    if(index==2&&fail_second){fail_second=false;return false;}
    std::uint8_t call[5]{0xe8};const auto relative=addr(target)-(addr(site)+5);
    put(call+1,relative);DWORD old=0,ignored=0;
    need(VirtualProtect(site,5,PAGE_READWRITE,&old)!=0,"private call writable");
    std::memcpy(site,call,5);need(VirtualProtect(site,5,old,&ignored)!=0,"private call protected");
    return true;
}
int main(int argc,char** argv) {
    using namespace a2fo::native11_textures;
    std::vector<a2fo::native11::SnapshotTexture> output;std::uint64_t generation=0;std::string error;
    need(!initialize(GetModuleHandleA(nullptr),false,log)&&!current(1)&&patches==0,"default off");
    need(collect(output,generation,error)&&output.empty(),"default off collect");
    need(!initialize(GetModuleHandleA(nullptr),true,log)&&patches==0,"foreign image refuses before patch");
    if(argc==2||argc==3) {
        HMODULE module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        need(module!=nullptr,"static private pinned FleetOpsHook mapping");image=reinterpret_cast<std::uint8_t*>(module);
        namespace p=a2fo::native11_textures::profile;
        const auto delta=addr(image)-p::preferred_base;
        const auto first=p::regions.front();
        if(delta&&word(image+first.rva+first.relocations[0])==word(first.bytes+first.relocations[0])) {
            need(!initialize(module,true,log)&&patches==0,"unrelocated private image refused");
            for(const auto& region:p::regions) {
                need(std::memcmp(image+region.rva,region.bytes,region.size)==0,"private profile preferred bytes");
                for(std::size_t i=0;i<region.relocation_count;++i) {
                    const auto off=region.relocations[i];const auto value=word(region.bytes+off);
                    mapped_word(image+region.rva+off,value,value+delta);
                }
            }
        } else {
            for(const auto& region:p::regions)
                for(std::size_t i=0;i<region.relocation_count;++i) {
                    const auto off=region.relocations[i];
                    need(word(image+region.rva+off)==word(region.bytes+off)+delta,"loader-relocated HIGHLOW");
                }
        }
        for(const auto& region:p::regions) {
            for(const auto off:{std::size_t{0},region.size-1})altered(image+region.rva+off,[&]{
                need(!initialize(module,true,log)&&patches==0,"profile boundary mutation refused");
            });
            for(std::size_t i=0;i<region.relocation_count;++i)altered(image+region.rva+region.relocations[i],[&]{
                need(!initialize(module,true,log)&&patches==0,"normalized HIGHLOW mutation refused");
            });
        }
        need(!initialize(module,true,log)&&patches==2&&
             a2fo_native11_texture_resolver_original==addr(image)+0x109944&&
             !current(1),"first patch owned and second refused; disabled with continuation seeded");
        need(initialize(module,true,log)&&patches==3,"retry recognizes owned first call and installs second");
        restart_image=module;
        char temp_dir[MAX_PATH]{},temp_file[MAX_PATH]{};
        need(GetTempPathA(MAX_PATH,temp_dir)>0&&GetTempFileNameA(temp_dir,"n11",0,temp_file)!=0,"temporary file path");
        inflight_budget(module,temp_file);
        const std::vector<std::uint8_t> original{'D','D','S',' ',1,2,3,4};write_file(temp_file,original);
        Fixture one("guard_one",temp_file),two("guard_two",temp_file);
        commit_mode=1;one.observe(1);
        need(!current(one.key()),"disable and reenable retires in-flight DDS commit");
        one.observe(1);need(current(one.key())!=nullptr,"DDS observer recovers after restart");
        overlap_fixture=&one;overlap_path=temp_file;commit_mode=2;one.observe(1);
        need(current(one.key())&&*current(one.key())->bytes==std::vector<std::uint8_t>({'D','D','S',' ',42,43,44,45}),
             "newer overlapping observation wins over older DDS bytes");
        write_file(temp_file,original);
        one.observe(1);two.observe(2);
        auto retained=current(one.key());auto second=current(two.key());
        need(!initialize_tga(GetModuleHandleA(nullptr),false,log)&&current(one.key())==retained&&current(two.key())==second,
             "disabled TGA configuration preserves captured DDS records");
        need(!initialize_tga(GetModuleHandleA(nullptr),true,log)&&current(one.key())==retained&&current(two.key())==second,
             "failed TGA admission preserves captured DDS records");
        need(retained&&second&&retained->flags==0x12345678&&!retained->colour_key&&
             *retained->bytes==original&&second->logical_name=="guard_two","both routes own DDS bytes and metadata");
        one.texture[0x18]=0;write_file(temp_file,{'D','D','S',' ',9,9,9,9});
        need(*retained->bytes==original,"retained bytes independent of source and object mutation");
        need(collect(output,generation,error)&&output.size()==2&&output[0].identifier=="guard_one"&&
             output[1].identifier=="guard_two"&&*output[0].bytes==original,"collect unique exact logical identifiers");
        need(retire_texture(one.key())&&!retire_texture(one.key())&&!retire_texture(0)&&
             !current(one.key())&&current(two.key())==second&&*retained->bytes==original,
             "captured DDS retires once while held owner and other texture survive");
        one.observe(1);auto after_retire=current(one.key());
        need(after_retire&&after_retire->serial>retained->serial&&after_retire!=retained,
             "reused DDS texture pointer receives a fresh version");
        const auto after_retire_bytes=*after_retire->bytes;
        overlap_fixture=&one;commit_mode=3;one.observe(1);
        need(!current(one.key())&&*after_retire->bytes==after_retire_bytes,
             "retirement during pending DDS commit blocks late publication");
        one.observe(1);need(current(one.key())&&current(one.key())->serial>after_retire->serial,
             "fresh DDS capture recovers after pending retirement");
        one.observe(1,"bad");need(!current(one.key())&&current(two.key()),"bad request retires prior only");
        one.observe(1);need(current(one.key())!=nullptr,"valid replacement after invalidation");
        one.output=0;one.observe(1);need(!current(one.key()),"nil result retires prior");
        one.output=addr(one.resolved.data());put(one.resolved.bytes.data(),32768);one.observe(1);
        need(!current(one.key()),"bad Delphi length rejected");put(one.resolved.bytes.data(),static_cast<std::uint32_t>(std::strlen(temp_file)));
        need(DeleteFileA(temp_file)!=0,"delete temporary source");one.observe(1);need(!current(one.key()),"missing resolved file rejected");
        write_file(temp_file,original);one.texture[0x2c]=1;one.observe(1);
        need(current(one.key())&&current(one.key())->colour_key,"colour key metadata retained");
        output.clear();need(!collect(output,generation,error)&&output.empty()&&!error.empty(),"colour key collect refused");
        one.texture[0x2c]=0;put(one.texture.data()+0x18,4);one.observe(1);
        need(current(one.key())&&!collect(output,generation,error),"surface metadata retained; collect refused");
        one.texture[0x58]=1;one.observe(1);need(!current(one.key()),"async source rejected");
        auto survivor=current(two.key());disable();need(!current(two.key())&&collect(output,generation,error)&&output.empty()&&
             *survivor->bytes==original,"disable clears registry and retained owner survives");
        need(initialize(module,true,log)&&patches==3,"reenable validates both owned calls without repatch");
        altered(image+0x1efec0,[&]{need(!initialize(module,true,log)&&patches==3,"mutated owned call rejected");});
        need(!current(two.key()),"ownership failure stays disabled");
        if(argc==3)gpu_handoff(argv[2],module);
        disable();need(DeleteFileA(temp_file)!=0,"temporary DDS cleanup");
        need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll"),"static mapping resolved no graphics imports");
        need(FreeLibrary(module)!=0,"private image unload");
        std::puts("PASS native11 DDS private profile, retry, owned observations and invalidation");
    } else need(argc==1,"usage: native11_texture_guard_test.exe [PRIVATE_PINNED_FLEETOPSHOOK_DLL [NATIVE11_RENDERER_DLL]]");
    std::puts("PASS native11 DDS default-off guard");
}

// Console-only: data-map images and replay checked leaf getters on synthetic
// objects. Name/destructor/status callbacks are fixture stubs, not live game state.
#include <windows.h>
HMODULE WINAPI fixture_module(LPCSTR);
#define GetModuleHandleA fixture_module
#include "../modules/A2FORefitMenus/loading_native.cpp"
#undef GetModuleHandleA
HMODULE WINAPI fixture_module(LPCSTR name){
    if(name&&!lstrcmpiA(name,"msvcp60.dll"))return GetModuleHandleA(nullptr);
    return GetModuleHandleA(name);
}
#include <cstdio>
#include <stdexcept>

namespace fixture_observations {
unsigned scene_begins{},scene_ends{},splash_begins{},splash_ends{};
bool begin_after_native{},kill_after_cleanup{};
bool continuing_launch{};
bool accept_setup{};unsigned accepted_setup_calls{};bool accepted_before_native{};
unsigned handover_ends{};
}
namespace a2fo::refit_menu::setup_native {
bool begin_loading_handover(){
    ++fixture_observations::accepted_setup_calls;
    fixture_observations::accepted_before_native=fixture_observations::splash_begins==0;
    return fixture_observations::continuing_launch=fixture_observations::accept_setup;
}
}
namespace a2fo::refit_menu::menu_scene {
void end_handover(){++fixture_observations::handover_ends;fixture_observations::continuing_launch=false;}
bool launch_handover_active(){return fixture_observations::continuing_launch;}
std::shared_ptr<const Artwork> retained_frame(){return {};}
std::shared_ptr<const Artwork> loading_frame(int,int,bool){return {};}
void loading_presented(const std::shared_ptr<const Artwork>&){}
bool requested(){return false;}
bool departing(){return false;}
bool begin_departure(const char*){
    ++fixture_observations::scene_begins;
    fixture_observations::begin_after_native=fixture_observations::splash_begins==1;
    return true;
}
void end_departure(){++fixture_observations::scene_ends;}
std::uint64_t menu_epoch(){return 0;}
}
namespace {
using namespace a2fo::refit_menu::loading_native;
namespace loading_bindings=a2fo::refit_menu::loading_bindings;
unsigned calls{},installs{},asset_patches{},release_calls{};
unsigned jump_patches{};
bool refuse_jump{};
void* observed{};
void* release_self{};
bool release_after_cleanup{};
std::uint32_t first_bits{},progress_bits{};
bool refuse{};unsigned refuse_install{};
bool refuse_asset{};
unsigned destroyed{},names{},statuses{},status_patches{};
const char* observed_status{};
const char* player_names[]={"Native Player","Native AI"};
void* __fastcall name_getter(void*,void*,void* result,int slot){
    ++names;
    auto* p=static_cast<std::uint32_t*>(result);
    p[0]=0;p[1]=reinterpret_cast<std::uint32_t>(player_names[slot]);
    p[2]=p[3]=static_cast<std::uint32_t>(std::strlen(player_names[slot]));return result;
}
void __fastcall destroy_string(void*,void*){++destroyed;}
void __cdecl status_update(const char* text){++statuses;observed_status=text;}
void mutate(void* target,const void* bytes,std::size_t length){
    DWORD old{};
    if(!VirtualProtect(target,length,PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("fixture write protection");
    std::memcpy(target,bytes,length);DWORD unused{};VirtualProtect(target,length,old,&unused);
    FlushInstructionCache(GetCurrentProcess(),target,length);
}
template<class T>void put(void* target,const T& value){mutate(target,&value,sizeof(value));}
void jump(void* target,void* replacement){
    BYTE bytes[5]{0xe9};const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(replacement)-reinterpret_cast<std::uintptr_t>(target)-5);
    std::memcpy(bytes+1,&delta,4);mutate(target,bytes,5);
}
void require(bool ok,const char* what);
bool A2FO_CALL patch_status(void* target,void*,const std::uint8_t* expected,std::size_t length){
    if(length!=5||std::memcmp(target,expected,length))throw std::runtime_error("checked observer CALL");
    if(target==at(engine,loading_bindings::status_site))++status_patches;
    else {
        if(refuse_asset&&target==at(engine,loading_bindings::load_db_sites[1]))return false;
        ++asset_patches;
    }
    return true;
}
bool A2FO_CALL patch_jump(void* target,void*,const std::uint8_t* expected,std::size_t length){
    require(target==at(engine,loading_bindings::release_device),"checked release transfer site");
    require(length==6&&std::memcmp(target,expected,length)==0,"checked release transfer bytes");
    ++jump_patches;
    return !refuse_jump;
}
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void __stdcall original(float first,void* device,float progress){
    ++calls;observed=device;std::memcpy(&first_bits,&first,4);std::memcpy(&progress_bits,&progress,4);
}
void __cdecl splash_begin_original(){++fixture_observations::splash_begins;}
void __cdecl splash_end_original(){
    ++fixture_observations::splash_ends;
    fixture_observations::kill_after_cleanup=!loading_session&&fixture_observations::scene_ends>=2;
}
void __fastcall release_original(void* self,void*){
    ++release_calls;release_self=self;
    release_after_cleanup=!native_frame_seen&&!loading_wrapper;
}
unsigned db_calls{},pixels_calls{};
void *db_self{},*db_name{},*db_path{};
float db_scale{};
void* __fastcall db_stub(void* self,void*,void* name,void* path,float scale){
    ++db_calls;db_self=self;db_name=name;db_path=path;db_scale=scale;
    return reinterpret_cast<void*>(0x12340000);
}
void* pixels_self{};int pixels_level{};
unsigned __fastcall pixels_stub(void* self,void*,int level){
    ++pixels_calls;pixels_self=self;pixels_level=level;return 0x76543210u;
}
bool A2FO_CALL install(void* target,void*,std::size_t length,const std::uint8_t* expected,A2FO_InlineHook* out){
    ++installs;
    require(!std::memcmp(target,expected,length),"exact original prologue");
    if(target==at(fo,loading_bindings::new_bar))require(length==6,"bar prologue length");
    else if(target==at(engine,loading_bindings::kill_splash))require(length==7,"kill splash prologue length");
    else if(target==at(engine,loading_bindings::show_splash))require(length==5,"show splash prologue length");
    else if(target==at(engine,loading_bindings::release_device))require(length==9,"release device prologue length");
    else throw std::runtime_error("unexpected inline hook site");
    if(refuse||installs==refuse_install)return false;
    void* gateway=target==at(fo,loading_bindings::new_bar)?reinterpret_cast<void*>(&original):
                  target==at(engine,loading_bindings::kill_splash)?reinterpret_cast<void*>(&splash_end_original):
                  target==at(engine,loading_bindings::show_splash)?reinterpret_cast<void*>(&splash_begin_original):
                  reinterpret_cast<void*>(&release_original);
    *out={target,gateway,length};return true;
}
void rebase_animation(HMODULE armada){
    namespace b=loading_bindings;
    const auto absolute=[&](std::uint32_t site,std::size_t offset,std::uint32_t value){
        const auto address=reinterpret_cast<std::uint32_t>(at(armada,value));
        put(static_cast<BYTE*>(at(armada,site))+offset,address);
    };
    // The data-mapped EXE keeps original absolute operands; rebase only the
    // four operands inspected by the production animation admission checks.
    BYTE kill[7]{},visible[6]{},fraction[13]{};
    require(read(at(armada,b::kill_splash),kill,sizeof(kill))&&kill[0]==0x8b&&kill[1]==0x0d&&kill[6]==0x53,
            "native kill splash opcode shape");
    require(read(at(armada,b::kill_splash+11),visible,sizeof(visible))&&visible[0]==0x88&&visible[1]==0x1d,
            "native visibility opcode shape");
    require(read(at(armada,b::compute_fraction),fraction,sizeof(fraction))&&
            fraction[0]==0xd9&&fraction[1]==0x05&&fraction[6]==0xd8&&fraction[7]==0x25&&fraction[12]==0xc3,
            "native fraction opcode shape");
    absolute(b::kill_splash,2,b::splash_model);
    absolute(b::kill_splash+11,2,b::status_visible);
    absolute(b::compute_fraction,2,b::float_one);
    absolute(b::compute_fraction,8,b::fraction_remaining);
}
void* executable_section(HMODULE image);
void fo_release_fixture(HMODULE armada,HMODULE fleetops,bool core,bool foreign){
    namespace b=loading_bindings;
    std::array<BYTE,6> transfer{{0x68,0,0,0,0,0xc3}};
    const auto target=reinterpret_cast<std::uint32_t>(at(fleetops,b::fo_release_device));
    std::memcpy(transfer.data()+1,&target,4);
    mutate(at(armada,b::release_device),transfer.data(),transfer.size());
    std::array<BYTE,12> tail{{0x8b,0x4d,0xfc,0xff,0x15,0,0,0,0,0x59,0x5d,0xc3}};
    const auto gateway=reinterpret_cast<std::uint32_t>(at(fleetops,b::fo_release_gateway));
    std::memcpy(tail.data()+5,&gateway,4);
    mutate(at(fleetops,b::fo_release_device+7),tail.data(),tail.size());
    const BYTE entry[]{0x55,0x8b,0xec,0x51,0x89,0x4d,0xfc};
    mutate(at(fleetops,b::fo_release_device),entry,sizeof(entry));
    if(core||foreign){
        HMODULE owner=foreign?fleetops:LoadLibraryExA(
            "Z:\\home\\tamsynn\\NVMeData\\Fleet Ops Roots\\Data\\A2FOExtensions.dll",
            nullptr,DONT_RESOLVE_DLL_REFERENCES);
        require(owner,"core release owner data mapping");
        if(!foreign)require(GetModuleHandleA("A2FOExtensions.dll")==owner,"core release owner registered");
        jump(at(fleetops,b::fo_release_device),executable_section(owner));
    }
}
void fo_pixels_fixture(HMODULE armada,HMODULE fleetops,bool foreign){
    namespace b=loading_bindings;
    const auto site=reinterpret_cast<std::uintptr_t>(at(armada,b::load_pixels_site));
    const auto target=reinterpret_cast<std::uintptr_t>(at(fleetops,b::fo_load_pixels));
    std::array<BYTE,5> call{{0xe8,0,0,0,0}};
    const auto delta=static_cast<std::int32_t>(target-site-5);
    std::memcpy(call.data()+1,&delta,4);
    mutate(at(armada,b::load_pixels_site),call.data(),call.size());
    mutate(at(fleetops,b::fo_load_pixels),b::fo_pixels_body.data(),b::fo_pixels_body.size());
    if(foreign){const BYTE bad=0x90;mutate(at(fleetops,b::fo_load_pixels),&bad,1);}
}
void* executable_section(HMODULE image){
    auto* bytes=reinterpret_cast<BYTE*>(image);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    require(dos->e_magic==IMAGE_DOS_SIGNATURE,"known module DOS header");
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(bytes+dos->e_lfanew);
    require(nt->Signature==IMAGE_NT_SIGNATURE,"known module NT header");
    auto* section=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
        if((section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)&&section[i].Misc.VirtualSize>16)
            return bytes+section[i].VirtualAddress;
    throw std::runtime_error("known module executable section");
}
const char* A2FO_CALL root(){return ".";}
void A2FO_CALL logger(const char*,const char* message){std::puts(message);}
}
int main(int argc,char** argv){
    if(argc!=4){std::fprintf(stderr,"usage: test ARMADAL FO success|refuse|changed|players-changed|status-changed|animation|animation-show-changed|animation-kill-changed|animation-fraction-changed|animation-status-changed|animation-kill-refuse|animation-show-refuse|animation-inner-changed|animation-inner-refuse|animation-cleanup-changed|animation-cleanup-refuse|animation-fo|animation-fo-core|animation-fo-release-refuse|animation-fo-foreign|animation-fo-pixels-foreign|known-race-detour|foreign-race-detour|core-race-detour|core-foreign-race-detour|core-race-tail-changed\n");return 2;}
    using namespace a2fo::refit_menu;
    using namespace a2fo::refit_menu::loading_native;
    try{
        HMODULE armada=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        HMODULE fleetops=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        require(armada&&fleetops,"binary data mappings");
        A2FO_ModuleApi api{};api.api_version=A2FO_MODULE_API_VERSION;api.struct_size=sizeof(api);
        api.log=&logger;api.root_directory=&root;api.install_inline_hook=&install;api.patch_call=&patch_status;api.patch_jump=&patch_jump;
        A2FO_MenuMissionV1 metadata{};metadata.size=sizeof(metadata);metadata.version=1;
        std::strcpy(metadata.map,"custom_campaign42.bzn");std::strcpy(metadata.title,"Custom Campaign 42");
        require(set_mission(&metadata),"bounded mission handoff accepted");
        metadata.title[0]='X';require(mission_metadata.title[0]=='C',"mission handoff copied without borrowed pointers");
        clear_mission();require(!mission_metadata.size,"rejected launch clears pending handoff");
        metadata.map[0]=0;require(!set_mission(&metadata),"empty mission map rejected");
        std::memset(metadata.map,'x',sizeof(metadata.map));require(!set_mission(&metadata),"unterminated mission map rejected");
        const std::string mode=argv[3];
        refuse=mode=="refuse";
        refuse_install=mode=="animation-kill-refuse"?2u:mode=="animation-show-refuse"?3u:
                       mode=="animation-cleanup-refuse"?4u:0u;
        refuse_asset=mode=="animation-inner-refuse";
        const bool changed=!std::strcmp(argv[3],"changed");
        const bool players_changed=!std::strcmp(argv[3],"players-changed");
        const bool status_changed=mode=="status-changed"||mode=="animation-status-changed";
        const bool animation=mode.rfind("animation",0)==0;
        const bool fo_case=mode.rfind("animation-fo",0)==0;
        const bool fo_core=mode=="animation-fo-core";
        const bool fo_foreign=mode=="animation-fo-foreign";
        const bool fo_pixels_foreign=mode=="animation-fo-pixels-foreign";
        refuse_jump=mode=="animation-fo-release-refuse";
        const bool known_detour=mode=="known-race-detour",foreign_detour=mode=="foreign-race-detour";
        const bool core_race=mode=="core-race-detour"||mode=="core-foreign-race-detour"||mode=="core-race-tail-changed";
        if(players_changed){const BYTE bad=0x90;mutate(at(armada,loading_bindings::player_team),&bad,1);}
        if(status_changed){const BYTE bad=0x90;mutate(at(armada,loading_bindings::status_site),&bad,1);}
        if(animation){
            rebase_animation(armada);
            if(fo_case){
                fo_release_fixture(armada,fleetops,fo_core,fo_foreign);
                fo_pixels_fixture(armada,fleetops,fo_pixels_foreign);
            }
            if(mode=="animation-show-changed"){
                const BYTE bad=0x90;mutate(at(armada,loading_bindings::show_splash),&bad,1);
            }else if(mode=="animation-kill-changed"){
                const BYTE bad=0x90;mutate(at(armada,loading_bindings::kill_splash),&bad,1);
            }else if(mode=="animation-fraction-changed"){
                const BYTE bad=0x90;mutate(at(armada,loading_bindings::compute_fraction),&bad,1);
            }else if(mode=="animation-inner-changed"){
                const BYTE bad=0x90;mutate(at(armada,loading_bindings::load_db_sites[1]),&bad,1);
            }else if(mode=="animation-cleanup-changed"){
                const BYTE bad=0x90;mutate(at(armada,loading_bindings::release_device+0xaf),&bad,1);
            }
        }
        if(known_detour||foreign_detour){
            HMODULE known=LoadLibraryExA("build/modules/A2FOInstantActionSettings.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);
            require(known&&GetModuleHandleA("A2FOInstantActionSettings.dll")==known,"known race owner mapping");
            jump(at(armada,loading_bindings::find_race),executable_section(known_detour?known:fleetops));
        }
        if(core_race){
            HMODULE core=LoadLibraryExA("build/A2FOExtensions.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);
            require(core&&GetModuleHandleA("A2FOExtensions.dll")==core,"core race owner data mapping");
            void* exported=reinterpret_cast<void*>(GetProcAddress(core,"A2FOBuiltinInstantActionSettings_RaceById"));
            require(exported,"core race detour export");
            if(mode=="core-foreign-race-detour"){
                void* other=executable_section(core);
                require(other!=exported,"core foreign detour differs from exported race function");
                jump(at(armada,loading_bindings::find_race),other);
            }else{
                jump(at(armada,loading_bindings::find_race),exported);
                if(mode=="core-race-tail-changed"){
                    const BYTE bad=0x90;mutate(at(armada,loading_bindings::find_race+5),&bad,1);
                }
            }
        }
        if(changed){
            auto* p=reinterpret_cast<BYTE*>(fleetops)+loading_bindings::new_bar;
            DWORD old{};require(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&old),"fixture protection");
            *p^=1;DWORD unused{};VirtualProtect(p,1,old,&unused);
        }
        const bool ok=initialize(&api,armada,fleetops,animation);
        require(ok==(!refuse&&!changed),"checked admission/failure");
        if(ok){
            if(animation){
                require(status_patches==(status_changed?0u:1u),"animation status admission");
                const bool admitted=mode=="animation"||mode=="animation-inner-changed"||
                                    mode=="animation-inner-refuse"||mode=="animation-cleanup-changed"||
                                    mode=="animation-cleanup-refuse"||fo_case;
                const bool cached=admitted&&mode!="animation-cleanup-changed"&&mode!="animation-cleanup-refuse"&&
                                  !fo_foreign&&!refuse_jump;
                require(animation_armed.load()==admitted,"animation needs both checked lifetime hooks");
                require(cache_armed.load()==cached,"cache needs checked device teardown observer");
                require(jump_patches==(fo_case&&!fo_foreign?1u:0u),"FO release transfer checked patch count");
                require(next_release.load()==(fo_case&&cached?at(fleetops,loading_bindings::fo_release_device):
                                             cached?release_hook.gateway:nullptr),"release forwarding target follows admitted chain");
                require(next_pixels.load()==(fo_case&&!fo_pixels_foreign?at(fleetops,loading_bindings::fo_load_pixels):
                                            admitted&&!fo_case?at(armada,loading_bindings::load_pixels):nullptr),
                        "pixel forwarding target follows admitted chain");
                require(installs==(mode=="animation-kill-refuse"?2u:
                                   mode=="animation-show-refuse"?3u:
                                   mode=="animation-cleanup-changed"?3u:
                                   admitted?(fo_case?3u:4u):1u),"animation inline hook admission count");
                require(asset_patches==(admitted?(mode=="animation-inner-changed"||mode=="animation-inner-refuse"||fo_pixels_foreign?3u:4u):0u),
                        "inner asset sites independently checked after lifetime admission");
                if(admitted){
                    a2fo_refit_splash_begin();
                    require(fixture_observations::splash_begins==1&&loading_session&&
                            fixture_observations::scene_begins==0&&fixture_observations::scene_ends==1&&departure_pending,
                            "show splash forwards once and queues departure for first draw");
                    if(cached){
                        if(fo_case)jump(at(fleetops,loading_bindings::fo_release_device),reinterpret_cast<void*>(&release_original));
                        native_frame_seen=true;loading_wrapper=reinterpret_cast<void*>(0x8877);
                        auto* self=reinterpret_cast<void*>(0x6677);
                        a2fo_refit_release_device(self,nullptr);
                        require(release_calls==1&&release_self==self&&release_after_cleanup&&loading_session,
                                "native device release clears loading state before forwarding once");
                        native_frame_seen=true;loading_wrapper=reinterpret_cast<void*>(0x8877);
                        a2fo_refit_release_device(self,nullptr);
                        require(release_calls==2&&release_after_cleanup,"repeated native cleanup forwards once per entry");
                    }
                    a2fo_refit_splash_end();
                    require(fixture_observations::splash_ends==1&&!loading_session&&
                            fixture_observations::scene_ends==2&&!departure_pending&&fixture_observations::kill_after_cleanup,
                            "kill splash forwards once after session cleanup");
                    jump(at(armada,loading_bindings::load_db),reinterpret_cast<void*>(&db_stub));
                    jump(at(fo_case&&!fo_pixels_foreign?fleetops:armada,
                            fo_case&&!fo_pixels_foreign?loading_bindings::fo_load_pixels:loading_bindings::load_pixels),
                         reinterpret_cast<void*>(&pixels_stub));
                    void* self=reinterpret_cast<void*>(0x4444),*name=reinterpret_cast<void*>(0x5555),
                        *path=reinterpret_cast<void*>(0x6666);
                    require(a2fo_refit_loaded_db(self,nullptr,name,path,.625f)==reinterpret_cast<void*>(0x12340000)&&
                            db_calls==1&&db_self==self&&db_name==name&&db_path==path&&db_scale==.625f,
                            "database observer forwards exact fastcall arguments and result once");
                    require(a2fo_refit_loaded_pixels(self,nullptr,7)==0x76543210u&&pixels_calls==1&&
                            pixels_self==self&&pixels_level==7,
                            "pixel observer forwards exact fastcall arguments and result once");
                    fixture_observations::continuing_launch=true;
                    const auto scene_ends_before=fixture_observations::scene_ends;
                    a2fo_refit_splash_begin();
                    require(loading_session&&!departure_pending&&fixture_observations::scene_ends==scene_ends_before,
                            "native splash restarted a continuing IA Launch camera");
                    fixture_observations::continuing_launch=false;
                    const auto handover_ends_before=fixture_observations::handover_ends;
                    a2fo_refit_splash_end();
                    require(fixture_observations::handover_ends==handover_ends_before+1,
                            "splash exit did not clear a handover before native cleanup");
                }else if(mode=="animation-show-refuse"){
                    a2fo_refit_splash_end();
                    require(fixture_observations::splash_ends==1&&!loading_session,
                            "partial kill hook forwards once without opening session");
                }
                a2fo_refit_loading_bar(0,reinterpret_cast<void*>(1),.5f);
                require(calls==1,"animation fallback preserves native bar forwarding");
                shutdown();std::puts("PASS: checked animation lifetime admission/fallback and native forwarding");return 0;
            }
            if(known_detour||foreign_detour||core_race){
                const bool accepted=known_detour||mode=="core-race-detour";
                require(players_ready==accepted,"known race owner accepted; foreign or changed race detour rejected");
                a2fo_refit_loading_bar(0,reinterpret_cast<void*>(1),.5f);
                require(calls==1,"detoured race binding still forwards native bar once");
                shutdown();std::puts("PASS: race detour owner admission and native forwarding");return 0;
            }
            if(players_changed||status_changed){
                require(players_ready==!players_changed,"changed player getter disables panel only");
                require(status_patches==(status_changed?0u:1u),"changed status site is never patched");
                if(players_changed){LoadingModel empty;populate_players(empty);require(empty.players.empty(),"no player getters called after failed admission");}
                a2fo_refit_loading_bar(0,reinterpret_cast<void*>(1),.5f);
                require(calls==1,"optional binding failure still forwards native bar");
                shutdown();std::puts("PASS: optional player/status binding failure remains native fallback");return 0;
            }
            require(players_ready,"new player signatures match installed binary");
            require(status_patches==1,"status observer admitted");
            // Replay the verified read-only leaf getters, with bounded synthetic
            // GameSetup/Transport/Race objects. Native string calls are stubs.
            std::array<BYTE,0xa0> setup{};std::array<BYTE,0x338> data{};
            std::array<BYTE,0x300> transport{};std::array<void*,0x60/4> table{};
            std::array<BYTE,0x450> race0{},race1{};
            void* gs=setup.data();void* details=data.data();void* t=transport.data();void* vt=table.data();
            put(at(armada,loading_bindings::transport),t);
            put(transport.data(),vt);put(transport.data()+0x30,gs);put(setup.data()+0x10,details);
            put(setup.data(),0); // Local slot.
            put(data.data()+0x124,1);put(data.data()+0x168,3); // Human, AI.
            put(data.data()+0x110,0);put(data.data()+0x154,1); // Race indices.
            put(data.data()+0x104,0);put(data.data()+0x148,1); // Teams.
            std::strcpy(reinterpret_cast<char*>(race0.data()+0x14),"federation");
            std::strcpy(reinterpret_cast<char*>(race1.data()+0x14),"borg");
            put(race0.data()+0x44c,0);put(race1.data()+0x44c,1);
            void* races[]={race0.data(),race1.data()};void* race_list=races;
            put(at(armada,0x337cb4),2);put(at(armada,0x337cb8),2);put(at(armada,0x337cbc),race_list);
            // Wine maps the EXE as data away from 0x400000 without fixing these
            // absolute code operands. Rebase only this verified leaf replay.
            for(const auto pair:{std::pair<unsigned,unsigned>{0x0f,0x337cb4},{0x17,0x337cb8},{0x23,0x337cbc}}){
                const auto address=reinterpret_cast<std::uint32_t>(at(armada,pair.second));
                put(at(armada,loading_bindings::find_race+pair.first),address);
            }
            table[0x10/4]=at(armada,0x159cd0);table[0x18/4]=at(armada,0x159cc0);
            table[0x40/4]=at(armada,0x158ad0);
            if(!animation){
                next_splash.store(reinterpret_cast<void*>(&splash_begin_original));
                next_kill.store(reinterpret_cast<void*>(&splash_end_original));animation_armed=true;
                fixture_observations::accept_setup=true;
                a2fo_refit_splash_begin();
                require(fixture_observations::accepted_setup_calls==1&&fixture_observations::accepted_before_native&&
                        loading_session&&!departure_pending,"accepted IA handover must precede splash preparation and retain departure");
                fixture_observations::continuing_launch=false;
                table[0x10/4]=at(armada,0x159cc0);
                a2fo_refit_splash_begin();
                require(fixture_observations::accepted_setup_calls==1,"non-IA loading consumed retained IA controls");
                a2fo_refit_splash_end();animation_armed=false;fixture_observations::accept_setup=false;
                table[0x10/4]=at(armada,0x159cd0);
            }
            jump(at(armada,loading_bindings::player_name),reinterpret_cast<void*>(&name_getter));
            void* destructor=reinterpret_cast<void*>(&destroy_string);put(at(armada,loading_bindings::string_dtor_iat),destructor);
            CreateDirectoryA("build/refit-loading-player-fixture",nullptr);
            CreateDirectoryA("build/refit-loading-player-fixture/RefitScene",nullptr);
            CreateDirectoryA("build/refit-loading-player-fixture/RefitScene/Loading",nullptr);
            require(CopyFileA("assets/refit-menus/loading/themes.ini","build/refit-loading-player-fixture/RefitScene/Loading/themes.ini",FALSE),"fixture theme registry");
            roots={"build/refit-loading-player-fixture"};selected_race.clear();content={};theme_cache.clear();
            LoadingModel players;populate_players(players);
            std::printf("Adapter fixture: rows=%u mode=%s race=%s form=%u names=%u destroyed=%u\n",
                static_cast<unsigned>(players.players.size()),players.mode.c_str(),selected_race.c_str(),
                static_cast<unsigned>(players.theme.form),names,destroyed);
            for(const auto& p:players.players)std::printf("row: %s / %s / team=%d / local=%d\n",p.name.c_str(),p.detail.c_str(),p.team,p.local);
            require(players.players.size()==2&&players.players[0].name=="Native Player"&&
                    players.players[1].name=="Native AI"&&players.players[1].detail=="Borg"&&
                    players.players[0].local&&!players.players[1].local&&players.players[1].team==1&&
                    players.mode=="INSTANT ACTION"&&selected_race=="federation"&&
                    players.theme.form==LoadingForm::elbow&&destroyed==2&&names==2,
                    "live adapter copies players/races/locality/teams, skips vacant, destroys native results");
            table[0x10/4]=at(armada,0x159cc0);table[0x40/4]=at(armada,0x161ce0);
            table[0x44/4]=at(armada,0x161ca0);table[0x5c/4]=at(armada,0x158b40);
            put(transport.data()+0x230,.42f);players={};populate_players(players);
            require(players.mode=="MULTIPLAYER"&&players.players.size()==2&&
                    std::abs(players.players[0].fraction-.42f)<.00001f&&players.players[1].fraction<0,
                    "transport progress belongs only to send-worthy player slots");
            put(data.data()+0x124+7*0x44,2); // Observer is not a game player.
            put(data.data()+0x110,2); // Random/unresolved sentinel has no Race.
            void* mission=race1.data();put(at(armada,loading_bindings::mission_race),mission);
            table[0x10/4]=at(armada,0x159cd0);players={};populate_players(players);
            require(players.players.size()==2&&players.players[0].detail.empty()&&
                    players.theme.form==LoadingForm::shell&&selected_race.empty(),
                    "unresolved IA race stays neutral despite stale mission race; observer omitted");
            table[0x10/4]=at(armada,0x159cc0);table[0x18/4]=at(armada,0x159cd0);
            players={};populate_players(players);
            require(players.mode=="SINGLE PLAYER"&&players.kind==LoadingMode::campaign&&selected_race=="borg"&&players.theme.form==LoadingForm::cube,
                    "campaign mission race selects panel theme");
            table[0x10/4]=at(armada,0x159cd0);table[0x18/4]=at(armada,0x159cc0);
            std::strcpy(reinterpret_cast<char*>(race1.data()+0x14),"minbari");put(data.data()+0x110,1);
            players={};populate_players(players);
            require(players.players[0].detail=="minbari"&&players.theme.form==LoadingForm::shell,
                    "custom faction retains native identifier with neutral fallback");
            void* missing_destructor=nullptr;put(at(armada,loading_bindings::string_dtor_iat),missing_destructor);
            const unsigned before_names=names;players={};populate_players(players);
            require(players.players[0].name=="Player 1"&&names==before_names,
                    "no native temporary is created without its owning destructor");
            put(at(armada,loading_bindings::string_dtor_iat),destructor);
            put(setup.data(),8);players={};populate_players(players);
            require(players.players.empty(),"invalid local slot does not call unchecked slot getters");put(setup.data(),0);
            // Captured minimap is copied and associated with native map name.
            std::strcpy(reinterpret_cast<char*>(data.data()+0x2c),"test_map");
            Artwork image{2,2,{1,2,3,4}};remember_setup_preview(image);image.pixels[0]=99;
            require(preview_map=="test_map"&&setup_preview.pixels[0]==1,"setup preview retained by value and map key");
            jump(at(armada,loading_bindings::status_update),reinterpret_cast<void*>(&status_update));
            const char* step="Constructing native objects";a2fo_refit_loading_status(step);
            require(statuses==1&&observed_status==step&&step_text==step,"status text copied before exact original forwarding");
            float progress=.375f;
            a2fo_refit_loading_bar(-2.5f,reinterpret_cast<void*>(1),progress);
            require(calls==1&&observed==reinterpret_cast<void*>(1),"invalid device forwards once");
            std::uint32_t expected{};std::memcpy(&expected,&progress,4);
            require(progress_bits==expected&&first_bits==0xc0200000,"stdcall argument preservation");
            float cfg[13]{.0167f,.925f,.0375f,.9625f,1,1,1,.8f,.8f,.3f,.1f,.1f,.1f};
            DWORD old{};auto* state=reinterpret_cast<BYTE*>(fleetops)+0x246a70;
            require(VirtualProtect(state,0x50,PAGE_READWRITE,&old),"fixture config write");
            std::memcpy(state,cfg,sizeof(cfg));
            const std::uintptr_t absent=0;
            std::memcpy(reinterpret_cast<BYTE*>(fleetops)+loading_bindings::tip_text,&absent,4);
            std::memcpy(reinterpret_cast<BYTE*>(fleetops)+loading_bindings::tip_sprite,&absent,4);
            state[0x34]=0;state[0x35]=0;
            LoadingModel model;
            require(protected_regions(model,1920,1080)&&model.protected_regions.size()==1,"native bar protected");
            require(model.protected_regions[0].x==68&&model.protected_regions[0].y==1035,"ODF bar coordinate offsets");
            require(model.bar_track.x==72&&model.bar_track.y==1039&&model.bar_track.width>=1776&&model.bar_track.width<=1777&&
                    model.bar_track.height==19,"modern track follows exact full ODF bar extent");
            const char* text="Tip fixture";void* sprite=reinterpret_cast<void*>(1);
            std::memcpy(reinterpret_cast<BYTE*>(fleetops)+loading_bindings::tip_text,&text,4);
            std::memcpy(reinterpret_cast<BYTE*>(fleetops)+loading_bindings::tip_sprite,&sprite,4);
            model={};require(protected_regions(model,1920,1080)&&model.protected_regions.size()==3,"tip/sprite rectangles");
            require(model.protected_regions[1].x==1532&&model.protected_regions[1].y==860,"tip coordinate offsets");
            require(model.protected_regions[2].x==188&&model.protected_regions[2].y==104,"sprite coordinate offsets");
            state[0x34]=1;state[0x35]=1;
            const float vertical_y=.1f;std::memcpy(state+12,&vertical_y,4);
            model={};require(protected_regions(model,1920,1080),"inverted vertical bar bounds");
            require(model.protected_regions[0].x==4&&model.protected_regions[0].y==188,"native vertical/invert convention");
            shutdown();a2fo_refit_loading_bar(0,nullptr,1.f);
            require(calls==2,"shutdown forwards exactly once");
            a2fo_refit_loading_status("after shutdown");require(statuses==2&&step_text==step,"status forwards after shutdown without collecting");
        }else require(installs==(changed?0u:1u),"changed bindings never patched");
        std::puts("PASS: loading binding admission, stdcall forwarding/fallback, native ODF regions, shutdown");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

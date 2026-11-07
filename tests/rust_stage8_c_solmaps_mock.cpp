#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>

#ifndef A2FO_STAGE8_SOL_SOURCE
#error A2FO_STAGE8_SOL_SOURCE is required
#endif
#include A2FO_STAGE8_SOL_SOURCE

extern "C" std::uintptr_t fixture_native_assign(void*,std::uintptr_t);
extern "C" std::uintptr_t fixture_native_add(void*,std::uintptr_t);
void* g_fixture_mission_ptr=nullptr;
void* g_fixture_vtable[7]{};
std::array<std::uint8_t,32> g_fixture_spawned_object{};
std::uint8_t g_fixture_class_failed=0,g_fixture_class_good=0;
void* g_fixture_details=nullptr;
std::array<std::uint8_t,32> g_fixture_start{};
void* g_fixture_starts[1]{g_fixture_start.data()};
extern "C" std::uintptr_t a2fo_sol_call_thiscall_0(void* function,void*) {
    if(function==at(kGameObjectSimulateRva)){
        std::printf("common-simulate\n");return 1;
    }
    if(function==g_sol_nebula_simulate_hook.gateway){
        std::printf("nebula-gateway\n");return 1;
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_sol_call_thiscall_1(
    void* function,void* self,std::uintptr_t value) {
    if(function==reinterpret_cast<void*>(&fixture_native_assign)){
        const auto address=reinterpret_cast<std::uintptr_t>(self);
        const auto base=reinterpret_cast<std::uintptr_t>(g_fixture_details);
        const auto offset=address>=base&&address<base+kMapDetailsSize
            ?address-base:0xffffu;
        std::printf("assign:%lu:%s\n",static_cast<unsigned long>(offset),
            reinterpret_cast<const char*>(value));
        return 0;
    }
    if(function==at(kKnownMapsAddMapsWithMaskRva)){
        std::printf("native-mask:%s\n",reinterpret_cast<const char*>(value));
        return 0;
    }
    if(function==at(kAiIsTeamControlledRva)){
        std::printf("native-ai:%lu\n",static_cast<unsigned long>(value));
        return 1;
    }
    if(function==reinterpret_cast<void*>(&fixture_native_add)){
        std::printf("mission-add:%d\n",value==reinterpret_cast<std::uintptr_t>(
            g_fixture_spawned_object.data()));
        return 1;
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_sol_call_thiscall_4(
    void* function,void* self,std::uintptr_t,std::uintptr_t team,
    std::uintptr_t,std::uintptr_t label) {
    if(function!=at(kGameObjectClassConstructRva))std::abort();
    std::printf("construct:%s:%lu\n",reinterpret_cast<const char*>(label),
                static_cast<unsigned long>(team));
    return self==&g_fixture_class_failed?0:
        reinterpret_cast<std::uintptr_t>(g_fixture_spawned_object.data());
}
extern "C" void a2fo_sol_add_maps_with_mask_hook() { std::abort(); }
extern "C" void a2fo_sol_is_ai_controlled_hook() { std::abort(); }
namespace a2fo::sol {
bool parse_file(const std::string& path,Map& map,std::string* error) {
    std::printf("parse:%s\n",path.c_str());
    if(path.find("bar.sol")!=std::string::npos){
        if(error)*error="fixture rejection";
        return false;
    }
    map.version=3021;map.game_type="Deathmatch";map.filename="foo.sol";
    map.title="Fixture SOL";map.mission_script="fixture.drl";
    map.minimum_extent={-10,-20,-30};map.size={100,200,300};
    map.start_locations.push_back({{1,2,3},"human"});
    a2fo::sol::Object missing{};missing.odf="missing";missing.team=1;
    a2fo::sol::Object failed{};failed.odf="failed";failed.team=2;
    a2fo::sol::Object good{};good.odf="good";good.instance_name="Good Ship";
    good.team=3;good.position={4,5,6};
    map.objects.push_back(missing);map.objects.push_back(failed);
    map.objects.push_back(good);
    return true;
}
}
std::array<std::string,2> g_fixture_roots;
void* g_fixture_armada=nullptr;
int g_fixture_main_mode=0;
extern "C" std::uintptr_t fixture_native_assign(void*,std::uintptr_t){return 0;}
extern "C" std::uintptr_t fixture_native_add(void*,std::uintptr_t){return 0;}
extern "C" void* __cdecl fixture_native_current(){return g_fixture_mission_ptr;}
extern "C" void* __cdecl fixture_native_find(const char* name){
    std::printf("class-find:%s\n",name);
    if(std::strcmp(name,"failed")==0)return &g_fixture_class_failed;
    if(std::strcmp(name,"good")==0)return &g_fixture_class_good;
    return nullptr;
}
extern "C" void* __cdecl fixture_native_new(std::size_t size){
    std::printf("native-new:%lu\n",static_cast<unsigned long>(size));
    return std::malloc(size);
}
extern "C" void* __attribute__((thiscall)) fixture_native_details_ctor(void* self){
    std::memset(self,0,kMapDetailsSize);
    std::memcpy(static_cast<std::uint8_t*>(self)+kMapDetailsStartLocationArrayOffset,
                &g_fixture_starts,sizeof(void*));
    g_fixture_details=self;
    std::printf("details-ctor\n");
    return self;
}
extern "C" bool __cdecl fixture_native_main(void* reader){
    std::printf("native-main:%d\n",g_fixture_main_mode);
    if(g_fixture_main_mode==2)A2FOSOLMaps_LoadGameObjects(reader);
    return g_fixture_main_mode==1;
}
extern "C" void* __attribute__((thiscall)) fixture_native_probe(
    void* result,std::uintptr_t,std::uintptr_t){
    std::printf("native-probe\n");return result;
}
void fixture_stub(std::uintptr_t rva,void* destination){
    std::uint8_t code[]{0xb8,0,0,0,0,0xff,0xe0};
    const auto address=reinterpret_cast<std::uintptr_t>(destination);
    std::memcpy(code+1,&address,4);
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+rva,code,sizeof(code));
}
int g_fixture_patch_calls=0,g_fixture_hook_calls=0;
int g_fixture_fail_patch=-1,g_fixture_throw_patch=-1;
void A2FO_CALL fixture_log(const char*,const char* message){
    std::printf("log:%s\n",message);
}
void* A2FO_CALL fixture_armada(){return g_fixture_armada;}
bool A2FO_CALL fixture_patch(void* target,void*,const std::uint8_t*,std::size_t){
    const int call=g_fixture_patch_calls++;
    std::printf("patch:%d:%lu\n",call,static_cast<unsigned long>(
        reinterpret_cast<std::uintptr_t>(target)-
        reinterpret_cast<std::uintptr_t>(g_fixture_armada)));
    if(call==g_fixture_throw_patch)throw std::runtime_error("patch throw");
    return call!=g_fixture_fail_patch;
}
bool A2FO_CALL fixture_hook(void* target,void*,std::size_t,
                           const std::uint8_t*,A2FO_InlineHook* hook){
    std::printf("hook:%d:%lu\n",g_fixture_hook_calls++,
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(target)-
            reinterpret_cast<std::uintptr_t>(g_fixture_armada)));
    hook->target=target;hook->gateway=target;return true;
}
template<std::size_t N>
void fixture_signature(std::uintptr_t rva,const std::uint8_t(&expected)[N]){
    std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+rva,expected,N);
}
void fixture_seed_signatures(){
    fixture_signature(kLoadGameVersionProbeCallRva,kExpectedLoadGameVersionProbeCall);
    fixture_signature(kLoadGameMainLoadCallRva,kExpectedLoadGameMainLoadCall);
    fixture_signature(kLoadGamePostMissionACallRva,kExpectedLoadGamePostMissionACall);
    fixture_signature(kLoadGamePostMissionBCallRva,kExpectedLoadGamePostMissionBCall);
    fixture_signature(kAiMissionGetCurrentRva,kExpectedAiMissionGetCurrent);
    fixture_signature(kGameObjectClassFindRva,kExpectedGameObjectClassFind);
    fixture_signature(kGameObjectClassConstructRva,kExpectedGameObjectClassConstruct);
    fixture_signature(kGameObjectIsAiControlledRva,kExpectedGameObjectIsAiControlled);
    fixture_signature(kAiIsTeamControlledRva,kExpectedAiIsTeamControlled);
    fixture_signature(kNebulaSimulateRva,kExpectedNebulaSimulate);
    fixture_signature(kGameSetupLoadMapDetailsCallRva,kExpectedGameSetupLoadMapDetailsCall);
    fixture_signature(kKnownMapsLoadMapDetailsCallRva,kExpectedKnownMapsLoadMapDetailsCall);
    fixture_signature(kLoadGameObjectsLoadCallRva,kExpectedLoadGameObjectsLoadCall);
    fixture_signature(kLoadGameA2CraftClassTableCallRva,kExpectedLoadGameA2CraftClassTableCall);
    fixture_signature(kLoadGameAiMissionLoadCallRva,kExpectedLoadGameAiMissionLoadCall);
    fixture_signature(kKnownMapsConstructorMaskCallRva,kExpectedKnownMapsConstructorMaskCall);
    fixture_signature(kKnownMapsRefreshMaskCallRva,kExpectedKnownMapsRefreshMaskCall);
}
std::uint32_t A2FO_CALL fixture_root_count(){return 2;}
const char* A2FO_CALL fixture_root(std::uint32_t index){
    std::printf("root-read:%u\n",index);
    return index<g_fixture_roots.size()?g_fixture_roots[index].c_str():nullptr;
}
void fixture_file(const std::string& path){
    FILE* file=std::fopen(path.c_str(),"wb");
    if(!file)std::abort();
    std::fputs("x",file);
    std::fclose(file);
}

int main(){
    for(const std::string path : {"foo.sol","C:\\maps\\FOO.SOL",
                                  "file.bzn",".sol","x.sol.tmp"}) {
        std::printf("path:%s:%s:%d\n",path.c_str(),
                    basename_of(path).c_str(),has_sol_extension(path));
    }
    g_sources["foo.sol"]={"cached/foo.sol","foo.sol",7};
    for(const char* request : {"foo.sol","missing.sol","bad.bzn"}){
        SolSource source{};
        const bool resolved=resolve_source(request,source);
        std::printf("resolve:%s:%d:%s:%u\n",request,resolved,
                    source.path.c_str(),source.root_index);
    }
    {
        char directory[MAX_PATH]{};
        if(!GetTempPathA(MAX_PATH,directory))std::abort();
        const std::string path=join_path(directory,"stage8_solmaps_probe.sol");
        FILE* file=std::fopen(path.c_str(),"wb");
        if(!file)std::abort();
        std::fputs("x",file);
        std::fclose(file);
        SolSource source{};
        const bool resolved=resolve_source(path.c_str(),source);
        std::printf("resolve-direct:%d:%s\n",resolved,
                    source.basename.c_str());
        std::remove(path.c_str());
    }
    {
        char directory[MAX_PATH]{};
        if(!GetTempPathA(MAX_PATH,directory))std::abort();
        g_fixture_roots[0]=join_path(directory,"stage8_solroot0");
        g_fixture_roots[1]=join_path(directory,"stage8_solroot1");
        for(const auto& root:g_fixture_roots){
            CreateDirectoryA(root.c_str(),nullptr);
            CreateDirectoryA(join_path(root,"sol").c_str(),nullptr);
            CreateDirectoryA(join_path(root,"bzn").c_str(),nullptr);
        }
        fixture_file(join_path(join_path(g_fixture_roots[0],"sol"),"foo.sol"));
        fixture_file(join_path(join_path(g_fixture_roots[1],"bzn"),"foo.sol"));
        fixture_file(join_path(join_path(g_fixture_roots[0],"bzn"),"bar.sol"));
        A2FO_ModuleApi api{};
        api.extension_root_count=&fixture_root_count;
        api.extension_root=&fixture_root;
        g_api=&api;
        std::printf("indexed:%d\n",build_source_index());
        for(const auto& entry:g_sources){
            std::printf("source:%s:%u\n",entry.first.c_str(),
                        entry.second.root_index);
        }
        g_fixture_armada=VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,
                                      PAGE_EXECUTE_READWRITE);
        if(!g_fixture_armada)std::abort();
        api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
        api.log=&fixture_log;api.armada_module=&fixture_armada;
        api.patch_call=&fixture_patch;api.patch_jump=&fixture_patch;
        api.install_inline_hook=&fixture_hook;
        std::printf("init-signature:%d\n",A2FO_ModuleInit(&api));
        fixture_seed_signatures();
        g_fixture_patch_calls=0;g_fixture_fail_patch=2;
        std::printf("init-patch-fail:%d\n",A2FO_ModuleInit(&api));
        g_fixture_patch_calls=0;g_fixture_fail_patch=-1;
        g_fixture_throw_patch=2;
        try{A2FO_ModuleInit(&api);}catch(const std::exception& error){
            std::printf("init-patch-throw:%s:%d\n",error.what(),
                        g_fixture_patch_calls);
        }
        g_fixture_patch_calls=0;g_fixture_throw_patch=-1;
        std::printf("init-success:%d\n",A2FO_ModuleInit(&api));
        HMODULE a1=LoadLibraryA("A1Compat.dll");
        if(!a1){
            std::fprintf(stderr,"fixture A1Compat.dll load failed: %lu\n",
                         static_cast<unsigned long>(GetLastError()));
            return 3;
        }
        g_fixture_patch_calls=0;g_fixture_hook_calls=0;
        std::printf("init-a1:%d\n",A2FO_ModuleInit(&api));
        std::printf("init-a1-patches:%d:%d\n",g_fixture_patch_calls,
                    g_fixture_hook_calls);
        FreeLibrary(a1);
        std::array<std::uint8_t,16> known_maps{};
        a2fo_sol_add_maps_with_mask_dispatch(known_maps.data(),"bzn\\*.bzn");
        fixture_stub(kOperatorNewRva,reinterpret_cast<void*>(&fixture_native_new));
        fixture_stub(kMapDetailsConstructorRva,
                     reinterpret_cast<void*>(&fixture_native_details_ctor));
        fixture_stub(kLoadGameMainLoadRva,reinterpret_cast<void*>(&fixture_native_main));
        fixture_stub(kLoadGameVersionProbeRva,reinterpret_cast<void*>(&fixture_native_probe));
        fixture_stub(kGameObjectClassFindRva,reinterpret_cast<void*>(&fixture_native_find));
        fixture_stub(kAiMissionGetCurrentRva,reinterpret_cast<void*>(&fixture_native_current));
        void* assign=reinterpret_cast<void*>(&fixture_native_assign);
        std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+
                    kStdStringAssignCharIatRva,&assign,sizeof(assign));
        const std::int32_t slots=1;
        std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+
                    kGameSetupSlotCountRva,&slots,sizeof(slots));
        std::printf("details-missing:%d\n",
                    A2FOSOLMaps_CreateMapDetails("missing.sol",0)!=nullptr);
        std::printf("details-reject:%d\n",
                    A2FOSOLMaps_CreateMapDetails("bar.sol",1)!=nullptr);
        void* discovered=reinterpret_cast<void*>(
            a2fo_sol_load_map_details("foo.sol"));
        std::printf("details-discovery:%d\n",discovered!=nullptr);
        std::free(discovered);
        void* selected_by_hook=reinterpret_cast<void*>(
            a2fo_sol_load_selected_map_details("foo.sol"));
        std::printf("details-hook-selected:%d:%d\n",
                    selected_by_hook!=nullptr,A2FOSOLMaps_IsSelected());
        std::free(selected_by_hook);
        void* details=A2FOSOLMaps_CreateMapDetails("foo.sol",1);
        std::printf("details-selected:%d:%d\n",details!=nullptr,
                    A2FOSOLMaps_IsSelected());
        std::array<std::uint8_t,0x200> game_object{};
        std::printf("fixture-role:game-object=%08lx\n",static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(game_object.data())));
        const std::int32_t team=3;
        std::memcpy(game_object.data()+kGameObjectTeamOffset,&team,sizeof(team));
        std::printf("ai-null:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_IsAiControlledSafe(game_object.data())));
        std::array<std::uint8_t,8> ai{};
        void* table=game_object.data();void* ai_ptr=ai.data();
        std::memcpy(ai.data()+sizeof(void*),&table,sizeof(table));
        std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+kAiGlobalPointerRva,
                    &ai_ptr,sizeof(ai_ptr));
        std::printf("ai-present:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_IsAiControlledSafe(game_object.data())));
        std::array<std::uint8_t,0x200> nebula{};
        std::printf("fixture-role:nebula=%08lx\n",static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(nebula.data())));
        std::printf("nebula-null:%d\n",sol_nebula_simulate_hook(nebula.data(),nullptr));
        void* serialized=game_object.data();
        std::memcpy(nebula.data()+kNebulaSerializedPathOffset,
                    &serialized,sizeof(serialized));
        std::printf("nebula-native:%d\n",sol_nebula_simulate_hook(nebula.data(),nullptr));
        std::array<std::uint8_t,32> mission{};
        std::printf("fixture-role:mission=%08lx\n",static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(mission.data())));
        std::array<std::uint8_t,12> list{};
        std::printf("fixture-role:list=%08lx\n",static_cast<unsigned long>(
            reinterpret_cast<std::uintptr_t>(list.data())));
        g_fixture_vtable[6]=reinterpret_cast<void*>(&fixture_native_add);
        void* vtable=g_fixture_vtable;
        std::memcpy(mission.data(),&vtable,sizeof(vtable));
        void* mission_ptr=mission.data();void* list_ptr=list.data();
        g_fixture_mission_ptr=mission_ptr;
        std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+kAiMissionCurrentRva,
                    &mission_ptr,sizeof(mission_ptr));
        std::memcpy(static_cast<std::uint8_t*>(g_fixture_armada)+kGameObjectListPointerRva,
                    &list_ptr,sizeof(list_ptr));
        std::printf("objects-first:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_LoadGameObjects(nullptr)));
        std::printf("objects-again:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_LoadGameObjects(nullptr)));
        std::printf("table:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_LoadA2CraftClassTable(nullptr)));
        std::printf("mission:%lu\n",static_cast<unsigned long>(
            A2FOSOLMaps_LoadAiMission(nullptr)));
        const auto post_a=A2FOSOLMaps_LoadPostMissionA(nullptr);
        const auto post_b=A2FOSOLMaps_LoadPostMissionB(nullptr);
        std::printf("post:%lu:%lu\n",static_cast<unsigned long>(post_a),
                    static_cast<unsigned long>(post_b));
        for(int mode=0;mode<3;++mode){
            g_fixture_main_mode=mode;
            std::printf("main:%d:%lu\n",mode,static_cast<unsigned long>(
                A2FOSOLMaps_LoadGameMain(nullptr)));
        }
        std::array<std::uint8_t,24> probe{};
        const std::uint32_t version=3021;
        std::uintptr_t reader=1,context_ptr=2;
        auto details_ptr=reinterpret_cast<std::uintptr_t>(details);
        std::memcpy(probe.data()+4,&version,sizeof(version));
        std::memcpy(probe.data()+0x0c,&reader,sizeof(reader));
        std::memcpy(probe.data()+0x10,&context_ptr,sizeof(context_ptr));
        std::memcpy(probe.data()+0x14,&details_ptr,sizeof(details_ptr));
        A2FOSOLMaps_LoadGameVersionProbe(probe.data(),0,0);
        std::uint32_t caller_version=0;
        std::memcpy(&caller_version,probe.data()+4,sizeof(caller_version));
        std::printf("probe:%u:%u\n",probe[0],caller_version);
        char selected_path[128]{};std::uint32_t object_count=99,start_count=99;
        std::printf("summary:%d\n",A2FOSOLMaps_GetSelectedSummary(
            selected_path,sizeof(selected_path),&object_count,&start_count));
        std::printf("summary-values:%s:%u:%u\n",selected_path,object_count,start_count);
        std::free(details);
        const auto state=[](const char* name){
            std::printf("lifecycle:%s:%d:%u:%lu:%lu:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%d:%d\n",
                name,g_selected_valid,g_selected_map.version,
                static_cast<unsigned long>(g_selected_path.size()),
                static_cast<unsigned long>(g_sources.size()),
                static_cast<long>(g_discovery_dispatch_count),
                static_cast<long>(g_map_details_hook_count),
                static_cast<long>(g_runtime_object_load_count),
                static_cast<long>(g_runtime_mission_load_count),
                static_cast<long>(g_runtime_table_skip_count),
                static_cast<long>(g_runtime_version_bypass_count),
                static_cast<long>(g_runtime_post_mission_skip_count),
                static_cast<long>(g_manual_spawn_state),
                static_cast<long>(g_manual_spawn_created),
                static_cast<long>(g_manual_spawn_failed),
                static_cast<long>(g_sol_nebula_null_guard_count),
                g_armada!=nullptr,g_api!=nullptr);
        };
        state("before-clear");
        A2FOSOLMaps_ClearSelected();
        state("after-clear");
        g_selected_map.version=99;g_selected_path="selected-again";
        g_selected_valid=true;
        InterlockedExchange(&g_discovery_dispatch_count,7);
        InterlockedExchange(&g_map_details_hook_count,8);
        InterlockedExchange(&g_runtime_object_load_count,9);
        InterlockedExchange(&g_runtime_mission_load_count,10);
        InterlockedExchange(&g_runtime_table_skip_count,11);
        InterlockedExchange(&g_runtime_version_bypass_count,12);
        InterlockedExchange(&g_runtime_post_mission_skip_count,13);
        InterlockedExchange(&g_manual_spawn_state,2);
        InterlockedExchange(&g_manual_spawn_created,14);
        InterlockedExchange(&g_manual_spawn_failed,15);
        InterlockedExchange(&g_sol_nebula_null_guard_count,16);
        state("before-shutdown");
        A2FO_ModuleShutdown();
        state("after-shutdown");
        VirtualFree(g_fixture_armada,0,MEM_RELEASE);
        g_fixture_armada=nullptr;
        g_api=nullptr;
        std::remove(join_path(join_path(g_fixture_roots[0],"sol"),"foo.sol").c_str());
        std::remove(join_path(join_path(g_fixture_roots[1],"bzn"),"foo.sol").c_str());
        std::remove(join_path(join_path(g_fixture_roots[0],"bzn"),"bar.sol").c_str());
        for(const auto& root:g_fixture_roots){
            RemoveDirectoryA(join_path(root,"sol").c_str());
            RemoveDirectoryA(join_path(root,"bzn").c_str());
            RemoveDirectoryA(root.c_str());
        }
    }
    std::array<std::uint8_t,128> data{};
    const std::uint32_t object_record[]={4,4,0};
    std::memcpy(data.data()+16,object_record,sizeof(object_record));
    const std::uint32_t mission_record[]={2,40};
    std::memcpy(data.data()+64,mission_record,sizeof(mission_record));
    std::memcpy(data.data()+72,"EmptyMission",13);
    std::array<std::uint8_t,0x80> reader{};
    std::uintptr_t base=reinterpret_cast<std::uintptr_t>(data.data());
    std::uintptr_t cursor=base+4,end=base+data.size();
    std::uint32_t size=static_cast<std::uint32_t>(data.size());
    std::memcpy(reader.data()+0x0c,&base,sizeof(base));
    std::memcpy(reader.data()+0x10,&size,sizeof(size));
    std::memcpy(reader.data()+0x54,&cursor,sizeof(cursor));
    std::memcpy(reader.data()+0x58,&end,sizeof(end));
    FileReaderCursorState state{};
    std::printf("cursor:%d\n",inspect_file_reader_cursor(reader.data(),&state));
    std::printf("cursor-state:%lu:%lu\n",
        static_cast<unsigned long>(state.offset),
        static_cast<unsigned long>(state.remaining));
    for(std::size_t offset : {0u,16u,64u,128u,129u}){
        std::printf("target:%lu:%d\n",static_cast<unsigned long>(offset),
                    set_reader_cursor_to_offset(reader.data(),offset));
    }
    g_selected_valid=true;
    for(std::size_t offset : {0u,16u,17u,64u,120u,129u}){
        std::printf("object:%lu:%d\n",static_cast<unsigned long>(offset),
                    validate_sol_object_boundary(state,offset));
        std::printf("mission:%lu:%d\n",static_cast<unsigned long>(offset),
                    validate_sol_mission_boundary(state,offset));
    }
    return 0;
}

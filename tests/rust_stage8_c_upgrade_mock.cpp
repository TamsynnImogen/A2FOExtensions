// Compile the complete matching frozen/current UpgradePods translation unit.
#include A2FO_STAGE8_UPGRADE_SOURCE

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>

using namespace a2fo;
void* g_lookup_class=nullptr;
void* g_lookup_class3=nullptr;
void* g_lookup_other=nullptr;
bool g_mutate_class_tier_on_read=false;

namespace a2fo {
bool register_research_station_hybrid_lists(void*,void*) noexcept {
    std::puts("hybrid_register");return true;
}
std::size_t publish_research_station_hybrid_items(void*) noexcept {
    std::puts("hybrid_publish");return 0;
}
void cleanup_hybrid_cocoon(void*) noexcept {std::puts("hybrid_cocoon_cleanup");}
void cleanup_hybrid_construction(void*) noexcept {std::puts("hybrid_construct_cleanup");}
}

extern "C" std::uintptr_t a2fo_call_thiscall_0(void* function,void*){
    if(function==g_pod_detach_hook.gateway){std::puts("native_detach");return 17;}
    if(function==g_station_destructor_hook.gateway){std::puts("native_station_dtor");return 19;}
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_1(void* function,void*,
    std::uintptr_t argument){
    if(function==g_same_type_hook.gateway){std::puts("native_same_type");return 23;}
    if(function==g_station_can_build_hook.gateway){std::puts("native_can_build");return 0;}
    if(function==g_pod_attach_hook.gateway){std::puts("native_attach");return 29;}
    if(function==g_station_constructor_hook.gateway){std::puts("native_station_ctor");return 31;}
    if(function==g_pod_class_dtor_hook.gateway){std::puts("native_class_dtor");return 37;}
    std::printf("unknown_native_1 %lu\n",static_cast<unsigned long>(argument));
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_2(void* function,void*,
    std::uintptr_t,std::uintptr_t){
    if(function==g_pod_class_hook.gateway){std::puts("native_pod_class");return 41;}
    if(function==g_station_class_hook.gateway){std::puts("native_station_class");return 43;}
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_3(void* function,void*,
    std::uintptr_t a,std::uintptr_t b,std::uintptr_t c){
    if(function==at(g_armada,kTeamSetUpgradeMultiplierRva)){
        std::printf("native_multiplier %lu %lu %08lx\n",
            static_cast<unsigned long>(a),static_cast<unsigned long>(b),
            static_cast<unsigned long>(c));return 1;
    }
    if(function==at(g_armada,kParameterDbGetProjectIdRva)){
        std::printf("native_project_id %s\n",
            reinterpret_cast<const char*>(a));
        *reinterpret_cast<std::uint32_t*>(b)=0;return 0;
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_4(void* function,void*,
    std::uintptr_t key,std::uintptr_t output,std::uintptr_t size,
    std::uintptr_t){
    if(function!=at(g_armada,kParameterDbGetStringRva))std::abort();
    const char* name=reinterpret_cast<const char*>(key);
    const char* answer=nullptr;
    if(std::strcmp(name,"tier2BuildItem0")==0)answer="pod4.odf";
    if(std::strcmp(name,"tier2BuildItem1")==0)answer="";
    if(std::strcmp(name,"tier2BuildItem2")==0)answer="folder\\pod4.odf";
    if(std::strcmp(name,"tier2BuildItem3")==0)answer="missing.odf";
    if(std::strcmp(name,"tier2BuildItem4")==0)answer="other.odf";
    if(std::strcmp(name,"tier2BuildItem5")==0)answer="pod3.odf";
    if(!answer)return 0;
    std::strncpy(reinterpret_cast<char*>(output),answer,size-1);
    std::printf("native_parameter %s %s\n",name,answer);
    if(g_mutate_class_tier_on_read&&
       std::strcmp(name,"tier2BuildItem0")==0){
        g_declared_tiers[g_lookup_class]=5;
        g_mutate_class_tier_on_read=false;
        std::puts("mutated declared tier before validation");
    }
    return 1;
}

namespace {
template <std::size_t N>
void put(void* memory,std::size_t offset,const std::array<std::uint8_t,N>& value){
    std::memcpy(static_cast<std::uint8_t*>(memory)+offset,value.data(),N);
}
void pointer_at(void* memory,std::size_t offset,void* value){
    std::memcpy(static_cast<std::uint8_t*>(memory)+offset,&value,sizeof(value));
}
void u32_at(void* memory,std::size_t offset,std::uint32_t value){
    std::memcpy(static_cast<std::uint8_t*>(memory)+offset,&value,sizeof(value));
}
void f32_at(void* memory,std::size_t offset,float value){
    std::memcpy(static_cast<std::uint8_t*>(memory)+offset,&value,sizeof(value));
}
void A2FO_CALL fixture_log(const char*,const char* message){
    std::printf("log %s\n",message);
}
std::uint32_t g_root_count=0;
std::uint32_t g_install_count=0;
int g_manager=0;
std::uint32_t A2FO_CALL fixture_roots(){return g_root_count;}
const char* A2FO_CALL fixture_root(std::uint32_t index){
    return index<g_root_count?".":nullptr;
}
bool A2FO_CALL fixture_install(void* target,void*,std::size_t length,
    const std::uint8_t*,A2FO_InlineHook* hook){
    ++g_install_count;
    hook->target=target;
    hook->gateway=reinterpret_cast<void*>(0x200+g_install_count);
    hook->length=length;
    std::printf("install %lu length %lu\n",
        static_cast<unsigned long>(g_install_count),
        static_cast<unsigned long>(length));
    return true;
}
void* __cdecl fixture_find_class(const char* name){
    std::printf("find_class %s\n",name);
    if(std::strcmp(name,"pod4")==0)return g_lookup_class;
    if(std::strcmp(name,"pod3")==0)return g_lookup_class3;
    if(std::strcmp(name,"other")==0)return g_lookup_other;
    return nullptr;
}
void* __cdecl fixture_team_manager(std::uint32_t team){
    std::printf("team_manager %lu\n",static_cast<unsigned long>(team));
    return &g_manager;
}
void trampoline(void* address,void* function){
    auto* code=static_cast<std::uint8_t*>(address);
    code[0]=0x68;
    const auto target=static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(function));
    std::memcpy(code+1,&target,sizeof(target));
    code[5]=0xc3;
    FlushInstructionCache(GetCurrentProcess(),code,6);
}
}

int main(){
    std::puts("case trim names");
    for(const char* value:{" pod4.odf ","folder\\pod5.ODF", "a/b/", " \t ",
                           "pod.odf","podx.odf"}){
        const auto result=trim_odf_name(value);
        std::printf("trim %s -> %s\n",value,result.c_str());
    }
    std::puts("case indexed items");
    std::array<void*,kSecondaryBuildItemCapacity> items{};
    std::array<std::uint8_t,kSecondaryBuildItemCapacity> flags{};
    flags.fill(9);
    int class_marker=0;
    StationTierList tier{};
    tier.present[0]=true;tier.items[0]=&class_marker;
    tier.present[3]=true;tier.items[3]=nullptr;
    assert(apply_indexed_tier_items(items.data(),flags.data(),items.size(),tier));
    std::printf("indexed %u %u %u %u\n",items[0]==&class_marker,
        flags[0],items[3]==nullptr,flags[3]);
    assert(!apply_indexed_tier_items(nullptr,nullptr,57,tier));

    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);api.log=&fixture_log;
    api.extension_root_count=&fixture_roots;
    api.extension_root=&fixture_root;
    api.install_inline_hook=&fixture_install;
    g_api=&api;
    InitializeCriticalSection(&g_upgrade_lock);
    g_upgrade_lock_ready=true;
    g_pod_class_hook.gateway=reinterpret_cast<void*>(0x101);
    g_station_class_hook.gateway=reinterpret_cast<void*>(0x102);
    g_pod_class_dtor_hook.gateway=reinterpret_cast<void*>(0x103);
    g_same_type_hook.gateway=reinterpret_cast<void*>(0x104);
    g_station_can_build_hook.gateway=reinterpret_cast<void*>(0x105);
    g_pod_attach_hook.gateway=reinterpret_cast<void*>(0x106);
    g_pod_detach_hook.gateway=reinterpret_cast<void*>(0x107);
    g_station_constructor_hook.gateway=reinterpret_cast<void*>(0x108);
    g_station_destructor_hook.gateway=reinterpret_cast<void*>(0x109);

    std::array<std::uint8_t,0x500> class2{},class3{},class4{},other{};
    for(auto* object:{class2.data(),class3.data(),class4.data()}){
        u32_at(object,kClassTagOffset,kResearchPodClassTag);
        object[kPodIsUpgradeOffset]=1;
        u32_at(object,kPodUpgradeSystemOffset,1);
    }
    u32_at(class2.data(),kPodUpgradeTierOffset,2);
    u32_at(class3.data(),kPodUpgradeTierOffset,3);
    u32_at(class4.data(),kPodUpgradeTierOffset,4);
    f32_at(class4.data(),kPodUpgradeMultiplierOffset,1.375f);
    std::puts("case pod class tiers");
    assert(pod_class_hook(class4.data(),nullptr,0,nullptr)==41);
    std::printf("class4 raw %lu declared %lu\n",
        static_cast<unsigned long>(raw_class_tier(class4.data())),
        static_cast<unsigned long>(declared_class_tier(class4.data())));
    assert(pod_class_hook(class2.data(),nullptr,0,nullptr)==41);

    std::array<std::uint8_t,0x300> pod{},station{};
    pointer_at(pod.data(),kObjectClassOffset,class4.data());
    pointer_at(pod.data(),kResearchPodStationOffset,station.data());
    std::puts("case same type");
    std::printf("same fallback %lu\n",static_cast<unsigned long>(
        same_type_hook(nullptr,nullptr,class2.data())));
    std::printf("same extended %lu\n",static_cast<unsigned long>(
        same_type_hook(pod.data(),nullptr,class4.data())));
    std::puts("case station list");
    StationClassState state{};
    state.tiers[2].present[0]=true;
    state.tiers[2].items[0]=class4.data();
    g_station_classes[other.data()]=state;
    std::printf("can extended %lu\n",static_cast<unsigned long>(
        station_can_build_hook(other.data(),nullptr,class4.data())));
    std::printf("can fallback %lu\n",static_cast<unsigned long>(
        station_can_build_hook(other.data(),nullptr,class2.data())));
    g_station_classes.clear();
    g_declared_tiers.clear();
    g_pod_instances.clear();
    g_station_instances.clear();
    g_upgrade_lock_ready=false;
    DeleteCriticalSection(&g_upgrade_lock);

    std::puts("case checked initialization and tier parser");
    auto* armada=VirtualAlloc(nullptr,0x220000,MEM_COMMIT|MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    auto* fleet=VirtualAlloc(nullptr,0x220000,MEM_COMMIT|MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    assert(armada&&fleet);
    const auto copy_sig=+[](void* module,std::uintptr_t rva,
        const std::uint8_t* expected,std::size_t size){
        std::memcpy(static_cast<std::uint8_t*>(module)+rva,expected,size);
    };
#define COPY_SIG(module,rva,name) copy_sig(module,rva,name,sizeof(name))
    COPY_SIG(armada,kResearchPodDetachRva,kExpectedResearchPodDetach);
    COPY_SIG(armada,kResearchPodAttachRva,kExpectedResearchPodAttach);
    COPY_SIG(armada,kResearchStationConstructorRva,kExpectedResearchStationConstructor);
    COPY_SIG(armada,kResearchStationDestructorRva,kExpectedResearchStationDestructor);
    COPY_SIG(armada,kTeamManagerForTeamRva,kExpectedTeamManagerForTeam);
    COPY_SIG(armada,kTeamSetUpgradeMultiplierRva,kExpectedTeamSetUpgradeMultiplier);
    COPY_SIG(armada,kGameObjectClassFindRva,kExpectedGameObjectClassFind);
    COPY_SIG(armada,kGameObjectClassFindProjectIdRva,kExpectedGameObjectClassFindProjectId);
    COPY_SIG(armada,kParameterDbGetProjectIdRva,kExpectedParameterDbGetProjectId);
    COPY_SIG(fleet,kFoResearchPodClassCallbackRva,kExpectedResearchPodClassCallback);
    COPY_SIG(fleet,kFoResearchPodClassDtorRva,kExpectedResearchPodClassDtor);
    COPY_SIG(fleet,kFoResearchStationClassCallbackRva,kExpectedResearchStationClassCallback);
    COPY_SIG(fleet,kFoResearchStationCanBuildRva,kExpectedResearchStationCanBuild);
    COPY_SIG(fleet,kFoResearchPodSameTypeRva,kExpectedResearchPodSameType);
#undef COPY_SIG
    std::ofstream config("RTS_CFG.h",std::ios::binary);
    config<<"upgradePodMaximumTier = 4;\n";
    config.close();
    g_root_count=1;
    *at<std::uint8_t>(reinterpret_cast<HMODULE>(armada),
        kResearchPodDetachRva)^=1;
    assert(!initialize_upgrade_pods(&api,
        reinterpret_cast<HMODULE>(armada),
        reinterpret_cast<HMODULE>(fleet)));
    *at<std::uint8_t>(reinterpret_cast<HMODULE>(armada),
        kResearchPodDetachRva)^=1;
    assert(initialize_upgrade_pods(&api,
        reinterpret_cast<HMODULE>(armada),
        reinterpret_cast<HMODULE>(fleet)));
    std::printf("initialized max %lu installs %lu\n",
        static_cast<unsigned long>(g_configured_maximum_tier),
        static_cast<unsigned long>(g_install_count));
    assert(g_install_count==9);
    g_root_count=4097;
    load_upgrade_pod_maximum_tier();
    std::printf("invalid roots max %lu\n",
        static_cast<unsigned long>(g_configured_maximum_tier));
    g_root_count=1;
    load_upgrade_pod_maximum_tier();
    trampoline(at(reinterpret_cast<HMODULE>(armada),kGameObjectClassFindRva),
        reinterpret_cast<void*>(&fixture_find_class));
    trampoline(at(reinterpret_cast<HMODULE>(armada),kTeamManagerForTeamRva),
        reinterpret_cast<void*>(&fixture_team_manager));
    g_lookup_class=class4.data();
    g_lookup_class3=class3.data();
    g_lookup_other=other.data();
    g_declared_tiers[class4.data()]=4;
    std::array<std::uint8_t,0x500> station_class{};
    std::array<void*,kProducerBuildItemCapacity> primary{};
    std::array<void*,kSecondaryBuildItemCapacity> secondary{};
    std::array<std::uint8_t,kSecondaryBuildItemCapacity> secondary_flags{};
    secondary_flags.fill(9);
    pointer_at(station_class.data(),kProducerBuildItemsOffset,primary.data());
    pointer_at(station_class.data(),kStationSecondaryBuildItemsOffset,
        secondary.data());
    pointer_at(station_class.data(),kStationSecondaryFlagsOffset,
        secondary_flags.data());
    int parameter_db=0;
    g_mutate_class_tier_on_read=true;
    assert(station_class_hook(station_class.data(),nullptr,0,
        &parameter_db)==43);
    std::printf("mutated tier registered %u\n",
        g_station_classes.find(station_class.data())!=g_station_classes.end());
    assert(!g_mutate_class_tier_on_read);
    g_declared_tiers[class4.data()]=4;
    assert(station_class_hook(station_class.data(),nullptr,0,
        &parameter_db)==43);
    std::printf("tier registered %u\n",
        g_station_classes.find(station_class.data())!=g_station_classes.end());
    assert(station_constructor_hook(station.data(),nullptr,
        station_class.data())==31);
    std::array<void*,1> pods{{pod.data()}};
    pointer_at(station.data(),kStationInstancePodsOffset,pods.data());
    u32_at(station.data(),kStationInstancePodCountOffset,1);
    pointer_at(pod.data(),kObjectClassOffset,class3.data());
    refresh_station_progression(station.data());
    auto* instance=g_station_instances.at(station.data()).get();
    std::printf("slot0 level4 %u\n",
        instance->active_secondary[0]==class4.data());
    std::printf("same attached %lu\n",static_cast<unsigned long>(
        same_type_hook(pod.data(),nullptr,class2.data())));
    pointer_at(pod.data(),kObjectClassOffset,class4.data());
    u32_at(pod.data(),kObjectTeamOffset,2);
    assert(pod_attach_hook(pod.data(),nullptr,
        reinterpret_cast<std::uintptr_t>(station.data()))==29);
    assert(pod_detach_hook(pod.data(),nullptr)==17);
    assert(station_destructor_hook(station.data(),nullptr)==19);
    g_station_classes.clear();
    g_station_instances.clear();
    g_pod_instances.clear();
    g_declared_tiers.clear();
    g_upgrade_lock_ready=false;
    DeleteCriticalSection(&g_upgrade_lock);
    VirtualFree(armada,0,MEM_RELEASE);
    VirtualFree(fleet,0,MEM_RELEASE);
    std::puts("UpgradePods base fixture PASS");
    return 0;
}

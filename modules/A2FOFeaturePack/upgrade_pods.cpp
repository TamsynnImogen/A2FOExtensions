/*
 * File: modules/A2FOFeaturePack/upgrade_pods.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Upgradeable ship-system pod parser and runtime progression engine with safe extended-tier sidecars.
 */

#include "upgrade_pods.hpp"

#include "hybrid_bridge_client.hpp"
#include "upgrade_pod_config.hpp"
#include "../../rust/stage8_c_upgrade_ffi.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";
constexpr std::uint32_t kNativeMaximumTier = 3;
constexpr std::uint32_t kDefaultMaximumTier = 6;
constexpr std::uint32_t kHardMaximumTier = 16;
constexpr const char* kRtsConfigFileName = "RTS_CFG.h";
constexpr std::streamoff kMaximumRtsConfigSize = 2 * 1024 * 1024;
constexpr std::uint32_t kResearchPodClassTag = 0x52535250u;
constexpr std::size_t kShipUpgradeSystemCount = 5;

// ArmadaL.exe RVAs from the supported Armada 1.1 symbol map/PDB.
constexpr std::uintptr_t kResearchPodDetachRva = 0x0b95a0;
constexpr std::uintptr_t kResearchPodAttachRva = 0x0b95f0;
constexpr std::uintptr_t kResearchStationConstructorRva = 0x0b99b0;
constexpr std::uintptr_t kResearchStationDestructorRva = 0x0b9b50;
constexpr std::uintptr_t kTeamManagerForTeamRva = 0x096340;
constexpr std::uintptr_t kTeamSetUpgradeMultiplierRva = 0x0987d0;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x0cd370;
constexpr std::uintptr_t kGameObjectClassFindProjectIdRva = 0x0cd1f0;
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x135200;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;

// FleetOpsHook.dll map offsets include a 0x1000 .text RVA. Fleet Ops already
// owns these callbacks, so A2FO chains them rather than patching Armada's
// original class parsers a second time.
constexpr std::uintptr_t kFoResearchPodClassCallbackRva = 0x10c5e4;
constexpr std::uintptr_t kFoResearchPodClassDtorRva = 0x10c618;
constexpr std::uintptr_t kFoResearchStationClassCallbackRva = 0x1e3e00;
constexpr std::uintptr_t kFoResearchStationCanBuildRva = 0x1e3ea0;
constexpr std::uintptr_t kFoResearchPodSameTypeRva = 0x1fcffc;

constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kObjectTeamOffset = 0xec;
constexpr std::size_t kResearchPodStationOffset = 0x250;
constexpr std::size_t kClassTagOffset = 0x70;
constexpr std::size_t kPodIsUpgradeOffset = 0x450;
constexpr std::size_t kPodUpgradeTierOffset = 0x454;
constexpr std::size_t kPodUpgradeSystemOffset = 0x458;
constexpr std::size_t kPodUpgradeMultiplierOffset = 0x45c;
constexpr std::size_t kProducerBuildItemsOffset = 0x450;
constexpr std::size_t kStationSecondaryBuildItemsOffset = 0x494;
constexpr std::size_t kStationSecondaryFlagsOffset = 0x4cc;
constexpr std::size_t kStationInstanceSecondaryItemsOffset = 0x2c0;
constexpr std::size_t kStationInstancePodCountOffset = 0x2ac;
constexpr std::size_t kStationInstancePodsOffset = 0x2b0;
constexpr std::size_t kProducerBuildItemCapacity = 57;
constexpr std::size_t kSecondaryBuildItemCapacity = 58;
constexpr std::size_t kStationTierListCount = kHardMaximumTier - 1;

const std::uint8_t kExpectedResearchPodDetach[] =
    {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x40};
const std::uint8_t kExpectedResearchPodAttach[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56};
const std::uint8_t kExpectedResearchStationConstructor[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0x7d, 0x08};
const std::uint8_t kExpectedResearchStationDestructor[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedResearchPodClassCallback[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
const std::uint8_t kExpectedResearchPodClassDtor[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedResearchStationClassCallback[] =
    {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf8, 0x53};
const std::uint8_t kExpectedResearchStationCanBuild[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x8b, 0x75, 0x08};
const std::uint8_t kExpectedResearchPodSameType[] =
    {0x55, 0x8b, 0xec, 0x51, 0x53};
const std::uint8_t kExpectedTeamManagerForTeam[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedTeamSetUpgradeMultiplier[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08, 0x8b, 0x45, 0x0c};
const std::uint8_t kExpectedGameObjectClassFind[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedGameObjectClassFindProjectId[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedParameterDbGetProjectId[] =
    {0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};

extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument);
extern "C" std::uintptr_t a2fo_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

using FindClassFunction = void* (__cdecl*)(const char* name);
using FindClassByProjectIdFunction = void* (__cdecl*)(
    const std::uint32_t* project_id);
using TeamManagerFunction = void* (__cdecl*)(std::uint32_t team);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
CRITICAL_SECTION g_upgrade_lock;
bool g_upgrade_lock_ready = false;
bool g_upgrade_hooks_ready = false;
volatile LONG g_station_progression_log_count = 0;
std::uint32_t g_configured_maximum_tier = kDefaultMaximumTier;

A2FO_InlineHook g_pod_class_hook{};
A2FO_InlineHook g_pod_class_dtor_hook{};
A2FO_InlineHook g_station_class_hook{};
A2FO_InlineHook g_station_can_build_hook{};
A2FO_InlineHook g_same_type_hook{};
A2FO_InlineHook g_pod_attach_hook{};
A2FO_InlineHook g_pod_detach_hook{};
A2FO_InlineHook g_station_constructor_hook{};
A2FO_InlineHook g_station_destructor_hook{};

struct PodInstanceState {
    void* manager = nullptr;
    std::uint32_t system = 0;
    std::uint32_t tier = kNativeMaximumTier;
    float multiplier = 1.0f;
};

struct StationTierList {
    bool supplied = false;
    std::array<bool, kSecondaryBuildItemCapacity> present{};
    std::array<void*, kSecondaryBuildItemCapacity> items{};
};

struct StationClassState {
    std::array<void*, kSecondaryBuildItemCapacity> legacy_secondary{};
    std::array<StationTierList, kStationTierListCount> tiers{};
    bool has_extended_tiers = false;
};

struct StationInstanceState {
    void* station_class = nullptr;
    std::array<void*, kSecondaryBuildItemCapacity> base_secondary{};
    std::array<void*, kSecondaryBuildItemCapacity> active_secondary{};
};

std::unordered_map<void*, std::uint32_t> g_declared_tiers;
std::unordered_map<void*, PodInstanceState> g_pod_instances;
std::unordered_map<void*, StationClassState> g_station_classes;
std::unordered_map<void*, std::unique_ptr<StationInstanceState>>
    g_station_instances;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

std::uint8_t* bytes(void* value) {
    return static_cast<std::uint8_t*>(value);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadReadPtr(pointer, size);
}

bool writable_range(void* pointer, std::size_t size) noexcept {
    return pointer && size != 0 && !IsBadWritePtr(pointer, size);
}

bool read_class_tag(void* object_class, std::uint32_t& tag) noexcept {
    if (!object_class) return false;
    const void* address = bytes(object_class) + kClassTagOffset;
    if (!readable_range(address, sizeof(tag))) return false;
    tag = *static_cast<const std::uint32_t*>(address);
    return true;
}

void log_message(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::uint32_t configured_maximum_tier() noexcept {
    return g_configured_maximum_tier;
}

std::string join_path(const std::string& left, const std::string& right) {
    const auto mode=a2fo_rs_c_upgrade_join_mode(
        reinterpret_cast<const std::uint8_t*>(left.data()),left.size(),
        reinterpret_cast<const std::uint8_t*>(right.data()),right.size());
    if(mode==0)return right;
    if(mode==1)return left;
    if(mode==2)return left+right;
    return left+"\\"+right;
}

bool read_small_text_file(const std::string& path, std::string& contents) {
    struct Context {
        const std::string& path;
        std::string& contents;
        std::optional<std::ifstream> input{};
        std::int64_t size{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{path,contents};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.contents.clear();return 1;
        case 1:c.input.emplace(c.path,std::ios::binary);
            return static_cast<bool>(*c.input);
        case 2:c.input->seekg(0,std::ios::end);
            c.size=static_cast<std::int64_t>(c.input->tellg());return 1;
        case 3:log_message("Ignored oversized RTS configuration: "+c.path);
            return 1;
        case 4:c.input->seekg(0,std::ios::beg);return 1;
        case 5:{std::ostringstream stream;
            stream<<c.input->rdbuf();
            c.contents=stream.str();
            return c.input->good()||c.input->eof();}
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const bool result=a2fo_rs_c_upgrade_read_file(&context,step,
        &context.size,kMaximumRtsConfigSize,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void load_upgrade_pod_maximum_tier() {
    struct Context {
        std::uint32_t root_count{};
        const char* root{};
        std::string path{};
        std::string contents{};
        std::uint32_t candidate{};
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:g_configured_maximum_tier=kDefaultMaximumTier;return 1;
        case 1:c.root_count=g_api->extension_root_count();
            return c.root_count;
        case 2:log_message(
            "Extension-root count is invalid; upgrade-pod maximum remains 6");
            return 1;
        case 3:c.root=g_api->extension_root(static_cast<std::uint32_t>(value));
            return c.root&&*c.root;
        case 4:c.path=join_path(c.root,kRtsConfigFileName);
            return read_small_text_file(c.path,c.contents);
        case 5:c.candidate=g_configured_maximum_tier;
            return static_cast<std::uintptr_t>(
                a2fo::upgrade_pods::parse_maximum_tier_setting(
                    c.contents,&c.candidate));
        case 6:g_configured_maximum_tier=c.candidate;
            log_message("Applied upgradePodMaximumTier=" +
                std::to_string(g_configured_maximum_tier)+" from "+c.path);
            return 1;
        case 7:log_message("Ignored invalid upgradePodMaximumTier in "+
            c.path+" (valid range 3-16)");return 1;
        case 8:log_message("RTS_CFG.h upgrade-pod maximum: "+
            std::to_string(g_configured_maximum_tier)+
            (value!=0?"":" (default)"));return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_load_config(&context,step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    return module &&
        std::memcmp(at(module, rva), expected, Size) == 0;
}

std::uint32_t raw_class_tier(void* object_class) noexcept {
    if (!object_class) return 0;
    const void* address = bytes(object_class) + kPodUpgradeTierOffset;
    return readable_range(address, sizeof(std::uint32_t))
        ? *static_cast<const std::uint32_t*>(address) : 0;
}

std::uint32_t declared_class_tier(void* object_class) noexcept {
    if (!object_class) return 0;
    if (g_upgrade_lock_ready) EnterCriticalSection(&g_upgrade_lock);
    const auto found = g_declared_tiers.find(object_class);
    const std::uint32_t tier = found != g_declared_tiers.end()
        ? found->second : raw_class_tier(object_class);
    if (g_upgrade_lock_ready) LeaveCriticalSection(&g_upgrade_lock);
    return tier;
}

bool is_research_upgrade_pod(void* object_class) noexcept {
    std::uint32_t tag = 0;
    if (!read_class_tag(object_class, tag) ||
        tag != kResearchPodClassTag) {
        return false;
    }
    const void* is_upgrade = bytes(object_class) + kPodIsUpgradeOffset;
    return readable_range(is_upgrade, sizeof(std::uint8_t)) &&
           *static_cast<const std::uint8_t*>(is_upgrade) != 0;
}

void set_effective_multiplier(void* manager, std::uint32_t system,
                              float multiplier) noexcept {
    if (!manager || system > 4) return;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &multiplier, sizeof(bits));
    a2fo_call_thiscall_3(
        at(g_armada, kTeamSetUpgradeMultiplierRva), manager,
        kNativeMaximumTier, system, bits);
}

void recompute_multiplier(void* manager, std::uint32_t system) noexcept {
    struct Context {
        void* manager;
        std::uint32_t system;
        decltype(g_pod_instances)::iterator item{};
        PodInstanceState selected{};
        std::uint8_t failed{};
    } context{manager,system};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:EnterCriticalSection(&g_upgrade_lock);
            c.item=g_pod_instances.begin();return 1;
        case 1:return c.item!=g_pod_instances.end();
        case 2:return reinterpret_cast<std::uintptr_t>(
            c.item->second.manager);
        case 3:return c.item->second.system;
        case 4:return c.item->second.tier;
        case 5:c.selected=c.item->second;return c.selected.tier;
        case 6:++c.item;return 1;
        case 7:LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 8:set_effective_multiplier(c.manager,c.system,
            c.selected.multiplier);return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_recompute(&context,
        reinterpret_cast<std::uintptr_t>(manager),system,
        g_upgrade_lock_ready,step,&context.failed);
    if(context.failed)std::terminate();
}

void remember_attached_pod(void* pod) noexcept {
    struct Context {
        void* pod;
        void* object_class{};
        std::uint32_t tier{};
        std::uint32_t system{};
        std::uint32_t team{};
        void* manager{};
        PodInstanceState state{};
        std::uint8_t failed{};
    } context{pod};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.object_class=*reinterpret_cast<void**>(
            bytes(c.pod)+kObjectClassOffset);
            return reinterpret_cast<std::uintptr_t>(c.object_class);
        case 1:return is_research_upgrade_pod(c.object_class);
        case 2:c.tier=declared_class_tier(c.object_class);return c.tier;
        case 3:c.system=*reinterpret_cast<const std::uint32_t*>(
            bytes(c.object_class)+kPodUpgradeSystemOffset);
            return c.system;
        case 4:c.team=*reinterpret_cast<const std::uint32_t*>(
            bytes(c.pod)+kObjectTeamOffset);return c.team;
        case 5:{auto team_manager=reinterpret_cast<TeamManagerFunction>(
            at(g_armada,kTeamManagerForTeamRva));
            c.manager=team_manager(c.team);
            return reinterpret_cast<std::uintptr_t>(c.manager);}
        case 6:c.state.manager=c.manager;
            c.state.system=c.system;
            c.state.tier=c.tier;
            c.state.multiplier=*reinterpret_cast<const float*>(
                bytes(c.object_class)+kPodUpgradeMultiplierOffset);
            return 1;
        case 7:EnterCriticalSection(&g_upgrade_lock);
            g_pod_instances[c.pod]=c.state;
            LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 8:{char message[160];
            std::snprintf(message,sizeof(message),
                "Extended upgrade pod attached: upgradeLevel %lu, "
                "system %lu, multiplier %.3f",
                static_cast<unsigned long>(c.tier),
                static_cast<unsigned long>(c.system),
                static_cast<double>(c.state.multiplier));
            log_message(message);return 1;}
        case 9:recompute_multiplier(c.manager,c.system);return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_attach_pod(&context,
        reinterpret_cast<std::uintptr_t>(pod),g_upgrade_lock_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
}

std::string trim_odf_name(std::string value) {
    A2foStage8UpgradeSpan span{};
    if(!a2fo_rs_c_upgrade_trim_name(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size(),&span))return {};
    value=value.substr(span.start,span.length);
    if (value.size() > 4 &&
        _stricmp(value.c_str() + value.size() - 4, ".odf") == 0) {
        value.resize(value.size() - 4);
    }
    return value;
}

bool read_parameter_string(void* parameter_db, const std::string& key,
                           std::string& value) noexcept {
    std::array<char, 512> output{};
    const std::uintptr_t found = a2fo_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(output.data()),
        static_cast<std::uintptr_t>(output.size()),
        reinterpret_cast<std::uintptr_t>(""));
    output.back() = '\0';
    value.assign(output.data());
    return (found & 0xffu) != 0;
}

bool read_parameter_project_id(void* parameter_db, const std::string& key,
                               std::uint32_t& project_id) noexcept {
    project_id = 0;
    const std::uintptr_t found = a2fo_call_thiscall_3(
        at(g_armada, kParameterDbGetProjectIdRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(key.c_str()),
        reinterpret_cast<std::uintptr_t>(&project_id), 0);
    return (found & 0xffu) != 0;
}

bool apply_indexed_tier_items(void** items, std::uint8_t* flags,
                              std::size_t capacity,
                              const StationTierList& tier_items) {
    struct Context {
        void** items;
        std::uint8_t* flags;
        std::size_t capacity;
        const StationTierList& tier_items;
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{items,flags,capacity,tier_items};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return writable_range(c.items,c.capacity*sizeof(void*));
        case 1:log_message("Upgrade station build-list storage is "
            "unavailable; existing list preserved");return 1;
        case 2:return writable_range(c.flags,c.capacity);
        case 3:log_message("Upgrade station build-list flags are "
            "unavailable; existing list preserved");return 1;
        case 4:return c.tier_items.present[index];
        case 5:c.items[index]=c.tier_items.items[index];
            if(c.flags)c.flags[index]=0;
            return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const bool result=a2fo_rs_c_upgrade_apply_items(&context,
        reinterpret_cast<std::uintptr_t>(items),flags!=nullptr,capacity,
        step,&context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void load_tier_build_items(void* station_class,
                           void* parameter_db) noexcept {
    struct Context {
        void* station_class;
        void* parameter_db;
        void** producer_primary{};
        void** secondary{};
        std::uint8_t* secondary_flags{};
        FindClassFunction find_class{};
        FindClassByProjectIdFunction find_by_id{};
        StationClassState station_state{};
        StationTierList tier_items{};
        std::vector<void*> replacements{};
        std::uint32_t command_tier{};
        std::uint32_t upgrade_level{};
        std::uint32_t project_id{};
        char key[48]{};
        std::string value{};
        std::string name{};
        void* item_class{};
        std::uint8_t failed{};
    } context{station_class,parameter_db};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return readable_range(c.station_class,
            kStationSecondaryFlagsOffset+sizeof(void*));
        case 1:log_message("Upgrade station class storage is unavailable; "
            "tier build lists were not changed");return 1;
        case 2:c.producer_primary=*reinterpret_cast<void***>(
                bytes(c.station_class)+kProducerBuildItemsOffset);
            c.secondary=*reinterpret_cast<void***>(
                bytes(c.station_class)+kStationSecondaryBuildItemsOffset);
            c.secondary_flags=*reinterpret_cast<std::uint8_t**>(
                bytes(c.station_class)+kStationSecondaryFlagsOffset);
            c.find_class=reinterpret_cast<FindClassFunction>(
                at(g_armada,kGameObjectClassFindRva));
            c.find_by_id=reinterpret_cast<FindClassByProjectIdFunction>(
                at(g_armada,kGameObjectClassFindProjectIdRva));
            return configured_maximum_tier();
        case 3:c.command_tier=static_cast<std::uint32_t>(index);
            c.upgrade_level=c.command_tier+2;
            c.tier_items=StationTierList{};
            std::vector<void*>{}.swap(c.replacements);
            return 1;
        case 4:std::snprintf(c.key,sizeof(c.key),"tier%luBuildItem%lu",
                static_cast<unsigned long>(c.command_tier),
                static_cast<unsigned long>(index));
            std::string{}.swap(c.value);
            return read_parameter_string(c.parameter_db,c.key,c.value);
        case 5:c.name=trim_odf_name(std::move(c.value));
            return !c.name.empty();
        case 6:c.tier_items.present[index]=true;return 1;
        case 7:return read_parameter_project_id(
            c.parameter_db,c.key,c.project_id);
        case 8:return c.project_id;
        case 9:c.item_class=c.find_by_id(&c.project_id);
            return reinterpret_cast<std::uintptr_t>(c.item_class);
        case 10:c.item_class=c.find_class(c.name.c_str());
            return reinterpret_cast<std::uintptr_t>(c.item_class);
        case 11:log_message(std::string(c.key)+" ignored: "+c.name+
            " could not be loaded (project ID "+
            std::to_string(c.project_id)+")");return 1;
        case 12:return is_research_upgrade_pod(c.item_class);
        case 13:log_message(std::string(c.key)+" ignored: "+c.name+
            " is not a ResearchPod ship upgrade");return 1;
        case 14:return declared_class_tier(c.item_class);
        case 15:log_message(std::string(c.key)+" ignored: "+c.name+
            " has a different upgradeLevel");return 1;
        case 16:c.tier_items.present[index]=true;
            c.tier_items.items[index]=c.item_class;return 1;
        case 17:c.replacements.push_back(c.item_class);return 1;
        case 18:log_message("Upgrade station tier "+
            std::to_string(c.command_tier)+
            " had no valid items; existing list preserved");return 1;
        case 19:c.tier_items.supplied=true;return 1;
        case 20:return apply_indexed_tier_items(c.producer_primary,nullptr,
            kProducerBuildItemCapacity,c.tier_items);
        case 21:return apply_indexed_tier_items(c.secondary,
            c.secondary_flags,kSecondaryBuildItemCapacity,c.tier_items);
        case 22:c.station_state.tiers[c.command_tier]=c.tier_items;
            if(index!=0)c.station_state.has_extended_tiers=true;
            return 1;
        case 23:{char message[144];
            std::snprintf(message,sizeof(message),
                "Upgrade station tier %lu (upgradeLevel %lu) build list: "
                "%lu item%s",
                static_cast<unsigned long>(c.command_tier),
                static_cast<unsigned long>(c.upgrade_level),
                static_cast<unsigned long>(index),
                index==1?"":"s");
            log_message(message);return 1;}
        case 24:return c.secondary&&readable_range(c.secondary,
            kSecondaryBuildItemCapacity*sizeof(void*));
        case 25:log_message("Upgrade station secondary table is unavailable; "
            "extended tier progression was not registered");return 1;
        case 26:std::copy_n(c.secondary,kSecondaryBuildItemCapacity,
                c.station_state.legacy_secondary.begin());
            EnterCriticalSection(&g_upgrade_lock);
            g_station_classes[c.station_class]=std::move(c.station_state);
            LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 27:log_message(
            "Upgrade station per-hardpoint tier progression registered");
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_tier_lists(&context,
        reinterpret_cast<std::uintptr_t>(station_class),
        reinterpret_cast<std::uintptr_t>(parameter_db),step,
        &context.failed);
    if(context.failed)std::terminate();
}

void initialize_station_progression(void* station,
                                    void* station_class) noexcept {
    struct Context {
        void* station;
        void* station_class;
        std::unique_ptr<StationInstanceState> state{};
        void** active_items{};
        std::uint8_t failed{};
    } context{station,station_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return writable_range(bytes(c.station)+
            kStationInstanceSecondaryItemsOffset,sizeof(void*));
        case 1:c.state=std::make_unique<StationInstanceState>();
            c.state->station_class=c.station_class;return 1;
        case 2:{EnterCriticalSection(&g_upgrade_lock);
            const auto found=g_station_classes.find(c.station_class);
            if(found!=g_station_classes.end()){
                c.state->base_secondary=found->second.legacy_secondary;
                c.state->active_secondary=c.state->base_secondary;
                c.active_items=c.state->active_secondary.data();
                g_station_instances[c.station]=std::move(c.state);
            }
            return c.active_items!=nullptr;}
        case 3:LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 4:*reinterpret_cast<void***>(bytes(c.station)+
            kStationInstanceSecondaryItemsOffset)=c.active_items;return 1;
        case 5:log_message("Upgrade station instance received a "
            "per-hardpoint tier list");return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_init_station(&context,
        reinterpret_cast<std::uintptr_t>(station),
        reinterpret_cast<std::uintptr_t>(station_class),
        g_upgrade_lock_ready,step,&context.failed);
    if(context.failed)std::terminate();
}

void refresh_station_progression(void* station) noexcept {
    struct Context {
        void* station;
        void** pods{};
        void* pod{};
        void* pod_class{};
        StationInstanceState* instance{};
        const StationClassState* class_state{};
        void* candidate{};
        void* configured{};
        std::size_t slot{};
        std::size_t logged_slot{};
        std::uint32_t logged_from{};
        std::uint32_t logged_to{};
        std::uint8_t failed{};
    } context{station};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:{EnterCriticalSection(&g_upgrade_lock);
            const bool tracked=g_station_instances.find(c.station)!=
                g_station_instances.end();
            LeaveCriticalSection(&g_upgrade_lock);return tracked;}
        case 1:return readable_range(
            bytes(c.station)+kStationInstancePodsOffset,sizeof(void*));
        case 2:return readable_range(
            bytes(c.station)+kStationInstancePodCountOffset,
            sizeof(std::int32_t));
        case 3:return static_cast<std::uint32_t>(
            *reinterpret_cast<const std::int32_t*>(
                bytes(c.station)+kStationInstancePodCountOffset));
        case 4:c.pods=*reinterpret_cast<void***>(
            bytes(c.station)+kStationInstancePodsOffset);
            return reinterpret_cast<std::uintptr_t>(c.pods);
        case 5:return readable_range(c.pods,value*sizeof(void*));
        case 6:c.pod=c.pods[value];
            return reinterpret_cast<std::uintptr_t>(c.pod);
        case 7:return readable_range(bytes(c.pod)+kObjectClassOffset,
            sizeof(void*));
        case 8:c.pod_class=*reinterpret_cast<void**>(
            bytes(c.pod)+kObjectClassOffset);
            return reinterpret_cast<std::uintptr_t>(c.pod_class);
        case 9:return is_research_upgrade_pod(c.pod_class);
        case 10:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.pod_class)+kPodUpgradeSystemOffset);
        case 11:return declared_class_tier(c.pod_class);
        case 12:{EnterCriticalSection(&g_upgrade_lock);
            const auto instance_found=g_station_instances.find(c.station);
            if(instance_found!=g_station_instances.end()){
                c.instance=instance_found->second.get();
                const auto class_found=g_station_classes.find(
                    c.instance->station_class);
                if(class_found!=g_station_classes.end()){
                    c.class_state=&class_found->second;
                }
            }
            return c.class_state!=nullptr;}
        case 13:c.slot=value;
            c.candidate=c.instance->base_secondary[value];
            return reinterpret_cast<std::uintptr_t>(c.candidate);
        case 14:return is_research_upgrade_pod(c.candidate);
        case 15:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.candidate)+kPodUpgradeSystemOffset);
        case 16:{const auto tier=value/kSecondaryBuildItemCapacity;
            const auto slot=value%kSecondaryBuildItemCapacity;
            const auto& list=c.class_state->tiers[tier];
            c.configured=list.present[slot]?list.items[slot]:nullptr;
            return reinterpret_cast<std::uintptr_t>(c.configured);}
        case 17:return is_research_upgrade_pod(c.configured);
        case 18:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.configured)+kPodUpgradeSystemOffset);
        case 19:return c.class_state->tiers[value/kSecondaryBuildItemCapacity]
            .present[value%kSecondaryBuildItemCapacity];
        case 20:return reinterpret_cast<std::uintptr_t>(
            c.class_state->tiers[value/kSecondaryBuildItemCapacity]
                .items[value%kSecondaryBuildItemCapacity]);
        case 21:c.candidate=reinterpret_cast<void*>(value);
            c.instance->active_secondary[c.slot]=c.candidate;return 1;
        case 22:{const auto found=g_declared_tiers.find(c.candidate);
            if(found!=g_declared_tiers.end())return found->second;
            return c.candidate?raw_class_tier(c.candidate):0;}
        case 23:*reinterpret_cast<void***>(bytes(c.station)+
            kStationInstanceSecondaryItemsOffset)=
            c.instance->active_secondary.data();return 1;
        case 24:LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 25:return static_cast<std::uint32_t>(InterlockedIncrement(
            &g_station_progression_log_count));
        case 26:c.logged_slot=value;return 1;
        case 27:c.logged_from=static_cast<std::uint32_t>(value);return 1;
        case 28:c.logged_to=static_cast<std::uint32_t>(value);return 1;
        case 29:{char message[176];
            std::snprintf(message,sizeof(message),
                "Upgrade station slot %lu advanced from level %lu to level %lu",
                static_cast<unsigned long>(c.logged_slot),
                static_cast<unsigned long>(c.logged_from),
                static_cast<unsigned long>(c.logged_to));
            log_message(message);return 1;}
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_refresh_station(&context,
        reinterpret_cast<std::uintptr_t>(station),g_upgrade_lock_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
}

bool pod_belongs_to_extended_station(void* pod) noexcept {
    if (!pod || !g_upgrade_lock_ready || !readable_range(
            bytes(pod) + kResearchPodStationOffset, sizeof(void*))) {
        return false;
    }
    void* station = *reinterpret_cast<void**>(
        bytes(pod) + kResearchPodStationOffset);
    if (!station) return false;
    EnterCriticalSection(&g_upgrade_lock);
    const bool tracked = g_station_instances.find(station) !=
                         g_station_instances.end();
    LeaveCriticalSection(&g_upgrade_lock);
    return tracked;
}

void forget_station_progression(void* station) noexcept {
    if (!station || !g_upgrade_lock_ready) return;
    EnterCriticalSection(&g_upgrade_lock);
    g_station_instances.erase(station);
    LeaveCriticalSection(&g_upgrade_lock);
}

std::uintptr_t __attribute__((fastcall)) pod_class_hook(
    void* object_class, void*, std::uintptr_t argument1,
    void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_2(
        g_pod_class_hook.gateway, object_class, argument1,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    struct Context {
        void* object_class;
        std::uint32_t requested{};
        std::uint32_t maximum{};
        std::uint8_t failed{};
    } context{object_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t value) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        if(op==6){log_message(
            "Upgrade-pod class sidecar failed; using native tier");return 1;}
        try {switch(op){
        case 0:c.requested=raw_class_tier(c.object_class);
            return c.requested;
        case 1:c.maximum=configured_maximum_tier();return c.maximum;
        case 2:log_message("ResearchPod upgradeLevel "+
            std::to_string(c.requested)+" exceeds configured maximum "+
            std::to_string(c.maximum)+"; using tier 3");return 1;
        case 3:*reinterpret_cast<std::uint32_t*>(
            bytes(c.object_class)+kPodUpgradeTierOffset)=
            kNativeMaximumTier;return 1;
        case 4:EnterCriticalSection(&g_upgrade_lock);
            g_declared_tiers[c.object_class]=
                static_cast<std::uint32_t>(value);
            LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 5:log_message("Extended ResearchPod class loaded: upgradeLevel "+
            std::to_string(value));return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_pod_class(&context,
        reinterpret_cast<std::uintptr_t>(object_class),step,
        &context.failed);
    return result;
}

std::uintptr_t __attribute__((fastcall)) pod_class_dtor_hook(
    void* object_class, void*, std::uintptr_t flags) noexcept {
    struct Context {void* object_class;std::uintptr_t flags;
        std::uint8_t failed{};} context{object_class,flags};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:EnterCriticalSection(&g_upgrade_lock);
            g_declared_tiers.erase(c.object_class);
            LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 1:return a2fo_call_thiscall_1(
            g_pod_class_dtor_hook.gateway,c.object_class,c.flags);
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_upgrade_class_dtor(&context,
        reinterpret_cast<std::uintptr_t>(object_class),
        g_upgrade_lock_ready,step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_class_hook(
    void* station_class, void*, std::uintptr_t argument1,
    void* parameter_db) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_2(
        g_station_class_hook.gateway, station_class, argument1,
        reinterpret_cast<std::uintptr_t>(parameter_db));
    struct Context {void* station_class;void* parameter_db;
        std::uint8_t failed{};} context{station_class,parameter_db};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:try{load_tier_build_items(c.station_class,c.parameter_db);
            return 1;}catch(...){return 0;}
        case 1:log_message("Upgrade station tier build-list parsing failed");
            return 1;
        case 2:try{register_research_station_hybrid_lists(
            c.station_class,c.parameter_db);return 1;}
            catch(...){return 0;}
        case 3:try{publish_research_station_hybrid_items(c.station_class);
            return 1;}catch(...){return 0;}
        case 4:log_message(
            "Hybrid ResearchStation build-list publication failed");
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_station_class(&context,step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_can_build_hook(
    void* station_class, void*, void* item_class) noexcept {
    struct Context {
        void* station_class;
        void* item_class;
        const StationClassState* state{};
        std::uint8_t failed{};
    } context{station_class,item_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:{EnterCriticalSection(&g_upgrade_lock);
            const auto found=g_station_classes.find(c.station_class);
            c.state=found==g_station_classes.end()?nullptr:&found->second;
            return c.state!=nullptr;}
        case 1:return c.state->tiers[index/kSecondaryBuildItemCapacity]
            .present[index%kSecondaryBuildItemCapacity];
        case 2:return reinterpret_cast<std::uintptr_t>(
            c.state->tiers[index/kSecondaryBuildItemCapacity]
                .items[index%kSecondaryBuildItemCapacity]);
        case 3:LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 4:return a2fo_call_thiscall_1(
            g_station_can_build_hook.gateway,c.station_class,
            reinterpret_cast<std::uintptr_t>(c.item_class));
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_upgrade_can_build(&context,
        reinterpret_cast<std::uintptr_t>(station_class),
        reinterpret_cast<std::uintptr_t>(item_class),g_upgrade_lock_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_constructor_hook(
    void* station, void*, void* station_class) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_station_constructor_hook.gateway, station,
        reinterpret_cast<std::uintptr_t>(station_class));
    struct Context {void* station;void* station_class;
        std::uint8_t failed{};} context{station,station_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:try{initialize_station_progression(c.station,
            c.station_class);return 1;}catch(...){return 0;}
        case 1:log_message("Upgrade station instance tier-list setup failed");
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_station_constructor(&context,step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) station_destructor_hook(
    void* station, void*) noexcept {
    struct Context {void* station;std::uint8_t failed{};} context{station};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:cleanup_hybrid_construction(c.station);return 1;
        case 1:cleanup_hybrid_cocoon(c.station);return 1;
        case 2:return a2fo_call_thiscall_0(
            g_station_destructor_hook.gateway,c.station);
        case 3:forget_station_progression(c.station);return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_upgrade_station_destructor(
        &context,step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) same_type_hook(
    void* pod, void*, void* other_class) noexcept {
    struct Context {
        void* pod;
        void* other_class;
        void* own_class{};
        std::uint8_t failed{};
    } context{pod,other_class};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:c.own_class=*reinterpret_cast<void**>(
            bytes(c.pod)+kObjectClassOffset);
            return reinterpret_cast<std::uintptr_t>(c.own_class);
        case 1:return declared_class_tier(c.own_class);
        case 2:return declared_class_tier(c.other_class);
        case 3:return is_research_upgrade_pod(c.own_class);
        case 4:return is_research_upgrade_pod(c.other_class);
        case 5:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.own_class)+kPodUpgradeSystemOffset);
        case 6:return *reinterpret_cast<const std::uint32_t*>(
            bytes(c.other_class)+kPodUpgradeSystemOffset);
        case 7:return pod_belongs_to_extended_station(c.pod);
        case 8:return a2fo_call_thiscall_1(g_same_type_hook.gateway,c.pod,
            reinterpret_cast<std::uintptr_t>(c.other_class));
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_upgrade_same_type(&context,
        reinterpret_cast<std::uintptr_t>(pod),
        reinterpret_cast<std::uintptr_t>(other_class),step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) pod_attach_hook(
    void* pod, void*, std::uintptr_t argument) noexcept {
    const std::uintptr_t result = a2fo_call_thiscall_1(
        g_pod_attach_hook.gateway, pod, argument);
    struct Context {void* pod;void* station;
        std::uint8_t failed{};} context{pod,reinterpret_cast<void*>(argument)};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:try{remember_attached_pod(c.pod);return 1;}
            catch(...){return 0;}
        case 1:try{refresh_station_progression(c.station);return 1;}
            catch(...){return 0;}
        case 2:log_message("Upgrade-pod attach tracking failed");return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    a2fo_rs_c_upgrade_pod_attach(&context,step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

std::uintptr_t __attribute__((fastcall)) pod_detach_hook(
    void* pod, void*) noexcept {
    struct Context {
        void* pod;
        void* station{};
        decltype(g_pod_instances)::iterator found{};
        PodInstanceState previous{};
        std::uint8_t failed{};
    } context{pod};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return readable_range(bytes(c.pod)+
            kResearchPodStationOffset,sizeof(void*));
        case 1:c.station=*reinterpret_cast<void**>(
            bytes(c.pod)+kResearchPodStationOffset);
            return reinterpret_cast<std::uintptr_t>(c.station);
        case 2:EnterCriticalSection(&g_upgrade_lock);
            c.found=g_pod_instances.find(c.pod);
            return c.found!=g_pod_instances.end();
        case 3:c.previous=c.found->second;
            g_pod_instances.erase(c.found);return 1;
        case 4:LeaveCriticalSection(&g_upgrade_lock);return 1;
        case 5:return a2fo_call_thiscall_0(
            g_pod_detach_hook.gateway,c.pod);
        case 6:recompute_multiplier(c.previous.manager,c.previous.system);
            return 1;
        case 7:try{refresh_station_progression(c.station);return 1;}
            catch(...){return 0;}
        case 8:log_message("Upgrade station tier-list rollback failed");
            return 1;
        default:return 0;
        }}catch(...){c.failed=1;return 0;}
    };
    const auto result=a2fo_rs_c_upgrade_detach_pod(&context,
        reinterpret_cast<std::uintptr_t>(pod),g_upgrade_lock_ready,
        step,&context.failed);
    if(context.failed)std::terminate();
    return result;
}

bool install_hook(void* target, void* replacement, std::size_t length,
                  const std::uint8_t* expected,
                  A2FO_InlineHook& hook) {
    return g_api->install_inline_hook(
        target, replacement, length, expected, &hook);
}

}  // namespace

bool initialize_upgrade_pods(const A2FO_ModuleApi* api,
                             HMODULE armada,
                             HMODULE fleet_ops) {
    struct SignatureSpec {
        bool fleet;
        std::uintptr_t rva;
        const std::uint8_t* expected;
        std::size_t length;
    };
    const std::array<SignatureSpec,14> signatures{{
        {false,kResearchPodDetachRva,kExpectedResearchPodDetach,sizeof(kExpectedResearchPodDetach)},
        {false,kResearchPodAttachRva,kExpectedResearchPodAttach,sizeof(kExpectedResearchPodAttach)},
        {false,kResearchStationConstructorRva,kExpectedResearchStationConstructor,sizeof(kExpectedResearchStationConstructor)},
        {false,kResearchStationDestructorRva,kExpectedResearchStationDestructor,sizeof(kExpectedResearchStationDestructor)},
        {false,kTeamManagerForTeamRva,kExpectedTeamManagerForTeam,sizeof(kExpectedTeamManagerForTeam)},
        {false,kTeamSetUpgradeMultiplierRva,kExpectedTeamSetUpgradeMultiplier,sizeof(kExpectedTeamSetUpgradeMultiplier)},
        {false,kGameObjectClassFindRva,kExpectedGameObjectClassFind,sizeof(kExpectedGameObjectClassFind)},
        {false,kGameObjectClassFindProjectIdRva,kExpectedGameObjectClassFindProjectId,sizeof(kExpectedGameObjectClassFindProjectId)},
        {false,kParameterDbGetProjectIdRva,kExpectedParameterDbGetProjectId,sizeof(kExpectedParameterDbGetProjectId)},
        {true,kFoResearchPodClassCallbackRva,kExpectedResearchPodClassCallback,sizeof(kExpectedResearchPodClassCallback)},
        {true,kFoResearchPodClassDtorRva,kExpectedResearchPodClassDtor,sizeof(kExpectedResearchPodClassDtor)},
        {true,kFoResearchStationClassCallbackRva,kExpectedResearchStationClassCallback,sizeof(kExpectedResearchStationClassCallback)},
        {true,kFoResearchStationCanBuildRva,kExpectedResearchStationCanBuild,sizeof(kExpectedResearchStationCanBuild)},
        {true,kFoResearchPodSameTypeRva,kExpectedResearchPodSameType,sizeof(kExpectedResearchPodSameType)},
    }};
    struct HookSpec {
        bool fleet;
        std::uintptr_t rva;
        void* replacement;
        const std::uint8_t* expected;
        std::size_t length;
        A2FO_InlineHook* hook;
    };
    const std::array<HookSpec,9> hooks{{
        {true,kFoResearchPodClassCallbackRva,reinterpret_cast<void*>(&pod_class_hook),kExpectedResearchPodClassCallback,sizeof(kExpectedResearchPodClassCallback),&g_pod_class_hook},
        {true,kFoResearchPodClassDtorRva,reinterpret_cast<void*>(&pod_class_dtor_hook),kExpectedResearchPodClassDtor,sizeof(kExpectedResearchPodClassDtor),&g_pod_class_dtor_hook},
        {true,kFoResearchStationClassCallbackRva,reinterpret_cast<void*>(&station_class_hook),kExpectedResearchStationClassCallback,sizeof(kExpectedResearchStationClassCallback),&g_station_class_hook},
        {true,kFoResearchStationCanBuildRva,reinterpret_cast<void*>(&station_can_build_hook),kExpectedResearchStationCanBuild,sizeof(kExpectedResearchStationCanBuild),&g_station_can_build_hook},
        {true,kFoResearchPodSameTypeRva,reinterpret_cast<void*>(&same_type_hook),kExpectedResearchPodSameType,sizeof(kExpectedResearchPodSameType),&g_same_type_hook},
        {false,kResearchPodAttachRva,reinterpret_cast<void*>(&pod_attach_hook),kExpectedResearchPodAttach,sizeof(kExpectedResearchPodAttach),&g_pod_attach_hook},
        {false,kResearchPodDetachRva,reinterpret_cast<void*>(&pod_detach_hook),kExpectedResearchPodDetach,sizeof(kExpectedResearchPodDetach),&g_pod_detach_hook},
        {false,kResearchStationConstructorRva,reinterpret_cast<void*>(&station_constructor_hook),kExpectedResearchStationConstructor,sizeof(kExpectedResearchStationConstructor),&g_station_constructor_hook},
        {false,kResearchStationDestructorRva,reinterpret_cast<void*>(&station_destructor_hook),kExpectedResearchStationDestructor,sizeof(kExpectedResearchStationDestructor),&g_station_destructor_hook},
    }};
    struct Context {
        const A2FO_ModuleApi* api;
        HMODULE armada;
        HMODULE fleet_ops;
        const std::array<SignatureSpec,14>& signatures;
        const std::array<HookSpec,9>& hooks;
        std::uint8_t failed{};
        std::exception_ptr error{};
    } context{api,armada,fleet_ops,signatures,hooks};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return c.api&&c.armada&&c.fleet_ops&&
            c.api->install_inline_hook&&c.api->extension_root_count&&
            c.api->extension_root;
        case 1:if(c.api&&c.api->log)c.api->log(kModuleName,
            "Configurable upgrade pods unavailable in this core");
            return 1;
        case 2:{const auto& spec=c.signatures[index];
            HMODULE module=spec.fleet?c.fleet_ops:c.armada;
            return module&&std::memcmp(at(module,spec.rva),
                spec.expected,spec.length)==0;}
        case 3:c.api->log(kModuleName,
            "Configurable upgrade-pod signatures mismatch; disabled");
            return 1;
        case 4:g_api=c.api;g_armada=c.armada;return 1;
        case 5:load_upgrade_pod_maximum_tier();return 1;
        case 6:InitializeCriticalSection(&g_upgrade_lock);
            g_upgrade_lock_ready=true;return 1;
        case 7:{const auto& spec=c.hooks[index];
            HMODULE module=spec.fleet?c.fleet_ops:c.armada;
            return install_hook(at(module,spec.rva),spec.replacement,
                spec.length,spec.expected,*spec.hook);}
        case 8:c.api->log(kModuleName,
            "Configurable upgrade-pod hook installation failed");
            return 1;
        case 9:g_upgrade_hooks_ready=true;return 1;
        case 10:log_message("Configurable upgrade pods enabled "
            "(RTS_CFG.h maximum "+
            std::to_string(g_configured_maximum_tier)+")");return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    const bool result=a2fo_rs_c_upgrade_initialize(&context,step,
        &context.failed);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

}  // namespace a2fo

/*
 * Star Trek: Legacy SOL map discovery / MapDetails compatibility.
 *
 * SOL support is being brought up incrementally. Phase 2k keeps the stable
 * discovery/MapDetails parser from phase 1, bridges Legacy objects through Armada's native object factory,
 * synthesizes the native LoadGame probe companions that SOL cannot deserialize
 * itself, and deliberately replaces Legacy mission deserialization with a
 * native empty mission so object-only map loading can be isolated first.
 *
 * A1Compat owns the two Armada MapDetailsFactory::Load call sites when its
 * STA1 policy is active.  Normal Fleet Ops mods do not activate A1Compat, so
 * this module can also own those calls itself.  At startup we detect whether
 * A1Compat is active: if it is, its existing bridge delegates .sol requests to
 * A2FOSOLMaps_CreateMapDetails; otherwise A2FOSOLMaps installs equivalent
 * checked call-site hooks and leaves ordinary BZN loading untouched.  Runtime object loading is now bridged directly from the parsed SOL
 * object table into Armada's native GameObjectClass factory instead of feeding
 * Legacy polymorphic object payloads to the A2 BZN deserializer.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/stage8_c_solmaps_ffi.h"
#include "../../rust/stage9_sol_runtime_ffi.h"
#include "../../rust/stage9_sol_numeric_ffi.h"
#include "../../rust/stage9_sol_discovery_ffi.h"
#include "../../rust/stage9_sol_completion_ffi.h"
#include "../../sdk/include/a2fo_module_provider.hpp"
#include "sol_format.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <limits>

extern "C" std::uintptr_t a2fo_sol_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_sol_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
extern "C" std::uintptr_t a2fo_sol_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
extern "C" void a2fo_sol_add_maps_with_mask_hook();
extern "C" void a2fo_sol_is_ai_controlled_hook();
extern "C" void __cdecl a2fo_sol_add_maps_with_mask_dispatch(
    void* known_maps, const char* native_mask);

namespace {

constexpr char kModuleName[] = "A2FOSOLMaps";

// Supported ArmadaL.exe build.
constexpr std::uintptr_t kKnownMapsAddMapsWithMaskRva = 0x001b7d50;
constexpr std::uintptr_t kKnownMapsConstructorMaskCallRva = 0x001b787d;
constexpr std::uintptr_t kKnownMapsRefreshMaskCallRva = 0x001b8bd8;
constexpr std::uint8_t kExpectedKnownMapsConstructorMaskCall[]{
    0xe8, 0xce, 0x04, 0x00, 0x00};
constexpr std::uint8_t kExpectedKnownMapsRefreshMaskCall[]{
    0xe8, 0x73, 0xf1, 0xff, 0xff};

// MapDetailsFactory::Load call sites. A1Compat patches these when its STA1
// policy is active; otherwise SOLMaps owns them so .sol discovery also works
// in ordinary Fleet Ops mods.
constexpr std::uintptr_t kMapDetailsFactoryLoadRva = 0x0014ba00;
constexpr std::uintptr_t kGameSetupLoadMapDetailsCallRva = 0x00147a59;
constexpr std::uint8_t kExpectedGameSetupLoadMapDetailsCall[]{
    0xe8, 0xa2, 0x3f, 0x00, 0x00};
constexpr std::uintptr_t kKnownMapsLoadMapDetailsCallRva = 0x001b7da9;
constexpr std::uint8_t kExpectedKnownMapsLoadMapDetailsCall[]{
    0xe8, 0x52, 0x3c, 0xf9, 0xff};

// Phase 2 runtime boundaries in LoadGame_MainLoad.  For a selected SOL we
// place the native FileReader on parser-validated Legacy boundaries before
// reusing Armada's existing object and EmptyMission loaders.  The A2-only
// craft-class table between Teams and AiMission is skipped for SOL streams;
// Legacy's post-object layout is not the A2 table and feeding it to that
// loader can invalidate the stream before AiMission is reached.
constexpr std::uintptr_t kLoadGameObjectsLoadCallRva = 0x00202581;
constexpr std::uint8_t kExpectedLoadGameObjectsLoadCall[]{
    0xe8, 0x8a, 0x01, 0xed, 0xff};
constexpr std::uintptr_t kGameObjectsLoadRva = 0x000d2710;
constexpr std::uintptr_t kLoadGameA2CraftClassTableCallRva = 0x002025c0;
constexpr std::uint8_t kExpectedLoadGameA2CraftClassTableCall[]{
    0xe8, 0xdb, 0x41, 0xe7, 0xff};
constexpr std::uintptr_t kA2CraftClassTableLoadRva = 0x000767a0;
constexpr std::uintptr_t kLoadGameAiMissionLoadCallRva = 0x00202608;
constexpr std::uint8_t kExpectedLoadGameAiMissionLoadCall[]{
    0xe8, 0xf3, 0xed, 0xdf, 0xff};
constexpr std::uintptr_t kAiMissionLoadRva = 0x00001400;
constexpr std::uintptr_t kAiMissionCurrentRva = 0x003347dc;
// Phase 2k retains the Phase 2g probe strategy and no longer tries to force the native A2/FO metadata probe to parse
// Legacy's post-version front matter.  The probe successfully opens the file,
// constructs a FileReader and reads the real 3021 version, but SOL diverges from
// A2 BZN immediately afterwards and the probe returns early with result+0x10
// and result+0x14 unset.  The caller later dereferences result+0x10 at
// ArmadaL+0x202f28.  For a selected SOL we therefore let the native probe stop
// naturally, preserve its live FileReader, then synthesize the two ordinary
// accepted-path companions that the caller requires: a native NONE load-context
// object and our already-constructed SOL MapDetails.  Only the caller-facing
// version word is changed to 2177; the FileReader remains a real 3021 stream.
constexpr std::uintptr_t kLoadGameVersionProbeRva = 0x00202760;
constexpr std::uintptr_t kLoadGameVersionProbeCallRva = 0x00202e0b;
constexpr std::uint8_t kExpectedLoadGameVersionProbeCall[]{
    0xe8, 0x50, 0xf9, 0xff, 0xff};
// Outer GameSetup call into LoadGame_MainLoad. Phase 2j wraps this boundary so
// a Legacy SOL can recover when A2-only front-matter deserializers fail before
// the ordinary GameObjects::Load call site is reached.
constexpr std::uintptr_t kLoadGameMainLoadRva = 0x00202290;
constexpr std::uintptr_t kLoadGameMainLoadCallRva = 0x00202fb4;
constexpr std::uint8_t kExpectedLoadGameMainLoadCall[]{
    0xe8, 0xd7, 0xf2, 0xff, 0xff};
constexpr std::uint32_t kLoadGameCompatibleVersion = 0x00000881u; // 2177
constexpr std::uintptr_t kLoadGameDefaultContextCtorRva = 0x001da980;
constexpr std::size_t kLoadGameDefaultContextSize = 0x2c;

// For the object-only bring-up pass, suppress the two native post-mission
// deserializers as well. Legacy stores camera/path/mission-side data after its
// EmptyMission payload in a layout A2/FO does not understand yet.
constexpr std::uintptr_t kLoadGamePostMissionACallRva = 0x0020263a;
constexpr std::uint8_t kExpectedLoadGamePostMissionACall[]{
    0xe8, 0xd1, 0xf3, 0xe4, 0xff};
constexpr std::uintptr_t kPostMissionALoadRva = 0x00051a10;
constexpr std::uintptr_t kLoadGamePostMissionBCallRva = 0x0020266c;
constexpr std::uint8_t kExpectedLoadGamePostMissionBCall[]{
    0xe8, 0x5f, 0xce, 0xe4, 0xff};
constexpr std::uintptr_t kPostMissionBLoadRva = 0x0004f4d0;

// Native RtimeClass factory registered for EmptyMission.  Calling the factory
// gives us a valid AiMission object without asking RtimeClass::Load to consume
// Legacy's incompatible serialized mission payload.
constexpr std::uintptr_t kEmptyMissionFactoryRva = 0x00050d20;
constexpr std::size_t kFileReaderInspectionSize = 0x60;

// Live world bounds used by the native GameObjects loader when converting the
// serialized transforms into the active simulation volume.
constexpr std::uintptr_t kWorldMinimumExtentRva = 0x00339590;
constexpr std::uintptr_t kWorldMaximumExtentRva = 0x0033959c;
constexpr std::uintptr_t kGameObjectListPointerRva = 0x00361084;

// Phase 2k no longer feeds Legacy object payloads to A2/FO's polymorphic
// GameObjects::Load serializer.  SOL already gives us the common ODF, team,
// label and Matrix34 for every placed object, so instantiate those through the
// same native GameObjectClass factory used by normal runtime-created objects.
constexpr std::uintptr_t kAiMissionGetCurrentRva = 0x00001370;
constexpr std::uintptr_t kGameObjectClassFindRva = 0x000cd370;
constexpr std::uintptr_t kGameObjectClassConstructRva = 0x000cd390;
constexpr std::uint8_t kExpectedAiMissionGetCurrent[]{
    0xa1, 0xdc, 0x47, 0x73, 0x00, 0xc3};
constexpr std::uint8_t kExpectedGameObjectClassFind[]{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::uint8_t kExpectedGameObjectClassConstruct[]{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x84, 0x00, 0x00, 0x00};

// GameObject::IsAIControlled is reached while AiMission::AddObject publishes
// some environment objects (Bajor is the first object in the test SOL that
// takes this route).  Native A2/FO assumes the global AI singleton already
// exists here.  Our object-only SOL recovery deliberately bypasses the A2
// front matter that normally creates it, so the stock helper dereferences a
// null AI pointer.  Keep the native semantics when AI exists, but return false
// (not AI controlled) while the SOL bring-up world has no AI singleton yet.
constexpr std::uintptr_t kGameObjectIsAiControlledRva = 0x000d5290;
constexpr std::uintptr_t kAiIsTeamControlledRva = 0x0004d0d0;
constexpr std::uintptr_t kAiGlobalPointerRva = 0x00335bc4;
constexpr std::uint8_t kExpectedGameObjectIsAiControlled[]{
    0x8b, 0x89, 0xec, 0x00, 0x00, 0x00};
constexpr std::uint8_t kExpectedAiIsTeamControlled[]{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b, 0x49, 0x04};
constexpr std::size_t kGameObjectTeamOffset = 0xec;

// Native Nebula::Simulate assumes its serialized AiPath/project-reference
// companion at +0x188 was populated by Nebula::Load. Phase 2k deliberately
// constructs SOL objects from common parsed fields instead of replaying the
// Legacy polymorphic payload, so that pointer is null. The first simulation
// pass then dereferences +0x04 from null at ArmadaL+0x0a4c96. Keep native
// behaviour whenever the companion exists; for parser-spawned SOL nebulas
// whose companion is absent, run only the common GameObject simulation path.
constexpr std::uintptr_t kNebulaSimulateRva = 0x000a4c80;
constexpr std::uintptr_t kGameObjectSimulateRva = 0x000d19f0;
constexpr std::size_t kNebulaSerializedPathOffset = 0x188;
constexpr std::uint8_t kExpectedNebulaSimulate[]{
    0x55, 0x8b, 0xec, 0x51, 0x56, 0x57};

// MapDetails native allocation/construction and MSVC std::string assignment.
constexpr std::uintptr_t kOperatorNewRva = 0x00252710;
constexpr std::uintptr_t kOperatorDeleteRva = 0x002527d0;
constexpr std::uintptr_t kMapDetailsConstructorRva = 0x00149d80;
constexpr std::uintptr_t kStdStringAssignCharIatRva = 0x003b7c68;
constexpr std::uintptr_t kGameSetupSlotCountRva = 0x002b6b0c;
constexpr std::size_t kMapDetailsSize = 0xc8;
constexpr std::size_t kMapDetailsGameTypeOffset = 0x00;
// Native Armada MapDetails string layout, verified against the MPD*
// serializer in ArmadaL.exe:
//   +0x24 MPDFilename
//   +0x34 MPDDescription
//   +0x44 MPDFullpath
constexpr std::size_t kMapDetailsFilenameOffset = 0x24;
constexpr std::size_t kMapDetailsDescriptionOffset = 0x34;
constexpr std::size_t kMapDetailsFullPathOffset = 0x44;
constexpr std::size_t kMapDetailsAuthorOffset = 0x54;
constexpr std::size_t kMapDetailsMinimumExtentOffset = 0x6c;
constexpr std::size_t kMapDetailsSizeOffset = 0x78;
constexpr std::size_t kMapDetailsMissionScriptOffset = 0xa8;
constexpr std::size_t kMapDetailsStartLocationCountOffset = 0xc0;
constexpr std::size_t kMapDetailsStartLocationArrayOffset = 0xc4;
constexpr std::size_t kStartLocationPositionOffset = 0x00;
constexpr std::size_t kStartLocationTypeOffset = 0x0c;

using OperatorNewFn = void* (__cdecl*)(std::size_t);
using OperatorDeleteFn = void (__cdecl*)(void*);
using MapDetailsConstructorFn = void* (__attribute__((thiscall))*)(void*);
using MapDetailsFactoryLoadFn = void* (__cdecl*)(const char*);
using FileReaderLoadFn = bool (__cdecl*)(void*);
using LoadGameMainLoadFn = FileReaderLoadFn;
using AiMissionLoadFn = FileReaderLoadFn;
using LoadGameVersionProbeFn = void* (__attribute__((thiscall))*)(
    void*, std::uintptr_t, std::uintptr_t);
using EmptyMissionFactoryFn = void* (__cdecl*)();
using DefaultLoadContextCtorFn = void* (__attribute__((thiscall))*)(void*);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;

struct SolSource {
    std::string path;
    std::string basename;
    std::uint32_t root_index = 0;
};

// Lower-case basename -> highest-precedence physical SOL source.
std::map<std::string, SolSource> g_sources;
a2fo::sol::Map g_selected_map;
std::string g_selected_path;
bool g_selected_valid = false;
LONG g_discovery_dispatch_count = 0;
LONG g_map_details_hook_count = 0;
LONG g_runtime_object_load_count = 0;
LONG g_runtime_mission_load_count = 0;
LONG g_runtime_table_skip_count = 0;
LONG g_runtime_version_bypass_count = 0;
LONG g_runtime_post_mission_skip_count = 0;
LONG g_manual_spawn_state = 0;  // 0=not attempted, 1=running, 2=completed
LONG g_manual_spawn_created = 0;
LONG g_manual_spawn_failed = 0;
LONG g_sol_ai_null_guard_count = 0;
LONG g_sol_nebula_null_guard_count = 0;
A2FO_InlineHook g_sol_nebula_simulate_hook{};

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(std::uintptr_t rva) noexcept {
    return g_armada ? reinterpret_cast<std::uint8_t*>(g_armada) + rva
                    : nullptr;
}

std::string join_path(const std::string& left, const std::string& right) {
    switch(a2fo_rs_c_sol_join_action(
        reinterpret_cast<const std::uint8_t*>(left.data()),left.size(),
        reinterpret_cast<const std::uint8_t*>(right.data()),right.size())){
    case 0:return right;
    case 1:return left;
    case 2:return left+right;
    default:return left+"\\"+right;
    }
}

std::string lower_ascii(std::string value) {
    const auto lower=[](std::uint8_t byte) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(std::tolower(byte));
    };
    a2fo_rs_c_sol_lower_ascii(reinterpret_cast<std::uint8_t*>(value.data()),
                              value.size(),lower);
    return value;
}

std::string basename_of(const std::string& path) {
    const std::size_t start=a2fo_rs_c_sol_basename_start(
        reinterpret_cast<const std::uint8_t*>(path.data()),path.size());
    return start==0?path:path.substr(start);
}

bool has_sol_extension(const std::string& value) {
    const std::string base = lower_ascii(basename_of(value));
    return a2fo_rs_c_sol_has_extension(
        reinterpret_cast<const std::uint8_t*>(base.data()),base.size());
}

bool file_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return a2fo_rs_stage9_sol_file_exists(
        attributes, INVALID_FILE_ATTRIBUTES, FILE_ATTRIBUTE_DIRECTORY);
}

void index_directory(const std::string& directory, std::uint32_t root_index) {
    struct Context {const std::string* directory;std::uint32_t root_index;
        WIN32_FIND_DATAA data{};HANDLE search=INVALID_HANDLE_VALUE;
        std::exception_ptr error{};};
    Context context{&directory,root_index};
    const auto step=[](void* opaque,std::uint32_t operation,
                       bool* is_directory) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:
                c.search=FindFirstFileA(join_path(*c.directory,a2fo_rs_stage9_sol_search_mask()).c_str(),
                                         &c.data);
                if(c.search==INVALID_HANDLE_VALUE)return 0;
                *is_directory=(c.data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
                return 1;
            case 1:{
                const std::string basename=c.data.cFileName;
                struct EntryContext {Context* directory;const std::string* basename;
                    std::exception_ptr error{};} entry{&c,&basename};
                const auto entry_step=+[](void* raw,std::uint32_t op) noexcept -> std::int32_t {
                    auto& e=*static_cast<EntryContext*>(raw);
                    try {
                        if(op==0)return has_sol_extension(*e.basename);
                        // Later extension roots intentionally replace earlier winners.
                        g_sources[lower_ascii(*e.basename)]=SolSource{
                            join_path(*e.directory->directory,*e.basename),
                            *e.basename,e.directory->root_index};
                        return 0;
                    }catch(...){e.error=std::current_exception();return -1;}
                };
                a2fo_rs_stage9_sol_index_entry(&entry,entry_step);
                if(entry.error)std::rethrow_exception(entry.error);
                return 0;
            }
            case 2:
                if(!FindNextFileA(c.search,&c.data))return 0;
                *is_directory=(c.data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
                return 1;
            default:
                FindClose(c.search);
                return 0;
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_sol_walk_directory(&context,step);
    if(context.error)std::rethrow_exception(context.error);
}

bool build_source_index() {
    g_sources.clear();
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    const std::uint32_t count = g_api->extension_root_count();
    struct Context {std::exception_ptr error;};
    Context context{};
    const auto index=[](void* opaque,std::uint32_t i) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            const char* root=g_api->extension_root(i);
            if(!a2fo_rs_stage9_sol_input_present(reinterpret_cast<const std::uint8_t*>(root)))return 0;
            struct RootContext {const char* root;std::uint32_t index;
                std::exception_ptr error{};} native{root,i};
            const auto directory_step=+[](void* raw,std::uint32_t directory) noexcept -> std::int32_t {
                auto& r=*static_cast<RootContext*>(raw);
                try {
                    // Legacy sol/ precedes colocated bzn/ within each root.
                    index_directory(join_path(r.root,a2fo_rs_stage9_sol_directory(directory)),r.index);
                    return 0;
                }catch(...){r.error=std::current_exception();return -1;}
            };
            a2fo_rs_stage9_sol_root_directories(&native,directory_step);
            if(native.error)std::rethrow_exception(native.error);
            return 0;
        }catch(...){c.error=std::current_exception();return -1;}
    };
    const bool indexed=a2fo_rs_c_sol_index_roots(&context,count,index);
    if(context.error)std::rethrow_exception(context.error);
    return indexed;
}

bool resolve_source(const char* requested, SolSource& source) {
    if (!a2fo_rs_stage9_sol_input_present(reinterpret_cast<const std::uint8_t*>(requested))) return false;
    const std::string request = requested;
    struct Context {const std::string* request;SolSource* source;
                    std::exception_ptr error;};
    Context context{&request,&source,{}};
    const auto step=[](void* opaque,std::uint32_t operation) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            switch(operation){
            case 0:return has_sol_extension(*c.request)?1:0;
            case 1:return file_exists(*c.request)?1:0;
            case 2:
                c.source->path=*c.request;
                c.source->basename=basename_of(*c.request);
                c.source->root_index=a2fo_rs_stage9_sol_direct_source_root();
                return 1;
            default:{
                const auto found=g_sources.find(
                    lower_ascii(basename_of(*c.request)));
                if(found==g_sources.end())return 0;
                *c.source=found->second;
                return 1;
            }
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    const bool resolved=a2fo_rs_c_sol_resolve_source(&context,
        reinterpret_cast<const std::uint8_t*>(request.data()),request.size(),step);
    if(context.error)std::rethrow_exception(context.error);
    return resolved;
}

bool assign_native_string(void* object, std::size_t offset,
                          const std::string& value) {
    if (!g_armada || !object) return false;
    void* const* slot = reinterpret_cast<void* const*>(
        reinterpret_cast<std::uint8_t*>(g_armada) +
        kStdStringAssignCharIatRva);
    if (!slot || !*slot) return false;
    a2fo_sol_call_thiscall_1(
        *slot, static_cast<std::uint8_t*>(object) + offset,
        reinterpret_cast<std::uintptr_t>(value.c_str()));
    return true;
}

bool memory_range_accessible(const void* pointer, std::size_t size,
                             bool writable) noexcept {
    if (!pointer || size == 0) return false;
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (size > std::numeric_limits<std::uintptr_t>::max() - begin) return false;
    const std::uintptr_t end = begin + size;
    std::uintptr_t cursor = begin;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &information,
                         sizeof(information)) != sizeof(information) ||
            information.State != MEM_COMMIT ||
            (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }
        const DWORD protection = information.Protect & 0xffu;
        if (writable && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY) {
            return false;
        }
        const std::uintptr_t region_begin =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress);
        if (information.RegionSize >
            std::numeric_limits<std::uintptr_t>::max() - region_begin) {
            return false;
        }
        const std::uintptr_t region_end = region_begin + information.RegionSize;
        if (cursor < region_begin || region_end <= cursor) return false;
        cursor = std::min(region_end, end);
    }
    return true;
}

bool signature_matches(std::uintptr_t rva, const std::uint8_t* expected,
                       std::size_t size) noexcept {
    const void* const address = at(rva);
    return expected && size != 0u &&
           memory_range_accessible(address, size, false) &&
           std::memcmp(address, expected, size) == 0;
}

struct FileReaderCursorState {
    std::uintptr_t base = 0;
    std::uintptr_t cursor = 0;
    std::uintptr_t end = 0;
    std::uint32_t buffer_size = 0;
    std::uintptr_t offset = 0;
    std::uintptr_t remaining = 0;
};

bool inspect_file_reader_cursor(const void* file_reader,
                                FileReaderCursorState* state) noexcept {
    if (!state || !memory_range_accessible(
            file_reader, kFileReaderInspectionSize, false)) {
        return false;
    }
    FileReaderCursorState inspected;
    const auto* reader = static_cast<const std::uint8_t*>(file_reader);
    std::memcpy(&inspected.base, reader + 0x0c, sizeof(inspected.base));
    std::memcpy(&inspected.buffer_size, reader + 0x10,
                sizeof(inspected.buffer_size));
    std::memcpy(&inspected.cursor, reader + 0x54, sizeof(inspected.cursor));
    std::memcpy(&inspected.end, reader + 0x58, sizeof(inspected.end));
    A2foStage8SolCursor policy{inspected.base, inspected.cursor,
        inspected.end, inspected.buffer_size, 0, 0};
    if (!a2fo_rs_c_sol_cursor(&policy)) return false;
    inspected.offset = policy.offset;
    inspected.remaining = policy.remaining;
    *state = inspected;
    return true;
}

bool set_reader_cursor_to_offset(void* file_reader, std::size_t offset) noexcept {
    if (!file_reader) return false;
    FileReaderCursorState state;
    if (!inspect_file_reader_cursor(file_reader, &state)) return false;
    const A2foStage8SolCursor policy{state.base,state.cursor,state.end,
        state.buffer_size,state.offset,state.remaining};
    std::uintptr_t target = 0;
    if (!a2fo_rs_c_sol_target(&policy,offset,&target)) return false;
    auto* slot = static_cast<std::uint8_t*>(file_reader) + 0x54;
    if (!memory_range_accessible(slot, sizeof(target), true)) {
        return false;
    }
    std::memcpy(slot, &target, sizeof(target));
    return true;
}

bool validate_sol_object_boundary(const FileReaderCursorState& state,
                                  std::size_t offset) noexcept {
    if (!a2fo_rs_c_sol_object_span(g_selected_valid,offset,
                                  state.buffer_size)) return false;
    const auto* record = reinterpret_cast<const std::uint8_t*>(state.base + offset);
    if (!memory_range_accessible(record, 12u, false)) return false;
    return a2fo_rs_c_sol_object_record(
        record,static_cast<std::uint32_t>(g_selected_map.objects.size()));
}

bool validate_sol_mission_boundary(const FileReaderCursorState& state,
                                   std::size_t offset) noexcept {
    if (!a2fo_rs_c_sol_mission_span(offset,state.buffer_size)) return false;
    const auto* record = reinterpret_cast<const std::uint8_t*>(state.base + offset);
    if (!memory_range_accessible(record, 48u, false)) return false;
    return a2fo_rs_c_sol_mission_record(record);
}

bool apply_sol_world_bounds() noexcept {
    if (!g_selected_valid || !g_armada) return false;
    auto* minimum = reinterpret_cast<float*>(at(kWorldMinimumExtentRva));
    auto* maximum = reinterpret_cast<float*>(at(kWorldMaximumExtentRva));
    if (!memory_range_accessible(minimum, sizeof(float) * 3u, true) ||
        !memory_range_accessible(maximum, sizeof(float) * 3u, true)) {
        return false;
    }
    static_assert(sizeof(a2fo::sol::Vec3) == sizeof(float) * 3 &&
                  offsetof(a2fo::sol::Vec3, x) == 0 &&
                  offsetof(a2fo::sol::Vec3, y) == sizeof(float) &&
                  offsetof(a2fo::sol::Vec3, z) == sizeof(float) * 2,
                  "private SOL vector view requires three C-layout floats");
    a2fo_rs_stage9_sol_publish_bounds(
        &g_selected_map.minimum_extent.x, &g_selected_map.size.x,
        minimum, maximum);
    return true;
}

std::uint32_t live_object_count() noexcept {
    if (!g_armada) return 0;
    void* list = nullptr;
    void* const* slot = reinterpret_cast<void* const*>(at(kGameObjectListPointerRva));
    if (!memory_range_accessible(slot, sizeof(*slot), false)) return 0;
    std::memcpy(&list, slot, sizeof(list));
    if (!memory_range_accessible(list, 12u, false)) return 0;
    std::uint32_t count = 0;
    std::memcpy(&count, static_cast<const std::uint8_t*>(list) + 8u, 4u);
    return count;
}

// The accepted A2/FO setup path reaches GameObjects::SimulateAll at
// ArmadaL+0x0d2980 before LoadGame_MainLoad has reached its serialized object
// table.  Normally the global GameObject list has already been lazily created by
// an earlier native GameObject constructor.  SOL probe synthesis has no native
// front-matter object to do that, so the first setup simulation dereferenced a
// null GameObject::objectList at ArmadaL+0x0d299c.  Build the exact empty MSVC
// list shell used by the native GameObject constructor: a 12-byte owner whose
// +0x04 points to a 12-byte self-linked sentinel and whose +0x08 count is zero.
// The normal Armada cleanup path understands this layout and will free it.
void* ensure_empty_game_object_list() noexcept {
    if (!g_armada) return nullptr;

    auto** slot = reinterpret_cast<void**>(at(kGameObjectListPointerRva));
    if (!memory_range_accessible(slot, sizeof(*slot), true)) return nullptr;
    if (*slot) return *slot;

    const auto allocate = reinterpret_cast<OperatorNewFn>(at(kOperatorNewRva));
    const auto release = reinterpret_cast<OperatorDeleteFn>(at(kOperatorDeleteRva));
    if (!allocate) return nullptr;

    auto* owner = static_cast<std::uint8_t*>(allocate(12u));
    auto* sentinel = static_cast<std::uint8_t*>(allocate(12u));
    if (!owner || !sentinel) {
        if (release) {
            if (sentinel) release(sentinel);
            if (owner) release(owner);
        }
        return nullptr;
    }

    std::memset(owner, 0, 12u);
    std::memset(sentinel, 0, 12u);
    void* sentinel_ptr = sentinel;
    std::memcpy(sentinel + 0u, &sentinel_ptr, sizeof(sentinel_ptr));
    std::memcpy(sentinel + 4u, &sentinel_ptr, sizeof(sentinel_ptr));
    std::memcpy(owner + 4u, &sentinel_ptr, sizeof(sentinel_ptr));
    const std::uint32_t zero_count = 0u;
    std::memcpy(owner + 8u, &zero_count, sizeof(zero_count));

    // Publish only if no native constructor won the race in the meantime.
    void* existing = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot), owner, nullptr);
    if (existing) {
        if (release) {
            release(sentinel);
            release(owner);
        }
        return existing;
    }
    return owner;
}

// Fleet Ops invokes the current AiMission from the setup path before the later
// LoadGame_MainLoad mission-deserialization call site is reached.  A native BZN
// probe normally leaves enough state for that path to publish a mission, but our
// SOL probe synthesis deliberately bypasses Legacy mission data.  Keep a native
// EmptyMission alive as soon as the SOL probe has been accepted so the early
// AiMission tick at ArmadaL+0x1440 cannot dereference a null current mission.
void* ensure_empty_mission() noexcept {
    if (!g_armada) return nullptr;
    auto** current_slot = reinterpret_cast<void**>(at(kAiMissionCurrentRva));
    if (!memory_range_accessible(current_slot, sizeof(*current_slot), true)) {
        return nullptr;
    }
    void* mission = *current_slot;
    if (mission) return mission;

    const auto factory = reinterpret_cast<EmptyMissionFactoryFn>(
        at(kEmptyMissionFactoryRva));
    mission = factory ? factory() : nullptr;
    if (mission) *current_slot = mission;
    return mission;
}

struct NativeMatrix34 {
    float values[12]{};
};

bool add_object_to_current_mission(void* object) noexcept {
    if (!object || !g_armada) return false;
    const auto get_current = reinterpret_cast<void* (__cdecl*)()>(
        at(kAiMissionGetCurrentRva));
    void* mission = get_current ? get_current() : ensure_empty_mission();
    if (!mission || !memory_range_accessible(mission, sizeof(void*), false)) {
        return false;
    }
    void** vtable = nullptr;
    std::memcpy(&vtable, mission, sizeof(vtable));
    if (!vtable || !memory_range_accessible(vtable, sizeof(void*) * 7u, false) ||
        !vtable[6]) {
        return false;
    }
    a2fo_sol_call_thiscall_1(
        vtable[6], mission, reinterpret_cast<std::uintptr_t>(object));
    return true;
}

extern "C" std::uintptr_t __cdecl A2FOSOLMaps_IsAiControlledSafe(void* object) {
    struct Context {void* object;std::int32_t team=0;void* ai=nullptr;
        void* table=nullptr;std::exception_ptr error{};} context{object};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return c.object&&g_armada;
        case 1:{auto* field=static_cast<std::uint8_t*>(c.object)+kGameObjectTeamOffset;
            if(!memory_range_accessible(field,sizeof(c.team),false))return 0;
            std::memcpy(&c.team,field,sizeof(c.team));return 1;}
        case 2:{
            auto** slot=reinterpret_cast<void**>(at(kAiGlobalPointerRva));
            if(memory_range_accessible(slot,sizeof(c.ai),false))
                std::memcpy(&c.ai,slot,sizeof(c.ai));
            if(c.ai&&memory_range_accessible(static_cast<std::uint8_t*>(c.ai)+sizeof(void*),
                    sizeof(c.table),false))
                std::memcpy(&c.table,static_cast<std::uint8_t*>(c.ai)+sizeof(void*),sizeof(c.table));
            return c.ai&&c.table;}
        case 3:return g_selected_valid;
        case 4:{const LONG pass=InterlockedIncrement(&g_sol_ai_null_guard_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass)){char message[320]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j AI-null guard #%ld: GameObject=%p team=%ld; "
                    "AI singleton/team table not initialized, reporting non-AI-controlled",
                    static_cast<long>(pass),c.object,static_cast<long>(c.team));
                log_line(message);}return 0;}
        case 5:return a2fo_sol_call_thiscall_1(at(kAiIsTeamControlledRva),c.ai,
            static_cast<std::uintptr_t>(c.team))!=0u;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_ai_guard(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

// Nebula::Simulate normally receives a project/AiPath companion from the
// serialized Nebula::Load payload. Parser-spawned SOL nebulas intentionally
// do not deserialize that incompatible payload yet. If +0x188 is null during
// a selected SOL, bypass only the Nebula-specific refresh and preserve the
// common GameObject simulation. Ordinary BZN nebulas and any SOL nebula whose
// companion is later initialized still use the untouched native gateway.
bool __attribute__((fastcall)) sol_nebula_simulate_hook(
    void* nebula, void*) noexcept {
    struct Context {void* nebula;void* path=nullptr;bool readable=false;}
        context{nebula};
    const auto step=+[](void* opaque,std::uint32_t op,
                        std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid&&c.nebula;
        case 1:{auto* slot=static_cast<std::uint8_t*>(c.nebula)+kNebulaSerializedPathOffset;
            c.readable=memory_range_accessible(slot,sizeof(c.path),false);
            if(c.readable)std::memcpy(&c.path,slot,sizeof(c.path));
            return c.readable;}
        case 2:return c.path==nullptr;
        case 3:{const LONG pass=InterlockedIncrement(&g_sol_nebula_null_guard_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass)){char message[320]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2l Nebula null-state guard #%ld: nebula=%p serialized companion +0x188 is null; running common GameObject simulate only",
                    static_cast<long>(pass),c.nebula);log_line(message);}return 0;}
        case 4:return a2fo_sol_call_thiscall_0(at(kGameObjectSimulateRva),c.nebula)!=0u;
        case 5:return g_sol_nebula_simulate_hook.gateway!=nullptr;
        case 6:return a2fo_sol_call_thiscall_0(g_sol_nebula_simulate_hook.gateway,c.nebula)!=0u;
        default:return 0;
        }}catch(...){std::terminate();}
    };
    return a2fo_rs_c_sol_nebula_guard(&context,step);
}

struct SolSpawnSummary {
    std::uint32_t attempted = 0;
    std::uint32_t created = 0;
    std::uint32_t missing_class = 0;
    std::uint32_t construct_failed = 0;
    std::uint32_t mission_add_failed = 0;
};

bool spawn_selected_sol_objects(SolSpawnSummary* summary_out = nullptr) noexcept {
    struct Context {
        SolSpawnSummary summary{};SolSpawnSummary* output;
        void* (__cdecl* find_class)(const char*)=nullptr;
        const a2fo::sol::Object* source=nullptr;void* object_class=nullptr;
        void* object=nullptr;const char* label=nullptr;
    } context{{},summary_out};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t index) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid&&g_armada;
        case 1:return static_cast<std::uintptr_t>(InterlockedCompareExchange(&g_manual_spawn_state,1,0));
        case 2:
            c.summary.attempted=a2fo_rs_stage9_sol_count_size(g_selected_map.objects.size());
            c.summary.created=a2fo_rs_stage9_sol_nonnegative_counter(g_manual_spawn_created);
            c.summary.construct_failed=a2fo_rs_stage9_sol_nonnegative_counter(g_manual_spawn_failed);
            if(c.output)*c.output=c.summary;
            return 0;
        case 3:
            if(c.output)*c.output=c.summary;
            return 0;
        case 4:
            c.find_class=reinterpret_cast<void* (__cdecl*)(const char*)>(at(kGameObjectClassFindRva));
            return c.find_class&&ensure_empty_mission()&&ensure_empty_game_object_list();
        case 5:apply_sol_world_bounds();return 0;
        case 6:c.summary.attempted=a2fo_rs_stage9_sol_count_size(g_selected_map.objects.size());return 0;
        case 7:return g_selected_map.objects.size();
        case 8:c.source=&g_selected_map.objects[index];c.object_class=c.find_class(c.source->odf.c_str());return c.object_class!=nullptr;
        case 9:{
            c.summary.missing_class=a2fo_rs_stage9_sol_increment_counter(c.summary.missing_class);
            if(a2fo_rs_stage9_sol_log_first16(c.summary.missing_class)){char message[384]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j object %lu/%lu skipped: ODF class '%s' was not found",
                    static_cast<unsigned long>(index+1u),
                    static_cast<unsigned long>(g_selected_map.objects.size()),c.source->odf.c_str());
                log_line(message);}return 0;
        }
        case 10:{
            NativeMatrix34 transform{};
            std::memcpy(transform.values,c.source->transform.data(),sizeof(transform.values));
            c.label=a2fo_rs_stage9_sol_label_odf(c.source->instance_name.size())
                ?c.source->odf.c_str():c.source->instance_name.c_str();
            c.object=reinterpret_cast<void*>(a2fo_sol_call_thiscall_4(
                at(kGameObjectClassConstructRva),c.object_class,
                reinterpret_cast<std::uintptr_t>(&transform),
                static_cast<std::uintptr_t>(c.source->team),0u,
                reinterpret_cast<std::uintptr_t>(c.label)));
            return c.object!=nullptr;
        }
        case 11:{
            c.summary.construct_failed=a2fo_rs_stage9_sol_increment_counter(c.summary.construct_failed);
            if(a2fo_rs_stage9_sol_log_first16(c.summary.construct_failed)){char message[384]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j object %lu/%lu construct failed: odf='%s' label='%s' team=%ld",
                    static_cast<unsigned long>(index+1u),
                    static_cast<unsigned long>(g_selected_map.objects.size()),
                    c.source->odf.c_str(),c.label,static_cast<long>(c.source->team));
                log_line(message);}return 0;
        }
        case 12:{
            c.summary.created=a2fo_rs_stage9_sol_increment_counter(c.summary.created);
            if(a2fo_rs_stage9_sol_log_index32(index)){char message[512]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j constructed %lu/%lu: odf='%s' label='%s' team=%ld object=%p; publishing to mission",
                    static_cast<unsigned long>(index+1u),
                    static_cast<unsigned long>(g_selected_map.objects.size()),
                    c.source->odf.c_str(),c.label,static_cast<long>(c.source->team),c.object);
                log_line(message);}return 0;
        }
        case 13:
            { const bool added=add_object_to_current_mission(c.object);
              a2fo_rs_stage9_sol_record_mission_add(added,&c.summary.mission_add_failed); }
            return 0;
        case 14:{
            if(a2fo_rs_stage9_sol_log_index32(index)){char message[512]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j spawned %lu/%lu: odf='%s' label='%s' team=%ld pos=(%.2f,%.2f,%.2f) object=%p",
                    static_cast<unsigned long>(index+1u),
                    static_cast<unsigned long>(g_selected_map.objects.size()),
                    c.source->odf.c_str(),c.label,static_cast<long>(c.source->team),
                    c.source->position.x,c.source->position.y,c.source->position.z,c.object);
                log_line(message);}return 0;
        }
        case 15:
            InterlockedExchange(&g_manual_spawn_created,static_cast<LONG>(c.summary.created));
            InterlockedExchange(&g_manual_spawn_failed,static_cast<LONG>(
                a2fo_rs_stage9_sol_sum_failures(c.summary.missing_class,c.summary.construct_failed)));
            InterlockedExchange(&g_manual_spawn_state,2);
            if(c.output)*c.output=c.summary;
            return 0;
        case 16:InterlockedExchange(&g_manual_spawn_state,0);return 0;
        default:return 0;
        }}catch(...){std::terminate();}
    };
    return a2fo_rs_c_sol_spawn(&context,step);
}

void* create_map_details(const a2fo::sol::Map& map, const std::string& fo_full_path) {
    struct Context {
        const a2fo::sol::Map& map;const std::string& path;
        OperatorNewFn allocate=nullptr;MapDetailsConstructorFn construct=nullptr;
        void* details=nullptr;void** starts=nullptr;std::uint32_t count=0;
        std::exception_ptr error{};
    } context{map,fo_full_path};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            auto* bytes=static_cast<std::uint8_t*>(c.details);
            switch(operation){
            case 0:
                if(!g_armada)return 0;
                c.allocate=reinterpret_cast<OperatorNewFn>(at(kOperatorNewRva));
                c.construct=reinterpret_cast<MapDetailsConstructorFn>(at(kMapDetailsConstructorRva));
                return c.allocate&&c.construct;
            case 1:c.details=c.allocate(kMapDetailsSize);return reinterpret_cast<std::uintptr_t>(c.details);
            case 2:c.details=c.construct(c.details);return reinterpret_cast<std::uintptr_t>(c.details);
            case 3:assign_native_string(c.details,kMapDetailsGameTypeOffset,c.map.game_type);return 0;
            case 4:assign_native_string(c.details,kMapDetailsFilenameOffset,c.map.filename);return 0;
            case 5:assign_native_string(c.details,kMapDetailsDescriptionOffset,
                a2fo_rs_stage9_sol_title_filename(c.map.title.size())
                    ?c.map.filename:c.map.title);return 0;
            case 6:assign_native_string(c.details,kMapDetailsFullPathOffset,c.path);return 0;
            case 7:assign_native_string(c.details,kMapDetailsAuthorOffset,a2fo_rs_stage9_sol_empty_metadata());return 0;
            case 8:assign_native_string(c.details,kMapDetailsMissionScriptOffset,a2fo_rs_stage9_sol_empty_metadata());return 0;
            case 9:std::memcpy(bytes+kMapDetailsMinimumExtentOffset,&c.map.minimum_extent,sizeof(c.map.minimum_extent));return 0;
            case 10:std::memcpy(bytes+kMapDetailsSizeOffset,&c.map.size,sizeof(c.map.size));return 0;
            case 11:{
                const auto* slot_count=reinterpret_cast<const std::int32_t*>(at(kGameSetupSlotCountRva));
                const auto read_slot=+[](void* raw) noexcept -> std::int32_t {
                    return *static_cast<const std::int32_t*>(raw);
                };
                c.count=a2fo_rs_stage9_sol_start_count(
                    const_cast<std::int32_t*>(slot_count),slot_count!=nullptr,
                    read_slot,c.map.start_locations.size());
                std::memcpy(bytes+kMapDetailsStartLocationCountOffset,&c.count,sizeof(c.count));return 0;
            }
            case 12:std::memcpy(&c.starts,bytes+kMapDetailsStartLocationArrayOffset,sizeof(c.starts));return c.starts!=nullptr;
            case 13:return c.count;
            case 14:{
                auto* start=static_cast<std::uint8_t*>(c.starts[index]);
                if(!start)return 0;
                const auto& source=c.map.start_locations[index];
                std::memcpy(start+kStartLocationPositionOffset,&source.position,sizeof(source.position));
                const std::string type=a2fo_rs_stage9_sol_type_empty(source.type.size())
                    ?std::string("empty"):std::string(source.type);
                assign_native_string(start,kStartLocationTypeOffset,type);
                return 0;
            }
            case 15:return reinterpret_cast<std::uintptr_t>(c.details);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_create_map_details(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return reinterpret_cast<void*>(result);
}

}  // namespace

extern "C" __declspec(dllexport)
void __cdecl A2FOSOLMaps_ClearSelected();

extern "C" __declspec(dllexport)
void* __cdecl A2FOSOLMaps_CreateMapDetails(const char* filename,
                                           int selected_map) {
    try {
        struct Context {
            const char* filename;bool selected;SolSource source{};
            a2fo::sol::Map map{};std::string error{},full_path{};
            std::exception_ptr exception{};
        } context{filename,selected_map!=0};
        const auto step=+[](void* opaque,std::uint32_t op,
                            std::size_t) noexcept -> std::uintptr_t {
            auto& c=*static_cast<Context*>(opaque);
            try{switch(op){
            case 0:return resolve_source(c.filename,c.source);
            case 1:log_line(std::string("SOL MapDetails source not found: '")+
                            (c.filename?c.filename:"<null>")+"'");return 0;
            case 2:return a2fo::sol::parse_file(c.source.path,c.map,&c.error);
            case 3:log_line("SOL parse rejected '"+c.source.path+"': "+c.error);return 0;
            case 4:g_selected_map=c.map;g_selected_path=c.source.path;g_selected_valid=true;return 0;
            case 5:{char message[640]{};
                std::snprintf(message,sizeof(message),
                    "SOL MapDetails: '%s' title='%s' version=%lu starts=%lu objects=%lu%s",
                    c.source.path.c_str(),c.map.title.c_str(),
                    static_cast<unsigned long>(c.map.version),
                    static_cast<unsigned long>(c.map.start_locations.size()),
                    static_cast<unsigned long>(c.map.objects.size()),
                    a2fo_rs_stage9_sol_selected_suffix(c.selected));
                log_line(message);return 0;
            }
            case 6:return a2fo_rs_stage9_sol_mission_present(c.map.mission_script.size());
            case 7:log_line("SOL mission script temporarily suppressed: '"+
                            c.map.mission_script+"'");return 0;
            case 8:{
                c.full_path=a2fo_rs_stage9_sol_virtual_prefix()+c.source.basename;
                char fields[640]{};
                std::snprintf(fields,sizeof(fields),
                    "SOL native MapDetails strings: type='%s' filename='%s' description='%s' fullpath='%s'",
                    c.map.game_type.c_str(),c.map.filename.c_str(),
                    (a2fo_rs_stage9_sol_title_filename(c.map.title.size())?c.map.filename:c.map.title).c_str(),
                    c.full_path.c_str());
                log_line(fields);return 0;
            }
            case 9:return reinterpret_cast<std::uintptr_t>(create_map_details(c.map,c.full_path));
            default:return 0;
            }}catch(...){c.exception=std::current_exception();return UINTPTR_MAX;}
        };
        const auto result=a2fo_rs_c_sol_create_selected(&context,
            selected_map!=0,step);
        if(context.exception)std::rethrow_exception(context.exception);
        return reinterpret_cast<void*>(result);
    } catch (...) {
        log_line("SOL MapDetails bridge caught an unexpected exception");
        return nullptr;
    }
}


std::uintptr_t sol_map_details_hook(const char* filename,bool selected){
    struct Context {const char* filename;bool selected;std::exception_ptr error{};}
        context{filename,selected};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept ->std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:A2FOSOLMaps_ClearSelected();return 0;
        case 1:return g_armada&&c.filename;
        case 2:return has_sol_extension(c.filename);
        case 3:log_line(std::string("SOL selected-map MapDetails hook: '")+c.filename+"'");return 0;
        case 6:{const LONG pass=InterlockedIncrement(&g_map_details_hook_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass))log_line(std::string("SOL discovery MapDetails hook: '")+
                c.filename+"'");return 0;}
        case 4:return reinterpret_cast<std::uintptr_t>(
            A2FOSOLMaps_CreateMapDetails(c.filename,c.selected?1:0));
        case 5:{const auto original=reinterpret_cast<MapDetailsFactoryLoadFn>(
            at(kMapDetailsFactoryLoadRva));
            return reinterpret_cast<std::uintptr_t>(original?original(c.filename):nullptr);}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_map_hook(&context,selected,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" std::uintptr_t __cdecl a2fo_sol_load_map_details(
    const char* filename) {return sol_map_details_hook(filename,false);}

extern "C" std::uintptr_t __cdecl a2fo_sol_load_selected_map_details(
    const char* filename) {return sol_map_details_hook(filename,true);}

extern "C" __declspec(dllexport)
void __cdecl A2FOSOLMaps_ClearSelected() {
    struct Context {std::exception_ptr error{};} context{};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(operation){
        case 0:g_selected_map=a2fo::sol::Map{};break;
        case 1:g_selected_path.clear();break;
        case 2:g_selected_valid=false;break;
        case 3:InterlockedExchange(&g_manual_spawn_state,0);break;
        case 4:InterlockedExchange(&g_manual_spawn_created,0);break;
        case 5:InterlockedExchange(&g_manual_spawn_failed,0);break;
        case 6:InterlockedExchange(&g_sol_ai_null_guard_count,0);break;
        case 7:InterlockedExchange(&g_sol_nebula_null_guard_count,0);break;
        default:break;
        }return 0;}catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_sol_lifecycle(&context,0,step);
    if(context.error)std::rethrow_exception(context.error);
}

extern "C" __declspec(dllexport)
int __cdecl A2FOSOLMaps_IsSelected() {
    return g_selected_valid ? 1 : 0;
}

extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadGameObjects(void* file_reader) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return 0;
#endif
    struct Context {void* reader;FileReaderLoadFn original;
        FileReaderCursorState before{};bool have=false,valid=false,bounds=false;
        std::size_t boundary=0;SolSpawnSummary summary{};
        std::exception_ptr error{};
    } context{file_reader,reinterpret_cast<FileReaderLoadFn>(at(kGameObjectsLoadRva))};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid;
        case 1:return c.original&&c.original(c.reader);
        case 2:c.have=inspect_file_reader_cursor(c.reader,&c.before);
               c.boundary=g_selected_map.object_count_record_offset;return c.have;
        case 3:c.valid=validate_sol_object_boundary(c.before,c.boundary);return c.valid;
        case 4:c.bounds=apply_sol_world_bounds();return 0;
        case 5:return spawn_selected_sol_objects(&c.summary);
        case 6:{
            const LONG pass=InterlockedIncrement(&g_runtime_object_load_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass)){char message[896]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2j object bridge #%ld: expectedOffset=0x%lx, before=0x%lx, "
                    "marker=%s, bounds=%s, parsed=%lu, created=%lu, missingClass=%lu, "
                    "constructFailed=%lu, missionAddFailed=%lu, liveObjects=%lu; native GameObjects::Load bypassed",
                    static_cast<long>(pass),static_cast<unsigned long>(c.boundary),
                    static_cast<unsigned long>(c.have?c.before.offset:0),
                    c.valid?"valid":"unreached",c.bounds?"yes":"no",
                    static_cast<unsigned long>(c.summary.attempted),
                    static_cast<unsigned long>(c.summary.created),
                    static_cast<unsigned long>(c.summary.missing_class),
                    static_cast<unsigned long>(c.summary.construct_failed),
                    static_cast<unsigned long>(c.summary.mission_add_failed),
                    static_cast<unsigned long>(live_object_count()));
                log_line(message);}return 0;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_load_stage(&context,0,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadA2CraftClassTable(void* file_reader) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return 0;
#endif
    struct Context {void* reader;std::exception_ptr error{};} context{file_reader};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid;
        case 1:{const auto original=reinterpret_cast<FileReaderLoadFn>(at(kA2CraftClassTableLoadRva));
               return original&&original(c.reader);}
        case 2:{
            const LONG pass=InterlockedIncrement(&g_runtime_table_skip_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass)){FileReaderCursorState state{};
                inspect_file_reader_cursor(c.reader,&state);
                char message[320]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2 load-order bridge #%ld: skipped A2-only craft-class table at cursor=0x%lx",
                    static_cast<long>(pass),static_cast<unsigned long>(state.offset));
                log_line(message);}return 0;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_load_stage(&context,1,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void* __attribute__((thiscall)) A2FOSOLMaps_LoadGameVersionProbe(
    void* result, std::uintptr_t argument1, std::uintptr_t argument2) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return result;
#endif
    struct Context {
        void* result;std::uintptr_t arg1,arg2;void* returned=nullptr;
        std::uint32_t version=0;std::uintptr_t reader=0,load_context=0,details=0;
        bool context_synthesized=false,details_synthesized=false;
        void* early_mission=nullptr;void* early_list=nullptr;
        std::exception_ptr error{};
    } context{result,argument1,argument2};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{
            auto* bytes=static_cast<std::uint8_t*>(c.result);
            switch(op){
            case 0:{
                const auto original=reinterpret_cast<LoadGameVersionProbeFn>(at(kLoadGameVersionProbeRva));
                c.returned=original?original(c.result,c.arg1,c.arg2):c.result;
                return 0;
            }
            case 1:return g_selected_valid&&c.result&&memory_range_accessible(c.result,24u,true);
            case 2:
                std::memcpy(&c.version,bytes+4u,sizeof(c.version));
                std::memcpy(&c.reader,bytes+0x0cu,sizeof(c.reader));
                std::memcpy(&c.load_context,bytes+0x10u,sizeof(c.load_context));
                std::memcpy(&c.details,bytes+0x14u,sizeof(c.details));
                return a2fo_rs_stage9_sol_probe_candidate(c.version,g_selected_map.version);
            case 3:{
                const auto allocate=reinterpret_cast<OperatorNewFn>(at(kOperatorNewRva));
                const auto construct=reinterpret_cast<DefaultLoadContextCtorFn>(at(kLoadGameDefaultContextCtorRva));
                void* storage=allocate?allocate(kLoadGameDefaultContextSize):nullptr;
                void* object=storage&&construct?construct(storage):nullptr;
                if(object){c.load_context=reinterpret_cast<std::uintptr_t>(object);
                    std::memcpy(bytes+0x10u,&c.load_context,sizeof(c.load_context));
                    c.context_synthesized=true;}return 0;
            }
            case 4:{
                const std::string full_path=a2fo_rs_stage9_sol_virtual_prefix()+basename_of(g_selected_path);
                void* object=create_map_details(g_selected_map,full_path);
                if(object){c.details=reinterpret_cast<std::uintptr_t>(object);
                    std::memcpy(bytes+0x14u,&c.details,sizeof(c.details));
                    c.details_synthesized=true;}return 0;
            }
            case 5:c.early_mission=ensure_empty_mission();
                   c.early_list=ensure_empty_game_object_list();return 0;
            case 6:{bytes[0]=0u;const std::uint32_t compatible=a2fo_rs_stage9_sol_compatible_version();
                std::memcpy(bytes+4u,&compatible,sizeof(compatible));return 0;}
            case 7:{
                const LONG pass=InterlockedIncrement(&g_runtime_version_bypass_count);
                if(a2fo_rs_stage9_sol_log_pass16(pass)){char message[640]{};
                    std::snprintf(message,sizeof(message),
                        "SOL phase2j probe synthesis #%ld: nativeVersion=%lu, reader=%p, "
                        "context=%p (%s), mapDetails=%p (%s), earlyMission=%p, "
                        "gameObjectList=%p, callerVersion=%lu; FileReader stream remains Legacy",
                        static_cast<long>(pass),static_cast<unsigned long>(c.version),
                        reinterpret_cast<void*>(c.reader),reinterpret_cast<void*>(c.load_context),
                        c.context_synthesized?"synthesized":"native",
                        reinterpret_cast<void*>(c.details),
                        c.details_synthesized?"synthesized":"native",
                        c.early_mission,c.early_list,
                        static_cast<unsigned long>(a2fo_rs_stage9_sol_caller_version(c.version,
                            c.reader!=0,c.load_context!=0,c.details!=0,c.early_mission!=nullptr,c.early_list!=nullptr)));
                    log_line(message);}return 0;
            }
            case 8:log_line("SOL phase2j probe synthesis incomplete: refusing to advertise a compatible version because LoadGame/setup would dereference missing state");return 0;
            case 9:return reinterpret_cast<std::uintptr_t>(c.returned);
            case 11:return a2fo_rs_stage9_sol_probe_gate(11,c.reader!=0,c.load_context!=0,c.details!=0,c.early_mission!=nullptr,c.early_list!=nullptr);
            case 12:return a2fo_rs_stage9_sol_probe_gate(12,c.reader!=0,c.load_context!=0,c.details!=0,c.early_mission!=nullptr,c.early_list!=nullptr);
            case 13:return a2fo_rs_stage9_sol_probe_gate(13,c.reader!=0,c.load_context!=0,c.details!=0,c.early_mission!=nullptr,c.early_list!=nullptr);
            case 14:return a2fo_rs_stage9_sol_probe_gate(14,c.reader!=0,c.load_context!=0,c.details!=0,c.early_mission!=nullptr,c.early_list!=nullptr);
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto returned=a2fo_rs_c_sol_probe(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return reinterpret_cast<void*>(returned);
}


extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadGameMain(void* file_reader) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return 0;
#endif
    struct Context {
        void* reader;LoadGameMainLoadFn original;FileReaderCursorState entry{},after{};
        bool have_entry=false,have_after=false,native_loaded=false,bounds=false;
        LONG objects_before=0;SolSpawnSummary summary{};void* mission=nullptr;
        std::exception_ptr error{};
    } context{file_reader,reinterpret_cast<LoadGameMainLoadFn>(at(kLoadGameMainLoadRva))};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(operation){
        case 0:return g_selected_valid;
        case 1:return c.original&&c.original(c.reader);
        case 2:c.have_entry=inspect_file_reader_cursor(c.reader,&c.entry);return 0;
        case 3:c.objects_before=g_runtime_object_load_count;return 0;
        case 4:c.native_loaded=c.original&&c.original(c.reader);return c.native_loaded;
        case 5:c.have_after=inspect_file_reader_cursor(c.reader,&c.after);return 0;
        case 6:return a2fo_rs_stage9_sol_counter_changed(g_runtime_object_load_count,c.objects_before);
        case 7:c.mission=ensure_empty_mission();return c.mission!=nullptr;
        case 8:{
            char message[768]{};
            std::snprintf(message,sizeof(message),
                "SOL phase2j MainLoad bridge: nativeLoaded=%s, objectBridgeReached=yes, "
                "entry=0x%lx, afterNative=0x%lx, spawned=%ld, failed=%ld, "
                "liveObjects=%lu, mission=%p; returning object-only success",
                c.native_loaded?"yes":"no",
                static_cast<unsigned long>(c.have_entry?c.entry.offset:0),
                static_cast<unsigned long>(c.have_after?c.after.offset:0),
                static_cast<long>(g_manual_spawn_created),
                static_cast<long>(g_manual_spawn_failed),
                static_cast<unsigned long>(live_object_count()),c.mission);
            log_line(message);return 0;
        }
        case 9:return a2fo_rs_stage9_sol_spawn_complete(g_manual_spawn_state);
        case 10:log_line("SOL phase2j MainLoad unexpectedly succeeded before the object bridge; preserving native success");return 0;
        case 11:c.bounds=apply_sol_world_bounds();return 0;
        case 12:return spawn_selected_sol_objects(&c.summary);
        case 13:c.mission=ensure_empty_mission();return c.mission!=nullptr;
        case 14:{
            char message[896]{};
            std::snprintf(message,sizeof(message),
                "SOL phase2j MainLoad recovery: nativeLoaded=no, objectBridgeReached=no, "
                "entry=0x%lx, failedAt=0x%lx, bounds=%s, parsed=%lu, created=%lu, "
                "missingClass=%lu, constructFailed=%lu, missionAddFailed=%lu, "
                "liveObjects=%lu, mission=%p",
                static_cast<unsigned long>(c.have_entry?c.entry.offset:0),
                static_cast<unsigned long>(c.have_after?c.after.offset:0),
                c.bounds?"yes":"no",
                static_cast<unsigned long>(c.summary.attempted),
                static_cast<unsigned long>(c.summary.created),
                static_cast<unsigned long>(c.summary.missing_class),
                static_cast<unsigned long>(c.summary.construct_failed),
                static_cast<unsigned long>(c.summary.mission_add_failed),
                static_cast<unsigned long>(live_object_count()),c.mission);
            log_line(message);return 0;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_load_main(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}


extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadAiMission(void* file_reader) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return 0;
#endif
    struct Context {void* reader;AiMissionLoadFn original;
        FileReaderCursorState before{};std::size_t boundary=0;
        bool have=false,valid=false,realigned=false,created=false;
        void* mission=nullptr;std::exception_ptr error{};
    } context{file_reader,reinterpret_cast<AiMissionLoadFn>(at(kAiMissionLoadRva))};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid;
        case 1:return c.original&&c.original(c.reader);
        case 2:c.have=inspect_file_reader_cursor(c.reader,&c.before);
               c.boundary=g_selected_map.runtime_mission_record_offset;return c.have;
        case 3:c.valid=validate_sol_mission_boundary(c.before,c.boundary);return c.valid;
        case 4:c.realigned=set_reader_cursor_to_offset(c.reader,c.boundary);return c.realigned;
        case 5:{
            auto** current_slot=reinterpret_cast<void**>(at(kAiMissionCurrentRva));
            const bool slot_ok=memory_range_accessible(current_slot,sizeof(*current_slot),true);
            void* before_mission=slot_ok?*current_slot:nullptr;
            c.mission=ensure_empty_mission();c.created=!before_mission&&c.mission;
            return c.mission!=nullptr;
        }
        case 6:{
            const LONG pass=InterlockedIncrement(&g_runtime_mission_load_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass)){char message[768]{};
                std::snprintf(message,sizeof(message),
                    "SOL phase2h AiMission bypass #%ld: expectedOffset=0x%lx, "
                    "before=0x%lx, marker=%s, realigned=%s, emptyMissionCreated=%s, "
                    "mission=%p; Legacy DSL/DRL payload not deserialized",
                    static_cast<long>(pass),static_cast<unsigned long>(c.boundary),
                    static_cast<unsigned long>(c.have?c.before.offset:0),
                    c.valid?"valid":"invalid",c.realigned?"yes":"no",
                    c.created?"yes":"no",c.mission);
                log_line(message);}return 0;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_load_stage(&context,2,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::uintptr_t load_post_mission(void* file_reader,bool second) {
#if defined(A2FO_BUILTIN_STARTUP)
    if (!g_api || !g_armada) return 0;
#endif
    struct Context {void* reader;bool second;std::exception_ptr error{};}
        context{file_reader,second};
    const auto step=+[](void* opaque,std::uint32_t op,std::size_t) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_selected_valid;
        case 1:{const auto original=reinterpret_cast<FileReaderLoadFn>(
            at(c.second?kPostMissionBLoadRva:kPostMissionALoadRva));
            return original&&original(c.reader);}
        case 2:{const LONG pass=InterlockedIncrement(&g_runtime_post_mission_skip_count);
            if(a2fo_rs_stage9_sol_log_pass16(pass))log_line(a2fo_rs_stage9_sol_post_mission_message(c.second));
            return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINTPTR_MAX;}
    };
    const auto result=a2fo_rs_c_sol_load_stage(&context,second?4u:3u,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadPostMissionA(void* file_reader) {
    return load_post_mission(file_reader,false);
}

extern "C" __declspec(dllexport)
std::uintptr_t __cdecl A2FOSOLMaps_LoadPostMissionB(void* file_reader) {
    return load_post_mission(file_reader,true);
}

extern "C" __declspec(dllexport)
int __cdecl A2FOSOLMaps_GetSelectedSummary(
    char* path, std::uint32_t path_size,
    std::uint32_t* object_count, std::uint32_t* start_count) {
    struct Context {char* path;std::uint32_t size;
        std::uint32_t* objects;std::uint32_t* starts;}
        context{path,path_size,object_count,start_count};
    const auto step=+[](void* opaque,std::uint32_t op,
                        std::size_t) noexcept ->std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:std::snprintf(c.path,c.size,"%s",g_selected_path.c_str());return 0;
        case 1:*c.objects=static_cast<std::uint32_t>(g_selected_map.objects.size());return 0;
        case 2:*c.starts=static_cast<std::uint32_t>(g_selected_map.start_locations.size());return 0;
        default:return 0;
        }
    };
    return a2fo_rs_c_sol_summary(&context,g_selected_valid,path!=nullptr,
        path_size,object_count!=nullptr,start_count!=nullptr,step);
}

extern "C" void __cdecl a2fo_sol_add_maps_with_mask_dispatch(
    void* known_maps, const char* native_mask) {
    struct Context {
        void* known_maps;const char* native_mask;LONG pass=0;
        void* function=nullptr;decltype(g_sources)::iterator iterator{};
        std::exception_ptr error{};
    } context{known_maps,native_mask};
    const auto step=+[](void* opaque,std::uint32_t op,
                        std::size_t) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:return g_armada&&c.known_maps;
        case 1:
            c.pass=InterlockedIncrement(&g_discovery_dispatch_count);
            if(a2fo_rs_stage9_sol_discovery_log_pass(c.pass))log_line(std::string("SOL KnownMaps discovery pass #")+
                std::to_string(static_cast<long>(c.pass))+": nativeMask='"+
                (c.native_mask?c.native_mask:"<null>")+"', indexedSOL="+
                std::to_string(g_sources.size()));
            return 0;
        case 2:c.function=at(kKnownMapsAddMapsWithMaskRva);return c.function!=nullptr;
        case 3:return a2fo_rs_stage9_sol_input_present(reinterpret_cast<const std::uint8_t*>(c.native_mask));
        case 4:a2fo_sol_call_thiscall_1(c.function,c.known_maps,
                   reinterpret_cast<std::uintptr_t>(c.native_mask));return 0;
        case 5:c.iterator=g_sources.begin();return 0;
        case 6:return c.iterator!=g_sources.end();
        case 7:{
            std::string parent=c.iterator->second.path;
            const std::size_t slash=a2fo_rs_stage9_sol_parent_end(
                reinterpret_cast<const std::uint8_t*>(parent.data()),parent.size());
            if(slash!=std::string::npos)parent.resize(slash);
            const std::string leaf=lower_ascii(basename_of(parent));
            return a2fo_rs_stage9_sol_is_bzn_leaf(
                reinterpret_cast<const std::uint8_t*>(leaf.data()),leaf.size());
        }
        case 8:++c.iterator;return 0;
        case 9:if(a2fo_rs_stage9_sol_discovery_log_pass(c.pass))log_line("SOL KnownMaps virtual discovery mask: 'bzn\\*.sol'");return 0;
        case 10:{const char* mask=a2fo_rs_stage9_sol_virtual_mask();
            a2fo_sol_call_thiscall_1(c.function,c.known_maps,
                reinterpret_cast<std::uintptr_t>(mask));return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_sol_discovery(&context,step);
    if(context.error)std::rethrow_exception(context.error);
}

bool sol_init_signature(std::uint32_t group,std::size_t index) noexcept {
    struct Check {std::uintptr_t rva;const std::uint8_t* bytes;std::size_t size;};
    static constexpr Check exclusive[]{
        {kLoadGameVersionProbeCallRva,kExpectedLoadGameVersionProbeCall,sizeof(kExpectedLoadGameVersionProbeCall)},
        {kLoadGameMainLoadCallRva,kExpectedLoadGameMainLoadCall,sizeof(kExpectedLoadGameMainLoadCall)},
        {kLoadGamePostMissionACallRva,kExpectedLoadGamePostMissionACall,sizeof(kExpectedLoadGamePostMissionACall)},
        {kLoadGamePostMissionBCallRva,kExpectedLoadGamePostMissionBCall,sizeof(kExpectedLoadGamePostMissionBCall)},
        {kAiMissionGetCurrentRva,kExpectedAiMissionGetCurrent,sizeof(kExpectedAiMissionGetCurrent)},
        {kGameObjectClassFindRva,kExpectedGameObjectClassFind,sizeof(kExpectedGameObjectClassFind)},
        {kGameObjectClassConstructRva,kExpectedGameObjectClassConstruct,sizeof(kExpectedGameObjectClassConstruct)},
        {kGameObjectIsAiControlledRva,kExpectedGameObjectIsAiControlled,sizeof(kExpectedGameObjectIsAiControlled)},
        {kAiIsTeamControlledRva,kExpectedAiIsTeamControlled,sizeof(kExpectedAiIsTeamControlled)},
        {kNebulaSimulateRva,kExpectedNebulaSimulate,sizeof(kExpectedNebulaSimulate)},
    };
    static constexpr Check shared[]{
        {kGameSetupLoadMapDetailsCallRva,kExpectedGameSetupLoadMapDetailsCall,sizeof(kExpectedGameSetupLoadMapDetailsCall)},
        {kKnownMapsLoadMapDetailsCallRva,kExpectedKnownMapsLoadMapDetailsCall,sizeof(kExpectedKnownMapsLoadMapDetailsCall)},
    };
    static constexpr Check runtime[]{
        {kLoadGameObjectsLoadCallRva,kExpectedLoadGameObjectsLoadCall,sizeof(kExpectedLoadGameObjectsLoadCall)},
        {kLoadGameA2CraftClassTableCallRva,kExpectedLoadGameA2CraftClassTableCall,sizeof(kExpectedLoadGameA2CraftClassTableCall)},
        {kLoadGameAiMissionLoadCallRva,kExpectedLoadGameAiMissionLoadCall,sizeof(kExpectedLoadGameAiMissionLoadCall)},
    };
    static constexpr Check discovery[]{
        {kKnownMapsConstructorMaskCallRva,kExpectedKnownMapsConstructorMaskCall,sizeof(kExpectedKnownMapsConstructorMaskCall)},
        {kKnownMapsRefreshMaskCallRva,kExpectedKnownMapsRefreshMaskCall,sizeof(kExpectedKnownMapsRefreshMaskCall)},
    };
    const Check* checks=nullptr;std::size_t count=0;
    switch(group){
    case 3:checks=exclusive;count=std::size(exclusive);break;
    case 5:checks=shared;count=std::size(shared);break;
    case 7:checks=runtime;count=std::size(runtime);break;
    case 15:checks=discovery;count=std::size(discovery);break;
    default:return false;
    }
    if(index>=count)return false;
    const auto& check=checks[index];
    return signature_matches(check.rva,check.bytes,check.size);
}

bool sol_init_patch(const A2FO_ModuleApi* api,std::uint32_t group,
                    std::size_t index) {
    if(group==9){switch(index){
    case 0:return api->patch_call(at(kGameSetupLoadMapDetailsCallRva),reinterpret_cast<void*>(&a2fo_sol_load_selected_map_details),kExpectedGameSetupLoadMapDetailsCall,sizeof(kExpectedGameSetupLoadMapDetailsCall));
    case 1:return api->patch_call(at(kKnownMapsLoadMapDetailsCallRva),reinterpret_cast<void*>(&a2fo_sol_load_map_details),kExpectedKnownMapsLoadMapDetailsCall,sizeof(kExpectedKnownMapsLoadMapDetailsCall));
    case 2:return api->patch_call(at(kLoadGameObjectsLoadCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadGameObjects),kExpectedLoadGameObjectsLoadCall,sizeof(kExpectedLoadGameObjectsLoadCall));
    case 3:return api->patch_call(at(kLoadGameA2CraftClassTableCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadA2CraftClassTable),kExpectedLoadGameA2CraftClassTableCall,sizeof(kExpectedLoadGameA2CraftClassTableCall));
    case 4:return api->patch_call(at(kLoadGameAiMissionLoadCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadAiMission),kExpectedLoadGameAiMissionLoadCall,sizeof(kExpectedLoadGameAiMissionLoadCall));
    default:return false;
    }}
    if(group==13){switch(index){
    case 0:return api->patch_call(at(kLoadGameVersionProbeCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadGameVersionProbe),kExpectedLoadGameVersionProbeCall,sizeof(kExpectedLoadGameVersionProbeCall));
    case 1:return api->patch_call(at(kLoadGameMainLoadCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadGameMain),kExpectedLoadGameMainLoadCall,sizeof(kExpectedLoadGameMainLoadCall));
    case 2:return api->patch_call(at(kLoadGamePostMissionACallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadPostMissionA),kExpectedLoadGamePostMissionACall,sizeof(kExpectedLoadGamePostMissionACall));
    case 3:return api->patch_call(at(kLoadGamePostMissionBCallRva),reinterpret_cast<void*>(&A2FOSOLMaps_LoadPostMissionB),kExpectedLoadGamePostMissionBCall,sizeof(kExpectedLoadGamePostMissionBCall));
    case 4:return api->patch_jump(at(kGameObjectIsAiControlledRva),reinterpret_cast<void*>(&a2fo_sol_is_ai_controlled_hook),kExpectedGameObjectIsAiControlled,sizeof(kExpectedGameObjectIsAiControlled));
    case 5:return api->install_inline_hook(at(kNebulaSimulateRva),reinterpret_cast<void*>(&sol_nebula_simulate_hook),sizeof(kExpectedNebulaSimulate),kExpectedNebulaSimulate,&g_sol_nebula_simulate_hook);
    default:return false;
    }}
    if(group==17){switch(index){
    case 0:return api->patch_call(at(kKnownMapsConstructorMaskCallRva),reinterpret_cast<void*>(&a2fo_sol_add_maps_with_mask_hook),kExpectedKnownMapsConstructorMaskCall,sizeof(kExpectedKnownMapsConstructorMaskCall));
    case 1:return api->patch_call(at(kKnownMapsRefreshMaskCallRva),reinterpret_cast<void*>(&a2fo_sol_add_maps_with_mask_hook),kExpectedKnownMapsRefreshMaskCall,sizeof(kExpectedKnownMapsRefreshMaskCall));
    default:return false;
    }}
    return false;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->patch_call || !api->patch_jump ||
        !api->extension_root_count || !api->extension_root) {
        return false;
    }
    g_api = api;
    struct Context {
        const A2FO_ModuleApi* api;
        decltype(g_sources)::iterator iterator{};
        std::exception_ptr error{};
    } context{api};
    const auto step=+[](void* opaque,std::uint32_t op,
                        std::size_t index) noexcept -> std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:g_armada=static_cast<HMODULE>(c.api->armada_module());return g_armada!=nullptr;
        case 1:return build_source_index();
        case 2:return a2fo::startup_module_provider(
            "A1Compat.dll",{"a2fo_a1_render_legacy_resource_panel"})!=nullptr;
        case 3:case 5:case 7:case 15:return sol_init_signature(op,index);
        case 4:log_line("SOL phase2l exclusive runtime hooks unavailable: version-gate/MainLoad/post-mission/object-factory/AI-guard signatures do not match the supported ArmadaL build (preflight; no exclusive patches applied)");return 0;
        case 6:log_line("SOL MapDetails hooks unavailable: call-site signatures do not match the supported ArmadaL build (preflight; no patches applied)");return 0;
        case 8:log_line("SOL phase2 runtime hooks unavailable: LoadGame call-site signatures do not match the supported ArmadaL build (preflight; no patches applied)");return 0;
        case 9:case 13:case 17:return sol_init_patch(c.api,op,index);
        case 10:log_line("SOL phase2 hook installation failed after successful signature preflight");return 0;
        case 11:log_line("A1Compat inactive; A2FOSOLMaps owns shared MapDetails and phase2 LoadGame call-site hooks");return 0;
        case 12:log_line("A1Compat active; using its shared MapDetails/LoadGame bridges for SOL maps");return 0;
        case 14:log_line("SOL phase2l exclusive runtime hook installation failed after successful signature preflight");return 0;
        case 16:log_line("SOL map discovery unavailable: KnownMaps call-site signatures do not match the supported ArmadaL build (preflight; no discovery patches applied)");return 0;
        case 18:log_line("SOL map discovery hook installation failed after successful signature preflight");return 0;
        case 19:
            log_line("SOL compatibility phase 2k enabled: discovery/MapDetails plus parsed Legacy object spawning, SOL probe-result synthesis, early EmptyMission/object-list scaffolding, AI-null publication guard and MainLoad object-only recovery active");
            log_line("Legacy DRL/DSL execution is fully bypassed for SOL bring-up; Phase 2k bypasses Legacy polymorphic object deserialization and instantiates every parser-decoded SOL object through GameObjectClass::Find/Construct; GameObject::IsAIControlled is null-safe until native AI exists; parser-spawned nebulas also bypass only their missing serialized-path refresh when +0x188 is null");
            return 0;
        case 20:return g_sources.empty();
        case 21:log_line("No .sol maps found under extension-root sol/ or bzn/ directories");return 0;
        case 22:log_line("Indexed "+std::to_string(g_sources.size())+" winning SOL map(s)");return 0;
        case 23:c.iterator=g_sources.begin();return 0;
        case 24:return c.iterator!=g_sources.end();
        case 25:log_line("Further indexed SOL source paths suppressed");return 0;
        case 26:log_line("Indexed SOL source: "+c.iterator->second.path);return 0;
        case 27:++c.iterator;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_sol_module_init(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    struct Context {std::exception_ptr error{};} context{};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(operation){
        case 0:g_selected_map=a2fo::sol::Map{};break;
        case 1:g_selected_path.clear();break;
        case 2:g_selected_valid=false;break;
        case 3:InterlockedExchange(&g_discovery_dispatch_count,0);break;
        case 4:InterlockedExchange(&g_map_details_hook_count,0);break;
        case 5:InterlockedExchange(&g_runtime_object_load_count,0);break;
        case 6:InterlockedExchange(&g_runtime_mission_load_count,0);break;
        case 7:InterlockedExchange(&g_runtime_table_skip_count,0);break;
        case 8:InterlockedExchange(&g_runtime_version_bypass_count,0);break;
        case 9:InterlockedExchange(&g_runtime_post_mission_skip_count,0);break;
        case 10:InterlockedExchange(&g_manual_spawn_state,0);break;
        case 11:InterlockedExchange(&g_manual_spawn_created,0);break;
        case 12:InterlockedExchange(&g_manual_spawn_failed,0);break;
        case 13:InterlockedExchange(&g_sol_nebula_null_guard_count,0);break;
        case 14:g_sources.clear();break;
        case 15:g_armada=nullptr;break;
        case 16:g_api=nullptr;break;
        default:break;
        }return 0;}catch(...){c.error=std::current_exception();return -1;}
    };
    a2fo_rs_c_sol_lifecycle(&context,1,step);
    if(context.error)std::rethrow_exception(context.error);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }

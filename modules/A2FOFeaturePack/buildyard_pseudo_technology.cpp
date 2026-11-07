/*
 * File: modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp
 * Module: A2FOFeaturePack
 * Purpose: Let BuildYard/Borg module slots use symbolic XML technology gates.
 *
 * v14: selected native tree and extended predicates; v13 tooltip path retained.
 * Existing gate design (unchanged):
 *   - XML technology trees may declare <object MODULE="name.odf"> blocks.
 *   - MODULE entries do not need a backing ODF and do not consume a Fleet Ops
 *     project ID.  BuildYard receives an A2FO-owned synthetic ID instead.
 *   - Direct <requirement><obj>...</obj></requirement> conditions are evaluated
 *     with the native XML object-count helper (completed count, excluding queued
 *     objects), matching stock <obj> and mustNotHave semantics.
 *   - Without logicGroups, requirement blocks are OR alternatives. Grouped
 *     requirements AND within each group and OR across groups. Objects and
 *     scoped ally/enemy blocks AND within each requirement; race is supported.
 *   - Only XML observed in the native selected-tree load supplies gameplay
 *     rules. Native include/parentInclude/FOFS selection remains authoritative.
 *   - moduleXTechnologyEntry is canonical and moduleXPseudoTechnology remains
 *     a compatibility alias.
 *   - If the same module also declares moduleXRequiredTechnology, v6 preserves
 *     it and ANDs it with the symbolic MODULE gate.
 *   - The module's moduleXButtonTooltip key is associated with the symbolic gate.
 *     v11 hooks Fleet Ops' private BuildYard module-button tooltip provider at
 *     RVA 0x13d4a4. The native provider first emits the complete AUTOTOOLTIP
 *     into Armada's ostream, then appends missing requirements through MSVCP60.
 *     This is the actual Borg module hover
 *     path and avoids the unrelated StandardComponent/localization hooks tried
 *     by v6-v9.
 *   - The previous real-ODF/project-ID gate remains as a compatibility fallback
 *     when no MODULE definition exists for the requested name.
 */

#include "buildyard_pseudo_technology.hpp"
#include "buildyard_tooltip_native.hpp"
#include "module_technology_policy.hpp"
#include "buildyard_tree_signatures.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace a2fo {

std::uint32_t resolve_featurepack_odf_project_id(const char* raw_name) noexcept;

namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kParameterDbGetProjectIdRva = 0x00135200;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kLocalizationLookupRva = 0x00081c90;
constexpr std::uintptr_t kTooltipRenderRva = 0x00105380;
constexpr std::uintptr_t kTooltipRenderVerboseRva = 0x00105720;
constexpr std::size_t kTooltipTextOffset = 0x38;
constexpr std::uintptr_t kStandardComponentSetTooltipTextRva = 0x0010c040;
constexpr std::uintptr_t kStandardComponentSetVerboseTooltipTextRva = 0x0010c080;
constexpr std::uintptr_t kLocalTeamRva = 0x000d0060;

// FleetOpsHook.dll RVAs from the supported build.
constexpr std::uintptr_t kBuildYardRequiredTechnologyCallRva = 0x0013b5cb;
// BuildYard's module-button tooltip provider. The current FleetOpsHook binary
// exposes this as the shared GetTooltip/GetVerboseTooltip implementation for
// the private Borg module button class. It reads moduleXButtonTooltip directly
// from the BuildYard module configuration and writes to Armada's tooltip stream.
constexpr std::uintptr_t kBuildYardModuleTooltipProviderRva = 0x0013d4a4;
// These two FleetOpsHook data slots are the exact helpers used by the native
// BuildYard module tooltip provider at +0x13d4a4.  In the supported build
// they currently resolve to ArmadaL 0x00482300 (AUTOTOOLTIP/localized-string
// resolver) and 0x00672d9c (stream insertion), but reading FleetOps' slots
// keeps the integration tied to the provider actually in use.
constexpr std::uintptr_t kNativeTooltipResolveSlotRva = 0x00210a48;
constexpr std::uintptr_t kNativeTooltipInsertSlotRva = 0x0020e640;
// Imported MSVCP60 helpers, verified by import names AND native call sites.
constexpr std::uintptr_t kNativeStringTidyIatRva = 0x003b7dcc;
constexpr std::uintptr_t kNativeStringAssignIatRva = 0x003b7dd8;
constexpr std::uintptr_t kNativeStringInsertIatRva = 0x003b7dac;
constexpr std::uintptr_t kNativeStreamSnapshotIatRva = 0x003b7db0;
buildyard_tooltip_native::Api g_native_tooltip_api;
constexpr std::uintptr_t kFleetOpsParameterDbGetProjectIdRva = 0x001e2d70;
constexpr std::uintptr_t kProjectIdTechnologyAvailableRva = 0x001def64;
constexpr std::uintptr_t kTechnologyObjectCountRva = 0x0011f0cc;
constexpr std::uintptr_t kNativeTeamCount = 9;

// Known-good recursive team technology evaluator retained for the v4
// real-ODF compatibility path.
constexpr std::uintptr_t kTeamTechnologyTreesPointerRva = 0x00212f08;
constexpr std::uintptr_t kTechnologyTreeAllowsProjectRva = 0x00120680;
constexpr std::size_t kTechnologyTreeItemsOffset = 0x0c;
constexpr std::int32_t kMaximumTeamIndex = 63;

constexpr std::size_t kMaximumModuleIndex = 1023;
constexpr std::uint32_t kSyntheticProjectIdStart = 0x70000000u;

constexpr std::array<std::uint8_t, 9> kExpectedParameterDbGetProjectId{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
constexpr std::array<std::uint8_t, 5> kExpectedRequiredTechnologyCall{
    0xe8, 0xa0, 0x77, 0x0a, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedBuildYardModuleTooltipProvider{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe8};
constexpr std::array<std::uint8_t, 6> kExpectedTechnologyAvailable{
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4};
constexpr std::array<std::uint8_t, 16> kExpectedTechnologyTreeAllowsProject{
    0x53, 0x51, 0x89, 0x14, 0x24, 0x33, 0xd2, 0x8b,
    0x40, 0x0c, 0x8b, 0x0c, 0x24, 0x8b, 0x5c, 0x88};
// Complete count routine, verified against native <obj> callers at +1214d8.
constexpr std::array<std::uint8_t, 36> kExpectedTechnologyObjectCount{
    0x53, 0x51, 0x89, 0x14, 0x24, 0x33, 0xd2, 0x8b, 0x40, 0x0c,
    0x8b, 0x1c, 0x24, 0x8b, 0x44, 0x98, 0xfc, 0x85, 0xc0, 0x74,
    0x0a, 0x8b, 0x50, 0x14, 0x84, 0xc9, 0x74, 0x03, 0x03, 0x50,
    0x18, 0x8b, 0xc2, 0x5a, 0x5b, 0xc3};
constexpr std::array<std::uint8_t, 11> kExpectedLocalTeam{
    0x8b, 0x0d, 0xd4, 0xb8, 0x76,
    0x00, 0xe9, 0x45, 0x78, 0x08, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedTooltipRender{
    0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x38};
constexpr std::array<std::uint8_t, 6> kExpectedTooltipRenderVerbose{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x58};
constexpr std::array<std::uint8_t, 9> kExpectedSetTooltipText{
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x20};
constexpr std::array<std::uint8_t, 9> kExpectedSetVerboseTooltipText{
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x24};

extern "C" void a2fo_buildyard_call_tooltip_provider(
    void* function, void* button, void* stream);
extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self);
extern "C" std::uintptr_t a2fo_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
extern "C" std::uintptr_t a2fo_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
extern "C" std::uintptr_t a2fo_buildyard_call_delphi_reg3_stack1(
    void* function, void* argument_eax, void* argument_edx,
    void* argument_ecx, std::uintptr_t stack_argument);
extern "C" std::uintptr_t a2fo_buildyard_call_delphi_reg2(
    void* function, std::uintptr_t argument_eax,
    std::uintptr_t argument_edx);
extern "C" void a2fo_buildyard_project_id_call_bridge();
extern "C" void a2fo_buildyard_localization_lookup_bridge();
extern "C" std::uintptr_t __cdecl
    a2fo_buildyard_localization_lookup_handler(void* manager,
                                                const char* key) noexcept;

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
A2FO_InlineHook g_technology_available_hook{};
A2FO_InlineHook g_buildyard_module_tooltip_hook{};
A2FO_InlineHook g_localization_lookup_hook{};
A2FO_InlineHook g_tooltip_render_hook{};
A2FO_InlineHook g_tooltip_render_verbose_hook{};
A2FO_InlineHook g_set_tooltip_text_hook{};
A2FO_InlineHook g_set_verbose_tooltip_text_hook{};
CRITICAL_SECTION g_policy_lock;
bool g_policy_lock_ready = false;
bool g_tooltip_runtime_ready = false;
bool g_requirement_team_ready = false;
bool g_native_count_ready = false;
std::uint32_t g_next_synthetic_project_id = kSyntheticProjectIdStart;

using ModuleObjectRequirement = module_technology_policy::Object;
using ModuleRequirementGroup = module_technology_policy::Requirement;
using ModuleTechnologyDefinition = module_technology_policy::Definition;

struct SymbolicBinding {
    std::string gate_key;
    std::uint32_t native_required_project_id = 0;
    std::string tooltip_key;
};

struct GateEvaluation {
    bool available = false;
    bool definition_missing = false;
    bool definition_unsupported = false;
    bool native_required_missing = false;
    std::string gate_key;
    std::string symbolic_missing;
};

// v4 compatibility: real project IDs declared by moduleXTechnologyEntry or
// moduleXPseudoTechnology are redirected through FO's recursive XML/.tt gate.
std::unordered_set<std::uint32_t> g_legacy_recursive_project_ids;

// v5 symbolic MODULE definitions and live BuildYard bindings.
std::unordered_map<std::string, ModuleTechnologyDefinition>
    g_module_technologies;
std::unordered_map<std::uint32_t, SymbolicBinding> g_symbolic_bindings;
std::unordered_map<std::string, std::uint32_t> g_tooltip_bindings;
std::unordered_map<std::string, std::uint32_t> g_requirement_project_ids;
std::unordered_map<std::string, std::string> g_odf_display_names;
std::unordered_set<std::string> g_logged_missing_requirements;
std::unordered_set<std::string> g_logged_tooltip_collisions;
std::unordered_set<std::string> g_logged_tooltip_appends;
std::unordered_set<std::string> g_logged_render_tooltip_lines;
std::unordered_set<std::string> g_logged_setter_tooltip_lines;
std::unordered_map<void*, std::string> g_component_module_tooltips;
std::unordered_map<void*, std::string> g_component_module_verbose_tooltips;

class PolicyLockGuard {
public:
    PolicyLockGuard() noexcept {
        if (g_policy_lock_ready) {
            EnterCriticalSection(&g_policy_lock);
            locked_ = true;
        }
    }

    ~PolicyLockGuard() {
        if (locked_) LeaveCriticalSection(&g_policy_lock);
    }

    PolicyLockGuard(const PolicyLockGuard&) = delete;
    PolicyLockGuard& operator=(const PolicyLockGuard&) = delete;

private:
    bool locked_ = false;
};

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uintptr_t>(module) + rva);
}

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

bool readable_range(const void* pointer, std::size_t length) noexcept {
    if (!pointer || length == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    const auto region_begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto region_end = region_begin + info.RegionSize;
    return begin >= region_begin && begin <= region_end &&
        length <= region_end - begin;
}

bool writable_range(void* pointer, std::size_t length) noexcept {
    if (!readable_range(pointer, length)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    const DWORD protection = info.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
T read_live_at(const void* object, std::size_t offset,
               T fallback = T{}) noexcept {
    if (!object) return fallback;
    const auto* address = static_cast<const std::uint8_t*>(object) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) {
    return module && readable_range(at(module, rva), expected.size()) &&
        std::memcmp(at(module, rva), expected.data(), expected.size()) == 0;
}

char lower_ascii_char(char value) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}

bool buildyard_classify(std::uint8_t byte,std::uint32_t kind) noexcept {
    switch(kind){
    case 0:return std::isspace(byte)!=0;
    case 1:return std::isalpha(byte)!=0;
    case 2:return std::isalnum(byte)!=0;
    default:return false;
    }
}
std::uint8_t buildyard_lower(std::uint8_t byte) noexcept {
    return static_cast<std::uint8_t>(lower_ascii_char(static_cast<char>(byte)));
}
A2foStage8BuildyardBytes buildyard_bytes(const std::string& value) {
    return {reinterpret_cast<const std::uint8_t*>(value.data()),value.size()};
}

std::string lower_ascii(std::string value) {
    a2fo_rs_c_buildyard_lower(
        reinterpret_cast<std::uint8_t*>(value.data()),value.size(),
        buildyard_lower);
    return value;
}

std::string trim_copy(const std::string& input) {
    A2foStage8BuildyardSpan span{};
    a2fo_rs_c_buildyard_trim(buildyard_bytes(input),buildyard_classify,&span);
    return input.substr(span.first,span.length);
}

bool ends_with_case_insensitive(const std::string& value,
                                const char* suffix) noexcept {
    if(!suffix)return false;
    const std::size_t size=std::strlen(suffix);
    return a2fo_rs_c_buildyard_ends(buildyard_bytes(value),
        {reinterpret_cast<const std::uint8_t*>(suffix),size},buildyard_lower);
}

std::string buildyard_normalized(const std::string& input,std::uint32_t mode) {
    std::string result;
    struct Context {std::string& result;std::exception_ptr error{};}
        context{result};
    const auto emit=+[](void* opaque,
        const A2foStage8BuildyardEvent* event) noexcept ->bool {
        auto& c=*static_cast<Context*>(opaque);
        try {c.result.assign(reinterpret_cast<const char*>(event->a.data),
                             event->a.size);return true;}
        catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_buildyard_normalize(buildyard_bytes(input),mode,
        buildyard_classify,buildyard_lower,&context,emit);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
std::string normalize_odf_key(const std::string& raw) {
    return buildyard_normalized(raw,0);
}
std::string basename_without_extension(const std::string& key) {
    return buildyard_normalized(key,1);
}

bool ascii_equal_case_insensitive(char left,char right) noexcept {
    return lower_ascii_char(left)==lower_ascii_char(right);
}

bool parse_required_technology_command(const char* command,
                                       std::size_t* module_index) noexcept {
    if(!module_index||!command)return false;
    struct Context {const char* command;std::size_t* index;}
        context{command,module_index};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(operation){
        case 0:return static_cast<unsigned char>(c.command[arg]);
        case 1:{const std::size_t index=arg>>8;
            const char expected=static_cast<char>(arg&0xff);
            return c.command[index]&&
                ascii_equal_case_insensitive(c.command[index],expected);}
        case 2:return std::isdigit(static_cast<unsigned char>(c.command[arg]))!=0;
        case 3:*c.index=arg;return 0;
        default:return 0;
        }
    };
    return a2fo_rs_c_buildyard_parse_command(
        &context,kMaximumModuleIndex,step);
}

void copy_delphi_string(void* value,char* output,
                        std::size_t output_size) noexcept {
    if(!output||output_size==0)return;
    struct Context {const char* text;char* output;}
        context{static_cast<const char*>(value),output};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(operation){
        case 0:c.output[0]='\0';return 0;
        case 1:return c.text!=nullptr;
        case 2:return readable_range(c.text+index,1);
        case 3:return static_cast<unsigned char>(c.text[index]);
        case 4:c.output[index]=c.text[index];c.output[index+1]='\0';return 0;
        default:return 0;
        }
    };
    a2fo_rs_c_buildyard_copy_delphi(&context,output_size,step);
}

bool read_project_id_command(void* parameter_db, const char* command_name,
                             std::uint32_t* project_id) noexcept {
    if (!parameter_db || !command_name || !project_id) return false;
    *project_id = 0;
    const std::uintptr_t found = a2fo_call_thiscall_3(
        at(g_armada, kParameterDbGetProjectIdRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command_name),
        reinterpret_cast<std::uintptr_t>(project_id), 0);
    return a2fo_rs_c_buildyard_byte_truth(found);
}

bool read_string_command(void* parameter_db, const char* command_name,
                         std::string* value) noexcept {
    if (!parameter_db || !command_name || !value) return false;
    std::array<char, 512> output{};
    const std::uintptr_t found = a2fo_call_thiscall_4(
        at(g_armada, kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(command_name),
        reinterpret_cast<std::uintptr_t>(output.data()),
        static_cast<std::uintptr_t>(output.size()),
        reinterpret_cast<std::uintptr_t>(""));
    output.back() = '\0';
    value->assign(output.data());
    return a2fo_rs_c_buildyard_byte_truth(found);
}

struct TechnologyEntryLookup {
    bool found = false;
    bool legacy_alias = false;
    std::string raw_name;
};

TechnologyEntryLookup read_module_technology_entry(
    void* parameter_db,std::size_t module_index) noexcept {
    TechnologyEntryLookup lookup{};
    std::array<char,64> command{};
    struct Context {
        void* parameter_db;
        std::size_t module_index;
        TechnologyEntryLookup& lookup;
        std::array<char,64>& command;
        std::string raw_name{};
        std::exception_ptr error{};
    } context{parameter_db,module_index,lookup,command};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return std::snprintf(c.command.data(),c.command.size(),
            "Module%lu%s",static_cast<unsigned long>(c.module_index),
            arg==0?"TechnologyEntry":"PseudoTechnology");
        case 1:return read_string_command(c.parameter_db,c.command.data(),
            &c.raw_name);
        case 2:c.lookup.found=true;c.lookup.legacy_alias=arg!=0;
            c.lookup.raw_name=trim_copy(c.raw_name);return 0;
        case 3:c.command.fill('\0');return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_tech_entry(&context,command.size(),step);
    if(context.error)std::terminate();
    return lookup;
}

std::string read_module_tooltip_key(void* parameter_db,
                                    std::size_t module_index) noexcept {
    std::array<char,64> command{};
    std::string value;
    std::string result;
    struct Context {
        void* parameter_db;
        std::size_t module_index;
        std::array<char,64>& command;
        std::string& value;
        std::string& result;
        std::exception_ptr error{};
    } context{parameter_db,module_index,command,value,result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return std::snprintf(c.command.data(),c.command.size(),
            "Module%luButtonTooltip",
            static_cast<unsigned long>(c.module_index));
        case 1:return read_string_command(c.parameter_db,c.command.data(),
            &c.value);
        case 2:c.result=trim_copy(c.value);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_tooltip_key(&context,command.size(),step);
    if(context.error)std::terminate();
    return result;
}

// ---------- MODULE XML parsing ------------------------------------------------

bool read_text_file(const std::string& path,std::string* output) {
    struct Context {
        const std::string& path;
        std::string* output;
        std::optional<std::ifstream> file{};
        std::streamoff size=0;
        std::exception_ptr error{};
    } context{path,output};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return c.output!=nullptr;
        case 1:c.file.emplace(c.path,std::ios::binary);
            return static_cast<bool>(*c.file);
        case 2:c.file->seekg(0,std::ios::end);c.size=c.file->tellg();
            return c.size;
        case 3:c.file->seekg(0,std::ios::beg);
            c.output->assign(std::istreambuf_iterator<char>(*c.file),
                std::istreambuf_iterator<char>());return 0;
        case 4:return c.file->good()||c.file->eof();
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_read_text(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

void register_module_definition(ModuleTechnologyDefinition definition) {
    if(!a2fo_rs_c_buildyard_has_bytes(buildyard_bytes(definition.key)))return;
    PolicyLockGuard lock;
    struct Context {
        ModuleTechnologyDefinition& definition;
        std::unordered_map<std::string,ModuleTechnologyDefinition>::iterator existing{};
        std::exception_ptr error{};
    } context{definition};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.existing=g_module_technologies.find(c.definition.key);
            return c.existing!=g_module_technologies.end()&&
                c.existing->second.source!=c.definition.source;
        case 1:log_line("MODULE technology '"+c.definition.key+"' from "+
            c.definition.source+" overrides "+c.existing->second.source);
            return 0;
        case 2:g_module_technologies[c.definition.key]=std::move(c.definition);
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_register_definition(&context,step);
    if(context.error)std::rethrow_exception(context.error);
}

std::size_t parse_module_definitions_from_xml(const std::string& path) {
    struct Context {
        const std::string& path;
        std::string text{};
        module_technology_policy::ParseResult parsed{};
        std::exception_ptr error{};
    } context{path};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return read_text_file(c.path,&c.text);
        case 1:c.parsed=module_technology_policy::parse_document(c.text,c.path);
            return static_cast<bool>(c.parsed);
        case 2:log_line("MODULE catalog XML rejected: "+c.path+": "+
            c.parsed.error);return 0;
        case 3:return c.parsed.definitions.size();
        case 4:register_module_definition(
            std::move(c.parsed.definitions[index]));return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool accepted=a2fo_rs_c_buildyard_parse_catalog(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return accepted?context.parsed.definitions.size():0;
}

std::vector<std::string> enumerate_xml_files(const std::string& directory) {
    std::vector<std::string> files;
    std::string pattern=directory;
    if(!pattern.empty()&&pattern.back()!='\\'&&pattern.back()!='/')
        pattern.push_back('\\');
    pattern+="*.xml";
    struct Context {
        const std::string& directory;
        const std::string& pattern;
        std::vector<std::string>& files;
        WIN32_FIND_DATAA data{};
        HANDLE find=INVALID_HANDLE_VALUE;
        std::exception_ptr error{};
    } context{directory,pattern,files};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.find=FindFirstFileA(c.pattern.c_str(),&c.data);
            return c.find!=INVALID_HANDLE_VALUE;
        case 1:return (c.data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        case 2:{std::string path=c.directory;
            if(!path.empty()&&path.back()!='\\'&&path.back()!='/')
                path.push_back('\\');
            path+=c.data.cFileName;c.files.push_back(std::move(path));
            return 0;}
        case 3:return FindNextFileA(c.find,&c.data);
        case 4:FindClose(c.find);return 0;
        case 5:std::sort(c.files.begin(),c.files.end(),
            [](const std::string& left,const std::string& right){
                return lower_ascii(left)<lower_ascii(right);
            });return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_enumerate_xml(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return files;
}

std::size_t load_symbolic_module_technologies() {
    if(!g_api||!A2FO_MODULE_API_HAS(g_api,extension_root_count)||
        !A2FO_MODULE_API_HAS(g_api,extension_root)||
        !g_api->extension_root_count||!g_api->extension_root)return 0;
    struct Context {
        std::string directory{};
        std::vector<std::string> files{};
        std::exception_ptr error{};
    } context{};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return g_api->extension_root_count();
        case 1:{const char* root=g_api->extension_root(
                static_cast<std::uint32_t>(index));
            if(!root||!*root)return 0;
            c.directory=root;
            if(!c.directory.empty()&&c.directory.back()!='\\'&&
                c.directory.back()!='/')c.directory.push_back('\\');
            c.directory+="techtree";return 1;}
        case 2:c.files=enumerate_xml_files(c.directory);return c.files.size();
        case 3:return parse_module_definitions_from_xml(c.files[index]);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const std::size_t result=a2fo_rs_c_buildyard_load_symbolic(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool has_module_definition(const std::string& gate_key) {
    PolicyLockGuard lock;
    return g_module_technologies.find(gate_key) !=
        g_module_technologies.end();
}

// ---------- requirement/display helpers --------------------------------------

std::string string_view_copy(A2FO_StringView view) {
    return view.data && view.size != 0
        ? std::string(view.data, view.data + view.size)
        : std::string{};
}

void A2FO_CALL object_class_loaded_for_module_tooltips(
    const A2FO_GameObjectClassLoadedEvent* event,void*) {
    if(!event||!event->source_odf.data||event->source_odf.size==0)return;
    struct Context {
        const A2FO_GameObjectClassLoadedEvent* event;
        std::string key{};
        std::string unit_name{};
        std::exception_ptr error{};
    } context{event};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.key=normalize_odf_key(
            string_view_copy(c.event->source_odf));return !c.key.empty();
        case 1:return 1;
        case 2:{const auto& field=c.event->odf_fields[index];
            if(lower_ascii(string_view_copy(field.name))!="unitname")return 0;
            c.unit_name=trim_copy(string_view_copy(field.value));return 1;}
        case 3:return !c.unit_name.empty();
        case 4:{PolicyLockGuard lock;
            g_odf_display_names[c.key]=std::move(c.unit_name);return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_class_loaded(&context,event->odf_field_count,step);
    if(context.error)std::rethrow_exception(context.error);
}

std::string display_name_for_requirement(const std::string& odf) {
    std::string result;
    struct Context {
        const std::string& odf;
        std::string& result;
        std::exception_ptr error{};
    } context{odf,result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:{PolicyLockGuard lock;
            const auto found=g_odf_display_names.find(c.odf);
            if(found==g_odf_display_names.end()||found->second.empty())return 0;
            c.result=found->second;return 1;}
        case 1:c.result=basename_without_extension(c.odf);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_display_name(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::uint32_t requirement_project_id(const std::string& odf) noexcept {
    struct Context {
        const std::string& odf;
        std::uint32_t resolved=0;
        std::exception_ptr error{};
    } context{odf};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:{PolicyLockGuard lock;
            const auto found=g_requirement_project_ids.find(c.odf);
            return found!=g_requirement_project_ids.end()?found->second:0;}
        case 1:c.resolved=resolve_featurepack_odf_project_id(c.odf.c_str());
            return c.resolved;
        case 2:{PolicyLockGuard lock;
            g_requirement_project_ids[c.odf]=c.resolved;return 0;}
        case 3:{PolicyLockGuard lock;
            return g_logged_missing_requirements.insert(c.odf).second;}
        case 4:log_line("MODULE technology requirement '"+c.odf+
            "' could not resolve to a Fleet Operations project ID");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const auto result=a2fo_rs_c_buildyard_requirement_id(&context,step);
    if(context.error)std::terminate();
    return result;
}

using TechnologyAvailableFunction =
    bool (__attribute__((regparm(3))) *)(
        const std::uint32_t* project_id, std::uintptr_t team_index);

bool native_project_present(std::uint32_t project_id,
                            std::uintptr_t team_index) noexcept {
    const auto original = reinterpret_cast<TechnologyAvailableFunction>(
        g_technology_available_hook.gateway);
    if (!original || project_id == 0) return false;
    const std::uint32_t value = project_id;
    return original(&value, team_index);
}

// Unlike RequiredTechnology availability, XML <obj> checks completed ownership.
// Delphi register ABI: EAX=tree, EDX=project ID, ECX=include queued (zero here).
// The native routine indexes a Delphi dynamic array; validate its bounds first.
std::int32_t module_requirement_count(std::uint32_t project_id,
                                      std::uintptr_t team_index) noexcept {
    struct Context {
        std::uint32_t project_id;
        std::uintptr_t team_index;
        void* trees=nullptr;
        void* tree=nullptr;
        std::uint8_t* items=nullptr;
        std::uint8_t* slot=nullptr;
        const std::uint8_t* item=nullptr;
        std::exception_ptr error{};
    } context{project_id,team_index};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.trees=read_live_at<void*>(
            at(g_fleet_ops,kTeamTechnologyTreesPointerRva),0,nullptr);
            return 1;
        case 1:c.tree=read_live_at<void*>(c.trees,
            c.team_index*sizeof(void*),nullptr);return 1;
        case 2:c.items=read_live_at<std::uint8_t*>(c.tree,
            kTechnologyTreeItemsOffset,nullptr);
            return c.items&&readable_range(c.items-4,4);
        case 3:return *reinterpret_cast<const std::int32_t*>(c.items-4);
        case 4:c.slot=c.items+(c.project_id-1)*sizeof(void*);
            return readable_range(c.slot,sizeof(void*));
        case 5:c.item=*reinterpret_cast<const std::uint8_t* const*>(c.slot);
            return !c.item||readable_range(c.item+0x14,sizeof(std::int32_t));
        case 6:{using CountFunction=std::int32_t (
            __attribute__((regparm(3))) *)(void*,std::uint32_t,std::uint32_t);
            return reinterpret_cast<CountFunction>(at(g_fleet_ops,
                kTechnologyObjectCountRva))(c.tree,c.project_id,0);}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const auto result=a2fo_rs_c_buildyard_count(&context,
        g_native_count_ready,g_fleet_ops!=nullptr,project_id,team_index,
        kNativeTeamCount,step);
    if(context.error)std::terminate();
    return result;
}

bool recursive_technology_allows(std::uint32_t project_id,
                                 std::uintptr_t team_index) noexcept {
    struct Context {
        std::uint32_t project_id;
        std::uintptr_t team_index;
        void* technology_trees=nullptr;
        void* team_tree=nullptr;
        void* technology_items=nullptr;
        void* technology_item=nullptr;
        std::exception_ptr error{};
    } context{project_id,team_index};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.technology_trees=read_live_at<void*>(
            at(g_fleet_ops,kTeamTechnologyTreesPointerRva),0,nullptr);
            return c.technology_trees!=nullptr;
        case 1:c.team_tree=read_live_at<void*>(c.technology_trees,
            c.team_index*sizeof(void*),nullptr);
            return c.team_tree!=nullptr;
        case 2:c.technology_items=read_live_at<void*>(c.team_tree,
            kTechnologyTreeItemsOffset,nullptr);
            return c.technology_items!=nullptr;
        case 3:c.technology_item=read_live_at<void*>(c.technology_items,
            static_cast<std::size_t>(c.project_id-1)*sizeof(void*),nullptr);
            return c.technology_item!=nullptr;
        case 4:return a2fo_rs_c_buildyard_byte_truth(
            a2fo_buildyard_call_delphi_reg2(
                at(g_fleet_ops,kTechnologyTreeAllowsProjectRva),
                reinterpret_cast<std::uintptr_t>(c.team_tree),
                static_cast<std::uintptr_t>(c.project_id)));
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_recursive_allow(&context,
        project_id,team_index,kMaximumTeamIndex,step);
    if(context.error)std::terminate();
    return result;
}

#include "buildyard_tree_runtime.inl"

GateEvaluation evaluate_symbolic_binding(std::uint32_t synthetic_id,
                                         std::uintptr_t team_index) {
    GateEvaluation evaluation{};
    struct Context {
        std::uint32_t synthetic_id;
        std::uintptr_t team_index;
        GateEvaluation& evaluation;
        SymbolicBinding binding{};
        ModuleTechnologyDefinition definition{};
        std::exception_ptr error{};
    } context{synthetic_id,team_index,evaluation};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:{PolicyLockGuard lock;
            struct Select {
                Context& context;
                const std::unordered_map<std::string,
                    ModuleTechnologyDefinition>* definitions=nullptr;
                const ModuleTechnologyDefinition* selected=nullptr;
                std::unordered_map<std::uint32_t,SymbolicBinding>::iterator binding{};
                std::exception_ptr error{};
            } select{c};
            const auto select_step=+[](void* value,std::uint32_t op,
                                       std::size_t) noexcept ->std::int64_t {
                auto& s=*static_cast<Select*>(value);
                auto& c=s.context;
                try {switch(op){
                case 0:s.binding=g_symbolic_bindings.find(c.synthetic_id);
                    return s.binding!=g_symbolic_bindings.end();
                case 1:c.binding=s.binding->second;
                    c.evaluation.gate_key=c.binding.gate_key;return 0;
                case 2:return g_require_active_tree;
                case 3:if(c.team_index>=kNativeTeamCount||
                    !g_active_trees[c.team_index].ready||
                    g_active_trees[c.team_index].failed)return 0;
                    s.definitions=&g_active_trees[c.team_index].definitions;
                    return 1;
                case 4:if(!s.definitions)s.definitions=&g_module_technologies;
                    {const auto found=s.definitions->find(c.binding.gate_key);
                    if(found==s.definitions->end())return 0;
                    s.selected=&found->second;return 1;}
                case 5:c.evaluation.definition_missing=true;return 0;
                case 6:c.definition=*s.selected;return 0;
                default:return 0;
                }}catch(...){s.error=std::current_exception();return INT64_MIN;}
            };
            const bool selected=a2fo_rs_c_buildyard_symbolic_select(
                &select,select_step);
            if(select.error)std::rethrow_exception(select.error);
            return selected;}
        case 1:return c.binding.native_required_project_id;
        case 2:return c.definition.unsupported;
        case 3:{module_technology_policy::Callbacks callbacks;
            callbacks.object_count=[](std::string_view odf,int team){
                return module_requirement_count(
                    requirement_project_id(std::string(odf)),team);
            };
            callbacks.race_matches=&module_race_matches;
            callbacks.eligible_peer=&module_eligible_peer;
            callbacks.display_name=[](std::string_view odf){
                return display_name_for_requirement(std::string(odf));
            };
            const auto symbolic=module_technology_policy::evaluate(
                c.definition,static_cast<int>(c.team_index),callbacks);
            c.evaluation.symbolic_missing=symbolic.missing_text;
            return symbolic.available;}
        case 4:return native_project_present(
            c.binding.native_required_project_id,c.team_index);
        case 5:c.evaluation.native_required_missing=true;return 0;
        case 6:c.evaluation.definition_unsupported=true;return 0;
        case 7:c.evaluation.available=arg!=0;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_symbolic(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return evaluation;
}

bool is_legacy_recursive_project(std::uint32_t project_id) noexcept {
    if (project_id == 0 || !g_policy_lock_ready) return false;
    PolicyLockGuard lock;
    return g_legacy_recursive_project_ids.find(project_id) !=
        g_legacy_recursive_project_ids.end();
}

bool is_symbolic_project(std::uint32_t project_id) noexcept {
    if (project_id == 0 || !g_policy_lock_ready) return false;
    PolicyLockGuard lock;
    return g_symbolic_bindings.find(project_id) != g_symbolic_bindings.end();
}

bool __attribute__((regparm(3))) technology_available_hook(
    const std::uint32_t* project_id,std::uintptr_t team_index) noexcept {
    struct Context {
        const std::uint32_t* project_id;
        std::uintptr_t team_index;
        TechnologyAvailableFunction original=nullptr;
        std::uint32_t id=0;
        std::exception_ptr error{};
    } context{project_id,team_index};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:c.original=reinterpret_cast<TechnologyAvailableFunction>(
            g_technology_available_hook.gateway);return c.original!=nullptr;
        case 1:return c.project_id&&readable_range(c.project_id,
            sizeof(*c.project_id));
        case 2:return c.original(c.project_id,c.team_index);
        case 3:c.id=*c.project_id;return is_symbolic_project(c.id);
        case 4:try{return evaluate_symbolic_binding(
            c.id,c.team_index).available;}catch(...){return 0;}
        case 5:return is_legacy_recursive_project(c.id);
        case 6:return recursive_technology_allows(c.id,c.team_index);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_technology_dispatch(&context,step);
    if(context.error)std::terminate();
    return result;
}

std::uint32_t allocate_symbolic_binding(const std::string& gate_key,
                                        std::uint32_t native_required_project_id,
                                        const std::string& tooltip_key) {
    PolicyLockGuard lock;
    struct Context {
        const std::string& gate_key;
        std::uint32_t required;
        const std::string& tooltip_key;
        std::uint32_t synthetic=0;
        std::string tooltip_lookup{};
        std::unordered_map<std::string,std::uint32_t>::iterator existing{};
        std::exception_ptr error{};
    } context{gate_key,native_required_project_id,tooltip_key};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation){
        case 0:return g_next_synthetic_project_id==0x7fffffffu;
        case 1:g_next_synthetic_project_id=kSyntheticProjectIdStart;return 0;
        case 2:c.synthetic=g_next_synthetic_project_id++;return c.synthetic;
        case 3:return g_symbolic_bindings.find(c.synthetic)!=
            g_symbolic_bindings.end();
        case 4:{SymbolicBinding binding{};
            binding.gate_key=c.gate_key;
            binding.native_required_project_id=c.required;
            binding.tooltip_key=c.tooltip_key;
            g_symbolic_bindings.emplace(c.synthetic,std::move(binding));
            return 0;}
        case 5:return !c.tooltip_key.empty();
        case 6:c.tooltip_lookup=lower_ascii(c.tooltip_key);
            c.existing=g_tooltip_bindings.find(c.tooltip_lookup);
            return c.existing==g_tooltip_bindings.end();
        case 7:g_tooltip_bindings.emplace(c.tooltip_lookup,c.synthetic);
            return 0;
        case 8:{const auto old=g_symbolic_bindings.find(c.existing->second);
            return old==g_symbolic_bindings.end()||
                old->second.gate_key!=c.gate_key;}
        case 9:return g_logged_tooltip_collisions.insert(
            c.tooltip_lookup).second;
        case 10:log_line("BuildYard tooltip key '"+c.tooltip_key+
            "' is shared by different MODULE gates; the first gate controls appended requirement text");
            return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const auto result=a2fo_rs_c_buildyard_allocate_binding(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::int32_t local_team_index() noexcept;
std::string format_missing_requirements(const GateEvaluation& evaluation);

// Fleet Ops' private BuildYard module button does not use Armada's ordinary
// StandardComponent tooltip fields. Its virtual GetTooltip/GetVerboseTooltip
// method at FleetOpsHook+0x13d4a4 reads moduleXButtonTooltip from the config and
// writes the generated AUTOTOOLTIP text straight into Armada's tooltip stream.
// Hooking that provider is therefore the first boundary which has both the
// exact module slot and the real output stream.
// Do not send a literal to the localization resolver: unknown keys may resolve
// to an empty string. Build the same MSVC string through its native imports.
bool bind_native_tooltip_api() {
    using namespace buildyard_tooltip_native;
    struct Context {
        HMODULE runtime{};
        Api api{};
        void* insertion{};
        void** resolve_slot{};
        void** insert_slot{};
        std::uint8_t* thunk{};
        std::exception_ptr error{};
        void* import(std::uintptr_t rva,const char* name){
            auto** slot=at<void*>(g_armada,rva);
            void* exported=reinterpret_cast<void*>(GetProcAddress(runtime,name));
            if(!exported||!readable_range(slot,sizeof(*slot))||
               *slot!=exported){
                log_line(std::string("BuildYard requirement append disabled: import mismatch: ")+name);
                return nullptr;
            }
            return exported;
        }
    } context;
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:
            c.runtime=GetModuleHandleA("msvcp60.dll");
            if(!c.runtime)log_line("BuildYard requirement append disabled: MSVCP60 is not loaded");
            return c.runtime!=nullptr;
        case 1:
            c.api.tidy=reinterpret_cast<Tidy>(c.import(kNativeStringTidyIatRva,
                "?_Tidy@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AAEX_N@Z"));
            return 1;
        case 2:
            c.api.assign=reinterpret_cast<Assign>(c.import(kNativeStringAssignIatRva,
                "?assign@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEAAV12@PBDI@Z"));
            return 1;
        case 3:
            c.insertion=c.import(kNativeStringInsertIatRva,
                "??6std@@YAAAV?$basic_ostream@DU?$char_traits@D@std@@@0@AAV10@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@@Z");
            return 1;
        case 4:
            c.api.snapshot=reinterpret_cast<Snapshot>(c.import(
                kNativeStreamSnapshotIatRva,
                "?str@?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ"));
            return 1;
        case 5: {
            c.resolve_slot=at<void*>(g_fleet_ops,kNativeTooltipResolveSlotRva);
            c.insert_slot=at<void*>(g_fleet_ops,kNativeTooltipInsertSlotRva);
            c.thunk=at<std::uint8_t>(g_armada,0x00272d9c);
            std::array<std::uint8_t,6> expected{0xff,0x25};
            const auto iat=reinterpret_cast<std::uintptr_t>(
                at(g_armada,kNativeStringInsertIatRva));
            std::memcpy(expected.data()+2,&iat,4);
            if(!readable_range(c.resolve_slot,4)||
               !readable_range(c.insert_slot,4)||
               *c.resolve_slot!=at(g_armada,0x00082300)||
               *c.insert_slot!=c.thunk||
               !readable_range(c.thunk,6)||
               std::memcmp(c.thunk,expected.data(),6)!=0){
                log_line("BuildYard requirement append disabled: provider helper slot/thunk signature mismatch");
                return 0;
            }
            return 1;
        }
        case 6:return c.api.tidy&&c.api.assign&&c.insertion&&
            c.api.snapshot;
        case 7: {
            c.api.insert=reinterpret_cast<Insert>(*c.insert_slot);
            g_native_tooltip_api=c.api;
            char message[512]{};
            std::snprintf(message,sizeof(message),
                "BuildYard tooltip native ABI: provider=%p ECX=button stack=stringstream ret4 ESI-clobber; "
                "resolve=%p cdecl(sret,key) [stock only]; insert=%p cdecl(ostream,string&); "
                "tidy=%p thiscall(string,bool); assign=%p thiscall(string,chars,len); str=%p thiscall(stream,sret)",
                at(g_fleet_ops,kBuildYardModuleTooltipProviderRva),
                *c.resolve_slot,*c.insert_slot,
                reinterpret_cast<void*>(c.api.tidy),
                reinterpret_cast<void*>(c.api.assign),
                reinterpret_cast<void*>(c.api.snapshot));
            log_line(message);
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_bind_tooltip_api(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool buildyard_button_tooltip_binding(void* button,
                                      std::uint32_t* synthetic_out,
                                      std::string* tooltip_key_out,
                                      std::size_t* module_index_out) noexcept {
    if (synthetic_out) *synthetic_out = 0;
    if (tooltip_key_out) tooltip_key_out->clear();
    if (module_index_out) *module_index_out = 0;
    struct Context {
        void* button;
        std::uint32_t* synthetic_out;
        std::string* tooltip_key_out;
        std::size_t* module_index_out;
        void* configuration{};
        std::uint32_t module_index{};
        void* entries{};
        std::uint32_t stored_project_id{};
        const char* tooltip_key{};
        std::string key{};
        std::exception_ptr error{};
    } context{button,synthetic_out,tooltip_key_out,module_index_out};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return c.button&&readable_range(c.button,0x80);
        case 1: {
            const auto* bytes=static_cast<const std::uint8_t*>(c.button);
            c.configuration=*reinterpret_cast<void* const*>(bytes+0x78);
            c.module_index=*reinterpret_cast<const std::uint32_t*>(bytes+0x7c);
            return c.configuration!=nullptr;
        }
        case 2:return c.module_index;
        case 3:return readable_range(c.configuration,sizeof(void*));
        case 4:c.entries=*static_cast<void**>(c.configuration);
            return c.entries!=nullptr;
        case 5: {
            constexpr std::size_t stride=0x60;
            constexpr std::size_t tooltip_offset=0x0c;
            return readable_range(
                static_cast<const std::uint8_t*>(c.entries)+arg*stride,
                tooltip_offset+sizeof(const char*));
        }
        case 6: {
            constexpr std::size_t stride=0x60;
            const auto* entry=static_cast<const std::uint8_t*>(c.entries)+
                c.module_index*stride;
            c.stored_project_id=*reinterpret_cast<const std::uint32_t*>(
                entry+0x08);
            c.tooltip_key=*reinterpret_cast<const char* const*>(entry+0x0c);
            return c.tooltip_key!=nullptr;
        }
        case 7:return readable_range(c.tooltip_key,1);
        case 8:return *c.tooltip_key!='\0';
        case 9:return readable_range(c.tooltip_key+arg,1);
        case 10:return c.tooltip_key[arg]!='\0';
        case 11:c.key.assign(c.tooltip_key,arg);return 1;
        case 12:return c.stored_project_id;
        case 13:return is_symbolic_project(c.stored_project_id);
        case 14:
            if(c.synthetic_out)*c.synthetic_out=c.stored_project_id;
            if(c.tooltip_key_out)*c.tooltip_key_out=c.key;
            if(c.module_index_out)*c.module_index_out=c.module_index;
            return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_button_binding(
        &context,kMaximumModuleIndex,step);
    if(context.error)std::terminate();
    return result;
}

void provider_log_once(const std::string& identity,
                       const std::string& suffix,
                       const std::string& text){
    bool first=false;
    {
        PolicyLockGuard lock;
        first=g_logged_tooltip_appends.insert(identity+suffix).second;
    }
    if(first)log_line(text);
}

void __attribute__((fastcall)) buildyard_module_tooltip_provider_hook(
    void* button, void*, void* stream_wrapper) noexcept {
    if(g_buildyard_module_tooltip_hook.gateway){
        a2fo_buildyard_call_tooltip_provider(
            g_buildyard_module_tooltip_hook.gateway,button,stream_wrapper);
    }
    struct Context {
        void* button;
        void* stream_wrapper;
        std::uint32_t synthetic{};
        std::string tooltip_key{};
        std::size_t module_index{};
        std::string identity{};
        std::int32_t team{};
        buildyard_tooltip_native::Result result{};
        std::string requirement_text{};
        std::exception_ptr error{};
    } context{button,stream_wrapper};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 1:return buildyard_button_tooltip_binding(c.button,
            &c.synthetic,&c.tooltip_key,&c.module_index);
        case 2: {
            c.identity=std::to_string(c.synthetic)+"|"+
                std::to_string(c.module_index)+"|"+lower_ascii(c.tooltip_key);
            char entry[640]{};
            std::snprintf(entry,sizeof(entry),
                "BuildYard MODULE tooltip provider invoked for module%lu '%s' (gate=0x%08lX, native=%p, stream=%p)",
                static_cast<unsigned long>(c.module_index),c.tooltip_key.c_str(),
                static_cast<unsigned long>(c.synthetic),
                at(g_fleet_ops,kBuildYardModuleTooltipProviderRva),
                c.stream_wrapper);
            provider_log_once(c.identity,"|entry",entry);
            return 1;
        }
        case 3:c.team=local_team_index();return c.team;
        case 4:return g_tooltip_runtime_ready;
        case 5:return g_native_count_ready;
        case 6:return c.stream_wrapper&&
            readable_range(c.stream_wrapper,0x8c);
        case 7: {
            const GateEvaluation evaluation=evaluate_symbolic_binding(
                c.synthetic,c.team);
            c.requirement_text=format_missing_requirements(evaluation);
            return evaluation.available;
        }
        case 8:return !c.requirement_text.empty();
        case 9: {
            const std::string suffix="\n"+c.requirement_text;
            c.result=buildyard_tooltip_native::append_text(
                g_native_tooltip_api,c.stream_wrapper,suffix.data(),
                static_cast<std::uint32_t>(suffix.size()));
            return 1;
        }
        case 11: {
            static const char* const reasons[]={
                "native tooltip ABI not ready",
                "local team unavailable or outside native table",
                "native object-count signature not verified",
                "null or unreadable native stringstream",
                "all MODULE requirements satisfied",
                "unavailable gate produced no missing requirement text"};
            c.result.reason=reasons[arg];
            return 1;
        }
        case 12: {
            char diagnostic[768]{};
            std::snprintf(diagnostic,sizeof(diagnostic),
                "BuildYard MODULE tooltip module%lu '%s' gate=0x%08lX team=%ld append %s: %s (bytes=%lu->%lu)",
                static_cast<unsigned long>(c.module_index),
                c.tooltip_key.c_str(),static_cast<unsigned long>(c.synthetic),
                static_cast<long>(c.team),
                c.result.verified?"stream-verified":"skipped/FAILED",
                c.result.reason,static_cast<unsigned long>(c.result.before),
                static_cast<unsigned long>(c.result.after));
            provider_log_once(c.identity,
                std::string("|")+std::to_string(c.team)+"|"+c.result.reason+
                    "|"+c.requirement_text,
                std::string(diagnostic)+"; chosen="+
                    (c.requirement_text.empty()?"<none>":c.requirement_text));
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_provider(&context,kNativeTeamCount,step);
    if(context.error){
        static bool reported=false;
        if(!reported){
            reported=true;
            log_line("BuildYard requirement append failed: C++ exception; native exceptions are not assumed catchable across runtimes");
        }
    }
}

bool install_buildyard_module_tooltip_provider_hook() noexcept {
    if (!g_api || !g_api->install_inline_hook || !g_fleet_ops) return false;
    // Entire provider body, including cleanup and ret 4, not just a generic
    // prologue. Only the two absolute slot operands need PE relocation.
    std::array<std::uint8_t, 118> expected{
        0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe8, 0x89, 0x4d, 0xfc, 0x8b, 0x45, 0xfc,
        0x8b, 0x40, 0x7c, 0x03, 0xc0, 0x03, 0xc0, 0x8d, 0x04, 0x40, 0x8b, 0x55,
        0xfc, 0x8b, 0x52, 0x78, 0x8b, 0x12, 0x8b, 0x44, 0xc2, 0x0c, 0x89, 0x45,
        0xf8, 0x8b, 0x45, 0x08, 0x85, 0xc0, 0x74, 0x06, 0x3e, 0x8d, 0x70, 0x08,
        0xeb, 0x02, 0x31, 0xf6, 0x8b, 0x4d, 0xf8, 0x8d, 0x45, 0xe8, 0x51, 0x50,
        0xff, 0x15, 0x48, 0x0a, 0xa1, 0x5a, 0x50, 0x56, 0xff, 0x15, 0x40, 0xe6,
        0xa0, 0x5a, 0x8d, 0x45, 0xe8, 0x8b, 0x40, 0x04, 0x83, 0xc4, 0x10, 0x85,
        0xc0, 0x74, 0x19, 0x3e, 0x8d, 0x48, 0xff, 0x3e, 0x8a, 0x40, 0xff, 0x84,
        0xc0, 0x74, 0x04, 0x3c, 0xff, 0x75, 0x09, 0x51, 0xe8, 0x4b, 0x47, 0x0a,
        0x00, 0x83, 0xc4, 0x04, 0x8b, 0xe5, 0x5d, 0xc2, 0x04, 0x00,
    };
    const auto resolve_slot = reinterpret_cast<std::uintptr_t>(at(g_fleet_ops, kNativeTooltipResolveSlotRva));
    const auto insert_slot = reinterpret_cast<std::uintptr_t>(at(g_fleet_ops, kNativeTooltipInsertSlotRva));
    std::memcpy(expected.data() + 0x3e, &resolve_slot, 4);
    std::memcpy(expected.data() + 0x46, &insert_slot, 4);
    struct Context {
        std::array<std::uint8_t,118>& expected;
        std::exception_ptr error{};
    } context{expected};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return signature_matches(g_fleet_ops,
            kBuildYardModuleTooltipProviderRva,c.expected);
        case 1:log_line("BuildYard module tooltip-provider full-body signature mismatch; requirement text disabled, MODULE gates retained");return 1;
        case 2:return bind_native_tooltip_api();
        case 3:return g_api->install_inline_hook(
            at(g_fleet_ops,kBuildYardModuleTooltipProviderRva),
            reinterpret_cast<void*>(&buildyard_module_tooltip_provider_hook),
            kExpectedBuildYardModuleTooltipProvider.size(),
            kExpectedBuildYardModuleTooltipProvider.data(),
            &g_buildyard_module_tooltip_hook);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_install_provider(&context,step);
    if(context.error)std::terminate();
    return result;
}

// ---------- tooltip integration ----------------------------------------------

std::int32_t local_team_index() noexcept {
    if (!g_requirement_team_ready || !g_armada) return -1;
    using LocalTeamFunction = std::int32_t (__cdecl*)();
    const auto function = reinterpret_cast<LocalTeamFunction>(
        at(g_armada, kLocalTeamRva));
    return function ? function() : -1;
}

std::string format_missing_requirements(const GateEvaluation& evaluation) {
    std::string text;
    struct Context { std::string& text; std::exception_ptr error{}; }
        context{text};
    const auto emit=+[](void* opaque,
        const A2foStage8BuildyardEvent* event) noexcept -> bool {
        auto& c=*static_cast<Context*>(opaque);
        try {c.text.assign(reinterpret_cast<const char*>(event->a.data),
                           event->a.size);return true;}
        catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_buildyard_format_missing(evaluation.available,
        evaluation.definition_missing,evaluation.definition_unsupported,
        evaluation.native_required_missing,buildyard_bytes(evaluation.gate_key),
        buildyard_bytes(evaluation.symbolic_missing),buildyard_lower,
        &context,emit);
    if(context.error)std::rethrow_exception(context.error);
    return text;
}

std::string first_tooltip_line(const char* text) {
    if (!text || !readable_range(text, 1)) return {};
    std::string line;
    struct Context {const char* source;std::string& line;
        std::exception_ptr error{};} context{text,line};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        if(op==0)return readable_range(c.source+index,1);
        return static_cast<unsigned char>(c.source[index]);
    };
    const auto emit=+[](void* opaque,
        const A2foStage8BuildyardEvent* event) noexcept ->bool {
        auto& c=*static_cast<Context*>(opaque);
        try {c.line.assign(reinterpret_cast<const char*>(event->a.data),
                           event->a.size);return true;}
        catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_buildyard_first_tooltip_line(&context,buildyard_classify,
        step,emit);
    if(context.error)std::rethrow_exception(context.error);
    return line;
}

struct AutoTooltipMatch {
    bool matched = false;
    std::uint32_t synthetic = 0;
    std::string tooltip_key;
    std::string source_odf;
};

AutoTooltipMatch match_tooltip_snapshot(
    std::uint32_t mode,const std::string& raw,
    const std::string& first_line,
    const std::vector<std::pair<std::string,std::uint32_t>>& bindings){
    struct Context {
        const std::vector<std::pair<std::string,std::uint32_t>>& bindings;
        AutoTooltipMatch result{};
        std::string display_name{};
        std::exception_ptr error{};
    } context{bindings};
    const auto fetch=+[](void* opaque,std::size_t index,
        A2foStage8BuildyardBytes* key) noexcept ->bool {
        auto& c=*static_cast<Context*>(opaque);
        try {*key=buildyard_bytes(c.bindings[index].first);return true;}
        catch(...){c.error=std::current_exception();return false;}
    };
    const auto display=+[](void* opaque,A2foStage8BuildyardBytes source,
        A2foStage8BuildyardBytes* name) noexcept ->bool {
        auto& c=*static_cast<Context*>(opaque);
        try {
            c.display_name=display_name_for_requirement(std::string(
                reinterpret_cast<const char*>(source.data),source.size));
            *name=buildyard_bytes(c.display_name);
            return true;
        }catch(...){c.error=std::current_exception();return false;}
    };
    const auto accept=+[](void* opaque,std::size_t index,
        A2foStage8BuildyardBytes source) noexcept ->bool {
        auto& c=*static_cast<Context*>(opaque);
        try {
            c.result.matched=true;
            c.result.synthetic=c.bindings[index].second;
            c.result.tooltip_key=c.bindings[index].first;
            c.result.source_odf.assign(
                reinterpret_cast<const char*>(source.data),source.size);
            return true;
        }catch(...){c.error=std::current_exception();return false;}
    };
    a2fo_rs_c_buildyard_match_tooltip(mode,buildyard_bytes(raw),
        buildyard_bytes(first_line),bindings.size(),buildyard_classify,
        buildyard_lower,&context,fetch,display,accept);
    if(context.error)std::rethrow_exception(context.error);
    return context.result;
}

AutoTooltipMatch match_generated_autotooltip(const char* text) {
    AutoTooltipMatch result{};
    const std::string first_line=first_tooltip_line(text);
    if(first_line.empty())return result;
    std::vector<std::pair<std::string,std::uint32_t>> bindings;
    {
        PolicyLockGuard lock;
        bindings.reserve(g_tooltip_bindings.size());
        for(const auto& item:g_tooltip_bindings)bindings.push_back(item);
    }
    result=match_tooltip_snapshot(0,{},first_line,bindings);
    if(result.matched)return result;
    bool should_log=false;
    {
        PolicyLockGuard lock;
        if(g_logged_render_tooltip_lines.size()<24){
            should_log=g_logged_render_tooltip_lines.insert(
                lower_ascii(first_line)).second;
        }
    }
    if(should_log){
        log_line("BuildYard tooltip render observed first line '"+
                 first_line+"' (no AUTOTOOLTIP binding match)");
    }
    return result;
}

bool compose_generated_module_tooltip(void* tooltip,
                                      std::string* composed,
                                      const char** original_text) noexcept {
    struct Context {
        void* tooltip;
        std::string* composed;
        const char** original_text;
        const char** field{};
        const char* native{};
        AutoTooltipMatch match{};
        std::int32_t team{};
        GateEvaluation evaluation{};
        std::string requirement_text{};
        std::exception_ptr error{};
    } context{tooltip,composed,original_text};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return c.tooltip&&c.composed&&c.original_text&&
            g_tooltip_runtime_ready;
        case 1:
            c.field=reinterpret_cast<const char**>(
                static_cast<std::uint8_t*>(c.tooltip)+kTooltipTextOffset);
            return readable_range(c.field,sizeof(*c.field))&&
                writable_range(c.field,sizeof(*c.field));
        case 2:c.native=*c.field;
            return c.native&&readable_range(c.native,1);
        case 3:c.match=match_generated_autotooltip(c.native);
            return c.match.matched&&c.match.synthetic!=0;
        case 4:c.team=local_team_index();
            return c.team>=0&&c.team<=kMaximumTeamIndex;
        case 5:c.evaluation=evaluate_symbolic_binding(c.match.synthetic,
            static_cast<std::uintptr_t>(c.team));
            return c.evaluation.available;
        case 6:c.requirement_text=format_missing_requirements(c.evaluation);
            return !c.requirement_text.empty();
        case 7:return std::strstr(c.native,c.requirement_text.c_str())!=nullptr;
        case 8: {
            c.composed->assign(c.native);
            c.composed->append("\n");
            c.composed->append(c.requirement_text);
            *c.original_text=c.native;
            *c.field=c.composed->c_str();
            bool first_append=false;
            {
                PolicyLockGuard lock;
                first_append=g_logged_tooltip_appends.insert(
                    lower_ascii(c.match.tooltip_key)+"|render").second;
            }
            if(first_append){
                log_line("BuildYard AUTOTOOLTIP render matched '"+
                    c.match.tooltip_key+"' via "+c.match.source_odf+
                    ": "+c.requirement_text);
            }
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_buildyard_compose_generated(&context,step);
    if(context.error)std::terminate();
    return result;
}


AutoTooltipMatch match_component_tooltip_text(const char* text) {
    AutoTooltipMatch result{};
    if(!text||!readable_range(text,1))return result;
    const std::string raw=trim_copy(text);
    const std::string raw_lower=lower_ascii(raw);
    std::vector<std::pair<std::string,std::uint32_t>> bindings;
    {
        PolicyLockGuard lock;
        bindings.reserve(g_tooltip_bindings.size());
        for(const auto& item:g_tooltip_bindings)bindings.push_back(item);
    }
    const std::string first_line=first_tooltip_line(text);
    result=match_tooltip_snapshot(1,raw_lower,first_line,bindings);
    if(result.matched)return result;
    bool should_log=false;
    if(!first_line.empty()){
        PolicyLockGuard lock;
        if(g_logged_setter_tooltip_lines.size()<32){
            should_log=g_logged_setter_tooltip_lines.insert(
                lower_ascii(first_line)).second;
        }
    }
    if(should_log){
        log_line("BuildYard SetTooltipText observed first line '"+
                 first_line+"' (no MODULE tooltip binding match)");
    }
    return result;
}

const char* compose_component_module_tooltip(
    void* component, const char* native, bool verbose) noexcept {
    struct Context {
        void* component;
        const char* native;
        bool verbose;
        const char* result;
        AutoTooltipMatch match{};
        std::int32_t team{};
        GateEvaluation evaluation{};
        std::string requirement_text{};
        std::exception_ptr error{};
    } context{component,native,verbose,native};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return c.component!=nullptr;
        case 1:return c.native&&readable_range(c.native,1);
        case 2: {
            auto& storage=c.verbose?g_component_module_verbose_tooltips
                :g_component_module_tooltips;
            PolicyLockGuard lock;
            storage.erase(c.component);
            return 1;
        }
        case 3:c.match=match_component_tooltip_text(c.native);
            return c.match.matched&&c.match.synthetic!=0;
        case 4:c.team=local_team_index();
            return c.team>=0&&c.team<=kMaximumTeamIndex;
        case 5:c.evaluation=evaluate_symbolic_binding(c.match.synthetic,
            static_cast<std::uintptr_t>(c.team));
            return c.evaluation.available;
        case 6:c.requirement_text=format_missing_requirements(c.evaluation);
            return !c.requirement_text.empty();
        case 7:return std::strstr(c.native,c.requirement_text.c_str())!=nullptr;
        case 8: {
            auto& storage=c.verbose?g_component_module_verbose_tooltips
                :g_component_module_tooltips;
            {
                PolicyLockGuard lock;
                std::string& composed=storage[c.component];
                composed.assign(c.native);
                composed.append("\n");
                composed.append(c.requirement_text);
                c.result=composed.c_str();
            }
            bool first_append=false;
            {
                PolicyLockGuard lock;
                first_append=g_logged_tooltip_appends.insert(
                    lower_ascii(c.match.tooltip_key)+
                    (c.verbose?"|setter-verbose":"|setter-normal")).second;
            }
            if(first_append){
                log_line("BuildYard MODULE SetTooltipText matched '"+
                    c.match.tooltip_key+"'"+
                    (c.match.source_odf.empty()?std::string{}:
                        " via "+c.match.source_odf)+
                    ": "+c.requirement_text);
            }
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_compose_component(&context,step);
    if(context.error)std::terminate();
    return context.result;
}

void __attribute__((fastcall)) standard_component_set_tooltip_text_hook(
    void* component, void*, const char* text) noexcept {
    const char* rewritten = compose_component_module_tooltip(
        component, text, false);
    if (g_set_tooltip_text_hook.gateway) {
        a2fo_call_thiscall_1(
            g_set_tooltip_text_hook.gateway, component,
            reinterpret_cast<std::uintptr_t>(rewritten));
    }
}

void __attribute__((fastcall)) standard_component_set_verbose_tooltip_text_hook(
    void* component, void*, const char* text) noexcept {
    const char* rewritten = compose_component_module_tooltip(
        component, text, true);
    if (g_set_verbose_tooltip_text_hook.gateway) {
        a2fo_call_thiscall_1(
            g_set_verbose_tooltip_text_hook.gateway, component,
            reinterpret_cast<std::uintptr_t>(rewritten));
    }
}

bool install_component_tooltip_setter_hooks() noexcept {
    if (!g_api || !g_api->install_inline_hook || !g_armada) return false;
    struct Context { std::exception_ptr error{}; } context;
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return signature_matches(g_armada,
            kStandardComponentSetTooltipTextRva,kExpectedSetTooltipText);
        case 1:return signature_matches(g_armada,
            kStandardComponentSetVerboseTooltipTextRva,kExpectedSetVerboseTooltipText);
        case 2:return g_api->install_inline_hook(
            at(g_armada,kStandardComponentSetTooltipTextRva),
            reinterpret_cast<void*>(&standard_component_set_tooltip_text_hook),
            kExpectedSetTooltipText.size(),kExpectedSetTooltipText.data(),
            &g_set_tooltip_text_hook);
        case 3:return g_api->install_inline_hook(
            at(g_armada,kStandardComponentSetVerboseTooltipTextRva),
            reinterpret_cast<void*>(&standard_component_set_verbose_tooltip_text_hook),
            kExpectedSetVerboseTooltipText.size(),
            kExpectedSetVerboseTooltipText.data(),
            &g_set_verbose_tooltip_text_hook);
        case 4:log_line("BuildYard StandardComponent::SetTooltipText signature mismatch");return 1;
        case 5:log_line("BuildYard StandardComponent::SetVerboseTooltipText signature mismatch");return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_two_hooks(&context,step);
    if(context.error)std::terminate();
    return result;
}

void restore_tooltip_text(void* tooltip, const char* original_text) noexcept {
    if (!tooltip || !original_text) return;
    auto* field = reinterpret_cast<const char**>(
        static_cast<std::uint8_t*>(tooltip) + kTooltipTextOffset);
    if (writable_range(field, sizeof(*field))) *field = original_text;
}

void __attribute__((fastcall)) tooltip_render_hook(
    void* tooltip, void*) noexcept {
    const char* original = nullptr;
    thread_local std::string composed;
    const bool replaced = compose_generated_module_tooltip(
        tooltip, &composed, &original);
    if (g_tooltip_render_hook.gateway) {
        a2fo_call_thiscall_0(g_tooltip_render_hook.gateway, tooltip);
    }
    if (replaced) restore_tooltip_text(tooltip, original);
}

void __attribute__((fastcall)) tooltip_render_verbose_hook(
    void* tooltip, void*) noexcept {
    const char* original = nullptr;
    thread_local std::string composed;
    const bool replaced = compose_generated_module_tooltip(
        tooltip, &composed, &original);
    if (g_tooltip_render_verbose_hook.gateway) {
        a2fo_call_thiscall_0(g_tooltip_render_verbose_hook.gateway, tooltip);
    }
    if (replaced) restore_tooltip_text(tooltip, original);
}

bool install_tooltip_render_hooks() noexcept {
    if (!g_api || !g_api->install_inline_hook || !g_armada) return false;
    struct Context { std::exception_ptr error{}; } context;
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return signature_matches(g_armada,kTooltipRenderRva,
            kExpectedTooltipRender);
        case 1:return signature_matches(g_armada,kTooltipRenderVerboseRva,
            kExpectedTooltipRenderVerbose);
        case 2:return g_api->install_inline_hook(
            at(g_armada,kTooltipRenderRva),
            reinterpret_cast<void*>(&tooltip_render_hook),
            kExpectedTooltipRender.size(),kExpectedTooltipRender.data(),
            &g_tooltip_render_hook);
        case 3:return g_api->install_inline_hook(
            at(g_armada,kTooltipRenderVerboseRva),
            reinterpret_cast<void*>(&tooltip_render_verbose_hook),
            kExpectedTooltipRenderVerbose.size(),
            kExpectedTooltipRenderVerbose.data(),
            &g_tooltip_render_verbose_hook);
        case 4:log_line("BuildYard normal Tooltip::Render signature mismatch");return 1;
        case 5:log_line("BuildYard verbose Tooltip::Render signature mismatch");return 1;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_two_hooks(&context,step);
    if(context.error)std::terminate();
    return result;
}

std::size_t localization_hook_length(const std::uint8_t* code) noexcept {
    if (!code || !readable_range(code, 16)) return 0;
    return a2fo_rs_c_buildyard_localization_length(code,16);
}

std::uintptr_t native_localization_lookup(void* manager,
                                          const char* key) noexcept {
    void* function = g_localization_lookup_hook.gateway
        ? g_localization_lookup_hook.gateway
        : at(g_armada, kLocalizationLookupRva);
    if (!function || !manager || !key) return 0;
    return a2fo_call_thiscall_1(
        function, manager, reinterpret_cast<std::uintptr_t>(key));
}

std::uintptr_t __attribute__((fastcall)) localization_lookup_hook(
    void* manager, void*, const char* key) noexcept {
    return a2fo_buildyard_localization_lookup_handler(manager, key);
}

bool install_global_localization_hook() noexcept {
    if(!g_api||!g_api->install_inline_hook||!g_armada)return false;
    struct Context {
        std::uint8_t* target;
        std::exception_ptr error{};
    } context{at<std::uint8_t>(g_armada,kLocalizationLookupRva)};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t length) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return localization_hook_length(c.target);
        case 1:
            if(c.target&&readable_range(c.target,10)){
                char message[192]{};
                std::snprintf(message,sizeof(message),
                    "BuildYard tooltip localization prologue unsupported: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                    c.target[0],c.target[1],c.target[2],c.target[3],
                    c.target[4],c.target[5],c.target[6],c.target[7],
                    c.target[8],c.target[9]);
                log_line(message);
            }else{
                log_line("BuildYard tooltip localization entry is unreadable");
            }
            return 1;
        case 2: {
            std::array<std::uint8_t,16> expected{};
            std::memcpy(expected.data(),c.target,length);
            return g_api->install_inline_hook(c.target,
                reinterpret_cast<void*>(&localization_lookup_hook),
                length,expected.data(),&g_localization_lookup_hook);
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_install_localization(&context,step);
    if(context.error)std::terminate();
    return result;
}

bool call_target_resolves_to(const std::uint8_t* target,
                             const void* wanted) noexcept {
    struct Context {const std::uint8_t* target;} context{target};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::int64_t {
        const auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return readable_range(c.target,6);
        case 1:return c.target[arg];
        case 2: {
            std::int32_t relative=0;
            std::memcpy(&relative,c.target+1,sizeof(relative));
            return relative;
        }
        case 3: {
            std::uint32_t address=0;
            std::memcpy(&address,c.target+2,sizeof(address));
            return address;
        }
        case 4:return readable_range(reinterpret_cast<const void*>(arg),
            sizeof(void*));
        case 5:return reinterpret_cast<std::uintptr_t>(
            *reinterpret_cast<const void* const*>(arg));
        default:return 0;
        }
    };
    return a2fo_rs_c_buildyard_resolve_target(
        reinterpret_cast<std::uintptr_t>(target),
        reinterpret_cast<std::uintptr_t>(wanted),&context,step);
}

std::size_t patch_fleetops_localization_calls() noexcept {
    if (!g_api || !g_api->patch_call || !g_fleet_ops || !g_armada) return 0;
    auto* base = reinterpret_cast<std::uint8_t*>(g_fleet_ops);
    if (!readable_range(base, sizeof(IMAGE_DOS_HEADER))) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    const auto* section = IMAGE_FIRST_SECTION(nt);
    if (!readable_range(section,
            nt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER))) {
        return 0;
    }

    struct Context {
        std::uint8_t* base;
        const IMAGE_NT_HEADERS32* nt;
        const IMAGE_SECTION_HEADER* sections;
        const IMAGE_SECTION_HEADER* current{};
        std::uint8_t* code{};
        const void* localization;
        std::exception_ptr error{};
    } context{base,nt,section,nullptr,nullptr,
        at(g_armada,kLocalizationLookupRva)};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t arg) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 0:return c.nt->FileHeader.NumberOfSections;
        case 1:c.current=&c.sections[arg];
            return c.current->Characteristics;
        case 2:return c.current->Misc.VirtualSize;
        case 3:return c.current->SizeOfRawData;
        case 4:c.code=c.base+c.current->VirtualAddress;return 1;
        case 5:return c.code[arg];
        case 6: {
            auto* call=c.code+arg;
            std::int32_t relative=0;
            std::memcpy(&relative,call+1,sizeof(relative));
            const auto target_address=reinterpret_cast<std::uintptr_t>(call)+
                5+relative;
            const auto* target=reinterpret_cast<const std::uint8_t*>(
                target_address);
            return call_target_resolves_to(target,c.localization);
        }
        case 7: {
            auto* call=c.code+arg;
            std::array<std::uint8_t,5> expected{};
            std::memcpy(expected.data(),call,expected.size());
            return g_api->patch_call(call,
                reinterpret_cast<void*>(
                    &a2fo_buildyard_localization_lookup_bridge),
                expected.data(),expected.size());
        }
        case 8:return readable_range(c.code,arg);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return -1;}
    };
    const std::size_t patched=a2fo_rs_c_buildyard_patch_calls(&context,step);
    if(context.error)std::terminate();
    return patched;
}

}  // namespace

extern "C" std::uintptr_t __cdecl
 a2fo_buildyard_localization_lookup_handler(void* manager,
                                            const char* key) noexcept {
    const char* native=reinterpret_cast<const char*>(
        native_localization_lookup(manager,key));
    struct Context {
        const char* native;
        const char* key;
        const char* result;
        std::uint32_t synthetic{};
        std::int32_t team{};
        GateEvaluation evaluation{};
        std::string requirement_text{};
        std::exception_ptr error{};
        bool before_try{};
    } context{native,key,native};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(op){
        case 1:return c.key&&*c.key&&g_tooltip_runtime_ready;
        case 2: {
            PolicyLockGuard lock;
            const auto found=g_tooltip_bindings.find(lower_ascii(c.key));
            if(found==g_tooltip_bindings.end())return 0;
            c.synthetic=found->second;
            return 1;
        }
        case 3:c.team=local_team_index();return c.team;
        case 4:c.evaluation=evaluate_symbolic_binding(c.synthetic,
            static_cast<std::uintptr_t>(c.team));
            return c.evaluation.available;
        case 5:c.requirement_text=format_missing_requirements(c.evaluation);
            return !c.requirement_text.empty();
        case 6: {
            thread_local std::string composed;
            composed.assign(c.native&&*c.native?c.native:c.key);
            composed+="\n";
            composed+=c.requirement_text;
            bool first_append=false;
            {
                PolicyLockGuard lock;
                first_append=g_logged_tooltip_appends.insert(
                    lower_ascii(c.key)).second;
            }
            if(first_append){
                log_line("BuildYard MODULE tooltip matched '"+
                    std::string(c.key)+"': "+c.requirement_text);
            }
            c.result=composed.c_str();
            return 1;
        }
        default:return 0;
        }}catch(...){c.error=std::current_exception();c.before_try=op<=3;
            return INT64_MIN;}
    };
    a2fo_rs_c_buildyard_localization_lookup(
        &context,kMaximumTeamIndex,step);
    if(context.error&&context.before_try)std::terminate();
    return reinterpret_cast<std::uintptr_t>(context.result);
}

extern "C" std::uintptr_t __cdecl
a2fo_buildyard_project_id_call_handler(
    void* parameter_db, void* delphi_command,
    std::uint32_t* required_project_id,
    std::uintptr_t default_project_id) noexcept {
    // Always execute Fleet Ops' original RequiredTechnology lookup first so a
    // configuration with no A2FO command behaves byte-for-byte like stock.
    const std::uintptr_t native_result =
        a2fo_buildyard_call_delphi_reg3_stack1(
            at(g_fleet_ops, kFleetOpsParameterDbGetProjectIdRva),
            parameter_db, delphi_command, required_project_id,
            default_project_id);

    if (!parameter_db || !required_project_id ||
        !writable_range(required_project_id, sizeof(*required_project_id))) {
        return native_result;
    }

    std::array<char, 64> native_command{};
    copy_delphi_string(
        delphi_command, native_command.data(), native_command.size());
    std::size_t module_index = 0;
    if (!parse_required_technology_command(
            native_command.data(), &module_index)) {
        return native_result;
    }

    try {
        struct Context {
            void* parameter_db;
            std::uint32_t* required_project_id;
            std::size_t module_index;
            std::uint32_t native_project_id;
            TechnologyEntryLookup custom{};
            std::string gate_key{};
            std::uint32_t project_id{};
            bool resolved_late{};
            std::exception_ptr error{};
        } context{parameter_db,required_project_id,module_index,*required_project_id};
        const auto step=+[](void* opaque,std::uint32_t op,
            std::size_t) noexcept ->std::int64_t {
            auto& c=*static_cast<Context*>(opaque);
            try {
                switch(op){
                case 0:
                    c.custom=read_module_technology_entry(
                        c.parameter_db,c.module_index);
                    return c.custom.found;
                case 1:
                    c.gate_key=normalize_odf_key(c.custom.raw_name);
                    if(c.gate_key.empty()){
                        log_line("BuildYard module"+std::to_string(c.module_index)+
                            " technology entry is empty; native RequiredTechnology retained");
                        return 0;
                    }
                    return 1;
                case 2:return has_module_definition(c.gate_key);
                case 3: {
                    const std::string tooltip_key=read_module_tooltip_key(
                        c.parameter_db,c.module_index);
                    const std::uint32_t synthetic=allocate_symbolic_binding(
                        c.gate_key,c.native_project_id,tooltip_key);
                    *c.required_project_id=synthetic;
                    ModuleTechnologyDefinition definition{};
                    {
                        PolicyLockGuard lock;
                        const auto found=g_module_technologies.find(c.gate_key);
                        if(found!=g_module_technologies.end())definition=found->second;
                    }
                    char message[512]{};
                    std::snprintf(message,sizeof(message),
                        "BuildYard module%lu%s '%s' -> symbolic MODULE gate 0x%08lX (%lu requirement alternative%s)%s%s",
                        static_cast<unsigned long>(c.module_index),
                        c.custom.legacy_alias?"PseudoTechnology":"TechnologyEntry",
                        c.custom.raw_name.c_str(),static_cast<unsigned long>(synthetic),
                        static_cast<unsigned long>(definition.requirements.size()),
                        definition.requirements.size()==1?"":"s",
                        c.native_project_id!=0?"; AND native RequiredTechnology project ID ":"",
                        c.native_project_id!=0?std::to_string(c.native_project_id).c_str():"");
                    log_line(message);
                    if(!tooltip_key.empty()){
                        log_line("BuildYard MODULE gate '"+c.gate_key+
                            "' bound to tooltip key '"+tooltip_key+"'");
                    }else{
                        log_line("BuildYard MODULE gate '"+c.gate_key+
                            "' has no moduleXButtonTooltip key; requirement text cannot be appended for this slot");
                    }
                    if(definition.unsupported){
                        log_line("MODULE gate '"+c.gate_key+"' disabled: "+definition.reason);
                    }
                    return 1;
                }
                case 4: {
                    std::array<char,64> command{};
                    std::snprintf(command.data(),command.size(),"Module%lu%s",
                        static_cast<unsigned long>(c.module_index),
                        c.custom.legacy_alias?"PseudoTechnology":"TechnologyEntry");
                    (void)read_project_id_command(c.parameter_db,command.data(),
                        &c.project_id);
                    return c.project_id;
                }
                case 5:
                    c.project_id=resolve_featurepack_odf_project_id(
                        c.custom.raw_name.c_str());
                    c.resolved_late=c.project_id!=0;
                    return c.project_id;
                case 6: {
                    *c.required_project_id=c.project_id;
                    {
                        PolicyLockGuard lock;
                        g_legacy_recursive_project_ids.insert(c.project_id);
                    }
                    char message[384]{};
                    std::snprintf(message,sizeof(message),
                        "BuildYard module%lu%s '%s' -> real ODF project ID %lu%s (v4 compatibility path)",
                        static_cast<unsigned long>(c.module_index),
                        c.custom.legacy_alias?"PseudoTechnology":"TechnologyEntry",
                        c.custom.raw_name.c_str(),
                        static_cast<unsigned long>(c.project_id),
                        c.resolved_late?" (late FOFS resolution)":"");
                    log_line(message);
                    return 1;
                }
                case 7: {
                    const std::string tooltip_key=read_module_tooltip_key(
                        c.parameter_db,c.module_index);
                    const std::uint32_t synthetic=allocate_symbolic_binding(
                        c.gate_key,c.native_project_id,tooltip_key);
                    *c.required_project_id=synthetic;
                    log_line("BuildYard module"+std::to_string(c.module_index)+
                        (c.custom.legacy_alias?"PseudoTechnology '":"TechnologyEntry '")+
                        c.custom.raw_name+
                        "' has no matching <object MODULE=...> entry and no backing ODF; module will remain unavailable");
                    return 1;
                }
                default:return 0;
                }
            }catch(...){c.error=std::current_exception();return -1;}
        };
        const bool replaced=a2fo_rs_c_buildyard_project_capture(&context,step);
        if(context.error)std::rethrow_exception(context.error);
        return replaced?native_result|1u:native_result;
    } catch (...) {
        log_line("BuildYard technology-entry capture failed; native RequiredTechnology behavior retained");
        return native_result;
    }
}

bool initialize_buildyard_pseudo_technology(
    const A2FO_ModuleApi* api, HMODULE armada,
    HMODULE fleet_ops) noexcept {
    if (!api || !api->log || !api->install_inline_hook || !api->patch_call ||
        !armada || !fleet_ops) return false;
    struct Context {
        const A2FO_ModuleApi* api;
        HMODULE armada;
        HMODULE fleet_ops;
        std::exception_ptr error{};
    } context{api,armada,fleet_ops};
    const auto step=+[](void* opaque,std::uint32_t op,
        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            switch(op){
            case 0:return signature_matches(c.armada,kParameterDbGetProjectIdRva,
                kExpectedParameterDbGetProjectId);
            case 1:return signature_matches(c.fleet_ops,kBuildYardRequiredTechnologyCallRva,
                kExpectedRequiredTechnologyCall);
            case 2:return signature_matches(c.fleet_ops,kProjectIdTechnologyAvailableRva,
                kExpectedTechnologyAvailable);
            case 3:return signature_matches(c.fleet_ops,kTechnologyTreeAllowsProjectRva,
                kExpectedTechnologyTreeAllowsProject);
            case 4:
                c.api->log(kModuleName,
                    "BuildYard MODULE technology signatures mismatch; module technology gates disabled");
                return 1;
            case 5:
                g_api=c.api;g_armada=c.armada;g_fleet_ops=c.fleet_ops;
                return 1;
            case 6:
                g_native_count_ready=signature_matches(c.fleet_ops,
                    kTechnologyObjectCountRva,kExpectedTechnologyObjectCount);
                g_requirement_team_ready=signature_matches(c.armada,
                    kLocalTeamRva,kExpectedLocalTeam);
                log_line(g_native_count_ready
                    ? "MODULE <obj> count verified: FleetOpsHook+0x11F0CC EAX=team tree EDX=project ID ECX=0; signed EAX completed count at item+0x14, queued excluded"
                    : "MODULE <obj> count signature mismatch; object requirements fail closed");
                if(!g_requirement_team_ready)
                    log_line("BuildYard tooltip local-team signature mismatch; requirement text disabled");
                return 1;
            case 7:
                InitializeCriticalSection(&g_policy_lock);
                g_policy_lock_ready=true;
                return 1;
            case 8:
                g_require_active_tree=true;
                g_team_predicates_ready=validate_team_predicates();
                return 1;
            case 9:return install_native_tree_observers();
            case 10:
                log_line("MODULE active-tree observer unavailable; initialization stopped before gameplay hooks");
                return 1;
            case 11: {
                const std::size_t count=load_symbolic_module_technologies();
                log_line("Loaded "+std::to_string(count)+
                    " symbolic BuildYard MODULE catalog entr"+
                    (count==1?"y":"ies")+
                    " from loose XML (labels only; gameplay awaits native selected tree)");
                return 1;
            }
            case 12:
                if(A2FO_MODULE_API_HAS(c.api,register_game_object_class_loaded_handler)&&
                   c.api->register_game_object_class_loaded_handler){
                    static const char* const fields[]={"unitName"};
                    if(!c.api->register_game_object_class_loaded_handler(
                            kModuleName,fields,1,
                            &object_class_loaded_for_module_tooltips,nullptr)){
                        log_line("BuildYard MODULE tooltip unitName cache registration failed; ODF basenames will be shown instead");
                    }
                }
                return 1;
            case 13:return c.api->install_inline_hook(
                at(c.fleet_ops,kProjectIdTechnologyAvailableRva),
                reinterpret_cast<void*>(&technology_available_hook),
                kExpectedTechnologyAvailable.size(),
                kExpectedTechnologyAvailable.data(),&g_technology_available_hook);
            case 14:return c.api->patch_call(
                at(c.fleet_ops,kBuildYardRequiredTechnologyCallRva),
                reinterpret_cast<void*>(&a2fo_buildyard_project_id_call_bridge),
                kExpectedRequiredTechnologyCall.data(),
                kExpectedRequiredTechnologyCall.size());
            case 15:
                c.api->log(kModuleName,
                    "BuildYard MODULE technology hook installation was incomplete; native behavior retained where unpatched");
                return 1;
            case 16: {
                const bool installed=install_buildyard_module_tooltip_provider_hook();
                g_tooltip_runtime_ready=installed;
                log_line(installed
                    ? "BuildYard MODULE requirement tooltip enabled (phase 1 TEST visually accepted; native stream readback)"
                    : "BuildYard MODULE gates enabled; requirement tooltip append disabled by ABI validation/hook failure");
                return 1;
            }
            case 17:
                c.api->log(kModuleName,
                    "BuildYard symbolic MODULE technology support v14 initialized (selected native tree, race/groups/ally/enemy; no dummy ODF required)");
                return 1;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return -1;}
    };
    const bool result=a2fo_rs_c_buildyard_initialize(&context,step);
    if(context.error)std::terminate();
    return result;
}

}  // namespace a2fo

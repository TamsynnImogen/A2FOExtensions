/*
 * ODF-driven persistent shield visibility.
 *
 * This module owns policy, native shield-effect calls, and checked observers
 * at the central GameObject mission-publication dispatcher and
 * Starbase::Simulate. The publication observer catches completed objects
 * regardless of their Fleet Operations subclass; the simulation observer
 * maintains stock Starbases after creation.
 * A2FOCraftIdentity, A2FOTurrets, and A2FOHybridBuild also forward lifecycle
 * events from sites they already own. Keeping every shared site single-owner
 * prevents load-order-dependent detour chains while leaving this feature
 * independently optional.
 */

#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/stage8_a_ffi.h"
#include "../../rust/stage10_visibility_swarm_ffi.h"

#include <windows.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
std::uintptr_t __cdecl a2fo_shields_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_shields_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_shields_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

using CreateShieldHit = std::int32_t (__cdecl *)(
    void* object, const void* shield_matrix, std::int32_t shield_type,
    float duration, std::int32_t flags);
using StopShieldEffect = void (__cdecl *)(std::int32_t effect_id);
using PublishGameObject = std::uintptr_t (__cdecl *)(
    void* mission, void* object);

constexpr const char* kModuleName = "A2FOAlwaysShowShields";
constexpr char kOdfCommand[] = "alwaysShowShields";
constexpr std::size_t kMaximumOdfSize = 4u * 1024u * 1024u;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kGameObjectClassGetOdfNameRva = 0x000ce370;
constexpr std::uintptr_t kGameObjectPublishRva = 0x0002b910;
constexpr std::uintptr_t kRenderGameObjectsRva = 0x00072b60;
constexpr std::uintptr_t kStarbaseSimulateRva = 0x000bdb10;
constexpr std::uintptr_t kCreateShieldHitRva = 0x000743b0;
constexpr std::uintptr_t kStopShieldEffectRva = 0x00074770;
constexpr std::uintptr_t kIdentityMatrixRva = 0x00369380;
constexpr std::uintptr_t kGameObjectListRva = 0x00361084;

// Type 7 loads WEclairlink1, the Clairvoyance link effect. It is deliberately
// reused here as the persistent visual because it gives the preferred clean,
// continuously visible outline. Keep our effect ID separate from Craft+0x208,
// which belongs to the native impact/collapse effect and must remain under
// Craft's control.
constexpr std::int32_t kContinuousShieldType = 7;
constexpr std::int32_t kNativeEffectColour = 0;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kCurrentShieldsOffset = 0x1c8;

constexpr std::array<std::uint8_t, 10> kExpectedCreateShieldHit{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0x87, 0xb5, 0x69, 0x00};
constexpr std::array<std::uint8_t, 8> kExpectedStopShieldEffect{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8d, 0x45};
constexpr std::array<std::uint8_t, 7> kExpectedStarbaseSimulate{
    0x55, 0x8b, 0xec, 0x53, 0x8b, 0x5d, 0x08};
constexpr std::array<std::uint8_t, 10> kExpectedGameObjectPublish{
    0x55, 0x8b, 0xec, 0x56, 0x8b,
    0x75, 0x0c, 0x8b, 0x46, 0x40};
constexpr std::array<std::uint8_t, 11> kExpectedRenderGameObjects{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c,
    0xa1, 0x10, 0xb6, 0x76, 0x00};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
A2FO_InlineHook g_starbase_simulate_hook{};
A2FO_InlineHook g_game_object_publish_hook{};
A2FO_InlineHook g_render_game_objects_hook{};
void* g_game_object_publish_original = nullptr;
std::unordered_set<void*> g_enabled_classes;
std::unordered_set<void*> g_loose_resolved_classes;
std::unordered_map<void*, std::int32_t> g_effect_ids;
std::unordered_set<void*> g_observed_crafts;
std::unordered_set<void*> g_logged_creation_failures;
std::unordered_map<std::string, std::string> g_loose_odf_paths;
bool g_logged_object_list_scan = false;
void* g_cached_list_owner = nullptr;
void* g_cached_list_sentinel = nullptr;
std::uint32_t g_last_global_scan_tick = 0;
bool g_global_scan_completed = false;

const char* class_odf_name(void* object_class) noexcept;
void update_craft(void* craft) noexcept;
void update_all_crafts() noexcept;
bool runtime_enabled() noexcept;

void* at(std::uintptr_t rva) noexcept {
    return g_armada
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(g_armada) + rva)
        : nullptr;
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) {
        g_api->log(kModuleName, message);
    }
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) +
        info.RegionSize;
    return start <= region_end && size <= region_end - start;
}

void* existing_detour_destination(
    const void* site, std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    if (readable_range(site, 6) && bytes[0] == 0x68 && bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool fleet_ops_executable_address(const void* address) noexcept {
    if (!address || !g_fleet_ops) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || info.AllocationBase != g_fleet_ops ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    switch (info.Protect & 0xffu) {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool publication_site_supported() noexcept {
    void* site = at(kGameObjectPublishRva);
    if (readable_range(site, kExpectedGameObjectPublish.size()) &&
        std::memcmp(site, kExpectedGameObjectPublish.data(),
                    kExpectedGameObjectPublish.size()) == 0) {
        return true;
    }
    std::size_t patch_length = 0;
    void* destination = existing_detour_destination(site, &patch_length);
    if (patch_length >= 5 && patch_length <= 6 &&
        fleet_ops_executable_address(destination)) {
        return true;
    }
    char actual[16 * 3 + 1]{};
    if (readable_range(site, 16)) {
        std::size_t used = 0;
        const auto* bytes = static_cast<const std::uint8_t*>(site);
        for (std::size_t index = 0; index < 16 && used < sizeof(actual);
             ++index) {
            const int written = std::snprintf(
                actual + used, sizeof(actual) - used,
                index == 0 ? "%02X" : " %02X", bytes[index]);
            if (written <= 0) break;
            used += static_cast<std::size_t>(written);
        }
    } else {
        std::snprintf(actual, sizeof(actual), "<unreadable>");
    }
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "Unsupported GameObject publication entry (destination=%p): %s",
        destination, actual);
    log_line(message);
    return false;
}

std::string lower_ascii(std::string value) {
    struct Context {
        std::string* value;
        std::exception_ptr error{};
        std::uint8_t failed{};
    } context{&value};
    const auto step = +[](void* opaque, std::size_t index) noexcept {
        auto& state = *static_cast<Context*>(opaque);
        try {
            char& character = (*state.value)[index];
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        } catch (...) {
            state.error = std::current_exception();
            state.failed = 1;
        }
    };
    a2fo_rs_stage10_visibility_lower(value.size(), &context, step, &context.failed);
    if (context.error) std::rethrow_exception(context.error);
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    const auto last = +[](const void* opaque) noexcept -> std::uint8_t {
        const auto& value = *static_cast<const std::string*>(opaque);
        return static_cast<std::uint8_t>(value.back());
    };
    switch (a2fo_rs_stage10_visibility_join(
        left.size(), right.size(), &left, last)) {
    case 0: return right;
    case 1: return left;
    case 2: return left + right;
    default: return left + "\\" + right;
    }
}

struct IndexChild {
    std::string name;
    DWORD attributes;
};
struct IndexContext {
    std::vector<IndexChild> selected;
    WIN32_FIND_DATAA data{};
    HANDLE search = INVALID_HANDLE_VALUE;
    std::optional<std::string> current_name;
    std::exception_ptr error;
};
std::uint8_t index_step(void* opaque, std::uint32_t operation,
                        std::uint32_t index, A2foStage8Bytes first,
                        A2foStage8Bytes second, A2foStage8Bytes* output,
                        std::uint32_t* value) noexcept {
    auto& context = *static_cast<IndexContext*>(opaque);
    if (context.error || !output || !value) return 0;
    *output = {nullptr, 0};
    *value = 0;
    try {
        switch (operation) {
        case 0:
            g_loose_odf_paths.clear();
            *value = g_api && g_api->extension_root_count && g_api->extension_root
                ? g_api->extension_root_count() : 0;
            return 1;
        case 1: {
            const char* root = g_api->extension_root(index);
            if (root && *root) {
                *output = {reinterpret_cast<const std::uint8_t*>(root),
                           std::strlen(root)};
            }
            return 1;
        }
        case 2: {
            context.selected.clear();
            const std::string directory(
                reinterpret_cast<const char*>(first.data), first.size);
            context.search = FindFirstFileA(
                join_path(directory, "*").c_str(), &context.data);
            *value = context.search != INVALID_HANDLE_VALUE;
            return 1;
        }
        case 3:
            if (context.search == INVALID_HANDLE_VALUE) return 0;
            context.current_name.emplace(context.data.cFileName);
            *output = {reinterpret_cast<const std::uint8_t*>(context.current_name->data()),
                       context.current_name->size()};
            *value = context.data.dwFileAttributes;
            return 1;
        case 4:
            context.selected.push_back(
                {*context.current_name, context.data.dwFileAttributes});
            return 1;
        case 5:
            FindClose(context.search);
            context.search = INVALID_HANDLE_VALUE;
            // Frozen libstdc++ std::sort is retained as the exact ordering
            // primitive when distinct filenames have equal folded keys.
            std::sort(context.selected.begin(), context.selected.end(),
                      [](const IndexChild& left, const IndexChild& right) {
                          return lower_ascii(left.name) < lower_ascii(right.name);
                      });
            *value = static_cast<std::uint32_t>(context.selected.size());
            return 1;
        case 6:
            if (index >= context.selected.size()) return 0;
            *output = {reinterpret_cast<const std::uint8_t*>(
                           context.selected[index].name.data()),
                       context.selected[index].name.size()};
            *value = context.selected[index].attributes;
            return 1;
        case 7:
            g_loose_odf_paths[std::string(
                reinterpret_cast<const char*>(first.data), first.size)] =
                std::string(reinterpret_cast<const char*>(second.data), second.size);
            return 1;
        case 8:
            context.current_name.reset();
            *value = FindNextFileA(context.search, &context.data) != 0;
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.error = std::current_exception();
        return 0;
    }
}
void index_loose_odfs() {
    IndexContext context{};
    const auto lower = [](void*, std::uint8_t value) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::tolower(static_cast<unsigned char>(value)));
    };
    a2fo_rs_stage8_shield_index(lower, index_step, &context);
    if (context.error) std::rethrow_exception(context.error);
}

bool read_small_text_file(const std::string& path, std::string* contents) {
    if (!contents) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(kMaximumOdfSize)) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    *contents = stream.str();
    return input.good() || input.eof();
}

std::optional<bool> resolve_loose_flag(
    const std::string& requested_name) {
    struct Context {
        std::string contents;
        std::exception_ptr error;
    } context;
    const auto lower = [](void*, std::uint8_t value) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::tolower(static_cast<unsigned char>(value)));
    };
    const auto space = [](void*, std::uint8_t value) noexcept -> std::uint8_t {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    };
    const auto load = [](void* opaque, A2foStage8Bytes name,
                         A2foStage8Bytes* result) noexcept -> std::uint8_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.error || !result) return 0;
        try {
            const auto selected = g_loose_odf_paths.find(std::string(
                reinterpret_cast<const char*>(name.data), name.size));
            if (selected == g_loose_odf_paths.end() ||
                !read_small_text_file(selected->second, &state.contents)) return 0;
            *result = {reinterpret_cast<const std::uint8_t*>(state.contents.data()),
                       state.contents.size()};
            return 1;
        } catch (...) {
            state.error = std::current_exception();
            return 0;
        }
    };
    const auto result = a2fo_rs_stage8_shield_resolve(
        reinterpret_cast<const std::uint8_t*>(requested_name.data()),
        requested_name.size(), lower, space, load, &context);
    if (context.error) std::rethrow_exception(context.error);
    return result == 0 ? std::nullopt : std::optional<bool>(result == 2);
}

bool class_policy_enabled(void* object_class) noexcept {
    // A completed ParameterDB callback that did not find the command is not
    // sufficient to suppress the loose-ODF fallback. Fleet Operations can
    // invoke that callback before an active-mod override is fully represented
    // in the class database. Resolve each otherwise-disabled class once from
    // the indexed Data/parent/active-mod hierarchy.
    struct Context {
        std::string name;
        std::exception_ptr error;
    } context;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t object, A2foStage8Bytes* name) -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        auto* object_class = reinterpret_cast<void*>(object);
        if (state.error) return UINTPTR_MAX;
        try {
            switch (operation) {
                case 0: return g_enabled_classes.find(object_class) != g_enabled_classes.end();
                case 1: return g_loose_resolved_classes.find(object_class) != g_loose_resolved_classes.end();
                case 2: g_loose_resolved_classes.insert(object_class); return 0;
                case 3: {
                    const char* native_name = class_odf_name(object_class);
                    state.name = native_name ? native_name : "";
                    *name = {reinterpret_cast<const std::uint8_t*>(state.name.data()), state.name.size()};
                    return 0;
                }
                case 4: {
                    const std::string odf_name(reinterpret_cast<const char*>(name->data), name->size);
                    return resolve_loose_flag(odf_name).value_or(false);
                }
                case 5: g_enabled_classes.insert(object_class); return 0;
                default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool enabled = a2fo_rs_stage8_shield_class_policy(
        reinterpret_cast<std::uintptr_t>(object_class), step, &context) != 0;
    if (context.error) {
        log_line("Could not resolve a preloaded shield class policy");
        return false;
    }
    return enabled;
}

bool signatures_supported() noexcept {
    const auto* create_entry = static_cast<const std::uint8_t*>(
        at(kCreateShieldHitRva));
    const bool create_supported =
        readable_range(create_entry, kExpectedCreateShieldHit.size()) &&
        (std::memcmp(create_entry, kExpectedCreateShieldHit.data(),
                     kExpectedCreateShieldHit.size()) == 0 ||
         create_entry[0] == 0xe9);
    const auto* stop_entry = static_cast<const std::uint8_t*>(
        at(kStopShieldEffectRva));
    const bool stop_supported =
        readable_range(stop_entry, kExpectedStopShieldEffect.size()) &&
        (std::memcmp(stop_entry, kExpectedStopShieldEffect.data(),
                     kExpectedStopShieldEffect.size()) == 0 ||
         stop_entry[0] == 0xe9);
    if (!g_armada || !create_supported || !stop_supported ||
        std::memcmp(at(kStarbaseSimulateRva),
                    kExpectedStarbaseSimulate.data(),
                    kExpectedStarbaseSimulate.size()) != 0 ||
        std::memcmp(at(kRenderGameObjectsRva),
                    kExpectedRenderGameObjects.data(),
                    kExpectedRenderGameObjects.size()) != 0 ||
        !publication_site_supported()) {
        log_line("Native shield-effect signature mismatch; feature disabled");
        return false;
    }
    return true;
}

void __attribute__((fastcall)) starbase_simulate_hook(
    void* starbase, void*, float elapsed_seconds) noexcept {
    std::uint32_t elapsed_bits = 0;
    static_assert(sizeof(elapsed_bits) == sizeof(elapsed_seconds),
                  "Armada float arguments must occupy four bytes");
    std::memcpy(&elapsed_bits, &elapsed_seconds, sizeof(elapsed_bits));
    struct Context { void* starbase; std::uint32_t elapsed_bits; } context{
        starbase, elapsed_bits};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0:
            return a2fo_shields_call_thiscall_1(
                g_starbase_simulate_hook.gateway,
                state.starbase, state.elapsed_bits);
        case 1: return g_runtime_ready;
        case 2: update_craft(reinterpret_cast<void*>(argument)); return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_shield_after_gateway(
        reinterpret_cast<std::uintptr_t>(starbase), 0, step, &context);
}

void __attribute__((fastcall)) render_game_objects_hook(
    void* renderer, void*, std::uintptr_t render_argument) noexcept {
    // This stock render pass is the one authoritative traversal of Armada's
    // complete GameObject list. Run immediately before it builds the frame so
    // hidden Fleet Operations subclasses cannot bypass shield observation.
    struct Context { void* renderer; std::uintptr_t argument; } context{
        renderer, render_argument};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return runtime_enabled();
        case 1: return GetTickCount();
        case 2: return g_enabled_classes.empty();
        case 3: return g_last_global_scan_tick;
        case 4: return g_global_scan_completed;
        case 5:
            g_last_global_scan_tick = static_cast<std::uint32_t>(argument);
            g_global_scan_completed = true;
            update_all_crafts();
            return 0;
        case 6:
            a2fo_shields_call_thiscall_1(g_render_game_objects_hook.gateway,
                                        state.renderer, state.argument);
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_shield_render(g_runtime_ready, step, &context);
}

std::uintptr_t __cdecl game_object_publish_hook(
    void* mission, void* object) noexcept {
    // Both AiMission::AddObject and the native object-creation path enter this
    // dispatcher after construction. Observing it avoids relying on either a
    // convenience wrapper or a hidden subclass' virtual methods.
    struct Context { void* mission; void* object; } context{mission, object};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        const auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: return reinterpret_cast<PublishGameObject>(
            g_game_object_publish_original)(state.mission, state.object);
        case 1: return g_runtime_ready;
        case 2: update_craft(reinterpret_cast<void*>(argument)); return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_shield_after_gateway(
        reinterpret_cast<std::uintptr_t>(object), 1, step, &context);
}

bool install_publication_hook(const A2FO_ModuleApi* api) noexcept {
    struct Context {
        const A2FO_ModuleApi* api;
        void* site = nullptr;
        void* destination = nullptr;
        std::size_t patch_length = 0;
        bool fatal = false;
    } context{api};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        const auto* api = state.api;
        if (state.fatal) return 0;
        try {
            switch (operation) {
            case 0: return api && api->install_inline_hook && api->patch_jump;
            case 1:
                state.site = at(kGameObjectPublishRva);
                return readable_range(state.site, kExpectedGameObjectPublish.size()) &&
                    std::memcmp(state.site, kExpectedGameObjectPublish.data(),
                                kExpectedGameObjectPublish.size()) == 0;
            case 2: return api->install_inline_hook(
                state.site, reinterpret_cast<void*>(&game_object_publish_hook),
                kExpectedGameObjectPublish.size(),
                kExpectedGameObjectPublish.data(), &g_game_object_publish_hook);
            case 3:
                g_game_object_publish_original = g_game_object_publish_hook.gateway;
                return g_game_object_publish_original != nullptr;
            case 4:
                state.destination = existing_detour_destination(
                    state.site, &state.patch_length);
                return state.patch_length >= 5 && state.patch_length <= 6 &&
                    fleet_ops_executable_address(state.destination);
            case 5: {
                std::array<std::uint8_t, 6> expected_detour{};
                std::memcpy(expected_detour.data(), state.site, state.patch_length);
                return api->patch_jump(
                    state.site, reinterpret_cast<void*>(&game_object_publish_hook),
                    expected_detour.data(), state.patch_length);
            }
            case 6: {
                g_game_object_publish_original = state.destination;
                const auto handler_rva =
                    reinterpret_cast<std::uintptr_t>(state.destination) -
                    reinterpret_cast<std::uintptr_t>(g_fleet_ops);
                char message[224]{};
                std::snprintf(message, sizeof(message),
                    "Chained Fleet Operations GameObject publication detour at RVA "
                    "0x%08lX", static_cast<unsigned long>(handler_rva));
                log_line(message);
                return 0;
            }
            default: return 0;
            }
        } catch (...) {
            state.fatal = true;
            return 0;
        }
    };
    const bool result = a2fo_rs_stage8_shield_install_publish(step, &context) != 0;
    if (context.fatal) std::terminate();
    return result;
}

bool parameter_flag_enabled(void* parameter_db) noexcept {
    if (!parameter_db || !g_armada) return false;

    std::array<char, 64> value{};
    const std::uintptr_t found = a2fo_shields_call_thiscall_4(
        at(kParameterDbGetStringRva), parameter_db,
        reinterpret_cast<std::uintptr_t>(kOdfCommand),
        reinterpret_cast<std::uintptr_t>(value.data()),
        static_cast<std::uintptr_t>(value.size()),
        reinterpret_cast<std::uintptr_t>(""));
    value.back() = '\0';
    if ((found & 0xffu) == 0) return false;

    const auto lower = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
        return static_cast<std::uint8_t>(
            std::tolower(static_cast<unsigned char>(byte)));
    };
    const auto space = [](void*, std::uint8_t byte) noexcept -> std::uint8_t {
        return std::isspace(static_cast<unsigned char>(byte)) != 0;
    };
    return a2fo_rs_stage8_shield_parameter(
        reinterpret_cast<const std::uint8_t*>(value.data()),
        std::strlen(value.data()), lower, space, nullptr) != 0;
}

template <typename T>
T read_at(const void* object, std::size_t offset) noexcept {
    T value{};
    std::memcpy(
        &value, static_cast<const std::uint8_t*>(object) + offset,
        sizeof(value));
    return value;
}

const char* class_odf_name(void* object_class) noexcept {
    if (!object_class || !g_armada) return nullptr;
    return reinterpret_cast<const char*>(a2fo_shields_call_thiscall_0(
        at(kGameObjectClassGetOdfNameRva), object_class));
}

void stop_effect(std::int32_t effect_id) noexcept {
    if (effect_id < 0) return;
    reinterpret_cast<StopShieldEffect>(at(kStopShieldEffectRva))(effect_id);
}

void remove_continuous_effect(void* craft) noexcept {
    struct Context {
        decltype(g_effect_ids)::iterator found;
        std::int32_t effect = -1;
    } context{};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0:
            state.found = g_effect_ids.find(reinterpret_cast<void*>(argument));
            return state.found != g_effect_ids.end();
        case 1:
            state.effect = state.found->second;
            g_effect_ids.erase(state.found);
            return static_cast<std::uint32_t>(state.effect);
        case 2: stop_effect(static_cast<std::int32_t>(argument)); return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_shield_remove(
        reinterpret_cast<std::uintptr_t>(craft), step, &context);
}

bool runtime_enabled() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    static std::uint64_t previous = 0;
    const auto activation = a2fo::builtin_module_activation(A2FO_BUILTIN_FEATURE);
    struct Context {
        std::uint64_t* previous;
        std::uint64_t activation;
        std::optional<decltype(g_effect_ids)> effects;
        decltype(g_effect_ids)::iterator current;
    } context{&previous, activation, std::nullopt, {}};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        switch (operation) {
        case 0: *state.previous = state.activation; return 0;
        case 1:
            // Only game observers enter here; never run native effect cleanup
            // from a module-menu policy change or CRT teardown.
            state.effects.emplace(std::move(g_effect_ids));
            g_effect_ids.clear();
            state.current = state.effects->begin();
            return state.effects->size();
        case 2: {
            const auto id = state.current->second;
            ++state.current;
            return static_cast<std::uint32_t>(id);
        }
        case 3: stop_effect(static_cast<std::int32_t>(argument)); return 0;
        case 4:
            g_observed_crafts.clear();
            g_logged_creation_failures.clear();
            g_global_scan_completed = false;
            g_last_global_scan_tick = 0;
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_shield_activation(
        activation, previous, step, &context) != 0;
#else
    return g_runtime_ready;
#endif
}

void create_continuous_effect(void* craft) noexcept {
    struct Context {
        void* craft;
        std::int32_t effect = -1;
        bool fatal = false;
    } context{craft};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.fatal) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0:
                state.effect = reinterpret_cast<CreateShieldHit>(
                    at(kCreateShieldHitRva))(
                        state.craft, at(kIdentityMatrixRva),
                        kContinuousShieldType, -1.0f, kNativeEffectColour);
                return static_cast<std::uint32_t>(state.effect);
            case 1: return g_logged_creation_failures.insert(state.craft).second;
            case 2:
                log_line("Armada rejected a continuous shield-effect creation");
                return 0;
            case 3: return g_effect_ids.emplace(state.craft, state.effect).second;
            case 4: stop_effect(static_cast<std::int32_t>(argument)); return 0;
            case 5: g_logged_creation_failures.erase(state.craft); return 0;
            case 6: {
                char message[256]{};
                const char* odf_name = class_odf_name(
                    read_at<void*>(state.craft, kObjectClassOffset));
                std::snprintf(message, sizeof(message),
                    "Created continuous shield effect %ld for '%s'",
                    static_cast<long>(state.effect),
                    odf_name && *odf_name ? odf_name : "<unknown>");
                log_line(message);
                return 0;
            }
            case 7:
                log_line("Could not retain a persistent shield-effect identifier");
                return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            if (operation == 0 || operation == 4) state.fatal = true;
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_shield_create(
        reinterpret_cast<std::uintptr_t>(craft), step, &context);
    if (context.fatal) std::terminate();
}

struct ShieldUpdateContext {
    void* craft;
    void* object_class;
    float current_shields;
};
std::uintptr_t shield_update_step(void* opaque, std::uint32_t operation,
                                  std::uintptr_t argument) noexcept {
    auto& context = *static_cast<ShieldUpdateContext*>(opaque);
    switch (operation) {
    case 0: return runtime_enabled();
    case 1:
        context.object_class = read_at<void*>(reinterpret_cast<void*>(argument),
                                             kObjectClassOffset);
        return reinterpret_cast<std::uintptr_t>(context.object_class);
    case 2: return class_policy_enabled(reinterpret_cast<void*>(argument));
    case 3: {
        context.current_shields = read_at<float>(reinterpret_cast<void*>(argument),
                                                kCurrentShieldsOffset);
        std::uint32_t bits = 0;
        std::memcpy(&bits, &context.current_shields, sizeof(bits));
        return bits;
    }
    case 4:
        try {
            if (g_observed_crafts.insert(context.craft).second) {
                char message[256]{};
                const char* odf_name = class_odf_name(context.object_class);
                std::snprintf(message, sizeof(message),
                    "Observed configured craft '%s' with %.1f shields",
                    odf_name && *odf_name ? odf_name : "<unknown>",
                    static_cast<double>(context.current_shields));
                log_line(message);
            }
        } catch (...) {}
        return 0;
    case 5: {
        const auto effect = g_effect_ids.find(context.craft);
        const std::int32_t id = effect == g_effect_ids.end() ? -1 : effect->second;
        return static_cast<std::uint32_t>(id);
    }
    case 6: create_continuous_effect(context.craft); return 0;
    case 7: remove_continuous_effect(context.craft); return 0;
    default: return 0;
    }
}
void update_craft(void* craft) noexcept {
    ShieldUpdateContext context{craft, nullptr, 0.0f};
    a2fo_rs_stage8_shield_update(g_runtime_ready,
        reinterpret_cast<std::uintptr_t>(craft), shield_update_step, &context);
}

void update_all_crafts() noexcept {
    // This hook runs immediately before Armada traverses this same live list
    // for rendering. Validate the owner/sentinel only when they change, then
    // use direct reads for the nodes Armada is about to consume itself. A
    // VirtualQuery for every node and object was a large Wine-side cost.
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: {
            void* list_global = at(kGameObjectListRva);
            if (!readable_range(list_global, sizeof(void*))) return UINTPTR_MAX;
            void* owner = read_at<void*>(list_global, 0);
            if (owner != g_cached_list_owner) {
                if (!readable_range(owner, sizeof(void*) * 2)) return UINTPTR_MAX;
                g_cached_list_owner = owner;
            }
            return reinterpret_cast<std::uintptr_t>(owner);
        }
        case 1: {
            void* owner = reinterpret_cast<void*>(argument);
            void* current = read_at<void*>(owner, sizeof(void*));
            if (current != g_cached_list_sentinel) {
                if (!readable_range(current, sizeof(void*) * 3)) return 0;
                g_cached_list_sentinel = current;
            }
            return reinterpret_cast<std::uintptr_t>(g_cached_list_sentinel);
        }
        case 2: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(reinterpret_cast<void*>(argument), 0));
        case 3: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(reinterpret_cast<void*>(argument), 0));
        case 4: return reinterpret_cast<std::uintptr_t>(
            read_at<void*>(reinterpret_cast<void*>(argument), sizeof(void*) * 2));
        case 5: update_craft(reinterpret_cast<void*>(argument)); return 0;
        case 6:
            if (!g_logged_object_list_scan) {
                g_logged_object_list_scan = true;
                char message[160]{};
                std::snprintf(message, sizeof(message),
                    "Render pass reached the global GameObject list (%lu object(s))",
                    static_cast<unsigned long>(argument));
                log_line(message);
            }
            return 0;
        default: return 0;
        }
    };
    a2fo_rs_stage8_shield_scan(step, nullptr);
}

}  // namespace

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_RegisterClass(
    void* object_class, void* parameter_db) noexcept {
    struct Context { std::exception_ptr error; } context;
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        if (state.error) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0: return parameter_flag_enabled(reinterpret_cast<void*>(argument));
            case 1:
                if (g_enabled_classes.insert(reinterpret_cast<void*>(argument)).second) {
                char message[256]{};
                const char* odf_name = class_odf_name(reinterpret_cast<void*>(argument));
                std::snprintf(
                    message, sizeof(message),
                    "Registered alwaysShowShields on '%s'",
                    odf_name && *odf_name ? odf_name : "<unknown>");
                log_line(message);
                }
                return 0;
            case 2: g_enabled_classes.erase(reinterpret_cast<void*>(argument)); return 0;
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_shield_register(g_runtime_ready,
        reinterpret_cast<std::uintptr_t>(object_class),
        reinterpret_cast<std::uintptr_t>(parameter_db), step, &context);
    if (context.error) {
        log_line("Could not retain an alwaysShowShields class policy");
    }
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_UpdateCraft(void* craft) noexcept {
    update_craft(craft);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_CleanupCraft(void* craft) noexcept {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t argument) noexcept -> std::uintptr_t {
        void* craft = reinterpret_cast<void*>(argument);
        switch (operation) {
        case 0: remove_continuous_effect(craft); break;
        case 1: g_observed_crafts.erase(craft); break;
        case 2: g_logged_creation_failures.erase(craft); break;
        default: break;
        }
        return 0;
    };
    a2fo_rs_stage8_shield_cleanup(g_runtime_ready,
        reinterpret_cast<std::uintptr_t>(craft), step, nullptr);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api;
        std::exception_ptr error;
    } context{api, nullptr};
    const auto step = [](void* opaque, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& state = *static_cast<Context*>(opaque);
        const auto* api = state.api;
        if (state.error) return UINTPTR_MAX;
        try {
            switch (operation) {
            case 0:
                return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                    api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                    api->armada_module && api->fleetops_module &&
                    api->install_inline_hook && api->patch_jump &&
                    api->extension_root_count && api->extension_root;
            case 1:
                g_api = api;
                g_armada = static_cast<HMODULE>(api->armada_module());
                g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
                return g_armada && g_fleet_ops;
            case 2:
                g_api = nullptr;
                g_armada = nullptr;
                g_fleet_ops = nullptr;
                return 0;
            case 3: return signatures_supported();
            case 4:
                return api->install_inline_hook(
                    at(kStarbaseSimulateRva),
                    reinterpret_cast<void*>(&starbase_simulate_hook),
                    kExpectedStarbaseSimulate.size(),
                    kExpectedStarbaseSimulate.data(), &g_starbase_simulate_hook);
            case 5:
                log_line("Could not install the Starbase::Simulate observer");
                return 0;
            case 6:
                return api->install_inline_hook(
                    at(kRenderGameObjectsRva),
                    reinterpret_cast<void*>(&render_game_objects_hook),
                    kExpectedRenderGameObjects.size(),
                    kExpectedRenderGameObjects.data(), &g_render_game_objects_hook);
            case 7:
                log_line("Could not install the RenderGameObjects observer");
                return 0;
            case 8: return install_publication_hook(api);
            case 9:
                log_line("Could not install the GameObject publication observer");
                return 0;
            case 10:
                try { index_loose_odfs(); }
                catch (...) {
                    log_line("Could not index loose ODFs for preloaded class fallback");
                }
                return 0;
            case 11: {
                g_runtime_ready = true;
                char message[192]{};
                std::snprintf(message, sizeof(message),
                    "alwaysShowShields policy initialized with global object-list, "
                    "mission-publication, and Starbase coverage; missing commands "
                    "default to 0; indexed %lu loose ODF basename(s)",
                    static_cast<unsigned long>(g_loose_odf_paths.size()));
                log_line(message);
                return 0;
            }
            default: return UINTPTR_MAX;
            }
        } catch (...) {
            state.error = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_shield_init(step, &context) != 0;
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    const auto step = [](void*, std::uint32_t operation,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        switch (operation) {
        case 0: g_runtime_ready = false; break;
        case 1: g_enabled_classes.clear(); break;
        case 2: g_loose_resolved_classes.clear(); break;
        case 3: g_effect_ids.clear(); break;
        case 4: g_observed_crafts.clear(); break;
        case 5: g_logged_creation_failures.clear(); break;
        case 6: g_loose_odf_paths.clear(); break;
        case 7: g_logged_object_list_scan = false; break;
        case 8: g_cached_list_owner = nullptr; break;
        case 9: g_cached_list_sentinel = nullptr; break;
        case 10: g_last_global_scan_tick = 0; break;
        case 11: g_global_scan_completed = false; break;
        default: break;
        }
        return 0;
    };
    a2fo_rs_stage8_shield_shutdown(step, nullptr);
}

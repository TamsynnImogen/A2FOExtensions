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

#include "../../sdk/include/a2fo_module_api.h"
#include "shield_visibility.hpp"

#include <windows.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

using a2fo::shields::EffectAction;
using CreateShieldHit = std::int32_t (__cdecl *)(
    void* object, const void* shield_matrix, std::int32_t shield_type,
    float duration, std::int32_t flags);
using StopShieldEffect = void (__cdecl *)(std::int32_t effect_id);
using PublishGameObject = std::uintptr_t (__cdecl *)(
    void* mission, void* object);

constexpr const char* kModuleName = "A2FOAlwaysShowShields";
constexpr char kOdfCommand[] = "alwaysShowShields";
constexpr std::size_t kMaximumOdfDirectoryDepth = 24;
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
constexpr std::uint32_t kConfiguredGlobalScanIntervalMs = 100;
constexpr std::uint32_t kIdleGlobalScanIntervalMs = 1000;

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
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string trim_copy(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    const char last = left.back();
    if (last == '\\' || last == '/') return left + right;
    return left + "\\" + right;
}

bool has_odf_extension(const std::string& name) {
    const std::string normalized = lower_ascii(name);
    return normalized.size() > 4 &&
        normalized.compare(normalized.size() - 4, 4, ".odf") == 0;
}

std::string normalize_odf_name(std::string name) {
    name = trim_copy(std::move(name));
    std::replace(name.begin(), name.end(), '/', '\\');
    const std::size_t slash = name.find_last_of('\\');
    if (slash != std::string::npos) name.erase(0, slash + 1);
    name = lower_ascii(std::move(name));
    if (!name.empty() && !has_odf_extension(name)) name += ".odf";
    return name;
}

void index_odf_directory(const std::string& directory,
                         std::size_t depth) {
    if (directory.empty() || depth > kMaximumOdfDirectoryDepth) return;
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(join_path(directory, "*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;

    std::vector<std::pair<std::string, bool>> children;
    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        const bool is_directory =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory &&
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        if (is_directory || has_odf_extension(name)) {
            children.emplace_back(name, is_directory);
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);

    std::sort(children.begin(), children.end(),
              [](const auto& left, const auto& right) {
                  return lower_ascii(left.first) < lower_ascii(right.first);
              });
    for (const auto& child : children) {
        const std::string path = join_path(directory, child.first);
        if (child.second) {
            index_odf_directory(path, depth + 1);
        } else {
            // Roots are visited from parent to child, so later active-mod
            // files intentionally replace a same-named parent ODF.
            g_loose_odf_paths[lower_ascii(child.first)] = path;
        }
    }
}

void index_loose_odfs() {
    g_loose_odf_paths.clear();
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return;
    }
    const std::uint32_t count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* root = g_api->extension_root(index);
        if (root && *root) {
            index_odf_directory(join_path(root, "odf"), 0);
        }
    }
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

std::string strip_line_comment(const std::string& line) {
    bool quoted = false;
    for (std::size_t index = 0; index + 1 < line.size(); ++index) {
        if (line[index] == '"') quoted = !quoted;
        if (!quoted && line[index] == '/' && line[index + 1] == '/') {
            return line.substr(0, index);
        }
    }
    return line;
}

std::optional<bool> parse_bool_value(std::string value) {
    value = lower_ascii(trim_copy(std::move(value)));
    if (!value.empty() && value.back() == ';') {
        value.pop_back();
        value = trim_copy(std::move(value));
    }
    if (value == "true" || value == "yes" || value == "on") return true;
    if (value == "false" || value == "no" || value == "off") return false;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end != value.c_str()) {
        while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
        if (*end == '\0') return parsed != 0;
    }
    return std::nullopt;
}

std::optional<bool> find_local_flag(
    const std::string& contents, std::vector<std::string>* includes) {
    std::optional<bool> result;
    std::istringstream lines(contents);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim_copy(strip_line_comment(line));
        if (line.empty()) continue;
        const std::string normalized = lower_ascii(line);
        if (normalized.rfind("#include", 0) == 0) {
            const std::size_t first_quote = line.find('"');
            const std::size_t last_quote = line.find_last_of('"');
            if (includes && first_quote != std::string::npos &&
                last_quote > first_quote) {
                includes->push_back(normalize_odf_name(line.substr(
                    first_quote + 1, last_quote - first_quote - 1)));
            }
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        if (lower_ascii(trim_copy(line.substr(0, equals))) !=
            lower_ascii(kOdfCommand)) {
            continue;
        }
        const std::optional<bool> parsed = parse_bool_value(
            line.substr(equals + 1));
        if (parsed.has_value()) result = parsed;
    }
    return result;
}

std::optional<bool> resolve_loose_flag(
    const std::string& requested_name,
    std::unordered_set<std::string>* visiting) {
    if (!visiting) return std::nullopt;
    const std::string key = normalize_odf_name(requested_name);
    if (key.empty() || !visiting->insert(key).second) return std::nullopt;
    const auto selected = g_loose_odf_paths.find(key);
    if (selected == g_loose_odf_paths.end()) {
        visiting->erase(key);
        return std::nullopt;
    }
    std::string contents;
    if (!read_small_text_file(selected->second, &contents)) {
        visiting->erase(key);
        return std::nullopt;
    }
    std::vector<std::string> includes;
    const std::optional<bool> local = find_local_flag(contents, &includes);
    if (local.has_value()) {
        visiting->erase(key);
        return local;
    }
    for (auto include = includes.rbegin(); include != includes.rend();
         ++include) {
        const std::optional<bool> inherited =
            resolve_loose_flag(*include, visiting);
        if (inherited.has_value()) {
            visiting->erase(key);
            return inherited;
        }
    }
    visiting->erase(key);
    return std::nullopt;
}

bool class_policy_enabled(void* object_class) noexcept {
    if (!object_class) return false;
    if (g_enabled_classes.find(object_class) != g_enabled_classes.end()) {
        return true;
    }
    // A completed ParameterDB callback that did not find the command is not
    // sufficient to suppress the loose-ODF fallback. Fleet Operations can
    // invoke that callback before an active-mod override is fully represented
    // in the class database. Resolve each otherwise-disabled class once from
    // the indexed Data/parent/active-mod hierarchy.
    if (g_loose_resolved_classes.find(object_class) !=
        g_loose_resolved_classes.end()) {
        return false;
    }
    try {
        g_loose_resolved_classes.insert(object_class);
        const char* native_name = class_odf_name(object_class);
        const std::string odf_name = native_name ? native_name : "";
        std::unordered_set<std::string> visiting;
        const std::optional<bool> enabled =
            resolve_loose_flag(odf_name, &visiting);
        if (enabled.value_or(false)) g_enabled_classes.insert(object_class);
        return enabled.value_or(false);
    } catch (...) {
        log_line("Could not resolve a preloaded shield class policy");
        return false;
    }
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
    a2fo_shields_call_thiscall_1(
        g_starbase_simulate_hook.gateway, starbase, elapsed_bits);
    if (g_runtime_ready) update_craft(starbase);
}

void __attribute__((fastcall)) render_game_objects_hook(
    void* renderer, void*, std::uintptr_t render_argument) noexcept {
    // This stock render pass is the one authoritative traversal of Armada's
    // complete GameObject list. Run immediately before it builds the frame so
    // hidden Fleet Operations subclasses cannot bypass shield observation.
    if (g_runtime_ready) {
        const std::uint32_t now = GetTickCount();
        const std::uint32_t interval = g_enabled_classes.empty()
            ? kIdleGlobalScanIntervalMs
            : kConfiguredGlobalScanIntervalMs;
        if (a2fo::shields::global_scan_due(
                now, g_last_global_scan_tick, g_global_scan_completed,
                interval)) {
            g_last_global_scan_tick = now;
            g_global_scan_completed = true;
            update_all_crafts();
        }
    }
    a2fo_shields_call_thiscall_1(
        g_render_game_objects_hook.gateway, renderer, render_argument);
}

std::uintptr_t __cdecl game_object_publish_hook(
    void* mission, void* object) noexcept {
    const std::uintptr_t result =
        reinterpret_cast<PublishGameObject>(
            g_game_object_publish_original)(mission, object);
    // Both AiMission::AddObject and the native object-creation path enter this
    // dispatcher after construction. Observing it avoids relying on either a
    // convenience wrapper or a hidden subclass' virtual methods.
    if (g_runtime_ready && object) update_craft(object);
    return result;
}

bool install_publication_hook(const A2FO_ModuleApi* api) noexcept {
    if (!api || !api->install_inline_hook || !api->patch_jump) return false;
    void* site = at(kGameObjectPublishRva);
    if (readable_range(site, kExpectedGameObjectPublish.size()) &&
        std::memcmp(site, kExpectedGameObjectPublish.data(),
                    kExpectedGameObjectPublish.size()) == 0) {
        if (!api->install_inline_hook(
                site, reinterpret_cast<void*>(&game_object_publish_hook),
                kExpectedGameObjectPublish.size(),
                kExpectedGameObjectPublish.data(),
                &g_game_object_publish_hook)) {
            return false;
        }
        g_game_object_publish_original =
            g_game_object_publish_hook.gateway;
        return g_game_object_publish_original != nullptr;
    }

    std::size_t patch_length = 0;
    void* destination = existing_detour_destination(site, &patch_length);
    if (patch_length < 5 || patch_length > 6 ||
        !fleet_ops_executable_address(destination)) {
        return false;
    }
    std::array<std::uint8_t, 6> expected_detour{};
    std::memcpy(expected_detour.data(), site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&game_object_publish_hook),
            expected_detour.data(), patch_length)) {
        return false;
    }
    g_game_object_publish_original = destination;
    const auto handler_rva = reinterpret_cast<std::uintptr_t>(destination) -
        reinterpret_cast<std::uintptr_t>(g_fleet_ops);
    char message[224]{};
    std::snprintf(
        message, sizeof(message),
        "Chained Fleet Operations GameObject publication detour at RVA "
        "0x%08lX",
        static_cast<unsigned long>(handler_rva));
    log_line(message);
    return true;
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

    std::string normalized(value.data());
    for (char& character : normalized) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    const std::size_t first = normalized.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return false;
    const std::size_t last = normalized.find_last_not_of(" \t\r\n");
    normalized = normalized.substr(first, last - first + 1);
    return normalized == "true" || normalized == "yes" ||
        normalized == "on" ||
        std::strtol(normalized.c_str(), nullptr, 10) != 0;
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
    const auto found = g_effect_ids.find(craft);
    if (found == g_effect_ids.end()) return;
    const std::int32_t effect_id = found->second;
    g_effect_ids.erase(found);
    stop_effect(effect_id);
}

void create_continuous_effect(void* craft) noexcept {
    const std::int32_t effect_id =
        reinterpret_cast<CreateShieldHit>(at(kCreateShieldHitRva))(
            craft, at(kIdentityMatrixRva), kContinuousShieldType,
            -1.0f, kNativeEffectColour);
    if (effect_id < 0) {
        try {
            if (g_logged_creation_failures.insert(craft).second) {
                log_line(
                    "Armada rejected a continuous shield-effect creation");
            }
        } catch (...) {
        }
        return;
    }
    try {
        const auto inserted = g_effect_ids.emplace(craft, effect_id);
        if (!inserted.second) {
            stop_effect(effect_id);
            return;
        }
        g_logged_creation_failures.erase(craft);
        char message[256]{};
        const char* odf_name = class_odf_name(
            read_at<void*>(craft, kObjectClassOffset));
        std::snprintf(
            message, sizeof(message),
            "Created continuous shield effect %ld for '%s'",
            static_cast<long>(effect_id),
            odf_name && *odf_name ? odf_name : "<unknown>");
        log_line(message);
    } catch (...) {
        stop_effect(effect_id);
        log_line("Could not retain a persistent shield-effect identifier");
    }
}

void update_craft(void* craft) noexcept {
    if (!g_runtime_ready || !craft) return;

    void* object_class = read_at<void*>(craft, kObjectClassOffset);
    if (!class_policy_enabled(object_class)) return;
    const float current_shields =
        read_at<float>(craft, kCurrentShieldsOffset);
    try {
        if (g_observed_crafts.insert(craft).second) {
            char message[256]{};
            const char* odf_name = class_odf_name(object_class);
            std::snprintf(
                message, sizeof(message),
                "Observed configured craft '%s' with %.1f shields",
                odf_name && *odf_name ? odf_name : "<unknown>",
                static_cast<double>(current_shields));
            log_line(message);
        }
    } catch (...) {
    }
    const auto effect = g_effect_ids.find(craft);
    const std::int32_t effect_id = effect == g_effect_ids.end()
        ? -1 : effect->second;
    switch (a2fo::shields::choose_effect_action(
                true, current_shields, effect_id)) {
        case EffectAction::show:
            create_continuous_effect(craft);
            break;
        case EffectAction::hide:
            remove_continuous_effect(craft);
            break;
        case EffectAction::none:
            break;
    }
}

void update_all_crafts() noexcept {
    void* list_global = at(kGameObjectListRva);
    if (!readable_range(list_global, sizeof(void*))) return;
    void* list_owner = read_at<void*>(list_global, 0);
    if (list_owner != g_cached_list_owner) {
        if (!readable_range(list_owner, sizeof(void*) * 2)) return;
        g_cached_list_owner = list_owner;
    }
    void* current_sentinel = read_at<void*>(list_owner, sizeof(void*));
    if (current_sentinel != g_cached_list_sentinel) {
        if (!readable_range(current_sentinel, sizeof(void*) * 3)) return;
        g_cached_list_sentinel = current_sentinel;
    }
    void* sentinel = g_cached_list_sentinel;
    if (!sentinel) return;

    // This hook runs immediately before Armada traverses this same live list
    // for rendering. Validate the owner/sentinel only when they change, then
    // use direct reads for the nodes Armada is about to consume itself. A
    // VirtualQuery for every node and object was a large Wine-side cost.
    void* node = read_at<void*>(sentinel, 0);
    std::size_t visited = 0;
    constexpr std::size_t kMaximumObjectsPerPass = 65536;
    while (node && node != sentinel && visited < kMaximumObjectsPerPass) {
        void* next = read_at<void*>(node, 0);
        void* object = read_at<void*>(node, sizeof(void*) * 2);
        if (object) update_craft(object);
        ++visited;
        if (next == node) break;
        node = next;
    }
    if (!g_logged_object_list_scan) {
        g_logged_object_list_scan = true;
        char message[160]{};
        std::snprintf(
            message, sizeof(message),
            "Render pass reached the global GameObject list (%lu object(s))",
            static_cast<unsigned long>(visited));
        log_line(message);
    }
}

}  // namespace

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_RegisterClass(
    void* object_class, void* parameter_db) noexcept {
    if (!g_runtime_ready || !object_class || !parameter_db) return;
    try {
        if (parameter_flag_enabled(parameter_db)) {
            if (g_enabled_classes.insert(object_class).second) {
                char message[256]{};
                const char* odf_name = class_odf_name(object_class);
                std::snprintf(
                    message, sizeof(message),
                    "Registered alwaysShowShields on '%s'",
                    odf_name && *odf_name ? odf_name : "<unknown>");
                log_line(message);
            }
        } else {
            g_enabled_classes.erase(object_class);
        }
    } catch (...) {
        log_line("Could not retain an alwaysShowShields class policy");
    }
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_UpdateCraft(void* craft) noexcept {
    update_craft(craft);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FOAlwaysShowShields_CleanupCraft(void* craft) noexcept {
    if (!g_runtime_ready || !craft) return;
    remove_continuous_effect(craft);
    g_observed_crafts.erase(craft);
    g_logged_creation_failures.erase(craft);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->patch_jump ||
        !api->extension_root_count ||
        !api->extension_root) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) {
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    if (!signatures_supported()) {
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    if (!api->install_inline_hook(
            at(kStarbaseSimulateRva),
            reinterpret_cast<void*>(&starbase_simulate_hook),
            kExpectedStarbaseSimulate.size(),
            kExpectedStarbaseSimulate.data(),
            &g_starbase_simulate_hook)) {
        log_line("Could not install the Starbase::Simulate observer");
        // Earlier inline hooks cannot be safely removed. Keep this DLL loaded
        // as a pass-through if a post-preflight installation unexpectedly
        // fails.
        return true;
    }
    if (!api->install_inline_hook(
            at(kRenderGameObjectsRva),
            reinterpret_cast<void*>(&render_game_objects_hook),
            kExpectedRenderGameObjects.size(),
            kExpectedRenderGameObjects.data(),
            &g_render_game_objects_hook)) {
        log_line("Could not install the RenderGameObjects observer");
        return true;
    }
    if (!install_publication_hook(api)) {
        log_line("Could not install the GameObject publication observer");
        return true;
    }
    try {
        index_loose_odfs();
    } catch (...) {
        log_line("Could not index loose ODFs for preloaded class fallback");
    }
    g_runtime_ready = true;
    char message[192]{};
    std::snprintf(
        message, sizeof(message),
        "alwaysShowShields policy initialized with global object-list, "
        "mission-publication, and Starbase coverage; missing commands "
        "default to 0; indexed %lu "
        "loose ODF basename(s)",
        static_cast<unsigned long>(g_loose_odf_paths.size()));
    log_line(message);
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    g_runtime_ready = false;
    g_enabled_classes.clear();
    g_loose_resolved_classes.clear();
    g_effect_ids.clear();
    g_observed_crafts.clear();
    g_logged_creation_failures.clear();
    g_loose_odf_paths.clear();
    g_logged_object_list_scan = false;
    g_cached_list_owner = nullptr;
    g_cached_list_sentinel = nullptr;
    g_last_global_scan_tick = 0;
    g_global_scan_completed = false;
}

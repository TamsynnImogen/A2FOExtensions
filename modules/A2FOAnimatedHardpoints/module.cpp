/*
 * Per-instance transform animation for Armada II hardpoint/null nodes.
 *
 * Storm3D evaluates SOD matrix channels against the visible database before
 * rendering. Gameplay hardpoint queries instead use the logical database and
 * therefore see only the static SOD hierarchy. This module evaluates the
 * existing visible channels at the owning instance's time and uses the
 * corresponding visible null node for world-position/transform queries.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>

extern "C" {
std::uintptr_t __cdecl a2fo_animated_hardpoints_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_animated_hardpoints_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
std::uintptr_t __cdecl a2fo_animated_hardpoints_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
}

namespace {

constexpr char kModuleName[] = "A2FOAnimatedHardpoints";

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs, derived from armada2.map
// and the supported ArmadaL image.
constexpr std::uintptr_t kInstanceGetWorldPositionRva = 0x0022e470;
constexpr std::uintptr_t kInstanceGetWorldTransformRva = 0x0022e500;
constexpr std::uintptr_t kNodeFindRecursiveRva = 0x00238780;
constexpr std::uintptr_t kAnimationSetStartTimeRva = 0x00213670;
constexpr std::uintptr_t kAnimationForceFirstFrameRva = 0x002136c0;
constexpr std::uintptr_t kDatabaseFirstAnimationRva = 0x0022f1c0;
constexpr std::uintptr_t kAnimationNextRva = 0x00221080;
constexpr std::uintptr_t kMatrixAnimationVtableRva = 0x002bc42c;
constexpr std::uintptr_t kAnimationCurrentTimeRva = 0x003a8e74;
constexpr std::uintptr_t kAnimationStartTimeRva = 0x003a8e78;

constexpr std::size_t kInstanceVisibleDatabaseOffset = 0x80;
constexpr std::size_t kDatabaseHierarchyRootOffset = 0x3c;
constexpr std::size_t kNodeNameOffset = 0x08;
constexpr std::size_t kNodeParentOffset = 0x18;
constexpr std::size_t kAnimationTriggeredOffset = 0x76;
constexpr std::size_t kAnimationStartOffset = 0x18;
constexpr std::size_t kMaximumHierarchyDepth = 256;
constexpr std::size_t kMaximumAnimationChannels = 4096;
constexpr std::size_t kMaximumIdentifierLength = 255;

constexpr std::array<std::uint8_t, 6> kExpectedWorldPosition{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x60};
constexpr std::array<std::uint8_t, 6> kExpectedWorldTransform{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x60};
constexpr std::array<std::uint8_t, 5> kExpectedFindRecursive{
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::array<std::uint8_t, 6> kExpectedSetStartTime{
    0x55, 0x8b, 0xec, 0x51, 0x8b, 0x4d};
constexpr std::array<std::uint8_t, 5> kExpectedFirstAnimation{
    0x8b, 0x41, 0x70, 0x85, 0xc0};
constexpr std::array<std::uint8_t, 6> kExpectedNextAnimation{
    0x8b, 0x41, 0x0c, 0x8b, 0x49, 0x04};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
A2FO_InlineHook g_world_position_hook{};
A2FO_InlineHook g_world_transform_hook{};
bool g_runtime_ready = false;
bool g_inside_lookup = false;
bool g_logged_first_animation = false;

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(module) + rva);
}

void log_line(const char* message) noexcept {
    if (g_api && g_api->log && message) g_api->log(kModuleName, message);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || size == 0) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    if (begin + size < begin) return false;

    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(pointer, &information, sizeof(information)) !=
        sizeof(information)) {
        return false;
    }
    if (information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto region_begin = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    const auto region_end = region_begin + information.RegionSize;
    return begin >= region_begin && begin + size <= region_end;
}

template <typename T>
T read_at(const void* base, std::size_t offset, T fallback = T{}) noexcept {
    if (!base) return fallback;
    const auto* address = reinterpret_cast<const std::uint8_t*>(base) + offset;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

bool readable_identifier(const char* value) noexcept {
    if (!value) return false;
    for (std::size_t index = 0; index <= kMaximumIdentifierLength; ++index) {
        if (!readable_range(value + index, 1)) return false;
        if (value[index] == '\0') return index != 0;
    }
    return false;
}

bool same_identifier(const char* left, const char* right) noexcept {
    return readable_identifier(left) && readable_identifier(right) &&
           lstrcmpiA(left, right) == 0;
}

bool signature_matches(std::uintptr_t rva, const std::uint8_t* expected,
                       std::size_t size) noexcept {
    const void* address = at(g_armada, rva);
    return readable_range(address, size) &&
           std::memcmp(address, expected, size) == 0;
}

bool is_null_node(void* node) noexcept {
    void** vtable = read_at<void**>(node, 0, nullptr);
    if (!vtable || !readable_range(vtable + 1, sizeof(void*))) return false;
    void* get_type = vtable[1];
    if (!readable_range(get_type, 1)) return false;
    return a2fo_animated_hardpoints_call_thiscall_0(get_type, node) == 0;
}

void* first_animation(void* database) noexcept {
    return reinterpret_cast<void*>(
        a2fo_animated_hardpoints_call_thiscall_0(
            at(g_armada, kDatabaseFirstAnimationRva), database));
}

void* next_animation(void* channel) noexcept {
    return reinterpret_cast<void*>(
        a2fo_animated_hardpoints_call_thiscall_0(
            at(g_armada, kAnimationNextRva), channel));
}

bool is_matrix_animation(void* channel) noexcept {
    return read_at<void*>(channel, 0, nullptr) ==
           at(g_armada, kMatrixAnimationVtableRva);
}

std::size_t collect_ancestor_names(
    void* node, std::array<const char*, kMaximumHierarchyDepth>& names) noexcept {
    std::size_t count = 0;
    while (node && count < names.size()) {
        const char* name = read_at<const char*>(node, kNodeNameOffset, nullptr);
        if (!readable_identifier(name)) break;
        names[count++] = name;
        void* parent = read_at<void*>(node, kNodeParentOffset, nullptr);
        if (parent == node) break;
        node = parent;
    }
    return count;
}

bool hierarchy_has_matrix_channel(
    void* database,
    const std::array<const char*, kMaximumHierarchyDepth>& names,
    std::size_t name_count) noexcept {
    void* channel = first_animation(database);
    for (std::size_t index = 0;
         channel && index < kMaximumAnimationChannels;
         ++index, channel = next_animation(channel)) {
        if (!is_matrix_animation(channel)) continue;
        const char* channel_name =
            read_at<const char*>(channel, kNodeNameOffset, nullptr);
        for (std::size_t name_index = 0; name_index < name_count;
             ++name_index) {
            if (same_identifier(channel_name, names[name_index])) return true;
        }
    }
    return false;
}

void evaluate_matrix_channels(void* instance, void* database) noexcept {
    const float saved_current = read_at<float>(
        at(g_armada, kAnimationCurrentTimeRva), 0, 0.0f);
    const float saved_start = read_at<float>(
        at(g_armada, kAnimationStartTimeRva), 0, 0.0f);

    using SetStartTime = float(__cdecl*)(float);
    using ForceFirstFrame = void(__cdecl*)();
    const bool triggered =
        read_at<std::uint8_t>(instance, kAnimationTriggeredOffset, 0) != 0;
    if (triggered) {
        const float start =
            read_at<float>(instance, kAnimationStartOffset, 0.0f);
        reinterpret_cast<SetStartTime>(
            at(g_armada, kAnimationSetStartTimeRva))(start);
    } else {
        reinterpret_cast<ForceFirstFrame>(
            at(g_armada, kAnimationForceFirstFrameRva))();
    }

    void* channel = first_animation(database);
    for (std::size_t index = 0;
         channel && index < kMaximumAnimationChannels;
         ++index, channel = next_animation(channel)) {
        if (!is_matrix_animation(channel)) continue;
        void** vtable = read_at<void**>(channel, 0, nullptr);
        if (!vtable || !readable_range(vtable + 4, sizeof(void*))) continue;
        void* evaluate_and_play = vtable[4];
        if (!readable_range(evaluate_and_play, 1)) continue;
        a2fo_animated_hardpoints_call_thiscall_1(
            evaluate_and_play, channel, 0);
    }

    if (readable_range(at(g_armada, kAnimationCurrentTimeRva), sizeof(float))) {
        std::memcpy(at(g_armada, kAnimationCurrentTimeRva),
                    &saved_current, sizeof(saved_current));
    }
    if (readable_range(at(g_armada, kAnimationStartTimeRva), sizeof(float))) {
        std::memcpy(at(g_armada, kAnimationStartTimeRva),
                    &saved_start, sizeof(saved_start));
    }
}

void* animated_visible_node(void* instance, void* logical_node) noexcept {
    if (!instance || !logical_node || !is_null_node(logical_node)) return nullptr;

    void* database = read_at<void*>(
        instance, kInstanceVisibleDatabaseOffset, nullptr);
    if (!database) return nullptr;

    std::array<const char*, kMaximumHierarchyDepth> names{};
    const std::size_t name_count = collect_ancestor_names(logical_node, names);
    if (name_count == 0 ||
        !hierarchy_has_matrix_channel(database, names, name_count)) {
        return nullptr;
    }

    void* root = read_at<void*>(
        database, kDatabaseHierarchyRootOffset, nullptr);
    if (!root) return nullptr;
    void* visible_node = reinterpret_cast<void*>(
        a2fo_animated_hardpoints_call_thiscall_1(
            at(g_armada, kNodeFindRecursiveRva), root,
            reinterpret_cast<std::uintptr_t>(names[0])));
    if (!visible_node) return nullptr;

    evaluate_matrix_channels(instance, database);
    if (!g_logged_first_animation) {
        g_logged_first_animation = true;
        log_line("Applied a per-instance SOD matrix channel to a hardpoint "
                 "world-transform query");
    }
    return visible_node;
}

std::uintptr_t call_original(const A2FO_InlineHook& hook, void* instance,
                             void* output, void* node) noexcept {
    return a2fo_animated_hardpoints_call_thiscall_2(
        hook.gateway, instance, reinterpret_cast<std::uintptr_t>(output),
        reinterpret_cast<std::uintptr_t>(node));
}

std::uintptr_t __attribute__((fastcall)) world_position_hook(
    void* instance, void*, void* output, void* node) noexcept {
    if (!g_runtime_ready || g_inside_lookup) {
        return call_original(g_world_position_hook, instance, output, node);
    }
    g_inside_lookup = true;
    void* selected = animated_visible_node(instance, node);
    const std::uintptr_t result = call_original(
        g_world_position_hook, instance, output, selected ? selected : node);
    g_inside_lookup = false;
    return result;
}

std::uintptr_t __attribute__((fastcall)) world_transform_hook(
    void* instance, void*, void* output, void* node) noexcept {
    if (!g_runtime_ready || g_inside_lookup) {
        return call_original(g_world_transform_hook, instance, output, node);
    }
    g_inside_lookup = true;
    void* selected = animated_visible_node(instance, node);
    const std::uintptr_t result = call_original(
        g_world_transform_hook, instance, output, selected ? selected : node);
    g_inside_lookup = false;
    return result;
}

bool preflight() noexcept {
    using a2fo::supported_armada::Identity;
    if (a2fo::supported_armada::identify(g_armada) == Identity::unsupported) {
        log_line("Unsupported ArmadaL executable; runtime disabled");
        return false;
    }
    if (!signature_matches(kInstanceGetWorldPositionRva,
                           kExpectedWorldPosition.data(),
                           kExpectedWorldPosition.size()) ||
        !signature_matches(kInstanceGetWorldTransformRva,
                           kExpectedWorldTransform.data(),
                           kExpectedWorldTransform.size()) ||
        !signature_matches(kNodeFindRecursiveRva,
                           kExpectedFindRecursive.data(),
                           kExpectedFindRecursive.size()) ||
        !signature_matches(kAnimationSetStartTimeRva,
                           kExpectedSetStartTime.data(),
                           kExpectedSetStartTime.size()) ||
        !signature_matches(kDatabaseFirstAnimationRva,
                           kExpectedFirstAnimation.data(),
                           kExpectedFirstAnimation.size()) ||
        !signature_matches(kAnimationNextRva,
                           kExpectedNextAnimation.data(),
                           kExpectedNextAnimation.size()) ||
        !readable_range(at(g_armada, kAnimationForceFirstFrameRva), 1) ||
        !readable_range(at(g_armada, kMatrixAnimationVtableRva), sizeof(void*)) ||
        !readable_range(at(g_armada, kAnimationCurrentTimeRva), sizeof(float)) ||
        !readable_range(at(g_armada, kAnimationStartTimeRva), sizeof(float))) {
        log_line("Supported hardpoint-animation signatures were not found; "
                 "runtime disabled");
        return false;
    }
    return true;
}

bool install_hooks() noexcept {
    if (!g_api || !g_api->install_inline_hook || !preflight()) return false;
    bool installed = g_api->install_inline_hook(
        at(g_armada, kInstanceGetWorldPositionRva),
        reinterpret_cast<void*>(&world_position_hook),
        kExpectedWorldPosition.size(), kExpectedWorldPosition.data(),
        &g_world_position_hook);
    installed = g_api->install_inline_hook(
        at(g_armada, kInstanceGetWorldTransformRva),
        reinterpret_cast<void*>(&world_transform_hook),
        kExpectedWorldTransform.size(), kExpectedWorldTransform.data(),
        &g_world_transform_hook) && installed;
    if (!installed) {
        log_line("Could not install both checked world-transform hooks; "
                 "installed hooks remain pass-through");
    }
    return installed;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada) return false;

    g_runtime_ready = install_hooks();
    log_line(g_runtime_ready
                 ? "Hardpoint matrix-channel playback initialized"
                 : "Hardpoint matrix-channel module loaded with runtime disabled");
    // Hooks are process-lifetime patches. Keep the DLL resident even if a
    // later hook failed so any installed entry remains a safe pass-through.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {}

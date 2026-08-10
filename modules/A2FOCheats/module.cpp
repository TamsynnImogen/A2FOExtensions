/*
 * File: modules/A2FOCheats/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Optional, signature-checked Fleet Operations cheat extensions.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

constexpr const char* kModuleName = "A2FOCheats";
constexpr const char* kRevision = "A2FOCheats-rebuild-20260809-05";
constexpr const char* kRtsConfigFileName = "RTS_CFG.h";
constexpr std::streamoff kMaximumRtsConfigSize = 2 * 1024 * 1024;
constexpr float kDefaultGrantAmount = 10000.0f;
constexpr float kMaximumConfiguredGrantAmount = 100000000.0f;

// FleetOpsHook.dll 3.2.7 preferred base 0x5a800000.
constexpr std::uintptr_t kChatRegisterCheatRva = 0x1fc2d0;
constexpr std::uintptr_t kShowMeTheMoneyRva = 0x1fc320;
constexpr std::uintptr_t kChatHookInitRva = 0x1fccc8;
constexpr std::uintptr_t kCheatRegistryRva = 0x24a304;
constexpr std::uintptr_t kCurrentPlayerDispatchLinkRva = 0x21326c;
constexpr std::uintptr_t kTeamLookupDispatchLinkRva = 0x2129bc;
constexpr std::uintptr_t kSelectionDispatchLinkRva = 0x2121bc;
constexpr std::uintptr_t kEntityGetDispatchLinkRva = 0x21275c;
constexpr std::uintptr_t kDynamicCastDispatchLinkRva = 0x211f04;
constexpr std::uintptr_t kGameTypeDispatchLinkRva = 0x210fe8;
constexpr std::uintptr_t kAddSuppliesRva = 0x1e35d0;
constexpr std::uintptr_t kAddCrewRva = 0x1e35ec;
constexpr std::uintptr_t kAddDilithiumRva = 0x1e3608;
constexpr std::uintptr_t kAddTritaniumRva = 0x1e3624;
constexpr std::uintptr_t kAddMetalRva = 0x1e3640;

// ArmadaL.exe preferred base 0x00400000.
constexpr std::uintptr_t kEliminateTeamRva = 0x0007e8a0;
constexpr std::uintptr_t kAddCrewCapacityRva = 0x000975b0;
constexpr std::uintptr_t kDisableShieldGeneratorRva = 0x000ca010;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kQueueCommandVectorRva = 0x000d4490;
constexpr std::uintptr_t kGameObjectTypeDescriptorRva = 0x002f1280;
constexpr std::uintptr_t kCraftTypeDescriptorRva = 0x002f12a0;
constexpr std::uintptr_t kGameTypeTypeDescriptorRva = 0x002fa7b8;
constexpr std::uintptr_t kGameTypeGeneralTypeDescriptorRva = 0x002fa870;

constexpr std::size_t kSelectionCountOffset = 0x00b8;
constexpr std::size_t kSelectionHandlesOffset = 0x03d0;
constexpr std::size_t kGameObjectTeamOffset = 0x00ec;
constexpr std::int32_t kMaximumSelectionCount = 512;
constexpr std::int32_t kMaximumTeamCount = 9;
constexpr std::int32_t kMoveCommand = 4;
constexpr float kMoveDistance = 200.0f;
constexpr float kShieldDisableDuration = 10000.0f;

struct GrantAmounts {
    float dilithium = kDefaultGrantAmount;
    float tritanium = kDefaultGrantAmount;
    float metal = kDefaultGrantAmount;
    float supplies = kDefaultGrantAmount;
    float crew = kDefaultGrantAmount;
};

struct GrantField {
    const char* name;
    float GrantAmounts::*member;
};

constexpr std::array<GrantField, 5> kGrantFields{{
    {"SHOWMETHEMONEY_DILITHIUM", &GrantAmounts::dilithium},
    {"SHOWMETHEMONEY_TRITANIUM", &GrantAmounts::tritanium},
    {"SHOWMETHEMONEY_METAL", &GrantAmounts::metal},
    {"SHOWMETHEMONEY_SUPPLIES", &GrantAmounts::supplies},
    {"SHOWMETHEMONEY_CREW", &GrantAmounts::crew},
}};

constexpr char kMoveCheat[] = "m";
constexpr char kDisableCheat[] = "dis";
constexpr char kCrashCheat[] = "crash";
constexpr char kEliminateCheat[] = "elim";

constexpr std::array<std::uint8_t, 9> kExpectedChatRegisterCheat{
    0x53, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xda, 0x8b, 0xf0};
constexpr std::array<std::uint8_t, 7> kExpectedResourceMutator{
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x45, 0xfc};
constexpr std::array<std::uint8_t, 10> kExpectedAddCrewCapacity{
    0x55, 0x8b, 0xec, 0x51, 0x8a,
    0x81, 0x4e, 0x02, 0x00, 0x00};
constexpr std::array<std::uint8_t, 9> kExpectedDisableShieldGenerator{
    0x55, 0x8b, 0xec, 0x8b, 0x91, 0xe0, 0x01, 0x00, 0x00};
constexpr std::array<std::uint8_t, 7> kExpectedEntityGetTransform{
    0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
constexpr std::array<std::uint8_t, 12> kExpectedQueueCommandVector{
    0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1,
    0x8b, 0x0d, 0x88, 0xb8, 0x76, 0x00};
constexpr std::array<std::uint8_t, 11> kExpectedEliminateTeam{
    0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08,
    0x53, 0x8b, 0xd9, 0x83, 0x7c};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
A2FO_InlineHook g_show_me_the_money_hook{};
A2FO_InlineHook g_chat_hook_init_hook{};
GrantAmounts g_grant_amounts;

extern "C" void a2fo_cheats_call_thiscall_float(
    void* function, void* self, float value);
extern "C" void a2fo_cheats_call_thiscall_int(
    void* function, void* self, std::int32_t value);
extern "C" void a2fo_cheats_call_thiscall_command_vector_int(
    void* function, void* self, std::int32_t command,
    const void* vector, std::int32_t value);
extern "C" void* a2fo_cheats_call_thiscall_pointer(
    void* function, void* self);

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool read_small_text_file(const std::string& path, std::string& contents) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > kMaximumRtsConfigSize) {
        log_line("Ignored oversized RTS configuration: " + path);
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    contents = stream.str();
    return input.good() || input.eof();
}

std::string strip_c_comments(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    bool line_comment = false;
    bool block_comment = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char current = input[index];
        const char next = index + 1 < input.size() ? input[index + 1] : '\0';
        if (line_comment) {
            if (current == '\n' || current == '\r') {
                line_comment = false;
                output.push_back(current);
            } else {
                output.push_back(' ');
            }
            continue;
        }
        if (block_comment) {
            if (current == '*' && next == '/') {
                output.append("  ");
                ++index;
                block_comment = false;
            } else {
                output.push_back(
                    current == '\n' || current == '\r' ? current : ' ');
            }
            continue;
        }
        if (current == '/' && next == '/') {
            output.append("  ");
            ++index;
            line_comment = true;
        } else if (current == '/' && next == '*') {
            output.append("  ");
            ++index;
            block_comment = true;
        } else {
            output.push_back(current);
        }
    }
    return output;
}

bool identifier_character(char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '_';
}

std::string assignment_identifier(
    const std::string& statement, std::size_t equals) {
    std::size_t end = equals;
    while (end != 0 && !identifier_character(statement[end - 1])) --end;
    std::size_t begin = end;
    while (begin != 0 && identifier_character(statement[begin - 1])) --begin;
    return statement.substr(begin, end - begin);
}

bool parse_grant_literal(
    const std::string& statement, std::size_t equals, float& value) noexcept {
    const char* begin = statement.c_str() + equals + 1;
    while (*begin == ' ' || *begin == '\t' ||
           *begin == '\r' || *begin == '\n') {
        ++begin;
    }
    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(begin, &end);
    if (end == begin || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        ++end;
    }
    if (*end == 'f' || *end == 'F') {
        ++end;
        while (*end == ' ' || *end == '\t' ||
               *end == '\r' || *end == '\n') {
            ++end;
        }
    }
    if (*end != '\0' || parsed < 0.0f ||
        parsed > kMaximumConfiguredGrantAmount) {
        return false;
    }
    value = parsed;
    return true;
}

std::uint32_t apply_rts_config(
    const std::string& contents, const std::string& path) {
    const std::string source = strip_c_comments(contents);
    std::uint32_t applied = 0;
    std::size_t begin = 0;
    while (begin < source.size()) {
        const std::size_t semicolon = source.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? source.size() : semicolon;
        const std::string statement = source.substr(begin, end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string::npos) {
            const std::string identifier =
                assignment_identifier(statement, equals);
            for (const GrantField& field : kGrantFields) {
                if (identifier != field.name) continue;
                float value = 0.0f;
                if (parse_grant_literal(statement, equals, value)) {
                    g_grant_amounts.*(field.member) = value;
                    ++applied;
                } else {
                    log_line("Ignored invalid " + identifier + " in " + path);
                }
                break;
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }
    return applied;
}

std::string format_grant_amount(float value) {
    std::ostringstream output;
    output << value;
    return output.str();
}

void load_grant_amounts() {
    g_grant_amounts = GrantAmounts{};
    std::uint32_t configured_fields = 0;
    const std::uint32_t root_count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < root_count; ++index) {
        const char* root = g_api->extension_root(index);
        if (!root || !*root) continue;
        const std::string path = join_path(root, kRtsConfigFileName);
        std::string contents;
        if (!read_small_text_file(path, contents)) continue;
        const std::uint32_t applied = apply_rts_config(contents, path);
        if (applied != 0) {
            configured_fields += applied;
            log_line("Applied " + std::to_string(applied) +
                     " showmethemoney value(s) from " + path);
        }
    }
    log_line(
        "showmethemoney grants: Dilithium=" +
        format_grant_amount(g_grant_amounts.dilithium) +
        ", Tritanium=" + format_grant_amount(g_grant_amounts.tritanium) +
        ", Metal=" + format_grant_amount(g_grant_amounts.metal) +
        ", Supplies=" + format_grant_amount(g_grant_amounts.supplies) +
        ", Crew=" + format_grant_amount(g_grant_amounts.crew) +
        (configured_fields == 0 ? " (defaults)" : " (RTS_CFG.h)"));
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    return start >= base && start - base <= info.RegionSize &&
        size <= info.RegionSize - (start - base);
}

bool writable_range(const void* address, std::size_t size) noexcept {
    if (!readable_range(address, size)) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    const DWORD protection = info.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

void* read_pointer(const void* address) noexcept {
    if (!readable_range(address, sizeof(void*))) return nullptr;
    void* value = nullptr;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <typename Value>
bool read_value(const void* address, Value& value) noexcept {
    if (!readable_range(address, sizeof(value))) return false;
    std::memcpy(&value, address, sizeof(value));
    return true;
}

template <typename Function>
Function function_pointer(void* address) noexcept {
    static_assert(sizeof(Function) == sizeof(address),
                  "32-bit function and data pointers must match");
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

void* linked_dispatch_function(std::uintptr_t rva) noexcept {
    void* link = read_pointer(at(g_fleet_ops, rva));
    return read_pointer(link);
}

using CheatHandler = bool (__cdecl*)();
using ChatRegisterCheatFunction =
    void (__attribute__((regparm(3))) *)(
        const char* command, std::uint32_t multiplayer_allowed,
        CheatHandler handler);
using CurrentPlayerFunction = void* (__cdecl*)();
using TeamLookupFunction = void* (__cdecl*)(void* player);
using EntityGetFunction = void* (__cdecl*)(std::int32_t handle);
using DynamicCastFunction = void* (__cdecl*)(
    void* object, std::int32_t vf_delta, void* source_type,
    void* target_type, std::int32_t is_reference);
using AddResourceFunction =
    void (__attribute__((regparm(1), stdcall)) *)(void* team, float amount);
using ChatHookInitFunction = void (__cdecl*)();

struct CheatRegistration {
    const char* command;
    std::uint8_t multiplayer_allowed;
    std::uint8_t reserved[3];
    CheatHandler handler;
};

static_assert(sizeof(CheatRegistration) == 12,
              "Fleet Operations cheat registration layout changed");

struct Vector3 {
    float x;
    float y;
    float z;
};

void* dynamic_cast_object(
    void* object, std::uintptr_t source_type_rva,
    std::uintptr_t target_type_rva) noexcept {
    if (!object) return nullptr;
    const auto dynamic_cast_function = function_pointer<DynamicCastFunction>(
        linked_dispatch_function(kDynamicCastDispatchLinkRva));
    if (!dynamic_cast_function) return nullptr;
    return dynamic_cast_function(
        object, 0, at(g_armada, source_type_rva),
        at(g_armada, target_type_rva), 0);
}

template <typename Visitor>
bool for_each_selected_craft(
    Visitor&& visitor, bool stop_after_action = false) noexcept {
    void* selection_link = read_pointer(
        at(g_fleet_ops, kSelectionDispatchLinkRva));
    void* selection = read_pointer(selection_link);
    if (!selection) return false;

    std::int32_t count = 0;
    if (!read_value(
            static_cast<std::uint8_t*>(selection) + kSelectionCountOffset,
            count) ||
        count <= 0 || count > kMaximumSelectionCount) {
        return false;
    }

    const auto entity_get = function_pointer<EntityGetFunction>(
        linked_dispatch_function(kEntityGetDispatchLinkRva));
    if (!entity_get) return false;

    bool acted = false;
    for (std::int32_t index = count - 1; index >= 0; --index) {
        std::int32_t handle = 0;
        const auto* handle_address =
            static_cast<const std::uint8_t*>(selection) +
            kSelectionHandlesOffset +
            static_cast<std::size_t>(index) * sizeof(handle);
        if (!read_value(handle_address, handle)) continue;

        void* object = entity_get(handle);
        void* craft = dynamic_cast_object(
            object, kGameObjectTypeDescriptorRva,
            kCraftTypeDescriptorRva);
        if (craft && visitor(craft, handle)) {
            acted = true;
            if (stop_after_action) break;
        }
    }
    return acted;
}

bool __cdecl move_selected_hook() noexcept {
    return for_each_selected_craft([](void* craft, std::int32_t) noexcept {
        std::array<float, 12> matrix{};
        const void* matrix_address = a2fo_cheats_call_thiscall_pointer(
            at(g_armada, kEntityGetTransformRva), craft);
        if (!readable_range(matrix_address,
                            matrix.size() * sizeof(matrix[0]))) {
            return false;
        }
        std::memcpy(matrix.data(), matrix_address,
                    matrix.size() * sizeof(matrix[0]));

        // This is the original Fleet Ops operation: transform local
        // (0, 0, 200) by the craft matrix and queue AiCommand 4 there.
        const Vector3 destination{
            matrix[9] + kMoveDistance * matrix[6],
            matrix[10] + kMoveDistance * matrix[7],
            matrix[11] + kMoveDistance * matrix[8],
        };
        a2fo_cheats_call_thiscall_command_vector_int(
            at(g_armada, kQueueCommandVectorRva), craft,
            kMoveCommand, &destination, 0);
        return true;
    });
}

bool __cdecl disable_selected_shields_hook() noexcept {
    return for_each_selected_craft([](void* craft, std::int32_t) noexcept {
        a2fo_cheats_call_thiscall_float(
            at(g_armada, kDisableShieldGeneratorRva), craft,
            kShieldDisableDuration);
        return true;
    });
}

bool __cdecl eliminate_selected_team_hook() noexcept {
    bool eliminated = false;
    return for_each_selected_craft(
        [&eliminated](void* craft, std::int32_t) noexcept {
            if (eliminated) return false;

            std::int32_t team = -1;
            if (!read_value(
                    static_cast<const std::uint8_t*>(craft) +
                        kGameObjectTeamOffset,
                    team) ||
                team < 0 || team >= kMaximumTeamCount) {
                return false;
            }

            void* game_type_link = read_pointer(
                at(g_fleet_ops, kGameTypeDispatchLinkRva));
            void* game_type = read_pointer(game_type_link);
            void* general_game_type = dynamic_cast_object(
                game_type, kGameTypeTypeDescriptorRva,
                kGameTypeGeneralTypeDescriptorRva);
            if (!general_game_type) return false;

            a2fo_cheats_call_thiscall_int(
                at(g_armada, kEliminateTeamRva), general_game_type, team);
            eliminated = true;
            return true;
        }, true);
}

bool __cdecl crash_hook() noexcept {
    log_line("crash requested; terminating Fleet Operations intentionally");
    return TerminateProcess(
               GetCurrentProcess(), EXCEPTION_ACCESS_VIOLATION) != FALSE;
}

bool __cdecl show_me_the_money_hook() noexcept {
    const auto current_player = function_pointer<CurrentPlayerFunction>(
        linked_dispatch_function(kCurrentPlayerDispatchLinkRva));
    const auto team_lookup = function_pointer<TeamLookupFunction>(
        linked_dispatch_function(kTeamLookupDispatchLinkRva));
    if (!current_player || !team_lookup) return false;

    void* player = current_player();
    void* team = player ? team_lookup(player) : nullptr;
    if (!team) return false;

    const auto add_resource = [team](
        std::uintptr_t rva, float amount) noexcept {
        const auto function = function_pointer<AddResourceFunction>(
            at(g_fleet_ops, rva));
        function(team, amount);
    };
    add_resource(kAddDilithiumRva, g_grant_amounts.dilithium);
    add_resource(kAddTritaniumRva, g_grant_amounts.tritanium);
    add_resource(kAddMetalRva, g_grant_amounts.metal);
    add_resource(kAddSuppliesRva, g_grant_amounts.supplies);
    // Team::AddCrew clamps the available pool to Team::crewCapacity.
    a2fo_cheats_call_thiscall_float(
        at(g_armada, kAddCrewCapacityRva), team, g_grant_amounts.crew);
    add_resource(kAddCrewRva, g_grant_amounts.crew);
    return true;
}

CheatRegistration* cheat_registry(std::uint32_t& count) noexcept {
    count = 0;
    auto* registrations = static_cast<CheatRegistration*>(
        read_pointer(at(g_fleet_ops, kCheatRegistryRva)));
    if (!registrations) return nullptr;

    const auto registrations_address =
        reinterpret_cast<std::uintptr_t>(registrations);
    if (registrations_address < sizeof(count) ||
        !read_value(reinterpret_cast<const void*>(
                        registrations_address - sizeof(count)),
                    count) ||
        count == 0 || count > 64 ||
        !readable_range(registrations,
                        static_cast<std::size_t>(count) *
                            sizeof(*registrations))) {
        count = 0;
        return nullptr;
    }
    return registrations;
}

CheatRegistration* find_registered_cheat(const char* command) noexcept {
    if (!command) return nullptr;
    const std::size_t command_length = std::strlen(command);
    std::uint32_t count = 0;
    CheatRegistration* registrations = cheat_registry(count);
    for (std::uint32_t index = 0; registrations && index < count; ++index) {
        const char* registered_command = registrations[index].command;
        if (readable_range(registered_command, command_length + 1) &&
            std::memcmp(registered_command, command,
                        command_length + 1) == 0) {
            return registrations + index;
        }
    }
    return nullptr;
}

bool ensure_cheat_registered(
    const char* command, CheatHandler handler) noexcept {
    CheatRegistration* registration = find_registered_cheat(command);
    if (registration) {
        if (!writable_range(registration, sizeof(*registration))) {
            return false;
        }
        registration->multiplayer_allowed = 0;
        registration->handler = handler;
        return true;
    }

    const auto register_cheat = function_pointer<ChatRegisterCheatFunction>(
        at(g_fleet_ops, kChatRegisterCheatRva));
    if (!register_cheat) return false;
    register_cheat(command, 0, handler);
    registration = find_registered_cheat(command);
    return registration && registration->handler == handler;
}

bool register_debug_cheats() noexcept {
    const bool move_registered =
        ensure_cheat_registered(kMoveCheat, &move_selected_hook);
    const bool disable_registered = ensure_cheat_registered(
        kDisableCheat, &disable_selected_shields_hook);
    const bool crash_registered =
        ensure_cheat_registered(kCrashCheat, &crash_hook);
    const bool eliminate_registered = ensure_cheat_registered(
        kEliminateCheat, &eliminate_selected_team_hook);
    return move_registered && disable_registered && crash_registered &&
        eliminate_registered;
}

void __cdecl chat_hook_init_hook() noexcept {
    const auto original = function_pointer<ChatHookInitFunction>(
        g_chat_hook_init_hook.gateway);
    if (original) original();
    if (!register_debug_cheats()) {
        log_line("Chat initialization completed, but one or more restored "
                 "cheat commands could not be registered");
    }
}

bool resource_mutator_signatures_match() noexcept {
    constexpr std::array<std::uintptr_t, 5> mutators{
        kAddDilithiumRva,
        kAddTritaniumRva,
        kAddMetalRva,
        kAddSuppliesRva,
        kAddCrewRva,
    };
    for (const std::uintptr_t rva : mutators) {
        const void* address = at(g_fleet_ops, rva);
        if (!readable_range(address, kExpectedResourceMutator.size()) ||
            std::memcmp(address, kExpectedResourceMutator.data(),
                        kExpectedResourceMutator.size()) != 0) {
            return false;
        }
    }
    return true;
}

template <std::size_t Size>
bool signature_matches(
    HMODULE module, std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& signature) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, signature.size()) &&
        std::memcmp(address, signature.data(), signature.size()) == 0;
}

std::array<std::uint8_t, 6> show_me_the_money_signature() noexcept {
    std::array<std::uint8_t, 6> signature{0x53, 0xa1, 0, 0, 0, 0};
    static_assert(sizeof(void*) == sizeof(std::uint32_t),
                  "Fleet Operations hooks require a 32-bit process");
    const std::uint32_t dispatch_link = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            at(g_fleet_ops, kCurrentPlayerDispatchLinkRva)));
    std::memcpy(signature.data() + 2, &dispatch_link,
                sizeof(dispatch_link));
    return signature;
}

std::array<std::uint8_t, 5> chat_hook_init_signature() noexcept {
    std::array<std::uint8_t, 5> signature{0xba, 0, 0, 0, 0};
    const std::uint32_t chat_callback = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(at(g_fleet_ops, 0x1fc764)));
    std::memcpy(signature.data() + 1, &chat_callback,
                sizeof(chat_callback));
    return signature;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module ||
        !api->install_inline_hook || !api->extension_root_count ||
        !api->extension_root) {
        return false;
    }

    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !g_fleet_ops) {
        log_line("Armada or Fleet Operations image unavailable; "
                 "cheat extension disabled");
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    load_grant_amounts();
    const auto money_signature = show_me_the_money_signature();
    const auto chat_init_signature = chat_hook_init_signature();
    if (!signature_matches(g_fleet_ops, kChatRegisterCheatRva,
                           kExpectedChatRegisterCheat) ||
        !signature_matches(g_armada, kAddCrewCapacityRva,
                           kExpectedAddCrewCapacity) ||
        !signature_matches(g_armada, kDisableShieldGeneratorRva,
                           kExpectedDisableShieldGenerator) ||
        !signature_matches(g_armada, kEntityGetTransformRva,
                           kExpectedEntityGetTransform) ||
        !signature_matches(g_armada, kQueueCommandVectorRva,
                           kExpectedQueueCommandVector) ||
        !signature_matches(g_armada, kEliminateTeamRva,
                           kExpectedEliminateTeam) ||
        !resource_mutator_signatures_match() ||
        !signature_matches(g_fleet_ops, kShowMeTheMoneyRva,
                           money_signature) ||
        !signature_matches(g_fleet_ops, kChatHookInitRva,
                           chat_init_signature)) {
        log_line("Cheat dependency signature mismatch; extension disabled");
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }

    if (!api->install_inline_hook(
            at(g_fleet_ops, kShowMeTheMoneyRva),
            reinterpret_cast<void*>(&show_me_the_money_hook),
            money_signature.size(), money_signature.data(),
            &g_show_me_the_money_hook)) {
        log_line("showmethemoney handler installation failed; "
                 "cheat extension disabled");
        return false;
    }
    if (!api->install_inline_hook(
            at(g_fleet_ops, kChatHookInitRva),
            reinterpret_cast<void*>(&chat_hook_init_hook),
            chat_init_signature.size(), chat_init_signature.data(),
            &g_chat_hook_init_hook)) {
        // The first hook is already process-lifetime state. Keep this DLL
        // loaded so that handler never points into an unloaded image.
        log_line("Chat initialization hook installation failed; restored "
                 "debug cheats disabled, showmethemoney remains active");
        return true;
    }

    std::uint32_t current_cheat_count = 0;
    if (cheat_registry(current_cheat_count) &&
        !register_debug_cheats()) {
        log_line("Existing chat registry found, but one or more restored "
                 "cheat commands could not be registered");
    }

    log_line(std::string(kRevision) +
             " initialized: restored m, dis, crash, and elim; "
             "showmethemoney uses RTS_CFG.h grant values");
    return true;
}

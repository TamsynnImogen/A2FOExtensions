/*
 * File: modules/A2FOCheats/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Optional, signature-checked Fleet Operations cheat extensions.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a6_ffi.h"

#include <windows.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <exception>
#include <sstream>
#include <string>

namespace {

constexpr const char* kModuleName = "A2FOCheats";
constexpr std::streamoff kMaximumRtsConfigSize = 2 * 1024 * 1024;
constexpr float kDefaultGrantAmount = 10000.0f;

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
constexpr std::size_t kTeamDilithiumOffset = 0x0028;
constexpr std::int32_t kMaximumSelectionCount = 512;
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


constexpr char kMoveCheat[] = "m";
constexpr char kDisableCheat[] = "dis";
constexpr char kCrashCheat[] = "crash";
constexpr char kEliminateCheat[] = "elim";
constexpr char kMoneyCheat[] = "showmethemoney";

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

struct CheatContext {
    std::string text;
    std::exception_ptr error;
    void* selection = nullptr;
    void* entity_get = nullptr;
    void* general_game_type = nullptr;
    void* current_player = nullptr;
    void* team_lookup = nullptr;
};
std::uintptr_t cheat_native_step(CheatContext&,std::uint32_t,A2foRsCheatValue*);
A2foRsCheatBytes cheat_bytes(const std::string& text) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(text.data()),text.size()};
}
std::string cheat_text(A2foRsCheatBytes value) {
    return value.data ? std::string(reinterpret_cast<const char*>(value.data),value.size)
                      : std::string{};
}
std::string format_grant_amount(float value) {
    std::ostringstream output;
    output << value;
    return output.str();
}
std::uintptr_t cheat_step(void* opaque,std::uint32_t operation,A2foRsCheatValue* value) noexcept {
    auto& context=*static_cast<CheatContext*>(opaque);
    if(context.error)return 0;
    try {
        switch(operation){
        case 0:g_grant_amounts=GrantAmounts{};return g_api->extension_root_count();
        case 1: {
            const char* root=g_api->extension_root(static_cast<std::uint32_t>(value->word));
            if(!root)return 0;
            value->first={reinterpret_cast<const std::uint8_t*>(root),std::strlen(root)};
            return 1;
        }
        case 2: {
            context.text.clear();
            if(!read_small_text_file(cheat_text(value->first),context.text))return 0;
            value->second=cheat_bytes(context.text);return 1;
        }
        case 3: {
            const std::string input=cheat_text(value->first);
            errno=0;char* end=nullptr;
            value->number=std::strtof(input.c_str(),&end);
            value->word=static_cast<std::uintptr_t>(end-input.c_str());
            value->other=errno==ERANGE;
            return 1;
        }
        case 4:log_line("Ignored invalid "+cheat_text(value->first)+" in "+cheat_text(value->second));return 1;
        case 5:log_line("Applied "+std::to_string(value->word)+
            " showmethemoney value(s) from "+cheat_text(value->first));return 1;
        case 6: {
            g_grant_amounts.dilithium=value->vector[0];
            g_grant_amounts.tritanium=value->vector[1];
            g_grant_amounts.metal=value->vector[2];
            g_grant_amounts.supplies=value->vector[3];
            g_grant_amounts.crew=value->vector[4];
            log_line("showmethemoney grants: Dilithium="+
                format_grant_amount(g_grant_amounts.dilithium)+
                ", Tritanium="+format_grant_amount(g_grant_amounts.tritanium)+
                ", Metal="+format_grant_amount(g_grant_amounts.metal)+
                ", Supplies="+format_grant_amount(g_grant_amounts.supplies)+
                ", Crew="+format_grant_amount(g_grant_amounts.crew)+
                (value->word==0?" (defaults)":" (RTS_CFG.h)"));return 1;
        }
        default:return cheat_native_step(context,operation,value);
        }
    }catch(...){context.error=std::current_exception();return 0;}
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

bool cheat_command(std::uint32_t kind) noexcept {
    try {
        CheatContext context{};
        const bool result=a2fo_rs_runtime_cheats_command(kind,cheat_step,&context)!=0;
        if(context.error)std::rethrow_exception(context.error);
        return result;
    }catch(...){return false;}
}
bool __cdecl move_selected_hook() noexcept {return cheat_command(0);}
bool __cdecl disable_selected_shields_hook() noexcept {return cheat_command(1);}
bool __cdecl eliminate_selected_team_hook() noexcept {return cheat_command(2);}
bool __cdecl crash_hook() noexcept {return cheat_command(3);}
bool __cdecl show_me_the_money_hook() noexcept {return cheat_command(4);}

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

void __cdecl chat_hook_init_hook() noexcept {
    try {
        CheatContext context{};
        a2fo_rs_runtime_cheats_chat_init(cheat_step,&context);
        if(context.error)std::rethrow_exception(context.error);
    }catch(...){log_line("Chat initialization completed, but one or more restored cheat commands could not be registered");}
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

std::array<std::uint8_t, 8> repurposed_money_signature() noexcept {
    // This entry now belongs to ShipSystemIcon rendering. Recognize it, but
    // never detour it or copy its relative jump into an inline gateway.
    std::array<std::uint8_t, 8> signature{0x53, 0xa1, 0, 0, 0, 0, 0xeb, 0x0c};
    const auto dispatch_link = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(at(g_fleet_ops, 0x212c50)));
    std::memcpy(signature.data() + 2, &dispatch_link, sizeof(dispatch_link));
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

std::uintptr_t cheat_native_step(CheatContext& context,std::uint32_t operation,A2foRsCheatValue* value) {
    const auto craft=reinterpret_cast<void*>(value->word);
    switch(operation){
    case 20: {
        void* link=read_pointer(at(g_fleet_ops,kSelectionDispatchLinkRva));
        context.selection=read_pointer(link);
        if(!context.selection)return 0;
        std::int32_t count=0;
        if(!read_value(static_cast<std::uint8_t*>(context.selection)+kSelectionCountOffset,count)||
           count<=0||count>kMaximumSelectionCount)return 0;
        context.entity_get=linked_dispatch_function(kEntityGetDispatchLinkRva);
        return function_pointer<EntityGetFunction>(context.entity_get)?static_cast<std::uintptr_t>(count):0;
    }
    case 21: {
        std::int32_t handle=0;
        const auto* address=static_cast<const std::uint8_t*>(context.selection)+
            kSelectionHandlesOffset+value->word*sizeof(handle);
        if(!read_value(address,handle))return 0;
        auto get=function_pointer<EntityGetFunction>(context.entity_get);
        return reinterpret_cast<std::uintptr_t>(dynamic_cast_object(get(handle),
            kGameObjectTypeDescriptorRva,kCraftTypeDescriptorRva));
    }
    case 22: {
        const void* matrix=a2fo_cheats_call_thiscall_pointer(at(g_armada,kEntityGetTransformRva),craft);
        if(!readable_range(matrix,sizeof(value->vector)))return 0;
        std::memcpy(value->vector,matrix,sizeof(value->vector));return 1;
    }
    case 23: {
        const Vector3 destination{
            value->vector[9]+kMoveDistance*value->vector[6],
            value->vector[10]+kMoveDistance*value->vector[7],
            value->vector[11]+kMoveDistance*value->vector[8]};
        value->vector[0]=destination.x;value->vector[1]=destination.y;
        value->vector[2]=destination.z;return 1;
    }
    case 24: {
        const Vector3 destination{value->vector[0],value->vector[1],value->vector[2]};
        a2fo_cheats_call_thiscall_command_vector_int(at(g_armada,kQueueCommandVectorRva),
            craft,kMoveCommand,&destination,0);return 1;
    }
    case 25:a2fo_cheats_call_thiscall_float(at(g_armada,kDisableShieldGeneratorRva),
            craft,kShieldDisableDuration);return 1;
    case 26: {
        std::int32_t team=-1;
        if(!read_value(static_cast<const std::uint8_t*>(craft)+kGameObjectTeamOffset,team))return static_cast<std::uint32_t>(-1);
        return static_cast<std::uint32_t>(team);
    }
    case 27: {
        void* link=read_pointer(at(g_fleet_ops,kGameTypeDispatchLinkRva));
        context.general_game_type=dynamic_cast_object(read_pointer(link),
            kGameTypeTypeDescriptorRva,kGameTypeGeneralTypeDescriptorRva);
        return context.general_game_type!=nullptr;
    }
    case 28:a2fo_cheats_call_thiscall_int(at(g_armada,kEliminateTeamRva),
            context.general_game_type,static_cast<std::int32_t>(value->other));return 1;
    case 29:log_line("crash requested; terminating Fleet Operations intentionally");return 1;
    case 30:return TerminateProcess(GetCurrentProcess(),EXCEPTION_ACCESS_VIOLATION)!=FALSE;
    case 31:context.current_player=linked_dispatch_function(kCurrentPlayerDispatchLinkRva);
        context.team_lookup=linked_dispatch_function(kTeamLookupDispatchLinkRva);
        return function_pointer<CurrentPlayerFunction>(context.current_player)&&
            function_pointer<TeamLookupFunction>(context.team_lookup);
    case 32: {
        const auto current=function_pointer<CurrentPlayerFunction>(context.current_player);
        const auto lookup=function_pointer<TeamLookupFunction>(context.team_lookup);
        void* player=current();return reinterpret_cast<std::uintptr_t>(player?lookup(player):nullptr);
    }
    case 33:case 40:return read_value(static_cast<const std::uint8_t*>(craft)+kTeamDilithiumOffset,value->number);
    case 34:case 35:case 36:case 37:case 39: {
        const std::uintptr_t rvas[]{kAddDilithiumRva,kAddTritaniumRva,kAddMetalRva,kAddSuppliesRva,kAddCrewRva};
        const float amounts[]{g_grant_amounts.dilithium,g_grant_amounts.tritanium,
            g_grant_amounts.metal,g_grant_amounts.supplies,g_grant_amounts.crew};
        const std::size_t index=operation==39?4:operation-34;
        function_pointer<AddResourceFunction>(at(g_fleet_ops,rvas[index]))(craft,amounts[index]);return 1;
    }
    case 38:a2fo_cheats_call_thiscall_float(at(g_armada,kAddCrewCapacityRva),
            craft,g_grant_amounts.crew);return 1;
    case 41:if(value->other){
            std::ostringstream message;
            message<<"showmethemoney invoked: Team="<<craft
                <<", Dilithium "<<value->number<<" -> "<<value->second_number
                <<" (requested +"<<g_grant_amounts.dilithium<<')';
            log_line(message.str());
        }else{log_line("showmethemoney invoked, but its Team balance could not be inspected");}
        return 1;
    case 42:log_line("showmethemoney invocation could not resolve the player/team dispatch functions");return 1;
    case 43:log_line("showmethemoney invocation did not resolve a current Team");return 1;
    case 50: {std::uint32_t count=0;cheat_registry(count);return count;}
    case 51: {
        std::uint32_t count=0;auto* registrations=cheat_registry(count);
        if(!registrations||value->word>=count)return 0;
        const char* command=registrations[value->word].command;
        if(!readable_range(command,value->first.size+1))return 0;
        value->second={reinterpret_cast<const std::uint8_t*>(command),value->first.size+1};
        return 1;
    }
    case 52:case 54: {
        std::uint32_t count=0;auto* registrations=cheat_registry(count);
        if(!registrations||value->word>=count||value->other>=5)return 0;
        const CheatHandler handlers[]{show_me_the_money_hook,move_selected_hook,
            disable_selected_shields_hook,crash_hook,eliminate_selected_team_hook};
        auto& registration=registrations[value->word];
        if(operation==54)return registration.handler==handlers[value->other];
        if(!writable_range(&registration,sizeof(registration)))return 0;
        registration.multiplayer_allowed=0;registration.handler=handlers[value->other];return 1;
    }
    case 53: {
        if(value->word>=5)return 0;
        const CheatHandler handlers[]{show_me_the_money_hook,move_selected_hook,
            disable_selected_shields_hook,crash_hook,eliminate_selected_team_hook};
        const auto register_cheat=function_pointer<ChatRegisterCheatFunction>(
            at(g_fleet_ops,kChatRegisterCheatRva));
        if(!register_cheat)return 0;
        const char* commands[]{kMoneyCheat,kMoveCheat,kDisableCheat,kCrashCheat,kEliminateCheat};
        register_cheat(commands[value->word],0,handlers[value->word]);return 1;
    }
    case 60: {
        const auto original=function_pointer<ChatHookInitFunction>(g_chat_hook_init_hook.gateway);
        if(original)original();
        return 1;
    }
    case 61:log_line("Chat initialization completed, but one or more restored cheat commands could not be registered");return 1;
    case 70:g_armada=static_cast<HMODULE>(g_api->armada_module());
        g_fleet_ops=static_cast<HMODULE>(g_api->fleetops_module());
        return g_armada&&g_fleet_ops;
    case 71:log_line("Armada or Fleet Operations image unavailable; cheat extension disabled");
        g_api=nullptr;g_armada=nullptr;g_fleet_ops=nullptr;return 1;
    case 72:return signature_matches(g_fleet_ops,kShowMeTheMoneyRva,repurposed_money_signature());
    case 73:switch(value->word){
        case 0:return signature_matches(g_fleet_ops,kChatRegisterCheatRva,kExpectedChatRegisterCheat);
        case 1:return signature_matches(g_armada,kAddCrewCapacityRva,kExpectedAddCrewCapacity);
        case 2:return signature_matches(g_armada,kDisableShieldGeneratorRva,kExpectedDisableShieldGenerator);
        case 3:return signature_matches(g_armada,kEntityGetTransformRva,kExpectedEntityGetTransform);
        case 4:return signature_matches(g_armada,kQueueCommandVectorRva,kExpectedQueueCommandVector);
        case 5:return signature_matches(g_armada,kEliminateTeamRva,kExpectedEliminateTeam);
        case 6:case 7:case 8:case 9:case 10: {
            const std::uintptr_t rvas[]{kAddDilithiumRva,kAddTritaniumRva,kAddMetalRva,kAddSuppliesRva,kAddCrewRva};
            const void* address=at(g_fleet_ops,rvas[value->word-6]);
            return readable_range(address,kExpectedResourceMutator.size())&&
                std::memcmp(address,kExpectedResourceMutator.data(),kExpectedResourceMutator.size())==0;
        }
        case 11:return signature_matches(g_fleet_ops,kShowMeTheMoneyRva,show_me_the_money_signature());
        case 12:return signature_matches(g_fleet_ops,kChatHookInitRva,chat_hook_init_signature());
        default:return 0;
    }
    case 74:log_line("Cheat dependency signature mismatch; extension disabled");
        g_api=nullptr;g_armada=nullptr;g_fleet_ops=nullptr;return 1;
    case 75: {const auto signature=show_me_the_money_signature();
        return g_api->install_inline_hook(at(g_fleet_ops,kShowMeTheMoneyRva),
            reinterpret_cast<void*>(&show_me_the_money_hook),signature.size(),signature.data(),
            &g_show_me_the_money_hook);}
    case 76:log_line("showmethemoney handler installation failed; cheat extension disabled");return 1;
    case 77:log_line("September Fleet Ops money entry is used by icon rendering; registering showmethemoney directly without detouring it");return 1;
    case 78: {const auto signature=chat_hook_init_signature();
        return g_api->install_inline_hook(at(g_fleet_ops,kChatHookInitRva),
            reinterpret_cast<void*>(&chat_hook_init_hook),signature.size(),signature.data(),
            &g_chat_hook_init_hook);}
    case 79:log_line(value->word?
        "Chat initialization hook installation failed; cheat extension disabled":
        "Chat initialization hook installation failed; restored debug cheats disabled, showmethemoney remains active");return 1;
    case 81: {std::uint32_t count=0;return cheat_registry(count)!=nullptr;}
    case 82:log_line("Existing chat registry found, but one or more restored cheat commands could not be registered");return 1;
    case 83:log_line("Cheat extensions initialized: restored m, dis, crash, and elim; showmethemoney uses RTS_CFG.h grant values");return 1;
    default:return 0;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size>=A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version==A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->fleetops_module && api->install_inline_hook &&
        api->extension_root_count && api->extension_root;
    if(valid)g_api=api;
    CheatContext context{};
    const bool result=a2fo_rs_runtime_cheats_init(valid,cheat_step,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

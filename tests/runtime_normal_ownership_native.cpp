// Headless native ownership fixture. Compile with NORMAL_SOURCE set to the
// live module source after the proposed adapter is integrated.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include NORMAL_SOURCE

extern "C" std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_thiscall_0(
    void*, void*) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_normal_weapon_tech_call_delphi_2(
    void*, std::uintptr_t, std::uintptr_t) { std::abort(); }

namespace {
std::vector<std::string> calls;
std::uint8_t* armada = nullptr;
std::uint8_t* fleet = nullptr;
bool reject_registration = false;
bool throw_module_lookup = false;
A2FO_WeaponTriggerHandler registered_handler = nullptr;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void A2FO_CALL log_message(const char* module, const char* message) {
    require(std::strcmp(module, "A2FONormalWeaponTech") == 0, "log owner");
    calls.emplace_back(std::string("log:") + message);
}
void* A2FO_CALL armada_module() {
    calls.emplace_back("armada");
    if (throw_module_lookup) throw std::range_error("synthetic module lookup");
    return armada;
}
void* A2FO_CALL fleetops_module() {
    calls.emplace_back("fleet");
    return fleet;
}
bool A2FO_CALL register_masked(const char* owner, std::uint32_t mask,
                               A2FO_WeaponTriggerHandler handler, void* data) {
    require(std::strcmp(owner, "A2FONormalWeaponTech") == 0, "registration owner");
    require(mask == A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK, "precheck mask");
    require(data == nullptr, "registration context");
    calls.emplace_back("register-masked");
    registered_handler = handler;
    return !reject_registration;
}
bool A2FO_CALL register_legacy(const char* owner,
                               A2FO_WeaponTriggerHandler handler, void* data) {
    require(std::strcmp(owner, "A2FONormalWeaponTech") == 0, "legacy owner");
    require(data == nullptr, "legacy context");
    calls.emplace_back("register-legacy");
    registered_handler = handler;
    return !reject_registration;
}
void seed_images() {
    armada = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x271100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x212000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(armada && fleet, "VirtualAlloc synthetic images");
    const std::uint8_t owner[]{0x8b, 0x49, 0x18, 0x51, 0xe8};
    const std::uint8_t tree[]{0x53, 0x51, 0x89, 0x14, 0x24, 0x33, 0xd2, 0x8b,
                              0x40, 0x0c, 0x8b, 0x0c, 0x24, 0x8b, 0x5c, 0x88};
    std::memcpy(armada + 0x271050, owner, sizeof(owner));
    std::memcpy(fleet + 0x120680, tree, sizeof(tree));
}
void clear_calls() { calls.clear(); registered_handler = nullptr; }
void expect_calls(std::initializer_list<const char*> expected) {
    require(calls.size() == expected.size(), "callback count");
    std::size_t index = 0;
    for (const char* item : expected) {
        require(calls[index] == item, "callback order or text");
        ++index;
    }
}
void run() {
    seed_images();
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &log_message;
    api.armada_module = &armada_module;
    api.fleetops_module = &fleetops_module;
    api.capabilities = A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.register_weapon_trigger_handler = &register_legacy;
    api.register_weapon_trigger_handler_masked = &register_masked;

    require(A2FO_ModuleInit(&api), "successful init");
    require(g_runtime_ready && registered_handler, "ready and registered");
    expect_calls({"armada", "fleet", "register-masked",
                  "log:Normal-weapon technology-tree filter initialized; unlisted weapons default to 0 (available)"});
    A2FO_ModuleShutdown();
    require(!g_runtime_ready, "shutdown clears ready");

    clear_calls(); reject_registration = true;
    require(!A2FO_ModuleInit(&api), "registration rejected");
    expect_calls({"armada", "fleet", "register-masked",
                  "log:Shared weapon-trigger registration is unavailable"});
    reject_registration = false;

    clear_calls(); armada[0x271050] ^= 1;
    require(A2FO_ModuleInit(&api), "signature mismatch leaves handler resident");
    require(!g_runtime_ready && registered_handler, "signature disables filter");
    expect_calls({"armada", "fleet", "register-masked",
                  "log:Supported weapon or Fleet Operations technology-tree signature was not found; filter disabled"});
    armada[0x271050] ^= 1;

    clear_calls(); throw_module_lookup = true;
    bool typed_exception = false;
    try { (void)A2FO_ModuleInit(&api); }
    catch (const std::range_error& error) {
        typed_exception = std::strcmp(error.what(), "synthetic module lookup") == 0;
    }
    require(typed_exception, "typed callback exception propagated");
    expect_calls({"armada"});
    throw_module_lookup = false;

    clear_calls();
    require(A2FO_ModuleInit(&api), "valid init after exception");
    require(g_runtime_ready && registered_handler, "subsequent init not poisoned");
    expect_calls({"armada", "fleet", "register-masked",
                  "log:Normal-weapon technology-tree filter initialized; unlisted weapons default to 0 (available)"});
    A2FO_ModuleShutdown();
    require(!g_runtime_ready, "final shutdown clears ready");
    VirtualFree(armada, 0, MEM_RELEASE);
    VirtualFree(fleet, 0, MEM_RELEASE);
}
} // namespace

int main() {
    try { run(); std::puts("normal ownership native: passed"); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "normal ownership native: %s\n", error.what());
        return 1;
    }
}

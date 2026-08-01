#include "a2fo_module_api.h"

#include <windows.h>

#include <cstring>

namespace {
HMODULE fleet_ops = nullptr;
void* fake_armada = nullptr;

template <std::size_t Size>
void set_signature(std::uintptr_t rva, const std::uint8_t (&value)[Size]) {
    std::memcpy(static_cast<std::uint8_t*>(fake_armada) + rva,
                value, Size);
}

void prepare_armada_signatures() {
    const std::uint8_t queue_class_command[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
    const std::uint8_t dequeue_class_command[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
    const std::uint8_t dtor[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t simulate[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
    const std::uint8_t load[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
    const std::uint8_t save[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
    const std::uint8_t find_by_project_id[] =
        {0x55, 0x8b, 0xec, 0xa1, 0xf8, 0x0b, 0x74, 0x00};
    set_signature(0x0d4280, queue_class_command);
    set_signature(0x0d45f0, dequeue_class_command);
    set_signature(0x0b77d0, dtor);
    set_signature(0x0b7840, simulate);
    set_signature(0x0b88d0, load);
    set_signature(0x0b8aa0, save);
    set_signature(0x0cd150, find_by_project_id);
}

void A2FO_CALL log_line(const char*, const char*) {}
void* A2FO_CALL armada_module() { return fake_armada; }
void* A2FO_CALL fleetops_module() { return fleet_ops; }
bool A2FO_CALL register_lookup(
    const char*, A2FO_FofsItemLookupHandler handler, void*) {
    return handler != nullptr;
}
bool A2FO_CALL register_alias(const char*, const char*, const char*) {
    return true;
}
bool A2FO_CALL register_cocoon(const char*, const char*) { return true; }
bool A2FO_CALL install_hook(void* target, void* replacement,
                            std::size_t length,
                            const std::uint8_t* expected,
                            A2FO_InlineHook* hook) {
    if (!target || !replacement || !expected || !hook || length == 0 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    hook->target = target;
    hook->gateway = target;
    hook->length = length;
    return true;
}
}

int main() {
    fake_armada = VirtualAlloc(nullptr, 0x00410000,
                              MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!fake_armada) return 1;
    prepare_armada_signatures();
    // The real FleetOpsHook DllMain assumes Armada has already initialized
    // fixed engine globals. Map the PE image and its sections for identity and
    // signature validation without resolving imports or executing DllMain.
    fleet_ops = LoadLibraryExA("FleetOpsHook.fixture.dll", nullptr,
                               DONT_RESOLVE_DLL_REFERENCES);
    if (!fleet_ops) return 2;
    HMODULE feature = LoadLibraryA("modules\\A2FOFeaturePack.dll");
    if (!feature) return 3;

    A2FO_ModuleInitFn init = nullptr;
    FARPROC address = GetProcAddress(feature, "A2FO_ModuleInit");
    std::memcpy(&init, &address, sizeof(init));
    if (!init) return 4;

    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &log_line;
    api.armada_module = &armada_module;
    api.fleetops_module = &fleetops_module;
    api.install_inline_hook = &install_hook;
    api.register_fofs_item_lookup_handler = &register_lookup;
    api.register_classlabel_alias = &register_alias;
    api.register_evolver_cocoon_command = &register_cocoon;
    api.api_revision = A2FO_MODULE_API_REVISION;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH;
    if (!init(&api)) return 5;

    FreeLibrary(feature);
    FreeLibrary(fleet_ops);
    VirtualFree(fake_armada, 0, MEM_RELEASE);
}

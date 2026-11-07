// Actual standalone DLL probe. Supplies only the documented ScriptInterface
// monitor slot; never invokes the captured callback or constructs native Worker.
#include "../sdk/include/a2fo_module_api.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
using Input = unsigned char (__stdcall*)();
static void* received_self;
static Input received_callback;
static unsigned registrations;
struct InterfaceProbe {
    std::uintptr_t* table;
    void __attribute__((thiscall, noinline, used)) monitor(Input) asm("_MmmRealMonitor");
};
void InterfaceProbe::monitor(Input callback) {
    received_self = this;
    received_callback = callback;
    ++registrations;
}
extern "C" void MmmRealMonitor(); // Address label only; the vtable uses thiscall.
void A2FO_CALL logger(const char*, const char*) {}
template<class T> T symbol(HMODULE dll, const char* name) {
    auto address = GetProcAddress(dll, name);
    T function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto dll = LoadLibraryA(argv[1]);
    if (!dll) return 3;
    auto start = symbol<bool (A2FO_CALL*)(const A2FO_ModuleApi*)>(dll, "A2FO_ModuleInit");
    auto stop = symbol<void (A2FO_CALL*)()>(dll, "A2FO_ModuleShutdown");
    auto extend = symbol<int (__cdecl*)(void*)>(dll, "A2FOMissions_ScriptExtendInterface");
    auto init = symbol<int (__cdecl*)(void*)>(dll, "A2FOMissions_ScriptInit");
    auto mark = symbol<void (__cdecl*)()>(dll, "A2FOMissions_MarkMissionUnloaded");
    if (!start || !stop || !extend || !init || !mark) return 4;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION; api.log = logger;
    if (!start(&api)) return 5;
    std::uintptr_t table[300]{};
    auto monitor = &MmmRealMonitor;
    static_assert(sizeof(monitor) == sizeof(table[266]));
    std::memcpy(&table[266], &monitor, sizeof(monitor));
    InterfaceProbe one{table}, two{table};
    if (init(nullptr) != 0 || extend(nullptr) != 51 || registrations != 0) return 6;
    if (extend(&one) != 51 || registrations != 0 || init(nullptr) != 0) return 7;
    if (registrations != 1 || received_self != &one || !received_callback) return 8;
    if (init(nullptr) != 0 || registrations != 2 || received_self != &one) return 9;
    mark();
    if (extend(nullptr) != 51 || extend(&two) != 51 || init(nullptr) != 0) return 10;
    if (registrations != 3 || received_self != &two || !received_callback) return 11;
    stop();
    if (!FreeLibrary(dll)) return 12;
    std::puts("actual-missions-dll:51/0:slot266:thiscall:stdcall-callback:reattach:PASS");
    return 0;
}

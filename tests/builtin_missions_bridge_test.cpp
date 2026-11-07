// Headless shim routing fixture. Build as a fake core, fake standalone module,
// and executable using TEST_CORE, TEST_STANDALONE, or neither.
#include <windows.h>

#include <cstring>

#if defined(TEST_CORE)
extern "C" __declspec(dllexport) bool __cdecl
A2FOBuiltinModule_StartupReady(const char* name) {
    return name && std::strcmp(name, "A2FOMissions") == 0 && TEST_CORE_READY;
}
extern "C" __declspec(dllexport) void __cdecl A2FOBuiltinMissions_Init() {}
extern "C" __declspec(dllexport) int __cdecl A2FOMissions_ScriptExtendInterface(void*) {
    return 61;
}
extern "C" __declspec(dllexport) int __cdecl A2FOMissions_ScriptInit(void*) {
    return 62;
}
static int unload_count = 0;
#if !defined(TEST_CORE_MISSING_CALLBACK)
extern "C" __declspec(dllexport) void __cdecl A2FOMissions_MarkMissionUnloaded() {
    ++unload_count;
}
#endif
extern "C" __declspec(dllexport) int __cdecl Missions_TestUnloadCount() {
    return unload_count;
}
#elif defined(TEST_STANDALONE)
extern "C" __declspec(dllexport) int __cdecl A2FOMissions_ScriptExtendInterface(void*) {
    return 71;
}
extern "C" __declspec(dllexport) int __cdecl A2FOMissions_ScriptInit(void*) {
    return 72;
}
static int unload_count = 0;
extern "C" __declspec(dllexport) void __cdecl A2FOMissions_MarkMissionUnloaded() {
    ++unload_count;
}
extern "C" __declspec(dllexport) int __cdecl Missions_TestUnloadCount() {
    return unload_count;
}
#elif defined(TEST_OLD_CORE)
extern "C" __declspec(dllexport) int __cdecl OldCoreMarker() { return 1; }
#else
using MissionFn = int (__cdecl*)(void*);
using CountFn = int (__cdecl*)();
template<class Function> Function imported(HMODULE module, const char* name) {
    const auto address = GetProcAddress(module, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}


int main(int argc, char** argv) {
    if (argc != 2) return 2;
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    HMODULE standalone = LoadLibraryA("A2FOMissions.dll");
    HMODULE shim = LoadLibraryA("mmm_loader.dll");
    if (!core || !standalone || !shim) return 3;

    auto extend = imported<MissionFn>(shim, "ScriptExtendInterface");
    auto init = imported<MissionFn>(shim, "ScriptInit");
    if (!extend || !init) return 4;

    const char mode = argv[1][0];
    const int expected_extend = mode == 'a' ? 61 : mode == 'o' ? 71 : 51;
    const int expected_init = mode == 'a' ? 62 : mode == 'o' ? 72 : 0;
    if (extend(nullptr) != expected_extend || init(nullptr) != expected_init) return 5;

    FreeLibrary(shim);
    auto core_count = imported<CountFn>(core, "Missions_TestUnloadCount");
    auto standalone_count = imported<CountFn>(standalone, "Missions_TestUnloadCount");
    if (mode == 'a' && (!core_count || core_count() != 1 || standalone_count() != 0)) return 6;
    if (mode == 'd' && (!core_count || core_count() != 0 || standalone_count() != 0)) return 7;
    if (mode == 'o' && (!standalone_count || standalone_count() != 1)) return 8;
    if (mode == 'p' && (!core_count || core_count() != 0 || standalone_count() != 0)) return 9;
    return 0;
}
#endif

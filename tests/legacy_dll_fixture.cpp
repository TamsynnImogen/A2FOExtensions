// Inert test DLLs only: no engine image, hooks, UI, input, networking or workers.
#include <windows.h>
#include <stdexcept>
#include "../core/initialize_boundary.hpp"
static volatile LONG calls=0;
static LONG fleet_ready=0;
extern "C" __declspec(dllexport) LONG __cdecl FixtureState() { return calls; }
extern "C" __declspec(dllexport) LONG __cdecl FixtureFleetReady() { return fleet_ready; }
#if defined(FIXTURE_CORE) && !defined(FIXTURE_NO_INIT)
extern "C" __declspec(dllexport) bool __cdecl A2FO_Initialize() {
    InterlockedIncrement(&calls);
    Sleep(5);
    if(GetEnvironmentVariableA("A2FO_FIXTURE_THROW",nullptr,0)!=0)throw std::runtime_error("native initialization failure");
    return GetEnvironmentVariableA("A2FO_FIXTURE_FAIL_INIT",nullptr,0)==0;
}
extern "C" __declspec(dllexport) bool __cdecl A2FO_InitializeNoUnwind() noexcept {
    return a2fo::initialize_without_unwind([]{return A2FO_Initialize();},[]() noexcept {});
}
#endif
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
#ifdef FIXTURE_CORE
        const auto fleet=GetModuleHandleA("FleetOpsHook.dll");
        using State=LONG(__cdecl*)();
        const auto state=fleet?reinterpret_cast<State>(GetProcAddress(fleet,"FixtureState")):nullptr;
        fleet_ready=state && state()==1;
#else
        InterlockedIncrement(&calls);
#endif
    }
    return TRUE;
}

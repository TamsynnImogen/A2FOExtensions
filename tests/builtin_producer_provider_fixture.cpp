#include <windows.h>
namespace { bool ready = false; }
extern "C" __declspec(dllexport) bool __cdecl A2FO_ProducerEventsReady() { return ready; }
extern "C" __declspec(dllexport) void __cdecl FixtureSetReady(bool value) { ready = value; }

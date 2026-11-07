#include <windows.h>

#ifndef FIXTURE_TAG
#error FIXTURE_TAG must identify this provider
#endif

namespace { int calls = 0; }

extern "C" __declspec(dllexport) int __cdecl FixtureTag() noexcept {
    return FIXTURE_TAG;
}

extern "C" __declspec(dllexport) int __cdecl FixtureCallCount() noexcept {
    return calls;
}

extern "C" __declspec(dllexport) bool __cdecl A2FO_AnimationsEvaluate(
    void*, void*, void*) noexcept {
    ++calls;
    return FIXTURE_TAG == 1;
}

#ifndef FIXTURE_OMIT_CONTROLLED
extern "C" __declspec(dllexport) bool __cdecl A2FO_AnimationsControlled(
    void*) noexcept {
    ++calls;
    return FIXTURE_TAG == 1;
}
#endif

extern "C" __declspec(dllexport) void __cdecl A2FO_AnimationsWeaponFired(
    void*) noexcept {
    ++calls;
}

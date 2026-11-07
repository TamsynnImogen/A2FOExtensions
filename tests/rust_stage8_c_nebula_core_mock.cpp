#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace {
int g_reject = 0;
}

extern "C" __declspec(dllexport) void __cdecl A2FOFixture_SetReject(int value) {
    g_reject = value;
}
extern "C" __declspec(dllexport) int __cdecl A2FO_NebulaRendererStatus() {
    std::puts("core-status");
    return 2;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaSetEmissiveBumpMultiplier(float value) {
    std::printf("core-bump-multiplier:%.2f\n", static_cast<double>(value));
    return g_reject != 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaSetBumpLightBias(float value) {
    std::printf("core-light-bias:%.2f\n", static_cast<double>(value));
    return g_reject != 2;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaSetEmissiveDiffuseRestore(float value) {
    std::printf("core-diffuse-restore:%.2f\n", static_cast<double>(value));
    return g_reject != 3;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaSetFastNonBumpEnabled(int value) {
    std::printf("core-fast:%d\n", value);
    return g_reject != 4;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaRegisterFactionTextureSuffix(const char* suffix) {
    std::printf("core-faction:%s\n", suffix);
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaRegisterEmissiveClass(void*, const char* const*, std::uint32_t) {
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaRegisterEmissiveMaterials(void*, const char* const*,
                                      const char* const*, std::uint32_t,
                                      std::uint32_t) {
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_NebulaRegisterSpecularMaterials(void*, const char* const*,
                                      const char* const*, std::uint32_t) {
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_DecalRegisterClass(void*, float, const void*, std::uint32_t) {
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl
A2FO_LogoDecalRegisterClass(void*, const void*, std::uint32_t) {
    return 1;
}

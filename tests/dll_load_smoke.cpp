#include <windows.h>

int main() {
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    if (!core || !GetProcAddress(core, "A2FO_Initialize") ||
        !GetProcAddress(core, "A2FO_NebulaRendererStatus") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveClass") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveMaterials") ||
        !GetProcAddress(core, "A2FO_DecalRegisterClass") ||
        !GetProcAddress(core, "A2FO_LogoDecalRegisterClass") ||
        !GetProcAddress(core, "A2FO_NebulaBeginCraftRender") ||
        !GetProcAddress(core, "A2FO_NebulaEndCraftRender")) return 1;

    HMODULE nebula = LoadLibraryA("modules\\A2FONebulaRenderer.dll");
    if (!nebula || !GetProcAddress(nebula, "A2FO_ModuleInit") ||
        !GetProcAddress(nebula, "A2FO_ModuleShutdown") ||
        !GetProcAddress(nebula, "A2FONebulaRenderer_RegisterClass")) {
        return 2;
    }

    HMODULE animated_hardpoints = LoadLibraryA(
        "modules\\A2FOAnimatedHardpoints.dll");
    if (!animated_hardpoints ||
        !GetProcAddress(animated_hardpoints, "A2FO_ModuleInit") ||
        !GetProcAddress(animated_hardpoints, "A2FO_ModuleShutdown")) {
        return 6;
    }

    HMODULE point_defense = LoadLibraryA(
        "modules\\A2FOPointDefenseCycles.dll");
    if (!point_defense ||
        !GetProcAddress(point_defense, "A2FO_ModuleInit") ||
        !GetProcAddress(point_defense, "A2FO_ModuleShutdown")) {
        return 3;
    }

    HMODULE swarm = LoadLibraryA("modules\\A2FOSwarmSystem.dll");
    if (!swarm || !GetProcAddress(swarm, "A2FO_ModuleInit") ||
        !GetProcAddress(swarm, "A2FO_ModuleShutdown")) {
        return 4;
    }

    HMODULE texture_variants = LoadLibraryA(
        "modules\\A2FOTextureVariants.dll");
    if (!texture_variants ||
        !GetProcAddress(texture_variants, "A2FO_ModuleInit") ||
        !GetProcAddress(texture_variants, "A2FO_ModuleShutdown")) {
        return 5;
    }

    HMODULE mission_selector = LoadLibraryA(
        "modules\\A2FOMissionSelector.dll");
    if (!mission_selector ||
        !GetProcAddress(mission_selector, "A2FO_ModuleInit") ||
        !GetProcAddress(mission_selector, "A2FO_ModuleShutdown")) {
        return 7;
    }

    // The proxy intentionally refuses to attach without the shipped renamed
    // Win2kDisableTaskSwitch.original.dll. Its exports are checked by
    // `make verify`; this standalone smoke test covers the self-contained core.
}

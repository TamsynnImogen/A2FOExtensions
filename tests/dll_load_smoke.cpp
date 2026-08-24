#include <windows.h>

int main() {
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    if (!core || !GetProcAddress(core, "A2FO_Initialize") ||
        !GetProcAddress(core, "A2FO_NebulaRendererStatus") ||
        !GetProcAddress(core, "A2FO_NebulaSetEmissiveBumpMultiplier") ||
        !GetProcAddress(core, "A2FO_NebulaSetBumpLightBias") ||
        !GetProcAddress(core, "A2FO_NebulaSetEmissiveDiffuseRestore") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveClass") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterEmissiveMaterials") ||
        !GetProcAddress(core, "A2FO_NebulaRegisterSpecularMaterials") ||
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

    HMODULE instant_action_settings = LoadLibraryA(
        "modules\\A2FOInstantActionSettings.dll");
    if (!instant_action_settings ||
        !GetProcAddress(instant_action_settings, "A2FO_ModuleInit") ||
        !GetProcAddress(instant_action_settings, "A2FO_ModuleShutdown")) {
        return 8;
    }

    HMODULE build_tooltips = LoadLibraryA(
        "modules\\A2FOBuildTooltips.dll");
    if (!build_tooltips ||
        !GetProcAddress(build_tooltips, "A2FO_ModuleInit") ||
        !GetProcAddress(build_tooltips, "A2FO_ModuleShutdown")) {
        return 9;
    }

    HMODULE resources = LoadLibraryA("modules\\A2FOResources.dll");
    if (!resources || !GetProcAddress(resources, "A2FO_ModuleInit") ||
        !GetProcAddress(resources, "A2FO_ModuleShutdown") ||
        !GetProcAddress(resources, "A2FOResources_Get") ||
        !GetProcAddress(resources, "A2FOResources_Set") ||
        !GetProcAddress(resources, "A2FOResources_Add") ||
        !GetProcAddress(resources, "A2FOResources_GetCost") ||
        !GetProcAddress(
            resources, "A2FOResources_GetPresentationText")) {
        return 10;
    }

    HMODULE energy_systems = LoadLibraryA(
        "modules\\A2FOEnergySystems.dll");
    if (!energy_systems ||
        !GetProcAddress(energy_systems, "A2FO_ModuleInit") ||
        !GetProcAddress(energy_systems, "A2FO_ModuleShutdown") ||
        !GetProcAddress(
            energy_systems, "A2FOEnergySystems_GetPhotonTorpedoes") ||
        !GetProcAddress(
            energy_systems, "A2FOEnergySystems_GetQuantumTorpedoes") ||
        !GetProcAddress(
            energy_systems, "A2FOEnergySystems_SetPhotonTorpedoes") ||
        !GetProcAddress(
            energy_systems, "A2FOEnergySystems_SetQuantumTorpedoes")) {
        return 11;
    }

    HMODULE directional_shields = LoadLibraryA(
        "modules\\A2FODirectionalShields.dll");
    if (!directional_shields ||
        !GetProcAddress(directional_shields, "A2FO_ModuleInit") ||
        !GetProcAddress(directional_shields, "A2FO_ModuleShutdown") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_ConnectDamageBridge") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_BeginDamage") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_EndDamage") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_IsEnabled") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_GetCurrent") ||
        !GetProcAddress(directional_shields,
                        "A2FODirectionalShields_GetMaximum")) {
        return 12;
    }

    HMODULE craft_identity = LoadLibraryA(
        "modules\\A2FOCraftIdentity.dll");
    if (!craft_identity ||
        !GetProcAddress(craft_identity, "A2FO_ModuleInit") ||
        !GetProcAddress(craft_identity, "A2FO_ModuleShutdown")) {
        return 13;
    }

    HMODULE refit_yards = LoadLibraryA(
        "modules\\A2FORefitYards.dll");
    if (!refit_yards ||
        !GetProcAddress(refit_yards, "A2FO_ModuleInit") ||
        !GetProcAddress(refit_yards, "A2FO_ModuleShutdown")) {
        return 14;
    }

    // The proxy intentionally refuses to attach without the shipped renamed
    // Win2kDisableTaskSwitch.original.dll. Its exports are checked by
    // `make verify`; this standalone smoke test covers the self-contained core.
}

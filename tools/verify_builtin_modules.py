#!/usr/bin/env python3
"""Fail closed if the consolidated DLL is missing its built-ins or imports them."""
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--objdump", default="i686-w64-mingw32-objdump")
parser.add_argument("dll")
args = parser.parse_args()
output = subprocess.check_output([args.objdump, "-p", args.dll], text=True)
exports = {
    match.group(1)
    for line in output.splitlines()
    if (match := re.match(
        r"\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+)?(\w+)\s*$", line
    ))
}
required = {
    "a2fo_a1_render_legacy_resource_panel",
    "A2FOSOLMaps_LoadGameVersionProbe", "A2FOSOLMaps_LoadGameMain",
    "A2FOSOLMaps_LoadPostMissionA", "A2FOSOLMaps_LoadPostMissionB",
    "A2FOSOLMaps_GetSelectedSummary",

    "A2FO_ProducerEventsReady",
    "A2FO_RegisterHybridBridge",
    "A2FO_RegisterRefitQueueBridge",
    "A2FO_ProducerPushRefit",
    "A2FO_ProducerCancelQueuedJob",
    "A2FO_ProducerLogicalQueueSize",
    "A2FO_ProducerLogicalQueueHasRoom",
    "A2FO_ProducerQueueExtendedBuild",
    "A2FOCraftIdentity_TakeLoadedDirectionalShields",
    "A2FOResources_Get",
    "A2FOResources_Set",
    "A2FOResources_Add",
    "A2FOResources_GetCost",
    "A2FOResources_GetPresentationText",
    "A2FOSquadrons_GetAdditionalCosts",
    "A2FOMissions_ScriptInit",
    "A2FOMissions_ScriptExtendInterface",
    "A2FOMissions_MarkMissionUnloaded",
    "A2FORefitMenus_PresentSceneV1",
    "A2FORefitMenus_SetMissionV1",
    "A2FORefitMenus_BeginDepartureV1",
    "A2FORefitMenus_EndDepartureV1",
    "A2FORefitMenus_QueryStatus",
    "A2FO_CopyShellCanvasV1",
    "A2FO_SetShellCanvasPresenterV1",
    "A2FO_ShellDialogBridgeReadyV1",
    "A2FO_SetShellCanvasSeedV1",
    "A2FO_SetShellCanvasRefreshV1",
    "A2FO_SetShellCanvasCompletionV1",
    "A2FO_ShellCanvasWindowV1",
    "A2FOSOLMaps_ClearSelected",
    "A2FOSOLMaps_CreateMapDetails",
    "A2FOSOLMaps_IsSelected",
    "A2FOSOLMaps_LoadGameObjects",
    "A2FOSOLMaps_LoadAiMission",
    "A2FOSOLMaps_LoadA2CraftClassTable",

    "A2FOBuiltinModule_StartupReady",
    "A2FOBuiltinA1Compat_Init",
    "A2FOBuiltinCraftIdentity_Init",
    "A2FOBuiltinFeaturePack_Init",
    "A2FOBuiltinLogMailer_Init",
    "A2FOBuiltinMissionSelector_Init",
    "A2FOBuiltinMissions_Init",
    "A2FOBuiltinRefitMenus_Init",
    "A2FOBuiltinResources_Init",
    "A2FOBuiltinSOLMaps_Init",
    "A2FOBuiltinShellDisplay_Init",
    "A2FOBuiltinSquadrons_Init",
    "A2FOBuiltinVideo_Init",

    "A2FOCollectiveXP_ResolveBuildClass",
    "A2FOCollectiveXP_GetProgress",
    "A2FOCollectiveXP_IsEnabledForObject",
    "A2FONebulaRenderer_RegisterClass",
    "A2FOTextureVariants_RegisterClass",
    "A2FO_RegisterRefitUiBridge",
    "A2FODirectionalShields_ConnectDamageBridge",
    "A2FODirectionalShields_BeginDamage",
    "A2FODirectionalShields_EndDamage",
    "A2FODirectionalShields_IsEnabled",
    "A2FODirectionalShields_GetMode",
    "A2FODirectionalShields_GetCurrent",
    "A2FODirectionalShields_GetMaximum",
    "A2FODirectionalShields_GetState",
    "A2FODirectionalShields_SetState",
    "A2FOEnergySystems_GetPhotonTorpedoes",
    "A2FOEnergySystems_GetQuantumTorpedoes",
    "A2FOEnergySystems_GetShuttleCraft",
    "A2FOEnergySystems_GetMaximumPhotonTorpedoes",
    "A2FOEnergySystems_GetMaximumQuantumTorpedoes",
    "A2FOEnergySystems_GetMaximumShuttleCraft",
    "A2FOEnergySystems_GetPhotonTorpedoReloadSeconds",
    "A2FOEnergySystems_GetQuantumTorpedoReloadSeconds",
    "A2FOEnergySystems_GetShuttleCraftReloadSeconds",
    "A2FOEnergySystems_SetPhotonTorpedoes",
    "A2FOEnergySystems_SetQuantumTorpedoes",
    "A2FOEnergySystems_SetShuttleCraft",
    "A2FOEnergySystems_AddPhotonTorpedoes",
    "A2FOEnergySystems_AddQuantumTorpedoes",
    "A2FOEnergySystems_AddShuttleCraft",
    "A2FOEnergySystems_GetResupplyWeaponState",

    "A2FOBuiltinCheats_Init",
    "A2FOBuiltinCollectiveXP_Init",
    "A2FOBuiltinDirectionalShields_Init",
    "A2FOBuiltinEnergySystems_Init",
    "A2FOBuiltinHybridBuild_Init",
    "A2FOBuiltinMultiYardBuild_Init",
    "A2FOBuiltinNebulaRenderer_Init",
    "A2FOBuiltinPointDefenseCycles_Init",
    "A2FOBuiltinRefitYards_Init",
    "A2FOBuiltinTurrets_Init",
    "A2FOBuiltinRGBTextures_Init",
    "A2FOBuiltinTextureVariants_Init",

    "A2FOBuiltinFireArcs_Init",
    "A2FOBuiltinAnimations_Init",
    "A2FOBuiltinAlwaysShowShields_Init",
    "A2FOBuiltinStationRotation_Init",
    "A2FOBuiltinRadarDiagnostics_Init",
    "A2FOBuiltinRecoveryTests_Init",
    "A2FO_AnimationsEvaluate", "A2FO_AnimationsControlled", "A2FO_AnimationsWeaponFired",
    "A2FOAlwaysShowShields_RegisterClass", "A2FOAlwaysShowShields_UpdateCraft", "A2FOAlwaysShowShields_CleanupCraft",
    "A2FORecoveryTests_Status",
    "A2FOBuiltinEditMenu_Init",
    "A2FOBuiltinM3D_Init",
    "A2FOBuiltinBreeders_Init",
    "A2FOBuiltinInfestation_Init",
    "A2FOBuiltinSwarmSystem_Init",
    "A2FOBuiltinODFVariants_Init",

    "A2FO_Initialize", "A2FOBuiltinWreckage_Init",
    "A2FOBuiltinNormalWeaponTech_Init", "A2FOBuiltinBuildTooltips_Init",
    "A2FOBuiltinInfoIni_Init", "A2FOBuiltinMuzzleFlashes_Init",
    "A2FOBuiltinTeamChangeWeapons_Init", "A2FOBuiltinWeaponDamageControls_Init",
    "A2FOBuiltinA1Fallbacks_Init", "A2FOBuiltinAnimatedHardpoints_Init",
    "A2FOBuiltinCameraControls_Init", "A2FOBuiltinViewDistance_Init",
    "A2FOBuiltinInstantActionSettings_Init",
    "A2FOBuiltinInstantActionSettings_RaceById",
    "A2FOBuiltinCameraControls_QueryStatus", "A2FOBuiltinViewDistance_QueryStatus",
    "A2FOWeaponDamageControls_RefreshDirectionalShieldsBridge",
    "A2FONormalWeaponTech_AllowWeaponTrigger",
}
missing = required - exports
if missing:
    raise SystemExit("Missing core exports: " + ", ".join(sorted(missing)))
if {"A2FO_ModuleInit", "A2FO_ModuleShutdown", "A2FORefit_QueryStatus"} & exports:
    raise SystemExit("Standalone module entrypoints leaked into the core DLL")
imports = re.findall(r"DLL Name:\s*(\S+)", output)
for name in imports:
    if name.lower() in {'a2fofeaturepack.dll', 'a2foshelldisplay.dll', 'a2forefitmenus.dll', 'a2fomissionselector.dll', 'a2foresources.dll', 'a2fosquadrons.dll', 'a2fovideo.dll', 'a2focraftidentity.dll', 'a2fomissions.dll', 'a1compat.dll', 'a2fosolmaps.dll', 'a2fologmailer.dll'} or name.lower() in {'a2foturrets.dll', 'a2focheats.dll', 'a2fonebularenderer.dll', 'a2fomultiyardbuild.dll', 'a2fopointdefensecycles.dll', 'a2forefityards.dll', 'a2focollectivexp.dll', 'a2foenergysystems.dll', 'a2forgbtextures.dll', 'a2fodirectionalshields.dll', 'a2fotexturevariants.dll', 'a2fohybridbuild.dll'} or name.lower().startswith(("libgcc", "libstdc++", "libwinpthread", "libatomic")) or name.lower() in {
        "a2fofirearcs.dll", "a2foanimations.dll", "a2foalwaysshowshields.dll", "a2fostationrotation.dll", "a2foradardiagnostics.dll", "a2forecoverytests.dll",
        "a2fowreckage.dll", "a2fonormalweapontech.dll", "a2fobuildtooltips.dll",
        "a2foinfoini.dll", "a2fomuzzleflashes.dll",
        "a2foteamchangeweapons.dll", "a2foweapondamagecontrols.dll",
        "a1fallbacks.dll", "a2foanimatedhardpoints.dll",
        "a2focameracontrols.dll", "a2foviewdistance.dll", "a2foinstantactionsettings.dll", "a2foeditmenu.dll", "a2fom3d.dll", "a2fobreeders.dll", "a2foinfestation.dll", "a2foswarmsystem.dll", "a2foodfvariants.dll"
    }:
        raise SystemExit("Unexpected runtime dependency: " + name)
print("Core built-in exports and runtime dependencies: PASS")

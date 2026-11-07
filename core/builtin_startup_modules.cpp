#include "builtin_startup_modules.hpp"

extern "C" {
bool A2FO_CALL A2FOBuiltinA1Compat_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinA1Compat_Shutdown();
bool A2FO_CALL A2FOBuiltinCraftIdentity_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinCraftIdentity_Shutdown();
bool A2FO_CALL A2FOBuiltinFeaturePack_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinFeaturePack_Shutdown();
bool A2FO_CALL A2FOBuiltinLogMailer_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinLogMailer_Shutdown();
bool A2FO_CALL A2FOBuiltinMissionSelector_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinMissionSelector_Shutdown();
bool A2FO_CALL A2FOBuiltinMissions_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinMissions_Shutdown();
bool A2FO_CALL A2FOBuiltinRefitMenus_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinRefitMenus_Shutdown();
bool A2FO_CALL A2FOBuiltinResources_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinResources_Shutdown();
bool A2FO_CALL A2FOBuiltinSOLMaps_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinSOLMaps_Shutdown();
bool A2FO_CALL A2FOBuiltinShellDisplay_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinShellDisplay_Shutdown();
bool A2FO_CALL A2FOBuiltinSquadrons_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinSquadrons_Shutdown();
bool A2FO_CALL A2FOBuiltinVideo_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinVideo_Shutdown();

bool A2FO_CALL A2FOBuiltinCheats_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinCheats_Shutdown();
bool A2FO_CALL A2FOBuiltinCollectiveXP_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinCollectiveXP_Shutdown();
bool A2FO_CALL A2FOBuiltinDirectionalShields_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinDirectionalShields_Shutdown();
bool A2FO_CALL A2FOBuiltinEnergySystems_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinEnergySystems_Shutdown();
bool A2FO_CALL A2FOBuiltinHybridBuild_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinHybridBuild_Shutdown();
bool A2FO_CALL A2FOBuiltinMultiYardBuild_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinMultiYardBuild_Shutdown();
bool A2FO_CALL A2FOBuiltinNebulaRenderer_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinNebulaRenderer_Shutdown();
bool A2FO_CALL A2FOBuiltinPointDefenseCycles_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinPointDefenseCycles_Shutdown();
bool A2FO_CALL A2FOBuiltinRefitYards_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinRefitYards_Shutdown();
bool A2FO_CALL A2FOBuiltinTurrets_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinTurrets_Shutdown();
bool A2FO_CALL A2FOBuiltinRGBTextures_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinRGBTextures_Shutdown();
bool A2FO_CALL A2FOBuiltinTextureVariants_Init(const A2FO_ModuleApi *);
void A2FO_CALL A2FOBuiltinTextureVariants_Shutdown();
}

#include "../rust/builtin_runtime_ffi.h"
#include <exception>
namespace {
const A2FO_ModuleInitFn initializers[] = {A2FOBuiltinCheats_Init,
                                          A2FOBuiltinCollectiveXP_Init,
                                          A2FOBuiltinDirectionalShields_Init,
                                          A2FOBuiltinEnergySystems_Init,
                                          A2FOBuiltinHybridBuild_Init,
                                          A2FOBuiltinMultiYardBuild_Init,
                                          A2FOBuiltinNebulaRenderer_Init,
                                          A2FOBuiltinPointDefenseCycles_Init,
                                          A2FOBuiltinRefitYards_Init,
                                          A2FOBuiltinTurrets_Init,
                                          A2FOBuiltinRGBTextures_Init,
                                          A2FOBuiltinTextureVariants_Init,
                                          A2FOBuiltinA1Compat_Init,
                                          A2FOBuiltinCraftIdentity_Init,
                                          A2FOBuiltinFeaturePack_Init,
                                          A2FOBuiltinLogMailer_Init,
                                          A2FOBuiltinMissionSelector_Init,
                                          A2FOBuiltinMissions_Init,
                                          A2FOBuiltinRefitMenus_Init,
                                          A2FOBuiltinResources_Init,
                                          A2FOBuiltinSOLMaps_Init,
                                          A2FOBuiltinShellDisplay_Init,
                                          A2FOBuiltinSquadrons_Init,
                                          A2FOBuiltinVideo_Init};
const A2FO_ModuleShutdownFn shutdowns[] = {A2FOBuiltinCheats_Shutdown,
                                           A2FOBuiltinCollectiveXP_Shutdown,
                                           A2FOBuiltinDirectionalShields_Shutdown,
                                           A2FOBuiltinEnergySystems_Shutdown,
                                           A2FOBuiltinHybridBuild_Shutdown,
                                           A2FOBuiltinMultiYardBuild_Shutdown,
                                           A2FOBuiltinNebulaRenderer_Shutdown,
                                           A2FOBuiltinPointDefenseCycles_Shutdown,
                                           A2FOBuiltinRefitYards_Shutdown,
                                           A2FOBuiltinTurrets_Shutdown,
                                           A2FOBuiltinRGBTextures_Shutdown,
                                           A2FOBuiltinTextureVariants_Shutdown,
                                           A2FOBuiltinA1Compat_Shutdown,
                                           A2FOBuiltinCraftIdentity_Shutdown,
                                           A2FOBuiltinFeaturePack_Shutdown,
                                           A2FOBuiltinLogMailer_Shutdown,
                                           A2FOBuiltinMissionSelector_Shutdown,
                                           A2FOBuiltinMissions_Shutdown,
                                           A2FOBuiltinRefitMenus_Shutdown,
                                           A2FOBuiltinResources_Shutdown,
                                           A2FOBuiltinSOLMaps_Shutdown,
                                           A2FOBuiltinShellDisplay_Shutdown,
                                           A2FOBuiltinSquadrons_Shutdown,
                                           A2FOBuiltinVideo_Shutdown};
struct InitContext {
    const A2FO_ModuleApi *api;
    std::exception_ptr error;
};
std::uint8_t initialize(void *p, std::int32_t i) noexcept {
    auto &c = *static_cast<InitContext *>(p);
    try {
        return initializers[i](c.api);
    } catch (...) {
        c.error = std::current_exception();
        return 0;
    }
}
} // namespace
namespace a2fo {
bool initialize_startup_builtin(BuiltinFeature feature, const A2FO_ModuleApi &api) {
    InitContext c{&api, {}};
    const bool ok = a2fo_rs_startup_initialize(static_cast<int>(feature), &c, initialize) != 0;
    if (c.error)
        std::rethrow_exception(c.error);
    return ok;
}
A2FO_ModuleShutdownFn startup_builtin_shutdown(BuiltinFeature feature) noexcept {
    const int i = a2fo_rs_startup_index(static_cast<int>(feature));
    return i < 0 ? nullptr : shutdowns[i];
}
} // namespace a2fo

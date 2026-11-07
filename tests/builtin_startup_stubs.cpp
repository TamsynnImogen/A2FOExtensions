#include "../core/builtin_state.hpp"
#include "../sdk/include/a2fo_module_api.h"

#include <array>
#include <string>
#include <vector>

extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept;

namespace startup_fixture {
constexpr unsigned kCount =
    static_cast<unsigned>(a2fo::BuiltinFeature::count) -
    static_cast<unsigned>(a2fo::BuiltinFeature::cheats);
static_assert(kCount == 24, "startup fixture must cover the current 24 modules");
std::array<unsigned, kCount> init_calls{};
std::array<unsigned, kCount> shutdown_calls{};
int fail_index = -1;
unsigned old_init_calls = 0;
unsigned camera_gate_calls = 0;
unsigned view_gate_calls = 0;
std::vector<std::string> init_order;
bool enforce_dependencies = false;
bool dependency_error = false;
bool producer_ready = false;

void observe_dependencies(unsigned index) {
    if (!enforce_dependencies) return;
    const auto ready = [](const char* name) {
        return A2FOBuiltinModule_StartupReady(name);
    };
    if ((index == 4 && !ready("A2FOFeaturePack")) ||
        (index == 8 && (!ready("A2FOFeaturePack") || !ready("A2FOHybridBuild"))) ||
        ((index == 2 || index == 3 || index == 11) &&
         !ready("A2FOCraftIdentity"))) {
        dependency_error = true;
    }
}
}

#define STARTUP_STUB(name, index) \
extern "C" bool A2FO_CALL A2FOBuiltin##name##_Init(const A2FO_ModuleApi* api) { \
    ++startup_fixture::init_calls[index]; \
    startup_fixture::init_order.emplace_back(a2fo::canonical_builtin_name( \
        static_cast<a2fo::BuiltinFeature>( \
            static_cast<unsigned>(a2fo::BuiltinFeature::cheats) + index))); \
    startup_fixture::observe_dependencies(index); \
    return api && startup_fixture::fail_index != index; \
} \
extern "C" void A2FO_CALL A2FOBuiltin##name##_Shutdown() { \
    ++startup_fixture::shutdown_calls[index]; \
}

STARTUP_STUB(Cheats, 0)
STARTUP_STUB(CollectiveXP, 1)
STARTUP_STUB(DirectionalShields, 2)
STARTUP_STUB(EnergySystems, 3)
STARTUP_STUB(HybridBuild, 4)
STARTUP_STUB(MultiYardBuild, 5)
STARTUP_STUB(NebulaRenderer, 6)
STARTUP_STUB(PointDefenseCycles, 7)
STARTUP_STUB(RefitYards, 8)
STARTUP_STUB(Turrets, 9)
STARTUP_STUB(RGBTextures, 10)
STARTUP_STUB(TextureVariants, 11)
STARTUP_STUB(A1Compat, 12)
STARTUP_STUB(CraftIdentity, 13)
STARTUP_STUB(FeaturePack, 14)
STARTUP_STUB(LogMailer, 15)
STARTUP_STUB(MissionSelector, 16)
STARTUP_STUB(Missions, 17)
STARTUP_STUB(RefitMenus, 18)
STARTUP_STUB(Resources, 19)
STARTUP_STUB(SOLMaps, 20)
STARTUP_STUB(ShellDisplay, 21)
STARTUP_STUB(Squadrons, 22)
STARTUP_STUB(Video, 23)

#define OLD_STUB(name) \
extern "C" bool A2FO_CALL A2FOBuiltin##name##_Init(const A2FO_ModuleApi*) { \
    ++startup_fixture::old_init_calls; return true; \
} \
extern "C" bool A2FOBuiltin##name##_Ready() noexcept { return true; }

#define OLD_INIT_ONLY(name) \
extern "C" bool A2FO_CALL A2FOBuiltin##name##_Init(const A2FO_ModuleApi*) { \
    ++startup_fixture::old_init_calls; return true; \
}

OLD_STUB(FireArcs)
OLD_STUB(Animations)
OLD_STUB(AlwaysShowShields)
OLD_STUB(StationRotation)
OLD_STUB(RadarDiagnostics)
OLD_STUB(RecoveryTests)
OLD_STUB(EditMenu)
OLD_STUB(M3D)
OLD_STUB(Breeders)
OLD_STUB(Infestation)
OLD_STUB(SwarmSystem)
OLD_STUB(ODFVariants)
OLD_STUB(NormalWeaponTech)
OLD_STUB(BuildTooltips)
OLD_STUB(MuzzleFlashes)
OLD_STUB(TeamChangeWeapons)
OLD_STUB(WeaponDamageControls)
OLD_STUB(A1Fallbacks)
OLD_STUB(AnimatedHardpoints)
OLD_STUB(CameraControls)
OLD_STUB(ViewDistance)
OLD_STUB(InstantActionSettings)
OLD_INIT_ONLY(Wreckage)
OLD_INIT_ONLY(InfoIni)

extern "C" void A2FOBuiltinCameraControls_SetEnabled(bool) noexcept {
    ++startup_fixture::camera_gate_calls;
}
extern "C" void A2FOBuiltinViewDistance_SetEnabled(bool) noexcept {
    ++startup_fixture::view_gate_calls;
}

extern "C" bool A2FO_CALL A2FO_ProducerEventsReady() {
    return startup_fixture::producer_ready;
}

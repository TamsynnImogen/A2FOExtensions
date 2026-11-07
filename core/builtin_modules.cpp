#include "builtin_modules.hpp"
#include "builtin_startup_modules.hpp"

#include <array>

extern "C" {
bool A2FO_CALL A2FOBuiltinFireArcs_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinFireArcs_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinAnimations_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinAnimations_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinAlwaysShowShields_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinAlwaysShowShields_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinStationRotation_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinStationRotation_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinRadarDiagnostics_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinRadarDiagnostics_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinRecoveryTests_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinRecoveryTests_Ready() noexcept;

bool A2FO_CALL A2FOBuiltinEditMenu_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinEditMenu_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinM3D_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinM3D_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinBreeders_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinBreeders_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinInfestation_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinInfestation_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinSwarmSystem_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinSwarmSystem_Ready() noexcept;
bool A2FO_CALL A2FOBuiltinODFVariants_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinODFVariants_Ready() noexcept;

bool A2FO_CALL A2FOBuiltinWreckage_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinNormalWeaponTech_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinBuildTooltips_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinInfoIni_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinMuzzleFlashes_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinTeamChangeWeapons_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinWeaponDamageControls_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinA1Fallbacks_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinAnimatedHardpoints_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinCameraControls_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinViewDistance_Init(const A2FO_ModuleApi *);
bool A2FO_CALL A2FOBuiltinInstantActionSettings_Init(const A2FO_ModuleApi *);
bool A2FOBuiltinNormalWeaponTech_Ready() noexcept;
bool A2FOBuiltinBuildTooltips_Ready() noexcept;
bool A2FOBuiltinMuzzleFlashes_Ready() noexcept;
bool A2FOBuiltinTeamChangeWeapons_Ready() noexcept;
bool A2FOBuiltinWeaponDamageControls_Ready() noexcept;
bool A2FOBuiltinA1Fallbacks_Ready() noexcept;
bool A2FOBuiltinAnimatedHardpoints_Ready() noexcept;
bool A2FOBuiltinCameraControls_Ready() noexcept;
bool A2FOBuiltinViewDistance_Ready() noexcept;
bool A2FOBuiltinInstantActionSettings_Ready() noexcept;
void A2FOBuiltinCameraControls_SetEnabled(bool) noexcept;
void A2FOBuiltinViewDistance_SetEnabled(bool) noexcept;
}

#include "../rust/builtin_runtime_ffi.h"
#include <cstring>
#include <exception>
namespace a2fo {
namespace {
std::vector<std::string> runtime_roots;
const A2FO_ModuleInitFn initializers[] = {A2FOBuiltinBuildTooltips_Init,
                                          A2FOBuiltinNormalWeaponTech_Init,
                                          A2FOBuiltinWreckage_Init,
                                          A2FOBuiltinInfoIni_Init,
                                          A2FOBuiltinMuzzleFlashes_Init,
                                          A2FOBuiltinTeamChangeWeapons_Init,
                                          A2FOBuiltinWeaponDamageControls_Init,
                                          A2FOBuiltinA1Fallbacks_Init,
                                          A2FOBuiltinAnimatedHardpoints_Init,
                                          A2FOBuiltinCameraControls_Init,
                                          A2FOBuiltinViewDistance_Init,
                                          A2FOBuiltinInstantActionSettings_Init,
                                          A2FOBuiltinEditMenu_Init,
                                          A2FOBuiltinM3D_Init,
                                          A2FOBuiltinBreeders_Init,
                                          A2FOBuiltinInfestation_Init,
                                          A2FOBuiltinSwarmSystem_Init,
                                          A2FOBuiltinODFVariants_Init,
                                          A2FOBuiltinFireArcs_Init,
                                          A2FOBuiltinAnimations_Init,
                                          A2FOBuiltinAlwaysShowShields_Init,
                                          A2FOBuiltinStationRotation_Init,
                                          A2FOBuiltinRadarDiagnostics_Init,
                                          A2FOBuiltinRecoveryTests_Init};
using Ready = bool (*)();
const Ready ready_functions[] = {A2FOBuiltinBuildTooltips_Ready,
                                 A2FOBuiltinNormalWeaponTech_Ready,
                                 nullptr,
                                 nullptr,
                                 A2FOBuiltinMuzzleFlashes_Ready,
                                 A2FOBuiltinTeamChangeWeapons_Ready,
                                 A2FOBuiltinWeaponDamageControls_Ready,
                                 A2FOBuiltinA1Fallbacks_Ready,
                                 A2FOBuiltinAnimatedHardpoints_Ready,
                                 A2FOBuiltinCameraControls_Ready,
                                 A2FOBuiltinViewDistance_Ready,
                                 A2FOBuiltinInstantActionSettings_Ready,
                                 A2FOBuiltinEditMenu_Ready,
                                 A2FOBuiltinM3D_Ready,
                                 A2FOBuiltinBreeders_Ready,
                                 A2FOBuiltinInfestation_Ready,
                                 A2FOBuiltinSwarmSystem_Ready,
                                 A2FOBuiltinODFVariants_Ready,
                                 A2FOBuiltinFireArcs_Ready,
                                 A2FOBuiltinAnimations_Ready,
                                 A2FOBuiltinAlwaysShowShields_Ready,
                                 A2FOBuiltinStationRotation_Ready,
                                 A2FOBuiltinRadarDiagnostics_Ready,
                                 A2FOBuiltinRecoveryTests_Ready};
struct PrepareContext {
    const A2FO_ModuleApi *api;
    std::exception_ptr error;
};
std::uint8_t initialize(void *p, std::int32_t i) noexcept {
    auto &c = *static_cast<PrepareContext *>(p);
    try {
        return i >= 24 ? initialize_startup_builtin(static_cast<BuiltinFeature>(i), *c.api)
                       : initializers[i](c.api);
    } catch (...) {
        c.error = std::current_exception();
        return 0;
    }
}
std::uint8_t ready(void *, std::int32_t i) noexcept { return ready_functions[i](); }
void camera_gate(std::uint8_t v) noexcept { A2FOBuiltinCameraControls_SetEnabled(v != 0); }
void view_gate(std::uint8_t v) noexcept { A2FOBuiltinViewDistance_SetEnabled(v != 0); }
A2foRsRuntimeBytes bytes(const std::string &s) noexcept {
    return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}
struct Text {
    std::string value;
    std::exception_ptr error;
};
void emit(void *p, const std::uint8_t *d, std::size_t n) noexcept {
    auto &c = *static_cast<Text *>(p);
    try {
        c.value.assign(reinterpret_cast<const char *>(d), n);
    } catch (...) {
        c.error = std::current_exception();
    }
}
std::string text_result(Text &t) {
    if (t.error)
        std::rethrow_exception(t.error);
    return std::move(t.value);
}
} // namespace
bool builtin_module_enabled(BuiltinFeature f) noexcept {
    return a2fo_rs_runtime_state(static_cast<int>(f), 0) != 0;
}
std::uint64_t builtin_module_activation(BuiltinFeature f) noexcept {
    return a2fo_rs_runtime_state(static_cast<int>(f), 0);
}
bool builtin_module_available(BuiltinFeature f) noexcept {
    return (a2fo_rs_runtime_state(static_cast<int>(f), 1) & 2) != 0;
}
bool builtin_module_initial_request(BuiltinFeature f, bool &requested) noexcept {
    std::uint8_t r = 0;
    if (!a2fo_rs_runtime_initial(static_cast<int>(f), &r))
        return false;
    requested = r != 0;
    return true;
}
bool prepare_builtin_module(BuiltinFeature f, const A2FO_ModuleApi &api, bool requested) {
    PrepareContext c{&api, {}};
    const bool ok =
        a2fo_rs_runtime_prepare(static_cast<int>(f), requested, &c, initialize, ready) != 0;
    if (c.error)
        std::rethrow_exception(c.error);
    return ok;
}
void finish_builtin_module_preparation(bool destroyed, bool weapon, bool weapon_class, bool info,
                                       bool craft, bool klass, bool race, bool producer) {
    const std::uint32_t mask = (destroyed ? 1u : 0) | (weapon ? 2u : 0) | (weapon_class ? 4u : 0) |
                               (info ? 8u : 0) | (craft ? 16u : 0) | (klass ? 32u : 0) |
                               (race ? 64u : 0) | (producer ? 128u : 0);
    a2fo_rs_runtime_finish(mask, camera_gate, view_gate);
}
void set_builtin_module_roots(const std::vector<std::string> &roots) { runtime_roots = roots; }
bool builtin_modules_match_roots(const std::vector<std::string> &roots) {
    std::vector<A2foRsRuntimeBytes> a, b;
    for (const auto &r : runtime_roots)
        a.push_back(bytes(r));
    for (const auto &r : roots)
        b.push_back(bytes(r));
    return a2fo_rs_runtime_roots_match(a.data(), a.size(), b.data(), b.size()) != 0;
}
bool apply_builtin_module_policy(const std::vector<std::string> &roots, const ModulePolicy &policy,
                                 std::string &error) {
    error.clear();
    const bool same = builtin_modules_match_roots(roots);
    std::vector<A2foRsRuntimeEntry> entries;
    for (const auto &e : policy.entries)
        entries.push_back({bytes(e.name), static_cast<std::uint32_t>(e.state)});
    Text text;
    const bool ok = a2fo_rs_runtime_apply(same, policy.valid, entries.data(), entries.size(), emit,
                                          &text, camera_gate, view_gate) != 0;
    error = text_result(text);
    return ok;
}
std::string builtin_module_status(const std::string &name, const std::vector<std::string> &roots) {
    Text text;
    a2fo_rs_runtime_status(bytes(name), builtin_modules_match_roots(roots), emit, &text);
    return text_result(text);
}
} // namespace a2fo
extern "C" __declspec(dllexport) bool A2FO_CALL
A2FOBuiltinModule_StartupReady(const char *name) noexcept {
    if (!name)
        return false;
    return a2fo_rs_runtime_startup_ready(
               {reinterpret_cast<const std::uint8_t *>(name), std::strlen(name)}) != 0;
}

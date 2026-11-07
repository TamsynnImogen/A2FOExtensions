#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::weapon_damage_controls
#define A2FO_ModuleInit A2FOBuiltinWeaponDamageControls_Init
#define A2FO_ModuleShutdown A2FOBuiltinWeaponDamageControls_Shutdown
#include "../modules/A2FOWeaponDamageControls/module.cpp"

extern "C" bool A2FOBuiltinWeaponDamageControls_Ready() noexcept {
    return g_runtime_ready;
}

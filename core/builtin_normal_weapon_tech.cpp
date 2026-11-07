#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::normal_weapon_tech
#define A2FO_ModuleInit A2FOBuiltinNormalWeaponTech_Init
#define A2FO_ModuleShutdown A2FOBuiltinNormalWeaponTech_Shutdown
#include "../modules/A2FONormalWeaponTech/module.cpp"

extern "C" bool A2FOBuiltinNormalWeaponTech_Ready() noexcept {
    return g_runtime_ready;
}

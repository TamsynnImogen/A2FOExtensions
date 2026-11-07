#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::team_change_weapons
#define A2FO_ModuleInit A2FOBuiltinTeamChangeWeapons_Init
#define A2FO_ModuleShutdown A2FOBuiltinTeamChangeWeapons_Shutdown
#include "../modules/A2FOTeamChangeWeapons/module.cpp"

extern "C" bool A2FOBuiltinTeamChangeWeapons_Ready() noexcept {
    return g_ready;
}

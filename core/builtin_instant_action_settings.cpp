#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::instant_action_settings
#define A2FO_ModuleInit A2FOBuiltinInstantActionSettings_Init
#define A2FO_ModuleShutdown A2FOBuiltinInstantActionSettings_Shutdown
#define race_by_id_hook A2FOBuiltinInstantActionSettings_RaceById
#include "../modules/A2FOInstantActionSettings/module.cpp"

extern "C" bool A2FOBuiltinInstantActionSettings_Ready() noexcept {
    return g_runtime_ready;
}

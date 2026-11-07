#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::radar_diagnostics
#define A2FO_ModuleInit A2FOBuiltinRadarDiagnostics_Init
#define A2FO_ModuleShutdown A2FOBuiltinRadarDiagnostics_Shutdown
#include "../modules/A2FORadarDiagnostics/module.cpp"

extern "C" bool A2FOBuiltinRadarDiagnostics_Ready() noexcept {
    return !radar_diagnostics::g_config_enabled || (radar_diagnostics::installed && radar_diagnostics::capturing.load());
}

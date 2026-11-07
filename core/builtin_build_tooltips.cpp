#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::build_tooltips
#define A2FO_ModuleInit A2FOBuiltinBuildTooltips_Init
#define A2FO_ModuleShutdown A2FOBuiltinBuildTooltips_Shutdown
#include "../modules/A2FOBuildTooltips/module.cpp"

extern "C" bool A2FOBuiltinBuildTooltips_Ready() noexcept {
    return g_runtime_ready;
}

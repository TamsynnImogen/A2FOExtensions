#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::infestation
#define A2FO_ModuleInit A2FOBuiltinInfestation_Init
#define A2FO_ModuleShutdown A2FOBuiltinInfestation_Shutdown
#include "../modules/A2FOInfestation/module.cpp"

extern "C" bool A2FOBuiltinInfestation_Ready() noexcept {
    return g_runtime_ready;
}
